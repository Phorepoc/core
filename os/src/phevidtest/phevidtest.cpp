// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phevidtest.cpp -- OURS (VDU rung V4): plays an MP4 file through the REAL Symbian MMF video player
// stack -- CVideoPlayerUtility (mediaclientvideo) -> the MMF controller framework -> phevidctl (the MP4 controller, found by
// the file's extension + 'ftyp' header) -> CMMFDevVideoPlay -> phevdec (A64 Video Engine) -> NV12 -> the screen through
// the client's direct-screen-access region -- and reports frames and fps.
//
//   phevidtest                   plays z:\resource\phevde\vmain.mp4 (100 frames, B-frames, + an AAC track played through DevSound) and vbase.mp4
//                                (75 frames, 360x640) twice each: real time (PTS paced) and bench (decode only, no draw)
//   phevidtest <file> [frames]   one file, both passes; [frames] = the expected picture count
//
// "PH: vid" lines. A pass line reads
//   PH: vid vmain real: 320x240 100/100 pictures, drawn 100 dropped 0 no-region 0, wall 4010 ms (nominal 4000), 24.9 fps, late avg 3 max 21 ms PASS
// Real time PASSES when every picture was delivered and the wall time is within 10 % of the clip duration; bench PASSES
// when every picture was delivered (its fps is the decode + demux + MMF throughput).
// Needs a visible window: the keyguard hides everything at boot (PhoneLab: key power) -- the pictures are then counted as
// "no-region" and the run still passes on timing.
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <w32std.h>
#include <videoplayer.h>
#include <mmf/common/mmfcontrollerframework.h>
#include "phevidctl.h"

class CPlay : public CBase, public MVideoPlayerUtilityObserver
	{
public:
	CPlay(RWsSession& aWs, CWsScreenDevice& aScreen, RWindow& aWin, const TDesC& aFile, const TDesC8& aTag, TBool aBench, TInt aExpect)
		: iWs(aWs), iScreen(aScreen), iWin(aWin), iFile(aFile), iBench(aBench), iExpect(aExpect), iResult(KErrNone)
		{
		iTag.Copy(aTag);
		iTag.ZeroTerminate();
		}
	~CPlay() { delete iVideo; }
	void StartL();
	TInt iResult;
	TBool iPass;
	// MVideoPlayerUtilityObserver
	void MvpuoOpenComplete(TInt aError);
	void MvpuoPrepareComplete(TInt aError);
	void MvpuoFrameReady(CFbsBitmap&, TInt) {}
	void MvpuoPlayComplete(TInt aError);
	void MvpuoEvent(const TMMFEvent& aEvent)
		{
		RDebug::Printf("PH: vid %s event type %x err %d", iTag.Ptr(), aEvent.iEventType.iUid, aEvent.iErrorCode);
		}
private:
	void Fail(TInt aError)
		{
		iResult = aError;
		RDebug::Printf("PH: vid %s FAIL err %d", iTag.Ptr(), aError);
		CActiveScheduler::Stop();
		}
	RWsSession& iWs;
	CWsScreenDevice& iScreen;
	RWindow& iWin;
	TFileName iFile;
	TBuf8<24> iTag;
	TBool iBench;
	TInt iExpect;
	CVideoPlayerUtility* iVideo;
	TUint32 iT0;
	};

void CPlay::StartL()
	{
	const TRect r(TPoint(0, 0), iScreen.SizeInPixels());
	iVideo = CVideoPlayerUtility::NewL(*this, EMdaPriorityNormal, EMdaPriorityPreferenceTime, iWs, iScreen, iWin, r, r);
	RDebug::Printf("PH: vid %s open '%S'", iTag.Ptr(), &iFile);
	iVideo->OpenFileL(iFile);					// (no controller UID: the framework picks by extension + header)
	}

void CPlay::MvpuoOpenComplete(TInt aError)
	{
	RDebug::Printf("PH: vid %s open complete %d", iTag.Ptr(), aError);
	if (aError != KErrNone)
		{
		Fail(aError);
		return;
		}
	TSize sz;
	TRAPD(e1, iVideo->VideoFrameSizeL(sz));
	TReal32 fps = 0;
	TRAPD(e2, fps = iVideo->VideoFrameRateL());
	TTimeIntervalMicroSeconds d(0);
	TRAPD(e3, d = iVideo->DurationL());
	RDebug::Printf("PH: vid %s clip %dx%d (r=%d), %d.%02d fps (r=%d), duration %d ms (r=%d), mime %S", iTag.Ptr(), sz.iWidth, sz.iHeight, e1,
		(TInt)fps, (TInt)(fps * 100) % 100, e2, (TInt)(d.Int64() / 1000), e3, &iVideo->VideoFormatMimeType());
	iVideo->Prepare();
	}

