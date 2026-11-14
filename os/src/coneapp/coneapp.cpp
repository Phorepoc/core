// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// coneapp.cpp -- OURS: the smallest application on the real control environment
// (cone.dll, from source): CCoeEnv owns the WSERV/FBS sessions, the active
// scheduler and the event loop; a CCoeAppUi owns one window-owning CCoeControl
// that draws itself and reacts to key events. No Uikon/Avkon, no resource file.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <coemain.h>
#include <coeaui.h>
#include <coecntrl.h>

class CHelloControl : public CCoeControl
	{
public:
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		SetRect(aRect);
		ActivateL();
		}
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
		{
		if (aType == EEventKey)
			{
			iKeys++;
			iLast = aKeyEvent.iCode;
			DrawNow();
			return EKeyWasConsumed;
			}
		return EKeyWasNotConsumed;
		}
private:
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		const TRect r = Rect();
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(0x18, 0x50, 0x38));
		gc.DrawRect(r);
		gc.SetBrushColor(TRgb(0x30, 0xB0, 0x70));
		gc.DrawRect(TRect(r.iTl, TSize(r.Width(), 40)));
		const CFont* font = iCoeEnv->NormalFont();
		gc.UseFont(font);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(KRgbWhite);
		gc.DrawText(_L("CONEAPP - CCoeEnv + CCoeAppUi + CCoeControl, from source"), TPoint(12, 27));
		TBuf<64> b;
		b.Format(_L("key events: %d   last code: 0x%x '%c'"), iKeys, iLast, iLast >= 32 ? iLast : '?');
		gc.SetPenColor(KRgbYellow);
		gc.DrawText(b, TPoint(20, 90));
		gc.DiscardFont();
		}
	TInt iKeys;
	TUint iLast;
	};

class CHelloAppUi : public CCoeAppUi
	{
public:
	void ConstructL()
		{
		CCoeAppUi::ConstructL();
		iControl = new (ELeave) CHelloControl;
		TRect r(iCoeEnv->ScreenDevice()->SizeInPixels());
		r.Shrink(30, 30);
		iControl->ConstructL(r);
		AddToStackL(iControl);
		RDebug::Printf("[coneapp] appui constructed");
		}
	~CHelloAppUi()
		{
		if (iControl)
			RemoveFromStack(iControl);
		delete iControl;
		}
private:
	CHelloControl* iControl;
	};

GLDEF_C TInt E32Main()
	{
	CCoeEnv* env = new CCoeEnv;
	TRAPD(err, env->ConstructL());
	RDebug::Printf("[coneapp] CCoeEnv::ConstructL err=%d", err);
	if (err)
		return err;
	CHelloAppUi* ui = new CHelloAppUi;
	env->SetAppUi(ui);
	TRAP(err, ui->ConstructL());
	RDebug::Printf("[coneapp] appui err=%d", err);
	if (err == KErrNone)
		env->ExecuteD();
	return err;
	}
