// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_codec.cpp -- OURS: see phepebble_codec.h.

#include "phepebble_codec.h"

namespace
	{
	inline void PutBe16(TUint8* p, TUint16 v) { p[0] = TUint8(v >> 8); p[1] = TUint8(v); }
	inline void PutBe32(TUint8* p, TUint32 v)
		{ p[0] = TUint8(v >> 24); p[1] = TUint8(v >> 16); p[2] = TUint8(v >> 8); p[3] = TUint8(v); }
	inline void PutLe16(TUint8* p, TUint16 v) { p[0] = TUint8(v); p[1] = TUint8(v >> 8); }
	inline void PutLe32(TUint8* p, TUint32 v)
		{ p[0] = TUint8(v); p[1] = TUint8(v >> 8); p[2] = TUint8(v >> 16); p[3] = TUint8(v >> 24); }
	inline TUint16 GetBe16(const TUint8* p) { return TUint16((p[0] << 8) | p[1]); }
	inline TUint32 GetBe32(const TUint8* p)
		{ return (TUint32(p[0]) << 24) | (TUint32(p[1]) << 16) | (TUint32(p[2]) << 8) | p[3]; }
	inline TUint16 GetLe16(const TUint8* p) { return TUint16(p[0] | (p[1] << 8)); }

	void Copy(TUint8* aDst, const TUint8* aSrc, TInt aLength)
		{
		for (TInt i = 0; i < aLength; ++i)
			aDst[i] = aSrc[i];
		}

	TInt AttrsSize(const TPebbleAttr* aAttrs, TInt aCount)
		{
		TInt n = 0;
		for (TInt i = 0; i < aCount; ++i)
			n += 3 + aAttrs[i].iLength;
		return n;
		}

	TUint8* PutAttrs(TUint8* p, const TPebbleAttr* aAttrs, TInt aCount)
		{
		for (TInt i = 0; i < aCount; ++i)
			{
			*p++ = aAttrs[i].iId;
			PutLe16(p, aAttrs[i].iLength);
			p += 2;
			Copy(p, aAttrs[i].iData, aAttrs[i].iLength);
			p += aAttrs[i].iLength;
			}
		return p;
		}
	}

// ---------------------------------------------------------------------------------------------------------------
// Frame reader

TPebbleFrameReader::TPebbleFrameReader()
	{
	Reset();
	}

void TPebbleFrameReader::Reset()
	{
	iUsed = 0;
	iConsumed = 0;
	}

TInt TPebbleFrameReader::Feed(const TUint8* aData, TInt aLength)
	{
	if (iConsumed)
		{
		// Drop the frame handed out by the last Next() before taking more bytes.
		for (TInt i = iConsumed; i < iUsed; ++i)
			iBuf[i - iConsumed] = iBuf[i];
		iUsed -= iConsumed;
		iConsumed = 0;
		}
	TInt room = KBufferSize - iUsed;
	TInt take = aLength < room ? aLength : room;
	Copy(iBuf + iUsed, aData, take);
	iUsed += take;
	return take;
	}

TInt TPebbleFrameReader::Next(TUint16& aEndpoint, const TUint8*& aPayload, TInt& aLength)
	{
	if (iConsumed)
		{
		for (TInt i = iConsumed; i < iUsed; ++i)
			iBuf[i - iConsumed] = iBuf[i];
		iUsed -= iConsumed;
		iConsumed = 0;
		}
	if (iUsed < KPebbleFrameHeader)
		return KErrNotFound;
	TInt length = GetBe16(iBuf);
	if (KPebbleFrameHeader + length > KBufferSize)
		{
		Reset();
		return KErrCorrupt;
		}
	if (iUsed < KPebbleFrameHeader + length)
		return KErrNotFound;
	aEndpoint = GetBe16(iBuf + 2);
	aPayload = iBuf + KPebbleFrameHeader;
	aLength = length;
	iConsumed = KPebbleFrameHeader + length;
	return KErrNone;
	}

// ---------------------------------------------------------------------------------------------------------------
// Framing

TInt PebbleCodec::EncodeFrame(TUint16 aEndpoint, const TUint8* aPayload, TInt aLength, TUint8* aOut, TInt aMax)
	{
	if (aLength < 0 || aLength > KPebbleMaxPayloadIn)
		return KErrArgument;
	if (aMax < KPebbleFrameHeader + aLength)
		return KErrOverflow;
	PutBe16(aOut, TUint16(aLength));
	PutBe16(aOut + 2, aEndpoint);
	Copy(aOut + KPebbleFrameHeader, aPayload, aLength);
	return KPebbleFrameHeader + aLength;
	}

// ---------------------------------------------------------------------------------------------------------------
// Version exchange

