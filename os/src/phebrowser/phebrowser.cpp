// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phebrowser.cpp -- OURS: the browser frontend (prototype). A custom-drawn CONE UI -- one window, flat bars drawn with
// CWindowGc, no Qt -- over any engine that implements the PheWebEngineApi table (phorepoc/phewebengine.h). This prototype:
//   * the bar (address row + action row) at the TOP or the BOTTOM, switched live from the menu and kept in settings.ini;
//   * a tab model (new / close / next tab), each tab = one engine view;
//   * the built-in dummy engine (pb_dummy.cpp) that draws a placeholder page, so it runs without WebKit or NetSurf;
//   * touch: tap the buttons, tap the URL field (address query), drag the page to scroll; keys: up/down scroll.
// Not here yet (docs/phorepoc/browser-frontend.md section 7): the tab grid, the overflow sheet, bookmarks/history/downloads, the
// find bar, ECom discovery of downloadable engines, bar auto-hide, pinch zoom, skin colours.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <aknquerydialog.h>
#include <aknnotewrappers.h>
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
#include <phebrowser.rsg>
#include "phebrowser.hrh"
#include "pb_layout.h"
#include "pb_engines.h"

const TUid KPheBrowserAppUid = { 0xE7016B20 };
const TUid KPheStoreUid = { 0xE70157FA };

static const TInt KMaxTabs = 8;
static const TUint16 KFindDemoText[] = { 'x' };
static const TRgb KBarBg(0xF4, 0xF5, 0xF7), KFieldBg(0xE3, 0xE6, 0xEB), KInk(0x20, 0x22, 0x26), KInkDim(0xA0, 0xA4, 0xAA),
	KAccent(0x3D, 0x7B, 0xF0), KGood(0x1E, 0x8E, 0x3E), KWarn(0xD9, 0x3A, 0x25);

// ---- settings (a tiny key=value file; CenRep later: one class to swap) -----------------------------------------------------
class CPbSettings
	{
public:
	TPbBarPos iBar;
	CPbSettings() : iBar(EPbBarTop) {}
	void Load(RFs& aFs)
		{
		TFileName f;
		Path(aFs, f);
		RFile file;
		if (file.Open(aFs, f, EFileRead | EFileShareReadersOnly) != KErrNone)
			return;
		TBuf8<128> b;
		file.Read(b);
		file.Close();
		if (b.Find(_L8("barpos=bottom")) != KErrNotFound)
			iBar = EPbBarBottom;
		}
	void Save(RFs& aFs) const
		{
		TFileName f;
		aFs.CreatePrivatePath(EDriveC);
		Path(aFs, f);
		RFile file;
		if (file.Replace(aFs, f, EFileWrite) != KErrNone)
			return;
		file.Write(iBar == EPbBarBottom ? _L8("barpos=bottom\n") : _L8("barpos=top\n"));
		file.Close();
		}
private:
	static void Path(RFs& aFs, TDes& aOut)
		{
		aFs.PrivatePath(aOut);
		aOut.Insert(0, _L("C:"));
		aOut.Append(_L("settings.ini"));
		}
	};

// ---- the tab model (no UI types) ----------------------------------------------------------------------------------------
struct TPbTab
	{
	PheView iView;
	TBuf<256> iUrl;
	TBuf<64> iTitle;
	TInt iProgress;
	TInt iSecurity;
	TBool iBack, iFwd, iLoading;
	};

