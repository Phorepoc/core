// goommonitor_null.cpp — OURS (not Symbian source): a null graphics-OOM monitor with the REAL
// goommonitoru.DEF ordinal table.
//
// Under SYMBIAN_BUILD_GCE akncapserver (AknCapServerEntry.cpp StartOOM) also creates the GOOM
// watcher thread. The real one belongs to the UI accelerator (ALF / hitchcock) and watches GPU
// memory through EGL extensions -- none of which exists on this board (no ALF, no GPU), so
// there is nothing to watch. All other entry points stay loud unimpl stubs, except the session's
// Connect: "no GOOM server" (KErrNotSupported) is an answer every client handles (the task switcher
// asks for free graphics memory before showing, 2026-09-30).
#include <e32std.h>

class RGOomMonitorSession : public RSessionBase
    {
public:
    IMPORT_C TInt Connect();
#ifdef PH_ALF_GOOM
    IMPORT_C TInt Connect(TRequestStatus& aStatus);	// ordinal 26: CAlfAsynchGoomSession (alfrenderstage, in WSERV)
    IMPORT_C TBool IsConnected();
    IMPORT_C void MemoryAllocationsComplete();
#endif
    };

EXPORT_C TInt RGOomMonitorSession::Connect()
	{
	return KErrNotSupported;
	}

#ifdef PH_ALF_GOOM	// the "goommonitor_alfnull" twin (ALF=s2): the default ROM's null stays byte-identical
// ALF=: alfrenderstage's async session asks "Connect(status)"; it was an unimpl stub (never called in the default ROM).
// Synchronous KErrNotSupported = no request is pending, the stage carries on without a GOOM server.
EXPORT_C TInt RGOomMonitorSession::Connect(TRequestStatus&)
	{
	return KErrNotSupported;
	}

EXPORT_C TBool RGOomMonitorSession::IsConnected()
	{
	return EFalse;
	}

EXPORT_C void RGOomMonitorSession::MemoryAllocationsComplete()
	{
	}

#endif

EXPORT_C void CreateGOOMWatcherThreadL()
	{
	}
