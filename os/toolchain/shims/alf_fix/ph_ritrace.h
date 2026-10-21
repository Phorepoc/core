/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// ph_ritrace.h -- OURS: the RI's (sflibopenvgrefimpl) generated trace lines (tools/alf_instrument.py, riMiniEGL_ph.cpp / riImage_ph.cpp)
// print only on the alf server thread: the same code runs constantly for WSERV's icon rasteriser and every VG client.
#ifndef PH_RITRACE_H
#define PH_RITRACE_H
#include <e32std.h>
#include <e32debug.h>
inline TBool PhRiAlfThread()
	{
	TName n = RThread().Name();
	return n.Length() >= 11 && n.Left(11).CompareF(_L("alfredserve")) == 0;
	}
#define PHRI(...) do { if (PhRiAlfThread()) RDebug::Printf(__VA_ARGS__); } while (0)
#endif
