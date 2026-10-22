// =====================================================================
//  MrDocx  -  app.h : application state, tool catalogue, job runner
//  Author : Ragul S
// =====================================================================
#pragma once
#include "common.h"
#include "theme.h"
#include "icons.h"
#include "ui.h"
#include "pdfengine.h"
#include <set>
#include <thread>
#include <deque>
#include <condition_variable>

// ---------------------------------------------------------------------
// Tools
// ---------------------------------------------------------------------
enum class ToolId {
    Merge = 0, Split, Compress, Organise, Rotate, RemovePages, ExtractPages,
    Watermark, PageNumbers, Protect, Unlock, Repair, Flatten,
    PdfToImage, ImageToPdf, PdfToText, PdfToHtml, PdfToWord, WordToPdf,
    ImageConvert, TextToPdf, PdfToMarkdown, Metadata,
    COUNT
};

enum class InputKind {
    MultiPdf,      // one or more PDFs, order matters
    SinglePdf,     // exactly one PDF
    OneOrMorePdf,  // one or more PDFs, each processed separately
    Images,        // image files
    PdfSingleOrMulti,
    SingleDocx,    // one Word .docx document
    SingleText     // one .txt / .md document
};

enum class Category { Organise, Optimise, Convert, Secure, Edit };

struct ToolDef {
    ToolId      id;
    const wchar_t* name;
    const wchar_t* blurb;
    const wchar_t* action;      // primary button caption
    Category    category;
    IconId      icon;
    COLORREF    tint;           // icon plate colour
    InputKind   input;
    bool        usesOrganiser;  // needs the page thumbnail grid
    bool        multiOut;       // produces several files
    const wchar_t* hint;        // short help line under the title
};

const ToolDef& toolDef(ToolId id);
const std::vector<ToolDef>& allTools();
const wchar_t* categoryName(Category c);

// ---------------------------------------------------------------------
// Staged input file
// ---------------------------------------------------------------------
struct StagedFile {
    std::wstring path;
    std::wstring password;
    pdf::DocInfo info;
    bool infoLoaded = false;
    bool locked = false;         // password still needed
    bool selected = true;        // used by merge (include in output)
    int  thumbPage = 0;
};

// ---------------------------------------------------------------------
// Per-tool working state
// ---------------------------------------------------------------------
struct ToolState {
    std::vector<StagedFile> files;

    // ranges / splitting
    std::wstring rangeSpec = L"1-";
    pdf::SplitMode splitMode = pdf::SplitMode::EveryN;
    int  everyN = 1;

    // compress
    pdf::CompressLevel compress = pdf::CompressLevel::Medium;

    // protect
    pdf::ProtectOptions protect;
    std::wstring protectConfirm;
    std::wstring unlockPassword;
    bool repairOptimise = true;

    // stamp
    pdf::StampOptions stamp;

    // image export
    pdf::ImageExportOptions imgExport;
    bool exportAllPages = true;

    // images -> pdf
    pdf::ImageToPdfOptions img2pdf;

    // image converter
    bool imgConvToPng = false;
    int  imgConvQuality = 85;
    int  imgConvMaxPx = 0;      // 0 = original size

    // text export
    bool pageBreaks = true;

    // metadata
    std::wstring mTitle, mAuthor, mSubject, mKeywords, mCreator, mProducer;
    bool metaLoaded = false;

    // organiser
    std::vector<pdf::PageThumb> pages;
    std::vector<int> order;
    std::vector<int> rotation;
    std::set<int> selectedPages;   // positions in 'order'
    bool pagesLoaded = false;

    // output
    std::wstring outDir;
    std::wstring outName;
    bool outNameManual = false;

    // last run
    bool ran = false;
    pdf::Result result;
};

// ---------------------------------------------------------------------
// Thumbnail cache (background rendering)
// ---------------------------------------------------------------------
struct ThumbKey {
    std::wstring path;
    std::wstring password;
    int page = 0;
    int maxPx = 0;
    bool operator<(const ThumbKey& o) const {
        if (path != o.path) return path < o.path;
        if (page != o.page) return page < o.page;
        if (maxPx != o.maxPx) return maxPx < o.maxPx;
        return password < o.password;
    }
};

class ThumbCache {
public:
    struct Entry {
        int w = 0, h = 0;
        HBITMAP bmp = nullptr;
        bool failed = false;
    };

    static ThumbCache& get() { static ThumbCache c; return c; }

    // UI thread: returns a ready bitmap or nullptr (and queues the render)
    HBITMAP fetch(HWND notify, const std::wstring& path, const std::wstring& password,
                  int page, int maxPx);

    void clear();
    void stop();
    void start();

private:
    void workerLoop();

