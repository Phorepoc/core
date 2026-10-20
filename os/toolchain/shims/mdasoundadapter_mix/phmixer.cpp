// phmixer.cpp -- OURS (audio Q0, docs/phorepoc/audio-mixing-plan.md section 2): the userland mixer behind RMixSoundSc.
//
// Every RMdaDevSound::Open in the audio server process opens a virtual TX channel here (ph_mixsc.h swaps RSoundSc for
// RMixSoundSc in the unmodified 2010 mdasoundadapter). A mixer thread sums all channels into 48 kHz stereo S16 and plays
// the sum on the ONE real SoundSc TX channel (the kernel LDD allows one channel per unit). The arithmetic (resample,
// gain, sum, clip) is phmixer_core.h, unit-tested on the host (tests/user/audiomix).
//
//   client thread:  PlayData  -> push {ptr,len,status} on the channel ring, wake the mixer
//   mixer thread:   per 10 ms quantum (480 frames = one A64 PDD period) mix every unpaused channel, write the quantum
//                   into a free slot of the real sink's shared chunk, RSoundSc::PlayData; keep up to 3 slots queued.
//                   A request completes (KErrNone) when its last frame has been consumed.
//   sink lifecycle: opened when the first channel has data, closed 250 ms after the last data (the PDD powers down).
//   master volume:  the real channel's hardware volume = P&S KPhAudioMasterVolume (ph_audio_props.h, written by phvolume);
//                   per-stream volume (RMixSoundSc::SetVolume) is a software gain in the mix.
//   record unit:    RX unit 4 is a pass-through to a REAL RSoundSc opened lazily on the first record call.
// Define PHMIX_TRACE for RDebug prints of the lifecycle (the audio A0 measurement).
#include "ph_mixsc.h"
#undef RSoundSc				// below: RSoundSc = the REAL kernel-driver channel, RMixSoundSc = the virtual one

#include <e32std.h>
#include <e32base.h>
#include <e32property.h>
#include <e32debug.h>
#include "phmixer_core.h"
#include "ph_audio_props.h"

#ifdef PHMIX_TRACE
#define PHMIX_LOG(...) RDebug::Printf(__VA_ARGS__)
#else
#define PHMIX_LOG(...) do {} while (0)
#endif

