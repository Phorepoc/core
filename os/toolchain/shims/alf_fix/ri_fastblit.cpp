// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// ri_fastblit.cpp -- OURS (ALF bring-up, sflibopenvgrefimpl_alf only): a straight-blit fast path for vgDrawImage.
//
// Hitchcock's VG canvas gc draws every window as a series of vgDrawImage(texture) with scale +-1 and an integer translation (the dominant canvas
// opcodes are BitBlt / BitBltMasked). The reference RI draws each of them through its generic scanline rasteriser + PixelPipe: per sample
// float colour maths -- a frame of hundreds of blits takes tens of seconds on this board (boot 777: the display swapped 8 frames in minutes).
// When the transformation is exactly a pixel-aligned flip/translate, no masking / colour transform, normal image mode, blend SRC or SRC_OVER and both
// pixels are 8-bit-per-channel 32-bit values, the result is one integer blend per pixel in the destination's own (premultiplied sRGB) space -- which is
// what PixelPipe computes after converting the source to the destination format. Anything else returns false and the caller takes the reference path.
//
// Hooked in from the generated copy of riApi.cpp (tools/alf_instrument.py EDITS: drawImage()).
#include "riDefs.h"
#define private public
#define protected public
#include "openvg.h"
#include "egl.h"
#include "riContext.h"
#include "riImage.h"
#include "riMath.h"
#include "riPath.h"
#include "riPixelPipe.h"
#undef private
#undef protected
#include <e32std.h>
#include <e32debug.h>
#include <hal.h>

