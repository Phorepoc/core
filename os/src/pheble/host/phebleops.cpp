// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// phebleops.cpp -- OURS (BLE plan P2-P4): the central / GATT-client / SMP operations of the "!PheBle" server, run on
// NimBLE's task. The server thread posts a TOpReq (PheBleHostRequest); results go back with PheBleServerComplete
// (request id, local error, ATT error, data) and link events with PheBleServerEvent. Everything here runs on NimBLE's
// task only, so the state below needs no lock.
//
//   Connect:  ble_gap_connect -> CONNECT -> (bonded peer: ble_gap_security_initiate -> ENC_CHANGE) -> MTU exchange
//             -> complete with the connection handle. A bonded link is encrypted before Connect completes.
//   Pair:     ble_gap_security_initiate -> ENC_CHANGE; NUMCMP passkey actions go to the client (ConfirmNumeric);
//             a repeated pairing deletes the old bond and retries.
//   Discover: all services -> per service all characteristics -> per characteristic its descriptors.
//   Read / Write / Subscribe: ATT errors in the separate iAttError (BLE_HS_ERR_ATT_BASE + code).

#include <e32base.h>
#include <e32debug.h>
#include "pheblesrv.h"

extern "C" {
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_store.h"
#include "os/os_mbuf.h"
}

#define OLOG(...) RDebug::Printf(__VA_ARGS__)

