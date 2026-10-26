// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phealloctest.cpp -- OURS: cost of the user-side allocator and of fast execs on this kernel (perf, 2026-10-26).
// The compositor's profile showed Exec::Heap() (the fast exec behind User::Alloc / User::Free) as its hottest
// address: this says whether one call is cheap (a few hundred ns: then the caller count is the problem) or not
// (then the fast-exec path is). Prints "PH: alloctest <case> <ns per op>"; User::FastCounter runs at 24 MHz.
#include <e32base.h>
#include <e32debug.h>

static const TInt KN = 100000;
static const TInt KTicksPerUs = 24;

static void Report(const char* aCase, TUint32 aTicks, TInt aOps)
	{
	const TUint ns = (TUint)((TUint64)aTicks * 1000 / KTicksPerUs / aOps);
	RDebug::Printf("PH: alloctest %s %u ns per op (%d ops)", aCase, ns, aOps);
	}

GLDEF_C TInt E32Main()
	{
	volatile TInt sink = 0;
	TUint32 t0 = User::FastCounter();
	for (TInt i = 0; i < KN; i++) sink += i;
	Report("empty loop", User::FastCounter() - t0, KN);

	t0 = User::FastCounter();
	for (TInt i = 0; i < KN; i++) sink += (TInt)User::FastCounter();
	Report("fast exec FastCounter", User::FastCounter() - t0, KN);

	t0 = User::FastCounter();
	for (TInt i = 0; i < KN; i++) sink += (TInt)&User::Allocator();
	Report("fast exec Heap (User::Allocator)", User::FastCounter() - t0, KN);

	t0 = User::FastCounter();
	for (TInt i = 0; i < KN; i++) sink += (TInt)User::NTickCount();
	Report("fast exec NTickCount", User::FastCounter() - t0, KN);

	t0 = User::FastCounter();
	for (TInt i = 0; i < KN; i++) { TAny* p = User::Alloc(64); User::Free(p); }
	Report("User::Alloc+Free 64 B", User::FastCounter() - t0, KN);

	RHeap& h = User::Allocator();
	t0 = User::FastCounter();
	for (TInt i = 0; i < KN; i++) { TAny* p = h.Alloc(64); h.Free(p); }
	Report("RHeap::Alloc+Free 64 B (heap held)", User::FastCounter() - t0, KN);

	const TInt kBig = KN / 100;
	t0 = User::FastCounter();
	for (TInt i = 0; i < kBig; i++) { TAny* p = User::Alloc(64 * 1024); User::Free(p); }
	Report("User::Alloc+Free 64 KB", User::FastCounter() - t0, kBig);

	t0 = User::FastCounter();
	for (TInt i = 0; i < kBig; i++) { TAny* p = User::Alloc(1024 * 1024); User::Free(p); }
	Report("User::Alloc+Free 1 MB", User::FastCounter() - t0, kBig);

	RDebug::Printf("PH: alloctest done (%d)", (TInt)sink & 1);
	return KErrNone;
	}
