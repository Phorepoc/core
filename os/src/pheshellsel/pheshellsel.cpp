// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pheshellsel.cpp -- OURS: the boot-time UI shell selector.
// SSM's security-check list starts this in the idle slot (r_cmd_idle, deferred wait-for-signal, restart monitored) instead of
// ailaunch.exe. It reads the developer-menu repository (src/phedevmenu/phedevmenu.h): key 10 = the user's choice (a shell id;
// key 1 = 0/1 for an older repository), and starts that shell. The shells are the built-in ones and the installed ones
// (src/phedevmenu/pheshells.h: description files in <drive>:\resource\phe\shells\):
//   S^3: ailaunch.exe (the idle launcher -> the S^3 homescreen), then matrixmenu.exe in the background (the S^3 list's r_cmd_menu)
//   any other: the exes in the shell's start list (fire-and-forget, or "wait"), then its launcher exe
// It signals SSM when the shell's launcher has signalled, then stays alive as long as that launcher, so SSM's restart monitoring
// of the idle slot keeps its meaning (the launcher dies -> we exit with its reason -> SSM restarts us -> the shell comes back).
// Safety (2026-11-08): the Menu key held at boot forces S^3; a shell that did not stay up KPheShellGoodSeconds twice in a row
// (key 11 counts the unconfirmed starts) is given up -- back to S^3, with a note (key 12); a choice that is not there falls back.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <w32std.h>
#include <centralrepository.h>
#include <apgcli.h>
#include <apacmdln.h>
#include "../phedevmenu/phedevmenu.h"
#include "../phedevmenu/pheshells.h"

_LIT(KAiLaunch, "z:\\sys\\bin\\ailaunch.exe");
_LIT(KMatrixMenu, "z:\\sys\\bin\\matrixmenu.exe");

// the chosen shell. aFellBack: the choice was not honoured (not installed any more)
static void ChooseShell(RFs& aFs, CRepository* aRepo, TPheShell& aShell, TBool& aFellBack)
	{
	RArray<TPheShell> shells;
	PheListShells(aFs, shells);
	TBuf<24> id;
	PheSelectedShellId(aRepo, id);
	TInt pick = 0;
	aFellBack = ETrue;
	for (TInt i = 0; i < shells.Count(); i++)
		if (shells[i].iId.CompareF(id) == 0)
			{
			pick = i;
			aFellBack = EFalse;
			}
	aShell = shells[pick];
	RDebug::Print(_L("PH: [shellsel] %d shells, wanted %S, chosen %S"), shells.Count(), &id, &aShell.iId);
	shells.Close();
	}

// Menu key held at boot: a window group capturing the key sees its down event (or the keyboard's auto-repeat) at once
static TBool MenuKeyHeld()
	{
	RWsSession ws;
	if (ws.Connect() != KErrNone)
		return EFalse;
	TBool held = EFalse;
	RWindowGroup grp(ws);
	if (grp.Construct(0xE70157B7) == KErrNone)
		{
		grp.EnableReceiptOfFocus(EFalse);
		const TInt kh = grp.CaptureKeyUpAndDowns(EStdKeyApplication0, 0, 0, 5000);
		TRequestStatus st;
		ws.EventReady(&st);
		RTimer timer;
		timer.CreateLocal();
		TRequestStatus ts;
		timer.After(ts, 1500000);
		User::WaitForRequest(st, ts);
		if (st == KRequestPending)
			{
			ws.EventReadyCancel();
			User::WaitForRequest(st);
			}
		else
			{
			held = ETrue;
			TWsEvent ev;
			ws.GetEvent(ev);
			timer.Cancel();
			User::WaitForRequest(ts);
			}
		timer.Close();
		if (kh >= 0)
			grp.CancelCaptureKeyUpAndDowns(kh);
		}
	grp.Close();
	ws.Close();
	RDebug::Printf("PH: [shellsel] Menu key held at boot: %d", held);
	return held;
	}

// the note after a shell was given up: the standard text notifier, from its own thread (it blocks until dismissed)
static TBuf<80> NoteText;
static TInt NoteThread(TAny*)
	{
	RNotifier n;
	if (n.Connect() == KErrNone)
		{
		TInt button;
		TRequestStatus st;
		n.Notify(NoteText, _L("Back to Symbian^3."), _L("OK"), KNullDesC, button, st);
		User::WaitForRequest(st);
		n.Close();
		}
	return KErrNone;
	}

