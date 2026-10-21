// =====================================================================
//  PDFForge  -  offline PDF toolbox for Windows
//  common.h  :  project-wide defines, string/path helpers, DPI scaling
//
//  Author : Ragul S
// =====================================================================
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Windows 10+ APIs (dark title bar, per-monitor DPI v2)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------
// Product identity
// ---------------------------------------------------------------------
#define APP_NAME        L"PDFForge"
#define APP_NAME_A      "PDFForge"
#define APP_VERSION     L"1.0.0"
#define APP_AUTHOR      L"Ragul S"
#define APP_TAGLINE     L"Every PDF tool you need. Offline. Forever."
#define APP_CLASS       L"PDFForgeMainWindow"
#define APP_MUTEX       L"PDFForge_SingleInstance_Mutex_RagulS"
#define APP_REGKEY      L"Software\\PDFForge"

// ---------------------------------------------------------------------
// Custom window messages
// ---------------------------------------------------------------------
#define WM_APP_JOB_PROGRESS   (WM_APP + 1)   // wParam = percent, lParam = step text
#define WM_APP_JOB_DONE       (WM_APP + 2)   // wParam = success, lParam = new JobResult*
#define WM_APP_INVALIDATE     (WM_APP + 3)   // generic repaint request
#define WM_APP_TOAST          (WM_APP + 4)

// ---------------------------------------------------------------------
// Small utilities
// ---------------------------------------------------------------------
namespace util {

inline std::wstring toWide(const std::string& s)
{
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

inline std::string toUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

inline std::wstring fmt(const wchar_t* f, ...)
{
    wchar_t buf[2048];
    va_list ap; va_start(ap, f);
    _vsnwprintf_s(buf, _TRUNCATE, f, ap);
    va_end(ap);
    return std::wstring(buf);
}

inline std::wstring lower(std::wstring s)
{
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

inline bool endsWith(const std::wstring& s, const std::wstring& suf)
{
    if (suf.size() > s.size()) return false;
    return _wcsicmp(s.c_str() + (s.size() - suf.size()), suf.c_str()) == 0;
}

inline std::wstring fileName(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

inline std::wstring dirName(const std::wstring& path)
{
    size_t p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? L"" : path.substr(0, p);
}

inline std::wstring baseName(const std::wstring& path)
{
    std::wstring n = fileName(path);
    size_t d = n.find_last_of(L'.');
    return d == std::wstring::npos ? n : n.substr(0, d);
}

inline std::wstring extension(const std::wstring& path)
{
    std::wstring n = fileName(path);
    size_t d = n.find_last_of(L'.');
    return d == std::wstring::npos ? L"" : n.substr(d);
}

inline std::wstring joinPath(const std::wstring& a, const std::wstring& b)
{
    if (a.empty()) return b;
    wchar_t last = a[a.size() - 1];
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

// "1.4 MB" style formatting
inline std::wstring sizeText(unsigned long long bytes)
{
    const wchar_t* u[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
    double v = (double)bytes; int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; i++; }
    if (i == 0) return fmt(L"%llu B", bytes);
    return fmt(L"%.1f %s", v, u[i]);
}

inline unsigned long long fileSize(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
    return ((unsigned long long)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
}

inline bool fileExists(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

inline bool dirExists(const std::wstring& path)
{
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

inline std::wstring uniquePath(const std::wstring& path)
{
    if (!fileExists(path)) return path;
    std::wstring dir = dirName(path), base = baseName(path), ext = extension(path);
    for (int i = 2; i < 10000; i++) {
        std::wstring cand = joinPath(dir, fmt(L"%s (%d)%s", base.c_str(), i, ext.c_str()));
        if (!fileExists(cand)) return cand;
    }
    return path;
}

inline std::wstring defaultDocumentsDir()
{
    wchar_t buf[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, buf)))
        return buf;
    return L"C:\\";
}

inline std::wstring systemFontDir()
{
    wchar_t buf[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_FONTS, nullptr, SHGFP_TYPE_CURRENT, buf)))
        return buf;
    return L"C:\\Windows\\Fonts";
}

inline std::wstring trim(const std::wstring& s)
{
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

inline std::vector<std::wstring> split(const std::wstring& s, wchar_t sep)
{
    std::vector<std::wstring> out; std::wstring cur;
    for (wchar_t c : s) { if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c; }
    out.push_back(cur);
    return out;
}

// Clamp
template <class T> inline T clampv(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

} // namespace util

// ---------------------------------------------------------------------
// DPI awareness helper (per-monitor v2 declared in the manifest; this
// gives us a scale factor for layout maths)
// ---------------------------------------------------------------------
class Dpi {
public:
    static Dpi& get() { static Dpi d; return d; }

    void init(HWND hwnd)
    {
        HMODULE u = LoadLibraryW(L"user32.dll");
        if (u) {
            typedef UINT(WINAPI* GDF)(HWND);
            GDF f = (GDF)GetProcAddress(u, "GetDpiForWindow");
            if (f && hwnd) dpi_ = (int)f(hwnd);
        }
        if (dpi_ <= 0) dpi_ = 96;
    }
    void set(int dpi) { if (dpi > 0) dpi_ = dpi; }
    int  value() const { return dpi_; }
    int  s(int v) const { return MulDiv(v, dpi_, 96); }      // scale
    int  sp(int pt) const { return MulDiv(pt, dpi_, 72); }   // font points -> px
    float scale() const { return dpi_ / 96.0f; }

private:
    int dpi_ = 96;
};

inline int S(int v) { return Dpi::get().s(v); }

// ---------------------------------------------------------------------
// Registry helpers (settings persistence)
// ---------------------------------------------------------------------
namespace settings {

inline std::wstring readStr(const wchar_t* name, const std::wstring& def = L"")
{
    HKEY k; std::wstring out = def;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, APP_REGKEY, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        wchar_t buf[1024]; DWORD cb = sizeof(buf), type = 0;
        if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)buf, &cb) == ERROR_SUCCESS &&
            (type == REG_SZ || type == REG_EXPAND_SZ))
            out.assign(buf, cb / sizeof(wchar_t) - (cb && buf[cb / sizeof(wchar_t) - 1] == 0 ? 1 : 0));
        RegCloseKey(k);
    }
    return out;
}

inline int readInt(const wchar_t* name, int def)
{
    HKEY k; int out = def;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, APP_REGKEY, 0, KEY_READ, &k) == ERROR_SUCCESS) {
        DWORD v = 0, cb = sizeof(v), type = 0;
        if (RegQueryValueExW(k, name, nullptr, &type, (LPBYTE)&v, &cb) == ERROR_SUCCESS && type == REG_DWORD)
            out = (int)v;
        RegCloseKey(k);
    }
    return out;
}

inline void writeStr(const wchar_t* name, const std::wstring& v)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, APP_REGKEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(k);
    }
}

inline void writeInt(const wchar_t* name, int v)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, APP_REGKEY, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        DWORD d = (DWORD)v;
        RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&d, sizeof(d));
        RegCloseKey(k);
    }
}

} // namespace settings
