// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// OURS: the REAL AlfDecoderServerClient/src/alfdecoderserverclient.cpp with "PH:" lines in the alfredserver thread, plus a SAMPLING WATCHDOG:
// a second thread ("alfwatch", same process = WSERV) that every 2 s suspends the alf server thread, reads its user registers
// (RThread::Context) and prints "PH: alf: watch pc=.. lr=.. sp=.." (symbolise with tools/romsym.py against the ROM map), then resumes it. A thread
// that sits in a loop or a wait shows the same pc each time -- no instrumentation needed to find where it is.
#include <e32debug.h>
#include <e32property.h>
#include <centralrepository.h>
#include <alf/alfappui.h>
#include "alfdecoderserverclient.h"
#include "alfstreamerconsts.h"
#include "alfuids.h"

class TPhArmRegs { public: TUint32 r[18]; };		// == the kernel's TArmRegSet: R0-R15, flags, DACR

// every thread of THIS process (WSERV's main thread, the compositor job thread, alfredserver, alfevents, ...) every 3 s: where each one sits.
// The suspended thread's user registers: a pc in the same place twice = a spin or a wait there (symbolise with tools/romsym.py + the ROM map).
// prints only when a thread's pc/lr CHANGED since the last sample (a parked thread is silent), plus every thread once per 30 s as a heartbeat
static TUint32 PhAlfLastPc[64], PhAlfLastLr[64];
static TUint PhAlfLastId[64];
static void PhAlfSample(RThread& aT, TInt aRound, const TDesC& aName)
	{
	TPhArmRegs regs;
	Mem::FillZ(&regs, sizeof(regs));
	TPckg<TPhArmRegs> pk(regs);
	aT.Suspend();
	if (aT.ExitType() != EExitPending)		// it ended between the caller's check and the suspend
		return;
	aT.Context(pk);
	aT.Resume();
	const TUint id = (TUint)aT.Id();
	TInt slot = -1;
	for (TInt i = 0; i < 64; i++)
		if (PhAlfLastId[i] == id) { slot = i; break; }
	if (slot < 0)
		for (TInt i = 0; i < 64; i++)
			if (!PhAlfLastId[i]) { slot = i; PhAlfLastId[i] = id; PhAlfLastPc[i] = 1; break; }
	const TBool changed = slot < 0 || PhAlfLastPc[slot] != regs.r[15] || PhAlfLastLr[slot] != regs.r[14];
	if (slot >= 0) { PhAlfLastPc[slot] = regs.r[15]; PhAlfLastLr[slot] = regs.r[14]; }
	if (!changed && (aRound % 10) != 0)
		return;
	TBuf8<96> n8;
	n8.Copy(aName.Left(80));
	RDebug::Printf("PH: alf: watch #%d %s pc=%08x lr=%08x sp=%08x%s", aRound, (const char*)n8.PtrZ(), regs.r[15], regs.r[14], regs.r[13], changed ? " (moved)" : "");
	}

static TInt PhAlfWatchMain(TAny* aId)
	{
	RThread srv;
	if (srv.Open(TThreadId((TUint)aId)) != KErrNone)
		return 0;
	RProcess me;
	TProcessId pid = me.Id();
	TThreadId self = RThread().Id();
	RDebug::Printf("PH: alf: watch started (alfwatch thread, sampling the threads of this process every 3 s)");
	TBool lightsDone = EFalse;
	for (TInt i = 0; i < 400 && srv.ExitType() == EExitPending; i++)
		{
		User::After(3000000);
		if (!lightsDone)
			{
			// ALF test images: SysAp's "forced lights on" (coreapplicationuisdomainpskeys.h KLightsVTForcedLightsOn = 0x202, EForcedLightsOn = 2: "until eternity")
			// keeps the display on while frames take seconds; SysAp defines the key late in its start-up, so retry until the Set works.
			const TInt lr = RProperty::Set(TUid::Uid(0x101F8767), 0x202, 2);
			if (lr == KErrNone)
				lightsDone = ETrue;
			if (lr == KErrNone || i == 3 || i == 20)
				RDebug::Printf("PH: alf: forced lights on (P&S 101F8767/0x202 = 2) -> %d", lr);
			}
		TFindThread ft(_L("*"));
		TFullName fn;
		while (ft.Next(fn) == KErrNone)
			{
			RThread t;
			if (t.Open(ft) != KErrNone)
				continue;
			RProcess p;
			if (t.Process(p) == KErrNone)
				{
				if (p.Id() == pid && t.Id() != self && t.ExitType() == EExitPending)
					PhAlfSample(t, i, fn);
				p.Close();
				}
			t.Close();
			}
		}
	srv.Close();
	RDebug::Printf("PH: alf: watch ended");
	return 0;
	}

