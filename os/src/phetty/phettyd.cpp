// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// phettyd.cpp -- OURS: the developer tty daemon (docs/phorepoc/phetty-plan.md, V1).
// Owns the ttys. A tty = scrollback of ANSI bytes + key queue + a pheshell process (eshell with its console on the tty).
// Started by the SSM non-critical list (r_cmd_phettyd) or on demand by a client (RPheTty::Connect). The CenRep key
// KPheTtyCenRepUid/1 (PHETTY=on|off at ROM build) turns the autostart off; "force" on the command line ignores it.
#include <e32debug.h>
#include <e32keys.h>
#include <centralrepository.h>
#include "phetty.h"

class CPheServer;
class CPheSession;
class CTty;

class CShellWatcher : public CActive
	{
public:
	CShellWatcher(CTty& aTty) : CActive(EPriorityStandard), iTty(aTty) { CActiveScheduler::Add(this); }
	~CShellWatcher() { Cancel(); }
	TRequestStatus& Status() { return iStatus; }
	void Watch() { SetActive(); }
private:
	void RunL();
	void DoCancel();
	CTty& iTty;
	};

struct TPendRead
	{
	RMessage2 iMsg;
	TBool iActive;
	TUint32 iFrom;
	const CPheSession* iOwner;
	};

class CTty : public CBase
	{
public:
	CTty(CPheServer& aServer) : iServer(aServer) {}
	~CTty();
	void ConstructL();
	void SpawnShell();
	void KillShell();
	void ShellExited();
	void RestartShell();
	void Append(const TUint8* aData, TInt aLen);
	void AppendText(const char* aText);
	void Notify();
	void Deliver(TPendRead& aObs);
	TInt Read(const RMessage2& aMsg, const CPheSession* aOwner);
	void QueueKey(TUint aCode, TUint aMods);
	void ConRead(const RMessage2& aMsg, const CPheSession* aOwner);
	void CancelFor(const CPheSession* aOwner);
	void Bytes(const TDesC8& aBytes);
	void Plain(TUint8 aByte);
	TUint32 StartSeq() const { return iEnd > (TUint32)KPheTtyRing ? iEnd - KPheTtyRing : 0; }
public:
	CPheServer& iServer;
	TInt iId;
	TInt iCols, iRows;
	TBuf<32> iTitle;
	TUint8 iRing[KPheTtyRing];
	TUint32 iEnd;
	TPheKey iKeys[64];
	TInt iKHead, iKCount;
	RMessage2 iConMsg;
	TBool iConPending;
	const CPheSession* iConOwner;
	TPendRead iObs[4];
	RProcess iProc;
	CShellWatcher* iWatch;
	TInt iShellState;      // 0 dead, 1 running, 2 restarted
	TInt iFast;            // consecutive short-lived shells
	TUint iStartTick;
	TBuf8<8> iEsc;         // pending escape / UTF-8 bytes of EPtBytes
	};

class CPheServer : public CServer2
	{
public:
	static CPheServer* NewL();
	~CPheServer();
	CSession2* NewSessionL(const TVersion& aVersion, const RMessage2& aMessage) const;
	CTty* Tty(TInt aId) const { return (aId >= 1 && aId <= KPheTtyMax) ? iTty[aId - 1] : NULL; }
	TInt NewTty(TInt aCols, TInt aRows);
	void KillTty(TInt aId);
	void ForgetSession(const CPheSession* aS);
	TUint8* Tmp() { return iTmp; }
private:
	CPheServer() : CServer2(EPriorityStandard) {}
	CTty* iTty[KPheTtyMax];
	TUint8 iTmp[4096 + 16];
	};

class CPheSession : public CSession2
	{
public:
	void ServiceL(const RMessage2& aMessage);
	void ServiceError(const RMessage2& aMessage, TInt aError);
	~CPheSession();
private:
	CPheServer& Srv() const { return *const_cast<CPheServer*>(static_cast<const CPheServer*>(Server())); }
	};

static TInt Clamp(TInt aV, TInt aLo, TInt aHi) { return aV < aLo ? aLo : (aV > aHi ? aHi : aV); }

// ---------------------------------------------------------------------------------------------------------------- watcher

void CShellWatcher::RunL() { iTty.ShellExited(); }
void CShellWatcher::DoCancel() { iTty.iProc.LogonCancel(iStatus); }

// ---------------------------------------------------------------------------------------------------------------- tty

