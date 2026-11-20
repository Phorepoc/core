// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheblerouter.cpp -- OURS (BLE plan rung 3, docs: core os/src/pheble/README.md): the HCI router. An HCTL plug-in
// (ECOM interface KHCTLBaseEcomInterfaceUid) that CoreHCI loads in place of the board's HCTL. It loads the real HCTL
// named by z:\private\101f7989\bluetooth\pheblerouter.ini and sits between it and the 2010 BR/EDR stack:
//
//   stack (CoreHCI/linkmgr) <-> CPheBleRouter <-> board HCTL (hctl_h5, hctl_uart_original ...) <-> controller
//                                    ^
//                                    +--> pheblehost.dll (NimBLE, the PheBle server)
//
// Routing (the 2010 stack never sees LE traffic, NimBLE never sees BR/EDR traffic):
//   - commands: one command in flight at a time across both owners; each Command Complete / Status goes to the owner
//     of the opcode in flight. A stack command that arrives while the LE host's is in flight is held (the command
//     channel is reported closed to the stack until it is sent; the HCTL contract keeps the frame valid until then).
//   - the stack's Set_Event_Mask gets the LE host's bits ORed in (Disconnection Complete, Encryption Change, HW error,
//     Data Buffer Overflow, Encryption Key Refresh, LE Meta); the LE host's own Set_Event_Mask and HCI_Reset are
//     answered by the router and never reach the controller (the stack owns the controller's reset).
//   - events: LE Meta (0x3E) to the LE host; LE Connection Complete records the handle as LE; Disconnection Complete,
//     Encryption Change, Key Refresh, Read Remote Version, Authenticated Payload Timeout go by handle;
//     Number Of Completed Packets is split by handle; Hardware Error and Data Buffer Overflow go to both.
//   - ACL: by connection handle, both ways.
//   - ACL buffers: when the controller has no LE buffers (LE_Read_Buffer_Size = 0, "shared"), the router keeps
//     le_acl_reserve of the BR/EDR buffer count (Read_Buffer_Size) for LE and tells each side only its share.

#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <ecom/implementationproxy.h>
#include <bluetooth/hci/hctlbase.h>
#include <bluetooth/hci/hctlinterface.h>
#include <bluetooth/hci/hctleventobserver.h>
#include <bluetooth/hci/hctldataobserver.h>
#include <bluetooth/hci/hctlchannelobserver.h>
#include <bluetooth/hci/controllerstateobserver.h>
#include "phebleroute.h"

#define RLOG(...) RDebug::Printf(__VA_ARGS__)

