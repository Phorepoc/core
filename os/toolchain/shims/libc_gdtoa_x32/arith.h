/*
 * arith.h -- x32 (amd64) board copy of Open C's libc/inc/arith.h for gdtoa (strtod, dtoa, printf's floating formats).
 *
 * The 2010 file is the ARM one: `#define IEEE_8087`, `Arith_Kind_ASL 1` AND `#define Sudden_Underflow`, which tells gdtoa the FPU flushes
 * subnormal results to zero. x86-64 SSE2 has gradual underflow, so with Sudden_Underflow dtoa() (printf %g/%e) prints every value below DBL_MIN with wrong digits
 * (found with GLib's glib/tests/strfuncs.c: g_ascii_strtod (g_ascii_dtostr (pow (2, -1024.1))) != pow (2, -1024.1))
 * Only the x32 build takes this copy (registry/components.py: "libc", incs_first); the ARM build keeps the original.
 * gdtoaimp.h and gdtoa.h in this directory are BYTE COPIES of the baseline ones: a quote-include finds the includer's own directory first, and the
 * baseline gdtoa.h (which includes "arith.h") sits beside the baseline arith.h, so only copies in this -iquote directory make this arith.h win.
 */

#define IEEE_8087
#define Arith_Kind_ASL 1