static void ShowNote(const TDesC& aCaption)
	{
	NoteText.Format(_L("%S did not start twice in a row."), &aCaption);
	RThread t;
	if (t.Create(_L("pheshellnote"), NoteThread, 8192, NULL, NULL) == KErrNone)
		{
		t.SetPriority(EPriorityLess);
		t.Resume();
		t.Close();
		}
	}

// start an exe and wait for its rendezvous (as SSM's deferred wait-for-signal would); the process stays open in aProc
static TInt StartAndRendezvous(const TDesC& aExe, const TDesC& aArgs, RProcess& aProc)
	{
	TInt r = aProc.Create(aExe, aArgs);
	if (r != KErrNone)
		{
		RDebug::Print(_L("PH: [shellsel] cannot create %S: %d"), &aExe, r);
		return r;
		}
	TRequestStatus rv;
	aProc.Rendezvous(rv);
	aProc.Resume();
	User::WaitForRequest(rv);
	RDebug::Print(_L("PH: [shellsel] %S signalled %d"), &aExe, rv.Int());
	return rv.Int();
	}

static void StartFireAndForget(const TDesC& aExe)
	{
	RProcess p;
	const TInt r = p.Create(aExe, KNullDesC);
	if (r == KErrNone)
		{
		p.Resume();
		p.Close();
		}
	RDebug::Print(_L("PH: [shellsel] started %S: %d"), &aExe, r);
	}

// the S^3 list's r_cmd_menu: matrixmenu as a BACKGROUND app (SSM_START_APP_INFO background = 1)
static void StartMatrixMenuInBackground()
	{
	RApaLsSession ls;
	TInt r = ls.Connect();
	if (r == KErrNone)
		{
		CApaCommandLine* cmd = NULL;
		TRAP(r, cmd = CApaCommandLine::NewL());
		if (r == KErrNone)
			{
			TRAP(r, cmd->SetExecutableNameL(KMatrixMenu));
			cmd->SetCommandL(EApaCommandBackground);
			r = ls.StartApp(*cmd);
			delete cmd;
			}
		ls.Close();
		}
	RDebug::Printf("PH: [shellsel] matrixmenu (background): %d", r);
	}

// a start list: one line each -- an exe (fire-and-forget), "wait <exe>" (start, wait for its rendezvous),
// "waitdir <dir> <ms>" (wait until a directory exists), '#' = comment
static void StartListL(const TDesC& aList)
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RFile f;
	if (f.Open(fs, aList, EFileRead | EFileShareReadersOnly) == KErrNone)
		{
		CleanupClosePushL(f);
		TBuf8<4096> buf;
		User::LeaveIfError(f.Read(buf));
		TPtrC8 rest(buf);
		while (rest.Length())
			{
			TInt nl = rest.Locate('\n');
			TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
			rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
			TBuf<KMaxFileName> name;
			name.Copy(line.Left(Min(line.Length(), KMaxFileName)));
			name.Trim();
			_LIT(KWait, "wait ");
			_LIT(KWaitDir, "waitdir ");
			if (name.Length() && name[0] != '#')
				{
				if (name.Left(KWaitDir().Length()).CompareF(KWaitDir) == 0)
					{
					// the DRM Rights Server makes c:\system\temp\ at start-up and never rendezvouses; Qt's QDir::tempPath()
					// lock files need it (the Hb theme server)
					TPtrC rest = name.Mid(KWaitDir().Length());
					const TInt sp = rest.Locate(' ');
					TPtrC dir = sp < 0 ? rest : rest.Left(sp);
					TInt ms = 3000;
					if (sp >= 0) { TLex lex(rest.Mid(sp + 1)); lex.Val(ms); }
					TInt waited = 0;
					TEntry e;
					while (fs.Entry(dir, e) != KErrNone && waited < ms) { User::After(50000); waited += 50; }
					RDebug::Print(_L("PH: [shellsel] waitdir %S: %d ms, %S"), &dir, waited, waited < ms ? &KNullDesC : &dir);
					}
				else if (name.Left(KWait().Length()).CompareF(KWait) == 0)
					{
					// master's WaitForSignal: hbthemeserver before the homescreen
					TPtrC exe = name.Mid(KWait().Length());
					RProcess p;
					StartAndRendezvous(exe, KNullDesC, p);
					p.Close();
					}
				else
					StartFireAndForget(name);
				}
			}
		CleanupStack::PopAndDestroy(&f);
		}
	CleanupStack::PopAndDestroy(&fs);
	}

