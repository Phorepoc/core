// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phestdio.cpp -- OURS: the Open C stdio server ("stdioserver", started by libc's stdio redirection on a program's
// first read/write of fd 0/1/2; the protocol of the 2010 client, backend/inc/StdioClient.h). Where the 2010 server
// opened a console window (or the media of a config.ini), this one routes each client by its process:
//   - a program a phetty shell started (pheshell adopts it: phettyd EPtTtyOf): stdout/stderr are written to that tty
//     (LF -> CR LF), stdin reads that tty's keys as an edited line (echo, backspace; Ctrl-D = end of file);
//   - anything else: its output becomes "PH: [stdio] ..." debug-log lines (PhoneLab sees them), its stdin is empty.
#include <e32base.h>
#include <e32debug.h>
#include <e32keys.h>
#include "../phetty/phetty.h"

_LIT(KStdioServerName, "stdioserver");

// the 2010 client's requests (backend/inc/StdioClient.h, TRedirStream)
enum TStdioOp { ERead, EWrite, EFlush, ECheckMedia, ENotifyActivity, ECancelNotify, EEcho };

class CStdioSession;

class CStdioServer : public CServer2
	{
public:
	static CStdioServer* NewLC()
		{
		CStdioServer* s = new (ELeave) CStdioServer;
		CleanupStack::PushL(s);
		s->StartL(KStdioServerName);
		s->iTtyOk = s->iTty.ConnectNoStart() == KErrNone;
		return s;
		}
	~CStdioServer() { iTty.Close(); }
	CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;
	// the tty a client process belongs to, or < 0
	TInt TtyOf(const RMessage2& aMsg)
		{
		if (!iTtyOk)
			iTtyOk = iTty.ConnectNoStart() == KErrNone;   // (phettyd may start after us)
		if (!iTtyOk)
			return KErrNotFound;
		RThread client;
		if (aMsg.Client(client) != KErrNone)
			return KErrNotFound;
		RProcess p;
		TInt r = client.Process(p);
		client.Close();
		if (r != KErrNone)
			return r;
		TUint pid = (TUint)p.Id();
		p.Close();
		TInt tty = iTty.TtyOf(pid);
		if (tty < 0)
			RDebug::Printf("PH: [stdio] process %u: no tty (%d)", pid, tty);
		return tty;
		}
	RPheTty& Tty() { return iTty; }
private:
	CStdioServer() : CServer2(EPriorityStandard, ESharableSessions) {}
	RPheTty iTty;
	TBool iTtyOk;
	};

// reads one key at a time from the tty (phettyd EPtConRead) for a pending stdin read
class CKeyReader : public CActive
	{
public:
	CKeyReader(CStdioSession& aOwner) : CActive(EPriorityStandard), iOwner(aOwner) { CActiveScheduler::Add(this); }
	~CKeyReader() { Cancel(); }
	void Start(RPheTty& aTty, TInt aId) { iTty = &aTty; aTty.ConRead(aId, iKey, iStatus); SetActive(); }
private:
	void RunL();
	void DoCancel() { iTty->ConReadCancel(); }
	CStdioSession& iOwner;
	RPheTty* iTty;
	TPheKeyBuf iKey;
	};

class CStdioSession : public CSession2
	{
public:
	~CStdioSession()
		{
		delete iKeys;
		if (!iRead.IsNull())
			iRead.Complete(KErrCancel);
		FlushLog();
		}
	void ServiceL(const RMessage2& aMessage);
	void KeyL(TInt aStatus, const TPheKey& aKey);
private:
	CStdioServer& Srv() { return *const_cast<CStdioServer*>(static_cast<const CStdioServer*>(Server())); }
	void Resolve(const RMessage2& aMsg)
		{
		if (!iResolved)
			{
			iTtyId = Srv().TtyOf(aMsg);
			iResolved = ETrue;
			}
		}
	TInt WriteL(const RMessage2& aMsg);
	void ReadL(const RMessage2& aMsg);
	void Echo(const TDesC8& aBytes) { Srv().Tty().ConWrite(iTtyId, aBytes); }
	void DeliverRead();
	void FlushLog()
		{
		if (iLog.Length())
			{
			RDebug::Printf("PH: [stdio] %S", &iLog);
			iLog.Zero();
			}
		}

	TBool iResolved;
	TInt iTtyId;
	TBuf8<200> iLog;            // the current output line of a client without a tty
	RMessage2 iRead;            // the pending stdin read
	TBuf8<512> iLine;           // the line being edited
	TBuf8<512> iReady;          // completed input not yet handed to the client
	TBool iEof;
	CKeyReader* iKeys;
	};

CSession2* CStdioServer::NewSessionL(const TVersion& /*aVersion*/, const RMessage2& /*aMessage*/) const
	{
	return new (ELeave) CStdioSession;
	}

void CKeyReader::RunL()
	{
	iOwner.KeyL(iStatus.Int(), iKey());
	}

