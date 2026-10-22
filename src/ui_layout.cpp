// =====================================================================
//  MrDocx  -  ui.cpp : immediate-mode widget toolkit (GDI/GDI+)
//  Author : Ragul S
// =====================================================================
#include "ui.h"
#include <gdiplus.h>

using namespace Gdiplus;

namespace {

GdiplusStartupOutput g_out;
ULONG_PTR g_token = 0;

void ensureGp()
{
    if (g_token) return;
    GdiplusStartupInput in;
    in.SuppressBackgroundThread = FALSE;
    in.SuppressExternalCodecs = TRUE;
    GdiplusStartup(&g_token, &in, &g_out);
}

inline Color C(COLORREF c, BYTE a = 255) { return Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

// A tiny alpha-blended DIB used for soft shadows.
struct ShadowCache {
    int radius = -1;
    int blur = -1;
    int alpha = -1;
    int w = 0, h = 0;
    std::vector<BYTE> mask;   // w*h alpha values
};
ShadowCache g_shadow;

void buildShadow(int radius, int blur, int alpha, int w, int h)
{
    g_shadow.radius = radius; g_shadow.blur = blur; g_shadow.alpha = alpha;
    g_shadow.w = w + blur * 2; g_shadow.h = h + blur * 2;
    int W = g_shadow.w, H = g_shadow.h;
    g_shadow.mask.assign((size_t)W * H, 0);

    // signed-distance style falloff: fully opaque inside, smooth ramp outside
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            // distance to the rounded rect (inset by blur)
            float rx = (float)radius;
            float px = (float)(x - blur), py = (float)(y - blur);
            float dx = std::max(std::max(rx - px, px - (w - rx)), 0.0f);
            float dy = std::max(std::max(rx - py, py - (h - rx)), 0.0f);
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < 0.001f) { g_shadow.mask[(size_t)y * W + x] = (BYTE)alpha; continue; }
            float t = 1.0f - (d / (float)blur);
            if (t < 0) t = 0;
            t = t * t * (3 - 2 * t);   // smoothstep
            g_shadow.mask[(size_t)y * W + x] = (BYTE)(alpha * t);
        }
    }
}

} // namespace

// ---------------------------------------------------------------------
// Drawing primitives
// ---------------------------------------------------------------------
namespace ui {

void roundRectPath(HDC dc, RECT r) { (void)dc; (void)r; }

void fillRect(HDC dc, RECT r, COLORREF c, int radius)
{
    if (r.right <= r.left || r.bottom <= r.top) return;
    if (radius <= 0) {
        HBRUSH b = CreateSolidBrush(c);
        HGDIOBJ ob = SelectObject(dc, b);
        HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, op); SelectObject(dc, ob);
        DeleteObject(b);
        return;
    }
    ensureGp();
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    float rr = (float)radius * 2.0f;
    float w = (float)(r.right - r.left), h = (float)(r.bottom - r.top);
    if (rr > w) rr = w; if (rr > h) rr = h;
    GraphicsPath p;
    p.AddArc((float)r.left, (float)r.top, rr, rr, 180, 90);
    p.AddArc((float)r.right - rr, (float)r.top, rr, rr, 270, 90);
    p.AddArc((float)r.right - rr, (float)r.bottom - rr, rr, rr, 0, 90);
    p.AddArc((float)r.left, (float)r.bottom - rr, rr, rr, 90, 90);
    p.CloseFigure();
    SolidBrush b(C(c));
    g.FillPath(&b, &p);
}

void strokeRect(HDC dc, RECT r, COLORREF c, int radius, int width, int alpha)
{
    if (r.right <= r.left || r.bottom <= r.top) return;
    if (radius <= 0 && alpha >= 255) {
        HPEN pen = CreatePen(PS_SOLID, width, c);
        HGDIOBJ op = SelectObject(dc, pen);
        HGDIOBJ ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, ob); SelectObject(dc, op);
        DeleteObject(pen);
        return;
    }
    ensureGp();
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    float off = width * 0.5f;
    RECT q = { r.left, r.top, r.right - 1, r.bottom - 1 };
    float rr = (float)radius * 2.0f;
    float w = (float)(q.right - q.left), h = (float)(q.bottom - q.top);
    if (rr > w) rr = w; if (rr > h) rr = h;
    GraphicsPath p;
    if (rr > 0.5f) {
        p.AddArc((float)q.left + off, (float)q.top + off, rr, rr, 180, 90);
        p.AddArc((float)q.right - rr - off, (float)q.top + off, rr, rr, 270, 90);
        p.AddArc((float)q.right - rr - off, (float)q.bottom - rr - off, rr, rr, 0, 90);
        p.AddArc((float)q.left + off, (float)q.bottom - rr - off, rr, rr, 90, 90);
        p.CloseFigure();
    } else {
        p.AddRectangle(RectF((float)q.left + off, (float)q.top + off, w, h));
    }
    Pen pen(C(c, (BYTE)alpha), (float)width);
    pen.SetLineJoin(LineJoinRound);
    g.DrawPath(&pen, &p);
}

