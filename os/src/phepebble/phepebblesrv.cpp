// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebblesrv.cpp -- OURS: the Pebble companion service. Holds the LE link to one bonded Pebble (reversed PPoGATT), runs the
// Pebble Protocol session (version, clock, ping, notifications) and bridges phone events to the watch: a new SMS in the
// Inbox (Message Server) and a new missed call (the log engine's recent missed-calls list). Talks to the UI only through
// Publish & Subscribe (phepebble_ps.h). Logs ids and states, never message text, numbers or names.
//
// Pairing sequence (verified on a Pebble 2 HR, docs: pebble-client.md 0.7): connect, read the Pairing Service status; not
// bonded -> write Trigger {no security request, reversed PPoGATT} (the 4.x firmware answers ATT 0x01 but applies it) ->
// pair as initiator -> status shows bonded + reversed -> subscribe the reversed Data characteristic -> PPoGATT reset ->
// the watch asks for our version -> we answer and set the clock.

#include <e32base.h>
#include <e32property.h>
#include <e32math.h>
#include <e32debug.h>
#include <f32file.h>
#include <s32file.h>
#include <utf.h>
#include <tz.h>
#include <tzid.h>
#include <msvapi.h>
#include <msvids.h>
#include <msvuids.h>
#include <smut.h>
#include <logcli.h>
#include <logview.h>
#include <logwrap.h>
#include <logwrap.hrh>
#include <logfilterandeventconstants.hrh>

#include "phepebble_ps.h"
#include "phepebble_letransport.h"
#include "phepebble_ppogatt.h"
#include "phepebble_session.h"

#define LOG(...) RDebug::Printf("[phepebble] " __VA_ARGS__)

_LIT(KBondFile, "c:\\private\\e7017d01\\watch.dat");
static const TUint8 KScanPrefix[] = {'P', 'e', 'b', 'b', 'l', 'e'};
const TInt KScanSeconds = 15;
const TInt KReconnectSeconds = 30;

class CPebbleService;

// ---------------------------------------------------------------------------------------------------------------
// Small active objects

/** Calls back when a P&S key changes. */
class CPropertyWatcher : public CActive
	{
public:
	CPropertyWatcher(CPebbleService& aOwner) : CActive(EPriorityStandard), iOwner(aOwner) { CActiveScheduler::Add(this); }
	~CPropertyWatcher() { Cancel(); iProperty.Close(); }
	void StartL(TUid aCat, TUint32 aKey) { User::LeaveIfError(iProperty.Attach(aCat, aKey)); Watch(); }
private:
	void Watch() { iProperty.Subscribe(iStatus); SetActive(); }
	void RunL();
	void DoCancel() { iProperty.Cancel(); }
	CPebbleService& iOwner;
	RProperty iProperty;
	};

/** Missed calls: waits for log changes, then reads the newest entry of the recent missed-calls list. */
class CMissedCallWatcher : public CActive
	{
public:
	static CMissedCallWatcher* NewL(CPebbleService& aOwner, RFs& aFs);
	~CMissedCallWatcher();
private:
	CMissedCallWatcher(CPebbleService& aOwner) : CActive(EPriorityLow), iOwner(aOwner) { CActiveScheduler::Add(this); }
	void ConstructL(RFs& aFs);
	void Notify();
	void RunL();
	void DoCancel();
	TInt RunError(TInt aError) { LOG("missed-call watcher error %d", aError); Notify(); return KErrNone; }
	enum TStep { EWaitChange, EReadList };
	CPebbleService& iOwner;
	CLogClient* iClient;
	CLogViewRecent* iView;
	TStep iStep;
	TLogId iLastId;
	TBool iPrimed;
	};

/** New SMS in the Inbox (a Message Server session observer). */
class CSmsWatcher : public CBase, public MMsvSessionObserver
	{
public:
	static CSmsWatcher* NewL(CPebbleService& aOwner);
	~CSmsWatcher();
private:
	CSmsWatcher(CPebbleService& aOwner) : iOwner(aOwner) {}
	void HandleSessionEventL(TMsvSessionEvent aEvent, TAny* aArg1, TAny* aArg2, TAny* aArg3);
	void Check(TMsvId aId);
	CPebbleService& iOwner;
	CMsvSession* iSession;
	RArray<TMsvId> iSeen;
	};

