// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheui.h -- OURS: Catalogue's own full-screen view. One CCoeControl that draws everything itself (CWindowGc, through an
// off-screen bitmap, so nothing flickers): a header, a list of fixed-height rows with kinetic touch scrolling and key
// navigation, and a simple details page. No list box, no Qt. The geometry is a 360x640 design scaled by the screen size.
#ifndef PHEUI_H
#define PHEUI_H

#include <coecntrl.h>
#include <e32base.h>
#include <fbs.h>
#include <bitdev.h>
#include <bitstd.h>

class CPheCatalogueView;

// What the view asks of the application.
class MPheCatalogueUi
	{
public:
	virtual void UiOpen(TInt aRow) = 0;          // a row was tapped / chosen
	virtual void UiAction(TInt aRow) = 0;        // the details button (install / update)
	virtual void UiBack() = 0;                   // back from the details page
	virtual void UiRetry() = 0;                  // "Try again" after a failed load
	virtual void UiFirstPaint() = 0;             // the first real frame is on the screen
	virtual void UiIconWanted() = 0;             // an icon may be loaded now (the list stopped, rows were added)
	virtual void UiExit() = 0;                   // the toolbar's exit button / right softkey on the root page
	virtual void UiOptions() = 0;                // the toolbar's options button / left softkey: the Options menu
	};

class TPheRow
	{
public:
	enum TState { EFree, EInstalled, EUpdate };
	enum TIcon { ENone, ELoading, EReady, EFailed };
	HBufC* iName;
	HBufC* iInfo;
	HBufC* iTag;
	HBufC8* iIconUrl;
	TInt iState;
	TInt iIconState;
	CFbsBitmap* iIcon;
	CFbsBitmap* iMask;
	};

