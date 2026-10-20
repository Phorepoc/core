// wsblit.cpp -- OURS (2026-10-09, GPU plan G1c): a bitmap-drawing test grid for the window server. The first NGA
// boot of the S60 UI on the PinePhone drew text and the wallpaper but no icons and no skin frames (the masked /
// alpha bitmap paths). Each numbered cell below is one CWindowGc bitmap call (source display mode, mask mode,
// compressed or not); a cell that stays grey (or shows garbage) names the broken path. Grey window, red/green
// sources (a green square in the top-left quarter), masks = a white disc on black.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>
#include <bitdev.h>
#include <bitstd.h>

static const TSize KCell(56, 56);

// a source: red with a green top-left quarter (alpha aAlpha, written as is: EDrawModeWriteAlpha)
static CFbsBitmap* MakeSrcL(TDisplayMode aMode, TInt aAlpha = 255)
	{
	CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
	CleanupStack::PushL(bmp);
	User::LeaveIfError(bmp->Create(KCell, aMode));
	CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL(bmp);
	CleanupStack::PushL(dev);
	CFbsBitGc* gc = NULL;
	User::LeaveIfError(dev->CreateContext(gc));
	gc->SetDrawMode(CGraphicsContext::EDrawModeWriteAlpha);
	gc->SetPenStyle(CGraphicsContext::ENullPen);
	gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc->SetBrushColor(TRgb(0xE0, 0x20, 0x20, aAlpha));
	gc->DrawRect(TRect(KCell));
	gc->SetBrushColor(TRgb(0x20, 0xC0, 0x20, aAlpha));
	gc->DrawRect(TRect(0, 0, KCell.iWidth / 2, KCell.iHeight / 2));
	delete gc;
	CleanupStack::PopAndDestroy(dev);
	CleanupStack::Pop(bmp);
	return bmp;
	}

// a mask: a white disc on black
static CFbsBitmap* MakeMaskL(TDisplayMode aMode)
	{
	CFbsBitmap* bmp = new (ELeave) CFbsBitmap;
	CleanupStack::PushL(bmp);
	User::LeaveIfError(bmp->Create(KCell, aMode));
	CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL(bmp);
	CleanupStack::PushL(dev);
	CFbsBitGc* gc = NULL;
	User::LeaveIfError(dev->CreateContext(gc));
	gc->SetPenStyle(CGraphicsContext::ENullPen);
	gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	gc->SetBrushColor(KRgbBlack);
	gc->DrawRect(TRect(KCell));
	gc->SetBrushColor(KRgbWhite);
	gc->DrawEllipse(TRect(TPoint(4, 4), TSize(KCell.iWidth - 8, KCell.iHeight - 8)));
	delete gc;
	CleanupStack::PopAndDestroy(dev);
	CleanupStack::Pop(bmp);
	return bmp;
	}

enum TOp { EBlt, EBltMasked, EDrawScaled, EDrawMaskedScaled };
struct TCase
	{
	const TText* iName;
	TOp iOp;
	TDisplayMode iSrc;
	TInt iAlpha;
	TDisplayMode iMask;		// ENone = no mask
	TBool iCompress;		// Compress() source and mask (as fbserv's background compression does)
	};

static const TCase KCases[] =
	{
	{ _S("1 16MU"),       EBlt,              EColor16MU,  255, ENone,    EFalse },
	{ _S("2 16MA a128"),  EBlt,              EColor16MA,  128, ENone,    EFalse },
	{ _S("3 16MAP a128"), EBlt,              EColor16MAP, 128, ENone,    EFalse },
	{ _S("4 64K"),        EBlt,              EColor64K,   255, ENone,    EFalse },
	{ _S("5 MU+G2"),      EBltMasked,        EColor16MU,  255, EGray2,   EFalse },
	{ _S("6 MU+G256"),    EBltMasked,        EColor16MU,  255, EGray256, EFalse },
	{ _S("7 64K+G256"),   EBltMasked,        EColor64K,   255, EGray256, EFalse },
	{ _S("8 16M+G256"),   EBltMasked,        EColor16M,   255, EGray256, EFalse },
	{ _S("9 MA+G256"),    EBltMasked,        EColor16MA,  255, EGray256, EFalse },
	{ _S("10 MU+G2 cmp"), EBltMasked,        EColor16MU,  255, EGray2,   ETrue  },
	{ _S("11 MU+G256c"),  EBltMasked,        EColor16MU,  255, EGray256, ETrue  },
	{ _S("12 MU cmp"),    EBlt,              EColor16MU,  255, ENone,    ETrue  },
	{ _S("13 draw sc"),   EDrawScaled,       EColor16MU,  255, ENone,    EFalse },
	{ _S("14 drawM sc"),  EDrawMaskedScaled, EColor16MU,  255, EGray256, EFalse },
	{ _S("15 64K+G2"),    EBltMasked,        EColor64K,   255, EGray2,   EFalse },
	{ _S("16 MA a128 M"), EBltMasked,        EColor16MA,  128, EGray256, EFalse },
	};
