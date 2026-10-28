// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheui.cpp -- OURS: see pheui.h
#include "pheui.h"
#include <e32math.h>
#include <eikenv.h>
#include <coemain.h>
#include <w32std.h>

// ---- palette
static const TRgb KPage(0xf1, 0xf1, 0xf1);
static const TRgb KText(0x28, 0x28, 0x28);
static const TRgb KText2(0x66, 0x66, 0x66);
static const TRgb KTextLight(0x99, 0x99, 0x99);
static const TRgb KBlue(0x4d, 0x8e, 0xcc);
static const TRgb KRed(0xe4, 0x34, 0x30);
static const TRgb KPressed(0x10, 0x80, 0xdd);
static const TRgb KDivider(0xd9, 0xd9, 0xd9);
static const TRgb KWhite(0xff, 0xff, 0xff);

static const TInt KPressDelayMs = 150;
static const TInt KTickUs = 33000;
static const TInt KIconWindow = 15;          // rows each side of the viewport that may hold an icon
static const TInt KKeepWindow = 30;

static const TInt KCos[12] = { 1000, 866, 500, 0, -500, -866, -1000, -866, -500, 0, 500, 866 };
static const TInt KSin[12] = { 0, 500, 866, 1000, 866, 500, 0, -500, -866, -1000, -866, -500 };

static TRgb Lerp(TRgb aA, TRgb aB, TInt aNum, TInt aDen)
	{
	if (aDen <= 0)
		return aA;
	return TRgb(aA.Red() + (aB.Red() - aA.Red()) * aNum / aDen,
				aA.Green() + (aB.Green() - aA.Green()) * aNum / aDen,
				aA.Blue() + (aB.Blue() - aA.Blue()) * aNum / aDen);
	}

// ---- construction

CPheCatalogueView* CPheCatalogueView::NewL(const TRect& aRect, MPheCatalogueUi& aUi)
	{
	CPheCatalogueView* self = new (ELeave) CPheCatalogueView(aUi);
	CleanupStack::PushL(self);
	self->ConstructL(aRect);
	CleanupStack::Pop(self);
	return self;
	}

void CPheCatalogueView::ConstructL(const TRect& aRect)
	{
	iSel = -1;
	iPressRow = -1;
	iDetailRow = -1;
	iDProgress = -1;
	iMode = EList;
	iLoad = ELoading;
	CreateWindowL();
	Window().SetPointerGrab(ETrue);
	EnableDragEvents();
	iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
	SetRect(aRect);                                // (SizeChanged: fonts, frame, first render)
	ActivateL();
	}

CPheCatalogueView::~CPheCatalogueView()
	{
	delete iTimer;
	ClearRows();
	iRows.Close();
	delete iLoadMsg;
	delete iDTitle;
	delete iDMeta;
	delete iDBody;
	delete iDButton;
	delete iDStatus;
	delete iFrameGc;
	delete iFrameDev;
	delete iFrame;
	CWsScreenDevice* dev = CEikonEnv::Static()->ScreenDevice();
	CFont* fonts[5] = { iFTitle, iFInfo, iFSmall, iFHeader, iFButton };
	for (TInt i = 0; i < 5; ++i)
		if (fonts[i])
			dev->ReleaseFont(fonts[i]);
	}

CFont* CPheCatalogueView::MakeFont(TInt aPixels, TBool aBold)
	{
	TFontSpec spec = CEikonEnv::Static()->NormalFont()->FontSpecInTwips();
	spec.iHeight = aPixels;                         // (GetNearestFontInPixels reads it as pixels)
	spec.iFontStyle.SetStrokeWeight(aBold ? EStrokeWeightBold : EStrokeWeightNormal);
	CFont* font = NULL;
	if (CEikonEnv::Static()->ScreenDevice()->GetNearestFontInPixels(font, spec) != KErrNone)
		return NULL;
	return font;
	}

void CPheCatalogueView::SizeChanged()
	{
	TRAP_IGNORE(LayoutL());
	}

void CPheCatalogueView::LayoutL()
	{
	TSize size = Rect().Size();
	if (size.iWidth <= 0 || size.iHeight <= 0)
		return;
	TInt base = Min(size.iWidth, size.iHeight);
	iScale = base * 256 / 360;
	if (iScale < 128)
		iScale = 128;
	iHeaderH = Sc(64);
	iRowH = Sc(83);
	iIconPx = Sc(66);
	iMargin = Sc(12);
	CWsScreenDevice* dev = CEikonEnv::Static()->ScreenDevice();
	CFont** fonts[5] = { &iFTitle, &iFInfo, &iFSmall, &iFHeader, &iFButton };
	for (TInt i = 0; i < 5; ++i)
		if (*fonts[i])
			{
			dev->ReleaseFont(*fonts[i]);
			*fonts[i] = NULL;
			}
	iFTitle = MakeFont(Sc(22), EFalse);
	iFInfo = MakeFont(Sc(18), EFalse);
	iFSmall = MakeFont(Sc(15), EFalse);
	iFHeader = MakeFont(Sc(26), ETrue);
	iFButton = MakeFont(Sc(22), EFalse);
	if (!iFTitle || !iFInfo || !iFSmall || !iFHeader || !iFButton)
		User::Leave(KErrNotFound);
	iAscTitle = iFTitle->AscentInPixels();
	iAscInfo = iFInfo->AscentInPixels();
	iAscSmall = iFSmall->AscentInPixels();
	iAscHeader = iFHeader->AscentInPixels();

	if (!iFrame || iFrame->SizeInPixels() != size)
		{
		delete iFrameGc;
		iFrameGc = NULL;
		delete iFrameDev;
		iFrameDev = NULL;
		delete iFrame;
		iFrame = NULL;
		iFrame = new (ELeave) CFbsBitmap;
		User::LeaveIfError(iFrame->Create(size, CEikonEnv::Static()->ScreenDevice()->DisplayMode()));
		iFrameDev = CFbsBitmapDevice::NewL(iFrame);
		User::LeaveIfError(iFrameDev->CreateContext(iFrameGc));
		}
	if (iMode == EDetails)
		{
		// re-wrap the body for the new width happens in RenderDetails (it wraps every frame it is shown: short text)
		}
	ScrollTo(iOffset);
	Render();
	}

