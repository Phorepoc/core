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
#include <eikmenup.h>
#include <avkon.hrh>
#include <eikbtgpc.h>
#include <eikspane.h>
#include <eikenv.h>
#include <utf.h>
#include <aknquerydialog.h>
#include <badesca.h>
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
	// the view's area: the whole screen (no status pane, no softkey bar). ClientRect() still starts below the status
	// pane until the hide takes effect, which left the system status pane showing where our header belongs
	TRect FullRect() const { return ApplicationRect(); }
	void HandleStatusPaneSizeChange()
		{
		CAknAppUi::HandleStatusPaneSizeChange();
		if (iView)
			iView->SetRect(FullRect());
		}
	void ConstructL()
		{
		gStart = NowUs();
		BaseConstructL(EAknEnableSkin);
		StatusPane()->MakeVisible(EFalse);             // our own header: the whole screen is the app's
		iGet = CPheGet::NewL(*this);
		iView = CPheCatalogueView::NewL(FullRect(), *this);
		AddToStackL(iView);
		iView->Window().SetOrdinalPosition(0);         // in front of the (empty) softkey bar: our own toolbar is drawn there, the system bar is never seen
		iLoader = CPheIconLoader::NewL(*iView);
		iRefreshJob = CPheJob::NewL(*this);
		iWork = CIdle::NewL(CActive::EPriorityIdle);
		iAddSource = new (ELeave) CAsyncCallBack(TCallBack(AddSourceCb, this), CActive::EPriorityStandard);
		iSearch = new (ELeave) CAsyncCallBack(TCallBack(SearchCb, this), CActive::EPriorityStandard);
		iPhase = EPhaseCache;
		iView->SetLoad(CPheCatalogueView::ELoading, KNullDesC);
		}
	~CPheStoreAppUi()
		{
		delete iExitTimer;
		delete iAddSource;
		delete iSearch;
		delete iTagFilter;
		iVisible.Close();
		iTagNames.ResetAndDestroy();
		iTagCounts.Close();
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
		iView->DrawDeferred();                         // once more after the first frame: the area the status pane / softkey bar covered is repainted by then
		iWork->Start(TCallBack(WorkCb, this));         // after this frame
		}
	void UiIconWanted() { iLoader->Kick(); }
	void UiRetry()
		{
		iPhase = EPhaseRefresh;
		iView->SetLoad(CPheCatalogueView::ELoading, KNullDesC);
		Schedule();
		}
	void UiOpen(TInt aRow)
		{
		if (iScope == EScopeCategories)                // a category row: show its apps
			{
			if (aRow < 0 || aRow >= iTagNames.Count())
				return;
			HBufC8* tag = iTagNames[aRow]->Alloc();
			if (!tag)
				return;
			delete iTagFilter;
			iTagFilter = tag;
			SetScope(EScopeTag);
			return;
			}
		TRAP_IGNORE(OpenDetailsL(aRow));
		}

	// ---- the toolbar's tabs. Every tab is the same list over a different selection of the items (iScope): Home = all,
	// Categories = the tags the listings carry (a tap on one lists its apps), Search = the items matching the words typed
	// into the Search dialog (name, summary, id, category, tags), My apps = the items that are installed
	enum TScope { EScopeAll, EScopeCategories, EScopeTag, EScopeSearch, EScopeMine };
	static TInt TabOfScope(TInt aScope)
		{
		return aScope == EScopeAll ? 0 : (aScope == EScopeCategories || aScope == EScopeTag) ? 1 : aScope == EScopeSearch ? 2 : 3;
		}
	void UiTab(TInt aTab)
		{
		switch (aTab)
			{
		case 0: SetScope(EScopeAll); break;
		case 1: SetScope(EScopeCategories); break;
		case 2:
			if (!iSearch->IsActive())
				iSearch->CallBack();                   // out of the pointer event: the dialog runs a nested loop
			break;
		default: SetScope(EScopeMine); break;
			}
		}
	void UiTabBack()
		{
		if (iScope == EScopeTag)
			{
			SetScope(EScopeCategories);
			return;
			}
		SetScope(EScopeAll);
		}
	static TInt SearchCb(TAny* aSelf)
		{
		static_cast<CPheStoreAppUi*>(aSelf)->SearchDialog();
		return 0;
		}
	void SearchDialog()
		{
		if (iView->Mode() != CPheCatalogueView::EList)
			return;
		TBuf<64> q(iQuery);
		CAknTextQueryDialog* dlg = NULL;
		TRAPD(err, dlg = CAknTextQueryDialog::NewL(q));
		TBool ok = EFalse;
		if (err == KErrNone)
			ok = dlg->ExecuteLD(R_PHESTORE_SEARCH_QUERY);
		q.Trim();
		if (!ok || q.Length() == 0)
			{
			iView->SelectTab(TabOfScope(iScope));      // cancelled: back to the tab that was showing
			return;
			}
		iQuery = q;
		iQuery8.Zero();
		CnvUtfConverter::ConvertFromUnicodeToUtf8(iQuery8, iQuery);
		RDebug::Printf("PH: [phestore] ui: search");
		SetScope(EScopeSearch);
		}
	void SetPageText()
		{
		TBuf<80> title;
		TPtrC empty(_L("No apps for this device"));
		switch (iScope)
			{
		case EScopeCategories:
			title.Copy(_L("Categories"));
			empty.Set(_L("No categories yet"));
			break;
		case EScopeTag:
			if (iTagFilter)
				TagName(title, *iTagFilter);
			empty.Set(_L("No apps in this category"));
			break;
		case EScopeSearch:
			title.Copy(_L("Search: "));
			title.Append(iQuery.Left(title.MaxLength() - title.Length()));
			empty.Set(_L("No apps match your search"));
			break;
		case EScopeMine:
			title.Copy(_L("My apps"));
			empty.Set(_L("Nothing installed from Catalogue yet"));
			break;
		default:
			title.Copy(_L("Catalogue"));
			break;
			}
		iView->SetPage(title, empty);
		}
	static void TagName(TDes& aOut, const TDesC8& aTag)       // a tag as the list shows it: the first letter capital
		{
		aOut.Zero();
		CnvUtfConverter::ConvertToUnicodeFromUtf8(aOut, aTag.Left(40));
		if (aOut.Length())
			aOut[0] = TChar(aOut[0]).GetUpperCase();
		}
	// show the items of a scope: the rows are rebuilt from the loaded indexes (BeginBuild reads iScope)
	void SetScope(TInt aScope)
		{
		iScope = aScope;
		SetPageText();                                 // (the page first: its title and scroll reset, then the tab)
		iView->SelectTab(TabOfScope(aScope));
		if (iPhase == EPhaseCache || iItems.Count() == 0)
			{
			iView->ClearRows();                        // (the first load builds the list for this scope when it finishes)
			iView->Redraw();
			return;
			}
		TInt next = iPhase == EPhaseBuild ? iNextAfterBuild : (iPhase == EPhaseRefresh ? EPhaseRefresh : EPhaseIdle);
		iKeepOffset = 0;
		BeginBuild();
		iNextAfterBuild = next;
		iPhase = EPhaseBuild;
		iView->Redraw();
		Schedule();
		}
	static TBool HasTag(const TPheGetItem& aItem, const TDesC8& aTag)
		{
		if (aItem.Category().CompareF(aTag) == 0)
			return ETrue;
		const CPheJson* t = aItem.iItem->Get(_L8("tags"));
		if (t && t->Type() == CPheJson::EArray)
			for (TInt i = 0; i < t->Count(); ++i)
				if (t->At(i).Str().CompareF(aTag) == 0)
					return ETrue;
		return EFalse;
		}
	TBool SearchMatch(const TPheGetItem& aItem)       // every word of the query is somewhere in the item
		{
		TLex8 lex(iQuery8);
		for (;;)
			{
			lex.SkipSpace();
			TPtrC8 w = lex.NextToken();
			if (w.Length() == 0)
				return ETrue;
			TBool found = aItem.Name().FindF(w) >= 0 || aItem.Id().FindF(w) >= 0 || aItem.Summary().FindF(w) >= 0
				|| aItem.Category().FindF(w) >= 0;
			const CPheJson* t = aItem.iItem->Get(_L8("tags"));
			if (!found && t && t->Type() == CPheJson::EArray)
				for (TInt i = 0; i < t->Count() && !found; ++i)
					found = t->At(i).Str().FindF(w) >= 0;
			if (!found)
				return EFalse;
			}
		}
	void AddTag(const TDesC8& aTag)                   // the sorted tag list with its counts
		{
		if (aTag.Length() == 0)
			return;
		TInt pos = 0;
		for (; pos < iTagNames.Count(); ++pos)
			{
			TInt c = iTagNames[pos]->CompareF(aTag);
			if (c == 0)
				{
				++iTagCounts[pos];
				return;
				}
			if (c > 0)
				break;
			}
		HBufC8* name = aTag.Alloc();
		if (!name)
			return;
		if (iTagNames.Insert(name, pos) != KErrNone)
			{
			delete name;
			return;
			}
		if (iTagCounts.Insert(1, pos) != KErrNone)
			{
			iTagNames.Remove(pos);
			delete name;
			}
		}
	// which items the current scope shows (iVisible), or for the categories the tags (iTagNames / iTagCounts)
	void ComputeVisible()
		{
		iVisible.Reset();
		iTagNames.ResetAndDestroy();
		iTagCounts.Reset();
		if (iScope == EScopeCategories)
			{
			for (TInt i = 0; i < iItems.Count(); ++i)
				{
				AddTag(iItems[i].Category());
				const CPheJson* t = iItems[i].iItem->Get(_L8("tags"));
				if (t && t->Type() == CPheJson::EArray)
					for (TInt k = 0; k < t->Count(); ++k)
						if (t->At(k).Str().CompareF(iItems[i].Category()) != 0)
							AddTag(t->At(k).Str());
				}
			for (TInt i = 0; i < iTagNames.Count(); ++i)
				iVisible.Append(i);
			return;
			}
		for (TInt i = 0; i < iItems.Count(); ++i)
			{
			TBool in = ETrue;
			switch (iScope)
				{
			case EScopeTag: in = iTagFilter && HasTag(iItems[i], *iTagFilter); break;
			case EScopeSearch: in = SearchMatch(iItems[i]); break;
			case EScopeMine:
				{
				TBuf8<32> installed;
				TRAPD(err, iGet->InstalledTagL(iItems[i].Id(), installed));
				in = err == KErrNone && installed.Length() > 0;
				break;
				}
			default: break;
				}
			if (in)
				iVisible.Append(i);
			}
		}
	void AddTagRowL(TInt aTag)
		{
		TBuf<48> name;
		TagName(name, *iTagNames[aTag]);
		TBuf<32> info;
		info.Format(iTagCounts[aTag] == 1 ? _L("%d app") : _L("%d apps"), iTagCounts[aTag]);
		iView->AddRowL(name, info, KNullDesC, TPheRow::EPlain, KNullDesC8);
		}
	void UiBack()
		{
		if (iView->Mode() == CPheCatalogueView::ESources)
			{
			CloseSources();
			return;
			}
		iView->ShowList();
		TRAP_IGNORE(Cba()->SetCommandSetL(R_AVKON_SOFTKEYS_OPTIONS_EXIT));   // (hidden behind the view: the framework's exit sequence wants a real softkey set)
		if (iRebuildPending)
			{
			iRebuildPending = EFalse;
			iMineDirty = EFalse;
			Rebuild();
			Schedule();
			}
		else if (iMineDirty)                               // something was installed: My apps has one more
			{
			iMineDirty = EFalse;
			if (iScope == EScopeMine)
				SetScope(EScopeMine);
			}
		}
	void UiAction(TInt /*aRow*/) { TRAP_IGNORE(InstallL()); }
	void UiExit()
		{
		// leave from a clean scheduler turn, a moment after the pointer event
		if (!iExitTimer)
			{
			TRAPD(err, iExitTimer = CPeriodic::NewL(CActive::EPriorityStandard));
			if (err != KErrNone)
				return;
			}
		if (!iExitTimer->IsActive())
			iExitTimer->Start(300000, 300000, TCallBack(ExitCb, this));
		}
	static TInt ExitCb(TAny* aSelf)
		{
		CPheStoreAppUi* self = static_cast<CPheStoreAppUi*>(aSelf);
		self->iExitTimer->Cancel();
		RDebug::Printf("PH: [phestore] exit (toolbar)");
		// the framework's own answer to a user exit command (Options > Exit) is to hide the app in the background
		self->HideInBackground();
		return 0;
		}
	void UiOptions() { RDebug::Printf("PH: [phestore] ui: options"); TRAP_IGNORE(ProcessCommandL(EAknSoftkeyOptions)); }

	// ---- software sources: the Sources page over the engine's indexes.txt (pheget.h); changes take effect on leaving it
	void UiSourceToggleDefault()
		{
		TBool on = EFalse;
		TRAPD(err, on = iGet->DefaultIndexEnabledL());
		if (err == KErrNone)
			TRAP(err, iGet->SetDefaultIndexL(!on));
		if (err == KErrNotSupported)
			{
			ShowSourcesStatus(_L("Add another source before switching this one off"), ETrue);
			return;
			}
		SourcesChanged(err, on ? _L("Phorepoc index switched off") : _L("Phorepoc index switched on"));
		}
	void UiSourceRemove(TInt aSource)
		{
		TInt err = KErrNone;
		TRAP(err, iGet->RemoveSourceL(aSource));
		SourcesChanged(err, _L("Source removed"));
		}
	void UiSourceAdd()
		{
		if (!iAddSource->IsActive())
			iAddSource->CallBack();                        // out of the pointer event: the dialog runs a nested loop
		}
	static TInt AddSourceCb(TAny* aSelf)
		{
		static_cast<CPheStoreAppUi*>(aSelf)->AddSourceDialog();
		return 0;
		}
	void AddSourceDialog()
		{
		if (iView->Mode() != CPheCatalogueView::ESources)
			return;
		TBuf<256> address(_L("https://"));
		CAknTextQueryDialog* dlg = NULL;
		TRAPD(err, dlg = CAknTextQueryDialog::NewL(address));
		if (err != KErrNone)
			return;
		RDebug::Printf("PH: [phestore] ui: add-source dialog");
		if (!dlg->ExecuteLD(R_PHESTORE_ADD_SOURCE_QUERY))
			return;                                        // cancelled
		address.Trim();
		if (address.Length() == 0 || address.CompareF(_L("https://")) == 0)
			return;
		TRAP(err, iGet->AddSourceL(address));
		if (err == KErrArgument)
			{
			ShowSourcesStatus(_L("Not an index address: use http(s)://host/... or a path like c:\\data\\index.json"), ETrue);
			return;
			}
		if (err == KErrAlreadyExists)
			{
			ShowSourcesStatus(_L("That source is already there"), ETrue);
			return;
			}
		SourcesChanged(err, _L("Source added"));
		}
	void ShowSourcesStatus(const TDesC& aText, TBool aError)
		{
		iView->SetSourcesStatus(aText, aError);
		}
	// after a change to indexes.txt (err = its result): rebuild the rows, say what happened
	void SourcesChanged(TInt aErr, const TDesC& aOk)
		{
		TBuf8<64> ok8;
		ok8.Copy(aOk.Left(64));
		RDebug::Printf("PH: [phestore] ui: sources changed (%d): %S", aErr, &ok8);
		if (aErr == KErrNone)
			iSourcesChanged = ETrue;
		FillSources();
		if (aErr == KErrNone)
			ShowSourcesStatus(aOk, EFalse);
		else
			{
			TBuf<48> msg;
			msg.Format(_L("Could not save the sources (%d)"), aErr);
			ShowSourcesStatus(msg, ETrue);
			}
		}
	void OpenSources()
		{
		if (iBusy || iPhase == EPhaseBuild || iPhase == EPhaseCache || iView->Mode() != CPheCatalogueView::EList || iView->Tab() != 0)
			return;
		iSourcesChanged = EFalse;
		FillSources();
		iView->SetSourcesStatus(KNullDesC, EFalse);
		TRAP_IGNORE(Cba()->SetCommandSetL(R_AVKON_SOFTKEYS_OPTIONS_BACK));   // the right softkey leaves this page, not the app
		iView->ShowSources();
		RDebug::Printf("PH: [phestore] ui: sources page open");
		}
	void FillSources()
		{
		iView->ClearSources();
		CDesCArrayFlat* sources = new CDesCArrayFlat(4);
		if (!sources)
			return;
		TBool def = ETrue;
		TRAP_IGNORE(iGet->SourcesL(*sources));
		TRAP_IGNORE(def = iGet->DefaultIndexEnabledL());
		TBool other = sources->Count() > 0;
		_LIT(KTitle, "Phorepoc index");
		TBuf<160> info;
		if (!other)
			info.Copy(_L("Add another source to switch it off"));
		else if (!def)
			info.Copy(_L("Switched off"));
		else
			info.Copy(KPheGetDefaultIndex);
		TRAP_IGNORE(iView->AddSourceRowL(TPheSrc::EDefault, KTitle, info, def, other));
		for (TInt i = 0; i < sources->Count(); ++i)
			TRAP_IGNORE(iView->AddSourceRowL(TPheSrc::EUser, (*sources)[i], (*sources)[i].Left(4).CompareF(_L("http")) == 0 ? _L("Web index") : _L("File on this device"), ETrue, ETrue));
		TRAP_IGNORE(iView->AddSourceRowL(TPheSrc::EAdd, _L("Add source"), _L("An index address: http(s) URL or a file path"), EFalse, ETrue));
		delete sources;
		iView->Redraw();
		}
	// back from the Sources page: if it changed anything, show the indexes as they now are and refresh them
	void CloseSources()
		{
		RDebug::Printf("PH: [phestore] ui: sources closed (changed %d)", iSourcesChanged ? 1 : 0);
		TRAP_IGNORE(Cba()->SetCommandSetL(R_AVKON_SOFTKEYS_OPTIONS_EXIT));   // (Options / Exit again)
		iView->ShowList();
		if (iRebuildPending)                               // an index changed while this page was open
			{
			iRebuildPending = EFalse;
			iSourcesChanged = ETrue;
			}
		if (!iSourcesChanged)
			return;
		iSourcesChanged = EFalse;
		Rebuild();                                         // from the caches (iPhase = build on success)
		if (iPhase == EPhaseBuild)
			iNextAfterBuild = EPhaseRefresh;
		else
			{
			iView->ClearRows();                            // nothing cached for the sources now in use: the spinner until the network answers
			iPhase = EPhaseRefresh;
			iView->SetLoad(CPheCatalogueView::ELoading, KNullDesC);
			}
		Schedule();
		}

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
			{
			iRefreshAgain = ETrue;                     // the running one may not know of a source added meanwhile: once more when it ends
			return;
			}
		TInt r = iRefreshJob->Start(1, CPheJob::ERefresh, KNullDesC, KNullDesC);
		if (r != KErrNone)
			JobDone(1, r);
		}

	// MPheJobObserver
	void JobDone(TInt /*aId*/, TInt aResult)
		{
		JobResult(aResult);
		if (iRefreshAgain)
			{
			iRefreshAgain = EFalse;
			if (iPhase == EPhaseBuild)
				iNextAfterBuild = EPhaseRefresh;       // (the rows are being rebuilt: refresh after)
			else
				iPhase = EPhaseRefresh;
			Schedule();
			}
		}
	void JobResult(TInt aResult)
		{
		TInt err = aResult < 0 ? aResult : KErrNone;
		TBool changed = aResult == 1;
		RDebug::Printf("PH: [phestore] TIMING refresh %s (%d) at %d ms", err == KErrNone ? (changed ? "changed" : "unchanged") : "failed", err, Since());
		if (err == KErrNone)
			iLoader->NetworkChanged();
		if (err == KErrNone && (changed || iItems.Count() == 0))
			{
			iFresh = ETrue;
			if (iView->Mode() != CPheCatalogueView::EList)
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
		ComputeVisible();
		iView->ClearRows();
		iBuilt = 0;
		}

	// a few rows per slice, so a touch is never starved
	TBool BuildSlice()
		{
		TInt64 start = NowUs();
		while (iBuilt < iVisible.Count())
			{
			TInt err = KErrNone;
			if (iScope == EScopeCategories)
				{
				TRAP(err, AddTagRowL(iVisible[iBuilt]));
				}
			else
				{
				TRAP(err, AddRowL(iItems[iVisible[iBuilt]]));
				}
			if (err != KErrNone)
				{
				iBuilt = iVisible.Count();
				break;
				}
			++iBuilt;
			if (NowUs() - start > 8000)
				break;
			}
		if (iBuilt == 1 || iBuilt >= iVisible.Count() || iBuilt % 8 == 0)
			iView->Redraw();
		if (iBuilt >= iVisible.Count())
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
		if (aRow < 0 || aRow >= iVisible.Count() || iBusy)
			return;
		const TPheGetItem& it = iItems[iVisible[aRow]];
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
			iMineDirty = ETrue;
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
			iView->SetRect(FullRect());
		}

	void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aMenuPane)
		{
		CAknAppUi::DynInitMenuPaneL(aResourceId, aMenuPane);
		RDebug::Printf("PH: [phestore] ui: options");
		}

	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheStoreCmdInstall:
			UiAction(0);
			break;
		case EPheStoreCmdSources:
			OpenSources();
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
			if (iView->Mode() != CPheCatalogueView::EList)
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
	CPeriodic* iExitTimer;
	CPheJob* iRefreshJob;
	RArray<TPheGetItem> iItems;
	TInt iPhase;
	TInt iNextAfterBuild;
	TInt iBuilt;
	TInt iBusy;
	TBool iExitPending;
	TBool iFresh;
	TBool iRebuildPending;
	TBool iRefreshAgain;
	TBool iSourcesChanged;
	CAsyncCallBack* iAddSource;
	CAsyncCallBack* iSearch;
	TInt iScope;                                       // which items the list shows (TScope)
	RArray<TInt> iVisible;                             // the items of the scope: indexes into iItems (the categories: into iTagNames)
	RPointerArray<HBufC8> iTagNames;                   // the categories page: every tag / category of the items, sorted
	RArray<TInt> iTagCounts;
	HBufC8* iTagFilter;                                // the category whose apps are listed
	TBuf<64> iQuery;                                   // the last search
	TBuf8<192> iQuery8;
	TBool iMineDirty;                                  // an install changed what My apps lists
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
