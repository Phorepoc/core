// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phemenuwarm.cpp -- OURS: make the FIRST open of the Applications grid as fast as every later one.
//
// Why: the 2010 launcher (matrixmenu) is started at boot, in the
// background, by SSM (securitycheckcmdlist r_cmd_menu, "background = 1") -- but it keeps its real grid container
// INVISIBLE until the first "foreground gained" event (CMmAppUi::HandleFullOrPartialForegroundGainedL swaps the
// dummy container for the real one). WSERV only draws a visible window, so the grid's icons are created and
// rasterised (WSERV's CSvgtFbsRasterizer: 20-1500 ms per NVG icon in the software RI) on the first Menu press.
//
// What: once matrixmenu has finished constructing (its P&S property KMMenuLastViewKey in category 0x101F4CD2 appears
// at the END of CMmAppUi::ConstructL) and the system has quietened after boot, bring it to the foreground ONCE, let
// WSERV draw the grid (every icon is rasterised into WSERV's recent-bitmap cache and the persistent
// C:\private\10003b20\iconcache), wait until WSERV answers promptly again, and send it back. The foreground change
// is the Menu key's own (TApaTask), so the launcher's own foreground/background handlers run exactly as for a user.
//
// Log lines (serial, all "PH: [menuwarm]"): started, matrixmenu ready after N ms, icon cache files before/after,
// warmed in N ms, done.
#include <e32base.h>
#include <e32property.h>
#include <e32debug.h>
#include <f32file.h>
#include <w32std.h>
#include <apgtask.h>
#include <hal.h>

static TInt FreeKb()
	{
	TInt v = 0;
	HAL::Get(HALData::EMemoryRAMFree, v);
	return v / 1024;
	}

const TUid KMatrixMenuUid = { 0x101F4CD2 };		// the launcher's UID3 and its P&S category
const TUint KMMenuLastViewKey = 0x01;

// the persistent rasterised-icon caches (SvgtFbsRasterizer.cpp): one directory per drawing process
_LIT(KWsCache, "C:\\private\\10003b20\\iconcache\\");	// WSERV
_LIT(KMmCache, "C:\\private\\101f4cd2\\iconcache\\");	// matrixmenu itself

// tunables (ms)
const TInt KReadyTimeoutMs = 120000;	// matrixmenu never appeared / never finished: give up
const TInt KSettleMs = 4000;			// after it is ready: let the rest of boot (homescreen, Phone, SysAp) finish
const TInt KMinHoldMs = 2000;			// shortest time the grid stays in front
const TInt KMaxHoldMs = 25000;			// longest (cold first-ever boot: ~25 NVG icons, ~3.4 s on the phone)
const TInt KProbeStepMs = 250;			// WSERV round-trip probe interval
const TInt KProbeBusyMs = 40;			// a round trip slower than this = WSERV is still drawing
const TInt KQuietProbes = 4;			// consecutive quiet probes = the grid is drawn

static TInt TickMs()
	{
	// elapsed ms since the first call, from the universal clock (NTickCount's period differs between the nanokernel
	// and system tick, which read 7x off under QEMU)
	static TTime base;
	static TBool have = EFalse;
	TTime now;
	now.UniversalTime();
	if (!have)
		{
		base = now;
		have = ETrue;
		}
	return (TInt)(now.MicroSecondsFrom(base).Int64() / 1000);
	}

static TInt CountFiles(RFs& aFs, const TDesC& aDir)
	{
	CDir* d = NULL;
	TInt n = 0;
	if (aFs.GetDir(aDir, KEntryAttNormal, ESortNone, d) == KErrNone && d)
		n = d->Count();
	delete d;
	return n;
	}

