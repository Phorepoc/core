// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// vgprobe2.cpp -- OURS: OpenVG O2 test. One scene that uses ONLY what the GPU engine
// (phvg) implements -- vgClear (full + partial) and solid-colour vgDrawPath(FILL): cubic blob, curves, a pentagram
// non-zero and even-odd, a donut with a hole (even-odd) and a filled one (non-zero), translucent SRC_OVER fills -- is
// drawn through the standard libEGL + libOpenVG twice: with the process mode property = 0 (the reference implementation)
// and = 1 (phvg). Prints the checksums, the pixels differing by more than 2 per channel (edges differ: the GPU
// antialiases by supersampling, the RI by its sample pattern), the gross differences (> 96, a geometry bug), and the
// time per frame over 100 frames of each. O4 adds a gradient scene (linear pad/repeat/reflect, radial with a focal point,
// translucent, transformed) and a stroke scene (joins, caps, dashes, a curve, translucent self-overlap, a gradient stroke,
// fill+stroke) drawn through both engines: gross-difference budget, an interior accuracy bound for the gradients, and the
// engine's gradient/stroke counters must show the GPU did them. A second, MIXED scene interleaves GPU fills with things phvg does not do
// (a gradient, a stroke) to check that the two engines compose on one surface.
#include <e32std.h>
#include <e32debug.h>
#include <e32property.h>
#include <fbs.h>
#include <EGL/egl.h>
#include <VG/openvg.h>
#include <VG/vgu.h>
#include "phvg.h"

const TInt KW = 128, KH = 128, KFrames = 100;

static VGPath NewPath()
	{
	return vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F, 1.0f, 0.0f, 0, 0, VG_PATH_CAPABILITY_ALL);
	}