TBool PebbleCodec::IsVersionRequest(const TUint8* aPayload, TInt aLength)
	{
	return aLength >= 1 && aPayload[0] == 0x00;
	}

TInt PebbleCodec::EncodeVersionResponse(TUint32 aPlatform, TUint8 aMajor, TUint8 aMinor, TUint8 aBugfix,
	TUint32 aCapabilities, TUint8* aOut, TInt aMax)
	{
	// [0x01] + v1 (library version 4, session capabilities 4: both deprecated, zero; platform 4, BE)
	// + v2 (response version 2, major, minor, bugfix) + v3 (protocol capabilities 4, LE).
	const TInt KSize = 1 + 12 + 4 + 4;
	if (aMax < KSize)
		return KErrOverflow;
	aOut[0] = 0x01;
	PutBe32(aOut + 1, 0);
	PutBe32(aOut + 5, 0);
	PutBe32(aOut + 9, aPlatform);
	aOut[13] = 2;
	aOut[14] = aMajor;
	aOut[15] = aMinor;
	aOut[16] = aBugfix;
	PutLe32(aOut + 17, aCapabilities);
	return KSize;
	}

// ---------------------------------------------------------------------------------------------------------------
// Ping

TInt PebbleCodec::DecodePing(const TUint8* aPayload, TInt aLength, TUint32& aCookie)
	{
	if (aLength < 5 || aPayload[0] != 0x00)
		return KErrCorrupt;
	aCookie = GetBe32(aPayload + 1);
	return KErrNone;
	}

static TInt PingLike(TUint8 aCmd, TUint32 aCookie, TUint8* aOut, TInt aMax)
	{
	if (aMax < 5)
		return KErrOverflow;
	aOut[0] = aCmd;
	PutBe32(aOut + 1, aCookie);
	return 5;
	}

TInt PebbleCodec::EncodePing(TUint32 aCookie, TUint8* aOut, TInt aMax)
	{
	return PingLike(0x00, aCookie, aOut, aMax);
	}

TInt PebbleCodec::EncodePong(TUint32 aCookie, TUint8* aOut, TInt aMax)
	{
	return PingLike(0x01, aCookie, aOut, aMax);
	}

// ---------------------------------------------------------------------------------------------------------------
// Clock

TInt PebbleCodec::EncodeSetUtcAndZone(TInt32 aUtc, TInt16 aOffsetMinutes, const TUint8* aZoneName,
	TInt aZoneLength, TUint8* aOut, TInt aMax)
	{
	if (aZoneLength < 0 || aZoneLength > 127)  // the length is an int8 on the watch
		return KErrArgument;
	TInt size = 1 + 4 + 2 + 1 + aZoneLength;
	if (aMax < size)
		return KErrOverflow;
	aOut[0] = 0x03;
	PutBe32(aOut + 1, TUint32(aUtc));
	PutBe16(aOut + 5, TUint16(aOffsetMinutes));
	aOut[7] = TUint8(aZoneLength);
	Copy(aOut + 8, aZoneName, aZoneLength);
	return size;
	}

TInt PebbleCodec::EncodeSetLocalTime(TInt32 aLocal, TUint8* aOut, TInt aMax)
	{
	return PingLike(0x02, TUint32(aLocal), aOut, aMax);
	}

TInt PebbleCodec::EncodeGetTime(TUint8* aOut, TInt aMax)
	{
	if (aMax < 1)
		return KErrOverflow;
	aOut[0] = 0x00;
	return 1;
	}

TInt PebbleCodec::DecodeGetTimeResponse(const TUint8* aPayload, TInt aLength, TUint32& aTime)
	{
	if (aLength < 5 || aPayload[0] != 0x01)
		return KErrCorrupt;
	aTime = GetBe32(aPayload + 1);
	return KErrNone;
	}

TBool PebbleCodec::IsTimeRequest(const TUint8* aPayload, TInt aLength)
	{
	return aLength >= 1 && aPayload[0] == 0x04;
	}

// ---------------------------------------------------------------------------------------------------------------
// BlobDB

TInt PebbleCodec::EncodeBlobInsert(TUint16 aToken, TUint8 aDb, const TUint8* aKey, TInt aKeyLength,
	const TUint8* aValue, TInt aValueLength, TUint8* aOut, TInt aMax)
	{
	if (aKeyLength <= 0 || aKeyLength > 255 || aValueLength <= 0 || aValueLength > 0xFFFF)
		return KErrArgument;
	TInt size = 1 + 2 + 1 + 1 + aKeyLength + 2 + aValueLength;
	if (aMax < size)
		return KErrOverflow;
	TUint8* p = aOut;
	*p++ = KPebbleBlobInsert;
	PutLe16(p, aToken);
	p += 2;
	*p++ = aDb;
	*p++ = TUint8(aKeyLength);
	Copy(p, aKey, aKeyLength);
	p += aKeyLength;
	PutLe16(p, TUint16(aValueLength));
	p += 2;
	Copy(p, aValue, aValueLength);
	return size;
	}

