// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// playtest.cpp -- OURS (audio U4): plays an audio FILE through the real MMF client
// (CMdaAudioPlayerUtility -> the controller the MMF picks for the file: WAV = mmfaudiocontroller, MP3 = the
// advanced audio controller -> DevSound -> A3F -> the HwDevice for the file's FourCC -> ... -> the AC'97 PDD).
//
//   playtest <file>        default c:\data\test.wav
//
// Each callback is printed; `[playtest] DONE <err>` ends it. With QEMU's SOUND_WAV=1 the host checks the capture:
// py tools/phorepoc-x86/wavcheck.py --expect <Hz of the file>
#include <e32base.h>
#include <e32debug.h>
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

static TInt RunL()
	{
	TFileName file;
	User::CommandLine(file);
	file.Trim();
	if (file.Length() == 0)
		file = _L("c:\\data\\test.wav");
	CActiveScheduler* s = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(s);
	CActiveScheduler::Install(s);
	CPlayTest* t = new (ELeave) CPlayTest;
	CleanupStack::PushL(t);
	t->StartL(file);
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
			RDebug::Printf("[playtest] left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}