// ---- rows

void CPheCatalogueView::ClearRow(TPheRow* aRow)
	{
	delete aRow->iName;
	delete aRow->iInfo;
	delete aRow->iTag;
	delete aRow->iIconUrl;
	delete aRow->iIcon;
	delete aRow->iMask;
	delete aRow;
	}

void CPheCatalogueView::ClearRows()
	{
	for (TInt i = 0; i < iRows.Count(); ++i)
		ClearRow(iRows[i]);
	iRows.Reset();
	++iGeneration;
	iOffset = 0;
	iSel = -1;
	iFling = EFalse;
	}

void CPheCatalogueView::AddRowL(const TDesC& aName, const TDesC& aInfo, const TDesC& aTag, TInt aState, const TDesC8& aIconUrl)
	{
	TPheRow* row = new (ELeave) TPheRow;
	Mem::FillZ(row, sizeof(TPheRow));
	TRAPD(err,
		row->iName = aName.AllocL();
		row->iInfo = aInfo.AllocL();
		row->iTag = aTag.AllocL();
		row->iIconUrl = aIconUrl.AllocL();
		row->iState = aState;
		iRows.AppendL(row));
	if (err != KErrNone)
		{
		ClearRow(row);
		User::Leave(err);
		}
	}

void CPheCatalogueView::UpdateRow(TInt aRow, TInt aState)
	{
	if (aRow >= 0 && aRow < iRows.Count())
		iRows[aRow]->iState = aState;
	}

void CPheCatalogueView::SetLoad(TLoad aLoad, const TDesC& aMessage)
	{
	iLoad = aLoad;
	delete iLoadMsg;
	iLoadMsg = aMessage.Alloc();
	if (aLoad == ELoading)
		StartTicking();
	Redraw();
	}

void CPheCatalogueView::Redraw()
	{
	if (!iFrame)
		return;
	Render();
	if (IsActivated() && IsVisible())
		DrawNow();
	}

// ---- details data

void CPheCatalogueView::ShowDetailsL(TInt aRow, const TDesC& aTitle, const TDesC& aMeta, const TDesC& aBody, const TDesC& aButton, TBool aButtonOn)
	{
	HBufC* t = aTitle.AllocLC();
	HBufC* m = aMeta.AllocLC();
	HBufC* b = aBody.AllocLC();
	HBufC* bt = aButton.AllocLC();
	delete iDTitle; iDTitle = t;
	delete iDMeta; iDMeta = m;
	delete iDBody; iDBody = b;
	delete iDButton; iDButton = bt;
	CleanupStack::Pop(4);
	delete iDStatus;
	iDStatus = NULL;
	iDProgress = -1;
	iDButtonOn = aButtonOn;
	iDButtonDown = EFalse;
	iDetailRow = aRow;
	if (iMode == EList)
		iListOffset = iOffset;
	iMode = EDetails;
	iFling = EFalse;
	iOffsetF = 0;
	iOffset = 0;
	Redraw();
	}

void CPheCatalogueView::SetDetailsStatus(const TDesC& aStatus, TBool aError, TInt aProgress)
	{
	delete iDStatus;
	iDStatus = aStatus.Length() ? aStatus.Alloc() : NULL;
	iDStatusError = aError;
	iDProgress = aProgress;
	Redraw();
	}

void CPheCatalogueView::SetDetailsButton(const TDesC& aButton, TBool aEnabled)
	{
	HBufC* b = aButton.Alloc();
	if (b)
		{
		delete iDButton;
		iDButton = b;
		}
	iDButtonOn = aEnabled;
	Redraw();
	}

void CPheCatalogueView::ShowList()
	{
	iMode = EList;
	iFling = EFalse;
	iOffset = iListOffset;
	iOffsetF = iOffset;
	ScrollTo(iOffset);
	Redraw();
	iUi.UiIconWanted();
	}

// ---- geometry

TInt CPheCatalogueView::ContentHeight() const
	{
	if (iMode == EDetails)
		return iDContentH;
	return iRows.Count() * iRowH + Sc(8);
	}

