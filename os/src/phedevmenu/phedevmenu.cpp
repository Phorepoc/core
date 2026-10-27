// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phedevmenu.cpp -- OURS: the hidden developer settings application.
// An Avkon settings list (CAknSettingItemList) whose values live in the CenRep repository KPheDevMenuRepoUid:
//   UI shell (S^3 | S^4, only those the image carries: key 6), Control Panel shortcut (off | on), boot guard timeout (seconds).
// Opened by the dialer secret code *#3386633# through the Phone app's real code path (tools/patch_phone_devmenu.py)
// and by the Control Panel entry (src/phedevmenu/gsphedevmenu, when that shortcut is switched on).
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknsettingitemlist.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <avkon.hrh>
#include <centralrepository.h>
#include <aknsutils.h>
#include <aknsconstants.h>
#include <aknsdrawutils.h>
#include <aknsbasicbackgroundcontrolcontext.h>
#include <akntitle.h>
#include <eikspane.h>
#include <phedevmenu.rsg>
#include "phedevmenu.hrh"
#include "phedevmenu.h"

class CPheDevList : public CAknSettingItemList
	{
public:
	CPheDevList(CRepository& aRepo) : iRepo(aRepo) {}
	// a single-renderer image (UI_RENDERER=classic|alfred) has nothing to choose: hide the item (key 5 = the renderers present)
	void HideRendererIfFixedL()
		{
		if (Get(KPheDevKeyRenderersAvail, KPheRenderersClassic | KPheRenderersAlfred) == (KPheRenderersClassic | KPheRenderersAlfred))
			return;
		for (TInt i = 0; i < SettingItemArray()->Count(); i++)
			if ((*SettingItemArray())[i]->Identifier() == EPheItemRenderer)
				(*SettingItemArray())[i]->SetHidden(ETrue);
		HandleChangeInItemArrayOrVisibilityL();
		}
	// the UI shell item lists only the shells THIS image carries (key 6, SHELLS= at build time): one shell = nothing to choose,
	// the item is hidden (and key 1 keeps the ROM default for that shell)
	void HideShellIfFixedL()
		{
		if (PheShellsInRom() == (KPheShellsS3 | KPheShellsS4))   // the ROM, not key 6 (persisted to C:)
			return;
		for (TInt i = 0; i < SettingItemArray()->Count(); i++)
			if ((*SettingItemArray())[i]->Identifier() == EPheItemShell)
				(*SettingItemArray())[i]->SetHidden(ETrue);
		HandleChangeInItemArrayOrVisibilityL();
		}
	void LoadValues()
		{
		iShell = Get(KPheDevKeyShell, KPheShellS3);
		iCp = Get(KPheDevKeyCpShortcut, 0);
		iGuard = Get(KPheDevKeyGuardTimeout, KPheGuardDefault);
		iRender = Get(KPheDevKeyRenderer, KPheRendererClassic);
		iAa = Get(KPheDevKeyAntialias, KPheAntialiasBest);
		iGpu = Get(KPheDevKeyGpu, KPheGpuOff);
		iCompose = Get(KPheDevKeyCompose, KPheComposeOn);
		RDebug::Printf("[phedevmenu] loaded: shell=%d cpshortcut=%d guard=%d renderer=%d antialias=%d gpu=%d compose=%d", iShell, iCp, iGuard, iRender, iAa, iGpu, iCompose);
		}
	CAknSettingItem* CreateSettingItemL(TInt aId)
		{
		switch (aId)
			{
		case EPheItemShell:		return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iShell);
		case EPheItemCpShortcut:	return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iCp);
		case EPheItemRenderer:		return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iRender);
		case EPheItemAntialias:		return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iAa);
		case EPheItemGpu:		return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iGpu);
		case EPheItemCompose:		return new (ELeave) CAknEnumeratedTextPopupSettingItem(aId, iCompose);
		case EPheItemGuardTimeout:	return new (ELeave) CAknIntegerEdwinSettingItem(aId, iGuard);
			}
		return NULL;
		}
	void EditItemL(TInt aIndex, TBool aFromMenu)
		{
		CAknSettingItemList::EditItemL(aIndex, aFromMenu);
		(*SettingItemArray())[aIndex]->StoreL();
		if (iGuard < KPheGuardMin) iGuard = KPheGuardMin;
		if (iGuard > KPheGuardMax) iGuard = KPheGuardMax;
		Save();
		LoadSettingsL();
		DrawNow();
		}
	void ChangeCurrentL() { EditItemL(ListBox()->CurrentItemIndex(), ETrue); }