// ---------------------------------------------------------------------------------------------------------------
// The service

class CPebbleService : public CBase, public MPebbleLeObserver, public MPPoGattObserver, public MPebbleLink,
	public MPebbleEnvironment, public MPebbleSessionObserver
	{
public:
	static CPebbleService* NewLC();
	~CPebbleService();
	void Command(TInt aValue);
	/** A phone event for the watch (UTF-16 in, converted here). */
	void Forward(const TDesC& aTitle, const TDesC& aBody);

private:
	CPebbleService();
	void ConstructL();
	void SetState(TPhePebbleState aState);
	void SetInfo(const TDesC& aText);
	void StartPair();
	void ConnectBonded();
	void Disconnected();
	void ScheduleReconnect();
	static TInt TickCb(TAny* aSelf);
	static TInt ReconnectCb(TAny* aSelf);
	TInt LoadBond();
	void SaveBondL();
	void ForgetBond();

	// MPebbleLeObserver
	void LeDeviceFound(const TPebbleLeAddr& aAddr, const TUint8* aName, TInt aNameLength, TInt aRssi);
	void LeScanStopped(TInt aError);
	void LeConnected(TInt aError);
	void LeDisconnected(TInt aReason);
	void LePaired(TInt aError, TPebblePairingMethod aMethod);
	void LeReadDone(TUint32 aChar, TInt aError, const TUint8* aValue, TInt aLength);
	void LeWriteDone(TUint32 aChar, TInt aError, TInt aAttError);
	void LeSubscribed(TUint32 aChar, TInt aError);
	void LeNotification(TUint32 aChar, const TUint8* aValue, TInt aLength);
	// MPPoGattObserver
	void PPoGattOpened();
	void PPoGattData(const TUint8* aData, TInt aLength);
	void PPoGattClosed(TInt aReason);
	// MPebbleLink
	TInt LinkSend(const TUint8* aData, TInt aLength);
	// MPebbleEnvironment
	TInt32 UtcNow();
	TInt16 UtcOffsetMinutes();
	TInt ZoneName(TUint8* aOut, TInt aMax);
	void Random(TUint8* aOut, TInt aLength);
	// MPebbleSessionObserver
	void SessionReady();
	void NotificationResult(TUint16 aToken, TUint8 aStatus);

	RFs iFs;
	MPebbleLeTransport* iLe;
	TPPoGattClient* iPPoGatt;
	TPebbleSession* iSession;
	CPeriodic* iTick;
	CPeriodic* iReconnect;
	CPropertyWatcher* iCommands;
	CSmsWatcher* iSms;
	CMissedCallWatcher* iCalls;
	TPhePebbleState iState;
	TPebbleLeAddr iBond;
	TBool iHaveBond;
	TBool iPairing;           // this connection should pair (a new watch)
	TBool iConnected;
	TInt iDelivered;
	TInt64 iSeed;
	TBuf<64> iWatchName;
	};

CPebbleService* CPebbleService::NewLC()
	{
	CPebbleService* self = new (ELeave) CPebbleService;
	CleanupStack::PushL(self);
	self->ConstructL();
	return self;
	}

CPebbleService::CPebbleService()
	{
	}