namespace
{
const TInt KQuantum = 480;						// output frames per mix cycle (10 ms @ 48 kHz)
const TInt KSinkBytes = KQuantum * 4;			// 1920 B = one stereo S16 quantum = one A64 PDD period
const TInt KSinkBuffers = 4;					// buffers in the real sink's shared chunk
const TInt KSinkSlots = 3;						// quanta queued on the real sink at once (<= KSinkBuffers)
const TInt KMaxChannels = 8;					// concurrent virtual TX channels
const TInt KMaxRequests = 8;					// pending PlayData requests per channel (the adapter queues 8)
const TInt KIdleCloseUs = 250000;				// close the real sink this long after the last data
const TInt KMixStack = 0x4000;

const TInt KRateHz[] = { 7350, 8000, 8820, 9600, 11025, 12000, 14700, 16000, 22050, 24000, 29400, 32000, 44100, 48000 };
const TInt KNumRates = sizeof(KRateHz) / sizeof(KRateHz[0]);

// the buffer-configuration layout the adapter packs (TPlaySharedChunkBufConfig = base + 8 offsets)
struct TMixBufCfg
	{
	TSharedChunkBufConfigBase iBase;
	TInt iOffsets[8];
	};

struct TReq
	{
	const TInt16* iPtr;
	TInt iFrames;
	TInt iPos;					// frames consumed
	TRequestStatus* iStatus;
	};

struct TChannel
	{
	TBool iUsed;
	RThread iOwner;				// the thread whose request statuses we complete (process-owned handle)
	TInt iRate;
	TInt iChannels;
	TInt iGainCode;
	PhMix::S32 iGainQ14;
	TBool iPaused;
	TUint8* iBase;				// the channel's (local) buffer chunk
	TMixBufCfg iCfg;
	TInt iCfgLen;
	TInt iHead;
	TInt iCount;
	TReq iReq[KMaxRequests];
	PhMix::TStream iStream;
	TInt iBytesPlayed;
	};

class CPhMixer : public CBase
	{
public:
	static CPhMixer* NewL();
	~CPhMixer();
	TInt AddChannel(TChannel*& aCh);
	void RemoveChannel(TChannel* aCh);
	void Wake()							{ iThread.RequestSignal(); }
	TInt ProbeSink();
	RMutex iLock;
	TChannel iCh[KMaxChannels];
private:
	CPhMixer() {}
	TInt Construct();
	static TInt ThreadEntry(TAny* aPtr);
	void Run();
	void Service();
	void ReadMasterVolume();
	void ReapDead();
	TBool AnyData();
	TInt OpenSink();
	void CloseSink();
	void FailAll(TInt aErr);
	TBool MixQuantum(TInt aSlot);
	void CompleteRequest(TChannel& aCh, TInt aErr);
private:
	RThread iThread;
	RSoundSc iSink;
	RChunk iSinkChunk;
	TMixBufCfg iSinkCfg;
	TBool iSinkOpen;
	TBool iSinkEverOk;
	TRequestStatus iSlot[KSinkSlots];
	TBool iSlotBusy[KSinkSlots];
	RTimer iTimer;
	TRequestStatus iTimerStatus;
	TBool iTimerArmed;
	RProperty iVolProp;
	TRequestStatus iVolStatus;
	TBool iVolSubscribed;
	TInt iMasterCode;
	TBool iVolDirty;
	PhMix::S32 iAcc[KQuantum * 2];
	};

CPhMixer* gMixer;				// WSD: one mixer per process (mdasoundadapter.dll is built allow_wsd)
TInt gInitTicket;
TInt gInitDone;

CPhMixer* Mixer()
	{
	if (gInitDone)
		return gMixer;
	if (User::LockedInc(gInitTicket) == 0)
		{
		gMixer = CPhMixer::NewL();		// NULL on failure: the adapter's Open then reports KErrNoMemory
		gInitDone = 1;
		}
	else
		{
		while (!gInitDone)
			User::After(5000);
		}
	return gMixer;
	}

CPhMixer* CPhMixer::NewL()
	{
	CPhMixer* m = new CPhMixer;
	if (!m)
		return NULL;
	if (m->Construct() != KErrNone)
		{
		delete m;
		return NULL;
		}
	return m;
	}

TInt CPhMixer::Construct()
	{
	iMasterCode = PhVolStepToCode(KPhVolDefaultStep);
	TInt r = iLock.CreateLocal();
	if (r != KErrNone)
		return r;
	for (TInt i = 0; i < KMaxChannels; ++i)
		iCh[i].iUsed = EFalse;
	r = iThread.Create(_L("PhMixer"), ThreadEntry, KMixStack, &User::Allocator(), this);
	if (r != KErrNone)
		return r;
	iThread.SetPriority(EPriorityAbsoluteHigh);		// = the DevSound server threads, above every client AO
	iThread.Resume();
	PHMIX_LOG("[phmix] mixer thread started");
	return KErrNone;
	}

CPhMixer::~CPhMixer()
	{
	// the singleton lives for the process; this only runs on a failed Construct
	iLock.Close();
	iThread.Close();
	}

TInt CPhMixer::ThreadEntry(TAny* aPtr)
	{
	((CPhMixer*)aPtr)->Run();
	return KErrNone;
	}

// --- client side (any thread) ----------------------------------------------------------------------------------------
TInt CPhMixer::AddChannel(TChannel*& aCh)
	{
	iLock.Wait();
	TChannel* ch = NULL;
	for (TInt i = 0; i < KMaxChannels; ++i)
		if (!iCh[i].iUsed)
			{
			ch = &iCh[i];
			break;
			}
	if (!ch)
		{
		iLock.Signal();
		return KErrInUse;
		}
	RThread me;
	TInt r = ch->iOwner.Open(me.Id());
	if (r != KErrNone)
		{
		iLock.Signal();
		return r;
	}
	ch->iUsed = ETrue;
	ch->iRate = 8000;
	ch->iChannels = 1;
	ch->iGainCode = 255;
	ch->iGainQ14 = PhMix::KUnityQ14;
	ch->iPaused = EFalse;
	ch->iBase = NULL;
	ch->iCfgLen = 0;
	ch->iHead = ch->iCount = 0;
	ch->iBytesPlayed = 0;
	PhMix::Reset(ch->iStream);
	aCh = ch;
	iLock.Signal();
	PHMIX_LOG("[phmix] channel open");
	return KErrNone;
	}

// complete every pending request of a channel with aErr (lock held)
void CPhMixer::CompleteRequest(TChannel& aCh, TInt aErr)
	{
	while (aCh.iCount > 0)
		{
		TRequestStatus* st = aCh.iReq[aCh.iHead].iStatus;
		aCh.iHead = (aCh.iHead + 1) % KMaxRequests;
		--aCh.iCount;
		aCh.iOwner.RequestComplete(st, aErr);
		}
	PhMix::Reset(aCh.iStream);
	}

void CPhMixer::RemoveChannel(TChannel* aCh)
	{
	iLock.Wait();
	CompleteRequest(*aCh, KErrCancel);
	aCh->iOwner.Close();
	aCh->iUsed = EFalse;
	iLock.Signal();
	Wake();
	PHMIX_LOG("[phmix] channel closed");
	}

// the first channel's Open reports whether a real sink exists at all (QEMU virt has none: KErrNotFound)
TInt CPhMixer::ProbeSink()
	{
	iLock.Wait();					// two first-time Opens must not probe at once (the unit is exclusive: KErrInUse)
	TInt r = KErrNone;
	if (!iSinkEverOk)
		{
		RSoundSc probe;
		r = probe.Open(KSoundScTxUnit0);
		if (r == KErrNone)
			{
			probe.Close();
			iSinkEverOk = ETrue;
			}
		}
	iLock.Signal();
	return r;
	}

// --- mixer thread -----------------------------------------------------------------------------------------------------
void CPhMixer::Run()
	{
	iTimer.CreateLocal();
	iTimerArmed = EFalse;
	iSinkOpen = EFalse;
	for (TInt i = 0; i < KSinkSlots; ++i)
		iSlotBusy[i] = EFalse;
	iVolDirty = EFalse;
	iVolSubscribed = EFalse;
	if (iVolProp.Attach(KPhAudioPropertyCategory, KPhAudioMasterVolume) == KErrNone)
		{
		iVolProp.Subscribe(iVolStatus);
		iVolSubscribed = ETrue;
		ReadMasterVolume();
		}
	for (;;)
		{
		User::WaitForAnyRequest();
		Service();
		}
	}

void CPhMixer::ReadMasterVolume()
	{
	TInt step = 0;
	if (iVolProp.Get(step) == KErrNone)
		{
		iMasterCode = PhVolStepToCode(step);
		iVolDirty = ETrue;
		PHMIX_LOG("[phmix] master volume step %d -> code %d", step, iMasterCode);
		}
	}

// a channel whose owner thread died can never be closed by it: drop its requests and free it (lock held)
void CPhMixer::ReapDead()
	{
	for (TInt i = 0; i < KMaxChannels; ++i)
		{
		TChannel& ch = iCh[i];
		if (ch.iUsed && ch.iOwner.ExitType() != EExitPending)
			{
			ch.iHead = ch.iCount = 0;
			ch.iOwner.Close();
			ch.iUsed = EFalse;
			PHMIX_LOG("[phmix] reaped a channel of a dead thread");
			}
		}
	}

TBool CPhMixer::AnyData()
	{
	for (TInt i = 0; i < KMaxChannels; ++i)
		if (iCh[i].iUsed && !iCh[i].iPaused && iCh[i].iCount > 0)
			return ETrue;
	return EFalse;
	}

TInt CPhMixer::OpenSink()
	{
	TInt r = iSink.Open(KSoundScTxUnit0);
	if (r != KErrNone)
		return r;
	TCurrentSoundFormatV02Buf fmt;
	fmt().iChannels = 2;
	fmt().iRate = ESoundRate48000Hz;
	fmt().iEncoding = ESoundEncoding16BitPCM;
	fmt().iDataFormat = ESoundDataFormatInterleaved;
	fmt().iReserved1 = 0;
	r = iSink.SetAudioFormat(fmt);
	if (r == KErrNone)
		{
		Mem::FillZ(&iSinkCfg, sizeof(iSinkCfg));
		iSinkCfg.iBase.iNumBuffers = KSinkBuffers;
		iSinkCfg.iBase.iBufferSizeInBytes = KSinkBytes;
		iSinkCfg.iBase.iFlags = 0;
		TPckg<TMixBufCfg> pk(iSinkCfg);
		r = iSink.SetBufferChunkCreate(pk, iSinkChunk);
		if (r == KErrNone)
			iSink.GetBufferConfig(pk);
		}
	if (r != KErrNone)
		{
		iSinkChunk.Close();
		iSink.Close();
		return r;
		}
	iSink.SetVolume(iMasterCode);
	iVolDirty = EFalse;
	iSinkOpen = ETrue;
	iSinkEverOk = ETrue;
	PHMIX_LOG("[phmix] real sink open (48000 stereo, %d x %d B, volume code %d)", KSinkBuffers, KSinkBytes, iMasterCode);
	return KErrNone;
	}

// only with every slot idle
void CPhMixer::CloseSink()
	{
	if (!iSinkOpen)
		return;
	iSinkChunk.Close();
	iSink.Close();
	iSinkOpen = EFalse;
	PHMIX_LOG("[phmix] real sink closed (idle)");
	}

// the real sink failed or cannot open: nothing will play, so fail the clients instead of hanging them
void CPhMixer::FailAll(TInt aErr)
	{
	PHMIX_LOG("[phmix] sink error %d: failing all channels", aErr);
	if (iSinkOpen)
		{
		iSink.CancelPlayData();
		for (TInt i = 0; i < KSinkSlots; ++i)
			if (iSlotBusy[i])
				{
				User::WaitForRequest(iSlot[i]);
				iSlotBusy[i] = EFalse;
				}
		CloseSink();
		}
	iLock.Wait();
	for (TInt i = 0; i < KMaxChannels; ++i)
		if (iCh[i].iUsed)
			CompleteRequest(iCh[i], aErr);
	iLock.Signal();
	}

// Mix one quantum from all unpaused channels into sink buffer aSlot; EFalse = no channel produced anything.
TBool CPhMixer::MixQuantum(TInt aSlot)
	{
	Mem::FillZ(iAcc, sizeof(iAcc));
	TBool any = EFalse;
	iLock.Wait();
	for (TInt i = 0; i < KMaxChannels; ++i)
		{
		TChannel& ch = iCh[i];
		if (!ch.iUsed || ch.iPaused || ch.iCount == 0)
			continue;
		TInt pos = 0;
		while (pos < KQuantum && ch.iCount > 0)
			{
			TReq& rq = ch.iReq[ch.iHead];
			TInt used = 0;
			TInt got = PhMix::MixSegment(ch.iStream, rq.iPtr + rq.iPos * ch.iChannels, rq.iFrames - rq.iPos,
										 ch.iChannels, ch.iRate, ch.iGainQ14, iAcc, KQuantum, pos, &used);
			pos += got;
			rq.iPos += used;
			ch.iBytesPlayed += used * ch.iChannels * 2;
			if (got > 0 || used > 0)
				any = ETrue;
			if (rq.iPos >= rq.iFrames)
				{
				TRequestStatus* st = rq.iStatus;
				ch.iHead = (ch.iHead + 1) % KMaxRequests;
				--ch.iCount;
				ch.iOwner.RequestComplete(st, KErrNone);
				}
			else
				break;			// the quantum is full
			}
		}
	iLock.Signal();
	if (any)
		{
		TUint8* base = iSinkChunk.Base() + iSinkCfg.iOffsets[aSlot];
		PhMix::ClipToS16(iAcc, (PhMix::S16*)base, KQuantum * 2);
		}
	return any;
	}

void CPhMixer::Service()
	{
	// 1. master volume changed by phvolume (re-subscribe first so no update is lost)
	if (iVolSubscribed && iVolStatus != KRequestPending)
		{
		if (iVolStatus.Int() >= KErrNone)
			{
			iVolProp.Subscribe(iVolStatus);
			ReadMasterVolume();
			}
		else
			iVolSubscribed = EFalse;
		}
	if (iVolDirty && iSinkOpen)
		{
		iSink.SetVolume(iMasterCode);
		iVolDirty = EFalse;
		}
	// 2. real-sink slots that finished
	for (TInt i = 0; i < KSinkSlots; ++i)
		if (iSlotBusy[i] && iSlot[i] != KRequestPending)
			{
			iSlotBusy[i] = EFalse;
			TInt r = iSlot[i].Int();
			if (r != KErrNone && r != KErrUnderflow)
				{
				FailAll(r);
				return;
				}
			}
	// 3. the idle timer
	TBool timerFired = EFalse;
	if (iTimerArmed && iTimerStatus != KRequestPending)
		{
		iTimerArmed = EFalse;
		timerFired = ETrue;
		}
	// 4. what is there to play
	iLock.Wait();
	ReapDead();
	TBool data = AnyData();
	iLock.Signal();
	if (data)
		{
		if (iTimerArmed)
			{
			iTimer.Cancel();
			User::WaitForRequest(iTimerStatus);
			iTimerArmed = EFalse;
			}
		if (!iSinkOpen)
			{
			TInt r = OpenSink();
			if (r != KErrNone)
				{
				FailAll(r);
				return;
				}
			}
		for (TInt i = 0; i < KSinkSlots; ++i)
			{
			if (iSlotBusy[i])
				continue;
			if (!MixQuantum(i))
				break;
			iSink.PlayData(iSlot[i], iSinkCfg.iOffsets[i], KSinkBytes, 0);
			iSlotBusy[i] = ETrue;
			}
		}
	else if (iSinkOpen)
		{
		TBool busy = EFalse;
		for (TInt i = 0; i < KSinkSlots; ++i)
			busy = busy || iSlotBusy[i];
		if (!busy)
			{
			if (timerFired)
				CloseSink();
			else if (!iTimerArmed)
				{
				iTimer.After(iTimerStatus, KIdleCloseUs);
				iTimerArmed = ETrue;
				}
			}
		}
	}

// --- the RX (record) pass-through -------------------------------------------------------------------------------------
RSoundSc* RealRec(RMixSoundSc& /*aDev*/, TAny*& aRec, TInt aVol)
	{
	if (!aRec)
		{
		RSoundSc* n = new RSoundSc;
		if (!n)
			return NULL;
		if (n->Open(KSoundScRxUnit0) != KErrNone)
			{
			delete n;
			return NULL;
			}
		n->SetVolume(aVol);
		aRec = n;
		}
	return (RSoundSc*)aRec;
	}
}	// namespace

