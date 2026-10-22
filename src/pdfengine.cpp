// =====================================================================
//  MrDocx  -  pdfengine.cpp : offline PDF operations
//  Author : Ragul S
//
//  Structure of every operation:
//    1. declare all C++ locals            (outside fz_try)
//    2. fz_try { ...engine work... }      (raw pointers only in here)
//    3. fz_catch { record the error }
//  MuPDF's fz_try/fz_catch is longjmp based, so no C++ object with a
//  destructor may be created between fz_try and fz_catch.
// =====================================================================
#include "pdfengine.h"
#include <zlib.h>

extern "C" {
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
}

namespace pdf {

// ---------------------------------------------------------------------
// per-thread MuPDF context
// ---------------------------------------------------------------------
namespace {

struct EngineCtx {
    fz_context* ctx = nullptr;
    EngineCtx()
    {
        ctx = fz_new_context(nullptr, nullptr, 192 << 20);
        if (ctx) {
            fz_locks_context locks{};
            fz_register_document_handlers(ctx);
            fz_set_aa_level(ctx, 8);
            (void)locks;
        }
    }
    ~EngineCtx() { if (ctx) fz_drop_context(ctx); }
};

thread_local EngineCtx g_engine;

inline fz_context* CTX() { return g_engine.ctx; }

std::string u8(const std::wstring& w) { return util::toUtf8(w); }
std::wstring w16(const char* s) { return s ? util::toWide(s) : std::wstring(); }

// Turn the currently caught MuPDF error into a friendly message.
std::wstring caughtError(fz_context* ctx)
{
    std::string raw = fz_caught_message(ctx) ? fz_caught_message(ctx) : "";
    std::wstring msg = util::toWide(raw);
    if (msg.empty()) msg = L"Unknown error";

    std::wstring low = util::lower(msg);
    if (low.find(L"cannot open") != std::wstring::npos || low.find(L"no such file") != std::wstring::npos)
        return L"The file could not be opened. It may be locked by another program.";
    if (low.find(L"password") != std::wstring::npos)
        return L"This PDF is password protected. Enter the password and try again.";
    if (low.find(L"not a pdf") != std::wstring::npos || low.find(L"cannot find object") != std::wstring::npos ||
        low.find(L"invalid pdf") != std::wstring::npos || low.find(L"corrupt") != std::wstring::npos ||
        low.find(L"syntax error") != std::wstring::npos)
        return L"The file appears to be damaged or is not a valid PDF.";
    if (low.find(L"out of memory") != std::wstring::npos || low.find(L"malloc") != std::wstring::npos)
        return L"Not enough memory to process this document.";
    if (low.find(L"permission") != std::wstring::npos || low.find(L"access denied") != std::wstring::npos)
        return L"Windows refused to write the output file. Check the folder is writable.";
    return msg;
}

bool isCancelled(fz_cookie* cookie) { return cookie && cookie->abort; }

} // namespace

// ---------------------------------------------------------------------
// page range parsing  "1-3,5,8-10" (1 based, inclusive)
// ---------------------------------------------------------------------
std::vector<int> parsePageRanges(const std::wstring& specIn, int pageCount)
{
    std::vector<int> out;
    if (pageCount <= 0) return out;
    std::wstring spec = util::trim(specIn);
    if (spec.empty()) return out;

    std::wstring low = util::lower(spec);
    if (low == L"all" || low == L"*") {
        for (int i = 0; i < pageCount; i++) out.push_back(i);
        return out;
    }
    if (low == L"odd") {
        for (int i = 0; i < pageCount; i += 2) out.push_back(i);
        return out;
    }
    if (low == L"even") {
        for (int i = 1; i < pageCount; i += 2) out.push_back(i);
        return out;
    }

    for (auto part : util::split(spec, L',')) {
        std::wstring p = util::trim(part);
        if (p.empty()) continue;
        size_t dash = p.find(L'-');
        if (dash == std::wstring::npos) {
            int v = _wtoi(p.c_str());
            if (v >= 1 && v <= pageCount) out.push_back(v - 1);
        } else {
            std::wstring a = util::trim(p.substr(0, dash));
            std::wstring b = util::trim(p.substr(dash + 1));
            int lo = a.empty() ? 1 : _wtoi(a.c_str());
            int hi = b.empty() ? pageCount : _wtoi(b.c_str());
            if (lo > hi) std::swap(lo, hi);
            lo = util::clampv(lo, 1, pageCount);
            hi = util::clampv(hi, 1, pageCount);
            for (int v = lo; v <= hi; v++) out.push_back(v - 1);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

std::wstring describePageSize(double w, double h)
{
    struct Named { const wchar_t* n; double w, h; };
    static const Named sizes[] = {
        { L"A0", 2384, 3370 }, { L"A1", 1684, 2384 }, { L"A2", 1191, 1684 },
        { L"A3", 842, 1191 },  { L"A4", 595, 842 },   { L"A5", 420, 595 },
        { L"A6", 298, 420 },   { L"B4", 709, 1001 },  { L"B5", 499, 709 },
        { L"Letter", 612, 792 }, { L"Legal", 612, 1008 }, { L"Tabloid", 792, 1224 },
        { L"Executive", 522, 756 }, { L"Ledger", 1224, 792 },
    };
    bool land = w > h;
    double a = land ? h : w, b = land ? w : h;
    for (auto& s : sizes) {
        if (std::fabs(a - s.w) < 12 && std::fabs(b - s.h) < 12)
            return util::fmt(L"%s %s", s.n, land ? L"landscape" : L"portrait");
    }
    return util::fmt(L"%.0f x %.0f pt%s", w, h, land ? L" (landscape)" : L"");
}

// ---------------------------------------------------------------------
// document info
// ---------------------------------------------------------------------
bool readInfo(const std::wstring& path, const std::wstring& password, DocInfo& out, std::wstring& err)
{
    out = DocInfo();
    out.fileSize = util::fileSize(path);
    if (!out.fileSize) { err = L"The file does not exist."; return false; }

    fz_context* ctx = CTX();
    if (!ctx) { err = L"PDF engine failed to start."; return false; }

    std::string p = u8(path), pw = u8(password);
    fz_document* doc = nullptr;
    std::string msg;
    bool ok = false;

    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, p.c_str());
        if (fz_needs_password(ctx, doc)) {
            out.needsPassword = true;
            if (!pw.empty() && fz_authenticate_password(ctx, doc, pw.c_str()))
                out.passwordOk = true;
            if (!out.passwordOk) { ok = true; fz_throw(ctx, FZ_ERROR_GENERIC, "needs password"); }
        }
        out.pageCount = fz_count_pages(ctx, doc);
        char buf[1024];
        auto meta = [&](const char* key, std::wstring& dst) {
            buf[0] = 0;
            if (fz_lookup_metadata(ctx, doc, key, buf, sizeof buf) >= 0) dst = w16(buf);
        };
        meta("info:Title", out.title);
        meta("info:Author", out.author);
        meta("info:Subject", out.subject);
        meta("info:Keywords", out.keywords);
        meta("info:Creator", out.creator);
        meta("info:Producer", out.producer);
        buf[0] = 0;
        if (fz_lookup_metadata(ctx, doc, "format", buf, sizeof buf) >= 0) out.pdfVersion = w16(buf);

        if (out.pageCount > 0) {
            fz_page* page = fz_load_page(ctx, doc, 0);
            fz_rect r = fz_bound_page(ctx, page);
            out.pageW = r.x1 - r.x0;
            out.pageH = r.y1 - r.y0;
            fz_drop_page(ctx, page);
        }
        out.pageSize = describePageSize(out.pageW, out.pageH);

        pdf_document* pdf = pdf_specifics(ctx, doc);
        if (pdf) {
            out.encrypted = (pdf->crypt != nullptr);
            // look for forms / annotations / outline on a sample of pages
            int probe = std::min(out.pageCount, 12);
            for (int i = 0; i < probe && !(out.hasForms && out.hasAnnotations); i++) {
                pdf_page* pp = pdf_load_page(ctx, pdf, i);
                if (!pp) continue;
                for (pdf_annot* a = pdf_first_annot(ctx, pp); a; a = pdf_next_annot(ctx, a))
                    out.hasAnnotations = true;
                for (pdf_annot* w = pdf_first_widget(ctx, pp); w; w = pdf_next_widget(ctx, w))
                    out.hasForms = true;
                pdf_drop_page(ctx, pp);
            }
            fz_outline* ol = nullptr;
            fz_var(ol);
            fz_try(ctx) { ol = fz_load_outline(ctx, doc); }
            fz_catch(ctx) { ol = nullptr; }
            out.hasOutline = (ol != nullptr);
            if (ol) fz_drop_outline(ctx, ol);
        }
        ok = true;
    }
    fz_catch(ctx)
    {
        msg = u8(caughtError(ctx));
    }
    if (doc) fz_drop_document(ctx, doc);

    out.valid = ok;
    if (!ok) {
        if (out.needsPassword && !out.passwordOk) { err = L""; return false; }
        err = util::toWide(msg);
        return false;
    }
    return true;
}

bool readPageList(const std::wstring& path, const std::wstring& password,
                  std::vector<PageThumb>& pages, std::wstring& err)
{
    pages.clear();
    fz_context* ctx = CTX();
    if (!ctx) { err = L"PDF engine failed to start."; return false; }
    std::string p = u8(path), pw = u8(password);
    fz_document* doc = nullptr;
    std::string msg;
    bool ok = false;

    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, p.c_str());
        if (fz_needs_password(ctx, doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, doc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        int n = fz_count_pages(ctx, doc);
        for (int i = 0; i < n; i++) {
            PageThumb t;
            t.index = i;
            fz_page* page = nullptr;
            fz_var(page);
            fz_try(ctx) {
                page = fz_load_page(ctx, doc, i);
                if (page) {
                    fz_rect r = fz_bound_page(ctx, page);
                    t.widthPt = r.x1 - r.x0;
                    t.heightPt = r.y1 - r.y0;
                    pdf_page* pp = pdf_page_from_fz_page(ctx, page);
                    if (pp && pp->obj) t.rotation = (int)pdf_dict_get_int(ctx, pp->obj, PDF_NAME(Rotate));
                }
            }
            fz_catch(ctx) { if (page) { fz_drop_page(ctx, page); page = nullptr; } }
            if (!page) { t.readable = false; t.widthPt = 595; t.heightPt = 842; }
            else fz_drop_page(ctx, page);
            pages.push_back(t);   // keep every index so page numbers stay honest
        }
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) fz_drop_document(ctx, doc);
    if (!ok) { err = util::toWide(msg); return false; }
    return true;
}

// ---------------------------------------------------------------------
// render a page to BGRA for the UI
// ---------------------------------------------------------------------
struct OpenDoc { fz_document* doc = nullptr; };

OpenDoc* openDocument(const std::wstring& path, const std::wstring& password)
{
    fz_context* ctx = CTX();
    if (!ctx) return nullptr;
    OpenDoc* o = new OpenDoc();
    std::string p = u8(path), pw = u8(password);
    fz_try(ctx)
    {
        o->doc = fz_open_document(ctx, p.c_str());
        if (fz_needs_password(ctx, o->doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, o->doc, pw.c_str())) {
                fz_drop_document(ctx, o->doc); o->doc = nullptr;
            }
        }
    }
    fz_catch(ctx) { if (o->doc) { fz_drop_document(ctx, o->doc); o->doc = nullptr; } }
    if (!o->doc) { delete o; return nullptr; }
    return o;
}

void closeDocument(OpenDoc* d)
{
    if (!d) return;
    fz_context* ctx = CTX();
    if (ctx && d->doc) fz_drop_document(ctx, d->doc);
    delete d;
}

bool openDocValid(OpenDoc* d) { return d && d->doc; }

BitmapData renderPageDoc(OpenDoc* o, int pageIndex, int maxPixels, bool annotations)
{
    BitmapData out;
    fz_context* ctx = CTX();
    if (!ctx || !o || !o->doc) return out;
    fz_document* doc = o->doc;
    fz_page* page = nullptr;
    fz_pixmap* pix = nullptr;
    fz_device* dev = nullptr;
    int w = 0, h = 0;
    unsigned char* samples = nullptr;
    int stride = 0, n = 0, alpha = 0;

    fz_var(page); fz_var(pix); fz_var(dev);
    fz_var(w); fz_var(h); fz_var(samples); fz_var(stride); fz_var(n); fz_var(alpha);
    fz_try(ctx)
    {
        int count = fz_count_pages(ctx, doc);
        if (pageIndex < 0 || pageIndex >= count) fz_throw(ctx, FZ_ERROR_GENERIC, "page out of range");
        page = fz_load_page(ctx, doc, pageIndex);
        fz_rect r = fz_bound_page(ctx, page);
        double rw = std::max(1.0, (double)(r.x1 - r.x0));
        double rh = std::max(1.0, (double)(r.y1 - r.y0));
        // maxPixels is the wanted LONG EDGE in pixels; hard cap at 2000px
        double scale = (double)maxPixels / std::max(rw, rh);
        double cap = 2000.0 / std::max(rw, rh);
        if (scale > cap) scale = cap;
        if (scale < 0.02) scale = 0.02;
        if (scale > 4.0) scale = 4.0;

        fz_matrix ctm = fz_scale((float)scale, (float)scale);
        fz_irect box = fz_round_rect(fz_transform_rect(r, ctm));
        w = box.x1 - box.x0; h = box.y1 - box.y0;
        if (w <= 0 || h <= 0) fz_throw(ctx, FZ_ERROR_GENERIC, "empty page");

        pix = fz_new_pixmap_with_bbox(ctx, fz_device_rgb(ctx), box, nullptr, 0);
        fz_clear_pixmap_with_value(ctx, pix, 255);
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        if (annotations) fz_run_page(ctx, page, dev, ctm, nullptr);
        else fz_run_page_contents(ctx, page, dev, ctm, nullptr);
        fz_close_device(ctx, dev);

        samples = pix->samples; stride = pix->stride; n = pix->n; alpha = pix->alpha;
        out.width = w; out.height = h;
        out.pixels.resize((size_t)w * h * 4);
        unsigned char* dst = out.pixels.data();
        for (int y = 0; y < h; y++) {
            const unsigned char* src = samples + (size_t)y * stride;
            unsigned char* d = dst + (size_t)y * w * 4;
            for (int x = 0; x < w; x++) {
                d[x * 4 + 0] = src[x * n + 2];  // B
                d[x * 4 + 1] = src[x * n + 1];  // G
                d[x * 4 + 2] = src[x * n + 0];  // R
                d[x * 4 + 3] = alpha ? src[x * n + 3] : 255;
            }
        }
        out.valid = true;
    }
    fz_catch(ctx) { out.valid = false; out.pixels.clear(); }

    if (dev) fz_drop_device(ctx, dev);
    if (pix) fz_drop_pixmap(ctx, pix);
    if (page) fz_drop_page(ctx, page);
    return out;
}

BitmapData renderPage(const std::wstring& path, const std::wstring& password, int pageIndex,
                      int maxPixels, bool annotations)
{
    OpenDoc* o = openDocument(path, password);
    if (!o) return BitmapData();
    BitmapData bd = renderPageDoc(o, pageIndex, maxPixels, annotations);
    closeDocument(o);
    return bd;
}

// ---------------------------------------------------------------------
// shared helpers
// ---------------------------------------------------------------------
namespace {

void savePdf(fz_context* ctx, pdf_document* doc, const std::string& outPath, int garbage, bool compress)
{
    pdf_write_options opts = pdf_default_write_options;
    opts.do_garbage = garbage;
    opts.do_compress = compress ? 1 : 0;
    opts.do_compress_images = compress ? 1 : 0;
    opts.do_compress_fonts = compress ? 1 : 0;
    opts.do_clean = 1;
    opts.do_pretty = 0;
    opts.compression_effort = 60;
    opts.do_use_objstms = 1;
    pdf_save_document(ctx, doc, outPath.c_str(), &opts);
}

// Save a subset of pages (0 based, in the given order) to outPath.
Result saveSubset(pdf_document* src, const std::vector<int>& pages0, const std::wstring& outPath,
                  JobContext& jc, const wchar_t* stepPrefix)
{
    fz_context* ctx = CTX();
    Result res;
    std::string op = u8(outPath);
    pdf_document* dst = nullptr;
    pdf_graft_map* map = nullptr;
    std::string msg;
    bool ok = false;
    int total = (int)pages0.size();

    fz_var(dst); fz_var(map); fz_var(ok);
    fz_try(ctx)
    {
        dst = pdf_create_document(ctx);
        map = pdf_new_graft_map(ctx, dst);
        int n = 0;
        for (int p : pages0) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            pdf_graft_mapped_page(ctx, map, n++, src, p);
            if (total > 0 && (n % 4 == 0 || n == total))
                jc.report(n * 100 / total, util::fmt(L"%s%d / %d pages", stepPrefix ? stepPrefix : L"", n, total));
        }
        savePdf(ctx, dst, op, 4, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }

    if (map) pdf_drop_graft_map(ctx, map);
    if (dst) pdf_drop_document(ctx, dst);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        res.error = util::toWide(msg);
        return res;
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.pages = total;
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

pdf_document* openPdf(fz_context* ctx, const std::string& path, const std::string& password, std::string& msg)
{
    pdf_document* doc = nullptr;
    fz_try(ctx)
    {
        fz_document* d = fz_open_document(ctx, path.c_str());
        if (fz_needs_password(ctx, d)) {
            if (password.empty() || !fz_authenticate_password(ctx, d, password.c_str())) {
                fz_drop_document(ctx, d);
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
            }
        }
        doc = pdf_specifics(ctx, d);
        if (!doc) { fz_drop_document(ctx, d); fz_throw(ctx, FZ_ERROR_GENERIC, "not a pdf"); }
        // keep the fz_document alive through the pdf_document pointer
        pdf_keep_document(ctx, doc);
        fz_drop_document(ctx, d);
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    return doc;
}

} // namespace

// ---------------------------------------------------------------------
// MERGE
// ---------------------------------------------------------------------
Result mergeDocuments(const std::vector<MergeItem>& files, const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    Result res;
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    if (files.empty()) return Result::failure(L"Add at least one PDF to merge.");

    // count total pages for progress
    int totalPages = 0;
    {
        std::vector<int> counts(files.size(), 0);
        for (size_t i = 0; i < files.size(); i++) {
            DocInfo info; std::wstring e;
            if (readInfo(files[i].path, files[i].password, info, e)) counts[i] = info.pageCount;
            totalPages += counts[i];
        }
        if (totalPages == 0) totalPages = 1;
        (void)counts;
    }

    std::string op = u8(outPath);
    pdf_document* dst = nullptr;
    std::string msg;
    bool ok = false;
    int done = 0;
    for (size_t i = 0; i < files.size(); i++) {
        if (jc.stop()) { res.error = L"cancelled"; return res; }
        std::string sp = u8(files[i].path), spw = u8(files[i].password);
        pdf_document* src = nullptr;
        pdf_graft_map* map = nullptr;
        fz_var(src); fz_var(map); fz_var(dst); fz_var(ok); fz_var(done);
        fz_try(ctx)
        {
            if (!dst) dst = pdf_create_document(ctx);
            src = openPdf(ctx, sp, spw, msg);
            if (!src) fz_throw(ctx, FZ_ERROR_GENERIC, "open failed: %s", sp.c_str());
            map = pdf_new_graft_map(ctx, dst);
            int n = pdf_count_pages(ctx, src);
            for (int p = 0; p < n; p++) {
                if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
                pdf_graft_mapped_page(ctx, map, done++, src, p);
                jc.report(done * 100 / totalPages,
                          util::fmt(L"Merging %s (%d of %d pages)",
                                    util::fileName(files[i].path).c_str(), p + 1, n));
            }
            ok = true;
        }
        fz_catch(ctx) { msg = u8(caughtError(ctx)); ok = false; }
        if (map) pdf_drop_graft_map(ctx, map);
        if (src) pdf_drop_document(ctx, src);
        if (!ok) {
            if (dst) { pdf_drop_document(ctx, dst); dst = nullptr; }
            if (msg == "cancelled") { res.error = L"cancelled"; return res; }
            res.error = util::fmt(L"%s\n\n%s", util::fileName(files[i].path).c_str(),
                                  util::toWide(msg).c_str());
            return res;
        }
    }

    msg.clear(); ok = false;
    fz_var(dst); fz_var(ok);
    fz_try(ctx)
    {
        jc.report(97, L"Writing merged PDF");
        savePdf(ctx, dst, op, 4, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (dst) pdf_drop_document(ctx, dst);

    if (!ok) {
        if (msg == "cancelled") { res.error = L"cancelled"; return res; }
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.pages = done;
    res.bytesAfter = util::fileSize(outPath);
    for (auto& f : files) res.bytesBefore += util::fileSize(f.path);
    return res;
}

// ---------------------------------------------------------------------
// SELECT / REMOVE PAGES
// ---------------------------------------------------------------------
Result selectPages(const std::wstring& path, const std::wstring& password,
                   const std::vector<int>& pages0, bool keepOnly,
                   const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), msg;
    pdf_document* src = openPdf(ctx, sp, pw, msg);
    if (!src) return Result::failure(util::toWide(msg));

    int n = pdf_count_pages(ctx, src);
    std::vector<int> keep;
    keep.reserve(n);
    if (keepOnly) {
        for (int p : pages0) if (p >= 0 && p < n) keep.push_back(p);
    } else {
        std::vector<char> drop(n, 0);
        for (int p : pages0) if (p >= 0 && p < n) drop[p] = 1;
        for (int i = 0; i < n; i++) if (!drop[i]) keep.push_back(i);
    }
    if (keep.empty()) { pdf_drop_document(ctx, src); return Result::failure(L"No pages left to save."); }

    Result r = saveSubset(src, keep, outPath, jc, keepOnly ? L"Extracting " : L"Removing ");
    pdf_drop_document(ctx, src);
    r.bytesBefore = util::fileSize(path);
    return r;
}

// ---------------------------------------------------------------------
// REORDER + per page rotation
// ---------------------------------------------------------------------
Result reorderPages(const std::wstring& path, const std::wstring& password,
                    const std::vector<int>& newOrder, const std::vector<int>& rotations,
                    const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    pdf_document* src = nullptr;
    bool ok = false;
    Result res;

    fz_var(src); fz_var(ok);
    fz_try(ctx)
    {
        src = openPdf(ctx, sp, pw, msg);
        if (!src) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());
        int n = pdf_count_pages(ctx, src);
        std::vector<int> order;
        order.reserve(newOrder.size());
        for (int p : newOrder) if (p >= 0 && p < n) order.push_back(p);
        if (order.empty()) fz_throw(ctx, FZ_ERROR_GENERIC, "no pages");
        if (order.size() != (size_t)n) {
            // pages were deleted in the organiser: build the doc from the kept order
            fz_throw(ctx, FZ_ERROR_GENERIC, "subset");
        }
        jc.report(20, L"Reordering pages");
        pdf_rearrange_pages(ctx, src, (int)order.size(), order.data(), PDF_CLEAN_STRUCTURE_KEEP);

        for (size_t i = 0; i < rotations.size() && i < order.size(); i++) {
            int deg = ((rotations[i] % 360) + 360) % 360;
            pdf_obj* pageobj = pdf_lookup_page_obj(ctx, src, (int)i);
            if (pageobj) pdf_dict_put_int(ctx, pageobj, PDF_NAME(Rotate), deg);
        }
        jc.report(70, L"Saving");
        savePdf(ctx, src, op, 4, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }

    std::vector<int> subsetOrder;
    if (!ok && msg == "subset") {
        // fall back to grafting a subset in the new order
        int n = pdf_count_pages(ctx, src);
        for (int p : newOrder) if (p >= 0 && p < n) subsetOrder.push_back(p);
        msg.clear();
        res = saveSubset(src, subsetOrder, outPath, jc, L"Saving ");
        // apply rotations on the produced file
        if (res.ok && !rotations.empty()) {
            pdf_document* out = nullptr;
            std::string m2;
            out = openPdf(ctx, op, "", m2);
            if (out) {
                bool ok2 = false;
                fz_var(out); fz_var(ok2);
                fz_try(ctx)
                {
                    int n2 = pdf_count_pages(ctx, out);
                    for (int i = 0; i < n2 && i < (int)rotations.size(); i++) {
                        int deg = ((rotations[i] % 360) + 360) % 360;
                        pdf_obj* po = pdf_lookup_page_obj(ctx, out, i);
                        if (po) pdf_dict_put_int(ctx, po, PDF_NAME(Rotate), deg);
                    }
                    savePdf(ctx, out, op + ".tmp", 4, true);
                    ok2 = true;
                }
                fz_catch(ctx) { m2 = u8(caughtError(ctx)); }
                pdf_drop_document(ctx, out);
                if (ok2) {
                    // swap the rotated copy into place
                    DeleteFileW(outPath.c_str());
                    MoveFileExW((outPath + L".tmp").c_str(), outPath.c_str(),
                                MOVEFILE_REPLACE_EXISTING);
                } else {
                    DeleteFileW((outPath + L".tmp").c_str());
                }
            }
        }
        if (src) pdf_drop_document(ctx, src);
        res.bytesBefore = util::fileSize(path);
        return res;
    }

    if (src) pdf_drop_document(ctx, src);
    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.pages = (int)newOrder.size();
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

// ---------------------------------------------------------------------
// ROTATE
// ---------------------------------------------------------------------
Result rotatePages(const std::wstring& path, const std::wstring& password,
                   const std::vector<int>& pages0, int degrees,
                   const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    pdf_document* doc = nullptr;
    bool ok = false;
    Result res;

    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = openPdf(ctx, sp, pw, msg);
        if (!doc) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());
        int n = pdf_count_pages(ctx, doc);
        std::vector<char> sel(n, 0);
        if (pages0.empty()) { for (int i = 0; i < n; i++) sel[i] = 1; }
        else for (int p : pages0) if (p >= 0 && p < n) sel[p] = 1;

        int done = 0;
        for (int i = 0; i < n; i++) {
            if (!sel[i]) continue;
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            pdf_obj* po = pdf_lookup_page_obj(ctx, doc, i);
            if (!po) continue;
            int cur = (int)pdf_dict_get_int(ctx, po, PDF_NAME(Rotate));
            int next = ((cur + degrees) % 360 + 360) % 360;
            pdf_dict_put_int(ctx, po, PDF_NAME(Rotate), next);
            done++;
            jc.report(done * 80 / std::max(1, (int)pages0.size() ? (int)pages0.size() : n),
                      util::fmt(L"Rotating page %d", i + 1));
        }
        jc.report(90, L"Saving");
        savePdf(ctx, doc, op, 3, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) pdf_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

// ---------------------------------------------------------------------
// SPLIT
// ---------------------------------------------------------------------
Result splitDocument(const std::wstring& path, const std::wstring& password,
                     const SplitOptions& opt, const std::wstring& outDir,
                     const std::wstring& baseName, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), msg;
    pdf_document* src = openPdf(ctx, sp, pw, msg);
    if (!src) return Result::failure(util::toWide(msg));

    int n = pdf_count_pages(ctx, src);
    std::vector<std::vector<int>> groups;

    switch (opt.mode) {
    case SplitMode::Ranges: {
        // each comma separated chunk becomes one output file
        for (auto part : util::split(opt.ranges, L',')) {
            std::vector<int> g = parsePageRanges(part, n);
            if (!g.empty()) groups.push_back(g);
        }
        if (groups.empty()) {
            pdf_drop_document(ctx, src);
            return Result::failure(L"Enter a valid page range, for example 1-3, 4-6.");
        }
        break;
    }
    case SplitMode::EveryN: {
        int step = std::max(1, opt.everyN);
        for (int i = 0; i < n; i += step) {
            std::vector<int> g;
            for (int j = i; j < std::min(n, i + step); j++) g.push_back(j);
            if (!g.empty()) groups.push_back(g);
        }
        break;
    }
    case SplitMode::OnePerFile:
        for (int i = 0; i < n; i++) groups.push_back({ i });
        break;
    case SplitMode::ExtractOdd: {
        std::vector<int> g;
        for (int i = 0; i < n; i += 2) g.push_back(i);
        if (!g.empty()) groups.push_back(g);
        break;
    }
    case SplitMode::ExtractEven: {
        std::vector<int> g;
        for (int i = 1; i < n; i += 2) g.push_back(i);
        if (!g.empty()) groups.push_back(g);
        break;
    }
    }

    if (groups.empty()) { pdf_drop_document(ctx, src); return Result::failure(L"Nothing to split."); }

    Result res;
    res.bytesBefore = util::fileSize(path);
    int idx = 0;
    for (auto& g : groups) {
        if (jc.stop()) { pdf_drop_document(ctx, src); return Result::failure(L"cancelled"); }
        idx++;
        std::wstring name = groups.size() == 1
            ? util::fmt(L"%s.pdf", baseName.c_str())
            : util::fmt(L"%s - part %d.pdf", baseName.c_str(), idx);
        std::wstring full = util::uniquePath(util::joinPath(outDir, name));
        Result r = saveSubset(src, g, full, jc, L"Splitting ");
        if (!r.ok) { pdf_drop_document(ctx, src); return r; }
        res.outputs.push_back(full);
        res.pages += (int)g.size();
        res.bytesAfter += r.bytesAfter;
        jc.report(idx * 100 / (int)groups.size(), util::fmt(L"Wrote %s", util::fileName(full).c_str()));
    }
    pdf_drop_document(ctx, src);
    res.ok = true;
    return res;
}

// ---------------------------------------------------------------------
// COMPRESS
// ---------------------------------------------------------------------
Result compressDocument(const std::wstring& path, const std::wstring& password,
                        CompressLevel level, const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");

    Result res;
    res.bytesBefore = util::fileSize(path);
    std::string sp = u8(path), op = u8(outPath), pw = u8(password);
    std::string msg;
    bool ok = false;

    static char qLow[] = "90", qMed[] = "78", qHigh[] = "62", qMax[] = "45";

    pdf_clean_options opts{};
    opts.write = pdf_default_write_options;
    opts.write.do_garbage = 4;
    opts.write.do_compress = 1;
    opts.write.do_compress_images = 1;
    opts.write.do_compress_fonts = 1;
    opts.write.do_clean = 1;
    opts.write.do_use_objstms = 1;
    opts.write.compression_effort = 100;
    opts.structure = PDF_CLEAN_STRUCTURE_DROP;

    int thr = 0, to = 0, losslessThr = 0;
    char* quality = qMed;
    int lossyMethod = FZ_RECOMPRESS_SAME;
    int losslessMethod = FZ_RECOMPRESS_SAME;
    int subset = 0;

    switch (level) {
    case CompressLevel::Low:
        subset = 1; lossyMethod = FZ_RECOMPRESS_JPEG; quality = qLow; thr = 300; to = 250;
        losslessMethod = FZ_RECOMPRESS_LOSSLESS; losslessThr = 0;
        break;
    case CompressLevel::Medium:
        subset = 1; lossyMethod = FZ_RECOMPRESS_JPEG; quality = qMed; thr = 200; to = 150;
        losslessMethod = FZ_RECOMPRESS_JPEG; losslessThr = 300;
        break;
    case CompressLevel::High:
        subset = 1; lossyMethod = FZ_RECOMPRESS_JPEG; quality = qHigh; thr = 150; to = 110;
        losslessMethod = FZ_RECOMPRESS_JPEG; losslessThr = 200;
        break;
    case CompressLevel::Maximum:
        subset = 1; lossyMethod = FZ_RECOMPRESS_JPEG; quality = qMax; thr = 100; to = 72;
        losslessMethod = FZ_RECOMPRESS_JPEG; losslessThr = 150;
        break;
    }
    opts.subset_fonts = subset;

    pdf_image_rewriter_options& im = opts.image;
    im.color_lossy_image_subsample_method = FZ_SUBSAMPLE_AVERAGE;
    im.color_lossless_image_subsample_method = FZ_SUBSAMPLE_AVERAGE;
    im.gray_lossy_image_subsample_method = FZ_SUBSAMPLE_AVERAGE;
    im.gray_lossless_image_subsample_method = FZ_SUBSAMPLE_AVERAGE;
    im.bitonal_image_subsample_method = FZ_SUBSAMPLE_AVERAGE;
    im.color_lossy_image_subsample_threshold = thr;   im.color_lossy_image_subsample_to = to;
    im.color_lossless_image_subsample_threshold = losslessThr; im.color_lossless_image_subsample_to = losslessThr ? to : 0;
    im.gray_lossy_image_subsample_threshold = thr;    im.gray_lossy_image_subsample_to = to;
    im.gray_lossless_image_subsample_threshold = losslessThr;  im.gray_lossless_image_subsample_to = losslessThr ? to : 0;
    im.bitonal_image_subsample_threshold = 0;         im.bitonal_image_subsample_to = 0;
    im.color_lossy_image_recompress_method = lossyMethod;
    im.color_lossless_image_recompress_method = losslessMethod;
    im.gray_lossy_image_recompress_method = lossyMethod;
    im.gray_lossless_image_recompress_method = losslessMethod;
    im.bitonal_image_recompress_method = FZ_RECOMPRESS_SAME;
    im.color_lossy_image_recompress_quality = quality;
    im.color_lossless_image_recompress_quality = quality;
    im.gray_lossy_image_recompress_quality = quality;
    im.gray_lossless_image_recompress_quality = quality;

    jc.report(15, L"Analysing document");
    fz_var(ok);
    fz_try(ctx)
    {
        jc.report(45, L"Rewriting images and streams");
        pdf_clean_file(ctx, (char*)sp.c_str(), (char*)op.c_str(), pw.empty() ? nullptr : (char*)pw.c_str(),
                       &opts, 0, nullptr);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        // Second chance: if the aggressive rewrite failed, retry with a plain clean save.
        std::string msg2;
        bool ok2 = false;
        pdf_document* doc = openPdf(ctx, sp, pw, msg2);
        if (doc) {
            fz_var(doc); fz_var(ok2);
            fz_try(ctx) { savePdf(ctx, doc, op, 4, true); ok2 = true; }
            fz_catch(ctx) { msg2 = u8(caughtError(ctx)); }
            pdf_drop_document(ctx, doc);
        }
        if (!ok2) return Result::failure(util::toWide(msg.empty() ? msg2 : msg));
    }

    jc.report(95, L"Finished");
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesAfter = util::fileSize(outPath);
    DocInfo di; std::wstring e;
    if (readInfo(outPath, L"", di, e)) res.pages = di.pageCount;
    return res;
}

// ---------------------------------------------------------------------
// PROTECT / UNLOCK
// ---------------------------------------------------------------------
Result protectDocument(const std::wstring& path, const std::wstring& password,
                       const ProtectOptions& opt, const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    if (opt.userPassword.empty() && opt.ownerPassword.empty())
        return Result::failure(L"Set at least one password.");

    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    std::string up = u8(opt.userPassword), opw = u8(opt.ownerPassword);
    pdf_document* doc = nullptr;
    bool ok = false;
    Result res;

    jc.report(20, L"Encrypting document");
    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = openPdf(ctx, sp, pw, msg);
        if (!doc) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());

        int perms = PDF_PERM_PRINT_HQ | PDF_PERM_ACCESSIBILITY;
        if (opt.allowPrinting) perms |= PDF_PERM_PRINT;
        if (opt.allowCopying)  perms |= PDF_PERM_COPY;
        if (opt.allowEditing)  perms |= PDF_PERM_MODIFY | PDF_PERM_ASSEMBLE;
        perms |= PDF_PERM_ANNOTATE | PDF_PERM_FORM;

        pdf_write_options wo = pdf_default_write_options;
        wo.do_garbage = 3;
        wo.do_compress = 1;
        wo.do_encrypt = opt.use256 ? PDF_ENCRYPT_AES_256 : PDF_ENCRYPT_RC4_128;
        wo.permissions = perms;
        strncpy(wo.upwd_utf8, up.c_str(), sizeof(wo.upwd_utf8) - 1);
        strncpy(wo.opwd_utf8, opw.empty() ? up.c_str() : opw.c_str(), sizeof(wo.opwd_utf8) - 1);

        jc.report(60, L"Writing protected PDF");
        pdf_save_document(ctx, doc, op.c_str(), &wo);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) pdf_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

Result unlockDocument(const std::wstring& path, const std::wstring& password,
                      const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    if (password.empty()) return Result::failure(L"Enter the password for this PDF.");

    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    pdf_document* doc = nullptr;
    bool ok = false;
    Result res;

    jc.report(25, L"Checking password");
    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = openPdf(ctx, sp, pw, msg);
        if (!doc) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());

        pdf_write_options wo = pdf_default_write_options;
        wo.do_garbage = 3;
        wo.do_compress = 1;
        wo.do_encrypt = PDF_ENCRYPT_NONE;
        wo.do_clean = 1;
        jc.report(60, L"Writing unprotected copy");
        pdf_save_document(ctx, doc, op.c_str(), &wo);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) pdf_drop_document(ctx, doc);

    if (!ok) {
        std::wstring m = util::toWide(msg);
        if (msg.find("password") != std::string::npos || msg.find("needs password") != std::string::npos)
            return Result::failure(L"That password is not correct.");
        return Result::failure(m);
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    DocInfo di; std::wstring e;
    if (readInfo(outPath, L"", di, e)) res.pages = di.pageCount;
    return res;
}

// ---------------------------------------------------------------------
// REPAIR  (rebuild the file structure from scratch)
// ---------------------------------------------------------------------
Result repairDocument(const std::wstring& path, const std::wstring& password,
                      const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    pdf_document* doc = nullptr;
    bool ok = false;
    Result res;

    jc.report(20, L"Rebuilding cross reference table");
    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = openPdf(ctx, sp, pw, msg);
        if (!doc) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());

        pdf_write_options wo = pdf_default_write_options;
        wo.do_garbage = 4;
        wo.do_compress = 1;
        wo.do_compress_images = 1;
        wo.do_compress_fonts = 1;
        wo.do_clean = 1;
        wo.do_sanitize = 1;
        wo.do_pretty = 1;
        wo.do_use_objstms = 0;
        wo.compression_effort = 100;
        jc.report(60, L"Writing repaired PDF");
        pdf_save_document(ctx, doc, op.c_str(), &wo);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) pdf_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    DocInfo di; std::wstring e;
    if (readInfo(outPath, L"", di, e)) res.pages = di.pageCount;
    return res;
}

// ---------------------------------------------------------------------
// FLATTEN  (bake annotations, form fields and stamps into page content)
// ---------------------------------------------------------------------
Result flattenDocument(const std::wstring& path, const std::wstring& password,
                       const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    fz_document* srcDoc = nullptr;
    pdf_document* dst = nullptr;
    bool ok = false;
    Result res;
    int total = 0;

    fz_var(srcDoc); fz_var(dst); fz_var(ok); fz_var(total);
    fz_try(ctx)
    {
        srcDoc = fz_open_document(ctx, sp.c_str());
        if (fz_needs_password(ctx, srcDoc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, srcDoc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        total = fz_count_pages(ctx, srcDoc);
        dst = pdf_create_document(ctx);

        // carry the metadata across
        char mbuf[2048];
        const char* keys[] = { "info:Title", "info:Author", "info:Subject", "info:Keywords",
                               "info:Creator", "info:CreationDate", "info:ModDate" };
        for (const char* k : keys) {
            mbuf[0] = 0;
            if (fz_lookup_metadata(ctx, srcDoc, k, mbuf, sizeof mbuf) >= 0 && mbuf[0])
                fz_set_metadata(ctx, &dst->super, k, mbuf);
        }

        fz_cookie cookie{};
        for (int i = 0; i < total; i++) {
            if (jc.stop()) { cookie.abort = 1; fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled"); }
            fz_page* sp2 = fz_load_page(ctx, srcDoc, i);
            if (!sp2) continue;
            fz_rect box = fz_bound_page(ctx, sp2);

            pdf_obj* pres = nullptr;
            fz_buffer* pcontents = nullptr;
            fz_device* dev = pdf_page_write(ctx, dst, box, &pres, &pcontents);
            fz_try(ctx)
            {
                fz_run_page(ctx, sp2, dev, fz_identity, &cookie);
                fz_close_device(ctx, dev);
            }
            fz_always(ctx) { fz_drop_device(ctx, dev); }
            fz_catch(ctx) { fz_drop_page(ctx, sp2); fz_rethrow(ctx); }

            pdf_obj* pageobj = pdf_add_page(ctx, dst, box, 0, pres, pcontents);
            pdf_insert_page(ctx, dst, i, pageobj);
            pdf_drop_obj(ctx, pageobj);
            if (pres) pdf_drop_obj(ctx, pres);
            if (pcontents) fz_drop_buffer(ctx, pcontents);
            fz_drop_page(ctx, sp2);
            jc.report((i + 1) * 85 / std::max(1, total), util::fmt(L"Flattening page %d of %d", i + 1, total));
        }
        jc.report(90, L"Saving flattened PDF");
        savePdf(ctx, dst, op, 4, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }

    if (dst) pdf_drop_document(ctx, dst);
    if (srcDoc) fz_drop_document(ctx, srcDoc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.pages = total;
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

// ---------------------------------------------------------------------
// STAMP  (watermark / page numbers) - appended as a Form XObject so the
// original page content, links, forms and annotations stay untouched.
// ---------------------------------------------------------------------
namespace {

// find a resource name that is not used yet
void uniqueResName(fz_context* ctx, pdf_obj* dict, const char* prefix, char* out, size_t outLen)
{
    for (int i = 0; i < 100000; i++) {
        snprintf(out, outLen, "%s%d", prefix, i);
        if (!pdf_dict_gets(ctx, dict, out)) return;
    }
    snprintf(out, outLen, "%sZ", prefix);
}

fz_font* loadStampFont(fz_context* ctx, const std::string& requested, std::string& usedPath)
{
    std::vector<std::wstring> cands;
    if (!requested.empty()) cands.push_back(util::toWide(requested));
    std::wstring fd = util::systemFontDir();
    const wchar_t* names[] = { L"segoeui.ttf", L"arial.ttf", L"calibri.ttf", L"times.ttf", L"segoeuib.ttf" };
    for (auto n : names) cands.push_back(util::joinPath(fd, n));

    for (auto& c : cands) {
        if (!util::fileExists(c)) continue;
        std::string u = util::toUtf8(c);
        fz_font* f = nullptr;
        fz_var(f);
        fz_try(ctx) { f = fz_new_font_from_file(ctx, "MrDocxStamp", u.c_str(), 0, 0); }
        fz_catch(ctx) { f = nullptr; }
        if (f) { usedPath = u; return f; }
    }
    fz_font* f = nullptr;
    fz_var(f);
    fz_try(ctx) { f = fz_new_base14_font(ctx, "helv"); }
    fz_catch(ctx) { f = nullptr; }
    usedPath = "";
    return f;
}

} // namespace

Result stampDocument(const std::wstring& path, const std::wstring& password,
                     const StampOptions& opt, const std::vector<int>& pages0,
                     const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    if (opt.kind == StampKind::Watermark && util::trim(opt.text).empty())
        return Result::failure(L"Enter the watermark text first.");

    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    std::string stampText = u8(opt.text);
    std::string fmtStr = u8(opt.numberFormat);
    std::string fontReq = u8(opt.fontFile), fontUsed;
    pdf_document* doc = nullptr;
    fz_font* font = nullptr;
    bool ok = false;
    int total = 0, donePages = 0;
    Result res;

    fz_var(doc); fz_var(font); fz_var(ok); fz_var(total); fz_var(donePages);
    fz_try(ctx)
    {
        doc = openPdf(ctx, sp, pw, msg);
        if (!doc) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());
        total = pdf_count_pages(ctx, doc);

        font = loadStampFont(ctx, fontReq, fontUsed);
        if (!font) fz_throw(ctx, FZ_ERROR_GENERIC, "no font available");

        std::vector<char> sel(total, 0);
        if (pages0.empty()) { for (int i = 0; i < total; i++) sel[i] = 1; }
        else for (int p : pages0) if (p >= 0 && p < total) sel[p] = 1;

        pdf_begin_operation(ctx, doc, "MrDocx stamp");

        int number = opt.pageNumStart;
        for (int i = 0; i < total; i++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            if (!sel[i]) { number++; continue; }

            pdf_obj* pageobj = pdf_lookup_page_obj(ctx, doc, i);
            if (!pageobj) { number++; continue; }

            fz_rect box; fz_matrix ctm;
            pdf_page_obj_transform(ctx, pageobj, &box, &ctm);
            float W = box.x1 - box.x0, H = box.y1 - box.y0;
            if (W <= 1 || H <= 1) { number++; continue; }

            // --- build the stamp content in fitz page space -------------
            pdf_obj* xres = pdf_add_new_dict(ctx, doc, 8);
            fz_buffer* buf = fz_new_buffer(ctx, 256);
            fz_device* dev = nullptr;
            fz_text* text = nullptr;
            bool closed = false;

            fz_var(dev); fz_var(text); fz_var(buf); fz_var(xres); fz_var(closed);
            fz_try(ctx)
            {
                dev = pdf_new_pdf_device(ctx, doc, ctm, xres, buf);

                char textBuf[256];
                const char* drawText = stampText.c_str();
                float size = (float)opt.fontSize;
                float r = opt.colorR / 255.f, g = opt.colorG / 255.f, b = opt.colorB / 255.f;
                float alpha = 1.0f;

                if (opt.kind == StampKind::PageNumber) {
                    // substitute {n} and {N}
                    std::string outStr;
                    char nb[32], Nb[32];
                    snprintf(nb, sizeof nb, "%d", number);
                    snprintf(Nb, sizeof Nb, "%d", total);
                    for (size_t k = 0; k < fmtStr.size(); k++) {
                        if (fmtStr[k] == '{' && k + 2 < fmtStr.size() && fmtStr[k + 2] == '}') {
                            char c = fmtStr[k + 1];
                            if (c == 'n') { outStr += nb; k += 2; continue; }
                            if (c == 'N') { outStr += Nb; k += 2; continue; }
                        }
                        outStr += fmtStr[k];
                    }
                    snprintf(textBuf, sizeof textBuf, "%s", outStr.c_str());
                    drawText = textBuf;
                    size = (float)opt.pageNumSize;
                    r = opt.pageNumColorR / 255.f; g = opt.pageNumColorG / 255.f; b = opt.pageNumColorB / 255.f;
                    alpha = util::clampv(opt.pageNumOpacity, 1, 100) / 100.f;
                } else {
                    snprintf(textBuf, sizeof textBuf, "%s", stampText.c_str());
                    drawText = textBuf;
                    alpha = util::clampv(opt.opacity, 1, 100) / 100.f;
                }

                text = fz_new_text(ctx);
                float cx, cy;
                fz_matrix m;
                if (opt.kind == StampKind::PageNumber) {
                    float margin = (float)opt.pageNumMargin;
                    int pos = util::clampv(opt.pageNumPos, 0, 8);
                    int col = pos % 3, row = pos / 3;
                    m = fz_scale(size, size);
                    fz_matrix adv = fz_measure_string(ctx, font, m, drawText, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
                    float tw = adv.e - m.e;
                    if (col == 0) cx = box.x0 + margin;
                    else if (col == 1) cx = box.x0 + W / 2 - tw / 2;
                    else cx = box.x1 - margin - tw;
                    if (row == 0) cy = box.y0 + margin + size * 0.8f;         // top
                    else if (row == 1) cy = box.y0 + H / 2 + size * 0.35f;    // middle
                    else cy = box.y1 - margin;                                // bottom
                    m = fz_concat(m, fz_translate(cx, cy));
                } else {
                    float deg = (float)opt.rotation;
                    m = fz_scale(size, size);
                    m = fz_concat(m, fz_rotate(-deg));
                    fz_matrix adv = fz_measure_string(ctx, font, m, drawText, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
                    float tw = adv.e - m.e, th = adv.f - m.f;
                    cx = box.x0 + W / 2 - tw / 2;
                    cy = box.y0 + H / 2 - th / 2 + size * 0.35f;
                    m = fz_concat(m, fz_translate(cx, cy));
                }

                fz_show_string(ctx, text, font, m, drawText, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);

                float col[3] = { r, g, b };
                fz_fill_text(ctx, dev, text, fz_identity, fz_device_rgb(ctx), col, alpha, fz_default_color_params);
                fz_drop_text(ctx, text); text = nullptr;
                fz_close_device(ctx, dev);
                closed = true;
            }
            fz_always(ctx)
            {
                if (text) fz_drop_text(ctx, text);
                if (dev && !closed) {
                    fz_try(ctx) { fz_close_device(ctx, dev); }
                    fz_catch(ctx) { /* the stamp failed; keep unwinding */ }
                }
                if (dev) fz_drop_device(ctx, dev);
            }
            fz_catch(ctx)
            {
                if (buf) fz_drop_buffer(ctx, buf);
                if (xres) pdf_drop_obj(ctx, xres);
                fz_rethrow(ctx);
            }

            // --- wrap it into a Form XObject ----------------------------
            fz_rect pdfBox = fz_transform_rect(box, ctm);
            pdf_obj* xdict = pdf_new_dict(ctx, doc, 6);
            pdf_dict_put_drop(ctx, xdict, PDF_NAME(Type), PDF_NAME(XObject));
            pdf_dict_put_drop(ctx, xdict, PDF_NAME(Subtype), PDF_NAME(Form));
            pdf_dict_put_drop(ctx, xdict, PDF_NAME(BBox), pdf_new_rect(ctx, doc, pdfBox));
            pdf_dict_put_drop(ctx, xdict, PDF_NAME(Resources), xres);   // consumes xres
            pdf_obj* xobj = pdf_add_stream(ctx, doc, buf, xdict, 1);
            fz_drop_buffer(ctx, buf);
            pdf_drop_obj(ctx, xdict);
            xres = nullptr; buf = nullptr;

            // --- hook the XObject into the page resources ---------------
            char nm[32];
            pdf_obj* res = pdf_dict_get_inheritable(ctx, pageobj, PDF_NAME(Resources));
            pdf_obj* newres = (res && pdf_is_dict(ctx, res)) ? pdf_copy_dict(ctx, res)
                                                             : pdf_new_dict(ctx, doc, 4);
            pdf_obj* xo = pdf_dict_get(ctx, newres, PDF_NAME(XObject));
            if (xo && pdf_is_dict(ctx, xo)) {
                pdf_obj* xoCopy = pdf_copy_dict(ctx, xo);
                pdf_dict_put_drop(ctx, newres, PDF_NAME(XObject), xoCopy);
                xo = xoCopy;
            } else {
                xo = pdf_new_dict(ctx, doc, 4);
                pdf_dict_put_drop(ctx, newres, PDF_NAME(XObject), xo);
            }
            uniqueResName(ctx, xo, "PFx", nm, sizeof nm);
            pdf_dict_puts_drop(ctx, xo, nm, xobj);      // consumes xobj
            pdf_dict_put_drop(ctx, pageobj, PDF_NAME(Resources), pdf_add_object_drop(ctx, doc, newres));

            // --- append (or prepend) the page content -------------------
            fz_buffer* pbuf = fz_new_buffer(ctx, 64);
            fz_append_printf(ctx, pbuf, "q /%s Do Q\n", nm);
            pdf_obj* pstm = pdf_add_stream(ctx, doc, pbuf, nullptr, 0);
            fz_drop_buffer(ctx, pbuf);

            pdf_obj* contents = pdf_dict_get(ctx, pageobj, PDF_NAME(Contents));
            if (!contents) {
                pdf_dict_put_drop(ctx, pageobj, PDF_NAME(Contents), pstm);
            } else if (pdf_is_array(ctx, contents)) {
                if (opt.behind) pdf_array_insert_drop(ctx, contents, pstm, 0);
                else pdf_array_push_drop(ctx, contents, pstm);
            } else {
                pdf_obj* arr = pdf_add_new_array(ctx, doc, 2);
                if (opt.behind) { pdf_array_push_drop(ctx, arr, pstm); pdf_array_push(ctx, arr, contents); }
                else { pdf_array_push(ctx, arr, contents); pdf_array_push_drop(ctx, arr, pstm); }
                pdf_dict_put_drop(ctx, pageobj, PDF_NAME(Contents), arr);
            }

            donePages++;
            number++;
            jc.report(donePages * 85 / std::max(1, (int)pages0.size() ? (int)pages0.size() : total),
                      util::fmt(L"Stamping page %d", i + 1));
        }

        jc.report(92, L"Saving");
        pdf_end_operation(ctx, doc);
        savePdf(ctx, doc, op, 2, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }

    if (font) fz_drop_font(ctx, font);
    if (doc) pdf_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.pages = donePages;
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

// ---------------------------------------------------------------------
// PDF -> IMAGES
// ---------------------------------------------------------------------
Result exportImages(const std::wstring& path, const std::wstring& password,
                    const ImageExportOptions& opt, const std::vector<int>& pages0,
                    const std::wstring& outDir, const std::wstring& baseName, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), msg, sdir = u8(outDir);
    bool jpg = (util::lower(opt.format) == L"jpg" || util::lower(opt.format) == L"jpeg");
    std::string ext = jpg ? ".jpg" : ".png";
    fz_document* doc = nullptr;
    bool ok = false;
    int total = 0, written = 0;
    Result res;

    fz_var(doc); fz_var(ok); fz_var(total); fz_var(written);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, sp.c_str());
        if (fz_needs_password(ctx, doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, doc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        int n = fz_count_pages(ctx, doc);
        std::vector<int> pages = pages0.empty() ? std::vector<int>() : pages0;
        if (pages.empty()) for (int i = 0; i < n; i++) pages.push_back(i);
        total = (int)pages.size();

        float dpi = (float)util::clampv(opt.dpi, 36, 1200);
        fz_matrix ctm = fz_scale(dpi / 72.0f, dpi / 72.0f);

        int idx = 0;
        for (int p : pages) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            if (p < 0 || p >= n) continue;
            idx++;
            char nameBuf[1024];
            snprintf(nameBuf, sizeof nameBuf, "%s/%s-%02d%s", sdir.c_str(),
                     u8(baseName).c_str(), idx, ext.c_str());
            std::string outU8 = nameBuf;

            fz_page* page = fz_load_page(ctx, doc, p);
            fz_pixmap* pix = nullptr;
            fz_output* out = nullptr;
            fz_var(page); fz_var(pix); fz_var(out);
            fz_try(ctx)
            {
                pix = fz_new_pixmap_from_page(ctx, page, ctm, fz_device_rgb(ctx), 0);
                out = fz_new_output_with_path(ctx, outU8.c_str(), 0);
                if (jpg) fz_write_pixmap_as_jpeg(ctx, out, pix, util::clampv(opt.jpegQuality, 20, 100), 0);
                else fz_write_pixmap_as_png(ctx, out, pix);
                fz_close_output(ctx, out);
            }
            fz_always(ctx)
            {
                if (out) fz_drop_output(ctx, out);
                if (pix) fz_drop_pixmap(ctx, pix);
                fz_drop_page(ctx, page);
            }
            fz_catch(ctx) { fz_rethrow(ctx); }

            written++;
            res.outputs.push_back(util::toWide(outU8));
            res.bytesAfter += util::fileSize(util::toWide(outU8));
            jc.report(written * 95 / std::max(1, total), util::fmt(L"Rendering page %d of %d", idx, total));
        }
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) fz_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = !res.outputs.empty();
    if (!res.ok) return Result::failure(L"No pages were rendered.");
    res.pages = written;
    res.bytesBefore = util::fileSize(path);
    return res;
}

// ---------------------------------------------------------------------
// IMAGES -> PDF
// ---------------------------------------------------------------------
Result imagesToPdf(const std::vector<std::wstring>& imagePaths, const ImageToPdfOptions& opt,
                   const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    if (imagePaths.empty()) return Result::failure(L"Add at least one image.");

    std::string op = u8(outPath), msg;
    pdf_document* dst = nullptr;
    bool ok = false;
    int total = (int)imagePaths.size(), done = 0;
    Result res;

    fz_var(dst); fz_var(ok); fz_var(done);
    fz_try(ctx)
    {
        dst = pdf_create_document(ctx);
        pdf_begin_operation(ctx, dst, "MrDocx images to pdf");

        for (int i = 0; i < total; i++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            std::string ip = u8(imagePaths[i]);
            fz_image* img = fz_new_image_from_file(ctx, ip.c_str());
            fz_var(img);

            float iw = (float)img->w, ih = (float)img->h;
            float xres = img->xres > 0 ? (float)img->xres : 96.f;
            float yres = img->yres > 0 ? (float)img->yres : 96.f;
            float wpt = iw * 72.f / xres;
            float hpt = ih * 72.f / yres;

            float pw, ph;
            if (opt.pageSize == 1) { pw = 595; ph = 842; }        // A4
            else if (opt.pageSize == 2) { pw = 612; ph = 792; }    // Letter
            else { pw = wpt; ph = hpt; }
            if (opt.landscape && opt.pageSize != 0) std::swap(pw, ph);

            fz_rect box = fz_make_rect(0, 0, pw, ph);
            pdf_obj* pres = nullptr;
            fz_buffer* pcontents = nullptr;
            fz_device* dev = pdf_page_write(ctx, dst, box, &pres, &pcontents);

            fz_try(ctx)
            {
                float margin = (float)opt.marginPt;
                float availW = pw - margin * 2, availH = ph - margin * 2;
                if (availW < 10) availW = pw;
                if (availH < 10) availH = ph;
                float sc = std::min(availW / wpt, availH / hpt);
                if (opt.pageSize == 0) sc = 1.0f;
                if (sc > 8.f) sc = 8.f;
                float dw = wpt * sc, dh = hpt * sc;
                float x = margin + (availW - dw) / 2;
                float y = margin + (availH - dh) / 2;
                if (opt.pageSize == 0) { x = 0; y = 0; }
                // the image is drawn into the unit square, so scale it to dw x dh
                fz_matrix m = fz_concat(fz_scale(dw, dh), fz_translate(x, y));
                fz_fill_image(ctx, dev, img, m, 1.0f, fz_default_color_params);
                fz_close_device(ctx, dev);
            }
            fz_always(ctx) { fz_drop_device(ctx, dev); fz_drop_image(ctx, img); }
            fz_catch(ctx) {
                if (pres) pdf_drop_obj(ctx, pres);
                if (pcontents) fz_drop_buffer(ctx, pcontents);
                fz_rethrow(ctx);
            }

            pdf_obj* pageobj = pdf_add_page(ctx, dst, box, 0, pres, pcontents);
            pdf_insert_page(ctx, dst, i, pageobj);
            pdf_drop_obj(ctx, pageobj);
            if (pres) pdf_drop_obj(ctx, pres);
            if (pcontents) fz_drop_buffer(ctx, pcontents);

            done++;
            jc.report(done * 90 / total, util::fmt(L"Adding %s", util::fileName(imagePaths[i]).c_str()));
            res.bytesBefore += util::fileSize(imagePaths[i]);
        }
        pdf_end_operation(ctx, dst);
        jc.report(95, L"Writing PDF");
        savePdf(ctx, dst, op, 4, true);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (dst) pdf_drop_document(ctx, dst);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.pages = done;
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

// ---------------------------------------------------------------------
// TEXT / HTML export
// ---------------------------------------------------------------------
Result exportText(const std::wstring& path, const std::wstring& password,
                  const std::wstring& outPath, bool pageBreaks, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    fz_document* doc = nullptr;
    fz_output* out = nullptr;
    bool ok = false;
    int total = 0, chars = 0;
    Result res;

    fz_var(doc); fz_var(out); fz_var(ok); fz_var(total); fz_var(chars);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, sp.c_str());
        if (fz_needs_password(ctx, doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, doc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        total = fz_count_pages(ctx, doc);
        out = fz_new_output_with_path(ctx, op.c_str(), 0);
        fz_write_data(ctx, out, "\xEF\xBB\xBF", 3);   // UTF-8 BOM for Notepad

        fz_stext_options sopts{};
        sopts.flags = FZ_STEXT_PRESERVE_LIGATURES | FZ_STEXT_DEHYPHENATE;
        for (int i = 0; i < total; i++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            fz_page* page = fz_load_page(ctx, doc, i);
            fz_stext_page* tp = nullptr;
            fz_var(page); fz_var(tp);
            fz_try(ctx) { tp = fz_new_stext_page_from_page(ctx, page, &sopts); }
            fz_always(ctx) { fz_drop_page(ctx, page); }
            fz_catch(ctx) { if (tp) fz_drop_stext_page(ctx, tp); fz_rethrow(ctx); }

            if (tp) {
                if (pageBreaks && i > 0) fz_write_printf(ctx, out, "\n\n");
                if (pageBreaks) fz_write_printf(ctx, out, "--- page %d ---\n\n", i + 1);
                int64_t before = chars;
                fz_print_stext_page_as_text(ctx, out, tp);
                (void)before;
                fz_drop_stext_page(ctx, tp);
            }
            jc.report((i + 1) * 90 / std::max(1, total), util::fmt(L"Reading page %d of %d", i + 1, total));
        }
        fz_close_output(ctx, out);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (out) fz_drop_output(ctx, out);
    if (doc) fz_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesAfter = util::fileSize(outPath);
    res.bytesBefore = util::fileSize(path);
    res.pages = total;
    if (res.bytesAfter < 8)
        return Result::failure(L"No selectable text was found. This PDF is probably a scan - it has no text layer.");
    (void)chars;
    return res;
}

Result exportHtml(const std::wstring& path, const std::wstring& password,
                  const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    std::wstring title = util::baseName(path);
    std::string titleU8 = u8(title);
    fz_document* doc = nullptr;
    fz_output* out = nullptr;
    bool ok = false;
    int total = 0;
    Result res;

    fz_var(doc); fz_var(out); fz_var(ok); fz_var(total);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, sp.c_str());
        if (fz_needs_password(ctx, doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, doc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        total = fz_count_pages(ctx, doc);
        out = fz_new_output_with_path(ctx, op.c_str(), 0);
        fz_write_printf(ctx, out,
            "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
            "<title>%s</title>\n<style>\n"
            "body{font-family:'Segoe UI',system-ui,sans-serif;background:#f4f6fa;color:#141824;margin:0;padding:32px}\n"
            "header{max-width:820px;margin:0 auto 24px}\n"
            "h1{font-size:22px;margin:0 0 4px}\n"
            "p.sub{color:#5a6478;margin:0;font-size:13px}\n"
            "article{max-width:820px;margin:0 auto 20px;background:#fff;border:1px solid #e1e6ee;"
            "border-radius:10px;padding:32px 40px;box-shadow:0 1px 2px rgba(16,24,40,.04)}\n"
            "article h2{font-size:12px;color:#8b94a6;text-transform:uppercase;letter-spacing:.08em;margin:0 0 16px}\n"
            "</style>\n</head>\n<body>\n<header><h1>%s</h1>"
            "<p class=\"sub\">Extracted with MrDocx - %d pages</p></header>\n",
            titleU8.c_str(), titleU8.c_str(), total);

        for (int i = 0; i < total; i++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            fz_page* page = fz_load_page(ctx, doc, i);
            fz_stext_page* tp = nullptr;
            fz_var(page); fz_var(tp);
            fz_try(ctx) { tp = fz_new_stext_page_from_page(ctx, page, nullptr); }
            fz_always(ctx) { fz_drop_page(ctx, page); }
            fz_catch(ctx) { if (tp) fz_drop_stext_page(ctx, tp); fz_rethrow(ctx); }

            fz_write_printf(ctx, out, "<article><h2>Page %d</h2>\n", i + 1);
            if (tp) { fz_print_stext_page_as_html(ctx, out, tp, i); fz_drop_stext_page(ctx, tp); }
            fz_write_printf(ctx, out, "</article>\n");
            jc.report((i + 1) * 90 / std::max(1, total), util::fmt(L"Reading page %d of %d", i + 1, total));
        }
        fz_write_printf(ctx, out, "</body>\n</html>\n");
        fz_close_output(ctx, out);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (out) fz_drop_output(ctx, out);
    if (doc) fz_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesAfter = util::fileSize(outPath);
    res.pages = total;
    return res;
}

// ---------------------------------------------------------------------
// METADATA
// ---------------------------------------------------------------------
Result setMetadata(const std::wstring& path, const std::wstring& password,
                   const std::wstring& title, const std::wstring& author,
                   const std::wstring& subject, const std::wstring& keywords,
                   const std::wstring& creator, const std::wstring& producer,
                   const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    std::string sTitle = u8(title), sAuthor = u8(author), sSubject = u8(subject),
                sKeys = u8(keywords), sCreator = u8(creator), sProducer = u8(producer);
    pdf_document* doc = nullptr;
    bool ok = false;
    Result res;

    jc.report(30, L"Updating document properties");
    fz_var(doc); fz_var(ok);
    fz_try(ctx)
    {
        doc = openPdf(ctx, sp, pw, msg);
        if (!doc) fz_throw(ctx, FZ_ERROR_GENERIC, "%s", msg.c_str());
        fz_document* d = &doc->super;
        if (!sTitle.empty())    fz_set_metadata(ctx, d, "info:Title", sTitle.c_str());
        if (!sAuthor.empty())   fz_set_metadata(ctx, d, "info:Author", sAuthor.c_str());
        if (!sSubject.empty())  fz_set_metadata(ctx, d, "info:Subject", sSubject.c_str());
        if (!sKeys.empty())     fz_set_metadata(ctx, d, "info:Keywords", sKeys.c_str());
        if (!sCreator.empty())  fz_set_metadata(ctx, d, "info:Creator", sCreator.c_str());
        if (!sProducer.empty()) fz_set_metadata(ctx, d, "info:Producer", sProducer.c_str());

        jc.report(70, L"Saving");
        pdf_write_options wo = pdf_default_write_options;
        wo.do_garbage = 2;
        wo.do_compress = 1;
        wo.do_incremental = (op == sp) ? 1 : 0;
        pdf_save_document(ctx, doc, op.c_str(), &wo);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) pdf_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

} // namespace pdf

// =====================================================================
//  PDF <-> Word (.docx), fully offline
//  Author : Ragul S

// =====================================================================
namespace pdf {
namespace {

struct ZipEntryW { std::string name; uint32_t crc = 0, size = 0, off = 0; };

struct ZipWriter {
    FILE* f = nullptr;
    std::vector<ZipEntryW> ents;
    uint32_t off = 0;
    bool open(const std::string& p) { f = fopen(p.c_str(), "wb"); return f != nullptr; }
    void put16(uint32_t v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }
    void put32(uint32_t v) { for (int i = 0; i < 4; i++) fputc((v >> (8 * i)) & 255, f); }
    void add(const std::string& name, const std::string& data) {
        ZipEntryW e;
        e.name = name;
        e.crc = (uint32_t)crc32(crc32(0L, Z_NULL, 0), (const Bytef*)data.data(), (uInt)data.size());
        e.size = (uint32_t)data.size();
        e.off = off;
        put32(0x04034b50); put16(20); put16(0); put16(0); put16(0); put16(0);
        put32(e.crc); put32(e.size); put32(e.size);
        put16((uint32_t)name.size()); put16(0);
        fwrite(name.data(), 1, name.size(), f);
        fwrite(data.data(), 1, data.size(), f);
        off += 30 + (uint32_t)name.size() + e.size;
        ents.push_back(e);
    }
    void finish() {
        uint32_t cd = off;
        for (auto& e : ents) {
            put32(0x02014b50); put16(20); put16(20); put16(0); put16(0); put16(0); put16(0);
            put32(e.crc); put32(e.size); put32(e.size);
            put16((uint32_t)e.name.size()); put16(0); put16(0); put16(0); put16(0);
            put32(0); put32(e.off);
            fwrite(e.name.data(), 1, e.name.size(), f);
            off += 46 + (uint32_t)e.name.size();
        }
        uint32_t n = (uint32_t)ents.size();
        put32(0x06054b50); put16(0); put16(0); put16(n); put16(n);
        put32(off - cd); put32(cd); put16(0);
        if (f) fclose(f);
        f = nullptr;
    }
};

uint16_t rd16(const unsigned char* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const unsigned char* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

// Reads one entry out of a zip (stored or deflated). Good enough for .docx.
bool zipReadEntry(const std::string& zipPath, const std::string& want, std::string& out)
{
    FILE* f = fopen(zipPath.c_str(), "rb");
    if (!f) return false;
    std::vector<unsigned char> buf;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 200 * 1024 * 1024) { fclose(f); return false; }
    buf.resize((size_t)sz);
    if (fread(buf.data(), 1, buf.size(), f) != buf.size()) { fclose(f); return false; }
    fclose(f);

    size_t i = 0;
    while (i + 30 <= buf.size()) {
        if (rd32(&buf[i]) != 0x04034b50) break;
        uint16_t flags = rd16(&buf[i + 6]);
        uint16_t method = rd16(&buf[i + 8]);
        uint32_t csize = rd32(&buf[i + 18]);
        uint16_t nlen = rd16(&buf[i + 26]);
        uint16_t elen = rd16(&buf[i + 28]);
        size_t nameAt = i + 30;
        if (nameAt + nlen > buf.size()) break;
        std::string name((const char*)&buf[nameAt], nlen);
        size_t dataAt = nameAt + nlen + elen;
        if (dataAt + csize > buf.size()) break;
        if (name == want) {
            if (method == 0) {
                out.assign((const char*)&buf[dataAt], csize);
                return true;
            }
            if (method == 8) {
                z_stream zs{};
                if (inflateInit2(&zs, -15) != Z_OK) return false;
                zs.next_in = (Bytef*)&buf[dataAt];
                zs.avail_in = csize;
                std::string grow;
                unsigned char tmp[65536];
                int rc;
                do {
                    zs.next_out = tmp; zs.avail_out = sizeof tmp;
                    rc = inflate(&zs, Z_NO_FLUSH);
                    if (rc != Z_OK && rc != Z_STREAM_END) { inflateEnd(&zs); return false; }
                    grow.append((const char*)tmp, sizeof tmp - zs.avail_out);
                    if (grow.size() > 400 * 1024 * 1024) { inflateEnd(&zs); return false; }
                } while (rc != Z_STREAM_END);
                inflateEnd(&zs);
                out.swap(grow);
                return true;
            }
            return false;
        }
        if ((flags & 8) && csize == 0) return false;   // streaming zip: not produced by Word
        i = dataAt + csize;
    }
    return false;
}

std::string xmlEsc(const std::string& s)
{
    std::string o; o.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': o += "&amp;"; break;
        case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;
        case '"': o += "&quot;"; break;
        default:  o += c;
        }
    }
    return o;
}

std::string utf8FromRune(int c)
{
    char b[5] = { 0, 0, 0, 0, 0 };
    if (c < 0x80) b[0] = (char)c;
    else if (c < 0x800) { b[0] = (char)(0xC0 | (c >> 6)); b[1] = (char)(0x80 | (c & 63)); }
    else if (c < 0x10000) { b[0] = (char)(0xE0 | (c >> 12)); b[1] = (char)(0x80 | ((c >> 6) & 63)); b[2] = (char)(0x80 | (c & 63)); }
    else { b[0] = (char)(0xF0 | (c >> 18)); b[1] = (char)(0x80 | ((c >> 12) & 63)); b[2] = (char)(0x80 | ((c >> 6) & 63)); b[3] = (char)(0x80 | (c & 63)); }
    return std::string(b);
}

std::string utf16Unescape(const std::string& s)   // docx XML -> utf8
{
    std::string o;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '&') {
            size_t sc = s.find(';', i);
            if (sc != std::string::npos && sc - i < 10) {
                std::string e = s.substr(i + 1, sc - i - 1);
                if (e == "amp") { o += '&'; i = sc + 1; continue; }
                if (e == "lt")  { o += '<'; i = sc + 1; continue; }
                if (e == "gt")  { o += '>'; i = sc + 1; continue; }
                if (e == "quot"){ o += '"'; i = sc + 1; continue; }
                if (e == "apos"){ o += '\''; i = sc + 1; continue; }
                if (!e.empty() && (e[0] == '#' )) {
                    int v = (e.size() > 1 && (e[1] == 'x' || e[1] == 'X'))
                            ? (int)strtol(e.c_str() + 2, nullptr, 16) : (int)strtol(e.c_str() + 1, nullptr, 10);
                    if (v > 0) { o += utf8FromRune(v); i = sc + 1; continue; }
                }
            }
        }
        o += s[i++];
    }
    return o;
}

std::string attrVal(const std::string& tag, const char* name)
{
    size_t p = tag.find(name);
    if (p == std::string::npos) return "";
    p += strlen(name);
    while (p < tag.size() && (tag[p] == ' ' || tag[p] == '=')) p++;
    if (p >= tag.size() || tag[p] != '"') return "";
    size_t q = tag.find('"', p + 1);
    if (q == std::string::npos) return "";
    return tag.substr(p + 1, q - p - 1);
}

} // namespace

Result pdfToWord(const std::wstring& path, const std::wstring& password,
                 const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    fz_document* doc = nullptr;
    bool ok = false;
    int total = 0, runs = 0;
    std::string body;
    Result res;

    fz_var(doc); fz_var(ok); fz_var(total); fz_var(runs);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, sp.c_str());
        if (fz_needs_password(ctx, doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, doc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        total = fz_count_pages(ctx, doc);
        fz_stext_options sopts{};
        sopts.flags = FZ_STEXT_DEHYPHENATE;

        for (int i = 0; i < total; i++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            jc.report(i * 90 / std::max(1, total), util::fmt(L"Reading page %d of %d", i + 1, total));
            fz_page* page = fz_load_page(ctx, doc, i);
            fz_stext_page* tp = nullptr;
            fz_var(page); fz_var(tp);
            fz_try(ctx) { tp = fz_new_stext_page_from_page(ctx, page, &sopts); }
            fz_always(ctx) { if (page) fz_drop_page(ctx, page); }
            fz_catch(ctx) { tp = nullptr; }
            if (!tp) continue;

            for (fz_stext_block* b = tp->first_block; b; b = b->next) {
                if (b->type != FZ_STEXT_BLOCK_TEXT) continue;
                body += "<w:p>";
                bool firstLine = true;
                for (fz_stext_line* l = b->u.t.first_line; l; l = l->next) {
                    if (!firstLine) body += "<w:r><w:br/></w:r>";
                    firstLine = false;
                    // group chars into styled runs
                    fz_stext_char* ch = l->first_char;
                    while (ch) {
                        bool bold = fz_font_is_bold(ctx, ch->font) != 0;
                        bool ital = fz_font_is_italic(ctx, ch->font) != 0;
                        bool und  = (ch->flags & FZ_STEXT_UNDERLINE) != 0;
                        int  size = (int)(ch->size * 2.0f + 0.5f);
                        uint32_t col = ch->argb & 0xFFFFFFu;
                        std::string text;
                        while (ch) {
                            bool b2 = fz_font_is_bold(ctx, ch->font) != 0;
                            bool i2 = fz_font_is_italic(ctx, ch->font) != 0;
                            bool u2 = (ch->flags & FZ_STEXT_UNDERLINE) != 0;
                            int  s2 = (int)(ch->size * 2.0f + 0.5f);
                            uint32_t c2 = ch->argb & 0xFFFFFFu;
                            if (b2 != bold || i2 != ital || u2 != und || s2 != size || c2 != col) break;
                            if (ch->c > 0) text += utf8FromRune(ch->c);
                            ch = ch->next;
                        }
                        if (!text.empty()) {
                            runs++;
                            body += "<w:r><w:rPr>";
                            if (bold) body += "<w:b/>";
                            if (ital) body += "<w:i/>";
                            if (und)  body += "<w:u w:val=\"single\"/>";
                            char tmp[64];
                            snprintf(tmp, sizeof tmp, "<w:sz w:val=\"%d\"/>", size);
                            body += tmp;
                            snprintf(tmp, sizeof tmp, "<w:color w:val=\"%06X\"/>", col);
                            body += tmp;
                            body += "</w:rPr><w:t xml:space=\"preserve\">";
                            body += xmlEsc(text);
                            body += "</w:t></w:r>";
                        }
                    }
                }
                body += "</w:p>";
            }
            fz_drop_stext_page(ctx, tp);
            if (i + 1 < total)
                body += "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>";
        }
        fz_drop_document(ctx, doc); doc = nullptr;

        if (runs == 0)
            fz_throw(ctx, FZ_ERROR_GENERIC,
                     "no text layer found - this PDF is probably a scan");

        jc.report(92, L"Writing the Word document");
        std::string docXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            "<w:body>" + body +
            "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/>"
            "<w:pgMar w:top=\"1134\" w:right=\"1134\" w:bottom=\"1134\" w:left=\"1134\" "
            "w:header=\"708\" w:footer=\"708\" w:gutter=\"0\"/></w:sectPr>"
            "</w:body></w:document>";
        std::string styles =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/>"
            "<w:rPr><w:rFonts w:ascii=\"Calibri\" w:hAnsi=\"Calibri\"/><w:sz w:val=\"22\"/></w:rPr></w:style>"
            "</w:styles>";
        std::string ctypes =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
            "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
            "<Override PartName=\"/word/document.xml\" ContentType=\"application/"
            "vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
            "<Override PartName=\"/word/styles.xml\" ContentType=\"application/"
            "vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
            "</Types>";
        std::string rels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/officeDocument\" Target=\"word/document.xml\"/></Relationships>";
        std::string docrels =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
            "relationships/styles\" Target=\"styles.xml\"/></Relationships>";

        ZipWriter zw;
        if (!zw.open(op)) fz_throw(ctx, FZ_ERROR_GENERIC, "cannot create the output file");
        zw.add("[Content_Types].xml", ctypes);
        zw.add("_rels/.rels", rels);
        zw.add("word/document.xml", docXml);
        zw.add("word/styles.xml", styles);
        zw.add("word/_rels/document.xml.rels", docrels);
        zw.finish();
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (doc) fz_drop_document(ctx, doc);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    res.pages = total;
    return res;
}

// ---------------------------------------------------------------------
// Word -> PDF : parse document.xml, flow-layout the paragraphs
// ---------------------------------------------------------------------
namespace {
struct DocRun  { std::string text; bool bold = false, ital = false; float size = 11; };
struct DocPara { std::string style; std::vector<DocRun> runs; };
} // namespace

static bool layoutParas(fz_context* ctx, std::vector<DocPara>& paras, const std::string& op,
                        int& pages, JobContext& jc, std::string& msg)
{
    fz_document_writer* wri = nullptr;
    fz_device* dev = nullptr;
    fz_font* fonts[4] = { nullptr, nullptr, nullptr, nullptr };   // R B I BI
    bool ok = false;
    fz_var(wri); fz_var(dev); fz_var(pages);
    fz_try(ctx)
    {
        fonts[0] = fz_new_base14_font(ctx, "Times-Roman");
        fonts[1] = fz_new_base14_font(ctx, "Times-Bold");
        fonts[2] = fz_new_base14_font(ctx, "Times-Italic");
        fonts[3] = fz_new_base14_font(ctx, "Times-BoldItalic");

        wri = fz_new_document_writer(ctx, op.c_str(), "pdf", "");
        const float PW = 595.28f, PH = 841.89f, ML = 64, MR = 64, MT = 64, MB = 64;
        float y = 0;   // current baseline from top; 0 = new page needed
        bool pageOpen = false;

        auto newPage = [&]() {
            if (pageOpen) fz_end_page(ctx, wri);
            dev = fz_begin_page(ctx, wri, fz_make_rect(0, 0, PW, PH));
            pageOpen = true; pages++;
            y = MT;
        };
        auto lineFont = [&](const DocRun& r) {
            return fonts[(r.bold ? 1 : 0) + (r.ital ? 2 : 0)];
        };
        auto styleSize = [](const std::string& st, bool& bold) {
            bold = false;
            if (st == "Title")    { bold = true; return 26.0f; }
            if (st == "Heading1") { bold = true; return 18.0f; }
            if (st == "Heading2") { bold = true; return 15.0f; }
            if (st == "Heading3") { bold = true; return 13.0f; }
            return 11.0f;
        };
        auto drawLine = [&](std::vector<std::pair<const DocRun*, std::string>>& parts, float maxSize) {
            float x = ML;
            for (auto& pr : parts) {
                fz_matrix m = fz_concat(fz_scale(pr.first->size, -pr.first->size), fz_translate(x, y));  // writer flips positions, keeps scales
                fz_text* text = fz_new_text(ctx);
                fz_show_string(ctx, text, lineFont(*pr.first), m, pr.second.c_str(), 0, 0,
                               FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
                float col[3] = { 0, 0, 0 };
                fz_fill_text(ctx, dev, text, fz_identity, fz_device_rgb(ctx), col, 1.0f,
                             fz_default_color_params);
                fz_drop_text(ctx, text);
                fz_matrix adv = fz_measure_string(ctx, lineFont(*pr.first), m, pr.second.c_str(),
                                                  0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
                x += adv.e - m.e;
            }
            y += maxSize * 1.30f;
        };

        for (size_t pi = 0; pi < paras.size(); pi++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            if ((pi & 31) == 0) jc.report((int)(pi * 90 / paras.size()),
                                          util::fmt(L"Laying out paragraph %d of %d",
                                                    (int)pi + 1, (int)paras.size()));
            DocPara& p = paras[pi];
            bool hBold = false;
            float hSize = styleSize(p.style, hBold);
            if (p.style.rfind("Heading", 0) == 0 || p.style == "Title") y += hSize * 0.6f;

            // word-wrap every run's text
            std::vector<std::pair<const DocRun*, std::string>> line;
            float lineW = 0, lineMax = hSize;
            for (auto& r : p.runs) {
                DocRun rr = r;
                if (hBold) rr.bold = true;
                if (p.style == "Title" || p.style.rfind("Heading", 0) == 0)
                    if (rr.size < hSize) rr.size = hSize;
                std::string txt = rr.text;
                size_t pos = 0;
                while (pos <= txt.size()) {
                    size_t nl = txt.find('\n', pos);
                    size_t ff = txt.find('\f', pos);
                    size_t cut = std::min(nl, ff);
                    std::string chunk = txt.substr(pos, cut == std::string::npos ? std::string::npos : cut - pos);
                    // split chunk into words
                    size_t w0 = 0;
                    while (w0 < chunk.size() || line.empty() && false) {
                        while (w0 < chunk.size() && chunk[w0] == ' ') w0++;
                        size_t w1 = chunk.find(' ', w0);
                        if (w1 == std::string::npos) w1 = chunk.size();
                        if (w1 > w0) {
                            std::string word = chunk.substr(w0, w1 - w0);
                            fz_matrix m = fz_scale(rr.size, rr.size);
                            fz_matrix adv = fz_measure_string(ctx, lineFont(rr), m, word.c_str(),
                                                              0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
                            float ww = adv.e - m.e;
                            if (lineW + ww > PW - ML - MR && !line.empty()) {
                                if (!pageOpen || y > PH - MB) newPage();
                                drawLine(line, lineMax);
                                line.clear(); lineW = 0; lineMax = hSize;
                            }
                            line.push_back({ &rr, word + " " });
                            lineW += ww;
                            lineMax = std::max(lineMax, rr.size);
                        }
                        w0 = w1;
                        if (w1 >= chunk.size()) break;
                    }
                    if (cut == std::string::npos) { pos = txt.size() + 1; }
                    else if (cut == ff) {
                        if (!line.empty()) {
                            if (!pageOpen || y > PH - MB) newPage();
                            drawLine(line, lineMax); line.clear(); lineW = 0; lineMax = hSize;
                        }
                        newPage();
                        pos = cut + 1;
                    } else {   // '\n'
                        if (!pageOpen || y > PH - MB) newPage();
                        drawLine(line, lineMax); line.clear(); lineW = 0; lineMax = hSize;
                        pos = cut + 1;
                    }
                }
            }
            if (!line.empty()) {
                if (!pageOpen || y > PH - MB) newPage();
                drawLine(line, lineMax);
                line.clear(); lineW = 0;
            } else if (pageOpen) {
                y += hSize * 0.7f;   // empty paragraph = spacer
            }
        }
        if (!pageOpen) newPage();    // guarantee at least one page
        fz_end_page(ctx, wri); pageOpen = false;
        fz_close_document_writer(ctx, wri);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (dev && !ok) { /* writer unwound by fz_catch */ }
    if (wri) fz_drop_document_writer(ctx, wri);
    for (auto f : fonts) if (f) fz_drop_font(ctx, f);

    return ok;
}

Result wordToPdf(const std::wstring& path, const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), op = u8(outPath), msg;
    Result res;

    std::string xml;
    if (!zipReadEntry(sp, "word/document.xml", xml))
        return Result::failure(L"This .docx could not be opened. Is it a valid Word file?");

    // ---- tiny XML walk -------------------------------------------------
    std::vector<DocPara> paras;
    DocPara cur; bool inPara = false, inRun = false, inText = false;
    DocRun run;
    size_t i = 0;
    while (i < xml.size()) {
        size_t lt = xml.find('<', i);
        if (lt == std::string::npos) break;
        if (inText) {
            size_t gt = xml.find('>', lt);   // we are inside <w:t ...> text
            size_t et = xml.find("</w:t", lt);
            size_t endTxt = (et == std::string::npos) ? gt : et;
            if (lt > i) run.text += utf16Unescape(xml.substr(i, lt - i));
            if (et != std::string::npos && (gt == std::string::npos || et < gt)) {
                inText = false;
                i = xml.find('>', et);
                i = (i == std::string::npos) ? xml.size() : i + 1;
                continue;
            }
            i = lt;   // some other tag inside text (rare): fall through
        }
        size_t gt = xml.find('>', lt);
        if (gt == std::string::npos) break;
        std::string tag = xml.substr(lt, gt - lt + 1);
        bool close = tag.size() > 1 && tag[1] == '/';
        std::string name = tag.substr(close ? 2 : 1);
        size_t spc = name.find_first_of(" />");
        if (spc != std::string::npos) name = name.substr(0, spc);
        bool selfClose = tag.size() > 2 && tag[tag.size() - 2] == '/';

        if (name == "w:p" && !close) { inPara = true; cur = DocPara(); }
        else if (name == "w:p" && close) { if (inPara) paras.push_back(cur); inPara = false; }
        else if (name == "w:r" && !close) { inRun = true; run = DocRun(); }
        else if (name == "w:r" && close)  { if (inPara && !run.text.empty()) cur.runs.push_back(run); inRun = false; }
        else if (inPara) {
            if (name == "w:pStyle") cur.style = attrVal(tag, "w:val");
            else if (name == "w:b"   && !close) run.bold = !(attrVal(tag, "w:val") == "0" || attrVal(tag, "w:val") == "false");
            else if (name == "w:i"   && !close) run.ital = !(attrVal(tag, "w:val") == "0" || attrVal(tag, "w:val") == "false");
            else if (name == "w:sz"  && !close) { int v = atoi(attrVal(tag, "w:val").c_str()); if (v > 0) run.size = v / 2.0f; }
            else if (name == "w:t"   && !close) { inText = true; i = gt + 1; continue; }
            else if (name == "w:tab") run.text += "    ";
            else if (name == "w:br")  { if (attrVal(tag, "w:type") == "page") run.text += "\f"; else run.text += "\n"; }
            else if (name == "w:cr")  run.text += "\n";
        }
        (void)inRun; (void)selfClose;
        i = gt + 1;
    }

    if (paras.empty())
        return Result::failure(L"The document has no paragraphs to convert.");

    // ---- flow layout into a new PDF -------------------------------------
    int pages = 0;
    bool ok = layoutParas(ctx, paras, op, pages, jc, msg);

    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    res.pages = pages;
    return res;
}
} // namespace pdf

// =====================================================================
//  More document tools: image conversion, text/markdown -> PDF, PDF -> MD
//  Author : Ragul S
// =====================================================================
namespace pdf {

Result convertImage(const std::wstring& inPath, const std::wstring& outPath,
                    bool toPng, int maxPx, int quality, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string ip = u8(inPath), op = u8(outPath), msg;
    fz_image* img = nullptr;
    fz_pixmap* pix = nullptr;
    bool ok = false;
    Result res;
    fz_var(img); fz_var(pix);
    fz_try(ctx)
    {
        jc.report(20, L"Decoding image");
        img = fz_new_image_from_file(ctx, ip.c_str());
        int iw = 0, ih = 0;
        pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, &iw, &ih);
        if (!toPng && pix->alpha) {
            fz_pixmap* p2 = fz_convert_pixmap(ctx, pix, fz_device_rgb(ctx), nullptr, nullptr, fz_default_color_params, 0);
            if (p2) { fz_drop_pixmap(ctx, pix); pix = p2; }
        }
        int edge = std::max(pix->w, pix->h);
        if (maxPx > 0 && edge > maxPx) {
            float sc = (float)maxPx / (float)edge;
            fz_pixmap* sc2 = fz_scale_pixmap(ctx, pix, 0, 0, (float)(int)(pix->w * sc), (float)(int)(pix->h * sc), nullptr);
            if (sc2) { fz_drop_pixmap(ctx, pix); pix = sc2; }
        }
        jc.report(60, L"Encoding");
        if (toPng) fz_save_pixmap_as_png(ctx, pix, op.c_str());
        else       fz_save_pixmap_as_jpeg(ctx, pix, op.c_str(), quality);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (pix) fz_drop_pixmap(ctx, pix);
    if (img) fz_drop_image(ctx, img);
    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(inPath);
    res.bytesAfter = util::fileSize(outPath);
    return res;
}

// ---- markdown / plain text -> PDF ------------------------------------
namespace {
void mdInline(const std::string& line, float base, bool forceBold, std::vector<DocRun>& out)
{
    size_t i = 0;
    bool bold = forceBold, ital = false;
    std::string cur;
    auto flush = [&]() { if (!cur.empty()) { DocRun r; r.text = cur; r.bold = bold; r.ital = ital; r.size = base; out.push_back(r); cur.clear(); } };
    while (i < line.size()) {
        if (line[i] == '*' && i + 1 < line.size() && line[i + 1] == '*') { flush(); bold = !bold; i += 2; continue; }
        if (line[i] == '*' ) { flush(); ital = !ital; i += 1; continue; }
        cur += line[i++];
    }
    flush();
}
} // namespace

Result textToPdf(const std::wstring& path, const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string op = u8(outPath), msg;
    Result res;

    std::string raw;
    {
        FILE* f = fopen(u8(path).c_str(), "rb");
        if (!f) return Result::failure(L"The file could not be opened.");
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        if (n <= 0 || n > 50 * 1024 * 1024) { fclose(f); return Result::failure(L"The text file is empty or far too large."); }
        raw.resize((size_t)n);
        if (fread(&raw[0], 1, raw.size(), f) != raw.size()) { fclose(f); return Result::failure(L"Could not read the file."); }
        fclose(f);
    }
    // UTF-16 LE/BE with BOM -> naive widen to utf8
    if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) {
        std::string u8s;
        for (size_t i = 2; i + 1 < raw.size(); i += 2) {
            int c = (unsigned char)raw[i] | ((unsigned char)raw[i + 1] << 8);
            if (c == '\r') continue;
            u8s += utf8FromRune(c == '\n' ? '\n' : c);
        }
        raw.swap(u8s);
    } else if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB) {
        raw.erase(0, 3);
    }

    jc.report(30, L"Laying out the text");
    std::vector<DocPara> paras;
    DocPara cur; bool have = false;
    auto flushP = [&]() { if (have) { paras.push_back(cur); cur = DocPara(); have = false; } };
    size_t i = 0;
    while (i < raw.size()) {
        size_t nl = raw.find('\n', i);
        std::string line = raw.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
        i = (nl == std::string::npos) ? raw.size() : nl + 1;
        while (!line.empty() && (line.back() == '\r')) line.pop_back();
        std::string t = util::trim(util::toWide(line)).empty() ? "" : line;
        if (util::trim(util::toWide(line)).empty()) { flushP(); continue; }
        if (line.rfind("### ", 0) == 0)      { flushP(); DocPara p; p.style = "Heading3"; DocRun r; r.text = line.substr(4); r.bold = true; r.size = 13; p.runs.push_back(r); paras.push_back(p); continue; }
        if (line.rfind("## ", 0) == 0)       { flushP(); DocPara p; p.style = "Heading2"; DocRun r; r.text = line.substr(3); r.bold = true; r.size = 15; p.runs.push_back(r); paras.push_back(p); continue; }
        if (line.rfind("# ", 0) == 0)        { flushP(); DocPara p; p.style = "Heading1"; DocRun r; r.text = line.substr(2); r.bold = true; r.size = 18; p.runs.push_back(r); paras.push_back(p); continue; }
        if (line.rfind("- ", 0) == 0)        { if (!have) { have = true; } mdInline(std::string("\xE2\x80\xA2  ") + line.substr(2), 11, false, cur.runs); have = true; flushP(); continue; }
        if (!have) have = true;
        mdInline(line, 11, false, cur.runs);
    }
    flushP();
    if (paras.empty()) return Result::failure(L"The text file has no content.");

    int pages = 0;
    bool ok = layoutParas(ctx, paras, op, pages, jc, msg);
    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    res.pages = pages;
    return res;
}

// ---- PDF -> Markdown ---------------------------------------------------
Result pdfToMarkdown(const std::wstring& path, const std::wstring& password,
                     const std::wstring& outPath, JobContext& jc)
{
    fz_context* ctx = CTX();
    if (!ctx) return Result::failure(L"PDF engine failed to start.");
    std::string sp = u8(path), pw = u8(password), op = u8(outPath), msg;
    fz_document* doc = nullptr;
    fz_output* out = nullptr;
    bool ok = false;
    int total = 0, blocks = 0;
    Result res;
    fz_var(doc); fz_var(out); fz_var(ok); fz_var(total);
    fz_try(ctx)
    {
        doc = fz_open_document(ctx, sp.c_str());
        if (fz_needs_password(ctx, doc)) {
            if (pw.empty() || !fz_authenticate_password(ctx, doc, pw.c_str()))
                fz_throw(ctx, FZ_ERROR_GENERIC, "needs password");
        }
        total = fz_count_pages(ctx, doc);
        out = fz_new_output_with_path(ctx, op.c_str(), 0);
        fz_stext_options sopts{};
        sopts.flags = FZ_STEXT_DEHYPHENATE;

        for (int i = 0; i < total; i++) {
            if (jc.stop()) fz_throw(ctx, FZ_ERROR_GENERIC, "cancelled");
            jc.report(i * 90 / std::max(1, total), util::fmt(L"Reading page %d of %d", i + 1, total));
            fz_page* page = fz_load_page(ctx, doc, i);
            fz_stext_page* tp = nullptr;
            fz_var(page); fz_var(tp);
            fz_try(ctx) { tp = fz_new_stext_page_from_page(ctx, page, &sopts); }
            fz_always(ctx) { if (page) fz_drop_page(ctx, page); }
            fz_catch(ctx) { tp = nullptr; }
            if (!tp) continue;
            if (i > 0) fz_write_string(ctx, out, "\n---\n\n");

            for (fz_stext_block* b = tp->first_block; b; b = b->next) {
                if (b->type != FZ_STEXT_BLOCK_TEXT) continue;
                float maxSize = 0; int charCount = 0;
                std::string para;
                bool firstLine = true;
                bool anyBold = true, allBold = true;
                for (fz_stext_line* l = b->u.t.first_line; l; l = l->next) {
                    if (!firstLine) para += " ";
                    firstLine = false;
                    for (fz_stext_char* ch = l->first_char; ch; ch = ch->next) {
                        if (ch->c > 0) para += utf8FromRune(ch->c);
                        maxSize = std::max(maxSize, ch->size);
                        charCount++;
                        bool bb = fz_font_is_bold(ctx, ch->font) != 0;
                        if (bb) anyBold = true; else allBold = false;
                    }
                }
                (void)anyBold;
                std::string trimmed = u8(util::trim(util::toWide(para)));
                if (trimmed.empty()) continue;
                blocks++;
                const char* prefix = "";
                if (allBold && charCount > 0 && maxSize >= 17) prefix = "# ";
                else if (allBold && charCount > 0 && maxSize >= 14) prefix = "## ";
                else if (allBold && charCount > 0 && maxSize >= 12.5f) prefix = "### ";
                fz_write_string(ctx, out, prefix);
                fz_write_string(ctx, out, trimmed.c_str());
                fz_write_string(ctx, out, "\n\n");
            }
            fz_drop_stext_page(ctx, tp);
        }
        fz_close_output(ctx, out);
        ok = true;
    }
    fz_catch(ctx) { msg = u8(caughtError(ctx)); }
    if (out) fz_drop_output(ctx, out);
    if (doc) fz_drop_document(ctx, doc);
    if (!ok) {
        if (msg == "cancelled") return Result::failure(L"cancelled");
        return Result::failure(util::toWide(msg));
    }
    if (blocks == 0)
        return Result::failure(L"No selectable text was found. This PDF is probably a scan.");
    res.ok = true;
    res.outputs.push_back(outPath);
    res.bytesBefore = util::fileSize(path);
    res.bytesAfter = util::fileSize(outPath);
    res.pages = total;
    return res;
}

} // namespace pdf
