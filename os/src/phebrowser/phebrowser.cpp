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
#include <e32math.h>
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
#include <eikedwin.h>
#include <eikbtgpc.h>
#include <txtfrmat.h>
#include <txtfmlyr.h>
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
#include "pb_enginehost.h"
#ifdef PHEBROWSER_STKDUMP
#include "pb_stkdump.h"
#endif

// the app's debug line: the serial log always; a file as well in remote-controlled (stock-phone test) builds, which have no debug port
static void PbTrace(const char* aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	TBuf8<256> b;
	b.AppendFormatList(TPtrC8((const TUint8*)aFmt), list);
	VA_END(list);
	TBuf<256> w;
	w.Copy(b);
	RDebug::Print(_L("%S"), &w);
#ifdef PHEBROWSER_REMOTE
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		RFile f;
		TInt r = f.Open(fs, _L("C:\\data\\phebrowser_trace.txt"), EFileWrite | EFileShareAny);
		if (r == KErrNotFound)
			r = f.Create(fs, _L("C:\\data\\phebrowser_trace.txt"), EFileWrite | EFileShareAny);
		if (r == KErrNone)
			{
			TInt pos = 0;
			f.Seek(ESeekEnd, pos);
			b.Append(_L8("\r\n"));
			f.Write(b);
			f.Close();
			}
		fs.Close();
		}
#endif
	}

struct TPbEarly { TPbEarly() { PbTrace("PH: [phebrowser] static init"); } };
static TPbEarly gPbEarly;
const TUid KPheBrowserAppUid = { 0xE7016B20 };
const TUid KPheStoreUid = { 0xE70157FA };

static const TInt KMaxTabs = 8;
static const TUint16 KFindDemoText[] = { 'x' };
static const TInt KThumbW = 160, KThumbH = 240;

static TRgb Rgb(unsigned aC) { return TRgb((aC >> 16) & 0xFF, (aC >> 8) & 0xFF, aC & 0xFF); }

// ---- the search engines: chosen on first launch, none preselected; %s = the query ---------------------------------------
struct TPbSearch { const char* iName; const char* iTemplate; unsigned iColor; };   // iColor: the lettermark tile (a plain colour, no logo)
static const TPbSearch KSearches[] =
	{
	{ "Google", "https://www.google.com/search?q=%s", 0x4285F4 },
	{ "DuckDuckGo", "https://duckduckgo.com/?q=%s", 0xDE5833 },
	{ "Bing", "https://www.bing.com/search?q=%s", 0x008373 },
	{ "Startpage", "https://www.startpage.com/do/search?q=%s", 0x6573FF },
	{ "Brave Search", "https://search.brave.com/search?q=%s", 0xFB542B },
	{ "Ecosia", "https://www.ecosia.org/search?q=%s", 0x4E9A06 },
	{ "Qwant", "https://www.qwant.com/?q=%s", 0x5C97FF },
	{ "Yahoo", "https://search.yahoo.com/search?p=%s", 0x6001D2 }
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

enum TPbMode { EModeBrowse, EModeGrid, EModeSheet, EModeSearchPick, EModeNav };
enum TPbSheetItem
	{
	ESheetNewTab, ESheetNewPrivate, ESheetFind, ESheetBar, ESheetTheme, ESheetSearch, ESheetEngine, ESheetStore, ESheetExit, ESheetCount
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
				if (iTabs[i].iView)
					iApi->view_destroy(iTabs[i].iView);
			iApi->shutdown();
			}
		delete iEngines;          // unloads the engine plug-in (after its shutdown)
		for (TInt i = 0; i < iTabs.Count(); ++i)
			delete iTabs[i].iThumb;
		delete iWake;
#ifdef PHEBROWSER_REMOTE
		delete iRemote;
#endif
		if (iSmall)
			iEikonEnv->ScreenDevice()->ReleaseFont(iSmall);
		delete iEdit;
		delete iPageBmp;
		iTabs.Close();
		}

	void ConstructL(const TRect& aRect, CPbSettings* aSettings)
		{
		iSet = aSettings;
		CreateWindowL();
		EnableDragEvents();
		Window().EnableAdvancedPointers();                    // multi-touch: the second finger arrives as pointer 1 (pinch zoom)
		iWake = CPbWake::NewL(TCallBack(Pump, this));          // the engine notifies from its own thread
		iEdit = new (ELeave) CEikEdwin;                       // the address editor: in place, inside the bar
		iEdit->SetContainerWindowL(*this);
		iEdit->ConstructL(EEikEdwinNoWrap, 0, 255, 1);
		iEdit->SetAknEditorCase(EAknEditorLowerCase);          // an address is not a sentence: no auto-capital, no predictive text
		iEdit->SetAknEditorFlags(EAknEditorFlagNoT9 | EAknEditorFlagNoEditIndicators | EAknEditorFlagLatinInputModesOnly);
		iEdit->MakeVisible(EFalse);
		{                                                    // the address font: about 2/3 of the normal one
		TFontSpec spec = iEikonEnv->NormalFont()->FontSpecInTwips();
		spec.iHeight = spec.iHeight * 2 / 3;
		iEikonEnv->ScreenDevice()->GetNearestFontInTwips(iSmall, spec);
		}
		SetRect(aRect);
		ActivateL();
		iEngines = CPbEngines::NewL(iEikonEnv->FsSession());   // lists the installed engines; none is loaded yet
		PbTrace("PH: [phebrowser] engines listed: %d", iEngines->Count());
		if (iEngines->Count() > 0)
			NewTabL(KNullDesC, EFalse);       // opens on the new-tab page (the engine loads on the first navigation)
		if (iSet->iSearchTemplate.Length() == 0)
			iMode = EModeSearchPick;          // first launch: the user chooses the search engine
		// (the start URL / open.txt / start.txt are read once the command line is known: CPheBrowserAppUi::ProcessCommandParametersL)
#ifdef PHEBROWSER_REMOTE
		iRemote = CPeriodic::NewL(CActive::EPriorityLow);
		iRemote->Start(500000, 500000, TCallBack(RemoteTick, this));
#endif
		}
