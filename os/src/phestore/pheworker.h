// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheworker.h -- OURS: network jobs of Catalogue on their own threads. A TLS handshake or a transfer must never run
// on the UI thread (it would freeze touch and animation for seconds on a slow CPU), so each job is a short-lived
// thread with its own engine (CPheGet: its own sockets and HTTP session; it only writes cache files), started at a
// lower priority than the UI. The UI thread learns of the end through an active object; nothing is shared.
#ifndef PHEWORKER_H
#define PHEWORKER_H

#include <e32base.h>
#include <f32file.h>

class MPheJobObserver
	{
public:
	virtual void JobDone(TInt aId, TInt aResult) = 0;
	};

class CPheJob : public CActive
	{
public:
	enum TKind
		{
		ERefresh,           // RefreshIndexesL: result 1 = an index changed, 0 = same, < 0 = error
		EFetch              // FetchFileL(location -> file): 0 = done, < 0 = error
		};
	static CPheJob* NewL(MPheJobObserver& aObserver);
	~CPheJob();
	TInt Start(TInt aId, TKind aKind, const TDesC& aLocation, const TDesC& aFile);
	TBool Busy() const { return IsActive(); }

private:
	CPheJob(MPheJobObserver& aObserver) : CActive(CActive::EPriorityStandard), iObserver(aObserver) {}
	void RunL();
	void DoCancel();
	static TInt Entry(TAny* aSelf);
	TInt Run();

	MPheJobObserver& iObserver;
	RThread iThread;
	TInt iId;
	TKind iKind;
	TFileName iLocation;
	TFileName iFile;
	};

#endif
