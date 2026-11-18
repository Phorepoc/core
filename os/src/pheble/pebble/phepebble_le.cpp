// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// phepebble_le.cpp -- OURS (BLE plan, Pebble P2-P4): the Pebble companion's LE transport (phepebble_transport.h)
// over the Phorepoc LE host (RPheBle, pheble.h). Built as the static library phepebble_le.lib that phepebblesrv
// links instead of phepebble_lestub.cpp. Every RPheBle request is an active object of this object, so every
// MPebbleLeObserver callback arrives on the creating thread.
//
//   Connect:  ConnectDevice (the host encrypts a bonded link before it completes) -> DiscoverAll -> the Pebble
//             characteristics (XXXXXXXX-328E-0FBB-C642-1AA6699BDADA) mapped by short id to value + CCCD handles
//             -> LeConnected.
//   Write:    LeWriteDone(char, local error, ATT error) -- the Pebble 2 answers its Trigger write with ATT 0x01.

#include <e32base.h>
#include <e32debug.h>
#include "pheble.h"
#include "phepebble_letransport.h"

namespace {

// the Pebble UUID base, 128-bit little-endian bytes 0..11 (bytes 12..15 = the short id, little-endian)
const TUint8 KPebbleBase[12] = { 0xDA, 0xDA, 0x9B, 0x69, 0xA6, 0x1A, 0x42, 0xC6, 0xBB, 0x0F, 0x8E, 0x32 };
const TInt KMaxEntries = 160;
const TInt KMaxChars = 24;
const TInt KMaxValue = 512;

struct TCharMap
	{
	TUint32 iShortId;
	TUint16 iValue;
	TUint16 iCccd;
	};

class CPebbleLe;

/** One outstanding RPheBle request; RunL hands the status to the owner by kind. */
class CReq : public CActive
	{
public:
	enum TKind { EScan, EConnect, EDiscover, EEvent, EPair, ERead, EWrite, ESubscribe };
	CReq(CPebbleLe& aOwner, TKind aKind) : CActive(EPriorityStandard), iOwner(aOwner), iKind(aKind)
		{
		CActiveScheduler::Add(this);
		}
	~CReq() { Cancel(); }
	void Go() { SetActive(); }
	TRequestStatus& Status() { return iStatus; }
	TUint32 iChar;
private:
	void RunL();
	void DoCancel();
	CPebbleLe& iOwner;
	TKind iKind;
	};

class CPebbleLe : public CBase, public MPebbleLeTransport
	{
public:
	static CPebbleLe* NewL(MPebbleLeObserver& aObserver);
	~CPebbleLe();

	// MPebbleLeTransport
	TInt StartScan(const TUint8* aNamePrefix, TInt aPrefixLength, TInt aTimeoutSeconds);
	void StopScan();
	TInt Connect(const TPebbleLeAddr& aAddr);
	void Disconnect();
	TInt Pair();
	TInt Read(TUint32 aChar);
	TInt Write(TUint32 aChar, const TUint8* aValue, TInt aLength, TBool aWithResponse);
	TInt Subscribe(TUint32 aChar, TBool aNotifications);
	TInt Mtu() const;

	void Done(CReq::TKind aKind, TInt aStatus, CReq& aReq);
	void CancelReq(CReq::TKind aKind);

private:
	CPebbleLe(MPebbleLeObserver& aObserver)
		: iObserver(aObserver), iConn(0xFFFF), iEntriesPtr(NULL, 0), iEventValuePtr(NULL, 0), iReadValuePtr(NULL, 0) {}
	void ConstructL();
	void NextAdvert();
	void NextEvent();
	void BuildMap();
	const TCharMap* Find(TUint32 aShortId) const;
	TUint32 ShortIdOf(TUint16 aValueHandle) const;
	void LinkDown();

	MPebbleLeObserver& iObserver;
	RPheBle iBle;
	TUint16 iConn;
	TBool iConnected;

	CReq* iScan;
	CReq* iConnect;
	CReq* iDiscover;
	CReq* iEvent;
	CReq* iPair;
	CReq* iRead;
	CReq* iWrite;
	CReq* iSubscribe;