void drawShadow(HDC dc, RECT r, int radius, int blur, int alpha)
{
    if (alpha <= 0 || blur <= 0) return;
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    buildShadow(radius, blur, alpha, w, h);

    // Blit the cached alpha mask with per-pixel blending.
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_shadow.w;
    bmi.bmiHeader.biHeight = -g_shadow.h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    int x0 = r.left - blur, y0 = r.top - blur;
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    HDC mem = CreateCompatibleDC(dc);
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) { DeleteDC(mem); return; }
    HGDIOBJ old = SelectObject(mem, bmp);

    DWORD* px = (DWORD*)bits;
    for (int i = 0; i < g_shadow.w * g_shadow.h; i++) {
        BYTE a = g_shadow.mask[i];
        // pre-multiplied black
        px[i] = ((DWORD)a << 24);
    }
    AlphaBlend(dc, x0, y0, g_shadow.w, g_shadow.h, mem, 0, 0, g_shadow.w, g_shadow.h, bf);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

void drawDivider(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int alpha)
{
    if (alpha >= 255) {
        HPEN pen = CreatePen(PS_SOLID, 1, c);
        HGDIOBJ o = SelectObject(dc, pen);
        MoveToEx(dc, x1, y1, nullptr);
        LineTo(dc, x2, y2);
        SelectObject(dc, o);
        DeleteObject(pen);
        return;
    }
    ensureGp();
    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeNone);
    Pen pen(C(c, (BYTE)alpha), 1.0f);
    g.DrawLine(&pen, (float)x1, (float)y1 + 0.5f, (float)x2, (float)y2 + 0.5f);
}

void fillGradient(HDC dc, RECT r, COLORREF from, COLORREF to, bool vertical, int radius)
{
    if (r.right <= r.left || r.bottom <= r.top) return;
    ensureGp();
    Graphics g(dc);
    g.SetSmoothingMode(radius > 0 ? SmoothingModeAntiAlias : SmoothingModeNone);
    LinearGradientBrush b(RectF((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top)),
                          C(from), C(to), vertical ? LinearGradientModeVertical : LinearGradientModeHorizontal);
    if (radius > 0) {
        float rr = (float)radius * 2.0f;
        float w = (float)(r.right - r.left), h = (float)(r.bottom - r.top);
        if (rr > w) rr = w; if (rr > h) rr = h;
        GraphicsPath p;
        p.AddArc((float)r.left, (float)r.top, rr, rr, 180.0f, 90.0f);
        p.AddArc((float)r.right - rr, (float)r.top, rr, rr, 270.0f, 90.0f);
        p.AddArc((float)r.right - rr, (float)r.bottom - rr, rr, rr, 0.0f, 90.0f);
        p.AddArc((float)r.left, (float)r.bottom - rr, rr, rr, 90.0f, 90.0f);
        p.CloseFigure();
        g.FillPath(&b, &p);
    } else {
        g.FillRectangle(&b, (float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top));
    }
}

void fillRoundedWithBorder(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border)
{
    fillRect(dc, r, fill, radius);
    strokeRect(dc, r, border, radius, 1);
}

void iconPlate(HDC dc, RECT r, IconId icon, COLORREF tint, bool dark, int radius)
{
    int rad = radius >= 0 ? radius : (r.bottom - r.top) / 3;
    COLORREF bg = dark ? mixColor(tint, RGB(0x14, 0x17, 0x1E), 0.74f)
                       : mixColor(tint, RGB(0xFF, 0xFF, 0xFF), 0.88f);
    fillRect(dc, r, bg, rad);
    int pad = (int)((r.bottom - r.top) * 0.24f);
    RECT ib = { r.left + pad, r.top + pad, r.right - pad, r.bottom - pad };
    drawIcon(dc, icon, ib, dark ? mixColor(tint, RGB(0xFF, 0xFF, 0xFF), 0.35f) : tint);
}