TInt CStdioSession::WriteL(const RMessage2& aMsg)
	{
	TInt total = aMsg.Int1();
	HBufC8* buf = HBufC8::NewLC(total);
	TPtr8 p = buf->Des();
	aMsg.ReadL(0, p);
	if (iTtyId > 0)
		{
		// LF -> CR LF in chunks phettyd takes whole (4 KB)
		TBuf8<2048> out;
		for (TInt i = 0; i < p.Length(); ++i)
			{
			if (p[i] == '\n')
				out.Append('\r');
			out.Append(p[i]);
			if (out.Length() >= out.MaxLength() - 2)
				{
				Srv().Tty().ConWrite(iTtyId, out);
				out.Zero();
				}
			}
		if (out.Length())
			Srv().Tty().ConWrite(iTtyId, out);
		}
	else
		{
		for (TInt i = 0; i < p.Length(); ++i)
			{
			if (p[i] == '\n')
				FlushLog();
			else if (p[i] != '\r')
				{
				if (iLog.Length() == iLog.MaxLength())
					FlushLog();
				iLog.Append(p[i]);
				}
			}
		}
	TInt n = p.Length();
	CleanupStack::PopAndDestroy(buf);
	return n;
	}

void CStdioSession::ReadL(const RMessage2& aMsg)
	{
	if (iTtyId <= 0)
		{
		aMsg.Complete(KErrEof);             // no terminal: stdin is empty
		return;
		}
	if (!iRead.IsNull())
		{
		aMsg.Complete(KErrInUse);
		return;
		}
	iRead = aMsg;
	if (iReady.Length() || iEof)
		{
		DeliverRead();
		return;
		}
	if (!iKeys)
		iKeys = new (ELeave) CKeyReader(*this);
	if (!iKeys->IsActive())
		iKeys->Start(Srv().Tty(), iTtyId);
	}

void CStdioSession::DeliverRead()
	{
	if (iRead.IsNull())
		return;
	if (iReady.Length() == 0 && iEof)
		{
		iEof = EFalse;
		TInt r = iRead.Write(0, KNullDesC8);
		iRead.Complete(r == KErrNone ? KErrEof : r);
		return;
		}
	TInt max = iRead.Int1();
	TPtrC8 give = iReady.Left(max);
	TInt r = iRead.Write(0, give);
	iReady.Delete(0, give.Length());
	iRead.Complete(r);
	}

void CStdioSession::KeyL(TInt aStatus, const TPheKey& aKey)
	{
	if (aStatus != KErrNone)
		{
		iEof = ETrue;                        // the tty went away
		DeliverRead();
		return;
		}
	TUint c = aKey.iCode;
	if (c == EKeyEnter || c == '\r' || c == '\n')
		{
		Echo(_L8("\r\n"));
		iLine.Append('\n');
		iReady.Append(iLine.Left(iReady.MaxLength() - iReady.Length()));
		iLine.Zero();
		DeliverRead();
		}
	else if (c == EKeyBackspace || c == 0x7F)
		{
		if (iLine.Length())
			{
			iLine.SetLength(iLine.Length() - 1);
			Echo(_L8("\b \b"));
			}
		}
	else if (c == 4)                         // Ctrl-D: end of file (after the current line, if any)
		{
		if (iLine.Length())
			{
			iReady.Append(iLine);
			iLine.Zero();
			}
		iEof = ETrue;
		DeliverRead();
		}
	else if (c >= 0x20 && c < 0x7F && iLine.Length() < iLine.MaxLength() - 1)
		{
		TBuf8<1> ch;
		ch.Append((TUint8)c);
		iLine.Append(ch);
		Echo(ch);
		}
	// keep reading while a read waits for a whole line
	if (!iRead.IsNull() && !iKeys->IsActive())
		iKeys->Start(Srv().Tty(), iTtyId);
	}

void CStdioSession::ServiceL(const RMessage2& aMessage)
	{
	Resolve(aMessage);
	switch (aMessage.Function())
		{
	case EWrite:
		aMessage.Complete(WriteL(aMessage));
		break;
	case ERead:
		ReadL(aMessage);
		break;
	case EFlush:
	case EEcho:
	case ECancelNotify:
		aMessage.Complete(KErrNone);
		break;
	case ECheckMedia:
		aMessage.Complete(KErrNone);         // both directions are available ("R" / "W" = that one is not)
		break;
	case ENotifyActivity:
		aMessage.Complete(KErrNotSupported); // (select() on stdio: not offered)
		break;
	default:
		aMessage.Complete(KErrNotSupported);
		break;
		}
	}

static void RunServerL()
	{
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	CStdioServer::NewLC();
	User::RenameThread(KStdioServerName);
	RProcess::Rendezvous(KErrNone);
	CActiveScheduler::Start();
	CleanupStack::PopAndDestroy(2, sched);
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt r = KErrNoMemory;
	if (cleanup)
		{
		TRAP(r, RunServerL());
		delete cleanup;
		}
	return r;
	}
