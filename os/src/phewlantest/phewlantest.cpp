// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phewlantest.cpp -- OURS: the WLAN regression benchmark. Run from a shell (or by phestart) at any time, as often as
// wanted; every number goes to the log as "[phewlantest] ..." and to C:\data\phewlantest.txt.
//
// Per cycle (cycles = the reconnect test: every cycle after the first starts from the idle disconnect of the last):
//   scan      CWlanMgmtClient::GetScanResults -- the scan every UI (WLAN app, Qt bearer, ConnMon) gets: frames,
//             distinct BSSIDs, the target SSID seen (best RX level), time
//   iaps      GetAvailableIaps -- what ConnMon / the Qt bearer offer apps: is the target IAP usable right now
//   connect   RConnection::Start() WITHOUT preferences -- the connection an application asks for (the default
//             destination; MPM picks the IAP), timed; then the IAP it got and the WLAN connection mode
//   dns       RHostResolver::GetByName, timed
//   http      GET / from <host>: time to the first response byte, the status line
//   latency   <n> TCP connects to <host>:80: min / mean / max ms, failures
//   download  GET <bigpath> from <bighost>: bytes, KB/s
//   release   everything closed; then the time until the WLAN engine leaves the network (idle timeout), bounded
// Ends "[phewlantest] RESULT PASS|FAIL cycles c/n ...". Every wait is bounded.
//
//   phewlantest [cycles <n>] [ssid <name>] [host <name>] [bighost <name>] [bigpath </path>] [pings <n>] [idle <s>]
#include <e32base.h>
#include <e32debug.h>
#include <f32file.h>
#include <es_sock.h>
#include <in_sock.h>
#include <commdb.h>
#include <cdbcols.h>
#include <wlanmgmtclient.h>
#include <wlanscaninfo.h>

static const TInt KConnectTimeout = 150;		// s: scan + join tries + DHCP (wlanjoin's bound)
static const TInt KStepTimeout = 30;			// s: DNS, a TCP connect, one receive
static const TInt KDownloadTimeout = 120;		// s: the whole download

_LIT8(KPass, "PASS");
_LIT8(KFail, "FAIL");
_LIT8(KAvailable, "AVAILABLE");
_LIT8(KNotAvailable, "NOT available");
_LIT8(KStillConnected, " (still connected: idle bound reached)");

static RFile LogFile;
static TBool LogOpen;

static void Log(TRefByValue<const TDesC8> aFmt, ...)
	{
	VA_LIST list;
	VA_START(list, aFmt);
	TBuf8<256> b;
	b.Copy(_L8("[phewlantest] "));
	b.AppendFormatList(aFmt, list);
	RDebug::Printf("%S", &b);
	if (LogOpen)
		{
		b.Append('\n');
		LogFile.Write(b);
		}
	}

static TInt64 NowUs()
	{
	TTime t;
	t.UniversalTime();
	return t.Int64();
	}

static TInt Ms(TInt64 aStart)
	{
	return (TInt)((NowUs() - aStart) / 1000);
	}

// wait for aStatus at most aSeconds; KErrTimedOut (the request still pending) else its completion code
static TInt Await(TRequestStatus& aStatus, TInt aSeconds)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, aSeconds * 1000000);
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

struct TArgs
	{
	TInt iCycles;
	TBuf8<32> iSsid;
	TBuf<64> iHost;
	TBuf<64> iBigHost;
	TBuf8<128> iBigPath;
	TInt iPings;
	TInt iIdle;
	};

static void ParseArgs(TArgs& aArgs)
	{
	aArgs.iCycles = 3;
	aArgs.iSsid.Copy(_L8("ASUS"));
	aArgs.iHost.Copy(_L("example.com"));
	aArgs.iBigHost.Copy(_L("ipv4.download.thinkbroadband.com"));
	aArgs.iBigPath.Copy(_L8("/1MB.zip"));
	aArgs.iPings = 10;
	aArgs.iIdle = 240;
	TBuf<256> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	while (!lex.Eos())
		{
		TPtrC key = lex.NextToken();
		TPtrC val = lex.NextToken();
		if (!val.Length())
			break;
		TLex num(val);
		if (key == _L("cycles")) num.Val(aArgs.iCycles);
		else if (key == _L("pings")) num.Val(aArgs.iPings);
		else if (key == _L("idle")) num.Val(aArgs.iIdle);
		else if (key == _L("ssid")) aArgs.iSsid.Copy(val.Left(32));
		else if (key == _L("host")) aArgs.iHost.Copy(val.Left(64));
		else if (key == _L("bighost")) aArgs.iBigHost.Copy(val.Left(64));
		else if (key == _L("bigpath")) aArgs.iBigPath.Copy(val.Left(128));
		}
	}