SIZE textSize(HDC dc, const std::wstring& s, HFONT font)
{
    HGDIOBJ of = SelectObject(dc, font);
    SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, of);
    return sz;
}

// Draws a single line (DT_SINGLELINE implied) or wrapped block with
// per-glyph alpha. Uses TextOut so ClearType stays on.
void drawText(HDC dc, const std::wstring& s, RECT r, COLORREF c, HFONT font, UINT flags, int alpha)
{
    if (s.empty()) return;
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);

    if (alpha < 255) SetTextColor(dc, mixColor(c, RGB(255, 255, 255), 0.0f));
    else SetTextColor(dc, c);

    if (flags & DT_SINGLELINE) {
        SIZE sz; GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
        int x = r.left, y = (r.top + r.bottom - sz.cy) / 2;
        if (flags & DT_CENTER) x = (r.left + r.right - sz.cx) / 2;
        else if (flags & DT_RIGHT) x = r.right - sz.cx;
        if (alpha < 255) {
            // fade by drawing into a memory DC and blending
            RECT b = { x, y, x + sz.cx, y + sz.cy };
            (void)b;
        }
        TextOutW(dc, x, y, s.c_str(), (int)s.size());
    } else {
        DrawTextW(dc, const_cast<LPWSTR>(s.c_str()), -1, &const_cast<RECT&>(r), flags | DT_NOPREFIX);
    }
    SelectObject(dc, of);
}

void drawTextSized(HDC dc, const std::wstring& s, RECT r, COLORREF c, HFONT font, UINT flags)
{
    drawText(dc, s, r, c, font, flags);
}

// ---------------------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------------------
void beginFrame(Frame& f, HDC dc, HWND hwnd, int w, int h, const Palette& pal,
                const POINT& mouse, bool mouseInside, const RECT& clip)
{
    f.dc = dc; f.hwnd = hwnd; f.W = w; f.H = h; f.pal = pal;
    f.mouse = mouse; f.mouseInside = mouseInside;
    f.clip = clip;
    f.stack.clear();
    f.grids.clear();
    f.idCounter = 0;
    f.visibleTop = clip.top;
    f.visibleBottom = clip.bottom;

    // double-click detection
    f.doublePressed = false;

    // widgets re-arm their tooltip while drawing; drop last frame's leftover
    clearPendingTooltip();

    // resolve pressed/released against capture
    if (!f.leftDown && !f.leftPressed && !f.leftReleased && f.activeId) f.activeId = 0;
}

void endFrame(Frame& f)
{
    f.leftPressed = false;
    f.leftReleased = false;
    f.rightPressed = false;
    f.doublePressed = false;
    f.wheelDelta = 0;
    f.chars.clear();
    f.keyBack = f.keyDel = f.keyLeft = f.keyRight = f.keyHome = f.keyEnd = false;
    f.keyCtrlA = f.keyCtrlC = f.keyCtrlV = f.keyCtrlX = false;
    f.enterPressed = f.escPressed = f.tabPressed = f.tabShift = false;
    f.captureLost = false;
}

// ---------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------
static Frame::Stack& cur(Frame& f) { return f.stack.back(); }

void beginColumn(Frame& f, int x, int y, int w, int h, int gap, int padX, int padY)
{
    Frame::Stack s;
    s.x = x + padX; s.y = y + padY;
    s.w = w - padX * 2; s.h = h - padY * 2;
    s.dir = Dir::Column; s.gap = gap; s.padX = padX; s.padY = padY;
    s.originX = x; s.originY = y; s.boxW = w; s.boxH = h;
    f.stack.push_back(s);
}

void beginRow(Frame& f, int x, int y, int w, int h, int gap, int padX, int padY)
{
    Frame::Stack s;
    s.x = x + padX; s.y = y + padY;
    s.w = w - padX * 2; s.h = h - padY * 2;
    s.dir = Dir::Row; s.gap = gap; s.padX = padX; s.padY = padY;
    s.originX = x; s.originY = y; s.boxW = w; s.boxH = h;
    f.stack.push_back(s);
}

