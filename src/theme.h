// =====================================================================
//  PDFForge  -  theme.h : colour palettes (light & dark) and fonts
//  Author : Ragul S
// =====================================================================
#pragma once
#include "common.h"

inline COLORREF RGBv(int r, int g, int b) { return RGB(r, g, b); }

struct Palette {
    // window chrome
    COLORREF titleBar;        // custom title bar background
    COLORREF titleText;
    COLORREF titleBtnHover;
    COLORREF titleBtnDown;
    COLORREF titleBtnClose;

    COLORREF sidebar;         // navigation rail
    COLORREF sidebarText;
    COLORREF sidebarTextDim;
    COLORREF sidebarHover;
    COLORREF sidebarActive;
    COLORREF sidebarActiveText;
    COLORREF sidebarSep;

    COLORREF background;      // content area
    COLORREF surface;         // cards
    COLORREF surfaceAlt;      // subtle insets
    COLORREF surfaceHover;
    COLORREF border;
    COLORREF borderStrong;

    COLORREF text;
    COLORREF textDim;
    COLORREF textFaint;

    COLORREF accent;
    COLORREF accentHover;
    COLORREF accentDown;
    COLORREF accentSoft;      // tinted background
    COLORREF accentText;      // text on accent

    COLORREF success;
    COLORREF successSoft;
    COLORREF warning;
    COLORREF warningSoft;
    COLORREF danger;
    COLORREF dangerSoft;

    COLORREF dropZone;
    COLORREF dropZoneActive;
    COLORREF dropText;

    COLORREF scrollThumb;
    COLORREF scrollThumbHover;
    COLORREF scrollTrack;

    COLORREF thumbPage;
    COLORREF thumbPageEdge;
    COLORREF thumbSelected;

    bool dark = false;
};

inline Palette lightPalette()
{
    Palette p;
    p.titleBar        = RGBv(0xFF, 0xFF, 0xFF);
    p.titleText       = RGBv(0x1A, 0x1A, 0x2E);
    p.titleBtnHover   = RGBv(0xEC, 0xEE, 0xF3);
    p.titleBtnDown    = RGBv(0xDD, 0xE1, 0xE8);
    p.titleBtnClose   = RGBv(0xC4, 0x2B, 0x1C);

    p.sidebar         = RGBv(0x14, 0x18, 0x24);
    p.sidebarText     = RGBv(0xE6, 0xE9, 0xF0);
    p.sidebarTextDim  = RGBv(0x8A, 0x93, 0xA6);
    p.sidebarHover    = RGBv(0x21, 0x28, 0x3A);
    p.sidebarActive   = RGBv(0x2E, 0x5B, 0xFF);
    p.sidebarActiveText = RGBv(0xFF, 0xFF, 0xFF);
    p.sidebarSep      = RGBv(0x25, 0x2C, 0x3E);

    p.background      = RGBv(0xF4, 0xF6, 0xFA);
    p.surface         = RGBv(0xFF, 0xFF, 0xFF);
    p.surfaceAlt      = RGBv(0xF0, 0xF3, 0xF8);
    p.surfaceHover    = RGBv(0xF7, 0xF9, 0xFC);
    p.border          = RGBv(0xE1, 0xE6, 0xEE);
    p.borderStrong    = RGBv(0xC6, 0xCE, 0xDC);

    p.text            = RGBv(0x14, 0x18, 0x24);
    p.textDim         = RGBv(0x5A, 0x64, 0x78);
    p.textFaint       = RGBv(0x8B, 0x94, 0xA6);

    p.accent          = RGBv(0x2E, 0x5B, 0xFF);
    p.accentHover     = RGBv(0x1F, 0x47, 0xE0);
    p.accentDown      = RGBv(0x18, 0x39, 0xBE);
    p.accentSoft      = RGBv(0xE8, 0xEE, 0xFF);
    p.accentText      = RGBv(0xFF, 0xFF, 0xFF);

    p.success         = RGBv(0x14, 0x8A, 0x4C);
    p.successSoft     = RGBv(0xE2, 0xF6, 0xEA);
    p.warning         = RGBv(0xB4, 0x6A, 0x00);
    p.warningSoft     = RGBv(0xFD, 0xF2, 0xDF);
    p.danger          = RGBv(0xC4, 0x2B, 0x1C);
    p.dangerSoft      = RGBv(0xFC, 0xE9, 0xE7);

    p.dropZone        = RGBv(0xF7, 0xF9, 0xFD);
    p.dropZoneActive  = RGBv(0xE8, 0xEE, 0xFF);
    p.dropText        = RGBv(0x6B, 0x76, 0x8C);

    p.scrollThumb     = RGBv(0xC3, 0xCA, 0xD8);
    p.scrollThumbHover= RGBv(0xA6, 0xAF, 0xC2);
    p.scrollTrack     = RGBv(0x00, 0x00, 0x00);

    p.thumbPage       = RGBv(0xFF, 0xFF, 0xFF);
    p.thumbPageEdge   = RGBv(0xD3, 0xDA, 0xE6);
    p.thumbSelected   = RGBv(0x2E, 0x5B, 0xFF);
    p.dark = false;
    return p;
}