// GPU mode: switch the OpenVG facade of this process to the GPU engine (phvg.dll: fills / gradients / strokes on the GPU; images and masked
// blits stay on the CPU = the RI fast blit). The developer menu's "UI GPU acceleration" (repository 0xE7015A01 key 8, src/phedevmenu/phedevmenu.h;
// 0 = off, the default) decides at every start -- a runtime setting, not a build switch (an env-only build switch used to stay on silently in
// partial builds). The engine reads an int property KPhvgModeKey in the process's own category (its SID; src/phvg/phvg.h, riPhvg.cpp ReadMode).
static void PhAlfPhvgOn()
	{
	TInt v = 0;
	// (called first thing in the alfredserver thread, before it has a cleanup stack: CRepository::NewL needs one, CBase 69)
	CTrapCleanup* tc = User::TrapHandler() ? NULL : CTrapCleanup::New();
	TRAPD(err, CRepository* rep = CRepository::NewL(TUid::Uid(0xE7015A01)); rep->Get(8, v); delete rep);
	delete tc;
	if (err != KErrNone)
		v = 0;
	const TUid cat = TUid::Uid((TInt)RProcess().SecureId());
	const TUint key = 0x50564731;			// 'PVG1' (phvg.h KPhvgModeKey)
	TInt r = RProperty::Define(cat, key, RProperty::EInt);
	if (r == KErrNone || r == KErrAlreadyExists)
		r = RProperty::Set(cat, key, v ? 1 : 0);
	RDebug::Printf("PH: alf: GPU acceleration setting %d (repository %d) -> phvg mode property %d: %d", v, err, v ? 1 : 0, r);
	}

// The watchdog is a debugging aid, built in only on request (ALF_WATCH=1, registry/components.py): it suspends every thread of the
// window server (the compositor and the alf server included) every 3 s and forces the display lights on for good.
static void PhAlfStartWatch()
	{
#ifndef PH_ALF_WATCH
	return;
#endif
	RThread w;
	const TInt r = w.Create(_L("alfwatch"), PhAlfWatchMain, 0x4000, NULL, (TAny*)RThread().Id().Id());
	RDebug::Printf("PH: alf: starting the watchdog thread -> %d", r);
	if (r == KErrNone)
		{
		w.SetPriority(EPriorityAbsoluteForeground);
		w.Resume();
		w.Close();
		}
	}

#define PH_PHVG_ON() PhAlfPhvgOn()
#define RenameThread(n) RenameThread(n) + (RDebug::Printf("PH: alf: alfredserver thread running, frame %x", (TUint)__builtin_frame_address(0)), PhAlfStartWatch(), PH_PHVG_ON(), 0)
#define ConstructL() ConstructL(); RDebug::Printf("PH: alf: alfredserver thread: CAlfAppUi::ConstructL returned, scheduler starting")

// UI_RENDERER=both: in the Classic chain there is NO alf server, but the real clients (homescreen's effect observer
// CAlfEffectObserver::NewL, CAlfBufferFlush, ...) still run and used to LEAVE -1 on Connect (alf-plan.md "no client-only stage"). WSERV
// (wserv_board/inifile_board.cpp) publishes the chosen renderer; while it says Classic, "server not found" is a soft success: the session
// stays unopened and every send on it answers KErrNotReady (async: the request completes with it) -- an inert client, as the null one.
// No property (a ROM without the switch) or Alfred = the real behaviour, byte for byte.
#include "../wserv_board/ph_renderer.h"
static TBool PhAlfClassic()
	{
	TInt v = 1;
	return RProperty::Get(TUid::Uid(PH_RENDERER_PS_CATEGORY), PH_RENDERER_PS_KEY, v) == KErrNone && v == 0;
	}
static TInt PhAlfSoft(TInt aErr)
	{
	return (aErr == KErrNotFound && PhAlfClassic()) ? KErrNone : aErr;
	}
static void PhAlfNotReady(TRequestStatus& aStatus)
	{
	TRequestStatus* p = &aStatus;
	aStatus = KRequestPending;
	User::RequestComplete(p, KErrNotReady);
	}
#define PH_SEL(_1, _2, _3, NAME, ...) NAME
#define PH_SR2(a, b) (Handle() ? RSessionBase::SendReceive(a, b) : (TInt)KErrNotReady)
#define PH_SR3(a, b, c) (Handle() ? (void)RSessionBase::SendReceive(a, b, c) : PhAlfNotReady(c))
#define SendReceive(...) PH_SEL(__VA_ARGS__, PH_SR3, PH_SR2)(__VA_ARGS__)
#define Send(a, b) (Handle() ? RSessionBase::Send(a, b) : (TInt)KErrNotReady)
#define CreateSession(n, v) PhAlfSoft(RSessionBase::CreateSession(n, v))
#include "gen/alfdecoderserverclient_stack.cpp"   // = the original with the thread stack raised 16 KB -> 64 KB (tools/alf_instrument.py)
