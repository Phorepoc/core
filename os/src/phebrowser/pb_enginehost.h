// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_enginehost.h -- engine discovery and loading for the browser frontend (docs/phorepoc/browser-frontend.md 3.1):
//   * CPbEngines lists the installed engine packages (ECom implementations of PHE_WEBENGINE_ECOM_IFACE_UID) WITHOUT loading them,
//     keeps the user's default engine (by implementation uid) and instantiates the chosen one on demand;
//   * the built-in dummy engine is the last entry in a PHEBROWSER_DUMMY (development) build;
//   * CPbWake turns the engine's any-thread notify into an active-object callback on the UI thread.
#ifndef PB_ENGINEHOST_H
#define PB_ENGINEHOST_H

#include <e32base.h>
#include <f32file.h>
#include <phorepoc/phewebengine.h>

struct TPbEngineInfo
	{
	TUid iUid;               // the ECom implementation uid; 0 = the built-in dummy engine
	TInt iVersion;           // implementation version_no (the package version)
	TBuf<32> iName;          // display_name
	TUint iCaps;             // PheCap bits from the registration's default_data ("caps=script,tls13;api=1")
	TInt iApi;               // the seam version the package was built for
	};

class CPbEngines : public CBase
	{
public:
	static CPbEngines* NewL(RFs& aFs);
	~CPbEngines();
	TInt Count() const { return iList.Count(); }
	const TPbEngineInfo& Info(TInt aIx) const { return iList[aIx]; }
	TInt Default() const { return iDefault; }            // index into the list
	void SetDefault(TInt aIx);                           // persisted by uid
	void Cycle() { if (Count() > 0) SetDefault((iDefault + 1) % Count()); }
	// the chosen engine's table, loading (ECom) it on the first call; NULL when it cannot be loaded or speaks another api version
	const PheWebEngineApi* Load();
	const PheWebEngineApi* Loaded() const { return iApi; }
	TInt LoadedIndex() const { return iLoaded; }
	TInt LastError() const { return iErr; }
	TUint DefaultCaps() const { return iList.Count() ? iList[iDefault].iCaps : 0; }
	void Unload();                                       // after the table's shutdown()
	void Refresh();                                      // re-list (an engine was installed or removed)
private:
	CPbEngines(RFs& aFs) : iFs(aFs) {}
	void ListL();
	void LoadDefault();
	void SaveDefault() const;
	RFs& iFs;
	RArray<TPbEngineInfo> iList;
	TInt iDefault = 0;
	TUint32 iWantUid = 0;
	TInt iLoaded = -1, iErr = 0;
	const PheWebEngineApi* iApi = NULL;
	TAny* iPlugin = NULL;                                // CPheWebEnginePlugin*
	};

// the engine's notify (any thread) -> a callback on the thread that made this object
class CPbWake : public CActive
	{
public:
	static CPbWake* NewL(TCallBack aCb);
	~CPbWake();
	void Signal();                                       // any thread
private:
	CPbWake(TCallBack aCb) : CActive(EPriorityStandard), iCb(aCb) {}
	void Arm();
	void RunL();
	void DoCancel();
	TCallBack iCb;
	TThreadId iTid;
	RFastLock iLock;
	TBool iArmed = EFalse;
	};

#endif
