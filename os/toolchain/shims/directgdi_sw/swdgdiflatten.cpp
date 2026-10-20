// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// swdgdiflatten.cpp -- OURS (2026-10-09, GPU plan G1c): extended bitmaps for the SOFTWARE DirectGDI adapter.
//
// The NGA window server draws every window through DirectGDI (windowserverplugins/openwfc directgdigcwrapper.cpp).
// The 2010 software adapter (directgdiadaptation/swsrc) refuses extended bitmaps -- "KErrNotSupported // Not
// supported for extended bitmaps" -- while S60's icon server makes every NVG icon (and skin graphic) one
// (uiresources AknIcon AknNvgFormatHandler.cpp CreateExtendedBitmap). The non-NGA window server drew them through
// BitGdi, which rasterizes extended bitmaps itself (CFbsRasterizer, fbsrasterizer.dll). So on the PinePhone's first
// NGA S60 boot every icon and skin frame was missing while text and the JPEG wallpaper drew (wsblit's 16 ordinary
// cases all correct). The hardware (OpenVG) adapter handles them; the software one is made to here: an extended
// bitmap is flattened by BitGdi into an ordinary bitmap of the same size + display mode (alpha kept: write-alpha),
// and that is drawn. The edited adapter files (tools/patch_directgdi_sw.py) call SwDgdiFlatten at each refusal.
//
// Cache: a flattened copy per (serial number, touch count) -- an icon is drawn on every redraw, rasterizing NVG each
// time would be slow. Round robin over KSlots; the window server is single-threaded (one engine per screen).
#include <e32std.h>
#include <fbs.h>
#include <bitdev.h>
#include <bitstd.h>

namespace
	{
	const TInt KSlots = 48;
	struct TSlot
		{
		TInt64 iSerial;
		TInt iTouch;
		CFbsBitmap* iFlat;
		};
	TSlot gSlots[KSlots];
	TInt gNext;
	}

// Returns an ordinary bitmap with aBmp's pixels (owned by the cache: valid until KSlots further flattens), or NULL.
const CFbsBitmap* SwDgdiFlatten(const CFbsBitmap& aBmp)
	{
	const TInt64 serial = aBmp.SerialNumber();
	const TInt touch = aBmp.TouchCount();
	for (TInt i = 0; i < KSlots; ++i)
		{
		if (gSlots[i].iFlat && gSlots[i].iSerial == serial && gSlots[i].iTouch == touch)
			return gSlots[i].iFlat;
		}
	const TSize size = aBmp.SizeInPixels();
	if (size.iWidth <= 0 || size.iHeight <= 0)
		return NULL;
	CFbsBitmap* flat = new CFbsBitmap;
	if (!flat)
		return NULL;
	TInt r = flat->Create(size, aBmp.DisplayMode());
	CFbsBitmapDevice* dev = NULL;
	if (r == KErrNone)
		TRAP(r, dev = CFbsBitmapDevice::NewL(flat));
	CFbsBitGc* gc = NULL;
	if (r == KErrNone)
		r = dev->CreateContext(gc);
	if (r == KErrNone)
		{
		gc->SetDrawMode(CGraphicsContext::EDrawModeWriteAlpha);
		gc->BitBlt(TPoint(0, 0), &aBmp);			// BitGdi rasterizes the extended bitmap (CFbsRasterizer)
		}
	delete gc;
	delete dev;
	if (r != KErrNone)
		{
		delete flat;
		return NULL;
		}
	TSlot& s = gSlots[gNext];
	gNext = (gNext + 1) % KSlots;
	delete s.iFlat;
	s.iSerial = serial;
	s.iTouch = touch;
	s.iFlat = flat;
	return flat;
	}
