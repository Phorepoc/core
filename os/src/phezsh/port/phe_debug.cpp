// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phe_debug.cpp -- a debug-log line from C (RDebug::Printf: the serial log on the PC, the phlink log on a phone)
#include <e32debug.h>

extern "C" void phe_debug(const char* aFmt, int aA, int aB)
	{
	RDebug::Printf(aFmt, aA, aB);
	}
