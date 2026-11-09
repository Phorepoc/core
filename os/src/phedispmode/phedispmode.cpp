// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phedispmode.cpp -- OURS: the display-mode follower of the PC image (dynamic resolution, x86-64-port.md X4.139, hildon-port.md 7.39).
//
// The PC display HAL (DYNRES=1) changes its active size when the host window is resized (VMMDev) or a client asks (HAL 0x7102).
// Non-NGA WSERV learns a new size only from a client: a screen mode's attributes and SetScreenMode (CScreen::doSetScreenMode sends
// EEventScreenDeviceChanged to every client) -- on S60 that client is akncapserver for the phone's fixed modes; a resizable display has
// no such piece. This is it, with no window: a window server session and a screen device only (SetScreenMode needs WriteDeviceData,
// no window group). It waits on the display driver's change notification (the P&S property KPhDisplayGenKey of phhal.h, no polling)
// and, when the display generation changes, gives the desktop screen mode (the LAST
// wsini mode when there are more than the phone's two -- the S60 modes keep their phone sizes) the new size and re-selects it.
// The logic was in core's phedynres test client (which owns a full-screen window to draw its test grid); phedynres now only follows
// the event like any application. Started by SSM (non-critical list) or bootui; exits at once where the HAL function does not exist
// (a phone, a PC kernel without DYNRES).
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32svr.h>
#include <u32hal.h>
#include <w32std.h>
#include <e32property.h>

#include <phhal.h>

static TInt ReadActive(TInt& aW, TInt& aH, TInt& aGen)
	{
	TInt v[3] = { 0, 0, 0 };
	const TInt r = UserSvr::HalFunction(EHalGroupDisplay, EPhHalDisplayActive, v, NULL);
	aW = v[0]; aH = v[1]; aGen = v[2];
	return r;
	}

static void ApplySize(CWsScreenDevice& aScreen, TInt aW, TInt aH)
	{
	const TInt modes = aScreen.NumScreenModes();
	if (modes >= 3)
		aScreen.SetScreenMode(modes - 1);
	TSizeMode m = aScreen.GetCurrentScreenModeAttributes();
	m.iScreenSize = TSize(aW, aH);
	m.iScreenTwipsSize = TSize(aW * 48 / 7, aH * 48 / 7);		// the HAL's pixel density (210 dpi)
	m.iOrigin = TPoint(0, 0);
	m.iPointerCursorArea = TRect(0, 0, aW, aH);
	aScreen.SetCurrentScreenModeAttributes(m);
	aScreen.SetScreenMode(aScreen.CurrentScreenMode());
	}

static TInt Run()
	{
	TInt w, h, gen;
	if (ReadActive(w, h, gen) != KErrNone)
		{
		RDebug::Printf("[phedispmode] no dynamic-resolution display: nothing to follow");
		return KErrNone;
		}
	RWsSession ws;
	TInt r = ws.Connect();
	if (r != KErrNone) return r;
	CWsScreenDevice* screen = new CWsScreenDevice(ws);
	if (!screen || (r = screen->Construct()) != KErrNone) { delete screen; ws.Close(); return r ? r : KErrNoMemory; }
	RDebug::Printf("[phedispmode] following the display: active %dx%d gen %d, %d screen modes", w, h, gen, screen->NumScreenModes());
	// the display may have changed before this started (a host window resized during boot): the mode gets its size now
	if (w > 0 && h > 0)
		{
		ApplySize(*screen, w, h);
		ws.Flush();
		RDebug::Printf("[phedispmode] screen mode %d: %dx%d", screen->CurrentScreenMode(), screen->SizeInPixels().iWidth, screen->SizeInPixels().iHeight);
		}
	RProperty gen_prop;
	if ((r = gen_prop.Attach(KUidSystemCategory, KPhDisplayGenKey)) != KErrNone)
		{ delete screen; ws.Close(); return r; }
	for (;;)
		{
		TRequestStatus st;
		gen_prop.Subscribe(st);
		TInt nw, nh, ngen;
		// (a change between the last read and the subscription is caught here, not lost)
		if (ReadActive(nw, nh, ngen) == KErrNone && ngen != gen)
			{
			gen_prop.Cancel();
			User::WaitForRequest(st);
			gen = ngen;
			RDebug::Printf("[phedispmode] display now %dx%d (gen %d): screen mode", nw, nh, gen);
			ApplySize(*screen, nw, nh);
			ws.Flush();
			continue;
			}
		User::WaitForRequest(st);
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RProcess::Rendezvous(KErrNone);
	const TInt r = Run();
	delete cleanup;
	return r;
	}