void CTty::ConstructL()
	{
	iProc.SetHandle(KNullHandle);          // RProcess() is a handle to the CURRENT process until told otherwise
	iWatch = new (ELeave) CShellWatcher(*this);
	iTitle.Copy(_L("tty"));
	iTitle.AppendNum(iId);
	}

CTty::~CTty()
	{
	KillShell();
	delete iWatch;
	for (TInt i = 0; i < 4; i++)
		if (iObs[i].iActive)
			{ iObs[i].iMsg.Complete(KErrCancel); iObs[i].iActive = EFalse; }
	if (iConPending)
		{ iConMsg.Complete(KErrCancel); iConPending = EFalse; }
	}

void CTty::SpawnShell()
	{
	if (iProc.Handle())
		return;
	RProcess p;
	TInt r = p.Create(KPheShellExe, KNullDesC);
	if (r == KErrNone)
		r = p.SetParameter(KPheShellSlot, iId);
	if (r != KErrNone)
		{
		RDebug::Printf("[phettyd] tty %d: pheshell create failed %d", iId, r);
		p.Close();
		iShellState = 0;
		AppendText("\r\n[phettyd] cannot start pheshell\r\n");
		return;
		}
	iProc = p;
	iProc.Logon(iWatch->Status());
	iWatch->Watch();
	iStartTick = User::NTickCount();
	iProc.Resume();
	if (iShellState == 0)
		iShellState = 1;
	RDebug::Printf("[phettyd] tty %d: pheshell started, pid %d", iId, (TInt)iProc.Id());
	}

void CTty::KillShell()
	{
	if (iWatch)
		iWatch->Cancel();
	if (iProc.Handle())
		{
		iProc.Kill(0);
		iProc.Close();
		}
	iShellState = 0;
	}

void CTty::RestartShell()
	{
	KillShell();
	iFast = 0;
	iShellState = 2;
	SpawnShell();
	}

void CTty::ShellExited()
	{
	TInt reason = iProc.ExitReason();
	TInt type = iProc.ExitType();
	iProc.Close();
	TUint lived = User::NTickCount() - iStartTick;
	RDebug::Printf("[phettyd] tty %d: pheshell exited (type %d reason %d) after %u ticks", iId, type, reason, lived);
	if (lived < 3000)
		iFast++;
	else
		iFast = 0;
	if (iFast >= 3)
		{
		iShellState = 0;
		AppendText("\r\n[phettyd] pheshell keeps failing; tty is dead\r\n");
		return;
		}
	AppendText("\r\n[phettyd] pheshell exited; restarting\r\n");
	iShellState = 2;
	SpawnShell();
	}

void CTty::Append(const TUint8* aData, TInt aLen)
	{
	for (TInt i = 0; i < aLen; i++)
		iRing[(iEnd + i) % KPheTtyRing] = aData[i];
	iEnd += aLen;
	Notify();
	}

void CTty::AppendText(const char* aText)
	{
	Append((const TUint8*)aText, User::StringLength((const TUint8*)aText));
	}

void CTty::Notify()
	{
	for (TInt i = 0; i < 4; i++)
		if (iObs[i].iActive)
			{
			TUint32 from = iObs[i].iFrom;
			if (from < StartSeq() || from > iEnd)
				from = StartSeq();
			if (iEnd > from)
				Deliver(iObs[i]);
			}
	}

void CTty::Deliver(TPendRead& aObs)
	{
	TUint32 from = aObs.iFrom;
	if (from < StartSeq() || from > iEnd)
		from = StartSeq();
	TInt maxLen = aObs.iMsg.GetDesMaxLength(1);
	TInt n = Clamp(maxLen - 4, 0, 4096);
	if ((TUint32)n > iEnd - from)
		n = iEnd - from;
	TUint8* tmp = iServer.Tmp();
	tmp[0] = (TUint8)from;
	tmp[1] = (TUint8)(from >> 8);
	tmp[2] = (TUint8)(from >> 16);
	tmp[3] = (TUint8)(from >> 24);
	for (TInt i = 0; i < n; i++)
		tmp[4 + i] = iRing[(from + i) % KPheTtyRing];
	TPtrC8 d(tmp, 4 + n);
	TInt r = aObs.iMsg.Write(1, d);
	aObs.iActive = EFalse;
	aObs.iMsg.Complete(r == KErrNone ? 4 + n : r);
	}