#ifdef PHEBROWSER_REMOTE
	// REMOTE CONTROL (stock-phone testing; PHEBROWSER_REMOTE builds only -- PyS60's graphics.screenshot() is stale on stock phones): the same
	// file hooks as the NetSurf port, polled every 500 ms, all under C:\data:
	//   netsurf_shot        exists -> the screen into netsurf_shot.bmp and the page bitmap into netsurf_shot_page.bmp (24-bit BMP); file deleted
	//   netsurf_tap         "x y" (screen pixels) -> a tap there (press + release); file deleted
	//   netsurf_query_auto  its first line answers the address editor (while it is open); file deleted
	//   netsurf_quit        exists -> the app exits; file deleted
	static TInt RemoteTick(TAny* aSelf) { static_cast<CPbContainer*>(aSelf)->RemoteStep(); return 0; }
	TBool RemoteTake(const TDesC& aName, TDes8& aBuf)
		{
		RFs& fs = iEikonEnv->FsSession();
		TFileName f(_L("C:\\data\\"));
		f.Append(aName);
		RFile file;
		if (file.Open(fs, f, EFileRead | EFileShareAny) != KErrNone)
			return EFalse;
		aBuf.Zero();
		file.Read(aBuf);
		file.Close();
		fs.Delete(f);
		return ETrue;
		}
	void RemoteStep()
		{
		RFs& fs = iEikonEnv->FsSession();
		TBuf8<256> b;
		if (RemoteTake(_L("netsurf_quit"), b))
			{ iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit); return; }
		if (RemoteTake(_L("netsurf_tap"), b))
			{
			TLex8 lex(b);
			TInt x = 0, y = 0;
			lex.SkipSpace(); lex.Val(x); lex.SkipSpace(); lex.Val(y);
			PbTrace("PH: [phebrowser] remote tap %d %d", x, y);
			TPointerEvent pe;
			pe.iModifiers = 0;
			pe.iPosition = TPoint(x, y) - PositionRelativeToScreen();
			pe.iParentPosition = pe.iPosition;
			pe.iType = TPointerEvent::EButton1Down;
			TRAP_IGNORE(HandlePointerEventL(pe));
			pe.iType = TPointerEvent::EButton1Up;
			TRAP_IGNORE(HandlePointerEventL(pe));
			}
		if (iEditing)
			{
			TBuf8<256> a;
			if (RemoteTake(_L("netsurf_query_auto"), a))
				{
				a.Trim();
				TInt e = a.Locate('\n');
				if (e >= 0) a.SetLength(e);
				a.Trim();
				TBuf<256> u;
				u.Copy(a);
				PbTrace("PH: [phebrowser] remote address answer");
				TRAP_IGNORE(iEdit->SetTextL(&u));
				TRAP_IGNORE(EndEditL(ETrue));
				}
			}
		if (RemoteTake(_L("netsurf_shot"), b))
			{
			CFbsBitmap* scr = new CFbsBitmap;
			if (scr && scr->Create(Size(), EColor16MU) == KErrNone)
				{
				const TInt r = iEikonEnv->ScreenDevice()->CopyScreenToBitmap(scr, Rect());
				PbTrace("PH: [phebrowser] remote shot screen %d", r);
				if (r == KErrNone)
					WriteBmp(fs, _L("C:\\data\\netsurf_shot.bmp"), scr);
				}
			delete scr;
			if (iPageBmp)
				WriteBmp(fs, _L("C:\\data\\netsurf_shot_page.bmp"), iPageBmp);
			}
		}
	// a 24-bit BMP of an EColor16MU bitmap (the host converts it to PNG)
	static void WriteBmp(RFs& aFs, const TDesC& aName, CFbsBitmap* aBmp)
		{
		const TSize sz = aBmp->SizeInPixels();
		const TInt rowBytes = (sz.iWidth * 3 + 3) & ~3;
		const TInt imgBytes = rowBytes * sz.iHeight;
		RFile f;
		if (f.Replace(aFs, aName, EFileWrite) != KErrNone)
			return;
		TUint8 hdr[54];
		Mem::FillZ(hdr, 54);
		hdr[0] = 'B'; hdr[1] = 'M';
		const TInt total = 54 + imgBytes;
		for (TInt i = 0; i < 4; ++i) hdr[2 + i] = (TUint8)(total >> (8 * i));
		hdr[10] = 54; hdr[14] = 40;
		for (TInt i = 0; i < 4; ++i) { hdr[18 + i] = (TUint8)(sz.iWidth >> (8 * i)); hdr[22 + i] = (TUint8)(sz.iHeight >> (8 * i)); }
		hdr[26] = 1; hdr[28] = 24;
		for (TInt i = 0; i < 4; ++i) hdr[34 + i] = (TUint8)(imgBytes >> (8 * i));
		f.Write(TPtrC8(hdr, 54));
		HBufC8* row = HBufC8::New(rowBytes);
		if (row)
			{
			TPtr8 rp = row->Des();
			aBmp->LockHeap();
			for (TInt y = sz.iHeight - 1; y >= 0; --y)
				{
				const TUint8* src = (const TUint8*)aBmp->DataAddress() + y * aBmp->DataStride();
				rp.SetLength(rowBytes);
				rp.FillZ();
				for (TInt x = 0; x < sz.iWidth; ++x)
					{ rp[x * 3] = src[x * 4]; rp[x * 3 + 1] = src[x * 4 + 1]; rp[x * 3 + 2] = src[x * 4 + 2]; }
				f.Write(rp);
				}
			aBmp->UnlockHeap();
			delete row;
			}
		f.Close();
		}
	CPeriodic* iRemote = NULL;
