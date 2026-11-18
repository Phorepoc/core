// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheblesrv.cpp -- OURS (BLE plan): the "!PheBle" server (pheble.h) in pheblehost.dll, on its own thread with an
// active scheduler. NimBLE's task hands results over through locked queues and a kick (RThread::RequestComplete on
// the server thread's CKick); only the server thread touches client messages.
//   - scanning: one advert queue (the last scanner's), NextAdvert drains it;
//   - requests: each asynchronous message gets an id, goes to NimBLE's task (PheBleHostRequest) and is completed
//     by id (PheBleServerComplete) with its data and TPheBleResult written to the message's slots;
//   - link events: copied to every session's queue, NextEvent drains it.

#include <e32base.h>
#include <e32debug.h>
#include "pheblesrv.h"

namespace {

const TInt KMaxAdverts = 64;
const TInt KMaxSessionEvents = 64;

class CPheBleServer;
CPheBleServer* gServer = NULL;
RMutex gQLock;
TThreadId gServerThread;

struct TDone
	{
	TUint iId;
	TInt iError;
	TInt iAttError;
	HBufC8* iData;
	};

struct TEvt
	{
	TPheBleEvent iEvent;
	HBufC8* iValue;
	};

// shared with NimBLE's task, under gQLock
RArray<TPheBleAdvert>* gAdverts = NULL;
RArray<TDone>* gDone = NULL;
RArray<TEvt>* gEvents = NULL;
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

class CPheBleSession;

struct TPending
	{
	TUint iId;
	TInt iOp;
	RMessage2 iMsg;
	CPheBleSession* iSession;
	};

class CPheBleServer : public CServer2
	{
public:
	CPheBleServer() : CServer2(EPriorityStandard, ESharableSessions), iNextId(1) {}
	CSession2* NewSessionL(const TVersion& aVersion, const RMessage2&) const;
	void Drain();
	void Complete(const TDone& aDone);
	void DropSession(CPheBleSession* aSession);
	TUint iNextId;
	RArray<TPending> iPending;
	};

class CPheBleSession : public CSession2
	{
public:
	~CPheBleSession();
	void ServiceL(const RMessage2& aMessage);
	void DeliverAdverts();
	void DeliverEvents();
	void QueueEvent(const TPheBleEvent& aEvent, const TDesC8& aValue);
	TBool iScanning;
	RMessage2 iAdvertMsg;
	RMessage2 iEventMsg;
	RArray<TEvt> iEvents;
private:
	void Request(const RMessage2& aMessage, TOpReq& aReq, const TDesC8& aData);
	};

CSession2* CPheBleServer::NewSessionL(const TVersion& aVersion, const RMessage2&) const
	{
	if (!User::QueryVersionSupported(TVersion(KPheBleMajor, KPheBleMinor, 0), aVersion))
		User::Leave(KErrNotSupported);
	return new (ELeave) CPheBleSession;
	}

void CKick::RunL()
	{
	Arm();
	if (gServer)
		gServer->Drain();
	}

void CPheBleServer::Drain()
	{
	for (;;)
		{
		gQLock.Wait();
		TBool have = gDone && gDone->Count() > 0;
		TDone d;
		if (have)
			{
			d = (*gDone)[0];
			gDone->Remove(0);
			}
		gQLock.Signal();
		if (!have)
			break;
		Complete(d);
		delete d.iData;
		}
	for (;;)
		{
		gQLock.Wait();
		TBool have = gEvents && gEvents->Count() > 0;
		TEvt e;
		if (have)
			{
			e = (*gEvents)[0];
			gEvents->Remove(0);
			}
		gQLock.Signal();
		if (!have)
			break;
		iSessionIter.SetToFirst();
		while (CSession2* s = iSessionIter++)
			static_cast<CPheBleSession*>(s)->QueueEvent(e.iEvent, e.iValue ? static_cast<const TDesC8&>(*e.iValue) : KNullDesC8());
		delete e.iValue;
		}
	iSessionIter.SetToFirst();
	while (CSession2* s = iSessionIter++)
		{
		static_cast<CPheBleSession*>(s)->DeliverAdverts();
		static_cast<CPheBleSession*>(s)->DeliverEvents();
		}
	}

void CPheBleServer::Complete(const TDone& aDone)
	{
	TInt idx = KErrNotFound;
	for (TInt i = 0; i < iPending.Count(); ++i)
		if (iPending[i].iId == aDone.iId)
			idx = i;
	if (idx == KErrNotFound)
		return;      // the session went away
	TPending p = iPending[idx];
	iPending.Remove(idx);
	TPtrC8 data(aDone.iData ? static_cast<const TDesC8&>(*aDone.iData) : KNullDesC8());
	TInt err = aDone.iError;
	TPheBleResult result;
	result.iError = aDone.iError;
	result.iAttError = aDone.iAttError;
	TPckgC<TPheBleResult> res(result);
	switch (p.iOp)
		{
	case EPheBleConnect:
	case EPheBleDiscoverAll:
		if (err == KErrNone)
			err = p.iMsg.Write(1, data);    // DiscoverAll: KErrOverflow if the client's buffer is too small
		break;
	case EPheBleRead:
		err = KErrNone;
		if (aDone.iError == KErrNone && aDone.iAttError == 0)
			err = p.iMsg.Write(2, data);
		if (err == KErrNone)
			err = p.iMsg.Write(3, res);
		break;
	case EPheBlePair:
		// success completes with the TPheBlePairing method (>= 0)
		if (err == KErrNone && data.Length() == sizeof(TInt))
			err = *reinterpret_cast<const TInt*>(data.Ptr());
		break;
	case EPheBleWrite:
	case EPheBleSubscribe:
		// complete KErrNone once the result reached the client: the outcome is in TPheBleResult
		err = p.iMsg.Write(3, res);
		break;
	default:
		break;
		}
	p.iMsg.Complete(err);
	}

void CPheBleServer::DropSession(CPheBleSession* aSession)
	{
	for (TInt i = iPending.Count() - 1; i >= 0; --i)
		{
		if (iPending[i].iSession == aSession)
			{
			iPending[i].iMsg.Complete(KErrCancel);
			iPending.Remove(i);
			}
		}
	}

CPheBleSession::~CPheBleSession()
	{
	if (iScanning)
		PheBleHostScan(EFalse, EFalse, 0);
	if (!iAdvertMsg.IsNull())
		iAdvertMsg.Complete(KErrCancel);
	if (!iEventMsg.IsNull())
		iEventMsg.Complete(KErrCancel);
	for (TInt i = 0; i < iEvents.Count(); ++i)
		delete iEvents[i].iValue;
	iEvents.Close();
	if (gServer)
		gServer->DropSession(this);
	}

void CPheBleSession::QueueEvent(const TPheBleEvent& aEvent, const TDesC8& aValue)
	{
	if (iEvents.Count() >= KMaxSessionEvents)
		{
		delete iEvents[0].iValue;
		iEvents.Remove(0);
		}
	TEvt e;
	e.iEvent = aEvent;
	e.iValue = aValue.Length() ? aValue.Alloc() : NULL;
	if (iEvents.Append(e) != KErrNone)
		delete e.iValue;
	}

void CPheBleSession::DeliverEvents()
	{
	if (iEventMsg.IsNull() || iEvents.Count() == 0)
		return;
	TEvt e = iEvents[0];
	iEvents.Remove(0);
	TPckgC<TPheBleEvent> p(e.iEvent);
	TInt err = iEventMsg.Write(0, p);
	if (err == KErrNone && e.iValue)
		err = iEventMsg.Write(1, *e.iValue);
	iEventMsg.Complete(err);
	delete e.iValue;
	}

void CPheBleSession::DeliverAdverts()
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

void CPheBleSession::Request(const RMessage2& aMessage, TOpReq& aReq, const TDesC8& aData)
	{
	aReq.iOp = aMessage.Function();
	aReq.iId = gServer->iNextId++;
	TPending p;
	p.iId = aReq.iId;
	p.iOp = aReq.iOp;
	p.iMsg = aMessage;
	p.iSession = this;
	TInt err = gServer->iPending.Append(p);
	if (err == KErrNone)
		{
		err = PheBleHostRequest(aReq, aData);
		if (err != KErrNone)
			gServer->iPending.Remove(gServer->iPending.Count() - 1);
		}
	if (err != KErrNone)
		aMessage.Complete(err);
	}

void CPheBleSession::ServiceL(const RMessage2& aMessage)
	{
	TOpReq req;
	Mem::FillZ(&req, sizeof(req));
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
		DeliverAdverts();
		return;
	case EPheBleStopScan:
		iScanning = EFalse;
		aMessage.Complete(PheBleHostScan(EFalse, EFalse, 0));
		return;
	case EPheBleNextEvent:
		if (!iEventMsg.IsNull())
			{
			aMessage.Complete(KErrInUse);
			return;
			}
		iEventMsg = aMessage;
		DeliverEvents();
		return;
	case EPheBleConnect:
	case EPheBleDeleteBond:
		{
		TPckg<TPheBleAddr> addr(req.iAddr);
		aMessage.ReadL(0, addr);
		Request(aMessage, req, KNullDesC8);
		return;
		}
	case EPheBleDisconnect:
	case EPheBlePair:
	case EPheBleExchangeMtu:
	case EPheBleDiscoverAll:
		req.iConn = (TUint16)aMessage.Int0();
		Request(aMessage, req, KNullDesC8);
		return;
	case EPheBleConfirmNumeric:
		req.iConn = (TUint16)aMessage.Int0();
		req.iFlag = aMessage.Int1() != 0;
		Request(aMessage, req, KNullDesC8);
		return;
	case EPheBleRead:
		req.iConn = (TUint16)aMessage.Int0();
		req.iHandle = (TUint16)aMessage.Int1();
		Request(aMessage, req, KNullDesC8);
		return;
	case EPheBleWrite:
		{
		req.iConn = (TUint16)(aMessage.Int0() & 0xFFFF);
		req.iHandle = (TUint16)((TUint)aMessage.Int0() >> 16);
		req.iFlag = aMessage.Int2() != 0;
		TInt len = aMessage.GetDesLengthL(1);
		HBufC8* value = HBufC8::NewLC(len);
		TPtr8 p = value->Des();
		aMessage.ReadL(1, p);
		Request(aMessage, req, *value);
		CleanupStack::PopAndDestroy(value);
		return;
		}
	case EPheBleSubscribe:
		req.iConn = (TUint16)aMessage.Int0();
		req.iHandle = (TUint16)(aMessage.Int1() & 0xFFFF);
		req.iValue = (TUint16)((TUint)aMessage.Int1() >> 16);
		Request(aMessage, req, KNullDesC8);
		return;
	case EPheBleGetMtu:
		aMessage.Complete(PheBleHostMtu((TUint16)aMessage.Int0()));
		return;
	case EPheBleCancelAll:
		if (!iAdvertMsg.IsNull())
			iAdvertMsg.Complete(KErrCancel);
		if (!iEventMsg.IsNull())
			iEventMsg.Complete(KErrCancel);
		gServer->DropSession(this);
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
	gDone = new RArray<TDone>(8);
	gEvents = new RArray<TEvt>(8);
	if (!gAdverts || !gDone || !gEvents)
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

void PheBleServerComplete(TUint aId, TInt aError, TInt aAttError, const TDesC8& aData)
	{
	TDone d;
	d.iId = aId;
	d.iError = aError;
	d.iAttError = aAttError;
	d.iData = aData.Length() ? aData.Alloc() : NULL;
	if (aData.Length() && !d.iData)
		d.iError = KErrNoMemory;
	gQLock.Wait();
	if (!gDone || gDone->Append(d) != KErrNone)
		delete d.iData;
	Kick();
	gQLock.Signal();
	}

void PheBleServerEvent(const TPheBleEvent& aEvent, const TDesC8& aValue)
	{
	TEvt e;
	e.iEvent = aEvent;
	e.iValue = aValue.Length() ? aValue.Alloc() : NULL;
	gQLock.Wait();
	if (!gEvents || gEvents->Append(e) != KErrNone)
		delete e.iValue;
	Kick();
	gQLock.Signal();
	}

void PheBleServerHostReset()
	{
	PheBleServerScanDone(KErrPheBleNotReady);
	}