// ============================================================================================================================
// RMixSoundSc: the virtual channel the adapter holds (declared in ph_mixsc.h)
// ============================================================================================================================
TInt RMixSoundSc::Open(TInt aUnit)
	{
	if (iHandle)
		return KErrInUse;
	if (aUnit == KSoundScRxUnit0)
		{
		// no hardware is touched until the first record call (a playback must not need, or hold, the record unit)
		iUnit = aUnit;
		iHandle = 1;
		return KErrNone;
		}
	if (aUnit != KSoundScTxUnit0)
		return KErrNotSupported;
	CPhMixer* m = Mixer();
	if (!m)
		return KErrNoMemory;
	TInt r = m->ProbeSink();
	if (r != KErrNone)
		return r;
	TChannel* ch = NULL;
	r = m->AddChannel(ch);
	if (r != KErrNone)
		return r;
	iChan = ch;
	iUnit = aUnit;
	iHandle = 1;
	return KErrNone;
	}

void RMixSoundSc::Close()
	{
	if (!iHandle)
		return;
	if (iUnit == KSoundScRxUnit0)
		{
		if (iRec)
			{
			((RSoundSc*)iRec)->Close();
			delete (RSoundSc*)iRec;
			iRec = NULL;
			}
		}
	else if (iChan)
		{
		Mixer()->RemoveChannel((TChannel*)iChan);
		iChan = NULL;
		}
	iHandle = 0;
	iUnit = -1;
	}

