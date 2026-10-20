// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// vgprobe.cpp -- OURS: probe of the software OpenVG stack the S60 vector pipeline sits on
// (libvgi -> libegl_sw -> sflibopenvgrefimpl, the Khronos reference implementation).
// VGI surface -> clear -> fill a path -> VGISymbianCopyToBitmap -> print pixels to the debug port.
#include <e32std.h>
#include <e32debug.h>
#include <fbs.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include <vg/vgcontext.h>
#include <vg/vgcontext_symbian.h>

static void Pixel(CFbsBitmap* aBmp, const char* aWhat, TInt aX, TInt aY)
	{
	TRgb c;
	aBmp->GetPixel(c, TPoint(aX, aY));
	RDebug::Printf("[vgprobe] %s (%d,%d) = r%d g%d b%d a%d", aWhat, aX, aY, c.Red(), c.Green(), c.Blue(), c.Alpha());
	}

static void RunL()
	{
	User::LeaveIfError(RFbsSession::Connect());
	const TSize size(64, 64);
	TInt r = VGISymbianInitialize(size, VGI_COLORSPACE_SRGB);
	RDebug::Printf("[vgprobe] VGISymbianInitialize -> %d", r);
	User::LeaveIfError(r);

	VGfloat clear[4] = { 1.0f, 0.0f, 0.0f, 1.0f };			// opaque red
	vgSetfv(VG_CLEAR_COLOR, 4, clear);
	vgClear(0, 0, size.iWidth, size.iHeight);
	RDebug::Printf("[vgprobe] vgClear error=0x%x", vgGetError());

	VGPaint paint = vgCreatePaint();
	VGfloat blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
	vgSetParameterfv(paint, VG_PAINT_COLOR, 4, blue);
	vgSetPaint(paint, VG_FILL_PATH);
	VGPath path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	vguRect(path, 16.0f, 16.0f, 32.0f, 32.0f);				// blue square in the middle
	vgDrawPath(path, VG_FILL_PATH);
	RDebug::Printf("[vgprobe] vgDrawPath error=0x%x", vgGetError());

	// every target mode the S60 icon rasterizer asks for (16MA/16MU, 64K pixels, Gray256 masks)
	const TDisplayMode modes[] = { EColor16MA, EColor16MU, EColor64K, EGray256 };
	for (TUint i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
		{
		CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
		CleanupStack::PushL(bmp);
		User::LeaveIfError(bmp->Create(size, modes[i]));
		CFbsBitmap* mask = new (ELeave) CFbsBitmap;
		CleanupStack::PushL(mask);
		User::LeaveIfError(mask->Create(size, EGray256));
		r = VGISymbianCopyToBitmap(bmp, mask, VGI_COPY_TRANSPARENT_PIXELS);
		RDebug::Printf("[vgprobe] mode %d: VGISymbianCopyToBitmap -> %d", modes[i], r);
		Pixel(bmp, "corner (expect red)", 2, 2);
		Pixel(bmp, "centre (expect blue)", 32, 32);
		Pixel(mask, "mask centre (expect white)", 32, 32);
		CleanupStack::PopAndDestroy(2, bmp);
		}

	// X4.73: frame-slice edges. A 9-slice skin frame is drawn as separate bitmaps that must meet at full opacity;
	// a rect covering the WHOLE surface (the viewBox of a slice), at 1:1 and stretched by a non-integer factor
	// (60 units -> 64 px, as a slice is scaled to its layout size), must leave every edge pixel opaque
	const VGfloat scales[] = { 1.0f, 64.0f / 60.0f };
	for (TUint s = 0; s < sizeof(scales) / sizeof(scales[0]); s++)
		{
		VGfloat none[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		vgSetfv(VG_CLEAR_COLOR, 4, none);
		vgClear(0, 0, size.iWidth, size.iHeight);
		vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
		vgLoadIdentity();
		vgScale(scales[s], scales[s]);
		VGPath full = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
		const VGfloat extent = size.iWidth / scales[s];
		vguRect(full, 0.0f, 0.0f, extent, extent);
		vgDrawPath(full, VG_FILL_PATH);
		CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
		CleanupStack::PushL(bmp);
		User::LeaveIfError(bmp->Create(size, EColor16MA));
		CFbsBitmap* mask = new (ELeave) CFbsBitmap;
		CleanupStack::PushL(mask);
		User::LeaveIfError(mask->Create(size, EGray256));
		r = VGISymbianCopyToBitmap(bmp, mask, VGI_COPY_TRANSPARENT_PIXELS);
		RDebug::Printf("[vgprobe] edge test scale %d/1000: copy -> %d", (TInt)(scales[s] * 1000), r);
		Pixel(mask, "edge mask left", 0, 32);
		Pixel(mask, "edge mask right", size.iWidth - 1, 32);
		Pixel(mask, "edge mask top", 32, 0);
		Pixel(mask, "edge mask bottom", 32, size.iHeight - 1);
		Pixel(mask, "edge mask inside", 32, 32);
		CleanupStack::PopAndDestroy(2, bmp);
		vgDestroyPath(full);
		}

	vgDestroyPath(path);
	vgDestroyPaint(paint);
	VGISymbianTerminate();
	RFbsSession::Disconnect();
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, RunL());
	RDebug::Printf("[vgprobe] done, err=%d", err);
	delete cleanup;
	return err;
	}
