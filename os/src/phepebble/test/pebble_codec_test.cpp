// Copyright The Phorepoc Authors
// Portions Copyright 2024 Google LLC (the byte vectors marked "PebbleOS", from the PebbleOS tests)
// SPDX-License-Identifier: Apache-2.0

// pebble_codec_test.cpp -- OURS: host test of phepebble_codec against byte vectors from the PebbleOS source
// (github.com/coredevices/PebbleOS, Apache-2.0): tests/subsys/pp/test_version.c, tests/pebble/services/
// blob_db/test_blob_db_endpoint.c, tests/pebble/services/test_timeline_item.c, and the packed layouts of
// pebble/services/clock/service.c (TimezoneCBData) and include/pbl/services/timeline/item.h
// (SerializedTimelineItemHeader). Build: test/build_msvc.bat <vcvars64.bat> <outdir> (or any C++ compiler with -Itest/shim). Prints RESULT: PASS / RESULT: FAIL.

#include <stdio.h>
#include <string.h>
#include "../phepebble_codec.h"

static int gChecks, gFails;
#define CHECK(c) do { ++gChecks; if (!(c)) { ++gFails; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static bool Same(const TUint8* a, const TUint8* b, int n)
	{
	if (memcmp(a, b, n) == 0)
		return true;
	for (int i = 0; i < n; ++i)
		if (a[i] != b[i]) { printf("  differ at %d: %02x vs %02x\n", i, a[i], b[i]); break; }
	return false;
	}

static const TUint8* S(const char* s) { return reinterpret_cast<const TUint8*>(s); }

static void TestFrame()
	{
	TUint8 buf[64];
	const TUint8 payload[] = {0x00, 0x00, 0x00, 0x00, 0x2a};
	TInt n = PebbleCodec::EncodeFrame(KPebbleEpPing, payload, sizeof payload, buf, sizeof buf);
	const TUint8 want[] = {0x00, 0x05, 0x07, 0xd1, 0x00, 0x00, 0x00, 0x00, 0x2a};
	CHECK(n == (TInt)sizeof want && Same(buf, want, n));
	CHECK(PebbleCodec::EncodeFrame(1, payload, sizeof payload, buf, 8) == KErrOverflow);

	// The stream may split anywhere (PebbleOS wire protocol): feed two frames byte by byte.
	TPebbleFrameReader r;
	TUint8 two[32];
	TInt a = PebbleCodec::EncodeFrame(0x0011, S("\x00"), 1, two, sizeof two);
	TInt b = PebbleCodec::EncodeFrame(0xB1DB, S("\x01\x02\x03"), 3, two + a, sizeof two - a);
	TUint16 ep; const TUint8* pl; TInt len; int got = 0;
	for (TInt i = 0; i < a + b; ++i)
		{
		CHECK(r.Feed(two + i, 1) == 1);
		while (r.Next(ep, pl, len) == KErrNone)
			{
			if (got == 0) CHECK(ep == 0x0011 && len == 1 && pl[0] == 0);
			if (got == 1) CHECK(ep == 0xB1DB && len == 3 && pl[2] == 3);
			++got;
			}
		}
	CHECK(got == 2);
	const TUint8 huge[] = {0xff, 0xff, 0x00, 0x01};
	r.Feed(huge, 4);
	CHECK(r.Next(ep, pl, len) == KErrCorrupt);
	}

static void TestVersion()
	{
	// PebbleOS test_version.c, prv_receive_v3_response(): 0x01, HEADER_V1 (library 0, capabilities 0, platform
	// Android = 2, big endian), response version 2, app 3.2.1, protocol capabilities little endian.
	const TUint32 caps = 0x00000280;  // NOTIFICATION_FILTERING | VOICE_API style bits, any value works
	const TUint8 want[] = {
		0x01,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
		0x02, 3, 2, 1,
		caps & 0xff, (caps >> 8) & 0xff, (caps >> 16) & 0xff, (caps >> 24) & 0xff,
	};
	TUint8 buf[32];
	TInt n = PebbleCodec::EncodeVersionResponse(KPebbleOsAndroid, 3, 2, 1, caps, buf, sizeof buf);
	CHECK(n == (TInt)sizeof want && Same(buf, want, n));
	CHECK(PebbleCodec::IsVersionRequest(S("\x00"), 1));
	CHECK(!PebbleCodec::IsVersionRequest(S("\x01"), 1));
	CHECK(PebbleCodec::EncodeVersionResponse(KPebbleOsLinux, 0, 0, 0, 0, buf, 20) == KErrOverflow);
	}