// the IAP named aName in CommsDat (0 = none)
static TUint32 IapByNameL(const TDesC& aName)
	{
	CCommsDatabase* db = CCommsDatabase::NewL();
	CleanupStack::PushL(db);
	CCommsDbTableView* v = db->OpenViewMatchingTextLC(TPtrC(IAP), TPtrC(COMMDB_NAME), aName);
	TUint32 id = 0;
	if (v->GotoFirstRecord() == KErrNone)
		v->ReadUintL(TPtrC(COMMDB_ID), id);
	CleanupStack::PopAndDestroy(2);		// v, db
	return id;
	}

// scan: frames, distinct BSSIDs, the target SSID's best RX level; ETrue when the SSID was seen
static TBool ScanL(CWlanMgmtClient& aMgmt, const TDesC8& aSsid)
	{
	CWlanScanInfo* info = CWlanScanInfo::NewL();
	CleanupStack::PushL(info);
	TInt64 t0 = NowUs();
	TRequestStatus st;
	aMgmt.GetScanResults(st, *info);
	TInt r = Await(st, KConnectTimeout);
	if (r == KErrTimedOut)
		{
		aMgmt.CancelGetScanResults();
		User::WaitForRequest(st);
		}
	TInt ms = Ms(t0);
	TInt frames = 0, target = 0, best = 0;
	RArray<TWlanBssid> seen;
	CleanupClosePushL(seen);
	if (r == KErrNone)
		{
		for (info->First(); !info->IsDone(); info->Next())
			{
			++frames;
			TWlanBssid bssid;
			info->Bssid(bssid);
			TBool dup = EFalse;
			for (TInt i = 0; i < seen.Count() && !dup; ++i)
				dup = seen[i] == bssid;
			if (!dup)
				seen.Append(bssid);
			TUint8 len = 0;
			const TUint8* data = NULL;
			if (info->InformationElement(0, len, &data) == KErrNone && TPtrC8(data, len) == aSsid)
				{
				++target;
				if (info->RXLevel() > best)
					best = info->RXLevel();
				}
			}
		}
	Log(_L8("scan: r=%d %d ms, %d frames, %d BSSIDs, \"%S\" %d frames, best rx level %d"),
		r, ms, frames, seen.Count(), &aSsid, target, best);
	CleanupStack::PopAndDestroy(2);		// seen, info
	return r == KErrNone && target > 0;
	}

// the IAPs the engine reports usable now; ETrue when aIap is among them
static TBool AvailableIaps(CWlanMgmtClient& aMgmt, TUint32 aIap)
	{
	RArray<TUint> iaps;
	TInt64 t0 = NowUs();
	TRequestStatus st;
	aMgmt.GetAvailableIaps(st, iaps);
	TInt r = Await(st, KConnectTimeout);
	if (r == KErrTimedOut)
		{
		aMgmt.CancelGetAvailableIaps();
		User::WaitForRequest(st);
		}
	TBuf8<120> list;
	TBool found = EFalse;
	for (TInt i = 0; i < iaps.Count() && list.Length() < 100; ++i)
		{
		list.AppendFormat(_L8(" %u"), iaps[i]);
		found = found || iaps[i] == aIap;
		}
	Log(_L8("iaps: r=%d %d ms, %d available:%S; target iap %u %S"), r, Ms(t0), iaps.Count(), &list, aIap,
		found ? &KAvailable() : &KNotAvailable());
	iaps.Close();
	return found;
	}

static TInt Resolve(RSocketServ& aSs, RConnection& aConn, const TDesC& aHost, TInetAddr& aAddr, TInt& aMs)
	{
	RHostResolver hr;
	TInt r = hr.Open(aSs, KAfInet, KProtocolInetUdp, aConn);
	if (r != KErrNone)
		return r;
	TNameEntry ne;
	TRequestStatus st;
	TInt64 t0 = NowUs();
	hr.GetByName(aHost, ne, st);
	r = Await(st, KStepTimeout);
	if (r == KErrTimedOut)
		{
		hr.Cancel();
		User::WaitForRequest(st);
		}
	aMs = Ms(t0);
	if (r == KErrNone)
		aAddr = TInetAddr(ne().iAddr);
	hr.Close();
	return r;
	}

static TInt TcpConnect(RSocketServ& aSs, RConnection& aConn, RSocket& aSock, TInetAddr aAddr, TInt& aMs)
	{
	TInt r = aSock.Open(aSs, KAfInet, KSockStream, KProtocolInetTcp, aConn);
	if (r != KErrNone)
		return r;
	aAddr.SetPort(80);
	TRequestStatus st;
	TInt64 t0 = NowUs();
	aSock.Connect(aAddr, st);
	r = Await(st, KStepTimeout);
	if (r == KErrTimedOut)
		{
		aSock.CancelConnect();
		User::WaitForRequest(st);
		}
	aMs = Ms(t0);
	if (r != KErrNone)
		aSock.Close();
	return r;
	}