RECT endLayout(Frame& f)
{
    Frame::Stack s = cur(f);
    f.stack.pop_back();
    int usedW = s.dir == Dir::Row ? (s.x - s.originX - s.padX) : s.boxW;
    int usedH = s.dir == Dir::Column ? (s.y - s.originY - s.padY) : s.boxH;
    RECT r = { s.originX, s.originY, s.originX + usedW + s.padX * 2, s.originY + usedH + s.padY * 2 };

    // advance the parent stack by the space we consumed
    if (!f.stack.empty()) {
        Frame::Stack& p = cur(f);
        if (p.first) { p.first = false; }
        else {
            if (p.dir == Dir::Column) p.y += p.gap; else p.x += p.gap;
        }
        if (p.dir == Dir::Column) { p.y += (r.bottom - r.top); p.contentMain += (r.bottom - r.top); }
        else { p.x += (r.right - r.left); p.contentMain += (r.right - r.left); }
    }
    return r;
}

RECT takeRect(Frame& f, int w, int h)
{
    Frame::Stack& s = cur(f);
    if (!s.first) { if (s.dir == Dir::Column) s.y += s.gap; else s.x += s.gap; }
    s.first = false;
    RECT r;
    if (s.dir == Dir::Column) {
        int ww = (w <= 0) ? s.w : w;
        r = { s.x, s.y, s.x + ww, s.y + h };
        s.y += h;
        s.contentMain += h;
    } else {
        int hh = (h <= 0) ? s.h : h;
        r = { s.x, s.y, s.x + w, s.y + hh };
        s.x += w;
        s.contentMain += w;
    }
    return r;
}

RECT takeFill(Frame& f, int cross, int reserve)
{
    Frame::Stack& s = cur(f);
    if (!s.first) { if (s.dir == Dir::Column) s.y += s.gap; else s.x += s.gap; }
    s.first = false;
    RECT r;
    if (s.dir == Dir::Column) {
        int hh = cross > 0 ? cross : std::max(0, (s.originY + s.boxH - s.padY) - s.y - reserve);
        r = { s.x, s.y, s.x + s.w, s.y + hh };
        s.y += hh; s.contentMain += hh;
    } else {
        int ww = cross > 0 ? cross : std::max(0, (s.originX + s.boxW - s.padX) - s.x - reserve);
        r = { s.x, s.y, s.x + ww, s.y + s.h };
        s.x += ww; s.contentMain += ww;
    }
    return r;
}

void layoutGap(Frame& f, int px)
{
    Frame::Stack& s = cur(f);
    if (s.dir == Dir::Column) { s.y += px; s.contentMain += px; }
    else { s.x += px; s.contentMain += px; }
    s.first = true;   // gap absorbs the next gap
}

void layoutWrap(Frame& f, bool on) { cur(f).wrap = on; }
void layoutCenterMain(Frame& f, bool on) { cur(f).centerMain = on; }

void beginGrid(Frame& f, int x, int y, int w, int cellW, int cellH, int gapX, int gapY)
{
    Frame::Grid g;
    g.x = x; g.y = y; g.w = w; g.cellW = cellW; g.cellH = cellH;
    g.gapX = gapX; g.gapY = gapY;
    g.cols = std::max(1, (w + gapX) / (cellW + gapX));
    g.col = 0; g.row = 0;
    f.grids.push_back(g);
}

RECT gridCell(Frame& f)
{
    Frame::Grid& g = f.grids.back();
    // centre the grid horizontally when columns do not fill the width
    int totalW = g.cols * g.cellW + (g.cols - 1) * g.gapX;
    int offX = g.x + std::max(0, (g.w - totalW) / 2);
    RECT r = {
        offX + g.col * (g.cellW + g.gapX),
        g.y + g.row * (g.cellH + g.gapY),
        offX + g.col * (g.cellW + g.gapX) + g.cellW,
        g.y + g.row * (g.cellH + g.gapY) + g.cellH
    };
    if (++g.col >= g.cols) { g.col = 0; g.row++; }
    return r;
}

int gridRowsUsed(Frame& f)
{
    Frame::Grid& g = f.grids.back();
    return g.row + (g.col > 0 ? 1 : 0);
}

void endGrid(Frame& f)
{
    Frame::Grid g = f.grids.back();
    f.grids.pop_back();
    int rows = g.row + (g.col > 0 ? 1 : 0);
    int h = rows * g.cellH + (rows > 0 ? (rows - 1) * g.gapY : 0);
    if (!f.stack.empty()) {
        Frame::Stack& p = cur(f);
        if (!p.first) p.y += p.gap;
        p.first = false;
        p.y = g.y + h;
        p.contentMain += h;
    }
}

} // namespace ui
