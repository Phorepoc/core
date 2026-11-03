// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phegettest.cpp -- OURS: tests of the download engine's software sources (../pheget): validation, add / remove,
// the default index switched off (only while another source exists), the index caches staying with their own source,
// and which indexes a load reads. Runs on the device; a failed check is a "PH: [phegettest] FAIL" line and the last
// line is "PH: [phegettest] RESULT PASS" or "RESULT FAIL <n>". It saves c:\data\pheget\indexes.txt and puts it back.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <badesca.h>
#include "pheget.h"
#include "phejson.h"

static TInt gFail = 0;
static TInt gChecks = 0;

#define CHECK(cond) Check((cond), #cond, __LINE__)
static void Check(TBool aOk, const char* aText, TInt aLine)
	{
	++gChecks;
	if (!aOk)
		{
		++gFail;
		RDebug::Printf("PH: [phegettest] FAIL line %d: %s", aLine, aText);
		}
	}

class TObs : public MPheGetObserver
	{
public:
	void PheGetLog(const TDesC8& aLine) { RDebug::Printf("PH: [phegettest]   engine: %S", &aLine); }
	void PheGetProgress(TInt, TInt) {}
	};

_LIT(KIndexes, "c:\\data\\pheget\\indexes.txt");
_LIT(KIndexesTmp, "c:\\data\\pheget\\indexes.txt.tmp");
_LIT(KOldCache, "c:\\data\\pheget\\index3.json");
_LIT(KLocalA, "c:\\data\\pheget\\test_a.json");
_LIT(KLocalB, "c:\\data\\pheget\\test_b.json");
_LIT(KUrlA, "http://sources.test/a/index.json");
_LIT(KUrlB, "https://sources.test/b/index.json");

static void WriteFileL(RFs& aFs, const TDesC& aName, const TDesC8& aData)
	{
	RFile f;
	User::LeaveIfError(f.Replace(aFs, aName, EFileWrite));
	TInt r = f.Write(aData);
	f.Close();
	User::LeaveIfError(r);
	}