void CPebbleService::ConstructL()
	{
	User::LeaveIfError(iFs.Connect());
	iFs.CreatePrivatePath(EDriveC);
	TTime now;
	now.UniversalTime();
	iSeed = now.Int64();

	const TSecurityPolicy pass(TSecurityPolicy::EAlwaysPass);
	const TSecurityPolicy own(TSecureId(KPhePebbleSrvUid.iUid));   // only we write state/info
	RProperty::Define(KPhePebbleKeyState, RProperty::EInt, pass, own);
	RProperty::Define(KPhePebbleKeyInfo, RProperty::EText, pass, own);
	RProperty::Define(KPhePebbleKeyCommand, RProperty::EInt, pass, pass);
	RProperty::Define(KPhePebbleKeyDelivered, RProperty::EInt, pass, own);
	RProperty::Set(KPhePebbleSrvUid, KPhePebbleKeyDelivered, 0);

	iLe = PebbleCreateLeTransportL(*this);
	iPPoGatt = new (ELeave) TPPoGattClient(*iLe, *this);
	iSession = new (ELeave) TPebbleSession(*this, *this, *this);
	iTick = CPeriodic::NewL(CActive::EPriorityStandard);
	iTick->Start(KPPoGattTickMs * 1000, KPPoGattTickMs * 1000, TCallBack(TickCb, this));
	iReconnect = CPeriodic::NewL(CActive::EPriorityIdle);
	iCommands = new (ELeave) CPropertyWatcher(*this);
	iCommands->StartL(KPhePebbleSrvUid, KPhePebbleKeyCommand);

	// the bridges are optional: a missing Message Server or log engine must not stop the watch link
	TRAPD(r, iSms = CSmsWatcher::NewL(*this));
	LOG("SMS bridge: %d", r);
	TRAP(r, iCalls = CMissedCallWatcher::NewL(*this, iFs));
	LOG("missed-call bridge: %d", r);

	iHaveBond = LoadBond() == KErrNone;
	SetState(iHaveBond ? EPhePebbleIdle : EPhePebbleNoWatch);
	SetInfo(iHaveBond ? iWatchName : KNullDesC());
	if (iHaveBond)
		ConnectBonded();
	}

CPebbleService::~CPebbleService()
	{
	delete iCalls;
	delete iSms;
	delete iCommands;
	delete iReconnect;
	delete iTick;
	delete iSession;
	delete iPPoGatt;
	if (iLe)
		iLe->Disconnect();
	delete iLe;
	iFs.Close();
	}

void CPebbleService::SetState(TPhePebbleState aState)
	{
	if (aState != iState)
		LOG("state %d -> %d", iState, aState);
	iState = aState;
	RProperty::Set(KPhePebbleSrvUid, KPhePebbleKeyState, aState);
	}

void CPebbleService::SetInfo(const TDesC& aText)
	{
	RProperty::Set(KPhePebbleSrvUid, KPhePebbleKeyInfo, aText.Left(128));
	}

// ---- commands from the UI

void CPebbleService::Command(TInt aValue)
	{
	const TInt cmd = aValue & 0xFF;
	LOG("command %d", cmd);
	switch (cmd)
		{
	case EPhePebbleCmdPair:
		StartPair();
		break;
	case EPhePebbleCmdTest:
		if (iSession->IsReady())
			{
			_LIT(KTitle, "Phorepoc");
			_LIT(KBody, "Test notification from your phone");
			Forward(KTitle, KBody);
			}
		else
			SetInfo(_L("The watch is not connected"));
		break;
	case EPhePebbleCmdForget:
		ForgetBond();
		break;
	case EPhePebbleCmdReconnect:
		if (iHaveBond && !iConnected)
			ConnectBonded();
		break;
	default:
		break;
		}
	}

void CPebbleService::StartPair()
	{
	iReconnect->Cancel();
	if (iConnected)
		{
		iLe->Disconnect();
		iConnected = EFalse;
		}
	iPairing = ETrue;
	TInt r = iLe->StartScan(KScanPrefix, sizeof KScanPrefix, KScanSeconds);
	LOG("scan: %d", r);
	if (r == KErrNotSupported)
		{
		SetState(EPhePebbleNoBluetooth);
		SetInfo(_L("Bluetooth LE is not available"));
		return;
		}
	if (r != KErrNone)
		{
		SetInfo(_L("Could not search for a watch"));
		return;
		}
	SetState(EPhePebbleScanning);
	SetInfo(_L("Searching for a Pebble..."));
	}

void CPebbleService::ConnectBonded()
	{
	iPairing = EFalse;
	TInt r = iLe->Connect(iBond);
	LOG("connect bonded: %d", r);
	if (r == KErrNotSupported)
		{
		SetState(EPhePebbleNoBluetooth);
		return;
		}
	if (r != KErrNone)
		{
		ScheduleReconnect();
		return;
		}
	SetState(EPhePebbleConnecting);
	}