void CPlay::MvpuoPrepareComplete(TInt aError)
	{
	RDebug::Printf("PH: vid %s prepare complete %d", iTag.Ptr(), aError);
	if (aError != KErrNone)
		{
		Fail(aError);
		return;
		}
	if (iBench)
		{
		TInt one = 1;
		TPckg<TInt> p(one);
		TMMFMessageDestinationPckg dest(TMMFMessageDestination(KPheVidCtlCmdIf, KMMFObjectHandleController));
		TBuf8<1> none;
		const TInt r = iVideo->CustomCommandSync(dest, EPheVidSetBench, p, none);
		RDebug::Printf("PH: vid %s bench mode r=%d", iTag.Ptr(), r);
		}
	iT0 = User::NTickCount();
	iVideo->Play();
	RDebug::Printf("PH: vid %s play", iTag.Ptr());
	}

void CPlay::MvpuoPlayComplete(TInt aError)
	{
	const TUint32 wall = User::NTickCount() - iT0;
	TPheVidStats s;
	Mem::FillZ(&s, sizeof(s));
	TPckg<TPheVidStats> out(s);
	TMMFMessageDestinationPckg dest(TMMFMessageDestination(KPheVidCtlCmdIf, KMMFObjectHandleController));
	TBuf8<1> none;
	const TInt r = iVideo->CustomCommandSync(dest, EPheVidGetStats, none, none, out);
	if (r != KErrNone)
		RDebug::Printf("PH: vid %s stats r=%d", iTag.Ptr(), r);
	const TInt want = iExpect > 0 ? iExpect : s.iSamples;
	const TInt fps100 = wall ? s.iDelivered * 100000 / (TInt)wall : 0;
	TBool pass = aError == KErrNone && r == KErrNone && s.iDelivered == want && s.iError == KErrNone;
	if (!iBench)
		pass = pass && (TInt)wall * 100 <= s.iDurationMs * 110 && (TInt)wall * 100 >= s.iDurationMs * 90;
	// audio (real-time pass of a clip with an AAC track): it must have played, about as much PCM as the clip is long, the
	// DevSound position must have reached it, and the pictures must not trail the audio clock by more than 80 ms on average
	TBuf8<160> au;
	if (iBench || !s.iHasAudio)
		au.Copy(_L8("none"));
	else
		{
		const TInt pcmMs = s.iAudioRate ? (TInt)((TInt64)s.iAudioPcmFrames * 1000 / s.iAudioRate) : 0;
		const TBool aok = s.iAudioOn && s.iAudioErr == KErrNone && pcmMs * 100 >= s.iDurationMs * 90 && pcmMs * 100 <= s.iDurationMs * 115 &&
			s.iAudioPlayedMs * 100 >= pcmMs * 85 && s.iDriftAvgMs <= 80;
		au.Format(_L8("%d Hz x%d %d frames played %d ms (%d buffers, clock %d) drift avg %d max %d ms err %d %s"), s.iAudioRate, s.iAudioCh,
			s.iAudioPcmFrames, s.iAudioPlayedMs, s.iAudioBuffers, s.iAudioClock, s.iDriftAvgMs, s.iDriftMaxMs, s.iAudioErr,
			aok ? "audio-PASS" : "audio-FAIL");
		pass = pass && aok;
		}
	au.ZeroTerminate();
	RDebug::Printf("PH: vid %s %s: %dx%d %d/%d pictures (fed %d), drawn %d dropped %d no-region %d, wall %u ms (nominal %d), %d.%02d fps,"
		" late avg %d max %d ms, audio track %d, audio: %s, play err %d %s", iTag.Ptr(), iBench ? "bench" : "real", s.iFrameW, s.iFrameH,
		s.iDelivered, want, s.iFed, s.iDrawn, s.iDropped, s.iNoRegion, wall, s.iDurationMs, fps100 / 100, fps100 % 100,
		s.iLateAvgMs, s.iLateMaxMs, s.iHasAudio, au.Ptr(), aError, pass ? "PASS" : "FAIL");
	iPass = pass;
	iResult = aError;
	CActiveScheduler::Stop();
	}

static TInt TimeoutCb(TAny*)
	{
	RDebug::Printf("PH: vid TIMEOUT");
	CActiveScheduler::Stop();
	return 0;
	}

