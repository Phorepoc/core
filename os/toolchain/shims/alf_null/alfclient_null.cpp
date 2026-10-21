// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// alfclient_null.cpp — OURS (not Symbian source): a null ALF (UI accelerator / hitchcock) client
// with the REAL alfclientu.DEF ordinal table, built from the real alfclient.mmp (nosrc).
//
// RCL_3 Avkon talks to the ALF server under SYMBIAN_BUILD_GCE: akncapserver holds an
// RAlfDirectClient (screen blanking during layout switch) and kicks CAlfAsynchStartup, aknphysics
// uses MeasureFPS. There is no ALF server on this board (no GPU compositor), so the session is
// never connected and every call is a no-op. All other entry points stay loud unimpl stubs.
#include <alf/alfdirectclient.h>

EXPORT_C RAlfClientBase::RAlfClientBase(TInt aServiceUid)
	: iServiceUid(aServiceUid), iEnv(0), iCmdLine(0), iApa(0)
	{
	}

EXPORT_C TUid RAlfClientBase::ServiceUid() const
	{
	return TUid::Uid(iServiceUid);
	}

EXPORT_C RAlfDirectClient::RAlfDirectClient()
	: RAlfClientBase(KAlfServiceUid.iUid)
	{
	}

EXPORT_C void RAlfDirectClient::Disconnect()
	{
	Close();
	}

EXPORT_C void RAlfDirectClient::MeasureFPS(TInt32, TBool)
	{
	}

EXPORT_C void RAlfDirectClient::EnableLowMemoryState(TBool, TBool)
	{
	}

EXPORT_C TInt RAlfDirectClient::ForceSwRendering(TBool)
	{
	return KErrNotSupported;
	}

EXPORT_C TInt RAlfDirectClient::BlankScreen(TBool)
	{
	return KErrNotSupported;
	}

EXPORT_C void CAlfAsynchStartup::StartL()
	{
	}
