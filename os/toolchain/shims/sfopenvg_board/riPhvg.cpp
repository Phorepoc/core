// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

/*------------------------------------------------------------------------
 * riPhvg.cpp -- OURS (OpenVG O2): the glue between the Khronos reference implementation
 * and the GPU engine phvg.dll (src/phvg). Part of sflibopenvgrefimpl; the engine DLL is loaded with RLibrary only when
 * the GPU mode is switched on, so nothing here costs anything while it is off.
 *
 * HOW THE TWO ENGINES COMPOSE (the contract; every hook below follows it)
 *   A VG surface (the RI "Surface", the colour buffer of an EGL surface) has ONE owner at a time:
 *     CPU   the RI's own pixels are the truth; the GPU has no shadow, or a stale one          (phvg state 0)
 *     SYNC  the GPU shadow equals the RI pixels                                              (phvg state 1)
 *     GPU   the GPU drew since the last sync: the shadow is newer, the RI pixels are stale   (phvg state 2)
 *   GPU operations (a full or partial clear, a solid-colour fill) first make the shadow current (an upload of the
 *   RI pixels when the shadow is stale, the only CPU->GPU transfer) and leave the surface in state GPU, remembering the
 *   dirty rectangle. Every RI code path that touches the colour buffer on the CPU calls PhvgCpuAccess first:
 *     - PixelPipe::pixelPipe (all CPU painting: fills with other paints, strokes, images, glyphs)   write
 *     - Surface::clear (scissored), Surface::blit (vgSetPixels/vgWritePixels/vgCopyPixels)         write
 *     - Image::blit(const Surface*) (vgGetPixels/vgReadPixels/eglCopyBuffers/OSBlitToWindow)       read
 *     - Surface::mask(const Surface*) (vgRenderToMask reads a temporary surface, no-op in practice) read
 *   PhvgCpuAccess in state GPU reads the dirty rectangle back (GPU resolve + glReadPixels) and writes it into every
 *   sample of the RI surface (state SYNC); for a write access it then drops the shadow (state CPU). So a surface drawn
 *   by two engines is always correct; it is slow only where the engines alternate (one readback and, after a CPU write,
 *   one upload per alternation). Anything not implemented just takes the normal RI path.
 *   A surface is GPU-eligible only if it owns its pixels (not a VGImage-backed pbuffer, not a wrapped client bitmap) and
 *   is sRGB, non-luminance (the stored values are then exactly the GPU's premultiplied sRGB arithmetic).
 *
 * KNOWN LIMITS (documented risks): the GPU antialiases by S x S supersampling, the RI by its sample pattern, so edge
 * pixels differ by a few LSB..tens of LSB; a readback quantises premultiplied 8-bit values, so a translucent pixel inside
 * a dirty rectangle that the GPU did not touch can move by 1-2 LSB; one VG rendering thread per surface is assumed.
 *
 * THE SWITCH: an int property, key KPhvgModeKey, in the process's own category (its SID) or in the system category;
 * 1 = GPU engine, else the reference implementation. Re-read at every vgClear and every 500 ms otherwise.
 *------------------------------------------------------------------------*/
#include "riDefs.h"
#include <e32property.h>
#include <e32debug.h>
// the glue needs the privates of Rasterizer / PixelPipe / Image (they have no accessors); this file is OURS
#define private public
#define protected public
#include "riRasterizer.h"
#include "riImage.h"
#include "riPixelPipe.h"
#undef private
#undef protected
#include "phvg.h"

#ifndef PHVG_DEFAULT_MODE
#define PHVG_DEFAULT_MODE 0
#endif

namespace OpenVGRI
{

volatile unsigned gPhvgImgEpoch = 0;	// see riImage.cpp
volatile TInt* gPhvgLive = NULL;		// the engine's count of valid shadows; NULL until the engine is loaded

namespace
	{
	struct TTable
		{
		TInt (*Init)();
		volatile TInt* (*LivePtr)();
		TInt (*State)(TAny*, TInt, TInt);
		TInt (*Upload)(TAny*, TInt, TInt, const TUint8*);
		TInt (*Fill)(const TPhvgFill*);
		TInt (*Clear)(TAny*, TInt, TInt, TInt, TInt, TInt, TInt, const TReal32*);
		TInt (*Dirty)(TAny*, TInt*);
		TInt (*Readback)(TAny*, const TInt*, TUint8*);
		void (*Invalidate)(TAny*);
		void (*Forget)(TAny*);
		void (*Stats)(TInt*);
		TInt (*ReadDirect)(TAny*, const TInt*, TUint8*);
		TInt (*DrawImage)(const TPhvgImage*);
		};
	TTable	gT;
	TInt	gLoad = 0;					// 0 not tried, 1 engine up, -1 unavailable
	TInt	gMode = 0;
	TUint	gModeTick = 0;
	TInt	gModeInit = 0;

	void Load()
		{
		gLoad = -1;
		RLibrary lib;
		_LIT(KLib, "phvg.dll");
		const TInt r = lib.Load(KLib);
		if (r != KErrNone)
			{
			RDebug::Printf("PH: vg2 phvg: phvg.dll not loaded (%d), staying on the reference implementation", r);
			return;
			}
		gT.Init = (TInt (*)())lib.Lookup(EPhvgInit);
		gT.LivePtr = (volatile TInt* (*)())lib.Lookup(EPhvgLivePtr);
		gT.State = (TInt (*)(TAny*, TInt, TInt))lib.Lookup(EPhvgState);
		gT.Upload = (TInt (*)(TAny*, TInt, TInt, const TUint8*))lib.Lookup(EPhvgUpload);
		gT.Fill = (TInt (*)(const TPhvgFill*))lib.Lookup(EPhvgFill);
		gT.Clear = (TInt (*)(TAny*, TInt, TInt, TInt, TInt, TInt, TInt, const TReal32*))lib.Lookup(EPhvgClear);
		gT.Dirty = (TInt (*)(TAny*, TInt*))lib.Lookup(EPhvgDirty);
		gT.Readback = (TInt (*)(TAny*, const TInt*, TUint8*))lib.Lookup(EPhvgReadback);
		gT.Invalidate = (void (*)(TAny*))lib.Lookup(EPhvgInvalidate);
		gT.Forget = (void (*)(TAny*))lib.Lookup(EPhvgForget);
		gT.Stats = (void (*)(TInt*))lib.Lookup(EPhvgStats);
		gT.ReadDirect = (TInt (*)(TAny*, const TInt*, TUint8*))lib.Lookup(EPhvgReadDirect);
		gT.DrawImage = (TInt (*)(const TPhvgImage*))lib.Lookup(EPhvgDrawImage);
		if (!gT.Init || !gT.LivePtr || !gT.State || !gT.Upload || !gT.Fill || !gT.Clear || !gT.Dirty || !gT.Readback ||
		    !gT.Invalidate || !gT.Forget)
			{
			RDebug::Printf("PH: vg2 phvg: phvg.dll has the wrong exports");
			return;
			}
		if (!gT.Init())
			{
			RDebug::Printf("PH: vg2 phvg: the GL worker did not come up, staying on the reference implementation");
			return;
			}
		gPhvgLive = gT.LivePtr();
		gLoad = 1;
		RDebug::Printf("PH: vg2 phvg: engine up");
		}

