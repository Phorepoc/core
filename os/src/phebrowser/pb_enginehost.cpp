// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_enginehost.cpp -- see pb_enginehost.h
#include "pb_enginehost.h"
#include "pb_engines.h"
#include <phorepoc/phewebengine_ecom.h>
#include <e32debug.h>

static void IniPath(RFs& aFs, TDes& aOut)
	{
	aFs.PrivatePath(aOut);
	aOut.Insert(0, _L("C:"));
	aOut.Append(_L("engine.ini"));
	}

static void DestroyInfos(TAny* aArr) { static_cast<RImplInfoPtrArray*>(aArr)->ResetAndDestroy(); }

// "caps=script,tls13,find;api=1" -> bits
static void ParseData(const TDesC8& aData, TUint& aCaps, TInt& aApi)
	{
	static const struct { const char* n; TUint b; } K[] =
		{ {"script", PHE_CAP_SCRIPT}, {"tls13", PHE_CAP_TLS13}, {"find", PHE_CAP_FIND}, {"snapshot", PHE_CAP_SNAPSHOT},
		  {"zoom", PHE_CAP_ZOOM}, {"download", PHE_CAP_DOWNLOAD}, {"private", PHE_CAP_PRIVATE}, {"suspend", PHE_CAP_SUSPEND},
		  {"state", PHE_CAP_STATE}, {"reflow", PHE_CAP_REFLOW} };
	aCaps = 0; aApi = 1;
	TInt p = aData.Find(_L8("caps="));
	if (p != KErrNotFound)
		{
		TPtrC8 rest = aData.Mid(p + 5);
		TInt e = rest.Locate(';');
		if (e >= 0) rest.Set(rest.Left(e));
		for (TUint i = 0; i < sizeof(K) / sizeof(K[0]); ++i)
			{
			TPtrC8 name((const TUint8*)K[i].n, User::StringLength((const TUint8*)K[i].n));
			TInt q = rest.Find(name);
			if (q != KErrNotFound && (q == 0 || rest[q - 1] == ',') && (q + name.Length() == rest.Length() || rest[q + name.Length()] == ','))
				aCaps |= K[i].b;
			}
		}
	p = aData.Find(_L8("api="));
	if (p != KErrNotFound)
		{
		TLex8 lex(aData.Mid(p + 4));
		lex.Val(aApi);
		}
	}

CPbEngines* CPbEngines::NewL(RFs& aFs)
	{
	CPbEngines* e = new (ELeave) CPbEngines(aFs);
	CleanupStack::PushL(e);
	TRAP_IGNORE(e->ListL());        // a failed listing = no engines (the store card), not a failed start
	e->LoadDefault();
	CleanupStack::Pop(e);
	return e;
	}

CPbEngines::~CPbEngines()
	{
	Unload();
	iList.Close();
	}

void CPbEngines::ListL()
	{
	iList.Reset();
	RImplInfoPtrArray infos;
	CleanupStack::PushL(TCleanupItem(DestroyInfos, &infos));
	REComSession::ListImplementationsL(TUid::Uid(PHE_WEBENGINE_ECOM_IFACE_UID), infos);
	for (TInt i = 0; i < infos.Count(); ++i)
		{
		TPbEngineInfo n;
		n.iUid = infos[i]->ImplementationUid();
		n.iVersion = infos[i]->Version();
		n.iName.Copy(infos[i]->DisplayName().Left(32));
		ParseData(infos[i]->DataType(), n.iCaps, n.iApi);
		RDebug::Printf("PH: [phebrowser] engine %x '%S' v%d caps %x api %d", n.iUid.iUid, &n.iName, n.iVersion, n.iCaps, n.iApi);
		iList.AppendL(n);
		}
	CleanupStack::PopAndDestroy();
#ifdef PHEBROWSER_DUMMY
	TPbEngineInfo d;                // development builds: the placeholder engine is the last entry
	d.iUid = TUid::Null(); d.iVersion = 1; d.iName = _L("Developer engine"); d.iApi = 1;
	d.iCaps = PheDummyGetApi(PHE_WEBENGINE_VERSION)->caps;
	iList.AppendL(d);
#endif
	}

void CPbEngines::Refresh()
	{
	TRAP_IGNORE(ListL());
	LoadDefault();
	}

