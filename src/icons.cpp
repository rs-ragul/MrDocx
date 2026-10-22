// =====================================================================
//  MrDocx  -  icons.cpp : vector icon geometry + GDI renderer
//  Author : Ragul S
// =====================================================================
#include "icons.h"
#include <gdiplus.h>

using namespace Gdiplus;

// ---------------------------------------------------------------------
// Geometry tables (24 x 24 unit design space)
// ---------------------------------------------------------------------
#define P(x,y) IconPoint{ x, y }

static IconStroke mkStroke(float w, bool closed, std::initializer_list<IconPoint> pts)
{
    IconStroke s{};
    s.width = w; s.closed = closed; s.roundCap = true;
    int i = 0;
    for (auto p : pts) { if (i < 10) s.pts[i++] = p; }
    s.n = i;
    return s;
}
static IconFill mkFill(std::initializer_list<IconPoint> pts)
{
    IconFill f{}; int i = 0;
    for (auto p : pts) { if (i < 10) f.pts[i++] = p; }
    f.n = i; return f;
}

static std::vector<IconDef>& defs()
{
    static std::vector<IconDef> v(ICON_COUNT);
    return v;
}

static bool gBuilt = false;

static void buildIcons()
{
    auto& D = defs();

    // --- MERGE : two arrows converging into one -----------------------
    D[ICON_MERGE] = {};
    D[ICON_MERGE].strokes[0] = mkStroke(2.0f, false, { P(3,7), P(13,12), P(3,17) });
    D[ICON_MERGE].strokes[1] = mkStroke(2.0f, false, { P(13,7.5f), P(13,16.5f) });
    D[ICON_MERGE].strokes[2] = mkStroke(2.0f, false, { P(17,7), P(17,17) });
    D[ICON_MERGE].strokes[3] = mkStroke(2.0f, false, { P(21,7), P(21,17) });
    D[ICON_MERGE].nStrokes = 4;

    // --- SPLIT : one arrow diverging into two -------------------------
    D[ICON_SPLIT] = {};
    D[ICON_SPLIT].strokes[0] = mkStroke(2.0f, false, { P(3,7), P(3,17) });
    D[ICON_SPLIT].strokes[1] = mkStroke(2.0f, false, { P(7,7), P(7,17) });
    D[ICON_SPLIT].strokes[2] = mkStroke(2.0f, false, { P(11,12), P(21,7) });
    D[ICON_SPLIT].strokes[3] = mkStroke(2.0f, false, { P(11,12), P(21,17) });
    D[ICON_SPLIT].nStrokes = 4;

    // --- COMPRESS : squeeze arrows ------------------------------------
    D[ICON_COMPRESS] = {};
    D[ICON_COMPRESS].strokes[0] = mkStroke(2.0f, false, { P(4,4), P(20,4) });
    D[ICON_COMPRESS].strokes[1] = mkStroke(2.0f, false, { P(4,20), P(20,20) });
    D[ICON_COMPRESS].strokes[2] = mkStroke(2.0f, false, { P(12,7), P(12,10) });
    D[ICON_COMPRESS].strokes[3] = mkStroke(1.8f, false, { P(10,8.2f), P(12,10.2f), P(14,8.2f) });
    D[ICON_COMPRESS].strokes[4] = mkStroke(2.0f, false, { P(12,17), P(12,14) });
    D[ICON_COMPRESS].strokes[5] = mkStroke(1.8f, false, { P(10,15.8f), P(12,13.8f), P(14,15.8f) });
    D[ICON_COMPRESS].nStrokes = 6;

    // --- ORGANIZE : 2x2 tiles -----------------------------------------
    D[ICON_ORGANIZE] = {};
    D[ICON_ORGANIZE].rects[0] = { 3.5f, 3.5f, 7.f, 7.f, 2.f, false, 1.9f };
    D[ICON_ORGANIZE].rects[1] = { 13.5f, 3.5f, 7.f, 7.f, 2.f, false, 1.9f };
    D[ICON_ORGANIZE].rects[2] = { 3.5f, 13.5f, 7.f, 7.f, 2.f, false, 1.9f };
    D[ICON_ORGANIZE].rects[3] = { 13.5f, 13.5f, 7.f, 7.f, 2.f, true, 0.f };
    D[ICON_ORGANIZE].nRects = 4;

    // --- ROTATE : circular arrow --------------------------------------
    D[ICON_ROTATE] = {};
    D[ICON_ROTATE].strokes[0] = mkStroke(2.0f, false, { P(20.2f,12.f), P(19.4f,15.2f), P(17.2f,17.7f), P(14.2f,19.1f), P(11.f,19.2f), P(8.f,18.f), P(5.8f,15.7f), P(4.7f,12.7f), P(4.9f,9.6f), P(6.3f,6.9f) });
    D[ICON_ROTATE].strokes[1] = mkStroke(2.0f, false, { P(4.2f,4.2f), P(6.6f,6.8f), P(9.4f,5.4f) });
    D[ICON_ROTATE].nStrokes = 2;

    // --- REMOVE PAGES : page with an X --------------------------------
    D[ICON_REMOVE] = {};
    D[ICON_REMOVE].strokes[0] = mkStroke(1.9f, false, { P(6,3), P(14,3), P(18,7), P(18,21), P(6,21), P(6,3) });
    D[ICON_REMOVE].strokes[1] = mkStroke(1.9f, false, { P(14,3), P(14,7), P(18,7) });
    D[ICON_REMOVE].strokes[2] = mkStroke(2.1f, false, { P(9.5f,11.5f), P(14.5f,16.5f) });
    D[ICON_REMOVE].strokes[3] = mkStroke(2.1f, false, { P(14.5f,11.5f), P(9.5f,16.5f) });
    D[ICON_REMOVE].nStrokes = 4;

    // --- EXTRACT : dashed box, page lifted out ------------------------
    D[ICON_EXTRACT] = {};
    D[ICON_EXTRACT].strokes[0] = mkStroke(1.8f, false, { P(4,10), P(4,20), P(20,20), P(20,10) });
    D[ICON_EXTRACT].strokes[1] = mkStroke(1.8f, false, { P(9,17), P(15,17), P(15,7) });
    D[ICON_EXTRACT].strokes[2] = mkStroke(1.8f, false, { P(9,7), P(9,17) });
    D[ICON_EXTRACT].strokes[3] = mkStroke(1.8f, false, { P(9,7), P(12.5f,3.5f), P(15,7) });
    D[ICON_EXTRACT].nStrokes = 4;

    // --- WATERMARK : page with a diagonal band ------------------------
    D[ICON_WATERMARK] = {};
    D[ICON_WATERMARK].strokes[0] = mkStroke(1.9f, false, { P(6,3), P(14,3), P(18,7), P(18,21), P(6,21), P(6,3) });
    D[ICON_WATERMARK].strokes[1] = mkStroke(1.9f, false, { P(14,3), P(14,7), P(18,7) });
    D[ICON_WATERMARK].strokes[2] = mkStroke(1.8f, false, { P(8.5f,16.5f), P(15.5f,9.5f) });
    D[ICON_WATERMARK].strokes[3] = mkStroke(1.8f, false, { P(11.5f,17.5f), P(16.5f,12.5f) });
    D[ICON_WATERMARK].nStrokes = 4;

    // --- PAGE NUMBERS : page with "1" and rule marks ------------------
    D[ICON_PAGENUM] = {};
    D[ICON_PAGENUM].strokes[0] = mkStroke(1.9f, false, { P(6,2.5f), P(14,2.5f), P(18,6.5f), P(18,21.5f), P(6,21.5f), P(6,2.5f) });
    D[ICON_PAGENUM].strokes[1] = mkStroke(1.9f, false, { P(14,2.5f), P(14,6.5f), P(18,6.5f) });
    D[ICON_PAGENUM].strokes[2] = mkStroke(1.7f, false, { P(9,9), P(15,9) });
    D[ICON_PAGENUM].strokes[3] = mkStroke(1.7f, false, { P(9,12), P(13,12) });
    D[ICON_PAGENUM].strokes[4] = mkStroke(1.8f, false, { P(10.5f,16.2f), P(12,15), P(12,19) });
    D[ICON_PAGENUM].strokes[5] = mkStroke(1.8f, false, { P(10.5f,19), P(13.5f,19) });
    D[ICON_PAGENUM].nStrokes = 6;

    // --- REPAIR : shield with a tick ----------------------------------
    D[ICON_REPAIR] = {};
    D[ICON_REPAIR].strokes[0] = mkStroke(1.9f, false, { P(12,3), P(19.5f,6), P(19.5f,12.5f), P(16,18), P(12,20.5f), P(8,18), P(4.5f,12.5f), P(4.5f,6), P(12,3) });
    D[ICON_REPAIR].strokes[1] = mkStroke(2.1f, false, { P(8.8f,12), P(11.2f,14.4f), P(15.4f,9.8f) });
    D[ICON_REPAIR].nStrokes = 2;

    // --- FLATTEN : page pressed onto a baseline -----------------------
    D[ICON_FLATTEN] = {};
    D[ICON_FLATTEN].strokes[0] = mkStroke(1.9f, false, { P(7,4), P(17,4), P(17,13.5f), P(7,13.5f), P(7,4) });
    D[ICON_FLATTEN].strokes[1] = mkStroke(1.7f, false, { P(9.5f,7), P(14.5f,7) });
    D[ICON_FLATTEN].strokes[2] = mkStroke(1.7f, false, { P(9.5f,10), P(13,10) });
    D[ICON_FLATTEN].strokes[3] = mkStroke(1.9f, false, { P(12,15), P(12,18) });
    D[ICON_FLATTEN].strokes[4] = mkStroke(1.9f, false, { P(10.2f,16.6f), P(12,18.4f), P(13.8f,16.6f) });
    D[ICON_FLATTEN].strokes[5] = mkStroke(2.0f, false, { P(4.5f,21), P(19.5f,21) });
    D[ICON_FLATTEN].nStrokes = 6;

    // --- LOCK : closed padlock ----------------------------------------
    D[ICON_LOCK] = {};
    D[ICON_LOCK].strokes[0] = mkStroke(2.0f, false, { P(8,10), P(8,7), P(8.5f,5), P(10,4), P(12,3.8f), P(14,4), P(15.5f,5), P(16,7), P(16,10) });
    D[ICON_LOCK].rects[0] = { 4.5f, 10.f, 15.f, 10.5f, 2.5f, false, 1.9f };
    D[ICON_LOCK].strokes[1] = mkStroke(2.0f, false, { P(12,13.8f), P(12,16.4f) });
    D[ICON_LOCK].nStrokes = 2; D[ICON_LOCK].nRects = 1;

    // --- UNLOCK : open padlock ----------------------------------------
    D[ICON_UNLOCK] = {};
    D[ICON_UNLOCK].strokes[0] = mkStroke(2.0f, false, { P(8,10), P(8,7), P(8.5f,5), P(10,4), P(12,3.8f), P(14,4), P(15.5f,5), P(16.4f,6.6f) });
    D[ICON_UNLOCK].rects[0] = { 4.5f, 10.f, 15.f, 10.5f, 2.5f, false, 1.9f };
    D[ICON_UNLOCK].strokes[1] = mkStroke(2.0f, false, { P(12,13.8f), P(12,16.4f) });
    D[ICON_UNLOCK].nStrokes = 2; D[ICON_UNLOCK].nRects = 1;

    // --- PDF -> IMAGE : page with a picture inside --------------------
    D[ICON_TO_IMAGE] = {};
    D[ICON_TO_IMAGE].strokes[0] = mkStroke(1.9f, false, { P(6,3), P(14,3), P(18,7), P(18,21), P(6,21), P(6,3) });
    D[ICON_TO_IMAGE].strokes[1] = mkStroke(1.9f, false, { P(14,3), P(14,7), P(18,7) });
    D[ICON_TO_IMAGE].strokes[2] = mkStroke(1.8f, false, { P(8.5f,17), P(11,13.5f), P(13,16), P(14.5f,14.2f), P(16,17) });
    D[ICON_TO_IMAGE].circles[0] = { 10.2f, 11.2f, 1.3f, true, 0.f };
    D[ICON_TO_IMAGE].nStrokes = 3; D[ICON_TO_IMAGE].nCircles = 1;

    // --- IMAGE -> PDF : picture with a page arrow ---------------------
    D[ICON_FROM_IMAGE] = {};
    D[ICON_FROM_IMAGE].rects[0] = { 3.f, 4.5f, 18.f, 13.f, 2.f, false, 1.9f };
    D[ICON_FROM_IMAGE].strokes[0] = mkStroke(1.8f, false, { P(5,15.5f), P(9,11), P(12,14), P(15,10.5f), P(19,15.5f) });
    D[ICON_FROM_IMAGE].circles[0] = { 8.f, 8.2f, 1.4f, true, 0.f };
    D[ICON_FROM_IMAGE].strokes[1] = mkStroke(1.9f, false, { P(12,21.5f), P(12,19) });
    D[ICON_FROM_IMAGE].nStrokes = 2; D[ICON_FROM_IMAGE].nRects = 1; D[ICON_FROM_IMAGE].nCircles = 1;

    // --- PDF -> TEXT : page with text rules ---------------------------
    // --- WORD : page with a W -----------------------------------------
    D[ICON_WORD] = {};
    D[ICON_WORD].strokes[0] = mkStroke(1.9f, false, { P(6,3), P(14,3), P(18,7), P(18,21), P(6,21), P(6,3) });
    D[ICON_WORD].strokes[1] = mkStroke(1.9f, false, { P(14,3), P(14,7), P(18,7) });
    D[ICON_WORD].strokes[2] = mkStroke(1.8f, false, { P(8.5f,11), P(10.5f,17), P(12,13), P(13.5f,17), P(15.5f,11) });
    D[ICON_WORD].nStrokes = 3;

    D[ICON_TO_TEXT] = {};
    D[ICON_TO_TEXT].strokes[0] = mkStroke(1.9f, false, { P(6,3), P(14,3), P(18,7), P(18,21), P(6,21), P(6,3) });
    D[ICON_TO_TEXT].strokes[1] = mkStroke(1.9f, false, { P(14,3), P(14,7), P(18,7) });
    D[ICON_TO_TEXT].strokes[2] = mkStroke(1.7f, false, { P(9,10.5f), P(15,10.5f) });
    D[ICON_TO_TEXT].strokes[3] = mkStroke(1.7f, false, { P(9,13.5f), P(15,13.5f) });
    D[ICON_TO_TEXT].strokes[4] = mkStroke(1.7f, false, { P(9,16.5f), P(13,16.5f) });
    D[ICON_TO_TEXT].nStrokes = 5;

    // --- HTML export ---------------------------------------------------
    D[ICON_TO_HTML] = {};
    D[ICON_TO_HTML].strokes[0] = mkStroke(2.0f, false, { P(8.5f,6.5f), P(4,12), P(8.5f,17.5f) });
    D[ICON_TO_HTML].strokes[1] = mkStroke(2.0f, false, { P(15.5f,6.5f), P(20,12), P(15.5f,17.5f) });
    D[ICON_TO_HTML].strokes[2] = mkStroke(2.0f, false, { P(13.2f,5), P(10.8f,19) });
    D[ICON_TO_HTML].nStrokes = 3;

    // --- INFO ----------------------------------------------------------
    D[ICON_INFO] = {};
    D[ICON_INFO].circles[0] = { 12.f, 12.f, 8.5f, false, 1.9f };
    D[ICON_INFO].strokes[0] = mkStroke(2.0f, false, { P(12,11), P(12,16.5f) });
    D[ICON_INFO].circles[1] = { 12.f, 8.1f, 1.15f, true, 0.f };
    D[ICON_INFO].nStrokes = 1; D[ICON_INFO].nCircles = 2;

    // --- CROP ----------------------------------------------------------
    D[ICON_CROP] = {};
    D[ICON_CROP].strokes[0] = mkStroke(2.0f, false, { P(6,3), P(6,18), P(21,18) });
    D[ICON_CROP].strokes[1] = mkStroke(2.0f, false, { P(3,6), P(18,6), P(18,21) });
    D[ICON_CROP].nStrokes = 2;

    // --- REDACT --------------------------------------------------------
    D[ICON_REDACT] = {};
    D[ICON_REDACT].strokes[0] = mkStroke(1.9f, false, { P(6,3), P(14,3), P(18,7), P(18,21), P(6,21), P(6,3) });
    D[ICON_REDACT].strokes[1] = mkStroke(1.9f, false, { P(14,3), P(14,7), P(18,7) });
    D[ICON_REDACT].rects[0] = { 8.5f, 10.f, 7.f, 2.6f, 0.6f, true, 0.f };
    D[ICON_REDACT].rects[1] = { 8.5f, 14.5f, 4.5f, 2.6f, 0.6f, true, 0.f };
    D[ICON_REDACT].nStrokes = 2; D[ICON_REDACT].nRects = 2;

    // --- ADD / CLOSE / CHEVRONS ---------------------------------------
    D[ICON_ADD] = {};
    D[ICON_ADD].strokes[0] = mkStroke(2.0f, false, { P(12,5), P(12,19) });
    D[ICON_ADD].strokes[1] = mkStroke(2.0f, false, { P(5,12), P(19,12) });
    D[ICON_ADD].nStrokes = 2;

    D[ICON_CLOSE] = {};
    D[ICON_CLOSE].strokes[0] = mkStroke(2.0f, false, { P(6,6), P(18,18) });
    D[ICON_CLOSE].strokes[1] = mkStroke(2.0f, false, { P(18,6), P(6,18) });
    D[ICON_CLOSE].nStrokes = 2;

    D[ICON_CHEVRON_DOWN] = {};
    D[ICON_CHEVRON_DOWN].strokes[0] = mkStroke(2.0f, false, { P(6,9.5f), P(12,15.5f), P(18,9.5f) });
    D[ICON_CHEVRON_DOWN].nStrokes = 1;

    D[ICON_CHEVRON_RIGHT] = {};
    D[ICON_CHEVRON_RIGHT].strokes[0] = mkStroke(2.0f, false, { P(9.5f,6), P(15.5f,12), P(9.5f,18) });
    D[ICON_CHEVRON_RIGHT].nStrokes = 1;

    D[ICON_CHEVRON_LEFT] = {};
    D[ICON_CHEVRON_LEFT].strokes[0] = mkStroke(2.0f, false, { P(14.5f,6), P(8.5f,12), P(14.5f,18) });
    D[ICON_CHEVRON_LEFT].nStrokes = 1;

    D[ICON_BACK] = {};
    D[ICON_BACK].strokes[0] = mkStroke(2.0f, false, { P(10,5.5f), P(4,12), P(10,18.5f) });
    D[ICON_BACK].strokes[1] = mkStroke(2.0f, false, { P(4.5f,12), P(20,12) });
    D[ICON_BACK].nStrokes = 2;

    // --- FOLDER --------------------------------------------------------
    D[ICON_FOLDER] = {};
    D[ICON_FOLDER].strokes[0] = mkStroke(1.9f, false, { P(3,6.5f), P(3,18), P(21,18), P(21,9), P(11.5f,9), P(9.5f,6.5f), P(3,6.5f) });
    D[ICON_FOLDER].nStrokes = 1;

    // --- FILE (generic) + PDF badge -----------------------------------
    D[ICON_FILE_PDF] = {};
    D[ICON_FILE_PDF].strokes[0] = mkStroke(1.8f, false, { P(6,2.5f), P(14,2.5f), P(18.5f,7), P(18.5f,21.5f), P(6,21.5f), P(6,2.5f) });
    D[ICON_FILE_PDF].strokes[1] = mkStroke(1.8f, false, { P(14,2.5f), P(14,7), P(18.5f,7) });
    D[ICON_FILE_PDF].strokes[2] = mkStroke(1.6f, false, { P(9,12.5f), P(15.5f,12.5f) });
    D[ICON_FILE_PDF].strokes[3] = mkStroke(1.6f, false, { P(9,15.5f), P(15.5f,15.5f) });
    D[ICON_FILE_PDF].strokes[4] = mkStroke(1.6f, false, { P(9,18.5f), P(13,18.5f) });
    D[ICON_FILE_PDF].nStrokes = 5;

    D[ICON_FILE_IMG] = {};
    D[ICON_FILE_IMG].rects[0] = { 3.f, 4.5f, 18.f, 15.f, 2.f, false, 1.9f };
    D[ICON_FILE_IMG].strokes[0] = mkStroke(1.8f, false, { P(4.5f,17), P(9,12), P(12,15), P(15.5f,10.5f), P(19.5f,16) });
    D[ICON_FILE_IMG].circles[0] = { 8.f, 9.f, 1.5f, true, 0.f };
    D[ICON_FILE_IMG].nStrokes = 1; D[ICON_FILE_IMG].nRects = 1; D[ICON_FILE_IMG].nCircles = 1;

    // --- STATUS --------------------------------------------------------
    D[ICON_CHECK] = {};
    D[ICON_CHECK].strokes[0] = mkStroke(2.2f, false, { P(5,12.5f), P(10,17.5f), P(19.5f,7) });
    D[ICON_CHECK].nStrokes = 1;

    D[ICON_WARN] = {};
    D[ICON_WARN].strokes[0] = mkStroke(1.9f, false, { P(12,3.5f), P(21.5f,20), P(2.5f,20), P(12,3.5f) });
    D[ICON_WARN].strokes[1] = mkStroke(2.0f, false, { P(12,10), P(12,15) });
    D[ICON_WARN].circles[0] = { 12.f, 17.6f, 1.1f, true, 0.f };
    D[ICON_WARN].nStrokes = 2; D[ICON_WARN].nCircles = 1;

    D[ICON_REFRESH] = {};
    D[ICON_REFRESH].strokes[0] = mkStroke(2.0f, false, { P(19.8f,10), P(19,7.5f), P(17,5.5f), P(14.4f,4.4f), P(11.5f,4.4f), P(8.8f,5.6f), P(6.8f,7.7f), P(5.8f,10.4f), P(5.9f,13.3f), P(7.1f,15.9f) });
    D[ICON_REFRESH].strokes[1] = mkStroke(2.0f, false, { P(4.5f,19.5f), P(7.3f,16.2f), P(10.4f,17.8f) });
    D[ICON_REFRESH].nStrokes = 2;

    D[ICON_TRASH] = {};
    D[ICON_TRASH].strokes[0] = mkStroke(1.9f, false, { P(4,7), P(20,7) });
    D[ICON_TRASH].strokes[1] = mkStroke(1.9f, false, { P(9,7), P(9,4.5f), P(15,4.5f), P(15,7) });
    D[ICON_TRASH].strokes[2] = mkStroke(1.9f, false, { P(6.5f,7), P(7.5f,20), P(16.5f,20), P(17.5f,7) });
    D[ICON_TRASH].strokes[3] = mkStroke(1.7f, false, { P(10.5f,10.5f), P(10.8f,16.5f) });
    D[ICON_TRASH].strokes[4] = mkStroke(1.7f, false, { P(13.5f,10.5f), P(13.2f,16.5f) });
    D[ICON_TRASH].nStrokes = 5;

    D[ICON_ARROW_UP] = {};
    D[ICON_ARROW_UP].strokes[0] = mkStroke(2.0f, false, { P(12,19), P(12,5) });
    D[ICON_ARROW_UP].strokes[1] = mkStroke(2.0f, false, { P(6.5f,10.5f), P(12,5), P(17.5f,10.5f) });
    D[ICON_ARROW_UP].nStrokes = 2;

    D[ICON_ARROW_DOWN] = {};
    D[ICON_ARROW_DOWN].strokes[0] = mkStroke(2.0f, false, { P(12,5), P(12,19) });
    D[ICON_ARROW_DOWN].strokes[1] = mkStroke(2.0f, false, { P(6.5f,13.5f), P(12,19), P(17.5f,13.5f) });
    D[ICON_ARROW_DOWN].nStrokes = 2;

    D[ICON_DRAG] = {};
    D[ICON_DRAG].circles[0] = { 9.f, 6.f, 1.5f, true, 0.f };
    D[ICON_DRAG].circles[1] = { 15.f, 6.f, 1.5f, true, 0.f };
    D[ICON_DRAG].circles[2] = { 9.f, 12.f, 1.5f, true, 0.f };
    D[ICON_DRAG].nRects = 0; D[ICON_DRAG].nCircles = 3;
    // extra dots via rects (circle array is small)
    D[ICON_DRAG].rects[0] = { 13.5f, 10.5f, 3.f, 3.f, 1.5f, true, 0.f };
    D[ICON_DRAG].rects[1] = { 7.5f, 16.5f, 3.f, 3.f, 1.5f, true, 0.f };
    D[ICON_DRAG].rects[2] = { 13.5f, 16.5f, 3.f, 3.f, 1.5f, true, 0.f };
    D[ICON_DRAG].nRects = 3;

    D[ICON_SEARCH] = {};
    D[ICON_SEARCH].circles[0] = { 11.f, 11.f, 6.5f, false, 2.0f };
    D[ICON_SEARCH].strokes[0] = mkStroke(2.0f, false, { P(15.8f,15.8f), P(20.5f,20.5f) });
    D[ICON_SEARCH].nStrokes = 1; D[ICON_SEARCH].nCircles = 1;

    D[ICON_MOON] = {};
    D[ICON_MOON].strokes[0] = mkStroke(1.9f, false, { P(16.5f,3.5f), P(13,3), P(9.5f,4), P(6.8f,6.4f), P(5.2f,9.7f), P(5,13.3f), P(6.2f,16.7f), P(8.7f,19.2f), P(12,20.5f), P(15.5f,20.3f), P(18.6f,18.7f) });
    D[ICON_MOON].strokes[1] = mkStroke(1.9f, false, { P(16.5f,3.5f), P(14,7), P(13.5f,11), P(15,14.8f), P(18.6f,18.7f) });
    D[ICON_MOON].nStrokes = 2;

    D[ICON_SUN] = {};
    D[ICON_SUN].circles[0] = { 12.f, 12.f, 4.2f, false, 1.9f };
    D[ICON_SUN].strokes[0] = mkStroke(1.9f, false, { P(12,2.5f), P(12,5) });
    D[ICON_SUN].strokes[1] = mkStroke(1.9f, false, { P(12,19), P(12,21.5f) });
    D[ICON_SUN].strokes[2] = mkStroke(1.9f, false, { P(2.5f,12), P(5,12) });
    D[ICON_SUN].strokes[3] = mkStroke(1.9f, false, { P(19,12), P(21.5f,12) });
    D[ICON_SUN].strokes[4] = mkStroke(1.9f, false, { P(5.3f,5.3f), P(7,7) });
    D[ICON_SUN].strokes[5] = mkStroke(1.9f, false, { P(17,17), P(18.7f,18.7f) });
    D[ICON_SUN].nStrokes = 6; D[ICON_SUN].nCircles = 1;

    // "sliders" glyph used for Settings
    D[ICON_GEAR] = {};
    D[ICON_GEAR].strokes[0] = mkStroke(1.9f, false, { P(4,8), P(20,8) });
    D[ICON_GEAR].strokes[1] = mkStroke(1.9f, false, { P(4,16), P(20,16) });
    D[ICON_GEAR].circles[0] = { 9.f, 8.f, 2.4f, true, 0.f };
    D[ICON_GEAR].circles[1] = { 15.f, 16.f, 2.4f, true, 0.f };
    D[ICON_GEAR].nStrokes = 2; D[ICON_GEAR].nCircles = 2;

    D[ICON_MINIMIZE] = {};
    D[ICON_MINIMIZE].strokes[0] = mkStroke(1.6f, false, { P(6,12), P(18,12) });
    D[ICON_MINIMIZE].nStrokes = 1;

    D[ICON_MAXIMIZE] = {};
    D[ICON_MAXIMIZE].rects[0] = { 6.f, 6.f, 12.f, 12.f, 1.5f, false, 1.6f };
    D[ICON_MAXIMIZE].nRects = 1;

    D[ICON_RESTORE] = {};
    D[ICON_RESTORE].rects[0] = { 8.f, 5.f, 11.f, 11.f, 1.5f, false, 1.6f };
    D[ICON_RESTORE].strokes[0] = mkStroke(1.6f, false, { P(16,19), P(5,19), P(5,8) });
    D[ICON_RESTORE].nRects = 1; D[ICON_RESTORE].nStrokes = 1;

    D[ICON_EXTERNAL] = {};
    D[ICON_EXTERNAL].strokes[0] = mkStroke(1.9f, false, { P(18,13.5f), P(18,19), P(5,19), P(5,6), P(10.5f,6) });
    D[ICON_EXTERNAL].strokes[1] = mkStroke(1.9f, false, { P(13,5), P(19,5), P(19,11) });
    D[ICON_EXTERNAL].strokes[2] = mkStroke(1.9f, false, { P(19,5), P(11,13) });
    D[ICON_EXTERNAL].nStrokes = 3;

    D[ICON_COPY] = {};
    D[ICON_COPY].rects[0] = { 8.5f, 8.5f, 11.f, 11.f, 2.f, false, 1.8f };
    D[ICON_COPY].strokes[0] = mkStroke(1.8f, false, { P(15.5f,5.5f), P(6,5.5f), P(6,15) });
    D[ICON_COPY].nRects = 1; D[ICON_COPY].nStrokes = 1;

    D[ICON_SAVE] = {};
    D[ICON_SAVE].strokes[0] = mkStroke(1.9f, false, { P(5,4), P(16,4), P(20,8), P(20,20), P(5,20), P(5,4) });
    D[ICON_SAVE].strokes[1] = mkStroke(1.7f, false, { P(9,4), P(9,9), P(15,9), P(15,4) });
    D[ICON_SAVE].rects[0] = { 8.5f, 13.f, 8.f, 7.f, 1.f, false, 1.7f };
    D[ICON_SAVE].nStrokes = 2; D[ICON_SAVE].nRects = 1;

    D[ICON_STOP] = {};
    D[ICON_STOP].rects[0] = { 7.f, 7.f, 10.f, 10.f, 2.f, true, 0.f };
    D[ICON_STOP].nRects = 1;

    D[ICON_SPARKLE] = {};
    D[ICON_SPARKLE].fills[0] = mkFill({ P(12,3), P(13.6f,9.2f), P(20,11), P(13.6f,12.8f), P(12,19), P(10.4f,12.8f), P(4,11), P(10.4f,9.2f) });
    D[ICON_SPARKLE].fills[1] = mkFill({ P(19,16), P(19.8f,18.4f), P(22,19.2f), P(19.8f,20), P(19,22.2f), P(18.2f,20), P(16,19.2f), P(18.2f,18.4f) });
    D[ICON_SPARKLE].nFills = 2;

    D[ICON_LAYERS] = {};
    D[ICON_LAYERS].strokes[0] = mkStroke(1.9f, false, { P(12,3), P(21,8), P(12,13), P(3,8), P(12,3) });
    D[ICON_LAYERS].strokes[1] = mkStroke(1.9f, false, { P(3,12), P(12,17), P(21,12) });
    D[ICON_LAYERS].strokes[2] = mkStroke(1.9f, false, { P(3,16), P(12,21), P(21,16) });
    D[ICON_LAYERS].nStrokes = 3;

    D[ICON_SHIELD] = {};
    D[ICON_SHIELD].strokes[0] = mkStroke(1.9f, false, { P(12,3), P(19.5f,6), P(19.5f,12.5f), P(16,18), P(12,20.5f), P(8,18), P(4.5f,12.5f), P(4.5f,6), P(12,3) });
    D[ICON_SHIELD].nStrokes = 1;

    D[ICON_LINK] = {};
    D[ICON_LINK].strokes[0] = mkStroke(2.0f, false, { P(10,13.5f), P(8,15.5f), P(6,16.5f), P(4.5f,15.5f), P(4,13.5f), P(5,11.5f), P(7.5f,9) });
    D[ICON_LINK].strokes[1] = mkStroke(2.0f, false, { P(14,10.5f), P(16,8.5f), P(18,7.5f), P(19.5f,8.5f), P(20,10.5f), P(19,12.5f), P(16.5f,15) });
    D[ICON_LINK].strokes[2] = mkStroke(2.0f, false, { P(9.5f,14.5f), P(14.5f,9.5f) });
    D[ICON_LINK].nStrokes = 3;

    gBuilt = true;
}

