// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// bootui.cpp -- OURS: a minimal system starter standing in for S60's SSM/sysstart while
// the UI stack is brought up. ROLE SINCE X4.72-X4.78: the phone boots with the REAL SSM (BOOT=ssm); bootui is
// (1) the app-regression cycler, started under SSM from the startup list (DSC, AMA starter) with a C:\bootui.txt
// holding only `*secs|app|...` (+ `!nettest`), and (2) the BOOT=@ fallback starter below (C:\bootui.txt override).
// As the fallback, WSERV runs it (wsini `startup bootui`); it reads
// Z:\sys\data\bootui.txt (8-bit text, one process per line) and starts each in order:
//     <exe>        start and WAIT for its rendezvous (servers)
//     !<exe>       start and do not wait (the application)
//     <exe>|<arg>|<arg>   ... with a command line: each `|` is a space (the list itself is
//                  whitespace-split by make_phorepoc_rom.sh), e.g. ced|-i|z:\sys\data\x.xml
//     =<cat>:<key>:<int>   define + set an integer Publish&Subscribe property (hex cat/key)
//                  -- the system-state keys a real boot's starter/installer publish, which
//                  components Subscribe to unconditionally (e.g. apparc's SWI monitor raises
//                  a stray signal if KUidSoftwareInstallKey was never defined)
//     ~<hierarchy>:<state>   (hex) Domain Manager: wait for domainsrv (start it first with
//                  `!domainsrv`, it never rendezvouses), add the domain hierarchy and run a
//                  system transition to <state>. `~2:10000` = SSM's startup hierarchy
//                  (domainpolicy2.dll) -> TSsmState(ESsmNormal, 0): what real SSM reaches
//                  at the end of boot, and what e.g. the view server waits for.
//     +capserver   start the Avkon cap server the way S60's startup does (akncustcmds
//                  akncapserverstart.cpp): the REAL RAknUiServer::StartAknCapserverL() -- an
//                  apparc server-app launch (SetServerRequiredL), so akncapserver opens its
//                  `<service>_10207218_AppServer`. A plain RProcess start gives it no server
//                  differentiator: no AppServer, and the first client spawns a 2nd instance.
//     +lights      the display backlight on, as SysAp does at startup (CHWRMLight::LightOnL; see DisplayLightOn)
//     ^            wait for apparc's first app-list scan (before the foreground app; make_phorepoc_rom
//                  puts one before the last line)
//     %<tsy>       load a phone module into ETel and keep it loaded (%PhoneTsy, the S60 common TSY;
//                  needs the comms stack, i.e. after c32start)
//     @<minutes>   the UTC offset (signed, e.g. @+180 = UTC+3): User::SetUTCOffset. EKA2 keeps UTC
//                  and the CMOS clock is UTC (X4.31); a phone's time-zone server (tzserver, not
//                  built for x32 yet) would set this from the zone rules -- no DST here (X4.39).
// Every step is logged to the debug port. Not Symbian source; replaces nothing on a
// real device.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32property.h>
#include <f32file.h>
#include <domainmanager.h>
#include <AknCapServerClient.h>
#include <etel.h>
#include <apgcli.h>
#include <apacmdln.h>
#include <hwrmlight.h>
#include <w32std.h>
#include <apgtask.h>
#include <apgwgnam.h>

static void DefineOne(const TDesC8& aSpec)
	{
	TLex8 lex(aSpec);
	TUint32 cat = 0, key = 0;
	TInt val = 0;
	TInt r = lex.Val(cat, EHex);
	lex.Inc();
	if (r == KErrNone)
		r = lex.Val(key, EHex);
	lex.Inc();
	if (r == KErrNone)
		r = lex.Val(val);
	if (r == KErrNone)
		{
		_LIT_SECURITY_POLICY_PASS(KPass);
		r = RProperty::Define(TUid::Uid(cat), key, RProperty::EInt, KPass, KPass);
		if (r == KErrNone || r == KErrAlreadyExists)
			r = RProperty::Set(TUid::Uid(cat), key, val);
		}
	RDebug::Printf("[bootui] property %08x:%08x = %d -> %d", cat, key, val, r);
	}