TInt CPheCatalogueView::MaxOffset() const
	{
	TInt max = ContentHeight() - (Rect().Height() - iHeaderH);
	return max > 0 ? max : 0;
	}

void CPheCatalogueView::ScrollTo(TInt aOffset)
	{
	TInt max = MaxOffset();
	if (aOffset > max)
		aOffset = max;
	if (aOffset < 0)
		aOffset = 0;
	iOffset = aOffset;
	iOffsetF = aOffset;
	}

TInt CPheCatalogueView::RowAt(TInt aY) const
	{
	if (iMode != EList || aY < iHeaderH || iRowH <= 0)
		return -1;
	TInt r = (aY - iHeaderH + iOffset) / iRowH;
	return (r >= 0 && r < iRows.Count()) ? r : -1;
	}

void CPheCatalogueView::EnsureVisible(TInt aRow)
	{
	TInt top = aRow * iRowH;
	TInt viewH = Rect().Height() - iHeaderH;
	if (top < iOffset)
		ScrollTo(top);
	else if (top + iRowH > iOffset + viewH)
		ScrollTo(top + iRowH - viewH);
	}

// ---- icons

TBool CPheCatalogueView::Moving() const
	{
	return iFling || (iDown && iDragging);
	}

TInt CPheCatalogueView::PickIconRow() const
	{
	if (iMode != EList || iRows.Count() == 0 || iRowH <= 0)
		return -1;
	TInt viewH = Rect().Height() - iHeaderH;
	TInt first = iOffset / iRowH;
	TInt last = (iOffset + viewH) / iRowH;
	TInt centre = (first + last) / 2;
	// the visible rows first (nearest the middle), then the window around them
	for (TInt d = 0; d <= (last - first) / 2 + KIconWindow; ++d)
		{
		TInt cand[2] = { centre - d, centre + d };
		for (TInt k = 0; k < 2; ++k)
			{
			TInt r = cand[k];
			if (r >= 0 && r < iRows.Count() && iRows[r]->iIconState == TPheRow::ENone && iRows[r]->iIconUrl->Length())
				return r;
			}
		}
	return -1;
	}

void CPheCatalogueView::SetIcon(TInt aRow, CFbsBitmap* aIcon, CFbsBitmap* aMask)
	{
	if (aRow < 0 || aRow >= iRows.Count())
		{
		delete aIcon;
		delete aMask;
		return;
		}
	TPheRow* r = iRows[aRow];
	delete r->iIcon;
	delete r->iMask;
	r->iIcon = aIcon;
	r->iMask = aMask;
	r->iIconState = TPheRow::EReady;
	TInt y = iHeaderH + aRow * iRowH - iOffset;
	if (iMode == EList && y + iRowH > iHeaderH && y < Rect().Height() && !Moving())
		Redraw();
	}

void CPheCatalogueView::MarkIcon(TInt aRow, TInt aState)
	{
	if (aRow >= 0 && aRow < iRows.Count())
		iRows[aRow]->iIconState = aState;
	}

void CPheCatalogueView::EvictFarIcons()
	{
	if (iRowH <= 0)
		return;
	TInt centre = (iOffset + (Rect().Height() - iHeaderH) / 2) / iRowH;
	for (TInt i = 0; i < iRows.Count(); ++i)
		{
		TPheRow* r = iRows[i];
		if (r->iIcon && Abs(i - centre) > KKeepWindow && !(iMode == EDetails && i == iDetailRow))
			{
			delete r->iIcon;
			delete r->iMask;
			r->iIcon = NULL;
			r->iMask = NULL;
			r->iIconState = TPheRow::ENone;
			}
		}
	}

// ---- drawing primitives

void CPheCatalogueView::Text(const TDesC& aText, const CFont& aFont, TInt aX, TInt aBaseline, TRgb aColour)
	{
	iFrameGc->UseFont(&aFont);
	iFrameGc->SetPenColor(aColour);
	iFrameGc->DrawText(aText, TPoint(aX, aBaseline));
	iFrameGc->DiscardFont();
	}

void CPheCatalogueView::TextClipped(const TDesC& aText, const CFont& aFont, TInt aX, TInt aBaseline, TInt aMaxW, TRgb aColour)
	{
	if (aMaxW <= 0 || aText.Length() == 0)
		return;
	if (aFont.TextWidthInPixels(aText) <= aMaxW)
		{
		Text(aText, aFont, aX, aBaseline, aColour);
		return;
		}
	_LIT(KDots, "...");
	TInt excess = 0;
	TInt n = aFont.TextCount(aText, aMaxW - aFont.TextWidthInPixels(KDots), excess);
	TBuf<160> buf;
	buf.Copy(aText.Left(Min(n, 150)));
	buf.Append(KDots);
	Text(buf, aFont, aX, aBaseline, aColour);
	}

void CPheCatalogueView::FillGradient(const TRect& aRect, TRgb aTop, TRgb aBottom)
	{
	TInt h = aRect.Height();
	iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
	iFrameGc->SetPenSize(TSize(1, 1));
	for (TInt y = 0; y < h; ++y)
		{
		iFrameGc->SetPenColor(Lerp(aTop, aBottom, y, h - 1));
		iFrameGc->DrawLine(TPoint(aRect.iTl.iX, aRect.iTl.iY + y), TPoint(aRect.iBr.iX, aRect.iTl.iY + y));
		}
	}