// ---- the container: the whole UI is this one control ------------------------------------------------------------------------
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
		delete iCallBack;
		delete iPageBmp;
		iTabs.Close();
		}

	void ConstructL(const TRect& aRect, CPbSettings* aSettings)
		{
		iSet = aSettings;
		CreateWindowL();
		EnableDragEvents();
		iCallBack = new (ELeave) CAsyncCallBack(TCallBack(Pump, this), CActive::EPriorityStandard);
		SetRect(aRect);
		ActivateL();
#ifdef PHEBROWSER_DUMMY
		iApi = PheDummyGetApi(PHE_WEBENGINE_VERSION);
#endif
		if (iApi && iApi->init(Notify, this) == 0)
			NewTabL(_L("https://example.org/"));
		}

	// commands from the app UI
	void NewTabL(const TDesC& aUrl)
		{
		if (!iApi || iTabs.Count() >= KMaxTabs)
			return;
		TPbTab t;
		t.iView = iApi->new_view_id();
		t.iUrl = aUrl;
		t.iTitle.Zero();
		t.iProgress = 0; t.iSecurity = 0; t.iBack = t.iFwd = EFalse; t.iLoading = ETrue;
		if (iCur >= 0)
			iApi->view_visible(iTabs[iCur].iView, 0);
		iTabs.AppendL(t);
		iCur = iTabs.Count() - 1;
		const TPbLayout l = Layout();
		iApi->view_create(t.iView, l.iPage.iW, l.iPage.iH, 0, NULL, 0);
		iApi->load(t.iView, t.iUrl.Ptr(), t.iUrl.Length());
		PageChanged();
		}
	void CloseTab()
		{
		if (!iApi || iCur < 0)
			return;
		iApi->view_destroy(iTabs[iCur].iView);
		iTabs.Remove(iCur);
		iCur = iTabs.Count() ? Min(iCur, iTabs.Count() - 1) : -1;
		if (iCur >= 0)
			iApi->view_visible(iTabs[iCur].iView, 1);
		else
			TRAP_IGNORE(NewTabL(_L("about:blank")));
		PageChanged();
		}
	void NextTab()
		{
		if (iTabs.Count() < 2)
			return;
		iApi->view_visible(iTabs[iCur].iView, 0);
		iCur = (iCur + 1) % iTabs.Count();
		iApi->view_visible(iTabs[iCur].iView, 1);
		PageChanged();
		}
	void ToggleBar()
		{
		iSet->iBar = (iSet->iBar == EPbBarTop) ? EPbBarBottom : EPbBarTop;
		iSet->Save(iEikonEnv->FsSession());
		Relayout();
		}
	TBool GoBackIfPossible()
		{
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
		TBuf<256> u(iTabs[iCur].iUrl);
		CAknTextQueryDialog* d = CAknTextQueryDialog::NewL(u);
		if (d->ExecuteLD(R_PHEBROWSER_URL_QUERY))
			{
			if (u.Locate(':') == KErrNotFound)            // "example.org" -> https://; a search would go here
				u.Insert(0, _L("https://"));
			iTabs[iCur].iUrl = u;
			iApi->load(iTabs[iCur].iView, u.Ptr(), u.Length());
			}
		}
	TInt TabCount() const { return iTabs.Count(); }
	TBool BarAtBottom() const { return iSet->iBar == EPbBarBottom; }