static HBufC8* ReadWholeL(RFs& aFs, const TDesC& aName)
	{
	RFile f;
	User::LeaveIfError(f.Open(aFs, aName, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(f);
	TInt size = 0;
	User::LeaveIfError(f.Size(size));
	HBufC8* b = HBufC8::NewL(size);
	TPtr8 p = b->Des();
	TInt r = f.Read(p, size);
	if (r != KErrNone)
		{
		delete b;
		User::Leave(r);
		}
	CleanupStack::PopAndDestroy(&f);
	return b;
	}

static void IndexWithItemL(RFs& aFs, const TDesC& aName, const TDesC8& aId)
	{
	TBuf8<256> j;
	j.Format(_L8("{\"items\":[{\"id\":\"%S\",\"name\":\"%S\",\"compatibility\":{\"arch\":[\"x32\",\"arm\"]}}]}"), &aId, &aId);
	WriteFileL(aFs, aName, j);
	}

static TBool Exists(RFs& aFs, const TDesC& aName)
	{
	TEntry e;
	return aFs.Entry(aName, e) == KErrNone;
	}

// the ids the engine loaded (all items, compatibility aside), joined with ',' in load order
static void IdsL(CPheGet& aGet, TDes8& aOut)
	{
	RArray<TPheGetItem> items;
	CleanupClosePushL(items);
	aGet.ItemsL(items, ETrue);
	aOut.Zero();
	for (TInt i = 0; i < items.Count(); ++i)
		{
		if (i)
			aOut.Append(',');
		aOut.Append(items[i].Id());
		}
	CleanupStack::PopAndDestroy(&items);
	}

static TInt LoadCachedL(CPheGet& aGet, TDes8& aIds)
	{
	aGet.ResetIndexes();
	TRAPD(r, aGet.LoadIndexesL(KNullDesC, EFalse, ETrue));
	aIds.Zero();
	if (r == KErrNone)
		IdsL(aGet, aIds);
	return r;
	}

static TInt SourceCount(CPheGet& aGet)
	{
	CDesCArrayFlat* s = new CDesCArrayFlat(4);
	if (!s)
		return -2;
	TRAPD(r, aGet.SourcesL(*s));
	TInt n = r == KErrNone ? s->Count() : -1;
	delete s;
	return n;
	}

static void TestValidation()
	{
	CHECK(CPheGet::ValidSource(_L("http://example.org/index.json")));
	CHECK(CPheGet::ValidSource(_L("https://example.org:8443/a/b.json?x=1")));
	CHECK(CPheGet::ValidSource(_L("HTTPS://Example.org")));
	CHECK(CPheGet::ValidSource(_L("c:\\data\\idx.json")));
	CHECK(CPheGet::ValidSource(_L("e:/idx/index.json")));
	CHECK(!CPheGet::ValidSource(KNullDesC));
	CHECK(!CPheGet::ValidSource(_L("http://")));
	CHECK(!CPheGet::ValidSource(_L("https:///path")));
	CHECK(!CPheGet::ValidSource(_L("ftp://example.org/i.json")));
	CHECK(!CPheGet::ValidSource(_L("example.org/index.json")));
	CHECK(!CPheGet::ValidSource(_L("http://exa mple.org/i.json")));
	CHECK(!CPheGet::ValidSource(_L("http://example.org/i.json#frag")));
	CHECK(!CPheGet::ValidSource(_L("\\data\\idx.json")));             // no drive
	CHECK(!CPheGet::ValidSource(_L("c:\\data\\")));                    // a directory
	CHECK(!CPheGet::ValidSource(_L("c:idx.json")));
	TBuf<2> odd;
	odd.Append(0x00e9);
	CHECK(!CPheGet::ValidSource(odd));
	}

static void TestSourcesL(RFs& aFs, CPheGet& aGet)
	{
	// empty: the default index only
	CHECK(SourceCount(aGet) == 0);
	CHECK(aGet.DefaultIndexEnabledL());
	TRAPD(r, aGet.SetDefaultIndexL(EFalse));
	CHECK(r == KErrNotSupported);                       // never without a source
	CHECK(aGet.DefaultIndexEnabledL());
	TRAP(r, aGet.AddSourceL(_L("not a source")));
	CHECK(r == KErrArgument);
	TRAP(r, aGet.AddSourceL(KPheGetDefaultIndex));
	CHECK(r == KErrAlreadyExists);

	aGet.AddSourceL(KUrlA);
	aGet.AddSourceL(KLocalB);
	TRAP(r, aGet.AddSourceL(KUrlA));
	CHECK(r == KErrAlreadyExists);
	TFileName upper(KUrlA);
	upper.UpperCase();
	TRAP(r, aGet.AddSourceL(upper));
	CHECK(r == KErrAlreadyExists);                      // (the same source in other case)
	CHECK(SourceCount(aGet) == 2);
	CDesCArrayFlat* s = new (ELeave) CDesCArrayFlat(4);
	CleanupStack::PushL(s);
	aGet.SourcesL(*s);
	CHECK(s->Count() == 2 && (*s)[0] == KUrlA && (*s)[1] == KLocalB);
	CleanupStack::PopAndDestroy(s);
	CHECK(!Exists(aFs, KIndexesTmp));                   // the write left no temporary file

	// the default off and back on
	aGet.SetDefaultIndexL(EFalse);
	CHECK(!aGet.DefaultIndexEnabledL());
	TObs obs;
	CPheGet* again = CPheGet::NewL(obs);                 // a new engine reads the same file
	CHECK(!again->DefaultIndexEnabledL());
	delete again;
	aGet.SetDefaultIndexL(ETrue);
	CHECK(aGet.DefaultIndexEnabledL());
	aGet.SetDefaultIndexL(EFalse);

	// removing a source keeps the setting while another remains; the last one gone puts the default back on
	TRAP(r, aGet.RemoveSourceL(5));
	CHECK(r == KErrNotFound);
	TRAP(r, aGet.RemoveSourceL(-1));
	CHECK(r == KErrNotFound);
	aGet.RemoveSourceL(0);
	CHECK(SourceCount(aGet) == 1 && !aGet.DefaultIndexEnabledL());
	aGet.RemoveSourceL(0);
	CHECK(SourceCount(aGet) == 0 && aGet.DefaultIndexEnabledL());

	// a hand-edited file: "default off" with no source is not honoured; comments, blanks, CRLF and two tokens a line
	WriteFileL(aFs, KIndexes, _L8("#default=off\r\n"));
	CHECK(aGet.DefaultIndexEnabledL());
	WriteFileL(aFs, KIndexes, _L8("# my sources\r\n\r\n  http://sources.test/a/index.json   c:\\data\\x.json # note\r\nc:\\data\\x.json\r\n#default=off\r\n"));
	CHECK(SourceCount(aGet) == 2);                      // (the duplicate is dropped)
	CHECK(!aGet.DefaultIndexEnabledL());
	aGet.AddSourceL(KLocalA);                           // rewriting keeps the setting
	CHECK(SourceCount(aGet) == 3 && !aGet.DefaultIndexEnabledL());
	aFs.Delete(KIndexes);
	CHECK(SourceCount(aGet) == 0 && aGet.DefaultIndexEnabledL());
	}

// which indexes a load reads, and caches that stay with their own source
static void TestLoadingL(RFs& aFs, CPheGet& aGet)
	{
	aFs.Delete(KIndexes);
	IndexWithItemL(aFs, KLocalA, _L8("local-a"));
	IndexWithItemL(aFs, KLocalB, _L8("local-b"));
	TFileName cDef, cA, cB, cAu;
	CPheGet::IndexCachePath(cDef, KPheGetDefaultIndex);
	CPheGet::IndexCachePath(cA, KUrlA);
	CPheGet::IndexCachePath(cB, KUrlB);
	CHECK(cDef != cA && cA != cB && cDef != cB);
	TFileName upper(KUrlA);
	upper.UpperCase();
	CPheGet::IndexCachePath(cAu, upper);
	CHECK(cAu == cA);
	IndexWithItemL(aFs, cDef, _L8("from-default"));
	IndexWithItemL(aFs, cA, _L8("from-a"));
	IndexWithItemL(aFs, cB, _L8("from-b"));

	TBuf8<200> ids;
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("from-default"));   // only the default index
	aGet.AddSourceL(KLocalA);
	aGet.AddSourceL(KUrlA);
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("from-default,local-a,from-a"));
	aGet.SetDefaultIndexL(EFalse);
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("local-a,from-a"));   // the other sources' items only
	aGet.SetDefaultIndexL(ETrue);
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("from-default,local-a,from-a"));

	// removing a URL source: its cache goes, the next source's cache is still its own
	aGet.AddSourceL(KUrlB);
	aGet.SetDefaultIndexL(EFalse);
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("local-a,from-a,from-b"));
	aGet.RemoveSourceL(1);                                // KUrlA
	CHECK(!Exists(aFs, cA));                              // its cache is gone
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("local-a,from-b"));
	aGet.AddSourceL(KUrlA);                               // added again: no cache, so nothing of the old one shows
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("local-a,from-b"));
	IndexWithItemL(aFs, cA, _L8("from-a2"));              // (a refresh landed)
	CHECK(LoadCachedL(aGet, ids) == KErrNone && ids == _L8("local-a,from-b,from-a2"));

	// a new engine prunes the caches nobody owns (a removed source's leftover, the old numbered caches), keeps the rest
	aGet.RemoveSourceL(2);                                // KUrlA again
	IndexWithItemL(aFs, cA, _L8("leftover"));             // (a refresh that was still running when it was removed)
	IndexWithItemL(aFs, KOldCache, _L8("old"));
	TObs obs;
	CPheGet* fresh = CPheGet::NewL(obs);
	delete fresh;
	CHECK(!Exists(aFs, cA));
	CHECK(!Exists(aFs, KOldCache));
	CHECK(Exists(aFs, cDef) && Exists(aFs, cB));         // (the disabled default's cache stays)

	// an extra index and no default (the command line) still work
	aGet.ResetIndexes();
	TRAPD(r, aGet.LoadIndexesL(KLocalB, ETrue, ETrue));
	IdsL(aGet, ids);
	CHECK(r == KErrNone && ids == _L8("local-b,local-a,from-b"));
	aGet.ResetIndexes();

	// a refresh with only local sources has nothing to download and does not fail
	aGet.RemoveSourceL(1);                                // KUrlB
	CHECK(SourceCount(aGet) == 1);
	TBool changed = ETrue;
	TRAP(r, changed = aGet.RefreshIndexesL(KNullDesC, EFalse));
	CHECK(r == KErrNone && !changed);
	}