static void TestPing()
	{
	// pebble/services/ping/service.c: {cmd 0, cookie BE, idle}; the reply is cmd 1 with the same cookie.
	const TUint8 ping[] = {0x00, 0x00, 0x00, 0x00, 0x2a, 0x01};
	TUint32 cookie = 0;
	CHECK(PebbleCodec::DecodePing(ping, sizeof ping, cookie) == KErrNone && cookie == 42);
	TUint8 buf[8];
	const TUint8 pong[] = {0x01, 0x00, 0x00, 0x00, 0x2a};
	CHECK(PebbleCodec::EncodePong(cookie, buf, sizeof buf) == 5 && Same(buf, pong, 5));
	CHECK(PebbleCodec::DecodePing(pong, 5, cookie) == KErrCorrupt);
	}

static void TestClock()
	{
	// clock/service.c case 0x03: TimezoneCBData {int32 utc BE, int16 offset min BE, int8 name length, name}, and
	// the length check "1 + header + name length" == message length.
	TUint8 buf[64];
	TInt n = PebbleCodec::EncodeSetUtcAndZone(1763467200, 60, S("Europe/Berlin"), 13, buf, sizeof buf);
	const TUint8 want[] = {0x03, 0x69, 0x1c, 0x5f, 0xc0, 0x00, 0x3c, 0x0d,
		'E', 'u', 'r', 'o', 'p', 'e', '/', 'B', 'e', 'r', 'l', 'i', 'n'};
	CHECK(n == (TInt)sizeof want && Same(buf, want, n));
	CHECK(n == 1 + 7 + buf[7]);
	n = PebbleCodec::EncodeSetUtcAndZone(0, -300, S(""), 0, buf, sizeof buf);
	const TUint8 neg[] = {0x03, 0, 0, 0, 0, 0xfe, 0xd4, 0x00};
	CHECK(n == 8 && Same(buf, neg, 8));
	n = PebbleCodec::EncodeSetLocalTime(0x01020304, buf, sizeof buf);
	const TUint8 local[] = {0x02, 0x01, 0x02, 0x03, 0x04};
	CHECK(n == 5 && Same(buf, local, 5));
	TUint32 t = 0;
	const TUint8 resp[] = {0x01, 0x69, 0x1c, 0x5f, 0xc0};
	CHECK(PebbleCodec::DecodeGetTimeResponse(resp, 5, t) == KErrNone && t == 1763467200u);
	CHECK(PebbleCodec::IsTimeRequest(S("\x04"), 1));
	}

static void TestBlobDb()
	{
	// PebbleOS test_blob_db_endpoint.c, s_insert_cmd_success: INSERT, token 0x0017, db 0x01, key size 16, UUID key,
	// value size 0x0140 (320), value = an 8-byte pattern 40 times.
	const TUint8 head[] = {0x01, 0x17, 0x00, 0x01, 0x10,
		0x6b, 0xf6, 0x21, 0x5b, 0xc9, 0x7f, 0x40, 0x9e, 0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4,
		0x40, 0x01};
	const TUint8 pat[] = {0x8c, 0x31, 0x4f, 0x55, 0x65, 0x72, 0x22, 0xb4};
	TUint8 value[320];
	for (int i = 0; i < 320; ++i) value[i] = pat[i % 8];
	TUint8 buf[400];
	TInt n = PebbleCodec::EncodeBlobInsert(0x0017, KPebbleBlobDbPins, head + 5, 16, value, 320, buf, sizeof buf);
	CHECK(n == 343 && Same(buf, head, sizeof head) && Same(buf + 23, value, 320));
	n = PebbleCodec::EncodeBlobDelete(0x0017, KPebbleBlobDbPins, head + 5, 16, buf, sizeof buf);
	CHECK(n == 21 && buf[0] == 0x04 && buf[1] == 0x17 && buf[3] == 0x01 && buf[4] == 16);
	TUint16 token; TUint8 status;
	const TUint8 resp[] = {0x17, 0x00, 0x01};
	CHECK(PebbleCodec::DecodeBlobResponse(resp, 3, token, status) == KErrNone && token == 0x17 &&
		status == KPebbleBlobSuccess);
	}

