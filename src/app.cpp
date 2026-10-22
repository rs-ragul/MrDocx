// =====================================================================
//  MrDocx  -  app.cpp : tool catalogue, theme, caches, job runner
//  Author : Ragul S
// =====================================================================
#include "app.h"

// ---------------------------------------------------------------------
// theme globals
// ---------------------------------------------------------------------
namespace {
Palette g_light = lightPalette();
Palette g_dark  = darkPalette();
bool    g_dark_on = false;
}

Palette& currentPalette() { return g_dark_on ? g_dark : g_light; }
bool isDarkTheme() { return g_dark_on; }
void setDarkTheme(bool dark)
{
    app().dark = dark;
    g_dark_on = dark;
    settings::writeInt(L"DarkMode", dark ? 1 : 0);
    if (app().hwnd) {
        // match the OS window frame to our theme
        BOOL value = dark ? TRUE : FALSE;
        DwmSetWindowAttribute(app().hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        COLORREF border = dark ? RGB(0x0D, 0x10, 0x16) : RGB(0xE1, 0xE6, 0xEE);
        DwmSetWindowAttribute(app().hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));
    }
    postRepaint();
}

AppState& app() { static AppState a; return a; }

// ---------------------------------------------------------------------
// tool catalogue
// ---------------------------------------------------------------------
#define TINT(r,g,b) RGBv(r,g,b)

static const std::vector<ToolDef>& toolTable()
{
    static std::vector<ToolDef> t = {
        { ToolId::Merge, L"Merge PDF", L"Combine several PDFs into one document",
          L"Merge PDFs", Category::Organise, ICON_MERGE, TINT(0x2E,0x5B,0xFF),
          InputKind::MultiPdf, false, false,
          L"Drop two or more PDFs, arrange the order, then merge." },

        { ToolId::Split, L"Split PDF", L"Break a PDF into separate files",
          L"Split PDF", Category::Organise, ICON_SPLIT, TINT(0x7C,0x4D,0xFF),
          InputKind::SinglePdf, false, true,
          L"Split by ranges, every N pages, or one file per page." },

        { ToolId::Compress, L"Compress PDF", L"Reduce file size while keeping quality",
          L"Compress", Category::Optimise, ICON_COMPRESS, TINT(0x0E,0x9F,0x8A),
          InputKind::OneOrMorePdf, false, false,
          L"Recompresses images, drops unused objects and subsets fonts." },

        { ToolId::Organise, L"Organise PDF", L"Reorder, rotate and delete pages visually",
          L"Save changes", Category::Organise, ICON_ORGANIZE, TINT(0x4F,0x46,0xE5),
          InputKind::SinglePdf, true, false,
          L"Drag pages to reorder. Click to select, rotate or remove." },

        { ToolId::Rotate, L"Rotate PDF", L"Turn pages the right way up",
          L"Rotate & save", Category::Organise, ICON_ROTATE, TINT(0x08,0x91,0xB2),
          InputKind::SinglePdf, true, false,
          L"Rotate every page or only the ones you pick." },

        { ToolId::RemovePages, L"Remove Pages", L"Delete pages you do not need",
          L"Remove pages", Category::Organise, ICON_REMOVE, TINT(0xE0,0x48,0x3C),
          InputKind::SinglePdf, true, false,
          L"Select pages in the grid, then remove them." },

        { ToolId::ExtractPages, L"Extract Pages", L"Pull selected pages into a new PDF",
          L"Extract pages", Category::Organise, ICON_EXTRACT, TINT(0xD9,0x83,0x24),
          InputKind::SinglePdf, true, false,
          L"Select the pages you want and save them as a new PDF." },

        { ToolId::Watermark, L"Watermark", L"Stamp text over your pages",
          L"Add watermark", Category::Edit, ICON_WATERMARK, TINT(0x5B,0x6C,0xFF),
          InputKind::OneOrMorePdf, false, false,
          L"Original content, links and forms are preserved." },

        { ToolId::PageNumbers, L"Page Numbers", L"Add page numbering to your document",
          L"Add numbers", Category::Edit, ICON_PAGENUM, TINT(0x14,0x8A,0x4C),
          InputKind::OneOrMorePdf, false, false,
          L"Choose the position, format and starting number." },

        { ToolId::Protect, L"Protect PDF", L"Encrypt with a password",
          L"Protect PDF", Category::Secure, ICON_LOCK, TINT(0x1E,0x40,0xAF),
          InputKind::OneOrMorePdf, false, false,
          L"AES encryption. Set what readers are allowed to do." },

        { ToolId::Unlock, L"Unlock PDF", L"Remove the password from a PDF",
          L"Unlock PDF", Category::Secure, ICON_UNLOCK, TINT(0x25,0x63,0xEB),
          InputKind::OneOrMorePdf, false, false,
          L"You need the current password to unlock a file." },

        { ToolId::Repair, L"Repair PDF", L"Rebuild a damaged or corrupt file",
          L"Repair PDF", Category::Optimise, ICON_REPAIR, TINT(0xC2,0x41,0x0C),
          InputKind::OneOrMorePdf, false, false,
          L"Rewrites the object table and cleans broken structures." },

        { ToolId::Flatten, L"Flatten PDF", L"Bake forms and notes into the page",
          L"Flatten PDF", Category::Edit, ICON_FLATTEN, TINT(0x6D,0x28,0xD9),
          InputKind::OneOrMorePdf, false, false,
          L"Annotations and form fields become fixed page content." },

        { ToolId::PdfToImage, L"PDF to Image", L"Export pages as PNG or JPG",
          L"Export images", Category::Convert, ICON_TO_IMAGE, TINT(0x0E,0x74,0x90),
          InputKind::SinglePdf, false, true,
          L"One image per page, at the resolution you choose." },

        { ToolId::ImageToPdf, L"Image to PDF", L"Turn JPG or PNG files into a PDF",
          L"Create PDF", Category::Convert, ICON_FROM_IMAGE, TINT(0xB4,0x53,0x09),
          InputKind::Images, false, false,
          L"Fit to the image or place on A4 / Letter pages." },

        { ToolId::PdfToText, L"PDF to Text", L"Extract the text layer to a .txt file",
          L"Extract text", Category::Convert, ICON_TO_TEXT, TINT(0x47,0x55,0x69),
          InputKind::SinglePdf, false, false,
          L"Works on PDFs that contain selectable text." },

        { ToolId::PdfToHtml, L"PDF to HTML", L"Convert pages to a styled web page",
          L"Convert to HTML", Category::Convert, ICON_TO_HTML, TINT(0xB0,0x3A,0x2E),
          InputKind::SinglePdf, false, false,
          L"Keeps paragraphs and reading order, offline." },

        { ToolId::PdfToWord, L"PDF to Word", L"Save the text and formatting as a .docx",
          L"Convert to Word", Category::Convert, ICON_WORD, TINT(0x2B,0x57,0x9A),
          InputKind::SinglePdf, false, false,
          L"Keeps paragraphs, bold, italic and colours - fully offline." },

        { ToolId::WordToPdf, L"Word to PDF", L"Turn a .docx into a clean PDF",
          L"Convert to PDF", Category::Convert, ICON_WORD, TINT(0x7C,0x4D,0xFF),
          InputKind::SingleDocx, false, false,
          L"Reflows headings and paragraphs onto fresh pages." },

        { ToolId::ImageConvert, L"Convert Images", L"JPG, PNG, BMP, TIFF or GIF to JPG or PNG",
          L"Convert images", Category::Convert, ICON_TO_IMAGE, TINT(0x0E,0x74,0x90),
          InputKind::Images, false, true,
          L"Resize and re-encode pictures, fully offline." },

        { ToolId::TextToPdf, L"Text / Markdown to PDF", L"Typeset a .txt or .md file into a clean PDF",
          L"Create PDF", Category::Convert, ICON_TO_TEXT, TINT(0x47,0x55,0x69),
          InputKind::SingleText, false, false,
          L"Supports # headings, **bold** and *italic*." },

        { ToolId::PdfToMarkdown, L"PDF to Markdown", L"Extract the text layer as clean Markdown",
          L"Extract Markdown", Category::Convert, ICON_TO_HTML, TINT(0xB0,0x3A,0x2E),
          InputKind::SinglePdf, false, false,
          L"Headings detected from the type size, ready for notes and repos." },

        { ToolId::Metadata, L"Edit Properties", L"Change title, author and keywords",
          L"Save properties", Category::Edit, ICON_INFO, TINT(0x52,0x5C,0x6B),
          InputKind::SinglePdf, false, false,
          L"Updates the document information dictionary." },
    };
    return t;
}

