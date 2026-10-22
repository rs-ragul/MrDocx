// =====================================================================
//  MrDocx  -  views.cpp : title bar, sidebar, home grid, about
//  Author : Ragul S
// =====================================================================
#include "cards.h"
#include <gdiplus.h>

using namespace Gdiplus;

// ---------------------------------------------------------------------
// card scaffolding shared by the tool view and the home view
// ---------------------------------------------------------------------
int cardHeaderH() { return S(52); }

void drawCardChrome(Frame& f, RECT r, const std::wstring& title, int step, const std::wstring& hint)
{
    ui::drawShadow(f.dc, r, S(12), S(10), f.pal.dark ? 60 : 26);
    ui::fillRect(f.dc, r, f.pal.surface, S(12));
    ui::strokeRect(f.dc, r, f.pal.border, S(12), 1);

    int x = r.left + S(20);
    int cy = r.top + S(26);
    if (step > 0) {
        int d = S(24);
        RECT b = { x, cy - d / 2, x + d, cy + d / 2 };
        ui::fillRect(f.dc, b, f.pal.accentSoft, d / 2);
        ui::drawText(f.dc, std::to_wstring(step), b, f.pal.accent, fontUI(12, FW_SEMIBOLD),
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += d + S(12);
    }
    RECT tr = { x, r.top + S(10), r.right - S(20) - (hint.empty() ? 0 : S(240)), r.top + S(44) };
    ui::drawText(f.dc, title, tr, f.pal.text, fontUI(15, FW_SEMIBOLD),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    if (!hint.empty()) {
        RECT hr = { r.right - S(20) - S(300), r.top + S(10), r.right - S(20), r.top + S(44) };
        ui::drawText(f.dc, hint, hr, f.pal.textFaint, fontUI(12),
                     DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    ui::drawDivider(f.dc, r.left + 1, r.top + cardHeaderH(), r.right - 1, r.top + cardHeaderH(),
                    f.pal.border, 255);
}

// Begins a card. In the measure pass nothing is drawn; in the draw pass
// the chrome is painted using the rect captured during measurement.
CardScope cardBegin(Frame& f, Cards& c, const std::wstring& title, int step,
                    const std::wstring& hint, int gap)
{
    Frame::Stack& p = f.top();
    if (!p.first) p.y += gap;
    p.first = false;

    int x = p.x, y = p.y, w = p.w;
    if (!f.measure && c.idx < (int)c.rects.size())
        drawCardChrome(f, c.rects[c.idx], title, step, hint);

    ui::beginColumn(f, x, y + cardHeaderH(), w, 1 << 20, gap, S(20), S(16));
    return CardScope{ x, y, w };
}

void cardEnd(Frame& f, Cards& c, CardScope cs)
{
    RECT inner = ui::endLayout(f);
    Frame::Stack& p = f.top();
    p.y = inner.bottom + S(18);
    p.contentMain += 0;
    RECT full = { cs.x, cs.y, cs.x + cs.w, p.y };
    if (f.measure) c.rects.push_back(full);
    else c.idx++;
}

// ---------------------------------------------------------------------
// title bar
// ---------------------------------------------------------------------
namespace views {

int titleBarH() { return S(40); }

static void captionButton(Frame& f, RECT r, IconId icon, bool isClose, RECT* store)
{
    if (store) *store = r;
    POINT m = f.mouse;
    bool hot = f.mouseInside && PtInRect(&r, m);
    bool down = hot && f.leftDown;

    if (hot) {
        COLORREF c = isClose ? f.pal.titleBtnClose : f.pal.titleBtnHover;
        HBRUSH b = CreateSolidBrush(down && !isClose ? f.pal.titleBtnDown : c);
        FillRect(f.dc, &r, b);
        DeleteObject(b);
    }
    COLORREF fg = (hot && isClose) ? RGB(255, 255, 255)
                                   : (app().active ? f.pal.titleText : f.pal.textFaint);
    int is = S(11);
    RECT ib = { (r.left + r.right - is) / 2, (r.top + r.bottom - is) / 2,
                (r.left + r.right + is) / 2, (r.top + r.bottom + is) / 2 };
    drawIcon(f.dc, icon, ib, fg, (float)std::max(1, S(1)));
}

void paintTitleBar(Frame& f, RECT area)
{
    HBRUSH b = CreateSolidBrush(f.pal.titleBar);
    FillRect(f.dc, &area, b);
    DeleteObject(b);
    ui::drawDivider(f.dc, area.left, area.bottom - 1, area.right, area.bottom - 1, f.pal.border);

    AppState& a = app();

    // left: breadcrumb
    int x = area.left + S(14);
    int h = area.bottom - area.top;
    RECT crumb = { x, area.top, area.left + S(420), area.bottom };

    if (a.view == ViewId::Tool) {
        const ToolDef& def = toolDef(a.tool);
        // back button
        RECT back = { x, area.top + (h - S(28)) / 2, x + S(28), area.top + (h + S(28)) / 2 };
        POINT m = f.mouse;
        bool hot = f.mouseInside && PtInRect(&back, m);
        if (hot) ui::fillRect(f.dc, back, f.pal.titleBtnHover, S(6));
        int is = S(15);
        RECT ib = { back.left + (S(28) - is) / 2, back.top + (S(28) - is) / 2,
                    back.left + (S(28) + is) / 2, back.top + (S(28) + is) / 2 };
        drawIcon(f.dc, ICON_BACK, ib, hot ? f.pal.text : f.pal.textDim);
        a.btnBack = back;
        x += S(34);

        ui::drawText(f.dc, L"Home", RECT{ x, area.top, x + S(60), area.bottom }, f.pal.textFaint,
                     fontUI(12), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += S(42);
        RECT chev = { x, area.top + (h - S(10)) / 2, x + S(10), area.top + (h + S(10)) / 2 };
        drawIcon(f.dc, ICON_CHEVRON_RIGHT, chev, f.pal.textFaint, (float)std::max(1, S(1)));
        x += S(18);
        RECT tintPlate = { x, area.top + (h - S(20)) / 2, x + S(20), area.top + (h + S(20)) / 2 };
        ui::iconPlate(f.dc, tintPlate, def.icon, def.tint, f.pal.dark, S(6));
        x += S(28);
        ui::drawText(f.dc, def.name, RECT{ x, area.top, area.right - S(200), area.bottom },
                     f.pal.titleText, fontUI(13, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else if (a.view == ViewId::About) {
        a.btnBack = RECT{ 0, 0, 0, 0 };
        RECT back = { x, area.top + (h - S(28)) / 2, x + S(28), area.top + (h + S(28)) / 2 };
        POINT m = f.mouse;
        if (f.mouseInside && PtInRect(&back, m)) ui::fillRect(f.dc, back, f.pal.titleBtnHover, S(6));
        int is = S(15);
        drawIcon(f.dc, ICON_BACK, RECT{ back.left + (S(28) - is) / 2, back.top + (S(28) - is) / 2,
                 back.left + (S(28) + is) / 2, back.top + (S(28) + is) / 2 }, f.pal.textDim);
        a.btnBack = back;
        x += S(34);
        ui::drawText(f.dc, L"About MrDocx", RECT{ x, area.top, area.right - S(200), area.bottom },
                     f.pal.titleText, fontUI(13, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        a.btnBack = RECT{ 0, 0, 0, 0 };
        (void)crumb;
    }

    // right: theme toggle + caption buttons
    int cw = S(46);
    RECT rc;
    rc = RECT{ area.right - cw, area.top, area.right, area.bottom };
    captionButton(f, rc, ICON_CLOSE, true, &a.btnClose);
    rc = RECT{ area.right - cw * 2, area.top, area.right - cw, area.bottom };
    captionButton(f, rc, a.maximized ? ICON_RESTORE : ICON_MAXIMIZE, false, &a.btnMax);
    rc = RECT{ area.right - cw * 3, area.top, area.right - cw * 2, area.bottom };
    captionButton(f, rc, ICON_MINIMIZE, false, &a.btnMin);

    rc = RECT{ area.right - cw * 4, area.top, area.right - cw * 3, area.bottom };
    {
        POINT m = f.mouse;
        bool hot = f.mouseInside && PtInRect(&rc, m);
        if (hot) ui::fillRect(f.dc, rc, f.pal.titleBtnHover, S(6));
        int is = S(16);
        drawIcon(f.dc, f.pal.dark ? ICON_SUN : ICON_MOON,
                 RECT{ rc.left + (cw - is) / 2, rc.top + (h - is) / 2, rc.left + (cw + is) / 2, rc.top + (h + is) / 2 },
                 hot ? f.pal.text : f.pal.textDim);
        a.btnTheme = rc;
    }
}

// ---------------------------------------------------------------------
// sidebar
// ---------------------------------------------------------------------
struct NavItem { const wchar_t* label; IconId icon; int kind; int value; };

void paintSidebar(Frame& f, RECT area)
{
    AppState& a = app();
    HBRUSH b = CreateSolidBrush(f.pal.sidebar);
    FillRect(f.dc, &area, b);
    DeleteObject(b);

    int w = area.right - area.left;

    // ---- brand -------------------------------------------------------
    int y = area.top + S(14);
    int plate = S(32);
    RECT logo = { area.left + S(16), y, area.left + S(16) + plate, y + plate };
    if (logo::available()) {
        logo::draw(f.dc, logo);
    } else {
        // rounded accent plate with the merge glyph
        ui::fillGradient(f.dc, logo, RGBv(0x3A, 0x66, 0xFF), RGBv(0x2A, 0x46, 0xD8), true, S(9));
        int is = S(19);
        drawIcon(f.dc, ICON_LAYERS, RECT{ logo.left + (plate - is) / 2, logo.top + (plate - is) / 2,
                 logo.left + (plate + is) / 2, logo.top + (plate + is) / 2 }, RGB(255, 255, 255), (float)std::max(1, S(2)));
    }
    int tx = logo.right + S(11);
    ui::drawText(f.dc, APP_NAME, RECT{ tx, y - S(1), tx + w - S(30), y + S(18) }, f.pal.sidebarText,
                 fontUI(15, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    ui::drawText(f.dc, L"offline document toolbox", RECT{ tx, y + S(16), tx + w - S(30), y + S(32) },
                 f.pal.sidebarTextDim, fontUI(10), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y += plate + S(18);

    // ---- nav ---------------------------------------------------------
    auto drawNav = [&](const wchar_t* text, IconId icon, bool active, int id, int indent) {
        int hh = S(32);
        RECT r = { area.left + S(10), y, area.right - S(10), y + hh };
        WidgetHit hit = ui::hitTest(f, id, r);
        if (active) {
            ui::fillRect(f.dc, r, f.pal.sidebarActive, S(8));
            // left accent bar
            RECT bar = { r.left, r.top + S(8), r.left + S(3), r.bottom - S(8) };
            ui::fillRect(f.dc, bar, RGB(255, 255, 255), S(2));
        } else if (hit.hovered) {
            ui::fillRect(f.dc, r, f.pal.sidebarHover, S(8));
        }
        COLORREF fg = active ? f.pal.sidebarActiveText : (hit.hovered ? f.pal.sidebarText : f.pal.sidebarTextDim);
        int is = S(15);
        int ix = r.left + S(10) + indent;
        drawIcon(f.dc, icon, RECT{ ix, r.top + (hh - is) / 2, ix + is, r.top + (hh + is) / 2 },
                 active ? f.pal.sidebarActiveText : fg, (float)std::max(1, S(2)));
        ui::drawText(f.dc, text, RECT{ ix + is + S(11), r.top, r.right - S(8), r.bottom },
                     active ? f.pal.sidebarActiveText : fg,
                     fontUI(13, active ? FW_SEMIBOLD : FW_NORMAL), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += hh + S(2);
        return hit;
    };

    auto drawGroupLabel = [&](const wchar_t* text) {
        int hh = S(26);
        ui::drawText(f.dc, text, RECT{ area.left + S(22), y, area.right - S(16), y + hh },
                     mixColor(f.pal.sidebarTextDim, f.pal.sidebar, 0.35f), fontUI(10, FW_SEMIBOLD),
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        y += hh;
    };

    WidgetHit h;
    h = drawNav(L"All tools", ICON_LAYERS, a.view == ViewId::Home && a.categoryFilter < 0, 2001, 0);
    if (h.clicked) { a.view = ViewId::Home; a.categoryFilter = -1; a.homeScroll = 0; postRepaint(); }

    y += S(8);
    drawGroupLabel(L"CATEGORIES");

    static const struct { const wchar_t* n; IconId i; int c; } cats[] = {
        { L"Organise", ICON_ORGANIZE, (int)Category::Organise },
        { L"Optimise", ICON_COMPRESS, (int)Category::Optimise },
        { L"Convert",  ICON_TO_IMAGE, (int)Category::Convert },
        { L"Secure",   ICON_SHIELD,   (int)Category::Secure },
        { L"Edit",     ICON_WATERMARK,(int)Category::Edit },
    };
    for (size_t i = 0; i < _countof(cats); i++) {
        bool act = (a.view == ViewId::Home && a.categoryFilter == cats[i].c) ||
                   (a.view == ViewId::Tool && (int)toolDef(a.tool).category == cats[i].c);
        WidgetHit hh = drawNav(cats[i].n, cats[i].i, act && a.view == ViewId::Home, 2100 + (int)i, 0);
        if (hh.clicked) {
            a.view = ViewId::Home;
            a.categoryFilter = (a.categoryFilter == cats[i].c) ? -1 : cats[i].c;
            a.homeScroll = 0;
            postRepaint();
        }
    }

    // ---- footer ------------------------------------------------------
    int footerH = S(86);
    int fy = area.bottom - footerH;
    ui::drawDivider(f.dc, area.left + S(14), fy, area.right - S(14), fy, f.pal.sidebarSep);
    fy += S(12);

    {
        int hh = S(30);
        RECT r = { area.left + S(10), fy, area.right - S(10), fy + hh };
        WidgetHit hit = ui::hitTest(f, 2200, r);
        bool act = (a.view == ViewId::About);
        if (act) ui::fillRect(f.dc, r, f.pal.sidebarActive, S(8));
        else if (hit.hovered) ui::fillRect(f.dc, r, f.pal.sidebarHover, S(8));
        COLORREF fg = act ? f.pal.sidebarActiveText : (hit.hovered ? f.pal.sidebarText : f.pal.sidebarTextDim);
        int is = S(15);
        drawIcon(f.dc, ICON_INFO, RECT{ r.left + S(10), r.top + (hh - is) / 2, r.left + S(10) + is, r.top + (hh + is) / 2 }, fg, (float)std::max(1, S(2)));
        ui::drawText(f.dc, L"About MrDocx", RECT{ r.left + S(10) + is + S(11), r.top, r.right - S(8), r.bottom },
                     fg, fontUI(13, act ? FW_SEMIBOLD : FW_NORMAL), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (hit.clicked) { a.view = ViewId::About; postRepaint(); }
        fy += hh + S(6);
    }
    ui::drawText(f.dc, L"v" APP_VERSION L"  \u00B7  by Ragul S",
                 RECT{ area.left + S(22), fy, area.right - S(12), fy + S(20) },
                 mixColor(f.pal.sidebarTextDim, f.pal.sidebar, 0.3f), fontUI(10),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ---------------------------------------------------------------------
// home view
// ---------------------------------------------------------------------
static bool toolMatchesSearch(const ToolDef& t, const std::wstring& q)
{
    if (q.empty()) return true;
    std::wstring lq = util::lower(q);
    std::wstring hay = util::lower(t.name);
    hay += L" "; hay += util::lower(t.blurb);
    hay += L" "; hay += util::lower(t.hint);
    hay += L" "; hay += util::lower(categoryName(t.category));
    // multi-word queries: every token must appear somewhere in the haystack
    std::vector<std::wstring> toks;
    size_t i = 0;
    while (i < lq.size()) {
        while (i < lq.size() && lq[i] == L' ') i++;
        size_t j = i;
        while (j < lq.size() && lq[j] != L' ') j++;
        if (j > i) toks.push_back(lq.substr(i, j - i));
        i = j;
    }
    if (toks.size() > 1) {
        // tiny words like "to", "a" would match almost everything
        toks.erase(std::remove_if(toks.begin(), toks.end(),
                   [](const std::wstring& tk) { return tk.size() < 3; }), toks.end());
    }
    if (toks.empty()) return hay.find(lq) != std::wstring::npos;
    for (auto& tk : toks) if (hay.find(tk) == std::wstring::npos) return false;
    return true;
}

void paintHome(Frame& f, RECT area)
{
    AppState& a = app();

    int padX = S(32);
    int maxW = S(1080);
    int availW = area.right - area.left - padX * 2;
    int contentW = std::min<int>(availW, maxW);
    int cx = area.left + (area.right - area.left - contentW) / 2;

    int barW = f.scrollVisible ? S(14) : 0;
    (void)barW;

    int y = area.top - a.homeScroll + S(22);

    Cards cards;

    // ---------- hero -------------------------------------------------
    {
        RECT hero = { cx, y, cx + contentW, y + S(150) };
        if (!f.measure) {
            COLORREF c1 = a.dark ? RGBv(0x1A, 0x24, 0x44) : RGBv(0xEC, 0xF1, 0xFF);
            COLORREF c2 = a.dark ? RGBv(0x14, 0x17, 0x1E) : RGBv(0xFA, 0xFB, 0xFF);
            ui::drawShadow(f.dc, hero, S(14), S(14), a.dark ? 70 : 30);
            ui::fillGradient(f.dc, hero, c1, c2, true, S(14));
            ui::strokeRect(f.dc, hero, a.dark ? RGBv(0x2A, 0x33, 0x52) : RGBv(0xD8, 0xE2, 0xFB), S(14), 1);

            // decorative icon cluster on the right
            int is = S(26);
            COLORREF tint = a.dark ? RGBv(0x3B, 0x4C, 0x86) : RGBv(0xC3, 0xD2, 0xFB);
            IconId deco[4] = { ICON_MERGE, ICON_SPLIT, ICON_COMPRESS, ICON_LOCK };
            for (int i = 0; i < 4; i++) {
                RECT ir = { hero.right - S(44) - (i % 2) * S(40), hero.top + S(28) + (i / 2) * S(40),
                            0, 0 };
                ir.right = ir.left + is; ir.bottom = ir.top + is;
                drawIcon(f.dc, deco[i], ir, tint, 0, 200);
            }
        }
        // hero text
        int hx = hero.left + S(28);
        int hy = hero.top + S(24);
        if (!f.measure) {
            ui::drawText(f.dc, L"Every document tool you need. All offline.",
                         RECT{ hx, hy, hero.right - S(120), hy + S(34) }, f.pal.text,
                         fontUI(a.dark ? 23 : 24, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ui::drawText(f.dc,
                         L"Merge, split, compress, convert, watermark and more \u2014 processed entirely on this PC.",
                         RECT{ hx, hy + S(36), hero.right - S(60), hy + S(58) }, f.pal.textDim,
                         fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ui::drawText(f.dc, L"No uploads. No accounts. No file ever leaves your machine.",
                         RECT{ hx, hy + S(58), hero.right - S(60), hy + S(78) }, f.pal.textFaint,
                         fontUI(12), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
        y = hero.bottom + S(20);
    }

    // ---------- search ----------------------------------------------
    {
        int sw = std::min<int>(S(460), contentW);
        RECT sr = { cx, y, cx + sw, y + S(38) };
        if (!f.measure) {
            ui::fillRect(f.dc, sr, f.pal.surface, S(10));
            ui::strokeRect(f.dc, sr, f.focusId == 3001 ? f.pal.accent : f.pal.border, S(10),
                           f.focusId == 3001 ? S(2) : 1);
            int is = S(16);
            drawIcon(f.dc, ICON_SEARCH, RECT{ sr.left + S(12), sr.top + (S(38) - is) / 2,
                     sr.left + S(12) + is, sr.top + (S(38) + is) / 2 }, f.pal.textFaint);
        }
        // text field drawn manually so it sits inside the styled pill
        WidgetHit hit = ui::hitTest(f, 3001, sr);
        Frame::EditState& e = f.edits[3001];
        if (hit.focused && !f.measure) ui::editKeys(f, e, false);
        if (f.focusId != 3001 && e.text != a.search) {
            e.text = a.search;
            e.caret = util::clampv<int>(e.caret, 0, (int)e.text.size());
        } else {
            a.search = e.text;
        }
        if (hit.focused && f.enterPressed && !f.measure) {
            // jump straight into the first tool that matches
            std::wstring q = util::trim(e.text);
            for (auto& t : allTools()) {
                if (toolMatchesSearch(t, q)) { openTool(t.id); f.enterPressed = false; break; }
            }
        }
        if (!f.measure) {
            RECT tr = { sr.left + S(36), sr.top, sr.right - S(34), sr.bottom };
            if (e.text.empty())
                ui::drawText(f.dc, L"Search tools\u2026  (try \u201Ccompress\u201D or \u201Cjpg\u201D)", tr,
                             f.pal.textFaint, fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            else {
                ui::drawText(f.dc, e.text, tr, f.pal.text, fontUI(13), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                int cxp = tr.left + ui::textSize(f.dc, e.text.substr(0, util::clampv<int>(e.caret, 0, (int)e.text.size())), fontUI(13)).cx;
                HPEN pen = CreatePen(PS_SOLID, std::max(1, S(2)), f.pal.accent);
                HGDIOBJ o = SelectObject(f.dc, pen);
                MoveToEx(f.dc, cxp, sr.top + S(9), nullptr); LineTo(f.dc, cxp, sr.bottom - S(9));
                SelectObject(f.dc, o); DeleteObject(pen);
            }
            if (!e.text.empty()) {
                int is = S(13);
                RECT xr = { sr.right - S(28), sr.top + (S(38) - is) / 2 - S(3), sr.right - S(28) + is + S(6), sr.top + (S(38) + is) / 2 + S(3) };
                WidgetHit xh = ui::hitTest(f, 3002, xr);
                if (xh.hovered) ui::fillRect(f.dc, xr, f.pal.surfaceAlt, xr.bottom - xr.top > 0 ? (xr.bottom - xr.top) / 2 : 0);
                drawIcon(f.dc, ICON_CLOSE, RECT{ xr.left + S(3), xr.top + S(3), xr.right - S(3), xr.bottom - S(3) },
                         f.pal.textDim, (float)std::max(1, S(2)));
                if (xh.clicked) { a.search.clear(); e.text.clear(); e.caret = 0; }
            }
        }
        y = sr.bottom + S(22);
    }

    // ---------- tool groups ------------------------------------------
    std::wstring q = util::trim(a.search);
    int shown = 0;
    int idBase = 4000;
    Category order[5] = { Category::Organise, Category::Optimise, Category::Edit, Category::Convert, Category::Secure };

    for (int ci = 0; ci < 5; ci++) {
        if (a.categoryFilter >= 0 && a.categoryFilter != (int)order[ci]) continue;

        std::vector<const ToolDef*> list;
        for (auto& t : allTools())
            if (t.category == order[ci] && toolMatchesSearch(t, q)) list.push_back(&t);
        if (list.empty()) continue;

        // heading
        if (!f.measure) {
            RECT hr = { cx, y, cx + contentW, y + S(24) };
            ui::drawText(f.dc, categoryName(order[ci]), RECT{ hr.left, hr.top, hr.left + S(300), hr.bottom },
                         f.pal.text, fontUI(14, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            ui::drawText(f.dc, util::fmt(L"%d tool%s", (int)list.size(), list.size() == 1 ? L"" : L"s"),
                         RECT{ hr.left + S(1), hr.top, hr.right, hr.bottom }, f.pal.textFaint, fontUI(11),
                         DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        y += S(28);

        int cellW = S(246), cellH = S(92), gap = S(12);
        int rows = ((int)list.size() + std::max(1, (contentW + gap) / (cellW + gap)) - 1) /
                   std::max(1, (contentW + gap) / (cellW + gap));
        (void)rows;

        if (!f.measure) ui::beginGrid(f, cx, y, contentW, cellW, cellH, gap, gap);
        else {
            int cols = std::max(1, (contentW + gap) / (cellW + gap));
            int rr = ((int)list.size() + cols - 1) / cols;
            y += rr * cellH + (rr - 1) * gap;
        }

        for (size_t i = 0; i < list.size(); i++) {
            const ToolDef& t = *list[i];
            RECT r = f.measure ? RECT{ 0, 0, cellW, cellH } : ui::gridCell(f);
            int id = idBase + (int)t.id;
            WidgetHit hit = ui::hitTest(f, id, r);
            if (hit.clicked) openTool(t.id);

            if (!f.measure) {
                if (y + cellH >= area.top - S(200) && y <= area.bottom + S(200)) {
                    COLORREF bg = f.pal.surface;
                    COLORREF bd = f.pal.border;
                    if (hit.pressed) { bg = f.pal.surfaceAlt; bd = f.pal.borderStrong; }
                    else if (hit.hovered) { bg = f.pal.surfaceHover; bd = mixColor(f.pal.accent, f.pal.border, 0.45f); }
                    if (hit.hovered) ui::drawShadow(f.dc, r, S(12), S(12), f.pal.dark ? 70 : 34);
                    ui::fillRect(f.dc, r, bg, S(12));
                    ui::strokeRect(f.dc, r, bd, S(12), hit.hovered ? S(2) : 1);

                    int plate = S(40);
                    RECT pr = { r.left + S(16), r.top + (cellH - plate) / 2, r.left + S(16) + plate,
                                r.top + (cellH + plate) / 2 };
                    ui::iconPlate(f.dc, pr, t.icon, t.tint, f.pal.dark, S(11));

                    int tx = pr.right + S(13);
                    RECT nr = { tx, r.top + S(20), r.right - S(28), r.top + S(40) };
                    ui::drawText(f.dc, t.name, nr, f.pal.text, fontUI(14, FW_SEMIBOLD),
                                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    RECT br = { tx, r.top + S(40), r.right - S(16), r.top + S(74) };
                    HGDIOBJ of = SelectObject(f.dc, fontUI(11));
                    SetBkMode(f.dc, TRANSPARENT);
                    SetTextColor(f.dc, f.pal.textDim);
                    std::wstring blurb = t.blurb;
                    DrawTextW(f.dc, const_cast<LPWSTR>(blurb.c_str()), -1, &br,
                              DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_END_ELLIPSIS);
                    SelectObject(f.dc, of);

                    if (hit.hovered) {
                        int is = S(13);
                        drawIcon(f.dc, ICON_CHEVRON_RIGHT,
                                 RECT{ r.right - S(24), r.top + (cellH - is) / 2, r.right - S(24) + is,
                                       r.top + (cellH + is) / 2 }, f.pal.accent, (float)std::max(1, S(2)));
                    }
                }
            }
        }
        if (!f.measure) {
            int rows = ui::gridRowsUsed(f);
            ui::endGrid(f);
            y += rows * cellH + (rows - 1) * gap + S(24);
        } else {
            y += S(24);
        }
        shown += (int)list.size();
    }

    // ---------- empty search result ----------------------------------
    if (shown == 0 && !f.measure) {
        RECT r = { cx, y, cx + contentW, y + S(160) };
        ui::fillRect(f.dc, r, f.pal.surface, S(12));
        ui::strokeRect(f.dc, r, f.pal.border, S(12), 1);
        int is = S(34);
        drawIcon(f.dc, ICON_SEARCH, RECT{ (r.left + r.right - is) / 2, r.top + S(30),
                 (r.left + r.right + is) / 2, r.top + S(30) + is }, f.pal.textFaint, (float)S(2));
        ui::drawText(f.dc, util::fmt(L"No tool matches \u201C%s\u201D", a.search.c_str()),
                     RECT{ r.left, r.top + S(76), r.right, r.top + S(100) }, f.pal.text,
                     fontUI(14, FW_SEMIBOLD), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ui::drawText(f.dc, L"Try \u201Cmerge\u201D, \u201Ccompress\u201D, \u201Cjpg\u201D or \u201Cpassword\u201D.",
                     RECT{ r.left, r.top + S(100), r.right, r.top + S(124) }, f.pal.textDim,
                     fontUI(12), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        y = r.bottom + S(24);
    } else if (shown == 0) {
        y += S(180);
    }

    // ---------- footer ------------------------------------------------
    if (!f.measure) {
        RECT fr = { cx, y, cx + contentW, y + S(46) };
        ui::drawText(f.dc,
                     util::fmt(L"%d tools  \u00B7  MrDocx %s  \u00B7  built by Ragul S  \u00B7  PDF engine: MuPDF (AGPL-3.0)",
                               (int)allTools().size(), APP_VERSION),
                     fr, f.pal.textFaint, fontUI(11), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        y = fr.bottom + S(20);
    } else y += S(66);

    // ---------- scroll --------------------------------------------------
    int contentH = (y + a.homeScroll) - area.top;
    f.scrollRange = std::max<int>(0, contentH - (int)(area.bottom - area.top));
    if (f.wheelDelta) {
        int step = std::max<int>(S(48), (int)(area.bottom - area.top) / 6);
        a.homeScroll = util::clampv<int>(a.homeScroll - (f.wheelDelta / 120) * step, 0, f.scrollRange);
        f.wheelDelta = 0;
    }
    if (f.scrollRange > 0) {
        f.scrollVisible = true;
        int bw = S(12);
        RECT track = { area.right - bw - S(3), area.top + S(6), area.right - S(3), area.bottom - S(6) };
        a.homeScroll = ui::scrollbar(f, 3500, track, contentH, (int)(area.bottom - area.top), a.homeScroll);
    } else f.scrollVisible = false;
    (void)cards;
}

// ---------------------------------------------------------------------
// about view
// ---------------------------------------------------------------------
void paintAbout(Frame& f, RECT area)
{
    AppState& a = app();
    int padX = S(32);
    int maxW = S(760);
    int contentW = std::min<int>((int)(area.right - area.left - padX * 2), maxW);
    int cx = area.left + (area.right - area.left - contentW) / 2;

    // scroll: consume the wheel against last frame's measured height
    static int aboutRange = 0;
    if (f.wheelDelta) {
        int step = std::max<int>(S(48), (area.bottom - area.top) / 6);
        a.aboutScroll = util::clampv<int>(a.aboutScroll - (f.wheelDelta / 120) * step,
                                           0, aboutRange);
        f.wheelDelta = 0;
    }
    int y0 = area.top + S(28) - a.aboutScroll;
    int y = y0;

    // brand panel
    RECT hero = { cx, y, cx + contentW, y + S(132) };
    ui::fillGradient(f.dc, hero, f.pal.dark ? RGBv(0x1B, 0x25, 0x45) : RGBv(0xEE, 0xF2, 0xFF),
                     f.pal.surface, true, S(14));
    ui::strokeRect(f.dc, hero, f.pal.border, S(14), 1);
    int plate = S(64);
    RECT lr = { hero.left + S(24), hero.top + (hero.bottom - hero.top - plate) / 2,
                hero.left + S(24) + plate, 0 };
    lr.bottom = lr.top + plate;
    if (logo::available()) logo::draw(f.dc, lr);
    else ui::fillGradient(f.dc, lr, RGBv(0x3A, 0x66, 0xFF), RGBv(0x27, 0x3F, 0xC4), true, S(14));
    int tx = lr.right + S(18);
    ui::drawText(f.dc, L"MrDocx", RECT{ tx, hero.top + S(18), hero.right - S(24), hero.top + S(48) },
                 f.pal.text, fontUI(22, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    ui::drawText(f.dc, L"Version " APP_VERSION L"  \u00B7  64-bit  \u00B7  offline document toolbox for Windows",
                 RECT{ tx, hero.top + S(50), hero.right - S(24), hero.top + S(74) }, f.pal.textDim,
                 fontUI(12), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    ui::drawText(f.dc, L"Designed and built by Ragul S",
                 RECT{ tx, hero.top + S(76), hero.right - S(24), hero.top + S(100) }, f.pal.textFaint,
                 fontUI(11, FW_SEMIBOLD), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    y = hero.bottom + S(20);

    // the card chrome is painted in the draw pass from rects captured in a
    // measure pass, so the whole body is built twice, exactly like the tools
    Cards c;
    auto buildBody = [&](bool measure) {
        f.measure = measure;
        c.idx = 0;
        if (measure) c.rects.clear();
        ui::beginColumn(f, cx, y, contentW, 1 << 20, 0);

        auto card = [&](const std::wstring& title, std::function<void()> body) {
            CardScope cs = cardBegin(f, c, title, 0, L"", S(14));
            body();
            cardEnd(f, c, cs);
        };

        card(L"What this is", [&] {
            ui::label(f, L"MrDocx is a native Win32 desktop application. Every pixel is drawn with GDI \u2014 there is no browser, no webview and no internet connection involved.", 13, f.pal.textDim, FW_NORMAL, 0, true);
            ui::label(f, L"Your documents are opened, processed and written on this machine only. Nothing is uploaded anywhere, ever.", 13, f.pal.textDim, FW_NORMAL, 0, true);
        });

        card(L"About the author", [&] {
            ui::label(f, L"MrDocx is designed and built by Ragul S \u2014 RS Creations Tech. Click a link to open it in your browser.", 13, f.pal.textDim, FW_NORMAL, 0, true);
            struct Lk { const wchar_t* t; const wchar_t* u; };
            static const Lk lks[] = {
                { L"\u00BB  Website   \u2014  rscreationstech.vercel.app/about", L"https://rscreationstech.vercel.app/about" },
                { L"\u00BB  GitHub    \u2014  github.com/rs-ragul",              L"https://github.com/rs-ragul" },
                { L"\u00BB  LinkedIn  \u2014  linkedin.com/in/ragul-rs",         L"https://www.linkedin.com/in/ragul-rs/" },
            };
            for (size_t i = 0; i < _countof(lks); i++) {
                ui::beginRow(f, f.top().x, f.top().y, f.top().w, S(28), 0);
                RECT rr = ui::takeRect(f, f.top().w, S(26));
                WidgetHit h = ui::hitTest(f, 9310 + (int)i, rr);
                if (h.hovered) {
                    ui::fillRect(f.dc, rr, f.pal.surfaceAlt, S(6));
                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                }
                ui::drawText(f.dc, lks[i].t, RECT{ rr.left + S(8), rr.top, rr.right, rr.bottom },
                             h.hovered ? f.pal.accent : f.pal.text, fontUI(12, FW_SEMIBOLD),
                             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                if (h.clicked)
                    ShellExecuteW(f.hwnd, L"open", lks[i].u, nullptr, nullptr, SW_SHOWNORMAL);
                ui::endLayout(f);
            }
        });

        card(L"Keyboard shortcuts", [&] {
            struct Row { const wchar_t* k; const wchar_t* v; };
            static const Row rows[] = {
                { L"Alt + \u2190", L"Back to all tools" },
                { L"Ctrl + O", L"Add files to the current tool" },
                { L"Ctrl + R", L"Run the current tool" },
                { L"Ctrl + A", L"Select every page in the organiser" },
                { L"Delete", L"Remove the selected pages" },
                { L"R / Shift + R", L"Rotate selected pages" },
                { L"Ctrl + D", L"Switch between light and dark" },
                { L"Esc", L"Cancel the running job" },
            };
            for (auto& r : rows) {
                ui::beginRow(f, f.top().x, f.top().y, f.top().w, S(26), 0);
                RECT kr = ui::takeRect(f, S(150), S(24));
                ui::fillRect(f.dc, kr, f.pal.surfaceAlt, S(6));
                ui::strokeRect(f.dc, kr, f.pal.border, S(6), 1);
                ui::drawText(f.dc, r.k, kr, f.pal.text, fontUI(11, FW_SEMIBOLD), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                RECT vr = ui::takeRect(f, f.top().w - S(150), S(24));
                ui::drawText(f.dc, r.v, vr, f.pal.textDim, fontUI(12), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                ui::endLayout(f);
            }
        });

        card(L"Credits and licence", [&] {
            ui::label(f, L"PDF engine: MuPDF 1.26.4 by Artifex Software, used under the GNU Affero General Public License v3. The complete source of this application and of the engine is distributed with it.", 13, f.pal.textDim, FW_NORMAL, 0, true);
            ui::label(f, L"Font rendering: FreeType. Compression: zlib. JPEG codec: libjpeg. UI: hand-written Win32 + GDI/GDI+.", 13, f.pal.textDim, FW_NORMAL, 0, true);
            ui::layoutGap(f, S(4));
            ui::label(f, L"MrDocx \u00A9 2026 Ragul S. Because MuPDF is AGPL, MrDocx is distributed with its source; keep the source available if you redistribute the binary.", 12, f.pal.textFaint, FW_NORMAL, 0, true);
        });

        RECT used = ui::endLayout(f);
        f.measure = false;
        if (measure)
            aboutRange = std::max<int>(0, (int)used.bottom - y0 + S(28) - (area.bottom - area.top));
    };
    buildBody(true);
    buildBody(false);
    aboutRange = std::max(0, aboutRange);
}

} // namespace views
