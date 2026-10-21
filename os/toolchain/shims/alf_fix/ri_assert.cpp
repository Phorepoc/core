// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// ri_assert.cpp -- OURS: sflibopenvgrefimpl_alf only. riDefs.h turns RI_ASSERT into libc assert() (RI_DEBUG is defined); libc's __assert on this
// board ends in abort() and the thread simply never comes back (S2, 2026-10-16: RIEGLSurface's ctor asserts EGL_BACK_BUFFER and hitchcock asked
// for EGL_SINGLE_BUFFER -> a silent hang). Here a failed RI assertion is printed ("PH:") and execution continues, as in an NDEBUG build.
#include <e32std.h>
#include <e32debug.h>

extern "C" void __assert(const char* aFile, int aLine, const char* aExpr)
	{
	static TInt n;
	if (++n <= 40)
		RDebug::Printf("PH: alf: RI_ASSERT failed (ignored) %s:%d  %s", aFile, aLine, aExpr);
	}
