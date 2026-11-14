// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// wsclient.cpp -- OURS: the smallest native WSERV client. Opens a window-server
// session, creates a window group + one RWindow, and draws into it with a
// CWindowGc (rects, lines, an ellipse, text in a real fntstore font), then
// services redraw events forever. Proves the ws32 client drawing path end-to-end
// on the from-source graphical window server (Track B M9 "UI client").
//
// Since Path C X4.24 it is also a tiny PAINT program -- the first UI to use a mouse:
// a mouse cursor (an arrow built into CFbsBitmaps, shown as a sprite on our window),
// drag with button 1 to draw, a click drops a dot, the title bar shows the pointer
// position, the 'c' key clears. Strokes are kept so a redraw repaints them. With no
// pointing device (a pen, or nothing) it is exactly the old static scene.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <fbs.h>

// the canvas below the swatches/ellipse scene
static const TInt KCanvasTop = 320;
static const TInt KMaxPoints = 4096;
static const TPoint KPenUp(-1, -1);			// stroke separator in the point list

struct SPaint
	{
	RArray<TPoint> iPoints;					// strokes, KPenUp between them
	TPoint iPos;							// last pointer position (window coordinates)
	TBool iDown;
	TInt iEvents;
	};

static void DrawTitle(CWindowGc& aGc, const TSize& aSize, CFont* aFont, const SPaint& aP)
	{
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(TRgb(0x10, 0x90, 0xE0));
	aGc.DrawRect(TRect(0, 0, aSize.iWidth, 40));
	if (!aFont)
		return;
	aGc.UseFont(aFont);
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KRgbWhite);
	TBuf<96> t;
	TPtrC button(aP.iDown ? _L("[button 1]") : _L(""));
	t.Format(_L("WSCLIENT - paint with the mouse    pointer %d,%d %S"), aP.iPos.iX, aP.iPos.iY, &button);
	aGc.DrawText(t, TPoint(12, 27));
	aGc.DiscardFont();
	}

static void DrawStrokes(CWindowGc& aGc, const SPaint& aP)
	{
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KRgbYellow);
	aGc.SetPenSize(TSize(3, 3));
	TPoint prev = KPenUp;
	for (TInt i = 0; i < aP.iPoints.Count(); i++)
		{
		const TPoint p = aP.iPoints[i];
		if (p != KPenUp)
			{
			if (prev == KPenUp)
				aGc.Plot(p);
			else
				aGc.DrawLine(prev, p);
			}
		prev = p;
		}
	}

static void DrawScene(CWindowGc& aGc, RWindow& aWin, CFont* aFont, const SPaint& aP)
	{
	const TSize size = aWin.Size();
	aGc.Activate(aWin);
	aWin.BeginRedraw();
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	aGc.SetBrushColor(TRgb(0x20, 0x30, 0x60));
	aGc.DrawRect(TRect(size));
	DrawTitle(aGc, size, aFont, aP);
	aGc.SetPenStyle(CGraphicsContext::ENullPen);
	aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
	// colour swatches
	const TRgb swatch[] = { KRgbRed, KRgbGreen, KRgbBlue, KRgbYellow, KRgbMagenta, KRgbCyan };
	for (TInt i = 0; i < 6; i++)
		{
		aGc.SetBrushColor(swatch[i]);
		aGc.DrawRect(TRect(TPoint(20 + i * 70, 70), TSize(60, 60)));
		}
	aGc.SetBrushColor(KRgbWhite);
	aGc.DrawEllipse(TRect(TPoint(20, 160), TSize(200, 100)));
	aGc.SetPenStyle(CGraphicsContext::ESolidPen);
	aGc.SetPenColor(KRgbWhite);
	aGc.SetPenSize(TSize(2, 2));
	aGc.DrawLine(TPoint(240, 160), TPoint(440, 260));
	aGc.DrawLine(TPoint(240, 260), TPoint(440, 160));
	aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
	aGc.DrawRect(TRect(TPoint(240, 160), TSize(200, 100)));
	// the canvas
	aGc.SetPenSize(TSize(1, 1));
	aGc.SetPenColor(TRgb(0x60, 0x70, 0xA0));
	aGc.DrawRect(TRect(10, KCanvasTop, size.iWidth - 10, size.iHeight - 10));
	if (aFont)
		{
		aGc.UseFont(aFont);
		aGc.SetPenColor(KRgbYellow);
		aGc.DrawText(_L("RWsSession + RWindowGroup + RWindow + CWindowGc + RWsSprite"), TPoint(20, 300));
		aGc.DiscardFont();
		}
	DrawStrokes(aGc, aP);
	aWin.EndRedraw();
	aGc.Deactivate();
	}

