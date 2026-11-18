// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebbleapp.cpp -- OURS: the Pebble app: pair a watch, see its status, send a test notification. The work is done by
// phepebblesrv.exe (started here if it is not running); the two talk through Publish & Subscribe (phepebble_ps.h).
//
// Look (Etesian design language where an Avkon app can follow it): content is the interface, edge to edge, no frames;
// calm light type with one large status line; high contrast; one accent (the Etesian default blue #4591FF) for the
// connected state and the warning red #DA201E for errors; actions in the Options menu, Back/Exit bottom right.

#include <e32property.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknutils.h>
#include <aknsutils.h>
#include <aknsdrawutils.h>
#include <aknsbasicbackgroundcontrolcontext.h>
#include <akntitle.h>
#include <aknnotewrappers.h>
#include <eikspane.h>
#include <eikmenup.h>
#include <avkon.hrh>
#include <phepebble.rsg>
#include "phepebble.hrh"
#include "phepebble_ps.h"

static const TRgb KAccent(0x45, 0x91, 0xFF);
static const TRgb KWarning(0xDA, 0x20, 0x1E);

class CPebbleView;

/** Redraws the view when a service key changes. */
class CStatusWatcher : public CActive
	{
public:
	CStatusWatcher(CPebbleView& aView, TUint32 aKey) : CActive(EPriorityStandard), iView(aView), iKey(aKey)
		{ CActiveScheduler::Add(this); }
	~CStatusWatcher() { Cancel(); iProperty.Close(); }
	void StartL() { User::LeaveIfError(iProperty.Attach(KPhePebbleSrvUid, iKey)); Watch(); }
private:
	void Watch() { iProperty.Subscribe(iStatus); SetActive(); }
	void RunL();
	void DoCancel() { iProperty.Cancel(); }
	CPebbleView& iView;
	TUint32 iKey;
	RProperty iProperty;
	};

class CPebbleView : public CCoeControl
	{
public:
	~CPebbleView() { delete iStateWatch; delete iInfoWatch; delete iCountWatch; delete iBg; }
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		iBg = CAknsBasicBackgroundControlContext::NewL(KAknsIIDQsnBgAreaMain, aRect, EFalse);
		iStateWatch = new (ELeave) CStatusWatcher(*this, KPhePebbleKeyState);
		iInfoWatch = new (ELeave) CStatusWatcher(*this, KPhePebbleKeyInfo);
		iCountWatch = new (ELeave) CStatusWatcher(*this, KPhePebbleKeyDelivered);
		Refresh();
		SetRect(aRect);
		ActivateL();
		}
	/** Attach once the service has defined its keys. */
	void WatchL()
		{
		TRAP_IGNORE(iStateWatch->StartL());
		TRAP_IGNORE(iInfoWatch->StartL());
		TRAP_IGNORE(iCountWatch->StartL());
		Refresh();
		}
	void Refresh()
		{
		TInt s = -1;
		iState = RProperty::Get(KPhePebbleSrvUid, KPhePebbleKeyState, s) == KErrNone ? s : -1;
		if (RProperty::Get(KPhePebbleSrvUid, KPhePebbleKeyInfo, iInfo) != KErrNone)
			iInfo.Zero();
		if (RProperty::Get(KPhePebbleSrvUid, KPhePebbleKeyDelivered, iDelivered) != KErrNone)
			iDelivered = 0;
		DrawDeferred();
		}
	TInt State() const { return iState; }