const ToolDef& toolDef(ToolId id)
{
    auto& t = toolTable();
    int i = (int)id;
    if (i < 0 || i >= (int)t.size()) return t[0];
    return t[i];
}
const std::vector<ToolDef>& allTools() { return toolTable(); }

const wchar_t* categoryName(Category c)
{
    switch (c) {
    case Category::Organise: return L"Organise";
    case Category::Optimise: return L"Optimise";
    case Category::Convert:  return L"Convert";
    case Category::Secure:   return L"Secure";
    case Category::Edit:     return L"Edit";
    }
    return L"";
}

// ---------------------------------------------------------------------
// thumbnail cache
// ---------------------------------------------------------------------
void ThumbCache::start()
{
    std::lock_guard<std::mutex> g(mtx);
    if (running) return;
    running = true;
    th = std::thread([this] { workerLoop(); });
}

void ThumbCache::stop()
{
    {
        std::lock_guard<std::mutex> g(mtx);
        if (!running) return;
        running = false;
    }
    cv.notify_all();
    if (th.joinable()) th.join();
    clear();
}

void ThumbCache::clear()
{
    std::lock_guard<std::mutex> g(mtx);
    for (auto& kv : cache) if (kv.second.bmp) DeleteObject(kv.second.bmp);
    cache.clear();
    queued.clear();
    queue.clear();
}

