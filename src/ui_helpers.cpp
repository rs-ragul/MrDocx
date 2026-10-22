// =====================================================================
//  MrDocx  -  ui_helpers.cpp : clipboard, popup menus, modal dialog
//  Author : Ragul S
// =====================================================================
#include "ui.h"
#include <gdiplus.h>

using namespace Gdiplus;

namespace ui {

// ---- caret blink ----------------------------------------------------
namespace { bool g_caretOn = true; }
bool caretBlinkOn()   { return g_caretOn; }
void caretBlinkTick() { g_caretOn = !g_caretOn; }
void caretBlinkReset(){ g_caretOn = true; }

int cur_width(Frame& f)
{
    if (f.stack.empty()) return f.W;
    Frame::Stack& s = f.stack.back();
    return std::max(0, s.w);
}

bool rectVisible(Frame& f, RECT r)
{
    return r.bottom > f.visibleTop && r.top < f.visibleBottom &&
           r.right > f.clip.left && r.left < f.clip.right;
}

// ---------------------------------------------------------------------
// clipboard
// ---------------------------------------------------------------------
std::wstring getClipboardText(HWND hwnd)
{
    std::wstring out;
    if (!OpenClipboard(hwnd)) return out;
    HANDLE h = GetClipboardData(CF_UNICODETEXT);
    if (h) {
        const wchar_t* p = (const wchar_t*)GlobalLock(h);
        if (p) { out = p; GlobalUnlock(h); }
    }
    CloseClipboard();
    return out;
}

void setClipboardText(HWND hwnd, const std::wstring& text)
{
    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();
    size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (g) {
        void* p = GlobalLock(g);
        if (p) { memcpy(p, text.c_str(), bytes); GlobalUnlock(g); SetClipboardData(CF_UNICODETEXT, g); }
        else GlobalFree(g);
    }
    CloseClipboard();
}

// ---------------------------------------------------------------------
// popup menu - a genuine Win32 menu (native shadow, keyboard, dismissal)
// ---------------------------------------------------------------------
int popupMenu(HWND owner, POINT screenPt, const std::vector<MenuItem>& items, int minWidth)
{
    (void)minWidth;
    if (items.empty()) return 0;
    HMENU m = CreatePopupMenu();
    if (!m) return 0;
    for (size_t i = 0; i < items.size(); i++) {
        const MenuItem& it = items[i];
        if (it.separator) { AppendMenuW(m, MF_SEPARATOR, 0, nullptr); continue; }
        UINT flags = MF_STRING;
        if (!it.enabled) flags |= MF_GRAYED;
        if (it.checked)  flags |= MF_CHECKED;
        AppendMenuW(m, flags, (UINT_PTR)(it.id != 0 ? it.id : (int)i + 1), it.text.c_str());
    }
    // Menus must be dismissed when the owner loses activation.
    SetForegroundWindow(owner);
    int res = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
                                  screenPt.x, screenPt.y, 0, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(m);
    return res;
}

} // namespace ui

