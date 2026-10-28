// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheworker.cpp -- OURS: see pheworker.h
#include "pheworker.h"
#include "pheget.h"
#include <e32debug.h>

static TInt gJobCount;

class TJobLog : public MPheGetObserver
	{
public:
	void PheGetLog(const TDesC8& aLine) { RDebug::Printf("PH: [phestore] job: %S", &aLine); }
	void PheGetProgress(TInt, TInt) {}
	};

CPheJob* CPheJob::NewL(MPheJobObserver& aObserver)
	{
	CPheJob* self = new (ELeave) CPheJob(aObserver);
	CActiveScheduler::Add(self);
	return self;
	}

CPheJob::~CPheJob()
	{
	Cancel();
	iThread.Close();
	}

void CPheJob::DoCancel()
	{
	// a job cannot be interrupted half-way: end the thread (it only writes temporary files); the logon then
	// completes with the reason
	iThread.Kill(KErrCancel);
	}

TInt CPheJob::Start(TInt aId, TKind aKind, const TDesC& aLocation, const TDesC& aFile)
	{
	if (IsActive())
		return KErrInUse;
	iId = aId;
	iKind = aKind;
	iLocation = aLocation.Left(iLocation.MaxLength());
	iFile = aFile.Left(iFile.MaxLength());
	TBuf<24> name;
	name.Format(_L("phejob%d"), ++gJobCount);
	TInt r = iThread.Create(name, Entry, 0x10000, 0x10000, 0x800000, this, EOwnerProcess);
	if (r != KErrNone)
		return r;
	iThread.SetPriority(EPriorityLess);              // below the UI thread
	iStatus = KRequestPending;
	iThread.Logon(iStatus);
	iThread.Resume();
	SetActive();
	return KErrNone;
	}

void CPheJob::RunL()
	{
	TInt result = iThread.ExitType() == EExitKill ? iThread.ExitReason() : KErrDied;
	iThread.Close();
	if (iStatus.Int() == KErrCancel)
		result = KErrCancel;
	iObserver.JobDone(iId, result);
	}

TInt CPheJob::Entry(TAny* aSelf)
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		return KErrNoMemory;
	CActiveScheduler* scheduler = new CActiveScheduler;
	TInt result = KErrNoMemory;
	if (scheduler)
		{
		CActiveScheduler::Install(scheduler);
		TRAPD(err, result = static_cast<CPheJob*>(aSelf)->Run());
		if (err != KErrNone)
			result = err;
		}
	delete scheduler;
	delete cleanup;
	return result;
	}

TInt CPheJob::Run()
	{
	TJobLog log;
	CPheGet* get = CPheGet::NewL(log);
	CleanupStack::PushL(get);
	TInt result;
	if (iKind == ERefresh)
		result = get->RefreshIndexesL(KNullDesC, EFalse) ? 1 : 0;
	else
		result = get->FetchFileL(iLocation, iFile);
	CleanupStack::PopAndDestroy(get);
	return result;
	}
