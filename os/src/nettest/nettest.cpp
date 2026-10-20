// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// nettest.cpp -- OURS: the Path C networking smoke test (X4.48). Not Symbian source.
//
// Walks the whole user-side stack once, the way any Symbian application does, and logs each
// step to the debug port:
//   RSocketServ::Connect          ESOCK (c32start -> the root server -> the ESock_* CPMs)
//   RConnection::Start            CommsDat's preferred IAP -> nullagt -> the ethint NIF ->
//                                 EtherPkt.drv -> RBusDevEthernet -> the e1000 PDD
//   RHostResolver::GetByName      tcpip6's resolver -> dnd.exe -> DNS at 10.0.2.3 (the NAT's)
//   RSocket TCP connect + GET     tcpip6 -> the host over the NAT
// Command line: [host [port [iap]]], default example.com 80 and CommsDat's preferred IAP (X4.49: DHCP;
// iap 1 = the static one; X4.68: iap -1 = what BrowserNG does on S^3 -- a TConnPrefList holding a
// TExtendedConnPref for the Internet-purpose destination, through MPM/S60MCPR; iap -<n> = destination id n;
// a 4th argument = seconds to wait before starting). Every wait is bounded (KStepTimeout) so a step that never completes is
// reported, not hung on.
#include <e32base.h>
#include <e32debug.h>
#include <es_sock.h>
#include <in_sock.h>
#include <f32file.h>
#include <commdbconnpref.h>
#include <extendedconnpref.h>
#include <cmmanagerdef.h>

static const TInt KStepTimeout = 20 * 1000000;

// wait for aStatus or the timeout; on timeout the request is left to the caller to cancel
static TInt Await(TRequestStatus& aStatus, const char* aWhat)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, KStepTimeout);
	User::WaitForRequest(aStatus, ts);
	if (aStatus == KRequestPending)
		{
		RDebug::Printf("[nettest] %s: TIMED OUT after %d s", aWhat, KStepTimeout / 1000000);
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	RDebug::Printf("[nettest] %s: %d", aWhat, aStatus.Int());
	return aStatus.Int();
	}

// diagnostics: CED's own log (errors + the tail) and the root server's CPM threads
static void DumpCedLog()
	{
	RFs fs;
	if (fs.Connect() != KErrNone)
		return;
	RFile f;
	TInt r = f.Open(fs, _L("c:\\ced.log"), EFileRead | EFileShareReadersOnly);
	if (r != KErrNone)
		{
		RDebug::Printf("[nettest] c:\\ced.log: %d", r);
		fs.Close();
		return;
		}
	TInt size = 0;
	f.Size(size);
	HBufC8* buf = HBufC8::New(size);
	if (buf)
		{
		TPtr8 p = buf->Des();
		f.Read(p);
		// CED logs in UTF-16 or 8-bit depending on build: fold UTF-16 LE to 8-bit
		if (p.Length() > 1 && p[1] == 0)
			{
			TInt j = 0;
			for (TInt i = 0; i < p.Length(); i += 2)
				p[j++] = p[i];
			p.SetLength(j);
			}
		RDebug::Printf("[nettest] c:\\ced.log: %d bytes", p.Length());
		TInt shown = 0, lines = 0;
		for (TPtrC8 rest(p); rest.Length() && shown < 40; )
			{
			TInt nl = rest.Locate('\n');
			TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
			rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
			++lines;
			// (the Syborg file's CDMA2000 table is deprecated in this CommsDat: ~40 known ERR lines)
			TBool noise = line.FindF(_L8("deprecated")) >= 0;
			if ((!noise && (line.FindF(_L8("err")) >= 0 || line.FindF(_L8("fail")) >= 0
				|| line.FindF(_L8("warn")) >= 0)) || rest.Length() < 600)
				{
				TPtrC8 l = line.Left(150);
				RDebug::Printf("[nettest] ced.log %d| %S", lines, &l);
				++shown;
				}
			}
		delete buf;
		}
	f.Close();
	fs.Close();
	}

static void DumpThreads()
	{
	TFindThread ft(_L("c32exe*"));
	TFullName n;
	while (ft.Next(n) == KErrNone)
		{
		TBuf8<96> n8;
		n8.Copy(n.Left(96));
		RDebug::Printf("[nettest] thread %S", &n8);
		}
	}