inline Palette darkPalette()
{
    Palette p;
    p.titleBar        = RGBv(0x14, 0x17, 0x1E);
    p.titleText       = RGBv(0xE8, 0xEB, 0xF2);
    p.titleBtnHover   = RGBv(0x25, 0x2A, 0x35);
    p.titleBtnDown    = RGBv(0x31, 0x37, 0x44);
    p.titleBtnClose   = RGBv(0xC4, 0x2B, 0x1C);

    p.sidebar         = RGBv(0x0D, 0x10, 0x16);
    p.sidebarText     = RGBv(0xDF, 0xE3, 0xEC);
    p.sidebarTextDim  = RGBv(0x77, 0x80, 0x93);
    p.sidebarHover    = RGBv(0x1A, 0x1F, 0x29);
    p.sidebarActive   = RGBv(0x3D, 0x68, 0xFF);
    p.sidebarActiveText = RGBv(0xFF, 0xFF, 0xFF);
    p.sidebarSep      = RGBv(0x1C, 0x21, 0x2B);

    p.background      = RGBv(0x14, 0x17, 0x1E);
    p.surface         = RGBv(0x1B, 0x1F, 0x28);
    p.surfaceAlt      = RGBv(0x22, 0x27, 0x32);
    p.surfaceHover    = RGBv(0x23, 0x29, 0x35);
    p.border          = RGBv(0x2A, 0x30, 0x3C);
    p.borderStrong    = RGBv(0x3B, 0x43, 0x52);

    p.text            = RGBv(0xEC, 0xEF, 0xF6);
    p.textDim         = RGBv(0xA2, 0xAB, 0xBD);
    p.textFaint       = RGBv(0x73, 0x7C, 0x8F);

    p.accent          = RGBv(0x4C, 0x76, 0xFF);
    p.accentHover     = RGBv(0x64, 0x89, 0xFF);
    p.accentDown      = RGBv(0x38, 0x5E, 0xE8);
    p.accentSoft      = RGBv(0x1E, 0x2B, 0x52);
    p.accentText      = RGBv(0xFF, 0xFF, 0xFF);

    p.success         = RGBv(0x3E, 0xC4, 0x7C);
    p.successSoft     = RGBv(0x14, 0x2E, 0x21);
    p.warning         = RGBv(0xE8, 0xA8, 0x3C);
    p.warningSoft     = RGBv(0x33, 0x28, 0x13);
    p.danger          = RGBv(0xF0, 0x6A, 0x5A);
    p.dangerSoft      = RGBv(0x35, 0x1A, 0x1A);

    p.dropZone        = RGBv(0x18, 0x1C, 0x25);
    p.dropZoneActive  = RGBv(0x1E, 0x2B, 0x52);
    p.dropText        = RGBv(0x8A, 0x93, 0xA6);

    p.scrollThumb     = RGBv(0x39, 0x40, 0x4F);
    p.scrollThumbHover= RGBv(0x4A, 0x53, 0x65);
    p.scrollTrack     = RGBv(0x00, 0x00, 0x00);

    p.thumbPage       = RGBv(0xF6, 0xF7, 0xFA);
    p.thumbPageEdge   = RGBv(0x3A, 0x41, 0x50);
    p.thumbSelected   = RGBv(0x4C, 0x76, 0xFF);
    p.dark = true;
    return p;
}

