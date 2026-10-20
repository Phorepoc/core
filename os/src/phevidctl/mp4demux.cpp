// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// mp4demux.cpp -- OURS (VDU rung V4): see mp4demux.h. ISO/IEC 14496-12 box walking over a moov payload held in memory.
#include "mp4demux.h"

namespace mp4
{
static inline uint32_t Be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static inline uint16_t Be16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static inline uint64_t Be64(const uint8_t* p) { return ((uint64_t)Be32(p) << 32) | Be32(p + 4); }

#define FOURCC(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

// a box inside a buffer
struct TBoxRef
	{
	uint32_t iType;
	const uint8_t* iPayload;
	uint32_t iLen;			// payload length
	};

// iterate the child boxes of [aBuf, aBuf + aLen): aPos is the running offset; false at the end / on a malformed box
static bool NextBox(const uint8_t* aBuf, uint32_t aLen, uint32_t& aPos, TBoxRef& aBox)
	{
	if (aPos + 8 > aLen)
		return false;
	uint64_t size = Be32(aBuf + aPos);
	const uint32_t type = Be32(aBuf + aPos + 4);
	uint32_t hdr = 8;
	if (size == 1)
		{
		if (aPos + 16 > aLen)
			return false;
		size = Be64(aBuf + aPos + 8);
		hdr = 16;
		}
	else if (size == 0)
		size = aLen - aPos;			// to the end of the container
	if (size < hdr || aPos + size > aLen)
		return false;
	aBox.iType = type;
	aBox.iPayload = aBuf + aPos + hdr;
	aBox.iLen = (uint32_t)(size - hdr);
	aPos += (uint32_t)size;
	return true;
	}

static bool FindChild(const TBoxRef& aParent, uint32_t aType, TBoxRef& aOut)
	{
	uint32_t pos = 0;
	TBoxRef b;
	while (NextBox(aParent.iPayload, aParent.iLen, pos, b))
		if (b.iType == aType)
			{
			aOut = b;
			return true;
			}
	return false;
	}

int Locate(TReadFn aRead, void* aCtx, uint64_t aFileSize, uint32_t aMaxMoov, uint64_t& aPos, uint32_t& aLen)
	{
	uint64_t pos = 0;
	bool sawMoof = false;
	while (pos + 8 <= aFileSize)
		{
		uint8_t h[16];
		if (aRead(aCtx, pos, h, 16) < 8)
			return KErrCorrupt;
		uint64_t size = Be32(h);
		const uint32_t type = Be32(h + 4);
		uint32_t hdr = 8;
		if (size == 1)
			{
			size = Be64(h + 8);
			hdr = 16;
			}
		else if (size == 0)
			size = aFileSize - pos;
		if (size < hdr || pos + size > aFileSize)
			{
			if (type == FOURCC('m', 'o', 'o', 'v'))
				return KErrCorrupt;			// a truncated moov
			break;
			}
		if (type == FOURCC('m', 'o', 'o', 'v'))
			{
			if (size - hdr > aMaxMoov)
				return KErrTooBig;
			aPos = pos + hdr;
			aLen = (uint32_t)(size - hdr);
			return KOk;
			}
		if (type == FOURCC('m', 'o', 'o', 'f'))
			sawMoof = true;
		pos += size;
		}
	return sawMoof ? KErrFragmented : KErrNoMoov;
	}

// parse a (ver/flags, count, entries) table box
static bool Table(const TBoxRef& aBox, uint32_t aEntrySize, TMovie::TTable& aOut)
	{
	if (aBox.iLen < 8)
		return false;
	const uint32_t count = Be32(aBox.iPayload + 4);
	if ((uint64_t)count * aEntrySize > aBox.iLen - 8)
		return false;
	aOut.iData = aBox.iPayload + 8;
	aOut.iCount = count;
	return true;
	}

// the sample tables of a track's stbl
int TMovie::LoadTables(const uint8_t* aStbl, uint32_t aLen, TTrack& aOut)
	{
	TBoxRef stbl, b;
	stbl.iType = 0;
	stbl.iPayload = aStbl;
	stbl.iLen = aLen;
	aOut.iStts.iData = aOut.iCtts.iData = aOut.iStss.iData = aOut.iStsc.iData = aOut.iStco.iData = aOut.iStsz.iData = 0;
	aOut.iStts.iCount = aOut.iCtts.iCount = aOut.iStss.iCount = aOut.iStsc.iCount = aOut.iStco.iCount = aOut.iStsz.iCount = 0;
	aOut.iCo64 = false;
	aOut.iFixedSize = 0;
	aOut.iSampleCount = 0;
	if (!FindChild(stbl, FOURCC('s', 't', 't', 's'), b) || !Table(b, 8, aOut.iStts))
		return KErrCorrupt;
	if (FindChild(stbl, FOURCC('c', 't', 't', 's'), b) && !Table(b, 8, aOut.iCtts))
		return KErrCorrupt;
	if (FindChild(stbl, FOURCC('s', 't', 's', 's'), b) && !Table(b, 4, aOut.iStss))
		return KErrCorrupt;
	if (!FindChild(stbl, FOURCC('s', 't', 's', 'c'), b) || !Table(b, 12, aOut.iStsc) || !aOut.iStsc.iCount)
		return KErrCorrupt;
	if (FindChild(stbl, FOURCC('s', 't', 'c', 'o'), b))
		{
		if (!Table(b, 4, aOut.iStco))
			return KErrCorrupt;
		}
	else if (FindChild(stbl, FOURCC('c', 'o', '6', '4'), b))
		{
		if (!Table(b, 8, aOut.iStco))
			return KErrCorrupt;
		aOut.iCo64 = true;
		}
	else
		return KErrCorrupt;
	if (!FindChild(stbl, FOURCC('s', 't', 's', 'z'), b) || b.iLen < 12)
		return KErrCorrupt;
	aOut.iFixedSize = Be32(b.iPayload + 4);
	aOut.iStsz.iCount = Be32(b.iPayload + 8);
	aOut.iStsz.iData = b.iPayload + 12;
	if (!aOut.iFixedSize && (uint64_t)aOut.iStsz.iCount * 4 > b.iLen - 12)
		return KErrCorrupt;
	aOut.iSampleCount = aOut.iStsz.iCount;
	return KOk;
	}

// mdhd -> timescale + duration
static bool Mdhd(const TBoxRef& aMdia, uint32_t& aTimescale, uint64_t& aDuration)
	{
	TBoxRef mdhd;
	if (!FindChild(aMdia, FOURCC('m', 'd', 'h', 'd'), mdhd) || mdhd.iLen < 24)
		return false;
	if (mdhd.iPayload[0] == 1)
		{
		if (mdhd.iLen < 32)
			return false;
		aTimescale = Be32(mdhd.iPayload + 20);
		aDuration = Be64(mdhd.iPayload + 24);
		}
	else
		{
		aTimescale = Be32(mdhd.iPayload + 12);
		aDuration = Be32(mdhd.iPayload + 16);
		}
	return aTimescale != 0;
	}

// an MPEG-4 descriptor header (14496-1 8.3.3): tag byte, then a length of 1..4 bytes of 7 bits each (high bit = more)
static bool Descr(const uint8_t*& aP, const uint8_t* aEnd, uint32_t& aTag, uint32_t& aLen)
	{
	if (aP >= aEnd)
		return false;
	aTag = *aP++;
	uint32_t len = 0;
	for (int i = 0; i < 4; ++i)
		{
		if (aP >= aEnd)
			return false;
		const uint8_t c = *aP++;
		len = (len << 7) | (c & 0x7f);
		if (!(c & 0x80))
			break;
		}
	if (len > (uint32_t)(aEnd - aP))
		return false;
	aLen = len;
	return true;
	}

// esds payload (full box) -> objectTypeIndication, avg bitrate and the DecoderSpecificInfo (AudioSpecificConfig)
static bool Esds(const TBoxRef& aEsds, uint32_t& aOti, uint32_t& aAvgBitrate, const uint8_t*& aAsc, uint32_t& aAscLen)
	{
	if (aEsds.iLen < 4)
		return false;
	const uint8_t* p = aEsds.iPayload + 4;
	const uint8_t* end = aEsds.iPayload + aEsds.iLen;
	uint32_t tag, len;
	if (!Descr(p, end, tag, len) || tag != 0x03 || len < 3)
		return false;
	end = p + len;
	const uint8_t flags = p[2];
	p += 3;							// ES_ID (2) + flags
	if (flags & 0x80)
		p += 2;						// dependsOn_ES_ID
	if (flags & 0x40)
		{
		if (p >= end)
			return false;
		p += 1 + *p;				// URL
		}
	if (flags & 0x20)
		p += 2;						// OCR_ES_ID
	if (p > end || !Descr(p, end, tag, len) || tag != 0x04 || len < 13)
		return false;
	end = p + len;
	aOti = p[0];
	aAvgBitrate = Be32(p + 9);
	p += 13;
	if (!Descr(p, end, tag, len) || tag != 0x05 || !len)
		return false;
	aAsc = p;
	aAscLen = len;
	return true;
	}

int TMovie::Parse(const uint8_t* aMoov, uint32_t aLen)
	{
	iBase = aMoov;
	iLen = aLen;
	iTimescale = 0;
	iDuration = 0;
	iWidth = iHeight = iSampleCount = 0;
	iNalLenSize = 4;
	iAvcC = 0;
	iAvcCLen = 0;
	iHasAudio = false;
	iAudioFourCC = 0;
	iAudioOk = false;
	iAudioTimescale = 0;
	iAudioDuration = 0;
	iAudioSampleCount = 0;
	iAudioChannels = iAudioRate = iAudioObjectType = iAudioAvgBitrate = 0;
	iAsc = 0;
	iAscLen = 0;
	iAudioSkip = 0;
	LoadTables(0, 0, iV);
	LoadTables(0, 0, iA);

	bool found = false;
	uint32_t pos = 0;
	TBoxRef trak;
	while (NextBox(aMoov, aLen, pos, trak))
		{
		if (trak.iType != FOURCC('t', 'r', 'a', 'k'))
			continue;
		TBoxRef mdia, hdlr, minf, stbl, stsd;
		if (!FindChild(trak, FOURCC('m', 'd', 'i', 'a'), mdia) || !FindChild(mdia, FOURCC('h', 'd', 'l', 'r'), hdlr) ||
			hdlr.iLen < 12)
			continue;
		const uint32_t handler = Be32(hdlr.iPayload + 8);
		if (!FindChild(mdia, FOURCC('m', 'i', 'n', 'f'), minf) || !FindChild(minf, FOURCC('s', 't', 'b', 'l'), stbl) ||
			!FindChild(stbl, FOURCC('s', 't', 's', 'd'), stsd) || stsd.iLen < 16)
			continue;
		const uint32_t entryType = Be32(stsd.iPayload + 12);
		if (handler == FOURCC('s', 'o', 'u', 'n'))
			{
			if (iHasAudio)
				continue;				// (only the first audio track is considered)
			iHasAudio = true;
			iAudioFourCC = entryType;
			if (entryType != FOURCC('m', 'p', '4', 'a'))
				continue;
			// AudioSampleEntry: 8-byte box header, 28 bytes (QuickTime sound version 1: +16), then child boxes (esds)
			const uint8_t* e = stsd.iPayload + 8;
			const uint32_t eAvail = stsd.iLen - 8;
			if (eAvail < 8 + 28)
				continue;
			const uint32_t esize = Be32(e);
			const uint32_t qtver = Be16(e + 8 + 8);
			const uint32_t fixed = 8 + 28 + (qtver == 1 ? 16 : 0);
			if (qtver > 1 || esize < fixed || esize > eAvail)
				continue;
			TBoxRef entryBoxes, esds;
			entryBoxes.iType = 0;
			entryBoxes.iPayload = e + fixed;
			entryBoxes.iLen = esize - fixed;
			uint32_t oti = 0, avg = 0, ascLen = 0;
			const uint8_t* asc = 0;
			uint32_t timescale = 0;
			uint64_t duration = 0;
			if (!FindChild(entryBoxes, FOURCC('e', 's', 'd', 's'), esds) || !Esds(esds, oti, avg, asc, ascLen) ||
				!(oti == 0x40 || (oti >= 0x66 && oti <= 0x68)) || !Mdhd(mdia, timescale, duration) ||
				LoadTables(stbl.iPayload, stbl.iLen, iA) != KOk || !iA.iSampleCount)
				continue;
			iAudioChannels = Be16(e + 8 + 16);
			iAudioRate = Be32(e + 8 + 24) >> 16;
			iAudioObjectType = oti;
			iAudioAvgBitrate = avg;
			iAsc = asc;
			iAscLen = ascLen;
			iAudioTimescale = timescale;
			iAudioDuration = duration;
			iAudioSampleCount = iA.iSampleCount;
			// edit list: the first non-empty edit's media_time = the priming samples a player must not play
			TBoxRef edts, elst;
			if (FindChild(trak, FOURCC('e', 'd', 't', 's'), edts) && FindChild(edts, FOURCC('e', 'l', 's', 't'), elst) && elst.iLen >= 8)
				{
				const uint32_t ver = elst.iPayload[0], cnt = Be32(elst.iPayload + 4), esz = ver == 1 ? 20 : 12;
				for (uint32_t k = 0; k < cnt && 8 + (k + 1) * esz <= elst.iLen; ++k)
					{
					const uint8_t* ent = elst.iPayload + 8 + k * esz;
					const int64_t mt = ver == 1 ? (int64_t)Be64(ent + 8) : (int64_t)(int32_t)Be32(ent + 4);
					if (mt >= 0)
						{
						iAudioSkip = (uint64_t)mt;
						break;
						}
					}
				}
			iAudioOk = true;
			continue;
			}
		if (found || handler != FOURCC('v', 'i', 'd', 'e'))
			continue;
		if (entryType != FOURCC('a', 'v', 'c', '1') && entryType != FOURCC('a', 'v', 'c', '3'))
			continue;						// another video codec: skipped (only H.264 so far)

		// the sample entry: VisualSampleEntry (78 bytes after its 8-byte header) then child boxes (avcC)
		const uint8_t* e = stsd.iPayload + 8;
		const uint32_t eAvail = stsd.iLen - 8;
		if (eAvail < 8 + 78)
			return KErrCorrupt;
		const uint32_t esize = Be32(e);
		if (esize < 8 + 78 || esize > eAvail)
			return KErrCorrupt;
		const uint32_t width = Be16(e + 32), height = Be16(e + 34);
		TBoxRef entryBoxes, avcc;
		entryBoxes.iType = 0;
		entryBoxes.iPayload = e + 8 + 78;
		entryBoxes.iLen = esize - 8 - 78;
		if (!FindChild(entryBoxes, FOURCC('a', 'v', 'c', 'C'), avcc) || avcc.iLen < 7)
			return KErrCorrupt;

		uint32_t timescale = 0;
		uint64_t duration = 0;
		if (!Mdhd(mdia, timescale, duration))
			return KErrCorrupt;
		if (LoadTables(stbl.iPayload, stbl.iLen, iV) != KOk)
			return KErrCorrupt;

		iAvcC = avcc.iPayload;
		iAvcCLen = avcc.iLen;
		iNalLenSize = (avcc.iPayload[4] & 3) + 1;
		iTimescale = timescale;
		iDuration = duration;
		iWidth = width;
		iHeight = height;
		iSampleCount = iV.iSampleCount;
		found = true;
		}
	return found ? KOk : KErrNoVideo;
	}

// the SPS / PPS of the avcC as Annex B
uint32_t TMovie::ParameterSetsSize() const
	{
	return ParameterSets(0, 0);
	}

uint32_t TMovie::ParameterSets(uint8_t* aDst, uint32_t aMax) const
	{
	if (!iAvcC || iAvcCLen < 7)
		return 0;
	const uint8_t* p = iAvcC + 5;
	const uint8_t* end = iAvcC + iAvcCLen;
	uint32_t out = 0;
	for (int pass = 0; pass < 2; ++pass)
		{
		if (p >= end)
			return 0;
		uint32_t n = pass == 0 ? (uint32_t)(*p & 0x1f) : (uint32_t)*p;
		++p;
		while (n--)
			{
			if (p + 2 > end)
				return 0;
			const uint32_t l = Be16(p);
			p += 2;
			if (p + l > end)
				return 0;
			if (aDst)
				{
				if (out + 4 + l > aMax)
					return 0;
				aDst[out] = aDst[out + 1] = aDst[out + 2] = 0;
				aDst[out + 3] = 1;
				for (uint32_t i = 0; i < l; ++i)
					aDst[out + 4 + i] = p[i];
				}
			out += 4 + l;
			p += l;
			}
		}
	return out;
	}

int TMovie::Walk(TSampleFn aFn, void* aCtx) const
	{
	if (!iV.iSampleCount)
		return KErrNoVideo;
	return WalkTrack(iV, aFn, aCtx);
	}

int TMovie::WalkAudio(TSampleFn aFn, void* aCtx) const
	{
	if (!iAudioOk)
		return KErrNoAudio;
	return WalkTrack(iA, aFn, aCtx);
	}

int TMovie::WalkTrack(const TTrack& aT, TSampleFn aFn, void* aCtx)
	{
	uint32_t sttsIdx = 0, sttsLeft = 0, sttsDelta = 0;
	uint32_t cttsIdx = 0, cttsLeft = 0;
	int32_t cttsOff = 0;
	uint32_t stssIdx = 0;
	uint32_t stscIdx = 0;
	int64_t dts = 0;
	uint32_t i = 0;
	for (uint32_t c = 1; c <= aT.iStco.iCount && i < aT.iSampleCount; ++c)
		{
		while (stscIdx + 1 < aT.iStsc.iCount && Be32(aT.iStsc.iData + (stscIdx + 1) * 12) <= c)
			++stscIdx;
		const uint32_t spc = Be32(aT.iStsc.iData + stscIdx * 12 + 4);
		uint64_t off = aT.iCo64 ? Be64(aT.iStco.iData + (c - 1) * 8) : (uint64_t)Be32(aT.iStco.iData + (c - 1) * 4);
		for (uint32_t j = 0; j < spc && i < aT.iSampleCount; ++j, ++i)
			{
			TSample s;
			s.iOffset = off;
			s.iSize = aT.iFixedSize ? aT.iFixedSize : Be32(aT.iStsz.iData + i * 4);
			off += s.iSize;
			while (sttsLeft == 0)
				{
				if (sttsIdx >= aT.iStts.iCount)
					return KErrCorrupt;
				sttsLeft = Be32(aT.iStts.iData + sttsIdx * 8);
				sttsDelta = Be32(aT.iStts.iData + sttsIdx * 8 + 4);
				++sttsIdx;
				}
			s.iDts = dts;
			dts += sttsDelta;
			--sttsLeft;
			int64_t co = 0;
			if (aT.iCtts.iCount)
				{
				while (cttsLeft == 0 && cttsIdx < aT.iCtts.iCount)
					{
					cttsLeft = Be32(aT.iCtts.iData + cttsIdx * 8);
					cttsOff = (int32_t)Be32(aT.iCtts.iData + cttsIdx * 8 + 4);
					++cttsIdx;
					}
				if (cttsLeft)
					{
					co = cttsOff;			// (version 1 signed; version 0 values stay below 2^31)
					--cttsLeft;
					}
				}
			s.iCts = s.iDts + co;
			if (aT.iStss.iCount)
				{
				s.iSync = stssIdx < aT.iStss.iCount && Be32(aT.iStss.iData + stssIdx * 4) == i + 1;
				if (s.iSync)
					++stssIdx;
				}
			else
				s.iSync = true;
			if (!aFn(aCtx, s))
				return KOk;
			}
		}
	return i == aT.iSampleCount ? KOk : KErrCorrupt;
	}
}