void CPheCatalogueView::DrawSpinner(const TPoint& aCentre, TInt aRadius)
	{
	TInt w = Max(2, aRadius / 6);
	iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
	iFrameGc->SetPenSize(TSize(w, w));
	for (TInt k = 0; k < 12; ++k)
		{
		TInt age = (iSpin - k + 12) % 12;
		iFrameGc->SetPenColor(Lerp(KBlue, KDivider, age, 11));
		TInt r0 = aRadius * 55 / 100;
		iFrameGc->DrawLine(TPoint(aCentre.iX + KCos[k] * r0 / 1000, aCentre.iY + KSin[k] * r0 / 1000),
						   TPoint(aCentre.iX + KCos[k] * aRadius / 1000, aCentre.iY + KSin[k] * aRadius / 1000));
		}
	iFrameGc->SetPenSize(TSize(1, 1));
	}

void CPheCatalogueView::DrawButton(const TRect& aRect, const TDesC& aLabel, TBool aEnabled)
	{
	TInt h = aRect.Height();
	TInt r = Sc(9);
	iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
	iFrameGc->SetPenSize(TSize(1, 1));
	for (TInt dy = 0; dy < h; ++dy)
		{
		TInt inset = 0;
		TInt d = dy < r ? r - dy : (dy >= h - r ? dy - (h - 1 - r) : 0);
		if (d > 0)
			{
			TReal root;
			Math::Sqrt(root, TReal(r * r - d * d));
			inset = r - TInt(root);
			}
		TRgb c;
		if (aEnabled)
			c = dy < h / 2 ? Lerp(TRgb(0x30, 0x91, 0xe2), TRgb(0x1a, 0x85, 0xde), dy, h / 2)
						   : Lerp(TRgb(0x1a, 0x85, 0xde), KPressed, dy - h / 2, h - h / 2);
		else
			c = TRgb(0xdc, 0xdc, 0xdc);
		iFrameGc->SetPenColor(c);
		iFrameGc->DrawLine(TPoint(aRect.iTl.iX + inset, aRect.iTl.iY + dy), TPoint(aRect.iBr.iX - inset, aRect.iTl.iY + dy));
		}
	TInt tw = iFButton->TextWidthInPixels(aLabel);
	TInt base = aRect.iTl.iY + (h + iFButton->AscentInPixels() - iFButton->DescentInPixels()) / 2;
	Text(aLabel, *iFButton, aRect.iTl.iX + (aRect.Width() - tw) / 2, base, aEnabled ? TRgb(0xf1, 0xf0, 0xf1) : KTextLight);
	}

void CPheCatalogueView::DrawProgress(const TRect& aRect, TInt aPermille)
	{
	iFrameGc->SetPenStyle(CGraphicsContext::ENullPen);
	iFrameGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	iFrameGc->SetBrushColor(KDivider);
	iFrameGc->DrawRoundRect(aRect, TSize(aRect.Height() / 2, aRect.Height() / 2));
	TInt w = aRect.Width() * Min(Max(aPermille, 0), 1000) / 1000;
	if (w > aRect.Height())
		{
		iFrameGc->SetBrushColor(KBlue);
		iFrameGc->DrawRoundRect(TRect(aRect.iTl, TSize(w, aRect.Height())), TSize(aRect.Height() / 2, aRect.Height() / 2));
		}
	iFrameGc->SetBrushStyle(CGraphicsContext::ENullBrush);
	iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
	}

// ---- the frame

void CPheCatalogueView::Render()
	{
	if (!iFrame || !iFrameGc || !iFTitle)
		return;
	iFrameGc->Reset();
	iFrameGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
	iFrameGc->SetBrushColor(KPage);
	iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
	iFrameGc->Clear();
	iFrameGc->SetBrushStyle(CGraphicsContext::ENullBrush);
	if (iMode == EDetails)
		RenderDetails();
	else
		RenderList();
	}

void CPheCatalogueView::DrawHeader(const TDesC& aTitle)
	{
	TInt w = Rect().Width();
	FillGradient(TRect(0, 0, w, iHeaderH - 1), TRgb(0xed, 0xed, 0xed), TRgb(0xe7, 0xe7, 0xe7));
	iFrameGc->SetPenColor(TRgb(0xcf, 0xcf, 0xcf));
	iFrameGc->DrawLine(TPoint(0, 0), TPoint(w, 0));
	iFrameGc->SetPenColor(KWhite);
	iFrameGc->DrawLine(TPoint(0, iHeaderH - 1), TPoint(w, iHeaderH - 1));
	TInt rightReserve = iMargin;
	if (iMode == EList)
		{
		if (iLoad == ELoading && iRows.Count() > 0)
			{
			DrawSpinner(TPoint(w - iMargin - Sc(12), iHeaderH / 2), Sc(11));
			rightReserve = iMargin + Sc(34);
			}
		else if (iLoad == EReady && iRows.Count() > 0)
			{
			TBuf<24> n;
			n.Format(_L("%d"), iRows.Count());
			TInt tw = iFInfo->TextWidthInPixels(n);
			Text(n, *iFInfo, w - iMargin - tw, iHeaderH / 2 + iAscInfo / 2, KText2);
			rightReserve = iMargin + tw + Sc(8);
			}
		}
	TextClipped(aTitle, *iFHeader, iMargin, Sc(17) + iAscHeader, w - iMargin - rightReserve, KText);
	}