// Blend two colours (used for hover tints and shadows)
inline COLORREF mixColor(COLORREF a, COLORREF b, float t)
{
    int ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
    int br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
    return RGB((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t));
}

// ---------------------------------------------------------------------
// Font cache: Segoe UI family with graceful fallback
// ---------------------------------------------------------------------
class FontCache {
public:
    static FontCache& get() { static FontCache f; return f; }

    void init()
    {
        if (ready_) return;
        // Prefer the modern Segoe UI Variable, fall back to Segoe UI.
        const wchar_t* cands[] = { L"Segoe UI Variable Text", L"Segoe UI Variable", L"Segoe UI", L"Tahoma" };
        for (const wchar_t* fam : cands) {
            if (fontInstalled(fam)) { family_ = fam; break; }
        }
        ready_ = true;
    }

    const std::wstring& family() { init(); return family_; }

    // key: size(hundredths) | weight | italic
    HFONT get(int pxHeight, int weight = FW_NORMAL, bool italic = false)
    {
        init();
        long long key = ((long long)pxHeight << 24) | ((long long)weight << 1) | (italic ? 1 : 0);
        auto it = cache_.find(key);
        if (it != cache_.end()) return it->second;

        LOGFONTW lf = {};
        lf.lfHeight = -pxHeight;
        lf.lfWeight = weight;
        lf.lfItalic = italic ? 1 : 0;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_TT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = CLEARTYPE_NATURAL_QUALITY;
        lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
        wcsncpy_s(lf.lfFaceName, LF_FACESIZE, family_.c_str(), _TRUNCATE);
        HFONT f = CreateFontIndirectW(&lf);
        cache_[key] = f;
        return f;
    }

    void destroy()
    {
        for (auto& kv : cache_) if (kv.second) DeleteObject(kv.second);
        cache_.clear();
    }

private:
    static bool fontInstalled(const std::wstring& fam)
    {
        struct Ctx { std::wstring want; bool found = false; } ctx{ fam, false };
        HDC dc = GetDC(nullptr);
        LOGFONTW lf = {}; lf.lfCharSet = DEFAULT_CHARSET;
        auto cb = [](const LOGFONTW* l, const TEXTMETRICW*, DWORD, LPARAM p) -> int {
            Ctx* c = (Ctx*)p;
            std::wstring n = l->lfFaceName;
            if (n == c->want) { c->found = true; return 0; }
            return 1;
        };
        EnumFontFamiliesExW(dc, &lf, cb, (LPARAM)&ctx, 0);
        ReleaseDC(nullptr, dc);
        return ctx.found;
    }

    bool ready_ = false;
    std::wstring family_ = L"Segoe UI";
    std::map<long long, HFONT> cache_;
};

// ---------------------------------------------------------------------
// Active theme accessor (implemented in app.cpp)
// ---------------------------------------------------------------------
Palette& currentPalette();
bool     isDarkTheme();
void     setDarkTheme(bool dark);

// semantic font accessors (px heights already DPI-scaled)
inline HFONT fontUI(int px, int weight = FW_NORMAL, bool italic = false)
{ return FontCache::get().get(S(px), weight, italic); }

inline HFONT fontUIPt(int pt, int weight = FW_NORMAL, bool italic = false)
{ return FontCache::get().get(Dpi::get().sp(pt), weight, italic); }