TInt PebbleCodec::EncodeBlobDelete(TUint16 aToken, TUint8 aDb, const TUint8* aKey, TInt aKeyLength,
	TUint8* aOut, TInt aMax)
	{
	if (aKeyLength <= 0 || aKeyLength > 255)
		return KErrArgument;
	TInt size = 1 + 2 + 1 + 1 + aKeyLength;
	if (aMax < size)
		return KErrOverflow;
	aOut[0] = KPebbleBlobDelete;
	PutLe16(aOut + 1, aToken);
	aOut[3] = aDb;
	aOut[4] = TUint8(aKeyLength);
	Copy(aOut + 5, aKey, aKeyLength);
	return size;
	}

TInt PebbleCodec::DecodeBlobResponse(const TUint8* aPayload, TInt aLength, TUint16& aToken, TUint8& aStatus)
	{
	if (aLength < 3)
		return KErrCorrupt;
	aToken = GetLe16(aPayload);
	aStatus = aPayload[2];
	return KErrNone;
	}

// ---------------------------------------------------------------------------------------------------------------
// TimelineItem

TInt PebbleCodec::EncodeItemPayload(const TPebbleItem& aItem, TUint8* aOut, TInt aMax)
	{
	TInt size = AttrsSize(aItem.iAttrs, aItem.iAttrCount);
	for (TInt i = 0; i < aItem.iActionCount; ++i)
		size += 3 + AttrsSize(aItem.iActions[i].iAttrs, aItem.iActions[i].iAttrCount);
	if (size > 0xFFFF)
		return KErrArgument;
	if (aMax < size)
		return KErrOverflow;
	TUint8* p = PutAttrs(aOut, aItem.iAttrs, aItem.iAttrCount);
	for (TInt i = 0; i < aItem.iActionCount; ++i)
		{
		const TPebbleAction& a = aItem.iActions[i];
		*p++ = a.iId;
		*p++ = a.iType;
		*p++ = a.iAttrCount;
		p = PutAttrs(p, a.iAttrs, a.iAttrCount);
		}
	return size;
	}

TInt PebbleCodec::EncodeItem(const TPebbleItem& aItem, TUint8* aOut, TInt aMax)
	{
	if (aMax < KPebbleItemHeaderSize)
		return KErrOverflow;
	TInt payload = EncodeItemPayload(aItem, aOut + KPebbleItemHeaderSize, aMax - KPebbleItemHeaderSize);
	if (payload < 0)
		return payload;
	// CommonTimelineItemHeader: id, parent id, timestamp, duration, type, flags, status, layout; then
	// payload length, attribute count, action count (packed, little endian).
	TUint8* p = aOut;
	Copy(p, aItem.iId, KPebbleUuidSize);
	p += KPebbleUuidSize;
	Copy(p, aItem.iParentId, KPebbleUuidSize);
	p += KPebbleUuidSize;
	PutLe32(p, TUint32(aItem.iTimestamp));
	p += 4;
	PutLe16(p, aItem.iDuration);
	p += 2;
	*p++ = aItem.iType;
	*p++ = aItem.iFlags;
	*p++ = aItem.iStatus;
	*p++ = aItem.iLayout;
	PutLe16(p, TUint16(payload));
	p += 2;
	*p++ = aItem.iAttrCount;
	*p++ = aItem.iActionCount;
	return KPebbleItemHeaderSize + payload;
	}

TInt PebbleCodec::EncodeNotificationInsert(TUint16 aToken, const TPebbleItem& aItem, TUint8* aOut, TInt aMax)
	{
	// Key and value both go into aOut: build the item first at its final place, then the BlobDB header in front.
	const TInt KHead = 1 + 2 + 1 + 1 + KPebbleUuidSize + 2;
	if (aMax < KHead)
		return KErrOverflow;
	TInt value = EncodeItem(aItem, aOut + KHead, aMax - KHead);
	if (value < 0)
		return value;
	TUint8* p = aOut;
	*p++ = KPebbleBlobInsert;
	PutLe16(p, aToken);
	p += 2;
	*p++ = KPebbleBlobDbNotifs;
	*p++ = KPebbleUuidSize;
	Copy(p, aItem.iId, KPebbleUuidSize);
	p += KPebbleUuidSize;
	PutLe16(p, TUint16(value));
	return KHead + value;
	}
