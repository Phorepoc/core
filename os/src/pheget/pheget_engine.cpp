// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheget_engine.cpp -- OURS: see pheget.h
#include "pheget.h"
#include "phejson.h"

#include <e32debug.h>
#include <bautils.h>
#include <badesca.h>
#include <uri8.h>
#include <hash.h>
#include <http/rhttptransaction.h>
#include <http/rhttpheaders.h>
#include <http/rhttpresponse.h>
#include <http/mhttpdatasupplier.h>
#include <http/rhttpconnectioninfo.h>
#include <httpstringconstants.h>
#include <swinstapi.h>
#include <swinstdefs.h>

_LIT8(KUserAgent, "pheget/1 (Phorepoc)");

// ---- TPheGetItem

TPtrC8 TPheGetItem::Id() const { return iItem->GetStr(_L8("id")); }
TPtrC8 TPheGetItem::Name() const { return iItem->GetStr(_L8("name")); }
TPtrC8 TPheGetItem::Summary() const { return iItem->GetStr(_L8("summary")); }
TPtrC8 TPheGetItem::Category() const { return iItem->GetStr(_L8("category")); }
TPtrC8 TPheGetItem::Tag() const { return iRelease ? iRelease->GetStr(_L8("tag")) : TPtrC8(KNullDesC8); }

// ---- construction

