// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phtrace_wfc.cpp -- OURS: exported wfc calls as traced pass-throughs (orientation work, 2026-10-12).
// libwfc's own wfcapi.c is compiled with -D<name>=ph_real_<name> (emitter/components.py); this file defines the
// exported names. A WFC_CONTEXT_ROTATION change -- the window server's display render stage rotating the screen for a
// landscape mode -- is logged with the "PH:" prefix, which the kernel seam forwards to PhoneLab. After a change to a
// non-zero rotation the next element rectangles are logged too (the rotated frame is black: where do the elements go?).
#include <e32debug.h>
#undef wfcSetContextAttribi
#undef wfcSetElementAttribiv
#undef wfcSetElementAttribfv
#undef wfcSetElementAttribi
#undef wfcSetElementAttribf
#include <WF/wfc.h>

extern "C" void ph_real_wfcSetContextAttribi(WFCDevice dev, WFCContext ctx, WFCContextAttrib attrib, WFCint value);
extern "C" void ph_real_wfcSetElementAttribiv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCint* values);
extern "C" void ph_real_wfcSetElementAttribfv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCfloat* values);

static TInt PhRectBudget = 0;		// element rectangles still to log (armed by a rotation change)

extern "C" EXPORT_C void wfcSetContextAttribi(WFCDevice dev, WFCContext ctx, WFCContextAttrib attrib, WFCint value)
	{
	if (attrib == WFC_CONTEXT_ROTATION)
		{
		RDebug::Printf("PH: wfc context %d rotation -> %d", (TInt)ctx, (TInt)value);
		PhRectBudget = 40;
		}
	ph_real_wfcSetContextAttribi(dev, ctx, attrib, value);
	}

static const char* PhRectName(WFCElementAttrib aAttrib)
	{
	return aAttrib == WFC_ELEMENT_DESTINATION_RECTANGLE ? "dst" : aAttrib == WFC_ELEMENT_SOURCE_RECTANGLE ? "src" : NULL;
	}

extern "C" EXPORT_C void wfcSetElementAttribiv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCint* values)
	{
	const char* n = PhRectName(attrib);
	if (n && PhRectBudget > 0 && values && count == 4)
		{
		--PhRectBudget;
		RDebug::Printf("PH: wfc element %d %s %d,%d %dx%d", (TInt)element, n, values[0], values[1], values[2], values[3]);
		}
	ph_real_wfcSetElementAttribiv(dev, element, attrib, count, values);
	}

extern "C" EXPORT_C void wfcSetElementAttribfv(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint count,
	const WFCfloat* values)
	{
	const char* n = PhRectName(attrib);
	if (n && PhRectBudget > 0 && values && count == 4)
		{
		--PhRectBudget;
		RDebug::Printf("PH: wfc element %d %s(f) %d,%d %dx%d", (TInt)element, n, (TInt)values[0], (TInt)values[1],
			(TInt)values[2], (TInt)values[3]);
		}
	ph_real_wfcSetElementAttribfv(dev, element, attrib, count, values);
	}

// ---- 2026-10-23: the DSA element investigation (SkyForce shows a white screen although the game's pixels are in the video RAM).
// Every element's transparency types / global alpha / source stream / insert + remove order is logged, capped so a long run does
// not flood the log. Read: does element 3001 (the DSA surface) get TRANSPARENCY_SOURCE, which element sits above it, and is
// a UI element inserted over it after the game's loading screen?
// (diag off) #define PH_DSA_DIAG_UI_HIDE 1
// (diag, proved the UI layer covers the DSA element: GAME PICTURE SHOWS) #define PH_DSA_DIAG_TOP 1
#undef wfcInsertElement
#undef wfcRemoveElement
extern "C" void ph_real_wfcInsertElement(WFCDevice dev, WFCElement element, WFCElement subordinate);
extern "C" void ph_real_wfcRemoveElement(WFCDevice dev, WFCElement element);
extern "C" void ph_real_wfcSetElementAttribi(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint value);
extern "C" void ph_real_wfcSetElementAttribf(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCfloat value);
static TInt PhAttrBudget = 600;

extern "C" EXPORT_C void wfcInsertElement(WFCDevice dev, WFCElement element, WFCElement subordinate)
	{
	if (PhAttrBudget > 0)
		{
		--PhAttrBudget;
		RDebug::Printf("PH: wfc insert element %d above %d", (TInt)element, (TInt)subordinate);
		}
#ifdef PH_DSA_DIAG_TOP
	if ((TInt)element == 3001 && !subordinate)
		{
		subordinate = (WFCElement)3000;		// DIAG: the DSA element ABOVE the UI element (EKA2L1 draws it on top)
		RDebug::Printf("PH: wfc DIAG: DSA element 3001 inserted above UI element 3000");
		}
#endif
	ph_real_wfcInsertElement(dev, element, subordinate);
#ifdef PH_DSA_DIAG_UI_HIDE
	if ((TInt)element == 3001)
		{
		ph_real_wfcSetElementAttribi(dev, (WFCElement)3000, WFC_ELEMENT_TRANSPARENCY_TYPES, WFC_TRANSPARENCY_SOURCE | WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA);
		ph_real_wfcSetElementAttribf(dev, (WFCElement)3000, WFC_ELEMENT_GLOBAL_ALPHA, 0.5f);
		RDebug::Printf("PH: wfc DIAG: UI element 3000 at 50% alpha");
		}
#endif
	}

