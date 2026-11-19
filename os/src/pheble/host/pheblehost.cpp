// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheblehost.cpp -- OURS (BLE plan rungs 3-4): the LE host DLL. Loaded into the Bluetooth stack's process by the HCI
// router (pheblerouter.dll). Runs:
//   - "PheBleHost": NimBLE's host task (nimble_port_run on the default event queue);
//   - "PheBleSrv":  the "!PheBle" Symbian server (pheble.h) with its own active scheduler.
// NimBLE's HCI transport ("custom" LL transport, nimble/transport) is bound to the router through MPheBleHciSink;
// packets from the router are copied and posted to NimBLE's event queue, so every NimBLE call happens on its task.
// Server requests are posted the same way; results come back to the server thread through a locked queue + a kick.
//
// Scanning lives here; connect, pairing and GATT are in phebleops.cpp.

#include <e32base.h>
#include <e32debug.h>
#include "phebleroute.h"
#include "pheble.h"
#include "pheblesrv.h"

extern "C" {
#include "os/os.h"
#include "nimble/nimble_port.h"
#include "nimble/transport.h"
#include "nimble/transport_impl.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "os/os_mbuf.h"
}

#define HLOG(...) RDebug::Printf(__VA_ARGS__)

namespace {

MPheBleHciSink* gRouter = NULL;
TBool gStarted = EFalse;
TBool gSynced = EFalse;
RLibrary gPin;
RMutex gRouterLock;          // gRouter changes on the stack thread, NimBLE's task reads it
ble_npl_eventq gRxQ;         // the router's events and ACL, run by RxThread (the controller's context, see RxThread)

// ---------------------------------------------------------------- router -> NimBLE task

struct TRxPacket
	{
	ble_npl_event iEv;
	TBool iAcl;
	TInt iLen;
	TUint8 iData[1];
	};

void RxHandler(ble_npl_event* aEv)
	{
	TRxPacket* p = static_cast<TRxPacket*>(ble_npl_event_get_arg(aEv));
	if (p->iAcl)
		{
		os_mbuf* om = ble_transport_alloc_acl_from_ll();
		if (om && os_mbuf_append(om, p->iData, (uint16_t)p->iLen) == 0)
			ble_transport_to_hs_acl(om);
		else if (om)
			os_mbuf_free_chain(om);
		}
	else
		{
		TUint8 code = p->iData[0];
		// LE advertising reports are discardable (NimBLE's own rule for its event pools)
		TBool discardable = code == 0x3E && p->iLen > 2 && p->iData[2] == 0x02;
		void* buf = ble_transport_alloc_evt(discardable ? 1 : 0);
		if (buf && p->iLen <= MYNEWT_VAL(BLE_TRANSPORT_EVT_SIZE))
			{
			memcpy(buf, p->iData, p->iLen);
			ble_transport_to_hs_evt(buf);
			}
		else if (buf)
			ble_transport_free(buf);
		else
			{
			// a scan floods the discardable pool: say so for the first few and then every 256th (one line per advert filled the log)
			static TInt dropped = 0;
			if (dropped < 4 || (dropped & 0xFF) == 0)
				HLOG("pheblehost: event 0x%02x dropped (no buffer), %d so far", code, dropped);
			++dropped;
			}
		}
	User::Free(p);
	}

void PostRx(const TDesC8& aData, TBool aAcl)
	{
	TRxPacket* p = static_cast<TRxPacket*>(User::Alloc(sizeof(TRxPacket) + aData.Length()));
	if (!p)
		return;
	ble_npl_event_init(&p->iEv, RxHandler, p);
	p->iAcl = aAcl;
	p->iLen = aData.Length();
	memcpy(p->iData, aData.Ptr(), aData.Length());
	ble_npl_eventq_put(&gRxQ, &p->iEv);
	}

// The controller's side of NimBLE's transport has its own context in every NimBLE port (a controller task, a UART rx task): the
// host blocks inside ble_hs_hci_cmd_tx on a semaphore until the Command Complete arrives, and the Command Complete is
// acknowledged by ble_transport_to_hs_evt called FROM THE CONTROLLER'S CONTEXT. Posting the router's events to the host's own
// default queue (as before) made the host wait for an event that only the host's thread could run: every sync sat out the 2 s
// HCI timeout and NimBLE reset with reason 19 (BLE_HS_ETIMEOUT_HCI) for ever (boot 2812 trace: HCI_Reset answered by the router
// 2 lines after it was queued, no ack seen). The events now run on this thread.
TInt RxThread(TAny*)
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	for (;;)
		ble_npl_eventq_run(&gRxQ);
	delete cleanup;       // (not reached)
	return KErrNone;
	}

