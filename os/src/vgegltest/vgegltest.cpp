// vgegltest.cpp -- OURS: OpenVG O1 test (docs/phorepoc/openvg-plan.md). Creates a VG context through the STANDARD
// libEGL (eglBindAPI(EGL_OPENVG_API), pbuffer surface) + libOpenVG/libOpenVGU, draws a filled cubic path, a linear
// gradient rect (vguRect) and an alpha circle, reads the pixels back and prints "PH: vg" lines with a checksum.
// The same drawing is then made through the reference implementation DIRECTLY (libegl_sw.dll / libopenvg_sw.dll /
// libopenvgu_sw.dll by ordinal, no front) and the two checksums must match.
#include <e32std.h>
#include <e32debug.h>
#include <EGL/egl.h>
#include <VG/openvg.h>
#include <VG/vgu.h>

const TInt KW = 64, KH = 64;

// the VG entry points the drawing needs (so the same code runs on libOpenVG and on libopenvg_sw)
struct TVg
	{
	void (*SetFv)(VGParamType, VGint, const VGfloat*);
	void (*Clear)(VGint, VGint, VGint, VGint);
	VGPaint (*CreatePaint)(void);
	void (*DestroyPaint)(VGPaint);
	void (*SetParameterfv)(VGHandle, VGint, VGint, const VGfloat*);
	void (*SetParameteri)(VGHandle, VGint, VGint);
	void (*SetPaint)(VGPaint, VGbitfield);
	VGPath (*CreatePath)(VGint, VGPathDatatype, VGfloat, VGfloat, VGint, VGint, VGbitfield);
	void (*DestroyPath)(VGPath);
	void (*AppendPathData)(VGPath, VGint, const VGubyte*, const void*);
	void (*DrawPath)(VGPath, VGbitfield);
	void (*ReadPixels)(void*, VGint, VGImageFormat, VGint, VGint, VGint, VGint);
	void (*Finish)(void);
	VGErrorCode (*GetError)(void);
	VGUErrorCode (*Ellipse)(VGPath, VGfloat, VGfloat, VGfloat, VGfloat);
	VGUErrorCode (*Rect)(VGPath, VGfloat, VGfloat, VGfloat, VGfloat);
	};

