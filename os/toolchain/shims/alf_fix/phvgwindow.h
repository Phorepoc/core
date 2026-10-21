/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phvgwindow.h -- OURS: an EGL "window surface" for the alf server's RWindow on the software OpenVG EGL (libEGL_sw = riMiniEGL).
//
// Why: Hitchcock's CHuiVg10RenderSurface::CreateSurface calls eglCreateWindowSurface(display, config, RDrawableWindow*, ...).
// The 2010 reference EGL (sfopenvg riEGLOS.cpp, which libEGL's OpenVG path forwards to) does not take an RWindow: its native
// window type is TNativeWindowType { TSize iSize; CFbsBitmap* iBitmap; }, and eglSwapBuffers vgReadPixels-es into the bitmap
// (the application then draws the bitmap itself). Handed an RWindow it reads the window object's first words as a width and
// height -> a giant Drawable allocation: the alfredserver thread never returned from CreateSurface (S2, 2026-10-16).
//
// Route (the 2010 NGA recipe, adapt.SimulatorGraphics guestegl.cpp; our symgl_window.cpp does the same for GLES): the window gets
// a SurfaceManager surface as its BACKGROUND SURFACE, which the compositor stage (alfopenwfrs / libWFC) composes like any
// window; the RI draws into a CFbsBitmap through the reference window type and swap copies it (premultiplied) into the surface
// and SubmitUpdate()s it. Used only by huirenderpluginvg (the ALF components); nothing in the default ROM includes this.
#ifndef PHVGWINDOW_H
#define PHVGWINDOW_H

#include <e32std.h>
#include <e32debug.h>
#include <w32std.h>
#include <hal.h>
#include <fbs.h>
#include <graphics/surface.h>
#include <graphics/surfacemanager.h>
#include <graphics/surfaceupdateclient.h>
#include <graphics/surfaceconfiguration.h>
#include <pixelformats.h>
#include <EGL/egl.h>
#include <VG/openvg.h>
#include "ph_dirty.h"

struct TPhRiWindow				// == sfopenvg's TNativeWindowType (symbian/eglosnativewindowtype.h)
	{
	TSize iSize;
	CFbsBitmap* iBitmap;
	};

class TPhVgWin
	{
public:
	EGLSurface iSurf;
	RWindow* iWin;
	TPhRiWindow iRi;
	RSurfaceManager iSm;
	RChunk iChunk;
	TSurfaceId iSid;
	RSurfaceUpdateSession iSus;
	TInt iStride;
	TInt iScreen;
	TInt iSwaps;
	TBool iOk;
	TBool iTried;
	};

static TPhVgWin* gPhVgWins[8];

// The dirty rectangles of the frame CHuiDisplay::Refresh just drew (ph_dirty.h: the list written by HuiDisplay_ph.cpp, or else the merged rectangle that
// CHuiVg10RenderSurface::SetDirtyRect stores); PhVgSwapBuffers consumes them: the swap then copies and submits only those rectangles.
static inline TUint PhVgUs(TUint aTicks)
	{
	static TInt freq;
	if (!freq)
		{
		HAL::Get(HAL::EFastCounterFrequency, freq);
		RDebug::Printf("PH: alf: fast counter frequency %d Hz (the swap timings below are in microseconds)", freq);
		}
	return freq ? (TUint)((TUint64)aTicks * 1000000 / freq) : 0;
	}

static inline TPhVgWin* PhVgFind(EGLSurface aSurf)
	{
	for (TInt i = 0; i < 8; i++)
		if (gPhVgWins[i] && gPhVgWins[i]->iSurf == aSurf)
			return gPhVgWins[i];
	return NULL;
	}

static inline void PhVgFree(TPhVgWin* w)
	{
	for (TInt i = 0; i < 8; i++)
		if (gPhVgWins[i] == w)
			gPhVgWins[i] = NULL;
	if (w->iOk)
		{
		w->iWin->RemoveBackgroundSurface(ETrue);
		w->iWin->Session()->UnregisterSurface(w->iScreen, w->iSid);
		}
	w->iSus.Close();
	w->iChunk.Close();
	if (!w->iSid.IsNull())
		w->iSm.CloseSurface(w->iSid);
	w->iSm.Close();
	delete w->iRi.iBitmap;
	delete w;
	}