#endif
	// test / automation hook: C:\private\E7016B20\open.txt = one URL per line; the first opens at start, each next one in a new tab
	// once the previous page has finished (the file is deleted when read)
	void LoadStartHook()
		{
		RFs& fs = iEikonEnv->FsSession();
		TFileName f;
		fs.PrivatePath(f);
		f.Insert(0, _L("C:"));
		f.Append(_L("open.txt"));
		RFile file;
		if (file.Open(fs, f, EFileRead) != KErrNone)
			{
			// the start URL file of the device test tools: C:\data\phebrowser\start.txt (one URL; used when the application was
			// started without a URL on its command line -- see OpenUrl -- and deleted when read)
			f = _L("C:\\data\\phebrowser\\start.txt");
			if (file.Open(fs, f, EFileRead) != KErrNone)
				return;
			}
		TBuf8<400> b;
		file.Read(b);
		file.Close();
		fs.Delete(f);
		iQueue.Copy(b);
		NextQueued();
		}
	// a URL given at launch (the apparc command line: the document name, or the tail): opens in the first empty tab, else a new one
	void OpenUrl(const TDesC& aUrl)
		{
		PbTrace("PH: [phebrowser] start URL on the command line (%d chars)", aUrl.Length());
		iQueue.Copy(aUrl.Left(iQueue.MaxLength()));
		if (iCur < 0)
			{
			TBuf<200> u;
			u.Copy(aUrl.Left(200));
			u.Trim();
			TRAP_IGNORE(NewTabL(u, EFalse));
			iQueue.Zero();
			return;
			}
		NextQueued();
		}
	// a URL handed to the RUNNING browser (phestart's open-file message): the current tab navigates to it
	void OpenRunning(const TDesC& aUrl)
		{
		if (iCur < 0)
			OpenUrl(aUrl);
		else
			Navigate(aUrl);
		}
	void NextQueued()
		{
		iQueue.TrimLeft();
		TInt e = iQueue.Locate('\n');
		TBuf<200> line;                    // (not a ?: of TBuf and TPtrC: that slices the descriptor)
		line.Copy(iQueue.Left(Min(e < 0 ? iQueue.Length() : e, 200)));
		iQueue.Delete(0, e < 0 ? iQueue.Length() : e + 1);
		line.Trim();
		if (line.Length() == 0 || iCur < 0)
			return;
		if (iTabs[iCur].iUrl.Length() == 0)
			Navigate(line);
		else
			TRAP_IGNORE(NewTabL(line, EFalse));
		}

	// ---- commands ----
	void NewTabL(const TDesC& aUrl, TBool aPrivate)
		{
		if (!iEngines || iEngines->Count() == 0 || iTabs.Count() >= KMaxTabs)
			return;
		if (aPrivate && !((iApi ? iApi->caps : iEngines->DefaultCaps()) & PHE_CAP_PRIVATE))
			{
			CAknInformationNote* n = new (ELeave) CAknInformationNote(ETrue);
			n->ExecuteLD(_L("This engine has no private tabs"));
			return;
			}
		if (iCur >= 0)
			{
			CaptureThumb(iCur);
			if (iApi && iTabs[iCur].iView)
				iApi->view_visible(iTabs[iCur].iView, 0);
			}
		TPbTab t;
		t.iView = 0;                       // no engine view until the first navigation (EnsureView)
		t.iUrl.Copy(aUrl.Left(Min(aUrl.Length(), 200)));
		t.iTitle.Zero();
		t.iProgress = 0; t.iSecurity = 0; t.iBack = t.iFwd = EFalse; t.iLoading = aUrl.Length() > 0;
		t.iPrivate = aPrivate;
		t.iThumb = NULL;
		iTabs.AppendL(t);
		iCur = iTabs.Count() - 1;
		if (aUrl.Length() && EnsureView(iCur))
			iApi->load(iTabs[iCur].iView, aUrl.Ptr(), aUrl.Length());
		iMode = EModeBrowse;
		PageChanged();
		}
	// the engine loads on the first navigation (ECom, lazily); a tab gets its engine view when it first has something to load
	TBool EnsureView(TInt aIx)
		{
		if (aIx < 0 || aIx >= iTabs.Count())
			return EFalse;
		if (!iApi)
			{
			const PheWebEngineApi* api = iEngines ? iEngines->Load() : NULL;
			PbTrace("PH: [phebrowser] engine load: %d engines, api %d, err %d", iEngines ? iEngines->Count() : -1, api ? 1 : 0, iEngines ? iEngines->LastError() : 0);
			if (!api || api->init(Notify, this) != 0)
				{
				if (api) iEngines->Unload();
				iEngineFailed = ETrue;
				return EFalse;
				}
			iApi = api;
			}
		if (iTabs[aIx].iView == 0)
			{
			const TPbLayout l = Layout();
			iTabs[aIx].iView = iApi->new_view_id();
			iApi->view_create(iTabs[aIx].iView, l.iPage.iW, l.iPage.iH, iTabs[aIx].iPrivate, NULL, 0);   // a private view persists nothing (seam)
			}
		return ETrue;
		}
	void CloseTab(TInt aIx = -1)
		{
		if (aIx < 0) aIx = iCur;
		if (aIx < 0 || aIx >= iTabs.Count())
			return;
		if (iApi && iTabs[aIx].iView)
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
		else if (iApi && iTabs[iCur].iView)
			iApi->view_visible(iTabs[iCur].iView, 1);
		PageChanged();
		}
	void SelectTab(TInt aIx)
		{
		if (aIx == iCur || aIx < 0 || aIx >= iTabs.Count())
			{ iMode = EModeBrowse; PageChanged(); return; }
		if (iApi && iTabs[iCur].iView)
			iApi->view_visible(iTabs[iCur].iView, 0);
		iCur = aIx;
		if (iApi && iTabs[iCur].iView)
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
		if (iEditing)
			{ EndEditL(EFalse); return ETrue; }
		if (iMode != EModeBrowse)
			{ iMode = EModeBrowse; DrawDeferred(); return ETrue; }
		if (iApi && iCur >= 0 && iTabs[iCur].iBack)
			{
			iApi->cmd(iTabs[iCur].iView, PHE_CMD_BACK);
			return ETrue;
			}
		return EFalse;
		}
	void FindDemo() { if (iApi && iCur >= 0 && iTabs[iCur].iView && iApi->find) iApi->find(iTabs[iCur].iView, KFindDemoText, 1, 0); }
	void AskUrlL()
		{
		if (iCur < 0)
			return;
		if (iSet->iSearchTemplate.Length() == 0)
			{ iMode = EModeSearchPick; DrawDeferred(); return; }
		StartEditL();
		}
	// ---- the address bar edits IN PLACE: a CEikEdwin inside the field (the FEP / on-screen keyboard attach to it) ----
	TRect EditRect(const TPbLayout& l) const
		{
		TRect f = FieldRect(l);
		f.iTl.iX += f.Height() / 2;
		f.iBr.iX -= f.Height();               // room for the clear (x) button
		f.Shrink(0, 3);
		return f;
		}
	TRect ClearRect(const TPbLayout& l) const
		{
		const TRect f = FieldRect(l);
		return TRect(f.iBr.iX - f.Height(), f.iTl.iY, f.iBr.iX, f.iBr.iY);
		}
	void StartEditL()
		{
		iEditing = ETrue;                      // (Layout() puts the bar on top while editing)
		Relayout();
		const TPbLayout l = Layout();
		const TPbTokens tk = Tk();
		iEdit->OverrideColorL(EColorControlBackground, Rgb(tk.iField));
		iEdit->OverrideColorL(EColorControlText, Rgb(tk.iInk));
		iEdit->SetBackgroundColorL(Rgb(tk.iField));
		iEdit->SetSkinBackgroundControlContextL(NULL);       // no skin background: the field behind it shows through
		// (no SetSuppressBackgroundDrawing: the editor redraws itself without the container, so it must clear its own
		// background -- with it suppressed, deleted characters stayed on screen and new text was drawn over them)
		{                                                    // text colour + font through a proper char format layer
		CCharFormatLayer* layer = CCharFormatLayer::NewL();
		CleanupStack::PushL(layer);
		TCharFormat fmt;
		TCharFormatMask mask;
		fmt.iFontPresentation.iTextColor = Rgb(tk.iInk);
		mask.SetAttrib(EAttColor);
		fmt.iFontSpec = iSmall ? iSmall->FontSpecInTwips() : iEikonEnv->NormalFont()->FontSpecInTwips();
		mask.SetAttrib(EAttFontTypeface);
		mask.SetAttrib(EAttFontHeight);
		layer->SetL(fmt, mask);
		iEdit->SetCharFormatLayer(layer);                    // takes ownership (copies the attributes into the live layer)
		CleanupStack::Pop(layer);
		CParaFormatLayer* pl = CParaFormatLayer::NewL();          // the paragraph fill is the editor background
		CleanupStack::PushL(pl);
		CParaFormat* pf = CParaFormat::NewLC();
		TParaFormatMask pm;
		pf->iFillColor = Rgb(tk.iField);
		pm.SetAttrib(EAttFillColor);
		pl->SetL(pf, pm);
		CleanupStack::PopAndDestroy(pf);
		iEdit->SetParaFormatLayer(pl);                       // takes ownership
		CleanupStack::Pop(pl);
		}
		iEdit->SetAknEditorFlags(EAknEditorFlagNoT9 | EAknEditorFlagNoEditIndicators | EAknEditorFlagLatinInputModesOnly
		                         | EAknEditorFlagLaunchPenInputAutomatic | EAknEditorFlagEnablePartialScreen);   // the editor stays visible above an overlay keyboard
		//   // focus opens the on-screen keyboard
		iEdit->SetRect(EditRect(l));
		iEdit->SetTextL(iPageInput ? &KNullDesC : &iTabs[iCur].iUrl);   // a page field: start empty, the text goes to the page
		iEdit->MakeVisible(ETrue);
		iEdit->SetFocus(ETrue);
		iEdit->SelectAllL();                  // select all on focus: typing replaces the address
		iEditing = ETrue;
		DrawDeferred();
		}
	void EndEditL(TBool aGo)
		{
		if (!iEditing)
			return;
		HBufC* text = NULL;
		if (aGo)
			{
			const TInt n = iEdit->TextLength();
			text = HBufC::NewL(n + 1);
			TPtr p = text->Des();
			iEdit->GetText(p);
			}
		iEdit->SetFocus(EFalse);
		iEdit->MakeVisible(EFalse);
		iEditing = EFalse;
		const TBool page = iPageInput;
		iPageInput = EFalse;
		Relayout();                            // the bar returns to its place
		if (text)
			{
			CleanupStack::PushL(text);
			if (page && iApi && iCur >= 0 && iTabs[iCur].iView)
				{                              // into the page's field, then Enter (submits a search box / form)
				iApi->text(iTabs[iCur].iView, (const unsigned short*)text->Ptr(), text->Length());
				iApi->key(iTabs[iCur].iView, 0, 13, 1);
				iApi->key(iTabs[iCur].iView, 0, 13, 0);
				}
			else
				Navigate(*text);
			CleanupStack::PopAndDestroy(text);
			}
		DrawDeferred();
		}
	TInt CountComponentControls() const { return (iEdit && iEditing) ? 1 : 0; }
	CCoeControl* ComponentControl(TInt) const { return iEdit; }
	TBool BarAtBottom() const { return iSet->iBar == EPbBarBottom; }

