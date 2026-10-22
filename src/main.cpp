// =====================================================================
//  MrDocx  -  main.cpp : window, custom frame, input, message loop
//
//  The whole user interface is drawn by hand with GDI/GDI+ into a
//  double buffer.  There are no child controls, no web view and no
//  third party widget kit: one window, one paint function, one loop.
//
//  Author : Ragul S
// =====================================================================
#include "common.h"
#include "theme.h"
#include "icons.h"
#include "ui.h"
#include "app.h"
#include "cards.h"

#include <gdiplus.h>
#include <commdlg.h>
#include <shobjidl.h>
#include <dwmapi.h>

using namespace Gdiplus;

// ---------------------------------------------------------------------
// window level globals
// ---------------------------------------------------------------------
namespace {

Frame   g_frame;                       // persistent immediate-mode state

HDC     g_memDC  = nullptr;            // double buffer
HBITMAP g_memBmp = nullptr;
HGDIOBJ g_memOld = nullptr;
int     g_memW = 0, g_memH = 0;

POINT   g_mouse = { 0, 0 };
bool    g_mouseInside = false;
bool    g_leftDown = false;
bool    g_doublePressed = false;
bool    g_selfRelease = false;   // true while WE call ReleaseCapture(); ignores our own WM_CAPTURECHANGED

bool    g_painting = false;
bool    g_repaintPending = false;

bool    g_caretTimer = false;
bool    g_toastTimer = false;
bool    g_tipTimer   = false;

// tooltip hover delay
std::wstring g_tipText;
RECT    g_tipRect  = { 0, 0, 0, 0 };
DWORD   g_tipSince = 0;
bool    g_tipShown = false;

// caption / title bar buttons are drawn by us, so clicks are ours too
enum CapBtn { CAP_NONE = 0, CAP_MIN, CAP_MAX, CAP_CLOSE, CAP_THEME, CAP_BACK };
CapBtn  g_capPressed = CAP_NONE;

const UINT TIMER_CARET = 1;
const UINT TIMER_TOAST = 2;
const UINT TIMER_TIP   = 3;

const int MIN_W = 940;
const int MIN_H = 600;

void forwardPaint(HWND hwnd);

// ---------------------------------------------------------------------
// back buffer
// ---------------------------------------------------------------------
void ensureBackBuffer(HDC ref, int w, int h)
{
    if (g_memDC && g_memW >= w && g_memH >= h) return;
    if (g_memDC) {
        SelectObject(g_memDC, g_memOld);
        DeleteObject(g_memBmp);
        DeleteDC(g_memDC);
        g_memDC = nullptr; g_memBmp = nullptr; g_memOld = nullptr;
    }
    g_memW = std::max(w, 64);
    g_memH = std::max(h, 64);
    g_memDC = CreateCompatibleDC(ref);
    g_memBmp = CreateCompatibleBitmap(ref, g_memW, g_memH);
    g_memOld = SelectObject(g_memDC, g_memBmp);
    SetStretchBltMode(g_memDC, HALFTONE);
    SetBkMode(g_memDC, TRANSPARENT);
}

void freeBackBuffer()
{
    if (g_memDC) {
        SelectObject(g_memDC, g_memOld);
        DeleteObject(g_memBmp);
        DeleteDC(g_memDC);
        g_memDC = nullptr; g_memBmp = nullptr; g_memOld = nullptr;
        g_memW = g_memH = 0;
    }
}

// ---------------------------------------------------------------------
// geometry helpers shared with the hit tester
// ---------------------------------------------------------------------
int  sidebarW()      { return S(app().sidebarWidth); }
bool inRect(RECT r, POINT p) { return r.right > r.left && r.bottom > r.top && PtInRect(&r, p); }

CapBtn captionAt(POINT p)
{
    AppState& a = app();
    if (inRect(a.btnClose, p)) return CAP_CLOSE;
    if (inRect(a.btnMax, p))   return CAP_MAX;
    if (inRect(a.btnMin, p))   return CAP_MIN;
    if (inRect(a.btnTheme, p)) return CAP_THEME;
    if (inRect(a.btnBack, p))  return CAP_BACK;
    return CAP_NONE;
}

void actOnCaption(HWND hwnd, CapBtn b)
{
    AppState& a = app();
    switch (b) {
    case CAP_CLOSE: PostMessageW(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0); break;
    case CAP_MAX:   PostMessageW(hwnd, WM_SYSCOMMAND, a.maximized ? SC_RESTORE : SC_MAXIMIZE, 0); break;
    case CAP_MIN:   PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0); break;
    case CAP_THEME: setDarkTheme(!isDarkTheme()); break;
    case CAP_BACK:  goHome(); break;
    default: break;
    }
}