static void DomainTransition(const TDesC8& aSpec)
	{
	TLex8 lex(aSpec);
	TUint32 hier = 0, state = 0;
	TInt r = lex.Val(hier, EHex);
	lex.Inc();
	if (r == KErrNone)
		r = lex.Val(state, EHex);
	if (r == KErrNone)
		r = RDmDomainManager::WaitForInitialization();
	if (r == KErrNone)
		{
		// domainsrv publishes its "initialised" property in CDmSvrManager::Construct,
		// just BEFORE E32Main starts the two servers -- so allow a bounded retry
		for (TInt i = 0; i < 50; i++)
			{
			r = RDmDomainManager::AddDomainHierarchy((TDmHierarchyId)hier);
			if (r != KErrNotFound)
				break;
			User::After(20000);
			}
		if (r == KErrAlreadyExists)
			r = KErrNone;
		}
	RDebug::Printf("[bootui] domain hierarchy %d: add -> %d", hier, r);
	RDmDomainManager mgr;
	if (r == KErrNone)
		{
		r = mgr.Connect((TDmHierarchyId)hier);
		RDebug::Printf("[bootui] domain hierarchy %d: connect -> %d", hier, r);
		}
	if (r == KErrNone)
		{
		TRequestStatus st;
		mgr.RequestSystemTransition((TDmDomainState)state, ETraverseDefault, st);
		User::WaitForRequest(st);
		r = st.Int();
		mgr.Close();
		}
	RDebug::Printf("[bootui] domain hierarchy %d -> state %08x: %d", hier, state, r);
	}

static void SetUtcOffset(const TDesC8& aSpec)
	{
	TLex8 lex(aSpec);
	TInt minutes = 0;
	TInt r = lex.Val(minutes);
	if (r == KErrNone)
		User::SetUTCOffset(TTimeIntervalSeconds(minutes * 60));
	RDebug::Printf("[bootui] UTC offset %d min -> %d", minutes, r);
	}

// %<tsy>: load a phone module into ETel, as a phone's startup does before any app runs. S60 code
// that asks ETel for "phone 0" (IMUM's IMEI read: EnumeratePhones < 1 -> KErrNotFound, so
// Messaging exited, X4.58) assumes it is there; CTelephony loads it itself, most callers do not.
// ETel unloads a TSY when the session that loaded it closes: this one stays open for good.
static RTelServer TheTelServer;

static void LoadPhoneModule(const TDesC8& aTsy)
	{
	TBuf<32> tsy;
	tsy.Copy(aTsy.Left(32));
	TInt r = TheTelServer.Handle() ? KErrNone : TheTelServer.Connect();
	if (r == KErrNone)
		r = TheTelServer.LoadPhoneModule(tsy);
	TInt phones = 0;
	if (r == KErrNone)
		TheTelServer.EnumeratePhones(phones);
	RDebug::Printf("[bootui] phone module %S -> %d (%d phones)", &aTsy, r, phones);
	}

// ^: wait until apparc's first application-list scan is complete. A UI app asks apparc for its own
// TApaAppInfo in CEikApplication::PreDocConstructL and leaves KErrNotFound while the scan is still
// running (Messaging as the foreground app, X4.59: the more apps registered, the longer the scan).
// A phone's startup waits for the list the same way before it starts applications.
static void WaitAppList()
	{
	RApaLsSession ls;
	TInt r = ls.Connect();
	if (r == KErrNone)
		{
		TRequestStatus st;
		ls.RegisterListPopulationCompleteObserver(st);
		User::WaitForRequest(st);
		r = st.Int();
		ls.Close();
		}
	RDebug::Printf("[bootui] app list populated -> %d", r);
	}