void CPbEngines::LoadDefault()
	{
	iWantUid = 0;
	TFileName f;
	IniPath(iFs, f);
	RFile file;
	if (file.Open(iFs, f, EFileRead | EFileShareReadersOnly) == KErrNone)
		{
		TBuf8<64> b;
		file.Read(b);
		file.Close();
		TInt p = b.Find(_L8("engine="));
		if (p != KErrNotFound)
			{
			TLex8 lex(b.Mid(p + 7));
			TUint32 u = 0;
			if (lex.Val(u, EHex) == KErrNone)
				iWantUid = u;
			}
		}
	// no (or a stale) choice: WebKit, else NetSurf, else any other installed package; the built-in placeholder (uid 0) only as the fallback
	iDefault = -1;
	for (TInt i = 0; i < iList.Count(); ++i)
		if (iList[i].iUid.iUid == iWantUid && iWantUid)
			iDefault = i;
	static const TUint32 KPref[] = { 0xE7016B10u, 0xE7016B11u };
	for (TUint k = 0; iDefault < 0 && k < sizeof(KPref) / sizeof(KPref[0]); ++k)
		for (TInt i = 0; i < iList.Count(); ++i)
			if (iList[i].iUid.iUid == KPref[k])
				{ iDefault = i; break; }
	for (TInt i = 0; iDefault < 0 && i < iList.Count(); ++i)
		if (iList[i].iUid.iUid != 0)
			iDefault = i;
	if (iDefault < 0)
		iDefault = 0;
	}

void CPbEngines::SetDefault(TInt aIx)
	{
	if (aIx < 0 || aIx >= iList.Count())
		return;
	iDefault = aIx;
	iWantUid = iList[aIx].iUid.iUid;
	SaveDefault();
	}

void CPbEngines::SaveDefault() const
	{
	TFileName f;
	iFs.CreatePrivatePath(EDriveC);
	IniPath(iFs, f);
	RFile file;
	if (file.Replace(iFs, f, EFileWrite) != KErrNone)
		return;
	TBuf8<48> b;
	b.Format(_L8("engine=%x\n"), iWantUid);
	file.Write(b);
	file.Close();
	}

const PheWebEngineApi* CPbEngines::Load()
	{
	if (iApi)
		return iApi;
	iErr = KErrNotFound;
	if (iDefault < 0 || iDefault >= iList.Count())
		return NULL;
	const TPbEngineInfo& n = iList[iDefault];
	if (n.iApi != PHE_WEBENGINE_VERSION)
		{ iErr = KErrNotSupported; return NULL; }
	if (n.iUid.iUid == 0)
		{
#ifdef PHEBROWSER_DUMMY
		iApi = PheDummyGetApi(PHE_WEBENGINE_VERSION);
		iLoaded = iDefault;
		iErr = iApi ? KErrNone : KErrNotSupported;
#endif
		return iApi;
		}
	CPheWebEnginePlugin* p = NULL;
	TRAP(iErr, p = CPheWebEnginePlugin::NewL(n.iUid));
	RDebug::Printf("PH: [phebrowser] load engine %x -> %d", n.iUid.iUid, iErr);
	if (iErr != KErrNone || !p)
		return NULL;
	iApi = p->Api();
	if (!iApi || iApi->version != PHE_WEBENGINE_VERSION)
		{ delete p; iApi = NULL; iErr = KErrNotSupported; return NULL; }
	iPlugin = p;
	iLoaded = iDefault;
	return iApi;
	}

void CPbEngines::Unload()
	{
	delete static_cast<CPheWebEnginePlugin*>(iPlugin);
	iPlugin = NULL;
	iApi = NULL;
	iLoaded = -1;
	}

// ---- CPbWake -----------------------------------------------------------------------------------------------------------------------
CPbWake* CPbWake::NewL(TCallBack aCb)
	{
	CPbWake* w = new (ELeave) CPbWake(aCb);
	CleanupStack::PushL(w);
	User::LeaveIfError(w->iLock.CreateLocal());
	w->iTid = RThread().Id();
	CActiveScheduler::Add(w);
	w->Arm();
	CleanupStack::Pop(w);
	return w;
	}

CPbWake::~CPbWake()
	{
	Cancel();
	iLock.Close();
	}

void CPbWake::Arm()
	{
	iLock.Wait();
	iStatus = KRequestPending;
	SetActive();
	iArmed = ETrue;
	iLock.Signal();
	}

void CPbWake::Signal()
	{
	iLock.Wait();
	if (iArmed)
		{
		iArmed = EFalse;
		RThread t;
		if (t.Open(iTid) == KErrNone)
			{
			TRequestStatus* s = &iStatus;
			t.RequestComplete(s, KErrNone);
			t.Close();
			}
		}
	iLock.Signal();
	}

void CPbWake::RunL()
	{
	Arm();                     // re-arm FIRST: a signal during the callback is not lost
	iCb.CallBack();
	}

void CPbWake::DoCancel()
	{
	iLock.Wait();
	if (iArmed)
		{
		iArmed = EFalse;
		TRequestStatus* s = &iStatus;
		User::RequestComplete(s, KErrCancel);
		}
	iLock.Signal();
	}