void CPebbleService::ScheduleReconnect()
	{
	iReconnect->Cancel();
	if (iHaveBond && iState != EPhePebbleNoBluetooth)
		iReconnect->Start(KReconnectSeconds * 1000000, KReconnectSeconds * 1000000, TCallBack(ReconnectCb, this));
	}

TInt CPebbleService::ReconnectCb(TAny* aSelf)
	{
	CPebbleService* self = static_cast<CPebbleService*>(aSelf);
	self->iReconnect->Cancel();
	if (!self->iConnected && self->iHaveBond)
		self->ConnectBonded();
	return KErrNone;
	}

TInt CPebbleService::TickCb(TAny* aSelf)
	{
	static_cast<CPebbleService*>(aSelf)->iPPoGatt->Tick();
	return KErrNone;
	}

// ---- bond file: address, type, name

TInt CPebbleService::LoadBond()
	{
	RFileReadStream in;
	TInt r = in.Open(iFs, KBondFile, EFileRead);
	if (r != KErrNone)
		return r;
	TRAP(r,
		{
		for (TInt i = 0; i < KPebbleAddrSize; ++i)
			iBond.iAddr[i] = in.ReadUint8L();
		iBond.iRandom = in.ReadUint8L();
		TInt n = in.ReadUint8L();
		if (n > iWatchName.MaxLength())
			User::Leave(KErrCorrupt);
		in.ReadL(iWatchName, n);
		});
	in.Close();
	return r;
	}

void CPebbleService::SaveBondL()
	{
	RFileWriteStream out;
	User::LeaveIfError(out.Replace(iFs, KBondFile, EFileWrite));
	CleanupClosePushL(out);
	for (TInt i = 0; i < KPebbleAddrSize; ++i)
		out.WriteUint8L(iBond.iAddr[i]);
	out.WriteUint8L(iBond.iRandom);
	out.WriteUint8L(iWatchName.Length());
	out.WriteL(iWatchName);
	out.CommitL();
	CleanupStack::PopAndDestroy(&out);
	}

void CPebbleService::ForgetBond()
	{
	iReconnect->Cancel();
	iPPoGatt->Stop();
	iSession->Reset();
	if (iConnected)
		iLe->Disconnect();
	iConnected = EFalse;
	iHaveBond = EFalse;
	iFs.Delete(KBondFile);
	iWatchName.Zero();
	SetState(EPhePebbleNoWatch);
	SetInfo(_L("Watch forgotten. Also remove the phone in the watch's Bluetooth settings."));
	}

// ---- LE events

void CPebbleService::LeDeviceFound(const TPebbleLeAddr& aAddr, const TUint8* aName, TInt aNameLength, TInt aRssi)
	{
	if (iState != EPhePebbleScanning)
		return;
	LOG("found a Pebble, rssi %d", aRssi);
	iLe->StopScan();
	iBond = aAddr;
	iWatchName.Copy(TPtrC8(aName, Min(aNameLength, iWatchName.MaxLength())));
	TInt r = iLe->Connect(aAddr);
	if (r != KErrNone)
		{
		LOG("connect: %d", r);
		SetInfo(_L("Could not connect to the watch"));
		SetState(iHaveBond ? EPhePebbleIdle : EPhePebbleNoWatch);
		return;
		}
	SetState(EPhePebbleConnecting);
	SetInfo(iWatchName);
	}

void CPebbleService::LeScanStopped(TInt aError)
	{
	LOG("scan stopped: %d", aError);
	if (iState == EPhePebbleScanning)
		{
		SetState(iHaveBond ? EPhePebbleIdle : EPhePebbleNoWatch);
		SetInfo(_L("No Pebble found. Is it in pairing mode?"));
		ScheduleReconnect();
		}
	}

void CPebbleService::LeConnected(TInt aError)
	{
	LOG("connected: %d", aError);
	if (aError != KErrNone)
		{
		iConnected = EFalse;
		SetState(iHaveBond ? EPhePebbleIdle : EPhePebbleNoWatch);
		ScheduleReconnect();
		return;
		}
	iConnected = ETrue;
	iReconnect->Cancel();
	SetState(iPairing ? EPhePebblePairing : EPhePebbleOpening);
	iLe->Read(KPebbleCharConnStatus);
	}

