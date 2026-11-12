// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// gsphshell.cpp -- OURS: Control Panel entry "UI shell" (Device). Lists every shell there is (the built-in ones and the ones
// installed from the Catalogue, pheshells.h), stores the choice (CenRep e7015a01 key 10) and offers a restart. Always
// visible: with a single shell it lists that one as the current choice. A dialog-type item: GS calls HandleSelection.
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
#include <starterclient.h>
#include "phedevmenu.h"
#include "pheshells.h"

const TUid KPheGsShellPluginUid = { 0xE7015A04 };
const TInt KPheGsShellImplUid = 0xE7015A05;

// a popup list of aItems with aTitle; the picked index, or KErrCancel. aCurrent = the item the cursor starts on
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
	CleanupStack::Pop(aItems);		// the model owns it now
	popup->SetTitleL(aTitle);
	list->SetCurrentItemIndex(aCurrent);
	CleanupStack::Pop(popup);
	TInt pick = KErrCancel;
	if (popup->ExecuteLD())
		pick = list->CurrentItemIndex();
	CleanupStack::PopAndDestroy(list);
	return pick;
	}

static void ChooseShellL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RPheShells shells;
	CleanupClosePushL(shells);
	PheListShellsL(fs, shells);
	CRepository* repo = CRepository::NewLC(KPheDevMenuRepoUid);
	HBufC* cur = PheSelectedShellIdL(repo);
	CleanupStack::PushL(cur);
	CDesCArrayFlat* names = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(names);
	TInt curIdx = 0;
	for (TInt i = 0; i < shells.Count(); i++)
		{
		names->AppendL(shells[i]->Name());
		if (shells[i]->Id().CompareF(*cur) == 0)
			curIdx = i;
		}
	CleanupStack::Pop(names);		// PickL owns it
	const TInt pick = PickL(_L("UI shell"), names, curIdx);
	if (pick >= 0 && pick != curIdx)
		{
		PheStoreShellId(repo, shells[pick]->Id());
		repo->Set(KPheDevKeyShellTries, 0);
		RDebug::Print(_L("PH: [gsphshell] shell %S chosen"), &shells[pick]->Id());
		CDesCArrayFlat* yn = new (ELeave) CDesCArrayFlat(2);
		CleanupStack::PushL(yn);
		yn->AppendL(_L("Restart now"));
		yn->AppendL(_L("Later"));
		CleanupStack::Pop(yn);
		if (PickL(_L("Takes effect after a restart"), yn, 0) == 0)
			{
			RStarterSession starter;
			if (starter.Connect() == KErrNone)
				{
				starter.Reset(RStarterSession::ELanguageSwitchReset);
				RDebug::Printf("PH: [gsphshell] restart requested");
				starter.Close();
				}
			}
		}
	CleanupStack::PopAndDestroy(4, &fs);		// cur, repo, shells, fs
	}

class CPheGsShell : public CGSPluginInterface
	{
public:
	static CPheGsShell* NewL(TAny*)
		{
		CPheGsShell* self = new (ELeave) CPheGsShell;
		CleanupStack::PushL(self);
		self->BaseConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	TUid Id() const { return KPheGsShellPluginUid; }
	void GetCaptionL(TDes& aCaption) const { aCaption.Copy(_L("UI shell")); }
	TInt PluginProviderCategory() const { return KGSPluginProviderInternal; }
	TGSListboxItemTypes ItemType() { return EGSItemTypeSingleLargeDialog; }
	CGulIcon* CreateIconL(const TUid aIconType) { return CGSPluginInterface::CreateIconL(aIconType); }
	void HandleSelection(const TGSSelectionTypes)
		{
		TRAPD(err, ChooseShellL());
		RDebug::Printf("PH: [gsphshell] selection: %d", err);
		}
	TBool Visible() const
		{
		RFs fs;
		if (fs.Connect() != KErrNone)
			return EFalse;
		RPheShells shells;
		TRAP_IGNORE(PheListShellsL(fs, shells));
		const TBool any = shells.Count() > 0;
		PheShellsClose(shells);
		fs.Close();
		return any;
		}
private:
	void DoActivateL(const TVwsViewId&, TUid, const TDesC8&) {}
	void DoDeactivate() {}
	};

const TImplementationProxy KPheGsShellImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheGsShellImplUid, CPheGsShell::NewL)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KPheGsShellImplementationTable) / sizeof(TImplementationProxy);
	return KPheGsShellImplementationTable;
	}
