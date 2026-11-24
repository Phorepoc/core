// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// playtest.cpp -- OURS (audio U4): plays an audio FILE through the real MMF client
// (CMdaAudioPlayerUtility -> the controller the MMF picks for the file: WAV = mmfaudiocontroller, MP3 = the
// advanced audio controller -> DevSound -> A3F -> the HwDevice for the file's FourCC -> ... -> the AC'97 PDD).
//
//   playtest <file>
//   playtest               each default clip that exists, one after the other, 10 s after the start (a boot test:
//                          SSM_TEST=playtest; a clip played across SSM's switch to Normal ends -2 part-way):
//                          c:\data\test.wav, z:\data\playtest.wav (PCM), z:\data\playtest.mp3
//
// Each callback is printed; `[playtest] DONE <err>` ends each clip, `[playtest] ALL <err>` the run. With QEMU's SOUND_WAV=1 the host checks the capture:
// py tools/phorepoc-x86/wavcheck.py --expect <Hz of the file>
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <mdaaudiosampleplayer.h>

class CPlayTest : public CBase, public MMdaAudioPlayerCallback
	{
public:
	~CPlayTest()	{ delete iPlayer; }
	void StartL(const TDesC& aFile)
		{
		RDebug::Print(_L("[playtest] NewFilePlayerL(%S)"), &aFile);
		iPlayer = CMdaAudioPlayerUtility::NewFilePlayerL(aFile, *this);
		}
	TInt iResult;
private:
	void Done(TInt aErr)
		{
		iResult = aErr;
		RDebug::Printf("[playtest] DONE %d", aErr);
		CActiveScheduler::Stop();
		}
	// MMdaAudioPlayerCallback
	void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration)
		{
		RDebug::Printf("[playtest] MapcInitComplete %d, duration %d ms", aError, I64INT(aDuration.Int64() / 1000));
		if (aError != KErrNone)
			{
			Done(aError);
			return;
			}
		iPlayer->SetVolume(iPlayer->MaxVolume());
		iPlayer->Play();
		RDebug::Printf("[playtest] Play()");
		}
	void MapcPlayComplete(TInt aError)
		{
		RDebug::Printf("[playtest] MapcPlayComplete %d", aError);
		Done(aError);
		}
private:
	CMdaAudioPlayerUtility* iPlayer;
	};

static TInt PlayL(const TDesC& aFile)
	{
	CPlayTest* t = new (ELeave) CPlayTest;
	CleanupStack::PushL(t);
	t->StartL(aFile);
	CActiveScheduler::Start();
	TInt r = t->iResult;
	CleanupStack::PopAndDestroy(t);
	return r;
	}

static TInt RunL()
	{
	TFileName file;
	User::CommandLine(file);
	file.Trim();
	CActiveScheduler* s = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(s);
	CActiveScheduler::Install(s);
	TInt r = KErrNotFound;
	if (file.Length())
		r = PlayL(file);
	else
		{
		User::After(10000000);
		RFs fs;
		User::LeaveIfError(fs.Connect());
		CleanupClosePushL(fs);
		_LIT(KClip0, "c:\\data\\test.wav");
		_LIT(KClip1, "z:\\data\\playtest.wav");
		_LIT(KClip2, "z:\\data\\playtest.mp3");
		const TDesC* clips[] = { &KClip0(), &KClip1(), &KClip2() };
		TEntry e;
		for (TInt i = 0; i < 3 && (r == KErrNone || r == KErrNotFound); ++i)
			if (fs.Entry(*clips[i], e) == KErrNone)
				r = PlayL(*clips[i]);
		CleanupStack::PopAndDestroy(&fs);
		}
	RDebug::Printf("[playtest] ALL %d", r);
	CleanupStack::PopAndDestroy(s);
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
			RDebug::Printf("[playtest] left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}
