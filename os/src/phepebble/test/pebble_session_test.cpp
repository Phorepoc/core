// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pebble_session_test.cpp -- OURS: host test of TPPoGattClient + TPebbleSession over a fake LE transport. The watch side
// replays the packets a real Pebble 2 HR sent in our first session (2026-11-18): reset request, reset complete {13, 4},
// version request, app launcher message, ping, BlobDB answer. Prints RESULT: PASS / RESULT: FAIL.

#include <stdio.h>
#include <string.h>
#include <vector>
#include "../phepebble_ppogatt.h"
#include "../phepebble_session.h"

static int gChecks, gFails;
#define CHECK(c) do { ++gChecks; if (!(c)) { ++gFails; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

typedef std::vector<TUint8> Bytes;
static Bytes B(std::initializer_list<int> l) { Bytes b; for (int x : l) b.push_back(TUint8(x)); return b; }

class FakeTransport : public MPebbleLeTransport
	{
public:
	std::vector<Bytes> iWrites;      // writes to the Data-write characteristic
	TUint32 iSubscribed = 0;
	TInt iMtu = 339;
	TInt StartScan(const TUint8*, TInt, TInt) { return KErrNone; }
	void StopScan() {}
	TInt Connect(const TPebbleLeAddr&) { return KErrNone; }
	void Disconnect() {}
	TInt Pair() { return KErrNone; }
	TInt Read(TUint32) { return KErrNone; }
	TInt Write(TUint32 aChar, const TUint8* aValue, TInt aLength, TBool aWithResponse)
		{
		if (aChar == KPebbleCharRevDataWr && !aWithResponse)
			iWrites.push_back(Bytes(aValue, aValue + aLength));
		return KErrNone;
		}
	TInt Subscribe(TUint32 aChar, TBool) { iSubscribed = aChar; return KErrNone; }
	TInt Mtu() const { return iMtu; }
	};

class Harness : public MPPoGattObserver, public MPebbleLink, public MPebbleEnvironment, public MPebbleSessionObserver
	{
public:
	FakeTransport iT;
	TPPoGattClient iP;
	TPebbleSession iS;
	int iOpened = 0, iClosed = 0, iReady = 0, iLastReason = 0, iResults = 0;
	TUint16 iLastToken = 0; TUint8 iLastStatus = 0;
	Harness() : iP(iT, *this), iS(*this, *this, *this) {}
	// MPPoGattObserver
	void PPoGattOpened() { ++iOpened; iS.Reset(); }
	void PPoGattData(const TUint8* d, TInt n) { iS.Receive(d, n); }
	void PPoGattClosed(TInt r) { ++iClosed; iLastReason = r; }
	// MPebbleLink
	TInt LinkSend(const TUint8* d, TInt n) { return iP.Send(d, n); }
	// MPebbleEnvironment
	TInt32 UtcNow() { return 1795009114; }
	TInt16 UtcOffsetMinutes() { return 120; }
	TInt ZoneName(TUint8* o, TInt m) { const char* z = "Europe/Athens"; int n = (int)strlen(z); if (n > m) n = m; memcpy(o, z, n); return n; }
	void Random(TUint8* o, TInt n) { for (int i = 0; i < n; ++i) o[i] = TUint8(0x10 + i); }
	// MPebbleSessionObserver
	void SessionReady() { ++iReady; }
	void NotificationResult(TUint16 t, TUint8 s) { ++iResults; iLastToken = t; iLastStatus = s; }

	void Watch(const Bytes& b) { iP.HandleNotification(b.data(), (TInt)b.size()); }
	Bytes Last() { return iT.iWrites.empty() ? Bytes() : iT.iWrites.back(); }
	// Reassemble the PP stream we sent in data packets from write index aFrom on; returns the data packets' seqs too
	Bytes Stream(size_t aFrom, std::vector<int>* aSeqs = 0)
		{
		Bytes s;
		for (size_t i = aFrom; i < iT.iWrites.size(); ++i)
			{
			const Bytes& w = iT.iWrites[i];
			if ((w[0] & 7) == 0) { s.insert(s.end(), w.begin() + 1, w.end()); if (aSeqs) aSeqs->push_back(w[0] >> 3); }
			}
		return s;
		}
	};

static bool StartsWith(const Bytes& a, const Bytes& b) { return a.size() >= b.size() && memcmp(a.data(), b.data(), b.size()) == 0; }