static void DoWarmL()
	{
	RDebug::Printf("PH: [menuwarm] started");
	const TInt t0 = TickMs();
	// 1. matrixmenu constructed? (the property is defined as the last step of ConstructL)
	TInt ready = EFalse;
	while (TickMs() - t0 < KReadyTimeoutMs)
		{
		TBuf<8> text;
		if (RProperty::Get(KMatrixMenuUid, KMMenuLastViewKey, text) != KErrNotFound)
			{
			ready = ETrue;
			break;
			}
		User::After(500000);
		}
	if (!ready)
		{
		RDebug::Printf("PH: [menuwarm] matrixmenu did not become ready in %d ms -- nothing warmed", KReadyTimeoutMs);
		return;
		}
	RDebug::Printf("PH: [menuwarm] matrixmenu ready after %d ms", TickMs() - t0);
	// 2. let the rest of boot finish
	User::After(KSettleMs * 1000);

	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	const TInt wsBefore = CountFiles(fs, KWsCache);
	const TInt mmBefore = CountFiles(fs, KMmCache);

	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CleanupClosePushL(ws);
	TApaTaskList tasks(ws);
	TApaTask menu = tasks.FindApp(KMatrixMenuUid);
	if (!menu.Exists())
		{
		RDebug::Printf("PH: [menuwarm] matrixmenu has no window group -- nothing warmed");
		CleanupStack::PopAndDestroy(2);
		return;
		}
	// remember who was in front, to give the screen back to it
	// 2026-10-18 (backlog "Menu press eaten"): warm only while the user is idle (>= 5 s = the lights have dimmed), so a Menu
	// press cannot land on the warmed grid and toggle it away; give up after 2 minutes of continuous use
	{
	const TInt w0 = TickMs();
	while (User::InactivityTime().Int() < 5 && TickMs() - w0 < 120000)
		User::After(500000);
	RDebug::Printf("PH: [menuwarm] idle wait %d ms (inactive %d s)", TickMs() - w0, User::InactivityTime().Int());
	}
	const TInt prevFront = ws.GetFocusWindowGroup();
	const TInt freeBefore = FreeKb();
	const TInt t1 = TickMs();
	menu.BringToForeground();
	// 3. hold until WSERV has drawn the grid: min hold, then KQuietProbes round trips in a row answered promptly
	TInt quiet = 0;
	TInt probes = 0;
	TInt worst = 0;
	while (TickMs() - t1 < KMaxHoldMs)
		{
		User::After(KProbeStepMs * 1000);
		const TInt p0 = TickMs();
		ws.Flush();
		ws.SyncMsgBuf();
		const TInt dt = TickMs() - p0;
		++probes;
		if (dt > worst) worst = dt;
		if (TickMs() - t1 < KMinHoldMs)
			continue;
		quiet = (dt <= KProbeBusyMs) ? quiet + 1 : 0;
		if (quiet >= KQuietProbes)
			break;
		}
	const TInt held = TickMs() - t1;
	// 2026-10-18 (boots 1001/1002): a Menu press DURING the warm-up was undone by the restore below (the grid flashed and
	// went away). Any user activity since the grid came forward = the user owns the screen now: leave it as it is.
	if (User::InactivityTime().Int() * 1000 < held)
		{
		RDebug::Printf("PH: [menuwarm] user activity during the warm-up (%d ms held): left the screen to the user", held);
		CleanupStack::PopAndDestroy(2);
		return;
		}
	const TInt freeShown = FreeKb();
	// 4. back where it was (matrixmenu sends itself back on Exit/Back; here we do it as the task switcher does)
	menu.SendToBackground();
	if (prevFront > 0)
		{
		TApaTask prev(ws);
		prev.SetWgId(prevFront);
		prev.BringToForeground();
		}
	ws.Flush();
	const TInt wsAfter = CountFiles(fs, KWsCache);
	const TInt mmAfter = CountFiles(fs, KMmCache);
	RDebug::Printf("PH: [menuwarm] grid warmed: held %d ms, %d probes (worst WSERV round trip %d ms); icon cache files wserv %d -> %d, matrixmenu %d -> %d",
		held, probes, worst, wsBefore, wsAfter, mmBefore, mmAfter);
	RDebug::Printf("PH: [menuwarm] free RAM %d KB before, %d KB with the grid shown, %d KB after (matrixmenu stays resident either way)", freeBefore, freeShown, FreeKb());
	RDebug::Printf("PH: [menuwarm] icons pre-warmed %d (new cache files %d; 0 new = every icon was already a disk hit)",
		(wsAfter - wsBefore) + (mmAfter - mmBefore), (wsAfter - wsBefore) + (mmAfter - mmBefore));
	CleanupStack::PopAndDestroy(2);
	}

TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	if (!tc)
		return KErrNoMemory;
	TRAPD(err, DoWarmL());
	RDebug::Printf("PH: [menuwarm] done (%d)", err);
	delete tc;
	return err;
	}