void CPebbleService::LeDisconnected(TInt aReason)
	{
	LOG("disconnected: %d", aReason);
	Disconnected();
	}

void CPebbleService::Disconnected()
	{
	iConnected = EFalse;
	iPPoGatt->Stop();
	iSession->Reset();
	SetState(iHaveBond ? EPhePebbleIdle : EPhePebbleNoWatch);
	ScheduleReconnect();
	}

void CPebbleService::LeReadDone(TUint32 aChar, TInt aError, const TUint8* aValue, TInt aLength)
	{
	if (aChar != KPebbleCharConnStatus)
		return;
	const TUint8 status = (aError == KErrNone && aLength >= 1) ? aValue[0] : 0;
	LOG("connectivity status %02x (err %d)", status, aError);
	const TBool ready = (status & KPebbleStatusBonded) && (status & KPebbleStatusReversed);
	if (ready && (status & KPebbleStatusEncrypted || !iPairing))
		{
		if (iPairing)
			{
			iPairing = EFalse;
			iHaveBond = ETrue;
			TRAPD(r, SaveBondL());
			LOG("bond saved: %d", r);
			}
		SetState(EPhePebbleOpening);
		TInt r = iPPoGatt->Start();
		if (r != KErrNone)
			LOG("PPoGATT start: %d", r);
		return;
		}
	if (!iPairing)
		{
		// bonded before but the watch lost it (or the reversed bit): only a new pairing helps
		SetInfo(_L("The watch forgot this phone: pair again"));
		iLe->Disconnect();
		return;
		}
	// a new watch: ask for the reversed PPoGATT service before pairing, no security request (we pair ourselves)
	const TUint8 trigger = KPebbleTriggerNoSecurityRequest | KPebbleTriggerReversedPPoGatt;
	iLe->Write(KPebbleCharTrigger, &trigger, 1, ETrue);
	}

void CPebbleService::LeWriteDone(TUint32 aChar, TInt aError, TInt aAttError)
	{
	if (aChar != KPebbleCharTrigger)
		return;
	// the 4.x firmware answers every trigger write with ATT 0x01 (Invalid Handle) and still applies it
	LOG("trigger written: %d, ATT %d", aError, aAttError);
	if (aError != KErrNone && aAttError == 0)
		{
		SetInfo(_L("Pairing failed"));
		iLe->Disconnect();
		return;
		}
	SetInfo(_L("Confirm the pairing on the watch"));
	iLe->Pair();
	}

void CPebbleService::LePaired(TInt aError, TPebblePairingMethod aMethod)
	{
	LOG("paired: %d, method %d", aError, aMethod);
	if (aError != KErrNone)
		{
		SetInfo(_L("Pairing failed"));
		iLe->Disconnect();
		return;
		}
	iLe->Read(KPebbleCharConnStatus);    // now bonded + encrypted + reversed
	}

void CPebbleService::LeSubscribed(TUint32 aChar, TInt aError)
	{
	if (aChar == KPebbleCharRevData)
		iPPoGatt->HandleSubscribed(aError);
	}

void CPebbleService::LeNotification(TUint32 aChar, const TUint8* aValue, TInt aLength)
	{
	if (aChar == KPebbleCharRevData)
		iPPoGatt->HandleNotification(aValue, aLength);
	}

// ---- PPoGATT and session

void CPebbleService::PPoGattOpened()
	{
	LOG("PPoGATT open, window %d", iPPoGatt->TxWindow());
	iSession->Reset();
	}

void CPebbleService::PPoGattData(const TUint8* aData, TInt aLength)
	{
	iSession->Receive(aData, aLength);
	}

void CPebbleService::PPoGattClosed(TInt aReason)
	{
	LOG("PPoGATT closed: %d", aReason);
	iSession->Reset();
	if (iConnected)
		{
		SetState(EPhePebbleOpening);
		if (aReason == KErrTimedOut)
			iLe->Disconnect();   // a fresh connection restarts the link cleanly
		}
	}

TInt CPebbleService::LinkSend(const TUint8* aData, TInt aLength)
	{
	return iPPoGatt->Send(aData, aLength);
	}

