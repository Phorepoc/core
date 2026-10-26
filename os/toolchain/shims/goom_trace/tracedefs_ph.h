// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// tracedefs_ph.h -- OURS: -include'd into the real GOOM monitor (registry goommonitor_real, built with _GOOM_DEBUG so its
// INFO/ERROR traces are compiled in). It takes the place of goommonitor/inc/tracedefs.h (same include guard): the trace
// prefix becomes "PH: goom: " -- the kernel log keeps "PH:" lines -- and every trace site is throttled to its first 8
// hits and then each power of two, so GOOM's per-entry dumps (one line per profiling value on every check) stay bounded.
// A throttled call prints an empty format.
#ifndef TRACEDEFS_H
#define TRACEDEFS_H
#include <e32std.h>

inline TPtrC PhGoomFmt(const TText* aFmt)
	{
	static const TText* sites[128];
	static TUint hits[128];
	TInt i = 0;
	for (; i < 128 && sites[i] && sites[i] != aFmt; i++) {}
	if (i == 128)
		return TPtrC();
	sites[i] = aFmt;
	const TUint n = ++hits[i];
	return n <= 8 || (n & (n - 1)) == 0 ? TPtrC(aFmt) : TPtrC();
	}

#define _PREFIX_TRACE( aMsg ) PhGoomFmt( (const TText*)L"PH: goom: " L##aMsg )
#define _PREFIX_TRACE_2( aMsg1, aMsg2 ) PhGoomFmt( (const TText*)L"PH: goom: " L##aMsg1 L##aMsg2 )
#define _PREFIX_CHAR( aMsg ) (const char*)"PH: goom: " ##aMsg
#endif
