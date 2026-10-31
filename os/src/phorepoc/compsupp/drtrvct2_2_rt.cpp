// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// drtrvct2_2_rt.cpp -- OURS (2026-10-31): the RVCT-internal part of drtrvct2_2.dll (the 35 exports of drtrvct2_2u.def that
// ARM's proprietary C library supplied): integer division helpers, memory helpers, signal raising, library init and the
// binary-to-decimal extended-precision helpers. Written from the observable behaviour of the library (register interface and
// result of each helper; checked differentially, bit for bit, against the library in an ARM emulator: drtrvct2_2_test.py).
//
// ---- Integer division (RVCT's non-AAPCS convention: divisor in r0, dividend in r1; quotient r0, remainder r1)
//   __rt_udiv __rt_sdiv       32-bit unsigned / signed; truncating division, the remainder takes the dividend's sign
//   __rt_udiv10 __rt_sdiv10   the same for the divisor 10 (dividend in r0)
//   __rt_udiv32by16 __rt_sdiv32by16   dividend r0 (32 bit), divisor the low 16 bits of r1, quotient (16 bit) r0, remainder r1;
//                             the quotient must fit 16 bits (the helper is a 16-step restoring division: a larger quotient wraps)
//   __rt_sdiv64by32           signed 64-bit dividend r0:r1 (r0 = high word) by the 32-bit divisor r2; quotient r1, remainder r0
//   __rt_divtest              r0 == 0 -> __rt_div0, else returns
//   Division by zero in any of them is a tail jump to __rt_div0.
// ---- 64-bit division, "reverse" convention: divisor r0:r1, dividend r2:r3; quotient r0:r1, remainder r2:r3
//   _ll_urdv _ll_srdv         unsigned / signed (signed: quotient truncates, remainder takes the dividend's sign)
//   _ll_udiv10                dividend r0:r1 / 10: quotient r0:r1, remainder r2 (r3 = 0)
//   _ll_udiv_small _ll_udiv_big _ll_udiv_ginormous   the three divisor-size cases of the library's internal unsigned 64-bit
//                             divide; they are entered from inside it with its state: dividend r4:r5 (r4 high); for _small
//                             r6 = s and ip = divisor << s (divisor < 2^31, s = clz - 1, so ip has bit 30 set), for _big
//                             ip:lr = divisor << r6 (2^32 <= divisor < 2^63, bit 62 of the pair set), for _ginormous
//                             ip:lr = divisor itself (>= 2^63). Result in r0:r1 (quotient), r2:r3 (remainder). They finish
//                             with the caller's frame: pop {r4-r6, lr}; return to lr.
// ---- Memory
//   __rt_memcpy r0 dst, r1 src, r2 n (forward copy); _memcpy_lastbytes: copies r2 & 3 bytes; __memmove_aligned: r0 / r1 are
//   the END pointers of the (word-aligned) destination and source, r2 the byte count, copied backwards (overlap-safe when
//   dst is above src); __rt_memset r0 dst, r1 byte, r2 n; _memset r0 dst, r1 n, r2 fill word (bytes: its low byte, words:
//   the whole word, so the caller replicates the byte); _memset_w the same for a word-aligned dst.
// ---- Signals / init
//   __rt_raise(signal, type): the RVCT signal number -> EPOC exception (User::RaiseException), returns the signal;
//   __rt_lib_init: _fp_init(); __rt_lib_shutdown: nothing.
// ---- Extended-precision decimal conversion helpers ("e" values: r0 = sign (bit 31) | exponent word (bias 0x3FFF, bit 30
//   = infinity / NaN flag), r1:r2 = 64-bit mantissa with the explicit leading one in bit 63 of r1:r2; a zero has r1 bit 31
//   clear), used by printf / strtod:
//   _btod_d2e        double (r0 = high word, r1 = low word) -> e value in r0:r1:r2 (a denormal is normalised)
//   _d2e_norm_op1    the denormal-normalising tail of _btod_d2e (state r0:r1:r2 as the head leaves it)
//   _d2e_denorm_low  its sub-case when the top 31 mantissa bits are all zero
//   _e2e             round an unrounded result to the e format: r0 sign, r1:r2 mantissa, r3 exponent, r6 guard word
//                    (bit 31 the guard bit, the other bits the sticky), r11 = rounding mode (-1 toward zero, 0 to nearest
//                    even, > 0 away from zero); r3 < 0 denormalises first; result r0 = sign | exponent, r1:r2
//   _e2d             the same, to double (r0 = high word, r1 = low word; the sign is not carried), exponent overflow -> +inf
//   _btod_ediv _btod_emul    r0 -> a, r1 -> b (three-word e values in memory), r2 = rounding mode: a / b, a * b rounded
//                    to an e value in r0:r1:r2; if an operand is zero / infinity / NaN (ediv) or flagged (emul) the first
//                    operand is returned unchanged
//   _btod_edivd _btod_emuld  the same, rounded to a double in r0:r1
//   __btod_div_common __btod_mult_common   the unrounded cores (a = r0:r1:r2, b = r3:r4:r5 -> sign r0, exponent r3, mantissa
//                    r1:r2 and guard word r6)
//   _btod_etento     r0 -> result (three words), r1 = n, r2 = rounding mode: 10^n, from the tables of 10^(2^k) and 10^(55*2^k)
// The inexact-status side effect of the library's rounding (it sets the VFP FPSCR inexact flag) is not reproduced: this
// image does not enable the VFP for compsupp, so the DLL needs no coprocessor.
#include <e32std.h>