    std::mutex mtx;
    std::condition_variable cv;
    std::map<ThumbKey, Entry> cache;
    std::set<ThumbKey> queued;
    std::deque<ThumbKey> queue;
    std::thread th;
    bool running = false;
    HWND notifyHwnd = nullptr;
};

// ---------------------------------------------------------------------
// Background job runner
// ---------------------------------------------------------------------
class JobRunner {
public:
    static JobRunner& get() { static JobRunner j; return j; }

    bool busy() const { return busy_; }
    int  percent() const { return percent_; }
    std::wstring step() const { std::lock_guard<std::mutex> g(mtx); return step_; }
    std::wstring title() const { std::lock_guard<std::mutex> g(mtx); return title_; }

    // Starts work on a background thread. Progress arrives as
    // WM_APP_JOB_PROGRESS and completion as WM_APP_JOB_DONE (lParam = new
    // heap pdf::Result that the receiver must delete).
    void start(HWND owner, const std::wstring& title,
               std::function<pdf::Result(pdf::JobContext&)> work);
    void requestCancel() { cancel_ = true; }
    bool cancelRequested() const { return cancel_; }
    void reset();

private:
    mutable std::mutex mtx;
    std::atomic<bool> busy_{false};
    std::atomic<bool> cancel_{false};
    std::atomic<int>  percent_{0};
    std::wstring step_;
    std::wstring title_;
    std::thread th;
};

// ---------------------------------------------------------------------
// Global application state
// ---------------------------------------------------------------------
enum class ViewId { Home, Tool, About };

struct AppState {
    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;
    ViewId view = ViewId::Home;
    ToolId tool = ToolId::Merge;

    bool dark = false;
    int  sidebarWidth = 232;
    int  titleBarHeight = 40;
    bool sidebarCollapsed = false;

    std::wstring search;
    int  searchId = 1001;

    int  categoryFilter = -1;     // -1 = all tools
    int  homeScroll = 0;
    int  toolScroll = 0;
    int  toolScrollRange = 0;
    int  organiserScroll = 0;
    int  organiserZoom = 150;     // percent
    int  passwordFileIndex = -1;  // file row waiting for a password
    ToolId jobTool = ToolId::Merge;   // tool a running job belongs to

    // caption button hit areas (filled during paint)
    RECT btnMin = {}, btnMax = {}, btnClose = {}, btnTheme = {}, btnBack = {};
    bool maximized = false;
    bool active = true;

    // per tool state
    std::map<ToolId, ToolState> tools;

    // drag & drop
    bool dragOver = false;
    int  dragOverIndex = -1;
    int  dropInsertIndex = -1;

    // toasts
    struct Toast { std::wstring text; int kind; DWORD until; };
    std::vector<Toast> toasts;

    // last folder used for output
    std::wstring lastOutDir;
    bool saveToSourceFolder = true;
    bool openAfterRun = false;
    bool keepWindowOnTop = false;

    ToolState& ts(ToolId t) { return tools[t]; }
    ToolState& cur() { return tools[tool]; }
};

AppState& app();

// theme accessors required by theme.h
Palette& currentPalette();
bool     isDarkTheme();
void     setDarkTheme(bool dark);

// ---------------------------------------------------------------------
// view entry points (implemented in views.cpp)
// ---------------------------------------------------------------------
namespace views {
int  titleBarH();
void paintHome(Frame& f, RECT area);
void paintTool(Frame& f, RECT area);
void paintAbout(Frame& f, RECT area);
void paintSidebar(Frame& f, RECT area);
void paintTitleBar(Frame& f, RECT area);
}

// ---------------------------------------------------------------------
// window level helpers (implemented in main.cpp)
// ---------------------------------------------------------------------
std::wstring defaultOutputDir(AppState& a, ToolState& st);
std::wstring defaultOutputFile(AppState& a, ToolState& st, const std::wstring& ext);
// kind: 0 = pdf, 1 = images, 2 = word
bool pickFilesDialog(HWND owner, std::vector<std::wstring>& out, int kind);
bool pickFolderDialog(HWND owner, std::wstring& out);
bool pickSaveDialog(HWND owner, std::wstring& out, const std::wstring& suggested, const wchar_t* filter);

void repaint();
void postRepaint();
void showToast(const std::wstring& text, int kind = 0);
void openTool(ToolId t);
void goHome();
void runCurrentTool();
void addFilesToCurrentTool(const std::vector<std::wstring>& paths);
void requestThumbRefresh();

// title bar caption button geometry (needed by WM_NCHITTEST)
RECT captionButtonRect(int index);   // 0 minimise, 1 maximise, 2 close
int  titleBarHeightPx();
