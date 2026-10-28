// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheicons.h -- OURS: Catalogue's lazy icon loader. One icon at a time, never while the list is moving: the nearest
// visible row that has no icon yet is fetched into the persistent cache (c:\data\pheget\icons\<hash>.png), decoded
// asynchronously and scaled to the row's icon size. The next start finds every icon in the cache and needs no network.
#ifndef PHEICONS_H
#define PHEICONS_H

#include <e32base.h>
#include <f32file.h>
#include <imageconversion.h>
#include "pheworker.h"

class CPheCatalogueView;

class CPheIconLoader : public CActive, public MPheJobObserver
	{
public:
	static CPheIconLoader* NewL(CPheCatalogueView& aView);
	~CPheIconLoader();
	void Kick();                                    // something may be loadable now
	void NetworkChanged() { iNetDown = EFalse; }    // (a refresh got through: icons may be fetched again)

private:
	CPheIconLoader(CPheCatalogueView& aView);
	void JobDone(TInt aId, TInt aResult);              // MPheJobObserver: the icon download thread ended
	void ConstructL();
	void RunL();
	void DoCancel();
	void Schedule(TInt aMs);
	void StartDecodeL(const TDesC& aPath);
	void Decode(const TDesC& aPath);
	void FinishDecodeL();
	void Drop();
	static void CachePath(const TDesC8& aUrl, TDes& aPath);

	enum TState { EIdle, EWait, EBusy, EFetch, EDecode };
	CPheCatalogueView& iView;
	CPheJob* iJob;
	TFileName iPath;
	RFs iFs;
	RTimer iTimer;
	TState iState;
	CImageDecoder* iDecoder;
	CFbsBitmap* iSrc;
	CFbsBitmap* iSrcMask;
	TInt iRow;
	TInt iGeneration;
	TBool iNetDown;
	TInt iDone;
	};

#endif