const IconDef& iconDef(IconId id)
{
    if (!gBuilt) buildIcons();
    if (id < 0 || id >= ICON_COUNT) { static IconDef empty{}; return empty; }
    return defs()[id];
}

// ---------------------------------------------------------------------
// GDI+ backed renderer (anti-aliased strokes and joins)
// ---------------------------------------------------------------------
namespace {
GdiplusStartupOutput g_gdipOut;
ULONG_PTR            g_gdipToken = 0;

void ensureGdiPlus()
{
    if (g_gdipToken) return;
    GdiplusStartupInput in;
    in.DebugEventCallback = nullptr;
    in.SuppressBackgroundThread = FALSE;
    in.SuppressExternalCodecs = TRUE;
    GdiplusStartup(&g_gdipToken, &in, &g_gdipOut);
}

inline PointF ptOf(const IconPoint& p, float k, float ox, float oy)
{
    return PointF(ox + p.x * k, oy + p.y * k);
}
} // namespace

void drawIcon(HDC dc, IconId id, RECT box, COLORREF color, float lineWidthPx, int alpha)
{
    if (id == ICON_NONE) return;
    ensureGdiPlus();
    const IconDef& d = iconDef(id);

    int w = box.right - box.left, h = box.bottom - box.top;
    if (w <= 0 || h <= 0) return;
    float k = (float)std::min(w, h) / 24.0f;
    float ox = box.left + (w - 24.0f * k) * 0.5f;
    float oy = box.top + (h - 24.0f * k) * 0.5f;

    Graphics g(dc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);

    Color col((BYTE)alpha, GetRValue(color), GetGValue(color), GetBValue(color));

    // filled polygons
    for (int i = 0; i < d.nFills; i++) {
        const IconFill& f = d.fills[i];
        if (f.n < 3) continue;
        PointF pts[10];
        for (int j = 0; j < f.n; j++) pts[j] = ptOf(f.pts[j], k, ox, oy);
        SolidBrush b(col);
        g.FillPolygon(&b, pts, f.n, FillModeAlternate);
    }

    // filled / stroked rects (rounded)
    for (int i = 0; i < d.nRects; i++) {
        const IconRect& r = d.rects[i];
        float x = ox + r.x * k, y = oy + r.y * k, rw = r.w * k, rh = r.h * k, rr = r.r * k * 2.0f;
        if (rr > rw) rr = rw; if (rr > rh) rr = rh;
        GraphicsPath path;
        if (rr > 0.5f) {
            float d2 = rr;
            path.AddArc(x, y, d2, d2, 180.0f, 90.0f);
            path.AddArc(x + rw - d2, y, d2, d2, 270.0f, 90.0f);
            path.AddArc(x + rw - d2, y + rh - d2, d2, d2, 0.0f, 90.0f);
            path.AddArc(x, y + rh - d2, d2, d2, 90.0f, 90.0f);
            path.CloseFigure();
        } else {
            path.AddRectangle(RectF(x, y, rw, rh));
        }
        if (r.filled) {
            SolidBrush b(col);
            g.FillPath(&b, &path);
        } else {
            Pen pen(col, r.width * k);
            pen.SetLineJoin(LineJoinRound);
            pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
            g.DrawPath(&pen, &path);
        }
    }

    // circles
    for (int i = 0; i < d.nCircles; i++) {
        const IconCircle& c = d.circles[i];
        float cx = ox + c.cx * k, cy = oy + c.cy * k, rr = c.r * k;
        if (c.filled) {
            SolidBrush b(col);
            g.FillEllipse(&b, cx - rr, cy - rr, rr * 2, rr * 2);
        } else {
            Pen pen(col, c.width * k);
            g.DrawEllipse(&pen, cx - rr, cy - rr, rr * 2, rr * 2);
        }
    }

    // strokes
    for (int i = 0; i < d.nStrokes; i++) {
        const IconStroke& s = d.strokes[i];
        if (s.n < 2) continue;
        PointF pts[10];
        for (int j = 0; j < s.n; j++) pts[j] = ptOf(s.pts[j], k, ox, oy);
        float wpx = lineWidthPx > 0.01f ? lineWidthPx : s.width * k;
        Pen pen(col, wpx);
        pen.SetLineJoin(LineJoinRound);
        if (s.roundCap) pen.SetLineCap(LineCapRound, LineCapRound, DashCapRound);
        if (s.closed) {
            g.DrawPolygon(&pen, pts, s.n);
        } else {
            g.DrawLines(&pen, pts, s.n);
        }
    }
}

