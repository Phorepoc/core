// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_codec.h -- OURS: the Pebble Protocol codec of the phepebble companion (framing, version exchange, ping,
// clock, BlobDB, notification TimelineItems). Written from the PebbleOS documentation and the layouts in its
// Apache-2.0 source; no third-party code. Transport independent: the caller feeds the byte stream of whatever link
// carries it (an RFCOMM socket, PPoGATT, a host pipe) and sends what the encoders produce.
//
// Only <e32def.h>/<e32err.h>; no allocation, no leaves: every encoder writes into the caller's buffer and returns the
// length, or KErrOverflow / KErrArgument; every decoder returns KErrNone or KErrCorrupt. This keeps it usable from
// a server, an app and a host test alike.
//
// Byte order (PebbleOS docs + source): the 4-byte frame header and the clock, ping and version platform fields are
// big endian; BlobDB, TimelineItem and the version capability word are little endian.

#ifndef PHEPEBBLE_CODEC_H
#define PHEPEBBLE_CODEC_H

#include <e32def.h>
#include <e32err.h>
#include <e32const.h>

// Endpoints used by the companion (PebbleOS include/pbl/pp/protocol.h).
const TUint16 KPebbleEpClock         = 0x000B;
const TUint16 KPebbleEpSystemVersion = 0x0010;
const TUint16 KPebbleEpRemoteVersion = 0x0011;
const TUint16 KPebbleEpPing          = 0x07D1;
const TUint16 KPebbleEpBlobDb        = 0xB1DB;

const TInt KPebbleFrameHeader     = 4;
const TInt KPebbleMaxPayloadIn    = 2044;   // phone -> watch limit
const TInt KPebbleUuidSize        = 16;

// Platform word of the version response: bits 0-2 = phone OS.
const TUint32 KPebbleOsLinux   = 4;
const TUint32 KPebbleOsAndroid = 2;

// Protocol capability bits (version response v3).
const TUint32 KPebbleCapExtendedNotifications = 1u << 3;
const TUint32 KPebbleCapNotificationFiltering = 1u << 9;

// BlobDB
const TUint8 KPebbleBlobInsert = 0x01;
const TUint8 KPebbleBlobDelete = 0x04;
const TUint8 KPebbleBlobDbPins   = 0x01;
const TUint8 KPebbleBlobDbNotifs = 0x04;
const TUint8 KPebbleBlobSuccess  = 0x01;
const TUint8 KPebbleBlobTryLater = 0x0B;

// TimelineItem
const TUint8 KPebbleItemNotification   = 1;    // TimelineItemType
const TUint8 KPebbleItemPin            = 2;
const TUint8 KPebbleLayoutGeneric      = 1;    // LayoutId
const TUint8 KPebbleLayoutNotification = 4;
const TUint8 KPebbleAttrTitle    = 1;          // AttributeId
const TUint8 KPebbleAttrSubtitle = 2;
const TUint8 KPebbleAttrBody     = 3;
const TUint8 KPebbleAttrIconTiny = 4;
const TUint8 KPebbleActionGeneric = 0x02;      // TimelineItemActionType
const TUint8 KPebbleActionDismiss = 0x04;
const TInt KPebbleItemHeaderSize = 46;         // SerializedTimelineItemHeader

/** One TimelineItem attribute: id, value bytes (strings without a terminator). */
struct TPebbleAttr
	{
	TUint8 iId;
	const TUint8* iData;
	TUint16 iLength;
	};

/** One TimelineItem action with its attributes. */
struct TPebbleAction
	{
	TUint8 iId;
	TUint8 iType;
	const TPebbleAttr* iAttrs;
	TUint8 iAttrCount;
	};

/** A notification/pin to serialise as a BlobDB value. */
struct TPebbleItem
	{
	TUint8 iId[KPebbleUuidSize];
	TUint8 iParentId[KPebbleUuidSize];
	TInt32 iTimestamp;          // UTC seconds
	TUint16 iDuration;          // minutes
	TUint8 iType;
	TUint8 iFlags;
	TUint8 iStatus;
	TUint8 iLayout;
	const TPebbleAttr* iAttrs;
	TUint8 iAttrCount;
	const TPebbleAction* iActions;
	TUint8 iActionCount;
	};