void RMixSoundSc::Caps(TDes8& aCapsBuf)
	{
	TSoundFormatsSupportedV02 caps;
	Mem::FillZ(&caps, sizeof(caps));
	if (iUnit == KSoundScRxUnit0)
		{
		RSoundSc* rec = RealRec(*this, iRec, iVol);
		if (rec)
			{
			rec->Caps(aCapsBuf);
			return;
			}
		caps.iDirection = ESoundDirRecord;
		}
	else
		{
		caps.iDirection = ESoundDirPlayback;
		caps.iChannels = KSoundMonoChannel | KSoundStereoChannel;
		caps.iRates = 0x3FFF;					// all 14 rates, 7.35 - 48 kHz: the mixer resamples
		caps.iEncodings = KSoundEncoding16BitPCM;
		caps.iDataFormats = KSoundDataFormatInterleaved;
		caps.iRequestMinSize = 4;
		caps.iRequestAlignment = 2;				// 4-byte aligned (2^2)
		caps.iHwConfigNotificationSupport = EFalse;
		}
	TPckg<TSoundFormatsSupportedV02> pk(caps);
	aCapsBuf.Copy(pk);
	}

void RMixSoundSc::AudioFormat(TDes8& aFormatBuf)
	{
	if (iUnit == KSoundScRxUnit0)
		{
		RSoundSc* rec = RealRec(*this, iRec, iVol);
		if (rec)
			rec->AudioFormat(aFormatBuf);
		return;
		}
	TChannel* ch = (TChannel*)iChan;
	if (!ch)
		return;
	TCurrentSoundFormatV02 f;
	Mem::FillZ(&f, sizeof(f));
	f.iChannels = ch->iChannels;
	for (TInt i = 0; i < KNumRates; ++i)
		if (KRateHz[i] == ch->iRate)
			f.iRate = (TSoundRate)i;
	f.iEncoding = ESoundEncoding16BitPCM;
	f.iDataFormat = ESoundDataFormatInterleaved;
	TPckg<TCurrentSoundFormatV02> pk(f);
	aFormatBuf.Copy(pk);
	}

