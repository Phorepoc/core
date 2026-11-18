// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_ppogatt.h -- OURS: the phone side of reversed PPoGATT (the watch hosts the service; we are the GATT client).
//
// Packet = 1 header byte (bits 0-2 type: 0 data, 1 ack, 2 reset request, 3 reset complete; bits 3-7 sequence mod 32) +
// payload. The watch subscribes nothing here: we subscribe its Data characteristic (notifications in) and write its
// Data-write characteristic (writes without response out). The watch's PPoGATT client starts with a reset request
// {u8 version, serial}; we answer reset complete {u8 our rx window, u8 our tx window} (v1); the watch answers its own
// reset complete {its rx window, its tx window}; then data flows, each data packet acked by sequence number (acks may be
// cumulative). Facts from the PebbleOS source (subsys/pp/transports/gatt, Apache-2.0) and a real Pebble 2 HR session.
//
// Portable (only e32def/e32err): timers come in as Tick() calls every KPPoGattTickMs, so it runs the same in the
// service and in the host test.

#ifndef PHEPEBBLE_PPOGATT_H
#define PHEPEBBLE_PPOGATT_H

#include "phepebble_transport.h"

const TInt KPPoGattTickMs = 2000;
const TInt KPPoGattTimeoutTicks = 3;       // unacked for this many ticks -> resend the window
const TInt KPPoGattMaxTimeouts = 2;        // then give up: the session closes
const TUint8 KPPoGattOurRxWindow = 25;
const TUint8 KPPoGattOurTxWindow = 25;
const TInt KPPoGattMaxWindow = 16;
const TInt KPPoGattMaxPacket = 244;        // payload bytes per packet we send (MTU - 4 caps it lower)
const TInt KPPoGattTxBuffer = 8192;

class MPPoGattObserver
	{
public:
	virtual void PPoGattOpened() = 0;
	/** In-order Pebble Protocol stream bytes from the watch (any split; feed a TPebbleFrameReader). */
	virtual void PPoGattData(const TUint8* aData, TInt aLength) = 0;
	/** The session ended (KErrTimedOut after repeated timeouts, KErrDisconnected, or a reset by the watch). */
	virtual void PPoGattClosed(TInt aReason) = 0;
	};

class TPPoGattClient
	{
public:
	TPPoGattClient(MPebbleLeTransport& aTransport, MPPoGattObserver& aObserver);
	/** Connected and bonded: subscribe to the watch's Data characteristic; the watch then starts with a reset request. */
	TInt Start();
	void Stop();
	TBool IsOpen() const { return iState == EOpen; }

	/** Queue Pebble Protocol bytes (whole frames) for the watch. KErrOverflow when the buffer is full. */
	TInt Send(const TUint8* aData, TInt aLength);

	// Feed from the transport observer
	void HandleNotification(const TUint8* aValue, TInt aLength);
	void HandleSubscribed(TInt aError);
	void Tick();

	// For tests and logs
	TInt TxWindow() const { return iTxWindow; }
	TInt InFlight() const { return iInFlightCount; }
	TInt Resends() const { return iResends; }

private:
	enum TState { EIdle, ESubscribing, EWaitResetRequest, EWaitResetComplete, EOpen };
	enum { ETypeData = 0, ETypeAck = 1, ETypeResetRequest = 2, ETypeResetComplete = 3 };
	void Pump();
	void SendControl(TUint8 aType, TUint8 aSeq, const TUint8* aPayload, TInt aLength);
	void ResetLink();
	void Close(TInt aReason);
	TInt PacketPayload() const;

	MPebbleLeTransport& iTransport;
	MPPoGattObserver& iObserver;
	TState iState;
	TUint8 iVersion;
	TInt iTxWindow;
	TUint8 iTxSeq;            // next sequence number to send
	TUint8 iRxSeq;            // next sequence number expected
	// unsent stream bytes (ring)
	TUint8 iTx[KPPoGattTxBuffer];
	TInt iTxHead, iTxCount;
	// sent, unacked packets (oldest first), kept for resending
	TUint8 iInFlight[KPPoGattMaxWindow][KPPoGattMaxPacket + 1];
	TInt iInFlightLen[KPPoGattMaxWindow];
	TInt iInFlightFirst, iInFlightCount;
	TInt iIdleTicks, iTimeouts, iResends;
	};

#endif
