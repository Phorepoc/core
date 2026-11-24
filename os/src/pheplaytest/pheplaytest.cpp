// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheplaytest.cpp -- OURS: the playback regression test. A real CMdaAudioPlayerUtility opens a file (the MMF picks the
// controller plugin by the file's format: the MP3 controller for .mp3) and plays it to the end:
//
//   pheplaytest [file]        default: each of z:\data\pheplaytest.wav (PCM) and z:\data\pheplaytest.mp3 the ROM has
//                             (2 s 440 Hz tones), one after the other
//
// Each callback is printed with the clip's duration; `[pheplaytest] DONE <err>` ends each clip, `[pheplaytest] ALL <err>`
// the run (tonetest checks the device chain under it; this checks the controller/codec layer).
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <mdaaudiosampleplayer.h>

_LIT(KDefaultWav, "z:\\data\\pheplaytest.wav");
_LIT(KDefaultMp3, "z:\\data\\pheplaytest.mp3");

class CPlayTest : public CBase, public MMdaAudioPlayerCallback
	{
public:
	~CPlayTest()			{ delete iPlayer; }
	void StartL(const TDesC& aFile)
		{
		RDebug::Print(_L("[pheplaytest] NewFilePlayerL %S"), &aFile);
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
	// started by SSM's noncritical list: a clip played across the switch to Normal ends KErrGeneral (-2) part-way, so
	// the run starts 10 s later
	User::After(10000000);
	if (file.Length())
		r = PlayL(file);
	else
		{
		RFs fs;
		User::LeaveIfError(fs.Connect());
		CleanupClosePushL(fs);
		const TDesC* defaults[] = { &KDefaultWav(), &KDefaultMp3() };
		TEntry e;
		for (TInt i = 0; i < 2 && (r == KErrNone || r == KErrNotFound); ++i)
			if (fs.Entry(*defaults[i], e) == KErrNone)
				r = PlayL(*defaults[i]);
		CleanupStack::PopAndDestroy(&fs);
		}
	RDebug::Printf("[pheplaytest] ALL %d", r);
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
			RDebug::Printf("[pheplaytest] left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}
