// mixtest.cpp -- OURS (audio A0/Q0, docs/phorepoc/audio-mixing-plan.md 1.3): TWO audio clients at once.
//
//   thread "mixmusic": CMdaAudioOutputStream, a 440 Hz sine in 4 KB buffers for ~6 s     (the "music" client)
//   thread "mixfx"   : CMdaAudioPlayerUtility on a 880 Hz WAV, 0.4 s long, replayed once a second, 5 times (the "effects")
//
// Both end up in the A3F audio server's RMdaDevSound -> SoundSc. On the stock chain the exclusive SoundSc channel makes
// the second one fail or corrupt the first (the symptom A0 reproduces); with the Q0 mixer both are heard together.
// Every callback is printed ("[mixtest] ..."); the last line is `[mixtest] DONE music=<err> fx=<err>`.
// With QEMU's SOUND_WAV=1 the host checks the capture:
//   py tools/phorepoc-x86/wavcheck.py --expect-all 440 880
//   mixtest [fx.wav]       default: it writes c:\data\mixfx.wav itself (880 Hz, 22050 Hz mono 16-bit)
#include <e32base.h>
#include <e32debug.h>
#include <e32math.h>
#include <f32file.h>
#include <mda/common/audio.h>
#include <mdaaudiooutputstream.h>
#include <mdaaudiosampleplayer.h>

const TInt KMusicRate = 44100;			// mono 16-bit
const TInt KMusicBufBytes = 4096;
const TInt KMusicBuffers = 130;			// ~6 s
const TInt KQueue = 3;					// buffers in flight
const TInt KFxRepeats = 5;
const TInt KFxGapUs = 1000000;
_LIT(KFxFile, "c:\\data\\mixfx.wav");

// --- the music client ---------------------------------------------------------------------------------------------
class CMusic : public CBase, public MMdaAudioOutputStreamCallback
	{
public:
	~CMusic()	{ delete iStream; }
	void StartL()
		{
		iStream = CMdaAudioOutputStream::NewL(*this);
		iSettings.iSampleRate = TMdaAudioDataSettings::ESampleRate44100Hz;
		iSettings.iChannels = TMdaAudioDataSettings::EChannelsMono;
		RDebug::Printf("[mixtest] music: Open");
		iStream->Open(&iSettings);
		}
	TInt iResult;
private:
	void Fill(TDes8& aBuf)
		{
		aBuf.SetLength(0);
		const TReal KTwoPi = 6.283185307179586;
		for (TInt i = 0; i < KMusicBufBytes / 2; ++i)
			{
			TReal s;
			Math::Sin(s, KTwoPi * 440.0 * (TReal)iSample / KMusicRate);
			++iSample;
			TInt16 v = (TInt16)(12000.0 * s);
			aBuf.Append((const TUint8*)&v, 2);
			}
		}
	void Done(TInt aErr)
		{
		iResult = aErr;
		RDebug::Printf("[mixtest] music: DONE %d (%d buffers written)", aErr, iWritten);
		CActiveScheduler::Stop();
		}
	void MaoscOpenComplete(TInt aError)
		{
		RDebug::Printf("[mixtest] music: MaoscOpenComplete %d", aError);
		if (aError != KErrNone)
			{
			Done(aError);
			return;
			}
		iStream->SetVolume(iStream->MaxVolume());
		for (TInt i = 0; i < KQueue; ++i)
			WriteNext(iBuf[i]);
		}
	void WriteNext(TDes8& aBuf)
		{
		Fill(aBuf);
		TRAPD(r, iStream->WriteL(aBuf));
		if (r != KErrNone)
			Done(r);
		else
			++iWritten;
		}
	void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer)
		{
		if (aError != KErrNone)
			{
			RDebug::Printf("[mixtest] music: MaoscBufferCopied %d after %d", aError, iCopied);
			if (aError != KErrAbort)
				Done(aError);
			return;
			}
		if (++iCopied % 20 == 0)
			RDebug::Printf("[mixtest] music: %d buffers copied", iCopied);
		if (iWritten < KMusicBuffers)
			{
			for (TInt i = 0; i < KQueue; ++i)
				if (&aBuffer == &iBuf[i])
					WriteNext(iBuf[i]);
			}
		else if (iCopied >= KMusicBuffers)
			Done(KErrNone);
		}
	void MaoscPlayComplete(TInt aError)
		{
		RDebug::Printf("[mixtest] music: MaoscPlayComplete %d", aError);
		if (iCopied >= KMusicBuffers)
			Done(KErrNone);
		else if (aError != KErrNone && aError != KErrUnderflow)
			Done(aError);
		}