static void Open(Harness& h)
	{
	CHECK(h.iP.Start() == KErrNone);
	CHECK(h.iT.iSubscribed == KPebbleCharRevData);
	h.iP.HandleSubscribed(KErrNone);
	// the real watch: reset request v1 + serial "C551446A2ND9"
	h.Watch(B({0x02, 0x01, 'C', '5', '5', '1', '4', '4', '6', 'A', '2', 'N', 'D', '9'}));
	CHECK(h.Last() == B({0x03, 25, 25}));
	CHECK(!h.iP.IsOpen());
	h.Watch(B({0x03, 0x0d, 0x04}));
	CHECK(h.iP.IsOpen() && h.iOpened == 1 && h.iP.TxWindow() == 13);
	}

static void TestHandshakeFromRealWatch()
	{
	Harness h;
	Open(h);
	size_t mark = h.iT.iWrites.size();
	// version request, data seq 0: frame {len 1, ep 0x0011, 0x00}
	h.Watch(B({0x00, 0x00, 0x01, 0x00, 0x11, 0x00}));
	CHECK(h.iT.iWrites[mark] == B({0x01}));                       // ack seq 0
	CHECK(h.iReady == 1 && h.iS.IsReady());
	std::vector<int> seqs;
	Bytes s = h.Stream(mark, &seqs);
	// version response (platform Linux, app 0.1.0, caps 0), the bytes the real watch accepted in P0
	Bytes ver = B({0x00, 0x15, 0x00, 0x11, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0x02, 0, 1, 0, 0, 0, 0, 0});
	CHECK(StartsWith(s, ver));
	// then Clock 0x03: UTC BE, +120 min, "Europe/Athens"
	Bytes clk = B({0x00, 0x15, 0x00, 0x0b, 0x03, 0x6a, 0xfd, 0xaa, 0x5a, 0x00, 0x78, 0x0d,
		'E', 'u', 'r', 'o', 'p', 'e', '/', 'A', 't', 'h', 'e', 'n', 's'});
	CHECK(s.size() == ver.size() + clk.size() && memcmp(s.data() + ver.size(), clk.data(), clk.size()) == 0);
	CHECK(seqs.size() == 2 && seqs[0] == 0 && seqs[1] == 1);      // one packet per frame, as written
	CHECK(h.iP.InFlight() == 2);
	h.Watch(B({0x09}));                                           // cumulative ack of seq 1
	CHECK(h.iP.InFlight() == 0);

	// app launcher message (seq 1) is acked and ignored; ping (seq 2) gets a pong
	h.Watch(B({0x08, 0x00, 0x1e, 0x00, 0x31, 0x01, 0x00, 0x07, 0xe0, 0xd9, 0xcb, 0x89, 0x57, 0x4b, 0xf7, 0x9d, 0x42, 0x35,
		0xbf, 0x47, 0xca, 0xad, 0xfe, 0x01, 0x01, 0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00}));
	CHECK(h.Last() == B({0x09}));
	mark = h.iT.iWrites.size();
	h.Watch(B({0x10, 0x00, 0x06, 0x07, 0xd1, 0x00, 0x00, 0x00, 0x00, 0x2a, 0x01}));
	CHECK(h.iT.iWrites[mark] == B({0x11}));
	CHECK(h.iT.iWrites[mark + 1] == B({0x10, 0x00, 0x05, 0x07, 0xd1, 0x01, 0x00, 0x00, 0x00, 0x2a}));   // pong, seq 2
	h.Watch(B({0x11}));

	// a notification: BlobDB insert into Notifs, answered {token, 0x01}
	mark = h.iT.iWrites.size();
	const char* title = "Phorepoc"; const char* body = "Hello from P0";
	TInt token = h.iS.SendNotification((const TUint8*)title, 8, (const TUint8*)body, 13);
	CHECK(token == 1);
	s = h.Stream(mark);
	CHECK(s.size() > 4 && s[2] == 0xb1 && s[3] == 0xdb && s[4] == 0x01 && s[5] == 0x01 && s[6] == 0x00 && s[7] == 0x04);
	CHECK((size_t)((s[0] << 8) | s[1]) + 4 == s.size());
	// value = 46-byte header + Title + Body + Dismiss action, key = item id (a v4 UUID)
	CHECK(s[8] == 16 && (s[9 + 6] & 0xf0) == 0x40);
	h.Watch(B({0x18, 0x00, 0x03, 0xb1, 0xdb, 0x01, 0x00, 0x01}));  // seq 3: the real watch's answer
	CHECK(h.iResults == 1 && h.iLastToken == 1 && h.iLastStatus == KPebbleBlobSuccess);

	// the watch asks for the time (Clock 0x04) -> we send 0x03 again
	mark = h.iT.iWrites.size();
	h.Watch(B({0x20, 0x00, 0x01, 0x00, 0x0b, 0x04}));
	s = h.Stream(mark);
	CHECK(s.size() == clk.size() && s[4] == 0x03);
	}

