/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later */

// ph_alflog.h -- OURS: -include'd into the ALF components (emitter/components.py _ALFTRACE): the release's __ALFLOGSTRING*
// macros (alflogger.h, empty in a release build) become "PH:"-prefixed RDebug::Printf lines -- the kernel log keeps only
// "PH:" lines -- so a hang during the alf bring-up is located. Throttled PER LOG SITE (the literal's address): the first
// 4 hits and then every power of two, so the per-frame lines of CAlfRenderStage::Begin/End cannot flood the serial link.
// alflogger.h is include-guarded: pulling it in first lets us redefine its macros; the sources' own include then does nothing.
#include <e32std.h>
#include <e32debug.h>
#include <e32base.h>
#include <stdarg.h>
#include <symbian-2010/oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/CommonInc/alflogger.h>
#define PH_ALFLOG 1
#undef __ALFLOGSTRING
#undef __ALFLOGSTRING1
#undef __ALFLOGSTRING2
#undef __ALFLOGSTRING3
#undef __ALFLOGSTRING4

inline TBool PhAlfAllow(const char* aSite)
	{
	static const char* sites[96];
	static TUint hits[96];
	TInt i = 0;
	for (; i < 96 && sites[i] && sites[i] != aSite; i++) {}
	if (i == 96) return EFalse;
	if (!sites[i]) { sites[i] = aSite; hits[i] = 0; }
	const TUint n = ++hits[i];
	return n <= 4 || (n & (n - 1)) == 0;
	}

inline void PhAlfLog(const char* aFmt, ...)
	{
	if (!PhAlfAllow(aFmt)) return;
	TBuf8<200> b;
	VA_LIST l;
	VA_START(l, aFmt);
	b.AppendFormatList(TPtrC8((const TUint8*)aFmt), l);
	VA_END(l);
	RDebug::Printf("PH: alf: %s", (const char*)b.PtrZ());
	}

#define __ALFLOGSTRING(C) PhAlfLog(C);
#define __ALFLOGSTRING1(C, X) PhAlfLog(C, X);
#define __ALFLOGSTRING2(C, X, Y) PhAlfLog(C, X, Y);
#define __ALFLOGSTRING3(C, X, Y, Z) PhAlfLog(C, X, Y, Z);
#define __ALFLOGSTRING4(C, X, Y, Z, W) PhAlfLog(C, X, Y, Z, W);
