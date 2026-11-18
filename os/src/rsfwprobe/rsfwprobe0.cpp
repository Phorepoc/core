// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// rsfwprobe0.cpp -- OURS: the smallest process SSM can start: one line on the debug port, then exit. It tells whether a process of ours
// started from SSM's non-critical list runs at all (rsfwprobe.exe printed nothing). Diagnosis only: remove with the RSFW trace.
#include <e32std.h>
#include <e32debug.h>

TInt E32Main()
	{
	RDebug::Printf("[rsfwprobe0] hello, tick %d", (TInt)User::NTickCount());
	return KErrNone;
	}
