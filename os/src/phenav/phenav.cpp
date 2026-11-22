// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phenav.cpp -- OURS: the nav.sh sequence (composition.md rounds 34-38) from inside the guest. After a settle delay it runs the phases idle,
// menu, switch, launch, idle2 with the keys the PC keyboard driver produces for nav.sh's scancodes (F10 = the Menu key EStdKeyApplication0,
// F7/F6 = the softkeys, the grey arrows), and logs "PH: [nav] <phase> begin/end" so a host script cuts the serial log per phase.
#include <e32std.h>
#include <e32debug.h>
#include <e32keys.h>
#include <w32std.h>

static RWsSession gWs;

static void Key(TInt aScan, TInt aHoldMs = 80)
	{
	TRawEvent e;
	e.Set(TRawEvent::EKeyDown, aScan, 0);
	gWs.SimulateRawEvent(e);
	gWs.Flush();
	User::After(aHoldMs * 1000);
	e.Set(TRawEvent::EKeyUp, aScan, 0);
	gWs.SimulateRawEvent(e);
	gWs.Flush();
	}

static void Press(TInt aScan, TInt aAfterMs)
	{
	Key(aScan);
	User::After(aAfterMs * 1000);
	}

static void Phase(const char* aName)
	{
	RDebug::Printf("PH: [nav] %s begin", aName);
	}

static void End(const char* aName)
	{
	RDebug::Printf("PH: [nav] %s end", aName);
	}

static void Run()
	{
	Phase("idle");
	User::After(15000000);
	End("idle");
	Phase("menu");
	Press(EStdKeyApplication0, 3000);
	for (TInt i = 0; i < 8; ++i)
		Press(EStdKeyDownArrow, 500);
	for (TInt i = 0; i < 8; ++i)
		Press(EStdKeyUpArrow, 500);
	Press(EStdKeyF7, 3000);
	End("menu");
	Phase("switch");
	for (TInt i = 0; i < 3; ++i)
		{
		Key(EStdKeyApplication0, 1500);		// a long press: the task switcher
		User::After(2000000);
		Press(EStdKeyF7, 2000);
		}
	End("switch");
	Phase("launch");
	Press(EStdKeyApplication0, 3000);
	Press(EStdKeyDownArrow, 1000);
	Press(EStdKeyUpArrow, 1000);
	Press(EStdKeyF6, 12000);
	Press(EStdKeyF7, 3000);
	End("launch");
	Phase("idle2");
	User::After(15000000);
	End("idle2");
	RDebug::Printf("PH: [nav] done");
	}

TInt E32Main()
	{
	// the home screen is up some time after SSM starts the noncritical list
	User::After(90000000);
	if (gWs.Connect() != KErrNone)
		{
		RDebug::Printf("PH: [nav] no window server session");
		return KErrNotReady;
		}
	Run();
	gWs.Close();
	return KErrNone;
	}
