// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

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
	}

CPheGet::~CPheGet()
	{
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

// The device's identity: the adaptation ships z:\resource\phorepoc\device.txt ("board=pinephone", "arch=arm");
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

void CPheGet::LoadIndexesL(const TDesC& aExtra, TBool aNoDefault)
	{
	CDesCArrayFlat* locations = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(locations);
	if (aExtra.Length())
		locations->AppendL(aExtra);
	if (!aNoDefault)
		locations->AppendL(KPheGetDefaultIndex);
	TFileName list(KPheGetDir);
	list.Append(_L("indexes.txt"));
	HBufC8* text = NULL;
	TRAPD(err, text = ReadFileL(iFs, list));
	if (err == KErrNone)
		{
		CleanupStack::PushL(text);
		TLex8 lex(*text);
		for (TPtrC8 tok = lex.NextToken(); tok.Length(); tok.Set(lex.NextToken()))
			{
			if (tok[0] == '#')
				{
				while (!lex.Eos() && lex.Peek() != '\n')
					lex.Inc();
				continue;
				}
			HBufC* loc = HBufC::NewLC(tok.Length());
			loc->Des().Copy(tok);
			locations->AppendL(*loc);
			CleanupStack::PopAndDestroy(loc);
			}
		CleanupStack::PopAndDestroy(text);
		}
	for (TInt i = 0; i < locations->Count(); ++i)
		{
		TRAPD(r, LoadIndexL((*locations)[i]));
		TBuf8<200> loc8;
		loc8.Copy((*locations)[i].Left(200));
		if (r != KErrNone)
			Log(_L8("index %S: failed (%d)"), &loc8, r);
		else
			Log(_L8("index %S: %d items"), &loc8, iIndexes[iIndexes.Count() - 1]->Get(_L8("items"))->Count());
		}
	CleanupStack::PopAndDestroy(locations);
	if (iIndexes.Count() == 0)
		User::Leave(KErrNotFound);
	}

void CPheGet::LoadIndexL(const TDesC& aLocation)
	{
	TFileName file;
	if (IsUrl(aLocation))
		{
		file = KPheGetDir;
		file.AppendFormat(_L("index%d.json"), iIndexes.Count());
		FetchL(aLocation, file);
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

void CPheGet::OpenNetworkL()
	{
	if (iSessionOpen)
		return;
	if (!iNetOpen)
		{
		User::LeaveIfError(iSockServ.Connect());
		TInt r = iConnection.Open(iSockServ);
		if (r == KErrNone)
			r = iConnection.Start();       // the default connection (the "Internet" destination)
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
	TFileName list(KPheGetDir);
	list.Append(_L("installed.txt"));
	HBufC8* text = NULL;
	TRAPD(err, text = ReadFileL(iFs, list));
	if (err != KErrNone)
		return;
	TLex8 lex(*text);
	while (!lex.Eos())
		{
		TPtrC8 id = lex.NextToken();
		TPtrC8 tag = lex.NextToken();
		if (id == aId)
			aTag = tag.Left(aTag.MaxLength());
		}
	delete text;
	}

void CPheGet::RecordInstalledL(const TDesC8& aId, const TDesC8& aTag)
	{
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
