// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// tonetest.cpp -- OURS (X4.89): the audio chain's regression test. A real CMMFDevSound (A3F: DevSound client ->
// the audio server -> the reference adaptation -> mdasoundadapter -> RSoundSc -> the AC'97 PDD) plays a tone:
//
//   tonetest [Hz] [ms]        default 440 Hz, 1000 ms
//
// Each observer callback is printed; `[tonetest] DONE <err>` ends it. With QEMU's SOUND_WAV=1 the host checks the
// result: py tools/phorepoc-x86/wavcheck.py --expect 440
#include <e32base.h>
#include <e32debug.h>
#include <mmf/server/sounddevice.h>

class CToneTest : public CBase, public MDevSoundObserver
	{
public:
	CToneTest(TInt aHz, TInt aMs) : iHz(aHz), iMs(aMs) {}
	~CToneTest()			{ delete iDevSound; }
	void StartL()
		{
		iDevSound = CMMFDevSound::NewL();
		RDebug::Printf("[tonetest] DevSound created; InitializeL(tone)");
		iDevSound->InitializeL(*this, EMMFStateTonePlaying);
		}
	TInt iResult;
private:
	void Done(TInt aErr)
		{
		iResult = aErr;
		RDebug::Printf("[tonetest] DONE %d", aErr);
		CActiveScheduler::Stop();
		}
	// MDevSoundObserver
	void InitializeComplete(TInt aError)
		{
		RDebug::Printf("[tonetest] InitializeComplete %d", aError);
		if (aError != KErrNone)
			{
			Done(aError);
			return;
			}
		iDevSound->SetVolume(iDevSound->MaxVolume());
		TRAPD(r, iDevSound->PlayToneL(iHz, TTimeIntervalMicroSeconds(TInt64(iMs) * 1000)));
		RDebug::Printf("[tonetest] PlayToneL(%d Hz, %d ms) -> %d", iHz, iMs, r);
		if (r != KErrNone)
			Done(r);
		}
	void ToneFinished(TInt aError)			{ RDebug::Printf("[tonetest] ToneFinished %d", aError); Done(aError == KErrUnderflow ? KErrNone : aError); }
	void BufferToBeFilled(CMMFBuffer*)		{}
	void PlayError(TInt aError)				{ RDebug::Printf("[tonetest] PlayError %d", aError); Done(aError); }
	void BufferToBeEmptied(CMMFBuffer*)		{}
	void RecordError(TInt)					{}
	void ConvertError(TInt)					{}
	void DeviceMessage(TUid, const TDesC8&)	{}
private:
	TInt iHz, iMs;
	CMMFDevSound* iDevSound;
	};

static TInt RunL()
	{
	TBuf<32> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	TInt hz = 440, ms = 1000;
	lex.SkipSpace();
	if (!lex.Eos())
		lex.Val(hz);
	lex.SkipSpace();
	if (!lex.Eos())
		lex.Val(ms);
	CActiveScheduler* s = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(s);
	CActiveScheduler::Install(s);
	CToneTest* t = new (ELeave) CToneTest(hz, ms);
	CleanupStack::PushL(t);
	t->StartL();
	CActiveScheduler::Start();
	TInt r = t->iResult;
	CleanupStack::PopAndDestroy(2, s);
	return r;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TInt r = KErrNoMemory;
	if (c)
		{
		TRAPD(leave, r = RunL());
		if (leave != KErrNone)
			{
			RDebug::Printf("[tonetest] left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}