static TBool RunOne(RWsSession& aWs, CWsScreenDevice& aScreen, RWindow& aWin, const TDesC& aFile, const TDesC8& aTag, TBool aBench, TInt aExpect)
	{
	CPlay* p = new CPlay(aWs, aScreen, aWin, aFile, aTag, aBench, aExpect);
	if (!p)
		return EFalse;
	TBool pass = EFalse;
	TRAPD(r, p->StartL());
	if (r != KErrNone)
		RDebug::Printf("PH: vid %s start leave %d", aTag.Ptr(), r);
	else
		{
		CPeriodic* wd = CPeriodic::NewL(CActive::EPriorityLow);
		wd->Start(120 * 1000000, 120 * 1000000, TCallBack(TimeoutCb, NULL));
		CActiveScheduler::Start();
		delete wd;
		pass = p->iPass;
		}
	delete p;							// (CVideoPlayerUtility closes the controller)
	return pass;
	}

static void RunAllL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CleanupClosePushL(ws);
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	CleanupStack::PushL(screen);
	User::LeaveIfError(screen->Construct());
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x56494431, ETrue));
	group.SetOrdinalPosition(0, 20000);			// above the keyguard (lockapp = ECoeWinPriorityAlwaysAtFront+1) and the cap-server backdrop (10000)
	CleanupClosePushL(group);
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x56494432));
	CleanupClosePushL(win);
	win.SetExtent(TPoint(0, 0), screen->SizeInPixels());
	win.SetRequiredDisplayMode(EColor16MU);
	win.SetBackgroundColor(KRgbBlack);
	win.Activate();
	ws.Flush();
	RDebug::Printf("PH: vid window up %dx%d", screen->SizeInPixels().iWidth, screen->SizeInPixels().iHeight);

	TFileName arg;
	User::CommandLine(arg);
	arg.Trim();
	TInt pass = 0, total = 0;
	if (arg.Length())
		{
		TInt expect = 0;
		const TInt sp = arg.Locate(' ');
		TFileName file = arg;
		if (sp > 0)
			{
			file = arg.Left(sp);
			TLex lex(arg.Mid(sp + 1));
			lex.Val(expect);
			}
		for (TInt bench = 0; bench < 2; ++bench, ++total)
			pass += RunOne(ws, *screen, win, file, bench ? _L8("file bench") : _L8("file real"), bench, expect);
		}
	else
		{
		static const TText* const names[] = { (const TText*)L"vmain", (const TText*)L"vbase" };
		const TInt frames[] = { 100, 75 };
		for (TInt i = 0; i < 2; ++i)
			for (TInt bench = 0; bench < 2; ++bench, ++total)
				{
				TFileName f;
				f.Format(_L("z:\\resource\\phevde\\%S.mp4"), &TPtrC((const TUint16*)names[i]));
				TBuf8<24> tag;
				tag.Copy(TPtrC((const TUint16*)names[i]));
				tag.Append(bench ? _L8(" bench") : _L8(" real"));
				pass += RunOne(ws, *screen, win, f, tag, bench, frames[i]);
				}
		}
	RDebug::Printf("PH: vid done %d/%d %s", pass, total, pass == total ? "PASS" : "FAIL");
	CleanupStack::PopAndDestroy(5, sched);
	}

// raw CPU speed of the guest (x86 board, 2026-10-16: the software decoder ran ~10x slower than a native FFmpeg should): integer loop, memcpy, float loop
static void CpuBench()
	{
	TTime w0, w1;
	w0.HomeTime();
	const TUint32 t0 = User::FastCounter();
	volatile TUint32 acc = 1;
	for (TUint32 i = 0; i < 100000000u; ++i)
		acc = acc * 1664525u + 1013904223u + i;
	const TUint32 t1 = User::FastCounter();
	w1.HomeTime();
	TUint8* a = (TUint8*)User::Alloc(4 << 20);
	TUint8* b = (TUint8*)User::Alloc(4 << 20);
	TUint32 t2 = t1, t3 = t1;
	if (a && b)
		{
		Mem::Fill(a, 4 << 20, 0x5A);
		t2 = User::FastCounter();
		for (TInt i = 0; i < 100; ++i)
			Mem::Copy(b, a, 4 << 20);			// 400 MB
		t3 = User::FastCounter();
		}
	volatile float fl = 1.0f;
	for (TUint32 i = 0; i < 20000000u; ++i)
		fl = fl * 1.0000001f + 0.5f;
	const TUint32 t4 = User::FastCounter();
	User::Free(a);
	User::Free(b);
	RDebug::Printf("PH: cpu bench: FastCounter ticks (the int loop took %d ms wall): 1e8 int mul-add %u ticks; memcpy 400 MB %u ticks (fill 4 MB %u); 2e7 float mul-add %u ticks", (TInt)((w1.Int64() - w0.Int64()) / 1000),
		t1 - t0, t3 - t2, t2 - t1, t4 - t3);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	CpuBench();
	RDebug::Printf("PH: vid start");
	TRAPD(r, RunAllL());
	if (r != KErrNone)
		RDebug::Printf("PH: vid leave %d", r);
	delete cleanup;
	return 0;
	}