static VGPaint ColorPaint(VGfloat r, VGfloat g, VGfloat b, VGfloat a)
	{
	VGPaint p = vgCreatePaint();
	const VGfloat c[4] = { r, g, b, a };
	vgSetParameteri(p, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	vgSetParameterfv(p, VG_PAINT_COLOR, 4, c);
	return p;
	}

static void FillPath(VGPath aPath, VGPaint aPaint, VGFillRule aRule)
	{
	vgSeti(VG_FILL_RULE, aRule);
	vgSetPaint(aPaint, VG_FILL_PATH);
	vgDrawPath(aPath, VG_FILL_PATH);
	}

// a pentagram of radius r around (cx, cy): five line segments joining every second vertex
static VGPath Star(VGfloat cx, VGfloat cy, VGfloat r)
	{
	static const VGfloat u[5][2] = { { 0.0f, -1.0f }, { 0.5878f, 0.8090f }, { -0.9511f, -0.3090f },
	                                 { 0.9511f, -0.3090f }, { -0.5878f, 0.8090f } };
	VGPath p = NewPath();
	const VGubyte seg[] = { VG_MOVE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS, VG_CLOSE_PATH };
	VGfloat d[10];
	for (TInt i = 0; i < 5; i++)
		{
		d[i * 2] = cx + r * u[i][0];
		d[i * 2 + 1] = cy + r * u[i][1];
		}
	vgAppendPathData(p, 6, seg, d);
	return p;
	}

static void ClearAll(VGfloat r, VGfloat g, VGfloat b)
	{
	const VGfloat c[4] = { r, g, b, 1.0f };
	vgSetfv(VG_CLEAR_COLOR, 4, c);
	vgClear(0, 0, KW, KH);
	}

// the O2-only scene
static void SceneGpu()
	{
	ClearAll(0.10f, 0.20f, 0.30f);
	const VGfloat c2[4] = { 0.90f, 0.90f, 0.80f, 1.0f };
	vgSetfv(VG_CLEAR_COLOR, 4, c2);
	vgClear(6, 6, 52, 30);							// a partial clear

	VGPaint orange = ColorPaint(1.0f, 0.5f, 0.0f, 1.0f);
	VGPaint blue = ColorPaint(0.1f, 0.3f, 1.0f, 1.0f);
	VGPaint green = ColorPaint(0.0f, 0.9f, 0.2f, 0.5f);
	VGPaint magenta = ColorPaint(0.9f, 0.1f, 0.7f, 0.6f);
	VGPaint white = ColorPaint(1.0f, 1.0f, 1.0f, 1.0f);

	// 1. a blob made of two cubics
	VGPath blob = NewPath();
	const VGubyte bseg[] = { VG_MOVE_TO_ABS, VG_CUBIC_TO_ABS, VG_CUBIC_TO_ABS, VG_CLOSE_PATH };
	const VGfloat bd[] = { 8.f, 70.f,  8.f, 40.f, 50.f, 40.f, 50.f, 70.f,  50.f, 100.f, 8.f, 100.f, 8.f, 70.f };
	vgAppendPathData(blob, 4, bseg, bd);
	FillPath(blob, orange, VG_NON_ZERO);

	// 2. the pentagram, non-zero (solid centre) and even-odd (hollow centre)
	VGPath s1 = Star(96.f, 34.f, 28.f);
	FillPath(s1, blue, VG_NON_ZERO);
	VGPath s2 = Star(96.f, 92.f, 28.f);
	FillPath(s2, blue, VG_EVEN_ODD);

	// 3. a translucent star over the blob (SRC_OVER)
	VGPath s3 = Star(36.f, 80.f, 26.f);
	FillPath(s3, green, VG_NON_ZERO);

	// 4. a donut: two same-direction circles in one path; even-odd leaves the hole
	VGPath donut = NewPath();
	vguEllipse(donut, 30.f, 108.f, 36.f, 36.f);
	vguEllipse(donut, 30.f, 108.f, 16.f, 16.f);
	FillPath(donut, magenta, VG_EVEN_ODD);

	// 5. a rounded rect and a plain rect (horizontal edges)
	VGPath rr = NewPath();
	vguRoundRect(rr, 62.f, 4.f, 26.f, 14.f, 8.f, 8.f);
	FillPath(rr, white, VG_NON_ZERO);
	VGPath rc = NewPath();
	vguRect(rc, 70.f, 60.f, 20.f, 10.f);
	FillPath(rc, white, VG_NON_ZERO);

	vgDestroyPath(blob); vgDestroyPath(s1); vgDestroyPath(s2); vgDestroyPath(s3);
	vgDestroyPath(donut); vgDestroyPath(rr); vgDestroyPath(rc);
	vgDestroyPaint(orange); vgDestroyPaint(blue); vgDestroyPaint(green); vgDestroyPaint(magenta); vgDestroyPaint(white);
	}

// GPU fills interleaved with what the GPU engine does not implement: composition on one surface
static void SceneMixed()
	{
	ClearAll(0.0f, 0.0f, 0.0f);
	VGPaint orange = ColorPaint(1.0f, 0.5f, 0.0f, 1.0f);
	VGPath s1 = Star(64.f, 64.f, 56.f);
	FillPath(s1, orange, VG_NON_ZERO);							// GPU
	VGPaint grad = vgCreatePaint();								// RI: a linear gradient rect over it
	vgSetParameteri(grad, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
	const VGfloat line[4] = { 20.f, 0.f, 108.f, 0.f };
	vgSetParameterfv(grad, VG_PAINT_LINEAR_GRADIENT, 4, line);
	const VGfloat stops[10] = { 0.0f, 1.f, 0.f, 0.f, 1.f,  1.0f, 0.f, 0.f, 1.f, 1.f };
	vgSetParameterfv(grad, VG_PAINT_COLOR_RAMP_STOPS, 10, stops);
	vgSetPaint(grad, VG_FILL_PATH);
	VGPath r = NewPath();
	vguRect(r, 20.f, 40.f, 88.f, 24.f);
	vgDrawPath(r, VG_FILL_PATH);
	VGPaint teal = ColorPaint(0.0f, 0.8f, 0.8f, 0.5f);
	VGPath e = NewPath();
	vguEllipse(e, 64.f, 70.f, 70.f, 50.f);
	FillPath(e, teal, VG_NON_ZERO);							// GPU again, over the RI's pixels
	VGPaint white = ColorPaint(1.0f, 1.0f, 1.0f, 1.0f);			// RI: a stroke
	vgSetPaint(white, VG_STROKE_PATH);
	vgSetf(VG_STROKE_LINE_WIDTH, 4.0f);
	vgDrawPath(s1, VG_STROKE_PATH);
	FillPath(r, teal, VG_EVEN_ODD);							// GPU a third time
	vgDestroyPath(s1); vgDestroyPath(r); vgDestroyPath(e);
	vgDestroyPaint(orange); vgDestroyPaint(grad); vgDestroyPaint(teal); vgDestroyPaint(white);
	}

// ---- O4: gradients and strokes (the things the icons use) -------------------------------------------------------------

static VGPaint GradPaint(VGPaintType aType, const VGfloat* aGeom, const VGfloat* aStops, TInt aStopCount, VGColorRampSpreadMode aMode)
	{
	VGPaint p = vgCreatePaint();
	vgSetParameteri(p, VG_PAINT_TYPE, aType);
	vgSetParameterfv(p, aType == VG_PAINT_TYPE_LINEAR_GRADIENT ? VG_PAINT_LINEAR_GRADIENT : VG_PAINT_RADIAL_GRADIENT,
	                 aType == VG_PAINT_TYPE_LINEAR_GRADIENT ? 4 : 5, aGeom);
	vgSetParameteri(p, VG_PAINT_COLOR_RAMP_SPREAD_MODE, aMode);
	vgSetParameterfv(p, VG_PAINT_COLOR_RAMP_STOPS, aStopCount * 5, aStops);
	return p;
	}

static void IdentityAll()
	{
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
	vgLoadIdentity();
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER);
	vgLoadIdentity();
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_STROKE_PAINT_TO_USER);
	vgLoadIdentity();
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
	}