namespace {

// Command trace (2026-11-19, NimBLE "reset, reason 19" = an LE-host HCI command never acknowledged): one line per command
// handed over / answered, bounded (the first KTraceMax lines per process) so a stuck loop cannot flood the log. The router
// runs on the stack thread and the LE host's commands are queued from its own thread: a plain increment (a lost update only
// shifts the bound by one).
const TInt KTraceMax = 300;      // (an ECOM plug-in has no writable static data: the counter is the member iTrace)
#define RTRC(...) do { if (iTrace < KTraceMax) { ++iTrace; RDebug::Printf(__VA_ARGS__); } } while (0)

const TInt KPheBleRouterImplUid = 0xE7015E11;

_LIT(KRouterIni, "z:\\private\\101f7989\\bluetooth\\pheblerouter.ini");

const TUint16 KOpSetEventMask = 0x0C01;
const TUint16 KOpReset = 0x0C03;
const TUint16 KOpReadBufferSize = 0x1005;
const TUint16 KOpLeReadBufferSize = 0x2002;

// Disconnection Complete | Encryption Change | Hardware Error | Data Buffer Overflow | Encryption Key Refresh | LE Meta
// (NimBLE's own startup mask, ble_hs_startup_set_evmask_tx)
const TUint32 KLeMaskLo = 0x02008090;
const TUint32 KLeMaskHi = 0x20008000;

const TUint KMaxLeAclLen = 1020;      // NimBLE's transport ACL buffer is 1024 incl. the 4-byte header

inline TUint16 Get16(const TDesC8& a, TInt i) { return (TUint16)(a[i] | (a[i + 1] << 8)); }

enum TOwner { EOwnerNone, EOwnerStack, EOwnerLe };

class CPheBleRouter;

/** Wakes the router on the stack's thread when the LE host (another thread) queued a packet. */
class CKick : public CActive
	{
public:
	CKick(CPheBleRouter& aRouter) : CActive(EPriorityStandard), iRouter(aRouter) { CActiveScheduler::Add(this); }
	~CKick() { Cancel(); }
	void Arm() { iStatus = KRequestPending; SetActive(); }
	TRequestStatus* StatusPtr() { return &iStatus; }
private:
	void RunL();
	void DoCancel() { TRequestStatus* s = &iStatus; User::RequestComplete(s, KErrCancel); }
	CPheBleRouter& iRouter;
	};

class CPheBleRouter : public CHCTLBase, public MHCTLInterface, public MHCTLEventObserver, public MHCTLDataObserver,
	public MHCTLChannelObserver, public MControllerStateObserver, public MPheBleHciSink
	{
public:
	static CHCTLBase* NewL();
	~CPheBleRouter();

	// CHCTLBase
	TAny* Interface(TUid aUid);

	// MHCTLInterface (from the stack)
	TInt MhiWriteCommand(const TDesC8& aData);
	TInt MhiWriteAclData(const TDesC8& aData);
	TInt MhiWriteSynchronousData(const TDesC8& aData) { return iInnerIf->MhiWriteSynchronousData(aData); }
	void MhiGetAclDataTransportOverhead(TUint& aHeader, TUint& aTrailer) const { iInnerIf->MhiGetAclDataTransportOverhead(aHeader, aTrailer); }
	void MhiGetSynchronousDataTransportOverhead(TUint& aHeader, TUint& aTrailer) const { iInnerIf->MhiGetSynchronousDataTransportOverhead(aHeader, aTrailer); }
	void MhiGetCommandTransportOverhead(TUint& aHeader, TUint& aTrailer) const { iInnerIf->MhiGetCommandTransportOverhead(aHeader, aTrailer); }
	void MhiSetDataObserver(MHCTLDataObserver& aObs) { iStackData = &aObs; }
	void MhiSetEventObserver(MHCTLEventObserver& aObs) { iStackEvent = &aObs; }
	void MhiSetChannelObserver(MHCTLChannelObserver& aObs) { iStackChannel = &aObs; }
	void MhiSetControllerStateObserver(MControllerStateObserver& aObs) { iStackState = &aObs; }
	void MhiSetQdpPluginInterfaceFinder(MQdpPluginInterfaceFinder& aFinder) { iInnerIf->MhiSetQdpPluginInterfaceFinder(aFinder); }

	// from the inner HCTL
	void MheoProcessEvent(const TDesC8& aEvent);
	void MhdoProcessAclData(const TDesC8& aData);
	void MhdoProcessSynchronousData(const TDesC8& aData) { if (iStackData) iStackData->MhdoProcessSynchronousData(aData); }
	void MhcoChannelOpen(THCITransportChannel aChannels);
	void MhcoChannelClosed(THCITransportChannel aChannels);
	void McsoProcessPowerChange(TInt aError, TControllerChangeType aChangeType, TBTPowerState aState);
	void McsoProcessHardResetPhaseChange(TInt aError, TControllerChangeType aChangeType, TBTHardResetState aState);

	// MPheBleHciSink (from the LE host; any thread)
	void PbsCommand(const TDesC8& aCommand);
	void PbsAcl(const TDesC8& aAcl);

	void Pump();

private:
	CPheBleRouter();
	void ConstructL();
	void ReadIniL(TUid& aInner);
	TBool IsLe(TUint16 aHandle) const { return iLeHandles.Find(aHandle) != KErrNotFound; }
	void ToStack(const TDesC8& aEvent) { if (iStackEvent) iStackEvent->MheoProcessEvent(aEvent); }
	void ToLe(const TDesC8& aEvent) { if (iHost) iHost->PbhEvent(aEvent); }
	void RouteByHandle(const TDesC8& aEvent, TInt aHandleOffset, TBool aRemove);
	void SplitCompletedPackets(const TDesC8& aEvent);
	void CommandDone(const TDesC8& aEvent, TUint16 aOpcode);
	void FakeComplete(TUint16 aOpcode);
	TInt SendFramed(HBufC8*& aKeep, const TDesC8& aPacket, TBool aAcl);
	void UpdateStackChannels();
	void ControllerGone();
	void Queue(RPointerArray<HBufC8>& aQueue, const TDesC8& aPacket);

private:
	CHCTLBase* iInner;
	MHCTLInterface* iInnerIf;
	MHCTLEventObserver* iStackEvent;
	MHCTLDataObserver* iStackData;
	MHCTLChannelObserver* iStackChannel;
	MControllerStateObserver* iStackState;
	MPheBleHost* iHost;

	THCITransportChannel iInnerOpen;      // what the inner HCTL reports
	THCITransportChannel iStackOpen;      // what we reported to the stack

	TBool iCmdInFlight;
	TUint16 iInFlightOpcode;
	TOwner iInFlightOwner;
	TOwner iLastOwner;
	const TDesC8* iHeldStackCmd;          // a stack frame waiting for the command slot (valid: channel reported closed)
	HBufC8* iLeCmdFrame;                  // the LE command frame last given to the inner HCTL
	HBufC8* iLeAclFrame;                  // the LE ACL frame last given to the inner HCTL

	RMutex iQLock;                        // guards the two queues + iKicked (the LE host's thread pushes)
	RPointerArray<HBufC8> iLeCmds;
	RPointerArray<HBufC8> iLeAcl;
	TBool iKicked;
	CKick* iKick;
	TThreadId iStackThread;

	RArray<TUint16> iLeHandles;
	TInt iAclReserve;                     // buffers kept for LE when the controller shares its ACL buffers
	TUint16 iStackAclLen;
	TBool iReady;
	TInt iTrace;                          // command trace lines printed (RTRC)
	};

void CKick::RunL()
	{
	iRouter.Pump();
	}

CPheBleRouter::CPheBleRouter()
	: iInFlightOwner(EOwnerNone), iLastOwner(EOwnerNone), iAclReserve(2), iTrace(0)
	{
	}

CHCTLBase* CPheBleRouter::NewL()
	{
	CPheBleRouter* self = new (ELeave) CPheBleRouter;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CPheBleRouter::ReadIniL(TUid& aInner)
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RFile f;
	User::LeaveIfError(f.Open(fs, KRouterIni, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TBuf8<1024> buf;
	User::LeaveIfError(f.Read(buf));
	CleanupStack::PopAndDestroy(2, &fs);

	// "key= value" lines, ';' comments (the shape of the 2010 HCI .ini files)
	TLex8 lines(buf);
	TBool haveInner = EFalse;
	while (!lines.Eos())
		{
		lines.Mark();
		while (!lines.Eos() && lines.Peek() != '\n')
			lines.Inc();
		TPtrC8 line = lines.MarkedToken();
		if (!lines.Eos())
			lines.Inc();
		TInt eq = line.Locate('=');
		if (eq <= 0 || line[0] == ';' || line[0] == '[')
			continue;
		TPtrC8 key = line.Left(eq);
		TLex8 val(line.Mid(eq + 1));
		val.SkipSpace();
		TUint v = 0;
		if (val.Remainder().Length() > 2 && val.Peek() == '0' && (val.Remainder()[1] == 'x' || val.Remainder()[1] == 'X'))
			{
			val.Inc(2);
			if (val.Val(v, EHex) != KErrNone)
				continue;
			}
		else if (val.Val(v, EDecimal) != KErrNone)
			continue;
		if (key.FindF(_L8("inner_hctl_uid")) == 0)
			{
			aInner = TUid::Uid((TInt)v);
			haveInner = ETrue;
			}
		else if (key.FindF(_L8("le_acl_reserve")) == 0)
			iAclReserve = (TInt)v;
		}
	if (!haveInner)
		User::Leave(KErrNotFound);
	}

void CPheBleRouter::ConstructL()
	{
	TUid inner = KNullUid;
	ReadIniL(inner);
	RLOG("pheblerouter: inner HCTL 0x%08x, LE ACL reserve %d", inner.iUid, iAclReserve);
	User::LeaveIfError(iQLock.CreateLocal(EOwnerProcess));
	iStackThread = RThread().Id();
	iKick = new (ELeave) CKick(*this);
	iKick->Arm();

	iInner = CHCTLBase::NewL(inner);
	iInnerIf = static_cast<MHCTLInterface*>(iInner->Interface(TUid::Uid(KHCTLInterfaceUid)));
	if (!iInnerIf)
		User::Leave(KErrNotSupported);
	iInnerIf->MhiSetEventObserver(*this);
	iInnerIf->MhiSetDataObserver(*this);
	iInnerIf->MhiSetChannelObserver(*this);
	iInnerIf->MhiSetControllerStateObserver(*this);

	iHost = PheBleHostAttach(this);    // NULL = no LE host: the router then only passes BR/EDR traffic through
	RLOG("pheblerouter: LE host %s", iHost ? "attached" : "absent");
	}

CPheBleRouter::~CPheBleRouter()
	{
	if (iHost)
		PheBleHostDetach(this);
	delete iInner;           // the board HCTL first: it may still call us while it closes
	delete iKick;
	iLeCmds.ResetAndDestroy();
	iLeAcl.ResetAndDestroy();
	delete iLeCmdFrame;
	delete iLeAclFrame;
	iLeHandles.Close();
	iQLock.Close();
	}

TAny* CPheBleRouter::Interface(TUid aUid)
	{
	if (aUid.iUid == KHCTLInterfaceUid)
		return static_cast<MHCTLInterface*>(this);
	// power, hard reset, the baud-rate config the initialisor uses ...: the board HCTL's own
	return iInner->Interface(aUid);
	}

// ---------------------------------------------------------------- stack -> controller

TInt CPheBleRouter::MhiWriteCommand(const TDesC8& aData)
	{
	TUint hdr = 0, trl = 0;
	iInnerIf->MhiGetCommandTransportOverhead(hdr, trl);
	TUint16 opcode = Get16(aData, hdr);
	if (opcode == KOpSetEventMask && aData.Length() >= (TInt)(hdr + 3 + 8))
		{
		// the frame is the stack's own writable buffer (HCTLs write their indicator into it the same way)
		TUint8* p = const_cast<TUint8*>(aData.Ptr()) + hdr + 3;
		TUint32 lo = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
		TUint32 hi = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
		lo |= KLeMaskLo;
		hi |= KLeMaskHi;
		for (TInt i = 0; i < 4; ++i)
			{
			p[i] = (TUint8)(lo >> (8 * i));
			p[4 + i] = (TUint8)(hi >> (8 * i));
			}
		}
	if (iCmdInFlight || iHeldStackCmd)
		{
		RTRC("pheblerouter: stack cmd %04x HELD (inflight %d op %04x)", opcode, iCmdInFlight, iInFlightOpcode);
		iHeldStackCmd = &aData;
		UpdateStackChannels();
		return KErrNone;
		}
	RTRC("pheblerouter: stack cmd %04x sent", opcode);
	iCmdInFlight = ETrue;
	iInFlightOpcode = opcode;
	iInFlightOwner = iLastOwner = EOwnerStack;
	TInt err = iInnerIf->MhiWriteCommand(aData);
	if (err != KErrNone)
		iCmdInFlight = EFalse;
	return err;
	}

TInt CPheBleRouter::MhiWriteAclData(const TDesC8& aData)
	{
	return iInnerIf->MhiWriteAclData(aData);
	}

// ---------------------------------------------------------------- LE host -> controller

void CPheBleRouter::Queue(RPointerArray<HBufC8>& aQueue, const TDesC8& aPacket)
	{
	HBufC8* copy = aPacket.Alloc();
	iQLock.Wait();
	if (copy && aQueue.Append(copy) != KErrNone)
		{
		delete copy;
		copy = NULL;
		}
	TBool kick = !iKicked;
	iKicked = ETrue;
	iQLock.Signal();
	if (!copy)
		RLOG("pheblerouter: out of memory, LE packet dropped");
	if (kick)
		{
		RThread stack;
		if (stack.Open(iStackThread) == KErrNone)
			{
			TRequestStatus* s = iKick->StatusPtr();
			stack.RequestComplete(s, KErrNone);
			stack.Close();
			}
		}
	}

void CPheBleRouter::PbsCommand(const TDesC8& aCommand)
	{
	RTRC("pheblerouter: LE host cmd %04x len %d queued", aCommand.Length() >= 2 ? Get16(aCommand, 0) : 0, aCommand.Length());
	Queue(iLeCmds, aCommand);
	}

void CPheBleRouter::PbsAcl(const TDesC8& aAcl)
	{
	Queue(iLeAcl, aAcl);
	}

TInt CPheBleRouter::SendFramed(HBufC8*& aKeep, const TDesC8& aPacket, TBool aAcl)
	{
	TUint hdr = 0, trl = 0;
	if (aAcl)
		iInnerIf->MhiGetAclDataTransportOverhead(hdr, trl);
	else
		iInnerIf->MhiGetCommandTransportOverhead(hdr, trl);
	HBufC8* frame = HBufC8::New(hdr + aPacket.Length() + trl);
	if (!frame)
		return KErrNoMemory;
	TPtr8 p = frame->Des();
	p.FillZ(hdr);
	p.Append(aPacket);
	p.AppendFill(0, trl);
	delete aKeep;           // the previous frame: the inner HCTL is done with it (the slot / channel was free again)
	aKeep = frame;
	return aAcl ? iInnerIf->MhiWriteAclData(*frame) : iInnerIf->MhiWriteCommand(*frame);
	}

void CPheBleRouter::FakeComplete(TUint16 aOpcode)
	{
	TBuf8<6> cc;
	cc.Append(0x0E);
	cc.Append(4);
	cc.Append(1);
	cc.Append((TUint8)aOpcode);
	cc.Append((TUint8)(aOpcode >> 8));
	cc.Append(0);    // success
	ToLe(cc);
	}

void CPheBleRouter::Pump()
	{
	iQLock.Wait();
	iKicked = EFalse;
	if (!iKick->IsActive())
		iKick->Arm();
	iQLock.Signal();

	// commands: one slot; alternate owners when both wait
	while (!iCmdInFlight)
		{
		iQLock.Wait();
		HBufC8* le = iLeCmds.Count() ? iLeCmds[0] : NULL;
		iQLock.Signal();
		TBool leOk = le && iReady && (iInnerOpen & KHCITransportCommandChannel);
		if (le && !leOk)
			RTRC("pheblerouter: LE cmd %04x waits: ready %d innerOpen %d", le->Length() >= 2 ? Get16(*le, 0) : 0, iReady, (TInt)iInnerOpen);
		TBool takeLe = leOk && (!iHeldStackCmd || iLastOwner == EOwnerStack);
		if (!takeLe && iHeldStackCmd)
			{
			const TDesC8* held = iHeldStackCmd;
			iHeldStackCmd = NULL;
			TUint hdr = 0, trl = 0;
			iInnerIf->MhiGetCommandTransportOverhead(hdr, trl);
			iCmdInFlight = ETrue;
			iInFlightOpcode = Get16(*held, hdr);
			iInFlightOwner = iLastOwner = EOwnerStack;
			if (iInnerIf->MhiWriteCommand(*held) != KErrNone)
				iCmdInFlight = EFalse;
			UpdateStackChannels();
			continue;
			}
		if (!takeLe)
			break;
		iQLock.Wait();
		iLeCmds.Remove(0);
		iQLock.Signal();
		TUint16 opcode = le->Length() >= 3 ? Get16(*le, 0) : 0;
		if (opcode == KOpReset || opcode == KOpSetEventMask)
			{
			// the stack owns the controller's reset and main event mask (the LE bits are in the stack's mask)
			RTRC("pheblerouter: LE cmd %04x answered by the router", opcode);
			FakeComplete(opcode);
			}
		else
			{
			RTRC("pheblerouter: LE cmd %04x -> controller (len %d)", opcode, le->Length());
			iCmdInFlight = ETrue;
			iInFlightOpcode = opcode;
			iInFlightOwner = iLastOwner = EOwnerLe;
			if (SendFramed(iLeCmdFrame, *le, EFalse) != KErrNone)
				iCmdInFlight = EFalse;
			}
		delete le;
		}

	// LE ACL: whenever the inner ACL channel is open
	while (iReady && (iInnerOpen & KHCITransportACLDataChannel))
		{
		iQLock.Wait();
		HBufC8* acl = NULL;
		if (iLeAcl.Count())
			{
			acl = iLeAcl[0];
			iLeAcl.Remove(0);
			}
		iQLock.Signal();
		if (!acl)
			break;
		const TInt wr = SendFramed(iLeAclFrame, *acl, ETrue);
		RTRC("pheblerouter: LE ACL -> controller handle %03x len %d, write %d", acl->Length() >= 2 ? Get16(*acl, 0) & 0x0FFF : 0, acl->Length(), wr);
		delete acl;
		}
	}

// ---------------------------------------------------------------- controller -> both

void CPheBleRouter::CommandDone(const TDesC8& aEvent, TUint16 aOpcode)
	{
	TOwner owner = EOwnerStack;
	if (iCmdInFlight && aOpcode == iInFlightOpcode)
		{
		owner = iInFlightOwner;
		iCmdInFlight = EFalse;
		}
	if (aOpcode == 0)
		owner = EOwnerStack;     // a NOP credit update: the stack's command queue counts credits
	RTRC("pheblerouter: %s op %04x -> %s (inflight %d op %04x)", aEvent[0] == 0x0E ? "CC" : "CS", aOpcode,
		owner == EOwnerLe ? "LE host" : "stack", iCmdInFlight, iInFlightOpcode);

	if (owner == EOwnerLe)
		{
		if (aOpcode == KOpLeReadBufferSize && aEvent[0] == 0x0E && aEvent.Length() >= 9 && aEvent[5] == 0
				&& Get16(aEvent, 6) == 0 && iStackAclLen)
			{
			// shared buffers: give the LE host its reserved share of the BR/EDR pool
			TBuf8<9> ev(aEvent.Left(9));
			TUint16 len = iStackAclLen < KMaxLeAclLen ? iStackAclLen : (TUint16)KMaxLeAclLen;
			ev[6] = (TUint8)len;
			ev[7] = (TUint8)(len >> 8);
			ev[8] = (TUint8)iAclReserve;
			RLOG("pheblerouter: LE shares the ACL buffers: %d x %d bytes", iAclReserve, len);
			ToLe(ev);
			}
		else
			ToLe(aEvent);
		}
	else
		{
		if (aOpcode == KOpReadBufferSize && aEvent[0] == 0x0E && aEvent.Length() >= 12 && aEvent[5] == 0)
			{
			iStackAclLen = Get16(aEvent, 6);
			TUint16 pkts = Get16(aEvent, 9);
			if (iHost && iAclReserve > 0 && pkts >= 2)
				{
				if (iAclReserve > pkts / 2)
					iAclReserve = pkts / 2;
				TBuf8<16> ev(aEvent.Left(aEvent.Length() < 16 ? aEvent.Length() : 16));
				TUint16 left = (TUint16)(pkts - iAclReserve);
				ev[9] = (TUint8)left;
				ev[10] = (TUint8)(left >> 8);
				RLOG("pheblerouter: ACL buffers %d x %d, %d kept for LE", pkts, iStackAclLen, iAclReserve);
				ToStack(ev);
				}
			else
				ToStack(aEvent);
			}
		else
			ToStack(aEvent);
		if (aOpcode == KOpSetEventMask && !iReady && iHost)
			{
			iReady = ETrue;      // the stack's init has reached the event mask: the LE host may sync now
			RLOG("pheblerouter: controller ready, LE host starts");
			iHost->PbhControllerReady(ETrue);
			}
		}
	Pump();
	}

void CPheBleRouter::RouteByHandle(const TDesC8& aEvent, TInt aHandleOffset, TBool aRemove)
	{
	if (aEvent.Length() < aHandleOffset + 2)
		{
		ToStack(aEvent);
		return;
		}
	TUint16 handle = (TUint16)(Get16(aEvent, aHandleOffset) & 0x0FFF);
	TInt idx = iLeHandles.Find(handle);
	if (idx == KErrNotFound)
		{
		ToStack(aEvent);
		return;
		}
	if (aRemove)
		iLeHandles.Remove(idx);
	ToLe(aEvent);
	}

void CPheBleRouter::SplitCompletedPackets(const TDesC8& aEvent)
	{
	if (aEvent.Length() < 3 || iLeHandles.Count() == 0)
		{
		ToStack(aEvent);
		return;
		}
	TInt n = aEvent[2];
	RTRC("pheblerouter: Number Of Completed Packets, %d handle(s), first %03x x%d", n, n ? Get16(aEvent, 3) & 0x0FFF : 0, n ? Get16(aEvent, 5) : 0);
	if (aEvent.Length() < 3 + 4 * n)
		{
		ToStack(aEvent);
		return;
		}
	TBuf8<3 + 4 * 32> stack, le;
	stack.Append(0x13); stack.Append(0); stack.Append(0);
	le.Append(0x13); le.Append(0); le.Append(0);
	for (TInt i = 0; i < n && i < 32; ++i)
		{
		TPtrC8 entry = aEvent.Mid(3 + 4 * i, 4);
		TBuf8<3 + 4 * 32>& to = IsLe((TUint16)(Get16(entry, 0) & 0x0FFF)) ? le : stack;
		to.Append(entry);
		to[2] = (TUint8)(to[2] + 1);
		to[1] = (TUint8)(to[1] + 4);
		}
	if (stack[2])
		{
		stack[1] = (TUint8)(stack[1] + 1);
		ToStack(stack);
		}
	if (le[2])
		{
		le[1] = (TUint8)(le[1] + 1);
		ToLe(le);
		}
	}

void CPheBleRouter::MheoProcessEvent(const TDesC8& aEvent)
	{
	if (aEvent.Length() < 2)
		{
		ToStack(aEvent);
		return;
		}
	if (aEvent[0] != 0x0E && aEvent[0] != 0x0F && aEvent[0] != 0x13 && iLeHandles.Count() > 0)
		RTRC("pheblerouter: event %02x len %d while an LE link is up", aEvent[0], aEvent.Length());
	switch (aEvent[0])
		{
	case 0x0E:      // Command Complete: ncmd(1) opcode(2)
		if (aEvent.Length() >= 5)
			CommandDone(aEvent, Get16(aEvent, 3));
		else
			ToStack(aEvent);
		break;
	case 0x0F:      // Command Status: status(1) ncmd(1) opcode(2)
		if (aEvent.Length() >= 6)
			CommandDone(aEvent, Get16(aEvent, 4));
		else
			ToStack(aEvent);
		break;
	case 0x3E:      // LE Meta
		if (aEvent.Length() >= 6 && (aEvent[2] == 0x01 || aEvent[2] == 0x0A) && aEvent[3] == 0)
			{
			TUint16 handle = (TUint16)(Get16(aEvent, 4) & 0x0FFF);
			if (!IsLe(handle))
				iLeHandles.Append(handle);
			}
		ToLe(aEvent);
		break;
	case 0x05:      // Disconnection Complete: status(1) handle(2)
		RouteByHandle(aEvent, 3, ETrue);
		break;
	case 0x08:      // Encryption Change: status(1) handle(2)
	case 0x30:      // Encryption Key Refresh Complete
	case 0x0C:      // Read Remote Version Information Complete
		RouteByHandle(aEvent, 3, EFalse);
		break;
	case 0x57:      // Authenticated Payload Timeout Expired: handle(2)
		RouteByHandle(aEvent, 2, EFalse);
		break;
	case 0x13:      // Number Of Completed Packets
		SplitCompletedPackets(aEvent);
		break;
	case 0x10:      // Hardware Error
	case 0x1A:      // Data Buffer Overflow
		ToStack(aEvent);
		ToLe(aEvent);
		break;
	default:
		ToStack(aEvent);
		break;
		}
	}

void CPheBleRouter::MhdoProcessAclData(const TDesC8& aData)
	{
	if (aData.Length() >= 2)
		RTRC("pheblerouter: ACL from controller handle %03x len %d (%s)", Get16(aData, 0) & 0x0FFF, aData.Length(),
			IsLe((TUint16)(Get16(aData, 0) & 0x0FFF)) ? "LE host" : "stack");
	if (aData.Length() >= 2 && IsLe((TUint16)(Get16(aData, 0) & 0x0FFF)))
		{
		if (iHost)
			iHost->PbhAcl(aData);
		return;
		}
	if (iStackData)
		iStackData->MhdoProcessAclData(aData);
	}

// ---------------------------------------------------------------- channels + controller state

void CPheBleRouter::UpdateStackChannels()
	{
	THCITransportChannel want = iInnerOpen;
	if (iHeldStackCmd)
		want &= ~KHCITransportCommandChannel;
	THCITransportChannel opened = (THCITransportChannel)(want & ~iStackOpen);
	THCITransportChannel closed = (THCITransportChannel)(iStackOpen & ~want);
	iStackOpen = want;
	if (iStackChannel)
		{
		if (closed)
			iStackChannel->MhcoChannelClosed(closed);
		if (opened)
			iStackChannel->MhcoChannelOpen(opened);
		}
	}

void CPheBleRouter::MhcoChannelOpen(THCITransportChannel aChannels)
	{
	if ((iInnerOpen | aChannels) != iInnerOpen)      // (the H5 HCTL re-reports "open" after every command: log only a change)
		RTRC("pheblerouter: inner channel open %d (now %d)", (TInt)aChannels, (TInt)(iInnerOpen | aChannels));
	iInnerOpen |= aChannels;
	UpdateStackChannels();
	Pump();
	}

void CPheBleRouter::MhcoChannelClosed(THCITransportChannel aChannels)
	{
	if (iInnerOpen & aChannels)
		RTRC("pheblerouter: inner channel closed %d (was %d)", (TInt)aChannels, (TInt)iInnerOpen);
	iInnerOpen &= ~aChannels;
	UpdateStackChannels();
	}

void CPheBleRouter::ControllerGone()
	{
	RTRC("pheblerouter: controller gone (ready %d inflight %d)", iReady, iCmdInFlight);
	if (iReady && iHost)
		iHost->PbhControllerReady(EFalse);
	iReady = EFalse;
	iCmdInFlight = EFalse;
	iLeHandles.Reset();
	iQLock.Wait();
	iLeCmds.ResetAndDestroy();
	iLeAcl.ResetAndDestroy();
	iQLock.Signal();
	}

void CPheBleRouter::McsoProcessPowerChange(TInt aError, TControllerChangeType aChangeType, TBTPowerState aState)
	{
	if (aState == EBTOff || aError != KErrNone)
		ControllerGone();
	if (iStackState)
		iStackState->McsoProcessPowerChange(aError, aChangeType, aState);
	}

void CPheBleRouter::McsoProcessHardResetPhaseChange(TInt aError, TControllerChangeType aChangeType, TBTHardResetState aState)
	{
	ControllerGone();
	if (iStackState)
		iStackState->McsoProcessHardResetPhaseChange(aError, aChangeType, aState);
	}

} // namespace

const TImplementationProxy ImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheBleRouterImplUid, CPheBleRouter::NewL),
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
	return ImplementationTable;
	}