static TUint32 Draw(const TVg& v, TUint32* aPix)
	{
	const VGfloat clear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	v.SetFv(VG_CLEAR_COLOR, 4, clear);
	v.Clear(0, 0, KW, KH);

	// 1. a filled path with a cubic, flat colour
	VGPaint flat = v.CreatePaint();
	const VGfloat orange[4] = { 1.0f, 0.5f, 0.0f, 1.0f };
	v.SetParameteri(flat, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	v.SetParameterfv(flat, VG_PAINT_COLOR, 4, orange);
	v.SetPaint(flat, VG_FILL_PATH);
	VGPath p = v.CreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	const VGubyte seg[] = { VG_MOVE_TO_ABS, VG_LINE_TO_ABS, VG_CUBIC_TO_ABS, VG_CLOSE_PATH };
	const VGfloat data[] = { 4.f, 4.f,  30.f, 6.f,  36.f, 20.f, 30.f, 40.f, 12.f, 58.f,  };
	v.AppendPathData(p, 4, seg, data);
	v.DrawPath(p, VG_FILL_PATH);

	// 2. a linear gradient across a rect (VGU)
	VGPaint grad = v.CreatePaint();
	v.SetParameteri(grad, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
	const VGfloat line[4] = { 32.f, 0.f, 64.f, 0.f };
	v.SetParameterfv(grad, VG_PAINT_LINEAR_GRADIENT, 4, line);
	const VGfloat stops[10] = { 0.0f, 1.f, 0.f, 0.f, 1.f,  1.0f, 0.f, 0.f, 1.f, 1.f };
	v.SetParameterfv(grad, VG_PAINT_COLOR_RAMP_STOPS, 10, stops);
	v.SetPaint(grad, VG_FILL_PATH);
	VGPath r = v.CreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	v.Rect(r, 32.f, 8.f, 28.f, 24.f);
	v.DrawPath(r, VG_FILL_PATH);

	// 3. a half-transparent ellipse over both
	VGPaint al = v.CreatePaint();
	const VGfloat green[4] = { 0.0f, 1.0f, 0.0f, 0.5f };
	v.SetParameterfv(al, VG_PAINT_COLOR, 4, green);
	v.SetPaint(al, VG_FILL_PATH);
	VGPath e = v.CreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	v.Ellipse(e, 32.f, 40.f, 40.f, 30.f);
	v.DrawPath(e, VG_FILL_PATH);

	v.Finish();
	v.ReadPixels(aPix, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
	const TInt err = v.GetError();
	v.DestroyPath(p); v.DestroyPath(r); v.DestroyPath(e);
	v.DestroyPaint(flat); v.DestroyPaint(grad); v.DestroyPaint(al);

	TUint32 sum = 2166136261u;				// FNV-1a over the pixel words
	for (TInt i = 0; i < KW * KH; i++)
		{
		sum ^= aPix[i];
		sum *= 16777619u;
		}
	RDebug::Printf("PH: vg draw: checksum %08x err=0x%x  px(10,10)=%08x px(48,16)=%08x px(32,40)=%08x",
	               sum, err, aPix[10 * KW + 10], aPix[16 * KW + 48], aPix[40 * KW + 32]);
	return sum;
	}

template<class T> static void Get(T& aDst, RLibrary& aLib, TInt aOrd)
	{
	aDst = (T)aLib.Lookup(aOrd);
	}

// ordinals: libopenvg11u.def / libopenvgu11u.def
static TBool FillSw(TVg& v, RLibrary& aVg, RLibrary& aVgu)
	{
	Get(v.SetFv, aVg, 70);		Get(v.Clear, aVg, 5);			Get(v.CreatePaint, aVg, 13);
	Get(v.DestroyPaint, aVg, 16);	Get(v.SetParameterfv, aVg, 65);	Get(v.SetParameteri, aVg, 66);
	Get(v.SetPaint, aVg, 63);	Get(v.CreatePath, aVg, 14);		Get(v.DestroyPath, aVg, 17);
	Get(v.AppendPathData, aVg, 3);	Get(v.DrawPath, aVg, 19);		Get(v.ReadPixels, aVg, 57);
	Get(v.Finish, aVg, 20);		Get(v.GetError, aVg, 24);
	Get(v.Ellipse, aVgu, 5);	Get(v.Rect, aVgu, 8);
	return v.SetFv && v.Clear && v.DrawPath && v.ReadPixels && v.Ellipse && v.Rect && v.AppendPathData;
	}

static TUint32 ViaFront(TUint32* aPix)
	{
	TVg v;
	v.SetFv = vgSetfv;		v.Clear = vgClear;		v.CreatePaint = vgCreatePaint;
	v.DestroyPaint = vgDestroyPaint;	v.SetParameterfv = vgSetParameterfv;	v.SetParameteri = vgSetParameteri;
	v.SetPaint = vgSetPaint;	v.CreatePath = vgCreatePath;	v.DestroyPath = vgDestroyPath;
	v.AppendPathData = vgAppendPathData;	v.DrawPath = vgDrawPath;	v.ReadPixels = vgReadPixels;
	v.Finish = vgFinish;		v.GetError = vgGetError;	v.Ellipse = vguEllipse;		v.Rect = vguRect;

	RDebug::Printf("PH: vg front: eglQueryAPI before bind = 0x%x", eglQueryAPI());
	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint maj = 0, min = 0;
	EGLBoolean ok = eglInitialize(dpy, &maj, &min);
	RDebug::Printf("PH: vg front: eglInitialize %d (%d.%d)", ok, maj, min);
	ok = eglBindAPI(EGL_OPENVG_API);
	RDebug::Printf("PH: vg front: eglBindAPI(OPENVG) %d, eglQueryAPI=0x%x", ok, eglQueryAPI());
	if (!ok)
		return 0;
	const EGLint cfgAttr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
	                           EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	EGLConfig cfg = 0;
	EGLint n = 0;
	ok = eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n);
	RDebug::Printf("PH: vg front: eglChooseConfig %d n=%d", ok, n);
	if (!ok || n < 1)
		return 0;
	const EGLint pbAttr[] = { EGL_WIDTH, KW, EGL_HEIGHT, KH, EGL_NONE };
	EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbAttr);
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
	RDebug::Printf("PH: vg front: surface %x context %x err 0x%x", (TUint)surf, (TUint)ctx, eglGetError());
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT)
		return 0;
	ok = eglMakeCurrent(dpy, surf, surf, ctx);
	RDebug::Printf("PH: vg front: eglMakeCurrent %d, current ctx %s", ok, eglGetCurrentContext() == ctx ? "ok" : "WRONG");
	const TUint32 sum = ok ? Draw(v, aPix) : 0;
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(dpy, surf);
	eglDestroyContext(dpy, ctx);
	eglReleaseThread();
	return sum;
	}