namespace {

const TInt KConnectTimeoutMs = 30000;
const TInt KMaxEntries = 160;

TInt HsError(int aRc)
	{
	if (aRc == 0)
		return KErrNone;
	if (aRc == BLE_HS_ETIMEOUT)
		return KErrTimedOut;
	if (aRc == BLE_HS_ENOTCONN)
		return KErrDisconnected;
	if (aRc == BLE_HS_ENOMEM)
		return KErrNoMemory;
	if (aRc == BLE_HS_EBUSY || aRc == BLE_HS_EALREADY)
		return KErrInUse;
	return KErrGeneral;
	}

/** A GATT error: (local error, ATT error code). */
void GattError(const ble_gatt_error* aErr, TInt& aLocal, TInt& aAtt)
	{
	aLocal = KErrNone;
	aAtt = 0;
	if (!aErr || aErr->status == 0)
		return;
	if (aErr->status > BLE_HS_ERR_ATT_BASE && aErr->status < BLE_HS_ERR_ATT_BASE + 0x100)
		aAtt = aErr->status - BLE_HS_ERR_ATT_BASE;
	else
		aLocal = HsError(aErr->status);
	}

void Uuid128(const ble_uuid_any_t& aUuid, TUint8* aOut)
	{
	static const TUint8 KBase[16] = { 0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0, 0, 0, 0 };
	switch (aUuid.u.type)
		{
	case BLE_UUID_TYPE_16:
		Mem::Copy(aOut, KBase, 16);
		aOut[12] = (TUint8)aUuid.u16.value;
		aOut[13] = (TUint8)(aUuid.u16.value >> 8);
		break;
	case BLE_UUID_TYPE_32:
		Mem::Copy(aOut, KBase, 16);
		aOut[12] = (TUint8)aUuid.u32.value;
		aOut[13] = (TUint8)(aUuid.u32.value >> 8);
		aOut[14] = (TUint8)(aUuid.u32.value >> 16);
		aOut[15] = (TUint8)(aUuid.u32.value >> 24);
		break;
	default:
		Mem::Copy(aOut, aUuid.u128.value, 16);
		break;
		}
	}

// ---------------------------------------------------------------- link state

TUint gConnectId = 0;           // the pending Connect request (one at a time)
TUint16 gConnecting = BLE_HS_CONN_HANDLE_NONE;
TBool gConnectWaitsEnc = EFalse;
TUint gPairId = 0;
TUint16 gPairConn = BLE_HS_CONN_HANDLE_NONE;
TInt gPairMethod = EPheBlePairJustWorks;

void Event(TInt aType, TUint16 aConn, TInt aError, const TDesC8& aValue = KNullDesC8)
	{
	TPheBleEvent ev;
	Mem::FillZ(&ev, sizeof(ev));
	ev.iType = aType;
	ev.iConn = aConn;
	ev.iError = aError;
	ev.iValueLength = aValue.Length();
	PheBleServerEvent(ev, aValue);
	}

int MtuDone(uint16_t aConn, const ble_gatt_error* aErr, uint16_t aMtu, void*)
	{
	(void)aErr;
	if (gConnectId && aConn == gConnecting)
		{
		TPckgBuf<TUint16> h(aConn);
		PheBleServerComplete(gConnectId, KErrNone, 0, h);
		gConnectId = 0;
		gConnecting = BLE_HS_CONN_HANDLE_NONE;
		TPheBleEvent ev;
		Mem::FillZ(&ev, sizeof(ev));
		ev.iType = EPheBleConnected;
		ev.iConn = aConn;
		PheBleServerEvent(ev, KNullDesC8);
		}
	TPheBleEvent ev;
	Mem::FillZ(&ev, sizeof(ev));
	ev.iType = EPheBleMtu;
	ev.iConn = aConn;
	ev.iMtu = aMtu ? aMtu : ble_att_mtu(aConn);
	PheBleServerEvent(ev, KNullDesC8);
	return 0;
	}

TBool PeerBonded(uint16_t aConn)
	{
	ble_gap_conn_desc desc;
	if (ble_gap_conn_find(aConn, &desc) != 0)
		return EFalse;
	ble_store_key_sec key;
	Mem::FillZ(&key, sizeof(key));
	key.peer_addr = desc.peer_id_addr;
	ble_store_value_sec value;
	return ble_store_read_peer_sec(&key, &value) == 0;
	}

void ConnectFailed(TInt aError)
	{
	if (gConnectId)
		PheBleServerComplete(gConnectId, aError, 0, KNullDesC8);
	gConnectId = 0;
	gConnecting = BLE_HS_CONN_HANDLE_NONE;
	gConnectWaitsEnc = EFalse;
	}

int GapConnEvent(ble_gap_event* aEvent, void*)
	{
	switch (aEvent->type)
		{
	case BLE_GAP_EVENT_CONNECT:
		if (aEvent->connect.status != 0)
			{
			OLOG("phebleops: connect failed %d", aEvent->connect.status);
			ConnectFailed(HsError(aEvent->connect.status));
			return 0;
			}
		gConnecting = aEvent->connect.conn_handle;
		OLOG("phebleops: connected, handle %d", gConnecting);
		if (PeerBonded(gConnecting))
			{
			gConnectWaitsEnc = ETrue;
			if (ble_gap_security_initiate(gConnecting) == 0)
				return 0;
			gConnectWaitsEnc = EFalse;
			}
		ble_gattc_exchange_mtu(gConnecting, MtuDone, NULL);
		return 0;

	case BLE_GAP_EVENT_DISCONNECT:
		{
		TUint16 conn = aEvent->disconnect.conn.conn_handle;
		OLOG("phebleops: disconnected, handle %d reason 0x%x", conn, aEvent->disconnect.reason);
		if (conn == gConnecting)
			ConnectFailed(KErrDisconnected);
		if (conn == gPairConn && gPairId)
			{
			PheBleServerComplete(gPairId, KErrDisconnected, 0, KNullDesC8);
			gPairId = 0;
			}
		TPheBleEvent ev;
		Mem::FillZ(&ev, sizeof(ev));
		ev.iType = EPheBleDisconnected;
		ev.iConn = conn;
		ev.iReason = aEvent->disconnect.reason;
		PheBleServerEvent(ev, KNullDesC8);
		return 0;
		}

	case BLE_GAP_EVENT_ENC_CHANGE:
		{
		TUint16 conn = aEvent->enc_change.conn_handle;
		TInt err = HsError(aEvent->enc_change.status);
		OLOG("phebleops: encryption change, handle %d status %d", conn, aEvent->enc_change.status);
		Event(EPheBleEncrypted, conn, err);
		if (conn == gConnecting && gConnectWaitsEnc)
			{
			gConnectWaitsEnc = EFalse;
			if (err != KErrNone)
				{
				ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
				ConnectFailed(err);
				}
			else
				ble_gattc_exchange_mtu(conn, MtuDone, NULL);
			}
		if (conn == gPairConn && gPairId)
			{
			TPheBleEvent ev;
			Mem::FillZ(&ev, sizeof(ev));
			ev.iType = EPheBlePaired;
			ev.iConn = conn;
			ev.iError = err;
			ev.iPairing = gPairMethod;
			PheBleServerEvent(ev, KNullDesC8);
			TPckgBuf<TInt> method(gPairMethod);
			PheBleServerComplete(gPairId, err, 0, method);
			gPairId = 0;
			gPairConn = BLE_HS_CONN_HANDLE_NONE;
			}
		return 0;
		}

	case BLE_GAP_EVENT_PASSKEY_ACTION:
		if (aEvent->passkey.params.action == BLE_SM_IOACT_NUMCMP)
			{
			gPairMethod = EPheBlePairNumericComparison;
			TPheBleEvent ev;
			Mem::FillZ(&ev, sizeof(ev));
			ev.iType = EPheBleNumericCompare;
			ev.iConn = aEvent->passkey.conn_handle;
			ev.iPasskey = aEvent->passkey.params.numcmp;
			PheBleServerEvent(ev, KNullDesC8);
			}
		else
			{
			// no display/keyboard path for passkey entry here: refuse
			ble_sm_io io;
			Mem::FillZ(&io, sizeof(io));
			io.action = aEvent->passkey.params.action;
			ble_sm_inject_io(aEvent->passkey.conn_handle, &io);
			}
		return 0;

	case BLE_GAP_EVENT_REPEAT_PAIRING:
		{
		// the peer lost its keys (or we did): forget ours and pair again
		ble_gap_conn_desc desc;
		if (ble_gap_conn_find(aEvent->repeat_pairing.conn_handle, &desc) == 0)
			ble_store_util_delete_peer(&desc.peer_id_addr);
		return BLE_GAP_REPEAT_PAIRING_RETRY;
		}

	case BLE_GAP_EVENT_NOTIFY_RX:
		{
		TInt len = OS_MBUF_PKTLEN(aEvent->notify_rx.om);
		HBufC8* value = HBufC8::New(len > 0 ? len : 1);
		if (value)
			{
			TPtr8 p = value->Des();
			p.SetLength(len);
			os_mbuf_copydata(aEvent->notify_rx.om, 0, len, (void*)p.Ptr());
			TPheBleEvent ev;
			Mem::FillZ(&ev, sizeof(ev));
			ev.iType = EPheBleNotification;
			ev.iConn = aEvent->notify_rx.conn_handle;
			ev.iHandle = aEvent->notify_rx.attr_handle;
			ev.iValueLength = len;
			PheBleServerEvent(ev, *value);
			delete value;
			}
		return 0;
		}

	case BLE_GAP_EVENT_MTU:
		{
		TPheBleEvent ev;
		Mem::FillZ(&ev, sizeof(ev));
		ev.iType = EPheBleMtu;
		ev.iConn = aEvent->mtu.conn_handle;
		ev.iMtu = aEvent->mtu.value;
		PheBleServerEvent(ev, KNullDesC8);
		return 0;
		}

	default:
		return 0;
		}
	}

// ---------------------------------------------------------------- discovery

struct TDisc
	{
	TUint iId;
	TUint16 iConn;
	TInt iCount;
	TInt iSvc;          // index of the service being walked (phase 2)
	TInt iChr;          // index of the characteristic whose descriptors are read (phase 3)
	TInt iServices;     // services found in phase 1
	TPheBleGattEntry iEntries[KMaxEntries];
	};
TDisc* gDisc = NULL;

void DiscDone(TInt aError)
	{
	if (!gDisc)
		return;
	TPtrC8 out(reinterpret_cast<const TUint8*>(gDisc->iEntries), gDisc->iCount * sizeof(TPheBleGattEntry));
	PheBleServerComplete(gDisc->iId, aError, 0, aError == KErrNone ? out : TPtrC8());
	User::Free(gDisc);
	gDisc = NULL;
	}

TPheBleGattEntry* NewEntry()
	{
	if (gDisc->iCount >= KMaxEntries)
		return NULL;
	TPheBleGattEntry* e = &gDisc->iEntries[gDisc->iCount++];
	Mem::FillZ(e, sizeof(*e));
	return e;
	}

void NextService();
void NextCharacteristic();

int DscFound(uint16_t, const ble_gatt_error* aErr, uint16_t, const ble_gatt_dsc* aDsc, void*)
	{
	if (!gDisc)
		return 1;
	if (aErr && aErr->status == BLE_HS_EDONE)
		{
		++gDisc->iChr;
		NextCharacteristic();
		return 0;
		}
	if (aErr && aErr->status != 0)
		{
		DiscDone(HsError(aErr->status));
		return 1;
		}
	if (TPheBleGattEntry* e = NewEntry())
		{
		e->iKind = TPheBleGattEntry::EDescriptor;
		e->iHandle = aDsc->handle;
		Uuid128(aDsc->uuid, e->iUuid);
		}
	return 0;
	}

void NextCharacteristic()
	{
	// walk the characteristics in entry order; each has its end handle set by phase 2
	while (gDisc->iChr < gDisc->iCount)
		{
		TPheBleGattEntry& c = gDisc->iEntries[gDisc->iChr];
		if (c.iKind == TPheBleGattEntry::ECharacteristic && c.iEndHandle > c.iHandle)
			{
			if (ble_gattc_disc_all_dscs(gDisc->iConn, c.iHandle, c.iEndHandle, DscFound, NULL) == 0)
				return;
			}
		++gDisc->iChr;
		}
	DiscDone(KErrNone);
	}

void SetCharacteristicEnds()
	{
	TUint16 svcEnd = 0;
	TInt last = -1;
	for (TInt i = 0; i < gDisc->iCount; ++i)
		{
		TPheBleGattEntry& e = gDisc->iEntries[i];
		if (e.iKind == TPheBleGattEntry::EService)
			{
			if (last >= 0)
				gDisc->iEntries[last].iEndHandle = svcEnd;
			last = -1;
			svcEnd = e.iEndHandle;
			}
		else if (e.iKind == TPheBleGattEntry::ECharacteristic)
			{
			if (last >= 0)
				gDisc->iEntries[last].iEndHandle = (TUint16)(e.iDeclHandle - 1);
			last = i;
			}
		}
	if (last >= 0)
		gDisc->iEntries[last].iEndHandle = svcEnd;
	}

int ChrFound(uint16_t, const ble_gatt_error* aErr, const ble_gatt_chr* aChr, void*)
	{
	if (!gDisc)
		return 1;
	if (aErr && aErr->status == BLE_HS_EDONE)
		{
		NextService();
		return 0;
		}
	if (aErr && aErr->status != 0)
		{
		DiscDone(HsError(aErr->status));
		return 1;
		}
	if (TPheBleGattEntry* e = NewEntry())
		{
		e->iKind = TPheBleGattEntry::ECharacteristic;
		e->iHandle = aChr->val_handle;
		e->iDeclHandle = aChr->def_handle;
		e->iProperties = aChr->properties;
		Uuid128(aChr->uuid, e->iUuid);
		}
	return 0;
	}

void NextService()
	{
	// phase 2: the characteristics of each service, entries appended after the services; then phase 3
	TInt services = gDisc->iServices;
	while (gDisc->iSvc < services)
		{
		TPheBleGattEntry svc = gDisc->iEntries[gDisc->iSvc++];
		// keep entries grouped: a service entry copy goes before its characteristics
		if (TPheBleGattEntry* e = NewEntry())
			*e = svc;
		if (ble_gattc_disc_all_chrs(gDisc->iConn, svc.iHandle, svc.iEndHandle, ChrFound, NULL) == 0)
			return;
		}
	// drop the first-pass service list (the grouped copies follow it)
	Mem::Move(gDisc->iEntries, gDisc->iEntries + services, (gDisc->iCount - services) * sizeof(TPheBleGattEntry));
	gDisc->iCount -= services;
	SetCharacteristicEnds();
	gDisc->iChr = 0;
	NextCharacteristic();
	}

int SvcFound(uint16_t, const ble_gatt_error* aErr, const ble_gatt_svc* aSvc, void*)
	{
	if (!gDisc)
		return 1;
	if (aErr && aErr->status == BLE_HS_EDONE)
		{
		gDisc->iSvc = 0;
		gDisc->iServices = gDisc->iCount;
		NextService();
		return 0;
		}
	if (aErr && aErr->status != 0)
		{
		DiscDone(HsError(aErr->status));
		return 1;
		}
	if (TPheBleGattEntry* e = NewEntry())
		{
		e->iKind = TPheBleGattEntry::EService;
		e->iHandle = aSvc->start_handle;
		e->iEndHandle = aSvc->end_handle;
		Uuid128(aSvc->uuid, e->iUuid);
		}
	return 0;
	}

// ---------------------------------------------------------------- read / write

int AttrDone(uint16_t, const ble_gatt_error* aErr, ble_gatt_attr* aAttr, void* aArg)
	{
	TUint id = (TUint)aArg;
	TInt local, att;
	GattError(aErr, local, att);
	if (local == KErrNone && att == 0 && aAttr && aAttr->om)
		{
		TInt len = OS_MBUF_PKTLEN(aAttr->om);
		HBufC8* value = HBufC8::New(len > 0 ? len : 1);
		if (value)
			{
			TPtr8 p = value->Des();
			p.SetLength(len);
			os_mbuf_copydata(aAttr->om, 0, len, (void*)p.Ptr());
			PheBleServerComplete(id, KErrNone, 0, *value);
			delete value;
			return 0;
			}
		local = KErrNoMemory;
		}
	PheBleServerComplete(id, local, att, KNullDesC8);
	return 0;
	}

void RunOp(TOpReq& r)
	{
	int rc = 0;
	switch (r.iOp)
		{
	case EPheBleConnect:
		{
		if (gConnectId)
			{
			PheBleServerComplete(r.iId, KErrInUse, 0, KNullDesC8);
			return;
			}
		ble_gap_disc_cancel();      // a running scan blocks the connection on many controllers
		uint8_t own = 0;
		ble_hs_id_infer_auto(0, &own);
		ble_addr_t peer;
		peer.type = r.iAddr.iType;
		Mem::Copy(peer.val, r.iAddr.iAddr, 6);
		gConnectId = r.iId;
		rc = ble_gap_connect(own, &peer, KConnectTimeoutMs, NULL, GapConnEvent, NULL);
		if (rc != 0)
			{
			gConnectId = 0;
			PheBleServerComplete(r.iId, HsError(rc), 0, KNullDesC8);
			}
		return;
		}
	case EPheBleDisconnect:
		rc = ble_gap_terminate(r.iConn, BLE_ERR_REM_USER_CONN_TERM);
		break;
	case EPheBlePair:
		{
		ble_gap_conn_desc desc;
		if (ble_gap_conn_find(r.iConn, &desc) == 0 && desc.sec_state.encrypted && desc.sec_state.bonded)
			{
			TPckgBuf<TInt> method(EPheBlePairAlreadyBonded);
			PheBleServerComplete(r.iId, KErrNone, 0, method);
			return;
			}
		if (gPairId)
			{
			PheBleServerComplete(r.iId, KErrInUse, 0, KNullDesC8);
			return;
			}
		gPairId = r.iId;
		gPairConn = r.iConn;
		gPairMethod = EPheBlePairJustWorks;
		rc = ble_gap_security_initiate(r.iConn);
		if (rc != 0)
			{
			gPairId = 0;
			PheBleServerComplete(r.iId, HsError(rc), 0, KNullDesC8);
			}
		return;
		}
	case EPheBleConfirmNumeric:
		{
		ble_sm_io io;
		Mem::FillZ(&io, sizeof(io));
		io.action = BLE_SM_IOACT_NUMCMP;
		io.numcmp_accept = r.iFlag ? 1 : 0;
		rc = ble_sm_inject_io(r.iConn, &io);
		break;
		}
	case EPheBleDeleteBond:
		{
		ble_addr_t peer;
		peer.type = r.iAddr.iType;
		Mem::Copy(peer.val, r.iAddr.iAddr, 6);
		rc = ble_store_util_delete_peer(&peer);
		break;
		}
	case EPheBleExchangeMtu:
		rc = ble_gattc_exchange_mtu(r.iConn, MtuDone, NULL);
		break;
	case EPheBleDiscoverAll:
		if (gDisc)
			{
			PheBleServerComplete(r.iId, KErrInUse, 0, KNullDesC8);
			return;
			}
		gDisc = static_cast<TDisc*>(User::AllocZ(sizeof(TDisc)));
		if (!gDisc)
			{
			PheBleServerComplete(r.iId, KErrNoMemory, 0, KNullDesC8);
			return;
			}
		gDisc->iId = r.iId;
		gDisc->iConn = r.iConn;
		rc = ble_gattc_disc_all_svcs(r.iConn, SvcFound, NULL);
		if (rc != 0)
			{
			gDisc->iId = r.iId;
			DiscDone(HsError(rc));
			}
		return;
	case EPheBleRead:
		rc = ble_gattc_read(r.iConn, r.iHandle, AttrDone, (void*)r.iId);
		if (rc != 0)
			break;
		return;
	case EPheBleWrite:
	case EPheBleSubscribe:
		{
		TUint8 cccd[2] = { (TUint8)r.iValue, (TUint8)(r.iValue >> 8) };
		const TUint8* data = r.iOp == EPheBleSubscribe ? cccd : r.iData;
		TInt len = r.iOp == EPheBleSubscribe ? 2 : r.iLen;
		if (r.iOp == EPheBleWrite && !r.iFlag)
			{
			rc = ble_gattc_write_no_rsp_flat(r.iConn, r.iHandle, data, (uint16_t)len);
			break;    // completes now: no response
			}
		rc = ble_gattc_write_flat(r.iConn, r.iHandle, data, (uint16_t)len, AttrDone, (void*)r.iId);
		if (rc != 0)
			break;
		return;
		}
	default:
		rc = BLE_HS_ENOTSUP;
		break;
		}
	PheBleServerComplete(r.iId, rc == BLE_HS_ENOTSUP ? KErrNotSupported : HsError(rc), 0, KNullDesC8);
	}

struct TOpNode
	{
	ble_npl_event iEv;
	TOpReq iReq;
	TUint8 iData[1];
	};

void OpHandler(ble_npl_event* aEv)
	{
	TOpNode* n = static_cast<TOpNode*>(ble_npl_event_get_arg(aEv));
	if (!ble_hs_synced())
		PheBleServerComplete(n->iReq.iId, KErrPheBleNotReady, 0, KNullDesC8);
	else
		RunOp(n->iReq);
	User::Free(n);
	}

} // namespace

TInt PheBleHostRequest(const TOpReq& aReq, const TDesC8& aData)
	{
	TOpNode* n = static_cast<TOpNode*>(User::Alloc(sizeof(TOpNode) + aData.Length()));
	if (!n)
		return KErrNoMemory;
	n->iReq = aReq;
	n->iReq.iLen = aData.Length();
	Mem::Copy(n->iData, aData.Ptr(), aData.Length());
	n->iReq.iData = n->iData;
	ble_npl_event_init(&n->iEv, OpHandler, n);
	ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &n->iEv);
	return KErrNone;
	}
