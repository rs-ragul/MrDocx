# Third-party components in MrDocx

MrDocx is built entirely from source in this repository. The following
upstream components are vendored under `third_party/` and compiled into
`MrDocx.exe`. Their licences are reproduced alongside them.

| Component | Version | Licence | File |
|---|---|---|---|
| **MuPDF** (fitz + pdf modules) | 1.26.4 | GNU AGPL v3 | `third_party/mupdf/LICENSE.mupdf.AGPL-3.0.txt` |
| **zlib** (via MuPDF) | 1.3.x | zlib licence | `third_party/mupdf/LICENSE.zlib.txt` |
| **libjpeg** (Independent JPEG Group) | 9.x | IJG licence | `third_party/mupdf/LICENSE.libjpeg.txt` |
| **FreeType** (slim subset, via MuPDF) | 2.13.x | FreeType Licence (BSD-style) / GPL-2+ | `third_party/mupdf/LICENSE.freetype.txt` |
| **URW base-14 fonts** (generated C arrays) | — | AGPL v3 (part of MuPDF) | see MuPDF licence |

## AGPL-3.0 source offer

MuPDF is licensed under the GNU Affero General Public Licence, version 3.
Because MrDocx links MuPDF statically, MrDocx as a whole is distributed
under the AGPL-3.0 as well.

> The complete Corresponding Source for this program — the MrDocx
> application sources, the build scripts, and the vendored MuPDF, zlib,
> libjpeg and FreeType sources — is provided together with the program, from
> the same location you obtained `MrDocx.exe`.

Copyright of the MrDocx application code: **© 2026 Ragul S**.

## Deliberately excluded codecs

The JPEG2000 (openjpeg) and JBIG2 (jbig2dec) decoders are **not** compiled in.
MuPDF's own stubs (and `build/jbig2-stub.c`) turn any attempt to decode such an
image into a clean *"unsupported in this build"* error. OCR (Tesseract) and
Office-format conversion are likewise out of scope: no such engine is bundled,
so nothing is ever shipped half-working.

## Windows-only dependencies

Everything else the executable imports is part of Microsoft Windows itself:
`user32`, `gdi32`, `gdiplus`, `comctl32`, `comdlg32`, `shell32`, `shlwapi`,
`ole32`, `dwmapi`, `uxtheme`, `msimg32`, `windowscodecs` and the UCRT.
