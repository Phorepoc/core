// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// drtrvct2_2.cpp -- OURS (2026-10-30): the RVCT 2.2 runtime DLL that applications built with ARM's RVCT 2.2 import
// (drtrvct2_2.dll, the release's kernel/eka/compsupp/rvct/drtrvct.mmp). The release builds it from a few Symbian sources
// (rtdiv0, rtexit, sftfpini, rtopnew, rtopdel: built here unmodified, see drtrvct2_2.mmp; rtabort below) plus RVCT's
// proprietary C library (c_t__un.l / h_t__un.l), which supplies the rest of drtrvct2_2u.def. This file is that rest.
//
// Why: 19 of 65 binaries of a set of S^3-era games import it (Cut the Rope, Worms, Tetris, The Sims 3, PES 2011...);
// without it the loader refuses the executable (KErrNotFound, apparc StartApp -1).
//
// The long-long helpers RVCT 2.2's Thumb code calls (the __ARM_ll_* family, its 64-bit arithmetic) are named for their
// operands: l = long long (two registers), s = signed int, u = unsigned int, so __ARM_ll_addls(long long, int) is a
// long long plus a sign-extended int and __ARM_ll_rsb* is the reverse subtraction (second operand minus the first).
// Their registers are the AAPCS ones. The comparisons return their result in the flags: __ARM_ll_cmpge, _cmple and
// _cmpu set N, Z, C and V as a 64-bit CMP would, so every signed and unsigned condition after them holds, and they
// preserve r0-r3. __ARM_switch8 is the Thumb switch dispatcher: the case-offset table follows the call (count byte at
// lr-1, then count+1 halfword offsets as bytes, the last the default), index in r0.
//
// The functions of the def whose behaviour is RVCT-internal (its decimal conversion _btod_* / _e2*, the division
// helpers _ll_*div* / __rt_*div*, the __rt_mem* and _memset* variants, signal raising) panic "DRTRVCT2_2" with their
// ordinal: no application here imports them, and their register interfaces are not derivable from their names.
#include <e32std.h>

// the release's rtabort.cpp, here because GCC wants std::terminate declared before it is defined
namespace std { void terminate(); }

extern "C" EXPORT_C void __rt_abort()		// never returns
	{
	RThread().Kill(KErrAbort);
	}

EXPORT_C void std::terminate()
	{
	__rt_abort();
	}

extern "C"
	{
	// ---- arithmetic (AAPCS: long long in r0:r1, the second operand in r2(:r3))
	EXPORT_C long long __ARM_ll_add(long long a, long long b) { return a + b; }
	EXPORT_C long long __ARM_ll_addls(long long a, int b) { return a + b; }
	EXPORT_C long long __ARM_ll_addlu(long long a, unsigned b) { return a + b; }
	EXPORT_C long long __ARM_ll_addss(int a, int b) { return (long long)a + b; }
	EXPORT_C long long __ARM_ll_adduu(unsigned a, unsigned b) { return (long long)a + b; }
	EXPORT_C long long __ARM_ll_sub(long long a, long long b) { return a - b; }
	EXPORT_C long long __ARM_ll_subls(long long a, int b) { return a - b; }
	EXPORT_C long long __ARM_ll_sublu(long long a, unsigned b) { return a - b; }
	EXPORT_C long long __ARM_ll_subss(int a, int b) { return (long long)a - b; }
	EXPORT_C long long __ARM_ll_subuu(unsigned a, unsigned b) { return (long long)a - b; }
	EXPORT_C long long __ARM_ll_rsb(long long a, long long b) { return b - a; }
	EXPORT_C long long __ARM_ll_rsbls(long long a, int b) { return b - a; }
	EXPORT_C long long __ARM_ll_rsblu(long long a, unsigned b) { return b - a; }
	EXPORT_C long long __ARM_ll_rsbss(int a, int b) { return (long long)b - a; }
	EXPORT_C long long __ARM_ll_rsbuu(unsigned a, unsigned b) { return (long long)b - a; }
	EXPORT_C long long _ll_mul(long long a, long long b) { return a * b; }
	EXPORT_C long long __ARM_ll_mulls(long long a, int b) { return a * b; }
	EXPORT_C long long __ARM_ll_mullu(long long a, unsigned b) { return a * b; }
	EXPORT_C long long __ARM_ll_mulss(int a, int b) { return (long long)a * b; }
	EXPORT_C long long __ARM_ll_muluu(unsigned a, unsigned b) { return (long long)((unsigned long long)a * b); }
	EXPORT_C long long __ARM_ll_mlass(long long acc, int a, int b) { return acc + (long long)a * b; }
	EXPORT_C long long __ARM_ll_mlauu(long long acc, unsigned a, unsigned b) { return acc + (long long)((unsigned long long)a * b); }
	EXPORT_C long long __ARM_ll_neg(long long a) { return -a; }
	EXPORT_C long long __ARM_ll_not(long long a) { return ~a; }
	EXPORT_C long long __ARM_ll_and(long long a, long long b) { return a & b; }
	EXPORT_C long long __ARM_ll_or(long long a, long long b) { return a | b; }
	EXPORT_C long long __ARM_ll_eor(long long a, long long b) { return a ^ b; }
	EXPORT_C long long __ARM_ll_from_l(int a) { return a; }
	EXPORT_C long long __ARM_ll_from_u(unsigned a) { return a; }
	EXPORT_C int __ARM_ll_to_l(long long a) { return (int)a; }

	// ---- the C library's integer functions
	struct div_t { int quot, rem; };
	struct ldiv_t { long quot, rem; };
	struct lldiv_t { long long quot, rem; };
	EXPORT_C int abs(int a) { return a < 0 ? -a : a; }
	EXPORT_C long long llabs(long long a) { return a < 0 ? -a : a; }
	EXPORT_C div_t div(int n, int d) { div_t r = { n / d, n % d }; return r; }
	EXPORT_C ldiv_t ldiv(long n, long d) { ldiv_t r = { n / d, n % d }; return r; }
	EXPORT_C lldiv_t lldiv(long long n, long long d) { lldiv_t r = { n / d, n % d }; return r; }

	// ---- RVCT internals not implemented: panic with the ordinal
	void DrtRvctUnimplemented(TInt aOrdinal)
		{
		_LIT(KPanic, "DRTRVCT2_2");
		User::Panic(KPanic, aOrdinal);
		}
	}