TInt RMixSoundSc::SetAudioFormat(const TDesC8& aFormatBuf)
	{
	if (iUnit == KSoundScRxUnit0)
		{
		RSoundSc* rec = RealRec(*this, iRec, iVol);
		return rec ? rec->SetAudioFormat(aFormatBuf) : KErrNotSupported;
		}
	TChannel* ch = (TChannel*)iChan;
	if (!ch || aFormatBuf.Length() < (TInt)sizeof(TCurrentSoundFormatV02))
		return KErrArgument;
	const TCurrentSoundFormatV02& f = *(const TCurrentSoundFormatV02*)aFormatBuf.Ptr();
	if (f.iEncoding != ESoundEncoding16BitPCM || f.iDataFormat != ESoundDataFormatInterleaved
		|| (f.iChannels != 1 && f.iChannels != 2) || TUint(f.iRate) >= TUint(KNumRates))
		return KErrNotSupported;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	ch->iRate = KRateHz[f.iRate];
	ch->iChannels = f.iChannels;
	PhMix::Reset(ch->iStream);
	m->iLock.Signal();
	return KErrNone;
	}

void RMixSoundSc::GetBufferConfig(TDes8& aConfigBuf)
	{
	if (iUnit == KSoundScRxUnit0)
		{
		RSoundSc* rec = RealRec(*this, iRec, iVol);
		if (rec)
			rec->GetBufferConfig(aConfigBuf);
		return;
		}
	TChannel* ch = (TChannel*)iChan;
	if (ch && ch->iCfgLen > 0)
		{
		TInt n = ch->iCfgLen < aConfigBuf.MaxLength() ? ch->iCfgLen : aConfigBuf.MaxLength();
		aConfigBuf.Copy((const TUint8*)&ch->iCfg, n);
		}
	}