void CPebbleService::SessionReady()
	{
	LOG("session ready");
	SetState(EPhePebbleConnected);
	SetInfo(iWatchName);
	}

void CPebbleService::NotificationResult(TUint16 aToken, TUint8 aStatus)
	{
	LOG("notification %d: status %d", aToken, aStatus);
	if (aStatus == KPebbleBlobSuccess)
		RProperty::Set(KPhePebbleSrvUid, KPhePebbleKeyDelivered, ++iDelivered);
	}

void CPebbleService::Forward(const TDesC& aTitle, const TDesC& aBody)
	{
	if (!iSession->IsReady())
		{
		LOG("event not forwarded: no session");
		return;
		}
	TBuf8<TPebbleSession::KMaxTitle * 2> title;
	HBufC8* body = HBufC8::New(TPebbleSession::KMaxBody * 2);
	if (!body)
		return;
	TPtr8 b = body->Des();
	// UTF-16 -> UTF-8; a too-long text converts partly, which the session cuts on a character boundary
	CnvUtfConverter::ConvertFromUnicodeToUtf8(title, aTitle.Left(TPebbleSession::KMaxTitle));
	CnvUtfConverter::ConvertFromUnicodeToUtf8(b, aBody.Left(TPebbleSession::KMaxBody));
	TInt token = iSession->SendNotification(title.Ptr(), title.Length(), b.Ptr(), b.Length());
	LOG("notification sent: token %d", token);
	delete body;
	}

// ---- environment

TInt32 CPebbleService::UtcNow()
	{
	TTime now;
	now.UniversalTime();
	TTimeIntervalSeconds s;
	now.SecondsFrom(TTime(_L("19700000:000000.000000")), s);
	return s.Int();
	}

TInt16 CPebbleService::UtcOffsetMinutes()
	{
	return TInt16(User::UTCOffset().Int() / 60);
	}

TInt CPebbleService::ZoneName(TUint8* aOut, TInt aMax)
	{
	TInt n = 0;
	RTz tz;
	if (tz.Connect() == KErrNone)
		{
		CTzId* id = 0;
		TRAPD(r, id = tz.GetTimeZoneIdL());
		if (r == KErrNone && id)
			{
			const TDesC8& name = id->TimeZoneNameID();
			n = Min(name.Length(), aMax);
			Mem::Copy(aOut, name.Ptr(), n);
			}
		delete id;
		tz.Close();
		}
	return n;
	}

void CPebbleService::Random(TUint8* aOut, TInt aLength)
	{
	for (TInt i = 0; i < aLength; ++i)
		aOut[i] = TUint8(Math::Rand(iSeed) >> 8);
	}

// ---------------------------------------------------------------------------------------------------------------
// Watchers

void CPropertyWatcher::RunL()
	{
	TInt status = iStatus.Int();
	Watch();
	TInt value = 0;
	if (status == KErrNone && iProperty.Get(value) == KErrNone && value)
		iOwner.Command(value);
	}

CSmsWatcher* CSmsWatcher::NewL(CPebbleService& aOwner)
	{
	CSmsWatcher* self = new (ELeave) CSmsWatcher(aOwner);
	CleanupStack::PushL(self);
	self->iSession = CMsvSession::OpenSyncL(*self);
	CleanupStack::Pop(self);
	return self;
	}

CSmsWatcher::~CSmsWatcher()
	{
	delete iSession;
	iSeen.Close();
	}

void CSmsWatcher::HandleSessionEventL(TMsvSessionEvent aEvent, TAny* aArg1, TAny* aArg2, TAny* /*aArg3*/)
	{
	if (aEvent != EMsvEntriesCreated && aEvent != EMsvEntriesChanged)
		return;
	if (!aArg2 || *static_cast<TMsvId*>(aArg2) != KMsvGlobalInBoxIndexEntryId)
		return;
	const CMsvEntrySelection* sel = static_cast<CMsvEntrySelection*>(aArg1);
	for (TInt i = 0; sel && i < sel->Count(); ++i)
		Check(sel->At(i));
	}