static const TInt KNumCases = sizeof(KCases) / sizeof(KCases[0]);

struct TPrepared { CFbsBitmap* iSrc; CFbsBitmap* iMask; };

static TPoint CellPos(TInt aIndex)
	{
	return TPoint(14 + (aIndex % 4) * 86, 50 + (aIndex / 4) * 100);
	}

static void DrawScene(CWindowGc& aGc, RWindow& aWin, CFont* aFont, const TPrepared* aP)
	{
	aGc.Activate(aWin);
	aWin.BeginRedraw();
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(TRgb(0x80, 0x80, 0x80));
	aGc.DrawRect(TRect(aWin.Size()));
	if (aFont)
		{
		aGc.UseFont(aFont);
		aGc.SetPenStyle(CGraphicsContext::ESolidPen);
		aGc.SetPenColor(KRgbWhite);
		aGc.DrawText(_L("WSBLIT: red + green quarter; masked = a disc"), TPoint(8, 30));
		}
	for (TInt i = 0; i < KNumCases; ++i)
		{
		const TCase& c = KCases[i];
		const TPoint pos = CellPos(i);
		aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
		if (aP[i].iSrc)
			{
			const TRect src(KCell);
			switch (c.iOp)
				{
			case EBlt:
				aGc.BitBlt(pos, aP[i].iSrc);
				break;
			case EBltMasked:
				if (aP[i].iMask)
					aGc.BitBltMasked(pos, aP[i].iSrc, src, aP[i].iMask, EFalse);
				break;
			case EDrawScaled:
				aGc.DrawBitmap(TRect(pos, TSize(KCell.iWidth + 20, KCell.iHeight)), aP[i].iSrc);
				break;
			case EDrawMaskedScaled:
				if (aP[i].iMask)
					aGc.DrawBitmapMasked(TRect(pos, TSize(KCell.iWidth + 20, KCell.iHeight)), aP[i].iSrc, src,
						aP[i].iMask, EFalse);
				break;
				}
			}
		if (aFont)
			{
			aGc.SetPenColor(aP[i].iSrc ? KRgbWhite : KRgbYellow);
			aGc.DrawText(TPtrC(c.iName), TPoint(pos.iX, pos.iY + KCell.iHeight + 16));
			}
		}
	if (aFont)
		aGc.DiscardFont();
	aWin.EndRedraw();
	aGc.Deactivate();
	}

static void MainL()
	{
	User::LeaveIfError(RFbsSession::Connect());
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	User::LeaveIfError(screen->Construct());
	CWindowGc* gc = NULL;
	User::LeaveIfError(screen->CreateContext(gc));
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x57534231, ETrue));
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x57534232));
	const TSize scr = screen->SizeInPixels();
	RDebug::Printf("[wsblit] screen %dx%d mode %d", scr.iWidth, scr.iHeight, screen->DisplayMode());
	win.SetExtent(TPoint(0, 0), scr);
	win.Activate();

	TPrepared prep[KNumCases];
	for (TInt i = 0; i < KNumCases; ++i)
		{
		const TCase& c = KCases[i];
		prep[i].iSrc = NULL;
		prep[i].iMask = NULL;
		TRAPD(r, prep[i].iSrc = MakeSrcL(c.iSrc, c.iAlpha));
		TInt rm = KErrNone;
		if (r == KErrNone && c.iMask != ENone)
			TRAP(rm, prep[i].iMask = MakeMaskL(c.iMask));
		TInt rc = KErrNone;
		if (c.iCompress)
			{
			if (prep[i].iSrc) rc = prep[i].iSrc->Compress();
			if (prep[i].iMask && rc == KErrNone) rc = prep[i].iMask->Compress();
			}
		RDebug::Printf("[wsblit] case %d: src r=%d mask r=%d compress r=%d", i + 1, r, rm, rc);
		}

	CFont* font = NULL;
	TFontSpec spec(_L("Arial"), 110);
	TInt r = screen->GetNearestFontToDesignHeightInTwips(font, spec);
	RDebug::Printf("[wsblit] font r=%d", r);

	DrawScene(*gc, win, font, prep);
	ws.Flush();
	RDebug::Printf("[wsblit] drawn");

	TRequestStatus redrawSt;
	ws.RedrawReady(&redrawSt);
	for (;;)
		{
		User::WaitForRequest(redrawSt);
		TWsRedrawEvent rev;
		ws.GetRedraw(rev);
		DrawScene(*gc, win, font, prep);
		ws.RedrawReady(&redrawSt);
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	RDebug::Printf("[wsblit] exit err=%d", err);
	delete cleanup;
	return err;
	}
