// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// c32_null.cpp — OURS (not Symbian source): a null C32 (serial comms) client library with
// the REAL c32U.def ordinal table.
//
// The Uikon server (EIKSRVUI.CPP CEikServAppUiBase::InitializeL) starts the comms root
// server during UI boot and tolerates only KErrNone/KErrAlreadyExists. This board has no
// comms subsystem at all (no c32exe, no CSYs, no comms PDD/LDD — the following
// User::LoadPhysicalDevice(COMMS_PDD_NAME) already accepts KErrNotFound), so "nothing to
// start" is reported as success. RCommServ/RComm are the real client code: with no server,
// RCommServ::Connect() returns KErrNotFound. Replace with the real os.commsfw serial server
// when comms matters.
#include <e32std.h>

EXPORT_C TInt StartC32()
	{
	return KErrNone;
	}

// the other half of the replaced CC_UTL.CPP (verbatim behaviour): the server's name
const TDesC& KCommServerName(void)
	{
	_LIT(KCommServerNameLit, "!CommServer");
	return KCommServerNameLit;
	}