// GET aPath from aHost on an open socket; aMaxBytes = stop after that many (0 = the whole body); returns the bytes
// received, aFirstMs = time to the first byte, aStatus = the status line
static TInt HttpGet(RSocket& aSock, const TDesC& aHost, const TDesC8& aPath, TInt aTimeout, TInt& aFirstMs,
		TDes8& aStatus, TInt& aErr)
	{
	TBuf8<256> req;
	TBuf8<64> host;
	host.Copy(aHost);
	req.Format(_L8("GET %S HTTP/1.0\r\nHost: %S\r\nUser-Agent: Phorepoc-phewlantest\r\nConnection: close\r\n\r\n"),
		&aPath, &host);
	TRequestStatus st;
	TInt64 t0 = NowUs();
	aSock.Write(req, st);
	aErr = Await(st, KStepTimeout);
	if (aErr != KErrNone)
		return 0;
	aFirstMs = -1;
	TInt total = 0;
	TInt64 deadline = t0 + (TInt64)aTimeout * 1000000;
	HBufC8* buf = HBufC8::New(16384);
	if (!buf)
		{
		aErr = KErrNoMemory;
		return 0;
		}
	TPtr8 p = buf->Des();
	for (;;)
		{
		TInt left = (TInt)((deadline - NowUs()) / 1000000);
		if (left <= 0)
			{
			aErr = KErrTimedOut;
			break;
			}
		TSockXfrLength len;
		p.Zero();
		aSock.RecvOneOrMore(p, 0, st, len);
		TInt r = Await(st, left < KStepTimeout ? left : KStepTimeout);
		if (r == KErrTimedOut)
			{
			aSock.CancelRecv();
			User::WaitForRequest(st);
			aErr = KErrTimedOut;
			break;
			}
		if (r == KErrEof)
			{
			aErr = KErrNone;
			break;
			}
		if (r != KErrNone)
			{
			aErr = r;
			break;
			}
		if (aFirstMs < 0)
			{
			aFirstMs = Ms(t0);
			TInt nl = p.Locate('\r');
			aStatus.Copy(p.Left(nl < 0 ? Min(p.Length(), aStatus.MaxLength()) : Min(nl, aStatus.MaxLength())));
			}
		total += p.Length();
		}
	delete buf;
	return total;
	}

// the WLAN connection mode: 0 = not connected
static TInt WlanMode(CWlanMgmtClient& aMgmt)
	{
	TWlanConnectionMode mode = EWlanConnectionModeNotConnected;
	aMgmt.GetConnectionMode(mode);
	return mode;
	}

