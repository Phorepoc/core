// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phenative.cpp -- OURS: a native group. After the home screen is up it connects to the window server, asks for screen mode 0 (the board's native
// mode) as its own application mode, and shows a full-screen window in it: a grid every 40 px and its size in text. Its group is placed at scale 1
// while the legacy groups run doubled. It stays 40 s, then leaves.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <gdi.h>
#include <fbs.h>

static TInt Run()
	{
	User::After(75 * 1000000);
	RWsSession ws;
	TInt r = ws.Connect();
	if (r != KErrNone)
		return r;
	CWsScreenDevice* screen = new CWsScreenDevice(ws);
	if (!screen || (r = screen->Construct()) != KErrNone)
		return r ? r : KErrNoMemory;
	screen->SetAppScreenMode(0);
	ws.Flush();
	const TSize size = screen->SizeInPixels();
	RDebug::Printf("PH: [native] app mode 0: %dx%d", size.iWidth, size.iHeight);
	RWindowGroup group(ws);
	group.Construct(0xE7015799, EFalse);
	group.SetOrdinalPosition(0, 1000);
	group.EnableReceiptOfFocus(EFalse);
	RWindow win(ws);
	win.Construct(group, 1);
	win.SetExtent(TPoint(0, 0), size);
	win.SetBackgroundColor(TRgb(20, 40, 80));
	win.Activate();
	CWindowGc* gc = NULL;
	screen->CreateContext(gc);
	win.BeginRedraw();
	gc->Activate(win);
	gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc->SetBrushColor(TRgb(20, 40, 80));
	gc->Clear();
	gc->SetPenColor(TRgb(200, 220, 255));
	for (TInt x = 0; x < size.iWidth; x += 40)
		gc->DrawLine(TPoint(x, 0), TPoint(x, size.iHeight));
	for (TInt y = 0; y < size.iHeight; y += 40)
		gc->DrawLine(TPoint(0, y), TPoint(size.iWidth, y));
	gc->SetBrushColor(TRgb(255, 255, 255));
	gc->DrawRect(TRect(TPoint(size.iWidth / 2 - 100, size.iHeight / 2 - 100), TSize(200, 200)));
	gc->Deactivate();
	win.EndRedraw();
	ws.Flush();
	User::After(40 * 1000000);
	delete gc;
	win.Close();
	group.Close();
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