// A 12x19 arrow: '#' black outline, '.' white fill, ' ' transparent. Colour bitmap + 256-grey mask.
static const char* const KArrow[] =
	{
	"#           ", "##          ", "#.#         ", "#..#        ", "#...#       ", "#....#      ",
	"#.....#     ", "#......#    ", "#.......#   ", "#........#  ", "#.........# ", "#......#####",
	"#...#..#    ", "#..# #..#   ", "#.#  #..#   ", "##    #..#  ", "#     #..#  ", "       #..# ",
	"       ##   "
	};
static const TInt KArrowW = 12, KArrowH = 19;

static TInt MakeArrow(CFbsBitmap*& aBmp, CFbsBitmap*& aMask)
	{
	aBmp = new CFbsBitmap;
	aMask = new CFbsBitmap;
	if (!aBmp || !aMask)
		return KErrNoMemory;
	TInt r = aBmp->Create(TSize(KArrowW, KArrowH), EColor16MU);
	if (r == KErrNone)
		r = aMask->Create(TSize(KArrowW, KArrowH), EGray256);
	if (r != KErrNone)
		return r;
	const TInt cStride = CFbsBitmap::ScanLineLength(KArrowW, EColor16MU);
	const TInt mStride = CFbsBitmap::ScanLineLength(KArrowW, EGray256);
	aBmp->BeginDataAccess();
	aMask->BeginDataAccess();
	TUint8* c = (TUint8*)aBmp->DataAddress();
	TUint8* m = (TUint8*)aMask->DataAddress();
	for (TInt y = 0; y < KArrowH; y++)
		for (TInt x = 0; x < KArrowW; x++)
			{
			const char ch = KArrow[y][x];
			((TUint32*)(c + y * cStride))[x] = ch == '.' ? 0xFFFFFFFFu : 0xFF000000u;
			m[y * mStride + x] = ch == ' ' ? 0 : 255;
			}
	aMask->EndDataAccess();
	aBmp->EndDataAccess();
	return KErrNone;
	}