static void RectFill(VGfloat aX, VGfloat aY, VGfloat aW, VGfloat aH, VGPaint aPaint)
	{
	VGPath r = NewPath();
	vguRect(r, aX, aY, aW, aH);
	FillPath(r, aPaint, VG_NON_ZERO);
	vgDestroyPath(r);
	}

// boxes well inside the gradient shapes of SceneGrad, for the pixel-accuracy check (VG coordinates, y up)
static const TInt KGradBoxes[][4] = { { 8, 8, 56, 32 },			// linear pad, horizontal
                                      { 8, 48, 56, 72 },		// linear reflect, diagonal
                                      { 84, 48, 108, 72 } };	// radial with a focal point
const TInt KGradBoxCount = 3;

static void SceneGrad()
	{
	ClearAll(0.15f, 0.15f, 0.20f);
	IdentityAll();
	// opaque three-stop ramp, translucent ramps
	const VGfloat rgb3[15] = { 0.0f, 1.0f, 0.1f, 0.1f, 1.0f,   0.5f, 0.1f, 0.9f, 0.2f, 1.0f,   1.0f, 0.1f, 0.2f, 1.0f, 1.0f };
	const VGfloat fade[10] = { 0.0f, 1.0f, 1.0f, 0.2f, 0.9f,   1.0f, 0.2f, 0.4f, 1.0f, 0.1f };
	const VGfloat sun[10] = { 0.0f, 1.0f, 0.95f, 0.3f, 1.0f,   1.0f, 0.5f, 0.0f, 0.4f, 1.0f };

	// 1. linear, pad: left to right
	const VGfloat l1[4] = { 12.f, 0.f, 52.f, 0.f };
	VGPaint g1 = GradPaint(VG_PAINT_TYPE_LINEAR_GRADIENT, l1, rgb3, 3, VG_COLOR_RAMP_SPREAD_PAD);
	RectFill(4.f, 4.f, 60.f, 36.f, g1);
	// 2. linear, repeat, the paint rotated by 30 degrees about the origin (paint-to-user)
	const VGfloat l2[4] = { 68.f, 4.f, 84.f, 4.f };
	VGPaint g2 = GradPaint(VG_PAINT_TYPE_LINEAR_GRADIENT, l2, rgb3, 3, VG_COLOR_RAMP_SPREAD_REPEAT);
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER);
	vgLoadIdentity();
	vgRotate(30.f);
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
	RectFill(68.f, 4.f, 56.f, 36.f, g2);
	IdentityAll();
	// 3. linear, reflect, diagonal
	const VGfloat l3[4] = { 4.f, 44.f, 28.f, 76.f };
	VGPaint g3 = GradPaint(VG_PAINT_TYPE_LINEAR_GRADIENT, l3, rgb3, 3, VG_COLOR_RAMP_SPREAD_REFLECT);
	RectFill(4.f, 44.f, 60.f, 32.f, g3);
	// 4. radial with a focal point, in a circle
	const VGfloat r4[5] = { 96.f, 60.f, 104.f, 60.f, 24.f };
	VGPaint g4 = GradPaint(VG_PAINT_TYPE_RADIAL_GRADIENT, r4, sun, 2, VG_COLOR_RAMP_SPREAD_PAD);
	VGPath e = NewPath();
	vguEllipse(e, 96.f, 60.f, 48.f, 48.f);
	FillPath(e, g4, VG_NON_ZERO);
	// 5. radial, repeat, small radius, on a star
	const VGfloat r5[5] = { 32.f, 100.f, 32.f, 100.f, 9.f };
	VGPaint g5 = GradPaint(VG_PAINT_TYPE_RADIAL_GRADIENT, r5, rgb3, 3, VG_COLOR_RAMP_SPREAD_REPEAT);
	VGPath s = Star(32.f, 104.f, 22.f);
	FillPath(s, g5, VG_EVEN_ODD);
	// 6. translucent linear gradient over the existing pixels (SRC_OVER), under a user-to-surface translate + rotate + scale
	const VGfloat l6[4] = { 0.f, 0.f, 48.f, 0.f };
	VGPaint g6 = GradPaint(VG_PAINT_TYPE_LINEAR_GRADIENT, l6, fade, 2, VG_COLOR_RAMP_SPREAD_PAD);
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
	vgLoadIdentity();
	vgTranslate(70.f, 90.f);
	vgRotate(-20.f);
	vgScale(1.2f, 0.7f);
	RectFill(0.f, 0.f, 48.f, 40.f, g6);
	IdentityAll();
	vgDestroyPath(e); vgDestroyPath(s);
	vgDestroyPaint(g1); vgDestroyPaint(g2); vgDestroyPaint(g3); vgDestroyPaint(g4); vgDestroyPaint(g5); vgDestroyPaint(g6);
	}