// ---------------------------------------------------------------------
// tooltip (delayed bubble drawn after the frame)
// ---------------------------------------------------------------------
void drawTooltip(Frame& f)
{
    const std::wstring& text = ui::pendingTooltip();
    RECT where = ui::pendingTooltipRect();

    if (text.empty()) {
        g_tipText.clear(); g_tipShown = false;
        if (g_tipTimer) { KillTimer(f.hwnd, TIMER_TIP); g_tipTimer = false; }
        return;
    }

    DWORD now = GetTickCount();
    if (text != g_tipText || !EqualRect(&where, &g_tipRect)) {
        g_tipText = text; g_tipRect = where; g_tipSince = now; g_tipShown = false;
    }
    if (!g_tipShown) {
        if (now - g_tipSince < 420) {
            if (!g_tipTimer) { SetTimer(f.hwnd, TIMER_TIP, 110, nullptr); g_tipTimer = true; }
            return;
        }
        g_tipShown = true;
        if (g_tipTimer) { KillTimer(f.hwnd, TIMER_TIP); g_tipTimer = false; }
    }

    HFONT font = fontUI(12);
    SIZE sz = ui::textSize(f.dc, text, font);
    int padX = S(10), padY = S(6);
    int w = (int)sz.cx + padX * 2;
    int h = std::max(S(26), (int)sz.cy + padY * 2);

    RECT r = { where.left + ((where.right - where.left) - w) / 2, where.bottom + S(7), 0, 0 };
    r.right = r.left + w; r.bottom = r.top + h;
    if (r.right > f.W - S(8)) { int d = r.right - (f.W - S(8)); r.left -= d; r.right -= d; }
    if (r.left < S(8))        { int d = S(8) - r.left;          r.left += d; r.right += d; }
    if (r.bottom > f.H - S(8)) {
        r.top = where.top - h - S(7);
        r.bottom = r.top + h;
    }

    ui::drawShadow(f.dc, r, S(7), S(10), f.pal.dark ? 110 : 60);
    ui::fillRect(f.dc, r, RGBv(0x1B, 0x1F, 0x2A), S(7));
    ui::strokeRect(f.dc, r, RGBv(0x3A, 0x41, 0x52), S(7), 1);
    ui::drawText(f.dc, text, r, RGB(245, 246, 248), font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---------------------------------------------------------------------
// toasts (bottom of the content area)
// ---------------------------------------------------------------------
void drawToasts(Frame& f, RECT area)
{
    AppState& a = app();
    DWORD now = GetTickCount();
    for (auto it = a.toasts.begin(); it != a.toasts.end(); ) {
        if ((int)(now - it->until) >= 0) it = a.toasts.erase(it);
        else ++it;
    }
    if (a.toasts.empty()) {
        if (g_toastTimer) { KillTimer(f.hwnd, TIMER_TOAST); g_toastTimer = false; }
        return;
    }
    if (!g_toastTimer) { SetTimer(f.hwnd, TIMER_TOAST, 200, nullptr); g_toastTimer = true; }

    int availW = area.right - area.left;
    int y = area.bottom - S(22);
    for (int i = (int)a.toasts.size() - 1; i >= 0; i--) {
        const AppState::Toast& t = a.toasts[(size_t)i];

        // fade out during the last 400 ms
        int remain = (int)(t.until - now);
        int alpha = remain < 400 ? std::max(0, remain * 255 / 400) : 255;
        if (alpha <= 0) continue;

        int is = S(16);
        SIZE sz = ui::textSize(f.dc, t.text, fontUI(13));
        int w = std::min((int)sz.cx + S(16) + is + S(11) + S(18), availW - S(48));
        int h = S(44);
        int cxm = area.left + availW / 2;
        RECT r = { cxm - w / 2, y - h, cxm - w / 2 + w, y };

        COLORREF accent = t.kind == 3 ? f.pal.danger
                        : t.kind == 2 ? f.pal.warning
                        : t.kind == 1 ? f.pal.success : f.pal.accent;

        ui::drawShadow(f.dc, r, S(11), S(14), f.pal.dark ? 90 : 44);
        ui::fillRect(f.dc, r, f.pal.surface, S(11));
        ui::strokeRect(f.dc, r, f.pal.border, S(11), 1, alpha);
        ui::fillRect(f.dc, RECT{ r.left + S(1), r.top + S(12), r.left + S(4), r.bottom - S(12) }, accent, S(2));

        IconId ic = t.kind == 3 ? ICON_WARN : t.kind == 2 ? ICON_WARN
                  : t.kind == 1 ? ICON_CHECK : ICON_INFO;
        RECT ir = { r.left + S(15), r.top + (h - is) / 2, r.left + S(15) + is, r.top + (h + is) / 2 };
        drawIcon(f.dc, ic, ir, accent, 0.0f, alpha);
        ui::drawText(f.dc, t.text, RECT{ ir.right + S(11), r.top, r.right - S(14), r.bottom },
                     f.pal.text, fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, alpha);

        y = r.top - S(9);
    }
}

// ---------------------------------------------------------------------
// the paint
// ---------------------------------------------------------------------
void paintWindow(HWND hwnd)
{
    AppState& a = app();
    RECT cr; GetClientRect(hwnd, &cr);
    int W = cr.right - cr.left, H = cr.bottom - cr.top;
    if (W <= 2 || H <= 2) return;

    HDC wdc = GetDC(hwnd);
    ensureBackBuffer(wdc, W, H);
    HDC dc = g_memDC;

    HBRUSH bg = CreateSolidBrush(currentPalette().background);
    FillRect(dc, &cr, bg);
    DeleteObject(bg);

    Frame& f = g_frame;
    f.caretVisible = false;

    ui::beginFrame(f, dc, hwnd, W, H, currentPalette(), g_mouse, g_mouseInside, cr);
    f.focusOrder.clear();
    f.doublePressed = g_doublePressed;   // beginFrame resets it; feed it back in

    // ---- layout regions ------------------------------------------------
    int sbw = sidebarW();
    int tbh = titleBarHeightPx();
    if (sbw > W - S(320)) sbw = std::max(0, W - S(320));
    RECT sideR    = { 0,   0,   sbw, H };
    RECT titleR   = { sbw, 0,   W,   tbh };
    RECT contentR = { sbw, tbh, W,   H };

    // root column: several views call takeRect()/endGrid() which need a stack
    ui::beginColumn(f, 0, 0, W, H, 0);

    views::paintSidebar(f, sideR);
    views::paintTitleBar(f, titleR);

    // ---- content, clipped to its own region ----------------------------
    RECT savedClip = f.clip;
    int savedTop = f.visibleTop, savedBottom = f.visibleBottom;
    IntersectClipRect(dc, contentR.left, contentR.top, contentR.right, contentR.bottom);
    f.clip = contentR;
    f.visibleTop = contentR.top;
    f.visibleBottom = contentR.bottom;

    switch (a.view) {
    case ViewId::Home:  views::paintHome(f, contentR);  break;
    case ViewId::Tool:  views::paintTool(f, contentR);  break;
    case ViewId::About: views::paintAbout(f, contentR); break;
    }

    SelectClipRgn(dc, nullptr);
    f.clip = savedClip;
    f.visibleTop = savedTop;
    f.visibleBottom = savedBottom;

    ui::endLayout(f);          // pop the root column

    // ---- overlays ------------------------------------------------------
    drawTooltip(f);
    drawToasts(f, contentR);

    ui::endFrame(f);

    BitBlt(wdc, 0, 0, W, H, dc, 0, 0, SRCCOPY);
    ReleaseDC(hwnd, wdc);

    // caret blink timer only while a text field really has the caret
    if (f.caretVisible && !g_caretTimer) { SetTimer(hwnd, TIMER_CARET, 530, nullptr); g_caretTimer = true; }
    if (!f.caretVisible && g_caretTimer) { KillTimer(hwnd, TIMER_CARET); g_caretTimer = false; ui::caretBlinkReset(); }
}

} // namespace

// ---------------------------------------------------------------------
// helpers declared in app.h and implemented here
// ---------------------------------------------------------------------
int titleBarHeightPx() { return views::titleBarH(); }

RECT captionButtonRect(int index)
{
    AppState& a = app();
    if (index == 0) return a.btnMin;
    if (index == 1) return a.btnMax;
    return a.btnClose;
}

void repaint()
{
    HWND hwnd = app().hwnd;
    if (!hwnd) return;
    if (g_painting) { g_repaintPending = true; return; }
    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
}

namespace {

void forwardPaint(HWND hwnd)
{
    if (g_painting) { g_repaintPending = true; return; }
    g_painting = true;
    paintWindow(hwnd);
    g_painting = false;
    g_doublePressed = false;
    if (g_repaintPending) {
        g_repaintPending = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    }
}

// ---------------------------------------------------------------------
// keyboard focus cycling
// ---------------------------------------------------------------------
void cycleFocus(bool backwards)
{
    Frame& f = g_frame;
    if (f.focusOrder.empty()) return;
    int n = (int)f.focusOrder.size();
    int idx = -1;
    for (int i = 0; i < n; i++) if (f.focusOrder[(size_t)i] == f.focusId) { idx = i; break; }
    if (idx < 0) idx = backwards ? n - 1 : 0;
    else idx = (idx + (backwards ? -1 : 1) + n) % n;
    f.focusId = f.focusOrder[(size_t)idx];
    f.tabPressed = true;
    f.tabShift = backwards;
}

// ---------------------------------------------------------------------
// "add files" from anywhere
// ---------------------------------------------------------------------
bool looksLikeImage(const std::wstring& path)
{
    std::wstring e = util::lower(util::extension(path));
    return e == L".jpg" || e == L".jpeg" || e == L".png" || e == L".bmp" ||
           e == L".tif" || e == L".tiff" || e == L".gif" || e == L".webp";
}

void ingestFiles(HWND hwnd, const std::vector<std::wstring>& files)
{
    if (files.empty()) return;
    AppState& a = app();

    if (a.view != ViewId::Tool) {
        bool allImages = true, allDocx = true;
        for (auto& p : files) {
            if (!looksLikeImage(p)) allImages = false;
            if (util::lower(util::extension(p)) != L".docx") allDocx = false;
        }
        openTool(allImages ? ToolId::ImageToPdf : allDocx ? ToolId::WordToPdf : ToolId::Merge);
    }
    addFilesToCurrentTool(files);
    (void)hwnd;
}

void openFilesDialog(HWND hwnd)
{
    AppState& a = app();
    int kind = 0;
    if (a.view == ViewId::Tool) {
        InputKind ik = toolDef(a.tool).input;
        kind = ik == InputKind::Images ? 1 : ik == InputKind::SingleDocx ? 2
             : ik == InputKind::SingleText ? 3 : 0;
    }
    std::vector<std::wstring> files;
    if (!pickFilesDialog(hwnd, files, kind)) return;
    ingestFiles(hwnd, files);
}

// ---------------------------------------------------------------------
// command line / WM_COPYDATA payload -> file list
// ---------------------------------------------------------------------
void splitPaths(const std::wstring& blob, std::vector<std::wstring>& out)
{
    size_t i = 0;
    while (i < blob.size()) {
        while (i < blob.size() && (blob[i] == L' ' || blob[i] == L'\t' ||
                                   blob[i] == L'\r' || blob[i] == L'\n' || blob[i] == L'\0')) i++;
        if (i >= blob.size()) break;
        std::wstring one;
        if (blob[i] == L'"') {
            i++;
            while (i < blob.size() && blob[i] != L'"') one += blob[i++];
            if (i < blob.size()) i++;
        } else {
            while (i < blob.size() && blob[i] != L' ' && blob[i] != L'\t' &&
                   blob[i] != L'\r' && blob[i] != L'\n' && blob[i] != L'\0') one += blob[i++];
        }
        if (!one.empty()) out.push_back(one);
    }
}

} // namespace

// ---------------------------------------------------------------------
// common dialogs
// ---------------------------------------------------------------------
bool pickFilesDialog(HWND owner, std::vector<std::wstring>& out, int kind)
{
    static wchar_t buf[120000];
    buf[0] = 0; buf[1] = 0;

    const wchar_t* pdfFilter =
        L"PDF documents (*.pdf)\0*.pdf\0"
        L"All files (*.*)\0*.*\0";
    const wchar_t* imgFilter =
        L"Images (*.jpg;*.jpeg;*.png;*.bmp;*.tif;*.tiff;*.gif)\0*.jpg;*.jpeg;*.png;*.bmp;*.tif;*.tiff;*.gif\0"
        L"All files (*.*)\0*.*\0";
    const wchar_t* docxFilter =
        L"Word documents (*.docx)\0*.docx\0"
        L"All files (*.*)\0*.*\0";
    const wchar_t* txtFilter =
        L"Text documents (*.txt;*.md)\0*.txt;*.md;*.markdown;*.log\0"
        L"All files (*.*)\0*.*\0";

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = app().instance;
    ofn.lpstrFilter = kind == 1 ? imgFilter : kind == 2 ? docxFilter : kind == 3 ? txtFilter : pdfFilter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD)_countof(buf);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT |
                OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    ofn.lpstrTitle = kind == 1 ? L"Choose images"
                   : kind == 2 ? L"Choose a Word document"
                   : kind == 3 ? L"Choose a text file" : L"Choose PDF files";

    if (!GetOpenFileNameW(&ofn)) return false;

    out.clear();
    const wchar_t* p = buf;
    std::wstring first = p;
    p += first.size() + 1;
    if (*p == 0) {
        out.push_back(first);
    } else {
        while (*p) {
            std::wstring name = p;
            out.push_back(first + L"\\" + name);
            p += name.size() + 1;
        }
    }
    return !out.empty();
}

