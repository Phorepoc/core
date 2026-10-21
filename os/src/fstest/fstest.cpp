// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// fstest.cpp -- OURS: probe of the file-server operations a persisting server relies on
// (Central Repository's commit = write temp file, close, RFs::Replace over the target).
// Prints each step's result to the debug port.
#include <e32std.h>
#include <e32debug.h>
#include <f32file.h>
#include <s32file.h>

static void Step(const char* aWhat, TInt aResult)
	{
	RDebug::Printf("[fstest] %s -> %d", aWhat, aResult);
	}

static void StoreCommitL(RFs& aFs, const TDesC& aTmp, const TDesC& aTarget, TInt aWords)
	{
	CDirectFileStore* store = CDirectFileStore::ReplaceLC(aFs, aTmp, (EFileWrite | EFileShareExclusive));
	store->SetTypeL(TUidType(KDirectFileStoreLayoutUid, KNullUid, TUid::Uid(0x10202BE9)));
	RStoreWriteStream root;
	TStreamId id = root.CreateLC(*store);
	for (TInt i = 0; i < aWords; i++)
		root.WriteInt32L(0x12345678);
	root.CommitL();
	CleanupStack::PopAndDestroy(&root);
	store->SetRootL(id);
	store->CommitL();
	CleanupStack::PopAndDestroy(store);
	User::LeaveIfError(aFs.Replace(aTmp, aTarget));
	}

static void StoreReadL(RFs& aFs, const TDesC& aName)
	{
	RFile file;
	User::LeaveIfError(file.Open(aFs, aName, EFileRead | EFileShareReadersOnly));
	CleanupClosePushL(file);
	CDirectFileStore* store = CDirectFileStore::FromLC(file);
	RStoreReadStream root;
	root.OpenLC(*store, store->Root());
	RDebug::Printf("[fstest] read word %x", root.ReadInt32L());
	CleanupStack::PopAndDestroy(3, &file);
	}

GLDEF_C TInt E32Main()
	{
	RFs fs;
	Step("connect", fs.Connect());
	Step("mkdirall", fs.MkDirAll(_L("C:\\fstest\\")));
	RFile f;
	Step("replace-create a.tmp", f.Replace(fs, _L("C:\\fstest\\a.tmp"), EFileWrite | EFileShareExclusive));
	Step("write", f.Write(_L8("hello file server")));
	f.Close();
	Step("RFs::Replace a.tmp->a.cre (target absent)", fs.Replace(_L("C:\\fstest\\a.tmp"), _L("C:\\fstest\\a.cre")));
	Step("replace-create b.tmp", f.Replace(fs, _L("C:\\fstest\\b.tmp"), EFileWrite | EFileShareExclusive));
	Step("write", f.Write(_L8("second version")));
	f.Close();
	Step("RFs::Replace b.tmp->a.cre (target exists)", fs.Replace(_L("C:\\fstest\\b.tmp"), _L("C:\\fstest\\a.cre")));
	Step("open a.cre for write", f.Open(fs, _L("C:\\fstest\\a.cre"), EFileWrite));
	TBuf8<32> buf;
	Step("read", f.Read(buf));
	RDebug::Printf("[fstest] content len=%d", buf.Length());
	f.Close();
	Step("rename a.cre->c.cre", fs.Rename(_L("C:\\fstest\\a.cre"), _L("C:\\fstest\\c.cre")));
	Step("delete c.cre", fs.Delete(_L("C:\\fstest\\c.cre")));
	_LIT(KCre, "C:\\fstest\\s.cre");
	CTrapCleanup* tc = CTrapCleanup::New();
	for (TInt i = 0; i < 6; i++)
		{
		TRAPD(e, StoreCommitL(fs, _L("C:\\fstest\\s.tmp"), _L("C:\\fstest\\s.cre"), i < 2 ? 1 : i < 4 ? 2000 : 40000));
		Step("store commit + Replace s.tmp->s.cre", e);
		TRAP(e, StoreReadL(fs, KCre));
		Step("store read-back", e);
		}
	// the failure path: an empty .cre -> FromLC leaves KErrEof -> cleanup must close the file
	Step("create empty z.cre", f.Replace(fs, _L("C:\\fstest\\z.cre"), EFileWrite));
	f.Close();
	TRAPD(e2, StoreReadL(fs, _L("C:\\fstest\\z.cre")));
	Step("store read of empty file (expect -25)", e2);
	TRAP(e2, StoreCommitL(fs, _L("C:\\fstest\\z.tmp"), _L("C:\\fstest\\z.cre"), 10));
	Step("store commit over it", e2);
	delete tc;
	fs.Close();
	RDebug::Printf("[fstest] done");
	return 0;
	}
