/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: MIT -- the seam headers a GL implementation compiles against */

/* symgl_native.h -- OURS: the seam between Mesa's C side (egl_symbian.c, sym_frontend.c, sym_winsys.c) and the
 * Symbian C++ window glue (src/symgl/symgl_window.cpp). An EGLNativeWindowType on Symbian is an RWindow*
 * (RWindowBase*); Mesa never looks inside it, only these functions do.
 *
 * Presentation (NGA): the window gets a SurfaceManager surface of SYMGL_NBUF buffers as its background surface; GL
 * renders straight into those buffers (softpipe through a display target over the memory, a GPU through an
 * import of the chunk), acquire/present rotate them with the compositor's "displayed" notifications. */
#ifndef SYMGL_NATIVE_H
#define SYMGL_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

#define SYMGL_NBUF 3

typedef struct {
   int chunk_handle;          /* the surface's RChunk handle in this process */
   void *base;                /* its CPU address (cached memory) */
   unsigned chunk_size;       /* bytes, a page multiple */
   unsigned width, height, stride;
   unsigned nbuf;
   unsigned offset[SYMGL_NBUF];   /* each buffer's offset in the chunk (page aligned) */
   unsigned buf_size;         /* bytes per buffer */
} symgl_surface;

/* the window's current size in pixels; 0 if aWindow is not usable */
int symgl_window_size(void *native_window, unsigned *width, unsigned *height);

/* the window's presentation surface of that size (created, or re-created after a resize); 0 = no SurfaceManager
 * on this ROM: use symgl_window_present_pixels instead */
int symgl_window_surface(void *native_window, unsigned width, unsigned height, symgl_surface *out);

/* the buffer to render the next frame into; blocks until the compositor no longer reads it */
int symgl_window_acquire(void *native_window);

/* hand a finished buffer to the compositor (pipelined: returns at once) */
void symgl_window_present(void *native_window, int buf);

/* the fallback: show a finished frame by copying it -- BGRX8888 rows (= Symbian EColor16MU), top row first */
void symgl_window_present_pixels(void *native_window, const void *pixels, unsigned stride, unsigned width,
                                 unsigned height);

/* the window is going away from EGL (eglDestroySurface): drop what the glue keeps for it */
void symgl_window_release(void *native_window);

/* debug output (RDebug) */
void symgl_log(const char *msg);

/* G6: the committed size of a chunk handle of this process (an EGLImage source), 0 = not a chunk */
unsigned symgl_chunk_size(int chunk_handle);

/* G6b: an RSgImage* (the 2010 graphics resource) seen as memory the GPU can map: its pixel data is a shared chunk
 * (the sgresource kernel extension's DChunk, mapped into this process), reached by its CPU address -- the chunk
 * handle stays private to the sgdevice channel; the GPU driver finds the chunk from the address
 * (RMaliChannel::ImportAddress). 1 = ok; 0 = not an RSgImage with pixel data in system memory, or a format we do not
 * import. */
#define SYMGL_SGFMT_ARGB8888 1   /* BGRA bytes (EColor16MA / ESgPixelFormatARGB_8888[_PRE]) */
#define SYMGL_SGFMT_XRGB8888 2   /* BGRX bytes (ESgPixelFormatXRGB_8888) */
#define SYMGL_SGFMT_RGB565 3
typedef struct {
   void *base;                /* the first row's CPU address (MSgImage_Sw::DataAddress) */
   unsigned stride;           /* bytes between rows (DataStride) */
   unsigned width, height;
   unsigned format;           /* SYMGL_SGFMT_* */
   unsigned usage;            /* TSgUsageBits */
} symgl_sgimage;
int symgl_sgimage_info(void *rsgimage, symgl_sgimage *out);

#ifdef __cplusplus
}
#endif

#endif
