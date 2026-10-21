// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phestore.cpp -- OURS: the download app ("Get apps"): the items of the indexes that fit this device in a two-line
// list (name / summary, version, installed state); Install downloads, checks and installs the selected one, Details
// shows its description and licence, Refresh reloads the indexes. The work is the engine's (../pheget/pheget.h, shared
// with the pheget command line); its network calls run a nested scheduler, so a wait note keeps drawing meanwhile.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknlists.h>
#include <aknnotewrappers.h>
#include <aknmessagequerydialog.h>
#include <aknwaitdialog.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eiktxlbx.h>
#include <eiktxlbm.h>
#include <avkon.hrh>
#include <aknsutils.h>
#include <aknsdrawutils.h>
#include <aknsbasicbackgroundcontrolcontext.h>
#include <akntitle.h>
#include <eikspane.h>
#include <utf.h>
#include <phestore.rsg>
#include "phestore.hrh"
#include "pheget.h"
#include "phejson.h"

const TUid KPheStoreAppUid = { 0xE70157FA };

static HBufC* FromUtf8LC(const TDesC8& aText)
	{
	HBufC* b = CnvUtfConverter::ConvertToUnicodeFromUtf8L(aText);
	CleanupStack::PushL(b);
	return b;
	}

// the window-owning container: the skinned background + the list
class CPheStoreView : public CCoeControl
	{
public:
	~CPheStoreView() { delete iList; delete iBg; }
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		iBg = CAknsBasicBackgroundControlContext::NewL(KAknsIIDQsnBgAreaMain, aRect, EFalse);
		iList = new (ELeave) CAknDoubleStyleListBox;
		iList->SetContainerWindowL(*this);
		iList->SetMopParent(this);
		iList->ConstructL(this, EAknListBoxSelectionList);
		iList->CreateScrollBarFrameL(ETrue);
		iList->ScrollBarFrame()->SetScrollBarVisibilityL(CEikScrollBarFrame::EOff, CEikScrollBarFrame::EAuto);
		iList->View()->SetListEmptyTextL(_L("No apps for this device"));
		SetRect(aRect);
		ActivateL();
		}
	CDesCArray& Items() { return *static_cast<CDesCArray*>(iList->Model()->ItemTextArray()); }
	void ItemsChangedL()
		{
		iList->HandleItemAdditionL();
		if (Items().Count())                 // (an empty list has no current item: -1 panics EIKON-LISTBOX 5)
			iList->SetCurrentItemIndexAndDraw(0);
		DrawDeferred();
		}
	TInt Current() const { return iList->CurrentItemIndex(); }
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
	CAknDoubleStyleListBox* iList;
	CAknsBasicBackgroundControlContext* iBg;
	};

class CPheStoreAppUi : public CAknAppUi, public MPheGetObserver
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		iGet = CPheGet::NewL(*this);
		iView = new (ELeave) CPheStoreView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		iRefreshPending = CIdle::NewL(CActive::EPriorityIdle);
		iRefreshPending->Start(TCallBack(StartRefresh, this));   // after the first draw
		}
	~CPheStoreAppUi()
		{
		delete iRefreshPending;
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		iItems.Close();
		delete iGet;
		}

	// MPheGetObserver
	void PheGetLog(const TDesC8& aLine) { RDebug::Printf("PH: [phestore] %S", &aLine); }
	void PheGetProgress(TInt /*aDone*/, TInt /*aTotal*/) {}

