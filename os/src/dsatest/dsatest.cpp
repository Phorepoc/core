// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// dsatest.cpp -- OURS: CDirectScreenAccess + CDirectScreenBitmap, the way a game
// (SkyForce) drives the screen under the NGA window server, with every step logged ("PH: dsa ...", forwarded to
// PhoneLab). Draws a known pattern so a screenshot proves the path end to end:
//   rows 0..3 bands (each 1/16 of the height): pure red, pure green, pure blue (fully opaque, alpha 0xFF)
//   below: a grey ramp; a progress bar in the second-to-last band whose length = frame counter / KFrames
//   the last band alternates black/white every frame (a flip-rate canary)
// Stops after KFrames frames, logs frames/s, then idles with the last frame on screen. A 2 s heartbeat prints the
// state, so a hang shows as "alive state=<N>" lines with a frozen frame count.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>
#include <bitdev.h>
#include <bitstd.h>
#include <cdsb.h>   // exported to the include root (SYMBIAN_OLD_EXPORT_LOCATION)

static const TInt KFrames = 300;

enum TState { EIdle, ECreate, EBegin, EWaitUpdate, EDone };

class CDsaTest : public CActive, public MDirectScreenAccess
	{
public:
	static CDsaTest* NewL(RWsSession& aWs, CWsScreenDevice& aScreen, RWindow& aWin);
	~CDsaTest();
	void StartL();
	// MDirectScreenAccess
	void Restart(RDirectScreenAccess::TTerminationReasons aReason);
	void AbortNow(RDirectScreenAccess::TTerminationReasons aReason);
	TInt iFrames;
	TInt iNotVisible;
	TInt iState;
private:
	CDsaTest(RWsSession& aWs, CWsScreenDevice& aScreen, RWindow& aWin);
	void RunL();
	void DoCancel();
	void DrawFrame();
	void NextFrame();
	void CloseBitmap();
	RWsSession& iWs;
	CWsScreenDevice& iScreen;
	RWindow& iWin;
	CDirectScreenAccess* iDsa;
	CDirectScreenBitmap* iBmp;
	TAcceleratedBitmapInfo iInfo;
	TRect iRect;
	TTime iStart;
	};

CDsaTest::CDsaTest(RWsSession& aWs, CWsScreenDevice& aScreen, RWindow& aWin)
	: CActive(EPriorityStandard), iWs(aWs), iScreen(aScreen), iWin(aWin)
	{
	}

CDsaTest* CDsaTest::NewL(RWsSession& aWs, CWsScreenDevice& aScreen, RWindow& aWin)
	{
	CDsaTest* t = new (ELeave) CDsaTest(aWs, aScreen, aWin);
	CleanupStack::PushL(t);
	CActiveScheduler::Add(t);
	t->iDsa = CDirectScreenAccess::NewL(aWs, aScreen, aWin, *t);
	RDebug::Printf("PH: dsa create CDirectScreenAccess ok");
	CleanupStack::Pop(t);
	return t;
	}

CDsaTest::~CDsaTest()
	{
	Cancel();
	CloseBitmap();
	delete iDsa;
	}

void CDsaTest::DoCancel()
	{
	}

void CDsaTest::CloseBitmap()
	{
	if (iBmp)
		{
		iBmp->Close();
		delete iBmp;
		iBmp = NULL;
		}
	}

void CDsaTest::StartL()
	{
	iState = ECreate;
	RDebug::Printf("PH: dsa StartL");
	iDsa->StartL();
	RRegion* reg = iDsa->DrawingRegion();
	iRect = reg->BoundingRect();
	RDebug::Printf("PH: dsa region rects=%d bounds=(%d,%d)-(%d,%d)", reg->Count(), iRect.iTl.iX, iRect.iTl.iY,
		iRect.iBr.iX, iRect.iBr.iY);
	if (iRect.IsEmpty())
		{
		RDebug::Printf("PH: dsa empty region, waiting for Restart");
		iState = EIdle;
		return;
		}
	iBmp = CDirectScreenBitmap::NewL();
	TInt r = iBmp->Create(iRect, CDirectScreenBitmap::EDoubleBuffer);
	RDebug::Printf("PH: dsa CDirectScreenBitmap::Create r=%d", r);
	User::LeaveIfError(r);
	iStart.HomeTime();
	NextFrame();
	}

void CDsaTest::Restart(RDirectScreenAccess::TTerminationReasons aReason)
	{
	RDebug::Printf("PH: dsa Restart reason=%d", (TInt)aReason);
	CloseBitmap();
	TRAPD(r, StartL());
	RDebug::Printf("PH: dsa Restart StartL r=%d", r);
	}

void CDsaTest::AbortNow(RDirectScreenAccess::TTerminationReasons aReason)
	{
	RDebug::Printf("PH: dsa AbortNow reason=%d state=%d", (TInt)aReason, iState);
	Cancel();
	CloseBitmap();
	iState = EIdle;
	}