	TInt ReadMode()
		{
		TInt v = 0;
		TUint32 sid = RProcess().SecureId();
		if (RProperty::Get(TUid::Uid((TInt)sid), KPhvgModeKey, v) == KErrNone)
			return v;
		if (RProperty::Get(TUid::Uid((TInt)KPhvgSystemCategory), KPhvgModeKey, v) == KErrNone)
			return v;
		return PHVG_DEFAULT_MODE;
		}

	// is the GPU engine switched on (and available)?
	TBool WantGpu(TBool aForce)
		{
		// (called for every CPU pixel access: the tick count, a kernel call, only every 64th time -- perf 2026-10-26)
		static TUint calls = 0;
		if (!aForce && gModeInit && (++calls & 63))
			return gMode && gLoad == 1;
		const TUint now = User::NTickCount();
		if (aForce || !gModeInit || now - gModeTick > 500)
			{
			gModeInit = 1;
			gModeTick = now;
			gMode = ReadMode();
			}
		if (!gMode)
			return EFalse;
		if (gLoad == 0)
			Load();
		return gLoad == 1;
		}

	TBool Eligible(const Surface* aS)
		{
		const Color::Descriptor& d = aS->getDescriptor();
		if (d.isLuminance() || !d.isNonlinear())
			return EFalse;
		const Image* im = aS->m_image;
		return im->m_ownsData && im->m_referenceCount == 1 && !im->m_parent;
		}

	TUint8 Pack(RIfloat aV)
		{
		if (!(aV > 0.0f))
			return 0;
		if (aV >= 1.0f)
			return 255;
		return (TUint8)(aV * 255.0f + 0.5f);
		}

	// O4b perf counters (ticks of User::FastCounter): 0 surface uploads, 1 their ticks, 2 readbacks into the RI, 3 their ticks, 4 DrawImage calls, 5 their round-trip ticks, 6 -2 returns
	TUint gPerf[8];

	// a single-sample 8:8:8:8 premultiplied sRGB surface: the stored bytes ARE the GPU's pixels, only the channel order differs
	TBool Plain8888(const Surface* aS)
		{
		const Color::Descriptor& d = aS->getDescriptor();
		const Image* im = aS->m_image;
		return aS->getNumSamples() == 1 && d.bitsPerPixel == 32 && d.redBits == 8 && d.greenBits == 8 && d.blueBits == 8 && d.alphaBits == 8 &&
			d.isPremultiplied() && d.isNonlinear() && !d.isLuminance() && im && im->m_data;
		}

	// the whole RI surface as RGBA8 premultiplied sRGB (samples averaged)
	void ReadSurface(const Surface* aS, TUint8* aBuf)
		{
		if (Plain8888(aS))
			{
			// integer fast path (the float loop below took ~170 ms for a 360x640 window)
			const Color::Descriptor& d = aS->getDescriptor();
			const Image* im = aS->m_image;
			const int w = aS->getWidth(), h = aS->getHeight();
			const TUint rs = d.redShift, gs = d.greenShift, bs = d.blueShift, as = d.alphaShift;
			for (int y = 0; y < h; y++)
				{
				const RIuint32* row = (const RIuint32*)(im->m_data + (y + im->m_storageOffsetY) * im->m_stride) + im->m_storageOffsetX;
				TUint8* o = aBuf + y * w * 4;
				for (int x = 0; x < w; x++, o += 4)
					{
					const RIuint32 p = row[x];
					o[0] = (TUint8)(p >> rs); o[1] = (TUint8)(p >> gs); o[2] = (TUint8)(p >> bs); o[3] = (TUint8)(p >> as);
					}
				}
			return;
			}
		const int w = aS->getWidth(), h = aS->getHeight(), n = aS->getNumSamples();
		const RIfloat inv = 1.0f / (RIfloat)n;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				{
				RIfloat r = 0, g = 0, b = 0, a = 0;
				for (int i = 0; i < n; i++)
					{
					Color c = aS->readSample(x, y, i);
					c.convert(Color::sRGBA_PRE);
					r += c.r; g += c.g; b += c.b; a += c.a;
					}
				TUint8* p = aBuf + (y * w + x) * 4;
				p[0] = Pack(r * inv); p[1] = Pack(g * inv); p[2] = Pack(b * inv); p[3] = Pack(a * inv);
				}
		}

