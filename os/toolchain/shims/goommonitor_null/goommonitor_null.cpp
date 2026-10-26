// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// goommonitor_null.cpp — OURS (not Symbian source): a null graphics-OOM monitor with the REAL
// goommonitoru.DEF ordinal table.
//
// Under SYMBIAN_BUILD_GCE akncapserver (AknCapServerEntry.cpp StartOOM) also creates the GOOM
// watcher thread. The real one belongs to the UI accelerator (ALF / hitchcock) and watches GPU
// memory through EGL extensions -- none of which exists on this board (no ALF, no GPU), so
// there is nothing to watch. The session answers like a monitor with nothing to free; the other entry
// points stay loud unimpl stubs.
#include <e32std.h>
#include <e32debug.h>

class RGOomMonitorSession : public RSessionBase
    {
public:
    IMPORT_C TInt Connect();
    IMPORT_C void RequestFreeMemory(TInt aBytesRequired, TRequestStatus& aStatus);	// ordinal 5
    IMPORT_C void CancelRequestFreeMemory();									// ordinal 9
#ifndef PH_ALF_GOOM
    IMPORT_C TBool IsConnected();					// ordinal 23
    IMPORT_C TInt RequestFreeMemory(TInt aBytesRequired);	// ordinal 4
#endif
#ifdef PH_ALF_GOOM
    IMPORT_C TInt Connect(TRequestStatus& aStatus);	// ordinal 26: CAlfAsynchGoomSession (alfrenderstage, in WSERV)
    IMPORT_C TBool IsConnected();
    IMPORT_C TInt RequestFreeMemory(TInt aBytesRequired);	// ordinal 4 (a game's start-up memory request)
    IMPORT_C void MemoryAllocationsComplete();
#endif
    };

// Connect: KErrNone, as the real one ("always return KErrNone for now" -- it never reports a missing server). Photos'
// full-screen view (CGlxRelaseGPUMemory) leaves on an error and the view never opened (2026-10-26). The task switcher
// then asks RequestFreeMemory, which answers at once below.
EXPORT_C TInt RGOomMonitorSession::Connect()
	{
	return KErrNone;
	}

// the async request (Photos before decoding a full-size image): nothing to free on this board, done at once
EXPORT_C void RGOomMonitorSession::RequestFreeMemory(TInt, TRequestStatus& aStatus)
	{
	TRequestStatus* s = &aStatus;
	User::RequestComplete(s, KErrNone);
	}

EXPORT_C void RGOomMonitorSession::CancelRequestFreeMemory()
	{
	}

#ifndef PH_ALF_GOOM
// a game (binary 0x7ab4cc): Connect(); if (!IsConnected()) fail; then up to 20 x { RequestFreeMemory(20 MB) until 0 }.
// With the old unimplemented stubs it never got a 0 and the game idled without a frame. Nothing to free on this board, so the
// request "succeeds" at once; IsConnected is the only way that game tells a real monitor from none (Connect's result is ignored).
EXPORT_C TBool RGOomMonitorSession::IsConnected()
	{
	return ETrue;
	}

EXPORT_C TInt RGOomMonitorSession::RequestFreeMemory(TInt)
	{
	return KErrNone;
	}
#endif

#ifdef PH_ALF_GOOM	// the "goommonitor_alfnull" twin (ALF=s2): the default ROM's null stays byte-identical
// ALF=: alfrenderstage's async session asks "Connect(status)"; it was an unimpl stub (never called in the default ROM).
// Synchronous KErrNotSupported = no request is pending, the stage carries on without a GOOM server.
EXPORT_C TInt RGOomMonitorSession::Connect(TRequestStatus&)
	{
	return KErrNotSupported;
	}

EXPORT_C TBool RGOomMonitorSession::IsConnected()
	{
	// WSERV's alfrenderstage keeps "no GOOM server"; every OTHER client (a game's start-up check: Connect, IsConnected, RequestFreeMemory x20)
	// is told "connected" -- with EFalse a game exits at once (the ROM carries THIS twin when ALF>=2)
	RDebug::Printf("PH: goom IsConnected sid=%x", (TUint)RProcess().SecureId().iId);
	return RProcess().SecureId().iId != 0x10003B20;
	}

EXPORT_C TInt RGOomMonitorSession::RequestFreeMemory(TInt)
	{
	RDebug::Printf("PH: goom RequestFreeMemory -> 0");
	return KErrNone;
	}

EXPORT_C void RGOomMonitorSession::MemoryAllocationsComplete()
	{
	}

#endif

EXPORT_C void CreateGOOMWatcherThreadL()
	{
	}
