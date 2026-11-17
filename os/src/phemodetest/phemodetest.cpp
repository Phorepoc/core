// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phemodetest.cpp -- OURS: the R1 test client of the per-client screen mode (etesian-port.md section 29).
// After the system is up it lists the window server's screen modes, switches the GLOBAL mode to the last one (the test image's wsini names a
// mode no Avkon layout pack has a size for), and a while later back to the first. What it proves is in the logs of the OTHER clients: with
// the stock server every Avkon client re-lays out at the odd size and the servers panic CdlEng 7; with per-client modes (wsini LEGACY_APPMODE)
// they keep their own mode and never hear of the change. Its own view: SizeInPixels and CurrentScreenMode answer for this client.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>

static TInt Run()
	{
	RWsSession ws;
	TInt r = ws.Connect();
	if (r != KErrNone)
		return r;
	CWsScreenDevice* screen = new CWsScreenDevice(ws);
	if (!screen || (r = screen->Construct()) != KErrNone)
		{
		delete screen;
		ws.Close();
		return r ? r : KErrNoMemory;
		}
	const TInt n = screen->NumScreenModes();
	for (TInt i = 0; i < n; ++i)
		{
		TPixelsAndRotation pr;
		screen->GetScreenModeSizeAndRotation(i, pr);
		RDebug::Printf("PH: [modetest] mode %d: %dx%d rotation %d", i, pr.iPixelSize.iWidth, pr.iPixelSize.iHeight, pr.iRotation);
		}
	User::After(90 * 1000000);		// the home screen is up by 60 s
	RDebug::Printf("PH: [modetest] before: current mode %d, size %dx%d", screen->CurrentScreenMode(), screen->SizeInPixels().iWidth, screen->SizeInPixels().iHeight);
	const TInt first = screen->CurrentScreenMode();
	screen->SetScreenMode(n - 1);
	ws.Flush();
	RDebug::Printf("PH: [modetest] switched to mode %d: current mode %d, size %dx%d", n - 1, screen->CurrentScreenMode(), screen->SizeInPixels().iWidth,
				   screen->SizeInPixels().iHeight);
	User::After(30 * 1000000);
	screen->SetScreenMode(first);
	ws.Flush();
	RDebug::Printf("PH: [modetest] back to mode %d: current mode %d, size %dx%d", first, screen->CurrentScreenMode(), screen->SizeInPixels().iWidth,
				   screen->SizeInPixels().iHeight);
	User::After(30 * 1000000);
	delete screen;
	ws.Close();
	return KErrNone;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RProcess::Rendezvous(KErrNone);
	const TInt r = Run();
	delete cleanup;
	return r;
	}
