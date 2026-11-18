// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_transport.h -- OURS: the narrow LE transport the Pebble companion needs, implemented by the Phorepoc LE host
// (NimBLE). Central role, GATT client only: scan, connect, pair (bond), read, write, subscribe, notifications.
//
// All characteristics the companion touches are Pebble UUIDs XXXXXXXX-328E-0FBB-C642-1AA6699BDADA, so a characteristic is
// named by its 32-bit short id (KPebbleChar*); the transport expands it, finds the handle on the connected peer (any
// service) and keeps that mapping per connection. Every call is asynchronous: it returns KErrNone when the request was
// queued (or an error at once) and completes through MPebbleLeObserver on the caller's thread. One request of each
// kind at a time is enough for the companion; a transport may queue more.

#ifndef PHEPEBBLE_TRANSPORT_H
#define PHEPEBBLE_TRANSPORT_H

#include <e32def.h>
#include <e32err.h>

// Pebble Pairing Service (16-bit 0xFED9) and the reversed (watch-hosted) PPoGATT service, as 32-bit short ids.
const TUint32 KPebbleCharConnStatus  = 0x00000001;  // read, notify: 4 bytes (bonded/encrypted/... bits)
const TUint32 KPebbleCharTrigger     = 0x00000002;  // write: pairing trigger flags
const TUint32 KPebbleCharRevData     = 0x30000004;  // notify: watch -> phone PPoGATT packets
const TUint32 KPebbleCharRevMeta     = 0x30000005;  // read
const TUint32 KPebbleCharRevDataWr   = 0x30000006;  // write without response: phone -> watch PPoGATT packets

// Trigger flags (Pebble Pairing Service, PairingTriggerRequestData)
const TUint8 KPebbleTriggerNoSecurityRequest = 0x02;
const TUint8 KPebbleTriggerReversedPPoGatt  = 0x10;

// Connectivity status, byte 0
const TUint8 KPebbleStatusBonded    = 0x02;
const TUint8 KPebbleStatusEncrypted = 0x04;
const TUint8 KPebbleStatusReversed  = 0x20;

const TInt KPebbleAddrSize = 6;

/** A Bluetooth LE device address, least significant byte first (HCI order), plus the address type. */
struct TPebbleLeAddr
	{
	TUint8 iAddr[KPebbleAddrSize];
	TUint8 iRandom;           // 0 = public, 1 = random
	};

/** How a pairing ended up (for the log and the UI). */
enum TPebblePairingMethod
	{
	EPebblePairingUnknown = 0,
	EPebblePairingJustWorks,
	EPebblePairingNumericComparison,
	EPebblePairingPasskey,
	EPebblePairingAlreadyBonded
	};

class MPebbleLeObserver
	{
public:
	/** A device advertising a name that starts with the scan prefix. */
	virtual void LeDeviceFound(const TPebbleLeAddr& aAddr, const TUint8* aName, TInt aNameLength, TInt aRssi) = 0;
	virtual void LeScanStopped(TInt aError) = 0;
	/** Connection up (KErrNone, services discovered, MTU exchanged) or failed. */
	virtual void LeConnected(TInt aError) = 0;
	virtual void LeDisconnected(TInt aReason) = 0;
	/** Pairing finished: KErrNone and the method, or an error. Bonded peers reconnect encrypted without a new pairing. */
	virtual void LePaired(TInt aError, TPebblePairingMethod aMethod) = 0;
	virtual void LeReadDone(TUint32 aChar, TInt aError, const TUint8* aValue, TInt aLength) = 0;
	/** A write with response finished. aAttError = the ATT error code from the peer (0 = none), aError = KErrNone or a
	    local/transport error. (The Pebble 2 firmware answers every Trigger write with ATT 0x01 yet applies it.) */
	virtual void LeWriteDone(TUint32 aChar, TInt aError, TInt aAttError) = 0;
	virtual void LeSubscribed(TUint32 aChar, TInt aError) = 0;
	virtual void LeNotification(TUint32 aChar, const TUint8* aValue, TInt aLength) = 0;
	};

class MPebbleLeTransport
	{
public:
	virtual ~MPebbleLeTransport() {}
	/** Report devices whose advertised name starts with aNamePrefix until StopScan() or aTimeoutSeconds. */
	virtual TInt StartScan(const TUint8* aNamePrefix, TInt aPrefixLength, TInt aTimeoutSeconds) = 0;
	virtual void StopScan() = 0;
	virtual TInt Connect(const TPebbleLeAddr& aAddr) = 0;
	virtual void Disconnect() = 0;
	/** Pair and bond as initiator (Just Works acceptable; LE Secure Connections preferred when the peer offers it). */
	virtual TInt Pair() = 0;
	virtual TInt Read(TUint32 aChar) = 0;
	virtual TInt Write(TUint32 aChar, const TUint8* aValue, TInt aLength, TBool aWithResponse) = 0;
	virtual TInt Subscribe(TUint32 aChar, TBool aNotifications) = 0;
	/** The negotiated ATT MTU (23 before the exchange). */
	virtual TInt Mtu() const = 0;
	};

#endif