namespace OpenVGRI
{
void PhvgCpuAccess(const Surface* aS, bool aWrite);	// riPhvg.cpp
bool PhvgSurfaceGpu(const Surface* aS);			// riPhvg.cpp (O4b)
unsigned PhvgImgEpoch();					// riPhvg.cpp: write generation of all Images
const unsigned* PhvgPerf();				// riPhvg.cpp: 0 surface uploads, 1 ticks, 2 readbacks, 3 ticks, 4 DrawImage calls, 5 round-trip ticks, 6 misses
int PhvgDrawImage(Surface* aS, const void* aKey, int aIw, int aIh, unsigned aHash, const float* aM, int aBlendSrc,
                  const float* aMul, const int* aRects, int aN, const unsigned char* aPixels);

static inline TInt PhRound(RIfloat v) { return (TInt)(v >= 0 ? v + 0.5f : v - 0.5f); }
static inline RIfloat PhAbs(RIfloat v) { return v < 0 ? -v : v; }

// Union of the scissor rectangles on row y, clipped to [xa,xb): sorted, merged, non-overlapping spans (the scissor is the UNION of its rectangles).
static const int KMaxClips = 128;
static int PhRowSpans(const Rectangle* clips, int n, int y, int xa, int xb, int (*sp)[2])
	{
	int ns = 0;
	for (int i = 0; i < n; i++)
		{
		const Rectangle& r = clips[i];
		if (y < r.y || y >= r.y + r.height)
			continue;
		int a = r.x > xa ? r.x : xa, b = r.x + r.width < xb ? r.x + r.width : xb;
		if (b <= a)
			continue;
		int k = ns++;
		while (k > 0 && sp[k - 1][0] > a)
			{ sp[k][0] = sp[k - 1][0]; sp[k][1] = sp[k - 1][1]; k--; }
		sp[k][0] = a; sp[k][1] = b;
		}
	int m = 0;
	for (int i = 0; i < ns; i++)
		{
		if (m > 0 && sp[i][0] <= sp[m - 1][1])
			{ if (sp[i][1] > sp[m - 1][1]) sp[m - 1][1] = sp[i][1]; }
		else
			{ sp[m][0] = sp[i][0]; sp[m][1] = sp[i][1]; m++; }
		}
	return m;
	}

// disjoint rectangles (x,y,w,h) covering the union of the clip rectangles inside bnd: y bands, merged x spans per band
static int PhBandRects(const Rectangle* clips, int n, const Rectangle& bnd, int (*out)[4], int maxOut)
	{
	int ys[2 * KMaxClips + 2];
	int ny = 0;
	ys[ny++] = bnd.y; ys[ny++] = bnd.y + bnd.height;
	for (int i = 0; i < n; i++)
		{
		const int a = clips[i].y, b = clips[i].y + clips[i].height;
		if (a > bnd.y && a < bnd.y + bnd.height) ys[ny++] = a;
		if (b > bnd.y && b < bnd.y + bnd.height) ys[ny++] = b;
		}
	for (int i = 1; i < ny; i++)
		{
		const int v = ys[i];
		int k = i;
		while (k > 0 && ys[k - 1] > v) { ys[k] = ys[k - 1]; k--; }
		ys[k] = v;
		}
	int sp[KMaxClips][2];
	int no = 0;
	for (int i = 0; i + 1 < ny; i++)
		{
		if (ys[i + 1] <= ys[i])
			continue;
		const int ns = PhRowSpans(clips, n, ys[i], bnd.x, bnd.x + bnd.width, sp);
		for (int j = 0; j < ns; j++)
			{
			if (no >= maxOut)
				return -1;
			out[no][0] = sp[j][0]; out[no][1] = ys[i]; out[no][2] = sp[j][1] - sp[j][0]; out[no][3] = ys[i + 1] - ys[i];
			no++;
			}
		}
	return no;
	}

static TUint PhHashWords(const RIuint8* base, int rows, int rowBytes, int stride)
	{
	TUint h1 = 2166136261u, h2 = 0x9e3779b9u;
	for (int y = 0; y < rows; y++)
		{
		const RIuint8* p = base + y * stride;
		int i = 0;
		if (((TUint)p & 3) == 0)
			{
			const TUint* q = (const TUint*)p;
			const int nw = rowBytes >> 2;
			int k = 0;
			for (; k + 2 <= nw; k += 2)
				{
				h1 = (h1 ^ q[k]) * 16777619u;
				h2 = (h2 ^ q[k + 1]) * 2246822519u;
				}
			if (k < nw)
				h1 = (h1 ^ q[k++]) * 16777619u;
			i = k << 2;
			}
		for (; i < rowBytes; i++)
			h1 = (h1 ^ p[i]) * 16777619u;
		h1 ^= h1 >> 15;
		}
	return h1 ^ (h2 * 31u);
	}

// content hash of a source image, validated by the write generation: an image the RI owns can only change through the RI (every Image / Surface write bumps the
// epoch), so while the epoch is the one the hash was taken at, the stored hash is still right. Images over client memory (m_ownsData false) are hashed every time.
struct PhHashRec { const Image* iImg; TInt iW, iH, iFmt; TUint iHash; TUint iEpoch; };
static PhHashRec gHashRec[64];
static TUint gHashSkips, gHashFull;
static TUint PhImageHash(Image* img, int w, int h)
	{
	const TUint ep = PhvgImgEpoch();
	const int fmt = (int)img->m_desc.internalFormat;
	PhHashRec& r = gHashRec[((TUint)img >> 4) & 63];
	if (img->m_ownsData && r.iImg == img && r.iW == w && r.iH == h && r.iFmt == fmt && r.iEpoch == ep)
		{
		++gHashSkips;
		return r.iHash;
		}
	const int bppBytes = img->m_desc.bitsPerPixel / 8;
	const TUint hash = PhHashWords(img->m_data + img->m_storageOffsetY * img->m_stride + img->m_storageOffsetX * bppBytes, h, w * bppBytes, img->m_stride)
		^ (TUint)(fmt * 2654435761u);
	r.iImg = img; r.iW = w; r.iH = h; r.iFmt = fmt; r.iHash = hash; r.iEpoch = ep;
	++gHashFull;
	return hash;
	}

static TUint gGpuImg, gGpuImgDecl, gGpuFill, gGpuFillDecl;
static TUint gHashTicks, gConvTicks;
static void PhGpuReport()
	{
	const TUint n = gGpuImg + gGpuImgDecl + gGpuFill + gGpuFillDecl;
	if ((n & 127) == 1)
		{
		static TInt freq;
		if (!freq) HAL::Get(HAL::EFastCounterFrequency, freq);
		const TUint* p = PhvgPerf();
		#define PHMS(t) ((TUint)((TUint64)(t) * 1000 / freq))
		RDebug::Printf("PH: alf: RI gpu: images %u (declined %u), rect fills %u (declined %u)", gGpuImg, gGpuImgDecl, gGpuFill, gGpuFillDecl);
		RDebug::Printf("PH: alf: RI gpu perf ms: hash %u (full %u, skipped %u) convert %u | surface upload %u (x%u) readback %u (x%u) | draw calls %u (x%u, %u misses)",
			PHMS(gHashTicks), gHashFull, gHashSkips, PHMS(gConvTicks), PHMS(p[1]), p[0], PHMS(p[3]), p[2], PHMS(p[5]), p[4], p[6]);
		}
	}

static TUint gFillDeny[12];		// 0 modes!=fill 1 mask/xform 2 blend 3 paint 4 no drawable/samples 5 dst format 6 not a rect path 7 shear 8 fractional 9 scissor overlap/count
static TUint gFillCalls;
static TUint gTimeTicks[3], gTimeN[3];	// 0 drawImage, 1 drawPath, 2 vgClear: total time INSIDE the RI call
static TUint gDeny[10];		// 0 masking/colour transform, 1 image mode, 2 blend, 3 matrix, 4 fractional, 5 format, 6 overlap, 7 paint, 8 samples, 9 other
static TUint gPrevDeny[10], gPrevFill[12];
// every RI call slower than 80 ms is logged with the fast-path refusals that happened during it (empty = the call was taken by a fast path, i.e. it was just big)
// OURS (timing builds): thread CPU time across the same calls; wall >> CPU = the call blocks
static TInt64 gCpuMark, gCpuUs[3];
TUint32 gPhDrawSplit[3];
extern TUint gPhSlowPix, gPhSlowWhy, gPhFillStat[5];	// drawPath: tessellate / rasterise / stroke ticks of the current call (riApi_ph.cpp)
void PhRiCpuMark()
	{
	TTimeIntervalMicroSeconds c;
	gPhDrawSplit[0] = gPhDrawSplit[1] = gPhDrawSplit[2] = 0; gPhSlowPix = gPhSlowWhy = 0; gPhFillStat[0] = gPhFillStat[1] = gPhFillStat[2] = gPhFillStat[3] = gPhFillStat[4] = 0;
	if (RThread().GetCpuTime(c) == KErrNone) gCpuMark = c.Int64();
	}
void PhRiTimeAdd(int kind, TUint ticks)
	{
	static TInt freq;
	if (!freq) HAL::Get(HAL::EFastCounterFrequency, freq);
	gTimeTicks[kind] += ticks; ++gTimeN[kind];
	{ TTimeIntervalMicroSeconds c; if (gCpuMark && RThread().GetCpuTime(c) == KErrNone) gCpuUs[kind] += c.Int64() - gCpuMark; gCpuMark = 0; }
	const TUint ms = (TUint)((TUint64)ticks * 1000 / freq);
	if (ms >= 80)
		{
		static TUint slow;
		if (++slow <= 200)
			{
			TBuf8<96> d;
			for (int i = 0; i < 10; i++) if (gDeny[i] != gPrevDeny[i]) d.AppendFormat(_L8("img%d "), i);
			for (int i = 0; i < 12; i++) if (gFillDeny[i] != gPrevFill[i]) d.AppendFormat(_L8("rect%d "), i);
			RDebug::Printf("PH: alf: RI slow call: %s %u ms (n=%u) tessellate %u fill %u stroke %u ms, float-path pixels %u (last 0x%08x), gpu-try %u ms, edges %u rows %u coverage evals %u pixels %u, refusals during it: %S", kind == 0 ? "drawImage" : kind == 1 ? "drawPath" : "vgClear", ms, gTimeN[kind], (TUint)((TUint64)gPhDrawSplit[0] * 1000 / freq), (TUint)((TUint64)gPhDrawSplit[1] * 1000 / freq), (TUint)((TUint64)gPhDrawSplit[2] * 1000 / freq), gPhSlowPix, gPhSlowWhy, (TUint)((TUint64)gPhFillStat[0] * 1000 / freq), gPhFillStat[1], gPhFillStat[2], gPhFillStat[3], gPhFillStat[4], &d);
			}
		}
	for (int i = 0; i < 10; i++) gPrevDeny[i] = gDeny[i];
	for (int i = 0; i < 12; i++) gPrevFill[i] = gFillDeny[i];
	}
// OURS (perf 2026-10-26): which paths does ALF draw, how often and from where. Keyed by (path, caller); every 256 vgDrawPath
// calls the 8 most expensive entries since the last report are printed and the table restarts. tx/ty = the path's translation
// on its target; caller = the return address into the ALF renderer (symbolise against the ROM map).
struct TPhPathProf { void* iPath; void* iCaller; TUint iTicks, iN; int iSegs, iTx, iTy; TUint iModes; };
static TPhPathProf gPathProf[32];
static TUint gPathProfCalls, gPathProfDropped;
void PhPathProf(void* aPath, int aSegs, int aTx, int aTy, unsigned aModes, unsigned aTicks, void* aCaller)
	{
	TPhPathProf* e = NULL;
	for (int i = 0; i < 32 && !e; i++)
		if (gPathProf[i].iN && gPathProf[i].iPath == aPath && gPathProf[i].iCaller == aCaller) e = &gPathProf[i];
	for (int i = 0; i < 32 && !e; i++)
		if (!gPathProf[i].iN) { e = &gPathProf[i]; e->iPath = aPath; e->iCaller = aCaller; }
	if (e) { e->iTicks += aTicks; ++e->iN; e->iSegs = aSegs; e->iTx = aTx; e->iTy = aTy; e->iModes = aModes; }
	else ++gPathProfDropped;
	if ((++gPathProfCalls & 255) != 0)
		return;
	static TInt freq;
	if (!freq) HAL::Get(HAL::EFastCounterFrequency, freq);
	RDebug::Printf("PH: alf: path prof: 256 drawPath calls, %u not tracked; top by time:", gPathProfDropped);
	for (int k = 0; k < 8; k++)
		{
		int best = -1;
		for (int i = 0; i < 32; i++)
			if (gPathProf[i].iN && (best < 0 || gPathProf[i].iTicks > gPathProf[best].iTicks)) best = i;
		if (best < 0) break;
		const TPhPathProf& b = gPathProf[best];
		RDebug::Printf("PH: alf: path prof: path %08x caller %08x n=%u %u us segs=%d at %d,%d modes=%u",
			(TUint)b.iPath, (TUint)b.iCaller, b.iN, (TUint)((TUint64)b.iTicks * 1000000 / freq), b.iSegs, b.iTx, b.iTy, b.iModes);
		gPathProf[best].iN = 0;
		}
	for (int i = 0; i < 32; i++) gPathProf[i].iN = 0;
	gPathProfDropped = 0;
	}
static TUint gBlitPix[2], gBlitTicks[2], gBlitMulPix;	// (timing builds) PhRiFastDrawImage CPU loops: pixels and ticks
static void PhTimeReport()
	{
	static TInt freq;
	if (!freq) HAL::Get(HAL::EFastCounterFrequency, freq);
	const TUint n = gTimeN[0] + gTimeN[1] + gTimeN[2];
	if ((n & 255) == 0 && n)
		RDebug::Printf("PH: alf: RI time inside the calls: drawImage %u ms (x%u), drawPath %u ms (x%u), vgClear %u ms (x%u)",
			(TUint)((TUint64)gTimeTicks[0] * 1000 / freq), gTimeN[0], (TUint)((TUint64)gTimeTicks[1] * 1000 / freq), gTimeN[1], (TUint)((TUint64)gTimeTicks[2] * 1000 / freq), gTimeN[2]);
	if ((n & 255) == 0 && n)
		RDebug::Printf("PH: alf: RI blit loops: same-format %u px %u ms, general %u px %u ms (multiply %u px)",
			gBlitPix[0], (TUint)((TUint64)gBlitTicks[0] * 1000 / freq), gBlitPix[1], (TUint)((TUint64)gBlitTicks[1] * 1000 / freq), gBlitMulPix);
	}
static TUint gFmtN;
static void PhDeny(TInt aWhy) { ++gDeny[aWhy]; }
static void PhReport(TUint calls, TUint taken)
	{
	if ((calls & 63) == 0)
		RDebug::Printf("PH: alf: RI fastpath: drawImage calls %u taken %u; refused: mask/xform %u mode %u blend %u matrix %u fractional %u format %u overlap %u paint %u samples %u other %u",
			calls, taken, gDeny[0], gDeny[1], gDeny[2], gDeny[3], gDeny[4], gDeny[5], gDeny[6], gDeny[7], gDeny[8], gDeny[9]);
	}

static void PhImageToRgba(Image* img, TUint8* out)
	{
	const Color::Descriptor& sd = img->m_desc;
	const TInt w = img->m_width, h = img->m_height, bpp = sd.bitsPerPixel;
	const TUint rs = sd.redShift, gs = sd.greenShift, bs = sd.blueShift, as = sd.alphaShift, ls = sd.luminanceShift;
	const TUint rmax = sd.redBits ? (1u << sd.redBits) - 1 : 0, gmax = sd.greenBits ? (1u << sd.greenBits) - 1 : 0, bmax = sd.blueBits ? (1u << sd.blueBits) - 1 : 0;
	const TUint amax = sd.alphaBits ? (1u << sd.alphaBits) - 1 : 0, lmax = sd.luminanceBits ? (1u << sd.luminanceBits) - 1 : 0;
	const bool srcPre = sd.isPremultiplied();
	for (TInt y = 0; y < h; y++)
		{
		const RIuint8* row = img->m_data + (y + img->m_storageOffsetY) * img->m_stride;
		for (TInt x = 0; x < w; x++)
			{
			const TInt i = x + img->m_storageOffsetX;
			TUint sa, sr, sg, sb;
			if (bpp == 32)
				{
				const RIuint32 p = ((const RIuint32*)row)[i];
				sa = amax ? ((p >> as) & 255) : 255;
				sr = (p >> rs) & 255; sg = (p >> gs) & 255; sb = (p >> bs) & 255;
				}
			else
				{
				const TUint p = bpp == 16 ? ((const RIuint16*)row)[i] : row[i];
				if (lmax)
					{ sr = sg = sb = (((p >> ls) & lmax) * 255 + lmax / 2) / lmax; }
				else
					{
					sr = rmax ? (((p >> rs) & rmax) * 255 + rmax / 2) / rmax : 255;
					sg = gmax ? (((p >> gs) & gmax) * 255 + gmax / 2) / gmax : 255;
					sb = bmax ? (((p >> bs) & bmax) * 255 + bmax / 2) / bmax : 255;
					}
				sa = amax ? (((p >> as) & amax) * 255 + amax / 2) / amax : 255;
				}
			if (!srcPre && sa != 255)
				{
				sr = (sr * sa + 127) / 255;
				sg = (sg * sa + 127) / 255;
				sb = (sb * sa + 127) / 255;
				}
			TUint8* o = out + (y * w + x) * 4;
			o[0] = (TUint8)sr; o[1] = (TUint8)sg; o[2] = (TUint8)sb; o[3] = (TUint8)sa;
			}
		}
	}

bool PhRiFastDrawImage(VGContext* c, Image* img, const Matrix3x3& m)
	{
	static TUint calls, taken, denied;
	++calls;
	PhReport(calls, taken);
	PhTimeReport();
	Drawable* d = c->getCurrentDrawable();
	if (!d)
		return false;
	Surface* s = d->getColorBuffer();
	if (!s || s->getNumSamples() != 1)
		{ PhDeny(8); return false; }
	bool ok = true;
	if (c->m_masking || c->m_colorTransform)
		{ PhDeny(0); ok = false; }
	// VG_DRAW_IMAGE_MULTIPLY with a solid colour paint = the image times the paint colour (Hitchcock's opacity / tint)
	Paint* paint = (Paint*)c->m_fillPaint;
	bool multiply = false;
	if (ok && c->m_imageMode == VG_DRAW_IMAGE_MULTIPLY)
		{
		if (!paint || paint->m_paintType != VG_PAINT_TYPE_COLOR)
			{ PhDeny(7); ok = false; }
		multiply = true;
		}
	else if (ok && c->m_imageMode != VG_DRAW_IMAGE_NORMAL)
		{ PhDeny(1); ok = false; }
	if (ok && c->m_blendMode != VG_BLEND_SRC && c->m_blendMode != VG_BLEND_SRC_OVER)
		{ PhDeny(2); ok = false; }
	if (!ok || !m.isAffine())
		{
		++denied;
		return false;
		}
	const RIfloat a = m[0].x, b = m[0].y, tx = m[0].z, cc = m[1].x, dd = m[1].y, ty = m[1].z;
	if (PhAbs(b) > 1e-3f || PhAbs(cc) > 1e-3f || PhAbs(PhAbs(a) - 1.0f) > 1e-3f || PhAbs(PhAbs(dd) - 1.0f) > 1e-3f)
		{
		PhDeny(3);
		++denied;
		return false;
		}
	const TInt itx = PhRound(tx), ity = PhRound(ty);
	if (PhAbs(tx - (RIfloat)itx) > 1e-3f || PhAbs(ty - (RIfloat)ity) > 1e-3f)
		{
		PhDeny(4);
		++denied;
		return false;
		}
	const bool flipX = a < 0, flipY = dd < 0;
	Image* dst = s->m_image;
	if (!dst || !dst->m_data || !img->m_data)
		return false;
	const Color::Descriptor& sd = img->m_desc;
	const Color::Descriptor& dsd = dst->m_desc;
	const bool srcColour = sd.redBits + sd.greenBits + sd.blueBits + sd.luminanceBits > 0;
	if (dsd.bitsPerPixel != 32 || dsd.redBits != 8 || dsd.greenBits != 8 || dsd.blueBits != 8 || dsd.alphaBits != 8 || !dsd.isPremultiplied() || dsd.isLuminance() ||
		(sd.bitsPerPixel != 8 && sd.bitsPerPixel != 16 && sd.bitsPerPixel != 32) || (srcColour && sd.isNonlinear() != dsd.isNonlinear()))
		{
		if (gFmtN < 12)
			{
			gFmtN++;
			RDebug::Printf("PH: alf: RI fast vgDrawImage refused format: src fmt %x bpp %d rgbl %d%d%d%d a %d, dst fmt %x bpp %d", (unsigned)sd.internalFormat, sd.bitsPerPixel, sd.redBits, sd.greenBits, sd.blueBits, sd.luminanceBits, sd.alphaBits, (unsigned)dsd.internalFormat, dsd.bitsPerPixel);
			}
		PhDeny(5);
		++denied;
		return false;
		}
	const bool srcAlpha = sd.alphaBits > 0, srcPre = sd.isPremultiplied();
	// destination rectangle (surface space) of the transformed image
	const TInt w = img->m_width, h = img->m_height;
	const TInt x0 = flipX ? itx - w : itx, x1 = x0 + w;
	const TInt y0 = flipY ? ity - h : ity, y1 = y0 + h;
	// clip rectangles: the surface, or the union of the scissor rectangles
	Rectangle clips[KMaxClips];
	int nclip = 0;
	const int sw = s->getWidth(), sh = s->getHeight();
	if (c->m_scissoring)
		{
		const int n = c->m_scissor.size();
		if (n > KMaxClips)
			{ PhDeny(6); return false; }
		for (int i = 0; i < n; i++)
			if (c->m_scissor[i].width > 0 && c->m_scissor[i].height > 0)
				clips[nclip++] = c->m_scissor[i];
		}
	else
		{
		clips[0].x = 0; clips[0].y = 0; clips[0].width = sw; clips[0].height = sh;
		nclip = 1;
		}
	if (PhvgSurfaceGpu(s))
		{
		Rectangle bnd; bnd.x = 0; bnd.y = 0; bnd.width = sw; bnd.height = sh;
		Rectangle fp; fp.x = x0; fp.y = y0; fp.width = w; fp.height = h;
		bnd.intersect(fp);
		if (bnd.width <= 0 || bnd.height <= 0)
			return true;
		int rects[64][4];
		const int nr = PhBandRects(clips, nclip, bnd, rects, 64);
		if (nr == 0)
			return true;
		if (nr > 0)
			{
			TUint mr = 255, mg = 255, mb = 255, ma = 255;
			if (multiply)
				{
				Color pc = paint->m_paintColor;
				pc.convert(Color::sRGBA_PRE);
				mr = (TUint)(pc.r * 255.0f + 0.5f); mg = (TUint)(pc.g * 255.0f + 0.5f); mb = (TUint)(pc.b * 255.0f + 0.5f); ma = (TUint)(pc.a * 255.0f + 0.5f);
				if (mr > 255) mr = 255;
				if (mg > 255) mg = 255;
				if (mb > 255) mb = 255;
				if (ma > 255) ma = 255;
				}
			const float M[6] = { flipX ? -1.0f : 1.0f, 0.0f, (float)itx, 0.0f, flipY ? -1.0f : 1.0f, (float)ity };
			const float mul[4] = { multiply ? mr / 255.0f : 1.0f, multiply ? mg / 255.0f : 1.0f, multiply ? mb / 255.0f : 1.0f, multiply ? ma / 255.0f : 1.0f };
			const TUint th0 = User::FastCounter();
			const TUint hash = PhImageHash(img, w, h);
			gHashTicks += User::FastCounter() - th0;
			const int blendSrc = c->m_blendMode == VG_BLEND_SRC;
			int r = PhvgDrawImage(s, img, w, h, hash, M, blendSrc, mul, &rects[0][0], nr, NULL);
			if (r == -2)
				{
				TUint8* buf = (TUint8*)User::Alloc(w * h * 4);
				if (buf)
					{
					const TUint tc0 = User::FastCounter();
					PhImageToRgba(img, buf);
					gConvTicks += User::FastCounter() - tc0;
					r = PhvgDrawImage(s, img, w, h, hash, M, blendSrc, mul, &rects[0][0], nr, buf);
					User::Free(buf);
					}
				else
					r = 0;
				}
			if (r == 1)
				{
				++taken; ++gGpuImg;
				PhGpuReport();
				return true;
				}
			}
		++gGpuImgDecl;
		PhGpuReport();
		}
	PhvgCpuAccess(s, true);
	const TInt dstOffX = dst->m_storageOffsetX, dstOffY = dst->m_storageOffsetY;
	const TInt srcOffX = img->m_storageOffsetX, srcOffY = img->m_storageOffsetY;
	const TUint rs = sd.redShift, gs = sd.greenShift, bs = sd.blueShift, as = sd.alphaShift;
	const TUint drs = dsd.redShift, dgs = dsd.greenShift, dbs = dsd.blueShift, das = dsd.alphaShift;
	const bool over = c->m_blendMode == VG_BLEND_SRC_OVER;
	TUint mr = 255, mg = 255, mb = 255, ma = 255;
	if (multiply)
		{
		Color pc = paint->m_paintColor;
		pc.convert(Color::sRGBA_PRE);
		mr = (TUint)(pc.r * 255.0f + 0.5f); mg = (TUint)(pc.g * 255.0f + 0.5f); mb = (TUint)(pc.b * 255.0f + 0.5f); ma = (TUint)(pc.a * 255.0f + 0.5f);
		if (mr > 255) mr = 255;
		if (mg > 255) mg = 255;
		if (mb > 255) mb = 255;
		if (ma > 255) ma = 255;
		}
	{
	Rectangle bnd; bnd.x = 0; bnd.y = 0; bnd.width = sw; bnd.height = sh;
	Rectangle dr; dr.x = x0; dr.y = y0; dr.width = w; dr.height = h;
	bnd.intersect(dr);
	const TInt bpp = sd.bitsPerPixel;
	const TUint rmax = sd.redBits ? (1u << sd.redBits) - 1 : 0, gmax = sd.greenBits ? (1u << sd.greenBits) - 1 : 0, bmax = sd.blueBits ? (1u << sd.blueBits) - 1 : 0;
	const TUint amax = sd.alphaBits ? (1u << sd.alphaBits) - 1 : 0, lmax = sd.luminanceBits ? (1u << sd.luminanceBits) - 1 : 0;
	const TUint ls = sd.luminanceShift;
	// (perf 2026-10-26) a 16/8-bit source channel scaled to 8 bits by a table built once per call: the per-pixel
	// "/ max" were library divisions (no hardware divide in the v5TE build)
	TUint8 eR[256], eG[256], eB[256], eA[256], eL[256];
	if (bpp != 32)
		{
		for (TUint v = 0; v <= rmax; v++) eR[v] = (TUint8)((v * 255 + rmax / 2) / rmax);
		for (TUint v = 0; v <= gmax; v++) eG[v] = (TUint8)((v * 255 + gmax / 2) / gmax);
		for (TUint v = 0; v <= bmax; v++) eB[v] = (TUint8)((v * 255 + bmax / 2) / bmax);
		for (TUint v = 0; v <= amax; v++) eA[v] = (TUint8)((v * 255 + amax / 2) / amax);
		for (TUint v = 0; v <= lmax; v++) eL[v] = (TUint8)((v * 255 + lmax / 2) / lmax);
		}
	int sp[KMaxClips][2];
	// (perf 2026-10-26) the common case -- a 32-bit premultiplied (or opaque) source in the destination's channel order,
	// not mirrored, no tint: a row copy where the result is the source, else two channels at a time in one multiply
	const bool same32 = bpp == 32 && !multiply && !flipX && rs == drs && gs == dgs && bs == dbs &&
		(srcAlpha ? (srcPre && as == das) : true);
#ifdef PH_RI_TIMING
	const TUint phB0 = User::FastCounter();
	if (!same32 && bnd.width * bnd.height >= 4096)
		{	// which big images miss the same-format loop (first 12 distinct)
		static TUint seen[12]; static int nseen;
		const TUint key = (TUint)sd.internalFormat << 24 ^ (TUint)bpp << 16 ^ rs << 12 ^ gs << 8 ^ bs << 4 ^ as ^ (multiply ? 1u << 31 : 0) ^ (flipX ? 1u << 30 : 0) ^ (srcPre ? 1u << 29 : 0);
		int k = 0;
		while (k < nseen && seen[k] != key) k++;
		if (k == nseen && nseen < 12)
			{
			seen[nseen++] = key;
			RDebug::Printf("PH: alf: RI blit general: %dx%d src fmt %x bpp %d shifts r%u g%u b%u a%u alpha %d pre %d, dst fmt %x shifts r%u g%u b%u a%u, flipX %d multiply %d",
				w, h, (unsigned)sd.internalFormat, bpp, rs, gs, bs, as, (int)srcAlpha, (int)srcPre, (unsigned)dsd.internalFormat, drs, dgs, dbs, das, (int)flipX, (int)multiply);
			}
		}
#endif
	if (same32)
		{
		const RIuint32 amask = 255u << das;
		for (TInt y = bnd.y; y < bnd.y + bnd.height; y++)
			{
			const int ns = PhRowSpans(clips, nclip, y, bnd.x, bnd.x + bnd.width, sp);
			const TInt j = flipY ? ity - y - 1 : y - ity;
			const RIuint32* srow = (const RIuint32*)(img->m_data + (j + srcOffY) * img->m_stride) + srcOffX - itx;
			RIuint32* drow = (RIuint32*)(dst->m_data + (y + dstOffY) * dst->m_stride) + dstOffX;
			for (int si = 0; si < ns; si++)
				{
				const TInt xa = sp[si][0], xb = sp[si][1];
				if (!srcAlpha)
					{
					for (TInt x = xa; x < xb; x++)
						drow[x] = srow[x] | amask;
					}
				else if (!over)
					Mem::Copy(drow + xa, srow + xa, (xb - xa) * 4);
				else
					for (TInt x = xa; x < xb; x++)
						{
						const RIuint32 s = srow[x];
						const RIuint32 sa = (s >> das) & 255;
						if (sa == 255)
							drow[x] = s;
						else if (sa != 0)
							{
							const RIuint32 ia = 255 - sa, d = drow[x];
							RIuint32 rb = (d & 0x00ff00ffu) * ia, ag = ((d >> 8) & 0x00ff00ffu) * ia;
							rb = ((rb + 0x00800080u + ((rb >> 8) & 0x00ff00ffu)) >> 8) & 0x00ff00ffu;	// /255, rounded
							ag = ((ag + 0x00800080u + ((ag >> 8) & 0x00ff00ffu)) >> 8) & 0x00ff00ffu;
							drow[x] = s + (rb | (ag << 8));
							}
						}
				}
			}
		}
	else
	for (TInt y = bnd.y; y < bnd.y + bnd.height; y++)
		{
		const int ns = PhRowSpans(clips, nclip, y, bnd.x, bnd.x + bnd.width, sp);
		const TInt j = flipY ? ity - y - 1 : y - ity;
		const RIuint8* srow8 = img->m_data + (j + srcOffY) * img->m_stride;
		RIuint32* drow = (RIuint32*)(dst->m_data + (y + dstOffY) * dst->m_stride);
		for (int si = 0; si < ns; si++)
			for (TInt x = sp[si][0]; x < sp[si][1]; x++)
				{
				const TInt i = (flipX ? itx - x - 1 : x - itx) + srcOffX;
				TUint sa, sr, sg, sb;
				if (bpp == 32)
					{
					const RIuint32 p = ((const RIuint32*)srow8)[i];
					sa = srcAlpha ? ((p >> as) & 255) : 255;
					sr = (p >> rs) & 255; sg = (p >> gs) & 255; sb = (p >> bs) & 255;
					}
				else
					{
					const TUint p = bpp == 16 ? ((const RIuint16*)srow8)[i] : srow8[i];
					if (lmax)
						{ sr = sg = sb = eL[(p >> ls) & lmax]; }
					else
						{
						sr = rmax ? eR[(p >> rs) & rmax] : 255;
						sg = gmax ? eG[(p >> gs) & gmax] : 255;
						sb = bmax ? eB[(p >> bs) & bmax] : 255;
						}
					sa = amax ? eA[(p >> as) & amax] : 255;
					}
				if (!srcPre && sa != 255)
					{
					sr = (sr * sa + 127) / 255;
					sg = (sg * sa + 127) / 255;
					sb = (sb * sa + 127) / 255;
					}
				if (multiply)
					{
					sr = (sr * mr + 127) / 255; sg = (sg * mg + 127) / 255; sb = (sb * mb + 127) / 255; sa = (sa * ma + 127) / 255;
					}
				RIuint32& q = drow[x + dstOffX];
				if (!over || sa == 255)
					{
					q = (sr << drs) | (sg << dgs) | (sb << dbs) | (sa << das);
					}
				else if (sa != 0)
					{
					const TUint ia = 255 - sa;
					const TUint dr_ = (q >> drs) & 255, dg_ = (q >> dgs) & 255, db_ = (q >> dbs) & 255, da_ = (q >> das) & 255;
					TUint orr = sr + (dr_ * ia + 127) / 255, og = sg + (dg_ * ia + 127) / 255, ob = sb + (db_ * ia + 127) / 255, oa = sa + (da_ * ia + 127) / 255;
					if (orr > 255) orr = 255;
					if (og > 255) og = 255;
					if (ob > 255) ob = 255;
					if (oa > 255) oa = 255;
					q = (orr << drs) | (og << dgs) | (ob << dbs) | (oa << das);
					}
				}
		}
#ifdef PH_RI_TIMING
	gBlitTicks[same32 ? 0 : 1] += User::FastCounter() - phB0;
	gBlitPix[same32 ? 0 : 1] += bnd.width * bnd.height;
	if (multiply) gBlitMulPix += bnd.width * bnd.height;
#endif
	}
	++taken;
	if (taken == 1 || (taken & 1023) == 0)
		RDebug::Printf("PH: alf: RI fast vgDrawImage: taken %u, refused %u, calls %u (last: %dx%d at %d,%d flip %d%d, %d clip rects, %s)", taken, denied, calls, w, h, itx, ity, (int)flipX, (int)flipY, nclip, multiply ? "multiply" : "normal");
	return true;
	}

// ---- vgDrawPath of an axis-aligned integer rectangle with a solid colour fill (Hitchcock's CHuiVg10Gc::DrawRect: the unit rect path under translate/scale)
static TUint gRectLog;
static bool PhPathIsRect(Path* path, RIfloat corner[4][2])
	{
	const int nseg = path->getNumSegments();
	bool ok = nseg >= 4 && nseg <= 7;
	RIfloat vx[8], vy[8];
	int nv = 0, ci = 0;
	RIfloat cx = 0, cy = 0;
	const int ncoord = path->getNumCoordinates();
	for (int i = 0; ok && i < nseg; i++)
		{
		const VGPathSegment sg = Path::getPathSegment(path->m_segments[i]);
		RIfloat x = cx, y = cy;
		// getPathSegment() masks the relative bit off (0x1e): the segment byte's bit 0 says relative, the masked value equals the *_ABS enum
		const bool rel = (path->m_segments[i] & 1) != 0;
		switch (sg)
			{
		case VG_MOVE_TO_ABS: case VG_LINE_TO_ABS:
			if (ci + 2 > ncoord || (sg == VG_MOVE_TO_ABS && i != 0)) { ok = false; break; }
			x = path->getCoordinate(ci) + (rel ? cx : 0); y = path->getCoordinate(ci + 1) + (rel ? cy : 0); ci += 2; break;
		case VG_HLINE_TO_ABS: if (ci + 1 > ncoord) { ok = false; break; } x = path->getCoordinate(ci++) + (rel ? cx : 0); break;
		case VG_VLINE_TO_ABS: if (ci + 1 > ncoord) { ok = false; break; } y = path->getCoordinate(ci++) + (rel ? cy : 0); break;
		case VG_CLOSE_PATH:
			if (i != nseg - 1) ok = false;
			break;
		default:
			ok = false;
			}
		if (!ok || sg == VG_CLOSE_PATH)
			continue;
		if (nv >= 8) { ok = false; break; }
		vx[nv] = cx = x; vy[nv] = cy = y; nv++;
		}
	if (ok && nv == 5 && PhAbs(vx[4] - vx[0]) < 5e-3f && PhAbs(vy[4] - vy[0]) < 5e-3f)
		nv = 4;
	if (ok && nv == 4)
		{
		const bool a = PhAbs(vx[0] - vx[1]) < 5e-3f && PhAbs(vy[1] - vy[2]) < 5e-3f && PhAbs(vx[2] - vx[3]) < 5e-3f && PhAbs(vy[3] - vy[0]) < 5e-3f;
		const bool b = PhAbs(vy[0] - vy[1]) < 5e-3f && PhAbs(vx[1] - vx[2]) < 5e-3f && PhAbs(vy[2] - vy[3]) < 5e-3f && PhAbs(vx[3] - vx[0]) < 5e-3f;
		if (a || b)
			{
			for (int i = 0; i < 4; i++) { corner[i][0] = vx[i]; corner[i][1] = vy[i]; }
			return true;
			}
		}
	if (gRectLog < 10)
		{
		gRectLog++;
		TBuf8<80> sgs;
		for (int i = 0; i < nseg && i < 12; i++) sgs.AppendFormat(_L8("%x "), (unsigned)path->m_segments[i]);
		RDebug::Printf("PH: alf: RI fast rect: not a rect: %d segs [%S] %d coords type %d nv %d ok %d", nseg, &sgs, ncoord, (int)path->getDatatype(), nv, (int)ok);
		}
	return false;
	}

// ---- linear gradient LUT (256 entries of premultiplied sRGB 8888 bytes), cached per colour ramp
struct PhGradKey { RIfloat v[8 * 5]; int n; int spread; int pre; };
struct PhGradLut { PhGradKey key; bool used; TUint8 rgba[256][4]; };
static PhGradLut gGradLut[4];
static TUint gGradNext;
static const TUint8 (*PhGradientLut(Paint* paint))[4]
	{
	PhGradKey k;
	Mem::FillZ(&k, sizeof(k));
	const int n = paint->m_colorRampStops.size();
	if (n < 2 || n > 8)
		return NULL;
	k.n = n; k.spread = (int)paint->m_colorRampSpreadMode; k.pre = (int)paint->m_colorRampPremultiplied;
	for (int i = 0; i < n; i++)
		{
		const Paint::GradientStop& g = paint->m_colorRampStops[i];
		k.v[i * 5] = g.offset; k.v[i * 5 + 1] = g.color.r; k.v[i * 5 + 2] = g.color.g; k.v[i * 5 + 3] = g.color.b; k.v[i * 5 + 4] = g.color.a;
		}
	for (int i = 0; i < 4; i++)
		if (gGradLut[i].used && Mem::Compare((const TUint8*)&gGradLut[i].key, sizeof(k), (const TUint8*)&k, sizeof(k)) == 0)
			return gGradLut[i].rgba;
	PhGradLut& e = gGradLut[gGradNext++ & 3];
	for (int i = 0; i < 256; i++)
		{
		// point-evaluate the ramp at the entry centre (the RI filters over one pixel of gradient; for piecewise linear ramps that only differs at the stops)
		const RIfloat gr = ((RIfloat)i + 0.5f) / 256.0f;
		int si = n - 1;
		RIfloat t = 0;
		for (int j = 0; j < n - 1; j++)
			if (gr >= paint->m_colorRampStops[j].offset && gr < paint->m_colorRampStops[j + 1].offset)
				{
				const RIfloat so = paint->m_colorRampStops[j].offset, eo = paint->m_colorRampStops[j + 1].offset;
				si = j; t = (gr - so) / (eo - so);
				break;
				}
		Color sc = paint->m_colorRampStops[si].color, ec = sc;
		if (si < n - 1)
			ec = paint->m_colorRampStops[si + 1].color;
		else
			t = 0;
		if (paint->m_colorRampPremultiplied)
			{ sc.premultiply(); ec.premultiply(); }
		Color c = (1.0f - t) * sc + t * ec;
		if (!paint->m_colorRampPremultiplied)
			c.premultiply();
		RIfloat ch[4] = { c.r, c.g, c.b, c.a };
		for (int j = 0; j < 4; j++)
			{
			RIfloat v = ch[j] * 255.0f + 0.5f;
			e.rgba[i][j] = (TUint8)(v < 0 ? 0 : v > 255 ? 255 : (TInt)v);
			}
		}
	e.key = k; e.used = true;
	return e.rgba;
	}

static inline TInt PhGradIndex(RIfloat g, int spread)
	{
	if (spread == VG_COLOR_RAMP_SPREAD_PAD)
		return g <= 0 ? 0 : g >= 1 ? 255 : (TInt)(g * 256.0f);
	TInt fl = (TInt)g;
	if (g < (RIfloat)fl) fl--;
	g -= (RIfloat)fl;
	if (spread == VG_COLOR_RAMP_SPREAD_REFLECT && (fl & 1))
		g = 1.0f - g;
	TInt gi = (TInt)(g * 256.0f);
	return gi > 255 ? 255 : gi;
	}

static const TUint8 gWhitePx[4] = { 255, 255, 255, 255 };

bool PhRiFastFillRect(VGContext* c, VGPath vgpath, const Matrix3x3& m, VGbitfield paintModes)
	{
	static TUint taken;
	const TUint calls = ++gFillCalls;
	if ((calls & 63) == 1)
		{
		RDebug::Printf("PH: alf: RI fast rect: vgDrawPath calls %u taken %u; refused: modes %u xform %u blend %u paint %u (null %u grad %u) surf %u",
			calls, taken, gFillDeny[0], gFillDeny[1], gFillDeny[2], gFillDeny[3], gFillDeny[10], gFillDeny[11], gFillDeny[4]);
		RDebug::Printf("PH: alf: RI fast rect (cont): dst-format %u not-rect %u shear %u fractional %u scissor %u", gFillDeny[5], gFillDeny[6], gFillDeny[7], gFillDeny[8], gFillDeny[9]);
		}
	if (paintModes != VG_FILL_PATH) { ++gFillDeny[0]; return false; }
	if (c->m_masking || c->m_colorTransform || !m.isAffine()) { ++gFillDeny[1]; return false; }
	if (c->m_blendMode != VG_BLEND_SRC && c->m_blendMode != VG_BLEND_SRC_OVER) { ++gFillDeny[2]; return false; }
	Paint* paint = (Paint*)c->m_fillPaint;
	const bool grad = paint && paint->m_paintType == VG_PAINT_TYPE_LINEAR_GRADIENT;
	if (!paint || (paint->m_paintType != VG_PAINT_TYPE_COLOR && !grad))
		{
		++gFillDeny[3];
		if (!paint) ++gFillDeny[10]; else ++gFillDeny[11];
		static TUint pl;
		if (paint && pl < 4) { pl++; RDebug::Printf("PH: alf: RI fast rect: refused paint type %x", (unsigned)paint->m_paintType); }
		return false;
		}
	Drawable* d = c->getCurrentDrawable();
	if (!d) { ++gFillDeny[4]; return false; }
	Surface* s = d->getColorBuffer();
	if (!s || s->getNumSamples() != 1) { ++gFillDeny[4]; return false; }
	Image* dst = s->m_image;
	const Color::Descriptor& dsd = dst->m_desc;
	if (!dst->m_data || dsd.bitsPerPixel != 32 || dsd.redBits != 8 || dsd.greenBits != 8 || dsd.blueBits != 8 || dsd.alphaBits != 8 || !dsd.isPremultiplied() || dsd.isLuminance() || !dsd.isNonlinear())
		{ ++gFillDeny[5]; return false; }
	RIfloat cn[4][2];
	if (!PhPathIsRect((Path*)vgpath, cn))
		{ ++gFillDeny[6]; return false; }
	if (PhAbs(m[0].y) > 1e-3f || PhAbs(m[1].x) > 1e-3f)
		{ ++gFillDeny[7]; return false; }
	RIfloat xs[4], ys[4];
	for (int i = 0; i < 4; i++)
		{
		xs[i] = m[0].x * cn[i][0] + m[0].z;
		ys[i] = m[1].y * cn[i][1] + m[1].z;
		}
	RIfloat xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
	for (int i = 1; i < 4; i++)
		{
		if (xs[i] < xmin) xmin = xs[i];
		if (xs[i] > xmax) xmax = xs[i];
		if (ys[i] < ymin) ymin = ys[i];
		if (ys[i] > ymax) ymax = ys[i];
		}
	const TInt ix0 = PhRound(xmin), ix1 = PhRound(xmax), iy0 = PhRound(ymin), iy1 = PhRound(ymax);
	if (PhAbs(xmin - ix0) > 8e-3f || PhAbs(xmax - ix1) > 8e-3f || PhAbs(ymin - iy0) > 8e-3f || PhAbs(ymax - iy1) > 8e-3f)
		{ ++gFillDeny[8]; return false; }			// fractional edges: anti-aliased coverage -> reference path
	// the paint colour in the destination's premultiplied space
	Color pc = paint->m_paintColor;
	pc.convert(Color::sRGBA_PRE);
	TUint pr = (TUint)(pc.r * 255.0f + 0.5f), pg = (TUint)(pc.g * 255.0f + 0.5f), pb = (TUint)(pc.b * 255.0f + 0.5f), pa = (TUint)(pc.a * 255.0f + 0.5f);
	if (pr > 255) pr = 255;
	if (pg > 255) pg = 255;
	if (pb > 255) pb = 255;
	if (pa > 255) pa = 255;
	const int sw = s->getWidth(), sh = s->getHeight();
	Rectangle clips[KMaxClips];
	int nclip = 0;
	if (c->m_scissoring)
		{
		const int n = c->m_scissor.size();
		if (n > KMaxClips)
			{ ++gFillDeny[9]; return false; }
		for (int i = 0; i < n; i++)
			if (c->m_scissor[i].width > 0 && c->m_scissor[i].height > 0)
				clips[nclip++] = c->m_scissor[i];
		}
	else
		{
		clips[0].x = 0; clips[0].y = 0; clips[0].width = sw; clips[0].height = sh;
		nclip = 1;
		}
	const TUint8 (*lut)[4] = NULL;
	RIfloat gdx = 0, gdy = 0, gc = 0;
	if (grad)
		{
		Matrix3x3 s2p = m * c->m_fillPaintToUser;
		Vector2 p0 = paint->m_linearGradientPoint0, p1 = paint->m_linearGradientPoint1;
		Vector2 dl = p1 - p0;
		const RIfloat dd = dl.x * dl.x + dl.y * dl.y;
		if (!s2p.invert() || dd <= 0.0f || (lut = PhGradientLut(paint)) == NULL)
			{ ++gFillDeny[11]; return false; }
		s2p[2].set(0, 0, 1);
		Vector2 zero = affineTransform(s2p, Vector2(0, 0));
		gdx = (s2p[0][0] * dl.x + s2p[1][0] * dl.y) / dd;
		gdy = (s2p[0][1] * dl.x + s2p[1][1] * dl.y) / dd;
		gc = ((zero.x - p0.x) * dl.x + (zero.y - p0.y) * dl.y) / dd + 0.5f * (gdx + gdy);
		}
	if (PhvgSurfaceGpu(s))
		{
		Rectangle bnd; bnd.x = 0; bnd.y = 0; bnd.width = sw; bnd.height = sh;
		Rectangle fr; fr.x = ix0; fr.y = iy0; fr.width = ix1 - ix0; fr.height = iy1 - iy0;
		bnd.intersect(fr);
		if (bnd.width <= 0 || bnd.height <= 0)
			return true;
		int rects[64][4];
		const int nr = PhBandRects(clips, nclip, bnd, rects, 64);
		if (nr == 0)
			return true;
		int r = 0;
		if (nr > 0 && !grad)
			{
			const float M[6] = { (float)(ix1 - ix0), 0.0f, (float)ix0, 0.0f, (float)(iy1 - iy0), (float)iy0 };
			const float mul[4] = { pr / 255.0f, pg / 255.0f, pb / 255.0f, pa / 255.0f };
			r = PhvgDrawImage(s, &gWhitePx, 1, 1, 1u, M, c->m_blendMode == VG_BLEND_SRC, mul, &rects[0][0], nr, NULL);
			if (r == -2)
				r = PhvgDrawImage(s, &gWhitePx, 1, 1, 1u, M, c->m_blendMode == VG_BLEND_SRC, mul, &rects[0][0], nr, gWhitePx);
			}
		else if (nr > 0 && grad && (PhAbs(gdx) < 1e-6f || PhAbs(gdy) < 1e-6f))
			{
			// a ramp varying along one axis only: a 1 x h (or w x 1) texture of the per-pixel colours, stretched over the rectangle
			const bool vertical = PhAbs(gdx) < 1e-6f;
			const int n = vertical ? iy1 - iy0 : ix1 - ix0;
			TUint8* tex = n > 0 && n <= 4096 ? (TUint8*)User::Alloc(n * 4) : NULL;
			if (tex)
				{
				const int spread = (int)paint->m_colorRampSpreadMode;
				for (int i = 0; i < n; i++)
					{
					const RIfloat g = vertical ? gc + gdy * (RIfloat)(iy0 + i) : gc + gdx * (RIfloat)(ix0 + i);
					Mem::Copy(tex + i * 4, lut[PhGradIndex(g, spread)], 4);
					}
				const TUint hash = PhHashWords(tex, 1, n * 4, n * 4) ^ (TUint)(vertical ? 0x5bd1e995 : 0x1b873593);
				const void* key = (const void*)(0x80000000u | (hash & 0x7fffffffu));
				const float M[6] = { vertical ? (float)(ix1 - ix0) : 1.0f, 0.0f, (float)ix0, 0.0f, vertical ? 1.0f : (float)(iy1 - iy0), (float)iy0 };
				const float one[4] = { 1, 1, 1, 1 };
				const int tw = vertical ? 1 : n, th = vertical ? n : 1;
				const int bs = c->m_blendMode == VG_BLEND_SRC;
				r = PhvgDrawImage(s, key, tw, th, hash, M, bs, one, &rects[0][0], nr, NULL);
				if (r == -2)
					r = PhvgDrawImage(s, key, tw, th, hash, M, bs, one, &rects[0][0], nr, tex);
				User::Free(tex);
				}
			}
		if (r == 1)
			{
			++taken; ++gGpuFill;
			PhGpuReport();
			return true;
			}
		++gGpuFillDecl;
		PhGpuReport();
		}
	PhvgCpuAccess(s, true);
	const TUint drs = dsd.redShift, dgs = dsd.greenShift, dbs = dsd.blueShift, das = dsd.alphaShift;
	const RIuint32 packed = (pr << drs) | (pg << dgs) | (pb << dbs) | (pa << das);
	const bool over = c->m_blendMode == VG_BLEND_SRC_OVER && pa != 255;
	if (grad)
		{
		const bool gover = c->m_blendMode == VG_BLEND_SRC_OVER;
		const int spread = (int)paint->m_colorRampSpreadMode;
		Rectangle bnd; bnd.x = 0; bnd.y = 0; bnd.width = sw; bnd.height = sh;
		Rectangle fr; fr.x = ix0; fr.y = iy0; fr.width = ix1 - ix0; fr.height = iy1 - iy0;
		bnd.intersect(fr);
		int sp[KMaxClips][2];
		for (TInt y = bnd.y; y < bnd.y + bnd.height; y++)
			{
			const int ns = PhRowSpans(clips, nclip, y, bnd.x, bnd.x + bnd.width, sp);
			RIuint32* drow = (RIuint32*)(dst->m_data + (y + dst->m_storageOffsetY) * dst->m_stride) + dst->m_storageOffsetX;
			for (int si = 0; si < ns; si++)
				for (TInt x = sp[si][0]; x < sp[si][1]; x++)
					{
					RIfloat g = gc + gdx * (RIfloat)x + gdy * (RIfloat)y;
					const TInt gi = PhGradIndex(g, spread);
					const TUint8* e = lut[gi];
					const TUint sr = e[0], sg = e[1], sb = e[2], sa = e[3];
					RIuint32& q = drow[x];
					if (!gover || sa == 255)
						q = (sr << drs) | (sg << dgs) | (sb << dbs) | (sa << das);
					else if (sa != 0)
						{
						const TUint ia = 255 - sa;
						TUint orr = sr + (((q >> drs) & 255) * ia + 127) / 255, og = sg + (((q >> dgs) & 255) * ia + 127) / 255;
						TUint ob = sb + (((q >> dbs) & 255) * ia + 127) / 255, oa = sa + (((q >> das) & 255) * ia + 127) / 255;
						if (orr > 255) orr = 255;
						if (og > 255) og = 255;
						if (ob > 255) ob = 255;
						if (oa > 255) oa = 255;
						q = (orr << drs) | (og << dgs) | (ob << dbs) | (oa << das);
						}
					}
			}
		++taken;
		return true;
		}
	{
	Rectangle bnd; bnd.x = 0; bnd.y = 0; bnd.width = sw; bnd.height = sh;
	Rectangle fr; fr.x = ix0; fr.y = iy0; fr.width = ix1 - ix0; fr.height = iy1 - iy0;
	bnd.intersect(fr);
	int sp[KMaxClips][2];
	const TUint ia = 255 - pa;
	for (TInt y = bnd.y; y < bnd.y + bnd.height; y++)
		{
		const int ns = PhRowSpans(clips, nclip, y, bnd.x, bnd.x + bnd.width, sp);
		RIuint32* drow = (RIuint32*)(dst->m_data + (y + dst->m_storageOffsetY) * dst->m_stride) + dst->m_storageOffsetX;
		for (int si = 0; si < ns; si++)
			{
			if (!over)
				for (TInt x = sp[si][0]; x < sp[si][1]; x++)
					drow[x] = packed;
			else
				for (TInt x = sp[si][0]; x < sp[si][1]; x++)
					{
					const RIuint32 q = drow[x];
					TUint orr = pr + (((q >> drs) & 255) * ia + 127) / 255, og = pg + (((q >> dgs) & 255) * ia + 127) / 255;
					TUint ob = pb + (((q >> dbs) & 255) * ia + 127) / 255, oa = pa + (((q >> das) & 255) * ia + 127) / 255;
					if (orr > 255) orr = 255;
					if (og > 255) og = 255;
					if (ob > 255) ob = 255;
					if (oa > 255) oa = 255;
					drow[x] = (orr << drs) | (og << dgs) | (ob << dbs) | (oa << das);
					}
			}
		}
	}
	++taken;
	if (taken == 1 || (taken & 1023) == 0)
		RDebug::Printf("PH: alf: RI fast rect fill: taken %u of %u calls (last %d,%d-%d,%d colour %08x)", taken, calls, ix0, iy0, ix1, iy1, (unsigned)packed);
	return true;
	}
}
