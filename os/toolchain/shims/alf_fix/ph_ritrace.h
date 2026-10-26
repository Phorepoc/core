/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// ph_ritrace.h -- OURS: the RI's (sflibopenvgrefimpl) generated trace lines (tools/alf_instrument.py, riMiniEGL_ph.cpp / riImage_ph.cpp)
// print only on the alf server thread: the same code runs constantly for WSERV's icon rasteriser and every VG client.
#ifndef PH_RITRACE_H
#define PH_RITRACE_H
#include <e32std.h>
#include <e32debug.h>
// (perf 2026-10-26) the answer is cached for the stack range of the last thread asked: a thread is known by the address
// of a local, with no exec (RThread().Id() on every call was ~10% of the compositor; the name copy + fold compare before it more)
inline TBool PhRiAlfThread()
	{
	static TUint lastLo, lastHi;
	static TBool lastAlf;
	const TUint sp = (TUint)&sp;
	if (sp - lastLo >= lastHi - lastLo)
		{
		TName n = RThread().Name();
		lastAlf = n.Length() >= 11 && n.Left(11).CompareF(_L("alfredserve")) == 0;
		TThreadStackInfo si;
		if (RThread().StackInfo(si) == KErrNone)
			{
			lastLo = (TUint)si.iLimit;
			lastHi = (TUint)si.iBase;
			}
		else
			lastLo = lastHi = 0;
		}
	return lastAlf;
	}
#define PHRI(...) do { if (PhRiAlfThread()) RDebug::Printf(__VA_ARGS__); } while (0)
#endif