TInt CTty::Read(const RMessage2& aMsg, const CPheSession* aOwner)
	{
	TUint32 from = (TUint32)aMsg.Int2();
	TBool wait = aMsg.Int3();
	if (from < StartSeq() || from > iEnd)
		from = StartSeq();
	TPendRead tmp;
	tmp.iMsg = aMsg;
	tmp.iFrom = from;
	tmp.iActive = ETrue;
	tmp.iOwner = aOwner;
	if (iEnd > from || !wait)
		{
		Deliver(tmp);
		return KErrNone;                 // Deliver completed the message
		}
	for (TInt i = 0; i < 4; i++)
		if (!iObs[i].iActive)
			{
			iObs[i] = tmp;
			return KErrNone;
			}
	return KErrInUse;
	}

void CTty::QueueKey(TUint aCode, TUint aMods)
	{
	if (iConPending)
		{
		TPheKeyBuf kb;
		kb().iCode = aCode;
		kb().iMods = aMods;
		iConPending = EFalse;
		TInt r = iConMsg.Write(1, kb);
		iConMsg.Complete(r);
		return;
		}
	if (iKCount == 64)
		{
		iKHead = (iKHead + 1) % 64;
		iKCount--;
		}
	TInt at = (iKHead + iKCount) % 64;
	iKeys[at].iCode = aCode;
	iKeys[at].iMods = aMods;
	iKCount++;
	}

void CTty::ConRead(const RMessage2& aMsg, const CPheSession* aOwner)
	{
	if (iConPending)
		{
		aMsg.Complete(KErrInUse);
		return;
		}
	if (iKCount)
		{
		TPheKeyBuf kb;
		kb() = iKeys[iKHead];
		iKHead = (iKHead + 1) % 64;
		iKCount--;
		TInt r = aMsg.Write(1, kb);
		aMsg.Complete(r);
		return;
		}
	iConMsg = aMsg;
	iConPending = ETrue;
	iConOwner = aOwner;
	}

void CTty::CancelFor(const CPheSession* aOwner)
	{
	for (TInt i = 0; i < 4; i++)
		if (iObs[i].iActive && iObs[i].iOwner == aOwner)
			{ iObs[i].iActive = EFalse; iObs[i].iMsg.Complete(KErrCancel); }
	}

// raw terminal bytes (PhoneLab / phettyctl) -> keys. ASCII, UTF-8, ESC [ A..D/H/F, ESC [ n ~
void CTty::Plain(TUint8 c)
	{
	if (c == '\r' || c == '\n')
		QueueKey(EKeyEnter, 0);
	else if (c == 0x7f || c == 0x08)
		QueueKey(EKeyBackspace, 0);
	else
		QueueKey(c, 0);
	}

void CTty::Bytes(const TDesC8& b)
	{
	for (TInt i = 0; i < b.Length(); i++)
		{
		TUint8 c = b[i];
		if (iEsc.Length() == 0)
			{
			if (c == 0x1b || c >= 0xc0)
				iEsc.Append(c);
			else
				Plain(c);
			continue;
			}
		if (iEsc[0] == 0x1b)
			{
			if (iEsc.Length() == 1 && c != '[' && c != 'O')
				{
				iEsc.Zero();
				QueueKey(EKeyEscape, 0);
				Plain(c);
				continue;
				}
			iEsc.Append(c);
			if (iEsc.Length() < 3)
				continue;
			if (c >= 0x40 && c <= 0x7e)
				{
				TUint code = 0;
				TInt num = 0;
				for (TInt k = 2; k < iEsc.Length() - 1; k++)
					if (iEsc[k] >= '0' && iEsc[k] <= '9')
						num = num * 10 + (iEsc[k] - '0');
				switch (c)
					{
				case 'A': code = EKeyUpArrow; break;
				case 'B': code = EKeyDownArrow; break;
				case 'C': code = EKeyRightArrow; break;
				case 'D': code = EKeyLeftArrow; break;
				case 'H': code = EKeyHome; break;
				case 'F': code = EKeyEnd; break;
				case '~':
					code = num == 3 ? EKeyDelete : num == 5 ? EKeyPageUp : num == 6 ? EKeyPageDown :
					       (num == 1 || num == 7) ? EKeyHome : (num == 4 || num == 8) ? EKeyEnd : num == 2 ? EKeyInsert : 0;
					break;
					}
				iEsc.Zero();
				if (code)
					QueueKey(code, 0);
				}
			else if (iEsc.Length() >= 8)
				iEsc.Zero();
			continue;
			}
		// UTF-8 continuation
		iEsc.Append(c);
		TInt need = iEsc[0] >= 0xe0 ? 3 : 2;
		if (iEsc.Length() == need)
			{
			TUint u = need == 2 ? ((iEsc[0] & 0x1f) << 6) | (iEsc[1] & 0x3f)
			                    : ((iEsc[0] & 0x0f) << 12) | ((iEsc[1] & 0x3f) << 6) | (iEsc[2] & 0x3f);
			iEsc.Zero();
			QueueKey(u, 0);
			}
		}
	}