private:
	static TInt StartRefresh(TAny* aSelf)
		{
		TRAPD(err, static_cast<CPheStoreAppUi*>(aSelf)->RefreshL());
		if (err != KErrNone)
			RDebug::Printf("PH: [phestore] refresh left %d", err);
		return EFalse;
		}

	void ShowWaitL()
		{
		iWait = new (ELeave) CAknWaitDialog(reinterpret_cast<CEikDialog**>(&iWait), ETrue);
		iWait->ExecuteLD(R_PHESTORE_WAIT_NOTE);
		}
	void HideWait()
		{
		if (iWait)
			TRAP_IGNORE(iWait->ProcessFinishedL());
		}

	void RefreshL()
		{
		iItems.Reset();
		iView->Items().Reset();
		iGet->ResetIndexes();
		ShowWaitL();
		TRAPD(err, iGet->LoadIndexesL(KNullDesC, EFalse));
		HideWait();
		if (err == KErrNone)
			iGet->ItemsL(iItems, EFalse);
		for (TInt i = 0; i < iItems.Count(); ++i)
			AppendRowL(iItems[i]);
		iView->ItemsChangedL();
		CAknTitlePane* title = (CAknTitlePane*)StatusPane()->ControlL(TUid::Uid(EEikStatusPaneUidTitle));
		TBuf<40> t;
		t.Format(_L("Get apps (%d)"), iItems.Count());
		title->SetTextL(t);
		if (err != KErrNone)
			{
			CAknErrorNote* note = new (ELeave) CAknErrorNote(ETrue);
			TBuf<64> msg;
			msg.Format(_L("Cannot load the app lists (%d)"), err);
			note->ExecuteLD(msg);
			}
		}

	// "\t<name>\t<summary> -- <tag>[, installed <tag>]"
	void AppendRowL(const TPheGetItem& aItem)
		{
		TBuf8<32> installed;
		iGet->InstalledTagL(aItem.Id(), installed);
		HBufC* name = FromUtf8LC(aItem.Name().Length() ? aItem.Name() : aItem.Id());
		HBufC* summary = FromUtf8LC(aItem.Summary());
		HBufC* tag = FromUtf8LC(aItem.Tag());
		HBufC* inst = FromUtf8LC(installed);
		HBufC* row = HBufC::NewLC(name->Length() + summary->Length() + tag->Length() + inst->Length() + 32);
		TPtr r = row->Des();
		r.Append('\t');
		r.Append(*name);
		r.Append('\t');
		r.Append(*summary);
		if (tag->Length())
			{
			r.Append(_L(" -- "));
			r.Append(*tag);
			}
		if (inst->Length() && *inst == *tag)
			r.Append(_L(", installed"));
		else if (inst->Length())
			{
			r.Append(_L(", installed "));     // an older release: an update is available
			r.Append(*inst);
			}
		iView->Items().AppendL(r);
		CleanupStack::PopAndDestroy(5, name);
		}

	void InstallL()
		{
		TInt i = iView->Current();
		if (i < 0 || i >= iItems.Count())
			return;
		ShowWaitL();
		TInt result = KErrGeneral;
		TRAPD(err, result = iGet->InstallL(iItems[i], 'c'));
		HideWait();
		if (err != KErrNone)
			result = err;
		HBufC* name = FromUtf8LC(iItems[i].Name());
		TBuf<128> msg;
		if (result == KErrNone)
			{
			msg.Format(_L("%S installed"), name);
			CAknConfirmationNote* note = new (ELeave) CAknConfirmationNote(ETrue);
			note->ExecuteLD(msg);
			}
		else
			{
			msg.Format(_L("%S: install failed (%d)"), name, result);
			CAknErrorNote* note = new (ELeave) CAknErrorNote(ETrue);
			note->ExecuteLD(msg);
			}
		CleanupStack::PopAndDestroy(name);
		// the installed state changed: rebuild the rows (the items stay valid: no index reload)
		iView->Items().Reset();
		for (TInt k = 0; k < iItems.Count(); ++k)
			AppendRowL(iItems[k]);
		iView->ItemsChangedL();
		}

	void DetailsL()
		{
		TInt i = iView->Current();
		if (i < 0 || i >= iItems.Count())
			return;
		const CPheJson& it = *iItems[i].iItem;
		HBufC* name = FromUtf8LC(iItems[i].Name());
		TPtrC8 desc8 = it.GetStr(_L8("description"));
		HBufC* desc = FromUtf8LC(desc8.Length() ? desc8 : iItems[i].Summary());
		HBufC* lic = FromUtf8LC(it.GetStr(_L8("license")));
		HBufC* home = FromUtf8LC(it.GetStr(_L8("homepage")));
		HBufC* tag = FromUtf8LC(iItems[i].Tag());
		HBufC* text = HBufC::NewLC(desc->Length() + lic->Length() + home->Length() + tag->Length() + 64);
		TPtr t = text->Des();
		t.Append(*desc);
		t.Append(_L("\n\nVersion: "));
		t.Append(tag->Length() ? static_cast<const TDesC&>(*tag) : static_cast<const TDesC&>(_L("none approved")));
		t.Append(_L("\nLicence: "));
		t.Append(*lic);
		if (home->Length())
			{
			t.Append(_L("\n"));
			t.Append(*home);
			}
		CAknMessageQueryDialog* dlg = CAknMessageQueryDialog::NewL(*text);
		dlg->PrepareLC(R_AVKON_MESSAGE_QUERY_DIALOG);
		dlg->QueryHeading()->SetTextL(*name);
		dlg->RunLD();
		CleanupStack::PopAndDestroy(6, name);
		}

	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheStoreCmdInstall:
			InstallL();
			break;
		case EPheStoreCmdDetails:
			DetailsL();
			break;
		case EPheStoreCmdRefresh:
			RefreshL();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
			}
		}

	CPheGet* iGet;
	CPheStoreView* iView;
	RArray<TPheGetItem> iItems;
	CAknWaitDialog* iWait;
	CIdle* iRefreshPending;
	};

class CPheStoreDocument : public CAknDocument
	{
public:
	CPheStoreDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CPheStoreAppUi; }
	};

class CPheStoreApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KPheStoreAppUid; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CPheStoreDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CPheStoreApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