/** Frame splitter: feed it stream bytes, take out whole messages. Fixed buffer, no allocation. */
class TPebbleFrameReader
	{
public:
	TPebbleFrameReader();
	void Reset();
	/** Append stream bytes; returns how many were taken (all, unless the buffer is full: then take frames first). */
	TInt Feed(const TUint8* aData, TInt aLength);
	/** KErrNone and one message (the payload points into the reader, valid until the next call), KErrNotFound when
	    no whole frame is buffered, KErrCorrupt when a header announces more than the buffer holds (then Reset). */
	TInt Next(TUint16& aEndpoint, const TUint8*& aPayload, TInt& aLength);
private:
	enum { KBufferSize = KPebbleFrameHeader + KPebbleMaxPayloadIn + 4 };
	TUint8 iBuf[KBufferSize];
	TInt iUsed;
	TInt iConsumed;             // bytes of the frame handed out by the last Next()
	};

class PebbleCodec
	{
public:
	// Framing
	static TInt EncodeFrame(TUint16 aEndpoint, const TUint8* aPayload, TInt aLength, TUint8* aOut, TInt aMax);

	// Version exchange (endpoint 0x0011): the watch asks with [0x00]; the phone answers [0x01, v3 body].
	static TBool IsVersionRequest(const TUint8* aPayload, TInt aLength);
	static TInt EncodeVersionResponse(TUint32 aPlatform, TUint8 aMajor, TUint8 aMinor, TUint8 aBugfix,
		TUint32 aCapabilities, TUint8* aOut, TInt aMax);

	// Ping (endpoint 0x07D1): [0x00, u32 BE cookie, optional idle]; pong = [0x01, cookie].
	static TInt DecodePing(const TUint8* aPayload, TInt aLength, TUint32& aCookie);
	static TInt EncodePing(TUint32 aCookie, TUint8* aOut, TInt aMax);
	static TInt EncodePong(TUint32 aCookie, TUint8* aOut, TInt aMax);

	// Clock (endpoint 0x000B, private): 0x03 = set UTC + timezone, 0x02 = deprecated set local time,
	// 0x00 = get time (watch answers 0x01 + u32 BE), 0x04 = the watch asks the phone for 0x03.
	static TInt EncodeSetUtcAndZone(TInt32 aUtc, TInt16 aOffsetMinutes, const TUint8* aZoneName, TInt aZoneLength,
		TUint8* aOut, TInt aMax);
	static TInt EncodeSetLocalTime(TInt32 aLocal, TUint8* aOut, TInt aMax);
	static TInt EncodeGetTime(TUint8* aOut, TInt aMax);
	static TInt DecodeGetTimeResponse(const TUint8* aPayload, TInt aLength, TUint32& aTime);
	static TBool IsTimeRequest(const TUint8* aPayload, TInt aLength);

	// BlobDB (endpoint 0xB1DB, private)
	static TInt EncodeBlobInsert(TUint16 aToken, TUint8 aDb, const TUint8* aKey, TInt aKeyLength,
		const TUint8* aValue, TInt aValueLength, TUint8* aOut, TInt aMax);
	static TInt EncodeBlobDelete(TUint16 aToken, TUint8 aDb, const TUint8* aKey, TInt aKeyLength,
		TUint8* aOut, TInt aMax);
	static TInt DecodeBlobResponse(const TUint8* aPayload, TInt aLength, TUint16& aToken, TUint8& aStatus);

	// TimelineItem (the BlobDB value of Notifs/Pins): 46-byte header + attributes + actions.
	static TInt EncodeItemPayload(const TPebbleItem& aItem, TUint8* aOut, TInt aMax);   // attributes + actions only
	static TInt EncodeItem(const TPebbleItem& aItem, TUint8* aOut, TInt aMax);
	/** A ready BlobDB insert of a notification into Notifs (key = item id). */
	static TInt EncodeNotificationInsert(TUint16 aToken, const TPebbleItem& aItem, TUint8* aOut, TInt aMax);
	};

#endif