CPheGet* CPheGet::NewL(MPheGetObserver& aObserver)
	{
	CPheGet* self = new (ELeave) CPheGet(aObserver);
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CPheGet::ConstructL()
	{
	User::LeaveIfError(iFs.Connect());
	iFs.MkDirAll(KPheGetDir);
	iFs.MkDirAll(KPheGetDownloads);
	ReadDeviceL();
	PruneCaches();
	}

CPheGet::~CPheGet()
	{
	delete iInstalledText;
	iIndexes.ResetAndDestroy();
	if (iSessionOpen)
		iSession.Close();
	if (iNetOpen)
		{
		iConnection.Close();
		iSockServ.Close();
		}
	iFs.Close();
	}

void CPheGet::Log(TRefByValue<const TDesC8> aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	TBuf8<256> line;
	line.AppendFormatList(aFmt, list);
	VA_END(list);
	iObserver.PheGetLog(line);
	}

static HBufC8* ReadFileL(RFs& aFs, const TDesC& aName)
	{
	RFile f;
	User::LeaveIfError(f.Open(aFs, aName, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TInt size = 0;
	User::LeaveIfError(f.Size(size));
	HBufC8* buf = HBufC8::NewLC(size);
	TPtr8 p = buf->Des();
	User::LeaveIfError(f.Read(p, size));
	CleanupStack::Pop(buf);
	CleanupStack::PopAndDestroy(&f);
	return buf;
	}

// The device's identity: the adaptation ships z:\resource\phorepoc\device.txt ("board=<name>", "arch=arm");
// without it the architecture is the one this program was built for and the board is unknown (= any).
void CPheGet::ReadDeviceL()
	{
#if defined(__arm__) || defined(__MARM__)
	iArch = _L8("arm");
#else
	iArch = _L8("x32");
#endif
	HBufC8* text = NULL;
	TRAPD(err, text = ReadFileL(iFs, KPheGetDeviceFile));
	if (err != KErrNone)
		return;
	TLex8 lex(*text);
	while (!lex.Eos())
		{
		lex.Mark();
		while (!lex.Eos() && lex.Peek() != '\n')
			lex.Inc();
		TPtrC8 line = lex.MarkedToken();
		if (!lex.Eos())
			lex.Inc();
		TInt eq = line.Locate('=');
		if (eq < 0)
			continue;
		TPtrC8 key = line.Left(eq);
		TPtrC8 val = line.Mid(eq + 1);
		while (val.Length() && (val[val.Length() - 1] == '\r' || val[val.Length() - 1] == ' '))
			val.Set(val.Left(val.Length() - 1));
		if (key == _L8("board"))
			iBoard = val.Left(iBoard.MaxLength());
		else if (key == _L8("arch"))
			iArch = val.Left(iArch.MaxLength());
		}
	delete text;
	}

// ---- indexes

static TBool IsUrl(const TDesC& aLocation)
	{
	return aLocation.Left(7).CompareF(_L("http://")) == 0 || aLocation.Left(8).CompareF(_L("https://")) == 0;
	}

// ---- software sources: the default index (unless switched off) + the ones in indexes.txt
//
// indexes.txt: one index URL or local path per line ('#' starts a comment). One comment is a setting:
// "#default=off" switches the default Phorepoc index off. It is honoured only while another source exists, so the
// device can never be left without a source. Written atomically (temporary file, then replace).

_LIT(KPheGetIndexesFile, "indexes.txt");
_LIT8(KPheGetDefaultOffLine, "default=off");

static void TrimLine(TPtrC8& aLine)
	{
	while (aLine.Length() && (aLine[0] == ' ' || aLine[0] == '\t' || aLine[0] == '\r'))
		aLine.Set(aLine.Mid(1));
	while (aLine.Length() && (aLine[aLine.Length() - 1] == ' ' || aLine[aLine.Length() - 1] == '\t' || aLine[aLine.Length() - 1] == '\r'))
		aLine.Set(aLine.Left(aLine.Length() - 1));
	}

static TBool HasLocation(const CDesCArray& aList, const TDesC& aLocation)
	{
	for (TInt i = 0; i < aList.Count(); ++i)
		if (aList[i].CompareF(aLocation) == 0)
			return ETrue;
	return EFalse;
	}

// Reads indexes.txt: the added sources in order (no duplicates), and the default-off setting.
// A missing file is an empty list; any other read error leaves.
void CPheGet::ReadSourcesL(CDesCArray& aSources, TBool& aDefaultOff)
	{
	aDefaultOff = EFalse;
	TFileName list(KPheGetDir);
	list.Append(KPheGetIndexesFile);
	HBufC8* text = NULL;
	TRAPD(err, text = ReadFileL(iFs, list));
	if (err == KErrNotFound || err == KErrPathNotFound)
		return;
	User::LeaveIfError(err);
	CleanupStack::PushL(text);
	TPtrC8 rest(*text);
	while (rest.Length())
		{
		TInt nl = rest.Locate('\n');
		TPtrC8 line = nl >= 0 ? rest.Left(nl) : rest;
		rest.Set(nl >= 0 ? rest.Mid(nl + 1) : TPtrC8());
		TrimLine(line);
		if (line.Length() == 0)
			continue;
		if (line[0] == '#')
			{
			TPtrC8 c = line.Mid(1);
			TrimLine(c);
			if (c.CompareF(KPheGetDefaultOffLine) == 0)
				aDefaultOff = ETrue;
			continue;
			}
		TLex8 lex(line);
		for (TPtrC8 tok = lex.NextToken(); tok.Length() && tok[0] != '#'; tok.Set(lex.NextToken()))
			{
			HBufC* loc = HBufC::NewLC(tok.Length());
			loc->Des().Copy(tok);
			if (!HasLocation(aSources, *loc))
				aSources.AppendL(*loc);
			CleanupStack::PopAndDestroy(loc);
			}
		}
	CleanupStack::PopAndDestroy(text);
	}

void CPheGet::WriteSourcesL(const CDesCArray& aSources, TBool aDefaultOff)
	{
	TInt need = 256;
	for (TInt i = 0; i < aSources.Count(); ++i)
		need += aSources[i].Length() + 1;
	HBufC8* out = HBufC8::NewLC(need);
	TPtr8 o = out->Des();
	o.Append(_L8("# Catalogue software sources: one index URL or local path per line.\n"));
	if (aDefaultOff)
		o.Append(_L8("#default=off\n"));
	for (TInt i = 0; i < aSources.Count(); ++i)
		{
		TBuf8<KMaxFileName> line;
		line.Copy(aSources[i]);
		o.Append(line);
		o.Append('\n');
		}
	TFileName target(KPheGetDir);
	target.Append(KPheGetIndexesFile);
	TFileName tmp(target);
	tmp.Append(_L(".tmp"));
	RFile f;
	User::LeaveIfError(f.Replace(iFs, tmp, EFileWrite));
	TInt r = f.Write(o);
	if (r == KErrNone)
		r = f.Flush();
	f.Close();
	if (r == KErrNone)
		r = iFs.Replace(tmp, target);
	if (r != KErrNone)
		iFs.Delete(tmp);
	CleanupStack::PopAndDestroy(out);
	User::LeaveIfError(r);
	}

TBool CPheGet::ValidSource(const TDesC& aLocation)
	{
	TInt n = aLocation.Length();
	if (n == 0 || n > KMaxFileName)
		return EFalse;
	for (TInt i = 0; i < n; ++i)
		if (aLocation[i] <= ' ' || aLocation[i] > '~' || aLocation[i] == '#')   // (the file format: one token per entry)
			return EFalse;
	if (IsUrl(aLocation))
		{
		TInt start = aLocation.Locate(':') + 3;               // past "://"
		TPtrC hostAndPath = aLocation.Mid(start);
		TInt end = hostAndPath.Locate('/');
		TPtrC host = end >= 0 ? hostAndPath.Left(end) : hostAndPath;
		return host.Length() > 0 && host[0] != ':' && host[0] != '@' && host[0] != '?';
		}
	// a local path: a drive and a path ("c:\data\index.json", '/' accepted), naming a file
	if (n < 4 || !TChar(aLocation[0]).IsAlpha() || aLocation[1] != ':' || (aLocation[2] != '\\' && aLocation[2] != '/'))
		return EFalse;
	TChar last(aLocation[n - 1]);
	return last != '\\' && last != '/';
	}

void CPheGet::SourcesL(CDesCArray& aSources)
	{
	TBool off;
	ReadSourcesL(aSources, off);
	}

TBool CPheGet::DefaultIndexEnabledL()
	{
	CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(sources);
	TBool off;
	ReadSourcesL(*sources, off);
	TBool on = !off || sources->Count() == 0;
	CleanupStack::PopAndDestroy(sources);
	return on;
	}

void CPheGet::AddSourceL(const TDesC& aLocation)
	{
	if (!ValidSource(aLocation))
		User::Leave(KErrArgument);
	CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(sources);
	TBool off;
	ReadSourcesL(*sources, off);
	if (HasLocation(*sources, aLocation) || aLocation.CompareF(KPheGetDefaultIndex) == 0)
		User::Leave(KErrAlreadyExists);
	sources->AppendL(aLocation);
	WriteSourcesL(*sources, off);
	CleanupStack::PopAndDestroy(sources);
	}

// Removes the aIndex-th added source and its cache. With the last one gone the default index is on again.
void CPheGet::RemoveSourceL(TInt aIndex)
	{
	CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(sources);
	TBool off;
	ReadSourcesL(*sources, off);
	if (aIndex < 0 || aIndex >= sources->Count())
		User::Leave(KErrNotFound);
	TFileName cache;
	IndexCachePath(cache, (*sources)[aIndex]);
	sources->Delete(aIndex);
	if (sources->Count() == 0)
		off = EFalse;
	WriteSourcesL(*sources, off);
	iFs.Delete(cache);
	CleanupStack::PopAndDestroy(sources);
	}

// Switches the default Phorepoc index on or off. Off needs another source (KErrNotSupported without one).
void CPheGet::SetDefaultIndexL(TBool aOn)
	{
	CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(sources);
	TBool off;
	ReadSourcesL(*sources, off);
	if (!aOn && sources->Count() == 0)
		User::Leave(KErrNotSupported);
	WriteSourcesL(*sources, !aOn);
	CleanupStack::PopAndDestroy(sources);
	}

// The locations to load: aExtra, the default index (unless aNoDefault, or switched off while another source exists),
// the added sources. Each once.
CDesCArray* CPheGet::LocationsL(const TDesC& aExtra, TBool aNoDefault)
	{
	CDesCArrayFlat* locations = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(locations);
	CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(sources);
	TBool off = EFalse;
	TRAP_IGNORE(ReadSourcesL(*sources, off));
	if (aExtra.Length())
		locations->AppendL(aExtra);
	if (!aNoDefault && (!off || sources->Count() == 0) && !HasLocation(*locations, KPheGetDefaultIndex))
		locations->AppendL(KPheGetDefaultIndex);
	for (TInt i = 0; i < sources->Count(); ++i)
		if (!HasLocation(*locations, (*sources)[i]))
			locations->AppendL((*sources)[i]);
	CleanupStack::PopAndDestroy(sources);
	CleanupStack::Pop(locations);
	return locations;
	}

void CPheGet::LoadIndexesL(const TDesC& aExtra, TBool aNoDefault, TBool aCachedOnly)
	{
	CDesCArray* locations = LocationsL(aExtra, aNoDefault);
	CleanupStack::PushL(locations);
	for (TInt i = 0; i < locations->Count(); ++i)
		{
		TRAPD(r, LoadIndexL((*locations)[i], aCachedOnly));
		TBuf8<200> loc8;
		loc8.Copy((*locations)[i].Left(200));
		if (r != KErrNone)
			Log(_L8("index %S: %s (%d)"), &loc8, aCachedOnly ? "no cached copy" : "failed", r);
		else
			Log(_L8("index %S: %d items"), &loc8, iIndexes[iIndexes.Count() - 1]->Get(_L8("items"))->Count());
		}
	CleanupStack::PopAndDestroy(locations);
	if (iIndexes.Count() == 0)
		User::Leave(KErrNotFound);
	}

// Downloads every URL index into its cache file (c:\data\pheget\ix<hash>.json) without parsing it; ETrue when one
// differs from the copy cached before (the caller then reloads). A download that fails leaves the cache as it was.
TBool CPheGet::RefreshIndexesL(const TDesC& aExtra, TBool aNoDefault)
	{
	CDesCArray* locations = LocationsL(aExtra, aNoDefault);
	CleanupStack::PushL(locations);
	TBool changed = EFalse;
	TInt fetched = 0;
	TInt urls = 0;
	for (TInt i = 0; i < locations->Count(); ++i)
		{
		if (!IsUrl((*locations)[i]))
			continue;
		++urls;
		TFileName file, tmp;
		IndexCachePath(file, (*locations)[i]);
		tmp = file;
		tmp.Append(_L(".tmp"));
		TRAPD(r, FetchL((*locations)[i], tmp));
		if (r != KErrNone)
			{
			iFs.Delete(tmp);
			continue;
			}
		++fetched;
		HBufC8* fresh = NULL;
		HBufC8* old = NULL;
		TRAPD(r1, fresh = ReadFileL(iFs, tmp));
		TRAPD(r2, old = ReadFileL(iFs, file));
		if (r1 == KErrNone && (r2 != KErrNone || *fresh != *old))
			{
			changed = ETrue;
			iFs.Delete(file);
			iFs.Rename(tmp, file);
			}
		else
			iFs.Delete(tmp);
		delete fresh;
		delete old;
		}
	CleanupStack::PopAndDestroy(locations);
	if (urls > 0 && fetched == 0)
		User::Leave(KErrCouldNotConnect);          // (only local sources: nothing to download)
	return changed;
	}

// The cache of a source is named after the source itself (two 32-bit FNV-1a style hashes of its text, case-folded),
// not its position: removing or reordering sources can never show one source's cache as another's.
void CPheGet::IndexCachePath(TDes& aFile, const TDesC& aLocation)
	{
	TUint32 h1 = 2166136261u, h2 = 0x9747b28cu;
	for (TInt i = 0; i < aLocation.Length(); ++i)
		{
		TChar folded(aLocation[i]);
		folded.Fold();
		TUint32 c = folded;
		h1 = (h1 ^ c) * 16777619u;
		h2 = (h2 ^ c) * 0x01000193u + 0x9e3779b9u;
		}
	aFile = KPheGetDir;
	aFile.AppendFormat(_L("ix%08x%08x.json"), h1, h2);
	}

// Deletes the cache files no current source owns (a removed source, the old numbered caches). Best effort.
void CPheGet::PruneCaches()
	{
	TBool off;
	CDesCArrayFlat* sources = new CDesCArrayFlat(4);
	if (sources)
		{
		TRAP_IGNORE(ReadSourcesL(*sources, off));       // (a disabled default's cache stays: switching it on again needs no wait)
		TRAP_IGNORE(sources->AppendL(KPheGetDefaultIndex));
		}
	TFileName pattern(KPheGetDir);
	pattern.Append(_L("*.json"));
	CDir* dir = NULL;
	if (iFs.GetDir(pattern, KEntryAttNormal, ESortNone, dir) == KErrNone)
		{
		for (TInt i = 0; i < dir->Count(); ++i)
			{
			TBool owned = EFalse;
			const TDesC& name = (*dir)[i].iName;
			for (TInt k = 0; sources && k < sources->Count() && !owned; ++k)
				{
				TFileName cache;
				IndexCachePath(cache, (*sources)[k]);
				owned = cache.Mid(KPheGetDir().Length()).CompareF(name) == 0;
				}
			if (!owned && (name.Left(5).CompareF(_L("index")) == 0 || name.Left(2).CompareF(_L("ix")) == 0))
				{
				TFileName path(KPheGetDir);
				path.Append(name);
				iFs.Delete(path);
				}
			}
		}
	delete dir;
	delete sources;
	}

void CPheGet::LoadIndexL(const TDesC& aLocation, TBool aCachedOnly)
	{
	TFileName file;
	if (IsUrl(aLocation))
		{
		IndexCachePath(file, aLocation);
		if (!aCachedOnly)
			{
			TFileName tmp(file);
			tmp.Append(_L(".tmp"));
			FetchL(aLocation, tmp);
			iFs.Delete(file);
			iFs.Rename(tmp, file);
			}
		}
	else
		{
		file = aLocation;
		for (TInt i = 0; i < file.Length(); ++i)
			if (file[i] == '/')
				file[i] = '\\';
		}
	HBufC8* text = ReadFileL(iFs, file);
	CleanupStack::PushL(text);
	CPheJson* root = CPheJson::ParseL(*text);
	CleanupStack::PopAndDestroy(text);
	CleanupStack::PushL(root);
	const CPheJson* items = root->Type() == CPheJson::EObject ? root->Get(_L8("items")) : NULL;
	if (!items || items->Type() != CPheJson::EArray)
		User::Leave(KErrCorrupt);
	iIndexes.AppendL(root);
	CleanupStack::Pop(root);
	}

TInt CPheGet::FetchFileL(const TDesC& aLocation, const TDesC& aFile)
	{
	TFileName tmp(aFile);
	tmp.Append(_L(".tmp"));
	TRAPD(r, FetchL(aLocation, tmp));
	if (r != KErrNone)
		{
		iFs.Delete(tmp);
		return r;
		}
	iFs.Delete(aFile);
	return iFs.Rename(tmp, aFile);
	}

static const CPheJson* NewestRelease(const CPheJson& aItem)
	{
	const CPheJson* rel = aItem.Get(_L8("resolved"));
	if (!rel || rel->Type() != CPheJson::EArray)
		return NULL;
	const CPheJson* best = NULL;
	for (TInt i = 0; i < rel->Count(); ++i)
		{
		const CPheJson& r = rel->At(i);
		if (r.Type() != CPheJson::EObject)
			continue;
		if (!best || r.GetStr(_L8("published")).Compare(best->GetStr(_L8("published"))) > 0)
			best = &r;
		}
	return best;
	}

void CPheGet::ItemsL(RArray<TPheGetItem>& aItems, TBool aAll)
	{
	for (TInt x = 0; x < iIndexes.Count(); ++x)
		{
		const CPheJson* items = iIndexes[x]->Get(_L8("items"));
		for (TInt i = 0; i < items->Count(); ++i)
			{
			const CPheJson& it = items->At(i);
			if (it.Type() != CPheJson::EObject || it.GetStr(_L8("id")).Length() == 0)
				continue;
			if (!aAll)
				{
				const CPheJson* compat = it.Get(_L8("compatibility"));
				const CPheJson* arch = compat ? compat->Get(_L8("arch")) : NULL;
				const CPheJson* boards = compat ? compat->Get(_L8("boards")) : NULL;
				if (!arch || !arch->ArrayHas(iArch))
					continue;
				if (iBoard.Length() && boards && boards->Count() && !boards->ArrayHas(iBoard))
					continue;
				}
			TBool dup = EFalse;
			for (TInt k = 0; k < aItems.Count() && !dup; ++k)
				dup = aItems[k].Id() == it.GetStr(_L8("id"));
			if (dup)
				continue;
			TPheGetItem item;
			item.iItem = &it;
			item.iRelease = NewestRelease(it);
			item.iIndex = x;
			aItems.AppendL(item);
			}
		}
	}

TBool CPheGet::FindL(const TDesC8& aId, TPheGetItem& aItem)
	{
	RArray<TPheGetItem> items;
	CleanupClosePushL(items);
	ItemsL(items, ETrue);
	TBool found = EFalse;
	for (TInt i = 0; i < items.Count() && !found; ++i)
		{
		if (items[i].Id() == aId)
			{
			aItem = items[i];
			found = ETrue;
			}
		}
	CleanupStack::PopAndDestroy(&items);
	return found;
	}

// ---- fetching

void CPheGet::FetchL(const TDesC& aLocation, const TDesC& aFile)
	{
	if (!IsUrl(aLocation))
		{
		TFileName src(aLocation);            // a local path (an offline index or package); '/' accepted too
		for (TInt i = 0; i < src.Length(); ++i)
			if (src[i] == '/')
				src[i] = '\\';
		CFileMan* fm = CFileMan::NewL(iFs);
		CleanupStack::PushL(fm);
		User::LeaveIfError(fm->Copy(src, aFile, CFileMan::EOverWrite));
		CleanupStack::PopAndDestroy(fm);
		return;
		}
	HBufC8* url = HBufC8::NewLC(aLocation.Length());
	url->Des().Copy(aLocation);
	HttpGetL(*url, aFile);
	CleanupStack::PopAndDestroy(url);
	}

// RConnection::Start() blocks the whole thread while the link comes up (many seconds); the asynchronous form with a
// nested scheduler keeps a UI that called us alive meanwhile.
class CPheConnStart : public CActive
	{
public:
	CPheConnStart() : CActive(CActive::EPriorityStandard) { CActiveScheduler::Add(this); }
	~CPheConnStart() { Cancel(); }
	TInt Run(RConnection& aConnection)
		{
		iConnection = &aConnection;
		aConnection.Start(iStatus);
		SetActive();
		iWait.Start();
		return iStatus.Int();
		}
private:
	void RunL() { iWait.AsyncStop(); }
	void DoCancel() { iConnection->Close(); }   // (no StartCancel: closing the connection cancels it)
	RConnection* iConnection;
	CActiveSchedulerWait iWait;
	};

TInt CPheGet::StartConnection()
	{
	CPheConnStart* starter = new CPheConnStart;
	if (!starter)
		return iConnection.Start();
	TInt r = starter->Run(iConnection);
	delete starter;
	return r;
	}

void CPheGet::OpenNetworkL()
	{
	if (iSessionOpen)
		return;
	if (!iNetOpen)
		{
		User::LeaveIfError(iSockServ.Connect());
		TInt r = iConnection.Open(iSockServ);
		if (r == KErrNone)
			r = StartConnection();         // the default connection (the "Internet" destination)
		if (r != KErrNone)
			{
			iConnection.Close();
			iSockServ.Close();
			User::Leave(r);
			}
		iNetOpen = ETrue;
		}
	iSession.OpenL();
	iSessionOpen = ETrue;
	RStringPool pool = iSession.StringPool();
	RHTTPConnectionInfo info = iSession.ConnectionInfo();
	info.SetPropertyL(pool.StringF(HTTP::EHttpSocketServ, RHTTPSession::GetTable()), THTTPHdrVal(iSockServ.Handle()));
	info.SetPropertyL(pool.StringF(HTTP::EHttpSocketConnection, RHTTPSession::GetTable()),
					  THTTPHdrVal(reinterpret_cast<TInt>(&iConnection)));
	}

void CPheGet::HttpGetL(const TDesC8& aUrl, const TDesC& aFile)
	{
	OpenNetworkL();
	TUriParser8 uri;
	User::LeaveIfError(uri.Parse(aUrl));
	User::LeaveIfError(iOut.Replace(iFs, aFile, EFileWrite));
	iHttpError = KErrNone;
	iStatus = 0;
	iDone = 0;
	iTotal = 0;
	RStringPool pool = iSession.StringPool();
	RHTTPTransaction t = iSession.OpenTransactionL(uri, *this, pool.StringF(HTTP::EGET, RHTTPSession::GetTable()));
	CleanupClosePushL(t);
	RStringF ua = pool.OpenFStringL(KUserAgent);
	CleanupClosePushL(ua);
	t.Request().GetHeaderCollection().SetFieldL(pool.StringF(HTTP::EUserAgent, RHTTPSession::GetTable()), THTTPHdrVal(ua));
	CleanupStack::PopAndDestroy(&ua);
	t.SubmitL();
	iWait.Start();                         // MHFRunL stops it on ESucceeded / EFailed
	CleanupStack::PopAndDestroy(&t);
	iOut.Close();
	if (iHttpError == KErrNone && iStatus != 200)
		iHttpError = (iStatus == 404) ? KErrNotFound : KErrGeneral;
	if (iHttpError != KErrNone)
		{
		iFs.Delete(aFile);
		Log(_L8("http: %S -> status %d, error %d"), &aUrl, iStatus, iHttpError);
		User::Leave(iHttpError);
		}
	}

void CPheGet::MHFRunL(RHTTPTransaction aTransaction, const THTTPEvent& aEvent)
	{
	switch (aEvent.iStatus)
		{
		case THTTPEvent::EGotResponseHeaders:
			{
			RHTTPResponse resp = aTransaction.Response();
			iStatus = resp.StatusCode();
			iDone = 0;
			iOut.SetSize(0);
			THTTPHdrVal len;
			RStringPool pool = iSession.StringPool();
			if (resp.GetHeaderCollection().GetField(pool.StringF(HTTP::EContentLength, RHTTPSession::GetTable()), 0, len) == KErrNone
				&& len.Type() == THTTPHdrVal::KTIntVal)
				iTotal = len.Int();
			break;
			}
		case THTTPEvent::EGotResponseBodyData:
			{
			MHTTPDataSupplier* body = aTransaction.Response().Body();
			TPtrC8 part;
			body->GetNextDataPart(part);
			if (iStatus == 200 && iHttpError == KErrNone)
				{
				TInt r = iOut.Write(part);
				if (r != KErrNone)
					iHttpError = r;
				iDone += part.Length();
				iObserver.PheGetProgress(iDone, iTotal);
				}
			body->ReleaseData();
			break;
			}
		case THTTPEvent::ESucceeded:
			iWait.AsyncStop();
			break;
		case THTTPEvent::EFailed:
			if (iHttpError == KErrNone && iStatus == 200)
				iHttpError = KErrGeneral;
			iWait.AsyncStop();
			break;
		default:
			if (aEvent.iStatus < 0 && iHttpError == KErrNone)
				iHttpError = aEvent.iStatus;   // EFailed follows
			break;
		}
	}

TInt CPheGet::MHFRunError(TInt aError, RHTTPTransaction /*aTransaction*/, const THTTPEvent& /*aEvent*/)
	{
	if (iHttpError == KErrNone)
		iHttpError = aError;
	return KErrNone;
	}

// ---- install

TBool CPheGet::HashMatchesL(const TDesC& aFile, const TDesC8& aSha256Hex)
	{
	RFile f;
	if (f.Open(iFs, aFile, EFileRead | EFileShareReadersOnly) != KErrNone)
		return EFalse;
	CleanupClosePushL(f);
	CSHA2* sha = CSHA2::NewL(E256Bit);
	CleanupStack::PushL(sha);
	HBufC8* chunk = HBufC8::NewLC(16384);
	TPtr8 p = chunk->Des();
	for (;;)
		{
		User::LeaveIfError(f.Read(p));
		if (p.Length() == 0)
			break;
		sha->Update(p);
		}
	TPtrC8 digest = sha->Final();
	TBuf8<64> hex;
	for (TInt i = 0; i < digest.Length(); ++i)
		hex.AppendFormat(_L8("%02x"), digest[i]);
	TBool ok = hex.CompareF(aSha256Hex) == 0;
	CleanupStack::PopAndDestroy(3, &f);
	return ok;
	}

TInt CPheGet::InstallL(const TPheGetItem& aItem, TChar aDrive)
	{
	TPtrC8 id = aItem.Id();
	if (!aItem.iRelease)
		{
		Log(_L8("%S: no approved release"), &id);
		return KErrNotFound;
		}
	const CPheJson& rel = *aItem.iRelease;
	TPtrC8 asset = rel.GetStr(_L8("asset"));
	TPtrC8 url = rel.GetStr(_L8("url"));
	TPtrC8 sha = rel.GetStr(_L8("sha256"));
	TPtrC8 tag = rel.GetStr(_L8("tag"));
	if (asset.Length() == 0 || asset.Length() > 100 || asset.Locate('\\') >= 0 || asset.Locate('/') >= 0
		|| asset.Find(_L8("..")) >= 0 || url.Length() == 0 || sha.Length() != 64)
		{
		Log(_L8("%S: the index entry is incomplete"), &id);
		return KErrCorrupt;
		}
	TFileName file(KPheGetDownloads);
	TBuf<100> asset16;
	asset16.Copy(asset);
	file.Append(asset16);

	if (HashMatchesL(file, sha))
		Log(_L8("%S %S: cached"), &id, &tag);
	else
		{
		Log(_L8("%S %S: downloading %S"), &id, &tag, &url);
		HBufC* url16 = HBufC::NewLC(url.Length());
		url16->Des().Copy(url);
		FetchL(*url16, file);
		CleanupStack::PopAndDestroy(url16);
		if (!HashMatchesL(file, sha))
			{
			iFs.Delete(file);
			Log(_L8("%S: SHA-256 mismatch, removed"), &id);
			return KErrCorrupt;
			}
		}

	SwiUI::TInstallOptions opts;
	opts.iUpgrade = SwiUI::EPolicyAllowed;
	opts.iOptionalItems = SwiUI::EPolicyAllowed;
	opts.iOCSP = SwiUI::EPolicyNotAllowed;
	opts.iIgnoreOCSPWarnings = SwiUI::EPolicyAllowed;
	opts.iUntrusted = SwiUI::EPolicyAllowed;
	opts.iPackageInfo = SwiUI::EPolicyAllowed;
	opts.iCapabilities = SwiUI::EPolicyAllowed;
	opts.iKillApp = SwiUI::EPolicyAllowed;
	opts.iOverwrite = SwiUI::EPolicyAllowed;
	opts.iDownload = SwiUI::EPolicyAllowed;
	opts.iDrive = aDrive;
	opts.iUsePhoneLang = ETrue;
	opts.iUpgradeData = SwiUI::EPolicyAllowed;
	SwiUI::TInstallOptionsPckg pckg(opts);
	SwiUI::RSWInstSilentLauncher launcher;
	TInt err = launcher.Connect();
	if (err == KErrNone)
		{
		err = launcher.SilentInstall(file, pckg);
		launcher.Close();
		}
	if (err == KErrNone)
		RecordInstalledL(id, tag);
	Log(_L8("%S %S: %s (%d)"), &id, &tag, err == KErrNone ? "INSTALLED" : "FAILED", err);
	return err;
	}

// installed.txt: one "<id> <tag>" line per installed item
void CPheGet::InstalledTagL(const TDesC8& aId, TDes8& aTag)
	{
	aTag.Zero();
	if (!iInstalledLoaded)
		{
		TFileName list(KPheGetDir);
		list.Append(_L("installed.txt"));
		delete iInstalledText;
		iInstalledText = NULL;
		TRAP_IGNORE(iInstalledText = ReadFileL(iFs, list));
		iInstalledLoaded = ETrue;
		}
	if (!iInstalledText)
		return;
	TLex8 lex(*iInstalledText);
	while (!lex.Eos())
		{
		TPtrC8 id = lex.NextToken();
		TPtrC8 tag = lex.NextToken();
		if (id == aId)
			aTag = tag.Left(aTag.MaxLength());
		}
	}

void CPheGet::RecordInstalledL(const TDesC8& aId, const TDesC8& aTag)
	{
	iInstalledLoaded = EFalse;                  // (InstalledTagL rereads)
	TFileName list(KPheGetDir);
	list.Append(_L("installed.txt"));
	HBufC8* text = NULL;
	TRAPD(err, text = ReadFileL(iFs, list));
	if (err != KErrNone)
		text = HBufC8::NewL(0);
	CleanupStack::PushL(text);
	HBufC8* out = HBufC8::NewLC(text->Length() + aId.Length() + aTag.Length() + 16);
	TPtr8 o = out->Des();
	TLex8 lex(*text);
	while (!lex.Eos())
		{
		TPtrC8 id = lex.NextToken();
		TPtrC8 tag = lex.NextToken();
		if (id.Length() == 0 || id == aId)
			continue;
		o.Append(id);
		o.Append(' ');
		o.Append(tag);
		o.Append('\n');
		}
	o.Append(aId);
	o.Append(' ');
	o.Append(aTag);
	o.Append('\n');
	RFile f;
	User::LeaveIfError(f.Replace(iFs, list, EFileWrite));
	CleanupClosePushL(f);
	User::LeaveIfError(f.Write(o));
	CleanupStack::PopAndDestroy(3, text);
	}
