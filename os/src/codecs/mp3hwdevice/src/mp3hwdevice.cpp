// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// mp3hwdevice.cpp -- OURS (audio, docs/phorepoc/audio.md "Codec policy"): the MP3 -> PCM16 HW device of the A3F
// reference adaptation. audiocodec.dll picks a CMMFHwDevice ECom plug-in by FourCC (" MP3" -> " P16"); the release
// configures one (refacladapt/data/HwDev0x1020658FMp3DecDefault.rss, Nokia's implementation UID 0x1020658F) whose
// code never shipped. This is that device, built the reference way (a CMMFSwCodecWrapper around a CMMFSwCodec), and
// its codec is the release's own Aricent MP3 plug-in (arimp3decmmfcodec: a CMMFCodec found by FourCC), over OUR
// decoder library (arimp3decwrapper = minimp3).
// NOT on the file-playback path: the S60 MP3 controller is built with __USE_MP3_CMMFCODEC and decodes in its own
// thread with the same Aricent plug-in, sending PCM to DevSound. This device serves DevSound clients that stream MP3
// themselves (CMMFDevSound / CMdaAudioOutputStream with the " MP3" FourCC). Untested so far.
//
// The one adaptation: CMMFSwCodec::ProcessL must consume a whole source buffer (the play data path has no
// "incomplete"), a CMMFCodec may stop when its destination fills. The sizes guarantee room: a source buffer of
// KMp3SourceSize bytes decodes to at most 96 PCM bytes per MP3 byte (8 kbit/s MPEG-2: 24-byte frames -> 2304 bytes).
#include <ecom/implementationproxy.h>
#include <mmf/server/mmfswcodecwrapper.h>
#include <mmf/server/mmfcodec.h>
#include <mmf/server/mmfdatabuffer.h>
#include <mmf/common/mmffourcc.h>
#include <e32debug.h>

namespace
{
const TUint KMp3SourceSize = 0x400;					// 1 KB of MP3 per DevSound buffer
const TUint KMp3SinkSize = KMp3SourceSize * 96 + 4608;	// worst-case PCM for it + one frame of residue
const TUint32 KUidMp3DecToPcm16 = 0x1020658F;		// = KUidHwCodecMP3DecToPCM16 (refacladapt)

class CMp3SwCodec : public CMMFSwCodec
	{
public:
	~CMp3SwCodec()	{ delete iCodec; }
	void ConstructL()
		{
		iCodec = CMMFCodec::NewL(TFourCC(' ', 'M', 'P', '3'), KMMFFourCCCodePCM16);
		}
	TCodecProcessResult ProcessL(const CMMFBuffer& aSrc, CMMFBuffer& aDst)
		{
		CMMFDataBuffer& src = const_cast<CMMFDataBuffer&>(static_cast<const CMMFDataBuffer&>(aSrc));
		CMMFDataBuffer& dst = static_cast<CMMFDataBuffer&>(aDst);
		const TInt srcLen = src.Data().Length();
		dst.Data().SetLength(0);
		dst.SetPosition(0);
		src.SetPosition(0);
		TCodecProcessResult out;
		out.iCodecProcessStatus = TCodecProcessResult::EDstNotFilled;
		// feed the Aricent codec until it has taken the whole source (it buffers up to a frame internally)
		for (TInt round = 0; round < 64; round++)
			{
			if (srcLen == 0 && !src.LastBuffer())
				break;
			::TCodecProcessResult r;	// the CMMFCodec one (its first five states are CMMFSwCodec's)
			TRAPD(err, r = iCodec->ProcessL(src, dst));
			if (err != KErrNone)		// e.g. KErrArgument: nothing buffered, nothing given
				break;
			src.SetPosition(src.Position() + r.iSrcBytesProcessed);
			dst.SetPosition(dst.Data().Length());
			out.iDstBytesAdded += r.iDstBytesAdded;
			if (r.iStatus == ::TCodecProcessResult::EEndOfData)
				{
				out.iCodecProcessStatus = TCodecProcessResult::EEndOfData;
				break;
				}
			if (r.iStatus == ::TCodecProcessResult::EProcessError)
				break;
			if (r.iStatus != ::TCodecProcessResult::EProcessIncomplete && src.Position() >= srcLen)
				break;					// the source is taken and nothing is pending
			if (dst.Data().Length() >= dst.Data().MaxLength())
				break;					// cannot happen with the sizes above
			}
		dst.SetPosition(0);
		src.SetPosition(0);
		out.iSrcBytesProcessed = srcLen;		// the data path hands a fresh source buffer next
		out.iCodecProcessStatus = src.LastBuffer() ? TCodecProcessResult::EEndOfData
												   : TCodecProcessResult::EProcessComplete;
		return out;
		}
	TUint SourceBufferSize()	{ return KMp3SourceSize; }
	TUint SinkBufferSize()		{ return KMp3SinkSize; }
private:
	CMMFCodec* iCodec;
	};

class CMp3ToPcm16HwDevice : public CMMFSwCodecWrapper
	{
public:
	static CMMFHwDevice* NewL()
		{
		CMp3ToPcm16HwDevice* self = new (ELeave) CMp3ToPcm16HwDevice;
		CleanupStack::PushL(self);
		CMp3SwCodec* codec = new (ELeave) CMp3SwCodec;
		self->iCodec = codec;
		codec->ConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	CMMFSwCodec& Codec()	{ return *iCodec; }
	};

const TImplementationProxy KImplementationTable[] =
	{
	IMPLEMENTATION_PROXY_ENTRY(KUidMp3DecToPcm16, CMp3ToPcm16HwDevice::NewL)
	};
}

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount)
	{
	aTableCount = sizeof(KImplementationTable) / sizeof(TImplementationProxy);
	return KImplementationTable;
	}