// the NGA background surface of the window (ARGB8888 premultiplied, one buffer, rows 16-pixel aligned like symgl_window)
static inline TInt PhVgOpenSurface(TPhVgWin& w)
	{
	RDebug::Printf("PH: alf: vgwin surface setup (first swap, alf scheduler running)");
	TInt r = w.iSm.Open();
	RDebug::Printf("PH: alf: vgwin RSurfaceManager::Open -> %d", r);
	if (r != KErrNone)
		return r;
	RSurfaceManager::TSurfaceCreationAttributesBuf buf;
	RSurfaceManager::TSurfaceCreationAttributes& a = buf();
	a.iSize = w.iRi.iSize;
	a.iBuffers = 1;
	a.iPixelFormat = EUidPixelFormatARGB_8888_PRE;
	a.iStride = 4 * ((w.iRi.iSize.iWidth + 15) & ~15);
	a.iOffsetToFirstBuffer = 0;
	a.iAlignment = RSurfaceManager::EPageAligned;
	a.iHintCount = 0;
	a.iSurfaceHints = NULL;
	a.iOffsetBetweenBuffers = 0;
	a.iContiguous = ETrue;
	a.iCacheAttrib = RSurfaceManager::ECached;
	a.iMappable = ETrue;
	if ((r = w.iSm.CreateSurface(buf, w.iSid)) != KErrNone)
		return r;
	if ((r = w.iSm.MapSurface(w.iSid, w.iChunk)) != KErrNone)
		return r;
	RDebug::Printf("PH: alf: vgwin surface created + mapped (stride %d)", a.iStride);
	w.iStride = a.iStride;
	w.iScreen = w.iWin->ScreenNumber();
	RDebug::Printf("PH: alf: vgwin -> RegisterSurface (WSERV round trip)");
	if ((r = w.iWin->Session()->RegisterSurface(w.iScreen, w.iSid)) != KErrNone)
		return r;
	RDebug::Printf("PH: alf: vgwin RegisterSurface ok -> SetBackgroundSurface");
	TSurfaceConfiguration cfg;
	cfg.SetSurfaceId(w.iSid);
	if ((r = w.iWin->SetBackgroundSurface(cfg, ETrue)) != KErrNone)
		return r;
	RDebug::Printf("PH: alf: vgwin SetBackgroundSurface ok -> Flush");
	w.iWin->Session()->Flush();
	if ((r = w.iSus.Connect()) != KErrNone)
		return r;
	return KErrNone;
	}

