// =====================================================================
//  PDFForge  -  pdfengine.h : the offline PDF engine
//  A thin, exception-free C++ facade over the bundled MuPDF core.
//  All work happens on a background thread; the UI only sees progress
//  callbacks and a Result struct.
//
//  Author : Ragul S
// =====================================================================
#pragma once
#include "common.h"

namespace pdf {

// ---------------------------------------------------------------------
// job plumbing
// ---------------------------------------------------------------------
struct JobContext {
    std::function<void(int percent, const std::wstring& step)> progress;
    std::function<bool()> cancelled;
    void report(int p, const std::wstring& s) const { if (progress) progress(p, s); }
    bool stop() const { return cancelled && cancelled(); }
};

struct Result {
    bool ok = false;
    std::wstring error;
    std::vector<std::wstring> outputs;      // files produced
    unsigned long long bytesBefore = 0;
    unsigned long long bytesAfter = 0;
    int pages = 0;

    static Result failure(const std::wstring& e) { Result r; r.ok = false; r.error = e; return r; }
    static Result success() { Result r; r.ok = true; return r; }
};

// ---------------------------------------------------------------------
// document info
// ---------------------------------------------------------------------
struct DocInfo {
    bool valid = false;
    bool needsPassword = false;
    bool passwordOk = false;
    int  pageCount = 0;
    std::wstring title, author, subject, keywords, creator, producer;
    std::wstring pdfVersion;
    std::wstring pageSize;             // human readable, e.g. "A4 portrait"
    double pageW = 0, pageH = 0;       // points
    unsigned long long fileSize = 0;
    bool hasForms = false;
    bool hasAnnotations = false;
    bool hasOutline = false;
    bool linearized = false;
    bool encrypted = false;
};

bool readInfo(const std::wstring& path, const std::wstring& password, DocInfo& out, std::wstring& err);

// Page geometry for the organiser grid
struct PageThumb {
    int   index = 0;
    int   rotation = 0;
    float widthPt = 0, heightPt = 0;
    bool  readable = true;   // false when the page could not be loaded at all
};
bool readPageList(const std::wstring& path, const std::wstring& password,
                  std::vector<PageThumb>& pages, std::wstring& err);

// Render one page to a 32bpp BGRA top-down buffer for fast UI display.
struct BitmapData {
    int width = 0, height = 0;
    std::vector<unsigned char> pixels;   // BGRA, top-down
    bool valid = false;
};
BitmapData renderPage(const std::wstring& path, const std::wstring& password, int pageIndex,
                      int maxPixels, bool annotations);

// ---------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------
// "1-3,5,8-10" (1-based, inclusive) -> sorted unique 0-based indices
std::vector<int> parsePageRanges(const std::wstring& spec, int pageCount);
// human readable page size
std::wstring describePageSize(double wPt, double hPt);

// ---------------------------------------------------------------------
// operations
// ---------------------------------------------------------------------
struct MergeItem { std::wstring path; std::wstring password; };

Result mergeDocuments(const std::vector<MergeItem>& files, const std::wstring& outPath, JobContext& jc);

// keepOnly=true  -> extract the listed pages
// keepOnly=false -> delete the listed pages
Result selectPages(const std::wstring& path, const std::wstring& password,
                   const std::vector<int>& pages0, bool keepOnly,
                   const std::wstring& outPath, JobContext& jc);

// newOrder[i] = source page index placed at position i
Result reorderPages(const std::wstring& path, const std::wstring& password,
                    const std::vector<int>& newOrder, const std::vector<int>& rotations,
                    const std::wstring& outPath, JobContext& jc);

Result rotatePages(const std::wstring& path, const std::wstring& password,
                   const std::vector<int>& pages0, int degrees,
                   const std::wstring& outPath, JobContext& jc);

enum class SplitMode { Ranges, EveryN, OnePerFile, ExtractOdd, ExtractEven };
struct SplitOptions {
    SplitMode mode = SplitMode::Ranges;
    std::wstring ranges;      // for Ranges
    int everyN = 2;           // for EveryN
};
// Produces one or more files named "<base> - part N.pdf" in outDir.
Result splitDocument(const std::wstring& path, const std::wstring& password,
                     const SplitOptions& opt, const std::wstring& outDir,
                     const std::wstring& baseName, JobContext& jc);

enum class CompressLevel { Low, Medium, High, Maximum };
Result compressDocument(const std::wstring& path, const std::wstring& password,
                        CompressLevel level, const std::wstring& outPath, JobContext& jc);

struct ProtectOptions {
    std::wstring userPassword;
    std::wstring ownerPassword;
    bool allowPrinting = true;
    bool allowCopying = true;
    bool allowEditing = false;
    bool use256 = true;
};
Result protectDocument(const std::wstring& path, const std::wstring& password,
                       const ProtectOptions& opt, const std::wstring& outPath, JobContext& jc);

Result unlockDocument(const std::wstring& path, const std::wstring& password,
                      const std::wstring& outPath, JobContext& jc);

Result repairDocument(const std::wstring& path, const std::wstring& password,
                      const std::wstring& outPath, JobContext& jc);

Result flattenDocument(const std::wstring& path, const std::wstring& password,
                       const std::wstring& outPath, JobContext& jc);

enum class StampKind { Watermark, PageNumber };
struct StampOptions {
    StampKind kind = StampKind::Watermark;
    std::wstring text;                 // watermark text
    int    fontSize = 48;
    int    opacity = 30;               // percent
    int    rotation = 45;              // degrees
    int    colorR = 128, colorG = 128, colorB = 128;
    bool   behind = false;             // draw under the page content
    // page numbers
    std::wstring numberFormat = L"{n}";  // supports {n} and {N}
    int    pageNumSize = 12;
    int    pageNumMargin = 36;          // points from the edge
    int    pageNumPos = 0;               // 0 bottom-left .. 8 bottom-right, etc.
    int    pageNumStart = 1;
    int    pageNumColorR = 90, pageNumColorG = 90, pageNumColorB = 90;
    int    pageNumOpacity = 100;
    std::wstring fontFile;               // optional TTF/OTF for Unicode stamps
};
Result stampDocument(const std::wstring& path, const std::wstring& password,
                     const StampOptions& opt, const std::vector<int>& pages0,
                     const std::wstring& outPath, JobContext& jc);

struct ImageExportOptions {
    std::wstring format = L"png";        // png | jpg
    int dpi = 150;
    int jpegQuality = 85;
};
// Writes "<base>-01.png" etc. into outDir; returns the list of files.
Result exportImages(const std::wstring& path, const std::wstring& password,
                    const ImageExportOptions& opt, const std::vector<int>& pages0,
                    const std::wstring& outDir, const std::wstring& baseName, JobContext& jc);

struct ImageToPdfOptions {
    int  pageSize = 0;                   // 0 = fit to image, 1 = A4, 2 = Letter
    int  marginPt = 0;
    bool landscape = false;
    int  jpegQuality = 90;               // used when re-encoding
};
Result imagesToPdf(const std::vector<std::wstring>& imagePaths, const ImageToPdfOptions& opt,
                   const std::wstring& outPath, JobContext& jc);

Result exportText(const std::wstring& path, const std::wstring& password,
                  const std::wstring& outPath, bool pageBreaks, JobContext& jc);

Result exportHtml(const std::wstring& path, const std::wstring& password,
                  const std::wstring& outPath, JobContext& jc);

// Office-flavoured conversions, fully offline.
Result pdfToWord(const std::wstring& path, const std::wstring& password,
                 const std::wstring& outPath, JobContext& jc);
Result wordToPdf(const std::wstring& path, const std::wstring& outPath, JobContext& jc);

Result setMetadata(const std::wstring& path, const std::wstring& password,
                   const std::wstring& title, const std::wstring& author,
                   const std::wstring& subject, const std::wstring& keywords,
                   const std::wstring& creator, const std::wstring& producer,
                   const std::wstring& outPath, JobContext& jc);

} // namespace pdf