private:
	CMdaAudioOutputStream* iStream;
	TMdaAudioDataSettings iSettings;
	TBuf8<KMusicBufBytes> iBuf[KQueue];
	TInt iSample;
	TInt iWritten;
	TInt iCopied;
	};

// --- the effects client ---------------------------------------------------------------------------------------------
class CEffects : public CBase, public MMdaAudioPlayerCallback
	{
public:
	~CEffects()	{ delete iPlayer; delete iTimer; }
	void StartL(const TDesC& aFile)
		{
		iTimer = CPeriodic::NewL(CActive::EPriorityStandard);
		RDebug::Print(_L("[mixtest] fx: NewFilePlayerL(%S)"), &aFile);
		iPlayer = CMdaAudioPlayerUtility::NewFilePlayerL(aFile, *this);
		}
	TInt iResult;
private:
	void Done(TInt aErr)
		{
		iResult = aErr;
		RDebug::Printf("[mixtest] fx: DONE %d (%d plays)", aErr, iPlays);
		CActiveScheduler::Stop();
		}
	static TInt Replay(TAny* aThis)
		{
		CEffects* t = (CEffects*)aThis;
		t->iTimer->Cancel();
		t->iPlayer->Play();
		RDebug::Printf("[mixtest] fx: Play() #%d", t->iPlays + 1);
		return 0;
		}
	void MapcInitComplete(TInt aError, const TTimeIntervalMicroSeconds& aDuration)
		{
		RDebug::Printf("[mixtest] fx: MapcInitComplete %d, duration %d ms", aError, I64INT(aDuration.Int64() / 1000));
		if (aError != KErrNone)
			{
			Done(aError);
			return;
			}
		iPlayer->SetVolume(iPlayer->MaxVolume());
		// start the first play after 1.5 s so the music is already running when the effect arrives
		iTimer->Start(1500000, 1500000, TCallBack(Replay, this));
		}
	void MapcPlayComplete(TInt aError)
		{
		RDebug::Printf("[mixtest] fx: MapcPlayComplete %d", aError);
		if (aError != KErrNone)
			{
			Done(aError);
			return;
			}
		if (++iPlays >= KFxRepeats)
			Done(KErrNone);
		else
			iTimer->Start(KFxGapUs, KFxGapUs, TCallBack(Replay, this));
		}
private:
	CMdaAudioPlayerUtility* iPlayer;
	CPeriodic* iTimer;
	TInt iPlays;
	};

