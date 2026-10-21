// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phebrowser.cpp -- OURS: the browser frontend (prototype). A custom-drawn CONE UI -- one window, flat bars drawn with
// CWindowGc, no Qt -- over any engine that implements the PheWebEngineApi table (the phewebengine repository). It has:
//   * the bar (address row + action row) at the BOTTOM (default) or TOP, switched live and kept in settings.ini;
//   * tabs (normal and PRIVATE), a tab grid with snapshots, an overflow sheet, a new-tab page;
//   * light / dark / follow-system themes (pb_theme.h holds every colour);
//   * first-launch search-engine choice (no preselected default) and address -> URL or search;
//   * the built-in dummy engine (pb_dummy.cpp), so it runs without WebKit or NetSurf.
// Not here yet (docs/phorepoc/browser-frontend.md section 7): bookmarks/history/downloads screens, the find bar, ECom
// discovery of downloadable engines, bar auto-hide, pinch zoom, dialogs from the engine, CenRep.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknquerydialog.h>
#include <aknnotewrappers.h>
#include <aknsutils.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikmenup.h>
#include <eikmenub.h>
#include <eikappui.h>
#include <avkon.hrh>
#include <apgcli.h>
#include <apacmdln.h>
#include <f32file.h>
#include <fbs.h>
#include <gdi.h>
#include <utf.h>
#include <phebrowser.rsg>
#include "phebrowser.hrh"
#include "pb_layout.h"
#include "pb_theme.h"
#include "pb_engines.h"

struct TPbEarly { TPbEarly() { RDebug::Printf("PH: [phebrowser] static init"); } };
static TPbEarly gPbEarly;
const TUid KPheBrowserAppUid = { 0xE7016B20 };
const TUid KPheStoreUid = { 0xE70157FA };

static const TInt KMaxTabs = 8;
static const TUint16 KFindDemoText[] = { 'x' };
static const TInt KThumbW = 160, KThumbH = 240;

static TRgb Rgb(unsigned aC) { return TRgb((aC >> 16) & 0xFF, (aC >> 8) & 0xFF, aC & 0xFF); }

// ---- the search engines: chosen on first launch, none preselected; %s = the query ---------------------------------------
struct TPbSearch { const char* iName; const char* iTemplate; };
static const TPbSearch KSearches[] =
	{
	{ "Google", "https://www.google.com/search?q=%s" },
	{ "DuckDuckGo", "https://duckduckgo.com/?q=%s" },
	{ "Bing", "https://www.bing.com/search?q=%s" },
	{ "Startpage", "https://www.startpage.com/do/search?q=%s" },
	{ "Brave Search", "https://search.brave.com/search?q=%s" },
	{ "Ecosia", "https://www.ecosia.org/search?q=%s" },
	{ "Qwant", "https://www.qwant.com/?q=%s" },
	{ "Yahoo", "https://search.yahoo.com/search?p=%s" }
	};
static const TInt KSearchCount = sizeof(KSearches) / sizeof(KSearches[0]);   // + "Custom..." as the last row

static TPtrC8 P8(const char* aS) { return TPtrC8((const TUint8*)aS, User::StringLength((const TUint8*)aS)); }

// ---- downloads go to E:\Downloads, else C:\data\Downloads --------------------------------------------------------------------
static void DownloadDir(RFs& aFs, TDes& aOut)
	{
	TDriveInfo di;
	aOut = (aFs.Drive(di, EDriveE) == KErrNone && di.iType != EMediaNotPresent) ? _L("E:\\Downloads\\") : _L("C:\\data\\Downloads\\");
	aFs.MkDirAll(aOut);
	}

// ---- settings (a tiny key=value file; CenRep later: this class is the only thing to swap) --------------------------------------
class CPbSettings
	{
public:
	TPbBarPos iBar;
	TInt iTheme;                 // TPbThemeSetting
	TBuf8<200> iSearchTemplate;  // empty = not chosen yet (first launch shows the chooser)
	TBuf8<40> iSearchName;
	CPbSettings() : iBar(EPbBarBottom), iTheme(EPbThemeFollow) {}
	void Load(RFs& aFs)
		{
		TFileName f;
		Path(aFs, f);
		RFile file;
		if (file.Open(aFs, f, EFileRead | EFileShareReadersOnly) != KErrNone)
			return;
		TBuf8<512> b;
		file.Read(b);
		file.Close();
		TBuf8<16> v;
		if (Value(b, _L8("barpos"), v)) iBar = v == _L8("top") ? EPbBarTop : EPbBarBottom;
		if (Value(b, _L8("theme"), v)) iTheme = v == _L8("light") ? EPbThemeLight : (v == _L8("dark") ? EPbThemeDark : EPbThemeFollow);
		Value(b, _L8("search"), iSearchTemplate);
		Value(b, _L8("searchname"), iSearchName);
		}
	void Save(RFs& aFs) const
		{
		TFileName f;
		aFs.CreatePrivatePath(EDriveC);
		Path(aFs, f);
		RFile file;
		if (file.Replace(aFs, f, EFileWrite) != KErrNone)
			return;
		TBuf8<512> b;
		b.Append(_L8("barpos=")); b.Append(iBar == EPbBarTop ? _L8("top") : _L8("bottom")); b.Append('\n');
		b.Append(_L8("theme=")); b.Append(iTheme == EPbThemeLight ? _L8("light") : (iTheme == EPbThemeDark ? _L8("dark") : _L8("follow"))); b.Append('\n');
		b.Append(_L8("search=")); b.Append(iSearchTemplate); b.Append('\n');
		b.Append(_L8("searchname=")); b.Append(iSearchName); b.Append('\n');
		file.Write(b);
		file.Close();
		}
private:
	static void Path(RFs& aFs, TDes& aOut)
		{
		aFs.PrivatePath(aOut);
		aOut.Insert(0, _L("C:"));
		aOut.Append(_L("settings.ini"));
		}
	static TBool Value(const TDesC8& aAll, const TDesC8& aKey, TDes8& aOut)
		{
		TBuf8<24> k(aKey);
		k.Append('=');
		TInt p = aAll.Find(k);
		if (p == KErrNotFound || (p > 0 && aAll[p - 1] != '\n'))
			return EFalse;
		TPtrC8 rest = aAll.Mid(p + k.Length());
		TInt e = rest.Locate('\n');
		if (e >= 0)
			rest.Set(rest.Left(e));
		aOut.Copy(rest.Left(Min(rest.Length(), aOut.MaxLength())));
		return ETrue;
		}
	};

