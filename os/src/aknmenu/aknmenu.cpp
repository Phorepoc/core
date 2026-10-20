// aknmenu.cpp -- OURS: an INTERIM S60 launcher on the from-source stack. It asks the real
// AppList server (apsexe, RApaLsSession) for the installed applications, shows them in a real
// Avkon selection list and starts the chosen one. The real launcher is the Symbian^3 matrixmenu;
// this is the demoable stand-in while its closure is built.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknlists.h>
#include <akntitle.h>
#include <eikspane.h>
#include <eiklbo.h>
#include <eikenv.h>
#include <badesca.h>
#include <apgcli.h>
#include <apacmdln.h>
#include <AknsBasicBackgroundControlContext.h>
#include <AknsDrawUtils.h>
#include <AknsUtils.h>
#include <avkon.hrh>
#include <aknmenu.rsg>
#include "aknmenu.hrh"

const TUid KUidAknMenu = { 0xE7015704 };

// The standard S60 main-pane container: owns the window + the skin background context
// (QsnBgAreaMain) that the list box finds through the MOP chain -- without one a list clears
// itself to a LAF colour instead of drawing the skin.
class CMenuContainer : public CCoeControl
	{
public:
	void ConstructL(const TRect& aRect, MEikListBoxObserver* aObserver)
		{
		CreateWindowL();
		iBg = CAknsBasicBackgroundControlContext::NewL(KAknsIIDQsnBgAreaMain, aRect, EFalse);
		iList = new (ELeave) CAknSingleStyleListBox;
		iList->SetContainerWindowL(*this);
		iList->SetMopParent(this);
		iList->ConstructL(this, EAknListBoxSelectionList);
		iList->SetListBoxObserver(aObserver);
		iList->CreateScrollBarFrameL(ETrue);
		iList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
		SetRect(aRect);
		}
	~CMenuContainer()
		{
		delete iList;
		delete iBg;
		}
	CAknSingleStyleListBox* List() const { return iList; }
	TInt CountComponentControls() const { return 1; }
	CCoeControl* ComponentControl(TInt) const { return iList; }
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
		{
		return iList->OfferKeyEventL(aKey, aType);
		}
protected:
	void SizeChanged()
		{
		if (iBg)
			iBg->SetRect(Rect());
		if (iList)
			iList->SetRect(Rect());
		}
	void Draw(const TRect& aRect) const
		{
		CWindowGc& gc = SystemGc();
		if (!AknsDrawUtils::Background(AknsUtils::SkinInstance(), iBg, this, gc, aRect))
			gc.Clear(aRect);
		}
	TTypeUid::Ptr MopSupplyObject(TTypeUid aId)
		{
		if (aId.iUid == MAknsControlContext::ETypeId && iBg)
			return MAknsControlContext::SupplyMopObject(aId, iBg);
		return CCoeControl::MopSupplyObject(aId);
		}
private:
	CAknSingleStyleListBox* iList;
	CAknsBasicBackgroundControlContext* iBg;
	};

class CMenuAppUi : public CAknAppUi, public MEikListBoxObserver
	{
public:
	void ConstructL()
		{
		RDebug::Printf("[aknmenu] AppUi::ConstructL");
		BaseConstructL();
		CAknTitlePane* title = (CAknTitlePane*)StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle));
		title->SetTextL(_L("Menu"));
		User::LeaveIfError(iLs.Connect());
		iContainer = new (ELeave) CMenuContainer;
		iContainer->SetMopParent(this);
		iContainer->ConstructL(ClientRect(), this);
		iList = iContainer->List();
		PopulateL();
		iList->HandleItemAdditionL();
		iContainer->ActivateL();
		AddToStackL(iContainer);
		RDebug::Printf("[aknmenu] list up, %d apps", iNames.Count());
		}
	~CMenuAppUi()
		{
		if (iContainer)
			RemoveFromStack(iContainer);
		delete iContainer;
		iNames.ResetAndDestroy();
		iLs.Close();
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EAknMenuCmdOpen:
			LaunchCurrentL();
			break;
		case EAknMenuCmdRefresh:
			PopulateL();
			iList->HandleItemAdditionL();
			iList->DrawNow();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
			}
		}
	void HandleListBoxEventL(CEikListBox*, TListBoxEvent aEvent)
		{
		if (aEvent == EEventEnterKeyPressed || aEvent == EEventItemDoubleClicked || aEvent == EEventItemSingleClicked)
			LaunchCurrentL();
		}
	void HandleResourceChangeL(TInt aType)
		{
		CAknAppUi::HandleResourceChangeL(aType);
		if (aType == KEikDynamicLayoutVariantSwitch && iContainer)
			iContainer->SetRect(ClientRect());
		}
private:
	void PopulateL()
		{
		// the AppList server scans asynchronously after boot: give it a moment
		TInt r = KErrNotReady;
		for (TInt i = 0; i < 20 && r != KErrNone; i++)
			{
			r = iLs.GetAllApps();
			if (r != KErrNone)
				User::After(250000);
			}
		RDebug::Printf("[aknmenu] GetAllApps r=%d", r);
		CDesCArray* items = (CDesCArray*)iList->Model()->ItemTextArray();
		items->Reset();
		iNames.ResetAndDestroy();
		TApaAppInfo info;
		while (r == KErrNone && iLs.GetNextApp(info) == KErrNone)
			{
			if (info.iUid == KUidAknMenu)
				continue;
			TBuf<KApaMaxAppCaption + 4> line;
			line.Append('\t');
			line.Append(info.iCaption.Length() ? info.iCaption : info.iFullName);
			line.Append(_L("\t\t"));
			items->AppendL(line);
			HBufC* name = info.iFullName.AllocLC();
			iNames.AppendL(name);
			CleanupStack::Pop(name);
			RDebug::Print(_L("[aknmenu] app %08x %S"), info.iUid.iUid, &info.iFullName);
			}
		}
	void LaunchCurrentL()
		{
		TInt i = iList->CurrentItemIndex();
		if (i < 0 || i >= iNames.Count())
			return;
		CApaCommandLine* cmd = CApaCommandLine::NewLC();
		cmd->SetExecutableNameL(*iNames[i]);
		cmd->SetCommandL(EApaCommandRun);
		TInt r = iLs.StartApp(*cmd);
		CleanupStack::PopAndDestroy(cmd);
		RDebug::Print(_L("[aknmenu] launch %S r=%d"), iNames[i], r);
		}
	RApaLsSession iLs;
	CMenuContainer* iContainer;
	CAknSingleStyleListBox* iList;		// owned by iContainer
	RPointerArray<HBufC> iNames;
	};

class CMenuDocument : public CAknDocument
	{
public:
	CMenuDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CMenuAppUi; }
	};

class CMenuApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KUidAknMenu; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CMenuDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CMenuApplication;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("[aknmenu] E32Main");
	TInt r = EikStart::RunApplication(NewApplication);
	RDebug::Printf("[aknmenu] RunApplication returned %d", r);
	return r;
	}