TInt RMixSoundSc::SetBufferChunkCreate(const TDesC8& aConfigBuf, RChunk& aChunk)
	{
	if (iUnit == KSoundScRxUnit0)
		{
		RSoundSc* rec = RealRec(*this, iRec, iVol);
		return rec ? rec->SetBufferChunkCreate(aConfigBuf, aChunk) : KErrNotSupported;
		}
	TChannel* ch = (TChannel*)iChan;
	if (!ch || aConfigBuf.Length() < (TInt)sizeof(TSharedChunkBufConfigBase))
		return KErrArgument;
	if (ch->iCount > 0)
		return KErrInUse;
	TMixBufCfg cfg;
	Mem::FillZ(&cfg, sizeof(cfg));
	TInt len = aConfigBuf.Length() < (TInt)sizeof(cfg) ? aConfigBuf.Length() : (TInt)sizeof(cfg);
	Mem::Copy(&cfg, aConfigBuf.Ptr(), len);
	const TInt n = cfg.iBase.iNumBuffers, sz = cfg.iBase.iBufferSizeInBytes;
	if (n < 1 || n > 8 || sz < 4 || sz > 0x10000 || (sz & 3) != 0)
		return KErrArgument;
	TInt r = aChunk.CreateLocal(n * sz, n * sz);		// the adapter reads/writes it via Base(); the mixer reads it by address
	if (r != KErrNone)
		return r;
	for (TInt i = 0; i < n; ++i)
		cfg.iOffsets[i] = i * sz;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	ch->iBase = aChunk.Base();
	ch->iCfg = cfg;
	ch->iCfgLen = len;			// GetBufferConfig hands back exactly as many bytes as the adapter packed
	m->iLock.Signal();
	return KErrNone;
	}

