/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// mp4demux.h -- OURS (VDU rung V4): a small ISO base media file (MP4 / 3GP / MOV) demuxer core
// for the phevidctl video controller. Plain C++ (no Symbian types, no allocation, no static data): it reads the moov box
// from a caller-supplied byte reader, finds the first H.264 ('avc1'/'avc3') video track, and enumerates its samples
// (offset, size, DTS, CTS, sync flag) straight from the sample tables (stts / ctts / stss / stsc / stsz / stco|co64).
// Written from ISO/IEC 14496-12, -14 (esds) and -15. Also finds the first mp4a AAC audio track (V4 audio). Fragmented files (moof) and edit lists are not handled.
#ifndef MP4DEMUX_H
#define MP4DEMUX_H

#include <stdint.h>

namespace mp4
{
// reads aLen bytes at file offset aPos into aDst; returns the number of bytes read (< aLen at end of file), <0 on error
typedef int (*TReadFn)(void* aCtx, uint64_t aPos, uint8_t* aDst, uint32_t aLen);

enum
	{
	KOk = 0,
	KErrNoMoov = -1,		// no moov box (a fragmented or truncated file)
	KErrCorrupt = -2,
	KErrNoVideo = -3,		// no H.264 video track
	KErrTooBig = -4,		// the moov box is larger than the cap given to Locate
	KErrFragmented = -5,
	KErrNoAudio = -6		// no usable (mp4a + esds AudioSpecificConfig) audio track
	};

struct TSample
	{
	uint64_t iOffset;
	uint32_t iSize;
	int64_t iDts;			// track timescale units
	int64_t iCts;			// DTS + the composition offset
	bool iSync;
	};

// Called per sample, in decode order; return false to stop the walk.
typedef bool (*TSampleFn)(void* aCtx, const TSample& aSample);

// Where the moov box payload is (top-level boxes are walked with aRead): KOk or an error.
int Locate(TReadFn aRead, void* aCtx, uint64_t aFileSize, uint32_t aMaxMoov, uint64_t& aPos, uint32_t& aLen);

class TMovie
	{
public:
	// aMoov = the moov payload (everything after its 8/16-byte box header); the buffer must outlive this object.
	int Parse(const uint8_t* aMoov, uint32_t aLen);
	// enumerate the video track's samples
	int Walk(TSampleFn aFn, void* aCtx) const;
	// enumerate the audio track's samples (iAudioOk); iDts / iCts are in iAudioTimescale units
	int WalkAudio(TSampleFn aFn, void* aCtx) const;

	uint32_t iTimescale;		// video track
	uint64_t iDuration;		// video track, in iTimescale units (from the mdhd; the tkhd/mvhd are not consulted)
	uint32_t iWidth, iHeight;	// from the sample entry
	uint32_t iSampleCount;
	uint32_t iNalLenSize;		// 1..4: the length prefix of the NAL units in the samples
	const uint8_t* iAvcC;		// the AVCDecoderConfigurationRecord (payload of the avcC box)
	uint32_t iAvcCLen;
	bool iHasAudio;			// an audio ('soun') track exists
	uint32_t iAudioFourCC;	// its sample entry type ('mp4a', ...)
	// the first 'mp4a' audio track with an esds AAC AudioSpecificConfig is usable (iAudioOk); other audio is noted and skipped
	bool iAudioOk;
	uint32_t iAudioTimescale;
	uint64_t iAudioDuration;	// mdhd, iAudioTimescale units
	uint32_t iAudioSampleCount;
	uint32_t iAudioChannels;	// from the sample entry (the AudioSpecificConfig is authoritative)
	uint32_t iAudioRate;		// Hz, from the sample entry (HE-AAC: may be the core rate)
	uint32_t iAudioObjectType;	// esds objectTypeIndication (0x40 = MPEG-4 AAC, 0x66-0x68 = MPEG-2 AAC)
	uint32_t iAudioAvgBitrate;	// esds, bit/s (0 = unknown)
	const uint8_t* iAsc;		// the AudioSpecificConfig (esds DecoderSpecificInfo)
	uint32_t iAscLen;
	uint64_t iAudioSkip;		// first edit-list media_time of the audio track (encoder priming), iAudioTimescale units: leading PCM to drop

	// avcC -> Annex B parameter sets: writes 00 00 00 01 + SPS... and PPS... to aDst (aMax bytes); returns the length, 0 on error
	uint32_t ParameterSets(uint8_t* aDst, uint32_t aMax) const;
	uint32_t ParameterSetsSize() const;

	// helper tables (offsets into the moov buffer; 0 = absent)
	struct TTable
		{
		const uint8_t* iData;		// first entry
		uint32_t iCount;			// entries
		};

private:
	struct TTrack
		{
		TTable iStts, iCtts, iStss, iStsc, iStco, iStsz;
		bool iCo64;
		uint32_t iFixedSize;		// stsz: the constant sample size (0 = per-sample table)
		uint32_t iSampleCount;
		};
	static int LoadTables(const uint8_t* aStbl, uint32_t aLen, TTrack& aOut);
	static int WalkTrack(const TTrack& aT, TSampleFn aFn, void* aCtx);

	const uint8_t* iBase;
	uint32_t iLen;
	TTrack iV, iA;
	};
}

#endif