HBITMAP ThumbCache::fetch(HWND notify, const std::wstring& path, const std::wstring& password,
                          int page, int maxPx)
{
    ThumbKey k{ path, password, page, maxPx };
    {
        std::lock_guard<std::mutex> g(mtx);
        notifyHwnd = notify;
        auto it = cache.find(k);
        if (it != cache.end()) return it->second.failed ? nullptr : it->second.bmp;
        if (queued.insert(k).second) queue.push_back(k);
        // hard cap so long sessions do not grow without bound
        if (cache.size() > 400) {
            for (auto& kv : cache) if (kv.second.bmp) DeleteObject(kv.second.bmp);
            cache.clear();
        }
    }
    cv.notify_one();
    return nullptr;
}

void ThumbCache::workerLoop()
{
    for (;;) {
        ThumbKey k;
        {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [this] { return !running || !queue.empty(); });
            if (!running && queue.empty()) return;
            if (queue.empty()) continue;
            k = queue.front();
            queue.pop_front();
        }

        static std::wstring curPath, curPw;
        static pdf::OpenDoc* curDoc = nullptr;
        if (!curDoc || curPath != k.path || curPw != k.password) {
            pdf::closeDocument(curDoc); curDoc = nullptr;
            curDoc = pdf::openDocument(k.path, k.password);
            curPath = k.path; curPw = k.password;
        }
        pdf::BitmapData bd = curDoc ? pdf::renderPageDoc(curDoc, k.page, k.maxPx, true)
                                    : pdf::renderPage(k.path, k.password, k.page, k.maxPx, true);
        {
            std::lock_guard<std::mutex> g(mtx);
            if (queue.empty()) { pdf::closeDocument(curDoc); curDoc = nullptr; curPath.clear(); curPw.clear(); }
        }
        Entry e;
        if (bd.valid && !bd.pixels.empty()) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = bd.width;
            bmi.bmiHeader.biHeight = -bd.height;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            HDC dc = GetDC(nullptr);
            void* bits = nullptr;
            HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
            ReleaseDC(nullptr, dc);
            if (bmp && bits) {
                memcpy(bits, bd.pixels.data(), bd.pixels.size());
                e.bmp = bmp; e.w = bd.width; e.h = bd.height;
            } else {
                if (bmp) DeleteObject(bmp);
                e.failed = true;
            }
        } else {
            e.failed = true;
        }

        HWND notify = nullptr;
        {
            std::lock_guard<std::mutex> g(mtx);
            cache[k] = e;
            queued.erase(k);
            notify = notifyHwnd;
        }
        if (notify) PostMessageW(notify, WM_APP_INVALIDATE, 0, 0);
    }
}

// ---------------------------------------------------------------------
// job runner
// ---------------------------------------------------------------------
void JobRunner::start(HWND owner, const std::wstring& title,
                      std::function<pdf::Result(pdf::JobContext&)> work)
{
    if (busy_) return;
    {
        std::lock_guard<std::mutex> g(mtx);
        title_ = title;
        step_ = L"Starting";
    }
    percent_ = 0;
    cancel_ = false;
    busy_ = true;

    if (th.joinable()) th.join();
    th = std::thread([owner, work, this] {
        pdf::JobContext jc;
        int lastPct = -1;
        DWORD lastPost = 0;
        jc.progress = [owner, this, &lastPct, &lastPost](int pct, const std::wstring& step) {
            pct = util::clampv<int>(pct, 0, 100);
            {
                std::lock_guard<std::mutex> g(mtx);
                step_ = step;
            }
            DWORD now = GetTickCount();
            if (pct != lastPct && (pct != lastPct || now - lastPost > 60)) {
                lastPct = percent_ = pct;
                lastPost = now;
                PostMessageW(owner, WM_APP_JOB_PROGRESS, (WPARAM)pct, 0);
            }
        };
        jc.cancelled = [this] { return cancel_.load(); };

        pdf::Result* res = new pdf::Result();
        *res = work ? work(jc) : pdf::Result::failure(L"No operation");
        busy_ = false;
        PostMessageW(owner, WM_APP_JOB_DONE, res->ok ? 1 : 0, (LPARAM)res);
    });
    th.detach();
}

void JobRunner::reset()
{
    percent_ = 0;
    cancel_ = false;
    std::lock_guard<std::mutex> g(mtx);
    step_.clear();
    title_.clear();
}

// ---------------------------------------------------------------------
// small app helpers
// ---------------------------------------------------------------------
void postRepaint()
{
    if (app().hwnd) PostMessageW(app().hwnd, WM_APP_INVALIDATE, 0, 0);
}

void showToast(const std::wstring& text, int kind)
{
    AppState& a = app();
    a.toasts.push_back({ text, kind, GetTickCount() + 4200 });
    postRepaint();
}

void goHome()
{
    app().view = ViewId::Home;
    app().search.clear();
    postRepaint();
}

