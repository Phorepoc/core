// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_session.h -- OURS: the phone's Pebble Protocol session over any byte link (PPoGATT here): answers the watch's
// version request (then sets the clock), answers pings, answers the watch's time request, and inserts notifications into
// BlobDB Notifs with tokens. Portable (only e32def/e32err) so the host test drives it with a fake link.

#ifndef PHEPEBBLE_SESSION_H
#define PHEPEBBLE_SESSION_H

#include "phepebble_codec.h"

/** Where the session's frames go (the PPoGATT client in the service). */
class MPebbleLink
	{
public:
	virtual TInt LinkSend(const TUint8* aData, TInt aLength) = 0;
	};

/** The phone's clock, zone and randomness. */
class MPebbleEnvironment
	{
public:
	virtual TInt32 UtcNow() = 0;
	/** Local time minus UTC, minutes (DST included). */
	virtual TInt16 UtcOffsetMinutes() = 0;
	/** The Olson zone name ("Europe/Berlin"), or 0 length. Returns the length written (max aMax). */
	virtual TInt ZoneName(TUint8* aOut, TInt aMax) = 0;
	virtual void Random(TUint8* aOut, TInt aLength) = 0;
	};

class MPebbleSessionObserver
	{
public:
	/** Version exchange done: the watch is ready for private endpoints. */
	virtual void SessionReady() = 0;
	/** A BlobDB answer for a notification the session sent. */
	virtual void NotificationResult(TUint16 aToken, TUint8 aStatus) = 0;
	};

const TUint8 KPebbleAppMajor = 0;
const TUint8 KPebbleAppMinor = 1;
const TUint8 KPebbleAppBugfix = 0;

class TPebbleSession
	{
public:
	TPebbleSession(MPebbleLink& aLink, MPebbleEnvironment& aEnv, MPebbleSessionObserver& aObserver);
	/** A new transport session: forget partial frames. */
	void Reset();
	/** Stream bytes from the watch (any split). */
	void Receive(const TUint8* aData, TInt aLength);
	/** Send the clock: Clock 0x03 with UTC, the real offset and the zone name. */
	TInt SendTime();
	/** Insert a notification: title + body (UTF-8, cut to fit), timestamp now; returns the token (>= 0) or an error. */
	TInt SendNotification(const TUint8* aTitle, TInt aTitleLength, const TUint8* aBody, TInt aBodyLength);
	TBool IsReady() const { return iReady; }

	// Limits (bytes of UTF-8 text) so a notification stays well under the 2044-byte inbound frame limit.
	enum { KMaxTitle = 64, KMaxBody = 512 };

private:
	void Handle(TUint16 aEndpoint, const TUint8* aPayload, TInt aLength);
	TInt SendFrame(TUint16 aEndpoint, const TUint8* aPayload, TInt aLength);

	MPebbleLink& iLink;
	MPebbleEnvironment& iEnv;
	MPebbleSessionObserver& iObserver;
	TPebbleFrameReader iReader;
	TUint16 iNextToken;
	TBool iReady;
	};

/** Cut a UTF-8 string to at most aMax bytes without splitting a character. */
TInt PebbleUtf8Cut(const TUint8* aText, TInt aLength, TInt aMax);

#endif
