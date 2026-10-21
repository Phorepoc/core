/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phvg.h -- OURS: the interface between the OpenVG reference implementation (sflibopenvgrefimpl, riPhvg.cpp) and the
// GPU engine DLL phvg.dll (O2). The engine is surface-agnostic: it never sees an RI
// object, only an opaque cookie (the RI Surface*) and plain pixel buffers (RGBA8, PREMULTIPLIED, sRGB-encoded values,
// row 0 = surface y 0, exactly the RI's own row order).
#ifndef PHVG_H
#define PHVG_H

#include <e32def.h>

struct TPhvgFill
	{
	TAny*		iCookie;		// the RI Surface*
	TInt		iW, iH;			// surface size in pixels
	const TReal32* iEdges;		// iCount directed edges x0,y0,x1,y1 in surface pixels (y up, as the RI)
	TInt		iCount;
	TInt		iEvenOdd;		// 1 = even-odd, 0 = non-zero
	TInt		iBlendSrc;		// 1 = VG_BLEND_SRC (replace), 0 = VG_BLEND_SRC_OVER
	TReal32		iColor[4];		// premultiplied r,g,b,a (iPaintType 0)
	// O4: gradient paints and strokes
	TInt		iPaintType;		// 0 solid colour, 1 linear gradient, 2 radial gradient
	TInt		iSpread;		// colour ramp spread: 0 pad, 1 repeat, 2 reflect
	// linear: g = iGrad[0]*x + iGrad[1]*y + iGrad[2] (surface pixels, y up, the RI's own coordinates)
	// radial: paint-space point relative to the centre in units of the radius,
	//         px = iGrad[0]*x + iGrad[1]*y + iGrad[2], py = iGrad[3]*x + iGrad[4]*y + iGrad[5]
	TReal32		iGrad[6];
	TReal32		iFocal[3];		// radial: focal point fx, fy (units of the radius, clamped inside), D = -1/(fx^2+fy^2-1)
	const TUint8* iRamp;		// KPhvgRamp RGBA8 PREMULTIPLIED sRGB texels: the colour ramp sampled at (i+0.5)/KPhvgRamp
	const TReal32* iLeft;		// optional, one per edge: the x its trapezoid starts from (default: the surface's left edge -1)
	TInt		iStroke;		// 1 = the edges are a stroke (union of pieces; statistics only)
	};

// O4b: one vgDrawImage on the GPU (pixel-aligned flip/translate of an RGBA8 premultiplied sRGB source image, NORMAL or MULTIPLY mode)
struct TPhvgImage
	{
	TAny*		iCookie;		// destination surface (the RI Surface*)
	TInt		iW, iH;			// destination size
	TAny*		iKey;			// the source Image* (texture cache key; the cache entry is valid for iKey + iIw + iIh + iHash)
	TInt		iIw, iIh;		// source size
	TUint32		iHash;			// hash of the source pixels (changes whenever the image content does)
	TReal32		iM[6];			// image -> surface, y up: x' = iM[0] x + iM[1] y + iM[2]; y' = iM[3] x + iM[4] y + iM[5]
	TInt		iBlendSrc;		// 1 = VG_BLEND_SRC, 0 = VG_BLEND_SRC_OVER
	TReal32		iMul[4];		// premultiplied colour the texels are multiplied by (1,1,1,1 = VG_DRAW_IMAGE_NORMAL)
	const TInt*	iRects;			// iNRects disjoint rectangles x,y,w,h (surface pixels) the draw is clipped to
	TInt		iNRects;
	const TUint8* iPixels;		// iIw*iIh RGBA8 premultiplied sRGB, row 0 = image y 0; NULL when the cache is expected to hold the texture
	};

const TInt KPhvgRamp = 512;

// ordinals of phvg.def (looked up by name-less ordinal with RLibrary::Lookup)
enum TPhvgOrdinal
	{
	EPhvgInit = 1,			// TInt (): 1 = GL worker up
	EPhvgLivePtr,			// volatile TInt* (): number of surfaces whose GPU shadow is valid (RI fast path)
	EPhvgState,			// TInt (cookie, w, h): 0 stale, 1 valid and equal to the CPU, 2 GPU ahead, -1 declined
	EPhvgUpload,			// TInt (cookie, w, h, const TUint8* rgba): shadow := pixels -> state 1
	EPhvgFill,			// TInt (const TPhvgFill*): 1 done, 0 declined
	EPhvgClear,			// TInt (cookie, w, h, x, y, cw, ch, const TReal32* rgba): 1 done, -1 needs Upload first, 0 declined
	EPhvgDirty,			// TInt (cookie, TInt* rect x0,y0,x1,y1): 1 if GPU ahead (rect = dirty pixels), else 0
	EPhvgReadback,			// TInt (cookie, const TInt* rect, TUint8* out): shadow -> out, state 2 -> 1
	EPhvgInvalidate,		// void (cookie): the CPU wrote the surface, the shadow is stale
	EPhvgForget,			// void (cookie): the surface is gone
	EPhvgStats,			// void (TInt out[12]): fills, clears, uploads, readbacks, declines, edges, glErrors, shadows, directReads, gradientFills, strokeFills, 0
	EPhvgReadDirect,		// TInt (cookie, const TInt* rect, TUint8* out): shadow -> out, the shadow STAYS ahead (O3 residency)
	EPhvgDrawImage			// TInt (const TPhvgImage*): 1 done, 0 declined, -2 texture not cached and no pixels given (O4b)
	};

// the process switch (RProperty, TInt): 1 = GPU engine, 0 / undefined = the reference implementation. Read in the
// process's own category (its SID), else in the system category (set by a process with WriteDeviceData).
const TUint KPhvgModeKey = 0x50564731;			// 'PVG1'
const TUint KPhvgSystemCategory = 0x101f75b6;	// KUidSystemCategoryValue

#endif