// ---- the tab model (no UI types except the thumbnail) ----------------------------------------------------------------------------
struct TPbTab
	{
	PheView iView;
	TBuf<200> iUrl;            // (RArray entries must stay under 640 bytes: USER 129) empty = the new-tab page (the engine is not asked to load anything)
	TBuf<48> iTitle;
	TInt iProgress;
	TInt iSecurity;
	TBool iBack, iFwd, iLoading;
	TBool iPrivate;
	CFbsBitmap* iThumb;        // KThumbW x KThumbH snapshot for the tab grid
	};

enum TPbMode { EModeBrowse, EModeGrid, EModeSheet, EModeSearchPick };
enum TPbSheetItem
	{
	ESheetNewTab, ESheetNewPrivate, ESheetFind, ESheetBar, ESheetTheme, ESheetSearch, ESheetStore, ESheetExit, ESheetCount
	};

// ---- the container: the whole UI is this one control ------------------------------------------------------------------------------
class CPbContainer : public CCoeControl
	{
public:
	~CPbContainer()
		{
		if (iApi)
			{
			for (TInt i = 0; i < iTabs.Count(); ++i)
				iApi->view_destroy(iTabs[i].iView);
			iApi->shutdown();
			}
		for (TInt i = 0; i < iTabs.Count(); ++i)
			delete iTabs[i].iThumb;
		delete iCallBack;
		delete iPageBmp;
		iTabs.Close();
		}

	void ConstructL(const TRect& aRect, CPbSettings* aSettings)
		{
		iSet = aSettings;
		CreateWindowL();
		RDebug::Printf("PH: [phebrowser] c1 window");
		EnableDragEvents();
		RDebug::Printf("PH: [phebrowser] c2 drag");
		iCallBack = new (ELeave) CAsyncCallBack(TCallBack(Pump, this), CActive::EPriorityStandard);
		RDebug::Printf("PH: [phebrowser] c3 callback");
		SetRect(aRect);
		RDebug::Printf("PH: [phebrowser] c4 setrect");
		ActivateL();
		RDebug::Printf("PH: [phebrowser] c5 activate");
#ifdef PHEBROWSER_DUMMY
		iApi = PheDummyGetApi(PHE_WEBENGINE_VERSION);
#endif
		RDebug::Printf("PH: [phebrowser] c6 api=%x", (unsigned)iApi);
		if (iApi && iApi->init(Notify, this) == 0)
			{ RDebug::Printf("PH: [phebrowser] c7 init ok");
			NewTabL(KNullDesC, EFalse);       // opens on the new-tab page
			RDebug::Printf("PH: [phebrowser] c8 newtab ok"); }
		else
			iApi = NULL;                      // no engine: the new-tab page offers the store
		if (iSet->iSearchTemplate.Length() == 0)
			iMode = EModeSearchPick;          // first launch: the user chooses the search engine
		}

	// ---- commands ----
	void NewTabL(const TDesC& aUrl, TBool aPrivate)
		{
		if (!iApi || iTabs.Count() >= KMaxTabs)
			return;
		if (aPrivate && !(iApi->caps & PHE_CAP_PRIVATE))
			{
			CAknInformationNote* n = new (ELeave) CAknInformationNote(ETrue);
			n->ExecuteLD(_L("This engine has no private tabs"));
			return;
			}
		if (iCur >= 0)
			{
			CaptureThumb(iCur);
			iApi->view_visible(iTabs[iCur].iView, 0);
			}
		TPbTab t;
		t.iView = iApi->new_view_id();
		t.iUrl.Copy(aUrl.Left(Min(aUrl.Length(), 200)));
		t.iTitle.Zero();
		t.iProgress = 0; t.iSecurity = 0; t.iBack = t.iFwd = EFalse; t.iLoading = aUrl.Length() > 0;
		t.iPrivate = aPrivate;
		t.iThumb = NULL;
		RDebug::Printf("PH: [phebrowser] n1");
		iTabs.AppendL(t);
		RDebug::Printf("PH: [phebrowser] n2 appended");
		iCur = iTabs.Count() - 1;
		const TPbLayout l = Layout();
		iApi->view_create(t.iView, l.iPage.iW, l.iPage.iH, aPrivate, NULL, 0);   // a private view persists nothing (seam)
		RDebug::Printf("PH: [phebrowser] n3 view_create %dx%d", l.iPage.iW, l.iPage.iH);
		if (aUrl.Length())
			iApi->load(t.iView, aUrl.Ptr(), aUrl.Length());
		iMode = EModeBrowse;
		PageChanged();
		}
	void CloseTab(TInt aIx = -1)
		{
		if (aIx < 0) aIx = iCur;
		if (!iApi || aIx < 0 || aIx >= iTabs.Count())
			return;
		iApi->view_destroy(iTabs[aIx].iView);
		delete iTabs[aIx].iThumb;
		iTabs.Remove(aIx);
		if (iCur > aIx || iCur >= iTabs.Count())
			iCur = iTabs.Count() - 1;
		if (iTabs.Count() == 0)
			{
			iCur = -1;
			TRAP_IGNORE(NewTabL(KNullDesC, EFalse));
			}
		else
			iApi->view_visible(iTabs[iCur].iView, 1);
		PageChanged();
		}
	void SelectTab(TInt aIx)
		{
		if (aIx == iCur || aIx < 0 || aIx >= iTabs.Count())
			{ iMode = EModeBrowse; PageChanged(); return; }
		iApi->view_visible(iTabs[iCur].iView, 0);
		iCur = aIx;
		iApi->view_visible(iTabs[iCur].iView, 1);
		iMode = EModeBrowse;
		PageChanged();
		}
	void NextTab() { if (iTabs.Count() > 1) SelectTab((iCur + 1) % iTabs.Count()); }
	void OpenSheet() { iMode = EModeSheet; DrawDeferred(); }
	void SetBar(TPbBarPos aPos)
		{
		iSet->iBar = aPos;
		iSet->Save(iEikonEnv->FsSession());
		Relayout();
		}
	void ToggleBar() { SetBar(iSet->iBar == EPbBarTop ? EPbBarBottom : EPbBarTop); }
	TBool GoBackIfPossible()
		{
		if (iMode != EModeBrowse)
			{ iMode = EModeBrowse; DrawDeferred(); return ETrue; }
		if (iCur >= 0 && iTabs[iCur].iBack)
			{
			iApi->cmd(iTabs[iCur].iView, PHE_CMD_BACK);
			return ETrue;
			}
		return EFalse;
		}
	void FindDemo() { if (iCur >= 0) iApi->find(iTabs[iCur].iView, KFindDemoText, 1, 0); }
	void AskUrlL()
		{
		if (iCur < 0)
			return;
		if (iSet->iSearchTemplate.Length() == 0)
			{ iMode = EModeSearchPick; DrawDeferred(); return; }
		TBuf<256> u(iTabs[iCur].iUrl);
		CAknTextQueryDialog* d = CAknTextQueryDialog::NewL(u);
		if (d->ExecuteLD(R_PHEBROWSER_URL_QUERY))
			Navigate(u);
		}
	TBool BarAtBottom() const { return iSet->iBar == EPbBarBottom; }

private:
	enum { EBtnBack, EBtnFwd, EBtnReload, EBtnTabs, EBtnMenu, EBtnCount };

	// ---- theme ----
	TBool SystemDark() const
		{
		TRgb c;
		if (AknsUtils::GetCachedColor(AknsUtils::SkinInstance(), c, KAknsIIDQsnTextColors, EAknsCIQsnTextColorsCG6) == KErrNone)
			return (c.Red() * 3 + c.Green() * 6 + c.Blue()) / 10 > 128;    // light text on the skin = a dark skin
		return EFalse;
		}
	TBool Dark() const { return iSet->iTheme == EPbThemeDark || (iSet->iTheme == EPbThemeFollow && SystemDark()); }
	TPbTokens Tk() const
		{
		return PbTokens(Dark(), iMode == EModeBrowse && iCur >= 0 && iTabs[iCur].iPrivate);
		}
	void HandleResourceChange(TInt aType)
		{
		CCoeControl::HandleResourceChange(aType);
		if (aType == KAknsMessageSkinChange)
			DrawDeferred();
		}

	// ---- layout ----
	TPbLayout Layout() const
		{
		const TSize s = Size();
		return PbLayout(s.iWidth, s.iHeight, iSet->iBar, s.iWidth > s.iHeight ? 48 : 56, 1);
		}
	static TRect R(const TPbRect& r) { return TRect(r.iX, r.iY, r.iX + r.iW, r.iY + r.iH); }
	TRect ButtonRect(TInt aIx, const TPbLayout& l) const
		{
		if (l.iActions.iW > 0)
			{
			const TInt w = l.iActions.iW / EBtnCount;
			return TRect(l.iActions.iX + aIx * w, l.iActions.iY, l.iActions.iX + (aIx + 1) * w, l.iActions.iY + l.iActions.iH);
			}
		const TInt h = l.iAddress.iH;
		if (aIx <= EBtnReload)
			return TRect(aIx * h, l.iAddress.iY, (aIx + 1) * h, l.iAddress.iY + h);
		const TInt fromRight = EBtnCount - aIx;
		return TRect(l.iAddress.iW - fromRight * h, l.iAddress.iY, l.iAddress.iW - (fromRight - 1) * h, l.iAddress.iY + h);
		}
	TRect FieldRect(const TPbLayout& l) const
		{
		TRect a = R(l.iAddress);
		if (l.iActions.iW == 0)
			{
			a.iTl.iX += 3 * a.Height();
			a.iBr.iX -= 2 * a.Height();
			}
		a.Shrink(8, 8);
		return a;
		}
	void SizeChanged() { Relayout(); }
	void Relayout()
		{
		if (!iApi)
			return;
		const TPbLayout l = Layout();
		for (TInt i = 0; i < iTabs.Count(); ++i)
			iApi->view_resize(iTabs[i].iView, l.iPage.iW, l.iPage.iH);
		delete iPageBmp; iPageBmp = NULL;
		PageChanged();
		}
	void PageChanged() { RDebug::Printf("PH: [phebrowser] p1"); CopyPixels(); RDebug::Printf("PH: [phebrowser] p2"); DrawDeferred(); RDebug::Printf("PH: [phebrowser] p3"); }

	// ---- address -> URL or search ----
	void Navigate(const TDesC& aInput)
		{
		TBuf<256> u(aInput);
		u.Trim();
		if (u.Length() == 0)
			return;
		if (u.Find(_L("://")) == KErrNotFound && u.Left(6).CompareF(_L("about:")) != 0)
			{
			if (u.Locate(' ') == KErrNotFound && u.Locate('.') != KErrNotFound)
				u.Insert(0, _L("https://"));                         // "example.org"
			else
				SearchUrl(u);                                         // words: a search with the chosen engine
			}
		iTabs[iCur].iUrl.Copy(u.Left(Min(u.Length(), 200)));
		iTabs[iCur].iLoading = ETrue;
		iApi->load(iTabs[iCur].iView, u.Ptr(), u.Length());
		}
	void SearchUrl(TDes& aQuery)
		{
		TBuf8<400> enc;
		HBufC8* utf8 = NULL;
		TRAPD(err, utf8 = CnvUtfConverter::ConvertFromUnicodeToUtf8L(aQuery));
		for (TInt i = 0; err == KErrNone && i < utf8->Length() && enc.Length() < 380; ++i)
			{
			const TUint8 ch = (*utf8)[i];
			if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_' || ch == '.')
				enc.Append(ch);
			else if (ch == ' ')
				enc.Append('+');
			else
				enc.AppendFormat(_L8("%%%02X"), ch);
			}
		delete utf8;
		TBuf8<600> full(iSet->iSearchTemplate);
		TInt p = full.Find(_L8("%s"));
		if (p != KErrNotFound)
			full.Replace(p, 2, enc);
		aQuery.Copy(full.Left(Min(full.Length(), aQuery.MaxLength())));
		}

	// ---- engine -> UI plumbing ----
	static void Notify(void* aCtx, PheView) { static_cast<CPbContainer*>(aCtx)->iCallBack->CallBack(); }
	static TInt Pump(TAny* aSelf) { static_cast<CPbContainer*>(aSelf)->DrainEvents(); return 0; }
	TInt IndexOf(PheView aV) const
		{
		for (TInt i = 0; i < iTabs.Count(); ++i)
			if (iTabs[i].iView == aV)
				return i;
		return KErrNotFound;
		}
	void FetchStr(PheView aV, TInt aWhich, TDes& aOut)
		{
		TUint16 tmp[256];
		const TInt n = iApi->get_str(aV, aWhich, tmp, 256);
		aOut.Copy(tmp, Min(Min(n, 256), aOut.MaxLength()));
		}
	void DrainEvents()
		{
		PheEvent e;
		TBool bar = EFalse, page = EFalse;
		while (iApi->poll(0, &e))
			{
			const TInt t = IndexOf(e.view);
			if (t < 0)
				continue;
			PheViewState st;
			iApi->get_state(e.view, &st);
			TPbTab& tab = iTabs[t];
			tab.iBack = st.can_back; tab.iFwd = st.can_forward; tab.iLoading = st.loading; tab.iProgress = st.progress;
			switch (e.kind)
				{
			case PHE_EV_COMMIT:
				tab.iSecurity = e.b;
				FetchStr(e.view, PHE_STR_URL, tab.iUrl);
				break;
			case PHE_EV_TITLE:
				FetchStr(e.view, PHE_STR_TITLE, tab.iTitle);
				break;
			case PHE_EV_REPAINT:
				if (t == iCur) page = ETrue;
				break;
			case PHE_EV_NEW_VIEW:
				{
				TBuf<256> u;
				FetchStr(e.view, PHE_STR_NEWVIEW, u);
				TRAP_IGNORE(NewTabL(u, tab.iPrivate));
				}
				return;
			case PHE_EV_DOWNLOAD:                    // accept into E:\Downloads (else C:\data\Downloads)
				{
				TFileName path;
				DownloadDir(iEikonEnv->FsSession(), path);
				TBuf<128> name;
				FetchStr(e.view, PHE_STR_DL_NAME, name);
				path.Append(name);
				iApi->download_accept(e.view, e.id, path.Ptr(), path.Length());
				}
				break;
			default:
				break;
				}
			if (t == iCur) bar = ETrue;
			}
		if (page) CopyPixels();
		if (page || bar) DrawDeferred();
		}

	// the engine buffer -> a CFbsBitmap (one copy; a zero-copy chunk-backed bitmap is the next step)
	void CopyPixels()
		{
		if (!iApi || iCur < 0)
			return;
		const TPbLayout l = Layout();
		const TSize ps(l.iPage.iW, l.iPage.iH);
		if (ps.iWidth <= 0 || ps.iHeight <= 0)
			return;
		if (!iPageBmp)
			{
			iPageBmp = new CFbsBitmap;
			if (!iPageBmp || iPageBmp->Create(ps, EColor16MU) != KErrNone)
				{ delete iPageBmp; iPageBmp = NULL; return; }
			}
		const unsigned int* px; int w, h, stride;
		if (iApi->lock_pixels(iTabs[iCur].iView, &px, &w, &h, &stride) != 0)
			return;
		iPageBmp->LockHeap();
		const TInt rows = Min(h, ps.iHeight), cols = Min(w, ps.iWidth);
		for (TInt y = 0; y < rows; ++y)
			Mem::Copy((TUint8*)iPageBmp->DataAddress() + y * iPageBmp->DataStride(), (const TUint8*)px + y * stride, cols * 4);
		iPageBmp->UnlockHeap();
		iApi->unlock_pixels(iTabs[iCur].iView);
		}
	// a tab-grid snapshot straight into the thumbnail bitmap (the engine scales: PheWebEngineApi::snapshot)
	void CaptureThumb(TInt aIx)
		{
		TPbTab& t = iTabs[aIx];
		if (!iApi || !iApi->snapshot || t.iUrl.Length() == 0)
			return;
		if (!t.iThumb)
			{
			t.iThumb = new CFbsBitmap;
			if (!t.iThumb || t.iThumb->Create(TSize(KThumbW, KThumbH), EColor16MU) != KErrNone)
				{ delete t.iThumb; t.iThumb = NULL; return; }
			}
		t.iThumb->LockHeap();
		iApi->snapshot(t.iView, (unsigned int*)t.iThumb->DataAddress(), KThumbW, KThumbH, t.iThumb->DataStride());
		t.iThumb->UnlockHeap();
		}

	// ---- drawing helpers ----
	static void Fill(CWindowGc& gc, const TRect& r, TRgb c)
		{
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(c);
		gc.DrawRect(r);
		}
	void Txt(CWindowGc& gc, const TDesC& s, const TRect& r, CGraphicsContext::TTextAlign a, TRgb c, TInt aMargin = 0) const
		{
		const CFont* f = iEikonEnv->NormalFont();
		gc.UseFont(f);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(c);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		gc.DrawText(s, r, (r.Height() + f->AscentInPixels() - f->DescentInPixels()) / 2, a, aMargin);
		gc.DiscardFont();
		}
	void Button(CWindowGc& gc, const TRect& r, const TDesC& s, TRgb bg, TRgb fg) const
		{
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(bg);
		gc.DrawRoundRect(r, TSize(r.Height() / 2, r.Height() / 2));
		Txt(gc, s, r, CGraphicsContext::ECenter, fg);
		}

	// ---- drawing ----
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		const TPbTokens tk = Tk();
		const TPbLayout l = Layout();
		const TPbTab* t = iCur >= 0 ? &iTabs[iCur] : NULL;
		// page area
		Fill(gc, R(l.iPage), Rgb(tk.iPage));
		if (t && t->iUrl.Length() && iPageBmp)
			gc.BitBlt(TPoint(l.iPage.iX, l.iPage.iY), iPageBmp);
		else
			DrawNewTab(gc, R(l.iPage), tk);
		// bars
		Fill(gc, R(l.iAddress), Rgb(tk.iBar));
		if (l.iActions.iW > 0)
			Fill(gc, R(l.iActions), Rgb(tk.iBar));
		DrawField(gc, l, tk);
		for (TInt b = 0; b < EBtnCount; ++b)
			DrawGlyph(gc, b, ButtonRect(b, l), tk);
		if (t && t->iLoading && t->iUrl.Length())
			Fill(gc, TRect(l.iProgress.iX, l.iProgress.iY, l.iProgress.iX + l.iProgress.iW * Max(t->iProgress, 4) / 100,
				l.iProgress.iY + l.iProgress.iH), Rgb(tk.iAccent));
		if (iMode == EModeGrid) DrawGrid(gc);
		if (iMode == EModeSheet) DrawSheet(gc);
		if (iMode == EModeSearchPick) DrawSearchPick(gc);
		}

	// the new-tab page: loosely coupled hand-off -- if no engine is installed it offers "Get a browser engine" (the store)
	TRect StoreButtonRect(const TRect& aR) const { return TRect(aR.iTl.iX + 24, aR.Center().iY + 20, aR.iBr.iX - 24, aR.Center().iY + 68); }
	void DrawNewTab(CWindowGc& gc, const TRect& aR, const TPbTokens& tk) const
		{
		const TBool priv = iCur >= 0 && iTabs[iCur].iPrivate;
		TRect a = aR;
		a.iBr.iY = aR.iTl.iY + aR.Height() / 2 - 28;
		Txt(gc, priv ? _L("Private tab") : _L("New tab"), a, CGraphicsContext::ECenter, Rgb(tk.iInk));
		TRect b = aR;
		b.iTl.iY = a.iBr.iY;
		b.iBr.iY = b.iTl.iY + 28;
		if (!iApi)
			{
			Txt(gc, _L("No browser engine is installed"), b, CGraphicsContext::ECenter, Rgb(tk.iDim));
			Button(gc, StoreButtonRect(aR), _L("Get a browser engine"), Rgb(tk.iAccent), Rgb(Dark() ? 0x101114 : 0xFFFFFF));
			}
		else
			{
			TBuf<64> s;
			if (priv) s = _L("Nothing is saved from this tab");
			else if (iSet->iSearchName.Length()) { s = _L("Search: "); TBuf<40> n; n.Copy(iSet->iSearchName); s.Append(n); }
			Txt(gc, s, b, CGraphicsContext::ECenter, Rgb(tk.iDim));
			}
		}

	void DrawField(CWindowGc& gc, const TPbLayout& l, const TPbTokens& tk) const
		{
		const TRect f = FieldRect(l);
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(Rgb(tk.iField));
		gc.DrawRoundRect(f, TSize(f.Height() / 2, f.Height() / 2));
		TRect text = f;
		text.iTl.iX += f.Height() + 2;
		text.iBr.iX -= f.Height() / 2;
		TBuf<256> shown;
		TBool hint = ETrue;
		if (iCur >= 0 && iTabs[iCur].iUrl.Length())
			{
			shown = iTabs[iCur].iUrl;
			TInt p = shown.Find(_L("://"));
			if (p >= 0)
				shown.Delete(0, p + 3);
			hint = EFalse;
			}
		else
			shown = _L("Search or enter address");
		Txt(gc, shown, text, CGraphicsContext::ELeft, Rgb(hint ? tk.iDim : tk.iInk));
		const TInt sec = (iCur >= 0 && iTabs[iCur].iUrl.Length()) ? iTabs[iCur].iSecurity : -1;
		const TPoint c(f.iTl.iX + f.Height() / 2, f.Center().iY);
		if (sec < 0)
			return;
		const TRgb col = Rgb(sec == PHE_SEC_SECURE ? tk.iGood : (sec == PHE_SEC_NONE ? tk.iDim : tk.iWarn));
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenSize(TSize(2, 2));
		gc.SetPenColor(col);
		gc.SetBrushStyle(sec == PHE_SEC_NONE ? CGraphicsContext::ENullBrush : CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(col);
		if (sec == PHE_SEC_NONE)
			gc.DrawEllipse(TRect(c.iX - 7, c.iY - 7, c.iX + 7, c.iY + 7));
		else
			{
			gc.DrawRect(TRect(c.iX - 6, c.iY - 1, c.iX + 6, c.iY + 7));
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			gc.DrawArc(TRect(c.iX - 4, c.iY - 8, c.iX + 4, c.iY + 2), TPoint(c.iX - 4, c.iY - 3), TPoint(c.iX + 4, c.iY - 3));
			}
		gc.SetPenSize(TSize(1, 1));
		}

	void DrawGlyph(CWindowGc& gc, TInt aIx, const TRect& aR, const TPbTokens& tk) const
		{
		const TPoint c = aR.Center();
		const TPbTab* t = iCur >= 0 ? &iTabs[iCur] : NULL;
		TBool on = ETrue;
		if (aIx == EBtnBack) on = t && t->iBack;
		if (aIx == EBtnFwd) on = t && t->iFwd;
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenSize(TSize(2, 2));
		gc.SetPenColor(Rgb(on ? tk.iInk : tk.iDim));
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		switch (aIx)
			{
		case EBtnBack:
			gc.DrawLine(TPoint(c.iX + 8, c.iY), TPoint(c.iX - 8, c.iY));
			gc.DrawLine(TPoint(c.iX - 8, c.iY), TPoint(c.iX, c.iY - 8));
			gc.DrawLine(TPoint(c.iX - 8, c.iY), TPoint(c.iX, c.iY + 8));
			break;
		case EBtnFwd:
			gc.DrawLine(TPoint(c.iX - 8, c.iY), TPoint(c.iX + 8, c.iY));
			gc.DrawLine(TPoint(c.iX + 8, c.iY), TPoint(c.iX, c.iY - 8));
			gc.DrawLine(TPoint(c.iX + 8, c.iY), TPoint(c.iX, c.iY + 8));
			break;
		case EBtnReload:
			if (t && t->iLoading && t->iUrl.Length())
				{
				gc.DrawLine(TPoint(c.iX - 7, c.iY - 7), TPoint(c.iX + 7, c.iY + 7));
				gc.DrawLine(TPoint(c.iX - 7, c.iY + 7), TPoint(c.iX + 7, c.iY - 7));
				}
			else
				{
				gc.DrawArc(TRect(c.iX - 8, c.iY - 8, c.iX + 8, c.iY + 8), TPoint(c.iX + 8, c.iY - 3), TPoint(c.iX + 8, c.iY + 3));
				gc.DrawLine(TPoint(c.iX + 8, c.iY - 7), TPoint(c.iX + 8, c.iY - 1));
				gc.DrawLine(TPoint(c.iX + 8, c.iY - 1), TPoint(c.iX + 2, c.iY - 3));
				}
			break;
		case EBtnTabs:
			{
			gc.DrawRoundRect(TRect(c.iX - 9, c.iY - 9, c.iX + 9, c.iY + 9), TSize(4, 4));
			gc.SetPenSize(TSize(1, 1));
			TBuf<4> n;
			n.Num(iTabs.Count());
			Txt(gc, n, TRect(c.iX - 9, c.iY - 9, c.iX + 9, c.iY + 9), CGraphicsContext::ECenter, Rgb(tk.iInk));
			}
			break;
		case EBtnMenu:
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(Rgb(tk.iInk));
			for (TInt k = -1; k <= 1; ++k)
				gc.DrawEllipse(TRect(c.iX - 2, c.iY + k * 7 - 2, c.iX + 2, c.iY + k * 7 + 2));
			break;
			}
		gc.SetPenSize(TSize(1, 1));
		}

	// ---- the tab grid ----
	TRect GridHeader() const
		{
		const TSize s = Size();
		return BarAtBottom() ? TRect(0, s.iHeight - 56, s.iWidth, s.iHeight) : TRect(0, 0, s.iWidth, 56);
		}
	TRect GridArea() const
		{
		const TSize s = Size();
		return BarAtBottom() ? TRect(0, 0, s.iWidth, s.iHeight - 56) : TRect(0, 56, s.iWidth, s.iHeight);
		}
	TInt GridCols() const { return Size().iWidth > Size().iHeight ? 3 : 2; }
	TRect CardRect(TInt aIx) const
		{
		const TRect a = GridArea();
		const TInt cols = GridCols(), m = 8;
		const TInt w = (a.Width() - m * (cols + 1)) / cols, th = w * 3 / 4, h = th + 28;
		const TInt row = aIx / cols, col = aIx % cols;
		const TInt x = a.iTl.iX + m + col * (w + m), y = a.iTl.iY + m + row * (h + m) - iGridScroll;
		return TRect(x, y, x + w, y + h);
		}
	TInt GridContentH() const
		{
		const TRect c = CardRect(0);
		return ((iTabs.Count() + GridCols() - 1) / GridCols()) * (c.Height() + 8) + 8;
		}
	void DrawGrid(CWindowGc& gc) const
		{
		const TPbTokens tk = PbTokens(Dark(), EFalse);
		const TRect area = GridArea();
		Fill(gc, area, Rgb(tk.iPage));
		gc.SetClippingRect(area);
		for (TInt i = 0; i < iTabs.Count(); ++i)
			{
			const TPbTab& t = iTabs[i];
			const TRect c = CardRect(i);
			if (!c.Intersects(area))
				continue;
			const TPbTokens ct = PbTokens(Dark(), t.iPrivate);
			gc.SetPenStyle(i == iCur ? CGraphicsContext::ESolidPen : CGraphicsContext::ENullPen);
			gc.SetPenColor(Rgb(tk.iAccent));
			gc.SetPenSize(TSize(2, 2));
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(Rgb(ct.iBar));
			gc.DrawRoundRect(c, TSize(8, 8));
			gc.SetPenSize(TSize(1, 1));
			const TRect th(c.iTl.iX + 3, c.iTl.iY + 28, c.iBr.iX - 3, c.iBr.iY - 3);
			if (t.iThumb)
				gc.DrawBitmap(th, t.iThumb, TRect(0, 0, KThumbW, KThumbW * th.Height() / th.Width()));
			else
				{
				Fill(gc, th, Rgb(ct.iPage));
				Txt(gc, t.iPrivate ? TPtrC(_L("Private")) : TPtrC(_L("New tab")), th, CGraphicsContext::ECenter, Rgb(ct.iDim));
				}
			TRect title(c.iTl.iX + 6, c.iTl.iY, c.iBr.iX - 28, c.iTl.iY + 28);
			Txt(gc, t.iTitle.Length() ? TPtrC(t.iTitle) : (t.iPrivate ? TPtrC(_L("Private tab")) : TPtrC(_L("New tab"))), title,
				CGraphicsContext::ELeft, Rgb(ct.iInk));
			const TRect x(c.iBr.iX - 28, c.iTl.iY, c.iBr.iX, c.iTl.iY + 28);       // close "x"
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenColor(Rgb(ct.iInk));
			gc.DrawLine(TPoint(x.iTl.iX + 9, x.iTl.iY + 9), TPoint(x.iBr.iX - 9, x.iBr.iY - 9));
			gc.DrawLine(TPoint(x.iTl.iX + 9, x.iBr.iY - 9), TPoint(x.iBr.iX - 9, x.iTl.iY + 9));
			}
		gc.CancelClippingRect();
		const TRect h = GridHeader();
		Fill(gc, h, Rgb(tk.iBar));
		const TInt half = h.Width() / 2;
		Button(gc, TRect(h.iTl.iX + 10, h.iTl.iY + 8, h.iTl.iX + half - 5, h.iBr.iY - 8), _L("+ New tab"), Rgb(tk.iAccent), Rgb(Dark() ? 0x101114 : 0xFFFFFF));
		Button(gc, TRect(h.iTl.iX + half + 5, h.iTl.iY + 8, h.iBr.iX - 10, h.iBr.iY - 8), _L("+ Private"), Rgb(0x3F3260), Rgb(0xECE6FA));
		}

	// ---- the overflow sheet (rises from the bar's side) ----
	enum { KRowH = 48 };
	TRect SheetRect() const
		{
		const TPbLayout l = Layout();
		const TInt h = ESheetCount * KRowH;
		const TRect pg = R(l.iPage);
		return BarAtBottom() ? TRect(pg.iTl.iX, pg.iBr.iY - h, pg.iBr.iX, pg.iBr.iY) : TRect(pg.iTl.iX, pg.iTl.iY, pg.iBr.iX, pg.iTl.iY + h);
		}
	void SheetLabel(TInt aItem, TDes& aOut) const
		{
		switch (aItem)
			{
		case ESheetNewTab: aOut = _L("New tab"); break;
		case ESheetNewPrivate: aOut = _L("New private tab"); break;
		case ESheetFind: aOut = _L("Find in page"); break;
		case ESheetBar: aOut = BarAtBottom() ? _L("Bar position: bottom (tap for top)") : _L("Bar position: top (tap for bottom)"); break;
		case ESheetTheme: aOut = iSet->iTheme == EPbThemeLight ? _L("Theme: light") : (iSet->iTheme == EPbThemeDark ? _L("Theme: dark") : _L("Theme: follow system")); break;
		case ESheetSearch: { aOut = _L("Search engine: "); TBuf<40> n; n.Copy(iSet->iSearchName); aOut.Append(n); } break;
		case ESheetStore: aOut = _L("Get a browser engine"); break;
		default: aOut = _L("Exit"); break;
			}
		}
	void DrawSheet(CWindowGc& gc) const
		{
		const TPbTokens tk = Tk();
		const TRect s = SheetRect();
		Fill(gc, s, Rgb(tk.iBar));
		for (TInt i = 0; i < ESheetCount; ++i)
			{
			const TRect r(s.iTl.iX, s.iTl.iY + i * KRowH, s.iBr.iX, s.iTl.iY + (i + 1) * KRowH);
			TBuf<64> lab;
			SheetLabel(i, lab);
			Txt(gc, lab, r, CGraphicsContext::ELeft, Rgb(tk.iInk), 16);
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenColor(Rgb(tk.iField));
			gc.DrawLine(TPoint(r.iTl.iX, r.iBr.iY - 1), TPoint(r.iBr.iX, r.iBr.iY - 1));
			}
		}

	// ---- first launch: choose the search engine (nothing preselected) ----
	TRect PickRow(TInt aIx) const
		{
		const TSize s = Size();
		const TInt rowH = 44, top = 64 + 8;
		const TInt perCol = Max(1, (s.iHeight - top) / rowH);
		const TInt col = aIx / perCol, row = aIx % perCol;
		const TInt colW = s.iWidth > s.iHeight ? s.iWidth / 2 : s.iWidth;
		return TRect(col * colW, top + row * rowH, (col + 1) * colW, top + (row + 1) * rowH);
		}
	void DrawSearchPick(CWindowGc& gc) const
		{
		const TPbTokens tk = PbTokens(Dark(), EFalse);
		const TSize s = Size();
		Fill(gc, TRect(0, 0, s.iWidth, s.iHeight), Rgb(tk.iPage));
		Txt(gc, _L("Choose your search engine"), TRect(0, 8, s.iWidth, 64), CGraphicsContext::ECenter, Rgb(tk.iInk));
		for (TInt i = 0; i <= KSearchCount; ++i)
			{
			const TRect r = PickRow(i);
			TBuf<48> n;
			if (i < KSearchCount) n.Copy(P8(KSearches[i].iName)); else n = _L("Custom address...");
			Txt(gc, n, r, CGraphicsContext::ELeft, Rgb(tk.iInk), 20);
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenColor(Rgb(tk.iField));
			gc.DrawLine(TPoint(r.iTl.iX + 12, r.iBr.iY - 1), TPoint(r.iBr.iX - 12, r.iBr.iY - 1));
			}
		}
	void PickSearchL(TInt aIx)
		{
		if (aIx < KSearchCount)
			{
			iSet->iSearchName.Copy(P8(KSearches[aIx].iName));
			iSet->iSearchTemplate.Copy(P8(KSearches[aIx].iTemplate));
			}
		else
			{
			TBuf<200> u(_L("https://example.org/search?q=%s"));
			CAknTextQueryDialog* d = CAknTextQueryDialog::NewL(u);
			if (!d->ExecuteLD(R_PHEBROWSER_URL_QUERY) || u.Find(_L("%s")) == KErrNotFound)
				return;                                               // cancelled, or no %s: ask again
			iSet->iSearchName.Copy(_L8("Custom"));
			iSet->iSearchTemplate.Copy(u);
			}
		iSet->Save(iEikonEnv->FsSession());
		iMode = EModeBrowse;
		DrawDeferred();
		}

	// ---- input ----
	void HandlePointerEventL(const TPointerEvent& aE)
		{
		if (aE.iType == TPointerEvent::EButton1Down)
			{ iDownPos = aE.iPosition; iDragY = aE.iPosition.iY; iMoved = EFalse; }
		else if (aE.iType == TPointerEvent::EDrag && Abs(aE.iPosition.iY - iDownPos.iY) > 6)
			iMoved = ETrue;
		switch (iMode)
			{
		case EModeGrid: GridPointer(aE); return;
		case EModeSheet: SheetPointer(aE); return;
		case EModeSearchPick:
			if (aE.iType == TPointerEvent::EButton1Up)
				for (TInt i = 0; i <= KSearchCount; ++i)
					if (PickRow(i).Contains(aE.iPosition))
						PickSearchL(i);
			return;
		default: break;
			}
		const TPbLayout l = Layout();
		switch (aE.iType)
			{
		case TPointerEvent::EButton1Down:
			iDragging = R(l.iPage).Contains(aE.iPosition);
			break;
		case TPointerEvent::EDrag:
			if (iDragging && iCur >= 0 && iApi && iTabs[iCur].iUrl.Length())
				{
				const TInt dy = iDragY - aE.iPosition.iY;
				if (dy)
					{
					iApi->scroll(iTabs[iCur].iView, 0, dy);
					iDragY = aE.iPosition.iY;
					}
				}
			break;
		case TPointerEvent::EButton1Up:
			{
			const TBool wasPage = iDragging;
			iDragging = EFalse;
			if (wasPage)
				{
				if (!iApi && StoreButtonRect(R(l.iPage)).Contains(aE.iPosition))
					iEikonEnv->EikAppUi()->HandleCommandL(EPheBrCmdGetEngine);
				break;
				}
			if (FieldRect(l).Contains(aE.iPosition))
				{ AskUrlL(); break; }
			for (TInt b = 0; b < EBtnCount; ++b)
				if (ButtonRect(b, l).Contains(aE.iPosition))
					Press(b);
			}
			break;
		default:
			break;
			}
		}
	void Press(TInt aBtn)
		{
		if (!iApi || iCur < 0)
			{ if (aBtn == EBtnMenu) OpenSheet(); return; }
		const PheView v = iTabs[iCur].iView;
		switch (aBtn)
			{
		case EBtnBack: iApi->cmd(v, PHE_CMD_BACK); break;
		case EBtnFwd: iApi->cmd(v, PHE_CMD_FORWARD); break;
		case EBtnReload: if (iTabs[iCur].iUrl.Length()) iApi->cmd(v, iTabs[iCur].iLoading ? PHE_CMD_STOP : PHE_CMD_RELOAD); break;
		case EBtnTabs:
			CaptureThumb(iCur);
			iGridScroll = 0;
			iMode = EModeGrid;
			DrawDeferred();
			break;
		case EBtnMenu: OpenSheet(); break;
			}
		}
	void GridPointer(const TPointerEvent& aE)
		{
		if (aE.iType == TPointerEvent::EDrag && GridArea().Contains(aE.iPosition))
			{
			iGridScroll = Max(0, Min(Max(0, GridContentH() - GridArea().Height()), iGridScroll + iDragY - aE.iPosition.iY));
			iDragY = aE.iPosition.iY;
			DrawDeferred();
			return;
			}
		if (aE.iType != TPointerEvent::EButton1Up || iMoved)
			return;
		const TRect h = GridHeader();
		if (h.Contains(aE.iPosition))
			{
			TRAP_IGNORE(NewTabL(KNullDesC, aE.iPosition.iX > h.Width() / 2));
			return;
			}
		for (TInt i = 0; i < iTabs.Count(); ++i)
			{
			const TRect c = CardRect(i);
			if (c.Contains(aE.iPosition))
				{
				if (aE.iPosition.iX > c.iBr.iX - 32 && aE.iPosition.iY < c.iTl.iY + 32)
					{ CloseTab(i); DrawDeferred(); }
				else
					SelectTab(i);
				return;
				}
			}
		}
	void SheetPointer(const TPointerEvent& aE)
		{
		if (aE.iType != TPointerEvent::EButton1Up)
			return;
		const TRect s = SheetRect();
		if (!s.Contains(aE.iPosition))
			{ iMode = EModeBrowse; DrawDeferred(); return; }
		const TInt item = (aE.iPosition.iY - s.iTl.iY) / KRowH;
		iMode = EModeBrowse;
		switch (item)
			{
		case ESheetNewTab: TRAP_IGNORE(NewTabL(KNullDesC, EFalse)); break;
		case ESheetNewPrivate: TRAP_IGNORE(NewTabL(KNullDesC, ETrue)); break;
		case ESheetFind: FindDemo(); break;
		case ESheetBar: ToggleBar(); break;
		case ESheetTheme:
			iSet->iTheme = (iSet->iTheme + 1) % 3;
			iSet->Save(iEikonEnv->FsSession());
			break;
		case ESheetSearch: iMode = EModeSearchPick; break;
		case ESheetStore: iEikonEnv->EikAppUi()->HandleCommandL(EPheBrCmdGetEngine); break;
		case ESheetExit: iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit); break;
			}
		DrawDeferred();
		}
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
		{
		if (aType != EEventKey || iCur < 0 || iMode != EModeBrowse || !iApi || !iTabs[iCur].iUrl.Length())
			return EKeyWasNotConsumed;
		const TInt step = Layout().iPage.iH / 3;
		if (aKey.iCode == EKeyDownArrow) { iApi->scroll(iTabs[iCur].iView, 0, step); return EKeyWasConsumed; }
		if (aKey.iCode == EKeyUpArrow) { iApi->scroll(iTabs[iCur].iView, 0, -step); return EKeyWasConsumed; }
		return EKeyWasNotConsumed;
		}

	const PheWebEngineApi* iApi = NULL;
	CPbSettings* iSet = NULL;
	RArray<TPbTab> iTabs;
	TInt iCur = -1;
	TPbMode iMode = EModeBrowse;
	CAsyncCallBack* iCallBack = NULL;
	CFbsBitmap* iPageBmp = NULL;
	TInt iDragY = 0, iGridScroll = 0;
	TPoint iDownPos;
	TBool iDragging = EFalse, iMoved = EFalse;
	};