static void RunL()
	{
	DumpThreads();
	TBuf<128> cmd;
	User::CommandLine(cmd);
	TLex lex(cmd);
	TPtrC host = lex.NextToken();
	if (!host.Length())
		host.Set(_L("example.com"));
	TInt port = 80;
	TLex(lex.NextToken()).Val(port);
	TInt iap = 0;
	TLex(lex.NextToken()).Val(iap);
	TInt delay = 0;                                     // X4.68: [delay s] -- start later, as an app does
	TLex(lex.NextToken()).Val(delay);
	if (delay > 0)
		{
		RDebug::Printf("[nettest] waiting %d s", delay);
		User::After(delay * 1000000);
		}
	TBuf8<128> host8;
	host8.Copy(host);
	RDebug::Printf("[nettest] start: %S port %d iap %d", &host8, port, iap);

	RSocketServ ss;
	TInt r = ss.Connect();
	RDebug::Printf("[nettest] RSocketServ::Connect: %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(ss);

	RConnection conn;
	r = conn.Open(ss);
	RDebug::Printf("[nettest] RConnection::Open: %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(conn);
	TRequestStatus st;
	TCommDbConnPref pref;
	TExtendedConnPref ext;
	TConnPrefList prefs;
	if (iap < 0)
		{
		if (iap == -1)
			ext.SetSnapPurpose(CMManager::ESnapPurposeInternet);
		else
			ext.SetSnapId(-iap);                        // iap -<n>: destination id n, as BrowserNG names it
		ext.SetNoteBehaviour(TExtendedConnPref::ENoteBehaviourDefault);
		prefs.AppendL(&ext);
		RDebug::Printf("[nettest] start via TConnPrefList + TExtendedConnPref (%s %d)", iap == -1 ? "Internet purpose" : "destination", -iap);
		conn.Start(prefs, st);
		}
	else if (iap)
		{
		pref.SetIapId(iap);
		pref.SetDialogPreference(ECommDbDialogPrefDoNotPrompt);
		conn.Start(pref, st);
		}
	else
		conn.Start(st);
	if ((r = Await(st, "RConnection::Start")) != KErrNone)
		{
		if (r == KErrTimedOut)
			conn.Stop();
		DumpCedLog();									// was the CommsDat import the problem?
		User::Leave(r);
		}

	RHostResolver hr;
	r = hr.Open(ss, KAfInet, KProtocolInetUdp, conn);
	RDebug::Printf("[nettest] RHostResolver::Open: %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(hr);
	TNameEntry ne;
	hr.GetByName(host, ne, st);
	if ((r = Await(st, "RHostResolver::GetByName")) != KErrNone)
		{
		if (r == KErrTimedOut)
			hr.Cancel();
		User::Leave(r);
		}
	TInetAddr addr(ne().iAddr);
	TBuf<64> a16;
	addr.Output(a16);
	TBuf8<64> a8;
	a8.Copy(a16);
	RDebug::Printf("[nettest] %S is %S", &host8, &a8);

	RSocket sock;
	r = sock.Open(ss, KAfInet, KSockStream, KProtocolInetTcp, conn);
	RDebug::Printf("[nettest] RSocket::Open(TCP): %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(sock);
	addr.SetPort(port);
	sock.Connect(addr, st);
	if ((r = Await(st, "RSocket::Connect")) != KErrNone)
		{
		if (r == KErrTimedOut)
			sock.CancelConnect();
		User::Leave(r);
		}
	// the address the interface got (static, or from the DHCP server)
	TInetAddr local;
	sock.LocalName(local);
	TBuf<64> l16;
	local.Output(l16);
	TBuf8<64> l8;
	l8.Copy(l16);
	RDebug::Printf("[nettest] local address %S port %d", &l8, local.Port());

	TBuf8<256> req;
	req.Format(_L8("GET / HTTP/1.0\r\nHost: %S\r\nUser-Agent: Phorepoc-nettest\r\n\r\n"), &host8);
	sock.Write(req, st);
	User::LeaveIfError(Await(st, "RSocket::Write"));

	// the status line + headers: print up to 6 lines of the first response segment
	TBuf8<512> resp;
	TSockXfrLength len;
	sock.RecvOneOrMore(resp, 0, st, len);
	if ((r = Await(st, "RSocket::RecvOneOrMore")) != KErrNone)
		{
		if (r == KErrTimedOut)
			sock.CancelRecv();
		User::Leave(r);
		}
	RDebug::Printf("[nettest] %d bytes received:", resp.Length());
	TPtrC8 rest(resp);
	for (TInt n = 0; n < 6 && rest.Length(); ++n)
		{
		TInt nl = rest.Locate('\n');
		TPtrC8 line = nl < 0 ? rest : rest.Left(nl);
		if (line.Length() && line[line.Length() - 1] == '\r')
			line.Set(line.Left(line.Length() - 1));
		RDebug::Printf("[nettest] | %S", &line);
		rest.Set(nl < 0 ? TPtrC8() : rest.Mid(nl + 1));
		}
	CleanupStack::PopAndDestroy(4, &ss);		// sock, hr, conn, ss
	RDebug::Printf("[nettest] PASS");
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* tc = CTrapCleanup::New();
	TRAPD(err, RunL());
	if (err != KErrNone)
		RDebug::Printf("[nettest] FAIL %d", err);
	delete tc;
	RProcess::Rendezvous(err);
	return err;
	}
