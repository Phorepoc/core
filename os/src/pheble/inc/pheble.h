// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheble.h -- OURS (BLE plan): the client API of the Phorepoc LE host (pheble.dll -> the "!PheBle" server that
// pheblehost.dll runs in the Bluetooth stack's process). Central role and GATT client: scan, connect, pair/bond,
// discover, read, write, subscribe, notifications. Every request is asynchronous (TRequestStatus); results come back
// as packaged structs. Attribute handles are the peer's; DiscoverAll lists them with their 128-bit UUIDs, so a client
// (e.g. the Pebble LE transport, phepebble_le.lib) maps its own ids to handles.
//
// Errors: Symbian codes, plus KErrPheBleAtt(code) for an ATT error from the peer: write/read completions carry it
// in TPheBleResult::iAttError separately from iError (the Pebble 2 answers its Trigger write with ATT 0x01 yet applies it).

#ifndef PHEBLE_H
#define PHEBLE_H

#include <e32std.h>

_LIT(KPheBleServerName, "!PheBle");
const TInt KPheBleMajor = 0;
const TInt KPheBleMinor = 1;

const TInt KErrPheBleBase = -18300;
const TInt KErrPheBleNotReady = KErrPheBleBase - 1;      // the controller is off / the host has not synced
const TInt KErrPheBleAttBase = KErrPheBleBase - 256;      // KErrPheBleAttBase - code
inline TInt KErrPheBleAtt(TInt aAttCode) { return KErrPheBleAttBase - aAttCode; }

/** An LE address, least significant byte first (HCI order). */
struct TPheBleAddr
	{
	TUint8 iAddr[6];
	TUint8 iType;            // 0 public, 1 random
	};

/** One advertising report (legacy advertising; the name is the Complete or Shortened Local Name). */
struct TPheBleAdvert
	{
	TPheBleAddr iAddr;
	TInt8 iRssi;
	TUint8 iEventType;
	TUint8 iDataLength;
	TUint8 iData[31];
	TBuf8<31> iName;
	};

/** One GATT entry of DiscoverAll: a service, a characteristic (iHandle = value handle, iDeclHandle) or a descriptor. */
struct TPheBleGattEntry
	{
	enum TKind { EService = 1, ECharacteristic = 2, EDescriptor = 3 };
	TUint8 iKind;
	TUint8 iProperties;      // characteristics: the ATT properties byte
	TUint16 iHandle;         // service: start; characteristic: value handle; descriptor: its handle
	TUint16 iEndHandle;      // service: end; characteristic: last handle of its descriptors
	TUint16 iDeclHandle;
	TUint8 iUuid[16];        // 128-bit, little-endian (16/32-bit UUIDs expanded with the Bluetooth base)
	};

enum TPheBleLinkEventType
	{
	EPheBleConnected = 1,     // iError, iConn
	EPheBleDisconnected,      // iReason (HCI reason)
	EPheBleEncrypted,         // iError (0 = link encrypted; bonded peers reach it before EPheBleConnected completes)
	EPheBlePaired,            // iError, iPairing (TPheBlePairing)
	EPheBleNumericCompare,    // iPasskey: answer with ConfirmNumeric
	EPheBleNotification,      // iHandle, value in the descriptor of NextEvent
	EPheBleMtu                // iMtu
	};

enum TPheBlePairing { EPheBlePairUnknown = 0, EPheBlePairJustWorks, EPheBlePairNumericComparison, EPheBlePairPasskey,
	EPheBlePairAlreadyBonded };

struct TPheBleEvent
	{
	TInt iType;               // TPheBleLinkEventType
	TInt iError;
	TUint16 iConn;
	TUint16 iHandle;
	TInt iReason;
	TInt iPairing;
	TUint32 iPasskey;
	TInt iMtu;
	TInt iValueLength;
	};

/** The completion of a read/write/subscribe. */
struct TPheBleResult
	{
	TInt iError;              // local / transport result
	TInt iAttError;           // the peer's ATT error code, 0 = none
	};

typedef TPckgBuf<TPheBleAdvert> TPheBleAdvertBuf;
typedef TPckgBuf<TPheBleEvent> TPheBleEventBuf;
typedef TPckgBuf<TPheBleResult> TPheBleResultBuf;
typedef TPckgBuf<TUint16> TPheBleConnBuf;

class RPheBle : public RSessionBase
	{
public:
	IMPORT_C TInt Connect();

	// scanning: passive unless aActive; adverts queue in the server (a slow client loses the oldest)
	IMPORT_C TInt StartScan(TBool aActive, TInt aTimeoutSeconds);
	IMPORT_C void NextAdvert(TPheBleAdvertBuf& aAdvert, TRequestStatus& aStatus);
	IMPORT_C TInt StopScan();

	// links: one session may own several; the server drops them when the session closes
	IMPORT_C void ConnectDevice(const TPheBleAddr& aAddr, TPheBleConnBuf& aConn, TRequestStatus& aStatus);
	IMPORT_C TInt Disconnect(TUint16 aConn);
	/** Link events (connected/disconnected/encrypted/paired/numeric comparison/notifications/MTU); aValue receives a
	    notification's value. */
	IMPORT_C void NextEvent(TPheBleEventBuf& aEvent, TDes8& aValue, TRequestStatus& aStatus);

	// security: bond as initiator; IO capability DisplayYesNo (numeric comparison when the peer can), Just Works otherwise
	/** Completes with the TPheBlePairing method (>= 0) or an error. */
	IMPORT_C void Pair(TUint16 aConn, TRequestStatus& aStatus);
	IMPORT_C TInt ConfirmNumeric(TUint16 aConn, TBool aAccept);
	IMPORT_C TInt DeleteBond(const TPheBleAddr& aAddr);

	// GATT client
	IMPORT_C void ExchangeMtu(TUint16 aConn, TRequestStatus& aStatus);
	/** All services, characteristics and descriptors into aEntries (an array of TPheBleGattEntry). */
	IMPORT_C void DiscoverAll(TUint16 aConn, TDes8& aEntries, TRequestStatus& aStatus);
	IMPORT_C void Read(TUint16 aConn, TUint16 aHandle, TDes8& aValue, TPheBleResultBuf& aResult, TRequestStatus& aStatus);
	IMPORT_C void Write(TUint16 aConn, TUint16 aHandle, const TDesC8& aValue, TBool aWithResponse,
		TPheBleResultBuf& aResult, TRequestStatus& aStatus);
	/** Write the CCCD at aCccdHandle (1 = notifications, 2 = indications, 0 = off). */
	IMPORT_C void Subscribe(TUint16 aConn, TUint16 aCccdHandle, TUint16 aValue, TPheBleResultBuf& aResult,
		TRequestStatus& aStatus);
	IMPORT_C TInt Mtu(TUint16 aConn);

	IMPORT_C void CancelAll();
	};

#endif // PHEBLE_H
