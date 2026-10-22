# MrDocx

**Every document tool you need. Offline. Forever.**

MrDocx is a native Windows desktop app that packs **23 document tools** into one
fast, keyboard-friendly window: merge, split, organise, compress, convert,
watermark, protect PDFs — plus Word ⇄ PDF, images ⇄ PDF, text/Markdown → PDF and
PDF → Markdown. No webview, no browser, no uploads, no accounts, no telemetry.
Every file is processed on your machine and never leaves it.

Built with plain Win32 C++ and the MuPDF engine. One `.exe`, no installer required
(an Inno Setup script is included if you want one).

> Author: **Ragul S** — v1.1.0

---

## The tools

| Category | Tools |
|---|---|
| **Organise** | Merge PDF · Split PDF · Organise PDF (visual page grid: reorder, rotate, delete) · Rotate PDF · Remove Pages · Extract Pages |
| **Optimise** | Compress PDF · Repair PDF |
| **Edit** | Watermark · Page Numbers · Flatten PDF · Edit Properties (metadata) |
| **Convert** | PDF → Image · Images → PDF · PDF → Text · PDF → HTML · PDF → Word (.docx) · Word (.docx) → PDF · **Convert Images** (JPG/PNG/BMP/TIFF/GIF → JPG/PNG with quality + resize) · **Text / Markdown → PDF** · **PDF → Markdown** |
| **Secure** | Protect PDF (encrypt) · Unlock PDF |

Every tool shows a live preview where it matters: the organiser renders real page
thumbnails, the file list reads page counts/sizes in a background thread, and jobs
run on a worker thread with progress and cancel.

## Download / install

- **Just run it:** grab `MrDocx-1.1.0-windows-x64.zip` from Releases, unzip,
  double-click `MrDocx.exe`. That is the whole installation.
- **Installer:** build `installer/MrDocx.iss` with Inno Setup 6 for a proper
  Start-menu / Add-Remove-Programs install.
- **From source:** see below.

Requirements: Windows 10/11 x64. Nothing else — the exe is fully static.

## Building from source

You need a MinGW-w64 toolchain (`x86_64-w64-mingw32-gcc/g++/windres`, GCC 12+).
On Debian/Ubuntu: `sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64`.

```bash
mkdir -p build && cd build
make -f ../makefiles/Makefile.engine -j8   # builds libpdfengine.a (MuPDF + zlib + libjpeg + FreeType)
make -f ../makefiles/Makefile.app          # links ../bin/MrDocx.exe
```

The engine makefile compiles a deliberately slim MuPDF configuration
(no JS, no XPS/SVG/EPUB/HTML engine, no JPX/JBIG2/JXR codecs — clean stubs throw a
clear "not supported in this build" error instead), which keeps the exe near 5 MB.

## Honest notes

- **Word ⇄ PDF** is a text-fidelity converter (paragraphs, headings, bold/italic/
  underline, sizes, colours, page breaks). It does not replicate every Word layout
  feature (tables, floating images, SmartArt) — that level of fidelity requires
  licensed server-side engines; MrDocx stays fully offline instead.
- Scanned PDFs have no text layer, so PDF → Text/Word/Markdown will say so plainly
  instead of returning blank pages. OCR is intentionally not bundled.
- JPEG2000 / JPEG-XR / JBIG2 images inside PDFs are not decoded (codec-free build);
  everything else — PNG, JPG, TIFF, GIF, BMP — works.

## Licence

MrDocx statically links MuPDF, therefore the whole project is distributed under
**GNU AGPL-3.0**. See `THIRD-PARTY.md` and the licence files under
`third_party/mupdf/`.

## Credits

Designed and built by **Ragul S**. PDF engine: MuPDF 1.26.4 (Artifex).