// one cycle; ETrue = every step passed
static TBool CycleL(TInt aN, const TArgs& aArgs, CWlanMgmtClient& aMgmt, TUint32 aIap)
	{
	Log(_L8("cycle %d: wlan mode %d (0 = not connected)"), aN, WlanMode(aMgmt));
	TBool ok = ETrue;
	if (!ScanL(aMgmt, aArgs.iSsid))
		ok = EFalse;
	if (!AvailableIaps(aMgmt, aIap))
		ok = EFalse;
	RSocketServ ss;
	User::LeaveIfError(ss.Connect());
	CleanupClosePushL(ss);
	RConnection conn;
	User::LeaveIfError(conn.Open(ss));
	CleanupClosePushL(conn);
	TRequestStatus st;
	TInt64 t0 = NowUs();
	conn.Start(st);					// no preferences: what an application's connection does
	TInt r = Await(st, KConnectTimeout);
	if (r == KErrTimedOut)
		{
		conn.Stop();
		User::WaitForRequest(st);
		}
	TUint32 got = 0;
	if (r == KErrNone)
		conn.GetIntSetting(_L("IAP\\Id"), got);
	TInt32 quality = 0;
	aMgmt.GetConnectionSignalQuality(quality);
	Log(_L8("connect: r=%d %d ms, iap %u, wlan mode %d, signal quality %d"), r, Ms(t0), got, WlanMode(aMgmt), quality);
	if (r != KErrNone)
		{
		CleanupStack::PopAndDestroy(2);		// conn, ss
		Log(_L8("cycle %d: FAIL (no connection)"), aN);
		return EFalse;
		}
	// dns
	TInetAddr addr;
	TInt dnsMs = 0;
	r = Resolve(ss, conn, aArgs.iHost, addr, dnsMs);
	TBuf<40> a16;
	addr.Output(a16);
	TBuf8<40> a8;
	a8.Copy(a16);
	Log(_L8("dns: r=%d %d ms -> %S"), r, dnsMs, &a8);
	if (r != KErrNone)
		ok = EFalse;
	// http
	if (r == KErrNone)
		{
		RSocket sock;
		TInt cms = 0;
		TInt cr = TcpConnect(ss, conn, sock, addr, cms);
		TInt first = -1, err = cr;
		TBuf8<48> status;
		TInt bytes = 0;
		if (cr == KErrNone)
			{
			bytes = HttpGet(sock, aArgs.iHost, _L8("/"), KStepTimeout, first, status, err);
			sock.Close();
			}
		Log(_L8("http: r=%d connect %d ms, first byte %d ms, %d bytes, \"%S\""), err, cms, first, bytes, &status);
		if (err != KErrNone || !bytes)
			ok = EFalse;
		// latency: TCP connects
		TInt mn = KMaxTInt, mx = 0, sum = 0, n = 0, fails = 0;
		for (TInt i = 0; i < aArgs.iPings; ++i)
			{
			RSocket s;
			TInt ms = 0;
			if (TcpConnect(ss, conn, s, addr, ms) == KErrNone)
				{
				s.Close();
				mn = Min(mn, ms);
				mx = Max(mx, ms);
				sum += ms;
				++n;
				}
			else
				++fails;
			}
		Log(_L8("latency: %d TCP connects, min %d ms, mean %d ms, max %d ms, %d failed"), aArgs.iPings,
			n ? mn : -1, n ? sum / n : -1, n ? mx : -1, fails);
		if (fails)
			ok = EFalse;
		}
	// download
	{
	TInetAddr big;
	TInt bms = 0;
	r = Resolve(ss, conn, aArgs.iBigHost, big, bms);
	TInt bytes = 0, first = -1, err = r, cms = 0;
	TBuf8<48> status;
	TInt64 d0 = NowUs();
	if (r == KErrNone)
		{
		RSocket sock;
		err = TcpConnect(ss, conn, sock, big, cms);
		if (err == KErrNone)
			{
			bytes = HttpGet(sock, aArgs.iBigHost, aArgs.iBigPath, KDownloadTimeout, first, status, err);
			sock.Close();
			}
		}
	TInt ms = Ms(d0);
	Log(_L8("download: r=%d %d bytes in %d ms = %d KB/s (dns %d ms, connect %d ms, first byte %d ms) \"%S\""), err, bytes,
		ms, ms ? (TInt)((TInt64)bytes * 1000 / 1024 / ms) : 0, bms, cms, first, &status);
	if (err != KErrNone || bytes < 1024)
		ok = EFalse;
	}
	// release, then the engine's own idle disconnect
	CleanupStack::PopAndDestroy(2);		// conn, ss
	TInt64 r0 = NowUs();
	TInt mode = WlanMode(aMgmt);
	while (mode != EWlanConnectionModeNotConnected && Ms(r0) < aArgs.iIdle * 1000)
		{
		User::After(2000000);
		mode = WlanMode(aMgmt);
		}
	Log(_L8("release: wlan mode %d after %d ms%S"), mode, Ms(r0),
		mode == EWlanConnectionModeNotConnected ? &KNullDesC8() : &KStillConnected());
	Log(_L8("cycle %d: %S"), aN, ok ? &KPass() : &KFail());
	return ok;
	}

static void RunL()
	{
	TArgs args;
	ParseArgs(args);
	TBuf<32> ssid16;
	ssid16.Copy(args.iSsid);
	TUint32 iap = IapByNameL(ssid16);
	TBuf8<64> host8, big8;
	host8.Copy(args.iHost);
	big8.Copy(args.iBigHost);
	Log(_L8("start: %d cycles, ssid \"%S\" (iap %u), host %S, download %S%S, %d pings, idle bound %d s"), args.iCycles,
		&args.iSsid, iap, &host8, &big8, &args.iBigPath, args.iPings, args.iIdle);
	CWlanMgmtClient* mgmt = CWlanMgmtClient::NewL();
	CleanupStack::PushL(mgmt);
	TInt passed = 0;
	for (TInt c = 1; c <= args.iCycles; ++c)
		{
		TBool ok = EFalse;
		TRAPD(err, ok = CycleL(c, args, *mgmt, iap));
		if (err != KErrNone)
			Log(_L8("cycle %d: left %d"), c, err);
		if (ok)
			++passed;
		}
	CleanupStack::PopAndDestroy(mgmt);
	Log(_L8("RESULT %S cycles %d/%d"), passed == args.iCycles ? &KPass() : &KFail(), passed, args.iCycles);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	RFs fs;
	if (fs.Connect() == KErrNone)
		{
		fs.MkDirAll(_L("C:\\data\\"));
		LogOpen = LogFile.Replace(fs, _L("C:\\data\\phewlantest.txt"), EFileWrite | EFileShareAny) == KErrNone;
		}
	TRAPD(err, RunL());
	if (err != KErrNone)
		Log(_L8("RESULT FAIL left %d"), err);
	if (LogOpen)
		LogFile.Close();
	fs.Close();
	delete tc;
	return err;
	}