typedef unsigned int u32;
typedef unsigned long long u64;
typedef int s32;

extern "C" void __rt_div0();
extern "C" void _fp_init();

// the register file of the ARM-state wrappers below: r0..r12 and lr, passed to a core as one structure (inputs on entry,
// results written back; the wrapper reloads every register from it)
struct Regs { u32 r[14]; };
enum { R_LR = 13 };

// ------------------------------------------------------------------------------------------------------------------
// signals, init
extern "C" {
EXPORT_C int __rt_raise(int aSignal, int aType)
	{
	// RVCT's <signal.h>: SIGABRT 1, SIGFPE 2, SIGILL 3, SIGINT 4, SIGSEGV 5, SIGTERM 6; FPE type DIVBYZERO 2
	TExcType exc = EExcGeneral;
	switch (aSignal)
		{
	case 1: exc = EExcAbort; break;
	case 2: exc = (aType == 2) ? EExcAbort : EExcFloatInvalidOperation; break;
	case 3: exc = EExcCodeAbort; break;
	case 4: exc = EExcUserInterrupt; break;
	case 5: exc = EExcDataAbort; break;
	case 6: exc = EExcKill; break;
		}
	User::RaiseException(exc);
	return aSignal;
	}

EXPORT_C void __rt_lib_init() { _fp_init(); }
EXPORT_C void __rt_lib_shutdown() { }
}


// ------------------------------------------------------------------------------------------------------------------
// the 16-step restoring divisions of the library, on one 32-bit register that holds the remainder (high bits) and the
// quotient (low bits, shifted in through the carry). Return quotient in r0, remainder in r1 (AAPCS u64: low, high).
// A divisor whose low 16 bits (32by16: its negation's low 17 bits) are zero is a division by zero.
extern "C" __attribute__((used)) u64 drt_udiv32by16(u32 n, u32 d)
	{
	u32 b = (u32)(0u - d) << 15;
	if (b == 0) { __rt_div0(); return 0; }
	u32 a = n;
	u64 t = (u64)b + a;
	u32 c = (u32)(t >> 32);
	a = (u32)t;
	if (!c) a -= b;
	for (int i = 0; i < 15; i++)
		{
		t = (u64)b + (u32)(a << 1) + c;
		c = (u32)(t >> 32);
		a = (u32)t;
		if (!c) a -= b;
		}
	a = a + a + c;
	return (u64)(a & 0xffff) | ((u64)(a >> 16) << 32);
	}

extern "C" __attribute__((used)) u64 drt_sdiv32by16(u32 n, u32 d)
	{
	u32 b = d << 16;
	if (b == 0) { __rt_div0(); return 0; }
	u32 sgn = (b >> 1) ^ (u32)((s32)n >> 1);	// bit 31: n negative, bit 30: the signs differ
	if ((s32)b >= 0) b = 0u - b;
	u32 a = n;
	if (sgn & 0x80000000u) a = 0u - a;
	u64 t = (u64)b + (u32)(a << 1);
	u32 c = (u32)(t >> 32);
	a = (u32)t;
	if (!c) a -= b;
	for (int i = 0; i < 15; i++)
		{
		t = (u64)b + (u32)(a << 1) + c;
		c = (u32)(t >> 32);
		a = (u32)t;
		if (!c) a -= b;
		}
	u32 r = a >> 16;			// the remainder is taken before the last quotient bit is shifted in
	u32 q = ((a & 0xffff) << 1) + c;
	if (sgn & 0x40000000u) q = 0u - q;
	if (sgn & 0x80000000u) r = 0u - r;
	return (u64)q | ((u64)r << 32);
	}

// signed 64-bit dividend hi:lo / 32-bit d, 32-step restoring division; the u64 is remainder (low word = r0) and quotient
// (high word = r1), the helper's own result registers
extern "C" __attribute__((used)) u64 drt_sdiv64by32(u32 hi, u32 lo, u32 d)
	{
	if (d == 0) { __rt_div0(); return 0; }
	u32 b = d;
	if ((s32)d >= 0) b = 0u - d;
	u32 sgn = (d >> 1) ^ (u32)((s32)hi >> 1);
	if (sgn & 0x80000000u)
		{
		lo = 0u - lo;
		hi = ~hi + (lo == 0 ? 1u : 0u);
		}
	u32 c = lo >> 31;
	lo <<= 1;
	for (int i = 0; i < 32; i++)
		{
		u64 t = (u64)b + (u32)(hi << 1) + c;
		c = (u32)(t >> 32);
		hi = (u32)t;
		if (!c) hi -= b;
		u32 nc = lo >> 31;
		lo = (lo << 1) + c;
		c = nc;
		}
	if (sgn & 0x40000000u) lo = 0u - lo;
	if (sgn & 0x80000000u) hi = 0u - hi;
	return (u64)hi | ((u64)lo << 32);
	}

