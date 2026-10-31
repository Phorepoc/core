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
// _cmpu set N, Z, C and V as a 64-bit CMP would (_cmple of the operands swapped: its callers test GE for a <= b), and they
// preserve r0-r3. __ARM_switch8 is the Thumb switch dispatcher: the case-offset table follows the call (count byte at
// lr-1, then count+1 halfword offsets as bytes, the last the default), index in r3.
//
// The functions of the def whose behaviour is RVCT-internal (its decimal conversion _btod_* / _e2*, the division
// helpers _ll_*div* / __rt_*div*, the __rt_mem* and _memset* variants, signal raising, library init) are in
// drtrvct2_2_rt.cpp.
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
	}

// the flag-returning comparisons and the switch dispatcher (ARM state; return with bx lr to either state)
// __ARM_ll_cmpge / _cmpu: the flags of the 64-bit compare r0:r1 - r2:r3 -- N (V clear) when less as signed, Z when equal,
// C when higher-or-same as unsigned; __ARM_ll_cmple the same for the operands swapped, r2:r3 - r0:r1 (its callers test
// GE for a <= b); r0-r3 preserved.
// __ARM_switch8: index r3; the table follows the (Thumb) call: lr-1 = the case count n, then n+1 byte entries (the last
// the default), each the halfword offset from lr to the case; r0-r3 preserved (the cases use them: RVCT 2.2 code loads
// r0-r2 before the call -- an index in r0 sent every switch to its default, Tetris's text width 0 and its word wrap
// spinning until the view server's activation timeout, ViewSrv 11).
asm(
	".text\n"
	".syntax unified\n"
	".arm\n"
	".balign 4\n"
	".global __ARM_ll_cmpge, __ARM_ll_cmple, __ARM_ll_cmpu\n"
	".type __ARM_ll_cmpge, %function\n"
	".type __ARM_ll_cmple, %function\n"
	".type __ARM_ll_cmpu, %function\n"
	"__ARM_ll_cmple:\n"
	"	cmp r3, r1\n"
	"	bne 2f\n"
	"	cmp r2, r0\n"
	"	moveq ip, #0x60000000\n"
	"	movhi ip, #0x20000000\n"
	"	movlo ip, #0x80000000\n"
	"	msr cpsr_f, ip\n"
	"	bx lr\n"
	"2:	mov ip, #0\n"
	"	orrlt ip, ip, #0x80000000\n"
	"	orrhs ip, ip, #0x20000000\n"
	"	msr cpsr_f, ip\n"
	"	bx lr\n"
	"__ARM_ll_cmpge:\n"
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
	"	ldrb ip, [lr, #-1]\n"
	"	cmp r3, ip\n"
	"	ldrblo ip, [lr, r3]\n"
	"	ldrbhs ip, [lr, ip]\n"
	"	add ip, lr, ip, lsl #1\n"
	"	bx ip\n"
	);
