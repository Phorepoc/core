// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_session.cpp -- OURS: see phepebble_session.h.

#include "phepebble_session.h"

TInt PebbleUtf8Cut(const TUint8* aText, TInt aLength, TInt aMax)
	{
	if (aLength <= aMax)
		return aLength;
	TInt n = aMax;
	while (n > 0 && (aText[n] & 0xC0) == 0x80)   // aText[n] is a continuation byte: back up to a character start
		--n;
	return n;
	}

TPebbleSession::TPebbleSession(MPebbleLink& aLink, MPebbleEnvironment& aEnv, MPebbleSessionObserver& aObserver)
	: iLink(aLink), iEnv(aEnv), iObserver(aObserver), iNextToken(1), iReady(EFalse)
	{
	}

void TPebbleSession::Reset()
	{
	iReader.Reset();
	iReady = EFalse;
	}

TInt TPebbleSession::SendFrame(TUint16 aEndpoint, const TUint8* aPayload, TInt aLength)
	{
	TUint8 frame[KPebbleFrameHeader + KPebbleMaxPayloadIn];
	TInt n = PebbleCodec::EncodeFrame(aEndpoint, aPayload, aLength, frame, sizeof frame);
	if (n < 0)
		return n;
	return iLink.LinkSend(frame, n);
	}

void TPebbleSession::Receive(const TUint8* aData, TInt aLength)
	{
	while (aLength > 0)
		{
		TInt took = iReader.Feed(aData, aLength);
		aData += took;
		aLength -= took;
		TUint16 ep;
		const TUint8* payload;
		TInt len;
		TInt r;
		while ((r = iReader.Next(ep, payload, len)) == KErrNone)
			Handle(ep, payload, len);
		if (r == KErrCorrupt)
			continue;            // the reader reset itself; keep feeding
		if (took == 0)
			break;               // cannot happen (Next frees space), but never spin
		}
	}

void TPebbleSession::Handle(TUint16 aEndpoint, const TUint8* aPayload, TInt aLength)
	{
	TUint8 out[32];
	switch (aEndpoint)
		{
	case KPebbleEpRemoteVersion:
		if (PebbleCodec::IsVersionRequest(aPayload, aLength))
			{
			TInt n = PebbleCodec::EncodeVersionResponse(KPebbleOsLinux, KPebbleAppMajor, KPebbleAppMinor,
				KPebbleAppBugfix, 0, out, sizeof out);
			if (n > 0)
				SendFrame(KPebbleEpRemoteVersion, out, n);
			iReady = ETrue;
			SendTime();
			iObserver.SessionReady();
			}
		break;
	case KPebbleEpPing:
		{
		TUint32 cookie;
		if (PebbleCodec::DecodePing(aPayload, aLength, cookie) == KErrNone)
			{
			TInt n = PebbleCodec::EncodePong(cookie, out, sizeof out);
			SendFrame(KPebbleEpPing, out, n);
			}
		break;
		}
	case KPebbleEpClock:
		if (PebbleCodec::IsTimeRequest(aPayload, aLength))
			SendTime();
		break;
	case KPebbleEpBlobDb:
		{
		TUint16 token;
		TUint8 status;
		if (PebbleCodec::DecodeBlobResponse(aPayload, aLength, token, status) == KErrNone)
			iObserver.NotificationResult(token, status);
		break;
		}
	default:
		break;                   // app launcher (0x0031), music, ... not handled yet
		}
	}

TInt TPebbleSession::SendTime()
	{
	TUint8 zone[64];
	TInt zl = iEnv.ZoneName(zone, sizeof zone);
	if (zl < 0 || zl > (TInt)sizeof zone)
		zl = 0;
	TUint8 out[80];
	TInt n = PebbleCodec::EncodeSetUtcAndZone(iEnv.UtcNow(), iEnv.UtcOffsetMinutes(), zone, zl, out, sizeof out);
	if (n < 0)
		return n;
	return SendFrame(KPebbleEpClock, out, n);
	}

TInt TPebbleSession::SendNotification(const TUint8* aTitle, TInt aTitleLength, const TUint8* aBody, TInt aBodyLength)
	{
	if (!iReady)
		return KErrNotReady;
	static const TUint8 KDismiss[] = {'D', 'i', 's', 'm', 'i', 's', 's'};
	TPebbleAttr attrs[2];
	TInt na = 0;
	attrs[na].iId = KPebbleAttrTitle;
	attrs[na].iData = aTitle;
	attrs[na].iLength = TUint16(PebbleUtf8Cut(aTitle, aTitleLength, KMaxTitle));
	++na;
	if (aBodyLength > 0)
		{
		attrs[na].iId = KPebbleAttrBody;
		attrs[na].iData = aBody;
		attrs[na].iLength = TUint16(PebbleUtf8Cut(aBody, aBodyLength, KMaxBody));
		++na;
		}
	const TPebbleAttr dismissAttr[1] = {{KPebbleAttrTitle, KDismiss, sizeof KDismiss}};
	const TPebbleAction actions[1] = {{0, KPebbleActionDismiss, dismissAttr, 1}};
	TPebbleItem item;
	iEnv.Random(item.iId, KPebbleUuidSize);
	item.iId[6] = TUint8((item.iId[6] & 0x0F) | 0x40);   // a version 4 UUID
	item.iId[8] = TUint8((item.iId[8] & 0x3F) | 0x80);
	for (TInt i = 0; i < KPebbleUuidSize; ++i)
		item.iParentId[i] = 0;
	item.iTimestamp = iEnv.UtcNow();
	item.iDuration = 0;
	item.iType = KPebbleItemNotification;
	item.iFlags = 0;
	item.iStatus = 0;
	item.iLayout = KPebbleLayoutNotification;
	item.iAttrs = attrs;
	item.iAttrCount = TUint8(na);
	item.iActions = actions;
	item.iActionCount = 1;

	TUint8 payload[KPebbleMaxPayloadIn];
	TUint16 token = iNextToken++;
	if (iNextToken == 0)
		iNextToken = 1;
	TInt n = PebbleCodec::EncodeNotificationInsert(token, item, payload, sizeof payload);
	if (n < 0)
		return n;
	TInt r = SendFrame(KPebbleEpBlobDb, payload, n);
	return r < 0 ? r : token;
	}