// ------------------------------------------------------------------------------------------------------------------
// 64-bit division cores (n, d unsigned; d != 0)
static void Udiv64(u64 n, u64 d, u64& q, u64& r)
	{
	u64 qq = 0, rr = n;
	if (n >= d)
		{
		int sh = 0;
		while (!(d >> 63) && (d << 1) <= n) { d <<= 1; sh++; }
		for (;;)
			{
			qq <<= 1;
			if (rr >= d) { rr -= d; qq |= 1; }
			if (!sh) break;
			d >>= 1;
			sh--;
			}
		}
	q = qq;
	r = rr;
	}

static inline u64 Join(u32 lo, u32 hi) { return ((u64)hi << 32) | lo; }
static inline void Split(u64 v, u32& lo, u32& hi) { lo = (u32)v; hi = (u32)(v >> 32); }

// _ll_urdv: divisor r0:r1, dividend r2:r3 -> quotient r0:r1, remainder r2:r3
extern "C" __attribute__((used)) void drt_ll_urdv(Regs* s)
	{
	u64 q, r;
	Udiv64(Join(s->r[2], s->r[3]), Join(s->r[0], s->r[1]), q, r);
	Split(q, s->r[0], s->r[1]);
	Split(r, s->r[2], s->r[3]);
	}

// _ll_srdv: signed; the quotient is negative when the signs differ, the remainder has the dividend's sign
extern "C" __attribute__((used)) void drt_ll_srdv(Regs* s)
	{
	long long dv = (long long)Join(s->r[0], s->r[1]);
	long long nd = (long long)Join(s->r[2], s->r[3]);
	u64 ud = dv < 0 ? 0ull - (u64)dv : (u64)dv;
	u64 un = nd < 0 ? 0ull - (u64)nd : (u64)nd;
	u64 q, r;
	Udiv64(un, ud, q, r);
	if ((dv < 0) != (nd < 0)) q = 0ull - q;
	if (nd < 0) r = 0ull - r;
	Split(q, s->r[0], s->r[1]);
	Split(r, s->r[2], s->r[3]);
	}

// _ll_udiv10: r0:r1 / 10 -> quotient r0:r1, remainder r2, r3 = 0
extern "C" __attribute__((used)) void drt_ll_udiv10(Regs* s)
	{
	u32 hi = s->r[1], lo = s->r[0];
	u32 qh = hi / 10, rh = hi % 10;
	u64 mid = ((u64)rh << 32) | lo;		// < 10 * 2^32
	u32 ql = (u32)(mid / 10), rl = (u32)(mid % 10);
	s->r[0] = ql; s->r[1] = qh; s->r[2] = rl; s->r[3] = 0;
	}

// _ll_udiv_small / _big / _ginormous: the library's internal states, see the header. r4:r5 = dividend (r4 high)
extern "C" __attribute__((used)) void drt_ll_udiv_small(Regs* s)
	{
	u64 q, r;
	u32 d = s->r[12] >> s->r[6];		// ip >> s
	Udiv64(Join(s->r[5], s->r[4]), d, q, r);
	Split(q, s->r[0], s->r[1]);
	s->r[2] = (u32)r;
	s->r[3] = 0;
	}

extern "C" __attribute__((used)) void drt_ll_udiv_big(Regs* s)
	{
	u64 q, r;
	u64 d = Join(s->r[R_LR], s->r[12]) >> s->r[6];		// ip:lr >> r6
	Udiv64(Join(s->r[5], s->r[4]), d, q, r);
	Split(q, s->r[0], s->r[1]);
	Split(r, s->r[2], s->r[3]);
	}

extern "C" __attribute__((used)) void drt_ll_udiv_ginormous(Regs* s)
	{
	u64 n = Join(s->r[5], s->r[4]);
	u64 d = Join(s->r[R_LR], s->r[12]);
	u32 q = 0;
	if (n >= d) { n -= d; q = 1; }
	s->r[0] = q; s->r[1] = 0;
	Split(n, s->r[2], s->r[3]);
	}

// ------------------------------------------------------------------------------------------------------------------
// extended-precision ("e") helpers
static inline u32 Fold(u32 x) { return ((x >> 16) | x) & 0xffff; }	// the library's sticky fold of a guard word

// shift count as the library computes it (a binary search: 31 for zero, like clz for non-zero values)
static u32 NormShift(u32 x)
	{
	u32 n = 0;
	if ((x >> 16) == 0) { n += 16; x <<= 16; }
	if ((x >> 24) == 0) { n += 8; x <<= 8; }
	if ((x >> 28) == 0) { n += 4; x <<= 4; }
	if ((x >> 30) == 0) { n += 2; x <<= 2; }
	if ((x >> 31) == 0) { n += 1; }
	return n;
	}

struct EVal { u32 e, hi, lo; };

// _d2e_denorm_low: the top 31 mantissa bits are zero; the mantissa is in the low word
static void D2eDenormLow(EVal& v)
	{
	u32 n = NormShift(v.lo);
	v.hi = v.lo << n;
	v.lo = 0;
	v.e = v.e - 0x1f - n;
	}