extern "C" EXPORT_C void wfcRemoveElement(WFCDevice dev, WFCElement element)
	{
	if (PhAttrBudget > 0)
		{
		--PhAttrBudget;
		RDebug::Printf("PH: wfc remove element %d", (TInt)element);
		}
	ph_real_wfcRemoveElement(dev, element);
	}

extern "C" EXPORT_C void wfcSetElementAttribi(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCint value)
	{
	if (PhAttrBudget > 0 && (attrib == WFC_ELEMENT_TRANSPARENCY_TYPES || attrib == WFC_ELEMENT_SOURCE))
		{
		--PhAttrBudget;
		RDebug::Printf("PH: wfc element %d %s = 0x%x", (TInt)element,
			attrib == WFC_ELEMENT_SOURCE ? "source stream" : "transparency types", (TInt)value);
		}
	ph_real_wfcSetElementAttribi(dev, element, attrib, value);
	}

extern "C" EXPORT_C void wfcSetElementAttribf(WFCDevice dev, WFCElement element, WFCElementAttrib attrib, WFCfloat value)
	{
	if (PhAttrBudget > 0 && attrib == WFC_ELEMENT_GLOBAL_ALPHA)
		{
		--PhAttrBudget;
		RDebug::Printf("PH: wfc element %d global alpha x1000 = %d", (TInt)element, (TInt)(value * 1000));
		}
	ph_real_wfcSetElementAttribf(dev, element, attrib, value);
	}

// DIAGNOSTIC (2026-10-23, remove after the SkyForce white-screen finding): when the DSA element (the one inserted right after the
// UI element 3000) appears, make the UI element fully transparent. If the game's picture then shows, the UI layer holds the opaque white.

// per compose pass: every element (handle >= 3000 = WSERV's) with its skipCompose verdict (the source lock gave no data = skipped), capped.
extern "C" void ph_wfc_scene_trace(int aHandle, int aSkip, int aAffects, int aHasData, int aTransp)
	{
	static TInt n = 0;
	static TInt last[16];	// per element (handle 3000..3015), 2000 lines at most
	TInt key = aHandle * 16 + aSkip * 8 + aAffects * 4 + (aHasData + 1) + 1;
	TInt& l = last[(aHandle - 3000) & 15];
	if (aHandle >= 3000 && n < 2000 && key != l)
		{
		++n; l = key;
		RDebug::Printf("PH: wfc scene element %d skipCompose=%d affects=%d srcHasData=%d transp=0x%x", aHandle, aSkip, aAffects, aHasData, aTransp);
		}
	}

extern "C" void ph_wfc_img_trace(int aHandle, int aW, int aH, int aStride, int aFmt, int aPre, int aLin, unsigned aP0, unsigned aP1, unsigned aP2)
	{
	static TInt n = 0;
	static unsigned last[16];	// per element (handle 3000..3015): print when its pixels change, 2000 lines at most
	unsigned key = aP0 ^ (aP1 * 3) ^ (aP2 * 5) ^ (unsigned)aHandle ^ (unsigned)aStride;
	unsigned& l = last[(aHandle - 3000) & 15];
	if (aHandle >= 3000 && n < 2000 && key != l)
		{
		++n; l = key;
		RDebug::Printf("PH: wfc img element %d %dx%d stride=%d pixfmt=%d premult=%d linear=%d px(0,0)=%08x px(mid)=%08x px(0,last)=%08x", aHandle, aW, aH, aStride, aFmt, aPre, aLin, aP0, aP1, aP2);
		}
	}

// 2026-10-27: the scene's element order (bottom first) with destination / source rotation / global alpha / source size,
// printed whenever a compose pass's list differs from the previous one (capped)
extern "C" void ph_wfc_order_trace(int aIdx, int aHandle, int aX, int aY, int aW, int aH, int aRot, int aAlpha, int aSrc)
	{
	static TInt n = 0;
	static TInt cur[8][8], prev[8][8], prevCount = -1;
	if (aIdx >= 0)
		{
		if (aIdx < 8)
			{
			TInt* c = cur[aIdx];
			c[0] = aHandle; c[1] = aX; c[2] = aY; c[3] = aW; c[4] = aH; c[5] = aRot; c[6] = aAlpha; c[7] = aSrc;
			}
		return;
		}
	const TInt count = aHandle < 8 ? aHandle : 8;
	TBool same = count == prevCount;
	for (TInt i = 0; same && i < count; ++i)
		for (TInt j = 0; j < 8; ++j)
			if (cur[i][j] != prev[i][j]) same = EFalse;
	if (same || n >= 300)
		return;
	++n; prevCount = count;
	for (TInt i = 0; i < count; ++i)
		{
		for (TInt j = 0; j < 8; ++j) prev[i][j] = cur[i][j];
		RDebug::Printf("PH: wfc order %d/%d: element %d dst %d,%d %dx%d rot %d alpha %d src %dx%d", i, count, cur[i][0], cur[i][1],
			cur[i][2], cur[i][3], cur[i][4], cur[i][5], cur[i][6], cur[i][7] / 10000, cur[i][7] % 10000);
		}
	}