ble_npl_event gStartEv;
ble_npl_event gStopEv;

void StartHandler(ble_npl_event*)
	{
	HLOG("pheblehost: controller ready -> NimBLE sync");
	ble_hs_sched_start();
	}

void StopHandler(ble_npl_event*)
	{
	if (gSynced)
		ble_hs_sched_reset(BLE_HS_ECONTROLLER);
	}

class THostGlue : public MPheBleHost
	{
public:
	void PbhEvent(const TDesC8& aEvent) { PostRx(aEvent, EFalse); }
	void PbhAcl(const TDesC8& aAcl) { PostRx(aAcl, ETrue); }
	void PbhControllerReady(TBool aReady)
		{
		ble_npl_eventq_put(nimble_port_get_dflt_eventq(), aReady ? &gStartEv : &gStopEv);
		}
	};
THostGlue gGlue;

void OnSync()
	{
	gSynced = ETrue;
	ble_hs_util_ensure_addr(0);
	HLOG("pheblehost: NimBLE synced");
	}

void OnReset(int aReason)
	{
	gSynced = EFalse;
	HLOG("pheblehost: NimBLE reset, reason %d", aReason);
	PheBleServerHostReset();
	}

TInt HostThread(TAny*)
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	nimble_port_run();       // never returns
	delete cleanup;
	return KErrNone;
	}

TInt StartHost()
	{
	TInt err = gRouterLock.CreateLocal(EOwnerProcess);
	if (err == KErrNone)
		err = phe_npl_init();
	if (err != KErrNone)
		return err;
	nimble_port_init();
	ble_hs_cfg.sync_cb = OnSync;
	ble_hs_cfg.reset_cb = OnReset;
	ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
	ble_hs_cfg.sm_bonding = 1;
	ble_hs_cfg.sm_sc = 1;
	ble_hs_cfg.sm_mitm = 0;
	ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
	ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
	PheBleStoreInit();       // the bond store (a file in the private dir)
	ble_npl_event_init(&gStartEv, StartHandler, NULL);
	ble_npl_event_init(&gStopEv, StopHandler, NULL);

	ble_npl_eventq_init(&gRxQ);
	RThread rx;
	err = rx.Create(_L("PheBleRx"), RxThread, 0x4000, NULL, NULL, EOwnerProcess);
	if (err != KErrNone)
		return err;
	rx.Resume();
	rx.Close();

	RThread t;
	err = t.Create(_L("PheBleHost"), HostThread, 0x8000, NULL, NULL, EOwnerProcess);
	if (err != KErrNone)
		return err;
	t.Resume();
	t.Close();
	return PheBleServerStart();
	}

} // namespace

// ---------------------------------------------------------------- NimBLE -> router (custom LL transport)

extern "C" {

void ble_transport_ll_init(void)
	{
	}

int ble_transport_to_ll_cmd_impl(void *buf)
	{
	const TUint8* cmd = static_cast<const TUint8*>(buf);
	TPtrC8 pkt(cmd, 3 + cmd[2]);
	gRouterLock.Wait();
	if (gRouter)
		gRouter->PbsCommand(pkt);
	gRouterLock.Signal();
	ble_transport_free(buf);
	return 0;
	}

int ble_transport_to_ll_acl_impl(struct os_mbuf *om)
	{
	TInt len = OS_MBUF_PKTLEN(om);
	HBufC8* flat = HBufC8::New(len);
	if (flat)
		{
		TPtr8 p = flat->Des();
		p.SetLength(len);
		os_mbuf_copydata(om, 0, len, (void*)p.Ptr());
		gRouterLock.Wait();
		if (gRouter)
			gRouter->PbsAcl(*flat);
		gRouterLock.Signal();
		delete flat;
		}
	os_mbuf_free_chain(om);
	return flat ? 0 : BLE_HS_ENOMEM;
	}

int ble_transport_to_ll_iso_impl(struct os_mbuf *om)
	{
	os_mbuf_free_chain(om);
	return 0;
	}

} // extern "C"

// ---------------------------------------------------------------- exports for the router

