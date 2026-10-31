// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// dfprvct2_2.cpp -- OURS (2026-10-31): the RVCT 2.2 floating-point runtime DLL that applications built with ARM's RVCT 2.2
// import (dfprvct2_2.dll, the release's kernel/eka/compsupp/rvct/dfprvct.mmp, which links ARM's proprietary fplib f_t_p.l /
// g_t_p.l; frozen ordinals in eabi/dfprvct2_2u.def). There is no source for those libraries; this file is written from the
// observable behaviour of the library: the register interface of each helper and the IEEE 754 result it computes.
//
// Why: a music player (Duomi) and other RVCT 2.2 applications import it; without it the loader refuses the executable.
//
// Register interface (RVCT "softvfp" helpers, AAPCS argument registers: a double is r0:r1 (low word in r0), the second
// operand r2:r3, a float r0 / r1, a long long r0:r1):
//   _dabs _fabs      absolute value (clear the sign bit)
//   _dneq _fneq      a != b as 0 / 1 in r0 (a NaN operand: 1)
//   _drdiv _frdiv    REVERSE divide: second operand divided by the first (r2:r3 / r0:r1)
//   _drem _frem      IEEE remainder x REM y (quotient rounded to nearest-even, so the result may be negative); x REM 0,
//                    inf REM y and NaN operands give a NaN, x REM inf is x
//   _drnd _frnd      round to an integral value, to nearest, ties to even (the default FP rounding mode)
//   _dsqrt _fsqrt    correctly rounded square root; negative operand: NaN
//   _dflt _dfltu     int / unsigned -> double      _fflt _ffltu   int / unsigned -> float (rounded to nearest)
//   _dfix_r _dfixu_r _ffix_r _ffixu_r   double / float -> int / unsigned, ROUNDING to nearest-even (_r = by the rounding mode);
//                    out of range saturates, NaN is 0, a negative value for the unsigned form is 0
//   _ll_sfrom_d _ll_ufrom_d _ll_sfrom_f _ll_ufrom_f     -> long long / unsigned long long, TRUNCATING (the C cast)
//   _ll_sfrom_d_r ... _r                same, ROUNDING to nearest-even
//   _ll_sto_d _ll_uto_d _ll_sto_f _ll_uto_f              long long / unsigned long long -> double / float (nearest)
//   __ARM_scalbn __ARM_scalbnf          x * 2^n (double r0:r1 + n in r2; float r0 + n in r1), n out of range clamps
//   __dcmp4 __fcmp4 _dcmp4 _fcmp4       compare (r0:r1 against r2:r3, r0 against r1), result in the CPSR FLAGS as a VFP
//                    FCMP delivers them: less N, equal Z+C, greater C, unordered (a NaN) C+V; r0-r3 are preserved
//   _dcmpge _fcmpge                     the same flags, but unordered gives N (the callers branch GE/GT: a NaN never passes)
// The float forms widen to double (exact) where a precise operation is needed; every result is exactly what a machine
// with IEEE round-to-nearest hardware produces (the NaN payload is the platform default NaN, 0x7FF80000 00000000).
//
// The floating-point operations the compiler emits for the plain C++ below are the soft-float helpers of libgcc (this
// image does not enable VFP for compsupp), so the DLL needs no coprocessor. The exact operations (remainder, sqrt, rounding,
// the float->integer conversions) are done on the IEEE bit patterns with integer arithmetic.
#include <e32std.h>

typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long s64;

