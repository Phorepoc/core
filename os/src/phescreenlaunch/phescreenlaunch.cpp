// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phescreenlaunch -- start an application on a given WSERV screen, or move the key focus to a screen.
//
//   phescreenlaunch <screen> <app.exe> [document]   AppArc starts the app with CApaCommandLine::SetDefaultScreenL(screen):
//                                                   EIKON builds its CCoeEnv on that screen (CEikonEnv takes the command
//                                                   line's default screen), so the app needs no change
//   phescreenlaunch focus <screen>                  RWsSession::SetFocusScreen: keys go to that screen's focused window group
//                                                   (needs WriteDeviceData)
//   phescreenlaunch window <app.exe> [document]     the app as a window: take a free virtual screen (1 .. NumberOfScreens-1),
//                                                   start phescreenpub for it, start the app there; rendezvous with the caller,
//                                                   then stay until the app's thread ends, stop that phescreenpub and give the
//                                                   screen back. A screen is taken by holding the global mutex "phescreen-slot-<n>":
//                                                   the kernel deletes it with its last handle, so a dead launcher frees its screen
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

static TThreadId LaunchL(TInt aScreen, const TDesC& aExe, const TDesC& aDoc)
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
	return tid;
	}

_LIT(KSlotName, "phescreen-slot-%d");
_LIT(KPubExe, "phescreenpub.exe");

// the first free virtual screen, held by aSlot until this process ends (or aSlot closes)
static TInt TakeScreen(RMutex& aSlot)
	{
	RWsSession ws;
	TInt r = ws.Connect();
	if (r != KErrNone)
		return r;
	const TInt screens = ws.NumberOfScreens();
	ws.Close();
	for (TInt n = 1; n < screens; ++n)
		{
		TBuf<32> name;
		name.Format(KSlotName, n);
		r = aSlot.CreateGlobal(name);
		if (r == KErrNone)
			return n;
		if (r != KErrAlreadyExists)
			return r;
		}
	return KErrInUse;				// every virtual screen has an app
	}

static TInt Window(const TDesC& aExe, const TDesC& aDoc)
	{
	RMutex slot;
	const TInt screen = TakeScreen(slot);
	if (screen < 0)
		{
		Log(_L("window %S: no free screen (%d)"), &aExe, screen);
		RProcess::Rendezvous(screen);
		return screen;
		}
	TThreadId tid;
	TRAPD(r, tid = LaunchL(screen, aExe, aDoc));
	// the screen's publisher watches the app's thread: it ends with the app, and its window leaves the registry
	RProcess pub;
	if (r == KErrNone)
		{
		TBuf<48> arg;
		arg.Format(_L("%d %Lu"), screen, tid.Id());
		r = pub.Create(KPubExe, arg);
		}
	if (r == KErrNone)
		{
		TRequestStatus up, died;
		pub.Rendezvous(up);
		pub.Logon(died);
		pub.Resume();
		User::WaitForRequest(up);
		r = up.Int();
		Log(_L("window %S: screen %d, publisher %d"), &aExe, screen, r);
		RProcess::Rendezvous(r);		// the caller goes on; the screen stays taken until the publisher ends
		User::WaitForRequest(died);
		Log(_L("window %S: publisher ended (%d), screen %d free"), &aExe, died.Int(), screen);
		pub.Close();
		}
	else
		{
		Log(_L("window %S: screen %d: %d"), &aExe, screen, r);
		RProcess::Rendezvous(r);
		}
	slot.Close();
	return r;
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
	if (first.CompareF(_L("window")) == 0)
		{
		TPtrC exe = lex.NextToken();
		lex.SkipSpace();
		TPtrC doc = lex.Remainder();
		return exe.Length() ? Window(exe, doc) : KErrArgument;
		}
	TLex n(first);
	TInt screen = 0;
	TPtrC exe = lex.NextToken();
	if (n.Val(screen) != KErrNone || !exe.Length())
		{
		Log(_L("usage: <screen> <app.exe> [document] | window <app.exe> [document] | focus <screen> (got \"%S\")"), &line);
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
