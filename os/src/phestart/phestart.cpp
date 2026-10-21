// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phestart.cpp -- OURS: start an application through apparc (RApaLsSession::StartApp, the launcher's path) and bring its
// task to the foreground. A process started with a plain RProcess::Create (the text shell) is not an apparc-started
// application: no app-server start-up sequence, no foreground. Prints "PH: [phestart] ..." lines.
#include <e32base.h>
#include <e32debug.h>
#include <apgcli.h>
#include <apacmdln.h>
#include <apgtask.h>
#include <w32std.h>

static TInt Run()
	{
	TBuf<128> cmdline;
	User::CommandLine(cmdline);
	TLex lex(cmdline);
	TPtrC what = lex.NextToken();
	TPtrC doc = lex.NextToken();
	if (!what.Length())
		{
		RDebug::Printf("PH: [phestart] usage: phestart <uid-hex | exe name> [document]");
		return KErrArgument;
		}
	RApaLsSession ls;
	TInt r = ls.Connect();
	if (r)
		return r;
	TApaAppInfo info;
	TUid uid = KNullUid;
	TUint32 val = 0;
	TLex ulex(what);
	if (ulex.Val(val, EHex) == KErrNone && ulex.Eos())
		uid = TUid::Uid(val);
	if (uid == KNullUid)
		{
		// by executable name: walk the application list
		ls.GetAllApps();
		while (ls.GetNextApp(info) == KErrNone)
			{
			TParse p;
			p.Set(info.iFullName, NULL, NULL);
			if (p.Name().CompareF(what) == 0 || p.NameAndExt().CompareF(what) == 0)
				{
				uid = info.iUid;
				break;
				}
			}
		}
	r = uid == KNullUid ? KErrNotFound : ls.GetAppInfo(info, uid);
	RDebug::Printf("PH: [phestart] app %x: %d", uid.iUid, r);
	if (r == KErrNone)
		{
		CApaCommandLine* cmd = NULL;
		TRAP(r, cmd = CApaCommandLine::NewL());
		if (r == KErrNone)
			{
			TRAP(r, cmd->SetExecutableNameL(info.iFullName));
			if (r == KErrNone)
				{
				if (doc.Length())
					{
					TRAP(r, cmd->SetDocumentNameL(doc));
					cmd->SetCommandL(EApaCommandOpen);
					}
				else
					cmd->SetCommandL(EApaCommandRun);
				}
			if (r == KErrNone)
				r = ls.StartApp(*cmd);
			delete cmd;
			}
		RDebug::Printf("PH: [phestart] StartApp: %d", r);
		}
	ls.Close();
	if (r != KErrNone)
		return r;
	// bring it to the foreground once its window group exists
	RWsSession ws;
	if (ws.Connect() == KErrNone)
		{
		for (TInt i = 0; i < 40; ++i)
			{
			TApaTaskList tl(ws);
			TApaTask task = tl.FindApp(uid);
			if (task.Exists())
				{
				task.BringToForeground();
				RDebug::Printf("PH: [phestart] foreground requested (try %d)", i);
				break;
				}
			User::After(250000);
			}
		ws.Close();
		}
	return KErrNone;
	}

TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	TInt r = Run();
	delete tc;
	return r;
	}
