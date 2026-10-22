// =====================================================================
//  MrDocx  -  ui_widgets.cpp : the widget set
//  Author : Ragul S
// =====================================================================
#include "ui.h"
#include <gdiplus.h>

using namespace Gdiplus;

namespace {
inline Gdiplus::Color C(COLORREF c, BYTE a = 255)
{ return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
std::wstring g_tipText;
RECT g_tipRect = {};
} // namespace

namespace ui {

// restore the frame clip after a temporary IntersectClipRect
static void restoreClip(Frame& f)
{
    SelectClipRgn(f.dc, nullptr);
    IntersectClipRect(f.dc, f.clip.left, f.clip.top, f.clip.right, f.clip.bottom);
}

// ---------------------------------------------------------------------
// hit testing / focus
// ---------------------------------------------------------------------
WidgetHit hitTest(Frame& f, int id, RECT r)
{
    WidgetHit h{};
    if (f.measure) return h;
    POINT m = f.mouse;
    bool inside = f.mouseInside && PtInRect(&f.clip, m) && PtInRect(&r, m);
    h.hovered = inside;
    h.focused = (f.focusId == id);

    if (inside && f.leftPressed && f.activeId == 0) {
        f.activeId = id;
        f.focusId = id;
        h.pressed = true;
        // double click
        DWORD now = GetTickCount();
        if (f.lastClickId == id && (now - f.lastClickTime) < (DWORD)GetDoubleClickTime()) {
            h.doubleClicked = true;
            f.lastClickTime = 0;
        } else {
            f.lastClickId = id; f.lastClickTime = now;
        }
    } else if (f.activeId == id) {
        h.pressed = f.leftDown;
    }

    if (f.activeId == id && f.leftReleased) {
        if (inside) h.clicked = true;
        f.activeId = 0;
    }

    if (inside && f.rightPressed) h.rightClick = true;

    // keyboard activation
    if (h.focused && f.enterPressed) { h.clicked = true; h.pressed = true; }

    if (id != 0) f.focusOrder.push_back(id);
    return h;
}

void setFocus(Frame& f, int id) { f.focusId = id; }
void clearFocus(Frame& f) { f.focusId = 0; }
bool hasFocus(Frame& f, int id) { return f.focusId == id; }

// ---------------------------------------------------------------------
// label
// ---------------------------------------------------------------------
RECT label(Frame& f, const std::wstring& text, int fontSize, COLORREF color,
           int weight, int maxW, bool wrap, int align)
{
    HFONT font = fontUI(fontSize, weight);
    int w = maxW > 0 ? maxW : cur_width(f);
    RECT r;
    if (!wrap) {
        SIZE sz = textSize(f.dc, text, font);
        int h = std::max(S(18), (int)sz.cy + S(2));
        r = takeRect(f, w, h);
        UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE;
        if (align == 1) flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        else if (align == 2) flags = DT_RIGHT | DT_VCENTER | DT_SINGLELINE;
        if (!f.measure && rectVisible(f, r)) drawText(f.dc, text, r, color, font, flags);
        return r;
    }
    // wrapped: measure with DrawText
    RECT probe = { 0, 0, w, 0 };
    HGDIOBJ of = SelectObject(f.dc, font);
    DrawTextW(f.dc, const_cast<LPWSTR>(text.c_str()), -1, &probe,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(f.dc, of);
    int h = probe.bottom;
    r = takeRect(f, w, h);
    if (!f.measure && rectVisible(f, r)) {
        UINT flags = DT_WORDBREAK | DT_NOPREFIX | DT_TOP;
        if (align == 1) flags |= DT_CENTER; else if (align == 2) flags |= DT_RIGHT; else flags |= DT_LEFT;
        HGDIOBJ o2 = SelectObject(f.dc, font);
        SetBkMode(f.dc, TRANSPARENT); SetTextColor(f.dc, color);
        RECT rr = r;
        DrawTextW(f.dc, const_cast<LPWSTR>(text.c_str()), -1, &rr, flags);
        SelectObject(f.dc, o2);
    }
    return r;
}

RECT sectionTitle(Frame& f, const std::wstring& text, const std::wstring& hint)
{
    int w = cur_width(f);
    int h = S(26);
    RECT r = takeRect(f, w, h);
    if (f.measure || !rectVisible(f, r)) return r;
    HFONT f1 = fontUI(13, FW_SEMIBOLD);
    RECT left = { r.left, r.top, r.left + w - (hint.empty() ? 0 : S(160)), r.bottom };
    drawText(f.dc, text, left, f.pal.textDim, f1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (!hint.empty()) {
        HFONT f2 = fontUI(12);
        RECT right = { r.right - S(220), r.top, r.right, r.bottom };
        drawText(f.dc, hint, right, f.pal.textFaint, f2, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }
    return r;
}

RECT badge(Frame& f, const std::wstring& text, COLORREF bg, COLORREF fg)
{
    HFONT font = fontUI(11, FW_SEMIBOLD);
    SIZE sz = textSize(f.dc, text, font);
    int h = sz.cy + S(8);
    int w = sz.cx + S(16);
    RECT r = takeRect(f, w, h);
    if (f.measure || !rectVisible(f, r)) return r;
    fillRect(f.dc, r, bg, h / 2);
    drawText(f.dc, text, r, fg, font, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return r;
}

RECT separator(Frame& f, int thickness, int margin)
{
    int w = cur_width(f);
    RECT r = takeRect(f, w, std::max(1, thickness) + margin * 2);
    if (thickness > 0 && !f.measure && rectVisible(f, r)) {
        int y = r.top + margin;
        HBRUSH b = CreateSolidBrush(f.pal.border);
        RECT line = { r.left, y, r.right, y + thickness };
        FillRect(f.dc, &line, b);
        DeleteObject(b);
    }
    return r;
}

// ---------------------------------------------------------------------
// buttons
// ---------------------------------------------------------------------
static void buttonColors(BtnStyle style, const Palette& p, bool hovered, bool pressed, bool enabled,
                         COLORREF& bg, COLORREF& fg, COLORREF& bd)
{
    switch (style) {
    case BtnStyle::Primary:
        bg = pressed ? p.accentDown : (hovered ? p.accentHover : p.accent);
        fg = p.accentText; bd = bg;
        break;
    case BtnStyle::Secondary:
        bg = pressed ? p.surfaceAlt : (hovered ? p.surfaceHover : p.surface);
        fg = p.text; bd = hovered ? p.borderStrong : p.border;
        break;
    case BtnStyle::Ghost:
        bg = pressed ? p.surfaceAlt : (hovered ? p.surfaceAlt : bgTransparent());
        fg = p.textDim; bd = bgTransparent();
        break;
    case BtnStyle::Danger:
        bg = pressed ? p.danger : (hovered ? mixColor(p.danger, RGB(255, 255, 255), 0.12f) : p.danger);
        fg = RGB(255, 255, 255); bd = bg;
        break;
    case BtnStyle::Success:
        bg = pressed ? p.success : (hovered ? mixColor(p.success, RGB(255, 255, 255), 0.12f) : p.success);
        fg = RGB(255, 255, 255); bd = bg;
        break;
    case BtnStyle::Icon:
        bg = pressed ? p.surfaceAlt : (hovered ? p.surfaceAlt : bgTransparent());
        fg = p.textDim; bd = bgTransparent();
        break;
    }
    if (!enabled) {
        bg = style == BtnStyle::Primary ? mixColor(p.accent, p.surface, 0.55f) : bg;
        fg = p.textFaint;
        bd = style == BtnStyle::Secondary ? p.border : bd;
    }
}

COLORREF bgTransparent() { return (COLORREF)0xFFFFFFFF; }
bool isTransparent(COLORREF c) { return c == (COLORREF)0xFFFFFFFF; }

RECT button(Frame& f, int id, const std::wstring& text, BtnStyle style,
            int minW, int h, IconId icon, bool enabled, WidgetHit* outHit)
{
    HFONT font = fontUI(13, style == BtnStyle::Primary ? FW_SEMIBOLD : FW_NORMAL);
    SIZE sz = textSize(f.dc, text, font);
    int iconW = icon != ICON_NONE ? S(16) + (text.empty() ? 0 : S(8)) : 0;
    int padX = style == BtnStyle::Ghost || style == BtnStyle::Icon ? S(10) : S(16);
    int w = std::max(minW, (int)sz.cx + iconW + padX * 2);
    int hh = h > 0 ? h : S(34);
    RECT r = takeRect(f, w, hh);

    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    if (f.measure || !rectVisible(f, r)) return r;

    COLORREF bg, fg, bd;
    buttonColors(style, f.pal, hit.hovered && enabled, hit.pressed && enabled, enabled, bg, fg, bd);

    int radius = S(8);
    if (style == BtnStyle::Primary && !enabled) {
        fillRect(f.dc, r, bg, radius);
    } else {
        if (!isTransparent(bg)) {
            if (style == BtnStyle::Primary) drawShadow(f.dc, r, radius, S(6), 34);
            fillRect(f.dc, r, bg, radius);
        }
        if (!isTransparent(bd)) strokeRect(f.dc, r, bd, radius, 1);
    }
    if (hit.focused && f.focusId != 0) {
        RECT fr = { r.left - S(2), r.top - S(2), r.right + S(2), r.bottom + S(2) };
        strokeRect(f.dc, fr, mixColor(f.pal.accent, f.pal.background, 0.25f), radius + 2, S(2), 120);
    }

    int tx = r.left;
    if (icon != ICON_NONE) {
        int is = S(16);
        int ix = text.empty() ? (r.left + r.right - is) / 2 : r.left + padX;
        RECT ib = { ix, (r.top + r.bottom - is) / 2, ix + is, (r.top + r.bottom + is) / 2 };
        drawIcon(f.dc, icon, ib, enabled ? fg : f.pal.textFaint);
        tx = ix + is + S(8);
    } else tx = r.left;

    if (!text.empty()) {
        RECT tr = { tx, r.top, r.right - padX, r.bottom };
        UINT flags = (icon != ICON_NONE ? DT_LEFT : DT_CENTER) | DT_VCENTER | DT_SINGLELINE;
        drawText(f.dc, text, tr, enabled ? fg : f.pal.textFaint, font, flags);
    }
    return r;
}

RECT iconButton(Frame& f, int id, IconId icon, int size, BtnStyle style, bool enabled,
                const wchar_t* tip, WidgetHit* outHit)
{
    int s = size > 0 ? size : S(30);
    RECT r = takeRect(f, s, s);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    if (rectVisible(f, r)) {
        COLORREF bg, fg, bd;
        buttonColors(style, f.pal, hit.hovered && enabled, hit.pressed && enabled, enabled, bg, fg, bd);
        int radius = S(7);
        if (!isTransparent(bg)) fillRect(f.dc, r, bg, radius);
        if (!isTransparent(bd)) strokeRect(f.dc, r, bd, radius, 1);
        int is = (int)(s * 0.55f);
        RECT ib = { (r.left + r.right - is) / 2, (r.top + r.bottom - is) / 2,
                    (r.left + r.right + is) / 2, (r.top + r.bottom + is) / 2 };
        drawIcon(f.dc, icon, ib, enabled ? fg : f.pal.textFaint);
    }
    if (tip && hit.hovered) requestTooltip(f, tip, r);
    return r;
}

// ---------------------------------------------------------------------
// checkbox / radio
// ---------------------------------------------------------------------
RECT checkbox(Frame& f, int id, const std::wstring& text, bool& value, bool enabled, WidgetHit* outHit)
{
    HFONT font = fontUI(13);
    SIZE sz = textSize(f.dc, text, font);
    int box = S(18);
    int h = std::max(box, (int)sz.cy + S(4));
    int w = box + S(10) + (int)sz.cx;
    RECT r = takeRect(f, w, h);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;
    if (hit.clicked && enabled) value = !value;

    if (f.measure || !rectVisible(f, r)) return r;

    RECT b = { r.left, r.top + (h - box) / 2, r.left + box, r.top + (h + box) / 2 };
    COLORREF bg = value ? f.pal.accent : f.pal.surface;
    COLORREF bd = value ? f.pal.accent : (hit.hovered ? f.pal.borderStrong : f.pal.borderStrong);
    if (!enabled) { bg = value ? mixColor(f.pal.accent, f.pal.surface, 0.5f) : f.pal.surfaceAlt; bd = f.pal.border; }
    fillRect(f.dc, b, bg, S(5));
    strokeRect(f.dc, b, bd, S(5), 1);
    if (hit.focused) strokeRect(f.dc, { b.left - S(2), b.top - S(2), b.right + S(2), b.bottom + S(2) },
                                mixColor(f.pal.accent, f.pal.background, 0.3f), S(6), S(2), 120);
    if (value) {
        int is = (int)(box * 0.66f);
        RECT cb = { b.left + (box - is) / 2, b.top + (box - is) / 2, b.left + (box + is) / 2, b.top + (box + is) / 2 };
        drawIcon(f.dc, ICON_CHECK, cb, RGB(255, 255, 255), (float)S(2));
    }
    RECT tr = { b.right + S(10), r.top, r.right, r.bottom };
    drawText(f.dc, text, tr, enabled ? f.pal.text : f.pal.textFaint, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    return r;
}

RECT radioButton(Frame& f, int id, const std::wstring& text, bool selected, bool enabled, WidgetHit* outHit)
{
    HFONT font = fontUI(13);
    SIZE sz = textSize(f.dc, text, font);
    int d = S(18);
    int h = std::max(d, (int)sz.cy + S(4));
    int w = d + S(10) + sz.cx;
    RECT r = takeRect(f, w, h);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    if (f.measure || !rectVisible(f, r)) return r;
    RECT b = { r.left, r.top + (h - d) / 2, r.left + d, r.top + (h + d) / 2 };
    COLORREF bd = selected ? f.pal.accent : (hit.hovered ? f.pal.borderStrong : f.pal.borderStrong);
    if (!enabled) bd = f.pal.border;
    fillRect(f.dc, b, f.pal.surface, d / 2);
    strokeRect(f.dc, b, bd, d / 2, selected ? S(2) : 1);
    if (selected) {
        int inner = (int)(d * 0.44f);
        RECT ib = { b.left + (d - inner) / 2, b.top + (d - inner) / 2,
                    b.left + (d + inner) / 2, b.top + (d + inner) / 2 };
        fillRect(f.dc, ib, enabled ? f.pal.accent : mixColor(f.pal.accent, f.pal.surface, 0.5f), inner / 2);
    }
    if (hit.focused) strokeRect(f.dc, { b.left - S(2), b.top - S(2), b.right + S(2), b.bottom + S(2) },
                                mixColor(f.pal.accent, f.pal.background, 0.3f), d / 2 + 2, S(2), 120);
    RECT tr = { b.right + S(10), r.top, r.right, r.bottom };
    drawText(f.dc, text, tr, enabled ? f.pal.text : f.pal.textFaint, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    return r;
}

// ---------------------------------------------------------------------
// text field
// ---------------------------------------------------------------------
static void editApplyKeys(Frame& f, Frame::EditState& e, bool password);

void editKeys(Frame& f, Frame::EditState& e, bool password)
{
    if (f.measure) return;
    editApplyKeys(f, e, password);
}

static void editApplyKeys(Frame& f, Frame::EditState& e, bool password)
{
    (void)password;
    auto selRange = [&](int& a, int& b) {
        if (e.selStart >= 0 && e.selEnd >= 0 && e.selStart != e.selEnd) {
            a = std::min(e.selStart, e.selEnd); b = std::max(e.selStart, e.selEnd);
            return true;
        }
        a = b = -1; return false;
    };
    auto deleteSel = [&]() {
        int a, b;
        if (selRange(a, b)) { e.text.erase(e.text.begin() + a, e.text.begin() + b); e.caret = a; e.selStart = e.selEnd = -1; return true; }
        return false;
    };

    if (f.keyCtrlA) { e.selStart = 0; e.selEnd = (int)e.text.size(); e.caret = (int)e.text.size(); }
    if (f.keyCtrlC || f.keyCtrlX) {
        int a, b;
        if (selRange(a, b)) {
            std::wstring sel = e.text.substr(a, b - a);
            setClipboardText(f.hwnd, sel);
            if (f.keyCtrlX) { e.text.erase(a, b - a); e.caret = a; e.selStart = e.selEnd = -1; }
        }
    }
    if (f.keyCtrlV) {
        std::wstring clip = getClipboardText(f.hwnd);
        // collapse newlines for single-line fields
        std::wstring clean;
        for (wchar_t c : clip) if (c != L'\r' && c != L'\n' && c != L'\t') clean += c;
        if (!clean.empty()) {
            deleteSel();
            e.text.insert(e.text.begin() + e.caret, clean.begin(), clean.end());
            e.caret += (int)clean.size(); e.selStart = e.selEnd = -1;
        }
    }
    for (wchar_t c : f.chars) {
        if (c < 32) continue;
        deleteSel();
        e.text.insert(e.text.begin() + e.caret, c);
        e.caret++; e.selStart = e.selEnd = -1;
    }
    if (f.keyBack) {
        if (!deleteSel() && e.caret > 0) { e.text.erase(e.text.begin() + e.caret - 1); e.caret--; }
    }
    if (f.keyDel) {
        if (!deleteSel() && e.caret < (int)e.text.size()) e.text.erase(e.text.begin() + e.caret);
    }
    if (f.keyLeft) {
        if (e.selStart >= 0 && e.selEnd >= 0 && e.selStart != e.selEnd && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            e.caret = std::min(e.selStart, e.selEnd); e.selStart = e.selEnd = -1;
        } else if (e.caret > 0) {
            e.caret--;
            if (GetKeyState(VK_SHIFT) & 0x8000) { if (e.selStart < 0) e.selStart = e.caret + 1; e.selEnd = e.caret; }
            else e.selStart = e.selEnd = -1;
        }
    }
    if (f.keyRight) {
        if (e.selStart >= 0 && e.selEnd >= 0 && e.selStart != e.selEnd && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            e.caret = std::max(e.selStart, e.selEnd); e.selStart = e.selEnd = -1;
        } else if (e.caret < (int)e.text.size()) {
            e.caret++;
            if (GetKeyState(VK_SHIFT) & 0x8000) { if (e.selStart < 0) e.selStart = e.caret - 1; e.selEnd = e.caret; }
            else e.selStart = e.selEnd = -1;
        }
    }
    if (f.keyHome) { e.caret = 0; e.selStart = e.selEnd = -1; }
    if (f.keyEnd)  { e.caret = (int)e.text.size(); e.selStart = e.selEnd = -1; }
    e.caret = util::clampv<int>(e.caret, 0, (int)e.text.size());
}

RECT textField(Frame& f, int id, std::wstring& text, int width, const std::wstring& placeholder,
               bool password, bool enabled, WidgetHit* outHit)
{
    HFONT font = fontUI(13);
    int h = S(34);
    int w = width > 0 ? width : cur_width(f);
    RECT r = takeRect(f, w, h);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    Frame::EditState& e = f.edits[id];
    if (e.text != text) {           // external change -> resync
        e.text = text;
        e.caret = util::clampv<int>(e.caret, 0, (int)e.text.size());
        e.selStart = e.selEnd = -1;
    }
    if (hit.clicked && !enabled) hit.clicked = false;
    if (hit.focused && enabled) editApplyKeys(f, e, password);
    if (e.text != text) text = e.text;

    if (f.measure || !rectVisible(f, r)) return r;

    COLORREF bg = enabled ? f.pal.surface : f.pal.surfaceAlt;
    COLORREF bd = hit.focused ? f.pal.accent : (hit.hovered && enabled ? f.pal.borderStrong : f.pal.border);
    fillRect(f.dc, r, bg, S(8));
    strokeRect(f.dc, r, bd, S(8), hit.focused ? S(2) : 1);
    if (hit.focused) strokeRect(f.dc, r, f.pal.accent, S(8), S(2), 60);

    RECT cr = { r.left + S(11), r.top, r.right - S(11), r.bottom };
    std::wstring shown = e.text;
    if (password) shown.assign(e.text.size(), L'\x25CF');

    if (shown.empty() && !hit.focused && !placeholder.empty()) {
        drawText(f.dc, placeholder, cr, f.pal.textFaint, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        // caret-based horizontal scrolling
        SIZE full = textSize(f.dc, shown, font);
        int caretX = 0;
        if (!shown.empty()) {
            SIZE upto = textSize(f.dc, shown.substr(0, std::min(e.caret, (int)shown.size())), font);
            caretX = upto.cx;
        }
        int viewW = cr.right - cr.left;
        int scroll = 0;
        if (full.cx > viewW) {
            scroll = util::clampv<int>(caretX - viewW / 2, 0, (int)full.cx - viewW);
        }
        // selection highlight
        if (e.selStart >= 0 && e.selEnd >= 0 && e.selStart != e.selEnd) {
            int a = std::min(e.selStart, e.selEnd), b = std::max(e.selStart, e.selEnd);
            int xa = textSize(f.dc, shown.substr(0, a), font).cx - scroll;
            int xb = textSize(f.dc, shown.substr(0, b), font).cx - scroll;
            RECT sr = { cr.left + xa, cr.top + S(5), cr.left + xb, cr.bottom - S(5) };
            RECT clipSave = f.clip;
            IntersectClipRect(f.dc, cr.left, cr.top, cr.right, cr.bottom);
            fillRect(f.dc, sr, f.pal.accentSoft, 0);
            restoreClip(f);
        }
        RECT tr = { cr.left - scroll, cr.top, cr.left - scroll + full.cx + S(4), cr.bottom };
        IntersectClipRect(f.dc, cr.left, cr.top, cr.right, cr.bottom);
        drawText(f.dc, shown, tr, enabled ? f.pal.text : f.pal.textFaint, font, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        restoreClip(f);

        if (hit.focused && enabled) {
            f.caretVisible = true;
            if (caretBlinkOn()) {
                int cx = cr.left + caretX - scroll;
                int cy1 = cr.top + S(7), cy2 = cr.bottom - S(7);
                HPEN pen = CreatePen(PS_SOLID, S(2), f.pal.accent);
                HGDIOBJ o = SelectObject(f.dc, pen);
                MoveToEx(f.dc, cx, cy1, nullptr); LineTo(f.dc, cx, cy2);
                SelectObject(f.dc, o); DeleteObject(pen);
                f.caretRect = { cx - S(1), cy1, cx + S(1), cy2 };
            }
        }
    }
    return r;
}

// ---------------------------------------------------------------------
// spinner (numeric field with up/down)
// ---------------------------------------------------------------------
RECT spinner(Frame& f, int id, int& value, int minV, int maxV, int width,
             const std::wstring& suffix, bool enabled, WidgetHit* outHit)
{
    int w = width > 0 ? width : S(120);
    int h = S(34);
    RECT r = takeRect(f, w, h);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    Frame::EditState& e = f.edits[id];
    std::wstring vs = std::to_wstring(value);
    if (e.text != vs && e.selStart == -2) { e.text = vs; e.caret = (int)e.text.size(); }
    if (e.selStart != -2) {
        if (hit.focused && enabled) editApplyKeys(f, e, false);
        // commit on focus loss or enter
        if (!hit.focused || f.enterPressed) {
            std::wstring digits;
            for (wchar_t c : e.text) if (iswdigit(c) || c == L'-') digits += c;
            if (!digits.empty()) value = util::clampv<int>(_wtoi(digits.c_str()), minV, maxV);
            e.text = std::to_wstring(value);
            e.selStart = -2;
        } else {
            std::wstring digits;
            for (wchar_t c : e.text) if (iswdigit(c) || c == L'-') digits += c;
            if (!digits.empty()) value = util::clampv<int>(_wtoi(digits.c_str()), minV, maxV);
        }
    }

    if (f.measure || !rectVisible(f, r)) return r;
    COLORREF bg = enabled ? f.pal.surface : f.pal.surfaceAlt;
    COLORREF bd = hit.focused ? f.pal.accent : (hit.hovered && enabled ? f.pal.borderStrong : f.pal.border);
    fillRect(f.dc, r, bg, S(8));
    strokeRect(f.dc, r, bd, S(8), hit.focused ? S(2) : 1);

    int btnW = S(26);
    RECT up = { r.right - btnW, r.top + 1, r.right - 1, r.top + h / 2 };
    RECT dn = { r.right - btnW, r.top + h / 2, r.right - 1, r.bottom - 1 };
    drawDivider(f.dc, r.right - btnW, r.top + S(4), r.right - btnW, r.bottom - S(4), f.pal.border);
    drawDivider(f.dc, r.right - btnW, r.top + h / 2, r.right - 1, r.top + h / 2, f.pal.border);

    if (enabled) {
        WidgetHit uh = hitTest(f, id * 1000 + 1, up);
        WidgetHit dh = hitTest(f, id * 1000 + 2, dn);
        if (uh.hovered) fillRect(f.dc, up, f.pal.surfaceAlt, 0);
        if (dh.hovered) fillRect(f.dc, dn, f.pal.surfaceAlt, 0);
        if (uh.clicked && value < maxV) { value++; e.text = std::to_wstring(value); }
        if (dh.clicked && value > minV) { value--; e.text = std::to_wstring(value); }
        int is = S(12);
        RECT iub = { up.left + (btnW - is) / 2, up.top + (up.bottom - up.top - is) / 2,
                     up.left + (btnW + is) / 2, up.top + (up.bottom - up.top + is) / 2 };
        RECT idb = { dn.left + (btnW - is) / 2, dn.top + (dn.bottom - dn.top - is) / 2,
                     dn.left + (btnW + is) / 2, dn.top + (dn.bottom - dn.top + is) / 2 };
        drawIcon(f.dc, ICON_CHEVRON_DOWN, iub, f.pal.textDim, 0, 255);
        // rotate chevron for "up" by drawing a mirrored glyph manually
        {
            Graphics g(f.dc); g.SetSmoothingMode(SmoothingModeAntiAlias);
            Pen pen(C(f.pal.textDim), (float)S(2));
            pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
            pen.SetLineJoin(LineJoinRound);
            float cx = (iub.left + iub.right) / 2.0f, cy = (iub.top + iub.bottom) / 2.0f;
            float hx = is * 0.42f, hy = is * 0.30f;
            PointF pts[3] = { PointF(cx - hx, cy + hy), PointF(cx, cy - hy), PointF(cx + hx, cy + hy) };
            g.DrawLines(&pen, pts, 3);
            float cx2 = (idb.left + idb.right) / 2.0f, cy2 = (idb.top + idb.bottom) / 2.0f;
            PointF pts2[3] = { PointF(cx2 - hx, cy2 - hy), PointF(cx2, cy2 + hy), PointF(cx2 + hx, cy2 - hy) };
            g.DrawLines(&pen, pts2, 3);
        }
    }

    RECT tr = { r.left + S(11), r.top, r.right - btnW - S(4), r.bottom };
    std::wstring shown = std::to_wstring(value) + suffix;
    if (hit.focused && enabled) {
        // draw with caret
        std::wstring t = e.text;
        SIZE full = textSize(f.dc, t, fontUI(13));
        int caretX = textSize(f.dc, t.substr(0, util::clampv(e.caret, 0, (int)t.size())), fontUI(13)).cx;
        drawText(f.dc, t, tr, f.pal.text, fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int cx = tr.left + caretX;
        HPEN pen = CreatePen(PS_SOLID, S(2), f.pal.accent);
        HGDIOBJ o = SelectObject(f.dc, pen);
        MoveToEx(f.dc, cx, r.top + S(7), nullptr); LineTo(f.dc, cx, r.bottom - S(7));
        SelectObject(f.dc, o); DeleteObject(pen);
        (void)full;
    } else {
        drawText(f.dc, shown, tr, enabled ? f.pal.text : f.pal.textFaint, fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    return r;
}

// ---------------------------------------------------------------------
// slider
// ---------------------------------------------------------------------
RECT slider(Frame& f, int id, int& value, int minV, int maxV, int width,
            const std::wstring& labelText, bool enabled, WidgetHit* outHit)
{
    int w = width > 0 ? width : cur_width(f);
    int h = S(24);
    RECT r = takeRect(f, w, h);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    int thumb = S(16);
    int trackY = r.top + h / 2;
    int x0 = r.left + thumb / 2, x1 = r.right - thumb / 2;
    float t = maxV > minV ? (float)(value - minV) / (float)(maxV - minV) : 0.f;
    int tx = x0 + (int)((x1 - x0) * t);

    if (hit.pressed && enabled && f.leftDown) {
        int nx = util::clampv<int>((int)f.mouse.x, x0, x1);
        float nt = (float)(nx - x0) / (float)std::max(1, x1 - x0);
        int nv = minV + (int)(nt * (maxV - minV) + 0.5f);
        if (nv != value) { value = util::clampv<int>(nv, minV, maxV); }
    }

    if (f.measure || !rectVisible(f, r)) return r;
    int th = S(4);
    RECT track = { x0, trackY - th / 2, x1, trackY + th / 2 };
    fillRect(f.dc, track, enabled ? f.pal.surfaceAlt : f.pal.surfaceAlt, th / 2);
    RECT fill = { x0, trackY - th / 2, tx, trackY + th / 2 };
    fillRect(f.dc, fill, enabled ? f.pal.accent : mixColor(f.pal.accent, f.pal.surface, 0.5f), th / 2);

    RECT kb = { tx - thumb / 2, trackY - thumb / 2, tx + thumb / 2, trackY + thumb / 2 };
    if (hit.pressed || hit.hovered) drawShadow(f.dc, kb, thumb / 2, S(4), 40);
    fillRect(f.dc, kb, f.pal.surface, thumb / 2);
    strokeRect(f.dc, kb, enabled ? f.pal.accent : f.pal.borderStrong, thumb / 2, S(2));
    if (hit.focused) strokeRect(f.dc, { kb.left - S(3), kb.top - S(3), kb.right + S(3), kb.bottom + S(3) },
                                mixColor(f.pal.accent, f.pal.background, 0.3f), thumb / 2 + 3, S(2), 120);
    return r;
}

// ---------------------------------------------------------------------
// combo box (owner-drawn popup)
// ---------------------------------------------------------------------
RECT combo(Frame& f, int id, const std::vector<std::wstring>& items, int& selected,
           int width, bool enabled, WidgetHit* outHit)
{
    int w = width > 0 ? width : cur_width(f);
    int h = S(34);
    RECT r = takeRect(f, w, h);
    WidgetHit hit = hitTest(f, id, r);
    if (!enabled) { hit.clicked = false; hit.pressed = false; }
    if (outHit) *outHit = hit;

    if (f.measure || !rectVisible(f, r)) return r;
    COLORREF bg = enabled ? f.pal.surface : f.pal.surfaceAlt;
    COLORREF bd = hit.focused ? f.pal.accent : (hit.hovered && enabled ? f.pal.borderStrong : f.pal.border);
    fillRect(f.dc, r, bg, S(8));
    strokeRect(f.dc, r, bd, S(8), hit.focused ? S(2) : 1);

    std::wstring cur = (selected >= 0 && selected < (int)items.size()) ? items[selected] : L"";
    RECT tr = { r.left + S(12), r.top, r.right - S(34), r.bottom };
    drawText(f.dc, cur, tr, enabled ? f.pal.text : f.pal.textFaint, fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    int is = S(14);
    RECT ib = { r.right - S(26), (r.top + r.bottom - is) / 2, r.right - S(26) + is, (r.top + r.bottom + is) / 2 };
    drawIcon(f.dc, ICON_CHEVRON_DOWN, ib, f.pal.textDim);

    if (hit.clicked && enabled && !items.empty()) {
        POINT pt = { r.left, r.bottom + S(4) };
        ClientToScreen(f.hwnd, &pt);
        std::vector<MenuItem> menu;
        for (size_t i = 0; i < items.size(); i++) {
            MenuItem mi; mi.text = items[i]; mi.id = (int)i + 1; mi.checked = ((int)i == selected);
            menu.push_back(mi);
        }
        int res = popupMenu(f.hwnd, pt, menu, w);
        if (res > 0) selected = res - 1;
    }
    return r;
}

// ---------------------------------------------------------------------
// progress bar
// ---------------------------------------------------------------------
RECT progressBar(Frame& f, int percent, int width, int height)
{
    int w = width > 0 ? width : cur_width(f);
    int h = height > 0 ? height : S(8);
    RECT r = takeRect(f, w, h);
    if (f.measure || !rectVisible(f, r)) return r;
    fillRect(f.dc, r, f.pal.surfaceAlt, h / 2);
    int p = util::clampv<int>(percent, 0, 100);
    if (p > 0) {
        int fw = std::max(h, (int)((w - 0) * p / 100.0f));
        RECT fr = { r.left, r.top, r.left + fw, r.bottom };
        fillRect(f.dc, fr, f.pal.accent, h / 2);
    }
    strokeRect(f.dc, r, f.pal.border, h / 2, 1);
    return r;
}

// ---------------------------------------------------------------------
// list box
// ---------------------------------------------------------------------
ListBoxResult listBox(Frame& f, int id, const std::vector<std::wstring>& items, int& selected,
                      int width, int height, bool enabled)
{
    ListBoxResult res;
    int w = width > 0 ? width : cur_width(f);
    RECT r = takeRect(f, w, height);
    if (f.measure || !rectVisible(f, r)) return res;

    fillRect(f.dc, r, f.pal.surface, S(8));
    strokeRect(f.dc, r, f.pal.border, S(8), 1);

    int rowH = S(30);
    int total = (int)items.size() * rowH;
    RECT cr = { r.left + 1, r.top + 1, r.right - 1, r.bottom - 1 };
    IntersectClipRect(f.dc, cr.left, cr.top, cr.right, cr.bottom);

    static int lbScroll = 0;
    if (f.wheelDelta != 0 && PtInRect(&r, f.mouse)) {
        lbScroll = util::clampv<int>(lbScroll - f.wheelDelta / 4, 0, std::max(0, total - (int)(cr.bottom - cr.top)));
        f.wheelDelta = 0;
    }

    for (int i = 0; i < (int)items.size(); i++) {
        RECT ir = { cr.left + S(4), cr.top + S(4) + i * rowH - lbScroll, cr.right - S(4), cr.top + S(4) + (i + 1) * rowH - lbScroll };
        if (ir.bottom < cr.top || ir.top > cr.bottom) continue;
        WidgetHit ih = hitTest(f, id * 10000 + i + 1, ir);
        if (ih.clicked && enabled) { selected = i; res.clicked = i; res.changed = true; }
        if (ih.doubleClicked && enabled) res.doubleClicked = i;
        COLORREF bg = (i == selected) ? f.pal.accentSoft : (ih.hovered && enabled ? f.pal.surfaceHover : (COLORREF)0xFFFFFFFF);
        if (!isTransparent(bg)) fillRect(f.dc, ir, bg, S(6));
        RECT tr = { ir.left + S(10), ir.top, ir.right - S(10), ir.bottom };
        drawText(f.dc, items[i], tr, (i == selected) ? f.pal.accent : f.pal.text,
                 fontUI(13, i == selected ? FW_SEMIBOLD : FW_NORMAL),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    restoreClip(f);
    return res;
}

// ---------------------------------------------------------------------
// scrollbar
// ---------------------------------------------------------------------
int scrollbar(Frame& f, int id, RECT track, int contentHeight, int viewHeight, int scrollY)
{
    f.scrollRange = std::max(0, contentHeight - viewHeight);
    f.scrollPage = std::max(1, viewHeight);
    if (f.scrollRange <= 0) { f.scrollVisible = false; return 0; }
    f.scrollVisible = true;
    scrollY = util::clampv(scrollY, 0, f.scrollRange);

    int th = track.bottom - track.top;
    int barH = std::max(S(30), (int)((float)th * viewHeight / (float)contentHeight));
    int barY = track.top + (int)((float)(th - barH) * scrollY / (float)f.scrollRange);
    RECT bar = { track.left, barY, track.right, barY + barH };

    if (f.wheelDelta != 0) {
        int step = std::max(S(40), viewHeight / 8);
        scrollY = util::clampv<int>(scrollY - (f.wheelDelta / 120) * step, 0, f.scrollRange);
        f.wheelDelta = 0;
    }

    WidgetHit hit = hitTest(f, id, track);
    if (hit.pressed && f.leftDown) {
        if (f.dragId == id) {
            int dy = f.mouse.y - f.dragStartY;
            int range = th - barH;
            if (range > 0) scrollY = util::clampv<int>((int)((float)(f.dragScrollBase + dy) / range * f.scrollRange), 0, f.scrollRange);
        } else if (PtInRect(&bar, f.mouse)) {
            f.dragId = id; f.dragStartY = f.mouse.y; f.dragScrollBase = barY - track.top;
        } else {
            // page up/down
            int dir = (f.mouse.y < bar.top) ? -1 : 1;
            scrollY = util::clampv<int>(scrollY + dir * viewHeight * 3 / 4, 0, f.scrollRange);
        }
    }
    if (f.activeId == id && f.leftReleased) f.dragId = 0;

    bool hovered = hit.hovered || f.dragId == id;
    fillRect(f.dc, track, isTransparent(f.pal.scrollTrack) ? f.pal.scrollTrack : f.pal.scrollTrack, track.right - track.left > S(8) ? S(4) : 0);
    RECT br = bar;
    br.left += S(1); br.right -= S(1);
    fillRect(f.dc, br, hovered ? f.pal.scrollThumbHover : f.pal.scrollThumb, (br.right - br.left) / 2);
    return scrollY;
}

// ---------------------------------------------------------------------
// tooltips
// ---------------------------------------------------------------------
void requestTooltip(Frame& f, const std::wstring& text, RECT where)
{
    g_tipText = text;
    g_tipRect = where;
    ClientToScreen(f.hwnd, (POINT*)&g_tipRect.left);
    ClientToScreen(f.hwnd, (POINT*)&g_tipRect.right);
}
const std::wstring& pendingTooltip() { return g_tipText; }
RECT pendingTooltipRect() { return g_tipRect; }
void clearPendingTooltip() { g_tipText.clear(); }

} // namespace ui
