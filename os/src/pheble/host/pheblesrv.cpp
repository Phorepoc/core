// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheblesrv.cpp -- OURS (BLE plan rung 4): the "!PheBle" server (pheble.h) in pheblehost.dll, on its own thread with
// an active scheduler. NimBLE's task hands results over through a locked queue and a kick (RThread::RequestComplete
// on the server thread's CKick); only the server thread completes client messages.
// Rung 4: scanning. The other requests answer KErrNotSupported until the GATT/SMP rungs (README.md).

#include <e32base.h>
#include <e32debug.h>
#include "pheblesrv.h"

namespace {

const TInt KMaxAdverts = 64;

class CPheBleServer;
CPheBleServer* gServer = NULL;
RMutex gQLock;
TThreadId gServerThread;

// shared with NimBLE's task, under gQLock
RArray<TPheBleAdvert>* gAdverts = NULL;
TInt gScanDone = KRequestPending;
TBool gKicked = EFalse;
TRequestStatus* gKickStatus = NULL;

class CKick : public CActive
	{
public:
	CKick() : CActive(EPriorityStandard) { CActiveScheduler::Add(this); }
	~CKick() { Cancel(); }
	void Arm()
		{
		iStatus = KRequestPending;
		SetActive();
		gQLock.Wait();
		gKickStatus = &iStatus;
		gKicked = EFalse;
		gQLock.Signal();
		}
private:
	void RunL();
	void DoCancel()
		{
		gQLock.Wait();
		TBool pending = !gKicked;
		gKickStatus = NULL;
		gQLock.Signal();
		if (pending)
			{
			TRequestStatus* s = &iStatus;
			User::RequestComplete(s, KErrCancel);
			}
		}
	};

void Kick()
	{
	// caller holds gQLock
	if (gKicked || !gKickStatus)
		return;
	gKicked = ETrue;
	RThread t;
	if (t.Open(gServerThread) == KErrNone)
		{
		TRequestStatus* s = gKickStatus;
		t.RequestComplete(s, KErrNone);
		t.Close();
		}
	}

class CPheBleSession : public CSession2
	{
public:
	~CPheBleSession();
	void ServiceL(const RMessage2& aMessage);
	void Deliver();
	TBool iScanning;
	RMessage2 iAdvertMsg;
	};

class CPheBleServer : public CServer2
	{
public:
	CPheBleServer() : CServer2(EPriorityStandard, ESharableSessions) {}
	CSession2* NewSessionL(const TVersion& aVersion, const RMessage2&) const
		{
		if (!User::QueryVersionSupported(TVersion(KPheBleMajor, KPheBleMinor, 0), aVersion))
			User::Leave(KErrNotSupported);
		return new (ELeave) CPheBleSession;
		}
	void DeliverAll()
		{
		iSessionIter.SetToFirst();
		while (CSession2* s = iSessionIter++)
			static_cast<CPheBleSession*>(s)->Deliver();
		}
	};

void CKick::RunL()
	{
	Arm();
	if (gServer)
		gServer->DeliverAll();
	}

CPheBleSession::~CPheBleSession()
	{
	if (iScanning)
		PheBleHostScan(EFalse, EFalse, 0);
	if (!iAdvertMsg.IsNull())
		iAdvertMsg.Complete(KErrCancel);
	}

void CPheBleSession::Deliver()
	{
	if (iAdvertMsg.IsNull())
		return;
	gQLock.Wait();
	TBool have = gAdverts && gAdverts->Count() > 0;
	TPheBleAdvert adv;
	if (have)
		{
		adv = (*gAdverts)[0];
		gAdverts->Remove(0);
		}
	TInt done = gScanDone;
	gQLock.Signal();
	if (have)
		{
		TPckgC<TPheBleAdvert> p(adv);
		TInt err = iAdvertMsg.Write(0, p);
		iAdvertMsg.Complete(err);
		}
	else if (done != KRequestPending)
		{
		iScanning = EFalse;
		iAdvertMsg.Complete(done == 0 ? KErrEof : done);
		}
	}

void CPheBleSession::ServiceL(const RMessage2& aMessage)
	{
	switch (aMessage.Function())
		{
	case EPheBleStartScan:
		{
		gQLock.Wait();
		if (gAdverts)
			gAdverts->Reset();
		gScanDone = KRequestPending;
		gQLock.Signal();
		TInt err = PheBleHostScan(ETrue, aMessage.Int0() != 0, aMessage.Int1() * 1000);
		iScanning = err == KErrNone;
		aMessage.Complete(err);
		return;
		}
	case EPheBleNextAdvert:
		if (!iAdvertMsg.IsNull())
			{
			aMessage.Complete(KErrInUse);
			return;
			}
		iAdvertMsg = aMessage;
		Deliver();
		return;
	case EPheBleStopScan:
		iScanning = EFalse;
		aMessage.Complete(PheBleHostScan(EFalse, EFalse, 0));
		return;
	case EPheBleMtu:
		aMessage.Complete(23);
		return;
	case EPheBleCancelAll:
		if (!iAdvertMsg.IsNull())
			iAdvertMsg.Complete(KErrCancel);
		aMessage.Complete(KErrNone);
		return;
	default:
		aMessage.Complete(KErrNotSupported);
		return;
		}
	}

TInt ServerThread(TAny*)
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	CActiveScheduler* sched = new CActiveScheduler;
	if (!cleanup || !sched)
		return KErrNoMemory;
	CActiveScheduler::Install(sched);
	TRAPD(err,
		gServer = new (ELeave) CPheBleServer;
		gServer->StartL(KPheBleServerName);
		CKick* kick = new (ELeave) CKick;
		kick->Arm());
	RDebug::Printf("pheblesrv: server start %d", err);
	if (err == KErrNone)
		CActiveScheduler::Start();
	delete sched;
	delete cleanup;
	return err;
	}

} // namespace

TInt PheBleServerStart()
	{
	TInt err = gQLock.CreateLocal(EOwnerProcess);
	if (err != KErrNone)
		return err;
	gAdverts = new RArray<TPheBleAdvert>(16);
	if (!gAdverts)
		return KErrNoMemory;
	RThread t;
	err = t.Create(_L("PheBleSrv"), ServerThread, 0x4000, NULL, NULL, EOwnerProcess);
	if (err != KErrNone)
		return err;
	gServerThread = t.Id();
	t.Resume();
	t.Close();
	return KErrNone;
	}

void PheBleServerAdvert(const TPheBleAdvert& aAdvert)
	{
	gQLock.Wait();
	if (gAdverts)
		{
		if (gAdverts->Count() >= KMaxAdverts)
			gAdverts->Remove(0);
		gAdverts->Append(aAdvert);
		}
	Kick();
	gQLock.Signal();
	}

void PheBleServerScanDone(TInt aReason)
	{
	gQLock.Wait();
	gScanDone = aReason;
	Kick();
	gQLock.Signal();
	}

void PheBleServerHostReset()
	{
	PheBleServerScanDone(KErrPheBleNotReady);
	}
