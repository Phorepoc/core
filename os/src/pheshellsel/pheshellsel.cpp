// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheshellsel.cpp -- OURS: the boot-time UI shell selector.
// SSM's security-check list starts this in the idle slot (r_cmd_idle, deferred wait-for-signal, restart monitored) instead of
// ailaunch.exe. It reads the developer-menu repository (src/phedevmenu/phedevmenu.h): key 6 = the shells this image carries
// (SHELLS= at build time), key 1 = the user's choice, and starts that shell:
//   S^3: ailaunch.exe (the idle launcher -> the S^3 homescreen), then matrixmenu.exe in the background (the S^3 list's r_cmd_menu)
//   S^4: the exes in Z:\resource\phe\s4start.txt (the S^4-only servers master's lists start; one name per line) fire-and-forget,
//        then hsapplicationlauncher.exe (the S^4 homescreen)
// It signals SSM when the shell's launcher has signalled, then stays alive as long as that launcher, so SSM's restart monitoring
// of the idle slot keeps its meaning (the launcher dies -> we exit with its reason -> SSM restarts us -> the shell comes back).
// A choice the image cannot honour (key 1 = S^4 on an S^3-only image) falls back to the shell that is there.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <centralrepository.h>
#include <apgcli.h>
#include <apacmdln.h>
#include "../phedevmenu/phedevmenu.h"

_LIT(KAiLaunch, "z:\\sys\\bin\\ailaunch.exe");
_LIT(KMatrixMenu, "z:\\sys\\bin\\matrixmenu.exe");
_LIT(KHsLauncher, "z:\\sys\\bin\\hsapplicationlauncher.exe");
_LIT(KS4StartList, "z:\\resource\\phe\\s4start.txt");

static TInt RepoGet(CRepository* aRepo, TUint32 aKey, TInt aDefault)
	{
	TInt v;
	return aRepo && aRepo->Get(aKey, v) == KErrNone ? v : aDefault;
	}

static TInt ChooseShell()
	{
	CRepository* repo = NULL;
	TRAPD(err, repo = CRepository::NewL(KPheDevMenuRepoUid));
	const TInt avail = PheShellsInRom();	// the ROM itself (key 6 persists to C: and outlives a reflash)
	TInt shell = RepoGet(repo, KPheDevKeyShell, KPheShellS3);
	delete repo;
	const TBool hasS4 = (avail & KPheShellsS4) != 0, hasS3 = (avail & KPheShellsS3) != 0 || !hasS4;
	if (shell == KPheShellS4 && !hasS4) shell = KPheShellS3;
	if (shell != KPheShellS4 && !hasS3) shell = KPheShellS4;
	RDebug::Printf("PH: [shellsel] repo %d, shells present mask %d, chosen %s", err, avail, shell == KPheShellS4 ? "S^4" : "S^3");
	return shell;
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

static void StartS4ServicesL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RFile f;
	if (f.Open(fs, KS4StartList, EFileRead | EFileShareReadersOnly) == KErrNone)
		{
		CleanupClosePushL(f);
		TBuf8<2048> buf;
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
					// "waitdir <dir> <ms>": wait until a directory exists (the DRM Rights Server makes c:\system\temp\ at
					// start-up and never rendezvouses; Qt's QDir::tempPath() lock files need it -- the Hb theme server)
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
					// "wait <exe>": start and wait for its rendezvous (master's WaitForSignal: hbthemeserver before the homescreen)
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

static TInt RunShellL()
	{
	const TInt shell = ChooseShell();
	RProcess launcher;
	TInt r;
	if (shell == KPheShellS4)
		{
		StartS4ServicesL();
		r = StartAndRendezvous(KHsLauncher, KNullDesC, launcher);
		}
	else
		{
		r = StartAndRendezvous(KAiLaunch, KNullDesC, launcher);
		if (r == KErrNone)
			StartMatrixMenuInBackground();
		}
	RProcess::Rendezvous(r);		// SSM's wait-for-signal: the shell's launcher is up (or its error)
	if (r != KErrNone)
		return r;
	// stay as long as the launcher: its exit = ours, so the idle slot's restart monitor still sees a dying shell
	TRequestStatus st;
	launcher.Logon(st);
	User::WaitForRequest(st);
	RDebug::Printf("PH: [shellsel] shell launcher exited: type %d reason %d", (TInt)launcher.ExitType(), launcher.ExitReason());
	r = launcher.ExitType() == EExitKill ? KErrNone : launcher.ExitReason();
	launcher.Close();
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