static void StartCapServer()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	TRAPD(err, RAknUiServer::StartAknCapserverL());
	delete tc;
	RDebug::Printf("[bootui] StartAknCapserverL: %d", err);
	}

// +lights (X4.70): the display backlight on, the way S60's SysAp lights controller does at startup -- the REAL
// CHWRMLight::LightOnL on the primary display, the session held for the life of bootui (SysAp holds its own). HWRM
// starts every light target ELightStatusUnknown and only a client command makes it known; the homescreen's
// Active Idle framework runs its content publishers only while "foreground AND backlight on" (aistatemanager.cpp),
// so without SysAp its date / profile widget stayed empty.
static CHWRMLight* TheLight;
static void DisplayLightOn()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	// CHWRMLight owns a status-observer active object: it needs an installed scheduler (never started here --
	// bootui does not want the callbacks)
	if (!CActiveScheduler::Current())
		CActiveScheduler::Install(new CActiveScheduler);
	TRAPD(err, TheLight = CHWRMLight::NewL();
		TheLight->LightOnL(CHWRMLight::EPrimaryDisplay, KHWRMInfiniteDuration));
	delete tc;
	RDebug::Printf("[bootui] display light on: %d", err);
	}

// *<secs>|<app>|<app>|...  (X4.53) the app regression in ONE boot: start each app in turn, give it
// <secs> seconds (polled each second), then either report how it died or -- still alive -- print
// "[shot] <app>" (the host's qmon.py --watch screendumps on it), hold 4 s, and kill it before the next.
// Ends with "[cycle] end", on which the host stops QEMU.
// X4.78, SSM boots: S60's startup lists preload some apps in the background (matrixmenu, phonebook2, logs, clock);
// a second instance hands over to the first and exits. As the task switcher does: find the running process's
// window group and bring it to the front (aToFront) or send it back. Returns ETrue if the app was already running.
// Matched by the process's IMAGE (RProcess::FileName): S60 apps rename their process to the caption
// ("Menu", "Phonebook 2", "Telephone"), so the exe name is not the process name.
static TBool FrontExisting(const TDesC& aExe, TBool aToFront)
	{
	TFileName want(aExe);
	if (want.Right(4).CompareF(_L(".exe")) != 0)
		want.Append(_L(".exe"));
	TFindProcess fp(_L("*"));
	TFullName name;
	TProcessId pid;
	TBool found = EFalse;
	while (!found && fp.Next(name) == KErrNone)
		{
		RProcess proc;
		if (proc.Open(fp) != KErrNone)
			continue;
		TParsePtrC image(proc.FileName());
		if (proc.ExitType() == EExitPending && image.NameAndExt().CompareF(want) == 0)
			{
			pid = proc.Id();
			found = ETrue;
			}
		proc.Close();
		}
	if (!found)
		return EFalse;
	RWsSession ws;
	if (ws.Connect() == KErrNone)
		{
		RArray<RWsSession::TWindowGroupChainInfo> wgs;
		if (ws.WindowGroupList(&wgs) == KErrNone)
			for (TInt i=0; i<wgs.Count(); ++i)
				{
				TThreadId tid;
				RThread th;
				if (ws.GetWindowGroupClientThreadId(wgs[i].iId, tid) != KErrNone || th.Open(tid) != KErrNone)
					continue;
				RProcess owner;
				TBool mine = th.Process(owner) == KErrNone && owner.Id() == pid;
				owner.Close();
				th.Close();
				if (mine && wgs[i].iParentId <= 0)
					{
					// the task switcher's call: reorder AND tell the app (EApaSystemEventBroughtToForeground --
					// view-based apps such as Calendar and Clock activate their top view on it)
					TApaTask task(ws);
					task.SetWgId(wgs[i].iId);
					if (aToFront)
						task.BringToForeground();
					else
						task.SendToBackground();
					break;
					}
				}
		wgs.Close();
		ws.Close();
		}
	return ETrue;
	}

