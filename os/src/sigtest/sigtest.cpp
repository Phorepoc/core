// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// sigtest.cpp -- OURS: request-signal bookkeeping test for the Path C port (X4.68). Not Symbian source.
//
// BrowserNG's connection start completed (iStatus 0) but its active object never ran: the thread's request
// semaphore had lost that completion's signal. A thread may only lose one if a wait swallows a signal meant
// for another request and fails to give it back. User::WaitForRequest(a) counts the extra signals it absorbs
// and returns them with Exec::RequestSignal(n) -- these cases check that path on this kernel:
//   1  WaitForRequest(b) while a completes first      -> a's signal must still be there afterwards
//   2  the same, two absorbed signals                 -> both must still be there
//   3  WaitForRequest(a, b) (the two-status variant)  -> the other one's signal is kept
// Each "must still be there" is tested with a bounded wait against a 2 s guard timer.
#include <e32base.h>
#include <e32debug.h>

static const TInt KGuard = 2000000;

// is a signal already pending for aStatus? (it completed earlier; a correct kernel still holds its signal)
static TBool SignalKept(TRequestStatus& aStatus, RTimer& aGuard)
	{
	TRequestStatus g;
	aGuard.After(g, KGuard);
	User::WaitForRequest(aStatus, g);
	const TBool kept = (g == KRequestPending);
	if (kept)
		{
		aGuard.Cancel();
		User::WaitForRequest(g);
		}
	return kept;
	}

GLDEF_C TInt E32Main()
	{
	RTimer t1, t2, t3, guard;
	t1.CreateLocal(); t2.CreateLocal(); t3.CreateLocal(); guard.CreateLocal();
	TInt fails = 0;

	// 1: one absorbed signal
	TRequestStatus a, b;
	t1.After(a, 100000);
	t2.After(b, 500000);
	User::WaitForRequest(b);
	TBool ok = SignalKept(a, guard);
	RDebug::Printf("[sigtest] 1 WaitForRequest(b) with a done first: a's signal %s", ok ? "kept" : "LOST");
	fails += !ok;

	// 2: two absorbed signals
	TRequestStatus c, d, e;
	t1.After(c, 100000);
	t2.After(d, 200000);
	t3.After(e, 600000);
	User::WaitForRequest(e);
	TBool ok1 = SignalKept(c, guard);
	TBool ok2 = SignalKept(d, guard);
	RDebug::Printf("[sigtest] 2 two absorbed: c %s, d %s", ok1 ? "kept" : "LOST", ok2 ? "kept" : "LOST");
	fails += !ok1 + !ok2;

	// 3: the two-status WaitForRequest
	TRequestStatus f, g, h;
	t1.After(f, 100000);
	t2.After(g, 300000);
	t3.After(h, 800000);
	User::WaitForRequest(g, h);                 // f completes first and is absorbed; g ends the wait
	TBool ok3 = SignalKept(f, guard);
	TBool ok4 = SignalKept(h, guard);           // h completes later; its own signal arrives normally
	RDebug::Printf("[sigtest] 3 WaitForRequest(g, h): f %s, h %s", ok3 ? "kept" : "LOST", ok4 ? "arrived" : "LOST");
	fails += !ok3 + !ok4;

	// 4: a request the kernel completes IMMEDIATELY, inside the requesting thread's own exec call
	// (Kern::QueueRequestComplete to the current thread -> a user-mode callback on the way out). A new
	// change notifier holds every change bit, so its first Logon completes at once; BrowserNG's clock
	// handlers (RChangeNotifier after each ntpsync clock set) were always "ready" with no signal.
	for (TInt n = 0; n < 3; n++)
		{
		RChangeNotifier cn;
		cn.Create();
		TRequestStatus s;
		cn.Logon(s);
		const TInt st = s.Int();
		TBool ok5 = SignalKept(s, guard);
		RDebug::Printf("[sigtest] 4.%d immediate RChangeNotifier::Logon (status %d): signal %s", n, st, ok5 ? "arrived" : "LOST");
		fails += !ok5;
		cn.Close();
		}

	RDebug::Printf("[sigtest] %s (%d failure(s))", fails ? "FAIL" : "PASS", fails);
	guard.Close(); t3.Close(); t2.Close(); t1.Close();
	return fails;
	}