void CPheCatalogueView::DrawRow(TInt aRow, TInt aY, TBool aPressed)
	{
	const TPheRow& row = *iRows[aRow];
	TInt w = Rect().Width();
	TRgb tc = aPressed ? KWhite : KText;
	TRgb sc = aPressed ? KWhite : KText2;
	if (aPressed)
		{
		iFrameGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
		iFrameGc->SetBrushColor(KPressed);
		iFrameGc->SetPenStyle(CGraphicsContext::ENullPen);
		iFrameGc->DrawRect(TRect(0, aY, w, aY + iRowH));
		iFrameGc->SetBrushStyle(CGraphicsContext::ENullBrush);
		iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
		}
	// icon (or its placeholder)
	TPoint ip(Sc(8), aY + (iRowH - iIconPx) / 2);
	if (row.iIcon && row.iIcon->Handle())
		{
		if (row.iMask)
			iFrameGc->BitBltMasked(ip, row.iIcon, TRect(row.iIcon->SizeInPixels()), row.iMask, EFalse);
		else
			iFrameGc->BitBlt(ip, row.iIcon);
		}
	else
		{
		iFrameGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
		iFrameGc->SetBrushColor(aPressed ? TRgb(0x5a, 0xa9, 0xec) : TRgb(0xe2, 0xe2, 0xe2));
		iFrameGc->SetPenStyle(CGraphicsContext::ENullPen);
		iFrameGc->DrawRoundRect(TRect(ip, TSize(iIconPx, iIconPx)), TSize(Sc(10), Sc(10)));
		iFrameGc->SetBrushStyle(CGraphicsContext::ENullBrush);
		iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
		}
	TInt tx = Sc(8) + iIconPx + Sc(16);
	TInt rx = w - Sc(14);
	TInt titleTop = aY + Sc(6);
	TextClipped(*row.iName, *iFTitle, tx, titleTop + iAscTitle, rx - tx, tc);
	// the state slot, right-aligned on the info line
	TPtrC state;
	TRgb stateColour = aPressed ? KWhite : KBlue;
	_LIT(KFree, "Free");
	_LIT(KInstalled, "Installed");
	_LIT(KUpdate, "Update");
	switch (row.iState)
		{
	case TPheRow::EInstalled: state.Set(KInstalled); break;
	case TPheRow::EUpdate: state.Set(KUpdate); if (!aPressed) stateColour = KRed; break;
	default: state.Set(KFree); break;
		}
	TInt infoTop = titleTop + iFTitle->HeightInPixels() + Sc(1);
	TInt stateW = iFInfo->TextWidthInPixels(state);
	Text(state, *iFInfo, rx - stateW, infoTop + iAscInfo, stateColour);
	TextClipped(*row.iInfo, *iFInfo, tx, infoTop + iAscInfo, rx - stateW - Sc(10) - tx, sc);
	if (row.iTag->Length())
		Text(*row.iTag, *iFSmall, tx, aY + iRowH - Sc(9) - (iFSmall->HeightInPixels() - iAscSmall), aPressed ? KWhite : KTextLight);
	}

void CPheCatalogueView::DrawEmpty(const TDesC& aMessage, TBool aError, TBool aButton)
	{
	TInt w = Rect().Width();
	TInt cy = iHeaderH + (Rect().Height() - iHeaderH) / 2;
	TInt y = cy - Sc(40);
	TInt tw = iFTitle->TextWidthInPixels(aMessage);
	if (tw > w - 2 * iMargin)
		tw = w - 2 * iMargin;
	TextClipped(aMessage, *iFTitle, (w - tw) / 2, y + iAscTitle, w - 2 * iMargin, aError ? KRed : KText2);
	if (aButton)
		{
		TInt bw = Sc(160);
		iRetryRect = TRect(TPoint((w - bw) / 2, y + Sc(50)), TSize(bw, Sc(48)));
		_LIT(KRetry, "Try again");
		DrawButton(iRetryRect, KRetry, ETrue);
		}
	}

