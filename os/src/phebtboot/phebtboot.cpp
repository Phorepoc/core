// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phebtboot -- OURS (2026-11-20): switches Bluetooth on at boot when the stored setting says on.
//
// The Bluetooth settings screen shows the CenRep key KBTPowerState (KCRUidBluetoothPowerState 0x10204DA9, key 1: 0 off, 1 on), but
// nothing in this ROM applies it at boot: no SSM command list starts the Bluetooth engine server (btengsrv is started by its
// first client), the engine only restores power from the key once it runs and is asked (CBTEngSrvSettingsMgr), and SysAp
// restores Bluetooth only when it leaves offline mode (CSysApAppUi iActivateBt). The setting therefore read "Enabled" while the
// radio stayed off. This process is the missing client: it reads the key and, when it is on, asks the engine to power on
// (the same call the Settings screen makes), holds the session while the patch downloads and the stack initialises, and exits.
// Started by the non-critical SSM list (noncriticalcmdlist_hw.rss); a ROM without this exe skips it.
#include <e32base.h>
#include <e32debug.h>
#include <centralrepository.h>
#include <btserversdkcrkeys.h>
#include <btengsettings.h>

static const TInt KHoldSeconds = 15;      // the firmware patch download + the stack's init take about 3 s; the engine drops the stack ~3 s after its last client

static void RunL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);

	TInt stored = EBTPowerOff;
	CRepository* repo = CRepository::NewLC(KCRUidBluetoothPowerState);
	const TInt err = repo->Get(KBTPowerState, stored);
	CleanupStack::PopAndDestroy(repo);
	RDebug::Printf("[phebtboot] stored power state %d (read %d)", stored, err);
	if (err == KErrNone && stored == EBTPowerOn)
		{
		CBTEngSettings* bt = CBTEngSettings::NewLC();
		const TInt r = bt->SetPowerState(EBTPowerOn);
		RDebug::Printf("[phebtboot] SetPowerState(on) -> %d", r);
		User::After(KHoldSeconds * 1000000);
		TBTPowerStateValue state = EBTPowerOff;
		const TInt g = bt->GetPowerState(state);
		RDebug::Printf("[phebtboot] GetPowerState -> %d, state %d (1 = on)", g, (TInt)state);
		CleanupStack::PopAndDestroy(bt);
		}
	CleanupStack::PopAndDestroy(sched);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		return KErrNoMemory;
	TRAPD(r, RunL());
	if (r != KErrNone)
		RDebug::Printf("[phebtboot] failed %d", r);
	delete cleanup;
	return r;
	}