namespace
	{
	union D { double d; u64 u; };
	union F { float f; u32 u; };

	inline u64 DBits(double x) { D v; v.d = x; return v.u; }
	inline double DFrom(u64 b) { D v; v.u = b; return v.d; }
	inline u32 FBits(float x) { F v; v.f = x; return v.u; }
	inline float FFrom(u32 b) { F v; v.u = b; return v.f; }

	const u64 KDefaultNaN = 0x7FF8000000000000ULL;
	const u64 KSignD = 0x8000000000000000ULL;

	inline bool IsNaNBits(u64 b) { return ((b << 1) > 0xFFE0000000000000ULL); }

	// |value| as an unsigned integer: truncated (aRound=0) or rounded to nearest-even (aRound=1).
	// aFlag: 0 ok, 1 NaN, 2 magnitude does not fit 64 bits (infinity included)
	u64 MagOfDouble(u64 b, int aRound, int& aNeg, int& aFlag)
		{
		aNeg = (int)(b >> 63);
		aFlag = 0;
		int e = (int)((b >> 52) & 0x7FF);
		u64 m = b & 0x000FFFFFFFFFFFFFULL;
		if (e == 0x7FF)
			{
			aFlag = m ? 1 : 2;
			return 0;
			}
		if (e == 0)
			return 0;				// zero or denormal: |x| < 1, and < 1/2 for the rounding form
		m |= 1ULL << 52;
		int sh = e - 1075;			// value = m * 2^sh
		if (sh >= 0)
			{
			if (sh >= 12)
				{
				aFlag = 2;
				return 0;
				}
			return m << sh;
			}
		int r = -sh;
		if (r >= 64)
			return 0;
		u64 q = m >> r;
		if (aRound)
			{
			u64 rem = m & ((1ULL << r) - 1);
			u64 half = 1ULL << (r - 1);
			if (rem > half || (rem == half && (q & 1)))
				++q;
			}
		return q;
		}

	// double bits -> integer of a given width with saturation. aBits 32/64, aSigned
	s64 SatSigned(u64 mag, int neg, int flag, int bits)
		{
		if (flag == 1)
			return 0;
		u64 lim = (bits == 32) ? 0x80000000ULL : 0x8000000000000000ULL;	// magnitude of the minimum
		if (flag == 2)
			mag = lim;
		if (neg)
			{
			if (mag >= lim)
				return (bits == 32) ? (s64)(int)0x80000000 : (s64)0x8000000000000000ULL;
			return -(s64)mag;
			}
		if (mag >= lim)
			return (bits == 32) ? (s64)0x7FFFFFFF : (s64)0x7FFFFFFFFFFFFFFFULL;
		return (s64)mag;
		}

	u64 SatUnsigned(u64 mag, int neg, int flag, int bits)
		{
		if (flag == 1)
			return 0;
		if (neg && !(flag == 0 && mag == 0))
			return 0;				// negative values (and -inf) have no unsigned form
		if (flag == 2)
			return (bits == 32) ? 0xFFFFFFFFULL : 0xFFFFFFFFFFFFFFFFULL;
		if (bits == 32 && mag > 0xFFFFFFFFULL)
			return 0xFFFFFFFFULL;
		return mag;
		}

	// a finite double as value = m * 2^e with bit 52 of m set (denormals normalised: e goes below the exponent range)
	void Unpack(u64 b, u64& m, int& e)
		{
		int ex = (int)((b >> 52) & 0x7FF);
		m = b & 0x000FFFFFFFFFFFFFULL;
		if (ex == 0)
			{
			ex = 1;
			while (!(m & (1ULL << 52)))
				{
				m <<= 1;
				--ex;
				}
			}
		else
			m |= 1ULL << 52;
		e = ex - 1075;
		}

	// r * 2^e (r < 2^54, the result exactly representable) with the sign, as double bits
	u64 PackExact(u64 sign, u64 r, int e)
		{
		if (r == 0)
			return sign;
		int p = 63;
		while (!(r & (1ULL << p)))
			--p;
		int E = p + e + 1023;		// biased exponent of the leading bit
		if (E >= 1)
			{
			u64 mant = (p <= 52) ? (r << (52 - p)) : (r >> (p - 52));
			return sign | ((u64)E << 52) | (mant & 0x000FFFFFFFFFFFFFULL);
			}
		// denormal: the quantum is 2^-1074
		int sh = -1074 - e;			// r * 2^e = (r >> sh) * 2^-1074
		if (sh < 0)
			return sign | (r << -sh);
		return sign | (sh >= 64 ? 0 : (r >> sh));
		}

	u64 DRemBits(u64 xb, u64 yb)
		{
		u64 sx = xb & KSignD;
		u64 ax = xb & ~KSignD, ay = yb & ~KSignD;
		if (IsNaNBits(xb) || IsNaNBits(yb))
			return IsNaNBits(xb) ? (xb | 0x0008000000000000ULL) : (yb | 0x0008000000000000ULL);
		if (ax == 0x7FF0000000000000ULL || ay == 0)
			return KDefaultNaN;		// inf REM y, x REM 0: invalid
		if (ay == 0x7FF0000000000000ULL || ax == 0)
			return xb;
		u64 mx, my;
		int ex, ey;
		Unpack(ax, mx, ex);
		Unpack(ay, my, ey);
		u64 r;
		int ev;
		int neg = 0;
		if (ex < ey - 1)
			return xb;				// |x| < |y| / 2
		if (ex == ey - 1)
			{
			// units of 2^ex: x = mx, y = 2*my; the quotient is 0 (even)
			u64 Y = my << 1;
			r = mx;
			ev = ex;
			if (2 * r > Y)
				{
				r = Y - r;
				neg = 1;
				}
			}
		else
			{
			int n = ex - ey;
			r = mx;
			ev = ey;
			int q = 0;
			for (int i = 0; i <= n; ++i)
				{
				q = (r >= my);
				if (q)
					r -= my;
				if (i < n)
					r <<= 1;
				}
			if (2 * r > my || (2 * r == my && q))
				{
				r = my - r;
				neg = 1;
				}
			}
		u64 s = neg ? (sx ^ KSignD) : sx;
		return PackExact(s, r, ev);
		}

	u64 DSqrtBits(u64 b)
		{
		if (IsNaNBits(b))
			return b | 0x0008000000000000ULL;
		u64 a = b & ~KSignD;
		if (a == 0)
			return b;				// +-0
		if (b & KSignD)
			return KDefaultNaN;
		if (a == 0x7FF0000000000000ULL)
			return b;
		u64 m;
		int e;
		Unpack(a, m, e);
		int s = (e & 1) ? 5 : 4;	// X = m << s, (e - s) even, 2^56 <= X < 2^58
		u64 X = m << s;
		int ee = (e - s) / 2;
		// digit-by-digit square root: 29 pairs of X, then 27 pairs of zeros -> a 56-bit root
		u64 rem = 0, q = 0;
		for (int i = 0; i < 56; ++i)
			{
			u64 two = (i < 29) ? ((X >> (2 * (28 - i))) & 3) : 0;
			rem = (rem << 2) | two;
			u64 t = (q << 2) | 1;
			if (rem >= t)
				{
				rem -= t;
				q = (q << 1) | 1;
				}
			else
				q <<= 1;
			}
		// q in [2^55, 2^56): value = q * 2^(ee - 27); keep 53 bits, round to nearest even
		u64 Q = q >> 3;
		u64 low = q & 7;
		bool sticky = (rem != 0) || (low & 3);
		bool guard = (low & 4) != 0;
		if (guard && (sticky || (Q & 1)))
			++Q;
		int exp2 = ee - 27 + 3;		// value = Q * 2^exp2
		if (Q == (1ULL << 53))
			{
			Q >>= 1;
			++exp2;
			}
		return ((u64)(exp2 + 52 + 1023) << 52) | (Q & 0x000FFFFFFFFFFFFFULL);
		}

	u64 DRndBits(u64 b)
		{
		u64 sign = b & KSignD;
		int e = (int)((b >> 52) & 0x7FF);
		if (e == 0x7FF)
			return IsNaNBits(b) ? (b | 0x0008000000000000ULL) : b;
		if (e >= 1075)
			return b;
		if (e < 1022)
			return sign;
		if (e == 1022)
			return ((b & 0x000FFFFFFFFFFFFFULL) == 0) ? sign : (sign | 0x3FF0000000000000ULL);
		int s = 1075 - e;			// 1..52: the low s bits are fractional
		u64 m = b & ~KSignD;
		u64 lowmask = (1ULL << s) - 1;
		u64 half = 1ULL << (s - 1);
		u64 rem = m & lowmask;
		m &= ~lowmask;
		if (rem > half || (rem == half && ((m >> s) & 1)))
			m += 1ULL << s;
		return sign | m;
		}

	double Pow2(int k)				// 2^k, k in the normal range
		{
		return DFrom((u64)(k + 1023) << 52);
		}

	float FPow2(int k)
		{
		return FFrom((u32)(k + 127) << 23);
		}

	// x * 2^n by exact steps (a denormal result is rounded once, in the last multiplication)
	double ScalbnD(double y, int n)
		{
		if (n > 1023)
			{
			y *= Pow2(1023);
			n -= 1023;
			if (n > 1023)
				{
				y *= Pow2(1023);
				n -= 1023;
				if (n > 1023)
					n = 1023;
				}
			}
		else if (n < -1022)
			{
			// 2^-1022 * 2^53 keeps the intermediate result normal
			y *= Pow2(-1022) * Pow2(53);
			n += 1022 - 53;
			if (n < -1022)
				{
				y *= Pow2(-1022) * Pow2(53);
				n += 1022 - 53;
				if (n < -1022)
					n = -1022;
				}
			}
		return y * Pow2(n);
		}

	float ScalbnF(float y, int n)
		{
		if (n > 127)
			{
			y *= FPow2(127);
			n -= 127;
			if (n > 127)
				{
				y *= FPow2(127);
				n -= 127;
				if (n > 127)
					n = 127;
				}
			}
		else if (n < -126)
			{
			y *= FPow2(-126) * FPow2(24);
			n += 126 - 24;
			if (n < -126)
				{
				y *= FPow2(-126) * FPow2(24);
				n += 126 - 24;
				if (n < -126)
					n = -126;
				}
			}
		return y * FPow2(n);
		}

	// the order of two doubles: 0 less, 1 equal, 2 greater, 3 unordered
	int OrderD(u64 a, u64 b)
		{
		if (IsNaNBits(a) || IsNaNBits(b))
			return 3;
		if (((a | b) << 1) == 0)
			return 1;				// +0 == -0
		bool na = (a >> 63) != 0, nb = (b >> 63) != 0;
		if (na != nb)
			return na ? 0 : 2;
		if (a == b)
			return 1;
		bool lessMag = (a & ~KSignD) < (b & ~KSignD);
		return (lessMag != na) ? 0 : 2;
		}

	int OrderF(u32 a, u32 b)
		{
		if (((a << 1) > 0xFF000000u) || ((b << 1) > 0xFF000000u))
			return 3;
		if (((a | b) << 1) == 0)
			return 1;
		bool na = (a >> 31) != 0, nb = (b >> 31) != 0;
		if (na != nb)
			return na ? 0 : 2;
		if (a == b)
			return 1;
		bool lessMag = (a & 0x7FFFFFFFu) < (b & 0x7FFFFFFFu);
		return (lessMag != na) ? 0 : 2;
		}

	double FloatToDouble(float f)
		{
		return (double)f;			// exact (libgcc __aeabi_f2d)
		}
	}

