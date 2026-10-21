// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phevidctl.cpp -- OURS (VDU rung V4): the MMF video play controller for MP4 / 3GP.
// Structure and call order follow the 2010 AviPlayController (baseline/symbian-2010/oss.FCL.sf.os.mmaudio/mmplugins/
// mmfwplugins/src/Plugin/Controller/Video/AviPlayController): AddDataSourceL opens the clip and reports OpenComplete;
// MvpcPrepare / PrimeL initialise DevVideo; PlayL starts DevVideo and the input feed; MdvpoNewBuffers refills; MdvpoStreamEnd
// ends playback with KMMFEventCategoryPlaybackComplete. The differences: the container is parsed by mp4demux (OURS), the
// decoder's output is NV12 (phevdec) so THIS controller is the post-processor (NV12 -> screen, PTS paced, drawn through
// CDirectScreenBitmap into the region the client's CDirectScreenAccess hands over), and audio is skipped.
// See phevidctl.h for the full contract.
#include <ecom/implementationproxy.h>
#include <e32base.h>
#include <e32debug.h>
#include <e32math.h>
#include <hal.h>
#include <w32std.h>
#include <platform/cdsb.h>
#include <mmf/common/mmfcontroller.h>
#include <mmf/server/mmfdatasource.h>
#include <mmf/server/mmfdatasink.h>
#include <mmf/server/mmffile.h>
#include <mmf/server/mmfbuffer.h>
#include <mmf/common/mmfstandardcustomcommands.h>
#include <mmf/devvideo/devvideoplay.h>
#include <mmf/server/mmfaudiooutput.h>
#include <mmf/server/sounddevice.h>
#include <hal.h>

#include "phevidctl.h"
#include "mp4demux.h"
#include "rphedeov.h"		// the DE2 video plane (src/phorepoc-arm/a64/display; de2-overlay-plan.md): used when the kernel has it

_LIT8(KMimeH264, "video/H264");
const TInt KReadChunk = 0x10000;			// file reads (the moov box is read in pieces of this size)
const TInt KMaxMoov = 32 * 1024 * 1024;
const TInt KMaxSample = 16 * 1024 * 1024;
const TInt KQueue = 3;						// pictures held (not yet returned) while waiting for their presentation time
const TInt KFeedBurst = 8;					// samples written per scheduler step

class CPheVidCtl;

// the pheffmpeg.dll C API (src/pheffmpeg/pheffmpeg.def; ordinals 2..5)
typedef void* (*TAacOpen)(const TUint8* aAsc, TInt aAscLen);
typedef TInt (*TAacDecode)(void* aH, const TUint8* aPkt, TInt aLen, TInt16* aPcm, TInt aMaxFrames, TInt* aRate, TInt* aCh);
typedef void (*TAacClose)(void* aH);
typedef TInt (*TAacSetOut)(void* aH, TInt aRate, TInt aCh);	// ordinal 5: convert the output (swresample) to aRate / aCh
const TInt KPcmMaxFrames = 8192;			// per access unit (AAC 1024, HE-AAC 2048; x48000/8000 when resampled up)
const TInt KOutRate = 48000;				// DevSound is always 48 kHz stereo: the only config proven on the A64 sound path (tonetest)
const TInt KOutCh = 2;
const TInt64 KAudioStartWaitUs = 3000000;	// wait this long for the audio clock to start before pacing by the wall clock

// the DSA-paced presenter: waits for a picture's due time (RTimer), then for the vsync after drawing it (EndUpdate)
class CPresenter : public CActive
	{
public:
	enum TState { EIdle, ETimer, EVsync };
	CPresenter(CPheVidCtl& aOwner) : CActive(CActive::EPriorityStandard), iOwner(aOwner), iState(EIdle)
		{ CActiveScheduler::Add(this); }
	void ConstructL() { User::LeaveIfError(iTimer.CreateLocal()); }
	~CPresenter() { Cancel(); iTimer.Close(); }
	void After(TInt aUs) { iState = ETimer; iTimer.After(iStatus, aUs); SetActive(); }
	TRequestStatus& Vsync() { iState = EVsync; iStatus = KRequestPending; return iStatus; }	// EndUpdate(status), then Armed()
	void Armed() { SetActive(); }
	void RunL();
	void DoCancel();
private:
	CPheVidCtl& iOwner;
	TState iState;
	RTimer iTimer;
	};

struct TSampleRec
	{
	TInt64 iOffset;
	TUint32 iSize;
	TBool iSync;
	TInt64 iPtsUs;
	};

