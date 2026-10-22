// =====================================================================
//  MrDocx  -  ui.h : immediate-mode widget toolkit drawn with GDI
//  Every control is owner-drawn (no HTML, no webview, no themed
//  Common Controls) so the app looks and behaves like a real Win32 app
//  while still being modern and crisp at any DPI.
//
//  Author : Ragul S
// =====================================================================
#pragma once
#include "common.h"
#include "theme.h"
#include "icons.h"

enum class BtnStyle { Primary, Secondary, Ghost, Danger, Success, Icon };
enum class TextAlign { Left, Center, Right };
enum class Dir { Column, Row };

struct WidgetHit {
    bool hovered  = false;
    bool pressed  = false;
    bool clicked  = false;   // mouse released inside
    bool rightClick = false;
    bool focused  = false;
    bool doubleClicked = false;
};

// ---------------------------------------------------------------------
// Frame: one layout + paint + input pass
// ---------------------------------------------------------------------
struct Frame {
    HDC          dc   = nullptr;
    Palette      pal;
    int          W = 0, H = 0;

    RECT         clip = {};            // current paint/hit clip
    POINT        mouse = { 0, 0 };
    bool         mouseInside = false;
    bool         leftDown = false, leftPressed = false, leftReleased = false;
    bool         rightPressed = false;
    bool         doublePressed = false;
    bool         captureLost = false;
    int          wheelDelta = 0;

    struct Stack {
        int   x = 0, y = 0;            // content cursor
        int   w = 0, h = 0;            // available box (after padding)
        int   originX = 0, originY = 0; // box origin including padding
        int   boxW = 0, boxH = 0;
        Dir   dir = Dir::Column;
        int   gap = 0;
        int   padX = 0, padY = 0;
        bool  first = true;
        int   alignCross = 0;          // 0 = stretch/leading
        int   crossSize = 0;           // measured cross size for rows
        int   mainSize  = 0;           // measured main size
        bool  wrap = false;
        int   wrapRowH = 0;
        int   contentMain = 0;         // total main-axis extent (for scrolling)
        bool  centerMain = false;
    };
    std::vector<Stack> stack;

    // grid layout state (used by beginGrid/gridCell/endGrid)
    struct Grid { int x = 0, y = 0, w = 0, cellW = 0, cellH = 0, gapX = 0, gapY = 0;
                  int col = 0, row = 0, cols = 1; };
    std::vector<Grid> grids;

    // keyboard state accumulated for this frame
    bool keyBack = false, keyDel = false, keyLeft = false, keyRight = false;
    bool keyHome = false, keyEnd = false;
    bool keyCtrlA = false, keyCtrlC = false, keyCtrlV = false, keyCtrlX = false;
    std::wstring chars;                // WM_CHAR text for this frame
    bool enterPressed = false, escPressed = false, tabPressed = false, tabShift = false;

    // persistent per-widget state (text fields, drag values)
    struct EditState { std::wstring text; int caret = 0; int selStart = -1; int selEnd = -1; };
    std::map<int, EditState> edits;

    int  scrollY = 0;                  // offset applied to the root stack
    int  scrollRange = 0;              // max scroll
    int  scrollPage = 1;
    bool scrollVisible = false;
    int  focusId = 0;
    int  hotId = 0;
    int  activeId = 0;
    int  dragId = 0;
    int  dragStartX = 0, dragStartY = 0;
    int  dragScrollBase = 0;
    std::vector<int> focusOrder;
    int  lastClickId = 0;
    DWORD lastClickTime = 0;
    int  idCounter = 0;
    int  visibleTop = 0, visibleBottom = 0;

    // caret for text fields
    bool caretVisible = false;
    RECT caretRect = {};

    HWND hwnd = nullptr;

    // measure pass: layout runs but nothing draws and input has no effect
    bool measure = false;

    int nextId() { return ++idCounter; }
    int topContentY() const { return stack.empty() ? 0 : stack.back().y; }
    Stack& top() { return stack.back(); }
    void setClip(RECT r) { clip = r; }
    bool visible(int y, int h) const { return y + h >= visibleTop && y <= visibleBottom; }
};