// _d2e_norm_op1: bit 31 of v.hi is the "non-zero" marker of the head; a clear marker is a zero (unchanged)
static void D2eNormOp1(EVal& v)
	{
	if (!(v.hi & 0x80000000u)) return;
	v.hi &= 0x7fffffffu;
	if (v.hi == 0) { D2eDenormLow(v); return; }
	u32 n = NormShift(v.hi);			// n >= 1: bit 31 was cleared
	v.hi = (v.hi << n) | (v.lo >> (32 - n));
	v.lo <<= n;
	v.e = v.e - n + 1;
	}

// _btod_d2e
static void D2e(u32 hiw, u32 low, EVal& v)
	{
	u32 ip = hiw << 1;
	bool nz = (ip != 0) || (low != 0);
	u32 t = ip >> 20;
	if (nz) t += 0x7800;
	v.e = (hiw & 0x80000000u) | (t >> 1);
	v.lo = low << 11;
	v.hi = (ip << 10) | (low >> 21);
	if (nz) v.hi |= 0x80000000u;
	s32 ex = (s32)ip >> 21;
	if (ex == 0) { D2eNormOp1(v); return; }
	if (ex == -1) v.e |= 0x40000000u;
	}

// _e2e: round r1:r2 (+ the guard word r6) with the exponent r3; r0 = sign | exponent
struct ERound { u32 r0, r1, r2, r3, r6; bool inexact; };
static void E2e(ERound& x, s32 mode)
	{
	u32 r1 = x.r1, r2 = x.r2, r3 = x.r3, r6 = x.r6;
	if ((s32)r3 < 0)
		{
		r6 = Fold(r6);
		if ((s32)r3 <= -0x40)
			{
			r6 |= r2;
			r6 = Fold(r6);
			r2 = 0;
			r6 |= r1;
			if ((s32)r3 < -0x40) r6 = Fold(r6);
			r1 = 0;
			r3 = 0;
			}
		else
			{
			if ((s32)r3 <= -0x20) { r6 |= r2; r2 = r1; r1 = 0; r3 += 0x20; }
			r3 = 0u - r3;
			if (r3)
				{
				r6 = Fold(r6);
				u32 sh = r3;
				r6 |= r2 << (32 - sh);
				r2 = (r2 >> sh) | (r1 << (32 - sh));
				r1 >>= sh;
				r3 = 0;
				}
			}
		}
	bool guard = (r6 >> 31) != 0;
	bool rest = (r6 << 1) != 0;
	bool inexact = true;
	u32 c = guard;
	if (!rest)
		{
		if (!guard) inexact = false;		// exact
		else c = r2 & 1;			// a tie: the least significant bit decides (to nearest)
		}
	if (inexact)
		{
		bool up = (mode == 0) ? (c != 0) : (mode != -1);
		if (up)
			{
			r2++;
			if (r2 == 0)
				{
				r1++;
				if (r1 == 0) { r1 = 0x80000000u; r3++; }
				}
			}
		}
	x.r0 = (x.r0 & 0x80000000u) | r3;
	x.r1 = r1;
	x.r2 = r2;
	x.r3 = r3;
	x.r6 = r6;
	x.inexact = inexact;
	}

// _e2d: to double (r0 high word, r1 low word); the sign is dropped by the library, an exponent overflow gives +infinity
static void E2d(ERound& x, s32 mode)
	{
	u32 r6 = Fold(x.r6);
	r6 |= x.r2 << 21;
	u32 r2 = (x.r2 >> 11) | (x.r1 << 21);
	x.r3 = x.r3 - 0x3c00 - 1;
	x.r1 >>= 11;
	x.r2 = r2;
	x.r6 = r6;
	E2e(x, mode);
	u32 e = x.r0 & 0x7fffffffu;
	if (e + 1 < 0x800)
		{
		x.r0 = x.r1 + (e << 20);
		x.r1 = x.r2;
		}
	else
		{
		x.r0 = 0x7ff00000u;
		x.r1 = 0;
		}
	}

// the unrounded cores
struct ECore { u32 r0, r1, r2, r3, r6; };	// sign word, mantissa r1:r2, exponent r3, guard word r6

static void MultCommon(u32 ae, u32 ahi, u32 alo, u32 be, u32 bhi, u32 blo, ECore& o)
	{
	u32 r3 = (ae & 0xffffff) + (be & 0xffffff) - 0x3ffe;
	u32 r0 = (ae ^ be) & 0x80000000u;
	u64 p00 = (u64)alo * blo, p01 = (u64)alo * bhi, p10 = (u64)ahi * blo, p11 = (u64)ahi * bhi;
	u64 mid = (p00 >> 32) + (u32)p01 + (u32)p10;
	u32 w0 = (u32)p00, w1 = (u32)mid;
	u64 t2 = (p01 >> 32) + (p10 >> 32) + (u32)p11 + (mid >> 32);
	u32 w2 = (u32)t2;
	u32 w3 = (u32)(p11 >> 32) + (u32)(t2 >> 32);
	u32 r1 = w3, r2 = w2, r6 = w1 | (w0 ? 1u : 0u);
	if (!(r1 & 0x80000000u))
		{
		r1 = (r1 << 1) | (r2 >> 31);
		r2 = (r2 << 1) | (r6 >> 31);
		r6 <<= 1;
		r3--;
		}
	o.r0 = r0; o.r1 = r1; o.r2 = r2; o.r3 = r3; o.r6 = r6;
	}

