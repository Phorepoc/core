// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phgpucompose.cpp -- OURS (GPU plan G8): libWFC's scene composed by the GPU (GLES2 through phgl) instead of
// the 2010 per-pixel float pipeline (~15 ms per full-screen element at 360x640, ~37 ms with a rotated game on top).
//
// wfccontext.c (our copy) calls WFC_Context_PhGpuCompose first for every full composition; it asks PhGpu_Want and
// PhGpu_SourceOk before it locks anything, then PhGpu_Begin (the target stream's write buffer), PhGpu_Element per
// visible element bottom to top, PhGpu_End. Every pixel stays where it is: the source buffers and the target buffer are
// shared chunks (surface manager), wrapped as EGLImages by address (Mesa egl_symbian.c EGL_SYMBIAN_MEMORY_IMAGE_PH, the
// G6b import) -- no copies; the CPU caches are synced around the draw (eglWaitNative before, eglWaitClient after).
// EGLImages are cached per (surface id, buffer index): a surface id is never reused, an address may be.
//
// Semantics (OpenWF Composition 1.0, as the 2010 pipeline): per element crop (float source rectangle) -> vertical flip
// -> clockwise source rotation -> scale to the destination rectangle (nearest, or bilinear for FASTER/BETTER) -> blend
// (none = opaque; SOURCE = per-pixel alpha, premultiplied or not; ELEMENT_GLOBAL_ALPHA = times the global alpha),
// src-over in premultiplied space over the background colour; then the context rotation (clockwise) into the target.
// Declined (the CPU composes): masks, linear-light / luminance sources, a scene the GPU cannot import, a board without a GPU channel (
// EGL does not come up), the developer menu switch off (repository 0xE7015A01 key 9).
#include <string.h>
#include <e32base.h>
#include <e32debug.h>
#include <hal.h>
#include <centralrepository.h>
#include <EGL/egl.h>
#include <graphics/symbianstream.h>

// ---- the GLES2 subset we use (the libwfc build has no GLES headers on its include path) --------------------------
typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef char GLchar;
typedef unsigned int GLbitfield;
typedef void* GLeglImageOES;
const GLenum KGlTexture2D = 0x0DE1, KGlTextureMinFilter = 0x2801, KGlTextureMagFilter = 0x2800,
	KGlTextureWrapS = 0x2802, KGlTextureWrapT = 0x2803, KGlNearest = 0x2600, KGlLinear = 0x2601,
	KGlClampToEdge = 0x812F, KGlFramebuffer = 0x8D40, KGlColorAttachment0 = 0x8CE0, KGlFramebufferComplete = 0x8CD5,
	KGlColorBufferBit = 0x4000, KGlBlend = 0x0BE2, KGlOne = 1, KGlOneMinusSrcAlpha = 0x0303, KGlTriangles = 0x0004,
	KGlFloat = 0x1406, KGlVertexShader = 0x8B31, KGlFragmentShader = 0x8B30, KGlCompileStatus = 0x8B81,
	KGlLinkStatus = 0x8B82, KGlTexture0 = 0x84C0, KGlScissorTest = 0x0C11, KGlDither = 0x0BD0;
#define EGL_SYMBIAN_MEMORY_IMAGE_PH 0x3FF1		// egl_symbian.c: buffer = a symgl_sgimage (below)
#define EGL_NO_CONFIG_KHR ((EGLConfig)0)
#ifndef EGL_KHR_image
typedef void* EGLImageKHR;
#endif