void CPheCatalogueView::RenderList()
	{
	TInt w = Rect().Width();
	TInt h = Rect().Height();
	TInt n = iRows.Count();
	iRetryRect = TRect();
	if (n > 0)
		{
		TInt first = Max(0, iOffset / iRowH);
		TInt last = Min(n - 1, (iOffset + h - iHeaderH) / iRowH);
		for (TInt i = first; i <= last; ++i)
			{
			TInt y = iHeaderH + i * iRowH - iOffset;
			TBool pressed = (iDown && iShowPressed && !iDragging && i == iPressRow) || iFlash && i == iPressRow || (iSelShown && i == iSel);
			DrawRow(i, y, pressed);
			if (i != n - 1 && !pressed)
				{
				iFrameGc->SetPenColor(KDivider);
				iFrameGc->DrawLine(TPoint(0, y + iRowH - 1), TPoint(w, y + iRowH - 1));
				}
			}
		}
	else if (iLoad == ELoading)
		{
		TInt cy = iHeaderH + (h - iHeaderH) / 2;
		DrawSpinner(TPoint(w / 2, cy - Sc(10)), Sc(24));
		if (iLoadMsg && iLoadMsg->Length())
			{
			TInt tw = iFInfo->TextWidthInPixels(*iLoadMsg);
			TextClipped(*iLoadMsg, *iFInfo, Max(iMargin, (w - tw) / 2), cy + Sc(48) + iAscInfo, w - 2 * iMargin, KText2);
			}
		}
	else if (iLoad == EFailed)
		{
		_LIT(KMsg, "Cannot load the catalogue");
		DrawEmpty(iLoadMsg && iLoadMsg->Length() ? *iLoadMsg : static_cast<const TDesC&>(KMsg), ETrue, ETrue);
		}
	else
		{
		_LIT(KNone, "No apps for this device");
		DrawEmpty(KNone, EFalse, EFalse);
		}
	_LIT(KTitle, "Catalogue");
	DrawHeader(KTitle);
	}

void CPheCatalogueView::RenderDetails()
	{
	TInt w = Rect().Width();
	TInt h = Rect().Height();
	TInt y = iHeaderH - iOffset;
	// item header block: icon + meta lines
	TInt icon = Sc(96);
	TInt ix = iMargin, iy = y + Sc(8);
	if (iDetailRow >= 0 && iDetailRow < iRows.Count() && iRows[iDetailRow]->iIcon)
		{
		TPheRow& r = *iRows[iDetailRow];
		TRect dest(TPoint(ix, iy), TSize(icon, icon));
		if (r.iMask)
			iFrameGc->DrawBitmapMasked(dest, r.iIcon, TRect(r.iIcon->SizeInPixels()), r.iMask, EFalse);
		else
			iFrameGc->DrawBitmap(dest, r.iIcon);
		}
	else
		{
		iFrameGc->SetBrushStyle(CGraphicsContext::ESolidBrush);
		iFrameGc->SetBrushColor(TRgb(0xe2, 0xe2, 0xe2));
		iFrameGc->SetPenStyle(CGraphicsContext::ENullPen);
		iFrameGc->DrawRoundRect(TRect(TPoint(ix, iy), TSize(icon, icon)), TSize(Sc(14), Sc(14)));
		iFrameGc->SetBrushStyle(CGraphicsContext::ENullBrush);
		iFrameGc->SetPenStyle(CGraphicsContext::ESolidPen);
		}
	TInt tx = ix + icon + Sc(16);
	if (iDMeta)
		{
		TInt ly = y + Sc(16);
		TInt pos = 0;
		for (TInt line = 0; line < 4 && pos < iDMeta->Length(); ++line)
			{
			TPtrC rest = iDMeta->Mid(pos);
			TInt nl = rest.Locate('\n');
			TPtrC one = nl >= 0 ? rest.Left(nl) : rest;
			TextClipped(one, line == 0 ? *iFTitle : *iFInfo, tx, ly + (line == 0 ? iAscTitle : iAscInfo), w - tx - iMargin, line == 0 ? KText : KText2);
			ly += (line == 0 ? iFTitle->HeightInPixels() : iFInfo->HeightInPixels()) + Sc(3);
			pos += (nl >= 0 ? nl + 1 : rest.Length());
			}
		}
	y += Sc(112);
	// status line
	if (iDStatus)
		{
		TextClipped(*iDStatus, *iFInfo, iMargin, y + Sc(4) + iAscInfo, w - 2 * iMargin, iDStatusError ? KRed : KText);
		y += Sc(30);
		}
	// the action button
	TRect btn(TPoint(iMargin, y + Sc(4)), TSize(w - 2 * iMargin, Sc(48)));
	iDButtonRect = btn;
	if (iDButton)
		DrawButton(btn, *iDButton, iDButtonOn);
	y += Sc(60);
	if (iDProgress >= 0)
		{
		DrawProgress(TRect(TPoint(iMargin, y + Sc(2)), TSize(w - 2 * iMargin, Sc(8))), iDProgress);
		y += Sc(22);
		}
	iFrameGc->SetPenColor(KDivider);
	iFrameGc->DrawLine(TPoint(0, y), TPoint(w, y));
	y += Sc(12);
	// the description: wrapped, only the visible lines are drawn
	if (iDBody)
		{
		TInt lineH = iFInfo->HeightInPixels() + Sc(4);
		TInt width = w - 2 * iMargin;
		TInt pos = 0;
		TInt len = iDBody->Length();
		while (pos < len)
			{
			TPtrC rest = iDBody->Mid(pos);
			TInt nl = rest.Locate('\n');
			TInt excess = 0;
			TInt fit = iFInfo->TextCount(rest, width, excess);
			if (fit < 1)
				fit = 1;
			TInt take = fit;
			TInt skip = 0;
			if (nl >= 0 && nl <= fit)
				{
				take = nl;
				skip = 1;
				}
			else if (fit < rest.Length())
				{
				TInt sp = rest.Left(fit).LocateReverse(' ');
				if (sp > 0)
					{
					take = sp;
					skip = 1;
					}
				}
			if (y + lineH > iHeaderH && y < h)
				Text(rest.Left(take), *iFInfo, iMargin, y + iAscInfo, KText);
			y += lineH;
			pos += take + skip;
			}
		}
	y += Sc(16);
	iDContentH = y + iOffset - iHeaderH;
	iFrameGc->SetBrushStyle(CGraphicsContext::ENullBrush);
	TPtrC title = iDTitle ? TPtrC(*iDTitle) : TPtrC();
	DrawHeader(title);
	}

