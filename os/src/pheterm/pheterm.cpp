// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheterm.cpp -- OURS: Terminal, the on-screen face of the developer tty daemon (V1).
// An Avkon app registered in the launcher grid (= the Menu key's existing menu); its Options menu lists the ttys:
// New tty / Next tty / Close tty / Switch to tty 1..8. The screen is a CPheScreen fed from the tty's byte stream
// (phettyd scrollback, read by sequence number), keys go to the tty's key queue (the shell's console reads them).
// Closing the app leaves the ttys and their shells running in phettyd.
#include <e32debug.h>
#include <eikstart.h>
#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <eikenv.h>
#include <eikmenup.h>
#include <w32std.h>
#include <avkon.hrh>
#include <pheterm.rsg>
#include "pheterm.hrh"
#include "phetty.h"
#include "pheansi.h"

const TUid KUidPheTerm = { 0xE7015900 };

class CPheTermView;

struct TTtyView
	{
	TInt iId;
	CPheScreen* iScr;
	TUint32 iSeq;
	};

// the pending EPtRead on the current tty
class CPheReader : public CActive
	{
public:
	CPheReader(CPheTermView& aView, RPheTty& aTty) : CActive(EPriorityStandard), iView(aView), iTty(aTty) { CActiveScheduler::Add(this); }
	~CPheReader() { Cancel(); }
	void Start(TInt aId, TUint32 aFrom)
		{
		Cancel();
		iId = aId;
		iTty.Read(aId, iBuf, aFrom, ETrue, iStatus);
		SetActive();
		}
	TInt Id() const { return iId; }
private:
	void RunL();
	void DoCancel() { iTty.ReadCancel(); }
	CPheTermView& iView;
	RPheTty& iTty;
	TBuf8<2048> iBuf;
	TInt iId = 0;
	};