void openTool(ToolId t)
{
    AppState& a = app();
    a.tool = t;
    a.view = ViewId::Tool;
    a.dragOver = false;

    ToolState& st = a.ts(t);
    const ToolDef& def = toolDef(t);

    // remember sensible defaults
    if (st.outDir.empty()) {
        st.outDir = a.lastOutDir.empty() ? util::defaultDocumentsDir() : a.lastOutDir;
    }
    if (!st.metaLoaded && t == ToolId::Metadata) {
        st.protect.userPassword.clear();
    }
    st.stamp.kind = (t == ToolId::PageNumbers) ? pdf::StampKind::PageNumber : pdf::StampKind::Watermark;
    if (t == ToolId::Watermark && st.stamp.text.empty()) st.stamp.text = L"CONFIDENTIAL";
    if (t == ToolId::PageNumbers && st.stamp.numberFormat == L"{n}" && st.stamp.pageNumSize == 12) {
        st.stamp.pageNumPos = 7;      // bottom centre
    }
    if (t == ToolId::ImageToPdf) { /* nothing */ }
    (void)def;

    // load info for staged files if needed
    for (auto& f : st.files) {
        if (!f.infoLoaded) {
            std::wstring err;
            if (pdf::readInfo(f.path, f.password, f.info, err)) f.infoLoaded = true;
            else if (f.info.needsPassword) f.locked = true;
            else f.infoLoaded = true;
        }
    }
    postRepaint();
}

// ---------------------------------------------------------------------
// file staging
// ---------------------------------------------------------------------
static bool acceptForTool(ToolId t, const std::wstring& path)
{
    std::wstring ext = util::lower(util::extension(path));
    const ToolDef& def = toolDef(t);
    if (def.input == InputKind::Images)
        return ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp" ||
               ext == L".tif" || ext == L".tiff" || ext == L".gif" || ext == L".pnm" ||
               ext == L".pgm" || ext == L".ppm" || ext == L".psd";
    if (def.input == InputKind::SingleDocx)
        return ext == L".docx";
    if (def.input == InputKind::SingleText)
        return ext == L".txt" || ext == L".md" || ext == L".markdown" || ext == L".log";
    if (ext != L".pdf") return false;
    return true;
}

void addFilesToCurrentTool(const std::vector<std::wstring>& paths)
{
    AppState& a = app();
    ToolState& st = a.cur();
    const ToolDef& def = toolDef(a.tool);
    if (a.view != ViewId::Tool) return;

    int added = 0, skipped = 0;
    for (auto& p : paths) {
        if (!util::fileExists(p)) { skipped++; continue; }
        if (!acceptForTool(a.tool, p)) { skipped++; continue; }
        if (def.input == InputKind::SinglePdf && !st.files.empty()) { st.files.clear(); }

        bool dup = false;
        for (auto& f : st.files) if (_wcsicmp(f.path.c_str(), p.c_str()) == 0) { dup = true; break; }
        if (dup) { skipped++; continue; }

        StagedFile sf;
        sf.path = p;
        std::wstring err;
        if (def.input == InputKind::SingleDocx || def.input == InputKind::SingleText) {
            sf.infoLoaded = true;
            sf.info.valid = true;
            sf.info.fileSize = util::fileSize(p);
        } else if (def.input != InputKind::Images) {
            if (pdf::readInfo(p, L"", sf.info, err)) sf.infoLoaded = true;
            else if (sf.info.needsPassword) { sf.locked = true; sf.infoLoaded = true; }
            else { sf.infoLoaded = true; sf.info.valid = false; }
        } else {
            sf.infoLoaded = true;
            sf.info.valid = true;
        }
        st.files.push_back(sf);
        added++;
    }

    if (st.outDir.empty() && !st.files.empty()) st.outDir = util::dirName(st.files.front().path);
    st.ran = false;
    st.result = pdf::Result();
    st.pagesLoaded = false;
    st.order.clear(); st.rotation.clear(); st.selectedPages.clear();

    if (def.input == InputKind::Images && a.tool == ToolId::ImageToPdf) {
        if (st.outName.empty()) st.outName = L"images.pdf";
    }

    if (added == 0 && skipped > 0)
        showToast(def.input == InputKind::Images ? L"Only image files can be added here."
                  : def.input == InputKind::SingleDocx ? L"Only Word (.docx) files can be added here."
                                                       : L"Only PDF files can be added here.", 2);
    else if (added > 0)
        showToast(util::fmt(L"Added %d file%s", added, added == 1 ? L"" : L"s"));

    postRepaint();
}

// ---------------------------------------------------------------------
// output path helpers
// ---------------------------------------------------------------------
static std::wstring suffixFor(ToolId t)
{
    switch (t) {
    case ToolId::Merge:       return L"merged";
    case ToolId::Split:       return L"split";
    case ToolId::Compress:    return L"compressed";
    case ToolId::Organise:    return L"organised";
    case ToolId::Rotate:      return L"rotated";
    case ToolId::RemovePages: return L"pages removed";
    case ToolId::ExtractPages:return L"extracted";
    case ToolId::Watermark:   return L"watermarked";
    case ToolId::PageNumbers: return L"numbered";
    case ToolId::Protect:     return L"protected";
    case ToolId::Unlock:      return L"unlocked";
    case ToolId::Repair:      return L"repaired";
    case ToolId::Flatten:     return L"flattened";
    case ToolId::PdfToText:   return L"";
    case ToolId::PdfToHtml:   return L"";
    case ToolId::PdfToWord:   return L"";
    case ToolId::WordToPdf:   return L"";
    case ToolId::ImageConvert:return L"";
    case ToolId::TextToPdf:   return L"";
    case ToolId::PdfToMarkdown: return L"";
    case ToolId::Metadata:    return L"updated";
    default:                  return L"out";
    }
}

