// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phestore.cpp -- OURS: the download app ("Catalogue"). A full-screen, custom-drawn list of the items of the indexes
// that fit this device (pheui.cpp: header, 83 px rows, kinetic scrolling, details page; pheicons.cpp: lazy icons), over
// the engine (../pheget/pheget.h, shared with the pheget command line). It starts fast: the first frame (header +
// spinner) is drawn before anything else happens; the last index is read from its cache on C: and shown at once, a
// few rows per idle slice; only then the network refreshes the cache (the list is rebuilt when the index changed).
// The engine's network calls run a nested scheduler, so the list stays live and the spinner keeps turning.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <avkon.hrh>
#include <eikbtgpc.h>
#include <eikspane.h>
#include <eikenv.h>
#include <utf.h>
#include <avkon.rsg>
#include <phestore.rsg>
#include "phestore.hrh"
#include "pheget.h"
#include "phejson.h"
#include "pheui.h"
#include "pheicons.h"
#include "pheworker.h"

const TUid KPheStoreAppUid = { 0xE70157FA };

static TInt64 NowUs()
	{
	TTime t;
	t.UniversalTime();
	return t.Int64();
	}
static TInt64 gStart;
TInt PheStoreSince() { return TInt((NowUs() - gStart) / 1000); }   // ms since start (the timing log)
static TInt Since() { return PheStoreSince(); }