namespace
	{
	// symgl_native.h's symgl_sgimage (the EGL_SYMBIAN_MEMORY_IMAGE_PH client buffer)
	struct TMemImage
		{
		void* iBase;
		unsigned iStride, iWidth, iHeight, iFormat;	// SYMGL_SGFMT_*: 1 ARGB8888 (BGRA bytes), 2 XRGB8888, 3 RGB565
		unsigned iUsage;
		};

	struct TGl
		{
		void (*GenTextures)(GLsizei, GLuint*);
		void (*DeleteTextures)(GLsizei, const GLuint*);
		void (*BindTexture)(GLenum, GLuint);
		void (*ActiveTexture)(GLenum);
		void (*TexParameteri)(GLenum, GLenum, GLint);
		void (*GenFramebuffers)(GLsizei, GLuint*);
		void (*DeleteFramebuffers)(GLsizei, const GLuint*);
		void (*BindFramebuffer)(GLenum, GLuint);
		void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
		GLenum (*CheckFramebufferStatus)(GLenum);
		void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
		void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
		void (*Clear)(GLbitfield);
		void (*Enable)(GLenum);
		void (*Disable)(GLenum);
		void (*BlendFunc)(GLenum, GLenum);
		GLuint (*CreateShader)(GLenum);
		void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
		void (*CompileShader)(GLuint);
		void (*GetShaderiv)(GLuint, GLenum, GLint*);
		GLuint (*CreateProgram)();
		void (*AttachShader)(GLuint, GLuint);
		void (*BindAttribLocation)(GLuint, GLuint, const GLchar*);
		void (*LinkProgram)(GLuint);
		void (*GetProgramiv)(GLuint, GLenum, GLint*);
		void (*UseProgram)(GLuint);
		GLint (*GetUniformLocation)(GLuint, const GLchar*);
		void (*Uniform1i)(GLint, GLint);
		void (*Uniform1f)(GLint, GLfloat);
		void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
		void (*EnableVertexAttribArray)(GLuint);
		void (*DrawArrays)(GLenum, GLint, GLsizei);
		void (*Finish)();
		void (*Flush)();
		GLenum (*GetError)();
		void (*EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
		EGLImageKHR (*CreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
		EGLBoolean (*DestroyImageKHR)(EGLDisplay, EGLImageKHR);
		void (*Scissor)(GLint, GLint, GLsizei, GLsizei);	// (round 42q) damage-only redraw
		};

	// one cached EGLImage (a source buffer, or a target buffer with its framebuffer)
	struct TEntry
		{
		TUint32 iSid[4];		// TSurfaceId
		TInt iIndex;			// buffer index in the surface
		void* iData;
		TInt iW, iH, iStride, iFormat;
		EGLImageKHR iImage;
		GLuint iTex, iFbo;
		TUint iUsed;			// composition number of the last use
		// (round 42q, a target buffer) the internal-scene box damaged since this buffer was last drawn; !iDrawn = all of it
		TBool iDrawn;
		TInt iPx0, iPy0, iPx1, iPy1;
		};
	const TInt KEntries = 20;		// egl_symbian.c registers at most 32 cache-maintenance ranges per process
	const TUint KIdleCompositions = 180;	// an image unused this long is dropped (its import pins the chunk's pages)

	TGl gl;
	TInt gState;				// 0 not tried, 1 up, -1 off / failed for good
	TUint gThread;				// the composer thread (the GL context is current there only)
	EGLDisplay gDpy = EGL_NO_DISPLAY;
	EGLContext gCtx = EGL_NO_CONTEXT;
	GLuint gProg;
	GLint gUniTex, gUniGa, gUniUseA, gUniPremul;
	TEntry gEntry[KEntries];
	TUint gComp;				// compositions so far
	TUint gTLook, gTBegun, gTElems, gTElLook;	// round 41 timing split
	TBool gClearPending;		// round 42: the target clear waits for the first element (an opaque full-target layer skips it)
	TBool gClearRgbBlack;		// the pending clear colour's rgb is 0 (a full-target alpha layer may stand in for the clear)
	TUint gClearSkipped;
	TUint gTs[10], gTMark;		// round 42: per GL call (0 bind fbo+viewport, 1 clear, 2 program state, 3 bind tex, 4 tex params, 5 blend+uniforms, 6 attrib pointers, 7 draw)
	EGLImageKHR gTargetImage;	// this composition's target (PhGpu_Begin), for eglPresentImagePH
	// (round 42q) damage-only redraw: the elements are drawn in PhGpu_End, scissored to the box this target buffer lacks
	struct TDraw
		{
		GLuint iTex;
		GLint iFilt;
		TBool iUseA;
		TReal32 iGa, iPremul;
		TReal32 iDst[4];
		GLfloat iPv[12], iUv[12];
		};
	const TInt KMaxDraws = 32;
	TDraw gDraw[KMaxDraws];
	TInt gDraws;
	TEntry* gTarget;			// this composition's target entry
	TUint32 gSig, gLastSig;		// the scene's layout (streams, rects, alpha, ...): a change without a post = a full frame
	TBool gCleanEnd;			// the last composition ended on the GPU (else the CPU pipeline wrote a target: all are stale)
	typedef EGLBoolean (*TPresent)(EGLDisplay, EGLImageKHR);
	TPresent gPresent;			// Mesa's eglPresentImagePH (svga: the target becomes the screen target), NULL = none
	TUint gShown, gNotShown;
	TBool gFailed;				// this composition hit a GPU failure
	TInt gTw, gTh, gIw, gIh, gRot;	// the current target, internal (pre-rotation) size, rotation (quarter turns cw)
	TUint gT0, gTicks, gFrames, gElems;
	TInt gFreq;

	TBool Proc(void*& aFn, const char* aName)
		{
		aFn = (void*)eglGetProcAddress(aName);
		if (!aFn)
			RDebug::Printf("PH: wfc gpu: no %s", aName);
		return aFn != NULL;
		}

	// developer menu key 9 "GPU composition" (1 = on, the default; 0 = the 2010 CPU pipeline; 2 = auto: on only over a
	// hardware GPU, i.e. not Mesa's software rasterizers, which compose slower than libWFC's own CPU pipeline)
	TInt Setting()
		{
		TInt v = 1;
		CTrapCleanup* tc = User::TrapHandler() ? NULL : CTrapCleanup::New();
		TRAPD(err, CRepository* rep = CRepository::NewL(TUid::Uid(0xE7015A01)); rep->Get(9, v); delete rep);
		delete tc;
		RDebug::Printf("PH: wfc gpu: GPU composition setting %d (repository %d)", v, err);
		return err == KErrNone || err == KErrNotFound ? v : 1;
		}

	GLuint Shader(GLenum aType, const char* aSrc)
		{
		GLuint s = gl.CreateShader(aType);
		gl.ShaderSource(s, 1, &aSrc, NULL);
		gl.CompileShader(s);
		GLint ok = 0;
		gl.GetShaderiv(s, KGlCompileStatus, &ok);
		return ok ? s : 0;
		}

	TBool Init()
		{
		gThread = (TUint)RThread().Id();
		const TInt setting = Setting();
		if (!setting)
			return EFalse;
		gDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		EGLint maj = 0, min = 0;
		if (gDpy == EGL_NO_DISPLAY || !eglInitialize(gDpy, &maj, &min))
			{
			RDebug::Printf("PH: wfc gpu: no EGL display (%x): the CPU composes", eglGetError());
			return EFalse;
			}
		if (!eglBindAPI(EGL_OPENGL_ES_API))
			return EFalse;
		static const EGLint ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		gCtx = eglCreateContext(gDpy, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ctxAttr);
		if (gCtx == EGL_NO_CONTEXT || !eglMakeCurrent(gDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, gCtx))
			{
			RDebug::Printf("PH: wfc gpu: no surfaceless GLES2 context (%x): the CPU composes", eglGetError());
			return EFalse;
			}
		if (setting == 2)
			{
			typedef const char* (*TGetString)(GLenum);
			TGetString getString = (TGetString)eglGetProcAddress("glGetString");
			const char* r = getString ? getString(0x1F01 /* GL_RENDERER */) : NULL;
			const TBool soft = !r || strstr(r, "softpipe") || strstr(r, "llvmpipe");
			RDebug::Printf("PH: wfc gpu: auto: renderer %s -> %s", r ? r : "(none)", soft ? "the CPU composes" : "the GPU composes");
			if (soft)
				{
				eglMakeCurrent(gDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
				eglDestroyContext(gDpy, gCtx);
				gCtx = EGL_NO_CONTEXT;
				return EFalse;
				}
			}
		void** f = (void**)&gl;
		static const char* const KNames[] = { "glGenTextures", "glDeleteTextures", "glBindTexture", "glActiveTexture",
			"glTexParameteri", "glGenFramebuffers", "glDeleteFramebuffers", "glBindFramebuffer", "glFramebufferTexture2D",
			"glCheckFramebufferStatus", "glViewport", "glClearColor", "glClear", "glEnable", "glDisable", "glBlendFunc",
			"glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv", "glCreateProgram", "glAttachShader",
			"glBindAttribLocation", "glLinkProgram", "glGetProgramiv", "glUseProgram", "glGetUniformLocation",
			"glUniform1i", "glUniform1f", "glVertexAttribPointer", "glEnableVertexAttribArray", "glDrawArrays",
			"glFinish", "glFlush", "glGetError", "glEGLImageTargetTexture2DOES", "eglCreateImageKHR", "eglDestroyImageKHR", "glScissor" };
		for (TUint i = 0; i < sizeof(KNames) / sizeof(KNames[0]); i++)
			if (!Proc(f[i], KNames[i]))
				return EFalse;
		// premultiplied src-over: the source's own alpha only when SOURCE transparency is on; non-premultiplied
		// sources are premultiplied here; the global alpha scales all four channels
		const GLuint vs = Shader(KGlVertexShader,
			"attribute vec2 pos; attribute vec2 uv; varying vec2 v;\n"
			"void main() { v = uv; gl_Position = vec4(pos, 0.0, 1.0); }\n");
		const GLuint fs = Shader(KGlFragmentShader,
			"precision mediump float; varying vec2 v; uniform sampler2D tex;\n"
			"uniform float ga; uniform float useA; uniform float premul;\n"
			"void main() { vec4 c = texture2D(tex, v);\n"
			"  float a = useA > 0.5 ? c.a : 1.0;\n"
			"  vec3 rgb = (useA > 0.5 && premul < 0.5) ? c.rgb * c.a : c.rgb;\n"
			"  gl_FragColor = vec4(rgb, a) * ga; }\n");
		if (!vs || !fs)
			{
			RDebug::Printf("PH: wfc gpu: shader compile failed");
			return EFalse;
			}
		gProg = gl.CreateProgram();
		gl.AttachShader(gProg, vs);
		gl.AttachShader(gProg, fs);
		gl.BindAttribLocation(gProg, 0, "pos");
		gl.BindAttribLocation(gProg, 1, "uv");
		gl.LinkProgram(gProg);
		GLint linked = 0;
		gl.GetProgramiv(gProg, KGlLinkStatus, &linked);
		if (!linked)
			{
			RDebug::Printf("PH: wfc gpu: program link failed");
			return EFalse;
			}
		gUniTex = gl.GetUniformLocation(gProg, "tex");
		gUniGa = gl.GetUniformLocation(gProg, "ga");
		gUniUseA = gl.GetUniformLocation(gProg, "useA");
		gUniPremul = gl.GetUniformLocation(gProg, "premul");
		HAL::Get(HAL::EFastCounterFrequency, gFreq);
		gPresent = (TPresent)eglGetProcAddress("eglPresentImagePH");
		RDebug::Printf("PH: wfc gpu: GPU composition up (EGL %d.%d)", maj, min);
		return ETrue;
		}

	// OWF pixel format -> SYMGL_SGFMT_*; 0 = not importable
	TInt MemFormat(TInt aOwf)
		{
		switch (aOwf)
			{
		case 0x8888: return 1;		// OWF_IMAGE_ARGB8888
		case 0xf888: return 2;		// OWF_IMAGE_XRGB8888
		case 0x565: return 3;		// OWF_IMAGE_RGB565
		default: return 0;
			}
		}

	void Drop(TEntry& e)
		{
		if (e.iFbo)
			gl.DeleteFramebuffers(1, &e.iFbo);
		if (e.iTex)
			gl.DeleteTextures(1, &e.iTex);
		if (e.iImage)
			gl.DestroyImageKHR(gDpy, e.iImage);
		Mem::FillZ(&e, sizeof(e));
		}

	// the cached EGLImage + texture of a stream buffer (made on first use); NULL = cannot import
	TEntry* Lookup(void* aStream, void* aBuffer, void* aData, TInt aW, TInt aH, TInt aStride, TInt aOwf, TUint aUsage = 0)
		{
		khronos_int32_t index = -1;
		const TSurfaceId* sid = NULL;
		if (SymbianStreamGetBufferId((SymbianStreamType)aStream, (SymbianStreamBuffer)aBuffer, &index, &sid) != KErrNone || !sid)
			return NULL;
		const TUint32* id = (const TUint32*)sid;
		TEntry* lru = NULL;
		for (TInt i = 0; i < KEntries; i++)
			{
			TEntry& e = gEntry[i];
			if (e.iImage && e.iIndex == index && Mem::Compare((const TUint8*)e.iSid, 16, (const TUint8*)id, 16) == 0)
				{
				if (e.iData == aData && e.iW == aW && e.iH == aH && e.iStride == aStride && e.iFormat == aOwf)
					{
					e.iUsed = gComp;
					return &e;
					}
				Drop(e);		// same buffer, new layout
				}
			// the slot to fill: an empty one, else the least recently used
			if (!lru || (lru->iImage && (!e.iImage || e.iUsed < lru->iUsed)))
				lru = &e;
			}
		if (lru->iImage)
			Drop(*lru);
		TMemImage m;
		m.iBase = aData;
		m.iStride = aStride;
		m.iWidth = aW;
		m.iHeight = aH;
		m.iFormat = MemFormat(aOwf);
		m.iUsage = aUsage;		// 1 = the composition target (EGL keeps it out of the sources' clean-only cache rule)
		EGLImageKHR img = gl.CreateImageKHR(gDpy, EGL_NO_CONTEXT, EGL_SYMBIAN_MEMORY_IMAGE_PH, (EGLClientBuffer)&m, NULL);
		if (!img)
			{
			RDebug::Printf("PH: wfc gpu: no EGLImage over %08x %dx%d stride %d fmt %x (%x)", aData, aW, aH, aStride, aOwf,
				eglGetError());
			return NULL;
			}
		TEntry& e = *lru;
		Mem::Copy(e.iSid, id, 16);
		e.iIndex = index;
		e.iData = aData;
		e.iW = aW; e.iH = aH; e.iStride = aStride; e.iFormat = aOwf;
		e.iImage = img;
		e.iUsed = gComp;
		gl.GenTextures(1, &e.iTex);
		gl.BindTexture(KGlTexture2D, e.iTex);
		gl.EGLImageTargetTexture2DOES(KGlTexture2D, img);
		gl.TexParameteri(KGlTexture2D, KGlTextureWrapS, KGlClampToEdge);
		gl.TexParameteri(KGlTexture2D, KGlTextureWrapT, KGlClampToEdge);
		if (gl.GetError())
			{
			RDebug::Printf("PH: wfc gpu: texture over the EGLImage failed");
			Drop(e);
			return NULL;
			}
		return &e;
		}

	void DropIdle()
		{
		for (TInt i = 0; i < KEntries; i++)
			if (gEntry[i].iImage && gComp - gEntry[i].iUsed > KIdleCompositions)
				Drop(gEntry[i]);
		}

	// internal (pre-rotation) point -> normalised device coordinates of the target (row 0 of memory at y = -1)
	void Ndc(TReal32 aX, TReal32 aY, GLfloat* aOut)
		{
		TReal32 x = aX, y = aY;
		switch (gRot)
			{
		case 1: x = (TReal32)gIh - aY; y = aX; break;			// 90 cw
		case 2: x = (TReal32)gIw - aX; y = (TReal32)gIh - aY; break;	// 180
		case 3: x = aY; y = (TReal32)gIw - aX; break;			// 270 cw
		default: break;
			}
		aOut[0] = x * 2.0f / (TReal32)gTw - 1.0f;
		aOut[1] = y * 2.0f / (TReal32)gTh - 1.0f;
		}
	}

extern "C" int PhStreamTakeDamage(const void* aId, int* aBox);	// wfcinterop (contentupdateproxy.cpp): the region since last time
// real damage step 1 (diagnostic): the frame's damaged box in target pixels, from every element's stream damage
static TInt gDx0 = KMaxTInt, gDy0 = KMaxTInt, gDx1 = KMinTInt, gDy1 = KMinTInt;
static TUint gDmgFrames, gDmgNone, gDmgFull, gDmgPart, gDmgPartArea, gDmgUnknown;	// (42o) unknown = elements whose surface was never posted
static void DamageAdd(TInt aX0, TInt aY0, TInt aX1, TInt aY1)
	{
	gDx0 = Min(gDx0, aX0); gDy0 = Min(gDy0, aY0); gDx1 = Max(gDx1, aX1); gDy1 = Max(gDy1, aY1);
	}

extern "C" {

int PhGpu_Want(void)
	{
	if (gState == 0)
		gState = Init() ? 1 : -1;
	return gState == 1 && (TUint)RThread().Id() == gThread;
	}

int PhGpu_SourceOk(const void* aData, int aFormat, int aLinear, int aW, int aH, int aStride)
	{
	// the GPU import maps whole pages: a buffer that does not start on one (Classic's streams: header + 0x60) stays
	// on the CPU
	const TBool ok = ((TUint)aData & 0xFFF) == 0 && MemFormat(aFormat) != 0 && !aLinear && aW > 0 && aH > 0 && aW <= 4096 && aH <= 4096 && (aStride & 7) == 0;
	static TInt nlog;
	if (!ok && nlog++ < 16)
		RDebug::Printf("PH: wfc gpu: source refused: %s (data %08x format %x linear %d %dx%d stride %d)",
			((TUint)aData & 0xFFF) ? "not page aligned" : !MemFormat(aFormat) ? "format" : aLinear ? "linear" : (aStride & 7) ? "stride" : "size",
			(TUint)aData, aFormat, aLinear, aW, aH, aStride);
	return ok;
	}

int PhGpu_Begin(void* aStream, void* aBuffer, void* aPixels, int aW, int aH, int aStride, int aFormat, int aIw, int aIh,
                int aRot, unsigned aBackground)
	{
	gComp++;
	gFailed = EFalse;
	gT0 = User::FastCounter();
	if (!gCleanEnd)		// the CPU pipeline may have written a target since: none holds what we drew
		for (TInt i = 0; i < KEntries; i++)
			gEntry[i].iDrawn = EFalse;
	gCleanEnd = EFalse;
	gDraws = 0;
	gTarget = NULL;
	gSig = 2166136261u;
	if (gComp <= 3 || gComp % 300 == 0)
		RDebug::Printf("PH: wfc gpu: comp %u target %dx%d stride %d fmt %x pixels %08x internal %dx%d rot %d", gComp, aW, aH, aStride, aFormat, (TUint)aPixels, aIw, aIh, aRot);
	if (!MemFormat(aFormat) || (aStride & 7) || ((TUint)aPixels & 0xFFF))
		return 0;
	TEntry* t = Lookup(aStream, aBuffer, aPixels, aW, aH, aStride, aFormat, 1);
	gTLook = User::FastCounter();
	if (!t)
		return 0;
	gTargetImage = aRot == 0 ? t->iImage : (EGLImageKHR)0;	// (a turned target is copied turned by the display HAL)
	if (!t->iFbo)
		{
		gl.GenFramebuffers(1, &t->iFbo);
		gl.BindFramebuffer(KGlFramebuffer, t->iFbo);
		gl.FramebufferTexture2D(KGlFramebuffer, KGlColorAttachment0, KGlTexture2D, t->iTex, 0);
		const GLenum st = gl.CheckFramebufferStatus(KGlFramebuffer);
		if (st != KGlFramebufferComplete)
			{
			RDebug::Printf("PH: wfc gpu: target framebuffer incomplete (%x): the CPU composes", st);
			Drop(*t);
			gState = -1;
			return 0;
			}
		}
	if (gTw != aW || gTh != aH || gIw != aIw || gIh != aIh || gRot != (aRot & 3))
		for (TInt i = 0; i < KEntries; i++)
			gEntry[i].iDrawn = EFalse;		// a new geometry: every target is drawn whole once
	gTw = aW; gTh = aH; gIw = aIw; gIh = aIh; gRot = aRot & 3;
	gTarget = t;
	gTMark = User::FastCounter();
	gl.BindFramebuffer(KGlFramebuffer, t->iFbo);
	gl.Viewport(0, 0, aW, aH);
	{ const TUint n = User::FastCounter(); gTs[0] += n - gTMark; gTMark = n; }
	gl.Disable(KGlScissorTest);
	gl.Disable(KGlDither);
	const TReal32 a = (TReal32)(aBackground & 0xFF) / 255.0f;
	gl.ClearColor((TReal32)((aBackground >> 24) & 0xFF) / 255.0f * a, (TReal32)((aBackground >> 16) & 0xFF) / 255.0f * a,
		(TReal32)((aBackground >> 8) & 0xFF) / 255.0f * a, a);
	gClearPending = ETrue;
	gClearRgbBlack = (aBackground >> 8) == 0;
	{ const TUint n = User::FastCounter(); gTs[1] += n - gTMark; gTMark = n; }
	gl.UseProgram(gProg);
	gl.ActiveTexture(KGlTexture0);
	gl.Uniform1i(gUniTex, 0);
	gl.EnableVertexAttribArray(0);
	gl.EnableVertexAttribArray(1);
	gTBegun = User::FastCounter();
	gTs[2] += gTBegun - gTMark;
	return 1;
	}

int PhGpu_Element(void* aStream, void* aBuffer, void* aPixels, int aW, int aH, int aStride, int aFormat,
                  int aPremultiplied, const float* aSrcRect, const float* aDstRect, int aFlip, int aRot,
                  int aLinearFilter, int aSourceAlpha, float aGlobalAlpha)
	{
	const TUint el0 = User::FastCounter();
	TEntry* e = Lookup(aStream, aBuffer, aPixels, aW, aH, aStride, aFormat);
	gTElLook += User::FastCounter() - el0;
	if (!e)
		{
		gFailed = ETrue;
		return 0;
		}
	{	// real damage step 1: this element's source damage, mapped to the target (rotated / flipped: its whole rect)
	int box[4];
	const int k = PhStreamTakeDamage(e->iSid, box);	// the surface id (the proxy keys posts by TSurfaceId), not the stream handle: 42o showed every lookup missing
	const TReal32 dx = aDstRect[0], dy = aDstRect[1], dw = aDstRect[2], dh = aDstRect[3];
	if (k == 3)
		gDmgUnknown++;
	if (k >= 2 || aRot || aFlip || aSrcRect[2] <= 0.0f || aSrcRect[3] <= 0.0f)
		DamageAdd((TInt)dx, (TInt)dy, (TInt)(dx + dw + 0.999f), (TInt)(dy + dh + 0.999f));
	else if (k == 1)
		{
		const TReal32 fx = dw / aSrcRect[2], fy = dh / aSrcRect[3];
		DamageAdd((TInt)(dx + (box[0] - aSrcRect[0]) * fx), (TInt)(dy + (box[1] - aSrcRect[1]) * fy),
			(TInt)(dx + (box[2] - aSrcRect[0]) * fx + 0.999f), (TInt)(dy + (box[3] - aSrcRect[1]) * fy + 0.999f));
		}
	}
	gElems++;
	if (gComp <= 3 || gComp % 300 == 0)
		RDebug::Printf("PH: wfc gpu: comp %u element %ux%u stride %d fmt %x pixels %08x src %d,%d %dx%d dst %d,%d %dx%d flip %d rot %d srcA %d ga %d",
			gComp, aW, aH, aStride, aFormat, (TUint)aPixels, (TInt)aSrcRect[0], (TInt)aSrcRect[1], (TInt)aSrcRect[2], (TInt)aSrcRect[3],
			(TInt)aDstRect[0], (TInt)aDstRect[1], (TInt)aDstRect[2], (TInt)aDstRect[3], aFlip, aRot, aSourceAlpha, (TInt)(aGlobalAlpha * 100));
	// source corners TL TR BR BL (source pixels), flipped vertically, then turned clockwise: each quarter turn moves
	// the bottom-left corner to the top-left
	const TReal32 sx = aSrcRect[0], sy = aSrcRect[1], sw = aSrcRect[2], sh = aSrcRect[3];
	TReal32 s[4][2] = { { sx, sy }, { sx + sw, sy }, { sx + sw, sy + sh }, { sx, sy + sh } };
	if (aFlip)
		{
		TReal32 f[4][2] = { { s[3][0], s[3][1] }, { s[2][0], s[2][1] }, { s[1][0], s[1][1] }, { s[0][0], s[0][1] } };
		Mem::Copy(s, f, sizeof(s));
		}
	for (TInt k = 0; k < (aRot & 3); k++)
		{
		TReal32 r[4][2] = { { s[3][0], s[3][1] }, { s[0][0], s[0][1] }, { s[1][0], s[1][1] }, { s[2][0], s[2][1] } };
		Mem::Copy(s, r, sizeof(s));
		}
	const TReal32 dx = aDstRect[0], dy = aDstRect[1], dw = aDstRect[2], dh = aDstRect[3];
	const TReal32 d[4][2] = { { dx, dy }, { dx + dw, dy }, { dx + dw, dy + dh }, { dx, dy + dh } };
	GLfloat p[4][2], uv[4][2];
	for (TInt i = 0; i < 4; i++)
		{
		Ndc(d[i][0], d[i][1], p[i]);
		uv[i][0] = s[i][0] / (TReal32)aW;
		uv[i][1] = s[i][1] / (TReal32)aH;		// t = 0 is memory row 0 (an EGLImage sampled as a texture)
		}
	static const TInt KTri[6] = { 0, 1, 2, 0, 2, 3 };
	GLfloat pv[12], uvv[12];
	for (TInt i = 0; i < 6; i++)
		{
		pv[i * 2] = p[KTri[i]][0]; pv[i * 2 + 1] = p[KTri[i]][1];
		uvv[i * 2] = uv[KTri[i]][0]; uvv[i * 2 + 1] = uv[KTri[i]][1];
		}
	// (round 42q) recorded, drawn in PhGpu_End once the frame's damage is known
	if (gDraws >= KMaxDraws)
		{
		gFailed = ETrue;		// more layers than recorded: the CPU pipeline composes this frame
		return 0;
		}
	TDraw& dr = gDraw[gDraws++];
	dr.iTex = e->iTex;
	dr.iFilt = aLinearFilter ? KGlLinear : KGlNearest;
	dr.iUseA = aSourceAlpha && aFormat == 0x8888;
	dr.iGa = aGlobalAlpha;
	dr.iPremul = aPremultiplied ? 1.0f : 0.0f;
	Mem::Copy(dr.iDst, aDstRect, sizeof(dr.iDst));
	Mem::Copy(dr.iPv, pv, sizeof(pv));
	Mem::Copy(dr.iUv, uvv, sizeof(uvv));
	{	// the layout signature (FNV-1a): a layer added, removed, moved or faded posts no damage of its own
	const TUint32 words[] = { (TUint32)(TUint)aStream, (TUint32)aFlip, (TUint32)aRot, (TUint32)aSourceAlpha, (TUint32)aPremultiplied,
		(TUint32)aLinearFilter, (TUint32)(aGlobalAlpha * 255.0f), (TUint32)aSrcRect[0], (TUint32)aSrcRect[1], (TUint32)aSrcRect[2],
		(TUint32)aSrcRect[3], (TUint32)aDstRect[0], (TUint32)aDstRect[1], (TUint32)aDstRect[2], (TUint32)aDstRect[3] };
	for (TUint i = 0; i < sizeof(words) / sizeof(words[0]); i++)
		gSig = (gSig ^ words[i]) * 16777619u;
	}
	gTElems = User::FastCounter();
	return 1;
	}

// (round 42q) one recorded layer, as PhGpu_Element drew it before (the clear waits for the first layer: one that covers
// the whole target stands in for it)
static void DrawOne(const TDraw& aD)
	{
	gTMark = User::FastCounter();
	gl.BindTexture(KGlTexture2D, aD.iTex);
	{ const TUint n = User::FastCounter(); gTs[3] += n - gTMark; gTMark = n; }
	gl.TexParameteri(KGlTexture2D, KGlTextureMinFilter, aD.iFilt);
	gl.TexParameteri(KGlTexture2D, KGlTextureMagFilter, aD.iFilt);
	{ const TUint n = User::FastCounter(); gTs[4] += n - gTMark; gTMark = n; }
	TBool coverDraw = EFalse;
	if (gClearPending)
		{
		gClearPending = EFalse;
		// an opaque layer over the whole target hides the clear colour (in svga a clear of the target costs 2-7 ms)
		// (the destination rect is in the INTERNAL, pre-rotation scene: gIw x gIh, not the doubled/rotated target)
		// a per-pixel-alpha layer covers too when the clear colour is black: premultiplied src-over onto black = the
		// source's rgb, so the layer drawn UNBLENDED leaves the same rgb (only the target's alpha differs; scanout ignores it)
		const TBool covers = (!aD.iUseA || gClearRgbBlack) && aD.iGa >= 1.0f && aD.iDst[0] <= 0.0f && aD.iDst[1] <= 0.0f &&
			aD.iDst[0] + aD.iDst[2] >= (TReal32)gIw && aD.iDst[1] + aD.iDst[3] >= (TReal32)gIh;
		if (covers)
			{
			gClearSkipped++;
			coverDraw = ETrue;
			}
		else
			{
			const TUint c0 = User::FastCounter();
			gl.Clear(KGlColorBufferBit);
			gTs[1] += User::FastCounter() - c0;
			}
		}
	if (!coverDraw && (aD.iUseA || aD.iGa < 1.0f))
		{
		gl.Enable(KGlBlend);
		gl.BlendFunc(KGlOne, KGlOneMinusSrcAlpha);
		}
	else
		gl.Disable(KGlBlend);
	gl.Uniform1f(gUniGa, aD.iGa);
	gl.Uniform1f(gUniUseA, aD.iUseA ? 1.0f : 0.0f);
	gl.Uniform1f(gUniPremul, aD.iPremul);
	{ const TUint n = User::FastCounter(); gTs[5] += n - gTMark; gTMark = n; }
	gl.VertexAttribPointer(0, 2, KGlFloat, 0, 0, aD.iPv);
	gl.VertexAttribPointer(1, 2, KGlFloat, 0, 0, aD.iUv);
	{ const TUint n = User::FastCounter(); gTs[6] += n - gTMark; gTMark = n; }
	gl.DrawArrays(KGlTriangles, 0, 6);
	gTs[7] += User::FastCounter() - gTMark;
	}

int PhGpu_End(void)
	{
	// submit + wait for the job (glFinish: eglWaitClient never reaches the driver from this thread -- Mesa sees no
	// current EGL context here -- and returned with the job still running). The CPU cache maintenance on both sides
	// rides on the job itself (drm_symbian.c sym_drm_sync_hook): sources cleaned ahead of the submit, the target
	// invalidated when it finished.
	{	// (round 42q) damage-only redraw: this frame's damage (all of it when the layout changed), plus what this target
		// buffer missed while the others were drawn (double/triple buffering), as one box in the internal scene
	if (gSig != gLastSig)
		DamageAdd(0, 0, gIw, gIh);
	gLastSig = gSig;
	const TInt fx0 = Max(gDx0, 0), fy0 = Max(gDy0, 0), fx1 = Min(gDx1, gIw), fy1 = Min(gDy1, gIh);
	const TBool frameDamaged = fx0 < fx1 && fy0 < fy1;
	TInt x0 = fx0, y0 = fy0, x1 = fx1, y1 = fy1;
	if (!gTarget || !gTarget->iDrawn || gRot)		// (a turned target: whole frames, for now)
		{ x0 = 0; y0 = 0; x1 = gIw; y1 = gIh; }
	else if (gTarget->iPx0 < gTarget->iPx1 && gTarget->iPy0 < gTarget->iPy1)
		{
		if (!frameDamaged)
			{ x0 = gTarget->iPx0; y0 = gTarget->iPy0; x1 = gTarget->iPx1; y1 = gTarget->iPy1; }
		else
			{ x0 = Min(x0, gTarget->iPx0); y0 = Min(y0, gTarget->iPy0); x1 = Max(x1, gTarget->iPx1); y1 = Max(y1, gTarget->iPy1); }
		}
	for (TInt i = 0; frameDamaged && i < KEntries; i++)		// the other targets miss this frame's damage
		{
		TEntry& o = gEntry[i];
		if (&o == gTarget || !o.iDrawn || !o.iFbo)
			continue;
		if (o.iPx0 < o.iPx1 && o.iPy0 < o.iPy1)
			{ o.iPx0 = Min(o.iPx0, fx0); o.iPy0 = Min(o.iPy0, fy0); o.iPx1 = Max(o.iPx1, fx1); o.iPy1 = Max(o.iPy1, fy1); }
		else
			{ o.iPx0 = fx0; o.iPy0 = fy0; o.iPx1 = fx1; o.iPy1 = fy1; }
		}
	if (gTarget)
		{
		gTarget->iDrawn = ETrue;
		gTarget->iPx0 = gTarget->iPy0 = gTarget->iPx1 = gTarget->iPy1 = 0;
		}
	const TBool full = x0 == 0 && y0 == 0 && x1 == gIw && y1 == gIh;
	if (x0 < x1 && y0 < y1)
		{
		if (!full)
			{	// unturned: the internal scene maps onto the target scaled, y up from row 0 (Ndc)
			const TInt sx0 = x0 * gTw / gIw, sy0 = y0 * gTh / gIh, sx1 = (x1 * gTw + gIw - 1) / gIw, sy1 = (y1 * gTh + gIh - 1) / gIh;
			gl.Enable(KGlScissorTest);
			gl.Scissor(sx0, sy0, sx1 - sx0, sy1 - sy0);
			}
		for (TInt i = 0; i < gDraws; i++)
			DrawOne(gDraw[i]);
		if (gClearPending)
			{
			gClearPending = EFalse;
			gl.Clear(KGlColorBufferBit);
			}
		gl.Disable(KGlScissorTest);
		}
	gClearPending = EFalse;
	gDraws = 0;
	gTElems = User::FastCounter();
	// the counts below: the box actually drawn
	gDx0 = x0; gDy0 = y0; gDx1 = x1; gDy1 = y1;
	}
	{	// real damage step 1: how much of each frame was damaged (every 60 frames; the payoff of damage-only updates)
	const TInt x0 = Max(gDx0, 0), y0 = Max(gDy0, 0), x1 = Min(gDx1, gIw), y1 = Min(gDy1, gIh);
	if (x0 >= x1 || y0 >= y1)
		gDmgNone++;
	else if (x0 == 0 && y0 == 0 && x1 == gIw && y1 == gIh)
		gDmgFull++;
	else
		{
		gDmgPart++;
		gDmgPartArea += (TUint)((TInt64)(x1 - x0) * (y1 - y0) * 100 / ((TInt64)gIw * gIh));
		}
	gDx0 = gDy0 = KMaxTInt; gDx1 = gDy1 = KMinTInt;
	if (++gDmgFrames == 60)
		{
		RDebug::Printf("PH: [dmg] x60: none %u, partial %u (avg %u%% of the target), full %u", gDmgNone, gDmgPart,
			gDmgPart ? gDmgPartArea / gDmgPart : 0, gDmgFull);
		gDmgFrames = gDmgNone = gDmgFull = gDmgPart = gDmgPartArea = 0;
		}
	}
	const TUint w0 = User::FastCounter();
	// (round 42m) a present through Mesa (svga: eglPresentImagePH) is queued on the same context after the draws, so
	// a flush orders it; only a target the CPU or another device reads next (lima) needs the finish
	if (gPresent && gTargetImage != (EGLImageKHR)0)
		gl.Flush();
	else
		gl.Finish();
	const TUint w1 = User::FastCounter();
	const GLenum err = gl.GetError();
	DropIdle();
	const TUint dt = User::FastCounter() - gT0;
	gTicks += dt;
	gFrames++;
	if (gComp <= 40 || gComp % 300 == 0)		// bring-up trace: the first compositions, then a sample
		RDebug::Printf("PH: wfc gpu: composition %u: %u elements so far, %u us (GPU wait %u us)", gComp, gElems,
			gFreq ? (TUint)((TUint64)dt * 1000000 / gFreq) : 0, gFreq ? (TUint)((TUint64)(w1 - w0) * 1000000 / gFreq) : 0);
	if (gFreq && (gComp <= 40 || gComp % 60 == 0))	// round 41: where the time goes (target lookup / setup / element lookups / draws / submit+wait / end)
		{
		const TUint64 f = gFreq;
		RDebug::Printf("PH: wfc gpu: split %u: look %u setup %u ellook %u draw %u finish %u total %u us", gComp,
			(TUint)((TUint64)(gTLook - gT0) * 1000000 / f), (TUint)((TUint64)(gTBegun - gTLook) * 1000000 / f),
			(TUint)((TUint64)gTElLook * 1000000 / f), (TUint)((TUint64)(gTElems - gTBegun - gTElLook) * 1000000 / f),
			(TUint)((TUint64)(w1 - w0) * 1000000 / f), (TUint)((TUint64)dt * 1000000 / f));
		}
	gTElLook = 0;
	if (gFreq && (gComp <= 40 || gComp % 60 == 0))
		{
		TUint u[9];
		for (TInt i = 0; i < 9; i++)
			u[i] = (TUint)((TUint64)gTs[i] * 1000000 / gFreq);
		RDebug::Printf("PH: wfc gpu: calls %u: fbo %u clear %u (skipped %u) prog %u | bindtex %u texparam %u uniforms %u attribs %u draw %u us | pre-wait %u us", gComp,
			u[0], u[1], gClearSkipped, u[2], u[3], u[4], u[5], u[6], u[7], u[8]);
		}
	Mem::FillZ(gTs, sizeof(gTs));
	if (err || gFailed)
		{
		static TInt said;
		if (said++ < 8)
			RDebug::Printf("PH: wfc gpu: composition %u failed (GL error %x%s)", gComp, err, gFailed ? ", an element" : "");
		}
	if (gFrames == 600)
		{
		RDebug::Printf("PH: wfc gpu: 600 compositions, %u elements, avg %u us each", gElems,
			gFreq ? (TUint)((TUint64)gTicks * 1000000 / gFreq / gFrames) : 0);
		gFrames = 0;
		gTicks = 0;
		gElems = 0;
		}
	gCleanEnd = !(err || gFailed);	// (round 42q) else the CPU pipeline composes this frame into the target
	if (err || gFailed)
		return 0;
	// the target straight to the display (svga screen target): no read-back, no copy by the display HAL. 2 = shown
	if (gPresent && gTargetImage != (EGLImageKHR)0)
		{
		const TBool shown = gPresent(gDpy, gTargetImage);
		shown ? ++gShown : ++gNotShown;
		if (gShown + gNotShown <= 3 || (gShown + gNotShown) % 600 == 0)
			RDebug::Printf("PH: wfc gpu: present: %u frames shown as the screen target, %u copied", gShown, gNotShown);
		if (shown)
			return 2;
		}
	return 1;
	}

}	// extern "C"
