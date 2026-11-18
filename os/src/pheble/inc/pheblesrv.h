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
	EPheBleMtu,                // a0 conn
	EPheBleCancelAll
	};

// NimBLE's task -> server thread (thread-safe; copies)
void PheBleServerAdvert(const TPheBleAdvert& aAdvert);
void PheBleServerScanDone(TInt aReason);
void PheBleServerHostReset();
// started once by the host
TInt PheBleServerStart();
// server thread -> NimBLE's task
TInt PheBleHostScan(TBool aStart, TBool aActive, TInt aTimeoutMs);
// NimBLE's bond store
void PheBleStoreInit();

#endif