// the flag-returning comparisons and the switch dispatcher (ARM state; return with bx lr to either state)
// __ARM_ll_cmp*: the flags of the 64-bit compare r0:r1 - r2:r3 -- N (V clear) when less as signed, Z when equal, C when
// higher-or-same as unsigned; r0-r3 preserved.
// __ARM_switch8: index r0; the table follows the (Thumb) call: lr-1 = the case count n, then n+1 byte entries (the last
// the default), each the halfword offset from lr to the case; r0 preserved.
asm(
	".text\n"
	".arm\n"
	".balign 4\n"
	".global __ARM_ll_cmpge, __ARM_ll_cmple, __ARM_ll_cmpu\n"
	".type __ARM_ll_cmpge, %function\n"
	".type __ARM_ll_cmple, %function\n"
	".type __ARM_ll_cmpu, %function\n"
	"__ARM_ll_cmpge:\n"
	"__ARM_ll_cmple:\n"
	"__ARM_ll_cmpu:\n"
	"	cmp r1, r3\n"
	"	bne 1f\n"
	"	cmp r0, r2\n"
	"	moveq ip, #0x60000000\n"		// equal: Z C
	"	movhi ip, #0x20000000\n"		// high words equal: the low words decide signed and unsigned alike
	"	movlo ip, #0x80000000\n"
	"	msr cpsr_f, ip\n"
	"	bx lr\n"
	"1:	mov ip, #0\n"
	"	orrlt ip, ip, #0x80000000\n"	// signed less (from the high words)
	"	orrhs ip, ip, #0x20000000\n"	// unsigned higher or same
	"	msr cpsr_f, ip\n"
	"	bx lr\n"
	".global __ARM_switch8\n"
	".type __ARM_switch8, %function\n"
	"__ARM_switch8:\n"
	"	push {r1}\n"
	"	ldrb r1, [lr, #-1]\n"
	"	cmp r0, r1\n"
	"	ldrblo r1, [lr, r0]\n"
	"	ldrbhs r1, [lr, r1]\n"
	"	add ip, lr, r1, lsl #1\n"
	"	pop {r1}\n"
	"	bx ip\n"
	);

#define DRT_UNIMPLEMENTED(name, ordinal) \
	asm(".text\n.arm\n.balign 4\n.global " #name "\n.type " #name ", %function\n" #name ":\n" \
		"\tmov r0, #" #ordinal "\n\tldr ip, =DrtRvctUnimplemented\n\tbx ip\n\t.ltorg\n");

DRT_UNIMPLEMENTED(__btod_div_common, 11)
DRT_UNIMPLEMENTED(__btod_mult_common, 12)
DRT_UNIMPLEMENTED(__memmove_aligned, 13)
DRT_UNIMPLEMENTED(__rt_divtest, 16)
DRT_UNIMPLEMENTED(__rt_lib_init, 19)
DRT_UNIMPLEMENTED(__rt_lib_shutdown, 20)
DRT_UNIMPLEMENTED(__rt_memcpy, 21)
DRT_UNIMPLEMENTED(__rt_memset, 22)
DRT_UNIMPLEMENTED(__rt_raise, 23)
DRT_UNIMPLEMENTED(__rt_sdiv, 24)
DRT_UNIMPLEMENTED(__rt_sdiv10, 25)
DRT_UNIMPLEMENTED(__rt_sdiv32by16, 26)
DRT_UNIMPLEMENTED(__rt_sdiv64by32, 27)
DRT_UNIMPLEMENTED(__rt_udiv, 29)
DRT_UNIMPLEMENTED(__rt_udiv10, 30)
DRT_UNIMPLEMENTED(__rt_udiv32by16, 31)
DRT_UNIMPLEMENTED(_btod_d2e, 32)
DRT_UNIMPLEMENTED(_btod_ediv, 33)
DRT_UNIMPLEMENTED(_btod_edivd, 34)
DRT_UNIMPLEMENTED(_btod_emul, 35)
DRT_UNIMPLEMENTED(_btod_emuld, 36)
DRT_UNIMPLEMENTED(_btod_etento, 37)
DRT_UNIMPLEMENTED(_d2e_denorm_low, 38)
DRT_UNIMPLEMENTED(_d2e_norm_op1, 39)
DRT_UNIMPLEMENTED(_e2d, 40)
DRT_UNIMPLEMENTED(_e2e, 41)
DRT_UNIMPLEMENTED(_ll_srdv, 68)
DRT_UNIMPLEMENTED(_ll_udiv10, 75)
DRT_UNIMPLEMENTED(_ll_udiv_big, 76)
DRT_UNIMPLEMENTED(_ll_udiv_ginormous, 77)
DRT_UNIMPLEMENTED(_ll_udiv_small, 78)
DRT_UNIMPLEMENTED(_ll_urdv, 79)
DRT_UNIMPLEMENTED(_memcpy_lastbytes, 80)
DRT_UNIMPLEMENTED(_memset, 81)
DRT_UNIMPLEMENTED(_memset_w, 82)
