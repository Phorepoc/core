/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// ph_ritrace.h -- OURS: the RI's (sflibopenvgrefimpl) generated trace lines (tools/alf_instrument.py, riMiniEGL_ph.cpp / riImage_ph.cpp)
// print only on the alf server thread: the same code runs constantly for WSERV's icon rasteriser and every VG client.
#ifndef PH_RITRACE_H
#define PH_RITRACE_H
#include <e32std.h>
#include <e32debug.h>
// (perf 2026-10-26: the answer is cached for the last thread asked -- the name copy + fold compare ran on every call)
inline TBool PhRiAlfThread()
	{
	static TUint lastId;
	static TBool lastAlf;
	const TUint id = (TUint)RThread().Id();
	if (id != lastId)
		{
		TName n = RThread().Name();
		lastAlf = n.Length() >= 11 && n.Left(11).CompareF(_L("alfredserve")) == 0;
		lastId = id;
		}
	return lastAlf;
	}
#define PHRI(...) do { if (PhRiAlfThread()) RDebug::Printf(__VA_ARGS__); } while (0)
#endif