// ---------------------------------------------------------------------------------------------------------------- server

CPheServer* CPheServer::NewL()
	{
	CPheServer* s = new (ELeave) CPheServer;
	CleanupStack::PushL(s);
	s->StartL(KPheTtyServerName);
	CleanupStack::Pop(s);
	return s;
	}

CPheServer::~CPheServer()
	{
	for (TInt i = 0; i < KPheTtyMax; i++)
		delete iTty[i];
	}

CSession2* CPheServer::NewSessionL(const TVersion& aVersion, const RMessage2&) const
	{
	if (!User::QueryVersionSupported(TVersion(1, 0, 0), aVersion))
		User::Leave(KErrNotSupported);
	return new (ELeave) CPheSession;
	}

TInt CPheServer::NewTty(TInt aCols, TInt aRows)
	{
	for (TInt i = 0; i < KPheTtyMax; i++)
		if (!iTty[i])
			{
			CTty* t = new CTty(*this);
			if (!t)
				return KErrNoMemory;
			t->iId = i + 1;
			t->iCols = aCols ? Clamp(aCols, 10, 250) : KPheTtyDefCols;
			t->iRows = aRows ? Clamp(aRows, 2, 100) : KPheTtyDefRows;
			TRAPD(r, t->ConstructL());
			if (r != KErrNone)
				{
				delete t;
				return r;
				}
			iTty[i] = t;
			t->SpawnShell();
			return i + 1;
			}
	return KErrOverflow;
	}

void CPheServer::KillTty(TInt aId)
	{
	if (aId >= 1 && aId <= KPheTtyMax)
		{
		delete iTty[aId - 1];
		iTty[aId - 1] = NULL;
		}
	}

void CPheServer::ForgetSession(const CPheSession* aS)
	{
	for (TInt i = 0; i < KPheTtyMax; i++)
		if (iTty[i])
			{
			iTty[i]->CancelFor(aS);
			if (iTty[i]->iConPending && iTty[i]->iConOwner == aS)
				{
				iTty[i]->iConPending = EFalse;
				iTty[i]->iConMsg.Complete(KErrCancel);
				}
			}
	}

CPheSession::~CPheSession()
	{
	if (Server())
		Srv().ForgetSession(this);
	}

void CPheSession::ServiceError(const RMessage2& aMessage, TInt aError)
	{
	if (!aMessage.IsNull())
		aMessage.Complete(aError);
	}