// --- threads --------------------------------------------------------------------------------------------------------
static TInt MusicThread(TAny*)
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TInt r = KErrNoMemory;
	if (c)
		{
		TRAPD(leave, {
			CActiveScheduler* s = new (ELeave) CActiveScheduler;
			CleanupStack::PushL(s);
			CActiveScheduler::Install(s);
			CMusic* m = new (ELeave) CMusic;
			CleanupStack::PushL(m);
			m->StartL();
			CActiveScheduler::Start();
			r = m->iResult;
			CleanupStack::PopAndDestroy(2, s);
			});
		if (leave != KErrNone)
			{
			RDebug::Printf("[mixtest] music left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}

static TInt EffectsThread(TAny* aFile)
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TInt r = KErrNoMemory;
	if (c)
		{
		TRAPD(leave, {
			CActiveScheduler* s = new (ELeave) CActiveScheduler;
			CleanupStack::PushL(s);
			CActiveScheduler::Install(s);
			CEffects* e = new (ELeave) CEffects;
			CleanupStack::PushL(e);
			e->StartL(*(const TDesC*)aFile);
			CActiveScheduler::Start();
			r = e->iResult;
			CleanupStack::PopAndDestroy(2, s);
			});
		if (leave != KErrNone)
			{
			RDebug::Printf("[mixtest] fx left %d", leave);
			r = leave;
			}
		delete c;
		}
	return r;
	}

// --- the effect's WAV: 880 Hz, 22050 Hz mono 16-bit, 0.4 s ----------------------------------------------------------------
static void Put32(TDes8& aD, TUint32 aV)	{ for (TInt i = 0; i < 4; ++i) aD.Append((TUint8)(aV >> (8 * i))); }
static void Put16(TDes8& aD, TUint16 aV)	{ aD.Append((TUint8)aV); aD.Append((TUint8)(aV >> 8)); }

static TInt WriteFxWav(const TDesC& aName)
	{
	const TInt KRate = 22050, KSamples = KRate * 4 / 10;
	RFs fs;
	TInt r = fs.Connect();
	if (r != KErrNone)
		return r;
	fs.MkDirAll(aName);
	RFile f;
	r = f.Replace(fs, aName, EFileWrite);
	if (r == KErrNone)
		{
		HBufC8* b = HBufC8::New(44 + KSamples * 2);
		if (!b)
			r = KErrNoMemory;
		else
			{
			TPtr8 d = b->Des();
			d.Append(_L8("RIFF"));
			Put32(d, 36 + KSamples * 2);
			d.Append(_L8("WAVEfmt "));
			Put32(d, 16);
			Put16(d, 1);					// PCM
			Put16(d, 1);					// mono
			Put32(d, KRate);
			Put32(d, KRate * 2);
			Put16(d, 2);
			Put16(d, 16);
			d.Append(_L8("data"));
			Put32(d, KSamples * 2);
			const TReal KTwoPi = 6.283185307179586;
			for (TInt i = 0; i < KSamples; ++i)
				{
				TReal s;
				Math::Sin(s, KTwoPi * 880.0 * (TReal)i / KRate);
				Put16(d, (TUint16)(TInt16)(12000.0 * s));
				}
			r = f.Write(d);
			delete b;
			}
		f.Close();
		}
	fs.Close();
	return r;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* c = CTrapCleanup::New();
	TFileName file;
	User::CommandLine(file);
	file.Trim();
	if (file.Length() == 0)
		{
		file = KFxFile;
		TInt w = WriteFxWav(file);
		RDebug::Printf("[mixtest] wrote the effect wav: %d", w);
		if (w != KErrNone)
			return w;
		}
	RThread music, fx;
	TRequestStatus ms, fs;
	TInt r = music.Create(_L("mixmusic"), MusicThread, 0x14000, 0x20000, 0x200000, NULL);
	if (r == KErrNone)
		r = fx.Create(_L("mixfx"), EffectsThread, 0x14000, 0x20000, 0x200000, &file);
	if (r != KErrNone)
		{
		RDebug::Printf("[mixtest] thread create failed %d", r);
		return r;
		}
	music.Logon(ms);
	fx.Logon(fs);
	music.Resume();
	fx.Resume();
	User::WaitForRequest(ms);
	User::WaitForRequest(fs);
	TInt mr = music.ExitReason(), fr = fx.ExitReason();
	RDebug::Printf("[mixtest] DONE music=%d fx=%d", mr, fr);
	music.Close();
	fx.Close();
	delete c;
	return mr != KErrNone ? mr : fr;
	}
