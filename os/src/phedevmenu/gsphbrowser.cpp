// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// gsphbrowser.cpp -- OURS: Control Panel entry "Default browser" (next to Display). Lists the applications that declare the
// data type x-scheme-handler/http in their registration (datatype_list; phebrowser does) and makes the chosen one apparc's
// application for that type (RApaLsSession::SetAppForDataType, persisted by apparc -- the 2010 default-app mechanism). The
// Scheme Handler's http(s) plug-in (toolchain/shims/schemedisp/HttpHandler.cpp) opens links with it: FindItem in Messaging,
// Notes and every other application that opens a link through CSchemeHandler. A dialog-type item: GS calls HandleSelection.
#include <e32std.h>
#include <e32debug.h>
#include <implementationproxy.h>
#include <gsplugininterface.h>
#include <gsfwviewuids.h>
#include <gsprivatepluginproviderids.h>
#include <aknlists.h>
#include <aknpopup.h>
#include <avkon.rsg>
#include <badesca.h>
#include <apgcli.h>
#include <apaid.h>
#include <apaidpartner.h>		// TApaAppServiceInfo, CApaAppServiceInfoArray, KOpenServiceUid

const TUid KPheGsBrowserPluginUid = { 0xE7015A08 };
const TInt KPheGsBrowserImplUid = 0xE7015A09;
const TUid KPheBrowserUid = { 0xE7016B20 };		// phebrowser: listed even if its registration is not read yet
_LIT8(KPheSchemeHttpType, "x-scheme-handler/http");
_LIT(KTitle, "Default browser");

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

// the applications that open web links: the declarers of the type (the open service), phebrowser first if none
static void ListBrowsersL(RApaLsSession& aLs, RArray<TUid>& aUids)
	{
	const TDataType type(KPheSchemeHttpType);
	CApaAppServiceInfoArray* impls = NULL;
	TRAPD(r, impls = aLs.GetServiceImplementationsLC(KOpenServiceUid, type); CleanupStack::Pop(impls));
	if (r == KErrNone && impls)
		{
		CleanupStack::PushL(impls);
		TArray<TApaAppServiceInfo> a = impls->Array();
		for (TInt i = 0; i < a.Count(); i++)
			if (aUids.Find(a[i].Uid()) < 0)
				aUids.AppendL(a[i].Uid());
		CleanupStack::PopAndDestroy(impls);
		}
	if (aUids.Find(KPheBrowserUid) < 0)
		aUids.AppendL(KPheBrowserUid);
	}

// the current default: apparc's mapping, else what apparc would pick (the highest-priority declarer), else phebrowser
static TUid CurrentBrowser(RApaLsSession& aLs)
	{
	TUid uid = KNullUid;
	if (aLs.AppForDataType(TDataType(KPheSchemeHttpType), uid) != KErrNone || uid == KNullUid)
		uid = KPheBrowserUid;
	return uid;
	}

static void CaptionL(RApaLsSession& aLs, TUid aUid, TDes& aName)
	{
	TApaAppInfo info;
	if (aLs.GetAppInfo(info, aUid) == KErrNone && info.iCaption.Length())
		aName.Copy(info.iCaption.Left(aName.MaxLength()));
	else
		aName.Format(_L("0x%08x"), aUid.iUid);
	}

static void ChooseBrowserL()
	{
	RApaLsSession ls;
	User::LeaveIfError(ls.Connect());
	CleanupClosePushL(ls);
	RArray<TUid> uids;
	CleanupClosePushL(uids);
	ListBrowsersL(ls, uids);
	const TUid cur = CurrentBrowser(ls);
	CDesCArrayFlat* names = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(names);
	TInt curIdx = 0;
	for (TInt i = 0; i < uids.Count(); i++)
		{
		TBuf<64> n;
		CaptionL(ls, uids[i], n);
		names->AppendL(n);
		if (uids[i] == cur)
			curIdx = i;
		}
	CleanupStack::Pop(names);		// PickL owns it
	const TInt pick = PickL(KTitle, names, curIdx);
	if (pick >= 0 && uids[pick] != cur)
		{
		const TInt r = ls.SetAppForDataType(TDataType(KPheSchemeHttpType), uids[pick]);
		RDebug::Printf("PH: [gsphbrowser] default browser 0x%08x: %d", uids[pick].iUid, r);
		}
	CleanupStack::PopAndDestroy(2, &ls);		// uids, ls
	}

class CPheGsBrowser : public CGSPluginInterface
	{
public:
	static CPheGsBrowser* NewL(TAny*)
		{
		CPheGsBrowser* self = new (ELeave) CPheGsBrowser;
		CleanupStack::PushL(self);
		self->BaseConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	TUid Id() const { return KPheGsBrowserPluginUid; }
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
		RApaLsSession ls;
		if (ls.Connect() != KErrNone)
			return;
		TRAP_IGNORE(CaptionL(ls, CurrentBrowser(ls), aValue));
		ls.Close();
		}
	CGulIcon* CreateIconL(const TUid aIconType) { return CGSPluginInterface::CreateIconL(aIconType); }
	void HandleSelection(const TGSSelectionTypes)
		{
		TRAPD(err, ChooseBrowserL());
		RDebug::Printf("PH: [gsphbrowser] selection: %d", err);
		}
private:
	void DoActivateL(const TVwsViewId&, TUid, const TDesC8&) {}
	void DoDeactivate() {}
	};

const TImplementationProxy KPheGsBrowserImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheGsBrowserImplUid, CPheGsBrowser::NewL)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KPheGsBrowserImplementationTable) / sizeof(TImplementationProxy);
	return KPheGsBrowserImplementationTable;
	}