std::wstring defaultOutputDir(AppState& a, ToolState& st)
{
    if (a.saveToSourceFolder && !st.files.empty())
        return util::dirName(st.files.front().path);
    if (!a.lastOutDir.empty()) return a.lastOutDir;
    return util::joinPath(util::defaultDocumentsDir(), L"MrDocx");
}

std::wstring defaultOutputFile(AppState& a, ToolState& st, const std::wstring& ext)
{
    std::wstring dir = defaultOutputDir(a, st);
    std::wstring base = st.files.empty() ? L"document" : util::baseName(st.files.front().path);
    std::wstring suf = suffixFor(a.tool);
    std::wstring name = suf.empty() ? base : util::fmt(L"%s (%s)", base.c_str(), suf.c_str());
    if (a.tool == ToolId::PdfToText) return util::uniquePath(util::joinPath(dir, base + L".txt"));
    if (a.tool == ToolId::PdfToHtml) return util::uniquePath(util::joinPath(dir, base + L".html"));
    if (a.tool == ToolId::PdfToWord) return util::uniquePath(util::joinPath(dir, base + L".docx"));
    if (a.tool == ToolId::PdfToMarkdown) return util::uniquePath(util::joinPath(dir, base + L".md"));
    return util::uniquePath(util::joinPath(dir, name + ext));
}

// ---------------------------------------------------------------------
// run the current tool
// ---------------------------------------------------------------------
static bool ensureOutputDir(const std::wstring& dir)
{
    if (util::dirExists(dir)) return true;
    return CreateDirectoryW(dir.c_str(), nullptr) != 0 || util::dirExists(dir);
}

