// ph_throttle.h -- OURS: PHT(...) = RDebug::Printf throttled PER CALL SITE (first 4 hits, then every power of two); the generated alfbridge trace copy uses it.
#ifndef PH_THROTTLE_H
#define PH_THROTTLE_H
#include <e32std.h>
#include <e32debug.h>
#define PHT(...) do { static TUint phn_; ++phn_; if (phn_ <= 4 || (phn_ & (phn_ - 1)) == 0) RDebug::Printf(__VA_ARGS__); } while (0)
#endif