// X4.79: `0x<uid>` in the cycle = launch the application by its UID through apparc, as the Menu does -- the only
// way to start a non-native app (a WRT widget: apparc runs its launcher, widgetlauncher -> WidgetUi). Alive = a task
// with that UID exists after <secs>; then the shot, and the task is ended.
// X4.101: `0x<uid>=<document>` = the same, opened on a document (EApaCommandOpen + the document name, as a caller of
// RApaLsSession::StartDocument hands BrowserNG a URL: `0x10008D39=https://www.example.com`).
static void CycleByUid(const TDesC8& aApp, TInt aSecs)
	{
	TLex8 lex(aApp.Mid(2));
	TUint32 u = 0;
	if (lex.Val(u, EHex) != KErrNone)
		{
		RDebug::Printf("[cycle] %S: bad uid", &aApp);
		return;
		}
	const TUid uid = TUid::Uid(u);
	CTrapCleanup* tc = CTrapCleanup::New();		// bootui's thread has none (E32USER-CBase 69 in CApaCommandLine::NewL)
	RApaLsSession ls;
	TInt r = ls.Connect();
	TApaAppInfo info;
	if (!r)
		r = ls.GetAppInfo(info, uid);
	if (!r)
		{
		CApaCommandLine* cmd = NULL;
		TRAP(r, cmd = CApaCommandLine::NewL());
		if (!r)
			{
			TRAP(r, cmd->SetExecutableNameL(info.iFullName));
			cmd->SetCommandL(EApaCommandRun);
			const TInt eq = aApp.Locate('=');
			if (!r && eq > 0)
				{
				TBuf<256> doc;
				doc.Copy(aApp.Mid(eq + 1).Left(256));
				TRAP(r, cmd->SetDocumentNameL(doc));
				cmd->SetCommandL(EApaCommandOpen);
				}
			if (!r)
				r = ls.StartApp(*cmd);
			delete cmd;
			}
		}
	ls.Close();
	if (r)
		{
		RDebug::Printf("[cycle] %S: launch failed %d", &aApp, r);
		delete tc;
		return;
		}
	RDebug::Printf("[bootui] %S: started (apparc)", &aApp);
	User::After(aSecs * 1000000);
	RWsSession ws;
	TBool alive = EFalse;
	if (ws.Connect() == KErrNone)
		{
		TApaTaskList tasks(ws);
		TApaTask task = tasks.FindApp(uid);
		alive = task.Exists();
		if (!alive)
			{
			// a non-native app runs inside its runtime's task (a WRT widget in WidgetUi, 0x10282822): accept the
			// FRONT task unless the launch fell back to the homescreen (0x102750F0) or the menu (0x101F4CD2)
			TApaTask frontTask = tasks.FindByPos(0);
			CApaWindowGroupName* wgn = NULL;
			TUid front = KNullUid;
			if (frontTask.Exists())
				{
				TRAP_IGNORE(wgn = CApaWindowGroupName::NewL(ws, frontTask.WgId()));
				if (wgn)
					front = wgn->AppUid();
				delete wgn;
				}
			alive = frontTask.Exists() && front != KNullUid && front.iUid != 0x102750F0 && front.iUid != 0x101F4CD2;
			if (alive)
				task.SetWgId(frontTask.WgId());
			if (alive)
				RDebug::Printf("[cycle] %S: hosted by the front task 0x%08x", &aApp, front.iUid);
			}
		RDebug::Printf("[cycle] %S: %s after %d s", &aApp, alive ? "ALIVE" : "DIED", aSecs);
		if (alive)
			{
			task.BringToForeground();
			RDebug::Printf("[shot] %S", &aApp);
			User::After(4000000);
			task.EndTask();
			User::After(2000000);
			}
		ws.Close();
		}
	delete tc;
	}