void drawIconAt(HDC dc, IconId id, int x, int y, int size, COLORREF color, float lineWidthPx)
{
    RECT r = { x, y, x + size, y + size };
    drawIcon(dc, id, r, color, lineWidthPx);
}

// ---------------------------------------------------------------------
// HICON builder (used for the window/taskbar icon at runtime)
// ---------------------------------------------------------------------
HICON buildIconHICON(IconId id, int size, COLORREF fg, COLORREF bg, bool roundedBg)
{
    ensureGdiPlus();
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = size;
    bmi.bmiHeader.biHeight = -size;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ old = SelectObject(mem, bmp);

    {
        Graphics g(mem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.Clear(Color(0, 0, 0, 0));
        if (bg != CLR_INVALID) {
            SolidBrush b(Color(255, GetRValue(bg), GetGValue(bg), GetBValue(bg)));
            if (roundedBg) {
                float rr = size * 0.24f;
                GraphicsPath p;
                p.AddArc(0.0f, 0.0f, rr * 2, rr * 2, 180.0f, 90.0f);
                p.AddArc((float)size - rr * 2, 0.0f, rr * 2, rr * 2, 270.0f, 90.0f);
                p.AddArc((float)size - rr * 2, (float)size - rr * 2, rr * 2, rr * 2, 0.0f, 90.0f);
                p.AddArc(0.0f, (float)size - rr * 2, rr * 2, rr * 2, 90.0f, 90.0f);
                p.CloseFigure();
                g.FillPath(&b, &p);
            } else {
                g.FillRectangle(&b, 0, 0, size, size);
            }
        }
    }

    int pad = (int)(size * 0.18f);
    RECT r = { pad, pad, size - pad, size - pad };
    drawIcon(mem, id, r, fg);

    SelectObject(mem, old);

    // AND mask (all zero -> alpha channel decides)
    HDC andDC = CreateCompatibleDC(screen);
    HBITMAP andBmp = CreateBitmap(size, size, 1, 1, nullptr);
    HGDIOBJ oldAnd = SelectObject(andDC, andBmp);
    PatBlt(andDC, 0, 0, size, size, WHITENESS);
    SelectObject(andDC, oldAnd);

    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.xHotspot = size / 2;
    ii.yHotspot = size / 2;
    ii.hbmMask = andBmp;
    ii.hbmColor = bmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(andBmp);
    DeleteObject(bmp);
    DeleteDC(andDC);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return icon;
}

// =====================================================================
//  embedded PNG logo
// =====================================================================
namespace logo {

static GpImage*  g_img = nullptr;
static IStream*  g_stream = nullptr;   // must outlive the image
static bool      g_tried = false;

static GpImage* get()
{
    if (g_tried) return g_img;
    g_tried = true;
    HRSRC rs = FindResourceW(nullptr, L"LOGO_PNG", RT_RCDATA);
    if (!rs) return nullptr;
    DWORD n = SizeofResource(nullptr, rs);
    HGLOBAL res = LoadResource(nullptr, rs);
    if (!res || !n) return nullptr;
    const void* p = LockResource(res);
    if (!p) return nullptr;
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!mem) return nullptr;
    void* m = GlobalLock(mem);
    memcpy(m, p, n);
    GlobalUnlock(mem);
    if (CreateStreamOnHGlobal(mem, TRUE, &g_stream) != S_OK) { GlobalFree(mem); g_stream = nullptr; return nullptr; }
    GpImage* img = nullptr;
    if (Gdiplus::DllExports::GdipLoadImageFromStream(g_stream, &img) != Gdiplus::Ok || !img) {
        if (img) Gdiplus::DllExports::GdipDisposeImage(img);
        g_stream->Release(); g_stream = nullptr;
        return nullptr;
    }
    g_img = img;
    return g_img;
}

bool available() { return get() != nullptr; }

void draw(HDC dc, RECT r)
{
    GpImage* img = get();
    if (!img) return;
    GpGraphics* g = nullptr;
    if (Gdiplus::DllExports::GdipCreateFromHDC(dc, &g) != Gdiplus::Ok) return;
    Gdiplus::DllExports::GdipSetInterpolationMode(g, Gdiplus::InterpolationModeHighQualityBicubic);
    Gdiplus::DllExports::GdipSetSmoothingMode(g, Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::DllExports::GdipSetPixelOffsetMode(g, Gdiplus::PixelOffsetModeHalf);
    Gdiplus::DllExports::GdipDrawImageRectI(g, img, r.left, r.top, r.right - r.left, r.bottom - r.top);
    Gdiplus::DllExports::GdipDeleteGraphics(g);
}

} // namespace logo