class CPheCatalogueView : public CCoeControl
	{
public:
	enum TMode { EList, EDetails };
	enum TLoad { ELoading, EReady, EFailed };

	static CPheCatalogueView* NewL(const TRect& aRect, MPheCatalogueUi& aUi);
	~CPheCatalogueView();

	// ---- the list's data (filled by the application, a few rows at a time)
	void ClearRows();
	void AddRowL(const TDesC& aName, const TDesC& aInfo, const TDesC& aTag, TInt aState, const TDesC8& aIconUrl);
	void UpdateRow(TInt aRow, TInt aState);
	TInt RowCount() const { return iRows.Count(); }
	void SetLoad(TLoad aLoad, const TDesC& aMessage);   // ELoading with rows = the small header spinner
	void Redraw();
	TInt Generation() const { return iGeneration; }
	TInt Offset() const { return iOffset; }
	void SetOffset(TInt aOffset) { ScrollTo(aOffset); }

	// ---- the details page
	void ShowDetailsL(TInt aRow, const TDesC& aTitle, const TDesC& aMeta, const TDesC& aBody, const TDesC& aButton, TBool aButtonOn);
	void SetDetailsStatus(const TDesC& aStatus, TBool aError, TInt aProgress);   // aProgress -1 = no bar, else 0..1000
	void SetDetailsButton(const TDesC& aButton, TBool aEnabled);
	void ShowList();
	TMode Mode() const { return iMode; }
	TInt Tab() const { return iTab; }
	TInt DetailsRow() const { return iDetailRow; }

	// ---- icons (the loader's side)
	TBool Moving() const;
	TInt PickIconRow() const;                       // the nearest row that wants an icon, or -1
	const TDesC8& RowIconUrl(TInt aRow) const { return *iRows[aRow]->iIconUrl; }
	void SetIcon(TInt aRow, CFbsBitmap* aIcon, CFbsBitmap* aMask);   // takes ownership
	void MarkIcon(TInt aRow, TInt aState);
	void EvictFarIcons();
	TSize IconSize() const { return TSize(iIconPx, iIconPx); }

private:
	CPheCatalogueView(MPheCatalogueUi& aUi) : iUi(aUi) {}
	void ConstructL(const TRect& aRect);
	// CCoeControl
	void SizeChanged();
	void Draw(const TRect& aRect) const;
	void HandlePointerEventL(const TPointerEvent& aEvent);
	TKeyResponse OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType);

	// geometry
	TInt Sc(TInt aV) const { return (aV * iScale + 128) >> 8; }
	void LayoutL();
	TInt ContentHeight() const;
	TInt MaxOffset() const;
	TInt ListTop() const { return iHeaderH; }
	TInt ViewH() const { return Rect().Height() - iBarH; }   // the content area: above the toolbar
	TInt RowAt(TInt aY) const;                      // the row under a view y, or -1
	void ScrollTo(TInt aOffset);
	void EnsureVisible(TInt aRow);

	// drawing
	void Render();
	void RenderList();
	void RenderDetails();
	void DrawHeader(const TDesC& aTitle);
	void RenderSoon();
	void DrawBar();
	void DrawBarIcon(TInt aKind, const TPoint& aCentre, TRgb aColour);
	TInt BarCell(const TPoint& aPos) const;         // 0 = back/exit, 1..4 = the tabs, 5 = options, -1 = not on the bar
	void BarAction(TInt aCell);
	void SetTab(TInt aTab);
	void DrawRow(TInt aRow, TInt aY, TBool aPressed);
	void DrawSpinner(const TPoint& aCentre, TInt aRadius);
	void DrawButton(const TRect& aRect, const TDesC& aLabel, TBool aEnabled);
	void DrawProgress(const TRect& aRect, TInt aPermille);
	void DrawEmpty(const TDesC& aMessage, TBool aError, TBool aButton);
	void DrawWrapped(const TDesC& aText, const CFont& aFont, TInt aX, TInt& aY, TInt aWidth, TRgb aColour);
	void Text(const TDesC& aText, const CFont& aFont, TInt aX, TInt aBaseline, TRgb aColour);
	void TextClipped(const TDesC& aText, const CFont& aFont, TInt aX, TInt aBaseline, TInt aMaxW, TRgb aColour);
	void FillGradient(const TRect& aRect, TRgb aTop, TRgb aBottom);
	CFont* MakeFont(TInt aPixels, TBool aBold);

	// motion
	static TInt TickCb(TAny* aSelf);
	void Tick();
	void StartTicking();
	TInt NowMs() const;
	void PointerDown(const TPoint& aPos);
	void PointerDrag(const TPoint& aPos);
	void PointerUp(const TPoint& aPos);
	void ClearRow(TPheRow* aRow);

	MPheCatalogueUi& iUi;
	RPointerArray<TPheRow> iRows;
	TInt iGeneration;
	TMode iMode;
	TLoad iLoad;
	HBufC* iLoadMsg;

	// geometry (pixels)
	TInt iScale;                // x256
	TInt iBarH;                 // the toolbar's height
	TInt iTab;                  // 0 Home, 1 Categories, 2 Search, 3 My apps
	TInt iBarDown;              // the toolbar cell under the finger, or -1
	TInt iHeaderH, iRowH, iIconPx, iMargin, iDividerCount;
	CFont* iFTitle;             // 22
	CFont* iFInfo;              // 18
	CFont* iFSmall;             // 15
	CFont* iFHeader;            // 26 bold
	CFont* iFButton;
	TInt iAscTitle, iAscInfo, iAscSmall, iAscHeader;

	// off-screen frame
	CFbsBitmap* iFrame;
	CFbsBitmapDevice* iFrameDev;
	CFbsBitGc* iFrameGc;
	// what Draw shows: a finished copy of iFrame. WSERV replays Draw's BitBlt whenever another window over ours redraws
	// (the WLAN 'connecting' note over the header), reading the bitmap AS IT IS THEN -- a half-rendered iFrame showed
	// the page colour where the header and toolbar belong (flashes while flinging during the start-up refresh)
	CFbsBitmap* iShown;
	CFbsBitmapDevice* iShownDev;
	CFbsBitGc* iShownGc;
	TBool iPainted;

	// scrolling
	TInt iOffset;               // pixels scrolled down (list or details)
	TReal iOffsetF;
	TInt iDContentH;
	TInt iSpinAcc;
	TBool iCaught;
	TInt iListOffset;           // the list's offset while the details page is shown
	TReal iVelocity;            // px/s
	TBool iFling;
	CPeriodic* iTimer;
	TInt iLastTick;
	TInt iSpin;

	// touch
	TBool iDown, iDragging, iShowPressed, iFlash;
	TPoint iDownPos, iLastPos;
	TInt iDownTime;
	TInt iDownOffset;
	TInt iPressRow;
	TInt iSamples[8][2];        // (ms, y) of the last moves, for the flick velocity
	TInt iSampleCount;
	TInt iFlashUntil;

	// keys
	TInt iSel;
	TBool iSelShown;

	// details
	TInt iDetailRow;
	HBufC* iDTitle;
	HBufC* iDMeta;
	HBufC* iDBody;
	HBufC* iDButton;
	HBufC* iDStatus;
	TBool iDButtonOn, iDStatusError, iDButtonDown;
	TInt iDProgress;
	TRect iDButtonRect;
	TRect iRetryRect;
	TBool iRetryDown;
	};

#endif