	// rect (x0,y0,x1,y1) of RGBA8 premultiplied sRGB into every sample of the RI surface
	void WriteSurface(Surface* aS, const TInt* aRect, const TUint8* aBuf)
		{
		if (Plain8888(aS))
			{
			const Color::Descriptor& d = aS->getDescriptor();
			const Image* im = aS->m_image;
			const TUint rs = d.redShift, gs = d.greenShift, bs = d.blueShift, as = d.alphaShift;
			const int rw = aRect[2] - aRect[0];
			for (int y = aRect[1]; y < aRect[3]; y++)
				{
				RIuint32* row = (RIuint32*)(im->m_data + (y + im->m_storageOffsetY) * im->m_stride) + im->m_storageOffsetX;
				const TUint8* p = aBuf + (y - aRect[1]) * rw * 4;
				for (int x = aRect[0]; x < aRect[2]; x++, p += 4)
					row[x] = ((RIuint32)p[0] << rs) | ((RIuint32)p[1] << gs) | ((RIuint32)p[2] << bs) | ((RIuint32)p[3] << as);
				}
			return;
			}
		const int n = aS->getNumSamples();
		const int rw = aRect[2] - aRect[0];
		const Color::InternalFormat fmt = aS->getDescriptor().internalFormat;
		for (int y = aRect[1]; y < aRect[3]; y++)
			for (int x = aRect[0]; x < aRect[2]; x++)
				{
				const TUint8* p = aBuf + ((y - aRect[1]) * rw + (x - aRect[0])) * 4;
				Color c(p[0] / 255.0f, p[1] / 255.0f, p[2] / 255.0f, p[3] / 255.0f, Color::sRGBA_PRE);
				c.clamp();
				c.convert(fmt);
				for (int i = 0; i < n; i++)
					aS->writeSample(x, y, i, c);
				}
		}

	TBool UploadSurface(Surface* aS)
		{
		const int w = aS->getWidth(), h = aS->getHeight();
		TUint8* buf = (TUint8*)User::Alloc(w * h * 4);
		if (!buf)
			return EFalse;
		const TUint t0 = User::FastCounter();
		ReadSurface(aS, buf);
		const TInt r = gT.Upload(aS, w, h, buf);
		User::Free(buf);
		gPerf[0]++; gPerf[1] += User::FastCounter() - t0;
		return r == 1;
		}
	}

// ---- O4: paints (solid, linear and radial gradient) ----------------------------------------------------------------

namespace
	{
	// colour ramps are baked on the CPU through the RI's own PixelPipe::colorRamp (point evaluation, rho = 0), so stop
	// handling, premultiplication and the sRGB-space interpolation are exactly the RI's; a small cache by content hash
	struct TRamp
		{
		TUint32	iH1, iH2;
		TInt	iValid;
		TUint8	iData[KPhvgRamp * 4];
		};
	TRamp	gRamps[4];
	TInt	gRampNext = 0;

	TUint32 FBits(RIfloat aV)
		{
		union { RIfloat f; TUint32 u; } x;
		x.f = aV;
		return x.u;
		}

	void HashStops(const Paint* aP, TUint32& aH1, TUint32& aH2)
		{
		aH1 = 2166136261u;
		aH2 = 0x9E3779B9u;
		const TUint32 head[3] = { (TUint32)aP->m_colorRampStops.size(), (TUint32)aP->m_colorRampPremultiplied, 0x5eed };
		for (int k = 0; k < 3; k++)
			{
			aH1 = (aH1 ^ head[k]) * 16777619u;
			aH2 = (aH2 + head[k]) * 0x85EBCA6Bu + (aH2 >> 13);
			}
		for (int i = 0; i < aP->m_colorRampStops.size(); i++)
			{
			const Paint::GradientStop& s = aP->m_colorRampStops[i];
			const TUint32 v[5] = { FBits(s.offset), FBits(s.color.r), FBits(s.color.g), FBits(s.color.b), FBits(s.color.a) };
			for (int k = 0; k < 5; k++)
				{
				aH1 = (aH1 ^ v[k]) * 16777619u;
				aH2 = (aH2 + v[k]) * 0x85EBCA6Bu + (aH2 >> 13);
				}
			}
		}

	const TUint8* RampFor(const PixelPipe* aP)
		{
		const Paint* pt = aP->m_paint;
		if (pt->m_colorRampStops.size() < 2)
			return NULL;
		TUint32 h1, h2;
		HashStops(pt, h1, h2);
		for (TInt i = 0; i < 4; i++)
			if (gRamps[i].iValid && gRamps[i].iH1 == h1 && gRamps[i].iH2 == h2)
				return gRamps[i].iData;
		TRamp& r = gRamps[gRampNext];
		gRampNext = (gRampNext + 1) & 3;
		for (TInt i = 0; i < KPhvgRamp; i++)
			{
			Color c = aP->colorRamp(((RIfloat)i + 0.5f) / (RIfloat)KPhvgRamp, 0.0f);
			c.premultiply();					// as PixelPipe::pixelPipe does; sRGBA_PRE from here on
			TUint8* d = r.iData + i * 4;
			d[0] = Pack(c.r); d[1] = Pack(c.g); d[2] = Pack(c.b); d[3] = Pack(c.a);
			}
		r.iH1 = h1;
		r.iH2 = h2;
		r.iValid = 1;
		return r.iData;
		}

	TBool Sane(const TReal32* aV, TInt aN)
		{
		for (TInt i = 0; i < aN; i++)
			if (!(aV[i] > -1.0e9f && aV[i] < 1.0e9f))
				return EFalse;
		return ETrue;
		}