void CPheSession::ServiceL(const RMessage2& m)
	{
	CPheServer& srv = Srv();
	TInt op = m.Function();
	CTty* t = NULL;
	if (op != EPtNew && op != EPtList && op != EPtReadCancel && op != EPtConReadCancel)
		{
		t = srv.Tty(m.Int0());
		if (!t)
			{
			m.Complete(KErrNotFound);
			return;
			}
		}
	switch (op)
		{
	case EPtNew:
		m.Complete(srv.NewTty(m.Int0(), m.Int1()));
		break;
	case EPtList:
		{
		TUint8 ids[KPheTtyMax];
		TInt n = 0;
		for (TInt i = 1; i <= KPheTtyMax; i++)
			if (srv.Tty(i))
				ids[n++] = (TUint8)i;
		TPtrC8 d(ids, n);
		TInt r = m.Write(0, d);
		m.Complete(r == KErrNone ? n : r);
		break;
		}
	case EPtInfo:
		{
		TPheTtyInfoBuf ib;
		ib().iId = t->iId;
		ib().iCols = t->iCols;
		ib().iRows = t->iRows;
		ib().iShell = t->iShellState;
		ib().iEndSeq = t->iEnd;
		ib().iStartSeq = t->StartSeq();
		ib().iTitle = t->iTitle;
		TInt r = m.Write(1, ib);
		m.Complete(r);
		break;
		}
	case EPtResize:
		{
		TInt c = Clamp(m.Int1(), 10, 250), r = Clamp(m.Int2(), 2, 100);
		TBool changed = c != t->iCols || r != t->iRows;
		t->iCols = c;
		t->iRows = r;
		m.Complete(KErrNone);
		// eshell reads its console size once (CLineEdit::NewL): a running shell is restarted at the new size
		// (its screen starts afresh: pheconsole clears it; history is lost)
		if (changed && t->iProc.Handle())
			t->RestartShell();
		break;
		}
	case EPtKill:
		m.Complete(KErrNone);
		srv.KillTty(t->iId);
		break;
	case EPtKey:
		t->QueueKey(m.Int1(), m.Int2());
		m.Complete(KErrNone);
		break;
	case EPtBytes:
		{
		TBuf8<256> b;
		TInt r = m.Read(1, b);
		if (r == KErrNone)
			t->Bytes(b);
		m.Complete(r);
		break;
		}
	case EPtRead:
		{
		TInt r = t->Read(m, this);
		if (r != KErrNone)
			m.Complete(r);
		break;
		}
	case EPtReadCancel:
		for (TInt i = 1; i <= KPheTtyMax; i++)
			if (srv.Tty(i))
				srv.Tty(i)->CancelFor(this);
		m.Complete(KErrNone);
		break;
	case EPtConWrite:
		{
		TUint8* tmp = srv.Tmp();
		TInt len = m.GetDesLength(1);
		if (len > 4096)
			len = 4096;
		TPtr8 p(tmp, 0, 4096);
		TInt r = m.Read(1, p);
		if (r == KErrNone)
			t->Append(tmp, p.Length());
		(void)len;
		m.Complete(r == KErrNone ? ((t->iCols << 16) | t->iRows) : r);
		break;
		}
	case EPtConRead:
		t->ConRead(m, this);
		break;
	case EPtConReadCancel:
		for (TInt i = 1; i <= KPheTtyMax; i++)
			{
			CTty* x = srv.Tty(i);
			if (x && x->iConPending && x->iConOwner == this)
				{
				x->iConPending = EFalse;
				x->iConMsg.Complete(KErrCancel);
				}
			}
		m.Complete(KErrNone);
		break;
	case EPtConSize:
		m.Complete((t->iCols << 16) | t->iRows);
		break;
	case EPtConTitle:
		{
		TBuf<32> b;
		TInt r = m.Read(1, b);
		if (r == KErrNone || r == KErrOverflow)
			t->iTitle = b;
		m.Complete(KErrNone);
		break;
		}
	case EPtShellState:
		m.Complete(t->iShellState);
		break;
	default:
		m.Complete(KErrNotSupported);
		break;
		}
	}

// ---------------------------------------------------------------------------------------------------------------- main

static TBool Enabled(TInt& aTtys)
	{
	aTtys = 1;
	TBool on = ETrue;
	CRepository* rep = NULL;
	TRAPD(r, rep = CRepository::NewL(TUid::Uid(KPheTtyCenRepUid)));
	if (r == KErrNone)
		{
		TInt v;
		if (rep->Get(1, v) == KErrNone)
			on = v != 0;
		if (rep->Get(2, v) == KErrNone)
			aTtys = Clamp(v, 0, 4);
		delete rep;
		}
	else
		RDebug::Printf("[phettyd] no CenRep repository (%d): defaults", r);
	return on;
	}

static void RunL()
	{
	TBuf<32> cmd;
	User::CommandLine(cmd);
	TInt ttys;
	TBool on = Enabled(ttys);
	if (!on && cmd.FindF(_L("force")) < 0)
		{
		RDebug::Printf("[phettyd] disabled (CenRep %08x/1 = 0); exiting", KPheTtyCenRepUid);
		return;
		}
	CActiveScheduler* sched = new (ELeave) CActiveScheduler;
	CleanupStack::PushL(sched);
	CActiveScheduler::Install(sched);
	CPheServer* srv = CPheServer::NewL();
	CleanupStack::PushL(srv);
	for (TInt i = 0; i < ttys; i++)
		srv->NewTty(0, 0);
	RDebug::Printf("[phettyd] up, %d tty(s)", ttys);
	RProcess::Rendezvous(KErrNone);
	CActiveScheduler::Start();
	CleanupStack::PopAndDestroy(2);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	if (!cleanup)
		return KErrNoMemory;
	TRAPD(r, RunL());
	if (r != KErrNone)
		{
		RDebug::Printf("[phettyd] failed %d", r);
		RProcess::Rendezvous(r);
		}
	delete cleanup;
	return r;
	}
