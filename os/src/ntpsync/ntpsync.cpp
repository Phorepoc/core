// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// ntpsync.cpp -- OURS: an SNTP client daemon (RFC 4330) for the Path C board (X4.50). Not Symbian source.
//
// A phone took its time from the cellular network (NITZ, through the telephony stack); the 2010 tree
// ships no NTP client, and a PC has no NITZ. This is the network time source X4.42's clock sync was
// waiting for: over the REAL stack (RConnection on CommsDat's preferred IAP, RHostResolver -> dnd,
// a UDP RSocket), ask an NTP server for the time, compute the clock offset from the four timestamps,
// and when it is 1 s or more, User::SetUTCTime -- which also writes the hardware RTC, so the kernel's
// own resync (xk_timesync.cpp: CMOS on a PC / QEMU, VMMDev host time on VirtualBox) agrees with it.
//
// Command line: [server [interval-seconds]], default pool.ntp.org 1800. Retries a failed sync after
// 60 s. Every step logs to the debug port; every wait is bounded.
#include <e32base.h>
#include <e32debug.h>
#include <es_sock.h>
#include <in_sock.h>
#include <commdbconnpref.h>

static const TInt KStepTimeout = 10 * 1000000;
static const TInt KNtpPort = 123;
static const TInt KRetrySeconds = 60;
static const TInt KSetThresholdMs = 1000;

static TInt Await(TRequestStatus& aStatus)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, KStepTimeout);
	User::WaitForRequest(aStatus, ts);
	if (aStatus == KRequestPending)
		{
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	return aStatus.Int();
	}

// NTP time = seconds (32) . fraction (32) since 1900-01-01 UTC
static TTime NtpEpoch()
	{
	return TTime(TDateTime(1900, EJanuary, 0, 0, 0, 0, 0));
	}

static TInt64 ToNtpMicro(const TTime& aTime)
	{
	return aTime.MicroSecondsFrom(NtpEpoch()).Int64();
	}

static TInt64 ReadNtpMicro(const TUint8* aP)
	{
	const TUint32 sec = aP[0] << 24 | aP[1] << 16 | aP[2] << 8 | aP[3];
	const TUint32 frac = aP[4] << 24 | aP[5] << 16 | aP[6] << 8 | aP[7];
	// (valid until 2036's era rollover: an era-0 reading only)
	return TInt64(sec) * 1000000 + ((TInt64(frac) * 1000000) >> 32);
	}

static void WriteNtp(TUint8* aP, TInt64 aMicro)
	{
	const TUint32 sec = TUint32(aMicro / 1000000);
	const TUint32 frac = TUint32(((aMicro % 1000000) << 32) / 1000000);
	for (TInt i = 0; i < 4; ++i)
		{
		aP[i] = TUint8(sec >> (24 - 8 * i));
		aP[4 + i] = TUint8(frac >> (24 - 8 * i));
		}
	}