static void SceneStroke()
	{
	ClearAll(0.05f, 0.05f, 0.10f);
	IdentityAll();
	VGPaint white = ColorPaint(1.0f, 1.0f, 1.0f, 1.0f);
	VGPaint orange = ColorPaint(1.0f, 0.5f, 0.0f, 1.0f);
	VGPaint glass = ColorPaint(0.2f, 0.6f, 1.0f, 0.5f);			// translucent: the union of the pieces must not double-paint
	VGPaint teal = ColorPaint(0.0f, 0.8f, 0.8f, 1.0f);
	const VGfloat bw[10] = { 0.0f, 1.0f, 0.9f, 0.2f, 1.0f,   1.0f, 0.2f, 0.3f, 1.0f, 1.0f };
	const VGfloat lg[4] = { 4.f, 0.f, 60.f, 0.f };
	VGPaint grad = GradPaint(VG_PAINT_TYPE_LINEAR_GRADIENT, lg, bw, 2, VG_COLOR_RAMP_SPREAD_PAD);

	// 1. a zig-zag, miter joins, butt caps
	VGPath z = NewPath();
	const VGubyte zs[] = { VG_MOVE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS };
	const VGfloat zd[] = { 6.f, 10.f, 22.f, 34.f, 36.f, 10.f, 52.f, 34.f, 60.f, 12.f };
	vgAppendPathData(z, 5, zs, zd);
	vgSetf(VG_STROKE_LINE_WIDTH, 6.0f);
	vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
	vgSetPaint(white, VG_STROKE_PATH);
	vgDrawPath(z, VG_STROKE_PATH);
	// 2. a cubic, round caps and joins
	VGPath c = NewPath();
	const VGubyte cs[] = { VG_MOVE_TO_ABS, VG_CUBIC_TO_ABS };
	const VGfloat cd[] = { 70.f, 12.f,  80.f, 50.f, 108.f, -4.f, 122.f, 34.f };
	vgAppendPathData(c, 2, cs, cd);
	vgSetf(VG_STROKE_LINE_WIDTH, 8.0f);
	vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_ROUND);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_ROUND);
	vgSetPaint(orange, VG_STROKE_PATH);
	vgDrawPath(c, VG_STROKE_PATH);
	// 3. square caps, bevel joins
	VGPath b = NewPath();
	const VGubyte bs[] = { VG_MOVE_TO_ABS, VG_LINE_TO_ABS, VG_LINE_TO_ABS };
	const VGfloat bd[] = { 8.f, 52.f, 30.f, 72.f, 54.f, 48.f };
	vgAppendPathData(b, 3, bs, bd);
	vgSetf(VG_STROKE_LINE_WIDTH, 7.0f);
	vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_SQUARE);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_BEVEL);
	vgSetPaint(teal, VG_STROKE_PATH);
	vgDrawPath(b, VG_STROKE_PATH);
	// 4. a dashed ellipse
	VGPath d = NewPath();
	vguEllipse(d, 94.f, 66.f, 40.f, 28.f);
	const VGfloat dash[2] = { 7.f, 4.f };
	vgSetfv(VG_STROKE_DASH_PATTERN, 2, dash);
	vgSetf(VG_STROKE_LINE_WIDTH, 3.0f);
	vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
	vgSetPaint(white, VG_STROKE_PATH);
	vgDrawPath(d, VG_STROKE_PATH);
	vgSetfv(VG_STROKE_DASH_PATTERN, 0, NULL);
	// 5. a translucent stroke of a self-crossing star (the union: no double paint where the pieces overlap)
	VGPath s = Star(34.f, 100.f, 24.f);
	vgSetf(VG_STROKE_LINE_WIDTH, 8.0f);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_ROUND);
	vgSetPaint(glass, VG_STROKE_PATH);
	vgDrawPath(s, VG_STROKE_PATH);
	// 6. a gradient stroke of a rounded rect, under a non-uniform scale (the stroke is scaled too)
	VGPath rr = NewPath();
	vguRoundRect(rr, 4.f, 4.f, 52.f, 40.f, 14.f, 14.f);
	vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
	vgLoadIdentity();
	vgTranslate(68.f, 86.f);
	vgScale(1.0f, 0.7f);
	vgSetf(VG_STROKE_LINE_WIDTH, 5.0f);
	vgSetPaint(grad, VG_STROKE_PATH);
	vgDrawPath(rr, VG_STROKE_PATH);
	// 7. fill + stroke in one call
	vgLoadIdentity();
	VGPath pz = NewPath();
	vguRect(pz, 100.f, 90.f, 20.f, 24.f);
	vgSetf(VG_STROKE_LINE_WIDTH, 4.0f);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
	vgSetPaint(orange, VG_FILL_PATH);
	vgSetPaint(white, VG_STROKE_PATH);
	vgDrawPath(pz, VG_FILL_PATH | VG_STROKE_PATH);
	IdentityAll();
	vgSetf(VG_STROKE_LINE_WIDTH, 1.0f);
	vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
	vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
	vgDestroyPath(z); vgDestroyPath(c); vgDestroyPath(b); vgDestroyPath(d); vgDestroyPath(s); vgDestroyPath(rr); vgDestroyPath(pz);
	vgDestroyPaint(white); vgDestroyPaint(orange); vgDestroyPaint(glass); vgDestroyPaint(teal); vgDestroyPaint(grad);
	}

