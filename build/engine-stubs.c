/* engine-stubs.c - format support intentionally trimmed from this build.
   Every stub throws a clean fz_error instead of linking heavy codecs. */
#include "mupdf/fitz.h"

fz_pixmap *fz_load_jpx(fz_context *ctx, const unsigned char *data, size_t size, fz_colorspace *cs)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPEG2000 images are not supported in this build"); }

void fz_load_jpx_info(fz_context *ctx, const unsigned char *data, size_t size,
                      int *w, int *h, int *xres, int *yres, fz_colorspace **cspace)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPEG2000 images are not supported in this build"); }

fz_pixmap *fz_load_jxr(fz_context *ctx, const unsigned char *data, size_t size)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPEG-XR images are not supported in this build"); }

void fz_load_jxr_info(fz_context *ctx, const unsigned char *data, size_t size,
                      int *w, int *h, int *xres, int *yres, fz_colorspace **cspace)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPEG-XR images are not supported in this build"); }

void *fz_load_jbig2(fz_context *ctx, const unsigned char *data, size_t size)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 images are not supported in this build"); }

int fz_count_jbig2_pages(fz_context *ctx, void *doc)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 images are not supported in this build"); }

void fz_drop_jbig2_document(fz_context *ctx, void *doc)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 images are not supported in this build"); }

fz_page *fz_load_jbig2_page(fz_context *ctx, void *doc, int number)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 images are not supported in this build"); }

void fz_load_jbig2_info(fz_context *ctx, const unsigned char *data, size_t size,
                        int *w, int *h, int *xres, int *yres, fz_colorspace **cspace)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 images are not supported in this build"); }

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *path, const char *options)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "the built-in docx writer is disabled; use MrDocx PDF to Word"); }

fz_document_writer *fz_new_docx_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "the built-in docx writer is disabled; use MrDocx PDF to Word"); }

fz_document_writer *fz_new_odt_writer(fz_context *ctx, const char *path, const char *options)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "ODT output is not supported in this build"); }

fz_document_writer *fz_new_odt_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "ODT output is not supported in this build"); }

fz_stream *fz_open_jbig2d(fz_context *ctx, fz_stream *chain, fz_jbig2_globals *globals, int embedded)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 streams are not supported in this build"); }

fz_jbig2_globals *fz_load_jbig2_globals(fz_context *ctx, fz_buffer *buf)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JBIG2 streams are not supported in this build"); }

void fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals) { }

void fz_drop_jbig2_globals_imp(fz_context *ctx, fz_storable *globals) { }

fz_buffer *fz_jbig2_globals_data(fz_context *ctx, fz_jbig2_globals *globals) { return NULL; }

unsigned char *fz_new_brotli_data(fz_context *ctx, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_brotli_level level)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Brotli compression is not supported in this build"); }

unsigned char *fz_new_brotli_data_from_buffer(fz_context *ctx, size_t *compressed_length, fz_buffer *buffer, fz_brotli_level level)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "Brotli compression is not supported in this build"); }

void fz_write_pixmap_as_jpx(fz_context *ctx, fz_output *out, fz_pixmap *pix, int quality)
{ fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "JPEG2000 output is not supported in this build"); }