TInt RMixSoundSc::Volume()
	{
	if (iUnit == KSoundScRxUnit0)
		return iRec ? ((RSoundSc*)iRec)->Volume() : iVol;
	TChannel* ch = (TChannel*)iChan;
	return ch ? ch->iGainCode : 255;
	}

TInt RMixSoundSc::SetVolume(TInt aVolume)
	{
	if (aVolume < 0 || aVolume > 255)
		return KErrArgument;
	if (iUnit == KSoundScRxUnit0)
		{
		iVol = aVolume;
		return iRec ? ((RSoundSc*)iRec)->SetVolume(aVolume) : KErrNone;
		}
	TChannel* ch = (TChannel*)iChan;
	if (!ch)
		return KErrNotReady;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	ch->iGainCode = aVolume;
	ch->iGainQ14 = PhMix::GainQ14FromCode(aVolume);		// per-stream software gain; the master is the hardware volume
	m->iLock.Signal();
	return KErrNone;
	}

void RMixSoundSc::PlayData(TRequestStatus& aStatus, TInt aBufferOffset, TInt aLength, TUint /*aFlags*/)
	{
	aStatus = KRequestPending;
	TChannel* ch = (TChannel*)iChan;
	TRequestStatus* st = &aStatus;
	if (!ch || !ch->iBase || aBufferOffset < 0 || aLength < 0)
		{
		User::RequestComplete(st, KErrNotReady);
		return;
		}
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	if (ch->iCount >= KMaxRequests)
		{
		m->iLock.Signal();
		User::RequestComplete(st, KErrOverflow);
		return;
		}
	TReq& rq = ch->iReq[(ch->iHead + ch->iCount) % KMaxRequests];
	rq.iPtr = (const TInt16*)(ch->iBase + aBufferOffset);
	rq.iFrames = aLength / (ch->iChannels * 2);
	rq.iPos = 0;
	rq.iStatus = st;
	++ch->iCount;
	m->iLock.Signal();
	m->Wake();
	}