	// the paint part of a TPhvgFill from a configured PixelPipe (blend mode included); EFalse = the GPU cannot paint it
	TBool BuildPaint(const PixelPipe* p, TPhvgFill& f)
		{
		if (p->m_image || p->m_masking || p->m_colorTransform || !p->m_drawable || !p->m_paint)
			return EFalse;
		if (p->m_blendMode != VG_BLEND_SRC_OVER && p->m_blendMode != VG_BLEND_SRC)
			return EFalse;
		const Paint* pt = p->m_paint;
		f.iPaintType = 0;
		f.iSpread = 0;
		f.iRamp = NULL;
		f.iStroke = 0;
		f.iBlendSrc = p->m_blendMode == VG_BLEND_SRC;
		switch (pt->m_paintType)
			{
		case VG_PAINT_TYPE_COLOR:
			{
			if (pt->m_paintColor.getInternalFormat() != Color::sRGBA_PRE)
				return EFalse;
			const Color& c = pt->m_paintColor;
			f.iColor[0] = c.r; f.iColor[1] = c.g; f.iColor[2] = c.b; f.iColor[3] = c.a;
			return ETrue;
			}
		case VG_PAINT_TYPE_LINEAR_GRADIENT:
		case VG_PAINT_TYPE_RADIAL_GRADIENT:
			break;
		default:
			return EFalse;						// patterns stay in the RI
			}
		const Matrix3x3& M = p->m_surfaceToPaintMatrix;
		if (pt->m_paintType == VG_PAINT_TYPE_LINEAR_GRADIENT)
			{
			const RIfloat ux = pt->m_linearGradientPoint1.x - pt->m_linearGradientPoint0.x;
			const RIfloat uy = pt->m_linearGradientPoint1.y - pt->m_linearGradientPoint0.y;
			const RIfloat usq = ux * ux + uy * uy;
			if (!(usq > 0.0f))
				return EFalse;					// degenerate: the RI paints the last stop everywhere
			const RIfloat oou = 1.0f / usq;
			f.iPaintType = 1;
			f.iGrad[0] = (ux * M[0][0] + uy * M[1][0]) * oou;
			f.iGrad[1] = (ux * M[0][1] + uy * M[1][1]) * oou;
			f.iGrad[2] = ((M[0][2] - pt->m_linearGradientPoint0.x) * ux + (M[1][2] - pt->m_linearGradientPoint0.y) * uy) * oou;
			f.iGrad[3] = f.iGrad[4] = f.iGrad[5] = 0.0f;
			}
		else
			{
			const RIfloat r = pt->m_radialGradientRadius;
			if (!(r > 0.0f))
				return EFalse;
			RIfloat fx = pt->m_radialGradientFocalPoint.x - pt->m_radialGradientCenter.x;
			RIfloat fy = pt->m_radialGradientFocalPoint.y - pt->m_radialGradientCenter.y;
			const RIfloat fl = (RIfloat)sqrt(fx * fx + fy * fy);
			if (fl > 0.999f * r)				// the RI clamps the focal point inside the circle
				{
				fx *= 0.999f * r / fl;
				fy *= 0.999f * r / fl;
				}
			const RIfloat ir = 1.0f / r;
			f.iPaintType = 2;
			f.iFocal[0] = fx * ir;
			f.iFocal[1] = fy * ir;
			f.iFocal[2] = -1.0f / (f.iFocal[0] * f.iFocal[0] + f.iFocal[1] * f.iFocal[1] - 1.0f);
			f.iGrad[0] = M[0][0] * ir;
			f.iGrad[1] = M[0][1] * ir;
			f.iGrad[2] = (M[0][2] - pt->m_radialGradientCenter.x) * ir;
			f.iGrad[3] = M[1][0] * ir;
			f.iGrad[4] = M[1][1] * ir;
			f.iGrad[5] = (M[1][2] - pt->m_radialGradientCenter.y) * ir;
			}
		if (!Sane(f.iGrad, 6) || !Sane(f.iFocal, 3))
			return EFalse;
		switch (pt->m_colorRampSpreadMode)
			{
		case VG_COLOR_RAMP_SPREAD_PAD:		f.iSpread = 0; break;
		case VG_COLOR_RAMP_SPREAD_REPEAT:	f.iSpread = 1; break;
		default:							f.iSpread = 2; break;
			}
		f.iRamp = RampFor(p);
		return f.iRamp != NULL;
		}
	}

// ---- hooks ---------------------------------------------------------------------------------------------------------

// Before any CPU access to a colour buffer (see the contract above).
void PhvgCpuAccess(const Surface* aS, bool aWrite)
	{
	if (aWrite)
		++gPhvgImgEpoch;			// a CPU write into a surface may be a write into a VGImage (pbuffer from client buffer)
	if (!gPhvgLive || !*gPhvgLive)
		return;
	TInt rect[4];
	if (gT.Dirty((TAny*)aS, rect))
		{
		const int w = rect[2] - rect[0], h = rect[3] - rect[1];
		TUint8* buf = (TUint8*)User::Alloc(w * h * 4);
		if (buf)
			{
			const TUint t0 = User::FastCounter();
			if (gT.Readback((TAny*)aS, rect, buf) == 1)
				WriteSurface((Surface*)aS, rect, buf);
			User::Free(buf);
			gPerf[2]++; gPerf[3] += User::FastCounter() - t0;
			}
		}
	if (aWrite)
		gT.Invalidate((TAny*)aS);
	}

// O3 -- RESIDENT SURFACES. A CPU consumer that only READS a GPU-ahead surface into a plain 8-bit-per-channel sRGB
// destination (vgReadPixels / vgGetPixels / eglCopyBuffers / OSBlitToWindow -> VGISymbianCopyToBitmap all end in
// Image::blit(const Surface*)) is served straight from the GPU shadow: one resolve (cached while the surface does not
// change) + one glReadPixels of exactly the requested rectangle + an integer repack into the destination layout. The RI
// surface is NOT made current, so nothing is written back and the surface stays GPU-resident for the next frame (the
// dirty rectangle is kept, so any later RI access still flushes correctly). This removes, per frame, the float
// WriteSurface of the dirty rectangle into every sample and the float FSAAResolve/convert/pack of the blit.
// True = done. The region (sx,sy,dx,dy,w,h) has been clipped by the caller.
bool PhvgTryBlitOut(const Surface* aS, Image* aDst, int aSx, int aSy, int aDx, int aDy, int aW, int aH)
	{
	if (!gPhvgLive || !*gPhvgLive || !aDst->m_data || aW <= 0 || aH <= 0)
		return false;
	const Color::Descriptor& d = aDst->m_desc;
	if (d.isLuminance() || !d.isNonlinear() || d.luminanceBits || (d.bitsPerPixel != 32 && d.bitsPerPixel != 16))
		return false;
	if (d.redBits < 1 || d.redBits > 8 || d.greenBits < 1 || d.greenBits > 8 || d.blueBits < 1 || d.blueBits > 8 ||
	    d.alphaBits > 8)
		return false;
	TInt dirty[4];
	if (!gT.Dirty((TAny*)aS, dirty) || !gT.ReadDirect)			// only a GPU-ahead surface; else the RI is current
		return false;
	TUint8* buf = (TUint8*)User::Alloc(aW * aH * 4);
	if (!buf)
		return false;
	const TInt rect[4] = { aSx, aSy, aSx + aW, aSy + aH };
	if (gT.ReadDirect((TAny*)aS, rect, buf) != 1)
		{
		User::Free(buf);
		return false;
		}
	{	// PH (SkyForce white hunt): the present path -- a GPU-ahead surface is blitted out into the destination (the ALF window bitmap = the UI image)
		static TInt phn = 0;
		if (phn < 300 && aW * aH >= 100000)
			{
			++phn;
			const TInt px = 180 - aSx, py = 320 - aSy;
			TUint gv = 0xdeadbeef;
			if (px >= 0 && py >= 0 && px < aW && py < aH)
				{ const TUint8* q = buf + (py * aW + px) * 4; gv = q[0] | (q[1] << 8) | (q[2] << 16) | ((TUint)q[3] << 24); }
			RDebug::Printf("PH: phvg BLITOUT surf=%08x dst=%08x(stride %d) src %d,%d dst %d,%d %dx%d dirty=%d,%d-%d,%d gpuPx(180,320)=%08x (RGBA bytes)", (TUint)aS, (TUint)aDst->m_data, (int)aDst->m_stride, aSx, aSy, aDx, aDy, aW, aH, dirty[0], dirty[1], dirty[2], dirty[3], gv);
			}
	}
	const TBool pre = d.isPremultiplied();
	const TInt rb = d.redBits, gb = d.greenBits, bb = d.blueBits, ab = d.alphaBits;
	const TInt rm = (1 << rb) - 1, gm = (1 << gb) - 1, bm = (1 << bb) - 1, am = (1 << ab) - 1;
	for (TInt j = 0; j < aH; j++)
		{
		RIuint8* line = aDst->m_data + (aDy + j + aDst->m_storageOffsetY) * aDst->m_stride;
		const TUint8* p = buf + j * aW * 4;
		for (TInt i = 0; i < aW; i++, p += 4)
			{
			TUint r = p[0], g = p[1], b = p[2];
			const TUint a = p[3];
			if (!pre)
				{
				if (a == 0)
					r = g = b = 0;
				else if (a != 255)
					{
					r = (r * 255 + a / 2) / a; if (r > 255) r = 255;
					g = (g * 255 + a / 2) / a; if (g > 255) g = 255;
					b = (b * 255 + a / 2) / a; if (b > 255) b = 255;
					}
				}
			// round(c8 / 255 * max), the RI's colorToInt for an 8 bit source
			const TUint cr = rb == 8 ? r : (2 * r * rm + 255) / 510;
			const TUint cg = gb == 8 ? g : (2 * g * gm + 255) / 510;
			const TUint cb = bb == 8 ? b : (2 * b * bm + 255) / 510;
			const TUint ca = ab == 0 ? 0 : (ab == 8 ? a : (2 * a * am + 255) / 510);
			const TUint v = (cr << d.redShift) | (cg << d.greenShift) | (cb << d.blueShift) | (ca << d.alphaShift);
			if (d.bitsPerPixel == 32)
				((RIuint32*)line)[aDx + i + aDst->m_storageOffsetX] = (RIuint32)v;
			else
				((RIuint16*)line)[aDx + i + aDst->m_storageOffsetX] = (RIuint16)v;
			}
		}
	aDst->m_mipmapsValid = false;
	User::Free(buf);
	return true;
	}

// The surface is being destroyed.
void PhvgForget(const Surface* aS)
	{
	if (gLoad == 1)
		gT.Forget((TAny*)aS);
	}

// Surface::clear (no scissor). True = the GPU did it (or there was nothing to do).
bool PhvgTryClear(Surface* aS, const Color& aColor, int aX, int aY, int aW, int aH)
	{
	if (!WantGpu(ETrue) || !Eligible(aS))
		return false;
	const int sw = aS->getWidth(), sh = aS->getHeight();
	Rectangle r(0, 0, sw, sh);
	r.intersect(Rectangle(aX, aY, aW, aH));
	if (!r.width || !r.height)
		return true;
	Color c = aColor;
	c.clamp();
	c.convert(Color::sRGBA_PRE);
	const TReal32 rgba[4] = { c.r, c.g, c.b, c.a };
	if (gT.State(aS, sw, sh) < 0)
		return false;
	TInt res = gT.Clear(aS, sw, sh, r.x, r.y, r.width, r.height, rgba);
	if (res == -1)
		{
		if (!UploadSurface(aS))
			return false;
		res = gT.Clear(aS, sw, sh, r.x, r.y, r.width, r.height, rgba);
		}
	return res == 1;
	}

// ---- O4: strokes ----------------------------------------------------------------------------------------------------
// The RI draws a stroke as the UNION of many small pieces (a quad per segment, joins, caps; Path::stroke), each rasterised
// by Rasterizer::fill() into a per-sample coverage buffer, and only then paints the buffer pixel by pixel (renderStroke).
// phvg takes the pieces instead: setup() with a coverage buffer and no pixel pipe BEGINS a capture when the stroke is
// GPU-paintable (the PixelPipe configured for it is the last one whose surface-to-paint matrix was set, gPhvgLastPipe),
// every fill() of the stroke then appends its edges (each piece oriented to a positive signed area, so that the non-zero
// edge count of ALL pieces together is exactly "covered by at least one piece" = the union), and the rasterizer's
// destruction (or the next setup()) ENDS it: one stencil-then-cover fill with the stroke paint. The RI's coverage buffer
// stays empty, so renderStroke's pixel loop does nothing. The geometry is the RI's own (caps, joins, dashes, miter limit,
// the transform), no tessellator is duplicated. Limits: a stroke of more than KStrokeMaxEdges edges is drawn in batches (a
// translucent paint then double-covers where two batches overlap); the capture owner is one Rasterizer at a time.
const PixelPipe* gPhvgLastPipe = NULL;		// the PixelPipe most recently given a surface-to-paint matrix (riPixelPipe.cpp)

namespace
	{
	const TInt KStrokeMaxEdges = 262144;

