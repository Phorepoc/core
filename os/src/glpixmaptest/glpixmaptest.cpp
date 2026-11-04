// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// glpixmaptest.cpp -- OURS: an RSgImage as an EGL pixmap surface (EGL_NOK_pixmap_type_rsgimage), the contract the graphics stack's
// compositors and apps rely on. One image, one GLES 1.1 context, four checks of EGL 1.4 section 3.9's native-rendering rules:
//   1. the image's initial contents are what GL sees (glReadPixels after the context is made current);
//   2. GL clears the pixmap, eglWaitClient: the image's memory holds the colour;
//   3. the CPU fills the image, eglWaitNative: GL reads the CPU's colour;
//   4. GL clears again, eglSwapBuffers (a no-op for a pixmap): the image holds that colour too (the implementation may be early).
// Result line: "[glpixmaptest] PASS" / "FAIL".
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <sgresource/sgresource.h>
#include <sgresource/sgimage.h>
#include <sgresource/sgimage_sw.h>
#include <EGL/egl.h>
#include <GLES/gl.h>

static const TInt KSize = 64;
static TInt gFail = 0;

static void Check(const char* aWhat, TUint32 aGot, TUint32 aWant)
	{
	const TBool ok = aGot == aWant;
	RDebug::Printf("[glpixmaptest] %s: got %08x want %08x %s", aWhat, aGot, aWant, ok ? "ok" : "FAIL");
	if (!ok)
		++gFail;
	}

static TUint32 ImagePixel(const MSgImage_Sw* aSw, TInt aX, TInt aY)
	{
	return *(const TUint32*)((const TUint8*)aSw->DataAddress() + aY * aSw->DataStride() + aX * 4);
	}

static TUint32 GlPixel(TInt aX, TInt aY)
	{
	TUint8 p[4] = { 0, 0, 0, 0 };
	glReadPixels(aX, aY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
	return (TUint32)p[3] << 24 | (TUint32)p[0] << 16 | (TUint32)p[1] << 8 | p[2];		// as BGRA bytes read as a dword: A R G B
	}

static void MainL()
	{
	RSgDriver drv;
	const TInt dr = drv.Open();
	if (dr != KErrNone)
		{
		// (the PC image ships the null graphics resource provider: no RSgImage exists there, so there is nothing to test)
		RDebug::Printf("[glpixmaptest] SKIP: RSgDriver::Open = %d (no graphics resource provider on this image)", dr);
		return;
		}
	TUint32 init[KSize * KSize];
	for (TInt i = 0; i < KSize * KSize; ++i)
		init[i] = 0xFF336699u;
	RSgImage image;
	const TSgImageInfo info(TSize(KSize, KSize), ESgPixelFormatARGB_8888, ESgUsageBitOpenGlesSurface | ESgUsageBitOpenGlesTexture2D);
	User::LeaveIfError(image.Create(info, init, KSize * 4));
	TAny* itf = NULL;
	User::LeaveIfError(image.GetInterface(TUid::Uid(MSgImage_Sw::EInterfaceUid), itf));
	const MSgImage_Sw* sw = static_cast<const MSgImage_Sw*>(itf);

	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint major = 0, minor = 0;
	if (!eglInitialize(dpy, &major, &minor))
		User::Leave(KErrNotSupported);
	const EGLint cfgAttr[] = { EGL_SURFACE_TYPE, EGL_PIXMAP_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	EGLConfig cfg;
	EGLint n = 0;
	if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1)
		{
		RDebug::Printf("[glpixmaptest] eglChooseConfig failed 0x%x", eglGetError());
		User::Leave(KErrNotSupported);
		}
	const EGLint pixAttr[] = { EGL_NONE };
	EGLSurface surf = eglCreatePixmapSurface(dpy, cfg, (EGLNativePixmapType)&image, pixAttr);
	if (surf == EGL_NO_SURFACE)
		{
		RDebug::Printf("[glpixmaptest] eglCreatePixmapSurface failed 0x%x", eglGetError());
		User::Leave(KErrGeneral);
		}
	eglBindAPI(EGL_OPENGL_ES_API);
	const EGLint ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
	if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx))
		{
		RDebug::Printf("[glpixmaptest] context / make current failed 0x%x", eglGetError());
		User::Leave(KErrGeneral);
		}
	RDebug::Printf("[glpixmaptest] GL_RENDERER '%s'", glGetString(GL_RENDERER));

	Check("1 initial contents seen by GL", GlPixel(KSize / 2, KSize / 2), 0xFF336699u);

	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	eglWaitClient();
	Check("2 GL clear red, image centre", ImagePixel(sw, KSize / 2, KSize / 2), 0xFFFF0000u);
	Check("2 GL clear red, image corner", ImagePixel(sw, 0, KSize - 1), 0xFFFF0000u);

	for (TInt y = 0; y < KSize; ++y)
		for (TInt x = 0; x < KSize; ++x)
			*(TUint32*)((TUint8*)sw->DataAddress() + y * sw->DataStride() + x * 4) = 0xFF00FF00u;
	eglWaitNative(EGL_CORE_NATIVE_ENGINE);
	Check("3 CPU fill green, GL reads", GlPixel(KSize / 3, KSize / 3), 0xFF00FF00u);

	glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(dpy, surf);
	eglWaitClient();
	Check("4 GL clear blue, image centre", ImagePixel(sw, KSize / 2, KSize / 2), 0xFF0000FFu);

	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(dpy, ctx);
	eglDestroySurface(dpy, surf);
	eglTerminate(dpy);
	image.Close();
	drv.Close();
	RDebug::Printf("[glpixmaptest] %s", gFail ? "FAIL" : "PASS");
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RDebug::Printf("[glpixmaptest] start");
	TRAPD(err, MainL());
	RDebug::Printf("[glpixmaptest] exit err=%d", err);
	delete cleanup;
	return err;
	}
