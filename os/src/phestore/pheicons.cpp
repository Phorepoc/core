// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheicons.cpp -- OURS: see pheicons.h
#include "pheicons.h"
#include "pheui.h"
#include <e32debug.h>
#include <bautils.h>
#include <bitdev.h>
#include <bitstd.h>

extern TInt PheStoreSince();
_LIT(KIconDir, "c:\\data\\pheget\\icons\\");
static const TInt KGapMs = 100;                // between two icons
static const TInt KMovingMs = 150;             // wait while the list moves

static TBool IsUrl(const TDesC8& aLocation)
	{
	return aLocation.Left(7).CompareF(_L8("http://")) == 0 || aLocation.Left(8).CompareF(_L8("https://")) == 0;
	}

CPheIconLoader* CPheIconLoader::NewL(CPheCatalogueView& aView)
	{
	CPheIconLoader* self = new (ELeave) CPheIconLoader(aView);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CPheIconLoader::CPheIconLoader(CPheCatalogueView& aView)
	: CActive(CActive::EPriorityLow), iView(aView), iRow(-1)
	{
	}

void CPheIconLoader::ConstructL()
	{
	User::LeaveIfError(iFs.Connect());
	User::LeaveIfError(iTimer.CreateLocal());
	iFs.MkDirAll(KIconDir);
	iJob = CPheJob::NewL(*this);
	CActiveScheduler::Add(this);
	}

CPheIconLoader::~CPheIconLoader()
	{
	Cancel();
	delete iJob;
	Drop();
	iTimer.Close();
	iFs.Close();
	}

void CPheIconLoader::Drop()
	{
	delete iDecoder;
	iDecoder = NULL;
	delete iSrc;
	iSrc = NULL;
	delete iSrcMask;
	iSrcMask = NULL;
	}

void CPheIconLoader::DoCancel()
	{
	iTimer.Cancel();
	if (iDecoder)
		iDecoder->Cancel();
	iState = EIdle;
	}

void CPheIconLoader::Kick()
	{
	if (iState == EIdle && !IsActive())
		Schedule(KGapMs);
	}

void CPheIconLoader::Schedule(TInt aMs)
	{
	iState = EWait;
	iTimer.After(iStatus, aMs * 1000);
	SetActive();
	}

// the cache file of an icon: a hash of its URL
void CPheIconLoader::CachePath(const TDesC8& aUrl, TDes& aPath)
	{
	TUint32 h = 2166136261u;
	for (TInt i = 0; i < aUrl.Length(); ++i)
		h = (h ^ aUrl[i]) * 16777619u;
	aPath = KIconDir;
	aPath.AppendNumFixedWidth(h, EHex, 8);
	aPath.Append(_L(".png"));
	}

void CPheIconLoader::RunL()
	{
	if (iState == EDecode)
		{
		TInt err = iStatus.Int();
		TInt row = iRow;
		TInt gen = iGeneration;
		iState = EIdle;
		if (gen == iView.Generation() && row >= 0 && row < iView.RowCount())
			{
			if (err == KErrNone)
				{
				TRAPD(e, FinishDecodeL());
				if (e != KErrNone)
					iView.MarkIcon(row, TPheRow::EFailed);
				}
			else
				{
				TFileName path;
				CachePath(iView.RowIconUrl(row), path);
				iFs.Delete(path);                       // a corrupt download must not stick
				iView.MarkIcon(row, TPheRow::EFailed);
				}
			}
		Drop();
		iRow = -1;
		Schedule(KGapMs);
		return;
		}
	// EWait: pick the next icon, unless the list is moving (EBusy: Kick() leaves us alone meanwhile)
	iState = EBusy;
	if (iView.Moving())
		{
		Schedule(KMovingMs);
		return;
		}
	TInt row = iView.PickIconRow();
	if (row < 0)
		{
		iView.EvictFarIcons();
		iState = EIdle;
		return;                                         // nothing to do until the next Kick
		}
	iRow = row;
	iGeneration = iView.Generation();
	if (iDone < 3 || iDone == 12)
		RDebug::Printf("PH: [phestore] icon: row %d picked (%d done) at %d ms", row, iDone, PheStoreSince());
	iView.MarkIcon(row, TPheRow::ELoading);
	TBuf8<256> url(iView.RowIconUrl(row).Left(256));
	TFileName path;
	CachePath(url, path);
	if (!BaflUtils::FileExists(iFs, path))
		{
		if (IsUrl(url) && iNetDown)
			{
			iView.MarkIcon(row, TPheRow::EFailed);
			Schedule(0);
			return;
			}
		TFileName loc;
		loc.Copy(url);
		iPath = path;
		iState = EFetch;                              // a download thread does it; JobDone continues
		if (iJob->Start(0, CPheJob::EFetch, loc, path) != KErrNone)
			{
			iView.MarkIcon(row, TPheRow::EFailed);
			Schedule(KGapMs);
			}
		return;
		}
	Decode(path);
	}

// the icon is in the cache: decode it
void CPheIconLoader::Decode(const TDesC& aPath)
	{
	TInt row = iRow;
	TRAPD(err, StartDecodeL(aPath));
	if (err != KErrNone)
		{
		iFs.Delete(aPath);
		iView.MarkIcon(row, TPheRow::EFailed);
		Drop();
		Schedule(KGapMs);
		}
	}

void CPheIconLoader::JobDone(TInt /*aId*/, TInt aResult)
	{
	if (iState != EFetch)
		return;
	iState = EBusy;
	TInt row = iRow;
	if (iGeneration != iView.Generation() || row < 0 || row >= iView.RowCount())
		{
		Schedule(KGapMs);
		return;
		}
	if (aResult != KErrNone)
		{
		TBuf8<8> head(iView.RowIconUrl(row).Left(8));
		if (IsUrl(head))
			iNetDown = ETrue;
		iView.MarkIcon(row, TPheRow::EFailed);
		Schedule(KGapMs);
		return;
		}
	Decode(iPath);
	}

void CPheIconLoader::StartDecodeL(const TDesC& aPath)
	{
	Drop();
	iDecoder = CImageDecoder::FileNewL(iFs, aPath, CImageDecoder::EOptionNone);
	TFrameInfo info = iDecoder->FrameInfo(0);
	iSrc = new (ELeave) CFbsBitmap;
	User::LeaveIfError(iSrc->Create(info.iOverallSizeInPixels, EColor16MU));
	if (info.iFlags & TFrameInfo::ETransparencyPossible)
		{
		iSrcMask = new (ELeave) CFbsBitmap;
		User::LeaveIfError(iSrcMask->Create(info.iOverallSizeInPixels, (info.iFlags & TFrameInfo::EAlphaChannel) ? EGray256 : EGray2));
		iDecoder->Convert(&iStatus, *iSrc, *iSrcMask, 0);
		}
	else
		iDecoder->Convert(&iStatus, *iSrc, 0);
	iState = EDecode;
	SetActive();
	}

// scale the decoded picture (and its mask) to the row's icon size
void CPheIconLoader::FinishDecodeL()
	{
	TSize target = iView.IconSize();
	TRect srcRect(iSrc->SizeInPixels());
	CFbsBitmap* icon = new (ELeave) CFbsBitmap;
	CleanupStack::PushL(icon);
	User::LeaveIfError(icon->Create(target, EColor16MU));
	{
	CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL(icon);
	CleanupStack::PushL(dev);
	CFbsBitGc* gc = NULL;
	User::LeaveIfError(dev->CreateContext(gc));
	gc->DrawBitmap(TRect(target), iSrc, srcRect);
	delete gc;
	CleanupStack::PopAndDestroy(dev);
	}
	CFbsBitmap* mask = NULL;
	if (iSrcMask)
		{
		mask = new (ELeave) CFbsBitmap;
		CleanupStack::PushL(mask);
		User::LeaveIfError(mask->Create(target, iSrcMask->DisplayMode()));
		CFbsBitmapDevice* dev = CFbsBitmapDevice::NewL(mask);
		CleanupStack::PushL(dev);
		CFbsBitGc* gc = NULL;
		User::LeaveIfError(dev->CreateContext(gc));
		gc->DrawBitmap(TRect(target), iSrcMask, TRect(iSrcMask->SizeInPixels()));
		delete gc;
		CleanupStack::PopAndDestroy(dev);
		CleanupStack::Pop(mask);
		}
	CleanupStack::Pop(icon);
	iView.SetIcon(iRow, icon, mask);
	++iDone;
	if (iDone <= 3 || iDone == 12)
		RDebug::Printf("PH: [phestore] TIMING icon %d ready (row %d) at %d ms", iDone, iRow, PheStoreSince());
	}