void CPheCatalogueView::Draw(const TRect& aRect) const
	{
	CWindowGc& gc = SystemGc();
	if (iFrame)
		{
		gc.BitBlt(TPoint(0, 0), iFrame, aRect);
		if (!iPainted)
			{
			const_cast<CPheCatalogueView*>(this)->iPainted = ETrue;
			iUi.UiFirstPaint();
			}
		}
	else
		{
		gc.SetBrushStyle(CGraphicsContext::ESolidBrush);
		gc.SetBrushColor(KPage);
		gc.Clear(aRect);
		}
	}

// ---- motion

TInt CPheCatalogueView::NowMs() const
	{
	TTime t;
	t.UniversalTime();
	return TInt((t.Int64() / 1000) & 0x3fffffff);
	}

void CPheCatalogueView::StartTicking()
	{
	if (!iTimer->IsActive())
		{
		iLastTick = NowMs();
		iTimer->Start(KTickUs, KTickUs, TCallBack(TickCb, this));
		}
	}

TInt CPheCatalogueView::TickCb(TAny* aSelf)
	{
	static_cast<CPheCatalogueView*>(aSelf)->Tick();
	return 0;
	}

void CPheCatalogueView::Tick()
	{
	TInt now = NowMs();
	TInt dt = now - iLastTick;
	if (dt < 1)
		dt = 1;
	if (dt > 100)
		dt = 100;
	iLastTick = now;
	TBool dirty = EFalse;
	TBool keep = EFalse;
	if (iFling)
		{
		TReal dts = TReal(dt) / 1000.0;
		TReal v = iVelocity;
		TReal decel = TReal(Sc(500)) * dts;
		v = v * (1.0 - 2.4 * dts);
		if (v > 0)
			v = (v > decel) ? v - decel : 0;
		else
			v = (v < -decel) ? v + decel : 0;
		iVelocity = v;
		iOffsetF += v * dts;
		TInt max = MaxOffset();
		if (iOffsetF < 0 || iOffsetF > max)
			{
			iOffsetF = iOffsetF < 0 ? 0 : max;
			iFling = EFalse;
			}
		if (v > -TReal(Sc(30)) && v < TReal(Sc(30)))
			iFling = EFalse;
		TInt o = TInt(iOffsetF);
		if (o != iOffset)
			{
			iOffset = o;
			dirty = ETrue;
			}
		if (!iFling)
			{
			dirty = ETrue;
			iUi.UiIconWanted();
			}
		else
			keep = ETrue;
		}
	if (iDown)
		{
		keep = ETrue;
		if (!iDragging && !iShowPressed && iPressRow >= 0 && now - iDownTime >= KPressDelayMs)
			{
			iShowPressed = ETrue;
			dirty = ETrue;
			}
		}
	if (iFlash && now >= iFlashUntil)
		{
		iFlash = EFalse;
		dirty = ETrue;
		}
	if (iFlash)
		keep = ETrue;
	if (iLoad == ELoading)
		{
		keep = ETrue;
		iSpinAcc += dt;
		if (iSpinAcc >= 80)
			{
			iSpinAcc = 0;
			iSpin = (iSpin + 1) % 12;
			dirty = ETrue;
			}
		}
	if (dirty)
		Redraw();
	if (!keep)
		iTimer->Cancel();
	}

void CPheCatalogueView::PointerDown(const TPoint& aPos)
	{
	iDown = ETrue;
	iDragging = EFalse;
	iShowPressed = EFalse;
	iCaught = iFling;
	iFling = EFalse;
	iDownPos = aPos;
	iLastPos = aPos;
	iDownTime = NowMs();
	iDownOffset = iOffset;
	iPressRow = RowAt(aPos.iY);
	iSampleCount = 0;
	iSelShown = EFalse;
	iSamples[0][0] = iDownTime;
	iSamples[0][1] = aPos.iY;
	iSampleCount = 1;
	iRetryDown = iRetryRect.Contains(aPos);
	iDButtonDown = (iMode == EDetails && iDButtonOn && iDButtonRect.Contains(aPos));
	StartTicking();
	}

void CPheCatalogueView::PointerDrag(const TPoint& aPos)
	{
	if (!iDown)
		return;
	TInt dy = aPos.iY - iDownPos.iY;
	if (!iDragging && Abs(dy) > Sc(10))
		{
		iDragging = ETrue;
		iShowPressed = EFalse;
		iPressRow = -1;
		iDButtonDown = EFalse;
		iRetryDown = EFalse;
		}
	if (iDragging)
		{
		TInt oldOff = iOffset;
		ScrollTo(iDownOffset - dy);
		TInt now = NowMs();
		if (iSampleCount == 8)
			{
			for (TInt i = 1; i < 8; ++i)
				{
				iSamples[i - 1][0] = iSamples[i][0];
				iSamples[i - 1][1] = iSamples[i][1];
				}
			--iSampleCount;
			}
		iSamples[iSampleCount][0] = now;
		iSamples[iSampleCount][1] = aPos.iY;
		++iSampleCount;
		if (iOffset != oldOff)
			Redraw();
		}
	iLastPos = aPos;
	}

