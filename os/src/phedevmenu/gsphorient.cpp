// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// gsphorient.cpp -- OURS: Control Panel entry "Screen orientation" (next to Display). Automatic (the sensor, or without one the
// display's own orientation) / Portrait / Landscape. The choice is stored (CenRep e7015a01 key 13) and applied at once through
// S60's own preferred-orientation switch (pheorient.h: RAknUiServer::RotateScreen); pheshellsel applies it again at every boot.
// An application that asks for an orientation (SetOrientationL) still gets it. A dialog-type item: GS calls HandleSelection.
#include <e32std.h>
#include <e32debug.h>
#include <implementationproxy.h>
#include <centralrepository.h>
#include <gsplugininterface.h>
#include <gsfwviewuids.h>
#include <gsprivatepluginproviderids.h>
#include <aknlists.h>
#include <aknpopup.h>
#include <avkon.rsg>
#include <badesca.h>
#include "phedevmenu.h"
#include "pheorient.h"

const TUid KPheGsOrientPluginUid = { 0xE7015A06 };
const TInt KPheGsOrientImplUid = 0xE7015A07;

_LIT(KTitle, "Screen orientation");

static TInt PickL(const TDesC& aTitle, CDesCArray* aItems, TInt aCurrent)
	{
	CleanupStack::PushL(aItems);
	CAknSinglePopupMenuStyleListBox* list = new (ELeave) CAknSinglePopupMenuStyleListBox;
	CleanupStack::PushL(list);
	CAknPopupList* popup = CAknPopupList::NewL(list, R_AVKON_SOFTKEYS_OK_CANCEL, AknPopupLayouts::EMenuWindow);
	CleanupStack::PushL(popup);
	list->ConstructL(popup, CEikListBox::ELeftDownInViewRect);
	list->CreateScrollBarFrameL(ETrue);
	list->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
	list->Model()->SetItemTextArray(aItems);
	list->Model()->SetOwnershipType(ELbmOwnsItemArray);
	CleanupStack::Pop(aItems);
	popup->SetTitleL(aTitle);
	list->SetCurrentItemIndex(aCurrent);
	CleanupStack::Pop(popup);
	TInt pick = KErrCancel;
	if (popup->ExecuteLD())
		pick = list->CurrentItemIndex();
	CleanupStack::PopAndDestroy(list);
	return pick;
	}

static void ChooseOrientationL()
	{
	const TInt cur = PheStoredOrientation();
	CDesCArrayFlat* items = new (ELeave) CDesCArrayFlat(3);
	CleanupStack::PushL(items);
	items->AppendL(_L("Automatic"));		// EPheOrientAuto
	items->AppendL(_L("Portrait"));			// EPheOrientPortrait
	items->AppendL(_L("Landscape"));		// EPheOrientLandscape
	CleanupStack::Pop(items);
	const TInt pick = PickL(KTitle, items, cur);
	if (pick < 0 || pick == cur)
		return;
	CRepository* repo = CRepository::NewLC(KPheDevMenuRepoUid);
	User::LeaveIfError(repo->Set(KPheDevKeyOrientation, pick));
	CleanupStack::PopAndDestroy(repo);
	TInt r = KErrNone;
	if (pick == EPheOrientAuto)
		{
		// back to the hardware state's own mode (Normal), what every boot starts from; a sensor turns the screen from there
		TInt pref = 0;
		if (RProperty::Get(KPheUikPSCategory, KPheUikPreferredOrientation, pref) == KErrNone && pref == KPhePreferredAlternate)
			{
			RAknUiServer ui;
			r = ui.Connect();
			if (r == KErrNone)
				{
				r = ui.RotateScreen();
				ui.Close();
				}
			}
		}
	else
		r = PheApplyOrientation(pick);
	RDebug::Printf("PH: [gsphorient] orientation %d stored, applied: %d", pick, r);
	}

class CPheGsOrient : public CGSPluginInterface
	{
public:
	static CPheGsOrient* NewL(TAny*)
		{
		CPheGsOrient* self = new (ELeave) CPheGsOrient;
		CleanupStack::PushL(self);
		self->BaseConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	TUid Id() const { return KPheGsOrientPluginUid; }
	void GetCaptionL(TDes& aCaption) const { aCaption.Copy(KTitle); }
	TInt PluginProviderCategory() const { return KGSPluginProviderInternal; }
	TGSListboxItemTypes ItemType() { return EGSItemTypeSettingDialog; }
	void GetValue(const TGSPluginValueKeys aKey, TDes& aValue)
		{
		if (aKey != EGSPluginKeySettingsItemValueString)
			{
			CGSPluginInterface::GetValue(aKey, aValue);
			return;
			}
		switch (PheStoredOrientation())
			{
		case EPheOrientPortrait:	aValue.Copy(_L("Portrait")); break;
		case EPheOrientLandscape:	aValue.Copy(_L("Landscape")); break;
		default:					aValue.Copy(_L("Automatic")); break;
			}
		}
	CGulIcon* CreateIconL(const TUid aIconType) { return CGSPluginInterface::CreateIconL(aIconType); }
	void HandleSelection(const TGSSelectionTypes)
		{
		TRAPD(err, ChooseOrientationL());
		RDebug::Printf("PH: [gsphorient] selection: %d", err);
		}
private:
	void DoActivateL(const TVwsViewId&, TUid, const TDesC8&) {}
	void DoDeactivate() {}
	};

const TImplementationProxy KPheGsOrientImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheGsOrientImplUid, CPheGsOrient::NewL)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KPheGsOrientImplementationTable) / sizeof(TImplementationProxy);
	return KPheGsOrientImplementationTable;
	}