static void DivCommon(u32 ae, u32 ahi, u32 alo, u32 be, u32 bhi, u32 blo, ECore& o)
	{
	u32 r3 = (ae & 0xffffff) - (be & 0xffffff) + 0x3fff;
	u32 r0 = (ae ^ be) & 0x80000000u;
	u64 A = Join(alo, ahi), B = Join(blo, bhi);
	// Q = floor(A * 2^95 / B) as three words, the lowest bit a sticky for a non-zero remainder
	u32 q[3] = { 0, 0, 0 };		// q[2] high
	u64 rem = A;
	if (rem >= B) { rem -= B; q[2] = 0x80000000u; }
	for (int i = 94; i >= 0; i--)
		{
		bool carry = (rem >> 63) != 0;
		rem <<= 1;
		if (carry || rem >= B) { rem -= B; q[i >> 5] |= 1u << (i & 31); }
		}
	if (rem) q[0] |= 1;
	u32 r1 = q[2], r2 = q[1], r6 = q[0];
	if (!(r1 & 0x80000000u))
		{
		r1 = (r1 << 1) | (r2 >> 31);
		r2 = (r2 << 1) | (r6 >> 31);
		r6 <<= 1;
		r3--;
		}
	o.r0 = r0; o.r1 = r1; o.r2 = r2; o.r3 = r3; o.r6 = r6;
	}

// the e-value operators: a (op) b; a special first operand (see the header) is returned unchanged
static void EDivMul(const u32* a, const u32* b, s32 mode, bool div, bool todouble, u32* out)
	{
	if (div)
		{
		bool aok = (a[1] & 0x80000000u) && !(a[0] & 0x40000000u);
		bool bok = (b[1] & 0x80000000u) && !(b[0] & 0x40000000u);
		if (!aok || !bok) { out[0] = a[0]; out[1] = a[1]; out[2] = a[2]; return; }
		}
	else if ((a[0] | b[0]) & 0x40000000u)
		{
		out[0] = a[0]; out[1] = a[1]; out[2] = a[2];
		return;
		}
	ECore c;
	if (div) DivCommon(a[0], a[1], a[2], b[0], b[1], b[2], c);
	else MultCommon(a[0], a[1], a[2], b[0], b[1], b[2], c);
	ERound x = { c.r0, c.r1, c.r2, c.r3, c.r6, false };
	if (todouble) E2d(x, mode); else E2e(x, mode);
	out[0] = x.r0; out[1] = x.r1; out[2] = x.r2;
	}

// 10^n: three-word entries for 10^1 .. 10^16 and four-word entries (the fourth a rounding hint) for 10^(55 * 2^k)
static const u32 KTen3[5][3] =
	{
	{ 0x4002, 0xa0000000u, 0 }, { 0x4005, 0xc8000000u, 0 }, { 0x400c, 0x9c400000u, 0 },
	{ 0x4019, 0xbebc2000u, 0 }, { 0x4034, 0x8e1bc9bfu, 0x04000000u }
	};
static const u32 KTen55[4][4] =
	{
	{ 0x40b5, 0xd0cf4b50u, 0xcfe20766u, 0x00000001u }, { 0x416c, 0xaa51823eu, 0x34a7eedfu, 0x00000001u },
	{ 0x42d9, 0xe2a0b5dcu, 0x971f303au, 0xffffffffu }, { 0x45b4, 0xc8a025fdu, 0x4fc1a3e9u, 0xffffffffu }
	};

static void ETenTo(s32 n, s32 mode, u32* out)
	{
	// n = 55 * hiq + m with |m| <= 27 (rounded division)
	s32 sum = n + 7067;
	s32 q = sum / 55, rem = sum % 55;
	if (rem < 0) { rem += 55; q--; }
	s32 hiq = q - 128, m = rem - 27;
	bool neg = m < 0;
	u32 am = neg ? (u32)(-m) : (u32)m;
	u32 acc1[3] = { 0x3fff, 0x80000000u, 0 }, acc2[3] = { 0x3fff, 0x80000000u, 0 };
	for (int i = 0; am && i < 5; i++, am >>= 1)
		if (am & 1) EDivMul(acc1, KTen3[i], mode, false, false, acc1);
	for (int i = 0; hiq > 0 && i < 4; i++, hiq >>= 1)
		if (hiq & 1)
			{
			u32 e[3] = { KTen55[i][0], KTen55[i][1], KTen55[i][2] };
			if (0u - KTen55[i][3] == (u32)mode) e[2] += (u32)mode;
			EDivMul(acc2, e, mode, false, false, acc2);
			}
	if (neg) EDivMul(acc2, acc1, mode, true, false, out);
	else EDivMul(acc2, acc1, mode, false, false, out);
	}

