// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phgpucompose.cpp -- OURS (GPU plan G8): libWFC's scene composed by the GPU (GLES2 on the Mali through Mesa) instead of
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
// Declined (the CPU composes): masks, linear-light / luminance sources, a scene the GPU cannot import, x86 (no Mali:
// EGL does not come up), the developer menu switch off (repository 0xE7015A01 key 9).
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
		GLenum (*GetError)();
		void (*EGLImageTargetTexture2DOES)(GLenum, GLeglImageOES);
		EGLImageKHR (*CreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
		EGLBoolean (*DestroyImageKHR)(EGLDisplay, EGLImageKHR);
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

	// developer menu key 9 "GPU composition" (1 = on, the default; 0 = the 2010 CPU pipeline)
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
		if (!Setting())
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
		void** f = (void**)&gl;
		static const char* const KNames[] = { "glGenTextures", "glDeleteTextures", "glBindTexture", "glActiveTexture",
			"glTexParameteri", "glGenFramebuffers", "glDeleteFramebuffers", "glBindFramebuffer", "glFramebufferTexture2D",
			"glCheckFramebufferStatus", "glViewport", "glClearColor", "glClear", "glEnable", "glDisable", "glBlendFunc",
			"glCreateShader", "glShaderSource", "glCompileShader", "glGetShaderiv", "glCreateProgram", "glAttachShader",
			"glBindAttribLocation", "glLinkProgram", "glGetProgramiv", "glUseProgram", "glGetUniformLocation",
			"glUniform1i", "glUniform1f", "glVertexAttribPointer", "glEnableVertexAttribArray", "glDrawArrays",
			"glFinish", "glGetError", "glEGLImageTargetTexture2DOES", "eglCreateImageKHR", "eglDestroyImageKHR" };
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

extern "C" {

int PhGpu_Want(void)
	{
	if (gState == 0)
		gState = Init() ? 1 : -1;
	return gState == 1 && (TUint)RThread().Id() == gThread;
	}

int PhGpu_SourceOk(const void* aData, int aFormat, int aLinear, int aW, int aH, int aStride)
	{
	// the Mali import maps whole pages: a buffer that does not start on one (Classic's streams: header + 0x60) stays
	// on the CPU
	return ((TUint)aData & 0xFFF) == 0 && MemFormat(aFormat) != 0 && !aLinear && aW > 0 && aH > 0 && aW <= 4096 && aH <= 4096 && (aStride & 7) == 0;
	}

int PhGpu_Begin(void* aStream, void* aBuffer, void* aPixels, int aW, int aH, int aStride, int aFormat, int aIw, int aIh,
                int aRot, unsigned aBackground)
	{
	gComp++;
	gFailed = EFalse;
	gT0 = User::FastCounter();
	if (gComp <= 3 || gComp % 300 == 0)
		RDebug::Printf("PH: wfc gpu: comp %u target %dx%d stride %d fmt %x pixels %08x internal %dx%d rot %d", gComp, aW, aH, aStride, aFormat, (TUint)aPixels, aIw, aIh, aRot);
	if (!MemFormat(aFormat) || (aStride & 7) || ((TUint)aPixels & 0xFFF))
		return 0;
	TEntry* t = Lookup(aStream, aBuffer, aPixels, aW, aH, aStride, aFormat, 1);
	if (!t)
		return 0;
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
	gTw = aW; gTh = aH; gIw = aIw; gIh = aIh; gRot = aRot & 3;
	gl.BindFramebuffer(KGlFramebuffer, t->iFbo);
	gl.Viewport(0, 0, aW, aH);
	gl.Disable(KGlScissorTest);
	gl.Disable(KGlDither);
	const TReal32 a = (TReal32)(aBackground & 0xFF) / 255.0f;
	gl.ClearColor((TReal32)((aBackground >> 24) & 0xFF) / 255.0f * a, (TReal32)((aBackground >> 16) & 0xFF) / 255.0f * a,
		(TReal32)((aBackground >> 8) & 0xFF) / 255.0f * a, a);
	gl.Clear(KGlColorBufferBit);
	gl.UseProgram(gProg);
	gl.ActiveTexture(KGlTexture0);
	gl.Uniform1i(gUniTex, 0);
	gl.EnableVertexAttribArray(0);
	gl.EnableVertexAttribArray(1);
	return 1;
	}

int PhGpu_Element(void* aStream, void* aBuffer, void* aPixels, int aW, int aH, int aStride, int aFormat,
                  int aPremultiplied, const float* aSrcRect, const float* aDstRect, int aFlip, int aRot,
                  int aLinearFilter, int aSourceAlpha, float aGlobalAlpha)
	{
	TEntry* e = Lookup(aStream, aBuffer, aPixels, aW, aH, aStride, aFormat);
	if (!e)
		{
		gFailed = ETrue;
		return 0;
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
	gl.BindTexture(KGlTexture2D, e->iTex);
	const GLint filt = aLinearFilter ? KGlLinear : KGlNearest;
	gl.TexParameteri(KGlTexture2D, KGlTextureMinFilter, filt);
	gl.TexParameteri(KGlTexture2D, KGlTextureMagFilter, filt);
	const TBool useA = aSourceAlpha && aFormat == 0x8888;
	if (useA || aGlobalAlpha < 1.0f)
		{
		gl.Enable(KGlBlend);
		gl.BlendFunc(KGlOne, KGlOneMinusSrcAlpha);
		}
	else
		gl.Disable(KGlBlend);
	gl.Uniform1f(gUniGa, aGlobalAlpha);
	gl.Uniform1f(gUniUseA, useA ? 1.0f : 0.0f);
	gl.Uniform1f(gUniPremul, aPremultiplied ? 1.0f : 0.0f);
	gl.VertexAttribPointer(0, 2, KGlFloat, 0, 0, pv);
	gl.VertexAttribPointer(1, 2, KGlFloat, 0, 0, uvv);
	gl.DrawArrays(KGlTriangles, 0, 6);
	return 1;
	}

int PhGpu_End(void)
	{
	// submit + wait for the job (glFinish: eglWaitClient never reaches the driver from this thread -- Mesa sees no
	// current EGL context here -- and returned with the job still running). The CPU cache maintenance on both sides
	// rides on the job itself (drm_symbian.c sym_drm_sync_hook): sources cleaned ahead of the submit, the target
	// invalidated when it finished.
	const TUint w0 = User::FastCounter();
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
	return !(err || gFailed);
	}

}	// extern "C"