class CPheTermView : public CCoeControl
	{
public:
	~CPheTermView()
		{
		delete iReader;
		for (TInt i = 0; i < iViews.Count(); i++)
			delete iViews[i].iScr;
		iViews.Close();
		iTty.Close();
		if (iOwnFont)
			iCoeEnv->ScreenDevice()->ReleaseFont(iOwnFont);
		}
	void ConstructL(const TRect& aRect)
		{
		CreateWindowL();
		SetRect(aRect);
		// a small fixed-pitch face (the nearest the font store has); the eikon DenseFont is ~20 px here
		iFont = NULL;
		TFontSpec spec(_L("DejaVu Sans Mono"), 13);
		if (iCoeEnv->ScreenDevice()->GetNearestFontInPixels(iOwnFont, spec) == KErrNone)
			iFont = iOwnFont;
		else
			iFont = iEikonEnv->DenseFont();
		iCellW = iFont->TextWidthInPixels(_L("M"));
		if (iCellW < 4)
			iCellW = 6;
		iCellH = iFont->HeightInPixels() + 1;
		iCols = Max(10, aRect.Width() / iCellW);
		iRows = Max(4, aRect.Height() / iCellH - 1);       // the last line shows the tty status
		TInt r = iTty.Connect();
		RDebug::Printf("[pheterm] connect -> %d, grid %dx%d cell %dx%d", r, iCols, iRows, iCellW, iCellH);
		User::LeaveIfError(r);
		iReader = new (ELeave) CPheReader(*this, iTty);
		TUint8 ids[KPheTtyMax];
		TInt n = iTty.List(ids, KPheTtyMax);
		TInt first = n > 0 ? ids[0] : iTty.New(iCols, iRows);
		User::LeaveIfError(first);
		Switch(first);
		ActivateL();
		}
	// attach to a tty (creating its screen on first use) and start reading it
	void Switch(TInt aId)
		{
		TPheTtyInfo in;
		if (iTty.Info(aId, in) != KErrNone)
			return;
		TInt at = Find(aId);
		if (at < 0)
			{
			TTtyView v;
			v.iId = aId;
			v.iScr = CPheScreen::NewL(iCols, iRows);
			v.iSeq = in.iStartSeq;
			if (in.iCols != iCols || in.iRows != iRows)
				{
				// a tty created with another size (the boot-time tty is 80x24): resizing restarts its shell at the new size
				// (eshell reads the console size once); show only what the new shell writes
				iTty.Resize(aId, iCols, iRows);
				iTty.Info(aId, in);
				v.iSeq = in.iEndSeq;
				}
			iViews.Append(v);
			at = iViews.Count() - 1;
			}
		iCur = at;
		iReader->Start(aId, iViews[at].iSeq);
		RDebug::Printf("[pheterm] attached to tty %d", aId);
		DrawNow();
		}
	TInt Find(TInt aId) const
		{
		for (TInt i = 0; i < iViews.Count(); i++)
			if (iViews[i].iId == aId)
				return i;
		return -1;
		}
	TInt CurrentId() const { return iCur >= 0 && iCur < iViews.Count() ? iViews[iCur].iId : 0; }
	RPheTty& Tty() { return iTty; }
	TInt ListIds(TUint8* aIds) { return iTty.List(aIds, KPheTtyMax); }
	void NewTty()
		{
		TInt id = iTty.New(iCols, iRows);
		if (id > 0)
			Switch(id);
		}
	void NextTty()
		{
		TUint8 ids[KPheTtyMax];
		TInt n = iTty.List(ids, KPheTtyMax);
		if (n < 1)
			return;
		TInt cur = CurrentId(), pick = ids[0];
		for (TInt i = 0; i < n; i++)
			if (ids[i] > cur) { pick = ids[i]; break; }
		Switch(pick);
		}
	void CloseTty()
		{
		TInt id = CurrentId();
		if (!id)
			return;
		iReader->Cancel();
		iTty.Kill(id);
		TInt at = Find(id);
		if (at >= 0)
			{
			delete iViews[at].iScr;
			iViews.Remove(at);
			}
		iCur = -1;
		TUint8 ids[KPheTtyMax];
		TInt n = iTty.List(ids, KPheTtyMax);
		if (n > 0)
			Switch(ids[0]);
		else
			Switch(iTty.New(iCols, iRows));
		}
	// the reader got data (aStatus = EPtRead result)
	void Data(TInt aId, TInt aStatus, const TDesC8& aBuf)
		{
		TInt at = Find(aId);
		if (aStatus >= 4 && at >= 0)
			{
			TUint32 first = aBuf[0] | (aBuf[1] << 8) | (aBuf[2] << 16) | ((TUint32)aBuf[3] << 24);
			TTtyView& v = iViews[at];
			if (first != v.iSeq)
				v.iScr->Clear();        // scrollback overran what we had not read
			v.iScr->Feed(aBuf.Ptr() + 4, aStatus - 4);
			v.iSeq = first + (aStatus - 4);
			if (at == iCur)
				DrawNow();
			iReader->Start(aId, v.iSeq);
			}
		else if (aStatus == KErrNotFound)
			{
			// the tty went away under us (closed elsewhere): fall back to a live one
			if (at >= 0)
				{
				delete iViews[at].iScr;
				iViews.Remove(at);
				}
			iCur = -1;
			TUint8 ids[KPheTtyMax];
			TInt n = iTty.List(ids, KPheTtyMax);
			if (n > 0)
				Switch(ids[0]);
			}
		else if (aStatus < 0 && aStatus != KErrCancel)
			RDebug::Printf("[pheterm] read error %d", aStatus);
		}
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
		{
		if (aType != EEventKey || !CurrentId())
			return EKeyWasNotConsumed;
		TUint c = aKey.iCode;
		TBool special = c == EKeyLeftArrow || c == EKeyRightArrow || c == EKeyUpArrow || c == EKeyDownArrow ||
		                c == EKeyHome || c == EKeyEnd || c == EKeyPageUp || c == EKeyPageDown || c == EKeyDelete || c == EKeyInsert;
		if (c >= 0xF700 && !special)
			return EKeyWasNotConsumed;           // softkeys, Menu, call keys: the framework's
		iTty.Key(CurrentId(), c, aKey.iModifiers);
		return EKeyWasConsumed;
		}
private:
	void Draw(const TRect&) const
		{
		CWindowGc& gc = SystemGc();
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(TRgb(0, 0, 0));
		gc.SetPenStyle(CGraphicsContext::ENullPen);
		gc.DrawRect(Rect());
		gc.UseFont(iFont);
		const TInt base = iFont->AscentInPixels();
		const TInt x0 = Rect().iTl.iX, y0 = Rect().iTl.iY;
		if (iCur >= 0 && iCur < iViews.Count())
			{
			CPheScreen& s = *iViews[iCur].iScr;
			for (TInt y = 0; y < s.Rows(); y++)
				for (TInt x = 0; x < s.Cols(); x++)
					{
					TUint16 ch = s.Ch(x, y);
					TUint8 at = s.Attr(x, y);
					TBool cursor = s.CursorOn() && x == s.CurX() && y == s.CurY();
					TBool inv = (at & KPheAttrInverse) != 0;
					if (cursor)
						inv = !inv;
					TRect cell(TPoint(x0 + x * iCellW, y0 + y * iCellH), TSize(iCellW, iCellH));
					if (inv)
						{
						gc.SetBrushColor(TRgb(0xc0, 0xc0, 0xc0));
						gc.DrawRect(cell);
						}
					if (ch != ' ')
						{
						gc.SetPenStyle(CGraphicsContext::ESolidPen);
						gc.SetPenColor(inv ? TRgb(0, 0, 0) : ((at & KPheAttrBold) ? TRgb(0xff, 0xff, 0xff) : TRgb(0x90, 0xe0, 0x90)));
						TBuf<1> one;
						one.Append(ch);
						gc.DrawText(one, TPoint(cell.iTl.iX, cell.iTl.iY + base));
						gc.SetPenStyle(CGraphicsContext::ENullPen);
						}
					}
			}
		// status line: which tty, and how many exist
		TBuf<48> st;
		st.Format(_L("tty %d  (Options: new / next / switch)"), CurrentId());
		gc.SetPenStyle(CGraphicsContext::ESolidPen);
		gc.SetPenColor(TRgb(0x80, 0xa0, 0xff));
		gc.DrawText(st, TPoint(x0 + 2, y0 + (iRows) * iCellH + base));
		gc.DiscardFont();
		}
private:
	RPheTty iTty;
	CPheReader* iReader = NULL;
	RArray<TTtyView> iViews;
	TInt iCur = -1;
	TInt iCols = 0, iRows = 0, iCellW = 6, iCellH = 10;
	const CFont* iFont = NULL;
	CFont* iOwnFont = NULL;
	};