class CPheVidCtl : public CMMFController,
				   public MMMFVideoPlayControllerCustomCommandImplementor,
				   public MMMFVideoControllerCustomCommandImplementor,
				   public MMMFVideoSetInitScreenCustomCommandImplementor,
				   public MMMFDevVideoPlayObserver,
				   public MDevSoundObserver
	{
public:
	static CPheVidCtl* NewL();
	~CPheVidCtl();

	// CMMFController
	void AddDataSourceL(MDataSource& aDataSource);
	void AddDataSinkL(MDataSink& aDataSink);
	void RemoveDataSourceL(MDataSource& aDataSource);
	void RemoveDataSinkL(MDataSink& aDataSink);
	void ResetL();
	void PrimeL();
	void PrimeL(TMMFMessage& aMessage);
	void PlayL();
	void PauseL();
	void StopL();
	TTimeIntervalMicroSeconds PositionL() const;
	void SetPositionL(const TTimeIntervalMicroSeconds& aPosition);
	TTimeIntervalMicroSeconds DurationL() const;
	void CustomCommand(TMMFMessage& aMessage);
	void SetPrioritySettings(const TMMFPrioritySettings& aPrioritySettings);
	void GetNumberOfMetaDataEntriesL(TInt& aNumberOfEntries);
	CMMFMetaDataEntry* GetMetaDataEntryL(TInt aIndex);

	// MMMFVideoPlayControllerCustomCommandImplementor
	void MvpcPrepare();
	void MvpcGetFrameL(MMMFVideoFrameMessage& aMessage);
	void MvpcGetAudioEnabledL(TBool& aEnabled);
	void MvpcSetDisplayWindowL(const TRect& aWindowRect, const TRect& aClipRect);
	void MvpcUpdateDisplayRegionL(const TRegion& aRegion);
	void MvpcDirectScreenAccessEventL(const TMMFDSAEvent aDSAEvent);
	void MvpcRefreshFrameL();
	void MvpcPlayL(const TTimeIntervalMicroSeconds& aBegin, const TTimeIntervalMicroSeconds& aEnd);
	void MvpcGetLoadingProgressL(TInt& aPercentage);
	void MvpcSetRotationL(TVideoRotation aRotation);
	void MvpcGetRotationL(TVideoRotation& aRotation);
	void MvpcSetScaleFactorL(TReal32 aWidthPercentage, TReal32 aHeightPercentage, TBool aAntiAliasFiltering);
	void MvpcGetScaleFactorL(TReal32& aWidthPercentage, TReal32& aHeightPercentage, TBool& aAntiAliasFiltering);
	void MvpcSetCropRegionL(const TRect& aCropRegion);
	void MvpcGetCropRegionL(TRect& aCropRegion);

	// MMMFVideoControllerCustomCommandImplementor
	void MvcGetAudioCodecL(TFourCC& aCodec);
	void MvcGetVideoBitRateL(TInt& aBitRate);
	void MvcGetAudioBitRateL(TInt& aBitRate);
	void MvcSetFrameRateL(TReal32 aFramesPerSecond);
	void MvcGetFrameRateL(TReal32& aFramesPerSecond);
	void MvcGetVideoFrameSizeL(TSize& aVideoFrameSize);
	void MvcGetVideoMimeTypeL(TDes8& aMimeType);

	// MMMFVideoSetInitScreenCustomCommandImplementor
	void MvsdSetInitScreenNumber(TInt aScreenNumber);

	// MMMFDevVideoPlayObserver
	void MdvpoNewBuffers();
	void MdvpoReturnPicture(TVideoPicture* aPicture);
	void MdvpoSupplementalInformation(const TDesC8& aData, const TTimeIntervalMicroSeconds& aTimestamp, const TPictureId& aPictureId);
	void MdvpoPictureLoss();
	void MdvpoPictureLoss(const TArray<TPictureId>& aPictures);
	void MdvpoSliceLoss(TUint aFirstMacroblock, TUint aNumMacroblocks, const TPictureId& aPicture);
	void MdvpoReferencePictureSelection(const TDesC8& aSelectionData);
	void MdvpoTimedSnapshotComplete(TInt aError, TPictureData* aPictureData, const TTimeIntervalMicroSeconds& aPresentationTimestamp, const TPictureId& aPictureId);
	void MdvpoFatalError(TInt aError);
	void MdvpoNewPictures();
	void MdvpoInitComplete(TInt aError);
	void MdvpoStreamEnd();

	// MDevSoundObserver (audio)
	void InitializeComplete(TInt aError);
	void ToneFinished(TInt) {}
	void BufferToBeFilled(CMMFBuffer* aBuffer);
	void PlayError(TInt aError);
	void BufferToBeEmptied(CMMFBuffer*) {}
	void RecordError(TInt) {}
	void ConvertError(TInt) {}
	void DeviceMessage(TUid, const TDesC8&) {}

	// the presenter's callback
	void PresenterDone() { ReleaseRetired(); Pump(); }
	void VsyncDone() { iTVsync += User::FastCounter() - iVsyncT0; ++iNVsync; }
	void CancelFlip() { if (iOvOn) iOv.CancelFlip(); }

private:
	class CSourceSinkEventHandler : public MAsyncEventHandler
		{
	public:
		CSourceSinkEventHandler(CPheVidCtl& aParent) : iParent(aParent) {}
		TInt SendEventToClient(const TMMFEvent& aEvent) { iParent.DoSendEventToClient(aEvent); return KErrNone; }
	private:
		CPheVidCtl& iParent;
		};
	class CMessageHolder : public CBase
		{
	public:
		CMessageHolder(TMMFMessage& aMessage) : iMessage(aMessage) {}
		void Complete(TInt aError) { iMessage.Complete(aError); }
	private:
		TMMFMessage iMessage;
		};

	CPheVidCtl() {}
	void ConstructL();
	void OpenClipL();
	TInt ReadAt(TInt64 aPos, TUint8* aDst, TInt aLen);
	static int ReadCb(void* aCtx, uint64_t aPos, uint8_t* aDst, uint32_t aLen);
	static bool SampleCb(void* aCtx, const mp4::TSample& aSample);
	void PrepareL();
	void SendEvent(TUid aType, TInt aError);
	void CompleteInit(TInt aError);
	void Feed();
	static TInt FeedCb(TAny* aSelf) { ((CPheVidCtl*)aSelf)->Feed(); return 0; }
	static TInt FinishCb(TAny* aSelf);
	TInt FillInputL(const TSampleRec& aRec, TVideoInputBuffer*& aBuf, TBool aWrite);
	void PullPictures();
	void Pump();
	TBool PresentOne();
	TBool Draw(const TVideoPicture& aPic);
	void PrimeBitmap();
	TBool OverlayShow(const TVideoPicture& aPic, const TRect& aCrop, const TRect& aDest);
	void OverlayOff();
	void ReleaseRetired();
	void CheckFinished();
	void FinishPlayback();
	void StopInternal();
	void FailPlayback(TInt aError);
	void UpdateClip();
	void CloseBitmap();
	// audio
	static bool AudioSampleCb(void* aCtx, const mp4::TSample& aSample);
	void PrepareAudioL();
	void CloseAac();
	void TryCompleteInit();
	void StartAudioL();
	void StopAudio();
	void AudioFailed(TInt aError);
	TInt FillPcmL(TDes8& aDst, TBool& aLast);
	TBool AudioClockUs(TInt64& aUs);
	static TInt64 NowUs() { TTime t; t.HomeTime(); return t.Int64(); }

	CMMFDevVideoPlay* iDvp;
	THwDeviceId iHw;
	CMMFFile* iClip;
	CSourceSinkEventHandler* iEventHandler;
	CMessageHolder* iPrimeMsg;
	TBool iSinkAdded;
	MMMFAudioOutput* iAudioOut;
	// audio: pheffmpeg.dll (RLibrary) -> AAC decoder -> PCM16 -> CMMFDevSound (owned by the audio output sink)
	CMMFDevSound* iDevSound;
	RLibrary iFf;
	TBool iFfLoaded;
	TAacOpen iAacOpen;
	TAacDecode iAacDecode;
	TAacClose iAacClose;
	TAacSetOut iAacSetOut;
	void* iAac;
	RArray<TSampleRec> iASamples;
	HBufC8* iARaw;					// scratch: one raw AAC access unit
	HBufC8* iPcm;					// the decoded PCM of the current access unit (S16, interleaved)
	TInt iPcmOff, iPcmLen;
	TInt iARate, iACh, iMaxASample;
	TInt iANext;
	TBool iAudioReady;				// DevSound initialised + configured, decoder verified
	TBool iAudioInitPending, iVideoInitDone;
	TInt iVideoInitErr;
	TBool iAudioActive;			// this run plays audio and paces the video by it
	TBool iAudioDone;				// DevSound reported the end of the last buffer
	TBool iAudioFallback;			// the audio clock never started: wall clock
	TInt iClockMode;				// 1 GetTimePlayed, 2 SamplesPlayed
	TInt64 iLastPlayed, iLastPlayedWall, iLastClock;
	TBool iClockSeen;
	TInt64 iDriftSumUs;
	TUint32 iTBegin, iTConv, iTDraw, iNDraw, iTVsync, iVsyncT0, iNVsync;	// stage timing (User::FastCounter ticks), printed at the end of a pass
	TInt iDriftN;
	TBool iAudioStarted, iUseA, iNeedResync;
	TInt iARateEnum;
	TInt iSkipFrames;
	TInt64 iClockFirstVal, iClockFirstWall;
	TMMFPrioritySettings iPriority;
	// clip
	HBufC8* iMoov;
	mp4::TMovie iMovie;
	RArray<TSampleRec> iSamples;
	CMMFDescriptorBuffer* iRead;
	HBufC8* iPsets;					// SPS+PPS as Annex B
	HBufC8* iConv;					// scratch: one raw sample
	TInt iFramePeriodUs;
	TInt iMaxSample;
	// state
	TBool iPreparing, iPrepared, iPrepareEvent, iPlaying, iInputEnd, iStreamEnd, iFinishing, iPumping, iBench;
	TInt iNext;
	TInt iError;
	TInt iScreenNumber;
	CAsyncCallBack* iFeeder;
	CAsyncCallBack* iFinisher;
	// pictures
	TVideoPicture* iQ[KQueue];
	TInt iQHead, iQCount;
	CPresenter* iPresenter;
	TInt64 iPlayStartUs, iClockStartUs, iTs0Us, iDueUs;
	TBool iHaveTs0;
	TInt64 iPosUs;
	TInt64 iLateSumUs;
	// display
	TRect iScreenRect, iClipRect;
	RRegion iDisplayRegion, iDerived;
	TBool iDsaOn;
	CDirectScreenBitmap* iBmp;
	TRect iBmpRect;
	HBufC8* iRowY;
	HBufC8* iRowUV;
	TInt* iXMap;
	TInt iXMapLen;
	// the DE2 video plane: pictures are scanned straight out of the VE arena (no CPU conversion) while the plane is on.
	// iShown = the picture on the plane (held, not returned to the decoder); iRetired = the one it replaced, returned once
	// the replacing flip completed (the vblank after which the DE no longer reads it)
	RPheDeOv iOv;
	TInt iOvState;					// 0 untried, 1 open, 2 unavailable (no driver / no capability)
	TBool iOvOn, iOvPosted;
	TVideoPicture* iShown;
	TVideoPicture* iRetired;
	TPheDeOvSet iOvGeom;
	TInt iOvFrames;
	TPheVidStats iStats;
	};

void CPresenter::RunL()
	{
	const TState s = iState;
	iState = EIdle;
	if (s == EVsync)
		iOwner.VsyncDone();
	iOwner.PresenterDone();
	}

// --------------------------------------------------------------------------------------------------------------------
// construction