static void TestChunkingWindowAndResend()
	{
	Harness h;
	h.iT.iMtu = 23;                                               // minimum MTU: 19 payload bytes per packet
	Open(h);
	h.Watch(B({0x00, 0x00, 0x01, 0x00, 0x11, 0x00}));             // version request -> version + clock queued
	// the window is 13; the version (25 bytes) and clock (25 bytes) need 2 + 2 packets
	CHECK(h.iP.InFlight() == 4);
	h.Watch(B({(3 << 3) | 1}));
	CHECK(h.iP.InFlight() == 0);
	// a long notification: many packets, limited by the window
	Bytes big(400, 'x');
	size_t mark = h.iT.iWrites.size();
	CHECK(h.iS.SendNotification(big.data(), 30, big.data(), 400) > 0);
	CHECK(h.iP.InFlight() == 13);
	for (size_t i = mark; i < h.iT.iWrites.size(); ++i)
		CHECK(h.iT.iWrites[i].size() <= 20);
	// no ack: after 3 ticks the whole window goes out again, same bytes
	size_t before = h.iT.iWrites.size();
	h.iP.Tick(); h.iP.Tick();
	CHECK(h.iT.iWrites.size() == before);
	h.iP.Tick();
	CHECK(h.iT.iWrites.size() == before + 13 && h.iP.Resends() == 13);
	CHECK(h.iT.iWrites[before] == h.iT.iWrites[mark]);
	// ack all 13 (seqs 4..16): the rest flows
	h.Watch(B({(16 << 3) | 1}));
	CHECK(h.iP.InFlight() > 0 && h.iP.InFlight() <= 13);
	// keep acking until done; the stream must equal one full notification frame
	for (int guard = 0; guard < 20 && h.iP.InFlight() > 0; ++guard)
		{
		const Bytes& w = h.iT.iWrites.back();
		h.Watch(B({(w[0] & 0xf8) | 1}));
		}
	CHECK(h.iP.InFlight() == 0);
	Bytes s = h.Stream(before + 13);
	Bytes first = h.Stream(mark);                                  // includes the resent copies
	(void)first;
	// 13 packets were sent before the resend; the frame = those + everything after the resend
	Bytes whole;
	for (size_t i = mark; i < mark + 13; ++i) whole.insert(whole.end(), h.iT.iWrites[i].begin() + 1, h.iT.iWrites[i].end());
	whole.insert(whole.end(), s.begin(), s.end());
	CHECK(whole.size() >= 4 && (size_t)((whole[0] << 8) | whole[1]) + 4 == whole.size());
	// Title cut at 30, Body cut at KMaxBody (512 > 400: whole body)
	CHECK(whole.size() == 4 + 23 + 46 + (3 + 30) + (3 + 400) + (3 + 3 + 7));
	}

static void TestTimeoutAndReset()
	{
	Harness h;
	Open(h);
	h.Watch(B({0x00, 0x00, 0x01, 0x00, 0x11, 0x00}));
	for (int i = 0; i < 3 * (KPPoGattMaxTimeouts + 1); ++i)
		h.iP.Tick();
	CHECK(h.iClosed == 1 && h.iLastReason == KErrTimedOut && !h.iP.IsOpen());

	// a new session; then the watch resets in the middle: the session closes and reopens
	Harness g;
	Open(g);
	g.Watch(B({0x02, 0x01, 'X'}));
	CHECK(g.iClosed == 1 && g.iLastReason == KErrAbort && g.Last() == B({0x03, 25, 25}));
	g.Watch(B({0x03, 0x08, 0x04}));
	CHECK(g.iOpened == 2 && g.iP.TxWindow() == 8);
	// out-of-order data: re-ack the last good one (none yet: 31) and drop it
	g.Watch(B({(5 << 3), 0x00, 0x01, 0x00, 0x11, 0x00}));
	CHECK(g.Last() == B({(31 << 3) | 1}) && g.iReady == 0);
	}

static void TestUtf8Cut()
	{
	const TUint8 s[] = {'a', 0xc3, 0xa9, 'b'};       // "aéb"
	CHECK(PebbleUtf8Cut(s, 4, 2) == 1);
	CHECK(PebbleUtf8Cut(s, 4, 3) == 3);
	CHECK(PebbleUtf8Cut(s, 4, 9) == 4);
	}

int main()
	{
	TestHandshakeFromRealWatch();
	TestChunkingWindowAndResend();
	TestTimeoutAndReset();
	TestUtf8Cut();
	printf("== checks: %d, failures: %d ==\n", gChecks, gFails);
	printf(gFails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
	return gFails ? 1 : 0;
	}
