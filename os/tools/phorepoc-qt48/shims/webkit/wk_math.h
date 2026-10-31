// WebKit (2011) expects the C99 isnan/isinf/signbit that Symbian's stdapis math.h provides globally. Force-included for the
// QtWebKit set: plain inline overloads (libstdc++ <cmath> would drag newlib's math.h/reent.h into the Symbian headers).
#include <time.h>
#ifdef __cplusplus
inline int isnan(double x) { return __builtin_isnan(x); }
inline int isnan(float x) { return __builtin_isnan(x); }
inline int isinf(double x) { return __builtin_isinf_sign(x); }
inline int isinf(float x) { return __builtin_isinf_sign(x); }
inline int signbit(double x) { return __builtin_signbit(x); }
inline int signbit(float x) { return __builtin_signbit(x); }
inline int isfinite(double x) { return __builtin_isfinite(x); }
inline int isfinite(float x) { return __builtin_isfinite(x); }
#endif
