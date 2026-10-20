/* phmath.c -- OURS (Phorepoc libphcompat): C99 <math.h> functions Open C's libm lacks (found by cmath_probe.sh / the JSC link census). */
#include <math.h>
#include <stdint.h>
#include <string.h>

double log2(double x)
{
    int e; double m;
    if (!(x > 0.0) || isinf(x)) return log(x);       /* NaN, 0 -> -inf, <0 -> NaN, +inf */
    m = frexp(x, &e);                                 /* x = m * 2^e, m in [0.5,1) */
    if (m == 0.5) return (double)(e - 1);             /* exact for powers of two */
    return log(m) * 1.4426950408889634074 + (double)e;
}
float log2f(float x) { return (float)log2((double)x); }

double nan(const char* tag) { (void)tag; return NAN; }
float nanf(const char* tag) { (void)tag; return NAN; }

#undef frexpl
#undef ldexpl
#undef ldexpf
/* sin(x) + cos(x) of one argument is folded by GCC into a call to sincos -- here that recursed until the stack overflowed (KERN-EXEC 3,
   2026-10-13): a volatile copy hides the shared argument */
void sincos(double x, double* s, double* c) { volatile double y = x; *s = sin(y); *c = cos(x); }
float ldexpf(float x, int n) { return (float)ldexp((double)x, n); }
long double ldexpl(long double x, int n) { return (long double)ldexp((double)x, n); }
long double frexpl(long double x, int* e) { return (long double)frexp((double)x, e); }