private:
	void SizeChanged() { if (iBg) iBg->SetRect(Rect()); }
	TTypeUid::Ptr MopSupplyObject(TTypeUid aId)
		{
		if (aId.iUid == MAknsControlContext::ETypeId && iBg)
			return MAknsControlContext::SupplyMopObject(aId, iBg);
		return CCoeControl::MopSupplyObject(aId);
		}
	const TDesC& StateText(TRgb& aColour) const
		{
		_LIT(KNoService, "Starting...");
		_LIT(KNoWatch, "No watch");
		_LIT(KIdle, "Not connected");
		_LIT(KScanning, "Searching");
		_LIT(KConnecting, "Connecting");
		_LIT(KPairing, "Pairing");
		_LIT(KOpening, "Opening session");
		_LIT(KConnected, "Connected");
		_LIT(KNoBt, "Bluetooth LE unavailable");
		aColour = iText;
		switch (iState)
			{
		case EPhePebbleNoWatch: return KNoWatch;
		case EPhePebbleIdle: return KIdle;
		case EPhePebbleScanning: return KScanning;
		case EPhePebbleConnecting: return KConnecting;
		case EPhePebblePairing: return KPairing;
		case EPhePebbleOpening: return KOpening;
		case EPhePebbleConnected: aColour = KAccent; return KConnected;
		case EPhePebbleNoBluetooth: aColour = KWarning; return KNoBt;
		default: return KNoService;
			}
		}
	void Draw(const TRect& aRect) const
		{
		CWindowGc& gc = SystemGc();
		MAknsSkinInstance* skin = AknsUtils::SkinInstance();
		if (!AknsDrawUtils::Background(skin, iBg, this, gc, aRect))
			{
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(KRgbWhite);
			gc.Clear(aRect);
			}
		// text colour from the skin (main area text), so light and dark themes both read well
		TRgb text = KRgbBlack;
		AknsUtils::GetCachedColor(skin, text, KAknsIIDQsnTextColors, EAknsCIQsnTextColorsCG6);
		const_cast<CPebbleView*>(this)->iText = text;

		const TRect r = Rect();
		const TInt margin = r.Width() / 20 + 4;            // Etesian default margin, scaled to the screen
		const CFont* big = AknLayoutUtils::FontFromId(EAknLogicalFontPrimaryFont);
		const CFont* small = AknLayoutUtils::FontFromId(EAknLogicalFontSecondaryFont);
		TInt y = r.iTl.iY + margin + big->AscentInPixels();

		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		TRgb stateColour;
		const TDesC& state = StateText(stateColour);
		// the state dot: accent when connected, outline otherwise
		const TInt dot = big->AscentInPixels() / 2;
		TRect dotRect(TPoint(r.iTl.iX + margin, y - big->AscentInPixels() / 2 - dot / 2), TSize(dot, dot));
		gc.SetPenColor(stateColour);
		if (iState == EPhePebbleConnected)
			{
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(stateColour);
			}
		gc.DrawEllipse(dotRect);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.UseFont(big);
		gc.DrawText(state, TPoint(r.iTl.iX + margin + dot * 2, y));
		gc.DiscardFont();

		gc.UseFont(small);
		gc.SetPenColor(text);
		y += small->HeightInPixels() * 2;
		if (iInfo.Length())
			{
			gc.DrawText(iInfo, TPoint(r.iTl.iX + margin, y));
			y += small->HeightInPixels() * 3 / 2;
			}
		if (iState == EPhePebbleConnected || iDelivered)
			{
			TBuf<64> line;
			line.Format(_L("Notifications sent: %d"), iDelivered);
			gc.DrawText(line, TPoint(r.iTl.iX + margin, y));
			y += small->HeightInPixels() * 3 / 2;
			}
		if (iState == EPhePebbleNoWatch)
			gc.DrawText(_L("Options > Pair a watch"), TPoint(r.iTl.iX + margin, y));
		gc.DiscardFont();
		}

	CAknsBasicBackgroundControlContext* iBg;
	CStatusWatcher* iStateWatch;
	CStatusWatcher* iInfoWatch;
	CStatusWatcher* iCountWatch;
	TInt iState;
	TBuf<128> iInfo;
	TInt iDelivered;
	TRgb iText;
	};

void CStatusWatcher::RunL()
	{
	TInt s = iStatus.Int();
	if (s != KErrNotFound)
		Watch();
	iView.Refresh();
	}

class CPebbleAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		CAknTitlePane* title = (CAknTitlePane*)StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle));
		title->SetTextL(_L("Pebble"));
		iView = new (ELeave) CPebbleView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		StartServiceL();
		iView->WatchL();
		}
	~CPebbleAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		}
private:
	void StartServiceL()
		{
		TFindProcess find(KPhePebbleSrvMatch);
		TFullName name;
		while (find.Next(name) == KErrNone)
			{
			RProcess p;
			if (p.Open(find) == KErrNone)
				{
				TBool alive = p.ExitType() == EExitPending;
				p.Close();
				if (alive)
					return;
				}
			}
		RProcess p;
		User::LeaveIfError(p.Create(KPhePebbleSrvExe, KNullDesC));
		TRequestStatus st;
		p.Rendezvous(st);
		p.Resume();
		User::WaitForRequest(st);       // the service has defined its keys
		p.Close();
		}
	void Send(TInt aCommand)
		{
		iSeq = (iSeq + 1) & 0xFFFF;
		TInt r = RProperty::Set(KPhePebbleSrvUid, KPhePebbleKeyCommand, (iSeq << 8) | aCommand);
		if (r != KErrNone)
			{
			CAknErrorNote* note = new (ELeave) CAknErrorNote;
			note->ExecuteLD(_L("The Pebble service is not running"));
			}
		}
	void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenu)
		{
		if (aResourceId != R_PHEPEBBLE_MENU)
			return;
		const TInt s = iView->State();
		aMenu->SetItemDimmed(EPhePebbleCmdUiTest, s != EPhePebbleConnected);
		aMenu->SetItemDimmed(EPhePebbleCmdUiReconnect, s != EPhePebbleIdle);
		aMenu->SetItemDimmed(EPhePebbleCmdUiForget, s == EPhePebbleNoWatch || s == EPhePebbleNoBluetooth);
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPhePebbleCmdUiPair:
			Send(EPhePebbleCmdPair);
			break;
		case EPhePebbleCmdUiTest:
			Send(EPhePebbleCmdTest);
			break;
		case EPhePebbleCmdUiReconnect:
			Send(EPhePebbleCmdReconnect);
			break;
		case EPhePebbleCmdUiForget:
			Send(EPhePebbleCmdForget);
			break;
		case EAknSoftkeyExit:
		case EAknSoftkeyBack:
		case EEikCmdExit:
			Exit();               // the service keeps running in the background
			break;
		default:
			break;
			}
		}
	void HandleResourceChangeL(TInt aType)
		{
		CAknAppUi::HandleResourceChangeL(aType);
		if (aType == KEikDynamicLayoutVariantSwitch && iView)
			iView->SetRect(ClientRect());
		}
	CPebbleView* iView;
	TInt iSeq;
	};

class CPebbleDocument : public CAknDocument
	{
public:
	CPebbleDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CPebbleAppUi; }
	};

class CPebbleApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KPhePebbleAppUid; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CPebbleDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CPebbleApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
