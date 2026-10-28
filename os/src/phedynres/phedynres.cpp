// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phedynres.cpp -- OURS: the dynamic-resolution test client.
//
// The PC display HAL (built with DYNRES) keeps a shadow screen of the largest size and shows its top-left
// "active" part; a host window resize, or this client's keys, changes the active size and the real framebuffer
// mode. This client is the piece that tells the window server: it polls the HAL's active size (function 0x7101),
// and when it changed it sets the current screen mode's attributes (size, twips, pointer area) and re-selects
// the mode, so WSERV's CScreen runs its normal screen-mode-change path (doSetScreenMode: device-changed events
// to every client, focus reset). Then it does what any application does on EEventScreenDeviceChanged: refreshes
// its screen device (SetAppScreenMode) and re-lays out -- here a full-screen window with a grid, a border, the
// size, and a count of the changes.
//
// Keys (to test where the host cannot resize the window, e.g. QEMU): 1 = 800x600, 2 = 1024x768, 3 = 1280x720,
// 4 = 1600x900, 5 = 640x360, 6 = 360x640 (a request to the HAL: HAL 0x7102).
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32svr.h>
#include <hal.h>
#include <u32hal.h>
#include <w32std.h>
#include <fbs.h>

#include <phhal.h>
static const TInt KHalPhoreActive = EPhHalDisplayActive;
static const TInt KHalPhoreRequest = EPhHalDisplayRequest;
static const TInt KPollUs = 150000;

struct SSizes { TInt iW, iH; };
static const SSizes KKeySizes[] = { {800,600}, {1024,768}, {1280,720}, {1600,900}, {640,360}, {360,640} };

static TInt ReadActive(TInt& aW, TInt& aH, TInt& aGen)
	{
	TInt v[3] = { 0, 0, 0 };
	TInt r = UserSvr::HalFunction(EHalGroupDisplay, KHalPhoreActive, v, NULL);
	aW = v[0];  aH = v[1];  aGen = v[2];
	return r;
	}

static void DrawScene(CWindowGc& aGc, RWindow& aWin, CFont* aFont, TInt aChanges, TInt aGen)
	{
	const TSize size = aWin.Size();
	aGc.Activate(aWin);
	aWin.BeginRedraw();
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(TRgb(0x14, 0x20, 0x40));
	aGc.DrawRect(TRect(size));
	// the grid, every 50 px (every 250 px brighter)
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
	for (TInt x = 0; x < size.iWidth; x += 50)
		{
		aGc.SetPenColor((x % 250) ? TRgb(0x30, 0x44, 0x70) : TRgb(0x70, 0x90, 0xD0));
		aGc.DrawLine(TPoint(x, 0), TPoint(x, size.iHeight));
		}
	for (TInt y = 0; y < size.iHeight; y += 50)
		{
		aGc.SetPenColor((y % 250) ? TRgb(0x30, 0x44, 0x70) : TRgb(0x70, 0x90, 0xD0));
		aGc.DrawLine(TPoint(0, y), TPoint(size.iWidth, y));
		}
	// the border (3 px, yellow) and the diagonals: the screen edge is the window edge
	aGc.SetPenColor(KRgbYellow);
	aGc.SetPenSize(TSize(3, 3));
	aGc.DrawRect(TRect(1, 1, size.iWidth - 1, size.iHeight - 1));
	aGc.SetPenSize(TSize(1, 1));
	aGc.SetPenColor(TRgb(0xE0, 0x60, 0x60));
	aGc.DrawLine(TPoint(0, 0), TPoint(size.iWidth, size.iHeight));
	aGc.SetPenColor(TRgb(0x60, 0xE0, 0x60));
	aGc.DrawLine(TPoint(size.iWidth, 0), TPoint(0, size.iHeight));
	// the numbers
	if (aFont)
		{
		aGc.UseFont(aFont);
		aGc.SetPenColor(KRgbWhite);
		TBuf<96> t;
		t.Format(_L("Screen %d x %d"), size.iWidth, size.iHeight);
		aGc.DrawText(t, TPoint(20, 60));
		t.Format(_L("change %d (display generation %d)"), aChanges, aGen);
		aGc.DrawText(t, TPoint(20, 100));
		aGc.DrawText(_L("keys 1-6 ask for another size"), TPoint(20, 140));
		aGc.DiscardFont();
		}
	aWin.EndRedraw();
	aGc.Deactivate();
	}

// tell WSERV the display now has this size: the desktop screen mode takes it (the LAST wsini mode when there are more than the
// phone's two -- the S60 modes keep their phone sizes; else the current mode), then the mode is re-selected
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
	aScreen.SetAppScreenMode(aScreen.CurrentScreenMode());		// what an application does on EEventScreenDeviceChanged
	}