static void SetRepoInt(CRepository* aRepo, TUint32 aKey, TInt aVal)
	{
	if (aRepo)
		aRepo->Set(aKey, aVal);
	}

static TInt RunShellL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	CRepository* repo = NULL;
	TRAPD(rerr, repo = CRepository::NewL(KPheDevMenuRepoUid));
	TPheShell shell;
	TBool fellBack;
	ChooseShell(fs, repo, shell, fellBack);
	const TBool wanted3 = shell.iId.CompareF(KPheShellIdS3) == 0;
	TBool forced = EFalse;
	if (!wanted3)
		{
		// the safety rules, for a shell other than S^3
		TInt tries = 0;
		if (repo) repo->Get(KPheDevKeyShellTries, tries);
		if (MenuKeyHeld())
			{
			RDebug::Printf("PH: [shellsel] Menu key held: S^3");
			forced = ETrue;
			}
		else if (tries >= KPheShellMaxTries)
			{
			RDebug::Print(_L("PH: [shellsel] %d unconfirmed starts of %S: given up, S^3"), tries, &shell.iName);
			if (repo)
				{
				PheStoreShellId(repo, KPheShellIdS3);
				repo->Set(KPheDevKeyShellNote, shell.iName);
				}
			SetRepoInt(repo, KPheDevKeyShellTries, 0);
			forced = ETrue;
			}
		else
			SetRepoInt(repo, KPheDevKeyShellTries, tries + 1);
		}
	if (forced)
		{
		shell = TPheShell();
		shell.iId.Copy(KPheShellIdS3);
		shell.iBuiltin = ETrue;
		}
	const TBool s3 = shell.iId.CompareF(KPheShellIdS3) == 0;
	RDebug::Print(_L("PH: [shellsel] repo %d, starting %S (fallback %d)"), rerr, &shell.iId, fellBack);
	RProcess launcher;
	TInt r;
	if (!s3)
		{
		if (shell.iStart.Length())
			StartListL(shell.iStart);
		r = StartAndRendezvous(shell.iLauncher, KNullDesC, launcher);
		}
	else
		{
		r = StartAndRendezvous(KAiLaunch, KNullDesC, launcher);
		if (r == KErrNone)
			StartMatrixMenuInBackground();
		}
	RProcess::Rendezvous(r);		// SSM's wait-for-signal: the shell's launcher is up (or its error)
	if (r != KErrNone)
		{
		CleanupStack::PopAndDestroy(&fs);
		delete repo;
		return r;
		}
	if (s3 && repo)
		{
		// a note left by an earlier give-up: shown once S^3 is up
		TBuf<40> n;
		if (repo->Get(KPheDevKeyShellNote, n) == KErrNone && n.Length())
			{
			User::After(15000000);
			ShowNote(n);
			repo->Set(KPheDevKeyShellNote, KNullDesC);
			}
		}
	// stay as long as the launcher: its exit = ours, so the idle slot's restart monitor still sees a dying shell.
	// A shell other than S^3 that stays up KPheShellGoodSeconds is confirmed good.
	TRequestStatus st;
	launcher.Logon(st);
	if (!s3)
		{
		RTimer timer;
		timer.CreateLocal();
		TRequestStatus ts;
		timer.After(ts, KPheShellGoodSeconds * 1000000);
		User::WaitForRequest(st, ts);
		if (st == KRequestPending)
			{
			SetRepoInt(repo, KPheDevKeyShellTries, 0);
			RDebug::Printf("PH: [shellsel] shell confirmed good");
			User::WaitForRequest(st);
			}
		else
			{
			timer.Cancel();
			User::WaitForRequest(ts);
			}
		timer.Close();
		}
	else
		User::WaitForRequest(st);
	RDebug::Printf("PH: [shellsel] shell launcher exited: type %d reason %d", (TInt)launcher.ExitType(), launcher.ExitReason());
	r = launcher.ExitType() == EExitKill ? KErrNone : launcher.ExitReason();
	launcher.Close();
	delete repo;
	CleanupStack::PopAndDestroy(&fs);
	return r;
	}

TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	if (!tc)
		return KErrNoMemory;
	TInt r = KErrNone;
	TRAPD(err, r = RunShellL());
	delete tc;
	return err != KErrNone ? err : r;
	}
