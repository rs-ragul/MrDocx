// =====================================================================
//  MrDocx  -  icons.h : vector glyph icons drawn with GDI
//  Every icon is defined as stroked/filled geometry on a 24x24 unit grid
//  so it stays pixel-crisp at any DPI.  No bitmaps, no web assets.
//
//  Author : Ragul S
// =====================================================================
#pragma once
#include "common.h"

enum IconId {
    ICON_NONE = -1,
    ICON_MERGE,
    ICON_SPLIT,
    ICON_COMPRESS,
    ICON_ORGANIZE,
    ICON_ROTATE,
    ICON_REMOVE,
    ICON_EXTRACT,
    ICON_WATERMARK,
    ICON_PAGENUM,
    ICON_REPAIR,
    ICON_FLATTEN,
    ICON_LOCK,
    ICON_UNLOCK,
    ICON_TO_IMAGE,
    ICON_FROM_IMAGE,
    ICON_TO_TEXT,
    ICON_TO_HTML,
    ICON_WORD,
    ICON_INFO,
    ICON_CROP,
    ICON_REDACT,
    ICON_ADD,
    ICON_CLOSE,
    ICON_CHEVRON_DOWN,
    ICON_CHEVRON_RIGHT,
    ICON_CHEVRON_LEFT,
    ICON_BACK,
    ICON_FOLDER,
    ICON_FILE_PDF,
    ICON_FILE_IMG,
    ICON_CHECK,
    ICON_WARN,
    ICON_REFRESH,
    ICON_TRASH,
    ICON_ARROW_UP,
    ICON_ARROW_DOWN,
    ICON_DRAG,
    ICON_SEARCH,
    ICON_MOON,
    ICON_SUN,
    ICON_GEAR,
    ICON_MINIMIZE,
    ICON_MAXIMIZE,
    ICON_RESTORE,
    ICON_EXTERNAL,
    ICON_COPY,
    ICON_SAVE,
    ICON_STOP,
    ICON_SPARKLE,
    ICON_LAYERS,
    ICON_SHIELD,
    ICON_LINK,
    ICON_COUNT
};

// Geometry primitives for icon definitions -----------------------------
struct IconPoint { float x, y; };

struct IconStroke {
    IconPoint pts[10];
    int       n;
    float     width;   // in 24-unit space
    bool      closed;
    bool      roundCap;
};

struct IconFill {
    IconPoint pts[10];
    int       n;
};

struct IconCircle { float cx, cy, r; bool filled; float width; };
struct IconRect   { float x, y, w, h, r; bool filled; float width; };

struct IconDef {
    IconStroke strokes[6]; int nStrokes;
    IconFill   fills[4];   int nFills;
    IconCircle circles[3]; int nCircles;
    IconRect   rects[4];   int nRects;
};

const IconDef& iconDef(IconId id);

// Draw an icon centred inside 'box' on dc, using 'color' for the strokes.
// lineWidthPx overrides the natural stroke weight (0 = use the icon's own).
void drawIcon(HDC dc, IconId id, RECT box, COLORREF color, float lineWidthPx = 0.0f, int alpha = 255);

// Convenience: draw at an explicit origin/size
void drawIconAt(HDC dc, IconId id, int x, int y, int size, COLORREF color, float lineWidthPx = 0.0f);

// Build an HICON (for the taskbar / window icon / exe resource fallback)
HICON buildIconHICON(IconId id, int size, COLORREF fg, COLORREF bg, bool roundedBg);