EXPORT_C MPheBleHost* PheBleHostAttach(MPheBleHciSink* aRouter)
	{
	if (!gStarted)
		{
		// pin the DLL: NimBLE's task and statics live as long as the process
		if (gPin.Load(_L("pheblehost.dll")) != KErrNone)
			return NULL;
		TInt err = StartHost();
		if (err != KErrNone)
			{
			HLOG("pheblehost: start failed %d", err);
			return NULL;
			}
		gStarted = ETrue;
		}
	gRouterLock.Wait();
	gRouter = aRouter;
	gRouterLock.Signal();
	return &gGlue;
	}

EXPORT_C void PheBleHostDetach(MPheBleHciSink* aRouter)
	{
	gRouterLock.Wait();
	if (gRouter == aRouter)
		gRouter = NULL;
	gRouterLock.Signal();
	gGlue.PbhControllerReady(EFalse);
	}

// ---------------------------------------------------------------- scanning (called on NimBLE's task)

namespace {

int GapEvent(struct ble_gap_event* aEvent, void*)
	{
	switch (aEvent->type)
		{
	case BLE_GAP_EVENT_DISC:
		{
		const ble_gap_disc_desc& d = aEvent->disc;
		TPheBleAdvert adv;
		// not FillZ(&adv, sizeof(adv)): iName is a TBuf8<31>, and zeroing its descriptor header makes its max length 0 -- the first
		// advert WITH a name then died in iName.Copy with USER 23 (TDes8 overflow), killing the PheBleHost thread (boot 2813, the
		// Pebble's name in the first report after "scan start rc 0").
		Mem::FillZ(&adv.iAddr, sizeof(adv.iAddr));
		Mem::FillZ(adv.iData, sizeof(adv.iData));
		adv.iName.Zero();
		Mem::Copy(adv.iAddr.iAddr, d.addr.val, 6);
		adv.iAddr.iType = d.addr.type;
		adv.iRssi = d.rssi;
		adv.iEventType = d.event_type;
		adv.iDataLength = d.length_data > 31 ? 31 : d.length_data;
		Mem::Copy(adv.iData, d.data, adv.iDataLength);
		ble_hs_adv_fields fields;
		if (ble_hs_adv_parse_fields(&fields, d.data, d.length_data) == 0 && fields.name && fields.name_len)
			adv.iName.Copy(TPtrC8(fields.name, fields.name_len > 31 ? 31 : fields.name_len));
		PheBleServerAdvert(adv);
		return 0;
		}
	case BLE_GAP_EVENT_DISC_COMPLETE:
		PheBleServerScanDone(aEvent->disc_complete.reason);
		return 0;
	default:
		return 0;
		}
	}

struct TScanReq
	{
	ble_npl_event iEv;
	TBool iStart;
	TBool iActive;
	TInt iTimeoutMs;
	};

void ScanHandler(ble_npl_event* aEv)
	{
	TScanReq* r = static_cast<TScanReq*>(ble_npl_event_get_arg(aEv));
	if (!r->iStart)
		{
		ble_gap_disc_cancel();
		}
	else if (!gSynced)
		{
		PheBleServerScanDone(KErrPheBleNotReady);
		}
	else
		{
		uint8_t own = 0;
		ble_hs_id_infer_auto(0, &own);
		ble_gap_disc_params params;
		Mem::FillZ(&params, sizeof(params));
		params.passive = r->iActive ? 0 : 1;
		params.filter_duplicates = 0;
		TInt rc = ble_gap_disc(own, r->iTimeoutMs > 0 ? r->iTimeoutMs : BLE_HS_FOREVER, &params, GapEvent, NULL);
		HLOG("pheblehost: scan start rc %d", rc);
		if (rc != 0)
			PheBleServerScanDone(rc);
		}
	User::Free(r);
	}

} // namespace

TInt PheBleHostScan(TBool aStart, TBool aActive, TInt aTimeoutMs)
	{
	if (!gStarted)
		return KErrPheBleNotReady;
	TScanReq* r = static_cast<TScanReq*>(User::Alloc(sizeof(TScanReq)));
	if (!r)
		return KErrNoMemory;
	ble_npl_event_init(&r->iEv, ScanHandler, r);
	r->iStart = aStart;
	r->iActive = aActive;
	r->iTimeoutMs = aTimeoutMs;
	ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &r->iEv);
	return KErrNone;
	}

TInt PheBleHostMtu(TUint16 aConn)
	{
	if (!gStarted)
		return KErrPheBleNotReady;
	uint16_t mtu = ble_att_mtu(aConn);
	return mtu ? mtu : KErrNotFound;
	}