bool pickFolderDialog(HWND owner, std::wstring& out)
{
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR);
    dlg->SetTitle(L"Choose where to save the result");

    bool ok = false;
    if (SUCCEEDED(dlg->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                out = path;
                CoTaskMemFree(path);
                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

bool pickSaveDialog(HWND owner, std::wstring& out, const std::wstring& suggested, const wchar_t* filter)
{
    static wchar_t buf[MAX_PATH * 2];
    wcsncpy_s(buf, _countof(buf), suggested.c_str(), _TRUNCATE);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.hInstance = app().instance;
    ofn.lpstrFilter = filter ? filter : L"All files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = (DWORD)_countof(buf);
    ofn.lpstrDefExt = L"";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER |
                OFN_NOCHANGEDIR | OFN_ENABLESIZING;
    ofn.lpstrTitle = L"Save as";

    if (!GetSaveFileNameW(&ofn)) return false;
    out = buf;
    return true;
}

// =====================================================================
//  window procedure
// =====================================================================
namespace {

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    AppState& a = app();
    Frame& f = g_frame;

    switch (msg) {

    // ------------------------------------------------------------------
    case WM_CREATE: {
        a.hwnd = hwnd;
        Dpi::get().init(hwnd);

        // settings
        bool dark = settings::readInt(L"DarkMode", 0) != 0;
        setDarkTheme(dark);
        a.sidebarWidth     = util::clampv<int>(settings::readInt(L"SidebarW", 232), 190, 320);
        a.lastOutDir       = settings::readStr(L"LastOutDir", L"");
        a.saveToSourceFolder = settings::readInt(L"SaveToSource", 1) != 0;
        a.openAfterRun     = settings::readInt(L"OpenAfterRun", 0) != 0;

        DragAcceptFiles(hwnd, TRUE);

        // keep the DWM border in step with the theme
        COLORREF border = dark ? RGB(0x0D, 0x10, 0x16) : RGB(0xE1, 0xE6, 0xEE);
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &border, sizeof(border));

        ThumbCache::get().start();
        return 0;
    }

    // ------------------------------------------------------------------
    // custom frame: keep the resize borders, drop the caption
    // ------------------------------------------------------------------
    case WM_NCCALCSIZE: {
        if (!wp) return DefWindowProcW(hwnd, msg, wp, lp);
        NCCALCSIZE_PARAMS* p = (NCCALCSIZE_PARAMS*)lp;
        RECT before = p->rgrc[0];
        LRESULT r = DefWindowProcW(hwnd, WM_NCCALCSIZE, wp, lp);
        if (r != 0) return r;
        if (IsZoomed(hwnd)) {
            int cx = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            int cy = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            p->rgrc[0] = before;
            p->rgrc[0].left += cx;  p->rgrc[0].right  -= cx;
            p->rgrc[0].top  += cy;  p->rgrc[0].bottom -= cy;
        } else {
            p->rgrc[0] = before;    // client area == whole window
        }
        return 0;
    }

    case WM_NCHITTEST: {
        POINT sp = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        POINT cp = sp;
        ScreenToClient(hwnd, &cp);

        RECT cr; GetClientRect(hwnd, &cr);
        int W = cr.right, H = cr.bottom;

        if (!a.maximized) {
            int b = GetSystemMetrics(SM_CXSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
            b = std::max(b, S(5));
            bool left = cp.x < b, right = cp.x >= W - b;
            bool top = cp.y < b, bottom = cp.y >= H - b;
            if (top && left)      return HTTOPLEFT;
            if (top && right)     return HTTOPRIGHT;
            if (bottom && left)   return HTBOTTOMLEFT;
            if (bottom && right)  return HTBOTTOMRIGHT;
            if (left)             return HTLEFT;
            if (right)            return HTRIGHT;
            if (bottom)           return HTBOTTOM;
            if (top)              return HTTOP;
        }

        int sbw = sidebarW(), tbh = titleBarHeightPx();
        if (cp.y < tbh) {
            if (cp.x >= sbw) {
                // our own buttons stay client side so they animate
                if (captionAt(cp) != CAP_NONE) return HTCLIENT;
                return HTCAPTION;
            }
            if (cp.y < S(58)) return HTCAPTION;   // brand plate doubles as a drag handle
        }
        return HTCLIENT;
    }

    case WM_NCACTIVATE:
        return DefWindowProcW(hwnd, msg, wp, (LPARAM)-1);

    case WM_ACTIVATE:
        a.active = (LOWORD(wp) != WA_INACTIVE);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    // ------------------------------------------------------------------
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        forwardPaint(hwnd);
        EndPaint(hwnd, &ps);
        (void)wdc;
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE: {
        a.maximized = (wp == SIZE_MAXIMIZED);
        if (wp != SIZE_MINIMIZED) InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOVE:
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = S(MIN_W);
        mmi->ptMinTrackSize.y = S(MIN_H);
        return 0;
    }

    case WM_DPICHANGED: {
        UINT dpi = LOWORD(wp);
        Dpi::get().set((int)dpi);
        FontCache::get().destroy();
        ThumbCache::get().clear();
        freeBackBuffer();
        RECT* sug = (RECT*)lp;
        SetWindowPos(hwnd, nullptr, sug->left, sug->top,
                     sug->right - sug->left, sug->bottom - sug->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }

    // ------------------------------------------------------------------
    // mouse -> immediate mode frame
    // ------------------------------------------------------------------
    case WM_MOUSEMOVE: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (p.x != g_mouse.x || p.y != g_mouse.y || !g_mouseInside) {
            g_mouse = p;
            g_mouseInside = true;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            forwardPaint(hwnd);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        g_mouseInside = false;
        g_capPressed = CAP_NONE;
        forwardPaint(hwnd);
        return 0;

    case WM_NCMOUSEMOVE: {
        POINT sp = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        POINT p = sp; ScreenToClient(hwnd, &p);
        if (p.x != g_mouse.x || p.y != g_mouse.y || !g_mouseInside) {
            g_mouse = p; g_mouseInside = true;
            forwardPaint(hwnd);
        }
        return 0;
    }

    case WM_NCMOUSELEAVE:
        g_mouseInside = false;
        forwardPaint(hwnd);
        return 0;

    case WM_LBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        g_mouse = p; g_mouseInside = true;
        g_leftDown = true;
        f.leftDown = true;
        f.leftPressed = true;
        g_capPressed = captionAt(p);
        SetCapture(hwnd);
        forwardPaint(hwnd);
        return 0;
    }

    case WM_LBUTTONUP: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        g_mouse = p; g_mouseInside = true;
        g_leftDown = false;
        f.leftDown = false;
        f.leftReleased = true;
        CapBtn pressed = g_capPressed;
        g_capPressed = CAP_NONE;
        g_selfRelease = true;
        ReleaseCapture();
        g_selfRelease = false;
        forwardPaint(hwnd);
        if (pressed != CAP_NONE && captionAt(p) == pressed) actOnCaption(hwnd, pressed);
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        g_mouse = p; g_mouseInside = true;
        g_leftDown = true;
        f.leftDown = true;
        f.leftPressed = true;
        g_doublePressed = true;
        SetCapture(hwnd);
        forwardPaint(hwnd);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        g_mouse = p; g_mouseInside = true;
        f.rightPressed = true;
        forwardPaint(hwnd);
        return 0;
    }

    case WM_RBUTTONUP:
        return 0;

    case WM_MOUSEWHEEL: {
        POINT sp = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        POINT p = sp; ScreenToClient(hwnd, &p);
        g_mouse = p; g_mouseInside = true;
        f.wheelDelta += GET_WHEEL_DELTA_WPARAM(wp);
        forwardPaint(hwnd);
        return 0;
    }

    case WM_CAPTURECHANGED:
        if (g_selfRelease) return 0;      // our own ReleaseCapture(): not a lost capture
        if ((HWND)lp != hwnd) {
            g_leftDown = false;
            f.leftDown = false;
            g_capPressed = CAP_NONE;
            f.captureLost = true;
            forwardPaint(hwnd);
        }
        return 0;

    // ------------------------------------------------------------------
    // keyboard
    // ------------------------------------------------------------------
    case WM_KEYDOWN: {
        bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool alt   = (GetKeyState(VK_MENU) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        if (ctrl && wp == 'O')            { openFilesDialog(hwnd); return 0; }
        if (ctrl && wp == VK_RETURN)      { if (a.view == ViewId::Tool) runCurrentTool(); return 0; }
        if (ctrl && shift && wp == 'D')   { setDarkTheme(!isDarkTheme()); return 0; }
        if (alt && wp == VK_LEFT)         { goHome(); return 0; }
        if (wp == VK_F1)                  { a.view = ViewId::About; repaint(); return 0; }

        if (wp == VK_TAB)                 { cycleFocus(shift); forwardPaint(hwnd); return 0; }

        if (wp == VK_ESCAPE) {
            if (f.focusId != 0) { f.focusId = 0; f.escPressed = true; }
            else if (a.view != ViewId::Home) goHome();
            forwardPaint(hwnd);
            return 0;
        }

        switch (wp) {
        case VK_BACK:   f.keyBack  = true; break;
        case VK_DELETE: f.keyDel   = true; break;
        case VK_LEFT:   f.keyLeft  = true; break;
        case VK_RIGHT:  f.keyRight = true; break;
        case VK_HOME:   f.keyHome  = true; break;
        case VK_END:    f.keyEnd   = true; break;
        case VK_RETURN: f.enterPressed = true; break;
        default:
            if (ctrl && wp == 'A') f.keyCtrlA = true;
            else if (ctrl && wp == 'C') f.keyCtrlC = true;
            else if (ctrl && wp == 'V') f.keyCtrlV = true;
            else if (ctrl && wp == 'X') f.keyCtrlX = true;
            else return DefWindowProcW(hwnd, msg, wp, lp);
            break;
        }
        forwardPaint(hwnd);
        return 0;
    }

    case WM_CHAR: {
        wchar_t ch = (wchar_t)wp;
        if (ch >= 32 && ch != 127) { f.chars += ch; forwardPaint(hwnd); }
        return 0;
    }

    // ------------------------------------------------------------------
    // drag & drop
    // ------------------------------------------------------------------
    case WM_DROPFILES: {
        HDROP h = (HDROP)wp;
        UINT n = DragQueryFileW(h, 0xFFFFFFFF, nullptr, 0);
        std::vector<std::wstring> files;
        for (UINT i = 0; i < n; i++) {
            UINT len = DragQueryFileW(h, i, nullptr, 0);
            if (!len) continue;
            std::wstring p((size_t)len, L'\0');
            DragQueryFileW(h, i, &p[0], len + 1);
            files.push_back(p);
        }
        DragFinish(h);
        ingestFiles(hwnd, files);
        return 0;
    }

    // a second instance forwards its command line here
    case WM_COPYDATA: {
        COPYDATASTRUCT* cds = (COPYDATASTRUCT*)lp;
        if (cds && cds->dwData == 0x50444647 && cds->cbData && cds->lpData) {
            std::wstring blob((const wchar_t*)cds->lpData, cds->cbData / sizeof(wchar_t));
            std::vector<std::wstring> files;
            splitPaths(blob, files);
            if (!files.empty()) {
                if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                ingestFiles(hwnd, files);
            }
        }
        return 0;
    }

    // ------------------------------------------------------------------
    // background jobs
    // ------------------------------------------------------------------
    case WM_APP_JOB_PROGRESS:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_APP_JOB_DONE: {
        pdf::Result* res = (pdf::Result*)lp;
        ToolState& st = a.ts(a.jobTool);
        if (res) { st.result = *res; delete res; }
        st.ran = true;
        JobRunner::get().reset();

        if (st.result.ok) {
            int n = (int)st.result.outputs.size();
            if (n == 1)      showToast(util::fmt(L"Saved %s", util::fileName(st.result.outputs[0]).c_str()), 1);
            else if (n > 1)  showToast(util::fmt(L"Saved %d files", n), 1);
            else             showToast(L"Done", 1);

            if (a.openAfterRun && n >= 1) {
                if (n == 1) ShellExecuteW(hwnd, L"open", st.result.outputs[0].c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                else        ShellExecuteW(hwnd, L"open", util::dirName(st.result.outputs[0]).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else if (st.result.error != L"cancelled") {
            showToast(L"Could not finish \u2014 see the message below.", 3);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_APP_INVALIDATE:
    case WM_APP_TOAST:
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    // ------------------------------------------------------------------
    case WM_TIMER:
        if (wp == TIMER_CARET) {
            ui::caretBlinkTick();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (wp == TIMER_TOAST || wp == TIMER_TIP) {
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        return 0;

    // ------------------------------------------------------------------
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_KEYMENU && lp == 0) return 0;   // swallow lone Alt
        return DefWindowProcW(hwnd, msg, wp, lp);

    case WM_CLOSE: {
        if (JobRunner::get().busy()) {
            int r = ui::appMessageBox(hwnd, L"Still working",
                                      L"MrDocx is processing a document right now.",
                                      L"Stop the job and close?", 2, 2);
            if (r != IDYES) return 0;
            JobRunner::get().requestCancel();
        }
        // remember where the window was
        WINDOWPLACEMENT wp2 = { sizeof(wp2) };
        if (GetWindowPlacement(hwnd, &wp2)) {
            settings::writeInt(L"WinX", wp2.rcNormalPosition.left);
            settings::writeInt(L"WinY", wp2.rcNormalPosition.top);
            settings::writeInt(L"WinW", wp2.rcNormalPosition.right - wp2.rcNormalPosition.left);
            settings::writeInt(L"WinH", wp2.rcNormalPosition.bottom - wp2.rcNormalPosition.top);
            settings::writeInt(L"WinMax", wp2.showCmd == SW_SHOWMAXIMIZED ? 1 : 0);
        }
        settings::writeInt(L"SidebarW", a.sidebarWidth);
        settings::writeInt(L"OpenAfterRun", a.openAfterRun ? 1 : 0);
        settings::writeInt(L"SaveToSource", a.saveToSourceFolder ? 1 : 0);
        DestroyWindow(hwnd);
        return 0;
    }

    case WM_DESTROY:
        DragAcceptFiles(hwnd, FALSE);
        ThumbCache::get().stop();
        JobRunner::get().requestCancel();
        a.hwnd = nullptr;
        PostQuitMessage(0);
        return 0;

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        FontCache::get().destroy();
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_DISPLAYCHANGE:
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------
// icons built at run time (no bitmap resources required)
// ---------------------------------------------------------------------
HICON makeAppIcon(int size)
{
    COLORREF plate = RGBv(0x2E, 0x5B, 0xFF);
    return buildIconHICON(ICON_LAYERS, size, RGB(255, 255, 255), plate, true);
}

} // namespace

// =====================================================================
//  entry point
// =====================================================================
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmdLine, int nCmdShow)
{
    // ---- single instance -------------------------------------------
    HANDLE mtx = CreateMutexW(nullptr, TRUE, APP_MUTEX);
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(APP_CLASS, nullptr);
        if (existing) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
            if (cmdLine && *cmdLine) {
                size_t bytes = (wcslen(cmdLine) + 1) * sizeof(wchar_t);
                COPYDATASTRUCT cds = {};
                cds.dwData = 0x50444647;      // "PDFG"
                cds.cbData = (DWORD)bytes;
                cds.lpData = (void*)cmdLine;
                SendMessageW(existing, WM_COPYDATA, 0, (LPARAM)&cds);
            }
        }
        if (mtx) ReleaseMutex(mtx);
        return 0;
    }

    // ---- DPI awareness (the manifest declares per-monitor v2; this is
    //      a fallback for systems where the manifest is stripped) ------
    {
        HMODULE u = LoadLibraryW(L"user32.dll");
        if (u) {
            typedef BOOL(WINAPI* SPDAC)(DPI_AWARENESS_CONTEXT);
            auto fn = (SPDAC)GetProcAddress(u, "SetProcessDpiAwarenessContext");
            if (fn) fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
    {
        UINT dpi = 96;
        HMODULE u = LoadLibraryW(L"user32.dll");
        if (u) {
            typedef UINT(WINAPI* GDFS)(void);
            auto fn = (GDFS)GetProcAddress(u, "GetDpiForSystem");
            if (fn) dpi = fn();
        }
        Dpi::get().set((int)dpi);
    }

    OleInitialize(nullptr);
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    app().instance = hInst;

    // ---- window class ------------------------------------------------
    HICON iconBig = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                      GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON),
                                      LR_DEFAULTCOLOR | LR_SHARED);
    HICON iconSm  = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                      GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                      LR_DEFAULTCOLOR | LR_SHARED);
    if (!iconBig) iconBig = makeAppIcon(GetSystemMetrics(SM_CXICON));
    if (!iconSm)  iconSm  = makeAppIcon(GetSystemMetrics(SM_CXSMICON));

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = wndProc;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hIcon         = iconBig;
    wc.hIconSm       = iconSm;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;                 // we paint every pixel
    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = APP_CLASS;
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(nullptr, L"MrDocx could not register its window.", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // ---- initial geometry --------------------------------------------
    int w = S(1200), h = S(780);
    RECT work = {};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int x = work.left + ((work.right - work.left) - w) / 2;
    int y = work.top + ((work.bottom - work.top) - h) / 2;

    int sx = settings::readInt(L"WinX", INT_MIN);
    int sy = settings::readInt(L"WinY", INT_MIN);
    int sw = settings::readInt(L"WinW", 0);
    int sh = settings::readInt(L"WinH", 0);
    bool startMax = settings::readInt(L"WinMax", 0) != 0;
    if (sx != INT_MIN && sy != INT_MIN && sw > 200 && sh > 200) {
        // only restore if the saved position is still on some monitor
        RECT probe = { sx, sy, sx + sw, sy + sh };
        if (MonitorFromRect(&probe, MONITOR_DEFAULTTONULL)) { x = sx; y = sy; w = sw; h = sh; }
    }

    DWORD style = WS_OVERLAPPEDWINDOW;
    HWND hwnd = CreateWindowExW(0, APP_CLASS, APP_NAME, style,
                                x, y, w, h, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) {
        MessageBoxW(nullptr, L"MrDocx could not create its window.", APP_NAME, MB_ICONERROR);
        return 1;
    }

    // shadow + modern frame for the custom title bar
    MARGINS margins = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)iconBig);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)iconSm);

    ShowWindow(hwnd, startMax ? SW_MAXIMIZE : nCmdShow);
    UpdateWindow(hwnd);

    // ---- files given on the command line ------------------------------
    if (cmdLine && *cmdLine) {
        std::vector<std::wstring> files;
        splitPaths(cmdLine, files);
        std::vector<std::wstring> keep;
        for (auto& p : files) if (util::fileExists(p)) keep.push_back(p);
        if (!keep.empty()) ingestFiles(hwnd, keep);
    }

    // ---- message loop --------------------------------------------------
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    // ---- cleanup --------------------------------------------------------
    ThumbCache::get().stop();
    FontCache::get().destroy();
    freeBackBuffer();
    // icons were loaded with LR_SHARED: owned by the system, not destroyed here
    OleUninitialize();
    if (mtx) { ReleaseMutex(mtx); CloseHandle(mtx); }
    return (int)m.wParam;
}
