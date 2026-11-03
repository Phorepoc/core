// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheget.cpp -- OURS: the download app's command line (the engine: pheget_engine.cpp). Every line goes to the debug
// log as "PH: [pheget] ..." (PhoneLab and `ph` read those).
//
//   pheget list [-all] [-index <url|path>] [-no-default]      the items that fit this device (-all: every item)
//   pheget install <id> [-drive <c|e>] [-index ...] [-no-default]
//   pheget device                                               the board / architecture the device reports
//   pheget sources                                              the default index (on/off) and the added sources
//   pheget add <url|path>  |  remove <n|url|path>               add / remove a software source (indexes.txt)
//   pheget default on|off                                       the default Phorepoc index (off needs another source)
#include <e32base.h>
#include <e32debug.h>
#include "pheget.h"

class TLogObserver : public MPheGetObserver
	{
public:
	void PheGetLog(const TDesC8& aLine) { RDebug::Printf("PH: [pheget] %S", &aLine); }
	void PheGetProgress(TInt aDone, TInt aTotal)
		{
		// one line per MB keeps the log quiet
		if (aDone / 1048576 != iLastMb)
			{
			iLastMb = aDone / 1048576;
			RDebug::Printf("PH: [pheget] %d / %d bytes", aDone, aTotal);
			}
		}
	TInt iLastMb;
	};

static TInt RunL()
	{
	TBuf<512> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	TPtrC verb = lex.NextToken();
	TPtrC id;
	TPtrC index;
	TBool noDefault = EFalse;
	TBool all = EFalse;
	TChar drive('c');
	for (TPtrC tok = lex.NextToken(); tok.Length(); tok.Set(lex.NextToken()))
		{
		if (tok == _L("-index"))
			index.Set(lex.NextToken());
		else if (tok == _L("-no-default"))
			noDefault = ETrue;
		else if (tok == _L("-all"))
			all = ETrue;
		else if (tok == _L("-drive"))
			{
			TPtrC d = lex.NextToken();
			if (d.Length())
				drive = d[0];
			}
		else
			id.Set(tok);
		}

	TLogObserver log;
	log.iLastMb = -1;
	CPheGet* get = CPheGet::NewL(log);
	CleanupStack::PushL(get);
	TInt result = KErrNone;
	if (verb == _L("device"))
		{
		RDebug::Printf("PH: [pheget] device board=%S arch=%S", &get->Board(), &get->Arch());
		}
	else if (verb == _L("sources"))
		{
		CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
		CleanupStack::PushL(sources);
		get->SourcesL(*sources);
		TBool on = get->DefaultIndexEnabledL();
		TBuf8<128> def;
		def.Copy(KPheGetDefaultIndex);
		RDebug::Printf("PH: [pheget] source default %S %s", &def, on ? "on" : "off");
		for (TInt i = 0; i < sources->Count(); ++i)
			{
			TBuf8<256> s;
			s.Copy((*sources)[i].Left(256));
			RDebug::Printf("PH: [pheget] source %d %S", i, &s);
			}
		RDebug::Printf("PH: [pheget] %d added sources", sources->Count());
		CleanupStack::PopAndDestroy(sources);
		}
	else if (verb == _L("add") && id.Length())
		{
		TRAP(result, get->AddSourceL(id));
		RDebug::Printf("PH: [pheget] add: %d", result);
		}
	else if (verb == _L("remove") && id.Length())
		{
		// by number (as "pheget sources" lists them) or by the source's text
		TInt n = -1;
		TLex lexId(id);
		if (lexId.Val(n) != KErrNone || !lexId.Eos())
			{
			n = -1;
			CDesCArrayFlat* sources = new (ELeave) CDesCArrayFlat(4);
			CleanupStack::PushL(sources);
			get->SourcesL(*sources);
			for (TInt i = 0; i < sources->Count() && n < 0; ++i)
				if ((*sources)[i].CompareF(id) == 0)
					n = i;
			CleanupStack::PopAndDestroy(sources);
			}
		TRAP(result, get->RemoveSourceL(n));
		RDebug::Printf("PH: [pheget] remove: %d", result);
		}
	else if (verb == _L("default") && (id == _L("on") || id == _L("off")))
		{
		TRAP(result, get->SetDefaultIndexL(id == _L("on")));
		RDebug::Printf("PH: [pheget] default %s: %d", id == _L("on") ? "on" : "off", result);
		}
	else if (verb == _L("list"))
		{
		get->LoadIndexesL(index, noDefault);
		RArray<TPheGetItem> items;
		CleanupClosePushL(items);
		get->ItemsL(items, all);
		for (TInt i = 0; i < items.Count(); ++i)
			{
			const TPheGetItem& it = items[i];
			TPtrC8 iid = it.Id(), tag = it.Tag(), name = it.Name();
			TBuf8<32> installed;
			get->InstalledTagL(iid, installed);
			RDebug::Printf("PH: [pheget] %S %S \"%S\"%s%S", &iid, &tag, &name,
						   installed.Length() ? " installed " : "", &installed);
			}
		RDebug::Printf("PH: [pheget] %d items for board=%S arch=%S", items.Count(), &get->Board(), &get->Arch());
		CleanupStack::PopAndDestroy(&items);
		}
	else if (verb == _L("install") && id.Length())
		{
		get->LoadIndexesL(index, noDefault);
		TBuf8<64> id8;
		id8.Copy(id.Left(64));
		TPheGetItem item;
		if (!get->FindL(id8, item))
			{
			RDebug::Printf("PH: [pheget] %S: not in any index", &id8);
			result = KErrNotFound;
			}
		else
			result = get->InstallL(item, drive);
		}
	else
		{
		RDebug::Printf("PH: [pheget] usage: pheget list [-all] | install <id> [-drive c] | device | sources | add <url|path> | remove <n|url> | default on|off  [-index <url|path>] [-no-default]");
		result = KErrArgument;
		}
	CleanupStack::PopAndDestroy(get);
	return result;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt err = KErrNoMemory;
	if (cleanup)
		{
		CActiveScheduler* sched = new CActiveScheduler;   // the HTTP stack's callbacks
		if (sched)
			{
			CActiveScheduler::Install(sched);
			TRAPD(leave, err = RunL());
			if (leave != KErrNone)
				{
				RDebug::Printf("PH: [pheget] left %d", leave);
				err = leave;
				}
			delete sched;
			}
		delete cleanup;
		}
	return err;
	}