class CPheStoreAppUi : public CAknAppUi, public MPheGetObserver, public MPheCatalogueUi, public MPheJobObserver
	{
public:
	void ConstructL()
		{
		gStart = NowUs();
		BaseConstructL(EAknEnableSkin);
		StatusPane()->MakeVisible(EFalse);             // our own header: the whole screen is the app's
		iGet = CPheGet::NewL(*this);
		iView = CPheCatalogueView::NewL(ClientRect(), *this);
		AddToStackL(iView);
		iLoader = CPheIconLoader::NewL(*iView);
		iRefreshJob = CPheJob::NewL(*this);
		iWork = CIdle::NewL(CActive::EPriorityIdle);
		iPhase = EPhaseCache;
		iView->SetLoad(CPheCatalogueView::ELoading, KNullDesC);
		}
	~CPheStoreAppUi()
		{
		delete iWork;
		delete iRefreshJob;
		delete iLoader;
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		iItems.Close();
		delete iGet;
		delete iDetailId;
		}

	// MPheGetObserver
	void PheGetLog(const TDesC8& aLine) { RDebug::Printf("PH: [phestore] %S", &aLine); }
	void PheGetProgress(TInt aDone, TInt aTotal)
		{
		if (iView->Mode() == CPheCatalogueView::EDetails && aTotal > 0)
			{
			TInt p = TInt(TInt64(aDone) * 1000 / aTotal);
			if (p != iLastProgress)
				{
				iLastProgress = p;
				_LIT(KDownloading, "Downloading");
				iView->SetDetailsStatus(KDownloading, EFalse, p);
				}
			}
		}

	// MPheCatalogueUi
	void UiFirstPaint()
		{
		RDebug::Printf("PH: [phestore] TIMING first paint %d ms", Since());
		iWork->Start(TCallBack(WorkCb, this));         // after this frame
		}
	void UiIconWanted() { iLoader->Kick(); }
	void UiRetry()
		{
		iPhase = EPhaseRefresh;
		iView->SetLoad(CPheCatalogueView::ELoading, KNullDesC);
		Schedule();
		}
	void UiOpen(TInt aRow) { TRAP_IGNORE(OpenDetailsL(aRow)); }
	void UiBack()
		{
		iView->ShowList();
		TRAP_IGNORE(Cba()->SetCommandSetL(R_AVKON_SOFTKEYS_OPTIONS_EXIT));
		Cba()->DrawDeferred();
		if (iRebuildPending)
			{
			iRebuildPending = EFalse;
			Rebuild();
			Schedule();
			}
		}
	void UiAction(TInt /*aRow*/) { TRAP_IGNORE(InstallL()); }

private:
	enum TPhase { EPhaseIdle, EPhaseCache, EPhaseBuild, EPhaseRefresh };

	static TInt WorkCb(TAny* aSelf)
		{
		return static_cast<CPheStoreAppUi*>(aSelf)->Work();
		}
	void Schedule() { if (!iWork->IsActive()) iWork->Start(TCallBack(WorkCb, this)); }

	// one slice of the start-up / refresh state machine; ETrue = call again
	TInt Work()
		{
		switch (iPhase)
			{
		case EPhaseCache:
			{
			iGet->ResetIndexes();
			TRAPD(err, iGet->LoadIndexesL(KNullDesC, EFalse, ETrue));
			if (err == KErrNone)
				{
				BeginBuild();
				iNextAfterBuild = EPhaseRefresh;
				iPhase = EPhaseBuild;
				return ETrue;
				}
			iPhase = EPhaseRefresh;                    // nothing cached: the spinner shows until the network answers
			return ETrue;
			}
		case EPhaseBuild:
			if (BuildSlice())
				return ETrue;
			RDebug::Printf("PH: [phestore] TIMING list ready %d ms (%d items, %s)", Since(), iItems.Count(), iFresh ? "fresh" : "cached");
			iPhase = iNextAfterBuild;
			if (iPhase == EPhaseIdle)
				iView->SetLoad(CPheCatalogueView::EReady, KNullDesC);
			iLoader->Kick();
			return iPhase != EPhaseIdle;
		case EPhaseRefresh:
			RefreshNow();
			return EFalse;
		default:
			return EFalse;
			}
		}

	// the network refresh runs on its own thread (pheworker.h): the UI stays live through the TLS handshake
	void RefreshNow()
		{
		iPhase = EPhaseIdle;
		if (iRefreshJob->Busy())
			return;
		TInt r = iRefreshJob->Start(1, CPheJob::ERefresh, KNullDesC, KNullDesC);
		if (r != KErrNone)
			JobDone(1, r);
		}

	// MPheJobObserver
	void JobDone(TInt /*aId*/, TInt aResult)
		{
		TInt err = aResult < 0 ? aResult : KErrNone;
		TBool changed = aResult == 1;
		RDebug::Printf("PH: [phestore] TIMING refresh %s (%d) at %d ms", err == KErrNone ? (changed ? "changed" : "unchanged") : "failed", err, Since());
		if (err == KErrNone)
			iLoader->NetworkChanged();
		if (err == KErrNone && (changed || iItems.Count() == 0))
			{
			iFresh = ETrue;
			if (iView->Mode() == CPheCatalogueView::EDetails)
				{
				iRebuildPending = ETrue;               // the details page stays as it is until back
				iView->SetLoad(CPheCatalogueView::EReady, KNullDesC);
				return;
				}
			Rebuild();
			Schedule();
			return;
			}
		if (iItems.Count() == 0)
			{
			TBuf<64> msg;
			msg.Format(_L("Cannot reach the catalogue (%d)"), err);
			iView->SetLoad(CPheCatalogueView::EFailed, msg);
			}
		else
			iView->SetLoad(CPheCatalogueView::EReady, KNullDesC);
		}

	// reload the indexes from their (fresh) caches and rebuild the rows, keeping the scroll position
	void Rebuild()
		{
		iKeepOffset = iView->RowCount() > 0 ? iView->Offset() : 0;
		iGet->ResetIndexes();
		TRAPD(err, iGet->LoadIndexesL(KNullDesC, EFalse, ETrue));
		if (err != KErrNone)
			{
			iItems.Reset();
			iView->SetLoad(CPheCatalogueView::EFailed, _L("Cannot read the catalogue"));
			return;
			}
		BeginBuild();
		iNextAfterBuild = EPhaseIdle;
		iPhase = EPhaseBuild;                          // (the caller runs Work again: a CIdle from inside its own RunL returns ETrue)
		}

	void BeginBuild()
		{
		iItems.Reset();
		TRAPD(err, iGet->ItemsL(iItems, EFalse));
		if (err != KErrNone)
			iItems.Reset();
		iView->ClearRows();
		iBuilt = 0;
		}

	// a few rows per slice, so a touch is never starved
	TBool BuildSlice()
		{
		TInt64 start = NowUs();
		while (iBuilt < iItems.Count())
			{
			TRAPD(err, AddRowL(iItems[iBuilt]));
			if (err != KErrNone)
				{
				iBuilt = iItems.Count();
				break;
				}
			++iBuilt;
			if (NowUs() - start > 8000)
				break;
			}
		if (iBuilt == 1 || iBuilt >= iItems.Count() || iBuilt % 8 == 0)
			iView->Redraw();
		if (iBuilt >= iItems.Count())
			{
			if (iKeepOffset)
				iView->SetOffset(iKeepOffset);
			iKeepOffset = 0;
			iView->Redraw();
			return EFalse;
			}
		return ETrue;
		}

	void AddRowL(const TPheGetItem& aItem)
		{
		TBuf8<32> installed;
		iGet->InstalledTagL(aItem.Id(), installed);
		TBuf<96> name;
		TBuf<160> info;
		TBuf<48> tag;
		TPtrC8 n8 = aItem.Name().Length() ? aItem.Name() : aItem.Id();
		CnvUtfConverter::ConvertToUnicodeFromUtf8(name, n8.Left(120));
		TBuf<160> summary;
		CnvUtfConverter::ConvertToUnicodeFromUtf8(summary, aItem.Summary().Left(150));
		CnvUtfConverter::ConvertToUnicodeFromUtf8(info, aItem.Category().Left(40));
		if (info.Length() && summary.Length())
			info.Append(_L(" - "));
		info.Append(summary.Left(info.MaxLength() - info.Length()));
		CnvUtfConverter::ConvertToUnicodeFromUtf8(tag, aItem.Tag().Left(40));
		TInt state = TPheRow::EFree;
		if (installed.Length())
			state = (installed == aItem.Tag()) ? TPheRow::EInstalled : TPheRow::EUpdate;
		iView->AddRowL(name, info, tag, state, aItem.iItem->GetStr(_L8("icon_url")));
		}

	static void Fmt(TDes& aOut, TInt64 aBytes)
		{
		if (aBytes >= 1024 * 1024)
			aOut.Format(_L("%d.%d MB"), TInt(aBytes / (1024 * 1024)), TInt((aBytes * 10 / (1024 * 1024)) % 10));
		else
			aOut.Format(_L("%d KB"), TInt((aBytes + 1023) / 1024));
		}

	void OpenDetailsL(TInt aRow)
		{
		if (aRow < 0 || aRow >= iItems.Count() || iBusy)
			return;
		const TPheGetItem& it = iItems[aRow];
		const CPheJson& j = *it.iItem;
		delete iDetailId;
		iDetailId = NULL;
		iDetailId = it.Id().AllocL();
		TBuf<96> name;
		CnvUtfConverter::ConvertToUnicodeFromUtf8(name, (it.Name().Length() ? it.Name() : it.Id()).Left(120));
		TBuf<400> meta;
		TBuf<64> tmp;
		CnvUtfConverter::ConvertToUnicodeFromUtf8(tmp, it.Tag().Left(40));
		if (tmp.Length())
			meta.Append(tmp);
		else
			meta.Append(_L("No approved release"));
		meta.Append('\n');
		CnvUtfConverter::ConvertToUnicodeFromUtf8(tmp, it.Category().Left(40));
		meta.Append(tmp);
		const CPheJson* size = it.iRelease ? it.iRelease->Get(_L8("size")) : NULL;
		if (size && size->Type() == CPheJson::ENumber && size->Int() > 0)
			{
			meta.Append('\n');
			Fmt(tmp, size->Int());
			meta.Append(tmp);
			}
		CnvUtfConverter::ConvertToUnicodeFromUtf8(tmp, j.GetStr(_L8("license")).Left(40));
		if (tmp.Length())
			{
			meta.Append('\n');
			meta.Append(tmp);
			}
		TPtrC8 desc8 = j.GetStr(_L8("description"));
		if (desc8.Length() == 0)
			desc8.Set(it.Summary());
		HBufC* desc = CnvUtfConverter::ConvertToUnicodeFromUtf8L(desc8);
		CleanupStack::PushL(desc);
		HBufC* home = CnvUtfConverter::ConvertToUnicodeFromUtf8L(j.GetStr(_L8("homepage")));
		CleanupStack::PushL(home);
		HBufC* body = HBufC::NewLC(desc->Length() + home->Length() + 4);
		body->Des().Append(*desc);
		if (home->Length())
			{
			body->Des().Append(_L("\n\n"));
			body->Des().Append(*home);
			}
		TInt state = TPheRow::EFree;
		TBuf8<32> installed;
		iGet->InstalledTagL(it.Id(), installed);
		if (installed.Length())
			state = (installed == it.Tag()) ? TPheRow::EInstalled : TPheRow::EUpdate;
		TPtrC button = _L("Install");
		TBool on = ETrue;
		if (!it.iRelease)
			{
			button.Set(_L("Not available"));
			on = EFalse;
			}
		else if (state == TPheRow::EInstalled)
			{
			button.Set(_L("Installed"));
			on = EFalse;
			}
		else if (state == TPheRow::EUpdate)
			button.Set(_L("Update"));
		iView->ShowDetailsL(aRow, name, meta, *body, button, on);
		CleanupStack::PopAndDestroy(3, desc);
		Cba()->SetCommandSetL(R_PHESTORE_CBA_DETAILS);
		Cba()->DrawDeferred();
		}

	void InstallL()
		{
		if (iBusy || iView->Mode() != CPheCatalogueView::EDetails || !iDetailId)
			return;
		TPheGetItem item;
		if (!iGet->FindL(*iDetailId, item))
			return;
		iLastProgress = -1;
		_LIT(KStarting, "Starting");
		iView->SetDetailsStatus(KStarting, EFalse, 0);
		iView->SetDetailsButton(_L("Installing"), EFalse);
		++iBusy;
		TInt result = KErrGeneral;
		TRAPD(err, result = iGet->InstallL(item, 'c'));
		--iBusy;
		if (err != KErrNone)
			result = err;
		if (iExitPending)
			{
			Exit();
			return;
			}
		TInt row = iView->DetailsRow();
		if (result == KErrNone)
			{
			iView->SetDetailsStatus(_L("Installed"), EFalse, -1);
			iView->SetDetailsButton(_L("Installed"), EFalse);
			iView->UpdateRow(row, TPheRow::EInstalled);
			}
		else
			{
			TBuf<64> msg;
			msg.Format(_L("Install failed (%d)"), result);
			iView->SetDetailsStatus(msg, ETrue, -1);
			iView->SetDetailsButton(_L("Try again"), ETrue);
			}
		}

	void HandleResourceChangeL(TInt aType)
		{
		CAknAppUi::HandleResourceChangeL(aType);
		if (aType == KEikDynamicLayoutVariantSwitch && iView)
			iView->SetRect(ClientRect());
		}

	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheStoreCmdInstall:
			UiAction(0);
			break;
		case EPheStoreCmdRefresh:
			if (!iBusy && iView->Mode() == CPheCatalogueView::EList)
				{
				iPhase = EPhaseRefresh;
				iView->SetLoad(CPheCatalogueView::ELoading, KNullDesC);
				Schedule();
				}
			break;
		case EAknSoftkeyBack:
			if (iView->Mode() == CPheCatalogueView::EDetails)
				UiBack();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			if (iBusy)
				iExitPending = ETrue;              // the nested wait is still running: leave when it returns
			else
				Exit();
			break;
			}
		}

	CPheGet* iGet;
	CPheCatalogueView* iView;
	CPheIconLoader* iLoader;
	CIdle* iWork;
	CPheJob* iRefreshJob;
	RArray<TPheGetItem> iItems;
	TInt iPhase;
	TInt iNextAfterBuild;
	TInt iBuilt;
	TInt iBusy;
	TBool iExitPending;
	TBool iFresh;
	TBool iRebuildPending;
	TInt iKeepOffset;
	TInt iLastProgress;
	HBufC8* iDetailId;
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