// ---- cores called from the assembly wrappers (Regs = r0..r12, lr)
extern "C" __attribute__((used)) void drt_btod_d2e(Regs* s)
	{
	EVal v;
	D2e(s->r[0], s->r[1], v);
	s->r[0] = v.e; s->r[1] = v.hi; s->r[2] = v.lo;
	}
extern "C" __attribute__((used)) void drt_d2e_norm_op1(Regs* s)
	{
	EVal v = { s->r[0], s->r[1], s->r[2] };
	D2eNormOp1(v);
	s->r[0] = v.e; s->r[1] = v.hi; s->r[2] = v.lo;
	}
extern "C" __attribute__((used)) void drt_d2e_denorm_low(Regs* s)
	{
	EVal v = { s->r[0], s->r[1], s->r[2] };
	D2eDenormLow(v);
	s->r[0] = v.e; s->r[1] = v.hi; s->r[2] = v.lo;
	}
extern "C" __attribute__((used)) void drt_e2e(Regs* s)
	{
	ERound x = { s->r[0], s->r[1], s->r[2], s->r[3], s->r[6], false };
	E2e(x, (s32)s->r[11]);
	s->r[0] = x.r0; s->r[1] = x.r1; s->r[2] = x.r2; s->r[3] = x.r3;
	if (x.inexact) s->r[11]++;		// the library leaves its rounding-mode register incremented on the inexact path
	}
extern "C" __attribute__((used)) void drt_e2d(Regs* s)
	{
	ERound x = { s->r[0], s->r[1], s->r[2], s->r[3], s->r[6], false };
	E2d(x, (s32)s->r[11]);
	s->r[0] = x.r0; s->r[1] = x.r1; s->r[2] = x.r2; s->r[3] = x.r3;
	if (x.inexact) s->r[11]++;
	}
static void EOp(Regs* s, bool div, bool todouble)
	{
	u32 out[3];
	EDivMul((const u32*)s->r[0], (const u32*)s->r[1], (s32)s->r[2], div, todouble, out);
	s->r[0] = out[0]; s->r[1] = out[1]; s->r[2] = out[2];
	}
extern "C" __attribute__((used)) void drt_btod_ediv(Regs* s) { EOp(s, true, false); }
extern "C" __attribute__((used)) void drt_btod_edivd(Regs* s) { EOp(s, true, true); }
extern "C" __attribute__((used)) void drt_btod_emul(Regs* s) { EOp(s, false, false); }
extern "C" __attribute__((used)) void drt_btod_emuld(Regs* s) { EOp(s, false, true); }
extern "C" __attribute__((used)) void drt_div_common(Regs* s)
	{
	ECore c;
	DivCommon(s->r[0], s->r[1], s->r[2], s->r[3], s->r[4], s->r[5], c);
	s->r[0] = c.r0; s->r[1] = c.r1; s->r[2] = c.r2; s->r[3] = c.r3; s->r[6] = c.r6;
	}
extern "C" __attribute__((used)) void drt_mult_common(Regs* s)
	{
	ECore c;
	MultCommon(s->r[0], s->r[1], s->r[2], s->r[3], s->r[4], s->r[5], c);
	s->r[0] = c.r0; s->r[1] = c.r1; s->r[2] = c.r2; s->r[3] = c.r3; s->r[6] = c.r6;
	}
extern "C" __attribute__((used)) void drt_btod_etento(Regs* s)
	{
	u32 out[3];
	ETenTo((s32)s->r[1], (s32)s->r[2], out);
	u32* p = (u32*)s->r[0];
	p[0] = out[0]; p[1] = out[1]; p[2] = out[2];
	}

// ------------------------------------------------------------------------------------------------------------------
// ARM-state entry points. RUN(core): spill r0-r12 + lr into a Regs on the stack, call core(Regs*), reload everything from it.
#define RUN(core) \
	"	push {r0-r12, lr}\n" \
	"	mov r0, sp\n" \
	"	mov r4, sp\n" \
	"	bic sp, sp, #7\n" \
	"	ldr ip, =" #core "\n" \
	"	blx ip\n" \
	"	mov sp, r4\n" \
	"	pop {r0-r12, lr}\n"

// a Regs entry that returns to lr
#define WRAP(name, core) \
	".global " #name "\n.type " #name ", %function\n" #name ":\n" RUN(core) "	bx lr\n	.ltorg\n"
// an entry reached with the caller's frame still on the stack (push {r4, r5, r6, lr}): finish by popping it
#define WRAPFRAME(name, core) \
	".global " #name "\n.type " #name ", %function\n" #name ":\n" RUN(core) "	pop {r4, r5, r6, lr}\n	bx lr\n	.ltorg\n"

