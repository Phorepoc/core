// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// inifile_board.cpp -- OURS: WSERV's REAL INIFILE.CPP (unmodified, #included) plus the UI renderer switch.
//
// One Wsini.ini carries both render-stage chains: a line starting "CLASSIC:" belongs to the Classic
// chain, "ALFRED:" to the Alfred (alf server + Hitchcock) chain, anything else to both. While the file is parsed, the lines of
// the other chain are blanked and the prefix of the chosen one is removed, so the rest of WSERV (PLUGINS, RENDERSTAGES, the
// plug-in sections) sees an ordinary Wsini.ini. The chain comes from the developer menu's CenRep key 4 (0xE7015A01), read here
// -- before any render stage exists, i.e. "at boot, before WSERV draws"; absent / unreadable = Classic. An ini file without any
// prefixed line (a UI_RENDERER=classic|alfred ROM) never reads the key. The answer is published for the alf clients
// (ph_renderer.h) so they can tell "no alf server because Classic" from "the server is not up yet".
#include <e32std.h>
#include <e32debug.h>
#include <e32property.h>
#include <centralrepository.h>
#include "ph_renderer.h"
#include "server.h"
#include "inifile.h"

static TInt PhIniRenderer()
	{
	static TInt mode = -1;
	if (mode >= 0)
		return mode;
	mode = 0;
	TInt key = -1, err = KErrNone;
	CRepository* repo = NULL;
	TRAP(err, repo = CRepository::NewL(TUid::Uid(PH_DEVMENU_REPO)));
	if (err == KErrNone)
		{
		err = repo->Get(PH_DEVMENU_KEY_RENDERER, key);
		delete repo;
		if (err == KErrNone && key == 1)
			mode = 1;
		}
	const TUid cat = TUid::Uid(PH_RENDERER_PS_CATEGORY);
	TInt r = RProperty::Define(cat, PH_RENDERER_PS_KEY, RProperty::EInt, TSecurityPolicy(TSecurityPolicy::EAlwaysPass), TSecurityPolicy(ECapabilityWriteDeviceData));
	if (r == KErrNone || r == KErrAlreadyExists)
		r = RProperty::Set(cat, PH_RENDERER_PS_KEY, mode);
	RDebug::Printf("PH: wserv: UI renderer key 4 = %d (err %d) -> %s render stages (property %d)", key, err, mode ? "ALFRED" : "CLASSIC", r);
	return mode;
	}

static void PhIniFilter(TDes& aLine)
	{
	TInt which = 0, len = 0;
	if (aLine.Left(8).CompareF(_L("CLASSIC:")) == 0)
		{ which = 1; len = 8; }
	else if (aLine.Left(7).CompareF(_L("ALFRED:")) == 0)
		{ which = 2; len = 7; }
	if (!which)
		return;
	aLine.Delete(0, len);
	aLine.TrimLeft();
	if (which != PhIniRenderer() + 1)
		aLine.Zero();
	}

#define TrimAll() TrimAll(); PhIniFilter(readLine)
#ifdef PH_WS_NGA
#include <symbian-2010/oss.FCL.sf.os.graphics/windowing/windowserver/nga/SERVER/INIFILE.CPP>
#else
#include <symbian-2010/oss.FCL.sf.os.graphics/windowing/windowserver/nonnga/SERVER/inifile.cpp>
#endif