// the largest per-channel difference inside the interior boxes of SceneGrad
static TInt BoxMax(const TUint32* aRef, const TUint32* aGpu)
	{
	TInt mx = 0;
	for (TInt b = 0; b < KGradBoxCount; b++)
		for (TInt y = KGradBoxes[b][1]; y < KGradBoxes[b][3]; y++)
			for (TInt x = KGradBoxes[b][0]; x < KGradBoxes[b][2]; x++)
				for (TInt sh = 0; sh < 32; sh += 8)
					{
					TInt d = (TInt)((aRef[y * KW + x] >> sh) & 0xFF) - (TInt)((aGpu[y * KW + x] >> sh) & 0xFF);
					if (d < 0)
						d = -d;
					if (d > mx)
						mx = d;
					}
	return mx;
	}

// ms x 100 per frame over 20 frames of aScene (the GPU frames finish at the readback)
static TInt TimeScene100(void (*aScene)(), TUint32* aPix)
	{
	const TUint t0 = User::NTickCount();
	for (TInt i = 0; i < 20; i++)
		{
		aScene();
		vgFinish();
		}
	vgReadPixels(aPix, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
	return (TInt)(((TUint32)(User::NTickCount() - t0) * 100u) / 20);
	}

static TUint32 Checksum(const TUint32* aPix)
	{
	TUint32 sum = 2166136261u;				// FNV-1a over the pixel words
	for (TInt i = 0; i < KW * KH; i++)
		{
		sum ^= aPix[i];
		sum *= 16777619u;
		}
	return sum;
	}

static void SetMode(TInt aMode)
	{
	const TUid cat = TUid::Uid((TInt)(TUint32)RProcess().SecureId());
	RProperty::Define(cat, KPhvgModeKey, RProperty::EInt);		// (KErrAlreadyExists is fine)
	RProperty::Set(cat, KPhvgModeKey, aMode);
	}

// ms per frame x 100 over KFrames runs of aScene (+ aRead: a vgReadPixels after each frame)
// (User::NTickCount: nanokernel ticks, 1 ms on this kernel)
static TInt TimeMs100(void (*aScene)(), TBool aRead, TUint32* aPix)
	{
	const TUint t0 = User::NTickCount();
	for (TInt i = 0; i < KFrames; i++)
		{
		aScene();
		vgFinish();
		if (aRead)
			vgReadPixels(aPix, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
		}
	if (!aRead)
		vgReadPixels(aPix, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);	// the GPU frames only finish at a readback
	const TUint t1 = User::NTickCount();
	return (TInt)(((TUint32)(t1 - t0) * 100u) / KFrames);		// ms * 100 / frame
	}

static EGLDisplay gDpy;
static EGLSurface gSurf;
static CFbsBitmap* gBmp;

// O3: the UI path -- a frame drawn, then copied into a CFbsBitmap the way VGISymbianCopyToBitmap does (eglCopyBuffers)
static TInt TimeBitmap100(void (*aScene)())
	{
	if (!gBmp)
		return -1;
	const TUint t0 = User::NTickCount();
	for (TInt i = 0; i < KFrames; i++)
		{
		aScene();
		eglCopyBuffers(gDpy, gSurf, (EGLNativePixmapType)gBmp);
		}
	const TUint t1 = User::NTickCount();
	return (TInt)(((TUint32)(t1 - t0) * 100u) / KFrames);
	}

struct TResult
	{
	TUint32 iSumGpuScene, iSumMixed;
	TInt iDrawOnly100, iDrawRead100, iBitmap100;
	TUint32 iBmpSum;
	};

static void RunMode(TInt aMode, TUint32* aPixScene, TUint32* aPixMixed, TResult& aR)
	{
	SetMode(aMode);
	SceneGpu();
	vgFinish();
	vgReadPixels(aPixScene, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
	aR.iSumGpuScene = Checksum(aPixScene);
	SceneMixed();
	vgFinish();
	vgReadPixels(aPixMixed, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
	aR.iSumMixed = Checksum(aPixMixed);
	static TUint32 scratch[KW * KH];
	aR.iDrawOnly100 = TimeMs100(SceneGpu, EFalse, scratch);
	aR.iDrawRead100 = TimeMs100(SceneGpu, ETrue, scratch);
	aR.iBitmap100 = TimeBitmap100(SceneGpu);
	aR.iBmpSum = 0;
	if (gBmp)
		{
		SceneGpu();
		eglCopyBuffers(gDpy, gSurf, (EGLNativePixmapType)gBmp);
		gBmp->BeginDataAccess();
		const TUint32* px = (const TUint32*)gBmp->DataAddress();
		TUint32 sum = 2166136261u;
		for (TInt i = 0; i < KW * KH; i++)
			{
			sum ^= px[i];
			sum *= 16777619u;
			}
		gBmp->EndDataAccess(ETrue);
		aR.iBmpSum = sum;
		}
	}

static void Compare(const char* aName, const TUint32* aRef, const TUint32* aGpu, TInt& aOver2, TInt& aGross, TInt& aMax)
	{
	aOver2 = aGross = aMax = 0;
	for (TInt i = 0; i < KW * KH; i++)
		{
		TInt worst = 0;
		for (TInt sh = 0; sh < 32; sh += 8)
			{
			TInt d = (TInt)((aRef[i] >> sh) & 0xFF) - (TInt)((aGpu[i] >> sh) & 0xFF);
			if (d < 0)
				d = -d;
			if (d > worst)
				worst = d;
			}
		if (worst > 2)
			aOver2++;
		if (worst > 96)
			aGross++;
		if (worst > aMax)
			aMax = worst;
		}
	RDebug::Printf("PH: vg2 %s: pixels differing by more than 2: %d of %d, by more than 96: %d, max channel diff %d", aName,
	               aOver2, KW * KH, aGross, aMax);
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("PH: vg2 start (O2: phvg GPU engine vs the reference implementation, %dx%d, %d frames)", KW, KH, KFrames);
	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint maj = 0, min = 0;
	if (!eglInitialize(dpy, &maj, &min) || !eglBindAPI(EGL_OPENVG_API))
		{
		RDebug::Printf("PH: vg2 result: FAIL (eglInitialize / eglBindAPI(OPENVG))");
		return KErrGeneral;
		}
	const EGLint cfgAttr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
	                           EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	EGLConfig cfg = 0;
	EGLint n = 0;
	const EGLint pbAttr[] = { EGL_WIDTH, KW, EGL_HEIGHT, KH, EGL_NONE };
	if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1)
		{
		RDebug::Printf("PH: vg2 result: FAIL (eglChooseConfig)");
		return KErrGeneral;
		}
	EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbAttr);
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, NULL);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx))
		{
		RDebug::Printf("PH: vg2 result: FAIL (surface/context 0x%x)", eglGetError());
		return KErrGeneral;
		}

	gDpy = dpy;
	gSurf = surf;
	gBmp = NULL;
	if (RFbsSession::Connect() == KErrNone)
		{
		CFbsBitmap* b = new CFbsBitmap;
		if (b && b->Create(TSize(KW, KH), EColor16MA) == KErrNone)
			gBmp = b;
		else
			delete b;
		}
	RDebug::Printf("PH: vg2 bitmap target: %s", gBmp ? "CFbsBitmap EColor16MA" : "none (no FBS)");

	static TUint32 refScene[KW * KH], refMixed[KW * KH], gpuScene[KW * KH], gpuMixed[KW * KH];
	TResult ref, gpu;
	RunMode(0, refScene, refMixed, ref);
	RunMode(1, gpuScene, gpuMixed, gpu);
	SetMode(0);

	// ---- O4: gradients + strokes, reference vs phvg ---------------------------------------------------------------------
	static TUint32 refGrad[KW * KH], gpuGrad[KW * KH], refStroke[KW * KH], gpuStroke[KW * KH], scratch4[KW * KH];
	TInt gradMs[2] = { 0, 0 }, strokeMs[2] = { 0, 0 };
	TInt before[12] = { 0 }, after[12] = { 0 };
	typedef void (*TStats4)(TInt*);
	TStats4 stats4 = NULL;
	RLibrary lib4;
	_LIT(KPhvg4, "phvg.dll");
	if (lib4.Load(KPhvg4) == KErrNone)
		stats4 = (TStats4)lib4.Lookup(EPhvgStats);
	for (TInt mode = 0; mode < 2; mode++)
		{
		SetMode(mode);
		if (mode == 1 && stats4)
			stats4(before);
		SceneGrad();
		vgFinish();
		vgReadPixels(mode ? gpuGrad : refGrad, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
		SceneStroke();
		vgFinish();
		vgReadPixels(mode ? gpuStroke : refStroke, KW * 4, VG_sARGB_8888, 0, 0, KW, KH);
		if (mode == 1 && stats4)
			stats4(after);
		gradMs[mode] = TimeScene100(SceneGrad, scratch4);
		strokeMs[mode] = TimeScene100(SceneStroke, scratch4);
		}
	SetMode(0);
	const TInt gpuGradFills = after[9] - before[9], gpuStrokeFills = after[10] - before[10], gpuDeclined = after[4] - before[4];
	RDebug::Printf("PH: vg2 O4 counters (one scene each): gradient fills %d stroke fills %d declined %d glErrors %d", gpuGradFills,
	               gpuStrokeFills, gpuDeclined, after[6]);
	TInt gO2 = 0, gGross = 0, gMax = 0, sO2 = 0, sGross = 0, sMax = 0;
	Compare("gradient", refGrad, gpuGrad, gO2, gGross, gMax);
	Compare("stroke", refStroke, gpuStroke, sO2, sGross, sMax);
	{	// diagnostics for a failing stroke scene: sample pixels and the differing pixels per band of 16 rows (VG y up)
		static const TInt pts[6][2] = { { 1, 1 }, { 126, 126 }, { 64, 64 }, { 30, 10 }, { 100, 20 }, { 10, 100 } };
		for (TInt i = 0; i < 6; i++)
			RDebug::Printf("PH: vg2 stroke dbg pixel (%d,%d): reference %08x phvg %08x", pts[i][0], pts[i][1],
			               refStroke[pts[i][1] * KW + pts[i][0]], gpuStroke[pts[i][1] * KW + pts[i][0]]);
		TInt band[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		for (TInt i = 0; i < KW * KH; i++)
			if (refStroke[i] != gpuStroke[i])
				band[(i / KW) / 16]++;
		RDebug::Printf("PH: vg2 stroke dbg differing pixels per 16-row band: %d %d %d %d %d %d %d %d", band[0], band[1], band[2],
		               band[3], band[4], band[5], band[6], band[7]);
	}
	const TInt boxMax = BoxMax(refGrad, gpuGrad);
	RDebug::Printf("PH: vg2 gradient interior (3 boxes away from edges): max channel diff %d (budget 12)", boxMax);
	RDebug::Printf("PH: vg2 time O4 (ms per frame x100, draw only): gradient scene reference %d phvg %d; stroke scene reference %d phvg %d",
	               gradMs[0], gradMs[1], strokeMs[0], strokeMs[1]);
	const TBool o4Engaged = gpuGradFills >= 5 && gpuStrokeFills >= 5;
	const TBool o4Pass = o4Engaged && gGross <= KW * KH / 100 && sGross <= KW * KH / 100 && boxMax <= 12;
	RDebug::Printf("PH: vg2 O4 result: gradients %s, strokes %s, gross gradient %d stroke %d, interior %d -> %s",
	               gpuGradFills >= 5 ? "ENGAGED" : "NOT ENGAGED", gpuStrokeFills >= 5 ? "ENGAGED" : "NOT ENGAGED", gGross, sGross,
	               boxMax, o4Pass ? "PASS" : "FAIL");

	// the engine's own counters (the same phvg.dll instance the RI loaded)
	TInt st[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	TBool engaged = EFalse;
	RLibrary lib;
	_LIT(KPhvg, "phvg.dll");
	if (lib.Load(KPhvg) == KErrNone)
		{
		typedef void (*TStats)(TInt*);
		TStats stats = (TStats)lib.Lookup(EPhvgStats);
		if (stats)
			{
			stats(st);
			engaged = st[0] > 0 && st[1] > 0;
			}
		}
	RDebug::Printf("PH: vg2 phvg counters: fills %d clears %d uploads %d readbacks %d declined %d edges %d glErrors %d liveShadows %d directReads %d",
	               st[0], st[1], st[2], st[3], st[4], st[5], st[6], st[7], st[8]);

	TInt over2 = 0, gross = 0, mx = 0, mOver2 = 0, mGross = 0, mMx = 0;
	RDebug::Printf("PH: vg2 scene: reference %08x phvg %08x", ref.iSumGpuScene, gpu.iSumGpuScene);
	Compare("scene", refScene, gpuScene, over2, gross, mx);
	RDebug::Printf("PH: vg2 mixed: reference %08x phvg %08x", ref.iSumMixed, gpu.iSumMixed);
	Compare("mixed", refMixed, gpuMixed, mOver2, mGross, mMx);
	RDebug::Printf("PH: vg2 time (ms per frame x100): scene draw-only reference %d phvg %d; scene draw+vgReadPixels reference %d phvg %d",
	               ref.iDrawOnly100, gpu.iDrawOnly100, ref.iDrawRead100, gpu.iDrawRead100);
	RDebug::Printf("PH: vg2 time: %d.%02d ms/frame (reference, draw+read) vs %d.%02d ms/frame (phvg, draw+read)",
	               ref.iDrawRead100 / 100, ref.iDrawRead100 % 100, gpu.iDrawRead100 / 100, gpu.iDrawRead100 % 100);

	RDebug::Printf("PH: vg2 time (O3, ms per frame x100): scene draw+eglCopyBuffers into a CFbsBitmap: reference %d phvg %d; bitmap checksum reference %08x phvg %08x",
	               ref.iBitmap100, gpu.iBitmap100, ref.iBmpSum, gpu.iBmpSum);
	if (ref.iBitmap100 > 0 && gpu.iBitmap100 > 0)
		RDebug::Printf("PH: vg2 end-to-end (UI path): %d.%02d ms/frame reference vs %d.%02d ms/frame phvg = %d.%02dx",
		               ref.iBitmap100 / 100, ref.iBitmap100 % 100, gpu.iBitmap100 / 100, gpu.iBitmap100 % 100,
		               ref.iBitmap100 / gpu.iBitmap100, (ref.iBitmap100 * 100 / gpu.iBitmap100) % 100);

	// gross = a geometry / state bug (an antialiasing difference is at most ~40% coverage); 1% of the pixels allowed
	const TInt budget = KW * KH / 100;
	const TBool pass = engaged && gross <= budget && mGross <= budget && o4Pass;
	RDebug::Printf("PH: vg2 result: engine %s, gross scene %d mixed %d (budget %d) -> %s", engaged ? "ENGAGED" : "NOT ENGAGED",
	               gross, mGross, budget, pass ? "PASS" : "FAIL");
	eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(dpy, surf);
	eglDestroyContext(dpy, ctx);
	eglReleaseThread();
	return pass ? KErrNone : KErrGeneral;
	}