	struct TStroke
		{
		const Rasterizer*	iOwner;		// NULL = no capture
		TPhvgFill			iFill;		// the paint + surface
		TUint8				iRamp[KPhvgRamp * 4];
		TReal32*			iEdges;
		TReal32*			iLeft;		// per edge: the x its trapezoid starts from (the piece's own left bound)
		TInt				iCount, iCap;
		};
	TStroke gStroke;

	TInt gStrokeDbg = 0;

	// diagnostics (first 10 strokes): the pieces together must be balanced on every scanline (sum of the signed edge
	// crossings = 0), and the bounding box says where the GPU will paint
	void StrokeDebug()
		{
		if (gStrokeDbg >= 10)
			return;
		gStrokeDbg++;
		const TInt h = gStroke.iFill.iH;
		TInt bad = 0, firstBad = -1, worst = 0;
		TReal32 x0 = 1.0e9f, x1 = -1.0e9f, y0 = 1.0e9f, y1 = -1.0e9f;
		for (TInt i = 0; i < gStroke.iCount; i++)
			{
			const TReal32* q = gStroke.iEdges + i * 4;
			if (q[0] < x0) x0 = q[0];
			if (q[2] < x0) x0 = q[2];
			if (q[0] > x1) x1 = q[0];
			if (q[2] > x1) x1 = q[2];
			if (q[1] < y0) y0 = q[1];
			if (q[3] < y0) y0 = q[3];
			if (q[1] > y1) y1 = q[1];
			if (q[3] > y1) y1 = q[3];
			}
		for (TInt row = 0; row < h; row++)
			{
			const TReal32 yc = (TReal32)row + 0.5f;
			TInt sum = 0;
			for (TInt i = 0; i < gStroke.iCount; i++)
				{
				const TReal32* q = gStroke.iEdges + i * 4;
				if (q[1] < q[3] ? (yc >= q[1] && yc < q[3]) : (yc >= q[3] && yc < q[1]))
					sum += q[3] > q[1] ? 1 : -1;
				}
			if (sum != 0)
				{
				bad++;
				if (firstBad < 0)
					firstBad = row;
				if (sum > worst || -sum > worst)
					worst = sum < 0 ? -sum : sum;
				}
			}
		RDebug::Printf("PH: vg2 stroke dbg #%d: %d edges, bbox x %d..%d y %d..%d (surface %dx%d), paint %d, rows with unbalanced crossings: %d (first %d, worst %d)",
		               gStrokeDbg, gStroke.iCount, (TInt)x0, (TInt)x1, (TInt)y0, (TInt)y1, gStroke.iFill.iW, h, gStroke.iFill.iPaintType,
		               bad, firstBad, worst);
		if (gStroke.iCount >= 2)
			RDebug::Printf("PH: vg2 stroke dbg edge0 %d,%d -> %d,%d  edge1 %d,%d -> %d,%d (x10)",
			               (TInt)(gStroke.iEdges[0] * 10), (TInt)(gStroke.iEdges[1] * 10), (TInt)(gStroke.iEdges[2] * 10),
			               (TInt)(gStroke.iEdges[3] * 10), (TInt)(gStroke.iEdges[4] * 10), (TInt)(gStroke.iEdges[5] * 10),
			               (TInt)(gStroke.iEdges[6] * 10), (TInt)(gStroke.iEdges[7] * 10));
		}