private:
	enum { EBtnBack, EBtnFwd, EBtnReload, EBtnTabs, EBtnMenu, EBtnNav };

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
		return PbTokens(Dark(), (iMode == EModeBrowse || iMode == EModeNav) && iCur >= 0 && iTabs[iCur].iPrivate);
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
		// while editing the bar goes to the TOP so the editor stays visible above the on-screen keyboard
		return PbLayout(s.iWidth, s.iHeight, iEditing ? EPbBarTop : iSet->iBar, s.iWidth > s.iHeight ? 48 : 56, 1);
		}
	static TRect R(const TPbRect& r) { return TRect(r.iX, r.iY, r.iX + r.iW, r.iY + r.iH); }
	TRect ButtonRect(TInt aIx, const TPbLayout& l) const
		{
		// the bar row: [nav] [field] [overflow]
		const TInt h = l.iAddress.iH;
		if (aIx == EBtnNav)
			return TRect(0, l.iAddress.iY, h, l.iAddress.iY + h);
		return TRect(l.iAddress.iW - h, l.iAddress.iY, l.iAddress.iW, l.iAddress.iY + h);      // EBtnMenu
		}
	TRect FieldRect(const TPbLayout& l) const
		{
		TRect a = R(l.iAddress);
		a.iTl.iX += a.Height();
		a.iBr.iX -= a.Height();
		a.Shrink(2, 12);                    // a slimmer field: the small font needs less height
		return a;
		}
	// the nav popover: back, forward, reload/stop, tabs -- hidden until the nav button is tapped, anchored to it,
	// opening toward the page (below the bar when the bar is on top, above it when at the bottom)
	TRect NavRect() const
		{
		const TPbLayout l = Layout();
		const TInt h = l.iAddress.iH;
		const TInt y = BarAtBottom() ? l.iAddress.iY - h - 8 : l.iAddress.iY + h + 8;
		return TRect(6, y, 6 + 4 * h, y + h);
		}
	TRect NavItemRect(TInt aIx) const
		{
		const TRect r = NavRect();
		const TInt w = r.Width() / 4;
		return TRect(r.iTl.iX + aIx * w, r.iTl.iY, r.iTl.iX + (aIx + 1) * w, r.iBr.iY);
		}
	void SizeChanged() { if (iEditing) iEdit->SetRect(EditRect(Layout())); Relayout(); }
	void Relayout()
		{
		if (!iApi)
			return;
		const TPbLayout l = Layout();
		for (TInt i = 0; i < iTabs.Count(); ++i)
			if (iTabs[i].iView)
				iApi->view_resize(iTabs[i].iView, l.iPage.iW, l.iPage.iH);
		delete iPageBmp; iPageBmp = NULL;
		PageChanged();
		}
	void PageChanged() { CopyPixels(); DrawDeferred(); }

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
		if (!EnsureView(iCur))
			{ DrawDeferred(); return; }       // the engine could not be loaded: the new-tab page says so
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
	static void Notify(void* aCtx, PheView) { static_cast<CPbContainer*>(aCtx)->iWake->Signal(); }   // any thread
	static TInt Pump(TAny* aSelf) { PbTrace("PH: [phebrowser] wake"); static_cast<CPbContainer*>(aSelf)->DrainEvents(); return 0; }
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
			if (e.kind != PHE_EV_REPAINT && e.kind != PHE_EV_PROGRESS)
				PbTrace("PH: [phebrowser] event %d view %d a %d b %d", e.kind, e.view, e.a, e.b);
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
			case PHE_EV_FINISHED:
				if (iQueue.Length())
					NextQueued();
				break;
			case PHE_EV_REPAINT:
				if (t == iCur) page = ETrue;
				break;
			case PHE_EV_FOCUS:                       // a = 2: a text field on the page has the caret -> our editor + keyboard
				if (t == iCur && e.a == 2 && !iEditing && iMode == EModeBrowse)
					{ iPageInput = ETrue; TRAP_IGNORE(StartEditL()); }
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
		if (!iApi || iCur < 0 || !iTabs[iCur].iView)
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
		if (!iApi || !t.iView || !iApi->snapshot || t.iUrl.Length() == 0)
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
	void Txt(CWindowGc& gc, const TDesC& s, const TRect& r, CGraphicsContext::TTextAlign a, TRgb c, TInt aMargin = 0, const CFont* aFont = NULL) const
		{
		const CFont* f = aFont ? aFont : iEikonEnv->NormalFont();
		gc.UseFont(f);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(c);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		// text wider than the rectangle ends in "..." (never clipped mid-glyph, never off screen)
		TBuf<160> fit;
		fit.Copy(s.Left(Min(s.Length(), 160)));
		const TInt avail = r.Width() - 2 * aMargin - 2;
		if (f->TextWidthInPixels(fit) > avail)
			{
			while (fit.Length() > 0 && f->TextWidthInPixels(fit) + f->TextWidthInPixels(_L("...")) > avail)
				fit.SetLength(fit.Length() - 1);
			fit.Append(_L("..."));
			}
		gc.DrawText(fit, r, (r.Height() + f->AscentInPixels() - f->DescentInPixels()) / 2, a, aMargin);
		gc.DiscardFont();
		}
	// A neutral lettermark tile for a search engine: a rounded square in a brand-ish colour with the engine's initial.
	// Drawn by us -- the companies' logos are trademarks and are NOT shipped (browser-frontend.md, "Icons and trademarks").
	void Lettermark(CWindowGc& gc, const TRect& r, const TDesC& aName, unsigned aColor) const
		{
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(Rgb(aColor));
		gc.DrawRoundRect(r, TSize(r.Height() / 4, r.Height() / 4));
		TBuf<2> ini;
		if (aName.Length())
			ini.Append(aName[0]);
		Txt(gc, ini, r, CGraphicsContext::ECenter, KRgbWhite);
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
		DrawField(gc, l, tk);
		DrawGlyph(gc, EBtnNav, ButtonRect(EBtnNav, l), tk);
		DrawGlyph(gc, EBtnMenu, ButtonRect(EBtnMenu, l), tk);
		if (t && t->iLoading && t->iUrl.Length())
			Fill(gc, TRect(l.iProgress.iX, l.iProgress.iY, l.iProgress.iX + l.iProgress.iW * Max(t->iProgress, 4) / 100,
				l.iProgress.iY + l.iProgress.iH), Rgb(tk.iAccent));
		if (iMode == EModeGrid) DrawGrid(gc);
		if (iMode == EModeNav) DrawNav(gc, tk);
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
		if (!iEngines || iEngines->Count() == 0 || iEngineFailed)
			{
			Txt(gc, iEngineFailed ? _L("The browser engine could not start") : _L("No browser engine is installed"), b, CGraphicsContext::ECenter, Rgb(tk.iDim));
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
		if (iEditing)                        // the editor draws the text; we draw the clear (x) button
			{
			const TPoint c = ClearRect(l).Center();
			gc.SetPenStyle(CGraphicsContext::ESolidPen);
			gc.SetPenSize(TSize(2, 2));
			gc.SetPenColor(Rgb(tk.iDim));
			gc.DrawLine(TPoint(c.iX - 6, c.iY - 6), TPoint(c.iX + 6, c.iY + 6));
			gc.DrawLine(TPoint(c.iX - 6, c.iY + 6), TPoint(c.iX + 6, c.iY - 6));
			gc.SetPenSize(TSize(1, 1));
			return;
			}
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
		Txt(gc, shown, text, CGraphicsContext::ELeft, Rgb(hint ? tk.iDim : tk.iInk), 0, iSmall);
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
		case EBtnNav:                                   // three bars, and the tab count as a small badge when there are several
			for (TInt k = -1; k <= 1; ++k)
				gc.DrawLine(TPoint(c.iX - 9, c.iY + k * 7), TPoint(c.iX + 9, c.iY + k * 7));
			if (iTabs.Count() > 1)
				{
				const TRect b(c.iX + 3, c.iY - 18, c.iX + 21, c.iY);
				gc.SetPenStyle(CGraphicsContext::ENullPen);
				gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
				gc.SetBrushColor(Rgb(tk.iAccent));
				gc.DrawEllipse(b);
				TBuf<4> n;
				n.Num(iTabs.Count());
				Txt(gc, n, b, CGraphicsContext::ECenter, KRgbWhite);
				}
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
		case ESheetEngine:
			aOut = _L("Engine: ");
			if (iEngines && iEngines->Count() > 0)
				{
				aOut.Append(iEngines->Info(iEngines->Default()).iName);
				if (iApi && iEngines->LoadedIndex() != iEngines->Default())
					aOut.Append(_L(" (next start)"));
				}
			else
				aOut.Append(_L("none"));
			break;
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
			TInt margin = 16;
			if (i == ESheetSearch)           // the engine row carries its lettermark
				{
				TBuf<48> en;
				en.Copy(iSet->iSearchName);
				unsigned col = 0x7A7F88;
				for (TInt k = 0; k < KSearchCount; ++k)
					if (iSet->iSearchName == P8(KSearches[k].iName))
						col = KSearches[k].iColor;
				Lettermark(gc, TRect(r.iTl.iX + 12, r.iTl.iY + 10, r.iTl.iX + 40, r.iTl.iY + 38), en, col);
				margin = 52;
				}
			Txt(gc, lab, r, CGraphicsContext::ELeft, Rgb(tk.iInk), margin);
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
			Lettermark(gc, TRect(r.iTl.iX + 16, r.iTl.iY + 6, r.iTl.iX + 48, r.iTl.iY + 38), n, i < KSearchCount ? KSearches[i].iColor : 0x7A7F88);
			Txt(gc, n, r, CGraphicsContext::ELeft, Rgb(tk.iInk), 64);
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
	// pinch: two fingers on the page; the zoom is applied once, on release (each zoom is a full reflow in the engine)
	TBool PinchPointerL(const TPointerEvent& aE)
		{
		const TInt n = aE.IsAdvancedPointerEvent() ? aE.AdvancedPointerEvent()->PointerNumber() : 0;
		if (n > 1)
			return ETrue;
		if (n == 1 && aE.iType == TPointerEvent::EButton1Down && iDragging && iApi && iCur >= 0 && iTabs[iCur].iView)
			{
			iPinch = ETrue;
			iMoved = ETrue;                    // no tap on release
			iP1 = aE.iPosition;
			iPinchD0 = Max(Dist(iP0, iP1), 1);
			PheViewState st;
			iApi->get_state(iTabs[iCur].iView, &st);
			iPinchZ0 = st.zoom_permille > 0 ? st.zoom_permille : 1000;
			return ETrue;
			}
		if (n == 0)
			iP0 = aE.iPosition;
		else
			iP1 = aE.iPosition;
		if (!iPinch)
			return n != 0;
		if (aE.iType == TPointerEvent::EButton1Up)
			{
			iPinch = EFalse;
			TInt z = (TInt)((TInt64)iPinchZ0 * Dist(iP0, iP1) / iPinchD0);
			z = Max(250, Min(z, 4000));
			const TPbLayout l = Layout();
			if (iApi && iCur >= 0 && iTabs[iCur].iView && Abs(z - iPinchZ0) > 30)
				iApi->zoom(iTabs[iCur].iView, z, (iP0.iX + iP1.iX) / 2 - l.iPage.iX, (iP0.iY + iP1.iY) / 2 - l.iPage.iY);
			iDragging = EFalse;
			}
		return ETrue;                          // no scrolling while two fingers are down
		}
	static TInt Dist(const TPoint& a, const TPoint& b)
		{
		TReal r;
		const TReal dx = a.iX - b.iX, dy = a.iY - b.iY;
		Math::Sqrt(r, dx * dx + dy * dy);
		return (TInt)r;
		}

	void HandlePointerEventL(const TPointerEvent& aE)
		{
		if (!iEditing && iMode == EModeBrowse && PinchPointerL(aE))
			return;
		if (aE.iType == TPointerEvent::EButton1Down)
			{ iDownPos = aE.iPosition; iDragY = aE.iPosition.iY; iDragX = aE.iPosition.iX; iMoved = EFalse; }
		else if (aE.iType == TPointerEvent::EDrag && (Abs(aE.iPosition.iY - iDownPos.iY) > 6 || Abs(aE.iPosition.iX - iDownPos.iX) > 6))
			iMoved = ETrue;
		if (!iEditing && aE.iType == TPointerEvent::EButton1Down && iMode == EModeBrowse && iCur >= 0 && FieldRect(Layout()).Contains(aE.iPosition))
			{                                   // the touch that starts editing is also the first touch ON the editor (opens the keyboard)
			AskUrlL();
			if (iEditing)
				{                               // the bar moved to the top: deliver the touch to the editor where it now is
				TPointerEvent pe = aE;
				pe.iPosition = EditRect(Layout()).Center();
				iStartTouch = ETrue;
				CCoeControl::HandlePointerEventL(pe);
				}
			return;
			}
		if (iEditing)
			{
			const TPbLayout el = Layout();
			if (iStartTouch)                    // the Up of the touch that started the editing
				{
				if (aE.iType == TPointerEvent::EButton1Up)
					{
					iStartTouch = EFalse;
					TPointerEvent pe = aE;
					pe.iPosition = EditRect(el).Center();
					CCoeControl::HandlePointerEventL(pe);
					}
				return;
				}
			if (EditRect(el).Contains(aE.iPosition))
				{ CCoeControl::HandlePointerEventL(aE); return; }      // caret placement, selection: the editor's
			if (aE.iType == TPointerEvent::EButton1Up)
				{
				if (ClearRect(el).Contains(aE.iPosition))
					{ iEdit->SetTextL(&KNullDesC); iEdit->SetFocus(ETrue); DrawDeferred(); }
				else if (!FieldRect(el).Contains(aE.iPosition))
					EndEditL(EFalse);                                   // tap outside = cancel
				}
			return;
			}
		switch (iMode)
			{
		case EModeGrid: GridPointer(aE); return;
		case EModeNav: NavPointer(aE); return;
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
				const TInt dy = iDragY - aE.iPosition.iY, dx = iDragX - aE.iPosition.iX;
				if (dy || dx)                      // both axes: a page wider than the screen pans sideways
					{
					iApi->scroll(iTabs[iCur].iView, dx, dy);
					iDragY = aE.iPosition.iY;
					iDragX = aE.iPosition.iX;
					}
				}
			break;
		case TPointerEvent::EButton1Up:
			{
			const TBool wasPage = iDragging;
			iDragging = EFalse;
			if (wasPage)
				{
				if ((!iEngines || iEngines->Count() == 0) && StoreButtonRect(R(l.iPage)).Contains(aE.iPosition))
					iEikonEnv->EikAppUi()->HandleCommandL(EPheBrCmdGetEngine);
				else if (iApi && iCur >= 0 && iTabs[iCur].iView && iTabs[iCur].iUrl.Length() && !iMoved)
					{                              // a tap on the page: the engine gets a press + release (page coordinates)
					const TInt x = aE.iPosition.iX - l.iPage.iX, y = aE.iPosition.iY - l.iPage.iY;
					iApi->pointer(iTabs[iCur].iView, PHE_PTR_DOWN, x, y);
					iApi->pointer(iTabs[iCur].iView, PHE_PTR_UP, x, y);
				}
				break;
				}
			if (FieldRect(l).Contains(aE.iPosition))
				{ AskUrlL(); break; }
			if (ButtonRect(EBtnNav, l).Contains(aE.iPosition))
				Press(EBtnNav);
			else if (ButtonRect(EBtnMenu, l).Contains(aE.iPosition))
				Press(EBtnMenu);
			}
			break;
		default:
			break;
			}
		}
	// the nav popover: a tap on an item acts and closes it; the nav button or anywhere else just closes it
	void NavPointer(const TPointerEvent& aE)
		{
		if (aE.iType != TPointerEvent::EButton1Up)
			return;
		iMode = EModeBrowse;
		for (TInt i = EBtnBack; i <= EBtnTabs; ++i)
			if (NavItemRect(i).Contains(aE.iPosition))
				Press(i);
		DrawDeferred();
		}
	void DrawNav(CWindowGc& gc, const TPbTokens& tk) const
		{
		const TRect r = NavRect();
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(Rgb(tk.iField));
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(Rgb(tk.iBar));
		gc.DrawRoundRect(r, TSize(10, 10));
		for (TInt i = EBtnBack; i <= EBtnTabs; ++i)
			DrawGlyph(gc, i, NavItemRect(i), tk);
		}
	void Press(TInt aBtn)
		{
		if (aBtn == EBtnNav)
			{ iMode = EModeNav; DrawDeferred(); return; }
		if (!iApi || iCur < 0 || !iTabs[iCur].iView)
			{ if (aBtn == EBtnMenu) OpenSheet(); else if (aBtn == EBtnTabs && iCur >= 0) { CaptureThumb(iCur); iGridScroll = 0; iMode = EModeGrid; DrawDeferred(); } return; }
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
		case ESheetEngine: if (iEngines) iEngines->Cycle(); DrawDeferred(); break;      // the default engine, persisted by uid; loaded engines stay until restart
		case ESheetStore: iEikonEnv->EikAppUi()->HandleCommandL(EPheBrCmdGetEngine); break;
		case ESheetExit: iEikonEnv->EikAppUi()->HandleCommandL(EEikCmdExit); break;
			}
		DrawDeferred();
		}
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
		{
		// no CBA in the full-screen UI: the hardware / touch soft keys (Options, Back) are ours
		if (aType == EEventKey || aType == EEventKeyUp || aType == EEventKeyDown)
			{
			const TBool opt = aKey.iCode == EKeyDevice0 || aKey.iScanCode == EStdKeyDevice0;
			const TBool back = aKey.iCode == EKeyDevice1 || aKey.iScanCode == EStdKeyDevice1;
			if (opt || back)
				{
				if (aType == EEventKey)
					{
					if (opt)
						{ if (iEditing) EndEditL(EFalse); else if (iMode == EModeSheet || iMode == EModeNav) { iMode = EModeBrowse; DrawDeferred(); } else OpenSheet(); }
					else
						iEikonEnv->EikAppUi()->HandleCommandL(EAknSoftkeyBack);
					}
				return EKeyWasConsumed;
				}
			}
		if (iEditing)
			{
			if (aType == EEventKey && (aKey.iCode == EKeyEnter || aKey.iCode == EKeyDevice3 || aKey.iCode == EKeyOK))
				{ EndEditL(ETrue); return EKeyWasConsumed; }              // Enter = go
			if (aType == EEventKey && aKey.iCode == EKeyEscape)
				{ EndEditL(EFalse); return EKeyWasConsumed; }
			return iEdit->OfferKeyEventL(aKey, aType);
			}
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
	CPbWake* iWake = NULL;
	CPbEngines* iEngines = NULL;
	TBool iEngineFailed = EFalse;
	TBuf<400> iQueue;
	CFbsBitmap* iPageBmp = NULL;
	CEikEdwin* iEdit = NULL;
	TBool iEditing = EFalse;
	TBool iPageInput = EFalse;               // the editor is typing into a page field (PHE_EV_FOCUS), not the address
	TBool iStartTouch = EFalse;
	CFont* iSmall = NULL;                    // the address-field font
	TInt iDragY = 0, iDragX = 0, iGridScroll = 0;
	TBool iPinch = EFalse;
	TPoint iP0, iP1;
	TInt iPinchD0 = 1, iPinchZ0 = 1000;
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
		// full screen: no status pane, no CBA -- our own bar has back / menu, and the hardware Options / Back keys are
		// handled by the container (OfferKeyEventL)
		if (StatusPane())
			StatusPane()->MakeVisible(EFalse);
		if (Cba())
			Cba()->MakeVisible(EFalse);
		PbTrace("PH: [phebrowser] AppUi base ok");
		iSet.Load(iEikonEnv->FsSession());
		PbTrace("PH: [phebrowser] settings ok, theme=%d bar=%d", iSet.iTheme, (int)iSet.iBar);
		iView = new (ELeave) CPbContainer;
		iView->ConstructL(ApplicationRect(), &iSet);
		PbTrace("PH: [phebrowser] container ok");
#ifdef PHEBROWSER_STKDUMP
		PbStartStackDump();
#endif
		AddToStackL(iView);
		}
	~CPheBrowserAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		}
	// the apparc command line: a URL as the document name (phestart phebrowser <url>) or as the tail; none = the start files
	TBool ProcessCommandParametersL(TApaCommand /*aCommand*/, TFileName& aDocumentName, const TDesC8& aTail)
		{
		TBuf<200> url;
		url.Copy(aDocumentName.Left(200));
		url.Trim();
		if (url.Length() == 0)
			{
			url.Copy(aTail.Left(200));
			url.Trim();
			}
		if (url.Length() && iView)
			iView->OpenUrl(url);
		else if (iView)
			iView->LoadStartHook();
		return EFalse;      // not a document: the application has opened it
		}
	// already running: phestart sends the document (a URL) as the apparc open-file message
	void OpenFileL(const TDesC& aFileName)
		{
		TBuf<200> url;
		url.Copy(aFileName.Left(200));
		url.Trim();
		PbTrace("PH: [phebrowser] OpenFileL url len=%d", url.Length());
		if (url.Length() && iView)
			iView->OpenRunning(url);
		}
	void HandleResourceChangeL(TInt aType)       // rotation: stay full screen
		{
		CAknAppUi::HandleResourceChangeL(aType);
		if (aType == KEikDynamicLayoutVariantSwitch && iView)
			{
			if (StatusPane())
				StatusPane()->MakeVisible(EFalse);
			if (Cba())
				Cba()->MakeVisible(EFalse);
			iView->SetRect(ApplicationRect());
			}
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
#ifndef PHEBROWSER_NO_STORE      // stock phone firmware: its apgrfx.dll does not serve the ordinals of the 2010 import library (loader -5)
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
		static_cast<RHandleBase&>(ls).Close();   // not RApaLsSession::Close: its 2010 import ordinal (323) is not served by stock phone firmware (loader -5)
#endif
		}
	CPbSettings iSet;
	CPbContainer* iView = NULL;
	};

class CPheBrowserDocument : public CAknDocument
	{
public:
	CPheBrowserDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { PbTrace("PH: [phebrowser] CreateAppUiL"); return new (ELeave) CPheBrowserAppUi; }
	};

class CPheBrowserApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KPheBrowserAppUid; }
	CApaDocument* CreateDocumentL() { PbTrace("PH: [phebrowser] CreateDocumentL"); return new (ELeave) CPheBrowserDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	PbTrace("PH: [phebrowser] NewApplication");
	return new CPheBrowserApplication;
	}

GLDEF_C TInt E32Main()
	{
	PbTrace("PH: [phebrowser] E32Main");
	TInt r = EikStart::RunApplication(NewApplication);
	PbTrace("PH: [phebrowser] RunApplication returned %d", r);
	return r;
	}
