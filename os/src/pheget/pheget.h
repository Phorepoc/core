// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheget.h -- OURS: the download app's engine (the format: the index repository's schema; the plan: pdk's
// `ph package`, which does the same from a computer). An index is a JSON file of items, each with its device
// compatibility and its approved releases (asset URL, size, SHA-256). The engine reads the device's identity, loads
// the indexes (HTTP(S) through the platform's HTTP stack, or a local file), downloads a release, checks its hash and
// installs it through the platform's software installer. The command line (pheget.cpp) and the UI share it.
#ifndef PHEGET_H
#define PHEGET_H

#include <e32base.h>
#include <f32file.h>
#include <es_sock.h>
#include <badesca.h>
#include <http/rhttpsession.h>
#include <http/mhttptransactioncallback.h>

class CPheJson;

_LIT(KPheGetDefaultIndex, "https://phorepoc.github.io/index/index.json");
_LIT(KPheGetDeviceFile, "z:\\resource\\phorepoc\\device.txt");   // board=<name> / arch=<arm|x32>, from the adaptation
_LIT(KPheGetDir, "c:\\data\\pheget\\");                          // indexes.txt (extra indexes), installed.txt
_LIT(KPheGetDownloads, "c:\\data\\installs\\");                  // the package cache (prefill = offline installs)

// One item of an index (pointers into the loaded index tree; valid while the engine is).
class TPheGetItem
	{
public:
	const CPheJson* iItem;
	const CPheJson* iRelease;       // the newest resolved release, or NULL
	TInt iIndex;                    // which loaded index it came from
	TPtrC8 Id() const;
	TPtrC8 Name() const;
	TPtrC8 Summary() const;
	TPtrC8 Category() const;
	TPtrC8 Tag() const;
	};

class MPheGetObserver
	{
public:
	virtual void PheGetLog(const TDesC8& aLine) = 0;              // progress, one line
	virtual void PheGetProgress(TInt aDone, TInt aTotal) = 0;     // bytes of the current download (aTotal 0 = unknown)
	};

class CPheGet : public CBase, public MHTTPTransactionCallback
	{
public:
	static CPheGet* NewL(MPheGetObserver& aObserver);
	~CPheGet();

	const TDesC8& Board() const { return iBoard; }
	const TDesC8& Arch() const { return iArch; }

	// Loads the default index (unless aNoDefault), the ones in c:\data\pheget\indexes.txt and aExtra (may be empty).
	// An index that fails is logged and skipped; leaves only if none loaded.
	// With aCachedOnly the URL indexes are read from their cache files (c:\data\pheget\index<n>.json) and nothing is
	// downloaded: the instant start; RefreshIndexesL then updates the caches over the network.
	void LoadIndexesL(const TDesC& aExtra, TBool aNoDefault, TBool aCachedOnly = EFalse);
	// Downloads the URL indexes into their cache files; ETrue when one changed (reload then). Leaves when none could be fetched.
	TBool RefreshIndexesL(const TDesC& aExtra, TBool aNoDefault);
	// Fetches a file (URL or local path) into aFile, through a temporary name; returns the error (KErrNone = done).
	TInt FetchFileL(const TDesC& aLocation, const TDesC& aFile);
	void ResetIndexes() { iIndexes.ResetAndDestroy(); }    // (a refresh; invalidates every TPheGetItem)
	// The items that fit this device (all items with aAll), first index first; an id seen earlier wins.
	void ItemsL(RArray<TPheGetItem>& aItems, TBool aAll);
	TBool FindL(const TDesC8& aId, TPheGetItem& aItem);
	// Downloads (or reuses a cached copy with the right hash), verifies and installs silently; returns the
	// installer's result.
	TInt InstallL(const TPheGetItem& aItem, TChar aDrive);
	// The installed tag of an item (recorded by InstallL), or empty.
	void InstalledTagL(const TDesC8& aId, TDes8& aTag);

	// MHTTPTransactionCallback
	void MHFRunL(RHTTPTransaction aTransaction, const THTTPEvent& aEvent);
	TInt MHFRunError(TInt aError, RHTTPTransaction aTransaction, const THTTPEvent& aEvent);

private:
	CPheGet(MPheGetObserver& aObserver) : iObserver(aObserver) {}
	void ConstructL();
	void ReadDeviceL();
	CDesCArray* LocationsL(const TDesC& aExtra, TBool aNoDefault);
	static void IndexCachePath(TDes& aFile, TInt aSlot);
	void LoadIndexL(const TDesC& aLocation, TInt aSlot, TBool aCachedOnly);
	void FetchL(const TDesC& aLocation, const TDesC& aFile);  // URL or local path -> aFile
	void HttpGetL(const TDesC8& aUrl, const TDesC& aFile);
	void OpenNetworkL();
	TInt StartConnection();
	TBool HashMatchesL(const TDesC& aFile, const TDesC8& aSha256Hex);
	void RecordInstalledL(const TDesC8& aId, const TDesC8& aTag);
	void Log(TRefByValue<const TDesC8> aFmt, ...);

	MPheGetObserver& iObserver;
	RFs iFs;
	TBuf8<32> iBoard;
	TBuf8<16> iArch;
	RPointerArray<CPheJson> iIndexes;
	HBufC8* iInstalledText;                    // installed.txt, read once (InstalledTagL)
	TBool iInstalledLoaded;

	// network (opened on the first HTTP fetch)
	TBool iNetOpen;
	RSocketServ iSockServ;
	RConnection iConnection;
	RHTTPSession iSession;
	TBool iSessionOpen;

	// the HTTP fetch in progress
	CActiveSchedulerWait iWait;
	RFile iOut;
	TInt iHttpError;
	TInt iStatus;
	TInt iDone;
	TInt iTotal;
	};

#endif