extern "C"
	{
	// ---- sign
	EXPORT_C double _dabs(double a) { return DFrom(DBits(a) & ~KSignD); }
	EXPORT_C float _fabs(float a) { return FFrom(FBits(a) & 0x7FFFFFFFu); }
	EXPORT_C int _dneq(double a, double b) { return OrderD(DBits(a), DBits(b)) != 1; }
	EXPORT_C int _fneq(float a, float b) { return OrderF(FBits(a), FBits(b)) != 1; }

	// ---- arithmetic
	EXPORT_C double _drdiv(double a, double b) { return b / a; }
	EXPORT_C float _frdiv(float a, float b) { return b / a; }
	EXPORT_C double _drem(double x, double y) { return DFrom(DRemBits(DBits(x), DBits(y))); }
	EXPORT_C float _frem(float x, float y) { return (float)DFrom(DRemBits(DBits(FloatToDouble(x)), DBits(FloatToDouble(y)))); }
	EXPORT_C double _drnd(double x) { return DFrom(DRndBits(DBits(x))); }
	EXPORT_C float _frnd(float x) { return (float)DFrom(DRndBits(DBits(FloatToDouble(x)))); }
	EXPORT_C double _dsqrt(double x) { return DFrom(DSqrtBits(DBits(x))); }
	EXPORT_C float _fsqrt(float x) { return (float)DFrom(DSqrtBits(DBits(FloatToDouble(x)))); }
	EXPORT_C double __ARM_scalbn(double x, int n) { return ScalbnD(x, n); }
	EXPORT_C float __ARM_scalbnf(float x, int n) { return ScalbnF(x, n); }

	// ---- integer -> floating point
	EXPORT_C double _dflt(int a) { return (double)a; }
	EXPORT_C double _dfltu(unsigned a) { return (double)a; }
	EXPORT_C float _fflt(int a) { return (float)a; }
	EXPORT_C float _ffltu(unsigned a) { return (float)a; }
	EXPORT_C double _ll_sto_d(long long a) { return (double)a; }
	EXPORT_C double _ll_uto_d(unsigned long long a) { return (double)a; }
	EXPORT_C float _ll_sto_f(long long a) { return (float)a; }
	EXPORT_C float _ll_uto_f(unsigned long long a) { return (float)a; }

	// ---- floating point -> integer
#define DRT_TO_INT(name, argt, bitsfn, round, sat, rett, nbits) \
	EXPORT_C rett name(argt a) \
		{ \
		int neg, flag; \
		u64 mag = MagOfDouble(bitsfn(a), round, neg, flag); \
		return (rett)sat(mag, neg, flag, nbits); \
		}
	static inline u64 DBitsOfFloat(float f) { return DBits(FloatToDouble(f)); }
	DRT_TO_INT(_dfix_r, double, DBits, 1, SatSigned, int, 32)
	DRT_TO_INT(_dfixu_r, double, DBits, 1, SatUnsigned, unsigned, 32)
	DRT_TO_INT(_ffix_r, float, DBitsOfFloat, 1, SatSigned, int, 32)
	DRT_TO_INT(_ffixu_r, float, DBitsOfFloat, 1, SatUnsigned, unsigned, 32)
	DRT_TO_INT(_ll_sfrom_d, double, DBits, 0, SatSigned, long long, 64)
	DRT_TO_INT(_ll_ufrom_d, double, DBits, 0, SatUnsigned, unsigned long long, 64)
	DRT_TO_INT(_ll_sfrom_f, float, DBitsOfFloat, 0, SatSigned, long long, 64)
	DRT_TO_INT(_ll_ufrom_f, float, DBitsOfFloat, 0, SatUnsigned, unsigned long long, 64)
	DRT_TO_INT(_ll_sfrom_d_r, double, DBits, 1, SatSigned, long long, 64)
	DRT_TO_INT(_ll_ufrom_d_r, double, DBits, 1, SatUnsigned, unsigned long long, 64)
	DRT_TO_INT(_ll_sfrom_f_r, float, DBitsOfFloat, 1, SatSigned, long long, 64)
	DRT_TO_INT(_ll_ufrom_f_r, float, DBitsOfFloat, 1, SatUnsigned, unsigned long long, 64)

	// ---- the orderings the flag-returning entries wrap (0 less, 1 equal, 2 greater, 3 unordered)
	int DfpOrderD(u32 alo, u32 ahi, u32 blo, u32 bhi)
		{
		return OrderD(((u64)ahi << 32) | alo, ((u64)bhi << 32) | blo);
		}

	int DfpOrderF(u32 a, u32 b)
		{
		return OrderF(a, b);
		}
	}