	void StrokeFlush()
		{
		if (gStroke.iCount > 0)
			{
			StrokeDebug();
			gStroke.iFill.iEdges = gStroke.iEdges;
			gStroke.iFill.iLeft = gStroke.iLeft;
			gStroke.iFill.iCount = gStroke.iCount;
			gStroke.iFill.iEvenOdd = 0;				// every piece has a positive winding: non-zero = union
			gStroke.iFill.iStroke = 1;
			gT.Fill(&gStroke.iFill);
			}
		gStroke.iCount = 0;
		}
	}

// Rasterizer::setup with a coverage buffer: True = captured (the pieces will go to the GPU).
bool PhvgStrokeBegin(const Rasterizer& aR)
	{
	const PixelPipe* p = gPhvgLastPipe;
	if (!p || aR.m_scissor || gStroke.iOwner || !p->m_drawable || !WantGpu(EFalse))
		return false;
	Surface* s = p->m_drawable->getColorBuffer();
	if (!s || !Eligible(s))
		return false;
	TPhvgFill f;
	Mem::FillZ(&f, sizeof(f));
	if (!BuildPaint(p, f))
		return false;
	const int w = s->getWidth(), h = s->getHeight();
	TInt st = gT.State(s, w, h);
	if (st < 0)
		return false;
	if (st == 0 && !UploadSurface(s))			// the CPU pixels (earlier fills of this very draw call) go to the GPU first
		return false;
	const TInt cap = 2048;
	TReal32* e = (TReal32*)User::Alloc(cap * 4 * sizeof(TReal32));
	TReal32* lf = (TReal32*)User::Alloc(cap * sizeof(TReal32));
	if (!e || !lf)
		{
		User::Free(e);
		User::Free(lf);
		return false;
		}
	gStroke.iFill = f;
	if (f.iRamp)
		{
		Mem::Copy(gStroke.iRamp, f.iRamp, KPhvgRamp * 4);
		gStroke.iFill.iRamp = gStroke.iRamp;
		}
	gStroke.iFill.iCookie = s;
	gStroke.iFill.iW = w;
	gStroke.iFill.iH = h;
	gStroke.iEdges = e;
	gStroke.iLeft = lf;
	gStroke.iCount = 0;
	gStroke.iCap = cap;
	gStroke.iOwner = &aR;
	return true;
	}

// Rasterizer::fill of one piece (the edges in aR). True = taken.
bool PhvgStrokePiece(const Rasterizer& aR)
	{
	if (gStroke.iOwner != &aR)
		return false;
	const int n = aR.m_edges.size();
	if (n == 0)
		return true;
	if (gStroke.iCount + n > gStroke.iCap)
		{
		if (gStroke.iCount + n > KStrokeMaxEdges)
			StrokeFlush();						// batch (see the limits above)
		if (gStroke.iCount + n > gStroke.iCap)
			{
			TInt cap = gStroke.iCap * 2;
			while (cap < gStroke.iCount + n)
				cap *= 2;
			TReal32* e = (TReal32*)User::ReAlloc(gStroke.iEdges, cap * 4 * sizeof(TReal32));
			if (e)
				gStroke.iEdges = e;
			TReal32* lf = e ? (TReal32*)User::ReAlloc(gStroke.iLeft, cap * sizeof(TReal32)) : NULL;
			if (lf)
				gStroke.iLeft = lf;
			if (!e || !lf)
				{
				StrokeFlush();					// out of memory: draw what there is, go on with an empty buffer
				if (n > gStroke.iCap)
					return true;				// a single piece larger than the buffer (cannot happen: RI_MAX_EDGES)
				}
			else
				gStroke.iCap = cap;
			}
		}
	TReal32* d = gStroke.iEdges + gStroke.iCount * 4;
	TReal32 area2 = 0.0f;						// twice the signed area, as the integral of x dy over the directed edges
	TBool sane = ETrue;
	for (int i = 0; i < n; i++)
		{
		const Rasterizer::Edge& e = aR.m_edges[i];
		TReal32* q = d + i * 4;
		if (e.direction > 0)
			{
			q[0] = e.v0.x; q[1] = e.v0.y; q[2] = e.v1.x; q[3] = e.v1.y;
			}
		else
			{
			q[0] = e.v1.x; q[1] = e.v1.y; q[2] = e.v0.x; q[3] = e.v0.y;
			}
		if (!Sane(q, 4))
			sane = EFalse;
		area2 += (q[0] + q[2]) * (q[3] - q[1]);
		}
	if (!sane || !(area2 > 1.0e-9f || area2 < -1.0e-9f))
		return true;							// NaN / huge, or no area: covers nothing
	if (area2 < 0.0f)							// clockwise: reverse every edge
		for (int i = 0; i < n; i++)
			{
			TReal32* q = d + i * 4;
			const TReal32 x = q[0], y = q[1];
			q[0] = q[2]; q[1] = q[3]; q[2] = x; q[3] = y;
			}
	// the piece's own left bound: its trapezoids start at its minimum x (minus one), not at the surface's left edge. A closed
	// piece is balanced for every sample left of it, so nothing is lost -- and the triangles stay inside the piece's bounding
	// box instead of each spanning the whole surface width (a 6000-edge stroke overflowed the Mali tile/PLBU heap)
	TReal32 minx = 1.0e9f;
	for (int i = 0; i < n; i++)
		{
		const TReal32* q = d + i * 4;
		if (q[0] < minx) minx = q[0];
		if (q[2] < minx) minx = q[2];
		}
	TReal32 left = (TReal32)floor(minx) - 1.0f;
	if (left < -1.0f)
		left = -1.0f;
	for (int i = 0; i < n; i++)
		gStroke.iLeft[gStroke.iCount + i] = left;
	gStroke.iCount += n;
	return true;
	}

// The rasterizer is being set up again or destroyed: paint a pending stroke.
void PhvgStrokeEnd(const Rasterizer* aR)
	{
	if (gStroke.iOwner != aR)
		return;
	StrokeFlush();
	User::Free(gStroke.iEdges);
	User::Free(gStroke.iLeft);
	gStroke.iEdges = NULL;
	gStroke.iLeft = NULL;
	gStroke.iOwner = NULL;
	}

// why path fills stay on the CPU (GPU mode only; ri_fastblit.cpp prints them with the "RI gpu" line):
// 0 done on the GPU, 1 coverage buffer / mask, 2 scissor, 3 surface not eligible (a pbuffer over a VGImage, a child image,
// luminance / linear format), 4 paint (pattern / image paint), 5 the surface or the fill failed on the GPU side
static TUint gFillStat[6];
const unsigned* PhvgFillStats() { return gFillStat; }

// the scissor rectangles of the last Rasterizer::setScissor (one rasterizer per draw call, set just before its fill)
static const void* gScOwner;
static TInt gScRect[KPhvgMaxScissor * 4];
static TInt gScN;			// -1 = too many to keep
void PhvgNoteScissor(const void* aOwner, const Array<Rectangle>& aRects)
	{
	gScOwner = aOwner;
	gScN = 0;
	for (int i = 0; i < aRects.size(); i++)
		{
		const Rectangle& r = aRects[i];
		if (r.width <= 0 || r.height <= 0)
			continue;
		if (gScN == KPhvgMaxScissor)
			{
			gScN = -1;
			return;
			}
		TInt* d = gScRect + gScN++ * 4;
		d[0] = r.x; d[1] = r.y; d[2] = r.width; d[3] = r.height;
		}
	}

// the noted rectangles clipped to the surface, into aOut; -1 = not usable (another rasterizer's, too many, or overlapping:
// the scissor is their union and the GPU would paint an overlap twice)
static TInt ScissorRects(const Rasterizer& aR, TInt aW, TInt aH, TInt* aOut)
	{
	if (gScOwner != &aR || gScN < 0)
		return -1;
	TInt n = 0;
	for (TInt i = 0; i < gScN; i++)
		{
		const TInt* r = gScRect + i * 4;
		TInt x0 = r[0] < 0 ? 0 : r[0], y0 = r[1] < 0 ? 0 : r[1];
		TInt x1 = r[0] + r[2] > aW ? aW : r[0] + r[2], y1 = r[1] + r[3] > aH ? aH : r[1] + r[3];
		if (x1 <= x0 || y1 <= y0)
			continue;
		for (TInt j = 0; j < n; j++)
			{
			const TInt* o = aOut + j * 4;
			if (x0 < o[0] + o[2] && o[0] < x1 && y0 < o[1] + o[3] && o[1] < y1)
				return -1;
			}
		TInt* d = aOut + n++ * 4;
		d[0] = x0; d[1] = y0; d[2] = x1 - x0; d[3] = y1 - y0;
		}
	return n;
	}

// Rasterizer::fill. True = the GPU painted the path (solid colour, linear or radial gradient).
bool PhvgTryFill(const Rasterizer& aR)
	{
	const PixelPipe* p = aR.m_pixelPipe;
	if (!p || !p->m_drawable || !WantGpu(EFalse))
		return false;
	if (aR.m_covBuffer)
		return gFillStat[1]++, false;
	Surface* s = p->m_drawable->getColorBuffer();
	if (!s || !Eligible(s))
		return gFillStat[3]++, false;
	TInt sc[KPhvgMaxScissor * 4];
	TInt nsc = 0;
	if (aR.m_scissor)
		{
		nsc = ScissorRects(aR, s->getWidth(), s->getHeight(), sc);
		if (nsc < 0)
			return gFillStat[2]++, false;
		if (nsc == 0)
			return gFillStat[0]++, true;		// scissored away: nothing to paint
		}
	TPhvgFill f;
	Mem::FillZ(&f, sizeof(f));
	if (!BuildPaint(p, f))
		return gFillStat[4]++, false;
	const int n = aR.m_edges.size();
	if (n == 0)
		return true;							// an empty path paints nothing
	const int w = s->getWidth(), h = s->getHeight();
	TReal32* edges = (TReal32*)User::Alloc(n * 4 * sizeof(TReal32));
	if (!edges)
		return false;
	TBool sane = ETrue;
	for (int i = 0; i < n; i++)
		{
		const Rasterizer::Edge& e = aR.m_edges[i];
		const TReal32 a[4] = { e.v0.x, e.v0.y, e.v1.x, e.v1.y };
		for (int k = 0; k < 4; k++)
			if (!(a[k] > -1.0e8f && a[k] < 1.0e8f))
				sane = EFalse;					// NaN / huge: let the RI deal with it
		TReal32* d = edges + i * 4;
		if (e.direction > 0)
			{
			d[0] = a[0]; d[1] = a[1]; d[2] = a[2]; d[3] = a[3];
			}
		else
			{
			d[0] = a[2]; d[1] = a[3]; d[2] = a[0]; d[3] = a[1];
			}
		}
	bool done = false;
	if (sane)
		{
		TInt st = gT.State(s, w, h);
		if (st == 0 && UploadSurface(s))
			st = 1;
		if (st > 0)
			{
			f.iCookie = s;
			f.iW = w;
			f.iH = h;
			f.iEdges = edges;
			f.iCount = n;
			f.iEvenOdd = aR.m_fillRule == VG_EVEN_ODD;
			f.iRects = nsc ? sc : NULL;
			f.iNRects = nsc;
			done = gT.Fill(&f) == 1;
			}
		}
	User::Free(edges);
	gFillStat[done ? 0 : 5]++;
	return done;
	}

// ---- O4b: images on the GPU ---------------------------------------------------------------------------------------
// True when drawing into aS goes to the GPU (engine on and up, surface eligible, image draws available).
bool PhvgSurfaceGpu(const Surface* aS)
	{
	return WantGpu(EFalse) && gT.DrawImage && Eligible(aS);
	}

// One pixel-aligned image draw. 1 done; 0 the GPU cannot (the caller draws on the CPU); -2 the engine has no texture
// for (aKey, size, hash) and aPixels was NULL: call again with the RGBA8 premultiplied sRGB pixels.
int PhvgDrawImage(Surface* aS, const void* aKey, int aIw, int aIh, unsigned aHash, const float* aM, int aBlendSrc,
                  const float* aMul, const int* aRects, int aN, const unsigned char* aPixels)
	{
	if (!PhvgSurfaceGpu(aS))
		return 0;
	const int w = aS->getWidth(), h = aS->getHeight();
	TInt st = gT.State(aS, w, h);
	if (st < 0)
		return 0;
	if (st == 0 && !UploadSurface(aS))
		return 0;
	TPhvgImage im;
	Mem::FillZ(&im, sizeof(im));
	im.iCookie = aS; im.iW = w; im.iH = h;
	im.iKey = (TAny*)aKey; im.iIw = aIw; im.iIh = aIh; im.iHash = aHash;
	for (int i = 0; i < 6; i++)
		im.iM[i] = aM[i];
	im.iBlendSrc = aBlendSrc;
	for (int i = 0; i < 4; i++)
		im.iMul[i] = aMul[i];
	im.iRects = aRects; im.iNRects = aN; im.iPixels = aPixels;
	const TUint t0 = User::FastCounter();
	const int res = gT.DrawImage(&im);
	gPerf[4]++; gPerf[5] += User::FastCounter() - t0;
	if (res == -2)
		gPerf[6]++;
	return res;
	}

const unsigned* PhvgPerf() { return gPerf; }
unsigned PhvgImgEpoch() { return gPhvgImgEpoch; }

// Scissored Surface::clear on the GPU: SRC "image" draws of one white texel scaled over each (region x scissor) rectangle, tinted
// with the premultiplied clear colour. A clear writes the same value wherever scissor rectangles overlap, so no union is needed.
bool PhvgTryClearRects(Surface* aS, const Color& aColor, int aX, int aY, int aW, int aH, const Array<Rectangle>& aScissors)
	{
	if (!PhvgSurfaceGpu(aS))
		return false;
	Rectangle r(0, 0, aS->getWidth(), aS->getHeight());
	r.intersect(Rectangle(aX, aY, aW, aH));
	if (r.width <= 0 || r.height <= 0)
		return true;
	int rects[64][4];
	int n = 0;
	for (int i = 0; i < aScissors.size(); i++)
		{
		Rectangle q = r;
		q.intersect(aScissors[i]);
		if (q.width <= 0 || q.height <= 0)
			continue;
		if (n >= 64)
			return false;
		rects[n][0] = q.x; rects[n][1] = q.y; rects[n][2] = q.width; rects[n][3] = q.height;
		n++;
		}
	if (n == 0)
		return true;
	Color c = aColor;
	c.clamp();
	c.convert(Color::sRGBA_PRE);
	const float mul[4] = { c.r, c.g, c.b, c.a };
	const float M[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f };		// the 1x1 texel stretched over the whole surface, clipped by the rectangles
	static const unsigned char white[4] = { 255, 255, 255, 255 };
	const float Mw[6] = { (float)aS->getWidth(), 0.0f, 0.0f, 0.0f, (float)aS->getHeight(), 0.0f };
	(void)M;
	int res = PhvgDrawImage(aS, &white, 1, 1, 2u, Mw, 1, mul, &rects[0][0], n, NULL);
	if (res == -2)
		res = PhvgDrawImage(aS, &white, 1, 1, 2u, Mw, 1, mul, &rects[0][0], n, white);
	return res == 1;
	}

// Counters for the tests: fills, clears, uploads, readbacks, declines, edges, GL errors, live shadows.
bool PhvgStats(int* aOut8)
	{
	if (gLoad != 1 || !gT.Stats)
		return false;
	gT.Stats(aOut8);
	return true;
	}

}	//namespace OpenVGRI
