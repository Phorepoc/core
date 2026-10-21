// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// mcssathandler_null.cpp — OURS (not Symbian source): replaces menucontentsrv/extsrc/mcssathandler.cpp
// (mcssatnotifier.cpp stays the real TU).
//
// The real CMcsSatHandler::ConstructL does `iSatSession.ConnectL()`, and the Menu Content Service
// builds one unconditionally (CMenuSrvEngUtils::ConstructL) -- so without a SAT server the whole menu
// engine fails to construct. This board has no SIM, no SAT server: the handler answers "there is no
// SIM Application Toolkit application" (the same answers the real code gives for an empty SIM).
// The RSatSession / RSatIcon members are the real satclient classes, left unconnected.
#include <e32base.h>
#include "mcssathandler.h"

EXPORT_C CMcsSatHandler* CMcsSatHandler::NewL()
	{
	return new (ELeave) CMcsSatHandler();
	}

CMcsSatHandler::~CMcsSatHandler()
	{
	}

CMcsSatHandler::CMcsSatHandler()
	{
	}

EXPORT_C CAknIcon* CMcsSatHandler::LoadIconL()
	{
	return NULL;
	}

EXPORT_C TInt CMcsSatHandler::GetName(TDes& aName)
	{
	aName.Zero();
	return KErrNotFound;
	}

EXPORT_C TBool CMcsSatHandler::CheckVisibility()
	{
	return EFalse;
	}