// ---- application ------------------------------------------------------------------------------------------------------------
class CPheBrowserAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		RDebug::Printf("PH: [phebrowser] AppUi base ok");
		iSet.Load(iEikonEnv->FsSession());
		RDebug::Printf("PH: [phebrowser] settings ok, theme=%d bar=%d", iSet.iTheme, (int)iSet.iBar);
		iView = new (ELeave) CPbContainer;
		iView->ConstructL(ClientRect(), &iSet);
		RDebug::Printf("PH: [phebrowser] container ok");
		AddToStackL(iView);
		}
	~CPheBrowserAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		}
	void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aPane)
		{
		if (aResourceId == R_PHEBROWSER_MENU_PANE)
			aPane->SetItemTextL(EPheBrCmdBarToggle, iView->BarAtBottom() ? _L("Move bar to top") : _L("Move bar to bottom"));
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheBrCmdMenu: iView->OpenSheet(); break;
		case EPheBrCmdNewTab: iView->NewTabL(KNullDesC, EFalse); break;
		case EPheBrCmdNewPrivate: iView->NewTabL(KNullDesC, ETrue); break;
		case EPheBrCmdCloseTab: iView->CloseTab(); break;
		case EPheBrCmdNextTab: iView->NextTab(); break;
		case EPheBrCmdBarToggle: iView->ToggleBar(); break;
		case EPheBrCmdFind: iView->FindDemo(); break;
		case EPheBrCmdGetEngine: StartStore(); break;
		case EAknSoftkeyBack:
			if (!iView->GoBackIfPossible())
				Exit();
			break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
			}
		}
