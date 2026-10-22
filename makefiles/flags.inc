CC  = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
AR  = x86_64-w64-mingw32-ar
WRC = x86_64-w64-mingw32-windres
MU  = ../third_party/mupdf

FZ_DEFS = -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_CBZ=0 -DFZ_ENABLE_IMG=0 \
          -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_HTML_ENGINE=0 \
          -DFZ_ENABLE_JPX=0 -DFZ_ENABLE_JBIG2=0 -DFZ_ENABLE_JS=0 -DFZ_ENABLE_JXR=0 \
          -DFZ_ENABLE_ACCELERATOR=1 -DFZ_ENABLE_ICC=0 \
          -DFZ_ENABLE_MOBI=0 -DFZ_ENABLE_FB2=0 -DFZ_ENABLE_TXT=0 -DFZ_ENABLE_OFFICE=0 \
          -DFZ_ENABLE_BROTLI=0 -DNO_CJK=1 -DTOFU=1 -DTOFU_CJK=1 -DTOFU_CJK_EXT=1 \
          -DTOFU_CJK_LANG=1 -DTOFU_SIL=1 -DTOFU_HISTORIC=1

ENGINE_CFLAGS = -std=gnu11 -O2 -DNDEBUG -mthreads -w -I$(MU)/include -I$(MU)/thirdparty/zlib -I$(MU)/thirdparty/libjpeg -I$(MU)/scripts/libjpeg -I$(MU)/thirdparty/freetype/include -I$(MU)/scripts/freetype $(FZ_DEFS)
FT_CFLAGS   = $(ENGINE_CFLAGS) -I$(MU)/thirdparty/freetype/include -I$(MU)/scripts/freetype \
              -DFT2_BUILD_LIBRARY -DDARWIN_NO_CARBON \
              -DFT_CONFIG_MODULES_H=\"slimftmodules.h\" -DFT_CONFIG_OPTIONS_H=\"slimftoptions.h\"
JPEG_CFLAGS = $(ENGINE_CFLAGS) -I$(MU)/thirdparty/libjpeg -I$(MU)/scripts/libjpeg
ZLIB_CFLAGS = $(ENGINE_CFLAGS) -I$(MU)/thirdparty/zlib

APP_CXXFLAGS = -municode -std=c++17 -O2 -DNDEBUG -mthreads -w -DUNICODE -D_UNICODE -I../src -I$(MU)/include -I$(MU)/thirdparty/zlib