void CPheCatalogueView::PointerUp(const TPoint& aPos)
	{
	if (!iDown)
		return;
	iDown = EFalse;
	TBool wasDragging = iDragging;
	iDragging = EFalse;
	if (wasDragging)
		{
		// the speed over the last ~100 ms of the touch
		TInt now = NowMs();
		TInt i = iSampleCount - 1;
		while (i > 0 && now - iSamples[i - 1][0] <= 120)
			--i;
		TInt span = iSamples[iSampleCount - 1][0] - iSamples[i][0];
		TInt dist = iSamples[iSampleCount - 1][1] - iSamples[i][1];
		TBool stale = now - iSamples[iSampleCount - 1][0] > 80;
		if (span > 0 && !stale)
			{
			iVelocity = -TReal(dist) * 1000.0 / TReal(span);
			TReal cap = TReal(Sc(6000));
			if (iVelocity > cap) iVelocity = cap;
			if (iVelocity < -cap) iVelocity = -cap;
			if (iVelocity > TReal(Sc(60)) || iVelocity < -TReal(Sc(60)))
				{
				iFling = ETrue;
				iOffsetF = iOffset;
				StartTicking();
				return;
				}
			}
		Redraw();
		iUi.UiIconWanted();
		return;
		}
	TBool caught = iCaught;
	iCaught = EFalse;
	TInt row = RowAt(aPos.iY);
	if (iMode == EList)
		{
		if (iRetryDown && iRetryRect.Contains(aPos))
			{
			iRetryDown = EFalse;
			iUi.UiRetry();
			return;
			}
		if (!caught && row >= 0 && row == iPressRow)
			{
			iFlash = ETrue;
			iFlashUntil = NowMs() + 120;
			iShowPressed = EFalse;
			Redraw();
			StartTicking();
			iUi.UiOpen(row);
			return;
			}
		}
	else
		{
		if (iDButtonDown && iDButtonOn && iDButtonRect.Contains(aPos))
			{
			iDButtonDown = EFalse;
			iUi.UiAction(iDetailRow);
			return;
			}
		if (aPos.iY < iHeaderH && !caught)
			{
			iUi.UiBack();
			return;
			}
		}
	iShowPressed = EFalse;
	iPressRow = -1;
	Redraw();
	}

void CPheCatalogueView::HandlePointerEventL(const TPointerEvent& aEvent)
	{
	switch (aEvent.iType)
		{
	case TPointerEvent::EButton1Down:
		PointerDown(aEvent.iPosition);
		break;
	case TPointerEvent::EDrag:
		PointerDrag(aEvent.iPosition);
		break;
	case TPointerEvent::EButton1Up:
		PointerUp(aEvent.iPosition);
		break;
	default:
		break;
		}
	}

TKeyResponse CPheCatalogueView::OfferKeyEventL(const TKeyEvent& aKey, TEventCode aType)
	{
	if (aType != EEventKey)
		return EKeyWasNotConsumed;
	TInt code = aKey.iCode;
	if (iMode == EDetails)
		{
		switch (code)
			{
		case EKeyUpArrow:
			ScrollTo(iOffset - Sc(80));
			Redraw();
			return EKeyWasConsumed;
		case EKeyDownArrow:
			ScrollTo(iOffset + Sc(80));
			Redraw();
			return EKeyWasConsumed;
		case EKeyEnter:
		case EKeyDevice3:
			if (iDButtonOn)
				iUi.UiAction(iDetailRow);
			return EKeyWasConsumed;
		default:
			return EKeyWasNotConsumed;
			}
		}
	TInt n = iRows.Count();
	switch (code)
		{
	case EKeyUpArrow:
	case EKeyDownArrow:
		if (n == 0)
			return EKeyWasConsumed;
		if (!iSelShown)
			{
			iSelShown = ETrue;
			if (iSel < 0)
				iSel = Min(n - 1, (iOffset + iRowH - 1) / iRowH);
			else if (iSel * iRowH + iRowH <= iOffset || iSel * iRowH >= iOffset + Rect().Height() - iHeaderH)
				iSel = Min(n - 1, (iOffset + iRowH - 1) / iRowH);
			}
		else
			iSel = Max(0, Min(n - 1, iSel + (code == EKeyUpArrow ? -1 : 1)));
		EnsureVisible(iSel);
		Redraw();
		iUi.UiIconWanted();
		return EKeyWasConsumed;
	case EKeyEnter:
	case EKeyDevice3:
		if (iSelShown && iSel >= 0 && iSel < n)
			iUi.UiOpen(iSel);
		else if (n > 0)
			{
			iSelShown = ETrue;
			if (iSel < 0)
				iSel = Min(n - 1, (iOffset + iRowH - 1) / iRowH);
			Redraw();
			}
		return EKeyWasConsumed;
	default:
		return EKeyWasNotConsumed;
		}
	}
