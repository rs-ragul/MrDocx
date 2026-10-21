// =====================================================================
//  PDFForge  -  views_tool.cpp : the working view of every tool
//  Author : Ragul S
// =====================================================================
#include "cards.h"
#include <gdiplus.h>

using namespace Gdiplus;

// ids -----------------------------------------------------------------
enum IdBase {
    ID_FILES      = 5000,
    ID_ORGANISER  = 20000,
    ID_OPTIONS    = 8000,
    ID_OUTPUT     = 9000,
    ID_ACTION     = 9500,
    ID_DROPZONE   = 4900,
};

namespace views {

// forward
static void drawFileRows(Frame& f, ToolState& st, const ToolDef& def);
static void drawOrganiser(Frame& f, ToolState& st, const ToolDef& def, int idBase);
static void drawToolOptions(Frame& f, ToolState& st, const ToolDef& def);
static void wrappedNote(Frame& f, const std::wstring& text);
// ---------------------------------------------------------------------
// page list for the organiser grid
// ---------------------------------------------------------------------
static void ensurePagesLoaded(ToolState& st)
{
    if (st.pagesLoaded || st.files.empty()) return;
    st.pagesLoaded = true;
    st.pages.clear(); st.order.clear(); st.rotation.clear(); st.selectedPages.clear();

    const StagedFile& sf = st.files.front();
    std::wstring err;
    std::vector<pdf::PageThumb> list;
    if (pdf::readPageList(sf.path, sf.password, list, err) && !list.empty()) {
        st.pages = list;
        st.order.reserve(list.size());
        st.rotation.assign(list.size(), 0);
        for (size_t i = 0; i < list.size(); i++) st.order.push_back((int)i);
    }
}

// Blits a cached page bitmap into dst, honouring 90/180/270 rotation.
// PlgBlt is the cheap exact path for the quarter turns (SetWorldTransform
// does not affect BitBlt/StretchBlt).
static void drawThumbBitmap(HDC dc, HBITMAP bmp, RECT dst, int rotateDeg)
{
    BITMAP bm;
    if (!GetObjectW(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) return;
    int sw = bm.bmWidth, sh = bm.bmHeight;
    bool swap = (rotateDeg == 90 || rotateDeg == 270);
    int dw = dst.right - dst.left, dh = dst.bottom - dst.top;
    if (dw <= 0 || dh <= 0) return;
    float ar = (float)(swap ? sh : sw) / (float)std::max(1, swap ? sw : sh);
    int w = dw, h = (int)((float)w / ar);
    if (h > dh) { h = dh; w = (int)((float)h * ar); }
    RECT r = { dst.left + (dw - w) / 2, dst.top + (dh - h) / 2, 0, 0 };
    r.right = r.left + w; r.bottom = r.top + h;

    HDC mem = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(mem, bmp);
    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, nullptr);
    switch (rotateDeg) {
    case 90: {
        POINT pts[3] = { { r.right - 1, r.top }, { r.right - 1, r.bottom - 1 }, { r.left, r.top } };
        PlgBlt(dc, pts, mem, 0, 0, sw, sh, nullptr, 0, 0);
        break;
    }
    case 180:
        StretchBlt(dc, r.right - 1, r.bottom - 1, -w, -h, mem, 0, 0, sw, sh, SRCCOPY);
        break;
    case 270: {
        POINT pts[3] = { { r.left, r.bottom - 1 }, { r.left, r.top }, { r.right - 1, r.bottom - 1 } };
        PlgBlt(dc, pts, mem, 0, 0, sw, sh, nullptr, 0, 0);
        break;
    }
    default:
        StretchBlt(dc, r.left, r.top, w, h, mem, 0, 0, sw, sh, SRCCOPY);
        break;
    }
    SelectObject(mem, old);
    DeleteDC(mem);
}

// ---------------------------------------------------------------------
// watermark colour swatches
// ---------------------------------------------------------------------
static void colourSwatches(Frame& f, int& r, int& g, int& b, int idBase, int width)
{
    static const COLORREF cols[] = {
        RGBv(0x9C,0xA3,0xAF), RGBv(0x14,0x18,0x24), RGBv(0xC4,0x2B,0x1C),
        RGBv(0x2E,0x5B,0xFF), RGBv(0x14,0x8A,0x4C), RGBv(0xB4,0x6A,0x00),
        RGBv(0x7C,0x4D,0xFF), RGBv(0xFF,0xFF,0xFF),
    };
    int n = (int)_countof(cols);
    int d = S(24), gap = S(8);
    int total = n * d + (n - 1) * gap;
    int x = f.top().x;
    int y = f.top().y;
    for (int i = 0; i < n; i++) {
        RECT rr = { x + i * (d + gap), y, x + i * (d + gap) + d, y + d };
        WidgetHit hit = ui::hitTest(f, idBase + i, rr);
        bool sel = (GetRValue(cols[i]) == r && GetGValue(cols[i]) == g && GetBValue(cols[i]) == b);
        if (!f.measure) {
            ui::fillRect(f.dc, rr, cols[i], d / 2);
            ui::strokeRect(f.dc, rr, sel ? f.pal.accent : f.pal.borderStrong, d / 2, sel ? S(2) : 1);
            if (hit.hovered) ui::strokeRect(f.dc, RECT{ rr.left - S(2), rr.top - S(2), rr.right + S(2), rr.bottom + S(2) },
                                            f.pal.accent, d / 2 + 2, S(2), 90);
            if (sel) {
                int is = S(12);
                drawIcon(f.dc, ICON_CHECK, RECT{ rr.left + (d - is) / 2, rr.top + (d - is) / 2,
                         rr.left + (d + is) / 2, rr.top + (d + is) / 2 },
                         (i == 7 || i == 0) ? RGB(0x33, 0x33, 0x33) : RGB(255, 255, 255), (float)S(2));
            }
        }
        if (hit.clicked) { r = GetRValue(cols[i]); g = GetGValue(cols[i]); b = GetBValue(cols[i]); }
    }
    ui::layoutGap(f, total);
    (void)width;
}

// ---------------------------------------------------------------------
// empty state: the drop zone
// ---------------------------------------------------------------------
static void drawDropZone(Frame& f, RECT r, const ToolDef& def, bool highlight)
{
    WidgetHit zone = ui::hitTest(f, ID_DROPZONE, r);
    bool hot = highlight || zone.hovered;

    int bw = S(158), bh = S(38);
    RECT br = { (r.left + r.right - bw) / 2, r.bottom - bh - S(22),
                (r.left + r.right + bw) / 2, r.bottom - S(22) };
    WidgetHit btn = ui::hitTest(f, ID_DROPZONE + 1, br);

    if (!f.measure) {
        ui::fillRect(f.dc, r, hot ? mixColor(f.pal.dropZone, f.pal.accent, 0.45f) : f.pal.dropZone, S(14));
        ui::strokeRect(f.dc, r, hot ? f.pal.accent : f.pal.borderStrong, S(14),
                       hot ? S(2) : 1, hot ? 255 : 170);

        int is = S(40);
        RECT ir = { (r.left + r.right - is) / 2, r.top + S(24),
                    (r.left + r.right + is) / 2, r.top + S(24) + is };
        ui::iconPlate(f.dc, ir, def.icon, def.tint, f.pal.dark, S(12));

        ui::drawText(f.dc, highlight ? L"Let go to add them" : L"Drag files anywhere in this window",
                     RECT{ r.left + S(24), ir.bottom + S(12), r.right - S(24), ir.bottom + S(36) },
                     f.pal.text, fontUI(14, FW_SEMIBOLD), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ui::drawText(f.dc, def.hint ? def.hint : L"",
                     RECT{ r.left + S(40), ir.bottom + S(36), r.right - S(40), ir.bottom + S(56) },
                     f.pal.textDim, fontUI(12), DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        COLORREF bg = btn.pressed ? mixColor(f.pal.accent, RGB(0, 0, 0), 0.14f)
                    : btn.hovered ? mixColor(f.pal.accent, RGB(255, 255, 255), 0.14f) : f.pal.accent;
        if (btn.hovered) ui::drawShadow(f.dc, br, S(9), S(10), 60);
        ui::fillRect(f.dc, br, bg, S(9));
        int bis = S(15);
        drawIcon(f.dc, ICON_ADD, RECT{ br.left + S(16), br.top + (bh - bis) / 2,
                 br.left + S(16) + bis, br.top + (bh + bis) / 2 }, RGB(255, 255, 255), (float)S(2));
        ui::drawText(f.dc, L"Choose files", RECT{ br.left + S(16) + bis + S(8), br.top, br.right - S(14), br.bottom },
                     RGB(255, 255, 255), fontUI(13, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    if ((btn.clicked || (zone.clicked && !btn.clicked)) && !f.measure) {
        std::vector<std::wstring> files;
        int kind0 = def.input == InputKind::Images ? 1 : def.input == InputKind::SingleDocx ? 2 : 0;
        if (pickFilesDialog(f.hwnd, files, kind0)) addFilesToCurrentTool(files);
    }
}

// ---------------------------------------------------------------------
// staged file list
// ---------------------------------------------------------------------
static void drawFileRows(Frame& f, ToolState& st, const ToolDef& def)
{
    int n = (int)st.files.size();
    int rowH = S(48);

    // header row: count (left) + add / clear (right)
    ui::beginRow(f, f.top().x, f.top().y, f.top().w, S(28), S(8));
    RECT midHdr = ui::takeFill(f, 0, S(224));
    if (!f.measure) {
        ui::drawText(f.dc, util::fmt(L"%d file%s in the list", n, n == 1 ? L"" : L"s"), midHdr,
                     f.pal.textDim, fontUI(12, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    WidgetHit clr, add;
    ui::button(f, ID_FILES + 2, L"Add files", BtnStyle::Ghost, 0, S(28), ICON_ADD, true, &add);
    ui::button(f, ID_FILES + 1, L"Clear list", BtnStyle::Ghost, 0, S(28), ICON_TRASH, true, &clr);
    ui::endLayout(f);
    if (add.clicked && !f.measure) {
        std::vector<std::wstring> more;
        int kind1 = def.input == InputKind::Images ? 1 : def.input == InputKind::SingleDocx ? 2 : 0;
        if (pickFilesDialog(f.hwnd, more, kind1)) addFilesToCurrentTool(more);
        return;
    }
    if (clr.clicked) {
        st.files.clear(); st.pagesLoaded = false; st.ran = false;
        st.order.clear(); st.rotation.clear(); st.selectedPages.clear();
        postRepaint();
        return;
    }

    for (int i = 0; i < n; i++) {
        StagedFile& sf = st.files[i];
        int base = ID_FILES + 100 + i * 30;

        ui::beginRow(f, f.top().x, f.top().y, f.top().w, rowH, S(10));

        if (def.id == ToolId::Merge) {
            ui::checkbox(f, base + 0, L"", sf.selected);
        } else {
            RECT chip = ui::takeRect(f, S(26), S(26));
            if (!f.measure) {
                ui::fillRect(f.dc, chip, f.pal.surfaceAlt, S(7));
                ui::drawText(f.dc, util::fmt(L"%d", i + 1), chip, f.pal.textDim,
                             fontUI(11, FW_SEMIBOLD), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        int nCtrl = (sf.locked || (sf.infoLoaded && sf.info.needsPassword)) ? 4 : 3;
        int ctrlW = nCtrl * (S(30) + S(10));
        RECT mid = ui::takeFill(f, 0, ctrlW);
        if (!f.measure) {
            int is = S(20);
            drawIcon(f.dc, def.input == InputKind::Images ? ICON_FILE_IMG : ICON_FILE_PDF,
                     RECT{ mid.left, mid.top + (rowH - is) / 2, mid.left + is, mid.top + (rowH + is) / 2 },
                     def.tint);
            RECT nr = { mid.left + is + S(10), mid.top + S(3), mid.right - S(6), mid.top + S(24) };
            ui::drawText(f.dc, util::fileName(sf.path), nr,
                         sf.locked ? f.pal.warning : f.pal.text, fontUI(13, FW_SEMIBOLD),
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            std::wstring meta;
            if (def.input == InputKind::SingleDocx)
                meta = util::fmt(L"Word document  \u00B7  %s", util::sizeText(sf.info.fileSize).c_str());
            else
                meta =
                (sf.infoLoaded && sf.info.valid)
                    ? util::fmt(L"%d pages  \u00B7  %s  \u00B7  %s", sf.info.pageCount,
                                sf.info.pageSize.c_str(), util::sizeText(sf.info.fileSize).c_str())
                    : (sf.locked ? L"password required" : L"this file could not be opened");
            ui::drawText(f.dc, meta, RECT{ mid.left + is + S(10), mid.top + S(24), mid.right - S(6), mid.bottom - S(3) },
                         sf.locked ? f.pal.warning : f.pal.textFaint, fontUI(11),
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        WidgetHit lk, up, dn, rm;
        if (sf.locked || (sf.infoLoaded && sf.info.needsPassword))
            ui::iconButton(f, base + 1, ICON_LOCK, S(30), BtnStyle::Ghost, true, L"Enter the password", &lk);
        ui::iconButton(f, base + 2, ICON_ARROW_UP, S(30), BtnStyle::Ghost, i > 0, L"Move up", &up);
        ui::iconButton(f, base + 3, ICON_ARROW_DOWN, S(30), BtnStyle::Ghost, i < n - 1, L"Move down", &dn);
        ui::iconButton(f, base + 4, ICON_TRASH, S(30), BtnStyle::Ghost, true, L"Remove from the list", &rm);
        ui::endLayout(f);

        if (lk.clicked) {
            std::wstring pw = sf.password;
            if (ui::inputBox(f.hwnd, L"Password required",
                             util::fmt(L"\u201C%s\u201D is protected. Enter its password.",
                                       util::fileName(sf.path).c_str()),
                             pw, true)) {
                sf.password = pw;
                std::wstring err;
                pdf::DocInfo info;
                if (pdf::readInfo(sf.path, pw, info, err)) {
                    sf.info = info; sf.locked = false; sf.infoLoaded = true;
                    st.pagesLoaded = false;
                } else {
                    showToast(L"That password did not work.", 3);
                }
                postRepaint();
            }
        }
        if (up.clicked && i > 0)        { std::swap(st.files[i], st.files[i - 1]); postRepaint(); }
        if (dn.clicked && i < n - 1)    { std::swap(st.files[i], st.files[i + 1]); postRepaint(); }
        if (rm.clicked) {
            st.files.erase(st.files.begin() + i);
            st.pagesLoaded = false;
            st.order.clear(); st.rotation.clear(); st.selectedPages.clear();
            postRepaint();
            break;
        }
    }
}

// ---------------------------------------------------------------------
// organiser: thumbnail grid with rotate / delete / select
// ---------------------------------------------------------------------
static void drawOrganiser(Frame& f, ToolState& st, const ToolDef& def, int idBase)
{
    AppState& a = app();
    (void)def;
    ensurePagesLoaded(st);
    if (st.pages.empty()) {
        wrappedNote(f, L"Add a PDF first and its pages will appear here, ready to rotate, reorder or delete.");
        return;
    }

    // ---- toolbar -------------------------------------------------------
    ui::beginRow(f, f.top().x, f.top().y, f.top().w, S(34), S(6));
    WidgetHit rl, rr, del, sel;
    ui::iconButton(f, idBase + 1, ICON_ROTATE, S(32), BtnStyle::Ghost, true, L"Rotate the selection left", &rl);
    ui::iconButton(f, idBase + 2, ICON_ROTATE, S(32), BtnStyle::Ghost, true, L"Rotate the selection right", &rr);
    ui::iconButton(f, idBase + 3, ICON_REMOVE, S(32), BtnStyle::Ghost, !st.selectedPages.empty(),
                   L"Delete the selected pages", &del);
    ui::button(f, idBase + 4, st.selectedPages.empty() ? L"Select all" : L"Clear selection",
               BtnStyle::Ghost, 0, S(32), ICON_CHECK, true, &sel);
    ui::slider(f, idBase + 90, a.organiserZoom, 90, 230, S(170));
    ui::endLayout(f);

    if (rl.clicked || rr.clicked) {
        int d = rl.clicked ? 270 : 90;
        if (st.selectedPages.empty()) {
            for (auto& rot : st.rotation) rot = (rot + d) % 360;
        } else {
            for (int pos : st.selectedPages)
                if (pos >= 0 && pos < (int)st.order.size())
                    st.rotation[st.order[(size_t)pos]] = (st.rotation[st.order[(size_t)pos]] + d) % 360;
        }
        postRepaint();
    }
    if (del.clicked && !st.selectedPages.empty()) {
        std::vector<int> keep;
        for (size_t pos = 0; pos < st.order.size(); pos++)
            if (!st.selectedPages.count((int)pos)) keep.push_back(st.order[pos]);
        st.order = keep;
        st.selectedPages.clear();
        postRepaint();
    }
    if (sel.clicked) {
        if (st.selectedPages.empty()) {
            for (size_t i = 0; i < st.order.size(); i++) st.selectedPages.insert((int)i);
        } else {
            st.selectedPages.clear();
        }
        postRepaint();
    }

    // ---- grid ----------------------------------------------------------
    int cw = S(a.organiserZoom);
    int ch = cw + S(30);
    ui::beginGrid(f, f.top().x, f.top().y + S(8), f.top().w, cw, ch, S(10), S(10));

    for (size_t pos = 0; pos < st.order.size(); pos++) {
        int pageIdx = st.order[pos];
        RECT r = ui::gridCell(f);
        WidgetHit hit = ui::hitTest(f, idBase + 100 + (int)pos, r);
        bool selected = st.selectedPages.count((int)pos) > 0;

        if (hit.clicked) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl) {
                if (!st.selectedPages.erase((int)pos)) st.selectedPages.insert((int)pos);
            } else {
                st.selectedPages.clear();
                st.selectedPages.insert((int)pos);
            }
            postRepaint();
        }

        if (hit.rightClick) {
            POINT sp = { f.mouse.x, f.mouse.y };
            ClientToScreen(f.hwnd, &sp);
            std::vector<ui::MenuItem> items = {
                { L"Rotate left",  ICON_ROTATE, 1, false, true, false },
                { L"Rotate right", ICON_ROTATE, 2, false, true, false },
                { L"",             ICON_NONE,   0, true,  true, false },
                { L"Delete page",  ICON_REMOVE, 3, false, true, false },
            };
            int choice = ui::popupMenu(f.hwnd, sp, items);
            if (choice == 1 || choice == 2) {
                int d = choice == 1 ? 270 : 90;
                st.rotation[pageIdx] = (st.rotation[pageIdx] + d) % 360;
                postRepaint();
            } else if (choice == 3) {
                st.order.erase(st.order.begin() + pos);
                st.selectedPages.clear();
                postRepaint();
            }
        }

        if (!f.measure && ui::rectVisible(f, r)) {
            RECT surf = { r.left, r.top, r.right, r.bottom - S(26) };
            if (hit.hovered) ui::drawShadow(f.dc, surf, S(8), S(8), f.pal.dark ? 60 : 30);
            ui::fillRect(f.dc, surf, selected ? f.pal.accentSoft : f.pal.surface, S(8));
            ui::strokeRect(f.dc, surf, selected ? f.pal.accent
                           : (hit.hovered ? f.pal.borderStrong : f.pal.border), S(8), selected ? S(2) : 1);

            const pdf::PageThumb& pt = st.pages[(size_t)pageIdx];
            int rot = (pt.rotation + st.rotation[(size_t)pageIdx]) % 360;
            RECT inner = { surf.left + S(7), surf.top + S(7), surf.right - S(7), surf.bottom - S(7) };
            HBITMAP bmp = ThumbCache::get().fetch(f.hwnd, st.files.front().path,
                                                  st.files.front().password, pageIdx,
                                                  S(a.organiserZoom) * 2);
            if (bmp) drawThumbBitmap(f.dc, bmp, inner, rot);
            else {
                int is = S(24);
                drawIcon(f.dc, ICON_FILE_PDF,
                         RECT{ (inner.left + inner.right - is) / 2, (inner.top + inner.bottom - is) / 2,
                               (inner.left + inner.right + is) / 2, (inner.top + inner.bottom + is) / 2 },
                         f.pal.textFaint);
            }

            if (!pt.readable) {
                ui::fillRect(f.dc, surf, f.pal.surfaceAlt, S(8));
                int ws = S(22);
                drawIcon(f.dc, ICON_WARN,
                         RECT{ (inner.left + inner.right - ws) / 2, (inner.top + inner.bottom - ws) / 2,
                               (inner.left + inner.right + ws) / 2, (inner.top + inner.bottom + ws) / 2 },
                         f.pal.warning);
            }

            RECT nb = { r.left + S(5), r.bottom - S(24), r.left + S(5) + S(36), r.bottom - S(4) };
            ui::fillRect(f.dc, nb, selected ? f.pal.accent : f.pal.surfaceAlt, S(6));
            ui::drawText(f.dc, util::fmt(L"%d", pageIdx + 1), nb,
                         selected ? RGB(255, 255, 255) : f.pal.textDim, fontUI(11, FW_SEMIBOLD),
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            if (st.rotation[(size_t)pageIdx] != 0) {
                int is = S(13);
                drawIcon(f.dc, ICON_ROTATE, RECT{ r.right - S(5) - is, r.bottom - S(22),
                         r.right - S(5), r.bottom - S(22) + is }, f.pal.accent, (float)S(2));
            }
        }
    }

    ui::endGrid(f);
}

// ---------------------------------------------------------------------
// option rows: "label ..... control"
// ---------------------------------------------------------------------
static void beginFieldRow(Frame& f, const wchar_t* text, int rowH = 0, int labelW = 0)
{
    int lw = labelW > 0 ? labelW : S(176);
    int rh = rowH > 0 ? rowH : S(34);
    ui::beginRow(f, f.top().x, f.top().y, f.top().w, rh, S(12));
    RECT l = ui::takeRect(f, lw, rh);
    if (!f.measure)
        ui::drawText(f.dc, text, l, f.pal.textDim, fontUI(12, FW_SEMIBOLD),
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void endFieldRow(Frame& f) { ui::endLayout(f); }

static void infoLine(Frame& f, const std::wstring& text, IconId icon = ICON_INFO)
{
    int h = S(22);
    RECT r = ui::takeRect(f, 0, h);
    if (f.measure || !ui::rectVisible(f, r)) return;
    int is = S(14);
    drawIcon(f.dc, icon, RECT{ r.left, r.top + (h - is) / 2, r.left + is, r.top + (h + is) / 2 }, f.pal.textFaint);
    ui::drawText(f.dc, text, RECT{ r.left + is + S(8), r.top, r.right, r.bottom },
                 f.pal.textFaint, fontUI(11), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static void wrappedNote(Frame& f, const std::wstring& text)
{
    ui::label(f, text, 11, f.pal.textFaint, FW_NORMAL, 0, true);
}

// ---------------------------------------------------------------------
// per-tool options
// ---------------------------------------------------------------------
static void drawToolOptions(Frame& f, ToolState& st, const ToolDef& def)
{
    int oid = ID_OPTIONS;

    switch (def.id) {

    case ToolId::Merge: {
        wrappedNote(f, L"Files are joined in the order shown above. Untick a file to skip it, and use the arrows to reorder.");
        break;
    }

    case ToolId::Split: {
        beginFieldRow(f, L"Mode");
        WidgetHit m1, m2, m3;
        ui::radioButton(f, oid + 1, L"One file per page", st.splitMode == pdf::SplitMode::OnePerFile, true, &m1);
        ui::radioButton(f, oid + 2, L"Every N pages", st.splitMode == pdf::SplitMode::EveryN, true, &m2);
        ui::radioButton(f, oid + 3, L"Custom ranges", st.splitMode == pdf::SplitMode::Ranges, true, &m3);
        endFieldRow(f);
        if (m1.clicked) st.splitMode = pdf::SplitMode::OnePerFile;
        if (m2.clicked) st.splitMode = pdf::SplitMode::EveryN;
        if (m3.clicked) st.splitMode = pdf::SplitMode::Ranges;

        if (st.splitMode == pdf::SplitMode::EveryN) {
            beginFieldRow(f, L"Pages per part");
            ui::spinner(f, oid + 10, st.everyN, 1, 999, S(120), L" pages");
            endFieldRow(f);
        }
        if (st.splitMode == pdf::SplitMode::Ranges) {
            beginFieldRow(f, L"Ranges");
            ui::textField(f, oid + 11, st.rangeSpec, S(320), L"1-3, 4, 7-9");
            endFieldRow(f);
            infoLine(f, L"Each range becomes its own PDF, e.g. \u201C1-3, 4\u201D gives two files.");
        }
        break;
    }

    case ToolId::RemovePages:
    case ToolId::ExtractPages: {
        beginFieldRow(f, L"Pages");
        ui::textField(f, oid + 20, st.rangeSpec, S(300), L"e.g. 1-3, 5, 8-10");
        WidgetHit ap;
        ui::button(f, oid + 21, L"All", BtnStyle::Ghost, 0, S(34), ICON_ADD, true, &ap);
        endFieldRow(f);
        if (ap.clicked) {
            int count = st.files.empty() ? 0 : st.files.front().info.pageCount;
            std::wstring spec;
            for (int i = 1; i <= count; i++) spec += (i == 1 ? L"" : L",") + std::to_wstring(i);
            if (!spec.empty()) { st.rangeSpec = spec; postRepaint(); }
        }
        infoLine(f, def.id == ToolId::RemovePages
                    ? L"Listed pages are deleted; everything else is kept."
                    : L"Listed pages are written to a new PDF.");
        break;
    }

    case ToolId::Rotate: {
        beginFieldRow(f, L"Rotation");
        WidgetHit r1, r2, r3;
        ui::radioButton(f, oid + 30, L"90\u00B0 clockwise", st.stamp.rotation == 90, true, &r1);
        ui::radioButton(f, oid + 31, L"180\u00B0", st.stamp.rotation == 180, true, &r2);
        ui::radioButton(f, oid + 32, L"90\u00B0 counter-clockwise", st.stamp.rotation == 270, true, &r3);
        endFieldRow(f);
        if (r1.clicked) st.stamp.rotation = 90;
        if (r2.clicked) st.stamp.rotation = 180;
        if (r3.clicked) st.stamp.rotation = 270;
        beginFieldRow(f, L"Apply to");
        WidgetHit a1, a2;
        ui::radioButton(f, oid + 33, L"All pages", !st.exportAllPages, true, &a1);
        ui::radioButton(f, oid + 34, L"Selected pages only", st.exportAllPages, true, &a2);
        endFieldRow(f);
        if (a1.clicked) st.exportAllPages = false;
        if (a2.clicked) st.exportAllPages = true;
        if (st.exportAllPages)
            infoLine(f, L"Pick the pages in the grid above (Ctrl-click for several).");
        break;
    }

    case ToolId::Watermark: {
        beginFieldRow(f, L"Text");
        ui::textField(f, oid + 40, st.stamp.text, S(360), L"CONFIDENTIAL, draft, \u2026");
        endFieldRow(f);

        beginFieldRow(f, L"Font size");
        ui::spinner(f, oid + 41, st.stamp.fontSize, 6, 400, S(110), L" pt");
        endFieldRow(f);

        {
            std::wstring lb = util::fmt(L"Opacity \u00B7 %d%%", st.stamp.opacity);
            beginFieldRow(f, lb.c_str());
            ui::slider(f, oid + 42, st.stamp.opacity, 5, 100, S(240));
            endFieldRow(f);
        }
        {
            std::wstring lb = util::fmt(L"Angle \u00B7 %d\u00B0", st.stamp.rotation);
            beginFieldRow(f, lb.c_str());
            ui::slider(f, oid + 43, st.stamp.rotation, -90, 90, S(240));
            endFieldRow(f);
        }

        beginFieldRow(f, L"Colour");
        colourSwatches(f, st.stamp.colorR, st.stamp.colorG, st.stamp.colorB, oid + 50, S(280));
        endFieldRow(f);

        ui::checkbox(f, oid + 63, L"Draw behind the page content", st.stamp.behind);
        wrappedNote(f, L"The watermark is added as real vector text, so it stays crisp and searchable.");
        break;
    }

    case ToolId::PageNumbers: {
        beginFieldRow(f, L"Format");
        int selFmt = st.stamp.numberFormat == L"{n}" ? 0
                   : st.stamp.numberFormat == L"{n} / {N}" ? 2 : 1;
        ui::combo(f, oid + 70, { L"1, 2, 3 \u2026", L"Page 1 of 12", L"1 / 12" }, selFmt, S(220));
        st.stamp.numberFormat = selFmt == 0 ? L"{n}" : selFmt == 2 ? L"{n} / {N}" : L"Page {n} of {N}";
        endFieldRow(f);

        beginFieldRow(f, L"Position", S(120));
        {
            int gx = f.top().x, gy = f.top().y;
            int cell = S(34), gap = S(6);
            for (int k = 0; k < 9; k++) {
                int col = k % 3, row = k / 3;
                RECT r = { gx + col * (cell + gap), gy + row * (cell + gap),
                           gx + col * (cell + gap) + cell, gy + row * (cell + gap) + cell };
                WidgetHit hit = ui::hitTest(f, oid + 71 + k, r);
                bool sel = st.stamp.pageNumPos == k;
                if (hit.clicked) { st.stamp.pageNumPos = k; postRepaint(); }
                if (!f.measure) {
                    ui::fillRect(f.dc, r, sel ? f.pal.accent : (hit.hovered ? f.pal.surfaceHover : f.pal.surfaceAlt), S(7));
                    ui::strokeRect(f.dc, r, sel ? f.pal.accent : f.pal.border, S(7), sel ? S(2) : 1);
                    int dot = S(8);
                    RECT dr = { (r.left + r.right - dot) / 2, (r.top + r.bottom - dot) / 2,
                                (r.left + r.right + dot) / 2, (r.top + r.bottom + dot) / 2 };
                    ui::fillRect(f.dc, dr, sel ? RGB(255, 255, 255) : f.pal.textFaint, dot / 2);
                }
            }
        }
        endFieldRow(f);

        beginFieldRow(f, L"Start at");
        ui::spinner(f, oid + 85, st.stamp.pageNumStart, 1, 9999, S(110));
        endFieldRow(f);

        beginFieldRow(f, L"Margin");
        ui::spinner(f, oid + 86, st.stamp.pageNumMargin, 8, 200, S(110), L" pt");
        endFieldRow(f);

        beginFieldRow(f, L"Text size");
        ui::spinner(f, oid + 87, st.stamp.pageNumSize, 6, 72, S(110), L" pt");
        endFieldRow(f);

        beginFieldRow(f, L"Colour");
        colourSwatches(f, st.stamp.pageNumColorR, st.stamp.pageNumColorG, st.stamp.pageNumColorB, oid + 90, S(280));
        endFieldRow(f);

        {
            std::wstring lb = util::fmt(L"Opacity \u00B7 %d%%", st.stamp.pageNumOpacity);
            beginFieldRow(f, lb.c_str());
            ui::slider(f, oid + 44, st.stamp.pageNumOpacity, 10, 100, S(240));
            endFieldRow(f);
        }
        break;
    }

    case ToolId::Protect: {
        beginFieldRow(f, L"Password");
        ui::textField(f, oid + 100, st.protect.userPassword, S(260), L"open password", true);
        endFieldRow(f);
        beginFieldRow(f, L"Confirm");
        ui::textField(f, oid + 101, st.protectConfirm, S(260), L"repeat it", true);
        endFieldRow(f);
        if (!st.protect.userPassword.empty() && st.protect.userPassword != st.protectConfirm)
            infoLine(f, L"The two passwords do not match yet.", ICON_WARN);

        ui::checkbox(f, oid + 105, L"Allow printing", st.protect.allowPrinting);
        ui::checkbox(f, oid + 106, L"Allow copying text", st.protect.allowCopying);
        ui::checkbox(f, oid + 107, L"Allow editing", st.protect.allowEditing);
        ui::checkbox(f, oid + 104, L"Use strong 256-bit AES encryption", st.protect.use256);
        infoLine(f, st.protect.use256 ? L"AES-256, the strongest option PDFForge offers."
                                      : L"128-bit AES \u2014 compatible with very old readers.");
        break;
    }

    case ToolId::Unlock: {
        beginFieldRow(f, L"Password");
        ui::textField(f, oid + 110, st.unlockPassword, S(300), L"the document password", true);
        endFieldRow(f);
        wrappedNote(f, L"PDFForge removes the password so the file opens freely. Only unlock documents you are allowed to.");
        break;
    }

    case ToolId::Repair: {
        ui::checkbox(f, oid + 120, L"Rebuild and optimise the file structure while repairing", st.repairOptimise);
        wrappedNote(f, L"Re-reads the document and writes a clean copy. Damaged cross-reference tables and page trees are rebuilt.");
        break;
    }

    case ToolId::Flatten: {
        wrappedNote(f, L"Form fields, signatures and comments are baked into the page content, so the result can no longer be filled in or edited. Text stays selectable.");
        break;
    }

    case ToolId::PdfToImage: {
        beginFieldRow(f, L"Format");
        int fmt = st.imgExport.format == L"jpg" ? 1 : 0;
        ui::combo(f, oid + 140, { L"PNG (lossless)", L"JPEG (smaller)" }, fmt, S(200));
        st.imgExport.format = fmt == 1 ? L"jpg" : L"png";
        endFieldRow(f);

        beginFieldRow(f, L"Resolution");
        static const int dpis[] = { 72, 96, 150, 300, 600 };
        int dpiSel = 2;
        for (int i = 0; i < 5; i++) if (dpis[i] == st.imgExport.dpi) dpiSel = i;
        ui::combo(f, oid + 141, { L"72 dpi \u00B7 screen", L"96 dpi", L"150 dpi \u00B7 good print",
                                 L"300 dpi \u00B7 high quality", L"600 dpi \u00B7 archive" }, dpiSel, S(240));
        st.imgExport.dpi = dpis[util::clampv<int>(dpiSel, 0, 4)];
        endFieldRow(f);

        if (st.imgExport.format == L"jpg") {
            std::wstring lb = util::fmt(L"JPG quality \u00B7 %d", st.imgExport.jpegQuality);
            beginFieldRow(f, lb.c_str());
            ui::slider(f, oid + 42, st.imgExport.jpegQuality, 30, 100, S(240));
            endFieldRow(f);
        }
        ui::checkbox(f, oid + 142, L"Convert every page", st.exportAllPages);
        if (!st.exportAllPages) {
            beginFieldRow(f, L"Pages");
            ui::textField(f, oid + 143, st.rangeSpec, S(300), L"e.g. 1-3, 5");
            endFieldRow(f);
        }
        break;
    }

    case ToolId::ImageToPdf: {
        beginFieldRow(f, L"Page size");
        ui::combo(f, oid + 150, { L"Match each image", L"A4", L"US Letter" }, st.img2pdf.pageSize, S(220));
        endFieldRow(f);

        beginFieldRow(f, L"Orientation");
        WidgetHit o1, o2;
        ui::radioButton(f, oid + 151, L"Portrait", !st.img2pdf.landscape, true, &o1);
        ui::radioButton(f, oid + 152, L"Landscape", st.img2pdf.landscape, true, &o2);
        endFieldRow(f);
        if (o1.clicked) st.img2pdf.landscape = false;
        if (o2.clicked) st.img2pdf.landscape = true;

        beginFieldRow(f, L"Margin");
        int mg = st.img2pdf.marginPt / 8;
        ui::spinner(f, oid + 153, mg, 0, 12, S(110), L"");
        st.img2pdf.marginPt = mg * 8;
        endFieldRow(f);

        wrappedNote(f, L"Images keep their aspect ratio and are centred on each page.");
        break;
    }

    case ToolId::PdfToText: {
        ui::checkbox(f, oid + 160, L"Insert a form feed between pages", st.pageBreaks);
        wrappedNote(f, L"Text is extracted in reading order, page by page, into a single .txt file.");
        break;
    }

    case ToolId::PdfToHtml: {
        wrappedNote(f, L"Each page becomes an HTML file with positioned text and images, so the layout looks the same in a browser.");
        break;
    }

    case ToolId::Compress: {
        beginFieldRow(f, L"Level");
        int lv = (int)st.compress;
        ui::combo(f, oid + 170,
                  { L"Low \u00B7 keep quality", L"Medium \u00B7 recommended", L"High \u00B7 much smaller", L"Maximum \u00B7 smallest" },
                  lv, S(240));
        st.compress = (pdf::CompressLevel)lv;
        endFieldRow(f);
        static const wchar_t* hints[] = {
            L"Downsamples to 250 dpi and recompresses photos gently. Barely any visible change.",
            L"150 dpi photos, lossless where possible. The best balance for sharing by e-mail.",
            L"110 dpi photos at medium JPEG quality. Text stays perfectly sharp.",
            L"72 dpi photos at low quality. For archiving or very tight size limits.",
        };
        infoLine(f, hints[util::clampv<int>(lv, 0, 3)], ICON_COMPRESS);
        ui::checkbox(f, oid + 171, L"Also discard hidden structure data", st.repairOptimise);
        break;
    }

    case ToolId::Organise: {
        wrappedNote(f, L"Click pages to select them, then rotate or delete with the toolbar. The order you leave them in becomes the new document.");
        break;
    }

    case ToolId::Metadata: {
        if (!st.metaLoaded && !st.files.empty()) {
            pdf::DocInfo info;
            std::wstring err;
            if (pdf::readInfo(st.files.front().path, st.files.front().password, info, err)) {
                st.mTitle = info.title; st.mAuthor = info.author; st.mSubject = info.subject;
                st.mKeywords = info.keywords; st.mCreator = info.creator; st.mProducer = info.producer;
                st.metaLoaded = true;
            }
        }
        beginFieldRow(f, L"Title");     ui::textField(f, oid + 180, st.mTitle, S(360));     endFieldRow(f);
        beginFieldRow(f, L"Author");   ui::textField(f, oid + 181, st.mAuthor, S(360));    endFieldRow(f);
        beginFieldRow(f, L"Subject");  ui::textField(f, oid + 182, st.mSubject, S(360));   endFieldRow(f);
        beginFieldRow(f, L"Keywords"); ui::textField(f, oid + 183, st.mKeywords, S(360));  endFieldRow(f);
        beginFieldRow(f, L"Creator");  ui::textField(f, oid + 184, st.mCreator, S(360));   endFieldRow(f);
        WidgetHit rh;
        ui::button(f, oid + 185, L"Reload from the file", BtnStyle::Ghost, 0, S(32), ICON_REFRESH,
                   !st.files.empty(), &rh);
        if (rh.clicked) { st.metaLoaded = false; postRepaint(); }
        break;
    }

    default:
        break;
    }
}

static void drawOutputCard(Frame& f, ToolState& st, const ToolDef& def)
{
    AppState& a = app();
    std::wstring dir = st.outDir.empty() ? defaultOutputDir(a, st) : st.outDir;

    // ---- folder chip (click anywhere on it to browse) ----------------
    beginFieldRow(f, L"Save to", S(40));
    RECT chip = ui::takeFill(f, 0);
    WidgetHit ch = ui::hitTest(f, ID_OUTPUT + 1, chip);
    if (!f.measure) {
        ui::fillRect(f.dc, chip, ch.pressed ? f.pal.surfaceAlt
                     : (ch.hovered ? f.pal.surfaceHover : f.pal.surfaceAlt), S(9));
        ui::strokeRect(f.dc, chip, ch.hovered ? f.pal.borderStrong : f.pal.border, S(9), 1);
        int is = S(16);
        drawIcon(f.dc, ICON_FOLDER, RECT{ chip.left + S(12), chip.top + (S(40) - is) / 2,
                 chip.left + S(12) + is, chip.top + (S(40) + is) / 2 }, f.pal.textDim);
        ui::drawText(f.dc, dir, RECT{ chip.left + S(12) + is + S(9), chip.top, chip.right - S(90), chip.bottom },
                     f.pal.text, fontUI(12), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT bb = { chip.right - S(80), chip.top + S(6), chip.right - S(6), chip.bottom - S(6) };
        ui::fillRect(f.dc, bb, ch.hovered ? f.pal.accentSoft : f.pal.surface, S(7));
        ui::strokeRect(f.dc, bb, ch.hovered ? f.pal.accent : f.pal.border, S(7), 1);
        ui::drawText(f.dc, L"Browse\u2026", bb, ch.hovered ? f.pal.accent : f.pal.textDim,
                     fontUI(12, FW_SEMIBOLD), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (ch.clicked) {
        std::wstring picked = dir;
        if (pickFolderDialog(f.hwnd, picked)) {
            a.lastOutDir = picked;
            a.saveToSourceFolder = false;
            settings::writeStr(L"LastOutDir", picked);
            settings::writeInt(L"SaveToSource", 0);
            st.outDir = picked;
            postRepaint();
        }
    }
    endFieldRow(f);

    // ---- quick source-folder switch, aligned under the value column ---
    {
        int indent = S(176) + S(12);
        ui::beginRow(f, f.top().x + indent, f.top().y, std::max(0, f.top().w - indent), S(30), S(8));
        WidgetHit src;
        ui::button(f, ID_OUTPUT + 2,
                   a.saveToSourceFolder ? L"Always beside the source file" : L"Keep beside the source file",
                   BtnStyle::Ghost, 0, S(30),
                   a.saveToSourceFolder ? ICON_CHECK : ICON_FOLDER, true, &src);
        ui::endLayout(f);
        if (src.clicked) {
            a.saveToSourceFolder = true;
            settings::writeInt(L"SaveToSource", 1);
            st.outDir.clear();
            postRepaint();
        }
    }

    // ---- resulting file name -------------------------------------------
    if (!def.multiOut && !st.files.empty()) {
        const wchar_t* ext = def.id == ToolId::PdfToText ? L".txt"
                           : def.id == ToolId::PdfToHtml ? L".html"
                           : def.id == ToolId::PdfToWord ? L".docx" : L".pdf";
        std::wstring out = defaultOutputFile(a, st, ext);
        beginFieldRow(f, L"File name");
        RECT r = ui::takeFill(f, 0);
        if (!f.measure)
            ui::drawText(f.dc, util::fileName(out), r, f.pal.textDim, fontUI(12),
                         DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        endFieldRow(f);
    }

    ui::checkbox(f, ID_OUTPUT + 3, L"Open the result when it finishes", a.openAfterRun);
    if (def.multiOut)
        infoLine(f, L"Several files will be written into the folder above.", ICON_LAYERS);
}

// ---------------------------------------------------------------------
// action bar (always visible at the bottom of the tool view)
// ---------------------------------------------------------------------
static void drawActionBar(Frame& f, RECT area, ToolState& st, const ToolDef& def)
{
    AppState& a = app();
    JobRunner& jr = JobRunner::get();

    HBRUSH b = CreateSolidBrush(f.pal.surface);
    FillRect(f.dc, &area, b);
    DeleteObject(b);
    ui::drawDivider(f.dc, area.left, area.top, area.right, area.top, f.pal.border);
    if (!f.measure) ui::drawShadow(f.dc, RECT{ area.left, area.top - S(10), area.right, area.top }, S(0), S(10), 18);

    int padX = S(28);
    int maxW = S(1080);
    int contentW = std::min<int>(area.right - area.left - padX * 2, maxW);
    int cx = area.left + (area.right - area.left - contentW) / 2;
    int cy = (area.top + area.bottom) / 2;

    if (jr.busy()) {
        // ---- running -------------------------------------------------
        int barW = contentW - S(140);
        RECT pb = { cx, cy - S(5), cx + barW, cy + S(5) };
        if (!f.measure) {
            ui::drawText(f.dc, jr.title(), RECT{ cx, area.top + S(12), cx + barW, area.top + S(30) },
                         f.pal.text, fontUI(13, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ui::drawText(f.dc, jr.step(), RECT{ cx, area.top + S(12), cx + barW, area.top + S(30) },
                         f.pal.textDim, fontUI(12), DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            ui::fillRect(f.dc, pb, f.pal.surfaceAlt, S(5));
            int pct = util::clampv<int>(jr.percent(), 0, 100);
            if (pct > 0) {
                int fw = std::max<int>(S(10), (int)((pb.right - pb.left) * pct / 100.0f));
                ui::fillGradient(f.dc, RECT{ pb.left, pb.top, pb.left + fw, pb.bottom },
                                 f.pal.accent, mixColor(f.pal.accent, RGB(255, 255, 255), 0.25f), false, S(5));
            }
            ui::strokeRect(f.dc, pb, f.pal.border, S(5), 1);
            ui::drawText(f.dc, util::fmt(L"%d%%", pct), RECT{ cx, pb.bottom + S(2), cx + barW, area.bottom - S(8) },
                         f.pal.textFaint, fontUI(11), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        WidgetHit ch;
        ui::beginRow(f, cx + barW + S(16), cy - S(17), contentW - barW - S(16), S(34), 0);
        ui::button(f, ID_ACTION + 1, L"Cancel", BtnStyle::Secondary, 0, S(34), ICON_STOP, true, &ch);
        ui::endLayout(f);
        if (ch.clicked) { jr.requestCancel(); showToast(L"Cancelling\u2026", 2); }
        return;
    }

    if (st.ran && st.result.ok) {
        // ---- success -------------------------------------------------
        int n = (int)st.result.outputs.size();
        if (!f.measure) {
            int is = S(22);
            RECT ir = { cx, cy - is / 2 - S(6), cx + is, cy + is / 2 - S(6) };
            ui::fillRect(f.dc, ir, f.pal.successSoft, is / 2);
            int cs = S(14);
            drawIcon(f.dc, ICON_CHECK, RECT{ ir.left + (is - cs) / 2, ir.top + (is - cs) / 2,
                     ir.left + (is + cs) / 2, ir.top + (is + cs) / 2 }, f.pal.success, (float)S(2));
            std::wstring head = n == 1 ? L"Done \u2014 file saved" : util::fmt(L"Done \u2014 %d files saved", n);
            ui::drawText(f.dc, head, RECT{ ir.right + S(12), area.top + S(12), cx + S(520), area.top + S(34) },
                         f.pal.text, fontUI(14, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            std::wstring sub = n == 1 ? st.result.outputs[0]
                                      : util::fmt(L"In %s", util::dirName(st.result.outputs[0]).c_str());
            ui::drawText(f.dc, sub, RECT{ ir.right + S(12), area.top + S(34), cx + S(560), area.bottom - S(12) },
                         f.pal.textDim, fontUI(11), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (st.result.bytesBefore && st.result.bytesAfter &&
                (def.id == ToolId::Compress || def.id == ToolId::Repair)) {
                double pct = 100.0 - (double)st.result.bytesAfter * 100.0 / (double)st.result.bytesBefore;
                std::wstring t = pct >= 0 ? util::fmt(L"%.1f%% smaller", pct) : util::fmt(L"%.1f%% larger", -pct);
                SIZE sz = ui::textSize(f.dc, t, fontUI(11, FW_SEMIBOLD));
                RECT pill = { cx + S(580), cy - S(13), cx + S(580) + (int)sz.cx + S(20), cy + S(13) };
                if (pill.right < area.right - S(200)) {
                    ui::fillRect(f.dc, pill, pct >= 0 ? f.pal.successSoft : f.pal.warningSoft, S(13));
                    ui::drawText(f.dc, t, pill, pct >= 0 ? f.pal.success : f.pal.warning, fontUI(11, FW_SEMIBOLD),
                                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                }
            }
        }
        ui::beginRow(f, area.right - padX - S(380), cy - S(18), S(380), S(36), S(8));
        WidgetHit h1, h2, h3;
        ui::button(f, ID_ACTION + 2, n == 1 ? L"Open file" : L"Open folder", BtnStyle::Primary, 0, S(36),
                   n == 1 ? ICON_EXTERNAL : ICON_FOLDER, true, &h1);
        ui::button(f, ID_ACTION + 3, L"Save as\u2026", BtnStyle::Secondary, 0, S(36), ICON_SAVE, n == 1, &h2);
        ui::button(f, ID_ACTION + 4, L"Start over", BtnStyle::Ghost, 0, S(36), ICON_REFRESH, true, &h3);
        ui::endLayout(f);

        if (h1.clicked && !st.result.outputs.empty()) {
            if (n == 1) ShellExecuteW(a.hwnd, L"open", st.result.outputs[0].c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            else {
                std::wstring dir = util::dirName(st.result.outputs[0]);
                ShellExecuteW(a.hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        if (h2.clicked && n == 1) {
            std::wstring src = st.result.outputs[0];
            std::wstring target = src;
            const wchar_t* filt = util::endsWith(src, L".txt") ? L"Text files (*.txt)\0*.txt\0All files\0*.*\0"
                                : util::endsWith(src, L".html") ? L"HTML files (*.html)\0*.html\0All files\0*.*\0"
                                : util::endsWith(src, L".png") ? L"PNG images (*.png)\0*.png\0All files\0*.*\0"
                                : util::endsWith(src, L".jpg") ? L"JPEG images (*.jpg)\0*.jpg\0All files\0*.*\0"
                                : L"PDF files (*.pdf)\0*.pdf\0All files\0*.*\0";
            if (pickSaveDialog(a.hwnd, target, util::fileName(src), filt)) {
                if (CopyFileW(src.c_str(), target.c_str(), FALSE))
                    showToast(util::fmt(L"Saved to %s", util::fileName(target).c_str()));
                else showToast(L"Could not copy the file there.", 3);
            }
        }
        if (h3.clicked) {
            st.ran = false; st.result = pdf::Result();
            postRepaint();
        }
        return;
    }

    if (st.ran && !st.result.ok) {
        // ---- failure --------------------------------------------------
        if (!f.measure) {
            int is = S(22);
            RECT ir = { cx, cy - is / 2 - S(6), cx + is, cy + is / 2 - S(6) };
            ui::fillRect(f.dc, ir, f.pal.dangerSoft, is / 2);
            int cs = S(14);
            drawIcon(f.dc, ICON_WARN, RECT{ ir.left + (is - cs) / 2, ir.top + (is - cs) / 2,
                     ir.left + (is + cs) / 2, ir.top + (is + cs) / 2 }, f.pal.danger);
            std::wstring head = st.result.error == L"cancelled" ? L"Cancelled" : L"Something went wrong";
            ui::drawText(f.dc, head, RECT{ ir.right + S(12), area.top + S(12), cx + S(620), area.top + S(34) },
                         f.pal.text, fontUI(14, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            std::wstring sub = st.result.error == L"cancelled" ? L"The job was stopped. Nothing was written."
                                                               : st.result.error;
            RECT sr = { ir.right + S(12), area.top + S(34), cx + S(620), area.bottom - S(10) };
            HGDIOBJ of = SelectObject(f.dc, fontUI(11));
            SetBkMode(f.dc, TRANSPARENT); SetTextColor(f.dc, f.pal.textDim);
            DrawTextW(f.dc, const_cast<LPWSTR>(sub.c_str()), -1, &sr, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
            SelectObject(f.dc, of);
        }
        ui::beginRow(f, area.right - padX - S(220), cy - S(18), S(220), S(36), S(8));
        WidgetHit h1;
        ui::button(f, ID_ACTION + 5, L"Try again", BtnStyle::Primary, 0, S(36), ICON_REFRESH, true, &h1);
        ui::endLayout(f);
        if (h1.clicked) { st.ran = false; st.result = pdf::Result(); runCurrentTool(); }
        return;
    }

    // ---- idle ---------------------------------------------------------
    bool canRun = !st.files.empty();
    if (def.input == InputKind::MultiPdf && st.files.size() < 2) canRun = false;
    if (!canRun && !f.measure) {
        ui::drawText(f.dc, def.input == InputKind::MultiPdf ? L"Add at least two files to continue"
                                                            : L"Add a file to continue",
                     RECT{ cx, area.top, cx + S(460), area.bottom }, f.pal.textFaint, fontUI(12),
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    ui::beginRow(f, area.right - padX - S(460), cy - S(23), S(460), S(46), S(10));
    WidgetHit runHit, clearHit;
    ui::button(f, ID_ACTION + 6, def.action, BtnStyle::Primary, S(200), S(46), ICON_SPARKLE, canRun, &runHit);
    ui::button(f, ID_ACTION + 7, L"Clear", BtnStyle::Ghost, 0, S(46), ICON_TRASH, !st.files.empty(), &clearHit);
    ui::endLayout(f);

    if (runHit.clicked) runCurrentTool();
    if (clearHit.clicked) {
        st.files.clear(); st.pages.clear(); st.order.clear(); st.rotation.clear();
        st.selectedPages.clear(); st.pagesLoaded = false; st.ran = false;
        postRepaint();
    }
}

// ---------------------------------------------------------------------
// the tool view
// ---------------------------------------------------------------------
void paintTool(Frame& f, RECT area)
{
    AppState& a = app();
    ToolState& st = a.cur();
    const ToolDef& def = toolDef(a.tool);

    int headerH = S(74);
    int actionH = S(84);
    RECT header = { area.left, area.top, area.right, area.top + headerH };
    RECT body = { area.left, header.bottom, area.right, area.bottom - actionH };
    RECT action = { area.left, body.bottom, area.right, area.bottom };

    // ---------- header ------------------------------------------------
    if (!f.measure) {
        int plate = S(42);
        RECT pr = { area.left + S(28), header.top + (headerH - plate) / 2,
                    area.left + S(28) + plate, header.top + (headerH + plate) / 2 };
        ui::iconPlate(f.dc, pr, def.icon, def.tint, f.pal.dark, S(12));
        int tx = pr.right + S(16);
        ui::drawText(f.dc, def.name, RECT{ tx, header.top + S(14), area.right - S(200), header.top + S(40) },
                     f.pal.text, fontUI(19, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        ui::drawText(f.dc, def.blurb, RECT{ tx, header.top + S(40), area.right - S(120), header.top + S(62) },
                     f.pal.textDim, fontUI(12), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // offline pill on the right
        std::wstring pill = L"100% offline";
        SIZE sz = ui::textSize(f.dc, pill, fontUI(11, FW_SEMIBOLD));
        int is = S(12);
        RECT pl = { area.right - S(28) - (int)sz.cx - S(24) - is, header.top + (headerH - S(26)) / 2,
                    area.right - S(28), 0 };
        pl.bottom = pl.top + S(26);
        ui::fillRect(f.dc, pl, f.pal.successSoft, S(13));
        drawIcon(f.dc, ICON_SHIELD, RECT{ pl.left + S(9), pl.top + (S(26) - is) / 2, pl.left + S(9) + is, pl.top + (S(26) + is) / 2 },
                 f.pal.success);
        ui::drawText(f.dc, pill, RECT{ pl.left + S(9) + is + S(6), pl.top, pl.right - S(10), pl.bottom },
                     f.pal.success, fontUI(11, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        ui::drawDivider(f.dc, area.left, header.bottom - 1, area.right, header.bottom - 1, f.pal.border);
    }

    // ---------- body (two pass) ---------------------------------------
    int padX = S(28);
    int maxW = S(1080);
    int contentW = std::min<int>(body.right - body.left - padX * 2, maxW);
    int cx = body.left + (body.right - body.left - contentW) / 2;
    int barW = S(14);

    // apply the wheel using the range measured last frame
    if (f.wheelDelta) {
        int step = std::max<int>(S(48), (body.bottom - body.top) / 6);
        a.toolScroll = util::clampv<int>(a.toolScroll - (f.wheelDelta / 120) * step, 0, std::max(0, a.toolScrollRange));
        f.wheelDelta = 0;
    }

    Cards cards;
    auto buildBody = [&](bool measure) {
        f.measure = measure;
        cards.idx = 0;
        if (measure) cards.rects.clear();
        ui::beginColumn(f, cx, body.top - a.toolScroll + S(18), contentW - (f.scrollVisible ? barW : 0), 1 << 20, 0);

        // card 1: files
        {
            CardScope cs = cardBegin(f, cards, def.input == InputKind::Images ? L"Add your images" : L"Add your PDFs", 1,
                                     st.files.empty() ? L"Drag and drop works anywhere in this window" : L"");
            if (st.files.empty()) {
                RECT dz = ui::takeRect(f, 0, S(196));
                drawDropZone(f, dz, def, a.dragOver);
            } else {
                drawFileRows(f, st, def);
            }
            cardEnd(f, cards, cs);
        }

        // card 2: organiser
        if (def.usesOrganiser) {
            CardScope cs = cardBegin(f, cards, L"Arrange pages", 2,
                                     util::fmt(L"%d page%s", (int)st.order.size(), st.order.size() == 1 ? L"" : L"s"));
            drawOrganiser(f, st, def, ID_ORGANISER + 600);
            cardEnd(f, cards, cs);
        }

        // card 3: options
        int stepNo = def.usesOrganiser ? 3 : 2;
        if (def.id != ToolId::PdfToHtml || true) {
            CardScope cs = cardBegin(f, cards, L"Options", stepNo, L"");
            drawToolOptions(f, st, def);
            cardEnd(f, cards, cs);
        }

        // card 4: output
        CardScope cs = cardBegin(f, cards, L"Output", stepNo + 1, L"");
        drawOutputCard(f, st, def);
        cardEnd(f, cards, cs);

        ui::layoutGap(f, S(24));
        int endY = f.top().y;
        ui::endLayout(f);
        f.measure = false;
        return endY;
    };

    int endY = buildBody(true);
    int contentH = (endY + a.toolScroll) - body.top - S(18);
    a.toolScrollRange = std::max(0, contentH - (int)(body.bottom - body.top));
    a.toolScroll = util::clampv<int>(a.toolScroll, 0, a.toolScrollRange);

    buildBody(false);

    // scrollbar
    if (a.toolScrollRange > 0) {
        f.scrollVisible = true;
        RECT track = { body.right - barW - S(3), body.top + S(6), body.right - S(3), body.bottom - S(6) };
        a.toolScroll = ui::scrollbar(f, 7777, track, contentH, (int)(body.bottom - body.top), a.toolScroll);
    } else f.scrollVisible = false;

    // ---------- action bar --------------------------------------------
    f.measure = false;
    drawActionBar(f, action, st, def);
}

} // namespace views
