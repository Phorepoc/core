// gsphedevmenu.cpp -- OURS: Control Panel entry for the developer menu (docs/phorepoc/devmenu.md).
// A GS plug-in whose item is visible only while CenRep KPheDevMenuRepoUid key 2 is 1; selecting it starts the hidden
// settings application (a dialog-type item: GS calls HandleSelection instead of activating a view).
#include <e32std.h>
#include <e32debug.h>
#include <implementationproxy.h>
#include <apgcli.h>
#include <centralrepository.h>
#include <gsplugininterface.h>
#include <gsfwviewuids.h>
#include <gsprivatepluginproviderids.h>
#include "phedevmenu.h"

const TUid KPheGsPluginUid = { 0xE7015A02 };
const TInt KPheGsPluginImplUid = 0xE7015A03;

class CPheGsDevMenu : public CGSPluginInterface
	{
public:
	static CPheGsDevMenu* NewL(TAny*)
		{
		CPheGsDevMenu* self = new (ELeave) CPheGsDevMenu;
		CleanupStack::PushL(self);
		self->BaseConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	TUid Id() const { return KPheGsPluginUid; }
	void GetCaptionL(TDes& aCaption) const { aCaption.Copy(_L("Developer menu")); }
	TInt PluginProviderCategory() const { return KGSPluginProviderInternal; }
	TGSListboxItemTypes ItemType() { return EGSItemTypeSingleLargeDialog; }
	CGulIcon* CreateIconL(const TUid aIconType) { return CGSPluginInterface::CreateIconL(aIconType); }
	void HandleSelection(const TGSSelectionTypes)
		{
		RApaLsSession ls;
		if (ls.Connect() == KErrNone)
			{
			TThreadId t;
			TInt r = ls.StartDocument(KNullDesC, KPheDevMenuAppUid, t);
			RDebug::Printf("[gsphedevmenu] start the developer menu: %d", r);
			ls.Close();
			}
		}
	TBool Visible() const
		{
		TInt on = 0;
		CRepository* repo = NULL;
		TRAPD(e, repo = CRepository::NewL(KPheDevMenuRepoUid));
		if (e == KErrNone)
			{
			repo->Get(KPheDevKeyCpShortcut, on);
			delete repo;
			}
		return on != 0;
		}
private:
	// views are not activated (dialog item); the CAknView hooks do nothing
	void DoActivateL(const TVwsViewId&, TUid, const TDesC8&) {}
	void DoDeactivate() {}
	};

const TImplementationProxy KPheGsImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheGsPluginImplUid, CPheGsDevMenu::NewL)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KPheGsImplementationTable) / sizeof(TImplementationProxy);
	return KPheGsImplementationTable;
	}