private:
	void StartStore()      // the hand-off: launch the store. All the browser knows is its uid; the store UI may change freely.
		{
		RApaLsSession ls;
		if (ls.Connect() != KErrNone)
			return;
		TApaAppInfo info;
		if (ls.GetAppInfo(info, KPheStoreUid) == KErrNone)
			{
			CApaCommandLine* cl = CApaCommandLine::NewLC();
			cl->SetExecutableNameL(info.iFullName);
			cl->SetCommandL(EApaCommandRun);
			ls.StartApp(*cl);
			CleanupStack::PopAndDestroy(cl);
			}
		ls.Close();
		}
	CPbSettings iSet;
	CPbContainer* iView = NULL;
	};

class CPheBrowserDocument : public CAknDocument
	{
public:
	CPheBrowserDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { RDebug::Printf("PH: [phebrowser] CreateAppUiL"); return new (ELeave) CPheBrowserAppUi; }
	};

class CPheBrowserApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KPheBrowserAppUid; }
	CApaDocument* CreateDocumentL() { RDebug::Printf("PH: [phebrowser] CreateDocumentL"); return new (ELeave) CPheBrowserDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	RDebug::Printf("PH: [phebrowser] NewApplication");
	return new CPheBrowserApplication;
	}

GLDEF_C TInt E32Main()
	{
	RDebug::Printf("PH: [phebrowser] E32Main");
	TInt r = EikStart::RunApplication(NewApplication);
	RDebug::Printf("PH: [phebrowser] RunApplication returned %d", r);
	return r;
	}
