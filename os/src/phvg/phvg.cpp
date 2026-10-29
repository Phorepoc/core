// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phvg.cpp -- OURS: "phvg", the OpenVG-on-GLES2 engine (milestone O2).
//
// What it is: a small GPU back end for the Khronos reference implementation. The RI keeps the whole OpenVG API, the
// state machine and the path geometry (flattening, transform); for what phvg implements it hands over the already
// flattened, transformed edge list instead of rasterising it on the CPU. phvg draws it with GLES 2.0 (the GL implementation
// through the frozen libEGL + libGLESv2 fronts) into a GPU "shadow" of the VG surface, and gives the pixels back when
// the CPU side needs them. Implemented: vgClear (no scissor) and solid-colour fills (non-zero and even-odd, SRC_OVER
// and SRC). Everything else stays in the reference implementation (see riPhvg.cpp for how the two compose).
//
// Fill technique -- stencil then cover, in the RI's own crossing-count semantics. The RI rasteriser counts, for a
// sample q, the signed non-horizontal edges that cross q's scanline to the RIGHT of q. Every edge becomes a
// trapezoid between the vertical line x = -1 and the edge (same y span); a trapezoid covers q exactly when the edge
// is right of q. Two-sided stencil (front INCR_WRAP, back DECR_WRAP; the vertex order carries the edge direction)
// accumulates the signed count (non-zero), GL_INVERT the parity (even-odd). A cover rectangle then paints the colour
// where stencil != 0 and zeroes the stencil again. No pivot point, no closing edges, horizontal edges are irrelevant,
// exactly like the CPU. Edges are clipped to the surface rows and their x clamped to [-1, W+1] (which preserves "is
// the edge left or right of q" for every q inside the surface).
//
// Antialiasing: the shadow is rendered at S x S the surface resolution (S = 4 up to 256x256, else 2) and box
// filtered down by GPU bilinear halving passes on readback. (mobile GPUs: fragment shaders may be FP16 -- the colour here is
// a uniform, nothing is computed per fragment; the only per-fragment work is the stencil test. 8-bit stencil, wrap ops.)
//
// Threading: GLES lives in ONE worker thread of the process with its own context. The front keeps a per-thread
// "bound API"; a VG client thread is bound to EGL_OPENVG_API and must stay so, so it never touches GL itself. The
// RI thread marshals a command (synchronously, under a mutex) and blocks until the worker is done.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32atomics.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "phvg.h"

#ifndef GL_DEPTH24_STENCIL8_OES
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#endif
#define EGL_SYMBIAN_MEMORY_IMAGE_PH 0x3FF1		// Mesa egl_symbian.c: the client buffer is a symgl_sgimage (TMemImage below)
#ifndef EGL_KHR_image
typedef void* EGLImageKHR;
#endif