// PebbleOS test_timeline_item.c, s_payload_complete: Title, Subtitle, Body; action 0 (type 2) Title "Dismiss";
// action 1 (type 2) Title "Like" + attribute 7 (ANCS UID) = 0x01.
static const TUint8 KPayloadComplete[] = {
	0x01, 0x11, 0x00,
	0x54, 0x65, 0x73, 0x74, 0x20, 0x4e, 0x6f, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e,
	0x02, 0x08, 0x00, 'S', 'u', 'b', 't', 'i', 't', 'l', 'e',
	0x03, 0x3f, 0x00,
	0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0x20, 0x6e,
	0x6f, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2e, 0x20, 0x4c, 0x6f, 0x6f,
	0x6b, 0x20, 0x61, 0x74, 0x20, 0x69, 0x74, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x62, 0x65, 0x68, 0x6f,
	0x6c, 0x64, 0x20, 0x74, 0x68, 0x65, 0x20, 0x61, 0x77, 0x65, 0x73, 0x6f, 0x6d, 0x65, 0x2e,
	0x00, 0x02, 0x01, 0x01, 0x07, 0x00, 'D', 'i', 's', 'm', 'i', 's', 's',
	0x01, 0x02, 0x02, 0x01, 0x04, 0x00, 'L', 'i', 'k', 'e', 0x07, 0x01, 0x00, 0x01,
};

static void TestNotification()
	{
	const char* body = "This is a test notification. Look at it and behold the awesome.";
	const TPebbleAttr attrs[] = {
		{KPebbleAttrTitle, S("Test Notification"), 17},
		{KPebbleAttrSubtitle, S("Subtitle"), 8},
		{KPebbleAttrBody, S(body), (TUint16)strlen(body)},
	};
	const TPebbleAttr a0[] = {{KPebbleAttrTitle, S("Dismiss"), 7}};
	const TPebbleAttr a1[] = {{KPebbleAttrTitle, S("Like"), 4}, {7, S("\x01"), 1}};
	const TPebbleAction actions[] = {{0, KPebbleActionGeneric, a0, 1}, {1, KPebbleActionGeneric, a1, 2}};
	TPebbleItem item;
	memset(&item, 0, sizeof item);
	for (int i = 0; i < 16; ++i) item.iId[i] = TUint8(0xa0 + i);
	item.iTimestamp = 1763467200;
	item.iType = KPebbleItemNotification;
	item.iLayout = KPebbleLayoutNotification;
	item.iAttrs = attrs; item.iAttrCount = 3;
	item.iActions = actions; item.iActionCount = 2;

	TUint8 buf[512];
	TInt n = PebbleCodec::EncodeItemPayload(item, buf, sizeof buf);
	CHECK(n == (TInt)sizeof KPayloadComplete && Same(buf, KPayloadComplete, n));

	// SerializedTimelineItemHeader (packed, LE): id 16, parent 16, timestamp 4, duration 2, type, flags, status,
	// layout, payload length 2, attribute count, action count = 46 bytes.
	n = PebbleCodec::EncodeItem(item, buf, sizeof buf);
	CHECK(n == KPebbleItemHeaderSize + (TInt)sizeof KPayloadComplete);
	CHECK(buf[0] == 0xa0 && buf[15] == 0xaf && buf[16] == 0 && buf[31] == 0);
	const TUint8 mid[] = {0xc0, 0x5f, 0x1c, 0x69, 0x00, 0x00, 0x01, 0x00, 0x00, 0x04,
		(TUint8)sizeof KPayloadComplete, 0x00, 0x03, 0x02};
	CHECK(Same(buf + 32, mid, sizeof mid));
	CHECK(Same(buf + 46, KPayloadComplete, sizeof KPayloadComplete));

	TUint8 ins[600];
	TInt m = PebbleCodec::EncodeNotificationInsert(0x1234, item, ins, sizeof ins);
	CHECK(m == 23 + n);
	CHECK(ins[0] == 0x01 && ins[1] == 0x34 && ins[2] == 0x12 && ins[3] == KPebbleBlobDbNotifs && ins[4] == 16);
	CHECK(Same(ins + 5, item.iId, 16) && ins[21] == (n & 0xff) && ins[22] == (n >> 8) && Same(ins + 23, buf, n));
	CHECK(PebbleCodec::EncodeNotificationInsert(1, item, ins, 100) == KErrOverflow);
	}

int main()
	{
	TestFrame();
	TestVersion();
	TestPing();
	TestClock();
	TestBlobDb();
	TestNotification();
	printf("== checks: %d, failures: %d ==\n", gChecks, gFails);
	printf(gFails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return gFails ? 1 : 0;
	}