CPheVidCtl* CPheVidCtl::NewL()
	{
	CPheVidCtl* self = new (ELeave) CPheVidCtl;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

void CPheVidCtl::ConstructL()
	{
	CMMFVideoPlayControllerCustomCommandParser* p1 = CMMFVideoPlayControllerCustomCommandParser::NewL(*this);
	CleanupStack::PushL(p1);
	AddCustomCommandParserL(*p1);
	CleanupStack::Pop(p1);
	CMMFVideoControllerCustomCommandParser* p2 = CMMFVideoControllerCustomCommandParser::NewL(*this);
	CleanupStack::PushL(p2);
	AddCustomCommandParserL(*p2);
	CleanupStack::Pop(p2);
	CMMFVideoSetInitScreenCustomCommandParser* p3 = CMMFVideoSetInitScreenCustomCommandParser::NewL(*this);
	CleanupStack::PushL(p3);
	AddCustomCommandParserL(*p3);
	CleanupStack::Pop(p3);
	iEventHandler = new (ELeave) CSourceSinkEventHandler(*this);
	iPresenter = new (ELeave) CPresenter(*this);
	iPresenter->ConstructL();
	iFeeder = new (ELeave) CAsyncCallBack(TCallBack(FeedCb, this), CActive::EPriorityStandard);
	iFinisher = new (ELeave) CAsyncCallBack(TCallBack(FinishCb, this), CActive::EPriorityStandard);
	}

CPheVidCtl::~CPheVidCtl()
	{
	StopAudio();
	CloseAac();
	delete iARaw;
	delete iPcm;
	iASamples.Close();
	if (iFfLoaded)
		iFf.Close();
	delete iFeeder;
	delete iFinisher;
	delete iPresenter;
	CloseBitmap();
	OverlayOff();
	if (iOvState == 1)
		iOv.Close();
	delete iDvp;
	delete iPrimeMsg;
	delete iEventHandler;
	delete iRead;
	delete iMoov;
	delete iPsets;
	delete iConv;
	delete iRowY;
	delete iRowUV;
	delete[] iXMap;
	iSamples.Close();
	iDisplayRegion.Close();
	iDerived.Close();
	}

void CPheVidCtl::SendEvent(TUid aType, TInt aError)
	{
	TMMFEvent ev;
	ev.iEventType = aType;
	ev.iErrorCode = aError;
	DoSendEventToClient(ev);
	}

// --------------------------------------------------------------------------------------------------------------------
// source / sink

int CPheVidCtl::ReadCb(void* aCtx, uint64_t aPos, uint8_t* aDst, uint32_t aLen)
	{
	if (aPos > (uint64_t)KMaxTInt)
		return -1;
	const TInt r = ((CPheVidCtl*)aCtx)->ReadAt((TInt64)aPos, aDst, (TInt)aLen);
	return r;
	}

TInt CPheVidCtl::ReadAt(TInt64 aPos, TUint8* aDst, TInt aLen)
	{
	TInt got = 0;
	while (got < aLen)
		{
		const TInt n = Min(aLen - got, KReadChunk);
		TRAPD(r, iRead->SetRequestSizeL(n); iClip->ReadBufferL(iRead, (TInt)aPos + got));
		if (r != KErrNone)
			return r < 0 ? r : -1;
		const TDesC8& d = iRead->Data();
		if (d.Length() <= 0)
			break;
		Mem::Copy(aDst + got, d.Ptr(), d.Length());
		got += d.Length();
		if (d.Length() < n)
			break;
		}
	return got;
	}

bool CPheVidCtl::AudioSampleCb(void* aCtx, const mp4::TSample& aS)
	{
	CPheVidCtl* c = (CPheVidCtl*)aCtx;
	TSampleRec r;
	r.iOffset = (TInt64)aS.iOffset;
	r.iSize = aS.iSize;
	r.iSync = ETrue;
	r.iPtsUs = aS.iDts * 1000000 / (TInt64)c->iMovie.iAudioTimescale;
	if (aS.iSize > (TUint32)c->iMaxASample)
		c->iMaxASample = aS.iSize;
	return c->iASamples.Append(r) == KErrNone;
	}

bool CPheVidCtl::SampleCb(void* aCtx, const mp4::TSample& aS)
	{
	CPheVidCtl* c = (CPheVidCtl*)aCtx;
	TSampleRec r;
	r.iOffset = (TInt64)aS.iOffset;
	r.iSize = aS.iSize;
	r.iSync = aS.iSync;
	r.iPtsUs = aS.iCts * 1000000 / (TInt64)c->iMovie.iTimescale;
	if (aS.iSize > (TUint32)c->iMaxSample)
		c->iMaxSample = aS.iSize;
	if (c->iSamples.Append(r) != KErrNone)
		return false;
	return true;
	}

void CPheVidCtl::OpenClipL()
	{
	iRead = CMMFDescriptorBuffer::NewL(KReadChunk);
	const TInt size = iClip->Size();
	if (size <= 16)
		User::Leave(KErrCorrupt);
	uint64_t pos = 0;
	uint32_t len = 0;
	int r = mp4::Locate(ReadCb, this, (uint64_t)size, KMaxMoov, pos, len);
	RDebug::Printf("PH: vc locate r=%d moov at %d len %d (file %d)", r, (TInt)pos, (TInt)len, size);
	if (r != mp4::KOk)
		User::Leave(r == mp4::KErrTooBig ? KErrNoMemory : (r == mp4::KErrNoMoov || r == mp4::KErrFragmented) ? KErrNotSupported : KErrCorrupt);
	iMoov = HBufC8::NewL(len);
	TUint8* dst = (TUint8*)iMoov->Ptr();
	if (ReadAt((TInt64)pos, dst, (TInt)len) != (TInt)len)
		User::Leave(KErrCorrupt);
	iMoov->Des().SetLength(len);
	r = iMovie.Parse(iMoov->Ptr(), len);
	if (r != mp4::KOk)
		User::Leave(r == mp4::KErrNoVideo ? KErrNotSupported : KErrCorrupt);
	iMaxSample = 0;
	iSamples.Reset();
	r = iMovie.Walk(SampleCb, this);
	if (r != mp4::KOk || iSamples.Count() != (TInt)iMovie.iSampleCount)
		User::Leave(KErrCorrupt);
	if (iMaxSample > KMaxSample)
		User::Leave(KErrTooBig);
	iMaxASample = 0;
	iASamples.Reset();
	if (iMovie.iAudioOk)
		{
		if (iMovie.WalkAudio(AudioSampleCb, this) != mp4::KOk || iASamples.Count() != (TInt)iMovie.iAudioSampleCount ||
			iMaxASample > KMaxSample || iMaxASample <= 0)
			{
			RDebug::Printf("PH: vc audio track unusable (%d samples, max %d): video only", iASamples.Count(), iMaxASample);
			iMovie.iAudioOk = false;
			iASamples.Reset();
			}
		else
			{
			iARaw = HBufC8::NewL(iMaxASample);
			iPcm = HBufC8::NewL(KPcmMaxFrames * 2 * 2);
			}
		}
	const TUint32 ps = iMovie.ParameterSetsSize();
	if (!ps)
		User::Leave(KErrCorrupt);
	iPsets = HBufC8::NewL(ps);
	iMovie.ParameterSets((TUint8*)iPsets->Ptr(), ps);
	iPsets->Des().SetLength(ps);
	iConv = HBufC8::NewL(iMaxSample > 0 ? iMaxSample : 1);
	const TInt64 durUs = (TInt64)iMovie.iDuration * 1000000 / (TInt64)iMovie.iTimescale;
	iFramePeriodUs = iSamples.Count() > 0 && durUs > 0 ? (TInt)(durUs / iSamples.Count()) : 40000;
	Mem::FillZ(&iStats, sizeof(iStats));
	iStats.iFrameW = iMovie.iWidth;
	iStats.iFrameH = iMovie.iHeight;
	iStats.iSamples = iSamples.Count();
	iStats.iDurationMs = (TInt)(durUs / 1000);
	iStats.iHasAudio = iMovie.iHasAudio;
	RDebug::Printf("PH: vc open %dx%d, %d samples, %d ms, nal length %d, max sample %d, %d b of parameter sets",
		iMovie.iWidth, iMovie.iHeight, iSamples.Count(), iStats.iDurationMs, iMovie.iNalLenSize, iMaxSample, ps);
	RDebug::Printf("PH: vc audio track %d (entry %x): aac %d, %d packets, %d Hz x%d ch (entry), object type %x, asc %d b, %d bit/s",
		iMovie.iHasAudio, iMovie.iAudioFourCC, iMovie.iAudioOk, iASamples.Count(), iMovie.iAudioRate, iMovie.iAudioChannels,
		iMovie.iAudioObjectType, iMovie.iAscLen, iMovie.iAudioAvgBitrate);
	}

void CPheVidCtl::AddDataSourceL(MDataSource& aDataSource)
	{
	if (iClip)
		User::Leave(KErrAlreadyExists);
	if (aDataSource.DataSourceType() != KUidMmfFileSource)
		User::Leave(KErrNotSupported);
	iClip = static_cast<CMMFFile*>(&aDataSource);
	User::LeaveIfError(iClip->SourceThreadLogon(*iEventHandler));
	iClip->SourcePrimeL();
	OpenClipL();
	SendEvent(KMMFEventCategoryVideoOpenComplete, KErrNone);
	}

void CPheVidCtl::AddDataSinkL(MDataSink& aDataSink)
	{
	if (iAudioOut)
		User::Leave(KErrAlreadyExists);
	if (aDataSink.DataSinkType() != KUidMmfAudioOutput)
		User::Leave(KErrNotSupported);
	iAudioOut = static_cast<MMMFAudioOutput*>(&aDataSink);
	User::LeaveIfError(iAudioOut->SinkThreadLogon(*iEventHandler));
	}

void CPheVidCtl::RemoveDataSourceL(MDataSource& aDataSource)
	{
	if (!iClip)
		User::Leave(KErrNotReady);
	if (iClip != &aDataSource)
		User::Leave(KErrArgument);
	StopInternal();
	iClip = NULL;
	}

void CPheVidCtl::RemoveDataSinkL(MDataSink& aDataSink)
	{
	if (!iAudioOut)
		User::Leave(KErrNotReady);
	if (aDataSink.DataSinkType() != KUidMmfAudioOutput)
		User::Leave(KErrNotSupported);
	StopAudio();
	iAudioReady = EFalse;
	iDevSound = NULL;
	iAudioOut = NULL;
	}

void CPheVidCtl::ResetL()
	{
	StopInternal();
	iClip = NULL;
	iDevSound = NULL;
	iAudioOut = NULL;
	}

// --------------------------------------------------------------------------------------------------------------------
// prepare / prime: DevVideo initialisation

void CPheVidCtl::MvpcPrepare()
	{
	if (iPrepared)
		{
		SendEvent(KMMFEventCategoryVideoPrepareComplete, KErrNone);
		return;
		}
	iPrepareEvent = ETrue;
	if (!iPreparing)
		{
		TRAPD(err, PrepareL());
		if (err != KErrNone)
			CompleteInit(err);
		}
	}

void CPheVidCtl::PrimeL(TMMFMessage& aMessage)
	{
	if (!iClip)
		User::Leave(KErrNotReady);
	if (iPrepared)
		{
		aMessage.Complete(KErrNone);
		return;
		}
	if (iPrimeMsg)
		User::Leave(KErrInUse);
	iPrimeMsg = new (ELeave) CMessageHolder(aMessage);
	if (!iPreparing)
		{
		TRAPD(err, PrepareL());
		if (err != KErrNone)
			CompleteInit(err);
		}
	}

void CPheVidCtl::PrimeL()
	{
	User::Leave(KErrNotSupported);			// (only the message form is used)
	}

void CPheVidCtl::PrepareL()
	{
	delete iDvp;
	iDvp = NULL;
	iDvp = CMMFDevVideoPlay::NewL(*this);
	RArray<TUid> decoders;
	CleanupClosePushL(decoders);
	iDvp->FindDecodersL(KMimeH264, 0, decoders, EFalse);
	RDebug::Printf("PH: vc devvideo decoders for H.264: %d", decoders.Count());
	TBool ok = EFalse;
	for (TInt i = 0; i < decoders.Count() && !ok; ++i)
		{
		iHw = iDvp->SelectDecoderL(decoders[i]);
		RArray<TUncompressedVideoFormat> fmts;
		CleanupClosePushL(fmts);
		iDvp->GetOutputFormatListL(iHw, fmts);
		for (TInt f = 0; f < fmts.Count() && !ok; ++f)
			if (fmts[f].iDataFormat == EYuvRawData)			// phevdec: NV12 (YUV 4:2:0 semi-planar)
				{
				iDvp->SetOutputFormatL(iHw, fmts[f]);
				ok = ETrue;
				}
		CleanupStack::PopAndDestroy(&fmts);
		if (ok)
			RDebug::Printf("PH: vc decoder %x selected", decoders[i].iUid);
		}
	CleanupStack::PopAndDestroy(&decoders);
	if (!ok)
		User::Leave(KErrNotFound);
	CCompressedVideoFormat* fmt = CCompressedVideoFormat::NewL(KMimeH264);
	CleanupStack::PushL(fmt);
	iDvp->SetInputFormatL(iHw, *fmt, EDuCodedPicture, EDuElementaryStream, ETrue);
	CleanupStack::PopAndDestroy(fmt);
	iPreparing = ETrue;
	iNext = 0;
	iInputEnd = iStreamEnd = iFinishing = EFalse;
	iVideoInitDone = EFalse;
	iVideoInitErr = KErrNone;
	iAudioReady = iAudioInitPending = EFalse;
	if (iMovie.iAudioOk && iAudioOut)
		{
		TRAPD(ae, PrepareAudioL());
		if (ae != KErrNone)
			{
			iAudioInitPending = EFalse;
			AudioFailed(ae);
			}
		}
	iDvp->Initialize();							// (MdvpoInitComplete may arrive before this returns)
	}

void CPheVidCtl::CompleteInit(TInt aError)
	{
	iPreparing = EFalse;
	iPrepared = aError == KErrNone;
	RDebug::Printf("PH: vc init complete r=%d", aError);
	if (iPrepareEvent)
		{
		iPrepareEvent = EFalse;
		SendEvent(KMMFEventCategoryVideoPrepareComplete, aError);
		}
	if (iPrimeMsg)
		{
		iPrimeMsg->Complete(aError);
		delete iPrimeMsg;
		iPrimeMsg = NULL;
		}
	}

void CPheVidCtl::MdvpoInitComplete(TInt aError)
	{
	iVideoInitDone = ETrue;
	iVideoInitErr = aError;
	TryCompleteInit();
	}

// DevVideo and (when used) DevSound both initialised
void CPheVidCtl::TryCompleteInit()
	{
	if (iPreparing && iVideoInitDone && !iAudioInitPending)
		CompleteInit(iVideoInitErr);
	}

// --------------------------------------------------------------------------------------------------------------------
// audio: pheffmpeg.dll (AAC) -> PCM16 -> the DevSound of the client's audio output sink. The audio clock (DevSound's play
// position) paces the video. Any audio error drops the audio and the video plays on against the wall clock.

static TInt RateEnum(TInt aHz)
	{
	switch (aHz)
		{
	case 8000: return EMMFSampleRate8000Hz;
	case 11025: return EMMFSampleRate11025Hz;
	case 12000: return EMMFSampleRate12000Hz;
	case 16000: return EMMFSampleRate16000Hz;
	case 22050: return EMMFSampleRate22050Hz;
	case 24000: return EMMFSampleRate24000Hz;
	case 32000: return EMMFSampleRate32000Hz;
	case 44100: return EMMFSampleRate44100Hz;
	case 48000: return EMMFSampleRate48000Hz;
	case 64000: return EMMFSampleRate64000Hz;
	case 88200: return EMMFSampleRate88200Hz;
	case 96000: return EMMFSampleRate96000Hz;
	default: return 0;
		}
	}

void CPheVidCtl::CloseAac()
	{
	if (iAac && iAacClose)
		iAacClose(iAac);
	iAac = NULL;
	}

void CPheVidCtl::PrepareAudioL()
	{
	iDevSound = &iAudioOut->SoundDevice();
	if (!iFfLoaded)
		{
		User::LeaveIfError(iFf.Load(_L("pheffmpeg.dll")));
		iFfLoaded = ETrue;
		}
	iAacOpen = (TAacOpen)iFf.Lookup(2);
	iAacDecode = (TAacDecode)iFf.Lookup(3);
	iAacClose = (TAacClose)iFf.Lookup(4);
	iAacSetOut = (TAacSetOut)iFf.Lookup(5);
	if (!iAacOpen || !iAacDecode || !iAacClose || !iAacSetOut)
		User::Leave(KErrNotFound);
	// probe: the real output rate / channels (HE-AAC doubles the rate) from the first access unit(s)
	void* h = iAacOpen(iMovie.iAsc, (TInt)iMovie.iAscLen);
	if (!h)
		User::Leave(KErrNotSupported);
	TInt rate = 0, ch = 0, n = 0;
	for (TInt i = 0; i < Min(4, iASamples.Count()) && n <= 0; ++i)
		{
		const TSampleRec& r = iASamples[i];
		if (ReadAt(r.iOffset, (TUint8*)iARaw->Ptr(), (TInt)r.iSize) != (TInt)r.iSize)
			break;
		n = iAacDecode(h, iARaw->Ptr(), (TInt)r.iSize, (TInt16*)iPcm->Ptr(), KPcmMaxFrames, &rate, &ch);
		}
	iAacClose(h);
	RDebug::Printf("PH: vc audio probe: %d frames, source %d Hz x%d -> output %d Hz x%d (swresample)", n, rate, ch, KOutRate, KOutCh);
	if (n <= 0 || rate < 8000 || (ch != 1 && ch != 2))
		User::Leave(KErrNotSupported);
	iARateEnum = EMMFSampleRate48000Hz;
	iARate = KOutRate;							// A/V sync math runs on the OUTPUT rate
	iACh = KOutCh;
	iDevSound->SetPrioritySettings(iPriority);
	iAudioInitPending = ETrue;
	iDevSound->InitializeL(*this, EMMFStatePlaying);
	}

void CPheVidCtl::InitializeComplete(TInt aError)
	{
	TInt err = aError;
	if (err == KErrNone)
		{
		TMMFCapabilities caps;
		caps.iRate = iARateEnum;
		caps.iChannels = EMMFStereo;
		caps.iEncoding = EMMFSoundEncoding16BitPCM;
		TRAP(err, iDevSound->SetConfigL(caps));
		if (err == KErrNone)
			{
			iDevSound->SetVolume(iDevSound->MaxVolume());
			iAudioReady = ETrue;
			}
		}
	RDebug::Printf("PH: vc audio devsound init %d (config %d Hz x%d)", err, iARate, iACh);
	iAudioInitPending = EFalse;
	if (err != KErrNone)
		AudioFailed(err);
	TryCompleteInit();
	}

void CPheVidCtl::AudioFailed(TInt aError)
	{
	RDebug::Printf("PH: vc audio dropped, error %d (video continues)", aError);
	if (!iStats.iAudioErr)
		iStats.iAudioErr = aError;
	iAudioReady = EFalse;
	if (iAudioActive)
		iNeedResync = ETrue;
	iAudioActive = EFalse;
	iAudioDone = ETrue;
	if (iAudioStarted && iDevSound)
		iDevSound->Stop();
	iAudioStarted = EFalse;
	CloseAac();
	if (iPlaying)
		CheckFinished();
	}

void CPheVidCtl::StartAudioL()
	{
	iAac = iAacOpen(iMovie.iAsc, (TInt)iMovie.iAscLen);
	if (!iAac)
		User::Leave(KErrNotSupported);
	if (iAacSetOut(iAac, KOutRate, KOutCh) != 0)
		User::Leave(KErrNotSupported);
	iPcmOff = iPcmLen = 0;
	iANext = 0;
	iSkipFrames = (TInt)(iMovie.iAudioSkip * (TUint64)iARate / iMovie.iAudioTimescale);
	iAudioDone = EFalse;
	iDevSound->PlayInitL();
	iAudioStarted = ETrue;
	}

void CPheVidCtl::StopAudio()
	{
	if (iAudioStarted && iDevSound)
		iDevSound->Stop();
	iAudioStarted = EFalse;
	iAudioActive = EFalse;
	CloseAac();
	}

// Fills aDst (a DevSound buffer) with decoded PCM; aLast = the stream's last data is in it.
TInt CPheVidCtl::FillPcmL(TDes8& aDst, TBool& aLast)
	{
	aDst.SetLength(0);
	const TInt frameBytes = 4;					// always interleaved stereo S16
	const TInt cap = aDst.MaxLength() - aDst.MaxLength() % frameBytes;
	while (aDst.Length() < cap)
		{
		if (iPcmOff < iPcmLen)
			{
			const TInt n = Min(cap - aDst.Length(), iPcmLen - iPcmOff);
			aDst.Append(iPcm->Ptr() + iPcmOff, n);
			iPcmOff += n;
			continue;
			}
		if (iANext >= iASamples.Count())
			break;
		const TSampleRec& r = iASamples[iANext++];
		if (ReadAt(r.iOffset, (TUint8*)iARaw->Ptr(), (TInt)r.iSize) != (TInt)r.iSize)
			User::Leave(KErrCorrupt);
		TInt rate = 0, ch = 0;
		const TInt n = iAacDecode(iAac, iARaw->Ptr(), (TInt)r.iSize, (TInt16*)iPcm->Ptr(), KPcmMaxFrames, &rate, &ch);
		if (n < 0)
			{
			RDebug::Printf("PH: vc audio packet %d decode error %d (skipped)", iANext - 1, n);
			continue;
			}
		if (n == 0 || ch != iACh || rate != iARate)
			continue;
		++iStats.iAudioPackets;
		iPcmOff = 0;
		iPcmLen = n * frameBytes;
		if (iSkipFrames > 0)					// encoder priming (the edit list's start): not played
			{
			const TInt k = Min(iSkipFrames, n);
			iSkipFrames -= k;
			iPcmOff = k * frameBytes;
			}
		}
	aLast = iANext >= iASamples.Count() && iPcmOff >= iPcmLen;
	return KErrNone;
	}

void CPheVidCtl::BufferToBeFilled(CMMFBuffer* aBuffer)
	{
	if (!CMMFBuffer::IsSupportedDataBuffer(aBuffer->Type()))		// DevSound hands a KUidMmfPtrBuffer (boot 630)
		{
		RDebug::Printf("PH: vc audio BufferToBeFilled: buffer type %x is not a data buffer", aBuffer->Type().iUid);
		AudioFailed(KErrNotSupported);
		return;
		}
	CMMFDataBuffer* db = static_cast<CMMFDataBuffer*>(aBuffer);
	if (!iAudioActive || !iAac)
		{
		db->Data().SetLength(0);
		aBuffer->SetLastBuffer(ETrue);
		iDevSound->PlayData();
		return;
		}
	TBool last = EFalse;
	TRAPD(e, FillPcmL(db->Data(), last));
	if (e != KErrNone)
		{
		RDebug::Printf("PH: vc audio FillPcmL left %d", e);
		AudioFailed(e);
		return;
		}
	iStats.iAudioPcmFrames += db->Data().Length() / 4;
	++iStats.iAudioBuffers;
	aBuffer->SetLastBuffer(last);
	iDevSound->PlayData();
	}

void CPheVidCtl::PlayError(TInt aError)
	{
	const TBool atEnd = iANext >= iASamples.Count() && iPcmOff >= iPcmLen;
	if (aError == KErrNone || (aError == KErrUnderflow && atEnd))
		{
		iAudioDone = ETrue;
		iAudioStarted = EFalse;
		RDebug::Printf("PH: vc audio end (%d pcm frames in %d buffers)", iStats.iAudioPcmFrames, iStats.iAudioBuffers);
		if (iPlaying)
			CheckFinished();
		}
	else
		{
		RDebug::Printf("PH: vc audio PlayError %d (anext %d of %d, buffers %d)", aError, iANext, iASamples.Count(), iStats.iAudioBuffers);
		AudioFailed(aError);
		}
	}

// The audio clock in us since the audio started: DevSound's play position, extrapolated by the wall clock between its updates.
// EFalse until the position has started to advance (or when audio is not in use).
TBool CPheVidCtl::AudioClockUs(TInt64& aUs)
	{
	if (!iAudioActive || iAudioFallback || !iDevSound)
		return EFalse;
	TInt64 played = 0;
	if (iClockMode == 1)
		{
		TTimeIntervalMicroSeconds t(0);
		if (iDevSound->GetTimePlayed(t) == KErrNone)
			played = t.Int64();
		else
			iClockMode = 2;
		}
	if (iClockMode == 2)
		played = (TInt64)iDevSound->SamplesPlayed() * 1000000 / iARate;
	const TInt64 now = NowUs();
	if (played != iLastPlayed)
		{
		iLastPlayed = played;
		iLastPlayedWall = now;
		if (played > 0 && !iClockSeen)
			{
			iClockSeen = ETrue;
			iClockFirstVal = played;
			iClockFirstWall = now;
			}
		}
	if (!iClockSeen)
		return EFalse;
	TInt64 c = iLastPlayed + (now - iLastPlayedWall);
	if (c < iLastClock)
		c = iLastClock;
	iLastClock = c;
	aUs = c;
	return ETrue;
	}

// --------------------------------------------------------------------------------------------------------------------
// play / stop

void CPheVidCtl::PlayL()
	{
	if (!iPrepared || !iDvp)
		User::Leave(KErrNotReady);
	if (iPlaying)
		User::Leave(KErrInUse);
	iPlaying = ETrue;
	iNext = 0;
	iQHead = iQCount = 0;
	iHaveTs0 = EFalse;
	iPosUs = 0;
	iLateSumUs = 0;
	iError = KErrNone;
	const TBool bench = iStats.iBench;
	Mem::FillZ(&iStats, sizeof(iStats));
	iStats.iBench = bench;
	iStats.iFrameW = iMovie.iWidth;
	iStats.iFrameH = iMovie.iHeight;
	iStats.iSamples = iSamples.Count();
	iStats.iDurationMs = (TInt)((TInt64)iMovie.iDuration * 1000 / (TInt64)iMovie.iTimescale);
	iStats.iHasAudio = iMovie.iHasAudio;
	iDriftSumUs = 0;
	iDriftN = 0;
	iUseA = iNeedResync = iAudioFallback = iClockSeen = EFalse;
	iLastPlayed = iLastPlayedWall = iLastClock = 0;
	iAudioDone = EFalse;
	iAudioActive = EFalse;
	iPlayStartUs = NowUs();
	if (iAudioReady && !iBench && iDevSound)
		{
		iAudioActive = ETrue;
		iClockMode = iDevSound->IsGetTimePlayedSupported() ? 1 : 2;
		TRAPD(ae, StartAudioL());
		if (ae != KErrNone)
			{
			iAudioActive = EFalse;
			AudioFailed(ae);
			}
		else
			{
			iStats.iAudioOn = 1;
			iStats.iAudioRate = iARate;
			iStats.iAudioCh = iACh;
			iStats.iAudioClock = iClockMode;
			}
		}
	else
		iAudioDone = ETrue;
	PrimeBitmap();
	iDvp->Start();
	RDebug::Printf("PH: vc play: bench %d, region %d rects, screen rect (%d,%d)-(%d,%d)", iBench, iDerived.Count(),
		iScreenRect.iTl.iX, iScreenRect.iTl.iY, iScreenRect.iBr.iX, iScreenRect.iBr.iY);
	Feed();
	}

void CPheVidCtl::PauseL()
	{
	User::Leave(KErrNotSupported);
	}

void CPheVidCtl::StopL()
	{
	StopInternal();
	}

// Stops everything and drops the DevVideo instance (a fresh Prime re-initialises). Must not run inside a DevVideo callback.
void CPheVidCtl::StopInternal()
	{
	iFeeder->Cancel();
	iFinisher->Cancel();
	iPresenter->Cancel();
	StopAudio();
	CloseBitmap();
	OverlayOff();
	if (iDvp)
		{
		for (TInt i = 0; i < iQCount; ++i)
			iDvp->ReturnPicture(iQ[(iQHead + i) % KQueue]);
		if (iPlaying)
			iDvp->Stop();
		delete iDvp;
		iDvp = NULL;
		}
	iQCount = iQHead = 0;
	iPlaying = iPrepared = iPreparing = iFinishing = EFalse;
	iPumping = EFalse;
	iNext = 0;
	}

TTimeIntervalMicroSeconds CPheVidCtl::PositionL() const
	{
	if (!iClip)
		return TTimeIntervalMicroSeconds(0);
	return TTimeIntervalMicroSeconds(iPosUs);
	}

void CPheVidCtl::SetPositionL(const TTimeIntervalMicroSeconds&)
	{
	User::Leave(KErrNotSupported);				// (no seeking yet)
	}

TTimeIntervalMicroSeconds CPheVidCtl::DurationL() const
	{
	if (!iClip || !iMovie.iTimescale)
		User::Leave(KErrNotReady);
	return TTimeIntervalMicroSeconds((TInt64)iMovie.iDuration * 1000000 / (TInt64)iMovie.iTimescale);
	}

void CPheVidCtl::SetPrioritySettings(const TMMFPrioritySettings& aPrioritySettings)
	{
	iPriority = aPrioritySettings;
	if (iDevSound)
		iDevSound->SetPrioritySettings(iPriority);
	}

void CPheVidCtl::GetNumberOfMetaDataEntriesL(TInt&)
	{
	User::Leave(KErrNotSupported);
	}

CMMFMetaDataEntry* CPheVidCtl::GetMetaDataEntryL(TInt)
	{
	User::Leave(KErrNotSupported);
	return NULL;
	}

// the test hook: any message to an interface the parsers do not own
void CPheVidCtl::CustomCommand(TMMFMessage& aMessage)
	{
	if (aMessage.Destination().InterfaceId() != KPheVidCtlCmdIf)
		{
		aMessage.Complete(KErrNotSupported);
		return;
		}
	switch (aMessage.Function())
		{
	case EPheVidGetStats:
		{
		TPheVidStats s = iStats;
		if (iStats.iDelivered)
			s.iLateAvgMs = (TInt)(iLateSumUs / 1000 / iStats.iDelivered);
		if (iDriftN)
			s.iDriftAvgMs = (TInt)(iDriftSumUs / 1000 / iDriftN);
		TPckgC<TPheVidStats> p(s);
		TRAPD(r, aMessage.WriteDataToClientL(p));
		aMessage.Complete(r);
		break;
		}
	case EPheVidSetBench:
		{
		TInt v = 0;
		TPckg<TInt> p(v);
		TRAPD(r, aMessage.ReadData1FromClientL(p));
		if (r == KErrNone)
			iBench = iStats.iBench = v != 0;
		aMessage.Complete(r);
		break;
		}
	default:
		aMessage.Complete(KErrNotSupported);
		}
	}

// --------------------------------------------------------------------------------------------------------------------
// input: file -> Annex B -> DevVideo

// Converts one sample (length-prefixed NAL units) to Annex B; with aWrite, gets a DevVideo buffer and fills it.
// Returns the Annex B length (aBuf = the buffer when aWrite and one was available, else NULL).
TInt CPheVidCtl::FillInputL(const TSampleRec& aRec, TVideoInputBuffer*& aBuf, TBool aWrite)
	{
	const TInt L = iMovie.iNalLenSize;
	const TInt n = (TInt)aRec.iSize;
	TUint8* raw = (TUint8*)iConv->Ptr();
	if (ReadAt(aRec.iOffset, raw, n) != n)
		User::Leave(KErrCorrupt);
	// size
	TInt total = aRec.iSync || iNext == 0 ? iPsets->Length() : 0;
	for (TInt p = 0; p < n;)
		{
		if (p + L > n)
			User::Leave(KErrCorrupt);
		TUint32 len = 0;
		for (TInt i = 0; i < L; ++i)
			len = (len << 8) | raw[p + i];
		p += L;
		if (len > (TUint32)(n - p))
			User::Leave(KErrCorrupt);
		total += 4 + len;
		p += len;
		}
	aBuf = NULL;
	if (!aWrite)
		return total;
	aBuf = iDvp->GetBufferL(total);
	if (!aBuf)
		return total;							// backpressure: MdvpoNewBuffers
	TUint8* d = (TUint8*)aBuf->iData.Ptr();
	TInt o = 0;
	if (aRec.iSync || iNext == 0)
		{
		Mem::Copy(d, iPsets->Ptr(), iPsets->Length());
		o = iPsets->Length();
		}
	for (TInt p = 0; p < n;)
		{
		TUint32 len = 0;
		for (TInt i = 0; i < L; ++i)
			len = (len << 8) | raw[p + i];
		p += L;
		d[o] = d[o + 1] = d[o + 2] = 0;
		d[o + 3] = 1;
		Mem::Copy(d + o + 4, raw + p, len);
		o += 4 + len;
		p += len;
		}
	aBuf->iData.SetLength(o);
	aBuf->iOptions = TVideoInputBuffer::EPresentationTimestamp;
	aBuf->iPresentationTimestamp = TTimeIntervalMicroSeconds(aRec.iPtsUs);
	return o;
	}

void CPheVidCtl::Feed()
	{
	if (!iPlaying || !iDvp || iInputEnd || iError != KErrNone)
		return;
	for (TInt burst = 0; burst < KFeedBurst; ++burst)
		{
		if (iNext >= iSamples.Count())
			{
			iInputEnd = ETrue;
			iDvp->InputEnd();
			RDebug::Printf("PH: vc input end after %d samples", iNext);
			return;
			}
		TVideoInputBuffer* b = NULL;
		TInt r = KErrNone;
		TRAP(r, FillInputL(iSamples[iNext], b, ETrue));
		if (r != KErrNone)
			{
			FailPlayback(r);
			return;
			}
		if (!b)
			return;							// MdvpoNewBuffers resumes
		TRAP(r, iDvp->WriteCodedDataL(b));
		if (r != KErrNone)
			{
			FailPlayback(r);
			return;
			}
		++iNext;
		++iStats.iFed;
		}
	iFeeder->CallBack();					// more later: let the decoder steps and the presenter run
	}

void CPheVidCtl::MdvpoNewBuffers()
	{
	Feed();
	}

// --------------------------------------------------------------------------------------------------------------------
// output: pictures -> timing -> screen

void CPheVidCtl::PullPictures()
	{
	while (iQCount < KQueue && iDvp)
		{
		TVideoPicture* pic = NULL;
		TRAPD(r, pic = iDvp->NextPictureL());
		if (r != KErrNone || !pic)
			return;
		iQ[(iQHead + iQCount) % KQueue] = pic;
		++iQCount;
		++iStats.iDelivered;
		}
	}

void CPheVidCtl::MdvpoNewPictures()
	{
	Pump();
	}

void CPheVidCtl::MdvpoStreamEnd()
	{
	RDebug::Printf("PH: vc stream end (delivered %d, fed %d)", iStats.iDelivered, iStats.iFed);
	iStreamEnd = ETrue;
	Pump();
	}

// Presents the queued pictures in turn: wait for the due time, draw, wait for the vsync; stops when the queue is empty.
void CPheVidCtl::Pump()
	{
	if (iPumping || iPresenter->IsActive() || !iPlaying || iFinishing)
		return;
	iPumping = ETrue;
	for (;;)
		{
		PullPictures();
		if (!iQCount)
			break;
		const TVideoPicture* pic = iQ[iQHead];
		const TInt64 now = NowUs();
		if (!iHaveTs0)
			{
			iHaveTs0 = ETrue;
			iTs0Us = pic->iTimestamp.Int64();
			iClockStartUs = now;
			}
		const TInt64 rel = pic->iTimestamp.Int64() - iTs0Us;
		TInt64 aud = 0;
		TBool useA = EFalse;
		if (!iBench && iAudioActive && !iAudioFallback)
			{
			useA = AudioClockUs(aud);			// the audio clock is the master once DevSound's position advances
			if (!useA)
				{
				if (now - iPlayStartUs > KAudioStartWaitUs)
					{
					iAudioFallback = ETrue;
					iNeedResync = ETrue;
					RDebug::Printf("PH: vc audio clock never started: wall clock pacing");
					}
				else
					{
					iPresenter->After(10000);		// hold the picture until the sound is running
					break;
					}
				}
			}
		if (iNeedResync && !useA)
			{
			iClockStartUs = now - rel;
			iNeedResync = EFalse;
			}
		iUseA = useA;
		const TInt64 waitUs = useA ? rel - aud : (iClockStartUs + rel) - now;
		iDueUs = now + waitUs;
		if (!iBench && waitUs > 0)
			{
			iPresenter->After((TInt)Min(waitUs, useA ? (TInt64)50000 : (TInt64)2000000));
			break;
			}
		if (PresentOne())
			break;							// waiting for the vsync
		}
	iPumping = EFalse;
	CheckFinished();
	}

// Presents (or drops) the head picture; ETrue when the presenter now waits for the vsync.
TBool CPheVidCtl::PresentOne()
	{
	TVideoPicture* pic = iQ[iQHead];
	iQHead = (iQHead + 1) % KQueue;
	--iQCount;
	const TInt64 now = NowUs();
	const TInt64 lag = iBench ? 0 : now - iDueUs;
	iPosUs = pic->iTimestamp.Int64() - iTs0Us;
	if (lag > 0)
		{
		iLateSumUs += lag;
		const TInt ms = (TInt)(lag / 1000);
		if (ms > iStats.iLateMaxMs)
			iStats.iLateMaxMs = ms;
		}
	iStats.iElapsedMs = (TInt)((now - iClockStartUs) / 1000);
	if (iUseA && !iBench)
		{
		iDriftSumUs += lag;
		++iDriftN;
		const TInt dm = (TInt)((lag < 0 ? -lag : lag) / 1000);
		if (dm > iStats.iDriftMaxMs)
			iStats.iDriftMaxMs = dm;
		}
	TBool vsync = EFalse;
	if (!iBench)
		{
		if (lag > 3 * iFramePeriodUs)
			++iStats.iDropped;
		else if (Draw(*pic))
			{
			++iStats.iDrawn;
			vsync = ETrue;
			}
		else
			++iStats.iNoRegion;
		}
	if (vsync && iOvPosted)
		{
		// the plane scans this picture until the next flip: it stays held (iShown); the one it replaced is returned when
		// the flip just issued by OverlayShow completes (PresenterDone)
		iOvPosted = EFalse;
		return ETrue;
		}
	iDvp->ReturnPicture(pic);				// (the pixels are consumed: Draw reads them synchronously)
	if (vsync)
		{
		iVsyncT0 = User::FastCounter();
		iBmp->EndUpdate(iPresenter->Vsync());
		iPresenter->Armed();
		}
	return vsync;
	}

void CPheVidCtl::CheckFinished()
	{
	if (iStreamEnd && iPlaying && !iFinishing && !iQCount && !iPresenter->IsActive() && (!iAudioActive || iAudioDone))
		{
		iFinishing = ETrue;
		iFinisher->CallBack();
		}
	}

TInt CPheVidCtl::FinishCb(TAny* aSelf)
	{
	((CPheVidCtl*)aSelf)->FinishPlayback();
	return 0;
	}

void CPheVidCtl::FinishPlayback()
	{
	iStats.iElapsedMs = (TInt)((NowUs() - iClockStartUs) / 1000);
	if (iStats.iDelivered)
		iStats.iLateAvgMs = (TInt)(iLateSumUs / 1000 / iStats.iDelivered);
	{
	TInt f = 0;
	HAL::Get(HAL::EFastCounterFrequency, f);
	RDebug::Printf("PH: vc stages (FastCounter ticks at %d Hz; %d drawn, %d vsync waits): draw total %u = BeginUpdate %u + NV12->RGB convert %u (+ region/setup %u); vsync wait (EndUpdate -> flip) %u",
		f, iNDraw, iNVsync, iTDraw, iTBegin, iTConv, iTDraw - iTBegin - iTConv, iTVsync);
	iTBegin = iTConv = iTDraw = iNDraw = iTVsync = iNVsync = 0;
	}
	RDebug::Printf("PH: vc done: fed %d delivered %d drawn %d dropped %d no-region %d, %d ms, late avg %d max %d ms",
		iStats.iFed, iStats.iDelivered, iStats.iDrawn, iStats.iDropped, iStats.iNoRegion, iStats.iElapsedMs,
		iStats.iLateAvgMs, iStats.iLateMaxMs);
	if (iDriftN)
		iStats.iDriftAvgMs = (TInt)(iDriftSumUs / 1000 / iDriftN);
	if (iStats.iAudioOn)
		{
		TInt64 played = iLastPlayed;
		if (iClockMode == 1 && iDevSound)
			{
			TTimeIntervalMicroSeconds t(0);
			if (iDevSound->GetTimePlayed(t) == KErrNone)
				played = t.Int64();
			}
		else if (iDevSound)
			played = (TInt64)iDevSound->SamplesPlayed() * 1000000 / iARate;
		iStats.iAudioPlayedMs = (TInt)(played / 1000);
		TInt wallDrift = 0;
		if (iClockSeen && iLastPlayedWall - iClockFirstWall > 500000)
			wallDrift = (TInt)(((iLastPlayed - iClockFirstVal) - (iLastPlayedWall - iClockFirstWall)) / 1000);
		RDebug::Printf("PH: vc audio: %d Hz x%d, %d packets, %d pcm frames (%d ms) in %d buffers, played %d ms, clock mode %d, "
			"A/V drift avg %d max %d ms (%d pictures), audio-vs-wall %d ms, err %d",
			iARate, iACh, iStats.iAudioPackets, iStats.iAudioPcmFrames, (TInt)((TInt64)iStats.iAudioPcmFrames * 1000 / iARate),
			iStats.iAudioBuffers, iStats.iAudioPlayedMs, iStats.iAudioClock, iStats.iDriftAvgMs, iStats.iDriftMaxMs, iDriftN,
			wallDrift, iStats.iAudioErr);
		}
	const TInt err = iError;
	StopInternal();
	SendEvent(KMMFEventCategoryPlaybackComplete, err);
	}

void CPheVidCtl::FailPlayback(TInt aError)
	{
	RDebug::Printf("PH: vc playback error %d", aError);
	iError = iStats.iError = aError;
	iInputEnd = ETrue;
	iStreamEnd = ETrue;
	iFeeder->Cancel();
	while (iQCount)
		{
		iDvp->ReturnPicture(iQ[iQHead]);
		iQHead = (iQHead + 1) % KQueue;
		--iQCount;
		}
	iFinishing = ETrue;
	iFinisher->CallBack();
	}

void CPheVidCtl::MdvpoFatalError(TInt aError)
	{
	// (not safe to delete DevVideo inside this callback: FailPlayback only schedules the stop)
	FailPlayback(aError);
	}

void CPheVidCtl::MdvpoReturnPicture(TVideoPicture*) {}
void CPheVidCtl::MdvpoSupplementalInformation(const TDesC8&, const TTimeIntervalMicroSeconds&, const TPictureId&) {}
void CPheVidCtl::MdvpoPictureLoss() {}
void CPheVidCtl::MdvpoPictureLoss(const TArray<TPictureId>&) {}
void CPheVidCtl::MdvpoSliceLoss(TUint, TUint, const TPictureId&) {}
void CPheVidCtl::MdvpoReferencePictureSelection(const TDesC8&) {}
void CPheVidCtl::MdvpoTimedSnapshotComplete(TInt, TPictureData*, const TTimeIntervalMicroSeconds&, const TPictureId&) {}

// --------------------------------------------------------------------------------------------------------------------
// display: the client's DSA region (MvpcSetDisplayWindowL / UpdateDisplayRegionL / DirectScreenAccessEventL)

void CPheVidCtl::UpdateClip()
	{
	iDerived.Copy(iDisplayRegion);
	iDerived.ClipRect(iClipRect);
	CloseBitmap();
	}

void CPheVidCtl::CloseBitmap()
	{
	if (iBmp)
		{
		iBmp->Close();
		delete iBmp;
		iBmp = NULL;
		}
	iBmpRect = TRect();
	}

void CPheVidCtl::MvpcSetDisplayWindowL(const TRect& aWindowRect, const TRect& aClipRect)
	{
	iScreenRect = aWindowRect;
	iClipRect = aClipRect;
	iDsaOn = ETrue;
	UpdateClip();
	}

void CPheVidCtl::MvpcUpdateDisplayRegionL(const TRegion& aRegion)
	{
	iDisplayRegion.Copy(aRegion);
	UpdateClip();
	}

void CPheVidCtl::MvpcDirectScreenAccessEventL(const TMMFDSAEvent aDSAEvent)
	{
	if (aDSAEvent == EAbortDSA)
		{
		iDsaOn = EFalse;
		iPresenter->Cancel();
		CloseBitmap();
		OverlayOff();
		}
	else if (aDSAEvent == EResumeDSA)
		iDsaOn = ETrue;
	}

void CPheVidCtl::MvpcRefreshFrameL() {}
void CPheVidCtl::MvpcPlayL(const TTimeIntervalMicroSeconds&, const TTimeIntervalMicroSeconds&) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvpcGetLoadingProgressL(TInt&) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvpcSetRotationL(TVideoRotation) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvpcGetRotationL(TVideoRotation& aRotation) { aRotation = EVideoRotationNone; }
void CPheVidCtl::MvpcSetScaleFactorL(TReal32, TReal32, TBool) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvpcGetScaleFactorL(TReal32& aW, TReal32& aH, TBool& aAa) { aW = aH = 100; aAa = EFalse; }
void CPheVidCtl::MvpcSetCropRegionL(const TRect&) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvpcGetCropRegionL(TRect& aCrop) { aCrop = TRect(0, 0, iMovie.iWidth, iMovie.iHeight); }
void CPheVidCtl::MvpcGetFrameL(MMMFVideoFrameMessage& aMessage) { aMessage.FrameReady(KErrNotSupported); }
void CPheVidCtl::MvpcGetAudioEnabledL(TBool& aEnabled) { aEnabled = iMovie.iAudioOk && iAudioOut && (!iPrepared || iAudioReady || iAudioActive); }
void CPheVidCtl::MvsdSetInitScreenNumber(TInt aScreenNumber) { iScreenNumber = aScreenNumber; }

void CPheVidCtl::MvcGetAudioCodecL(TFourCC&) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvcGetVideoBitRateL(TInt&) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvcGetAudioBitRateL(TInt& aBitRate)
	{
	if (!iMovie.iAudioOk)
		User::Leave(KErrNotSupported);
	aBitRate = (TInt)iMovie.iAudioAvgBitrate;
	}
void CPheVidCtl::MvcSetFrameRateL(TReal32) { User::Leave(KErrNotSupported); }
void CPheVidCtl::MvcGetFrameRateL(TReal32& aFps)
	{
	if (!iClip || iFramePeriodUs <= 0)
		User::Leave(KErrNotReady);
	aFps = (TReal32)1000000.0 / (TReal32)iFramePeriodUs;
	}
void CPheVidCtl::MvcGetVideoFrameSizeL(TSize& aSize)
	{
	if (!iClip)
		User::Leave(KErrNotReady);
	aSize = TSize(iMovie.iWidth, iMovie.iHeight);
	}
void CPheVidCtl::MvcGetVideoMimeTypeL(TDes8& aMimeType) { aMimeType.Copy(KMimeH264); }

// The first Draw used to pay for creating the DSA double-buffer bitmap (a chunk) and the scratch rows while the clock already ran: measured on VirtualBox (stage
// timing, 2026-10-16) about 0.7e9 ticks of "region/setup" = the first frames 300-800 ms late. Done here, before the clock starts, with the movie's own size.
void CPheVidCtl::PrimeBitmap()
	{
	if (iBench || !iDsaOn || iDerived.CheckError() || iDerived.IsEmpty() || iScreenRect.IsEmpty() || iMovie.iWidth <= 0 || iMovie.iHeight <= 0)
		return;
	const TInt sw = iMovie.iWidth, sh = iMovie.iHeight;
	TInt dw = iScreenRect.Width(), dh = dw * sh / sw;
	if (dh > iScreenRect.Height())
		{
		dh = iScreenRect.Height();
		dw = dh * sw / sh;
		}
	if (dw <= 0 || dh <= 0)
		return;
	const TRect dest(TPoint(iScreenRect.iTl.iX + (iScreenRect.Width() - dw) / 2, iScreenRect.iTl.iY + (iScreenRect.Height() - dh) / 2), TSize(dw, dh));
	RRegion vis;
	vis.Copy(iDerived);
	vis.ClipRect(dest);
	if (!vis.CheckError() && !vis.IsEmpty())
		{
		const TRect bound = vis.BoundingRect();
		if (!iBmp || iBmpRect != bound)
			{
			CloseBitmap();
			TRAPD(e, iBmp = CDirectScreenBitmap::NewL());
			if (e == KErrNone)
				e = iBmp->Create(bound, CDirectScreenBitmap::EDoubleBuffer);
			if (e != KErrNone)
				CloseBitmap();
			else
				iBmpRect = bound;
			}
		const TInt pitch = (sw + 15) & ~15;
		if (!iRowY || iRowY->Des().MaxLength() < pitch)
			{
			delete iRowY;
			delete iRowUV;
			iRowY = HBufC8::New(pitch);
			iRowUV = HBufC8::New(pitch);
			}
		if (iXMapLen < dw)
			{
			delete[] iXMap;
			iXMap = new TInt[dw];
			iXMapLen = iXMap ? dw : 0;
			}
		}
	vis.Close();
	}

// NV12 -> the screen, nearest-neighbour scaled to fit (aspect preserved, centred) inside the window rectangle, drawn only
// where the DSA region allows. ETrue when a bitmap update was begun (the caller ends it with EndUpdate).
TBool CPheVidCtl::Draw(const TVideoPicture& aPic)
	{
	const TPictureData& pd = aPic.iData;
	if (!iDsaOn || iDerived.CheckError() || iDerived.IsEmpty() || iScreenRect.IsEmpty() ||
		pd.iDataFormat != EYuvRawData || !pd.iRawData)
		return EFalse;
	const TInt pitch = pd.iDataSize.iWidth, ah = pd.iDataSize.iHeight;
	const TRect crop = aPic.iOptions & TVideoPicture::ECropRect ? aPic.iCropRect : TRect(0, 0, pitch, ah);
	const TInt sw = crop.Width(), sh = crop.Height();
	if (sw <= 0 || sh <= 0)
		return EFalse;
	// fit
	TInt dw = iScreenRect.Width(), dh = dw * sh / sw;
	if (dh > iScreenRect.Height())
		{
		dh = iScreenRect.Height();
		dw = dh * sw / sh;
		}
	if (dw <= 0 || dh <= 0)
		return EFalse;
	const TPoint dtl(iScreenRect.iTl.iX + (iScreenRect.Width() - dw) / 2, iScreenRect.iTl.iY + (iScreenRect.Height() - dh) / 2);
	const TRect dest(dtl, TSize(dw, dh));
	if (OverlayShow(aPic, crop, dest))
		return ETrue;						// scanned out by the DE2 video plane (flip issued, picture held)
	const TUint32 tD0 = User::FastCounter();
	RRegion vis;
	vis.Copy(iDerived);
	vis.ClipRect(dest);
	if (vis.CheckError() || vis.IsEmpty())
		{
		vis.Close();
		return EFalse;
		}
	const TRect bound = vis.BoundingRect();
	if (!iBmp || iBmpRect != bound)
		{
		CloseBitmap();
		TRAPD(e, iBmp = CDirectScreenBitmap::NewL());
		if (e == KErrNone)
			e = iBmp->Create(bound, CDirectScreenBitmap::EDoubleBuffer);
		if (e != KErrNone)
			{
			RDebug::Printf("PH: vc DSA bitmap create (%d,%d)-(%d,%d) r=%d", bound.iTl.iX, bound.iTl.iY, bound.iBr.iX, bound.iBr.iY, e);
			CloseBitmap();
			vis.Close();
			return EFalse;
			}
		iBmpRect = bound;
		}
	TAcceleratedBitmapInfo info;
	const TUint32 tB0 = User::FastCounter();
	const TInt beginR = iBmp->BeginUpdate(info);
	iTBegin += User::FastCounter() - tB0;
	if (beginR != KErrNone || !info.iAddress || (info.iPixelShift != 5 && info.iPixelShift != 4))
		{
		vis.Close();
		return EFalse;
		}
	// scratch rows
	if (!iRowY || iRowY->Des().MaxLength() < pitch)
		{
		delete iRowY;
		delete iRowUV;
		iRowY = iRowUV = NULL;
		iRowY = HBufC8::New(pitch);
		iRowUV = HBufC8::New(pitch);
		if (!iRowY || !iRowUV)
			{
			vis.Close();
			return ETrue;						// (BeginUpdate is balanced by the caller's EndUpdate; nothing drawn)
			}
		}
	if (iXMapLen < dw)
		{
		delete[] iXMap;
		iXMap = new TInt[dw];
		iXMapLen = iXMap ? dw : 0;
		}
	if (!iXMap)
		{
		vis.Close();
		return ETrue;
		}
	for (TInt x = 0; x < dw; ++x)
		iXMap[x] = crop.iTl.iX + x * sw / dw;
	const TUint32 tC0 = User::FastCounter();
	const TUint8* y0 = pd.iRawData->Ptr();
	const TUint8* uv0 = y0 + pitch * ah;
	TUint8* ry = (TUint8*)iRowY->Ptr();
	TUint8* ruv = (TUint8*)iRowUV->Ptr();
	TInt haveSy = -1, haveSuv = -1;
	const TInt bytes = 1 << (info.iPixelShift - 3);
	for (TInt ri = 0; ri < vis.Count(); ++ri)
		{
		const TRect r = vis[ri];
		for (TInt y = r.iTl.iY; y < r.iBr.iY; ++y)
			{
			const TInt sy = crop.iTl.iY + (y - dest.iTl.iY) * sh / dh;
			if (sy != haveSy)
				{
				Mem::Copy(ry, y0 + sy * pitch, pitch);			// one burst read of the uncached picture per row
				haveSy = sy;
				}
			if ((sy >> 1) != haveSuv)
				{
				Mem::Copy(ruv, uv0 + (sy >> 1) * pitch, pitch);
				haveSuv = sy >> 1;
				}
			TUint8* out = info.iAddress + (y - bound.iTl.iY) * info.iLinePitch + (r.iTl.iX - bound.iTl.iX) * bytes;
			for (TInt x = r.iTl.iX; x < r.iBr.iX; ++x, out += bytes)
				{
				const TInt sx = iXMap[x - dest.iTl.iX];
				const TInt c = 298 * (ry[sx] - 16), d = ruv[sx & ~1] - 128, e = ruv[sx | 1] - 128;
				TInt rr = (c + 409 * e + 128) >> 8, g = (c - 100 * d - 208 * e + 128) >> 8, b = (c + 516 * d + 128) >> 8;
				rr = rr < 0 ? 0 : rr > 255 ? 255 : rr;
				g = g < 0 ? 0 : g > 255 ? 255 : g;
				b = b < 0 ? 0 : b > 255 ? 255 : b;
				if (bytes == 4)
					*(TUint32*)out = 0xFF000000 | (rr << 16) | (g << 8) | b;
				else
					*(TUint16*)out = (TUint16)(((rr & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
				}
			}
		}
	const TUint32 tC1 = User::FastCounter();
	iTConv += tC1 - tC0;
	iTDraw += tC1 - tD0;
	++iNDraw;
	vis.Close();
	return ETrue;
	}

// --------------------------------------------------------------------------------------------------------------------
// the DE2 video plane: one hardware scale from the decoded NV12 picture straight to the
// panel, instead of the CPU conversion above. Only when the whole destination rectangle is visible (no dialog over it), the
// UI is portrait, and the kernel has the PheDeOv driver; anything else falls back to the CDirectScreenBitmap path.

void CPresenter::DoCancel()
	{
	iTimer.Cancel();
	iOwner.CancelFlip();
	}

void CPheVidCtl::ReleaseRetired()
	{
	if (iRetired && iDvp)
		iDvp->ReturnPicture(iRetired);
	iRetired = NULL;
	}

void CPheVidCtl::OverlayOff()
	{
	if (iOvOn)
		{
		iOv.CancelFlip();
		iOv.Disable();					// (the driver waits two frames: the plane no longer reads the pictures after this)
		iOvOn = EFalse;
		RDebug::Printf("PH: vc overlay off after %d frames", iOvFrames);
		}
	if (iDvp)
		{
		if (iShown)
			iDvp->ReturnPicture(iShown);
		if (iRetired)
			iDvp->ReturnPicture(iRetired);
		}
	iShown = iRetired = NULL;
	iOvPosted = EFalse;
	}

TBool CPheVidCtl::OverlayShow(const TVideoPicture& aPic, const TRect& aCrop, const TRect& aDest)
	{
	if (iOvState == 2)
		return EFalse;
	if (iOvState == 0)
		{
		const TInt r = iOv.Open();
		iOvState = r == KErrNone ? 1 : 2;
		RDebug::Printf("PH: vc overlay driver %s (r=%d)", r == KErrNone ? "open" : "unavailable, DSA bitmap path", r);
		if (r != KErrNone)
			return EFalse;
		}
	// the whole destination visible?
	RRegion vis;
	vis.Copy(iDerived);
	vis.ClipRect(aDest);
	const TBool whole = !vis.CheckError() && vis.Count() == 1 && vis[0] == aDest;
	vis.Close();
	// the UI is the nHD picture doubled, centred on the panel (a64_display.cpp ProgramSource); landscape screens would need a
	// rotated plane (rung V6b): software path
	TInt sx = 0, sy = 0;
	HAL::Get(HAL::EDisplayXPixels, sx);
	HAL::Get(HAL::EDisplayYPixels, sy);
	TPheDeOvInfo info;
	if (!whole || sx <= 0 || sx >= sy || iOv.GetInfo(info) != KErrNone)
		{
		OverlayOff();
		return EFalse;
		}
	const TInt scale = sx * 2 <= info.iPanelW && sy * 2 <= info.iPanelH ? 2 : 1;
	const TInt offX = (info.iPanelW - sx * scale) / 2, offY = (info.iPanelH - sy * scale) / 2;
	const TPictureData& pd = aPic.iData;
	const TInt pitch = pd.iDataSize.iWidth, ah = pd.iDataSize.iHeight;
	const TInt cx = aCrop.iTl.iX & ~1, cy = aCrop.iTl.iY & ~1;
	TPheDeOvSet s;
	Mem::FillZ(&s, sizeof(s));
	s.iY = pd.iRawData->Ptr() + cy * pitch + cx;
	s.iUV = pd.iRawData->Ptr() + pitch * ah + (cy / 2) * pitch + cx;
	s.iPitch = pitch;
	s.iSrcW = aCrop.Width() & ~1;
	s.iSrcH = aCrop.Height() & ~1;
	s.iDstX = Max(0, offX + aDest.iTl.iX * scale);
	s.iDstY = Max(0, offY + aDest.iTl.iY * scale);
	s.iDstW = (aDest.Width() * scale) & ~1;
	s.iDstH = (aDest.Height() * scale) & ~1;
	s.iFormat = EPheDeOvNV12;
	s.iColor = EPheDeOvAuto;
	s.iAlpha = 255;
	TBool newGeom = !iOvOn || s.iSrcW != iOvGeom.iSrcW || s.iSrcH != iOvGeom.iSrcH || s.iDstX != iOvGeom.iDstX ||
		s.iDstY != iOvGeom.iDstY || s.iDstW != iOvGeom.iDstW || s.iDstH != iOvGeom.iDstH || s.iPitch != iOvGeom.iPitch;
	TInt r = KErrNone;
	if (newGeom)
		{
		r = iOv.Set(s);
		RDebug::Printf("PH: vc overlay set %dx%d -> (%d,%d) %dx%d r=%d", s.iSrcW, s.iSrcH, s.iDstX, s.iDstY, s.iDstW, s.iDstH, r);
		if (r != KErrNone)
			{
			OverlayOff();
			return EFalse;
			}
		iOvGeom = s;
		iOvOn = ETrue;
		}
	TPheDeOvFlip fl;
	fl.iY = s.iY;
	fl.iUV = s.iUV;
	// (the picture handed to this call is the one the plane now scans: it is held until the NEXT one replaced it)
	iOv.Flip(fl, iPresenter->Vsync());
	iPresenter->Armed();
	++iOvFrames;
	if (iRetired && iDvp)
		iDvp->ReturnPicture(iRetired);
	iRetired = iShown;
	iShown = const_cast<TVideoPicture*>(&aPic);
	iOvPosted = ETrue;
	return ETrue;
	}

// --------------------------------------------------------------------------------------------------------------------
// ECOM

const TImplementationProxy ImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KPheVidCtlImplUid, CPheVidCtl::NewL)
	};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
	return ImplementationTable;
	}
