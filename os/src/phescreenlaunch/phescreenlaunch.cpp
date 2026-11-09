// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phescreenlaunch -- start an application on a given WSERV screen, or move the key focus to a screen.
//
//   phescreenlaunch <screen> <app.exe> [document]   AppArc starts the app with CApaCommandLine::SetDefaultScreenL(screen):
//                                                   EIKON builds its CCoeEnv on that screen (CEikonEnv takes the command
//                                                   line's default screen), so the app needs no change
//   phescreenlaunch focus <screen>                  RWsSession::SetFocusScreen: keys go to that screen's focused window group
//                                                   (needs WriteDeviceData)
//
// Multi-window stage 1: each classic app on its own (virtual) screen, the display shows the screens side by side.
#include <e32base.h>
#include <apgcli.h>
#include <apacmdln.h>
#include <w32std.h>

_LIT(KTag, "[phescreenlaunch] ");

static void Log(TRefByValue<const TDesC> aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	TBuf<256> b(KTag);
	b.AppendFormatList(aFmt, list);
	RDebug::Print(_L("%S"), &b);
	}

static TInt Focus(TInt aScreen)
	{
	RWsSession ws;
	TInt r = ws.Connect();
	if (r != KErrNone)
		return r;
	const TInt before = ws.GetFocusScreen();
	r = ws.SetFocusScreen(aScreen);
	Log(_L("focus screen %d -> %d: %d (now %d, %d screens)"), before, aScreen, r, ws.GetFocusScreen(), ws.NumberOfScreens());
	ws.Close();
	return r;
	}

static void LaunchL(TInt aScreen, const TDesC& aExe, const TDesC& aDoc)
	{
	RApaLsSession ls;
	User::LeaveIfError(ls.Connect());
	CleanupClosePushL(ls);
	CApaCommandLine* cmd = CApaCommandLine::NewLC();
	cmd->SetExecutableNameL(aExe);
	cmd->SetCommandL(EApaCommandRun);
	if (aDoc.Length())
		cmd->SetDocumentNameL(aDoc);
	cmd->SetDefaultScreenL(aScreen);
	TThreadId tid;
	TRequestStatus started;
	const TInt r = ls.StartApp(*cmd, tid, &started);
	if (r == KErrNone)
		User::WaitForRequest(started);
	Log(_L("%S on screen %d: StartApp %d, rendezvous %d"), &aExe, aScreen, r, r == KErrNone ? started.Int() : r);
	User::LeaveIfError(r);
	CleanupStack::PopAndDestroy(2, &ls);
	}

static TInt Run()
	{
	TBuf<256> line;
	User::CommandLine(line);
	TLex lex(line);
	TPtrC first = lex.NextToken();
	if (first.CompareF(_L("focus")) == 0)
		{
		TLex n(lex.NextToken());
		TInt s = 0;
		if (n.Val(s) != KErrNone)
			return KErrArgument;
		return Focus(s);
		}
	TLex n(first);
	TInt screen = 0;
	TPtrC exe = lex.NextToken();
	if (n.Val(screen) != KErrNone || !exe.Length())
		{
		Log(_L("usage: <screen> <app.exe> [document] | focus <screen> (got \"%S\")"), &line);
		return KErrArgument;
		}
	lex.SkipSpace();
	TPtrC doc = lex.Remainder();
	TRAPD(r, LaunchL(screen, exe, doc));
	return r;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	const TInt r = tc ? Run() : KErrNoMemory;
	delete tc;
	return r;
	}
