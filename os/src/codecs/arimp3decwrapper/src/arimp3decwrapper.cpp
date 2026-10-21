// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// arimp3decwrapper.cpp -- OURS (audio): the MP3 decoder library the release's
// Aricent MP3 plug-in links (oss.FCL.sf.adapt.codecs.aricent/mp3_dec: only its MMF wrapper and this API's header were
// published, the Aricent decoder itself never was). It implements that API (arimp3decwrapper.h) with minimp3
// (third_party/minimp3, CC0 / public domain; MP3 patents have expired).
//
// The contract, as arimp3decmmfcodec.cpp drives it:
//   SeekSync(buf, len, pos)          KErrNone + the offset of the first frame header in buf, else an error
//   GetFrameInfo(buf, len, flen, i)  the length of the frame AT buf (0 = no valid header) + its format
//   GetMaxFrameInfo(flen, i)         the largest frame + the largest decoded frame (i.iBufferSize, bytes)
//   Decode(src, used, dst, dlen)     one frame: in = bytes available / dst capacity, out = consumed / produced
// Output: interleaved signed 16-bit PCM at the stream's own rate and channel count.
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#define MINIMP3_ONLY_MP3
#include <e32base.h>
#include "arimp3decwrapper.h"
// minimp3 decodes with ~16 KB of scratch on the stack, more than the MMF controller thread has: our copy takes it
// from here instead (third_party/minimp3/README.md). The decoder state is the first member of a TMp3State, so the
// scratch sits right behind it in the same heap cell.
static void* Mp3ScratchFor(void* aDec);
#define MINIMP3_SCRATCH Mp3ScratchFor(dec)
#include "../../../../third_party/minimp3/minimp3.h"

namespace
{
struct TMp3State
	{
	mp3dec_t iDec;					// first: minimp3's `dec` pointer is this struct's address
	mp3dec_scratch_t iScratch;
	};
}

static void* Mp3ScratchFor(void* aDec)
	{
	return &static_cast<TMp3State*>(aDec)->iScratch;
	}

namespace
{
// ISO 11172-3 / 13818-3 header tables (Layer III)
const TInt KBitrates[2][15] =		// [MPEG-1 = 0 | MPEG-2/2.5 = 1][index] kbit/s
	{
	{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },
	{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 }
	};
const TInt KRates[3] = { 44100, 48000, 32000 };	// MPEG-1; MPEG-2 = /2, MPEG-2.5 = /4
const TInt KMaxFrameBytes = 1441;					// 320 kbit/s at 32 kHz + padding
const TInt KMaxFrameOutBytes = MINIMP3_MAX_SAMPLES_PER_FRAME * 2;	// 1152 x 2 channels x 16 bit

// A Layer III header at p: its frame length (0 = not a valid header) and format
TInt ParseHeader(const TUint8* p, TInt aLen, TMp3FrameInfo& aInfo)
	{
	if (aLen < 4 || p[0] != 0xFF || (p[1] & 0xE0) != 0xE0)
		return 0;
	const TInt version = (p[1] >> 3) & 3;		// 0 = 2.5, 1 = reserved, 2 = MPEG-2, 3 = MPEG-1
	const TInt layer = (p[1] >> 1) & 3;			// 1 = Layer III
	const TInt brIndex = p[2] >> 4;
	const TInt srIndex = (p[2] >> 2) & 3;
	if (version == 1 || layer != 1 || brIndex == 0 || brIndex == 15 || srIndex == 3)
		return 0;								// reserved, not Layer III, free format, bad
	const TBool mpeg1 = version == 3;
	const TInt rate = KRates[srIndex] >> (mpeg1 ? 0 : (version == 2 ? 1 : 2));
	const TInt bitrate = KBitrates[mpeg1 ? 0 : 1][brIndex] * 1000;
	const TInt padding = (p[2] >> 1) & 1;
	aInfo.iSamplingFrequency = rate;
	aInfo.iNumberOfChannels = ((p[3] >> 6) == 3) ? 1 : 2;
	aInfo.iBitRate = bitrate;
	aInfo.iBufferSize = (mpeg1 ? 1152 : 576) * aInfo.iNumberOfChannels * 2;
	return (mpeg1 ? 144 : 72) * bitrate / rate + padding;
	}

class CMiniMp3Dec : public CAriMp3DecWrapper
	{
public:
	~CMiniMp3Dec()		{ delete iState; }
	void ConstructL()	{ iState = new (ELeave) TMp3State; Reset(); }
	TInt Reset()		{ mp3dec_init(&iState->iDec); iSynced = EFalse; return KErrNone; }

	TInt GetFrameInfo(const TUint8* aBuf, TInt aBufLen, TInt& aFrameLen, TMp3FrameInfo& aInfo)
		{
		aFrameLen = ParseHeader(aBuf, aBufLen, aInfo);
		return KErrNone;
		}

	TInt GetMaxFrameInfo(TInt& aFrameLen, TMp3FrameInfo& aInfo)
		{
		aFrameLen = KMaxFrameBytes;
		aInfo.iSamplingFrequency = 48000;
		aInfo.iNumberOfChannels = 2;
		aInfo.iBitRate = 320000;
		aInfo.iBufferSize = KMaxFrameOutBytes;
		return KErrNone;
		}

	TInt SeekSync(const TUint8* aBuf, TInt aBufLen, TInt& aSyncPos)
		{
		TMp3FrameInfo info;
		for (TInt i = 0; i + 4 <= aBufLen; i++)
			{
			if (ParseHeader(aBuf + i, aBufLen - i, info))
				{
				aSyncPos = i;
				return KErrNone;
				}
			}
		return KErrNotFound;
		}

	TInt Decode(TUint8* aSrc, TInt& aSrcUsed, TUint8* aDst, TInt& aDstLen)
		{
		if (aDstLen < KMaxFrameOutBytes)
			return KErrOverflow;
		// minimp3 trusts a frame without seeing the NEXT header only once it has synced (it keeps the last header),
		// and only when handed exactly that frame: then pass exactly the frame
		TMp3FrameInfo hdr;
		const TInt flen = ParseHeader(aSrc, aSrcUsed, hdr);
		const TInt give = (iSynced && flen > 0 && flen <= aSrcUsed) ? flen : aSrcUsed;
		mp3dec_frame_info_t info;
		const TInt samples = mp3dec_decode_frame(&iState->iDec, aSrc, give, reinterpret_cast<mp3d_sample_t*>(aDst), &info);
		if (info.frame_bytes == 0)
			{
			if (flen > 0 && flen <= aSrcUsed)
				{								// a whole frame minimp3 will not take: skip it (the caller must progress)
				aSrcUsed = flen;
				aDstLen = 0;
				return KErrNone;
				}
			return KErrUnderflow;				// no whole frame in the input
			}
		iSynced = ETrue;
		aSrcUsed = info.frame_bytes;			// skipped garbage (ID3 tags...) included
		aDstLen = samples * info.channels * 2;	// 0 while the bit reservoir fills
		return KErrNone;
		}
private:
	TMp3State* iState;		// decoder + scratch, one heap cell
	TBool iSynced;			// CBase zero-fills
	};
}

EXPORT_C CAriMp3DecWrapper* CAriMp3DecWrapper::NewL()
	{
	CMiniMp3Dec* self = new (ELeave) CMiniMp3Dec;
	CleanupStack::PushL(self);
	self->ConstructL();
	CleanupStack::Pop(self);
	return self;
	}

CAriMp3DecWrapper::~CAriMp3DecWrapper()
	{
	}