private:
	TInt Get(TUint32 aKey, TInt aDefault)
		{
		TInt v;
		return iRepo.Get(aKey, v) == KErrNone ? v : aDefault;
		}
	void Save()
		{
		TInt e1 = iRepo.Set(KPheDevKeyShell, iShell);
		TInt e2 = iRepo.Set(KPheDevKeyCpShortcut, iCp);
		TInt e3 = iRepo.Set(KPheDevKeyGuardTimeout, iGuard);
		TInt e4 = iRepo.Set(KPheDevKeyRenderer, iRender);
		TInt e5 = iRepo.Set(KPheDevKeyAntialias, iAa);
		TInt e6 = iRepo.Set(KPheDevKeyGpu, iGpu);
		TInt e7 = iRepo.Set(KPheDevKeyCompose, iCompose);
		RDebug::Printf("[phedevmenu] saved renderer=%d (%d) antialias=%d (%d) gpu=%d (%d) compose=%d (%d)", iRender, e4, iAa, e5, iGpu, e6, iCompose, e7);
		RDebug::Printf("[phedevmenu] saved: shell=%d cpshortcut=%d guard=%d (%d %d %d)", iShell, iCp, iGuard, e1, e2, e3);
		}
	CRepository& iRepo;
	TInt iShell, iCp, iGuard, iRender, iAa, iGpu, iCompose;
	};

// the window-owning container; the list is a component control
class CPheDevView : public CCoeControl
	{
public:
	~CPheDevView() { delete iList; delete iBg; }
	void ConstructL(const TRect& aRect, CRepository& aRepo)
		{
		CreateWindowL();
		// the skin background (as every Avkon container: without the context and EAknEnableSkin the window stays the
		// default cyan and the setting list falls back to its un-skinned colours)
		iBg = CAknsBasicBackgroundControlContext::NewL(KAknsIIDQsnBgAreaMain, aRect, EFalse);
		iList = new (ELeave) CPheDevList(aRepo);
		iList->SetContainerWindowL(*this);
		iList->SetMopParent(this);
		iList->ConstructFromResourceL(R_PHEDEVMENU_ITEMS);
		iList->LoadValues();
		iList->LoadSettingsL();
		iList->HideRendererIfFixedL();
		iList->HideShellIfFixedL();
		SetRect(aRect);
		ActivateL();
		}
	CPheDevList* List() { return iList; }
private:
	TInt CountComponentControls() const { return iList ? 1 : 0; }
	CCoeControl* ComponentControl(TInt) const { return iList; }
	void SizeChanged()
		{
		if (iBg) iBg->SetRect(Rect());
		if (iList) iList->SetRect(Rect());
		}
	void Draw(const TRect& aRect) const
		{
		CWindowGc& gc = SystemGc();
		if (!AknsDrawUtils::Background(AknsUtils::SkinInstance(), iBg, this, gc, aRect))
			{
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(KRgbWhite);
			gc.Clear(aRect);
			}
		}
	TTypeUid::Ptr MopSupplyObject(TTypeUid aId)
		{
		if (aId.iUid == MAknsControlContext::ETypeId && iBg)
			return MAknsControlContext::SupplyMopObject(aId, iBg);
		return CCoeControl::MopSupplyObject(aId);
		}
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
		{
		return iList ? iList->OfferKeyEventL(aKey, aType) : EKeyWasNotConsumed;
		}
	CPheDevList* iList;
	CAknsBasicBackgroundControlContext* iBg;
	};

class CPheDevAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		// the title pane (the status pane comes from the app's resource, r_phedevmenu_status_pane)
		CAknTitlePane* title = (CAknTitlePane*)StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle));
		title->SetTextL(_L("Developer menu"));
		iRepo = CRepository::NewL(KPheDevMenuRepoUid);
		iView = new (ELeave) CPheDevView;
		iView->ConstructL(ClientRect(), *iRepo);
		AddToStackL(iView);
		RDebug::Printf("[phedevmenu] up");
		}
	~CPheDevAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		delete iRepo;
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheDevCmdChange:
			iView->List()->ChangeCurrentL();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
			}
		}
private:
	CRepository* iRepo;
	CPheDevView* iView;
	};

class CPheDevDocument : public CAknDocument
	{
public:
	CPheDevDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CPheDevAppUi; }
	};

class CPheDevApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KPheDevMenuAppUid; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CPheDevDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CPheDevApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