static void MainL()
	{
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	User::LeaveIfError(screen->Construct());
	CWindowGc* gc = NULL;
	User::LeaveIfError(screen->CreateContext(gc));
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x50445231, ETrue));
	group.EnableScreenChangeEvents();
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x50445232));
	win.SetBackgroundColor(KRgbBlack);
	win.Activate();

	CFont* font = NULL;
	TFontSpec spec(_L("Arial"), 240);
	screen->GetNearestFontToDesignHeightInTwips(font, spec);

	TInt aw = 0, ah = 0, gen = -1, changes = 0;
	ReadActive(aw, ah, gen);
	RDebug::Printf("[phedynres] physical %dx%d, active %dx%d gen %d", screen->SizeInPixels().iWidth,
		screen->SizeInPixels().iHeight, aw, ah, gen);
	if (aw > 0 && ah > 0)
		ApplySize(*screen, aw, ah);
	TSize size = screen->SizeInPixels();
	win.SetExtent(TPoint(0, 0), size);
	RDebug::Printf("[phedynres] mode size %dx%d", size.iWidth, size.iHeight);
	DrawScene(*gc, win, font, changes, gen);
	ws.Flush();

	RTimer timer;
	timer.CreateLocal();
	TRequestStatus redrawSt, eventSt, timerSt;
	ws.RedrawReady(&redrawSt);
	ws.EventReady(&eventSt);
	timer.After(timerSt, KPollUs);
	TInt keyPresses = 0;
	// the self-test walk (a host window cannot be resized in QEMU and the keys need focus): after KWalkStartTicks polls the
	// client asks for each size of KWalk in turn, KWalkStepTicks polls apart
	static const SSizes KWalk[] = { {1280,720}, {1600,900}, {1024,768}, {640,360}, {360,640}, {1280,720} };
	static const TInt KWalkStartTicks = 60, KWalkStepTicks = 50;
	TInt ticks = 0, walked = 0;
	for (;;)
		{
		User::WaitForAnyRequest();
		if (timerSt != KRequestPending)
			{
			timer.After(timerSt, KPollUs);
			++ticks;
			if (walked < (TInt)(sizeof(KWalk) / sizeof(KWalk[0])) && ticks >= KWalkStartTicks + walked * KWalkStepTicks)
				{
				RDebug::Printf("[phedynres] walk: asking for %dx%d", KWalk[walked].iW, KWalk[walked].iH);
				UserSvr::HalFunction(EHalGroupDisplay, KHalPhoreRequest, (TAny*)KWalk[walked].iW, (TAny*)KWalk[walked].iH);
				++walked;
				}
			TInt w, h, g;
			if (ReadActive(w, h, g) == KErrNone && g != gen && w > 0 && h > 0)
				{
				gen = g;
				++changes;
				RDebug::Printf("[phedynres] display changed: %dx%d (gen %d)", w, h, g);
				ApplySize(*screen, w, h);
				size = screen->SizeInPixels();
				win.SetExtent(TPoint(0, 0), size);
				win.Invalidate();
				RDebug::Printf("[phedynres] WSERV screen now %dx%d, window %dx%d", size.iWidth, size.iHeight,
					win.Size().iWidth, win.Size().iHeight);
				DrawScene(*gc, win, font, changes, gen);
				ws.Flush();
				}
			}
		else if (redrawSt != KRequestPending)
			{
			TWsRedrawEvent rev;
			ws.GetRedraw(rev);
			DrawScene(*gc, win, font, changes, gen);
			ws.RedrawReady(&redrawSt);
			}
		else if (eventSt != KRequestPending)
			{
			TWsEvent ev;
			ws.GetEvent(ev);
			if (ev.Type() == EEventScreenDeviceChanged)
				RDebug::Printf("[phedynres] EEventScreenDeviceChanged (mode %d size %dx%d)", screen->CurrentScreenMode(),
					screen->SizeInPixels().iWidth, screen->SizeInPixels().iHeight);
			else if (ev.Type() == EEventKey && ev.Key()->iCode >= '1' && ev.Key()->iCode <= '6')
				{
				const SSizes& s = KKeySizes[ev.Key()->iCode - '1'];
				RDebug::Printf("[phedynres] key %c: asking for %dx%d", ev.Key()->iCode, s.iW, s.iH);
				UserSvr::HalFunction(EHalGroupDisplay, KHalPhoreRequest, (TAny*)s.iW, (TAny*)s.iH);
				++keyPresses;
				}
			ws.EventReady(&eventSt);
			}
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	RDebug::Printf("[phedynres] exit err=%d", err);
	delete cleanup;
	return err;
	}