static TInt SyncOnce(RSocketServ& aSs, RConnection& aConn, const TDesC& aServer)
	{
	RHostResolver hr;
	TInt r = hr.Open(aSs, KAfInet, KProtocolInetUdp, aConn);
	if (r != KErrNone)
		return r;
	TNameEntry ne;
	TRequestStatus st;
	hr.GetByName(aServer, ne, st);
	r = Await(st);
	if (r == KErrTimedOut)
		hr.Cancel();
	hr.Close();
	if (r != KErrNone)
		{
		RDebug::Printf("[ntpsync] resolve: %d", r);
		return r;
		}
	TInetAddr addr(ne().iAddr);
	addr.SetPort(KNtpPort);
	TBuf<40> a16;
	addr.Output(a16);
	TBuf8<40> a8;
	a8.Copy(a16);

	RSocket sock;
	r = sock.Open(aSs, KAfInet, KSockDatagram, KProtocolInetUdp, aConn);
	if (r != KErrNone)
		return r;
	TBuf8<48> pkt;
	pkt.FillZ(48);
	pkt[0] = 0x23;										// LI 0, version 4, mode 3 (client)
	TTime t1;
	t1.UniversalTime();
	WriteNtp(&pkt[40], ToNtpMicro(t1));					// our transmit time (echoed as "originate")
	sock.SendTo(pkt, addr, 0, st);
	r = Await(st);
	if (r == KErrNone)
		{
		TBuf8<68> resp;
		TInetAddr from;
		sock.RecvFrom(resp, from, 0, st);
		r = Await(st);
		if (r == KErrTimedOut)
			sock.CancelRecv();
		TTime t4;
		t4.UniversalTime();
		if (r == KErrNone && resp.Length() < 48)
			r = KErrCorrupt;
		if (r == KErrNone && ((resp[0] & 7) != 4 || resp[1] == 0 || resp[1] > 15))
			r = KErrNotReady;								// not a server reply, or unsynchronised (stratum 0/16)
		if (r == KErrNone)
			{
			const TInt64 T1 = ToNtpMicro(t1), T4 = ToNtpMicro(t4);
			const TInt64 T2 = ReadNtpMicro(&resp[32]), T3 = ReadNtpMicro(&resp[40]);
			const TInt64 offset = ((T2 - T1) + (T3 - T4)) / 2;
			const TInt64 delay = (T4 - T1) - (T3 - T2);
			const TInt64 offMs = offset / 1000;				// 64-bit: a dead CMOS battery is years off
			RDebug::Printf("[ntpsync] %S stratum %d: offset %Ld ms, round trip %d ms", &a8, resp[1],
						   offMs, TInt(delay / 1000));
			if (offMs >= KSetThresholdMs || offMs <= -KSetThresholdMs)
				{
				TTime now;
				now.UniversalTime();
				now += TTimeIntervalMicroSeconds(offset);
				r = User::SetUTCTime(now);
				RDebug::Printf("[ntpsync] clock set: %d", r);
				}
			}
		}
	if (r == KErrTimedOut)
		RDebug::Printf("[ntpsync] %S: no reply", &a8);
	else if (r != KErrNone)
		RDebug::Printf("[ntpsync] %S: %d", &a8, r);
	sock.Close();
	return r;
	}

static void RunL()
	{
	TBuf<128> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	TPtrC server = lex.NextToken();
	if (!server.Length())
		server.Set(_L("pool.ntp.org"));
	TInt interval = 1800;
	TLex(lex.NextToken()).Val(interval);
	TUint iap = 0;							// 3rd argument: this IAP (the PinePhone's WLAN), else the preferred one
	TLex(lex.NextToken()).Val(iap, EDecimal);
	TBuf8<64> s8;
	s8.Copy(server.Left(64));
	RDebug::Printf("[ntpsync] server %S, every %d s", &s8, interval);
	RProcess::Rendezvous(KErrNone);						// a daemon: don't hold the starter up

	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);
	RConnection conn;
	User::LeaveIfError(conn.Open(ss));
	CleanupClosePushL(conn);
	TBool up = EFalse;
	for (;;)
		{
		TInt r = KErrNone;
		if (!up)
			{
			TRequestStatus st;
			TCommDbConnPref pref;
			if (iap)
				{
				pref.SetIapId(iap);
				pref.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
				conn.Start(pref, st);
				}
			else
				conn.Start(st);
			r = Await(st);
			if (r == KErrTimedOut)
				conn.Stop();
			up = (r == KErrNone || r == KErrAlreadyExists);
			if (!up)
				RDebug::Printf("[ntpsync] RConnection::Start: %d", r);
			}
		if (up)
			r = SyncOnce(ss, conn, server);
		User::After((r == KErrNone ? interval : KRetrySeconds) * 1000000);
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	TRAPD(err, RunL());
	RDebug::Printf("[ntpsync] stopped: %d", err);
	delete tc;
	return err;
	}