void CDsaTest::DrawFrame()
	{
	const TInt w = iInfo.iSize.iWidth, h = iInfo.iSize.iHeight;
	const TInt bpp = 1 << iInfo.iPixelShift;
	const TInt band = h / 16;
	const TInt barLen = (w * iFrames) / KFrames;
	TUint32 row[1024];
	const TInt n = w < 1024 ? w : 1024;
	for (TInt y = 0; y < h; ++y)
		{
		TUint32 c;
		if (y < band) c = 0xFFFF0000;				// red
		else if (y < 2 * band) c = 0xFF00FF00;		// green
		else if (y < 3 * band) c = 0xFF0000FF;		// blue
		else if (y >= h - band) c = (iFrames & 1) ? 0xFFFFFFFF : 0xFF000000;		// the canary
		else { TUint g = (y * 255) / h; c = 0xFF000000 | (g << 16) | (g << 8) | g; }
		for (TInt x = 0; x < n; ++x)
			row[x] = c;
		if (y >= h - 2 * band && y < h - band)		// progress bar
			{
			for (TInt x = 0; x < n; ++x)
				row[x] = x < barLen ? 0xFFFFFFFF : 0xFF202020;
			}
		TUint8* line = iInfo.iAddress + y * iInfo.iLinePitch;
		if (bpp == 32)
			Mem::Copy(line, row, n * 4);
		else if (bpp == 16)
			{
			TUint16* p = (TUint16*)line;
			for (TInt x = 0; x < n; ++x)
				{
				TUint32 v = row[x];
				p[x] = (TUint16)(((v >> 8) & 0xF800) | ((v >> 5) & 0x07E0) | ((v >> 3) & 0x001F));
				}
			}
		}
	}

void CDsaTest::NextFrame()
	{
	iState = EBegin;
	TInt r = iBmp->BeginUpdate(iInfo);
	if (iFrames == 0 || r != KErrNone)
		RDebug::Printf("PH: dsa BeginUpdate r=%d addr=%08x size=%dx%d pitch=%d shift=%d mode=%d", r,
			(TUint)iInfo.iAddress, iInfo.iSize.iWidth, iInfo.iSize.iHeight, iInfo.iLinePitch, iInfo.iPixelShift,
			(TInt)iInfo.iDisplayMode);
	if (r != KErrNone || !iInfo.iAddress)
		{
		iState = EDone;
		return;
		}
	DrawFrame();
	iState = EWaitUpdate;
	iStatus = KRequestPending;
	iBmp->EndUpdate(iStatus);
	SetActive();
	if (iFrames == 0)
		RDebug::Printf("PH: dsa first EndUpdate issued");
	}

void CDsaTest::RunL()
	{
	if (iFrames == 0 || iStatus.Int() != KErrNone)
		RDebug::Printf("PH: dsa EndUpdate complete frame=%d status=%d", iFrames, iStatus.Int());
	// -223 = KErrNotVisible (suerror.h): the compositor has not composed the DSA element (yet) -- count, go on
	if (iStatus.Int() == -223 && ++iNotVisible < 600)
		{
		if (iNotVisible % 60 == 1)
			RDebug::Printf("PH: dsa not visible x%d (frames %d)", iNotVisible, iFrames);
		NextFrame();
		return;
		}
	if (iStatus.Int() != KErrNone)
		{
		iState = EDone;
		return;
		}
	++iFrames;
	if (iFrames % 60 == 0)
		RDebug::Printf("PH: dsa frame %d", iFrames);
	if (iFrames >= KFrames)
		{
		TTime now;
		now.HomeTime();
		TInt ms = (TInt)(now.MicroSecondsFrom(iStart).Int64() / 1000);
		RDebug::Printf("PH: dsa DONE %d frames in %d ms (fps x10 = %d), not visible %d", iFrames, ms, ms ? (iFrames * 10000) / ms : 0, iNotVisible);
		iState = EDone;
		return;
		}
	NextFrame();
	}

static TInt Heartbeat(TAny* aPtr)
	{
	CDsaTest* t = (CDsaTest*)aPtr;
	RDebug::Printf("PH: dsa alive state=%d frames=%d", t->iState, t->iFrames);
	return 1;
	}

static void MainL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	User::LeaveIfError(RFbsSession::Connect());
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	User::LeaveIfError(screen->Construct());
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x44534131, ETrue));
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x44534132));
	const TSize scr = screen->SizeInPixels();
	RDebug::Printf("PH: dsa screen %dx%d mode %d", scr.iWidth, scr.iHeight, (TInt)screen->DisplayMode());
	win.SetExtent(TPoint(0, 0), scr);
	win.SetRequiredDisplayMode(EColor16MU);
	win.Activate();
	ws.Flush();
	RDebug::Printf("PH: dsa window active");

	CDsaTest* t = CDsaTest::NewL(ws, *screen, win);
	CleanupStack::PushL(t);
	CPeriodic* beat = CPeriodic::NewL(CActive::EPriorityLow);
	CleanupStack::PushL(beat);
	beat->Start(2000000, 2000000, TCallBack(Heartbeat, t));
	TRAPD(r, t->StartL());
	RDebug::Printf("PH: dsa StartL r=%d", r);
	CActiveScheduler::Start();
	CleanupStack::PopAndDestroy(3, sched);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	RDebug::Printf("PH: dsa exit err=%d", err);
	delete cleanup;
	return err;
	}