static void CycleApps(const TDesC8& aSpec)
	{
	// SSM boot (X4.78): the AMA starter runs us in the non-critical phase, while SSM is still starting the
	// preloaded apps -- wait, as a startup-list app should, for KPSStartupUiPhase = EStartupUiPhaseAllDone (104;
	// "clients can begin to use UI services"), then let the non-critical SWP lists finish. Under BOOT=@ bootui
	// published 104 itself long before, so this returns at once.
	const TUid KPSUidStartupCat = { 0x101F8766 };
	const TUint32 KPSStartupUiPhaseKey = 0x46;
	TInt phase = 0, waited = 0;
	while ((RProperty::Get(KPSUidStartupCat, KPSStartupUiPhaseKey, phase) != KErrNone || phase < 104) && waited < 180)
		{
		User::After(1000000);
		++waited;
		}
	if (waited)
		{
		RDebug::Printf("[cycle] startup UI phase %d after %d s; settling 20 s", phase, waited);
		User::After(20000000);
		}
	TInt bar = aSpec.Locate('|');
	TInt secs = 20;
	TPtrC8 num(bar < 0 ? aSpec.Left(aSpec.Length()) : aSpec.Left(bar));
	TLex8 lex(num);
	if (lex.Val(secs) != KErrNone || secs <= 0)
		secs = 20;
	TPtrC8 rest = bar < 0 ? TPtrC8() : aSpec.Mid(bar + 1);
	while (rest.Length())
		{
		TInt b = rest.Locate('|');
		// the name is COPIED: with NET=1 the list text read as empty by the time the app had
		// run (the [cycle]/[shot] lines named no app), so nothing below points into it
		TBuf8<64> app;
		app.Copy((b < 0 ? rest : rest.Left(b)).Left(60));
		rest.Set(b < 0 ? TPtrC8() : rest.Mid(b + 1));
		if (!app.Length())
			continue;
		TBuf<64> exe;
		exe.Copy(app.Left(60));
		if (app.Length() > 2 && app[0] == '0' && (app[1] == 'x' || app[1] == 'X'))
			{
			CycleByUid(app, secs);
			continue;
			}
		if (FrontExisting(exe, ETrue))
			{
			User::After(secs * 1000000);
			const TBool alive = FrontExisting(exe, EFalse);	// ... and back to the background after the shot
			RDebug::Printf("[cycle] %S: %s (already running, brought to the front)", &app, alive ? "ALIVE" : "DIED");
			if (alive)
				{
				FrontExisting(exe, ETrue);
				RDebug::Printf("[shot] %S", &app);
				User::After(4000000);
				FrontExisting(exe, EFalse);
				User::After(1000000);
				}
			continue;
			}
		RProcess p;
		TInt r = p.Create(exe, KNullDesC);
		if (r != KErrNone)
			{
			RDebug::Printf("[cycle] %S: Create failed %d", &app, r);
			continue;
			}
		p.Resume();
		RDebug::Printf("[bootui] %S: started", &app);
		TInt t = 0;
		for (; t < secs && p.ExitType() == EExitPending; ++t)
			User::After(1000000);
		if (p.ExitType() != EExitPending)
			{
			TBuf8<KMaxExitCategoryName> cat;
			cat.Copy(p.ExitCategory());
			RDebug::Printf("[cycle] %S: DIED after %d s: exit type %d reason %d %S", &app, t, p.ExitType(),
						   p.ExitReason(), &cat);
			}
		else
			{
			RDebug::Printf("[cycle] %S: ALIVE after %d s", &app, t);
			RDebug::Printf("[shot] %S", &app);
			User::After(4000000);
			TRequestStatus st;
			p.Logon(st);
			p.Kill(KErrNone);
			User::WaitForRequest(st);
			User::After(1000000);           // let the window server and the app's servers settle
			}
		p.Close();
		}
	RDebug::Printf("[cycle] end");
	}