// the same through libegl_sw.dll / libopenvg_sw.dll / libopenvgu_sw.dll directly (libegl13U.def ordinals)
static TUint32 ViaReference(TUint32* aPix)
	{
	RLibrary egl, vg, vgu;
	_LIT(KEgl, "libegl_sw.dll");
	_LIT(KVg, "libopenvg_sw.dll");
	_LIT(KVgu, "libopenvgu_sw.dll");
	if (egl.Load(KEgl) || vg.Load(KVg) || vgu.Load(KVgu))
		{
		RDebug::Printf("PH: vg ref: a reference library did not load");
		return 0;
		}
	TVg v;
	if (!FillSw(v, vg, vgu))
		{
		RDebug::Printf("PH: vg ref: lookup failed");
		return 0;
		}
	typedef EGLDisplay (*TGetDisplay)(EGLNativeDisplayType);
	typedef EGLBoolean (*TInit)(EGLDisplay, EGLint*, EGLint*);
	typedef EGLBoolean (*TBind)(EGLenum);
	typedef EGLBoolean (*TChoose)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
	typedef EGLSurface (*TPbuf)(EGLDisplay, EGLConfig, const EGLint*);
	typedef EGLContext (*TCtx)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
	typedef EGLBoolean (*TMake)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
	typedef EGLBoolean (*TDelS)(EGLDisplay, EGLSurface);
	typedef EGLBoolean (*TDelC)(EGLDisplay, EGLContext);
	EGLDisplay dpy = ((TGetDisplay)egl.Lookup(18))(EGL_DEFAULT_DISPLAY);
	EGLint a = 0, b = 0;
	((TInit)egl.Lookup(21))(dpy, &a, &b);
	((TBind)egl.Lookup(2))(EGL_OPENVG_API);
	const EGLint cfgAttr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
	                           EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	EGLConfig cfg = 0;
	EGLint n = 0;
	((TChoose)egl.Lookup(4))(dpy, cfgAttr, &cfg, 1, &n);
	const EGLint pbAttr[] = { EGL_WIDTH, KW, EGL_HEIGHT, KH, EGL_NONE };
	EGLSurface surf = ((TPbuf)egl.Lookup(8))(dpy, cfg, pbAttr);
	EGLContext ctx = ((TCtx)egl.Lookup(6))(dpy, cfg, EGL_NO_CONTEXT, NULL);
	TUint32 sum = 0;
	if (n > 0 && surf && ctx && ((TMake)egl.Lookup(22))(dpy, surf, surf, ctx))
		{
		sum = Draw(v, aPix);
		((TMake)egl.Lookup(22))(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		}
	else
		RDebug::Printf("PH: vg ref: context setup failed (n=%d)", n);
	((TDelS)egl.Lookup(12))(dpy, surf);
	((TDelC)egl.Lookup(11))(dpy, ctx);
	return sum;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("PH: vg start (O1: libEGL + libOpenVG/libOpenVGU vs the reference implementation)");
	static TUint32 pixA[KW * KH], pixB[KW * KH];
	const TUint32 a = ViaFront(pixA);
	const TUint32 b = ViaReference(pixB);
	TInt diff = 0;
	for (TInt i = 0; i < KW * KH; i++)
		if (pixA[i] != pixB[i])
			diff++;
	const TBool pass = a && a == b && diff == 0;
	RDebug::Printf("PH: vg result: front %08x reference %08x differing pixels %d -> %s", a, b, diff, pass ? "PASS" : "FAIL");
	return pass ? KErrNone : KErrGeneral;
	}
