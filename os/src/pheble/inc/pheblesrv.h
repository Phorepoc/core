// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// pheblesrv.h -- OURS: internal to pheblehost.dll -- the seam between NimBLE's task (pheblehost.cpp, pheblestore.cpp)
// and the "!PheBle" server thread (pheblesrv.cpp), plus the IPC opcodes of pheble.h.

#ifndef PHEBLESRV_H
#define PHEBLESRV_H

#include <e32base.h>
#include "pheble.h"

enum TPheBleOp
	{
	EPheBleStartScan = 1,      // a0 active, a1 timeout s
	EPheBleNextAdvert,         // a0 TPheBleAdvertBuf
	EPheBleStopScan,
	EPheBleConnect,            // a0 TPckgC<TPheBleAddr>, a1 TPheBleConnBuf
	EPheBleDisconnect,         // a0 conn
	EPheBleNextEvent,          // a0 TPheBleEventBuf, a1 value
	EPheBlePair,               // a0 conn
	EPheBleConfirmNumeric,     // a0 conn, a1 accept
	EPheBleDeleteBond,         // a0 TPckgC<TPheBleAddr>
	EPheBleExchangeMtu,        // a0 conn
	EPheBleDiscoverAll,        // a0 conn, a1 entries
	EPheBleRead,               // a0 conn, a1 handle, a2 value, a3 result
	EPheBleWrite,              // a0 conn | handle << 16, a1 value, a2 with response, a3 result
	EPheBleSubscribe,          // a0 conn, a1 cccd | value << 16, a3 result
	EPheBleGetMtu,             // a0 conn
	EPheBleCancelAll
	};

/** A request for NimBLE's task (phebleops.cpp). */
struct TOpReq
	{
	TInt iOp;                  // TPheBleOp
	TUint iId;                 // the server's request id, echoed in PheBleServerComplete
	TUint16 iConn;
	TUint16 iHandle;
	TUint16 iValue;            // CCCD value
	TBool iFlag;               // write: with response; ConfirmNumeric: accept
	TPheBleAddr iAddr;
	TInt iLen;                 // set by PheBleHostRequest
	const TUint8* iData;       // set by PheBleHostRequest (its own copy)
	};

// NimBLE's task -> server thread (thread-safe; copies)
/** Complete request aId: local error, the peer's ATT error, data (read value, connection handle, discovery entries). */
void PheBleServerComplete(TUint aId, TInt aError, TInt aAttError, const TDesC8& aData);
/** A link event for every session (notifications carry aValue). */
void PheBleServerEvent(const TPheBleEvent& aEvent, const TDesC8& aValue);
void PheBleServerAdvert(const TPheBleAdvert& aAdvert);
void PheBleServerScanDone(TInt aReason);
void PheBleServerHostReset();
// started once by the host
TInt PheBleServerStart();
// server thread -> NimBLE's task
TInt PheBleHostScan(TBool aStart, TBool aActive, TInt aTimeoutMs);
TInt PheBleHostRequest(const TOpReq& aReq, const TDesC8& aData);
/** The ATT MTU of a connection (NimBLE takes its own lock: callable from the server thread). */
TInt PheBleHostMtu(TUint16 aConn);
// NimBLE's bond store
void PheBleStoreInit();

#endif