static void AddPoint(SPaint& aP, const TPoint& aPt)
	{
	if (aP.iPoints.Count() < KMaxPoints)
		aP.iPoints.Append(aPt);
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
	User::LeaveIfError(group.Construct(0x57534331, ETrue));
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x57534332));
	const TSize scr = screen->SizeInPixels();
	RDebug::Printf("[wsclient] screen %dx%d", scr.iWidth, scr.iHeight);
	win.SetExtent(TPoint(40, 40), TSize(scr.iWidth - 80, scr.iHeight - 80));
	win.SetBackgroundColor(KRgbBlack);
	win.PointerFilter(EPointerFilterEnterExit | EPointerFilterMove | EPointerFilterDrag, 0);	// all of them
	win.Activate();

	// The mouse cursor: an ordinary sprite on OUR window, moved to each pointer event's position
	// (hot spot = the arrow's tip at the sprite origin). Not RWsPointerCursor: tried first
	// (Construct/AppendMember/Activate/SetCustomPointerCursor all KErrNone) -- in this non-NGA
	// WSERV the pointer cursor is a sprite of the ROOT window and never showed over a child
	// window, while a sprite of the window itself does (X4.24).
	CFbsBitmap* arrow = NULL;
	CFbsBitmap* arrowMask = NULL;
	RWsSprite sprite(ws);
	TInt r = MakeArrow(arrow, arrowMask);
	if (r == KErrNone)
		r = sprite.Construct(win, TPoint(win.Size().iWidth / 2, win.Size().iHeight / 2), 0);
	if (r == KErrNone)
		{
		TSpriteMember sm;
		sm.iBitmap = arrow;
		sm.iMaskBitmap = arrowMask;
		sm.iInvertMask = EFalse;
		sm.iDrawMode = CGraphicsContext::EDrawModePEN;
		sm.iOffset = TPoint(0, 0);
		sm.iInterval = TTimeIntervalMicroSeconds32(0);
		r = sprite.AppendMember(sm);
		if (r == KErrNone)
			r = sprite.Activate();
		}
	RDebug::Printf("[wsclient] cursor sprite r=%d", r);

	CFont* font = NULL;
	TFontSpec spec(_L("Arial"), 200);
	r = screen->GetNearestFontToDesignHeightInTwips(font, spec);
	RDebug::Printf("[wsclient] font r=%d", r);

	SPaint paint;
	paint.iDown = EFalse;
	paint.iEvents = 0;
	DrawScene(*gc, win, font, paint);
	ws.Flush();
	RDebug::Printf("[wsclient] drawn");

	TRequestStatus redrawSt, eventSt;
	ws.RedrawReady(&redrawSt);
	ws.EventReady(&eventSt);
	for (;;)
		{
		User::WaitForAnyRequest();
		if (redrawSt != KRequestPending)
			{
			TWsRedrawEvent rev;
			ws.GetRedraw(rev);
			DrawScene(*gc, win, font, paint);
			ws.RedrawReady(&redrawSt);
			}
		else if (eventSt != KRequestPending)
			{
			TWsEvent ev;
			ws.GetEvent(ev);
			TBool strokes = EFalse, title = EFalse;
			if (ev.Type() == EEventPointer)
				{
				const TPointerEvent& p = *ev.Pointer();
				if (paint.iEvents++ < 4)
					RDebug::Printf("[wsclient] pointer event %d at %d,%d", p.iType, p.iPosition.iX, p.iPosition.iY);
				paint.iPos = p.iPosition;
				sprite.SetPosition(p.iPosition);
				title = ETrue;
				const TBool onCanvas = p.iPosition.iY > KCanvasTop;
				switch (p.iType)
					{
				case TPointerEvent::EButton1Down:
					paint.iDown = ETrue;
					if (onCanvas)
						{ AddPoint(paint, KPenUp); AddPoint(paint, p.iPosition); strokes = ETrue; }
					break;
				case TPointerEvent::EDrag:
					if (paint.iDown && onCanvas)
						{ AddPoint(paint, p.iPosition); strokes = ETrue; }
					break;
				case TPointerEvent::EButton1Up:
					paint.iDown = EFalse;
					break;
				default:
					break;
					}
				}
			else if (ev.Type() == EEventKey && (ev.Key()->iCode == 'c' || ev.Key()->iCode == 'C'))
				{
				paint.iPoints.Reset();
				win.Invalidate();
				}
			if (title || strokes)
				{
				// incremental drawing outside a redraw: the title bar, and the newest stroke segment
				gc->Activate(win);
				if (title)
					DrawTitle(*gc, win.Size(), font, paint);
				if (strokes)
					{
					const TInt n = paint.iPoints.Count();
					gc->SetPenStyle(CGraphicsContext::ESolidPen);
					gc->SetPenColor(KRgbYellow);
					gc->SetPenSize(TSize(3, 3));
					if (n >= 2 && paint.iPoints[n-2] != KPenUp)
						gc->DrawLine(paint.iPoints[n-2], paint.iPoints[n-1]);
					else if (n >= 1)
						gc->Plot(paint.iPoints[n-1]);
					}
				gc->Deactivate();
				ws.Flush();
				}
			ws.EventReady(&eventSt);
			}
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TRAPD(err, MainL());
	RDebug::Printf("[wsclient] exit err=%d", err);
	delete cleanup;
	return err;
	}