// eglCreateWindowSurface for an RWindow of the given size (CHuiDisplay's, already known). NO call into WSERV here: the alfredserver
// thread is inside CAlfAppUi::ConstructL while WSERV is already drawing (S2, 2026-10-16: the first synchronous round trip -- RWindow::Size --
// never returned). The window's background surface is set up at the first swap, from the running scheduler.
static inline EGLSurface PhVgCreateWindowSurface(EGLDisplay aDpy, EGLConfig aCfg, RDrawableWindow* aWin, const TSize& aSize, const EGLint* aAttr)
	{
	RDebug::Printf("PH: alf: vgwin create: RWindow %x size %dx%d (no WSERV call)", (TUint)aWin, aSize.iWidth, aSize.iHeight);
	TPhVgWin* w = new TPhVgWin;
	if (!w)
		return EGL_NO_SURFACE;
	Mem::FillZ(w, sizeof(TPhVgWin));
	w->iSid = TSurfaceId::CreateNullId();
	w->iWin = static_cast<RWindow*>(aWin);
	w->iRi.iSize = aSize;
	if (aSize.iWidth <= 0 || aSize.iHeight <= 0)
		{
		delete w;
		return EGL_NO_SURFACE;
		}
	w->iRi.iBitmap = new CFbsBitmap;
	TInt r = w->iRi.iBitmap ? w->iRi.iBitmap->Create(aSize, EColor16MA) : KErrNoMemory;
	RDebug::Printf("PH: alf: vgwin CFbsBitmap::Create -> %d", r);
	if (r != KErrNone)
		{
		PhVgFree(w);
		return EGL_NO_SURFACE;
		}
	// the RI reads the window size from, and swaps into, the reference native window type. sflibopenvgrefimpl is built with
	// throw = User::Leave(-4) and try/catch removed, so an allocation failure in the Drawable (width*height*SAMPLES*4 bytes) is a
	// LEAVE through this frame: trap it and say so. (Facts first: the config's samples and this thread's heap.)
	EGLint samples = -1, sbuf = -1;
	eglGetConfigAttrib(aDpy, aCfg, EGL_SAMPLES, &samples);
	eglGetConfigAttrib(aDpy, aCfg, EGL_SAMPLE_BUFFERS, &sbuf);
	TInt big = 0;
	RDebug::Printf("PH: alf: vgwin config %x samples %d sample_buffers %d; heap size %d, largest free %d; calling the RI eglCreateWindowSurface",
		(TUint)aCfg, samples, sbuf, User::Heap().Size(), User::Heap().Available(big));
	// Hitchcock (SYMBIAN_GRAPHICS_WSERV_QT_EFFECTS, defined by the S^3 variant) asks for EGL_SINGLE_BUFFER; the reference EGL "ignores the
	// renderBuffer parameter" but then asserts EGL_BACK_BUFFER in RIEGLSurface's constructor (libc assert -> abort -> the thread never returned).
	// Our window is always "back buffer then blit": take that attribute out.
	EGLint attrs[16];
	TInt na = 0;
	for (TInt i = 0; aAttr && aAttr[i] != EGL_NONE && na < 12; i += 2)
		if (aAttr[i] != EGL_RENDER_BUFFER)
			{
			attrs[na++] = aAttr[i];
			attrs[na++] = aAttr[i + 1];
			}
	attrs[na] = EGL_NONE;
	EGLSurface riSurf = EGL_NO_SURFACE;
	TRAPD(trapErr, riSurf = eglCreateWindowSurface(aDpy, aCfg, (EGLNativeWindowType)&w->iRi, attrs));
	if (trapErr != KErrNone)
		RDebug::Printf("PH: alf: vgwin the RI LEFT with %d (its allocation 'throw')", trapErr);
	w->iSurf = riSurf;
	RDebug::Printf("PH: alf: vgwin eglCreateWindowSurface(RI window) -> %x", (TUint)w->iSurf);
	if (w->iSurf == EGL_NO_SURFACE)
		{
		PhVgFree(w);
		return EGL_NO_SURFACE;
		}
	for (TInt i = 0; i < 8; i++)
		if (!gPhVgWins[i])
			{
			gPhVgWins[i] = w;
			return w->iSurf;
			}
	eglDestroySurface(aDpy, w->iSurf);
	PhVgFree(w);
	return EGL_NO_SURFACE;
	}