	TBuf8<32> iPrefix;
	TPheBleAdvertBuf iAdvert;
	TPheBleConnBuf iConnBuf;
	HBufC8* iEntries;
	TPtr8 iEntriesPtr;
	TPheBleEventBuf iEventBuf;
	HBufC8* iEventValue;
	TPtr8 iEventValuePtr;
	HBufC8* iReadValue;
	TPtr8 iReadValuePtr;
	TPheBleResultBuf iReadResult;
	HBufC8* iWriteValue;
	TPheBleResultBuf iWriteResult;
	TPheBleResultBuf iSubResult;

	TCharMap iMap[KMaxChars];
	TInt iMapCount;
	};

void CReq::RunL()
	{
	iOwner.Done(iKind, iStatus.Int(), *this);
	}

void CReq::DoCancel()
	{
	iOwner.CancelReq(iKind);
	}

CPebbleLe* CPebbleLe::NewL(MPebbleLeObserver& aObserver)
	{
	CPebbleLe* self = new (ELeave) CPebbleLe(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CPebbleLe::ConstructL()
	{
	User::LeaveIfError(iBle.Connect());
	iScan = new (ELeave) CReq(*this, CReq::EScan);
	iConnect = new (ELeave) CReq(*this, CReq::EConnect);
	iDiscover = new (ELeave) CReq(*this, CReq::EDiscover);
	iEvent = new (ELeave) CReq(*this, CReq::EEvent);
	iPair = new (ELeave) CReq(*this, CReq::EPair);
	iRead = new (ELeave) CReq(*this, CReq::ERead);
	iWrite = new (ELeave) CReq(*this, CReq::EWrite);
	iSubscribe = new (ELeave) CReq(*this, CReq::ESubscribe);
	iEntries = HBufC8::NewL(KMaxEntries * sizeof(TPheBleGattEntry));
	iEntriesPtr.Set(iEntries->Des());
	iEventValue = HBufC8::NewL(KMaxValue);
	iEventValuePtr.Set(iEventValue->Des());
	iReadValue = HBufC8::NewL(KMaxValue);
	iReadValuePtr.Set(iReadValue->Des());
	NextEvent();
	}

CPebbleLe::~CPebbleLe()
	{
	// requests first (their DoCancel uses the session)
	delete iScan;
	delete iConnect;
	delete iDiscover;
	delete iEvent;
	delete iPair;
	delete iRead;
	delete iWrite;
	delete iSubscribe;
	if (iConnected)
		iBle.Disconnect(iConn);
	iBle.Close();
	delete iEntries;
	delete iEventValue;
	delete iReadValue;
	delete iWriteValue;
	}

void CPebbleLe::CancelReq(CReq::TKind aKind)
	{
	// the server has no per-request cancel: CancelAll ends every request of this session; the event and scan loops
	// are re-armed by the caller where needed
	(void)aKind;
	iBle.CancelAll();
	}

// ---------------------------------------------------------------- scanning

TInt CPebbleLe::StartScan(const TUint8* aNamePrefix, TInt aPrefixLength, TInt aTimeoutSeconds)
	{
	if (iScan->IsActive())
		return KErrInUse;
	iPrefix.Copy(TPtrC8(aNamePrefix, aPrefixLength > 32 ? 32 : aPrefixLength));
	TInt err = iBle.StartScan(ETrue, aTimeoutSeconds);
	if (err == KErrNone)
		NextAdvert();
	return err;
	}

void CPebbleLe::StopScan()
	{
	iBle.StopScan();      // the pending NextAdvert completes with the scan's end
	}

void CPebbleLe::NextAdvert()
	{
	iBle.NextAdvert(iAdvert, iScan->Status());
	iScan->Go();
	}

// ---------------------------------------------------------------- link

TInt CPebbleLe::Connect(const TPebbleLeAddr& aAddr)
	{
	if (iConnect->IsActive() || iDiscover->IsActive() || iConnected)
		return KErrInUse;
	TPheBleAddr addr;
	Mem::Copy(addr.iAddr, aAddr.iAddr, 6);
	addr.iType = aAddr.iRandom;
	iBle.ConnectDevice(addr, iConnBuf, iConnect->Status());
	iConnect->Go();
	return KErrNone;
	}

void CPebbleLe::Disconnect()
	{
	if (iConnected || iConnect->IsActive())
		iBle.Disconnect(iConnected ? iConn : iConnBuf());
	}

void CPebbleLe::LinkDown()
	{
	iConnected = EFalse;
	iConn = 0xFFFF;
	iMapCount = 0;
	}

TInt CPebbleLe::Pair()
	{
	if (!iConnected)
		return KErrNotReady;
	if (iPair->IsActive())
		return KErrInUse;
	iBle.Pair(iConn, iPair->Status());
	iPair->Go();
	return KErrNone;
	}

TInt CPebbleLe::Mtu() const
	{
	if (!iConnected)
		return 23;
	TInt mtu = const_cast<RPheBle&>(iBle).Mtu(iConn);
	return mtu > 0 ? mtu : 23;
	}

void CPebbleLe::NextEvent()
	{
	iEventValuePtr.Zero();
	iBle.NextEvent(iEventBuf, iEventValuePtr, iEvent->Status());
	iEvent->Go();
	}

// ---------------------------------------------------------------- GATT

void CPebbleLe::BuildMap()
	{
	iMapCount = 0;
	const TPheBleGattEntry* e = reinterpret_cast<const TPheBleGattEntry*>(iEntries->Ptr());
	TInt n = iEntries->Length() / sizeof(TPheBleGattEntry);
	TCharMap* current = NULL;
	for (TInt i = 0; i < n; ++i)
		{
		if (e[i].iKind == TPheBleGattEntry::ECharacteristic)
			{
			current = NULL;
			if (Mem::Compare(e[i].iUuid, 12, KPebbleBase, 12) == 0 && iMapCount < KMaxChars)
				{
				current = &iMap[iMapCount++];
				current->iShortId = e[i].iUuid[12] | (e[i].iUuid[13] << 8) | (e[i].iUuid[14] << 16)
					| ((TUint32)e[i].iUuid[15] << 24);
				current->iValue = e[i].iHandle;
				current->iCccd = 0;
				}
			}
		else if (e[i].iKind == TPheBleGattEntry::EDescriptor)
			{
			// CCCD = 0x2902 on the Bluetooth base
			if (current && e[i].iUuid[12] == 0x02 && e[i].iUuid[13] == 0x29 && e[i].iUuid[14] == 0 && e[i].iUuid[15] == 0
					&& e[i].iUuid[0] == 0xFB && e[i].iUuid[1] == 0x34)
				current->iCccd = e[i].iHandle;
			}
		else
			current = NULL;
		}
	RDebug::Printf("phepebble_le: %d Pebble characteristics mapped", iMapCount);
	}

const TCharMap* CPebbleLe::Find(TUint32 aShortId) const
	{
	for (TInt i = 0; i < iMapCount; ++i)
		if (iMap[i].iShortId == aShortId)
			return &iMap[i];
	return NULL;
	}

TUint32 CPebbleLe::ShortIdOf(TUint16 aValueHandle) const
	{
	for (TInt i = 0; i < iMapCount; ++i)
		if (iMap[i].iValue == aValueHandle)
			return iMap[i].iShortId;
	return 0;
	}

TInt CPebbleLe::Read(TUint32 aChar)
	{
	const TCharMap* c = Find(aChar);
	if (!iConnected || !c)
		return iConnected ? KErrNotFound : KErrNotReady;
	if (iRead->IsActive())
		return KErrInUse;
	iRead->iChar = aChar;
	iReadValuePtr.Zero();
	iBle.Read(iConn, c->iValue, iReadValuePtr, iReadResult, iRead->Status());
	iRead->Go();
	return KErrNone;
	}

TInt CPebbleLe::Write(TUint32 aChar, const TUint8* aValue, TInt aLength, TBool aWithResponse)
	{
	const TCharMap* c = Find(aChar);
	if (!iConnected || !c)
		return iConnected ? KErrNotFound : KErrNotReady;
	if (iWrite->IsActive())
		return KErrInUse;
	delete iWriteValue;
	iWriteValue = TPtrC8(aValue, aLength).Alloc();
	if (!iWriteValue)
		return KErrNoMemory;
	iWrite->iChar = aChar;
	iBle.Write(iConn, c->iValue, *iWriteValue, aWithResponse, iWriteResult, iWrite->Status());
	iWrite->Go();
	return KErrNone;
	}

TInt CPebbleLe::Subscribe(TUint32 aChar, TBool aNotifications)
	{
	const TCharMap* c = Find(aChar);
	if (!iConnected || !c)
		return iConnected ? KErrNotFound : KErrNotReady;
	if (!c->iCccd)
		return KErrNotSupported;
	if (iSubscribe->IsActive())
		return KErrInUse;
	iSubscribe->iChar = aChar;
	iBle.Subscribe(iConn, c->iCccd, aNotifications ? 1 : 0, iSubResult, iSubscribe->Status());
	iSubscribe->Go();
	return KErrNone;
	}

// ---------------------------------------------------------------- completions (creating thread)

void CPebbleLe::Done(CReq::TKind aKind, TInt aStatus, CReq& aReq)
	{
	switch (aKind)
		{
	case CReq::EScan:
		if (aStatus == KErrNone)
			{
			const TPheBleAdvert& a = iAdvert();
			if (a.iName.Length() && a.iName.Left(iPrefix.Length()) == iPrefix)
				{
				TPebbleLeAddr addr;
				Mem::Copy(addr.iAddr, a.iAddr.iAddr, 6);
				addr.iRandom = a.iAddr.iType;
				iObserver.LeDeviceFound(addr, a.iName.Ptr(), a.iName.Length(), a.iRssi);
				}
			NextAdvert();
			}
		else
			iObserver.LeScanStopped(aStatus == KErrEof ? KErrNone : aStatus);
		break;

	case CReq::EConnect:
		if (aStatus != KErrNone)
			{
			iObserver.LeConnected(aStatus);
			break;
			}
		iConn = iConnBuf();
		iEntriesPtr.Zero();
		iBle.DiscoverAll(iConn, iEntriesPtr, iDiscover->Status());
		iDiscover->Go();
		break;

	case CReq::EDiscover:
		if (aStatus != KErrNone)
			{
			iBle.Disconnect(iConn);
			LinkDown();
			iObserver.LeConnected(aStatus);
			break;
			}
		BuildMap();
		iConnected = ETrue;
		iObserver.LeConnected(KErrNone);
		break;

	case CReq::EEvent:
		{
		if (aStatus != KErrNone)
			{
			if (aStatus != KErrCancel)
				NextEvent();
			break;
			}
		TPheBleEvent ev = iEventBuf();
		TBool ours = ev.iConn == iConn;
		if (ev.iType == EPheBleDisconnected && ours)
			{
			LinkDown();
			iObserver.LeDisconnected(ev.iReason);
			}
		else if (ev.iType == EPheBleNotification && ours && iConnected)
			{
			TUint32 id = ShortIdOf(ev.iHandle);
			if (id)
				iObserver.LeNotification(id, iEventValuePtr.Ptr(), iEventValuePtr.Length());
			}
		else if (ev.iType == EPheBleNumericCompare && ours)
			{
			// the companion has no comparison UI yet: the Pebble shows the number and the user confirms there
			iBle.ConfirmNumeric(iConn, ETrue);
			}
		NextEvent();
		break;
		}

	case CReq::EPair:
		if (aStatus >= 0)
			iObserver.LePaired(KErrNone, static_cast<TPebblePairingMethod>(aStatus));
		else
			iObserver.LePaired(aStatus, EPebblePairingUnknown);
		break;

	case CReq::ERead:
		if (aStatus == KErrNone && iReadResult().iError == KErrNone && iReadResult().iAttError == 0)
			iObserver.LeReadDone(aReq.iChar, KErrNone, iReadValuePtr.Ptr(), iReadValuePtr.Length());
		else
			iObserver.LeReadDone(aReq.iChar, aStatus != KErrNone ? aStatus
				: (iReadResult().iError != KErrNone ? iReadResult().iError : KErrPheBleAtt(iReadResult().iAttError)), NULL, 0);
		break;

	case CReq::EWrite:
		iObserver.LeWriteDone(aReq.iChar, aStatus != KErrNone ? aStatus : iWriteResult().iError,
			aStatus != KErrNone ? 0 : iWriteResult().iAttError);
		break;

	case CReq::ESubscribe:
		{
		TInt err = aStatus;
		if (err == KErrNone)
			err = iSubResult().iError != KErrNone ? iSubResult().iError
				: (iSubResult().iAttError ? KErrPheBleAtt(iSubResult().iAttError) : KErrNone);
		iObserver.LeSubscribed(aReq.iChar, err);
		break;
		}
		}
	}

} // namespace

MPebbleLeTransport* PebbleCreateLeTransportL(MPebbleLeObserver& aObserver)
	{
	return CPebbleLe::NewL(aObserver);
	}