// The flag-returning comparisons (ARM state; they return with bx lr to either state). Every register is preserved (r0-r3 and
// ip as well as the callee-saved ones). The order code (0 less, 1 equal, 2 greater, 3 unordered) becomes the flags: less N,
// equal Z+C, greater C, unordered C+V (the _cmp4 forms) or N (the _cmpge forms, whose callers branch GE/GT). The flags are
// written just before the registers are popped, and a pop leaves the flags alone.
#define DFP_FLAGS_HEAD(name) \
	".global " #name "\n.type " #name ", %function\n" #name ":\n" \
	"	push {r0-r3, ip, lr}\n"
#define DFP_FLAGS_TAIL(unordered) \
	"	cmp r0, #1\n" \
	"	movlo r1, #0x80000000\n" \
	"	moveq r1, #0x60000000\n" \
	"	cmp r0, #2\n" \
	"	moveq r1, #0x20000000\n" \
	"	cmp r0, #3\n" \
	"	moveq r1, #" unordered "\n" \
	"	msr cpsr_f, r1\n" \
	"	pop {r0-r3, ip, lr}\n" \
	"	bx lr\n" \
	"	.ltorg\n"
#define DFP_CMP_D(name, unordered) \
	DFP_FLAGS_HEAD(name) \
	"	ldr ip, =DfpOrderD\n" \
	"	blx ip\n" \
	DFP_FLAGS_TAIL(unordered)
#define DFP_CMP_F(name, unordered) \
	DFP_FLAGS_HEAD(name) \
	"	ldr ip, =DfpOrderF\n" \
	"	blx ip\n" \
	DFP_FLAGS_TAIL(unordered)

asm(
	".text\n"
	".syntax unified\n"
	".arm\n"
	".balign 4\n"
	DFP_CMP_D(__dcmp4, "0x30000000")
	DFP_CMP_D(_dcmp4, "0x30000000")
	DFP_CMP_D(_dcmpge, "0x80000000")
	DFP_CMP_F(__fcmp4, "0x30000000")
	DFP_CMP_F(_fcmp4, "0x30000000")
	DFP_CMP_F(_fcmpge, "0x80000000")
	);