static void CleanupFiles(RFs& aFs)
	{
	aFs.Delete(KLocalA);
	aFs.Delete(KLocalB);
	TFileName c;
	CPheGet::IndexCachePath(c, KPheGetDefaultIndex);
	aFs.Delete(c);
	CPheGet::IndexCachePath(c, KUrlA);
	aFs.Delete(c);
	CPheGet::IndexCachePath(c, KUrlB);
	aFs.Delete(c);
	aFs.Delete(KOldCache);
	}

static TInt RunL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	fs.MkDirAll(KPheGetDir);
	HBufC8* saved = NULL;
	TRAP_IGNORE(saved = ReadWholeL(fs, KIndexes));
	CleanupStack::PushL(saved);
	fs.Delete(KIndexes);

	TestValidation();
	TObs obs;
	CPheGet* get = CPheGet::NewL(obs);
	CleanupStack::PushL(get);
	TRAPD(r, TestSourcesL(fs, *get));
	CHECK(r == KErrNone);
	fs.Delete(KIndexes);
	TRAP(r, TestLoadingL(fs, *get));
	CHECK(r == KErrNone);
	CleanupStack::PopAndDestroy(get);

	CleanupFiles(fs);
	fs.Delete(KIndexes);
	if (saved)
		WriteFileL(fs, KIndexes, *saved);
	CleanupStack::PopAndDestroy(saved);
	CleanupStack::PopAndDestroy(&fs);
	if (gFail == 0)
		RDebug::Printf("PH: [phegettest] RESULT PASS (%d checks)", gChecks);
	else
		RDebug::Printf("PH: [phegettest] RESULT FAIL %d of %d", gFail, gChecks);
	return gFail;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt err = KErrNoMemory;
	if (cleanup)
		{
		CActiveScheduler* sched = new CActiveScheduler;
		if (sched)
			{
			CActiveScheduler::Install(sched);
			TRAPD(leave, err = RunL());
			if (leave != KErrNone)
				{
				RDebug::Printf("PH: [phegettest] RESULT FAIL left %d", leave);
				err = leave;
				}
			delete sched;
			}
		delete cleanup;
		}
	return err;
	}
