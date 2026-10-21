// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// aknapp.cpp -- OURS: the smallest real S60 application on the from-source Avkon:
// CAknApplication -> CAknDocument -> CAknAppUi -> one window-owning view control.
// Status pane, softkeys (Options/Exit) and the Options menu come from Avkon itself.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <avkon.hrh>
#include <aknapp.rsg>
#include "aknapp.hrh"

const TUid KUidAknApp = { 0xE7015703 };

class CHelloView : public CCoeControl
	{
public:
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		SetRect(aRect);
		ActivateL();
		}
	void SetHello() { iHello = ETrue; DrawNow(); }
private:
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(0x20, 0x30, 0x60));
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.DrawRect(Rect());
		const CFont* font = iEikonEnv->NormalFont();
		gc.UseFont(font);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(KRgbWhite);
		gc.DrawText(_L("AKNAPP: a real Avkon app"), TPoint(10, 40));
		if (iHello)
			{
			gc.SetPenColor(KRgbYellow);
			gc.DrawText(_L("Hello from a from-source Symbian UI"), TPoint(10, 80));
			}
		gc.DiscardFont();
		}
	TBool iHello;
	};

class CHelloAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		RDebug::Printf("[aknapp] AppUi::ConstructL");
		BaseConstructL();
		RDebug::Printf("[aknapp] BaseConstructL done");
		iView = new (ELeave) CHelloView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		RDebug::Printf("[aknapp] view up");
		}
	~CHelloAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EAknAppCmdHello:
			iView->SetHello();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
			}
		}
private:
	CHelloView* iView;
	};

class CHelloDocument : public CAknDocument
	{
public:
	CHelloDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CHelloAppUi; }
	};

class CHelloApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KUidAknApp; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CHelloDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CHelloApplication;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[aknapp] E32Main");
	TInt r = EikStart::RunApplication(NewApplication);
	RDebug::Printf("[aknapp] RunApplication returned %d", r);
	return r;
	}
