# PDFForge

**Every PDF tool you need, in one offline Windows app.**
Built by **Ragul S**.

PDFForge is a native Win32 application — no browser, no web view, no Electron,
no installer service, no account. It bundles the MuPDF rendering engine and
does all of its work on your machine: a file you open never leaves your PC,
because there is nowhere for it to go.

---

## What it does

Eighteen tools behind one home screen, grouped into five categories:

| Category | Tools |
|---|---|
| **Organise** | Merge PDFs · Split · Organise (rotate / reorder / delete pages) · Rotate · Remove pages · Extract pages |
| **Optimise** | Compress (4 levels) · Repair |
| **Convert** | PDF → JPG / PNG · JPG / PNG → PDF · PDF → Text · PDF → HTML |
| **Secure** | Protect (AES-128/256 passwords & permissions) · Unlock |
| **Edit** | Watermark · Page numbers · Flatten forms & annotations · Metadata |

### Small things that make it pleasant

* **Drag & drop anywhere** in the window; dropped images open the
  image→PDF tool automatically, dropped PDFs open Merge.
* **No save dialog before you run.** PDFForge picks a sensible output name
  next to the source file (`report (compressed).pdf`) and shows a result bar
  with *Open*, *Open folder* and *Save as* afterwards.
* **Page organiser** with real rendered thumbnails, click / Ctrl-click
  selection, per-page rotation and delete, and a zoom slider.
* **Passwords are asked for inline** with a styled dialog, never a console.
* **Light and dark themes**, per-monitor DPI aware, keyboard friendly
  (Tab cycles focus, Enter runs, Esc backs out, Ctrl+O adds files,
  Ctrl+Enter runs, F1 opens About).
* **Single instance**: opening a second copy forwards its files to the first.
* Jobs run on a background thread with a progress bar and a **Cancel** button;
  closing the window while a job runs asks first.

---

## Building it

You need a MinGW-w64 UCRT toolchain (`x86_64-w64-mingw32ucrt-g++`) and GNU make.

```sh
cd build
make -f Makefile.engine     # builds libpdfengine.a  (MuPDF subset, ~285 objects)
make -f Makefile.app        # builds ../bin/PDFForge.exe
make -f Makefile.app strip  # optional: strip symbols
```

### Installing (optional - it is portable)

`PDFForge.exe` needs **no installation**: copy it anywhere and double-click it.
Settings (theme, window position, output folder) are stored per user in
`HKCU\Software\PDFForge`, and nothing else on the system is touched.

If you prefer a classic install (Start Menu entry, an *Add or remove programs*
entry, optional desktop icon and an "Open with PDFForge" verb for PDFs),
build the bundled Inno Setup script with the free Inno Setup 6:

```sh
iscc installer\PDFForge.iss      # -> installer\output\PDFForgeSetup-1.0.0.exe
```

The output is a **single self-contained `PDFForge.exe`** (≈6.8 MB): the C++
runtime is linked statically and the only DLLs it loads are the ones that ship
with Windows (user32, gdi32, gdiplus, comctl32, …). Copy it to any
Windows 10/11 x64 machine and run it.

### Source layout

```
src/common.h      product metadata, Win32 includes, DPI + registry settings
src/theme.h       light/dark palettes, font cache
src/icons.h/.cpp  vector icon geometry, drawn with GDI+
src/ui.h          the immediate-mode widget toolkit
src/ui_layout.cpp layout stack, rounded fills, shadows, gradients
src/ui_widgets.cpp buttons, fields, combos, sliders, lists, scrollbars
src/ui_helpers.cpp popup menus, message box, password input box
src/pdfengine.*   MuPDF facade: every PDF operation, one thread-local context
src/app.h/.cpp    tool table, app state, thumbnail cache, job runner
src/views*.cpp    the home / tool / about views
src/main.cpp      window, custom frame, input, message loop, dialogs
res/              icon, version info, per-monitor-DPI manifest
build/            Makefiles and the vendored engine build
third_party/      vendored MuPDF subset (see THIRD-PARTY.md)
```

### Design notes

* The UI is **immediate mode**: one `Frame` struct, one paint per message,
  widget state (text, focus, drags) kept between frames. Two layout passes
  (measure → draw) give cards their height; scrolling is manual and clipped.
* The title bar, sidebar, scrollbars and every control are hand-drawn with
  GDI/GDI+ into a double buffer. `WM_NCCALCSIZE` removes the stock caption and
  `WM_NCHITTEST` hands back `HTCAPTION` / resize edges so dragging, Aero Snap
  and resizing still behave like a normal window.
* One **thread-local `fz_context`** per thread; the UI thread never blocks on
  PDF work — jobs post progress and completion messages back.

---

## Licence

PDFForge itself is © 2026 **Ragul S**.

It links **MuPDF 1.26.4**, which is licensed under the **GNU Affero General
Public Licence v3** (see `third_party/mupdf/LICENSE.mupdf.AGPL-3.0.txt`).
Consequently this whole project is distributed under the AGPL-3.0: the complete
corresponding source — including this repository and the vendored engine — is
available from the same place you obtained the program. See `THIRD-PARTY.md`
for every bundled component and its licence.

JPEG2000 and JBIG2 codecs are intentionally **not** built in; documents using
them report a clear "unsupported" error instead of silently mis-rendering.