static void StartOne(const TDesC& aExe, const TDesC& aArgs, TBool aWait)
	{
	RProcess p;
	TInt r = p.Create(aExe, aArgs);
	TBuf8<64> n8;
	n8.Copy(aExe.Left(60));
	if (r != KErrNone)
		{
		RDebug::Printf("[bootui] %S: Create failed %d", &n8, r);
		return;
		}
	TRequestStatus st;
	p.Rendezvous(st);
	p.Resume();
	if (aWait)
		{
		User::WaitForRequest(st);
		RDebug::Printf("[bootui] %S: rendezvous %d (exit type %d)", &n8, st.Int(), p.ExitType());
		}
	else
		{
		RDebug::Printf("[bootui] %S: started", &n8);
		// leave the rendezvous request outstanding; this process then idles forever
		}
	p.Close();
	}

GLDEF_C TInt E32Main()
	{
	RFs fs;
	TInt r = fs.Connect();
	RFile f;
	// X4.53: C:\bootui.txt (the disk, DISK=1) overrides the ROM's list -- which app to run / test
	// without rebuilding the ROM (app_regress.sh writes it into a scratch copy of the disk image)
	TInt rc = KErrNotFound;
	if (r == KErrNone)
		{
		rc = f.Open(fs, _L("C:\\bootui.txt"), EFileRead | EFileShareReadersOnly);
		r = rc == KErrNone ? rc : f.Open(fs, _L("Z:\\sys\\data\\bootui.txt"), EFileRead | EFileShareReadersOnly);
		}
	if (r != KErrNone)
		{
		RDebug::Printf("[bootui] no bootui.txt (%d)", r);
		return r;
		}
	RDebug::Printf("[bootui] list from %s", rc == KErrNone ? "C:\\bootui.txt" : "Z:\\sys\\data\\bootui.txt");
	TBuf8<1024> text;
	f.Read(text);
	f.Close();
	fs.Close();
	TInt pos = 0;
	while (pos < text.Length())
		{
		TPtrC8 rest = text.Mid(pos);
		TInt nl = rest.Locate('\n');
		TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
		pos += (nl < 0 ? rest.Length() : nl + 1);
		while (line.Length() && (line[line.Length() - 1] == '\r' || line[line.Length() - 1] == ' '))
			line.Set(line.Left(line.Length() - 1));
		if (!line.Length() || line[0] == '#')
			continue;
		if (line[0] == '=')
			{
			DefineOne(line.Mid(1));
			continue;
			}
		if (line[0] == '~')
			{
			DomainTransition(line.Mid(1));
			continue;
			}
		if (line[0] == '@')
			{
			SetUtcOffset(line.Mid(1));
			continue;
			}
		if (line[0] == '+')
			{
			if (line.Mid(1) == _L8("lights"))
				DisplayLightOn();
			else
				StartCapServer();
			continue;
			}
		if (line[0] == '%')
			{
			LoadPhoneModule(line.Mid(1));
			continue;
			}
		if (line[0] == '^')
			{
			WaitAppList();
			continue;
			}
		if (line[0] == '*')
			{
			CycleApps(line.Mid(1));
			continue;
			}
		TBool wait = ETrue;
		if (line[0] == '!')
			{
			wait = EFalse;
			line.Set(line.Mid(1));
			}
		TInt bar = line.Locate('|');
		TBuf<64> exe;
		exe.Copy(line.Left(Min(bar < 0 ? line.Length() : bar, 60)));
		TBuf<256> args;
		if (bar >= 0)
			{
			args.Copy(line.Mid(bar + 1).Left(250));
			for (TInt i = 0; i < args.Length(); ++i)
				if (args[i] == '|')
					args[i] = ' ';
			}
		StartOne(exe, args, wait);
		}
	RDebug::Printf("[bootui] done");
	for (;;)
		User::After(60 * 1000000);
	}