// eglSwapBuffers: the RI blits into the bitmap; copy that (premultiplying) into the window's surface and submit it
static inline EGLBoolean PhVgSwapBuffers(EGLDisplay aDpy, EGLSurface aSurf, EGLContext aCtx)
	{
	// FAST PATH (only the dirty rectangle): no eglSwapBuffers (its OSBlitToWindow reads the WHOLE drawable into the CFbsBitmap, and the loop below premultiplies the whole
	// bitmap into the surface): vgReadPixels the rectangle straight into the NGA surface (premultiplied sARGB, rows flipped with a negative stride exactly like
	// ReadPixelsToCFbsBitmap) and SubmitUpdate that region. The surface keeps the rest of the picture (the window surface is preserved). The first swaps, the diagnostic
	// swaps and any swap without a dirty rectangle take the full copy below.
	static TUint gSwapCount;
	const TUint sc = ++gSwapCount;
	const TBool diagSwap = (sc == 1 || sc == 5 || sc == 20 || (sc & 127) == 0);
	TPhVgWin* w0 = PhVgFind(aSurf);
	TPhDirty& dirty = PhDirty();
	if (w0 && w0->iOk && !diagSwap && w0->iSwaps >= 3 && (dirty.iN > 0 || dirty.iMergedValid) && eglGetCurrentSurface(EGL_DRAW) == aSurf)
		{
		const TInt wd0 = w0->iRi.iSize.iWidth, ht0 = w0->iRi.iSize.iHeight;
		TRect rects[KPhDirtyMax];
		TInt nr = 0;
		if (dirty.iN > 0)
			{
			for (TInt i = 0; i < dirty.iN; i++)
				{
				TRect r(dirty.iRect[i][0], dirty.iRect[i][1], dirty.iRect[i][2], dirty.iRect[i][3]);
				r.Intersection(TRect(0, 0, wd0, ht0));
				if (!r.IsEmpty())
					rects[nr++] = r;
				}
			}
		else
			{
			TRect r(dirty.iMerged[0], dirty.iMerged[1], dirty.iMerged[2], dirty.iMerged[3]);
			r.Intersection(TRect(0, 0, wd0, ht0));
			if (!r.IsEmpty())
				rects[nr++] = r;
			}
		dirty.iN = 0;
		dirty.iMergedValid = EFalse;
		if (nr == 0)
			return EGL_TRUE;
		const TUint t0 = User::FastCounter();
		TInt pixels = 0;
		TRegionFix<KPhDirtyMax> reg;
		for (TInt i = 0; i < nr; i++)
			{
			const TRect& r = rects[i];
			TUint8* p = w0->iChunk.Base() + (r.iBr.iY - 1) * w0->iStride + r.iTl.iX * 4;
			vgReadPixels(p, -w0->iStride, VG_sARGB_8888_PRE, r.iTl.iX, ht0 - r.iBr.iY, r.Width(), r.Height());
			reg.AddRect(r);
			pixels += r.Width() * r.Height();
			}
		const TUint t1 = User::FastCounter();
		const TInt rc = w0->iSus.SubmitUpdate(w0->iScreen, w0->iSid, 0, &reg);
		const TUint t2 = User::FastCounter();
		++w0->iSwaps;
		static TUint nfast;
		if (++nfast <= 6 || (nfast & 15) == 0)
			RDebug::Printf("PH: alf: vgwin FAST swap #%u: %d rects, %d px (first %d,%d-%d,%d): vgReadPixels %u us, SubmitUpdate %u us (rc %d)",
				nfast, nr, pixels, rects[0].iTl.iX, rects[0].iTl.iY, rects[0].iBr.iX, rects[0].iBr.iY, PhVgUs(t1 - t0), PhVgUs(t2 - t1), rc);
		return EGL_TRUE;
		}
	dirty.iN = 0;
	dirty.iMergedValid = EFalse;
	const TUint tFull0 = User::FastCounter();
	{	// what is drawn on the window surface RIGHT BEFORE the swap, as the VG context sees it (and which surface is current)
	static TUint nb;
	++nb;
	if (nb == 1 || nb == 5 || nb == 20 || (nb & 127) == 0)
		{
		TPhVgWin* pw = PhVgFind(aSurf);
		const TInt W = pw ? pw->iRi.iSize.iWidth : 0, H = pw ? pw->iRi.iSize.iHeight : 0;
		TUint8* tmp = (W > 0 && H > 0) ? (TUint8*)User::Alloc(W * H * 4) : NULL;
		TUint nz = 0, nzA = 0;
		if (tmp)
			{
			vgReadPixels(tmp, W * 4, VG_sARGB_8888, 0, 0, W, H);
			for (TInt i = 0; i < W * H; i++)
				{
				const TUint32 v = ((const TUint32*)tmp)[i];
				if (v & 0x00ffffff) nz++;
				if (v >> 24) nzA++;
				}
			User::Free(tmp);
			}
		RDebug::Printf("PH: alf: vgwin BEFORE swap #%u: current draw surface %x (window %x) ctx %x; vgReadPixels: rgb!=0 %u alpha!=0 %u of %d", nb, (unsigned)eglGetCurrentSurface(EGL_DRAW), (unsigned)aSurf, (unsigned)eglGetCurrentContext(), nz, nzA, W * H);
		}
	}
	EGLBoolean ok = eglSwapBuffers(aDpy, aSurf);
	if (!ok)
		{
		// The reference EGL swaps only the CURRENT draw surface of the calling thread (riMiniEGL eglSwapBuffers: EGL_BAD_SURFACE otherwise). Hitchcock
		// switches the thread's VG context to offscreen pbuffers (canvas render buffers) and its own "current surface" bookkeeping does not always match:
		// make the window surface current again, then swap.
		static TUint nf;
		const EGLint err = eglGetError();
		if (++nf <= 5 || (nf & 255) == 0)
			RDebug::Printf("PH: alf: vgwin eglSwapBuffers failed (#%u, egl error %x): making the window surface current and retrying", nf, (TUint)err);
		if (eglMakeCurrent(aDpy, aSurf, aSurf, aCtx))
			ok = eglSwapBuffers(aDpy, aSurf);
		if (nf <= 5)
			RDebug::Printf("PH: alf: vgwin retry -> eglSwapBuffers %d", (int)ok);
		}
	const TUint tFull1 = User::FastCounter();
	TPhVgWin* w = PhVgFind(aSurf);
	{ static TUint n; if (++n <= 5 || (n & 255) == 0) RDebug::Printf("PH: alf: vgwin SwapBuffers #%u surface %x -> eglSwapBuffers %d, our window %s", n, (TUint)aSurf, (int)ok, w ? "found" : "NOT FOUND"); }
	if (!w || !ok)
		return ok;
	if (!w->iTried)
		{
		w->iTried = ETrue;
		const TInt sr = PhVgOpenSurface(*w);
		RDebug::Printf("PH: alf: vgwin background surface -> %d", sr);
		w->iOk = (sr == KErrNone);
		}
	if (!w->iOk)
		return ok;
	CFbsBitmap& bmp = *w->iRi.iBitmap;
	const TInt wd = w->iRi.iSize.iWidth, ht = w->iRi.iSize.iHeight;
	bmp.BeginDataAccess();
	const TUint8* src = reinterpret_cast<const TUint8*>(bmp.DataAddress());
	const TInt sstride = bmp.DataStride();
	TUint8* dst = w->iChunk.Base();
	for (TInt y = 0; y < ht; y++)
		{
		const TUint32* s = reinterpret_cast<const TUint32*>(src + y * sstride);
		TUint32* d = reinterpret_cast<TUint32*>(dst + y * w->iStride);
		for (TInt x = 0; x < wd; x++)
			{
			const TUint32 p = s[x];
			const TUint32 al = p >> 24;
			if (al == 255)
				d[x] = p;
			else
				d[x] = (al << 24) | ((((p >> 16) & 255) * al / 255) << 16) | ((((p >> 8) & 255) * al / 255) << 8) | ((p & 255) * al / 255);
			}
		}
	bmp.EndDataAccess(ETrue);
	const TUint tFull2 = User::FastCounter();
	{	// one-shot pixel statistics: the RI bitmap (source) and the mapped surface (destination) -- black screen: nothing drawn, or alpha 0?
	const TInt sn = w->iSwaps + 1;
	if (sn == 1 || sn == 5 || sn == 20 || (sn & 127) == 0)
		{
		bmp.BeginDataAccess();
		const TUint8* sp = reinterpret_cast<const TUint8*>(bmp.DataAddress());
		TUint nzS = 0, nzA = 0, nzD = 0, nzDA = 0;
		for (TInt y = 0; y < ht; y++)
			{
			const TUint32* a2 = reinterpret_cast<const TUint32*>(sp + y * sstride);
			const TUint32* b2 = reinterpret_cast<const TUint32*>(dst + y * w->iStride);
			for (TInt x = 0; x < wd; x++)
				{
				if (a2[x] & 0x00ffffff) nzS++;
				if (a2[x] >> 24) nzA++;
				if (b2[x] & 0x00ffffff) nzD++;
				if (b2[x] >> 24) nzDA++;
				}
			}
		{	// where does the picture differ from its corner colour? (a bounding box + a coarse 6x8 map of the number of differing pixels per cell)
		const TUint32 ref = *reinterpret_cast<const TUint32*>(sp);
		TInt x0 = wd, y0 = ht, x1 = -1, y1 = -1;
		TUint diff = 0;
		TUint cell[8][6];
		Mem::FillZ(cell, sizeof(cell));
		for (TInt y = 0; y < ht; y++)
			{
			const TUint32* a2 = reinterpret_cast<const TUint32*>(sp + y * sstride);
			for (TInt x = 0; x < wd; x++)
				if (a2[x] != ref)
					{
					diff++;
					if (x < x0) x0 = x;
					if (x > x1) x1 = x;
					if (y < y0) y0 = y;
					if (y > y1) y1 = y;
					cell[y * 8 / ht][x * 6 / wd]++;
					}
			}
		{	// the most frequent colours (a 64-slot table keyed by a hash; collisions are dropped): is the content just dark / one flat colour?
		TUint32 hc[64]; TUint hn[64];
		Mem::FillZ(hc, sizeof(hc)); Mem::FillZ(hn, sizeof(hn));
		for (TInt y = 0; y < ht; y += 2)
			{
			const TUint32* a2 = reinterpret_cast<const TUint32*>(sp + y * sstride);
			for (TInt x = 0; x < wd; x += 2)
				{
				const TUint32 v = a2[x];
				const TUint h = ((v * 2654435761u) >> 26) & 63;
				if (hn[h] == 0) { hc[h] = v; hn[h] = 1; }
				else if (hc[h] == v) hn[h]++;
				}
			}
		for (TInt n = 0; n < 5; n++)
			{
			TInt best = -1;
			for (TInt i = 0; i < 64; i++)
				if (hn[i] && (best < 0 || hn[i] > hn[best])) best = i;
			if (best < 0) break;
			RDebug::Printf("PH: alf:   top colour %d: %08x x%u (of %u sampled)", n + 1, hc[best], hn[best], (TUint)((wd / 2) * (ht / 2)));
			hn[best] = 0;
			}
		}
		RDebug::Printf("PH: alf: vgwin picture: %u pixels differ from the corner colour %08x, bounding box %d,%d-%d,%d", diff, ref, x0, y0, x1, y1);
		for (TInt cy = 0; cy < 8; cy++)
			RDebug::Printf("PH: alf:   map row %d: %u %u %u %u %u %u", cy, cell[cy][0], cell[cy][1], cell[cy][2], cell[cy][3], cell[cy][4], cell[cy][5]);
		}
		const TInt px[4][2] = { { 0, 0 }, { wd / 2, ht / 2 }, { wd - 1, ht - 1 }, { 10, 10 } };
		{	// is the display actually on while we measure? (HAL display state: 1 on / 0 off; brightness)
		TInt dstate = -1, dbright = -1;
		HAL::Get(HALData::EDisplayState, dstate);
		HAL::Get(HALData::EDisplayBrightness, dbright);
		RDebug::Printf("PH: alf: vgwin display state at swap #%d: HAL EDisplayState %d (1 = on), brightness %d", sn, dstate, dbright);
		}
		RDebug::Printf("PH: alf: vgwin pixels at swap #%d: RI bitmap rgb!=0 %u alpha!=0 %u / surface rgb!=0 %u alpha!=0 %u of %u", sn, nzS, nzA, nzD, nzDA, (TUint)(wd * ht));
		for (TInt i = 0; i < 4; i++)
			RDebug::Printf("PH: alf:   (%d,%d) bitmap %08x surface %08x", px[i][0], px[i][1],
				*(reinterpret_cast<const TUint32*>(sp + px[i][1] * sstride) + px[i][0]),
				*(reinterpret_cast<const TUint32*>(dst + px[i][1] * w->iStride) + px[i][0]));
		bmp.EndDataAccess(ETrue);
		}
	}
	const TUint tFull3 = User::FastCounter();
	const TInt r = w->iSus.SubmitUpdate(w->iScreen, w->iSid, 0);
	{
	static TUint nfull;
	if (++nfull <= 6 || (nfull & 15) == 0)
		RDebug::Printf("PH: alf: vgwin FULL swap #%u: eglSwapBuffers (full read into the bitmap) %u us, premultiply copy %u us, SubmitUpdate %u us (statistics block excluded)",
			nfull, PhVgUs(tFull1 - tFull0), PhVgUs(tFull2 - tFull1), PhVgUs(User::FastCounter() - tFull3));
	}
	if (++w->iSwaps <= 3 || w->iSwaps == 5 || w->iSwaps == 20 || (w->iSwaps & 63) == 0)
		RDebug::Printf("PH: alf: vgwin swap #%d -> SubmitUpdate %d", w->iSwaps, r);
	return ok;
	}

static inline EGLBoolean PhVgDestroySurface(EGLDisplay aDpy, EGLSurface aSurf)
	{
	TPhVgWin* w = PhVgFind(aSurf);
	const EGLBoolean ok = eglDestroySurface(aDpy, aSurf);
	if (w)
		PhVgFree(w);
	return ok;
	}

#endif
