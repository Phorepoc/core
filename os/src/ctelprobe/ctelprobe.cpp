// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
// ctelprobe: what CTelephony (the third-party telephony API QtMobility's systeminfo uses) says about the network, every 10 s
// for three minutes after the process starts (its requests complete through active objects: it runs a scheduler like a Qt app).
#include <e32base.h>
#include <e32debug.h>
#include <etel3rdparty.h>

class CWaiter : public CActive
	{
public:
	CWaiter(CActiveSchedulerWait& aWait) : CActive(EPriorityStandard), iWait(aWait) { CActiveScheduler::Add(this); }
	~CWaiter() { Cancel(); }
	TRequestStatus& Status() { return iStatus; }
	void Go() { SetActive(); iWait.Start(); }
	void RunL() { iWait.AsyncStop(); }
	void DoCancel() {}
private:
	CActiveSchedulerWait& iWait;
	};

static void RunL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	CTelephony* t = CTelephony::NewLC();
	CActiveSchedulerWait* wait = new (ELeave) CActiveSchedulerWait;
	CleanupStack::PushL(wait);
	CWaiter* w = new (ELeave) CWaiter(*wait);
	CleanupStack::PushL(w);
	static const TInt gaps[] = { 0, 1, 1, 1, 2, 5, 10, 10, 10 };
	for (TInt n = 0; n < 9; ++n)
		{
		CTelephony::TNetworkInfoV1 info;
		CTelephony::TNetworkInfoV1Pckg pckg(info);
		t->GetCurrentNetworkInfo(w->Status(), pckg);
		w->Go();
		TBuf<64> ln(info.iLongName.Left(60));
		RDebug::Print(_L("[ctelprobe] call %d GetCurrentNetworkInfo -> %d  mode %d long '%S'"), n, w->Status().Int(), (TInt)info.iMode, &ln);
		User::After(gaps[n] * 1000000);
		}
	CleanupStack::PopAndDestroy(3);
	CleanupStack::PopAndDestroy(t);
	CleanupStack::PopAndDestroy(sched);
	}

TInt E32Main()
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TRAPD(r, RunL());
	RDebug::Printf("[ctelprobe] exit %d", r);
	delete c;
	return r;
	}