void CPheReader::RunL()
	{
	TInt st = iStatus.Int();
	iView.Data(iId, st, iBuf);
	}

class CPheTermAppUi : public CAknAppUi
	{
public:
	void ConstructL()
		{
		RDebug::Printf("[pheterm] AppUi::ConstructL");
		BaseConstructL();
		iView = new (ELeave) CPheTermView;
		iView->ConstructL(ClientRect());
		AddToStackL(iView);
		RDebug::Printf("[pheterm] view up");
		}
	~CPheTermAppUi()
		{
		if (iView)
			RemoveFromStack(iView);
		delete iView;
		}
	void DynInitMenuPaneL(TInt aResourceId, CEikMenuPane* aPane)
		{
		if (aResourceId != R_PHETERM_MENU)
			return;
		TUint8 ids[KPheTtyMax];
		TInt n = iView->ListIds(ids);
		for (TInt i = 1; i <= KPheTtyMax; i++)
			{
			TBool live = EFalse;
			for (TInt k = 0; k < n; k++)
				if (ids[k] == i)
					live = ETrue;
			aPane->SetItemDimmed(EPheCmdTty1 + i - 1, !live || i == iView->CurrentId());
			}
		aPane->SetItemDimmed(EPheCmdNew, n >= KPheTtyMax);
		aPane->SetItemDimmed(EPheCmdNext, n < 2);
		}
	void HandleCommandL(TInt aCommand)
		{
		switch (aCommand)
			{
		case EPheCmdNew: iView->NewTty(); break;
		case EPheCmdNext: iView->NextTty(); break;
		case EPheCmdClose: iView->CloseTty(); break;
		case EAknSoftkeyExit:
		case EEikCmdExit:
			Exit();
			break;
		default:
			if (aCommand >= EPheCmdTty1 && aCommand <= EPheCmdTty8)
				iView->Switch(aCommand - EPheCmdTty1 + 1);
			break;
			}
		}
private:
	CPheTermView* iView = NULL;
	};

class CPheTermDocument : public CAknDocument
	{
public:
	CPheTermDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
	CEikAppUi* CreateAppUiL() { return new (ELeave) CPheTermAppUi; }
	};

class CPheTermApplication : public CAknApplication
	{
public:
	TUid AppDllUid() const { return KUidPheTerm; }
	CApaDocument* CreateDocumentL() { return new (ELeave) CPheTermDocument(*this); }
	};

LOCAL_C CApaApplication* NewApplication()
	{
	return new CPheTermApplication;
	}

GLDEF_C TInt E32Main()
	{
	return EikStart::RunApplication(NewApplication);
	}