void CSmsWatcher::Check(TMsvId aId)
	{
	if (iSeen.Find(aId) >= 0)
		return;
	TMsvId service;
	TMsvEntry entry;
	if (iSession->GetEntry(aId, service, entry) != KErrNone)
		return;
	// a finished, unread SMS: the SMS watcher creates it in preparation and completes it when the body is stored
	if (entry.iMtm != KUidMsgTypeSMS || entry.iType != KUidMsvMessageEntry || entry.InPreparation() || !entry.Complete() ||
		!entry.Unread())
		return;
	iSeen.Append(aId);
	if (iSeen.Count() > 64)
		iSeen.Remove(0);
	RDebug::Printf("[phepebble] new SMS, entry %d", (TInt)aId);
	// iDetails = the sender (contact name or number), iDescription = the start of the text
	iOwner.Forward(entry.iDetails, entry.iDescription);
	}

CMissedCallWatcher* CMissedCallWatcher::NewL(CPebbleService& aOwner, RFs& aFs)
	{
	CMissedCallWatcher* self = new (ELeave) CMissedCallWatcher(aOwner);
	CleanupStack::PushL(self);
	self->ConstructL(aFs);
	CleanupStack::Pop(self);
	return self;
	}

void CMissedCallWatcher::ConstructL(RFs& aFs)
	{
	iClient = CLogClient::NewL(aFs);
	iView = CLogViewRecent::NewL(*iClient);
	// read the list once to learn the newest id (old missed calls are not forwarded)
	iStep = EReadList;
	if (iView->SetRecentListL(KLogRecentMissedCalls, iStatus))
		SetActive();
	else
		{
		iPrimed = ETrue;
		Notify();
		}
	}

CMissedCallWatcher::~CMissedCallWatcher()
	{
	Cancel();
	delete iView;
	delete iClient;
	}

void CMissedCallWatcher::Notify()
	{
	iStep = EWaitChange;
	iClient->NotifyChange(1000000, iStatus);
	SetActive();
	}

void CMissedCallWatcher::DoCancel()
	{
	if (iStep == EWaitChange)
		iClient->NotifyChangeCancel();
	else
		iView->Cancel();
	}

void CMissedCallWatcher::RunL()
	{
	if (iStep == EWaitChange)
		{
		if (iStatus.Int() < KErrNone)
			{
			Notify();
			return;
			}
		iStep = EReadList;
		if (iView->SetRecentListL(KLogRecentMissedCalls, iStatus))
			{
			SetActive();
			return;
			}
		Notify();             // the list is empty
		return;
		}
	// EReadList: the view is at its first (newest) event
	if (iStatus.Int() == KErrNone && iView->CountL() > 0)
		{
		const CLogEvent& ev = iView->Event();
		if (!iPrimed)
			iLastId = ev.Id();
		else if (ev.Id() > iLastId)
			{
			iLastId = ev.Id();
			RDebug::Printf("[phepebble] missed call, log id %d", (TInt)ev.Id());
			_LIT(KMissed, "Missed call");
			iOwner.Forward(KMissed, ev.RemoteParty().Length() ? ev.RemoteParty() : ev.Number());
			}
		}
	iPrimed = ETrue;
	Notify();
	}

// ---------------------------------------------------------------------------------------------------------------

static void RunL()
	{
	CActiveScheduler* s = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(s);
	CActiveScheduler::Install(s);
	CPebbleService* service = CPebbleService::NewLC();
	RProcess::Rendezvous(KErrNone);
	CActiveScheduler::Start();
	CleanupStack::PopAndDestroy(service);
	CleanupStack::PopAndDestroy(s);
	}

TInt E32Main()
	{
	// one instance only
	TFindProcess find(KPhePebbleSrvMatch);
	TFullName name;
	TInt count = 0;
	while (find.Next(name) == KErrNone)
		{
		RProcess p;
		if (p.Open(find) == KErrNone)
			{
			if (p.ExitType() == EExitPending)
				++count;
			p.Close();
			}
		}
	if (count > 1)
		return KErrAlreadyExists;
	User::RenameThread(_L("phepebblesrv"));
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(r, RunL());
	if (r != KErrNone)
		LOG("leave %d", r);
	delete cleanup;
	return r;
	}