asm(
	".text\n"
	".syntax unified\n"
	".arm\n"
	".balign 4\n"

	// __rt_divtest
	".global __rt_divtest\n.type __rt_divtest, %function\n"
	"__rt_divtest:\n"
	"	cmp r0, #0\n"
	"	bxne lr\n"
	"	b __rt_div0\n"

	// unsigned divide core: r0 = n, r1 = d (non-zero) -> r0 = quotient, r1 = remainder (clobbers r2, r3)
	".Ldrt_udivmod:\n"
	"	clz r3, r1\n"
	"	clz r2, r0\n"
	"	subs r3, r3, r2\n"		// shift = clz(d) - clz(n)
	"	movlo r1, r0\n"			// d > n: q = 0, r = n
	"	movlo r0, #0\n"
	"	bxlo lr\n"
	"	lsl r1, r1, r3\n"
	"	mov r2, #0\n"
	"	add r3, r3, #1\n"
	"1:	cmp r0, r1\n"
	"	subhs r0, r0, r1\n"
	"	adc r2, r2, r2\n"
	"	lsr r1, r1, #1\n"
	"	subs r3, r3, #1\n"
	"	bne 1b\n"
	"	mov r1, r0\n"
	"	mov r0, r2\n"
	"	bx lr\n"

	// __rt_udiv: divisor r0, dividend r1 -> quotient r0, remainder r1
	".global __rt_udiv\n.type __rt_udiv, %function\n"
	"__rt_udiv:\n"
	"	cmp r0, #0\n"
	"	beq __rt_div0\n"
	"	mov r2, r0\n"
	"	mov r0, r1\n"
	"	mov r1, r2\n"
	"	b .Ldrt_udivmod\n"

	// __rt_sdiv: signed; the remainder takes the dividend's sign
	".global __rt_sdiv\n.type __rt_sdiv, %function\n"
	"__rt_sdiv:\n"
	"	cmp r0, #0\n"
	"	beq __rt_div0\n"
	"	push {r4, r5, lr}\n"
	"	eor r5, r0, r1\n"		// bit 31: the quotient is negative
	"	mov r4, r1\n"			// the dividend, for the remainder's sign
	"	cmp r1, #0\n"
	"	rsblt r1, r1, #0\n"
	"	cmp r0, #0\n"
	"	rsblt r0, r0, #0\n"
	"	mov r2, r0\n"
	"	mov r0, r1\n"
	"	mov r1, r2\n"			// r0 = |n|, r1 = |d|
	"	bl .Ldrt_udivmod\n"
	"	cmp r5, #0\n"
	"	rsblt r0, r0, #0\n"
	"	cmp r4, #0\n"
	"	rsblt r1, r1, #0\n"
	"	pop {r4, r5, pc}\n"

	// divide by 10: dividend r0 -> quotient r0, remainder r1 (via the reciprocal 0xCCCCCCCD / 2^35)
	".global __rt_udiv10\n.type __rt_udiv10, %function\n"
	"__rt_udiv10:\n"
	"	ldr r1, =0xCCCCCCCD\n"
	"	umull r3, r2, r0, r1\n"
	"	lsr r2, r2, #3\n"
	"	add r3, r2, r2, lsl #2\n"
	"	sub r1, r0, r3, lsl #1\n"
	"	mov r0, r2\n"
	"	bx lr\n"
	".ltorg\n"
	".global __rt_sdiv10\n.type __rt_sdiv10, %function\n"
	"__rt_sdiv10:\n"
	"	movs r3, r0\n"
	"	rsbmi r0, r0, #0\n"
	"	ldr r1, =0xCCCCCCCD\n"
	"	umull ip, r2, r0, r1\n"
	"	lsr r2, r2, #3\n"
	"	add ip, r2, r2, lsl #2\n"
	"	sub r1, r0, ip, lsl #1\n"
	"	mov r0, r2\n"
	"	cmp r3, #0\n"
	"	rsblt r0, r0, #0\n"
	"	rsblt r1, r1, #0\n"
	"	bx lr\n"
	".ltorg\n"

	// 32 by 16 and 64 by 32 (C: drt_udiv32by16 ... return the two result registers as one u64)
	".global __rt_udiv32by16\n.type __rt_udiv32by16, %function\n"
	"__rt_udiv32by16:\n"
	"	push {r4, lr}\n"
	"	bl drt_udiv32by16\n"
	"	pop {r4, pc}\n"
	".global __rt_sdiv32by16\n.type __rt_sdiv32by16, %function\n"
	"__rt_sdiv32by16:\n"
	"	push {r4, lr}\n"
	"	bl drt_sdiv32by16\n"
	"	pop {r4, pc}\n"
	".global __rt_sdiv64by32\n.type __rt_sdiv64by32, %function\n"
	"__rt_sdiv64by32:\n"
	"	push {r4, lr}\n"
	"	bl drt_sdiv64by32\n"
	"	pop {r4, pc}\n"

	// 64-bit "reverse" divisions: a zero divisor is a tail jump to __rt_div0
	".global _ll_urdv\n.type _ll_urdv, %function\n"
	"_ll_urdv:\n"
	"	orrs ip, r0, r1\n"
	"	beq __rt_div0\n"
	RUN(drt_ll_urdv)
	"	bx lr\n	.ltorg\n"
	".global _ll_srdv\n.type _ll_srdv, %function\n"
	"_ll_srdv:\n"
	"	orrs ip, r0, r1\n"
	"	beq __rt_div0\n"
	RUN(drt_ll_srdv)
	"	bx lr\n	.ltorg\n"
	WRAP(_ll_udiv10, drt_ll_udiv10)
	WRAPFRAME(_ll_udiv_small, drt_ll_udiv_small)
	WRAPFRAME(_ll_udiv_big, drt_ll_udiv_big)
	WRAPFRAME(_ll_udiv_ginormous, drt_ll_udiv_ginormous)

	WRAP(_btod_d2e, drt_btod_d2e)
	WRAP(_d2e_norm_op1, drt_d2e_norm_op1)
	WRAP(_d2e_denorm_low, drt_d2e_denorm_low)
	WRAP(_e2e, drt_e2e)
	WRAP(_e2d, drt_e2d)
	WRAP(_btod_ediv, drt_btod_ediv)
	WRAP(_btod_edivd, drt_btod_edivd)
	WRAP(_btod_emul, drt_btod_emul)
	WRAP(_btod_emuld, drt_btod_emuld)
	WRAP(__btod_div_common, drt_div_common)
	WRAP(__btod_mult_common, drt_mult_common)
	WRAP(_btod_etento, drt_btod_etento)

	// ---- memory
	// __rt_memcpy: r0 dst, r1 src, r2 n, forward
	".global __rt_memcpy\n.type __rt_memcpy, %function\n"
	"__rt_memcpy:\n"
	"	cmp r2, #8\n"
	"	blo 3f\n"
	"	eor r3, r0, r1\n"
	"	tst r3, #3\n"
	"	bne 3f\n"
	"	ands r3, r0, #3\n"
	"	beq 2f\n"
	"	rsb r3, r3, #4\n"
	"	sub r2, r2, r3\n"
	"1:	ldrb ip, [r1], #1\n"
	"	strb ip, [r0], #1\n"
	"	subs r3, r3, #1\n"
	"	bne 1b\n"
	"2:	subs r2, r2, #4\n"
	"	ldrhs r3, [r1], #4\n"
	"	strhs r3, [r0], #4\n"
	"	bhs 2b\n"
	"	add r2, r2, #4\n"
	"3:	cmp r2, #0\n"
	"	bxeq lr\n"
	"4:	ldrb r3, [r1], #1\n"
	"	strb r3, [r0], #1\n"
	"	subs r2, r2, #1\n"
	"	bne 4b\n"
	"	bx lr\n"
	// _memcpy_lastbytes: copy r2 & 3 bytes
	".global _memcpy_lastbytes\n.type _memcpy_lastbytes, %function\n"
	"_memcpy_lastbytes:\n"
	"	ands r2, r2, #3\n"
	"	bxeq lr\n"
	"1:	ldrb r3, [r1], #1\n"
	"	strb r3, [r0], #1\n"
	"	subs r2, r2, #1\n"
	"	bne 1b\n"
	"	bx lr\n"
	// __memmove_aligned: r0 / r1 end pointers (word aligned), r2 n; backwards
	".global __memmove_aligned\n.type __memmove_aligned, %function\n"
	"__memmove_aligned:\n"
	"1:	cmp r2, #4\n"
	"	blo 2f\n"
	"	ldr r3, [r1, #-4]!\n"
	"	str r3, [r0, #-4]!\n"
	"	sub r2, r2, #4\n"
	"	b 1b\n"
	"2:	cmp r2, #0\n"
	"	bxeq lr\n"
	"3:	ldrb r3, [r1, #-1]!\n"
	"	strb r3, [r0, #-1]!\n"
	"	subs r2, r2, #1\n"
	"	bne 3b\n"
	"	bx lr\n"
	// __rt_memset: r0 dst, r1 byte, r2 n
	".global __rt_memset\n.type __rt_memset, %function\n"
	"__rt_memset:\n"
	"	and r3, r1, #0xff\n"
	"	mov r1, r2\n"
	"	orr r2, r3, r3, lsl #8\n"
	"	orr r2, r2, r2, lsl #16\n"
	// falls into _memset: r0 dst, r1 n, r2 fill word
	".global _memset\n.type _memset, %function\n"
	"_memset:\n"
	"	cmp r1, #4\n"
	"	blo 2f\n"
	"	ands ip, r0, #3\n"
	"	beq _memset_w\n"
	"	rsb ip, ip, #4\n"
	"	sub r1, r1, ip\n"
	"1:	strb r2, [r0], #1\n"
	"	subs ip, ip, #1\n"
	"	bne 1b\n"
	"	b _memset_w\n"
	"2:	cmp r1, #0\n"
	"	bxeq lr\n"
	"3:	strb r2, [r0], #1\n"
	"	subs r1, r1, #1\n"
	"	bne 3b\n"
	"	bx lr\n"
	// _memset_w: r0 dst (word aligned), r1 n, r2 fill word
	".global _memset_w\n.type _memset_w, %function\n"
	"_memset_w:\n"
	"1:	cmp r1, #4\n"
	"	blo 2f\n"
	"	str r2, [r0], #4\n"
	"	sub r1, r1, #4\n"
	"	b 1b\n"
	"2:	cmp r1, #0\n"
	"	bxeq lr\n"
	"3:	strb r2, [r0], #1\n"
	"	subs r1, r1, #1\n"
	"	bne 3b\n"
	"	bx lr\n"
	);