namespace ui {

// --- small internal helpers -----------------------------------------
COLORREF bgTransparent();               // sentinel meaning "no background"
bool     isTransparent(COLORREF c);
int      cur_width(Frame& f);           // available width of the current stack
bool     rectVisible(Frame& f, RECT r); // intersects the visible viewport
std::wstring getClipboardText(HWND hwnd);
void         setClipboardText(HWND hwnd, const std::wstring& text);

// --- frame lifecycle -------------------------------------------------
void beginFrame(Frame& f, HDC dc, HWND hwnd, int w, int h, const Palette& pal,
                const POINT& mouse, bool mouseInside, const RECT& clip);
void endFrame(Frame& f);

// --- layout ----------------------------------------------------------
void beginColumn(Frame& f, int x, int y, int w, int h, int gap = 0, int padX = 0, int padY = 0);
void beginRow(Frame& f, int x, int y, int w, int h, int gap = 0, int padX = 0, int padY = 0);
RECT endLayout(Frame& f);              // returns the box actually used
void layoutGap(Frame& f, int px);      // extra space
void layoutWrap(Frame& f, bool on);
void layoutCenterMain(Frame& f, bool on);

// Grid: fixed cell size, as many columns as fit in the given width.
void beginGrid(Frame& f, int x, int y, int w, int cellW, int cellH, int gapX, int gapY);
RECT gridCell(Frame& f);               // next cell rect in the grid
int  gridRowsUsed(Frame& f);           // rows consumed so far
void endGrid(Frame& f);                // advances the parent stack past the grid

// Advance the cursor and return a rect of the requested size.
RECT takeRect(Frame& f, int w, int h);
// Fill the remaining cross-axis space (rows) or main-axis (columns).
RECT takeFill(Frame& f, int cross = 0, int reserve = 0);

// --- drawing primitives ---------------------------------------------
void fillRect(HDC dc, RECT r, COLORREF c, int radius = 0);
void strokeRect(HDC dc, RECT r, COLORREF c, int radius = 0, int width = 1, int alpha = 255);
void roundRectPath(HDC dc, RECT r, int radius);
void drawText(HDC dc, const std::wstring& s, RECT r, COLORREF c, HFONT font,
              UINT flags = DT_LEFT | DT_VCENTER | DT_SINGLELINE, int alpha = 255);
void drawTextSized(HDC dc, const std::wstring& s, RECT r, COLORREF c, HFONT font, UINT flags);
SIZE textSize(HDC dc, const std::wstring& s, HFONT font);
void drawShadow(HDC dc, RECT r, int radius, int blur, int alpha);
void drawDivider(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int alpha = 255);
void fillGradient(HDC dc, RECT r, COLORREF from, COLORREF to, bool vertical, int radius = 0);
void fillRoundedWithBorder(HDC dc, RECT r, int radius, COLORREF fill, COLORREF border);
// Draws a small circular/rounded "plate" behind an icon.
void iconPlate(HDC dc, RECT r, IconId icon, COLORREF tint, bool dark, int radius = -1);

// --- widgets ---------------------------------------------------------
WidgetHit hitTest(Frame& f, int id, RECT r);

// Label; returns height used. align: 0 left, 1 center, 2 right
RECT label(Frame& f, const std::wstring& text, int fontSize, COLORREF color,
           int weight = FW_NORMAL, int maxW = 0, bool wrap = false, int align = 0);

RECT button(Frame& f, int id, const std::wstring& text, BtnStyle style,
            int minW = 0, int h = 0, IconId icon = ICON_NONE, bool enabled = true,
            WidgetHit* outHit = nullptr);

RECT iconButton(Frame& f, int id, IconId icon, int size, BtnStyle style = BtnStyle::Ghost,
                bool enabled = true, const wchar_t* tip = nullptr, WidgetHit* outHit = nullptr);

RECT checkbox(Frame& f, int id, const std::wstring& text, bool& value, bool enabled = true,
              WidgetHit* outHit = nullptr);

RECT radioButton(Frame& f, int id, const std::wstring& text, bool selected, bool enabled = true,
                 WidgetHit* outHit = nullptr);

RECT combo(Frame& f, int id, const std::vector<std::wstring>& items, int& selected,
           int width, bool enabled = true, WidgetHit* outHit = nullptr);

RECT textField(Frame& f, int id, std::wstring& text, int width, const std::wstring& placeholder = L"",
               bool password = false, bool enabled = true, WidgetHit* outHit = nullptr);

// Feeds the frame's keyboard input into a retained edit state.  Used by views
// that draw their own styled box around an edit (e.g. the home search pill).
void editKeys(Frame& f, Frame::EditState& e, bool password);

RECT spinner(Frame& f, int id, int& value, int minV, int maxV, int width,
             const std::wstring& suffix = L"", bool enabled = true, WidgetHit* outHit = nullptr);

RECT slider(Frame& f, int id, int& value, int minV, int maxV, int width,
            const std::wstring& label = L"", bool enabled = true, WidgetHit* outHit = nullptr);

RECT progressBar(Frame& f, int percent, int width, int height = 0);

RECT separator(Frame& f, int thickness = 1, int margin = 0);

// Section heading used inside tool panels (title + optional hint on the right)
RECT sectionTitle(Frame& f, const std::wstring& text, const std::wstring& hint = std::wstring());

// Small rounded pill/badge
RECT badge(Frame& f, const std::wstring& text, COLORREF bg, COLORREF fg);

// Scrollable list box with single selection. Returns clicked index or -1.
struct ListBoxResult { int clicked = -1; int doubleClicked = -1; bool changed = false; };
ListBoxResult listBox(Frame& f, int id, const std::vector<std::wstring>& items, int& selected,
                      int width, int height, bool enabled = true);

// Custom scrollbar for the current root region. Returns the new scroll offset.
int scrollbar(Frame& f, int id, RECT track, int contentHeight, int viewHeight, int scrollY);

// Keyboard focus helpers
void setFocus(Frame& f, int id);
void clearFocus(Frame& f);
bool hasFocus(Frame& f, int id);

// Caret blink (driven by the window's timer)
bool caretBlinkOn();
void caretBlinkTick();
void caretBlinkReset();

// Tooltip request queue (rendered by the window after the frame)
void requestTooltip(Frame& f, const std::wstring& text, RECT where);
const std::wstring& pendingTooltip();
RECT pendingTooltipRect();
void clearPendingTooltip();

// Popup menu support (owner drawn)
struct MenuItem {
    std::wstring text;
    IconId icon = ICON_NONE;
    int id = 0;
    bool separator = false;
    bool enabled = true;
    bool checked = false;
};
int popupMenu(HWND owner, POINT screenPt, const std::vector<MenuItem>& items, int minWidth = 0);

// Single-line input dialog (used for PDF passwords). Returns false on cancel.
bool inputBox(HWND owner, const std::wstring& title, const std::wstring& prompt,
              std::wstring& value, bool password);

// Message box with app styling (native buttons, custom text)
int appMessageBox(HWND owner, const std::wstring& title, const std::wstring& text,
                  const std::wstring& detail, int buttons /*0 ok,1 okcancel,2 yesno*/,
                  int iconKind /*0 none,1 info,2 warn,3 error,4 success*/);

} // namespace ui