void RMixSoundSc::RecordData(TRequestStatus& aStatus, TInt& aLength)
	{
	RSoundSc* rec = RealRec(*this, iRec, iVol);
	if (rec)
		rec->RecordData(aStatus, aLength);
	else
		{
		aStatus = KRequestPending;
		TRequestStatus* st = &aStatus;
		User::RequestComplete(st, KErrNotSupported);
		}
	}

TInt RMixSoundSc::ReleaseBuffer(TInt aChunkOffset)
	{
	return iRec ? ((RSoundSc*)iRec)->ReleaseBuffer(aChunkOffset) : KErrNotFound;
	}

void RMixSoundSc::CancelPlayData()
	{
	TChannel* ch = (TChannel*)iChan;
	if (!ch)
		return;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	while (ch->iCount > 0)
		{
		TRequestStatus* st = ch->iReq[ch->iHead].iStatus;
		ch->iHead = (ch->iHead + 1) % KMaxRequests;
		--ch->iCount;
		ch->iOwner.RequestComplete(st, KErrCancel);
		}
	PhMix::Reset(ch->iStream);
	m->iLock.Signal();
	}

void RMixSoundSc::CancelRecordData()
	{
	if (iRec)
		((RSoundSc*)iRec)->CancelRecordData();
	}

void RMixSoundSc::Cancel(const TRequestStatus& aStatus)
	{
	if (iUnit == KSoundScRxUnit0)
		{
		if (iRec)
			((RSoundSc*)iRec)->Cancel(aStatus);
		return;
		}
	TChannel* ch = (TChannel*)iChan;
	if (!ch)
		return;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	for (TInt i = 0; i < ch->iCount; ++i)
		{
		TInt idx = (ch->iHead + i) % KMaxRequests;
		if (ch->iReq[idx].iStatus == &aStatus)
			{
			TRequestStatus* st = ch->iReq[idx].iStatus;
			// close the gap, keeping the order of the rest
			for (TInt j = i; j + 1 < ch->iCount; ++j)
				ch->iReq[(ch->iHead + j) % KMaxRequests] = ch->iReq[(ch->iHead + j + 1) % KMaxRequests];
			--ch->iCount;
			ch->iOwner.RequestComplete(st, KErrCancel);
			break;
			}
		}
	m->iLock.Signal();
	}

TInt RMixSoundSc::BytesTransferred()
	{
	TChannel* ch = (TChannel*)iChan;
	return ch ? ch->iBytesPlayed : 0;
	}

void RMixSoundSc::ResetBytesTransferred()
	{
	TChannel* ch = (TChannel*)iChan;
	if (ch)
		ch->iBytesPlayed = 0;
	}

TInt RMixSoundSc::Pause()
	{
	if (iUnit == KSoundScRxUnit0)
		return iRec ? ((RSoundSc*)iRec)->Pause() : KErrNone;
	TChannel* ch = (TChannel*)iChan;
	if (!ch)
		return KErrNotReady;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	ch->iPaused = ETrue;
	m->iLock.Signal();
	return KErrNone;
	}

TInt RMixSoundSc::Resume()
	{
	if (iUnit == KSoundScRxUnit0)
		return iRec ? ((RSoundSc*)iRec)->Resume() : KErrNone;
	TChannel* ch = (TChannel*)iChan;
	if (!ch)
		return KErrNotReady;
	CPhMixer* m = Mixer();
	m->iLock.Wait();
	ch->iPaused = EFalse;
	m->iLock.Signal();
	m->Wake();
	return KErrNone;
	}