void runCurrentTool()
{
    AppState& a = app();
    ToolState& st = a.cur();
    const ToolDef& def = toolDef(a.tool);

    if (JobRunner::get().busy()) return;
    if (st.files.empty()) {
        ui::appMessageBox(a.hwnd, L"Nothing to do", L"Add a file first.",
                          L"Drag files into the window or use the Add files button.", 0, 2);
        return;
    }

    // password gate
    for (auto& f : st.files) {
        if (f.locked && f.password.empty()) {
            ui::appMessageBox(a.hwnd, L"Password needed",
                              util::fmt(L"%s is password protected.", util::fileName(f.path).c_str()),
                              L"Unlock it first with the Unlock PDF tool, then try again.", 0, 2);
            return;
        }
    }

    if (a.tool == ToolId::Protect && st.protect.userPassword != st.protectConfirm) {
        ui::appMessageBox(a.hwnd, L"Passwords do not match",
                          st.protect.userPassword.empty() ? L"Enter the password you want to set."
                                                          : L"The two passwords you typed are different.",
                          L"Type the same password in both boxes.", 0, 2);
        return;
    }
    if (false) {
        ui::appMessageBox(a.hwnd, L"Password needed", L"Enter the password you want to set.",
                          L"It is used to encrypt the output file.", 0, 2);
        return;
    }
    if (a.tool == ToolId::Unlock) {
        bool any = !st.unlockPassword.empty();
        for (auto& f : st.files) if (!f.password.empty()) any = true;
        if (!any) {
            ui::appMessageBox(a.hwnd, L"Password needed", L"Enter the current password.",
                              L"MrDocx needs it to decrypt the file before saving an open copy.", 0, 2);
            return;
        }
    }
    if (a.tool == ToolId::Watermark && util::trim(st.stamp.text).empty()) {
        ui::appMessageBox(a.hwnd, L"Watermark text", L"Type the watermark text first.", L"", 0, 2);
        return;
    }
    if ((a.tool == ToolId::ExtractPages || a.tool == ToolId::RemovePages) && st.selectedPages.empty()) {
        ui::appMessageBox(a.hwnd, L"No pages selected",
                          L"Select at least one page in the grid.",
                          L"Click a page to select it. Hold Ctrl to select several.", 0, 2);
        return;
    }
    if (a.tool == ToolId::Split && st.splitMode == pdf::SplitMode::Ranges) {
        pdf::DocInfo di; std::wstring err;
        int n = 0;
        if (!st.files.empty() && pdf::readInfo(st.files[0].path, st.files[0].password, di, err)) n = di.pageCount;
        if (pdf::parsePageRanges(st.rangeSpec, n).empty() && n > 0) {
            ui::appMessageBox(a.hwnd, L"Invalid ranges",
                              L"That page range does not match this document.",
                              util::fmt(L"The file has %d page%s. Try something like 1-3, 4-%d",
                                        n, n == 1 ? L"" : L"s", n).c_str(), 0, 2);
            return;
        }
    }

    std::wstring outDir = defaultOutputDir(a, st);
    if (!ensureOutputDir(outDir)) {
        ui::appMessageBox(a.hwnd, L"Cannot create folder",
                          util::fmt(L"MrDocx cannot write to %s", outDir.c_str()),
                          L"Pick another output folder in Settings.", 0, 3);
        return;
    }
    a.lastOutDir = outDir;
    settings::writeStr(L"LastOutDir", outDir);

    // ---- build the work item -----------------------------------------
    ToolId tid = a.tool;
    std::vector<StagedFile> files = st.files;

    // organiser-derived page lists
    std::vector<int> order = st.order;
    std::vector<int> rotation = st.rotation;
    std::vector<int> selectedPos(st.selectedPages.begin(), st.selectedPages.end());
    std::sort(selectedPos.begin(), selectedPos.end());

    std::wstring rangeSpec = st.rangeSpec;
    pdf::SplitMode splitMode = st.splitMode;
    int everyN = st.everyN;
    pdf::CompressLevel level = st.compress;
    pdf::ProtectOptions prot = st.protect;
    pdf::StampOptions stamp = st.stamp;
    pdf::ImageExportOptions imgOpt = st.imgExport;
    pdf::ImageToPdfOptions i2p = st.img2pdf;
    bool pageBreaks = st.pageBreaks;
    bool exportAll = st.exportAllPages;
    std::wstring mTitle = st.mTitle, mAuthor = st.mAuthor, mSubject = st.mSubject,
                 mKeys = st.mKeywords, mCreator = st.mCreator, mProducer = st.mProducer;
    std::wstring baseName = files.empty() ? L"document" : util::baseName(files.front().path);

    std::function<pdf::Result(pdf::JobContext&)> work;

    switch (tid) {
    case ToolId::Merge: {
        std::wstring out = defaultOutputFile(a, st, L".pdf");
        std::vector<pdf::MergeItem> items;
        for (auto& f : files) if (f.selected) items.push_back({ f.path, f.password });
        if (items.size() < 2) {
            ui::appMessageBox(a.hwnd, L"Need more files", L"Merge needs at least two PDFs.",
                              L"Add another file to the list.", 0, 2);
            return;
        }
        work = [items, out](pdf::JobContext& jc) { return pdf::mergeDocuments(items, out, jc); };
        st.outName = util::fileName(out);
        break;
    }
    case ToolId::Split: {
        pdf::SplitOptions so; so.mode = splitMode; so.ranges = rangeSpec; so.everyN = everyN;
        std::wstring src = files[0].path, pw = files[0].password;
        work = [src, pw, so, outDir, baseName](pdf::JobContext& jc) {
            return pdf::splitDocument(src, pw, so, outDir, baseName, jc);
        };
        break;
    }
    case ToolId::Compress: {
        std::vector<std::pair<std::wstring, std::wstring>> jobs;
        for (auto& f : files) {
            std::wstring out = util::uniquePath(util::joinPath(outDir,
                util::fmt(L"%s (compressed).pdf", util::baseName(f.path).c_str())));
            jobs.push_back({ f.path, out });
        }
        std::vector<std::wstring> pws;
        for (auto& f : files) pws.push_back(f.password);
        work = [jobs, pws, level](pdf::JobContext& jc) {
            pdf::Result agg;
            agg.ok = true;
            for (size_t i = 0; i < jobs.size(); i++) {
                if (jc.stop()) return pdf::Result::failure(L"cancelled");
                jc.report((int)(i * 100 / jobs.size()), util::fmt(L"Compressing %s",
                          util::fileName(jobs[i].first).c_str()));
                pdf::JobContext sub;
                int basePct = (int)(i * 100 / jobs.size());
                int span = (int)(100 / jobs.size());
                sub.progress = [&jc, basePct, span](int p, const std::wstring& s) {
                    jc.report(basePct + p * span / 100, s);
                };
                sub.cancelled = [&jc] { return jc.stop(); };
                pdf::Result r = pdf::compressDocument(jobs[i].first, pws[i], level, jobs[i].second, sub);
                if (!r.ok) return r;
                agg.outputs.insert(agg.outputs.end(), r.outputs.begin(), r.outputs.end());
                agg.bytesBefore += r.bytesBefore;
                agg.bytesAfter += r.bytesAfter;
                agg.pages += r.pages;
            }
            return agg;
        };
        break;
    }
    case ToolId::Organise:
    case ToolId::Rotate:
    case ToolId::RemovePages: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".pdf");
        std::vector<int> useOrder;
        std::vector<int> useRot;
        if (tid == ToolId::RemovePages) {
            std::set<int> removeSet;
            for (int pos : selectedPos) if (pos >= 0 && pos < (int)order.size()) removeSet.insert(order[pos]);
            for (size_t i = 0; i < order.size(); i++)
                if (!removeSet.count(order[i])) { useOrder.push_back(order[i]); useRot.push_back(rotation[i]); }
        } else if (tid == ToolId::Organise) {
            useOrder = order; useRot = rotation;
        } else {   // Rotate: keep order, apply rotation to selected (or all)
            useOrder = order;
            useRot = rotation;
            if (!selectedPos.empty()) {
                std::set<int> sel(selectedPos.begin(), selectedPos.end());
                for (size_t i = 0; i < useRot.size(); i++)
                    if (!sel.count((int)i)) useRot[i] = 0;   // unchanged pages keep original rotation
            }
        }
        if (useOrder.empty()) {
            ui::appMessageBox(a.hwnd, L"No pages left", L"That would remove every page.",
                              L"Select fewer pages and try again.", 0, 2);
            return;
        }
        if (tid == ToolId::Rotate && selectedPos.empty()) {
            // rotate everything by 90 degrees
            for (size_t i = 0; i < useRot.size(); i++) useRot[i] = (useRot[i] + 90) % 360;
        }
        work = [src, pw, useOrder, useRot, out](pdf::JobContext& jc) {
            return pdf::reorderPages(src, pw, useOrder, useRot, out, jc);
        };
        st.outName = util::fileName(out);
        break;
    }
    case ToolId::ExtractPages: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".pdf");
        std::vector<int> pages;
        for (int pos : selectedPos) if (pos >= 0 && pos < (int)order.size()) pages.push_back(order[pos]);
        work = [src, pw, pages, out](pdf::JobContext& jc) {
            return pdf::selectPages(src, pw, pages, true, out, jc);
        };
        st.outName = util::fileName(out);
        break;
    }
    case ToolId::Watermark:
    case ToolId::PageNumbers: {
        std::vector<std::tuple<std::wstring, std::wstring, std::wstring>> jobs;
        for (auto& f : files) {
            std::wstring out = util::uniquePath(util::joinPath(outDir, util::fmt(L"%s (%s).pdf",
                util::baseName(f.path).c_str(),
                tid == ToolId::Watermark ? L"watermarked" : L"numbered")));
            jobs.push_back(std::make_tuple(f.path, f.password, out));
        }
        work = [jobs, stamp, tid](pdf::JobContext& jc) {
            pdf::Result agg; agg.ok = true;
            for (size_t i = 0; i < jobs.size(); i++) {
                if (jc.stop()) return pdf::Result::failure(L"cancelled");
                int basePct = (int)(i * 100 / jobs.size()), span = (int)(100 / jobs.size());
                pdf::JobContext sub;
                sub.progress = [&jc, basePct, span](int p, const std::wstring& s) {
                    jc.report(basePct + p * span / 100, s);
                };
                sub.cancelled = [&jc] { return jc.stop(); };
                pdf::StampOptions so = stamp;
                so.kind = (tid == ToolId::PageNumbers) ? pdf::StampKind::PageNumber : pdf::StampKind::Watermark;
                pdf::Result r = pdf::stampDocument(std::get<0>(jobs[i]), std::get<1>(jobs[i]), so,
                                                   std::vector<int>(), std::get<2>(jobs[i]), sub);
                if (!r.ok) return r;
                agg.outputs.insert(agg.outputs.end(), r.outputs.begin(), r.outputs.end());
                agg.bytesBefore += r.bytesBefore; agg.bytesAfter += r.bytesAfter; agg.pages += r.pages;
            }
            return agg;
        };
        break;
    }
    case ToolId::Protect:
    case ToolId::Unlock: {
        std::vector<std::tuple<std::wstring, std::wstring, std::wstring>> jobs;
        for (auto& f : files) {
            std::wstring out = util::uniquePath(util::joinPath(outDir, util::fmt(L"%s (%s).pdf",
                util::baseName(f.path).c_str(), tid == ToolId::Protect ? L"protected" : L"unlocked")));
            jobs.push_back(std::make_tuple(f.path, f.password, out));
        }
        std::wstring fallbackPw = st.unlockPassword;
        work = [jobs, prot, tid, fallbackPw](pdf::JobContext& jc) {
            pdf::Result agg; agg.ok = true;
            for (size_t i = 0; i < jobs.size(); i++) {
                if (jc.stop()) return pdf::Result::failure(L"cancelled");
                int basePct = (int)(i * 100 / jobs.size()), span = (int)(100 / jobs.size());
                pdf::JobContext sub;
                sub.progress = [&jc, basePct, span](int p, const std::wstring& s) {
                    jc.report(basePct + p * span / 100, s);
                };
                sub.cancelled = [&jc] { return jc.stop(); };
                std::wstring pw = std::get<1>(jobs[i]).empty() ? fallbackPw : std::get<1>(jobs[i]);
                pdf::Result r = (tid == ToolId::Protect)
                    ? pdf::protectDocument(std::get<0>(jobs[i]), pw, prot, std::get<2>(jobs[i]), sub)
                    : pdf::unlockDocument(std::get<0>(jobs[i]), pw, std::get<2>(jobs[i]), sub);
                if (!r.ok) return r;
                agg.outputs.insert(agg.outputs.end(), r.outputs.begin(), r.outputs.end());
                agg.bytesBefore += r.bytesBefore; agg.bytesAfter += r.bytesAfter; agg.pages += r.pages;
            }
            return agg;
        };
        break;
    }
    case ToolId::Repair:
    case ToolId::Flatten: {
        std::vector<std::tuple<std::wstring, std::wstring, std::wstring>> jobs;
        for (auto& f : files) {
            std::wstring out = util::uniquePath(util::joinPath(outDir, util::fmt(L"%s (%s).pdf",
                util::baseName(f.path).c_str(), tid == ToolId::Repair ? L"repaired" : L"flattened")));
            jobs.push_back(std::make_tuple(f.path, f.password, out));
        }
        work = [jobs, tid](pdf::JobContext& jc) {
            pdf::Result agg; agg.ok = true;
            for (size_t i = 0; i < jobs.size(); i++) {
                if (jc.stop()) return pdf::Result::failure(L"cancelled");
                int basePct = (int)(i * 100 / jobs.size()), span = (int)(100 / jobs.size());
                pdf::JobContext sub;
                sub.progress = [&jc, basePct, span](int p, const std::wstring& s) {
                    jc.report(basePct + p * span / 100, s);
                };
                sub.cancelled = [&jc] { return jc.stop(); };
                pdf::Result r = (tid == ToolId::Repair)
                    ? pdf::repairDocument(std::get<0>(jobs[i]), std::get<1>(jobs[i]), std::get<2>(jobs[i]), sub)
                    : pdf::flattenDocument(std::get<0>(jobs[i]), std::get<1>(jobs[i]), std::get<2>(jobs[i]), sub);
                if (!r.ok) return r;
                agg.outputs.insert(agg.outputs.end(), r.outputs.begin(), r.outputs.end());
                agg.bytesBefore += r.bytesBefore; agg.bytesAfter += r.bytesAfter; agg.pages += r.pages;
            }
            return agg;
        };
        break;
    }
    case ToolId::PdfToImage: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::vector<int> pages;
        if (!exportAll) {
            for (int pos : selectedPos) if (pos >= 0 && pos < (int)order.size()) pages.push_back(order[pos]);
        }
        work = [src, pw, imgOpt, pages, outDir, baseName](pdf::JobContext& jc) {
            return pdf::exportImages(src, pw, imgOpt, pages, outDir, baseName, jc);
        };
        break;
    }
    case ToolId::ImageToPdf: {
        std::vector<std::wstring> imgs;
        for (auto& f : files) imgs.push_back(f.path);
        std::wstring out = util::uniquePath(util::joinPath(outDir,
            st.outNameManual && !st.outName.empty() ? st.outName : L"images.pdf"));
        work = [imgs, i2p, out](pdf::JobContext& jc) {
            return pdf::imagesToPdf(imgs, i2p, out, jc);
        };
        break;
    }
    case ToolId::PdfToText: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".txt");
        work = [src, pw, out, pageBreaks](pdf::JobContext& jc) {
            return pdf::exportText(src, pw, out, pageBreaks, jc);
        };
        break;
    }
    case ToolId::PdfToHtml: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".html");
        work = [src, pw, out](pdf::JobContext& jc) {
            return pdf::exportHtml(src, pw, out, jc);
        };
        break;
    }
    case ToolId::PdfToWord: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".docx");
        work = [src, pw, out](pdf::JobContext& jc) {
            return pdf::pdfToWord(src, pw, out, jc);
        };
        break;
    }
    case ToolId::WordToPdf: {
        std::wstring src = files[0].path;
        std::wstring out = defaultOutputFile(a, st, L".pdf");
        work = [src, out](pdf::JobContext& jc) {
            return pdf::wordToPdf(src, out, jc);
        };
        break;
    }
    case ToolId::ImageConvert: {
        std::vector<std::wstring> ins;
        for (auto& f : files) ins.push_back(f.path);
        bool png = st.imgConvToPng; int q = st.imgConvQuality; int mx = st.imgConvMaxPx;
        work = [ins, png, q, mx, outDir](pdf::JobContext& jc) {
            pdf::Result agg; agg.ok = true;
            for (size_t i = 0; i < ins.size(); i++) {
                if (jc.stop()) { agg.ok = false; agg.error = L"cancelled"; break; }
                std::wstring out = util::uniquePath(util::joinPath(outDir,
                    util::baseName(ins[i]) + (png ? L".png" : L".jpg")));
                pdf::Result r = pdf::convertImage(ins[i], out, png, mx, q, jc);
                if (!r.ok) { agg.ok = false; agg.error = r.error; break; }
                agg.outputs.push_back(out);
                agg.bytesAfter += r.bytesAfter; agg.bytesBefore += r.bytesBefore;
            }
            return agg;
        };
        break;
    }
    case ToolId::TextToPdf: {
        std::wstring src = files[0].path;
        std::wstring out = defaultOutputFile(a, st, L".pdf");
        work = [src, out](pdf::JobContext& jc) {
            return pdf::textToPdf(src, out, jc);
        };
        break;
    }
    case ToolId::PdfToMarkdown: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".md");
        work = [src, pw, out](pdf::JobContext& jc) {
            return pdf::pdfToMarkdown(src, pw, out, jc);
        };
        break;
    }
    case ToolId::Metadata: {
        std::wstring src = files[0].path, pw = files[0].password;
        std::wstring out = defaultOutputFile(a, st, L".pdf");
        work = [src, pw, mTitle, mAuthor, mSubject, mKeys, mCreator, mProducer, out](pdf::JobContext& jc) {
            return pdf::setMetadata(src, pw, mTitle, mAuthor, mSubject, mKeys, mCreator, mProducer, out, jc);
        };
        break;
    }
    default:
        ui::appMessageBox(a.hwnd, L"Not available", L"This tool is not wired up yet.", L"", 0, 2);
        return;
    }

    if (!work) return;

    st.ran = false;
    st.result = pdf::Result();
    a.jobTool = a.tool;
    JobRunner::get().start(a.hwnd, def.name, work);
    postRepaint();
    (void)def;
}

void requestThumbRefresh() { postRepaint(); }