private:
	enum { EBtnBack, EBtnFwd, EBtnReload, EBtnTabs, EBtnMenu, EBtnCount };

	TPbLayout Layout() const
		{
		const TSize s = Size();
		return PbLayout(s.iWidth, s.iHeight, iSet->iBar, s.iWidth > s.iHeight ? 48 : 56, 1);
		}
	static TRect R(const TPbRect& r) { return TRect(r.iX, r.iY, r.iX + r.iW, r.iY + r.iH); }

	// the five buttons: in the action row (portrait) or around the field in the one folded row (landscape)
	TRect ButtonRect(TInt aIx, const TPbLayout& l) const
		{
		if (l.iActions.iW > 0)
			{
			const TInt w = l.iActions.iW / EBtnCount;
			return TRect(l.iActions.iX + aIx * w, l.iActions.iY, l.iActions.iX + (aIx + 1) * w, l.iActions.iY + l.iActions.iH);
			}
		const TInt h = l.iAddress.iH;                      // landscape: three on the left, two on the right
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

	void SizeChanged()
		{
		Relayout();
		}
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
	void PageChanged() { CopyPixels(); DrawDeferred(); }

	// engine -> UI plumbing ---------------------------------------------------------------------------------------------------
	static void Notify(void* aCtx, PheView) { static_cast<CPbContainer*>(aCtx)->iCallBack->CallBack(); }
	static TInt Pump(TAny* aSelf) { static_cast<CPbContainer*>(aSelf)->DrainEvents(); return 0; }

	TInt IndexOf(PheView aV) const
		{
		for (TInt i = 0; i < iTabs.Count(); ++i)
			if (iTabs[i].iView == aV)
				return i;
		return KErrNotFound;
		}
	void FetchStr(TInt aTab, TInt aWhich, TDes& aOut)
		{
		TUint16 tmp[256];
		const TInt n = iApi->get_str(iTabs[aTab].iView, aWhich, tmp, 256);
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
				FetchStr(t, PHE_STR_URL, tab.iUrl);
				break;
			case PHE_EV_TITLE:
				FetchStr(t, PHE_STR_TITLE, tab.iTitle);
				break;
			case PHE_EV_REPAINT:
				if (t == iCur) page = ETrue;
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

	// drawing -------------------------------------------------------------------------------------------------------------------
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		const TPbLayout l = Layout();
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		// page
		gc.SetBrushColor(KRgbWhite);
		gc.DrawRect(R(l.iPage));
		if (iPageBmp && iCur >= 0)
			gc.BitBlt(TPoint(l.iPage.iX, l.iPage.iY), iPageBmp);
		else
			DrawNoEngine(gc, R(l.iPage));
		// bars
		gc.SetBrushColor(KBarBg);
		gc.DrawRect(R(l.iAddress));
		if (l.iActions.iW > 0)
			gc.DrawRect(R(l.iActions));
		DrawField(gc, l);
		for (TInt b = 0; b < EBtnCount; ++b)
			DrawButton(gc, b, ButtonRect(b, l));
		// progress line
		const TPbTab* t = iCur >= 0 ? &iTabs[iCur] : NULL;
		if (t && t->iLoading)
			{
			gc.SetBrushColor(KAccent);
			gc.DrawRect(TRect(l.iProgress.iX, l.iProgress.iY, l.iProgress.iX + l.iProgress.iW * Max(t->iProgress, 4) / 100,
				l.iProgress.iY + l.iProgress.iH));
			}
		}

	void DrawNoEngine(CWindowGc& gc, const TRect& aR) const
		{
		const CFont* f = iEikonEnv->NormalFont();
		gc.UseFont(f);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(KInk);
		gc.DrawText(_L("No browser engine installed"), aR, aR.Height() / 2, CGraphicsContext::ECenter);
		gc.DiscardFont();
		}

	void DrawField(CWindowGc& gc, const TPbLayout& l) const
		{
		const TRect f = FieldRect(l);
		gc.SetBrushColor(KFieldBg);
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.DrawRoundRect(f, TSize(f.Height() / 2, f.Height() / 2));
		const CFont* font = iEikonEnv->NormalFont();
		gc.UseFont(font);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(KInk);
		gc.SetBrushStyle(CGraphicsContext::ENullBrush);
		TRect text = f;
		text.iTl.iX += f.Height() + 2;
		text.iBr.iX -= f.Height() / 2;
		TBuf<256> shown;
		if (iCur >= 0)
			{
			shown = iTabs[iCur].iUrl;
			TInt p = shown.Find(_L("://"));                // show "host/path" like the mobile browsers
			if (p >= 0)
				shown.Delete(0, p + 3);
			}
		else
			shown = _L("Search or enter address");
		gc.DrawText(shown, text, (text.Height() + font->AscentInPixels()) / 2 - 1, CGraphicsContext::ELeft);
		gc.DiscardFont();
		// the security glyph at the left end of the field
		const TInt sec = iCur >= 0 ? iTabs[iCur].iSecurity : 0;
		const TPoint c(f.iTl.iX + f.Height() / 2, f.Center().iY);
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenSize(TSize(2, 2));
		gc.SetPenColor(sec == PHE_SEC_SECURE ? KGood : (sec == PHE_SEC_NONE ? KInkDim : KWarn));
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(sec == PHE_SEC_SECURE ? KGood : (sec == PHE_SEC_NONE ? KFieldBg : KWarn));
		if (sec == PHE_SEC_NONE)
			gc.DrawEllipse(TRect(c.iX - 7, c.iY - 7, c.iX + 7, c.iY + 7));       // "i" circle (plain http)
		else
			{
			gc.DrawRect(TRect(c.iX - 6, c.iY - 1, c.iX + 6, c.iY + 7));          // padlock body
			gc.SetBrushStyle(CGraphicsContext::ENullBrush);
			gc.DrawArc(TRect(c.iX - 4, c.iY - 8, c.iX + 4, c.iY + 2), TPoint(c.iX - 4, c.iY - 3), TPoint(c.iX + 4, c.iY - 3));
			}
		gc.SetPenSize(TSize(1, 1));
		}

	void DrawButton(CWindowGc& gc, TInt aIx, const TRect& aR) const
		{
		const TPoint c = aR.Center();
		const TPbTab* t = iCur >= 0 ? &iTabs[iCur] : NULL;
		TBool on = ETrue;
		if (aIx == EBtnBack) on = t && t->iBack;
		if (aIx == EBtnFwd) on = t && t->iFwd;
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenSize(TSize(2, 2));
		gc.SetPenColor(on ? KInk : KInkDim);
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
			if (t && t->iLoading)                                   // stop: an X
				{
				gc.DrawLine(TPoint(c.iX - 7, c.iY - 7), TPoint(c.iX + 7, c.iY + 7));
				gc.DrawLine(TPoint(c.iX - 7, c.iY + 7), TPoint(c.iX + 7, c.iY - 7));
				}
			else                                                    // reload: an open circle with a head
				{
				gc.DrawArc(TRect(c.iX - 8, c.iY - 8, c.iX + 8, c.iY + 8), TPoint(c.iX + 8, c.iY - 3), TPoint(c.iX + 8, c.iY + 3));
				gc.DrawLine(TPoint(c.iX + 8, c.iY - 7), TPoint(c.iX + 8, c.iY - 1));
				gc.DrawLine(TPoint(c.iX + 8, c.iY - 1), TPoint(c.iX + 2, c.iY - 3));
				}
			break;
		case EBtnTabs:
			{
			gc.DrawRoundRect(TRect(c.iX - 9, c.iY - 9, c.iX + 9, c.iY + 9), TSize(4, 4));
			gc.UseFont(iEikonEnv->NormalFont());
			gc.SetPenSize(TSize(1, 1));
			TBuf<4> n;
			n.Num(iTabs.Count());
			gc.DrawText(n, TRect(c.iX - 9, c.iY - 9, c.iX + 9, c.iY + 9), 14, CGraphicsContext::ECenter);
			gc.DiscardFont();
			}
			break;
		case EBtnMenu:
			gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
			gc.SetBrushColor(KInk);
			for (TInt k = -1; k <= 1; ++k)
				gc.DrawEllipse(TRect(c.iX - 2, c.iY + k * 7 - 2, c.iX + 2, c.iY + k * 7 + 2));
			break;
			}
		gc.SetPenSize(TSize(1, 1));
		}

	// input -----------------------------------------------------------------------------------------------------------------------
	void HandlePointerEventL(const TPointerEvent& aE)
		{
		const TPbLayout l = Layout();
		switch (aE.iType)
			{
		case TPointerEvent::EButton1Down:
			iDragY = aE.iPosition.iY;
			iDragging = R(l.iPage).Contains(aE.iPosition);
			iMoved = EFalse;
			break;
		case TPointerEvent::EDrag:
			if (iDragging && iCur >= 0)
				{
				const TInt dy = iDragY - aE.iPosition.iY;
				if (Abs(dy) > 0)
					{
					iMoved = ETrue;
					iApi->scroll(iTabs[iCur].iView, 0, dy);
					iDragY = aE.iPosition.iY;
					}
				}
			break;
		case TPointerEvent::EButton1Up:
			if (iDragging)
				{
				iDragging = EFalse;
				break;
				}
			if (!iApi || iCur < 0)
				break;
			if (FieldRect(l).Contains(aE.iPosition))
				{
				AskUrlL();
				break;
				}
			for (TInt b = 0; b < EBtnCount; ++b)
				if (ButtonRect(b, l).Contains(aE.iPosition))
					Press(b);
			break;
		default:
			break;
			}
		}
	void Press(TInt aBtn)
		{
		const PheView v = iTabs[iCur].iView;
		switch (aBtn)
			{
		case EBtnBack: iApi->cmd(v, PHE_CMD_BACK); break;
		case EBtnFwd: iApi->cmd(v, PHE_CMD_FORWARD); break;
		case EBtnReload: iApi->cmd(v, iTabs[iCur].iLoading ? PHE_CMD_STOP : PHE_CMD_RELOAD); break;
		case EBtnTabs: NextTab(); break;             // the tab grid replaces this
		case EBtnMenu: iEikonEnv->EikAppUi()->HandleCommandL(EPheBrCmdMenu); break;
			}
		}
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
		{
		if (aType != EEventKey || iCur < 0)
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
	CAsyncCallBack* iCallBack = NULL;
	CFbsBitmap* iPageBmp = NULL;
	TInt iDragY = 0;
	TBool iDragging = EFalse, iMoved = EFalse;
	};

// ---- application ------------------------------------------------------------------------------------------------------------
class CPheBrowserAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		BaseConstructL(EAknEnableSkin);
		iSet.Load(iEikonEnv->FsSession());
		iView = new (ELeave) CPbContainer;
		iView->ConstructL(ClientRect(), &iSet);
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
		case EPheBrCmdMenu: iEikonEnv->AppUiFactory()->MenuBar()->TryDisplayMenuBarL(); break;
		case EPheBrCmdNewTab: iView->NewTabL(_L("https://example.org/")); break;
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
	void StartStore()      // the "Get a browser engine" hand-off: launch the store (Get apps)
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
	CEikAppUi* CreateAppUiL() { return new (ELeave) CPheBrowserAppUi; }
	};

class CPheBrowserApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KPheBrowserAppUid; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CPheBrowserDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CPheBrowserApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
