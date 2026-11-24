// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheplaytest.cpp -- OURS: the playback regression test. A real CMdaAudioPlayerUtility opens a file (the MMF picks the
// controller plugin by the file's format: the MP3 controller for .mp3) and plays it to the end:
//
//   pheplaytest [file]        default z:\data\pheplaytest.wav (a 2 s 440 Hz tone, 16 kHz mono PCM)
//
// Each callback is printed with the clip's duration; `[pheplaytest] DONE <err>` ends it (tonetest checks the device
// chain under it; this checks the controller/codec layer).
#include <e32base.h>
#include <e32debug.h>
#include <mdaaudiosampleplayer.h>

_LIT(KDefaultFile, "z:\\data\\pheplaytest.wav");

class CPlayTest : public CBase, public MMdaAudioPlayerCallback
	{
public:
	~CPlayTest()			{ delete iPlayer; }
	void StartL(const TDesC& aFile)
		{
		RDebug::Printf("[pheplaytest] NewFilePlayerL");
		iPlayer = CMdaAudioPlayerUtility::NewFilePlayerL(aFile, *this);
		}
	void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration)
		{
		RDebug::Printf("[pheplaytest] InitComplete %d duration %d ms", aError, (TInt)(aDuration.Int64() / 1000));
		if (aError != KErrNone)
			{
			Done(aError);
			return;
			}
		iStart.UniversalTime();
		iPlayer->Play();
		}
	void MapcPlayComplete(TInt aError)
		{
		TTime now;
		now.UniversalTime();
		RDebug::Printf("[pheplaytest] PlayComplete %d after %d ms", aError,
			(TInt)(now.MicroSecondsFrom(iStart).Int64() / 1000));
		Done(aError);
		}
	TInt iResult;
private:
	void Done(TInt aErr)
		{
		iResult = aErr;
		RDebug::Printf("[pheplaytest] DONE %d", aErr);
		CActiveScheduler::Stop();
		}
	CMdaAudioPlayerUtility* iPlayer;
	TTime iStart;
	};

static TInt RunL()
	{
	TFileName file;
	User::CommandLine(file);
	file.Trim();
	if (!file.Length())
		file = KDefaultFile;
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
			RDebug::Printf("[pheplaytest] left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}