// =====================================================================
//  Themed modal dialog
// =====================================================================
namespace {

struct MsgBoxData {
    std::wstring title, text, detail;
    int buttons = 0;    // 0 ok, 1 okcancel, 2 yesno
    int iconKind = 0;   // 0 none, 1 info, 2 warn, 3 error, 4 success
    Palette pal;
    int result = 0;
    bool done = false;
    int hoverBtn = -1;
    int pressBtn = -1;
    int defaultBtn = 0;
    int focusBtn = 0;
    HWND hwnd = nullptr;
};

const wchar_t* MSGBOX_CLASS = L"MrDocxMsgBox";
bool g_msgboxClassReady = false;

int msgboxWidth(MsgBoxData* d)
{
    HDC dc = GetDC(nullptr);
    HFONT f1 = fontUI(15, FW_SEMIBOLD);
    HFONT f2 = fontUI(13);
    SIZE t = ui::textSize(dc, d->text, f2);
    SIZE ti = ui::textSize(dc, d->title, f1);
    SIZE de = d->detail.empty() ? SIZE{ 0, 0 } : ui::textSize(dc, d->detail, f2);
    ReleaseDC(nullptr, dc);
    int w = std::max({ t.cx, ti.cx, de.cx }) + S(64);
    return util::clampv(w, S(380), S(560));
}

int msgboxHeight(MsgBoxData* d, int w)
{
    HDC dc = GetDC(nullptr);
    HFONT f2 = fontUI(13);
    RECT probe = { 0, 0, w - S(64), 0 };
    HGDIOBJ o = SelectObject(dc, f2);
    std::wstring body = d->text;
    if (!d->detail.empty()) body += L"\n\n" + d->detail;
    DrawTextW(dc, const_cast<LPWSTR>(body.c_str()), -1, &probe, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, o);
    ReleaseDC(nullptr, dc);
    return S(28) + S(20) + probe.bottom + S(28) + S(40) + S(20);
}

LRESULT CALLBACK msgboxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MsgBoxData* d = (MsgBoxData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        d = (MsgBoxData*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)d);
        d->hwnd = hwnd;
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        HDC dc = CreateCompatibleDC(wdc);
        HBITMAP bmp = CreateCompatibleBitmap(wdc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(dc, bmp);

        HBRUSH bgb = CreateSolidBrush(d->pal.background);
        FillRect(dc, &cr, bgb); DeleteObject(bgb);

        // title
        HFONT ft = fontUI(15, FW_SEMIBOLD);
        int y = S(24);
        int iconSize = 0;
        IconId ic = ICON_NONE;
        COLORREF icCol = d->pal.accent;
        if (d->iconKind == 1) { ic = ICON_INFO; icCol = d->pal.accent; iconSize = S(22); }
        else if (d->iconKind == 2) { ic = ICON_WARN; icCol = d->pal.warning; iconSize = S(22); }
        else if (d->iconKind == 3) { ic = ICON_WARN; icCol = d->pal.danger; iconSize = S(22); }
        else if (d->iconKind == 4) { ic = ICON_CHECK; icCol = d->pal.success; iconSize = S(22); }

        int textX = S(28);
        if (ic != ICON_NONE) {
            RECT ib = { S(28), y, S(28) + iconSize, y + iconSize };
            drawIcon(dc, ic, ib, icCol);
            textX = S(28) + iconSize + S(12);
        }
        RECT tr = { textX, y - S(2), cr.right - S(28), y + iconSize + S(2) };
        ui::drawText(dc, d->title, tr, d->pal.text, ft, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += std::max(iconSize, S(22)) + S(18);

        // body
        std::wstring body = d->text;
        RECT br = { S(28), y, cr.right - S(28), cr.bottom - S(80) };
        HGDIOBJ of = SelectObject(dc, fontUI(13));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, d->pal.textDim);
        std::wstring mainText = d->text;
        RECT mr = br;
        DrawTextW(dc, const_cast<LPWSTR>(mainText.c_str()), -1, &mr, DT_WORDBREAK | DT_NOPREFIX | DT_TOP);
        if (!d->detail.empty()) {
            RECT dr = { br.left, mr.bottom + S(10), br.right, br.bottom };
            SetTextColor(dc, d->pal.textFaint);
            SelectObject(dc, fontUI(12));
            DrawTextW(dc, const_cast<LPWSTR>(d->detail.c_str()), -1, &dr, DT_WORDBREAK | DT_NOPREFIX | DT_TOP);
        }
        SelectObject(dc, of);

        // buttons
        struct Btn { const wchar_t* label; int id; BtnStyle style; };
        Btn btns[3]; int nb = 0;
        if (d->buttons == 0) { btns[nb++] = { L"OK", IDOK, BtnStyle::Primary }; }
        else if (d->buttons == 1) { btns[nb++] = { L"Cancel", IDCANCEL, BtnStyle::Secondary }; btns[nb++] = { L"OK", IDOK, BtnStyle::Primary }; }
        else { btns[nb++] = { L"No", IDNO, BtnStyle::Secondary }; btns[nb++] = { L"Yes", IDYES, BtnStyle::Primary }; }

        HFONT bf = fontUI(13, FW_SEMIBOLD);
        int bw = S(96), bh = S(34), gap = S(10);
        int totalW = nb * bw + (nb - 1) * gap;
        int bx = cr.right - S(28) - totalW;
        int by = cr.bottom - S(24) - bh;
        for (int i = 0; i < nb; i++) {
            RECT r = { bx, by, bx + bw, by + bh };
            bool hov = (d->hoverBtn == i), prs = (d->pressBtn == i);
            COLORREF bgc, fg, bd;
            BtnStyle st = btns[i].style;
            if (st == BtnStyle::Primary) {
                bgc = prs ? d->pal.accentDown : (hov ? d->pal.accentHover : d->pal.accent);
                fg = d->pal.accentText; bd = bgc;
            } else {
                bgc = prs ? d->pal.surfaceAlt : (hov ? d->pal.surfaceHover : d->pal.surface);
                fg = d->pal.text; bd = hov ? d->pal.borderStrong : d->pal.border;
            }
            if (st == BtnStyle::Primary) ui::drawShadow(dc, r, S(8), S(6), 30);
            ui::fillRect(dc, r, bgc, S(8));
            ui::strokeRect(dc, r, bd, S(8), 1);
            if (d->focusBtn == i)
                ui::strokeRect(dc, { r.left - S(2), r.top - S(2), r.right + S(2), r.bottom + S(2) },
                               d->pal.accent, S(10), S(2), 90);
            ui::drawText(dc, btns[i].label, r, fg, bf, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            bx += bw + gap;
        }

        BitBlt(wdc, 0, 0, cr.right, cr.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int nb = d->buttons == 0 ? 1 : 2;
        int bw = S(96), bh = S(34), gap = S(10);
        RECT cr; GetClientRect(hwnd, &cr);
        int totalW = nb * bw + (nb - 1) * gap;
        int bx = cr.right - S(28) - totalW, by = cr.bottom - S(24) - bh;
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hov = -1;
        for (int i = 0; i < nb; i++) {
            RECT r = { bx + i * (bw + gap), by, bx + i * (bw + gap) + bw, by + bh };
            if (PtInRect(&r, POINT{ x, y })) { hov = i; break; }
        }
        if (hov != d->hoverBtn) { d->hoverBtn = hov; InvalidateRect(hwnd, nullptr, FALSE); }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (d->hoverBtn != -1) { d->hoverBtn = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        int nb = d->buttons == 0 ? 1 : 2;
        int bw = S(96), bh = S(34), gap = S(10);
        RECT cr; GetClientRect(hwnd, &cr);
        int totalW = nb * bw + (nb - 1) * gap;
        int bx = cr.right - S(28) - totalW, by = cr.bottom - S(24) - bh;
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        for (int i = 0; i < nb; i++) {
            RECT r = { bx + i * (bw + gap), by, bx + i * (bw + gap) + bw, by + bh };
            if (PtInRect(&r, POINT{ x, y })) { d->pressBtn = i; d->focusBtn = i; SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break; }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (d->pressBtn >= 0) {
            int nb = d->buttons == 0 ? 1 : 2;
            int bw = S(96), bh = S(34), gap = S(10);
            RECT cr; GetClientRect(hwnd, &cr);
            int totalW = nb * bw + (nb - 1) * gap;
            int bx = cr.right - S(28) - totalW, by = cr.bottom - S(24) - bh;
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            RECT r = { bx + d->pressBtn * (bw + gap), by, bx + d->pressBtn * (bw + gap) + bw, by + bh };
            if (PtInRect(&r, POINT{ x, y })) {
                if (d->buttons == 0) d->result = IDOK;
                else if (d->buttons == 1) d->result = (d->pressBtn == 1) ? IDOK : IDCANCEL;
                else d->result = (d->pressBtn == 1) ? IDYES : IDNO;
                d->done = true;
            }
            d->pressBtn = -1;
            ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            d->result = (d->buttons == 0) ? IDOK : (d->buttons == 1 ? IDCANCEL : IDNO);
            d->done = true;
        } else if (wp == VK_RETURN) {
            int nb = d->buttons == 0 ? 1 : 2;
            int idx = util::clampv(d->focusBtn, 0, nb - 1);
            if (d->buttons == 0) d->result = IDOK;
            else if (d->buttons == 1) d->result = (idx == 1) ? IDOK : IDCANCEL;
            else d->result = (idx == 1) ? IDYES : IDNO;
            d->done = true;
        } else if (wp == VK_TAB || wp == VK_LEFT || wp == VK_RIGHT) {
            int nb = d->buttons == 0 ? 1 : 2;
            d->focusBtn = (d->focusBtn + 1) % nb;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_CLOSE:
        d->result = (d->buttons == 0) ? IDOK : (d->buttons == 1 ? IDCANCEL : IDNO);
        d->done = true;
        return 0;
    case WM_ERASEBKGND: return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

namespace ui {

int appMessageBox(HWND owner, const std::wstring& title, const std::wstring& text,
                  const std::wstring& detail, int buttons, int iconKind)
{
    if (!g_msgboxClassReady) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = msgboxProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = MSGBOX_CLASS;
        wc.style = CS_DBLCLKS;
        RegisterClassExW(&wc);
        g_msgboxClassReady = true;
    }

    MsgBoxData d;
    d.title = title; d.text = text; d.detail = detail;
    d.buttons = buttons; d.iconKind = iconKind;
    d.pal = currentPalette();
    d.defaultBtn = d.focusBtn = (buttons == 0) ? 0 : 1;

    int w = msgboxWidth(&d);
    int h = msgboxHeight(&d, w);

    // centre on owner (or screen)
    RECT rc;
    if (owner) GetWindowRect(owner, &rc);
    else { rc.left = rc.top = 0; rc.right = GetSystemMetrics(SM_CXSCREEN); rc.bottom = GetSystemMetrics(SM_CYSCREEN); }
    int x = (rc.left + rc.right - w) / 2;
    int y = (rc.top + rc.bottom - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, MSGBOX_CLASS, L"",
                                WS_POPUP | WS_CLIPSIBLINGS, x, y, w, h, owner, nullptr,
                                GetModuleHandle(nullptr), &d);
    if (!hwnd) return IDOK;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(hwnd);
    if (owner) EnableWindow(owner, FALSE);

    MSG m;
    while (!d.done && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.hwnd == hwnd || IsDialogMessageW(hwnd, &m)) {
            TranslateMessage(&m); DispatchMessageW(&m);
        } else {
            TranslateMessage(&m); DispatchMessageW(&m);
        }
    }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(hwnd);
    return d.result;
}

} // namespace ui

// =====================================================================
//  Single-line input dialog (PDF passwords)
// =====================================================================
namespace {

struct InputBoxData {
    std::wstring title, prompt, value;
    bool password = false;
    bool ok = false;
    bool done = false;
    Palette pal;
    HWND hwnd = nullptr;
    HWND edit = nullptr;
    HBRUSH editBrush = nullptr;
    int hoverBtn = -1;
    int pressBtn = -1;
};

const wchar_t* INPUTBOX_CLASS = L"MrDocxInputBox";
bool g_inputboxClassReady = false;

LRESULT CALLBACK inputboxProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    InputBoxData* d = (InputBoxData*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lp;
        d = (InputBoxData*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)d);
        d->hwnd = hwnd;
        DWORD es = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT;
        if (d->password) es |= ES_PASSWORD;
        d->edit = CreateWindowExW(0, L"EDIT", d->value.c_str(), es,
                                  S(24), S(110), S(372), S(34), hwnd, (HMENU)1001,
                                  GetModuleHandleW(nullptr), nullptr);
        SendMessageW(d->edit, WM_SETFONT, (WPARAM)fontUI(13), TRUE);
        SendMessageW(d->edit, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC edc = (HDC)wp;
        SetTextColor(edc, d->pal.text);
        SetBkColor(edc, d->pal.surfaceAlt);
        if (!d->editBrush) d->editBrush = CreateSolidBrush(d->pal.surfaceAlt);
        return (LRESULT)d->editBrush;
    }
    case WM_COMMAND: {
        if (LOWORD(wp) == 1001 && HIWORD(wp) == EN_CHANGE) {
            int len = (int)SendMessageW(d->edit, WM_GETTEXTLENGTH, 0, 0);
            std::wstring buf((size_t)len + 1, L'\0');
            int got = (int)SendMessageW(d->edit, WM_GETTEXT, (WPARAM)(len + 1), (LPARAM)&buf[0]);
            d->value.assign(buf.c_str(), (size_t)got);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_SIZE:
        if (d->edit) {
            RECT cr; GetClientRect(hwnd, &cr);
            MoveWindow(d->edit, S(24), S(110), cr.right - S(48), S(34), TRUE);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(hwnd, &ps);
        RECT cr; GetClientRect(hwnd, &cr);
        HDC dc = CreateCompatibleDC(wdc);
        HBITMAP bmp = CreateCompatibleBitmap(wdc, cr.right, cr.bottom);
        HGDIOBJ ob = SelectObject(dc, bmp);

        HBRUSH bgb = CreateSolidBrush(d->pal.surface);
        FillRect(dc, &cr, bgb); DeleteObject(bgb);
        ui::strokeRect(dc, cr, d->pal.border, S(10), 1);

        int is = S(20);
        RECT ir = { S(24), S(22), S(24) + is, S(22) + is };
        ui::fillRect(dc, ir, d->pal.accentSoft, is / 2);
        int cs = S(12);
        drawIcon(dc, ICON_LOCK, RECT{ ir.left + (is - cs) / 2, ir.top + (is - cs) / 2,
                 ir.left + (is + cs) / 2, ir.top + (is + cs) / 2 }, d->pal.accent);
        ui::drawText(dc, d->title, RECT{ ir.right + S(10), S(20), cr.right - S(24), S(44) },
                     d->pal.text, fontUI(15, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT pr = { S(24), S(56), cr.right - S(24), S(102) };
        HGDIOBJ of = SelectObject(dc, fontUI(12));
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, d->pal.textDim);
        DrawTextW(dc, const_cast<LPWSTR>(d->prompt.c_str()), -1, &pr,
                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, of);

        // edit border (the control paints its own background)
        RECT er = { S(24), S(110), cr.right - S(24), S(110) + S(34) };
        InflateRect(&er, 1, 1);
        ui::strokeRect(dc, er, d->pal.borderStrong, S(5), 1);

        const wchar_t* labels[2] = { L"Cancel", L"Unlock" };
        int bw = S(96), bh = S(34), gap = S(10);
        int bx = cr.right - S(24) - bw * 2 - gap;
        int by = cr.bottom - S(24) - bh;
        for (int i = 0; i < 2; i++) {
            RECT r = { bx + i * (bw + gap), by, bx + i * (bw + gap) + bw, by + bh };
            bool primary = (i == 1);
            bool hov = (d->hoverBtn == i), prs = (d->pressBtn == i);
            COLORREF bg = primary ? (prs ? mixColor(d->pal.accent, RGB(0,0,0), 0.18f)
                                          : hov ? mixColor(d->pal.accent, RGB(255,255,255), 0.12f) : d->pal.accent)
                                : (prs ? d->pal.surfaceAlt : hov ? d->pal.surfaceAlt : d->pal.surface);
            ui::fillRect(dc, r, bg, S(8));
            if (!primary) ui::strokeRect(dc, r, hov ? d->pal.borderStrong : d->pal.border, S(8), 1);
            ui::drawText(dc, labels[i], r, primary ? RGB(255,255,255) : d->pal.text,
                         fontUI(13, primary ? FW_SEMIBOLD : FW_NORMAL),
                         DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        BitBlt(wdc, 0, 0, cr.right, cr.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob);
        DeleteObject(bmp);
        DeleteDC(dc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        RECT cr; GetClientRect(hwnd, &cr);
        int bw = S(96), bh = S(34), gap = S(10);
        int bx = cr.right - S(24) - bw * 2 - gap;
        int by = cr.bottom - S(24) - bh;
        int hit = -1;
        for (int i = 0; i < 2; i++) {
            RECT r = { bx + i * (bw + gap), by, bx + i * (bw + gap) + bw, by + bh };
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) hit = i;
        }
        if (hit != d->hoverBtn) { d->hoverBtn = hit; InvalidateRect(hwnd, nullptr, FALSE); }
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (d->hoverBtn != -1) { d->hoverBtn = -1; d->pressBtn = -1; InvalidateRect(hwnd, nullptr, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        RECT cr; GetClientRect(hwnd, &cr);
        int bw = S(96), bh = S(34), gap = S(10);
        int bx = cr.right - S(24) - bw * 2 - gap;
        int by = cr.bottom - S(24) - bh;
        for (int i = 0; i < 2; i++) {
            RECT r = { bx + i * (bw + gap), by, bx + i * (bw + gap) + bw, by + bh };
            if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                d->pressBtn = i; SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
        int was = d->pressBtn;
        d->pressBtn = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        RECT cr; GetClientRect(hwnd, &cr);
        int bw = S(96), bh = S(34), gap = S(10);
        int bx = cr.right - S(24) - bw * 2 - gap;
        int by = cr.bottom - S(24) - bh;
        for (int i = 0; i < 2; i++) {
            RECT r = { bx + i * (bw + gap), by, bx + i * (bw + gap) + bw, by + bh };
            if (was == i && x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
                d->ok = (i == 1);
                d->done = true;
            }
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_RETURN) { d->ok = true; d->done = true; return 0; }
        if (wp == VK_ESCAPE) { d->ok = false; d->done = true; return 0; }
        return 0;
    case WM_CLOSE:
        d->ok = false; d->done = true; return 0;
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        if (d->editBrush) { DeleteObject(d->editBrush); d->editBrush = nullptr; }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

namespace ui {

bool inputBox(HWND owner, const std::wstring& title, const std::wstring& prompt,
              std::wstring& value, bool password)
{
    if (!g_inputboxClassReady) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = inputboxProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = INPUTBOX_CLASS;
        wc.style = CS_DBLCLKS;
        RegisterClassExW(&wc);
        g_inputboxClassReady = true;
    }

    InputBoxData d;
    d.title = title; d.prompt = prompt; d.value = value; d.password = password;
    d.pal = currentPalette();

    int w = S(420), h = S(196);
    RECT rc;
    if (owner) GetWindowRect(owner, &rc);
    else { rc.left = rc.top = 0; rc.right = GetSystemMetrics(SM_CXSCREEN); rc.bottom = GetSystemMetrics(SM_CYSCREEN); }
    int x = (rc.left + rc.right - w) / 2;
    int y = (rc.top + rc.bottom - h) / 2;

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, INPUTBOX_CLASS, L"",
                                WS_POPUP | WS_CLIPSIBLINGS, x, y, w, h, owner, nullptr,
                                GetModuleHandleW(nullptr), &d);
    if (!hwnd) return false;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetFocus(d.edit);
    if (owner) EnableWindow(owner, FALSE);

    // forward Enter/Escape typed in the EDIT control to the dialog
    MSG m;
    while (!d.done && GetMessageW(&m, nullptr, 0, 0)) {
        if (m.message == WM_KEYDOWN && m.hwnd == d.edit &&
            (m.wParam == VK_RETURN || m.wParam == VK_ESCAPE)) {
            d.ok = (m.wParam == VK_RETURN);
            d.done = true;
            break;
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    if (owner) { EnableWindow(owner, TRUE); SetForegroundWindow(owner); }
    DestroyWindow(hwnd);
    if (d.ok) value = d.value;
    return d.ok;
}

} // namespace ui