namespace
	{
	const TInt KMaxShadows = 24;
	const TInt KMaxSide = 1024;				// larger surfaces are declined (the RI renders them)
	const TInt KBatchPieces = 1024;			// trapezoids per draw call

	struct TShadow
		{
		TAny*	iCookie;
		TInt	iW, iH, iS;
		TInt	iState;						// 0 stale/none, 1 valid == CPU, 2 GPU ahead
		TInt	iResolved;					// 1 = iTexR holds the 1x resolve of the current GPU content (O3)
		TInt	iDx0, iDy0, iDx1, iDy1;		// dirty pixels while state 2
		GLuint	iTex0, iFbo0, iRbDs;		// S x S colour + stencil
		GLuint	iTexM, iFboM;				// S/2 (only S == 4)
		GLuint	iTexR, iFboR;				// 1x resolve + upload staging
		GLuint	iRbRd, iFboRd;				// 1x RENDERBUFFER the resolve passes write and glReadPixels reads (a texture may be tiled by the GPU; a renderbuffer is linear = direct rows)
		};

	TShadow			gSh[KMaxShadows];
	TInt			gLast = 0;				// lookup cache
	volatile TInt	gLive = 0;				// shadows with iState != 0
	TInt			gStat[12];
	TInt			gUp = 0;				// 0 not started, 1 up, -1 failed

	// ---- the worker thread ---------------------------------------------------------------------------------------
	enum { EOpInit, EOpUpload, EOpFill, EOpClear, EOpReadback, EOpForget, EOpReadDirect, EOpImage, EOpNop, EOpPresent };
	struct TCmd
		{
		TInt			iOp;
		TAny*			iCookie;
		TInt			iW, iH, iX, iY, iCw, iCh;
		const TUint8*	iIn;
		TUint8*			iOut;
		const TPhvgFill* iFill;
		const TReal32*	iCol;
		const TInt*		iRect;
		const TPhvgImage* iImg;
		TInt			iResult;
		};
	TCmd			gCmd;
	TUint			gRingWaitTicks;		// ticks the worker spent draining the image ring before the current synchronous command
	TInt			gLockH, gReqH, gDoneH;		// handles (no global handle objects: no static constructors)

	// GL objects of the worker
	EGLDisplay		gDpy = EGL_NO_DISPLAY;
	GLuint			gProgFlat, gProgTex, gProgLin, gProgRad, gProgImg;
	GLint			gImgScale, gImgM0, gImgM1, gImgInv, gImgMul, gImgSampler;	// O4b
	GLint			gFlatScale, gFlatColor, gTexScale, gTexInv, gTexSampler;
	GLuint			gProgFlip;											// the present pass: a texture copied upside down
	GLint			gFlipSampler;
	GLint			gLinScale, gLinG, gLinRamp;							// O4: linear gradient program
	GLint			gRadScale, gRadM0, gRadM1, gRadFocal, gRadD, gRadRamp;	// O4: radial gradient program
	GLuint			gRampTex;											// KPhvgRamp x 1 colour ramp
	TUint8			gRampCache[KPhvgRamp * 4];
	TBool			gRampValid;
	TBool			gPacked;
	TReal32*		gVtx;					// KBatchPieces * 12 floats

	const char* const KVsFlat =
		"attribute vec2 a_pos;\n"
		"uniform vec2 u_scale;\n"
		"void main() { gl_Position = vec4(a_pos * u_scale - vec2(1.0, 1.0), 0.0, 1.0); }\n";
	const char* const KFsFlat =
		"precision mediump float;\n"
		"uniform vec4 u_color;\n"
		"void main() { gl_FragColor = u_color; }\n";
	const char* const KVsTex =
		"attribute vec2 a_pos;\n"
		"uniform vec2 u_scale;\n"
		"uniform vec2 u_inv;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_Position = vec4(a_pos * u_scale - vec2(1.0, 1.0), 0.0, 1.0); v_uv = a_pos * u_inv; }\n";
	const char* const KFsTex =
		"precision mediump float;\n"
		"uniform sampler2D u_tex;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_FragColor = texture2D(u_tex, v_uv); }\n";
	// the present pass: a unit quad over the whole target, the texture sampled upside down (memory row 0 of the target is
	// the surface's TOP row; GL's y = 0 is memory row 0 of a render target, the shadow's y = 0 is the surface's bottom)
	const char* const KVsFlip =
		"attribute vec2 a_pos;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_Position = vec4(a_pos * 2.0 - vec2(1.0, 1.0), 0.0, 1.0); v_uv = vec2(a_pos.x, 1.0 - a_pos.y); }\n";

	// O4b: image draw. The vertex shader maps image-space corners through the affine image->surface matrix (fp32 on the
	// vertex stage) and passes the texel position straight to the texture fetch.
	const char* const KVsImg =
		"attribute vec2 a_pos;\n"
		"uniform vec2 u_scale;\n"
		"uniform vec3 u_m0;\n"
		"uniform vec3 u_m1;\n"
		"uniform vec2 u_inv;\n"
		"varying vec2 v_uv;\n"
		"void main() { vec3 q = vec3(a_pos, 1.0);\n"
		"  vec2 s = vec2(dot(u_m0, q), dot(u_m1, q));\n"
		"  gl_Position = vec4(s * u_scale - vec2(1.0, 1.0), 0.0, 1.0); v_uv = a_pos * u_inv; }\n";
	const char* const KFsImg =
		"precision mediump float;\n"
		"uniform sampler2D u_tex;\n"
		"uniform vec4 u_mul;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_FragColor = texture2D(u_tex, v_uv) * u_mul; }\n";

	// O4 gradients. The gradient coordinate is an AFFINE function of the surface position, so the vertex shader (fp32 on
	// the vertex stage) evaluates it exactly and the rasteriser interpolates it; the fragment shader (fp16)
	// only samples the ramp. Linear: the interpolated coordinate goes straight into the texture fetch (full-precision
	// path), the spread mode is the texture wrap mode (pad = clamp, repeat = repeat, reflect = mirrored repeat) so there is
	// no fragment arithmetic on g at all. Radial: the paint-space point is relative to the centre in units of the radius
	// (magnitudes ~1), the RI's closed form g = (fp.d + sqrt(|d|^2 - (p x fp)^2)) * D runs in the fragment shader.
	const char* const KVsLin =
		"attribute vec2 a_pos;\n"
		"uniform vec2 u_scale;\n"
		"uniform vec3 u_g;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_Position = vec4(a_pos * u_scale - vec2(1.0, 1.0), 0.0, 1.0);\n"
		"  v_uv = vec2(dot(u_g.xy, a_pos) + u_g.z, 0.5); }\n";
	const char* const KFsLin =
		"precision mediump float;\n"
		"uniform sampler2D u_ramp;\n"
		"varying vec2 v_uv;\n"
		"void main() { gl_FragColor = texture2D(u_ramp, v_uv); }\n";
	const char* const KVsRad =
		"attribute vec2 a_pos;\n"
		"uniform vec2 u_scale;\n"
		"uniform vec3 u_m0;\n"
		"uniform vec3 u_m1;\n"
		"varying vec2 v_p;\n"
		"void main() { gl_Position = vec4(a_pos * u_scale - vec2(1.0, 1.0), 0.0, 1.0);\n"
		"  vec3 q = vec3(a_pos, 1.0);\n"
		"  v_p = vec2(dot(u_m0, q), dot(u_m1, q)); }\n";
	const char* const KFsRad =
		"precision mediump float;\n"
		"uniform sampler2D u_ramp;\n"
		"uniform vec2 u_focal;\n"
		"uniform float u_d;\n"
		"varying vec2 v_p;\n"
		"void main() {\n"
		"  vec2 d = v_p - u_focal;\n"
		"  float cr = v_p.x * u_focal.y - v_p.y * u_focal.x;\n"
		"  float s = sqrt(max(dot(d, d) - cr * cr, 0.0));\n"
		"  float g = (dot(u_focal, d) + s) * u_d;\n"
		"  gl_FragColor = texture2D(u_ramp, vec2(g, 0.5)); }\n";

	void Log(const char* aMsg, TInt a = 0, TInt b = 0)
		{
		RDebug::Printf("PH: vg2 phvg: %s %d %d", aMsg, a, b);
		}

	GLuint Compile(GLenum aType, const char* aSrc)
		{
		GLuint s = glCreateShader(aType);
		glShaderSource(s, 1, &aSrc, NULL);
		glCompileShader(s);
		GLint ok = 0;
		glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
		if (!ok)
			{
			char log[160];
			GLsizei n = 0;
			glGetShaderInfoLog(s, sizeof(log) - 1, &n, log);
			log[n < 0 ? 0 : n] = 0;
			RDebug::Printf("PH: vg2 phvg: shader compile failed: %s", log);
			return 0;
			}
		return s;
		}

	GLuint Link(const char* aVs, const char* aFs)
		{
		GLuint vs = Compile(GL_VERTEX_SHADER, aVs), fs = Compile(GL_FRAGMENT_SHADER, aFs);
		if (!vs || !fs)
			return 0;
		GLuint p = glCreateProgram();
		glAttachShader(p, vs);
		glAttachShader(p, fs);
		glBindAttribLocation(p, 0, "a_pos");
		glLinkProgram(p);
		GLint ok = 0;
		glGetProgramiv(p, GL_LINK_STATUS, &ok);
		if (!ok)
			{
			Log("program link failed");
			return 0;
			}
		return p;
		}

	TBool HasExt(const char* aName)
		{
		const char* e = (const char*)glGetString(GL_EXTENSIONS);
		if (!e)
			return EFalse;
		TInt n = 0;
		while (aName[n])
			n++;
		for (; *e; )
			{
			TInt k = 0;
			while (k < n && e[k] == aName[k])
				k++;
			if (k == n && (e[n] == ' ' || e[n] == 0))
				return ETrue;
			while (*e && *e != ' ')
				e++;
			while (*e == ' ')
				e++;
			}
		return EFalse;
		}

	TBool GlInit()
		{
		gDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		EGLint maj = 0, min = 0;
		if (!eglInitialize(gDpy, &maj, &min))
			{
			Log("eglInitialize failed", eglGetError());
			return EFalse;
			}
		eglBindAPI(EGL_OPENGL_ES_API);
		const EGLint cfgAttr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
		EGLConfig cfg = 0;
		EGLint n = 0;
		if (!eglChooseConfig(gDpy, cfgAttr, &cfg, 1, &n) || n < 1)
			{
			Log("eglChooseConfig failed", eglGetError());
			return EFalse;
			}
		const EGLint ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
		EGLContext ctx = eglCreateContext(gDpy, cfg, EGL_NO_CONTEXT, ctxAttr);
		if (ctx == EGL_NO_CONTEXT)
			{
			Log("eglCreateContext failed", eglGetError());
			return EFalse;
			}
		TBool cur = eglMakeCurrent(gDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx);	// EGL_KHR_surfaceless_context
		if (!cur)
			{
			// no surfaceless contexts: a small pbuffer to make the context current on
			const EGLint pbCfg[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
			EGLConfig pc = 0;
			EGLint pn = 0;
			if (eglChooseConfig(gDpy, pbCfg, &pc, 1, &pn) && pn > 0)
				{
				const EGLint pa[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
				EGLSurface ps = eglCreatePbufferSurface(gDpy, pc, pa);
				if (ps != EGL_NO_SURFACE)
					cur = eglMakeCurrent(gDpy, ps, ps, ctx);
				}
			}
		if (!cur)
			{
			Log("no way to make a GLES2 context current", eglGetError());
			return EFalse;
			}
		gPacked = HasExt("GL_OES_packed_depth_stencil");
		gProgFlat = Link(KVsFlat, KFsFlat);
		gProgTex = Link(KVsTex, KFsTex);
		if (!gProgFlat || !gProgTex)
			return EFalse;
		gProgFlip = Link(KVsFlip, KFsTex);		// optional: without it a present is declined (the readback path)
		if (gProgFlip)
			gFlipSampler = glGetUniformLocation(gProgFlip, "u_tex");
		// the gradient programs are optional: a machine that cannot compile them just declines gradient fills
		gProgLin = Link(KVsLin, KFsLin);
		gProgRad = Link(KVsRad, KFsRad);
		if (gProgLin)
			{
			gLinScale = glGetUniformLocation(gProgLin, "u_scale");
			gLinG = glGetUniformLocation(gProgLin, "u_g");
			gLinRamp = glGetUniformLocation(gProgLin, "u_ramp");
			}
		if (gProgRad)
			{
			gRadScale = glGetUniformLocation(gProgRad, "u_scale");
			gRadM0 = glGetUniformLocation(gProgRad, "u_m0");
			gRadM1 = glGetUniformLocation(gProgRad, "u_m1");
			gRadFocal = glGetUniformLocation(gProgRad, "u_focal");
			gRadD = glGetUniformLocation(gProgRad, "u_d");
			gRadRamp = glGetUniformLocation(gProgRad, "u_ramp");
			}
		gProgImg = Link(KVsImg, KFsImg);
		if (gProgImg)
			{
			gImgScale = glGetUniformLocation(gProgImg, "u_scale");
			gImgM0 = glGetUniformLocation(gProgImg, "u_m0");
			gImgM1 = glGetUniformLocation(gProgImg, "u_m1");
			gImgInv = glGetUniformLocation(gProgImg, "u_inv");
			gImgMul = glGetUniformLocation(gProgImg, "u_mul");
			gImgSampler = glGetUniformLocation(gProgImg, "u_tex");
			}
		glGenTextures(1, &gRampTex);
		glBindTexture(GL_TEXTURE_2D, gRampTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, KPhvgRamp, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gRampValid = EFalse;
		gFlatScale = glGetUniformLocation(gProgFlat, "u_scale");
		gFlatColor = glGetUniformLocation(gProgFlat, "u_color");
		gTexScale = glGetUniformLocation(gProgTex, "u_scale");
		gTexInv = glGetUniformLocation(gProgTex, "u_inv");
		gTexSampler = glGetUniformLocation(gProgTex, "u_tex");
		gVtx = (TReal32*)User::Alloc(KBatchPieces * 12 * sizeof(TReal32));
		if (!gVtx)
			return EFalse;
		glDisable(GL_DITHER);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glEnableVertexAttribArray(0);
		RDebug::Printf("PH: vg2 phvg: GL up: %s / %s, packed depth-stencil %d", (const char*)glGetString(GL_RENDERER),
		               (const char*)glGetString(GL_VERSION), gPacked);
		return ETrue;
		}

	// ---- shadows -------------------------------------------------------------------------------------------------
	void SetState(TShadow& aS, TInt aState)
		{
		if ((aS.iState != 0) != (aState != 0))
			gLive += aState ? 1 : -1;
		aS.iState = aState;
		}

	TShadow* Find(TAny* aCookie)
		{
		if (gSh[gLast].iCookie == aCookie)
			return &gSh[gLast];
		for (TInt i = 0; i < KMaxShadows; i++)
			if (gSh[i].iCookie == aCookie)
				{
				gLast = i;
				return &gSh[i];
				}
		return NULL;
		}

	void FreeGl(TShadow& aS)
		{
		if (aS.iFbo0) glDeleteFramebuffers(1, &aS.iFbo0);
		if (aS.iFboM) glDeleteFramebuffers(1, &aS.iFboM);
		if (aS.iFboR) glDeleteFramebuffers(1, &aS.iFboR);
		if (aS.iFboRd) glDeleteFramebuffers(1, &aS.iFboRd);
		if (aS.iRbRd) glDeleteRenderbuffers(1, &aS.iRbRd);
		if (aS.iTex0) glDeleteTextures(1, &aS.iTex0);
		if (aS.iTexM) glDeleteTextures(1, &aS.iTexM);
		if (aS.iTexR) glDeleteTextures(1, &aS.iTexR);
		if (aS.iRbDs) glDeleteRenderbuffers(1, &aS.iRbDs);
		aS.iFboRd = aS.iRbRd = 0;
		aS.iFbo0 = aS.iFboM = aS.iFboR = aS.iTex0 = aS.iTexM = aS.iTexR = aS.iRbDs = 0;
		}

	TInt ChooseScale(TInt aW, TInt aH)
		{
		if (aW < 1 || aH < 1 || aW > KMaxSide || aH > KMaxSide)
			return 0;
		return (aW <= 256 && aH <= 256) ? 4 : 2;
		}

	GLuint MakeTex(TInt aW, TInt aH, GLint aFilter)
		{
		GLuint t = 0;
		glGenTextures(1, &t);
		glBindTexture(GL_TEXTURE_2D, t);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aW, aH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, aFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, aFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		return t;
		}

	GLuint MakeFbo(GLuint aTex)
		{
		GLuint f = 0;
		glGenFramebuffers(1, &f);
		glBindFramebuffer(GL_FRAMEBUFFER, f);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, aTex, 0);
		return f;
		}

	// the GL objects of a shadow (lazily; EFalse = this machine cannot do it -> the surface stays in the RI)
	TBool EnsureGl(TShadow& aS)
		{
		if (aS.iFbo0)
			return ETrue;
		const TInt w = aS.iW * aS.iS, h = aS.iH * aS.iS;
		aS.iTex0 = MakeTex(w, h, GL_LINEAR);
		aS.iFbo0 = MakeFbo(aS.iTex0);
		glGenRenderbuffers(1, &aS.iRbDs);
		glBindRenderbuffer(GL_RENDERBUFFER, aS.iRbDs);
		TBool complete = EFalse;
		if (gPacked)
			{
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, w, h);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, aS.iRbDs);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, aS.iRbDs);
			complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
			}
		if (!complete)
			{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, w, h);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, aS.iRbDs);
			complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
			}
		if (!complete)
			{
			Log("shadow framebuffer incomplete", w, h);
			FreeGl(aS);
			return EFalse;
			}
		glClearStencil(0);
		glStencilMask(0xFF);
		glDisable(GL_SCISSOR_TEST);
		glClear(GL_STENCIL_BUFFER_BIT);
		if (aS.iS == 4)
			{
			aS.iTexM = MakeTex(aS.iW * 2, aS.iH * 2, GL_LINEAR);
			aS.iFboM = MakeFbo(aS.iTexM);
			}
		aS.iTexR = MakeTex(aS.iW, aS.iH, GL_NEAREST);
		aS.iFboR = MakeFbo(aS.iTexR);
		// the read-back target: an RGBA8 renderbuffer (falls back to the texture when this machine cannot make one)
		glGenRenderbuffers(1, &aS.iRbRd);
		glBindRenderbuffer(GL_RENDERBUFFER, aS.iRbRd);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, aS.iW, aS.iH);
		glGenFramebuffers(1, &aS.iFboRd);
		glBindFramebuffer(GL_FRAMEBUFFER, aS.iFboRd);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, aS.iRbRd);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR)
			{
			Log("renderbuffer read-back target unavailable, reading the texture", aS.iW, aS.iH);
			glDeleteFramebuffers(1, &aS.iFboRd);
			glDeleteRenderbuffers(1, &aS.iRbRd);
			aS.iFboRd = aS.iRbRd = 0;
			}
		return ETrue;
		}

	GLuint FboRead(const TShadow& aS) { return aS.iFboRd ? aS.iFboRd : aS.iFboR; }

	// find or create the record of a cookie; a size change drops the GL objects
	TShadow* Get(TAny* aCookie, TInt aW, TInt aH, TBool aCreate)
		{
		TShadow* s = Find(aCookie);
		if (s)
			{
			if (s->iW != aW || s->iH != aH)
				{
				FreeGl(*s);
				SetState(*s, 0);
				s->iResolved = 0;
				s->iW = aW;
				s->iH = aH;
				s->iS = ChooseScale(aW, aH);
				}
			return s->iS ? s : NULL;
			}
		if (!aCreate)
			return NULL;
		const TInt sc = ChooseScale(aW, aH);
		if (!sc)
			return NULL;
		for (TInt i = 0; i < KMaxShadows; i++)
			if (!gSh[i].iCookie)
				{
				Mem::FillZ(&gSh[i], sizeof(TShadow));
				gSh[i].iCookie = aCookie;
				gSh[i].iW = aW;
				gSh[i].iH = aH;
				gSh[i].iS = sc;
				gLast = i;
				return &gSh[i];
				}
		return NULL;
		}

	void QuadVerts(TReal32* aV, TReal32 aX0, TReal32 aY0, TReal32 aX1, TReal32 aY1)
		{
		aV[0] = aX0; aV[1] = aY0;  aV[2] = aX1; aV[3] = aY0;  aV[4] = aX1; aV[5] = aY1;
		aV[6] = aX0; aV[7] = aY0;  aV[8] = aX1; aV[9] = aY1;  aV[10] = aX0; aV[11] = aY1;
		}

	void SetNoTests()
		{
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}

	// draw aTex (of aTw x aTh texels) into the bound framebuffer of aDw x aDh pixels, the whole quad
	void BlitTex(GLuint aTex, TInt aDw, TInt aDh, GLint aFilter)
		{
		TReal32 v[12];
		QuadVerts(v, 0, 0, (TReal32)aDw, (TReal32)aDh);
		glViewport(0, 0, aDw, aDh);
		glUseProgram(gProgTex);
		glUniform2f(gTexScale, 2.0f / aDw, 2.0f / aDh);
		glUniform2f(gTexInv, 1.0f / aDw, 1.0f / aDh);
		glUniform1i(gTexSampler, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, aTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, aFilter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, aFilter);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		}

	void DirtyAdd(TShadow& aS, TInt aX0, TInt aY0, TInt aX1, TInt aY1)
		{
		if (aX0 < 0) aX0 = 0;
		if (aY0 < 0) aY0 = 0;
		if (aX1 > aS.iW) aX1 = aS.iW;
		if (aY1 > aS.iH) aY1 = aS.iH;
		if (aX1 <= aX0 || aY1 <= aY0)
			return;
		if (aS.iState != 2)
			{
			aS.iDx0 = aX0; aS.iDy0 = aY0; aS.iDx1 = aX1; aS.iDy1 = aY1;
			}
		else
			{
			if (aX0 < aS.iDx0) aS.iDx0 = aX0;
			if (aY0 < aS.iDy0) aS.iDy0 = aY0;
			if (aX1 > aS.iDx1) aS.iDx1 = aX1;
			if (aY1 > aS.iDy1) aS.iDy1 = aY1;
			}
		SetState(aS, 2);
		}

	// ---- worker commands -----------------------------------------------------------------------------------------
	TInt DoUpload(const TCmd& c)
		{
		TShadow* s = Get(c.iCookie, c.iW, c.iH, ETrue);
		if (!s || !EnsureGl(*s))
			return 0;
		glBindTexture(GL_TEXTURE_2D, s->iTexR);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->iW, s->iH, 0, GL_RGBA, GL_UNSIGNED_BYTE, c.iIn);
		glBindFramebuffer(GL_FRAMEBUFFER, s->iFbo0);
		SetNoTests();
		BlitTex(s->iTexR, s->iW * s->iS, s->iH * s->iS, GL_NEAREST);
		s->iResolved = s->iFboRd ? 0 : 1;	// iTexR is the (identical) 1x image; the read target is the renderbuffer when there is one
		SetState(*s, 1);
		gStat[2]++;
		return 1;
		}

	// ---- fill: trapezoid emission ----------------------------------------------------------------------------------
	struct TEmit
		{
		TInt	iN;				// pieces in gVtx
		TReal32	iXl, iXr, iH;	// clamp range, surface height
		TReal32	iMinY, iMaxY;
		TBool	iEvenOdd;
		};

	void Flush(TEmit& aE)
		{
		if (aE.iN)
			{
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, gVtx);
			glDrawArrays(GL_TRIANGLES, 0, aE.iN * 6);
			aE.iN = 0;
			}
		}

	// one directed piece (x0,y0)->(x1,y1) with y0 != y1, x already inside [Xl,Xr]
	void AddTrap(TEmit& aE, TReal32 x0, TReal32 y0, TReal32 x1, TReal32 y1)
		{
		if (x0 <= aE.iXl && x1 <= aE.iXl)
			return;										// left of every sample: contributes nothing
		const TBool up = y1 > y0;
		const TReal32 lx = up ? x0 : x1, ly = up ? y0 : y1;	// lower (smaller y) end
		const TReal32 hx = up ? x1 : x0, hy = up ? y1 : y0;
		if (ly < aE.iMinY) aE.iMinY = ly;
		if (hy > aE.iMaxY) aE.iMaxY = hy;
		const TReal32 L = aE.iXl;
		// A=(L,ly) B=(lx,ly) C=(hx,hy) D=(L,hy): A,B,C,D is counter-clockwise (front face) for an upward edge
		TReal32* v = gVtx + aE.iN * 12;
		if (up)
			{
			v[0] = L;  v[1] = ly;  v[2] = lx; v[3] = ly;  v[4] = hx; v[5] = hy;		// A B C
			v[6] = L;  v[7] = ly;  v[8] = hx; v[9] = hy;  v[10] = L; v[11] = hy;	// A C D
			}
		else
			{
			v[0] = L;  v[1] = ly;  v[2] = hx; v[3] = hy;  v[4] = lx; v[5] = ly;		// A C B
			v[6] = L;  v[7] = ly;  v[8] = L;  v[9] = hy;  v[10] = hx; v[11] = hy;	// A D C
			}
		if (++aE.iN == KBatchPieces)
			Flush(aE);
		}

	// x-split a y-clipped piece at Xl / Xr and clamp
	void AddEdge(TEmit& aE, TReal32 x0, TReal32 y0, TReal32 x1, TReal32 y1)
		{
		TReal32 t[4];
		TInt n = 0;
		t[n++] = 0.0f;
		const TReal32 dx = x1 - x0;
		if (dx != 0.0f)
			{
			TReal32 ta = (aE.iXl - x0) / dx, tb = (aE.iXr - x0) / dx;
			if (ta > tb)
				{
				const TReal32 tt = ta; ta = tb; tb = tt;
				}
			if (ta > 0.0f && ta < 1.0f) t[n++] = ta;
			if (tb > 0.0f && tb < 1.0f) t[n++] = tb;
			}
		t[n++] = 1.0f;
		const TReal32 dy = y1 - y0;
		for (TInt i = 0; i + 1 < n; i++)
			{
			TReal32 ax = x0 + dx * t[i], ay = y0 + dy * t[i];
			TReal32 bx = x0 + dx * t[i + 1], by = y0 + dy * t[i + 1];
			if (ax < aE.iXl) ax = aE.iXl; else if (ax > aE.iXr) ax = aE.iXr;
			if (bx < aE.iXl) bx = aE.iXl; else if (bx > aE.iXr) bx = aE.iXr;
			if (ay != by)
				AddTrap(aE, ax, ay, bx, by);
			}
		}

	// O4: select the cover program + its uniforms for the paint of f; EFalse = cannot (the caller declines)
	TBool SetupCoverPaint(const TPhvgFill& f, TReal32 aW, TReal32 aH)
		{
		if (f.iPaintType == 0)
			{
			glUseProgram(gProgFlat);
			glUniform2f(gFlatScale, 2.0f / aW, 2.0f / aH);
			glUniform4f(gFlatColor, f.iColor[0], f.iColor[1], f.iColor[2], f.iColor[3]);
			return ETrue;
			}
		if (!f.iRamp || (f.iPaintType == 1 && !gProgLin) || (f.iPaintType == 2 && !gProgRad))
			return EFalse;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gRampTex);
		if (!gRampValid || Mem::Compare(gRampCache, KPhvgRamp * 4, f.iRamp, KPhvgRamp * 4) != 0)
			{
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, KPhvgRamp, 1, GL_RGBA, GL_UNSIGNED_BYTE, f.iRamp);
			Mem::Copy(gRampCache, f.iRamp, KPhvgRamp * 4);
			gRampValid = ETrue;
			}
		const GLint wrap = f.iSpread == 1 ? GL_REPEAT : (f.iSpread == 2 ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		if (f.iPaintType == 1)
			{
			glUseProgram(gProgLin);
			glUniform2f(gLinScale, 2.0f / aW, 2.0f / aH);
			glUniform3f(gLinG, f.iGrad[0], f.iGrad[1], f.iGrad[2]);
			glUniform1i(gLinRamp, 0);
			}
		else
			{
			glUseProgram(gProgRad);
			glUniform2f(gRadScale, 2.0f / aW, 2.0f / aH);
			glUniform3f(gRadM0, f.iGrad[0], f.iGrad[1], f.iGrad[2]);
			glUniform3f(gRadM1, f.iGrad[3], f.iGrad[4], f.iGrad[5]);
			glUniform2f(gRadFocal, f.iFocal[0], f.iFocal[1]);
			glUniform1f(gRadD, f.iFocal[2]);
			glUniform1i(gRadRamp, 0);
			}
		return ETrue;
		}

	TInt DoFillOne(const TPhvgFill& f, const TInt* aClip);

	// a scissored fill: the whole stencil-and-cover once per (disjoint) rectangle, under glScissor
	TInt DoFill(const TPhvgFill& f)
		{
		if (f.iNRects <= 0)
			return DoFillOne(f, NULL);
		for (TInt i = 0; i < f.iNRects; i++)
			if (!DoFillOne(f, f.iRects + i * 4))
				return 0;
		return 1;
		}

	TInt DoFillOne(const TPhvgFill& f, const TInt* aClip)
		{
		TShadow* s = Get(f.iCookie, f.iW, f.iH, EFalse);
		if (!s || !s->iFbo0 || s->iState == 0)
			return 0;
		if (f.iPaintType != 0 && (!f.iRamp || (f.iPaintType == 1 && !gProgLin) || (f.iPaintType == 2 && !gProgRad)))
			return 0;
		const TReal32 W = (TReal32)s->iW, H = (TReal32)s->iH;
		glBindFramebuffer(GL_FRAMEBUFFER, s->iFbo0);
		glViewport(0, 0, s->iW * s->iS, s->iH * s->iS);
		if (aClip)
			{
			glEnable(GL_SCISSOR_TEST);
			glScissor(aClip[0] * s->iS, aClip[1] * s->iS, aClip[2] * s->iS, aClip[3] * s->iS);
			}
		else
			glDisable(GL_SCISSOR_TEST);
		glDisable(GL_BLEND);
		glUseProgram(gProgFlat);
		glUniform2f(gFlatScale, 2.0f / W, 2.0f / H);
		// 1. stencil: accumulate the signed edge count
		glEnable(GL_STENCIL_TEST);
		glStencilMask(0xFF);
		glStencilFunc(GL_ALWAYS, 0, 0xFF);
		if (f.iEvenOdd)
			glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
		else
			{
			glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_INCR_WRAP);
			glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_DECR_WRAP);
			}
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		TEmit e;
		e.iN = 0;
		e.iXl = -1.0f;
		e.iXr = W + 1.0f;
		e.iH = H;
		e.iMinY = H;
		e.iMaxY = 0.0f;
		e.iEvenOdd = f.iEvenOdd;
		TReal32 minX = W + 1.0f, maxX = -1.0f;
		for (TInt i = 0; i < f.iCount; i++)
			{
			TReal32 x0 = f.iEdges[i * 4], y0 = f.iEdges[i * 4 + 1], x1 = f.iEdges[i * 4 + 2], y1 = f.iEdges[i * 4 + 3];
			if (y0 == y1)
				continue;
			if (x0 < minX) minX = x0;
			if (x1 < minX) minX = x1;
			if (x0 > maxX) maxX = x0;
			if (x1 > maxX) maxX = x1;
			if (f.iLeft)			// O4: the piece's own left bound (strokes), else the surface's left edge
				e.iXl = f.iLeft[i] < -1.0f ? -1.0f : f.iLeft[i];
			// clip to the surface rows
			const TReal32 dy = y1 - y0;
			TReal32 ta = (0.0f - y0) / dy, tb = (H - y0) / dy;
			if (ta > tb)
				{
				const TReal32 tt = ta; ta = tb; tb = tt;
				}
			const TReal32 lo = ta > 0.0f ? ta : 0.0f, hi = tb < 1.0f ? tb : 1.0f;
			if (lo >= hi)
				continue;
			const TReal32 dx = x1 - x0;
			AddEdge(e, x0 + dx * lo, y0 + dy * lo, x0 + dx * hi, y0 + dy * hi);
			}
		Flush(e);
		// 2. cover: the colour where the stencil is not zero; the stencil goes back to zero
		if (e.iMaxY > e.iMinY)
			{
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
			glStencilOpSeparate(GL_FRONT_AND_BACK, GL_KEEP, GL_ZERO, GL_ZERO);
			if (f.iBlendSrc)
				glDisable(GL_BLEND);
			else
				{
				glEnable(GL_BLEND);
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				}
			SetupCoverPaint(f, W, H);
			TReal32 v[12];
			QuadVerts(v, 0.0f, e.iMinY, W, e.iMaxY);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			}
		SetNoTests();
		// dirty pixels: the edge bounding box, one pixel of slack
		TInt x0 = minX < 0.0f ? 0 : (TInt)minX - 1, x1 = maxX > W ? s->iW : (TInt)maxX + 2;
		TInt y0 = e.iMinY < 0.0f ? 0 : (TInt)e.iMinY - 1, y1 = (TInt)e.iMaxY + 2;
		if (e.iMaxY > e.iMinY && x1 > x0)
			DirtyAdd(*s, x0, y0, x1, y1);
		s->iResolved = 0;
		gStat[0]++;
		gStat[5] += f.iCount;
		if (f.iPaintType != 0)
			gStat[9]++;
		if (f.iStroke)
			gStat[10]++;
		return 1;
		}

	TInt DoClear(const TCmd& c)
		{
		const TBool full = c.iX == 0 && c.iY == 0 && c.iCw == c.iW && c.iCh == c.iH;
		TShadow* s = Get(c.iCookie, c.iW, c.iH, ETrue);
		if (!s)
			return 0;
		if (s->iState == 0 && !full)
			return -1;
		if (!EnsureGl(*s))
			return 0;
		glBindFramebuffer(GL_FRAMEBUFFER, s->iFbo0);
		glViewport(0, 0, s->iW * s->iS, s->iH * s->iS);
		SetNoTests();
		glEnable(GL_SCISSOR_TEST);
		glScissor(c.iX * s->iS, c.iY * s->iS, c.iCw * s->iS, c.iCh * s->iS);
		glClearColor(c.iCol[0], c.iCol[1], c.iCol[2], c.iCol[3]);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_SCISSOR_TEST);
		if (s->iState == 0)				// a full clear made a stale shadow whole
			SetState(*s, 1);
		DirtyAdd(*s, c.iX, c.iY, c.iX + c.iCw, c.iY + c.iCh);
		s->iResolved = 0;
		gStat[1]++;
		return 1;
		}

	// shadow (S x S) -> iTexR (1x), by GPU bilinear halving; skipped while iTexR is current
	void Resolve(TShadow& aS)
		{
		if (aS.iResolved)
			return;
		SetNoTests();
		if (aS.iS == 4)
			{
			glBindFramebuffer(GL_FRAMEBUFFER, aS.iFboM);
			BlitTex(aS.iTex0, aS.iW * 2, aS.iH * 2, GL_LINEAR);
			glBindFramebuffer(GL_FRAMEBUFFER, FboRead(aS));
			BlitTex(aS.iTexM, aS.iW, aS.iH, GL_LINEAR);
			}
		else
			{
			glBindFramebuffer(GL_FRAMEBUFFER, FboRead(aS));
			BlitTex(aS.iTex0, aS.iW, aS.iH, GL_LINEAR);
			}
		aS.iResolved = 1;
		}

	// shadow (S x S) -> iTexR (1x), only inside the pixel rectangle aR (x0,y0,x1,y1): scissored passes; iTexR is then valid inside the rectangle only (iResolved stays 0)
	void ResolveRect(TShadow& aS, const TInt* aR)
		{
		SetNoTests();
		glEnable(GL_SCISSOR_TEST);
		const TInt x0 = aR[0], y0 = aR[1], w = aR[2] - aR[0], h = aR[3] - aR[1];
		if (aS.iS == 4)
			{
			glBindFramebuffer(GL_FRAMEBUFFER, aS.iFboM);
			glScissor(x0 * 2, y0 * 2, w * 2, h * 2);
			BlitTex(aS.iTex0, aS.iW * 2, aS.iH * 2, GL_LINEAR);
			glBindFramebuffer(GL_FRAMEBUFFER, FboRead(aS));
			glScissor(x0, y0, w, h);
			BlitTex(aS.iTexM, aS.iW, aS.iH, GL_LINEAR);
			}
		else
			{
			glBindFramebuffer(GL_FRAMEBUFFER, FboRead(aS));
			glScissor(x0, y0, w, h);
			BlitTex(aS.iTex0, aS.iW, aS.iH, GL_LINEAR);
			}
		glDisable(GL_SCISSOR_TEST);
		}

	void ReadRect(TShadow& aS, const TCmd& c)
		{
		glBindFramebuffer(GL_FRAMEBUFFER, FboRead(aS));
		const TInt x0 = c.iRect[0], y0 = c.iRect[1], w = c.iRect[2] - c.iRect[0], h = c.iRect[3] - c.iRect[1];
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x0, y0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, c.iOut);
		if (glGetError() != GL_NO_ERROR)
			gStat[6]++;
		}

	TInt DoReadback(const TCmd& c)
		{
		TShadow* s = Find(c.iCookie);
		if (!s || s->iState == 0 || !s->iFbo0)
			return 0;
		Resolve(*s);
		ReadRect(*s, c);
		SetState(*s, 1);
		gStat[3]++;
		return 1;
		}

	// O3: read pixels out of the shadow for a CPU consumer WITHOUT making the RI surface current: the shadow stays
	// "GPU ahead" (resident), the RI's float pixels are not touched. Valid for state 1 and 2 (the shadow is complete).
	TInt DoReadDirect(const TCmd& c)
		{
		TShadow* s = Find(c.iCookie);
		if (!s || s->iState == 0 || !s->iFbo0)
			return 0;
		const TUint t0 = User::FastCounter();
		const TInt full = c.iRect[0] <= 0 && c.iRect[1] <= 0 && c.iRect[2] >= s->iW && c.iRect[3] >= s->iH;
		if (full)
			Resolve(*s);
		else if (!s->iResolved)
			ResolveRect(*s, c.iRect);			// only the rectangle the caller reads (a full resolve is a whole-screen pass)
		const TUint t1 = User::FastCounter();
		ReadRect(*s, c);
		const TUint t2 = User::FastCounter();
		gStat[8]++;
		{	// one-off probe: the same 128x16 block read from the texture-backed FBO and from the renderbuffer-backed FBO (is the slow read the tiled texture?)
		static TInt probed;
		if (!probed && s->iFboRd && s->iW >= 128 && s->iH >= 16)
			{
			probed = 1;
			static TUint8 buf[128 * 16 * 4];
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			for (TInt k = 0; k < 4; k++)
				{
				glBindFramebuffer(GL_FRAMEBUFFER, (k & 1) ? s->iFboRd : s->iFboR);
				const TUint a = User::FastCounter();
				glReadPixels(0, 0, 128, 16, GL_RGBA, GL_UNSIGNED_BYTE, buf);
				const TUint b = User::FastCounter();
				RDebug::Printf("PH: vg2 phvg read probe %d: 128x16 from the %s FBO: %u us = %u ns/px (fast counter = 24 MHz)", k, (k & 1) ? "renderbuffer" : "texture", (b - a) / 24, (b - a) * 125 / 3 / (128 * 16));
				}
			}
		}
		static TUint nrd;
		if (++nrd <= 6 || (nrd & 15) == 0)
			{
			const TInt px = (c.iRect[2] - c.iRect[0]) * (c.iRect[3] - c.iRect[1]);
			RDebug::Printf("PH: vg2 phvg readdirect #%u: %dx%d at %d,%d (%s): ring wait %u, resolve %u, glReadPixels %u us = %u ns/px (ring wait / resolve in 24 MHz ticks)",
				nrd, c.iRect[2] - c.iRect[0], c.iRect[3] - c.iRect[1], c.iRect[0], c.iRect[1], s->iFboRd ? "renderbuffer" : "texture", gRingWaitTicks, t1 - t0, (t2 - t1) / 24, px > 0 ? (TUint)((TUint64)(t2 - t1) * 1000 / 24 / px) : 0);
			}
		return 1;
		}

	// ---- PRESENT: the shadow resolved by the GPU straight into a window's surface buffer -------------------------------
	// The buffer is imported once as a render target (Mesa's EGL_SYMBIAN_MEMORY_IMAGE_PH over its memory, usage 1 = the GPU
	// writes it: the GPU job does the cache maintenance) and kept while its layout stays; the last resolve pass of the
	// shadow writes it, flipped. The CPU never touches the pixels: what was 60-80 ms of glReadPixels + repack per frame is
	// one GPU pass.
	struct TMemImage					// symgl_native.h's symgl_sgimage
		{
		void* iBase;
		unsigned iStride, iWidth, iHeight, iFormat;	// 1 = ARGB8888 (BGRA bytes)
		unsigned iUsage;			// 1 = a render target
		};
	struct TTarget
		{
		TUint8*		iBase;
		TInt		iStride, iW, iH;
		EGLImageKHR	iImg;
		GLuint		iTex, iFbo;
		TUint		iUsed;
		};
	const TInt KTargets = 4;
	TTarget			gTgt[KTargets];
	TUint			gTgtClock;
	EGLImageKHR		(*gCreateImage)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
	EGLBoolean		(*gDestroyImage)(EGLDisplay, EGLImageKHR);
	void			(*gImageTarget)(GLenum, GLeglImageOES);
	TInt			gPresentUp;			// 0 not tried, 1 ready, -1 unavailable

	void TargetFree(TTarget& aT)
		{
		if (aT.iFbo)
			glDeleteFramebuffers(1, &aT.iFbo);
		if (aT.iTex)
			glDeleteTextures(1, &aT.iTex);
		if (aT.iImg)
			gDestroyImage(gDpy, aT.iImg);
		Mem::FillZ(&aT, sizeof(aT));
		}

	TTarget* TargetGet(TUint8* aBase, TInt aStride, TInt aW, TInt aH)
		{
		if (!gPresentUp)
			{
			gCreateImage = (EGLImageKHR (*)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*))eglGetProcAddress("eglCreateImageKHR");
			gDestroyImage = (EGLBoolean (*)(EGLDisplay, EGLImageKHR))eglGetProcAddress("eglDestroyImageKHR");
			gImageTarget = (void (*)(GLenum, GLeglImageOES))eglGetProcAddress("glEGLImageTargetTexture2DOES");
			gPresentUp = (gCreateImage && gDestroyImage && gImageTarget && gProgFlip) ? 1 : -1;
			if (gPresentUp < 0)
				RDebug::Printf("PH: vg2 phvg present: no EGLImage entry points or flip program, reading back instead");
			}
		if (gPresentUp < 0)
			return NULL;
		TTarget* lru = &gTgt[0];
		for (TInt i = 0; i < KTargets; i++)
			{
			TTarget& t = gTgt[i];
			if (t.iImg && t.iBase == aBase && t.iStride == aStride && t.iW == aW && t.iH == aH)
				{
				t.iUsed = ++gTgtClock;
				return &t;
				}
			// the slot to fill: an empty one, else the least recently used
			if (lru->iImg && (!t.iImg || t.iUsed < lru->iUsed))
				lru = &t;
			}
		TargetFree(*lru);
		TMemImage m;
		m.iBase = aBase;
		m.iStride = aStride;
		m.iWidth = aW;
		m.iHeight = aH;
		m.iFormat = 1;
		m.iUsage = 1;
		EGLImageKHR img = gCreateImage(gDpy, EGL_NO_CONTEXT, EGL_SYMBIAN_MEMORY_IMAGE_PH, (EGLClientBuffer)&m, NULL);
		if (!img)
			{
			RDebug::Printf("PH: vg2 phvg present: no EGLImage over %08x %dx%d stride %d (%x)", (TUint)aBase, aW, aH, aStride, eglGetError());
			return NULL;
			}
		TTarget& t = *lru;
		t.iImg = img;
		glGenTextures(1, &t.iTex);
		glBindTexture(GL_TEXTURE_2D, t.iTex);
		gImageTarget(GL_TEXTURE_2D, img);
		glGenFramebuffers(1, &t.iFbo);
		glBindFramebuffer(GL_FRAMEBUFFER, t.iFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.iTex, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE || glGetError() != GL_NO_ERROR)
			{
			RDebug::Printf("PH: vg2 phvg present: %08x %dx%d stride %d is not a render target", (TUint)aBase, aW, aH, aStride);
			TargetFree(t);
			return NULL;
			}
		t.iBase = aBase;
		t.iStride = aStride;
		t.iW = aW;
		t.iH = aH;
		t.iUsed = ++gTgtClock;
		RDebug::Printf("PH: vg2 phvg present: window buffer %08x %dx%d stride %d imported as a render target", (TUint)aBase, aW, aH, aStride);
		return &t;
		}

	TInt DoPresent(const TCmd& c)
		{
		TShadow* s = Find(c.iCookie);
		if (!s || s->iState == 0 || !s->iFbo0 || s->iW != c.iW || s->iH != c.iH)
			{
			static TInt nd;
			if (nd++ < 6)
				RDebug::Printf("PH: vg2 phvg present declined: shadow %d state %d fbo %d size %dx%d (asked %dx%d)", s != NULL,
					s ? s->iState : -1, s ? (TInt)s->iFbo0 : 0, s ? s->iW : 0, s ? s->iH : 0, c.iW, c.iH);
			return 0;
			}
		const TUint t0 = User::FastCounter();
		TTarget* t = TargetGet(c.iOut, c.iX, c.iW, c.iH);
		if (!t)
			return 0;
		SetNoTests();
		GLuint src = s->iTex0;
		if (s->iS == 4)
			{
			glBindFramebuffer(GL_FRAMEBUFFER, s->iFboM);		// S x S -> 2 x 2 (the same first halving as Resolve)
			BlitTex(s->iTex0, s->iW * 2, s->iH * 2, GL_LINEAR);
			src = s->iTexM;
			}
		glBindFramebuffer(GL_FRAMEBUFFER, t->iFbo);
		glViewport(0, 0, s->iW, s->iH);
		glUseProgram(gProgFlip);
		glUniform1i(gFlipSampler, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, src);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		TReal32 v[12];
		QuadVerts(v, 0, 0, 1, 1);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glFinish();						// the buffer is complete (and out of the GPU's way) before the compositor is told
		const TInt err = glGetError();
		const TUint t1 = User::FastCounter();
		static TUint np;
		if (++np <= 6 || (np & 63) == 0)
			RDebug::Printf("PH: vg2 phvg present #%u: %dx%d (S %d) into %08x: %u us, GL error %x",
				np, s->iW, s->iH, s->iS, (TUint)c.iOut, (t1 - t0) / 24, err);
		if (err != GL_NO_ERROR)
			{
			gStat[6]++;
			TargetFree(*t);
			return 0;
			}
		gStat[8]++;
		return 1;
		}

	// ---- O4b: source image textures and ASYNCHRONOUS image draws ---------------------------------------------------------
	// Image draws do not wait for the GL worker: the caller (the one VG rendering thread, under gLockH) appends an entry to an ordered ring and goes on; the
	// worker drains the ring in order before it runs any synchronous command (Call), so every other operation (fills, clears, uploads, readbacks, forget, and
	// the exports that only read shadow state, via Drain()) sees all earlier image draws as done. The texture cache is DECIDED on the caller's side (a
	// directory of slots with LRU + byte budget, so a draw can answer "texture missing, send the pixels" without a round trip) and EXECUTED on the worker's
	// side (gTexGl[slot]); an upload entry carries its own copy of the pixels, a drop entry frees a slot's GL texture.
	const TInt KMaxTex = 96;
	const TInt KTexBudget = 24 * 1024 * 1024;
	const TInt KQ = 128;
	const TInt KQRects = 64;
	struct TDirEnt
		{
		TAny*	iKey;
		TInt	iW, iH;
		TUint32	iHash;
		TUint	iStamp;
		TBool	iUsed;
		};
	enum { EQDraw, EQDrop };
	struct TQEnt
		{
		TInt		iType;
		TInt		iSlot;
		TUint8*		iOwnPixels;			// upload these into the slot first (freed by the worker)
		TPhvgImage	iIm;
		TInt		iRects[KQRects * 4];
		};
	TDirEnt			gDir[KMaxTex];		// caller side (under gLockH)
	TInt			gDirBytes;
	TUint			gDirClock;
	GLuint			gTexGl[KMaxTex];	// worker side
	TQEnt			gQ[KQ];
	volatile TUint	gQHead, gQTail;
	volatile TUint	gCmdPending;
	TUint			gImgPerf[8];		// 0 draws, 1 cache hits, 2 misses, 3 uploaded bytes, 4 ticks in the worker's draw, 5 ticks in texture uploads

	void TexFree(TInt aSlot)
		{
		if (gTexGl[aSlot])
			{
			glDeleteTextures(1, &gTexGl[aSlot]);
			gTexGl[aSlot] = 0;
			}
		}

	void TexSet(TInt aSlot, const TUint8* aPixels, TInt aW, TInt aH)
		{
		TexFree(aSlot);
		glGenTextures(1, &gTexGl[aSlot]);
		glBindTexture(GL_TEXTURE_2D, gTexGl[aSlot]);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aW, aH, 0, GL_RGBA, GL_UNSIGNED_BYTE, aPixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

	// the worker executes one ring entry
	void DoQueued(TQEnt& e)
		{
		const TUint t0 = User::FastCounter();
		if (e.iType == EQDrop)
			{
			TexFree(e.iSlot);
			return;
			}
		if (e.iOwnPixels)
			{
			const TUint u0 = User::FastCounter();
			TexSet(e.iSlot, e.iOwnPixels, e.iIm.iIw, e.iIm.iIh);
			gImgPerf[5] += User::FastCounter() - u0;
			gImgPerf[3] += e.iIm.iIw * e.iIm.iIh * 4;
			gImgPerf[2]++;
			User::Free(e.iOwnPixels);
			e.iOwnPixels = NULL;
			}
		else
			gImgPerf[1]++;
		const TPhvgImage& f = e.iIm;
		TShadow* s = Get(f.iCookie, f.iW, f.iH, EFalse);
		if (!s || !s->iFbo0 || s->iState == 0 || !gProgImg || !gTexGl[e.iSlot])
			return;
		const TReal32 W = (TReal32)s->iW, H = (TReal32)s->iH;
		const TInt S = s->iS;
		glBindFramebuffer(GL_FRAMEBUFFER, s->iFbo0);
		glViewport(0, 0, s->iW * S, s->iH * S);
		SetNoTests();
		glUseProgram(gProgImg);
		glUniform2f(gImgScale, 2.0f / W, 2.0f / H);
		glUniform3f(gImgM0, f.iM[0], f.iM[1], f.iM[2]);
		glUniform3f(gImgM1, f.iM[3], f.iM[4], f.iM[5]);
		glUniform2f(gImgInv, 1.0f / f.iIw, 1.0f / f.iIh);
		glUniform4f(gImgMul, f.iMul[0], f.iMul[1], f.iMul[2], f.iMul[3]);
		glUniform1i(gImgSampler, 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, gTexGl[e.iSlot]);
		if (f.iBlendSrc)
			glDisable(GL_BLEND);
		else
			{
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
		TReal32 v[12];
		QuadVerts(v, 0, 0, (TReal32)f.iIw, (TReal32)f.iIh);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
		glEnable(GL_SCISSOR_TEST);
		TInt bx0 = s->iW, by0 = s->iH, bx1 = 0, by1 = 0;
		for (TInt i = 0; i < f.iNRects; i++)
			{
			const TInt* r = e.iRects + i * 4;
			glScissor(r[0] * S, r[1] * S, r[2] * S, r[3] * S);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			if (r[0] < bx0) bx0 = r[0];
			if (r[1] < by0) by0 = r[1];
			if (r[0] + r[2] > bx1) bx1 = r[0] + r[2];
			if (r[1] + r[3] > by1) by1 = r[1] + r[3];
			}
		SetNoTests();
		DirtyAdd(*s, bx0, by0, bx1, by1);		// the caller clipped the rectangles to the image's footprint
		s->iResolved = 0;
		gStat[11]++;
		gImgPerf[4] += User::FastCounter() - t0;
		if ((++gImgPerf[0] & 255) == 0)
			RDebug::Printf("PH: vg2 phvg images: draws %u cache hits %u misses %u uploaded %u KB; worker ticks %u (texture upload ticks %u)",
				gImgPerf[0], gImgPerf[1], gImgPerf[2], gImgPerf[3] / 1024, gImgPerf[4], gImgPerf[5]);
		}

	// the worker: run every queued entry, in order
	void DrainRing()
		{
		for (;;)
			{
			const TUint h = gQHead;
			if (h == __e32_atomic_load_acq32(&gQTail))
				break;
			DoQueued(gQ[h % KQ]);
			__e32_atomic_store_rel32(&gQHead, h + 1);
			}
		}

	TInt DoForget(const TCmd& c)
		{
		TShadow* s = Find(c.iCookie);
		if (s)
			{
			FreeGl(*s);
			SetState(*s, 0);
			Mem::FillZ(s, sizeof(TShadow));
			}
		return 1;
		}

	TInt WorkerMain(TAny*)
		{
		CTrapCleanup* tc = CTrapCleanup::New();
		RSemaphore req, done;
		req.SetHandle(gReqH);
		done.SetHandle(gDoneH);
		const TBool ok = GlInit();
		gCmd.iResult = ok;
		done.Signal();
		if (!ok)
			{
			delete tc;
			return KErrGeneral;
			}
		for (;;)
			{
			req.Wait();
			const TUint tRing0 = User::FastCounter();
			DrainRing();
			gRingWaitTicks = User::FastCounter() - tRing0;
			if (!__e32_atomic_load_acq32(&gCmdPending))
				continue;
			switch (gCmd.iOp)
				{
				case EOpUpload:		gCmd.iResult = DoUpload(gCmd); break;
				case EOpFill:		gCmd.iResult = DoFill(*gCmd.iFill); break;
				case EOpClear:		gCmd.iResult = DoClear(gCmd); break;
				case EOpReadback:	gCmd.iResult = DoReadback(gCmd); break;
				case EOpForget:		gCmd.iResult = DoForget(gCmd); break;
				case EOpReadDirect:	gCmd.iResult = DoReadDirect(gCmd); break;
				case EOpPresent:	gCmd.iResult = DoPresent(gCmd); break;
				case EOpNop:		gCmd.iResult = 1; break;
				default:			gCmd.iResult = 0; break;
				}
			__e32_atomic_store_rel32(&gCmdPending, 0);
			done.Signal();
			}
		}

	TInt Call(TCmd& aCmd)
		{
		if (gUp != 1)
			return 0;
		RMutex lock;
		RSemaphore req, done;
		lock.SetHandle(gLockH);
		req.SetHandle(gReqH);
		done.SetHandle(gDoneH);
		lock.Wait();
		gCmd = aCmd;
		__e32_atomic_store_rel32(&gCmdPending, 1);
		req.Signal();
		done.Wait();
		const TInt r = gCmd.iResult;
		lock.Signal();
		return r;
		}

	// wait until every queued image draw has been executed (before reading shadow state from the caller's thread)
	void Drain()
		{
		if (gUp != 1 || gQHead == gQTail)
			return;
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpNop;
		Call(c);
		}

	// ---- the caller's side of the ring (call with gLockH held) ----
	TQEnt& QSlot()
		{
		RSemaphore req;
		req.SetHandle(gReqH);
		while (gQTail - __e32_atomic_load_acq32(&gQHead) >= (TUint)KQ)
			{
			req.Signal();
			User::After(1000);
			}
		return gQ[gQTail % KQ];
		}

	void QPublish()
		{
		RSemaphore req;
		req.SetHandle(gReqH);
		__e32_atomic_store_rel32(&gQTail, gQTail + 1);
		req.Signal();
		}

	void QDrop(TInt aSlot)
		{
		TQEnt& e = QSlot();
		e.iType = EQDrop;
		e.iSlot = aSlot;
		e.iOwnPixels = NULL;
		QPublish();
		}

	// 1 queued, 0 declined, -2 the texture is not cached and no pixels were given
	TInt QueueImage(const TPhvgImage& f)
		{
		TInt slot = -1;
		TUint8* own = NULL;
		for (TInt i = 0; i < KMaxTex; i++)
			if (gDir[i].iUsed && gDir[i].iKey == f.iKey && gDir[i].iW == f.iIw && gDir[i].iH == f.iIh && gDir[i].iHash == f.iHash)
				{
				slot = i;
				gDir[i].iStamp = ++gDirClock;
				break;
				}
		if (slot < 0)
			{
			if (!f.iPixels)
				return -2;
			const TInt bytes = f.iIw * f.iIh * 4;
			if (bytes > KTexBudget)
				return 0;
			TInt reuse = -1;
			for (TInt i = 0; i < KMaxTex; i++)			// a stale version of the same image is replaced in its slot
				if (gDir[i].iUsed && gDir[i].iKey == f.iKey)
					{
					if (reuse < 0)
						reuse = i;
					else
						{
						gDirBytes -= gDir[i].iW * gDir[i].iH * 4;
						gDir[i].iUsed = EFalse;
						QDrop(i);
						}
					}
			if (reuse >= 0)
				{
				gDirBytes -= gDir[reuse].iW * gDir[reuse].iH * 4;
				gDir[reuse].iUsed = EFalse;
				}
			for (;;)
				{
				TInt freeSlot = reuse, lru = -1;
				for (TInt i = 0; i < KMaxTex; i++)
					{
					if (!gDir[i].iUsed)
						{
						if (freeSlot < 0)
							freeSlot = i;
						}
					else if (lru < 0 || gDir[i].iStamp < gDir[lru].iStamp)
						lru = i;
					}
				if (freeSlot >= 0 && gDirBytes + bytes <= KTexBudget)
					{
					slot = freeSlot;
					break;
					}
				if (lru < 0)
					return 0;
				gDirBytes -= gDir[lru].iW * gDir[lru].iH * 4;
				gDir[lru].iUsed = EFalse;
				if (reuse < 0 || lru != reuse)
					QDrop(lru);
				}
			own = (TUint8*)User::Alloc(bytes);
			if (!own)
				return 0;
			Mem::Copy(own, f.iPixels, bytes);
			gDir[slot].iKey = f.iKey; gDir[slot].iW = f.iIw; gDir[slot].iH = f.iIh; gDir[slot].iHash = f.iHash;
			gDir[slot].iStamp = ++gDirClock; gDir[slot].iUsed = ETrue;
			gDirBytes += bytes;
			}
		TQEnt& e = QSlot();
		e.iType = EQDraw;
		e.iSlot = slot;
		e.iOwnPixels = own;
		e.iIm = f;
		e.iIm.iPixels = NULL;
		e.iIm.iRects = NULL;
		for (TInt i = 0; i < f.iNRects * 4; i++)
			e.iRects[i] = f.iRects[i];
		QPublish();
		return 1;
		}
	}

// ---- the exports (phvg.def) ---------------------------------------------------------------------------------------
extern "C"
	{
	EXPORT_C void PhvgForget(TAny* aCookie);

	EXPORT_C TInt PhvgInit()
		{
		if (gUp != 0)
			return gUp == 1;
		gUp = -1;
		RMutex lock;
		RSemaphore req, done;
		if (lock.CreateLocal() || req.CreateLocal(0) || done.CreateLocal(0))
			return 0;
		gLockH = lock.Handle();
		gReqH = req.Handle();
		gDoneH = done.Handle();
		// a GL thread; shares the process heap, a generous stack (the GLSL compiler recurses deeply)
		RThread th;
		if (th.Create(KNullDesC, WorkerMain, 0x40000, NULL, NULL))
			return 0;
		th.Resume();
		done.Wait();
		if (!gCmd.iResult)
			{
			th.Kill(0);
			return 0;
			}
		gUp = 1;
		return 1;
		}

	EXPORT_C volatile TInt* PhvgLivePtr()
		{
		return &gLive;
		}

	EXPORT_C TInt PhvgState(TAny* aCookie, TInt aW, TInt aH)
		{
		Drain();
		TShadow* old = Find(aCookie);
		if (old && (old->iW != aW || old->iH != aH))
			PhvgForget(aCookie);			// GL objects are the worker's to delete
		TShadow* s = Get(aCookie, aW, aH, ETrue);
		return s ? s->iState : -1;
		}

	EXPORT_C TInt PhvgUpload(TAny* aCookie, TInt aW, TInt aH, const TUint8* aRgba)
		{
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpUpload; c.iCookie = aCookie; c.iW = aW; c.iH = aH; c.iIn = aRgba;
		return Call(c);
		}

	EXPORT_C TInt PhvgFill(const TPhvgFill* aFill)
		{
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpFill; c.iFill = aFill;
		const TInt r = Call(c);
		if (!r)
			gStat[4]++;
		return r;
		}

	EXPORT_C TInt PhvgClear(TAny* aCookie, TInt aW, TInt aH, TInt aX, TInt aY, TInt aCw, TInt aCh, const TReal32* aRgba)
		{
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpClear; c.iCookie = aCookie; c.iW = aW; c.iH = aH; c.iX = aX; c.iY = aY; c.iCw = aCw; c.iCh = aCh;
		c.iCol = aRgba;
		const TInt r = Call(c);
		if (!r)
			gStat[4]++;
		return r;
		}

	EXPORT_C TInt PhvgDirty(TAny* aCookie, TInt* aRect)
		{
		Drain();
		TShadow* s = Find(aCookie);
		if (!s || s->iState != 2)
			return 0;
		aRect[0] = s->iDx0; aRect[1] = s->iDy0; aRect[2] = s->iDx1; aRect[3] = s->iDy1;
		return 1;
		}

	EXPORT_C TInt PhvgReadback(TAny* aCookie, const TInt* aRect, TUint8* aOut)
		{
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpReadback; c.iCookie = aCookie; c.iRect = aRect; c.iOut = aOut;
		return Call(c);
		}

	EXPORT_C TInt PhvgReadDirect(TAny* aCookie, const TInt* aRect, TUint8* aOut)
		{
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpReadDirect; c.iCookie = aCookie; c.iRect = aRect; c.iOut = aOut;
		return Call(c);
		}

	EXPORT_C TInt PhvgPresent(TAny* aCookie, TUint8* aBase, TInt aStride, TInt aW, TInt aH)
		{
		if (gUp != 1 || !aBase || aStride <= 0 || aW <= 0 || aH <= 0)
			return 0;
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpPresent; c.iCookie = aCookie; c.iOut = aBase; c.iX = aStride; c.iW = aW; c.iH = aH;
		return Call(c);
		}

	EXPORT_C TInt PhvgDrawImage(const TPhvgImage* aImg)
		{
		TInt res = 0;
		if (gUp == 1 && aImg->iNRects >= 1 && aImg->iNRects <= KQRects && gProgImg)
			{
			RMutex lock;
			lock.SetHandle(gLockH);
			lock.Wait();
			TShadow* s = Find(aImg->iCookie);
			if (s && s->iFbo0 && s->iState != 0 && s->iW == aImg->iW && s->iH == aImg->iH)
				res = QueueImage(*aImg);
			lock.Signal();
			}
		if (res == 0)
			gStat[4]++;
		return res;
		}

	EXPORT_C void PhvgInvalidate(TAny* aCookie)
		{
		Drain();
		TShadow* s = Find(aCookie);
		if (s && s->iState)
			SetState(*s, 0);
		}

	EXPORT_C void PhvgForget(TAny* aCookie)
		{
		if (!Find(aCookie))
			return;
		TCmd c;
		Mem::FillZ(&c, sizeof(c));
		c.iOp = EOpForget; c.iCookie = aCookie;
		Call(c);
		}

	EXPORT_C void PhvgStats(TInt* aOut)
		{
		for (TInt i = 0; i < 7; i++)
			aOut[i] = gStat[i];
		aOut[7] = gLive;
		aOut[8] = gStat[8];
		aOut[9] = gStat[9];			// O4: gradient fills
		aOut[10] = gStat[10];		// O4: stroke fills
		aOut[11] = gStat[11];		// O4b: image draws
		}
	}
