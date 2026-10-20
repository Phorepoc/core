// pheltsy_at.h -- OURS (2026-10-17, modem rungs M2/M3, docs/phorepoc/telephony-plan.md sections 14-15): the AT engine
// of "pheltsy", the AT-based licensee TSY that grows out of the fake modem (classic MMessageRouter contract kept;
// the engine shape decision is in telephony-plan.md section 15). Compiled only with PHELTSY=1 (-DPHE_LTSY on
// licenseetsy); the default build is the plain fake modem.
//
// CPheAt owns the modem's serial channel (RBusDevComm unit 3 = the "Comm.A64" UART3 PDD of the MODEM=1 kernel:
// opening it powers the EG25-G) and runs a polled AT loop on the fake modem's 100 ms ticker:
//   init (after "RDY"): ATE0, AT+CMEE=2, AT+CREG=2, AT+CGSN (IMEI)
//   every 5 s: AT+CPIN?, AT+CREG?, AT+COPS=3,2 + AT+COPS? (MCC/MNC), AT+COPS=3,0 + AT+COPS? (long name), AT+CSQ,
//              AT+QSPN (the SIM's service provider name), AT+CIMI (until the IMSI is known)
//   one-shot, ahead of the poll: AT+CPIN="<pin>" / AT+CPIN="<puk>","<new pin>" (EnterCode, from VerifySecurityCode)
// Results land in TPheNet / TPheSim; the observer (CFakeModem) turns changes into the CTSY completions.
// On destruction (TSY shutdown) AT+QPOWD=1 is sent and "POWERED DOWN" awaited (bounded) before the port closes; the
// kernel then cuts VBAT once STATUS drops (or forces it: a64uart.cpp A64ModemRelease).
#ifndef PHELTSY_AT_H
#define PHELTSY_AT_H

#include <e32base.h>
#include <e32debug.h>
#include <d32comm.h>
#include <etelmm.h>

struct TPheNet
	{
	RMobilePhone::TMobilePhoneRegistrationStatus iReg;
	RMobilePhone::TMobilePhoneNetworkMode iMode;
	RMobilePhone::TMobilePhoneNetworkAccess iAccess;
	TBuf<4> iMcc;
	TBuf<4> iMnc;
	TBuf<32> iName;
	TBuf<32> iSpn;							// AT+QSPN <spn> (e.g. "giffgaff" on O2-UK)
	TInt iLac, iCellId;
	TInt iBars;								// 0..7 (the Avkon signal pane's scale, as the fake modem)
	TInt iRssiDbm;							// 0 = unknown
	};

_LIT8(KPheHidden, "AT+CPIN=..");		// never log a PIN

enum TPheSim { EPheSimUnknown, EPheSimAbsent, EPheSimPin, EPheSimPuk, EPheSimReady, EPheSimBlocked };

class MPheAtObserver
	{
public:
	virtual void PheRegChanged() = 0;		// iReg changed
	virtual void PheNetChanged() = 0;		// operator / mode / signal / SPN changed
	virtual void PheSimChanged() = 0;		// Sim() changed
	virtual void PheCodeResult(TInt aErr) = 0;	// the EnterCode command finished
	// M4 calls: the first voice call of AT+CLCC (27.007 <stat>: 0 active, 1 held, 2 dialing, 3 alerting, 4 incoming,
	// 5 waiting; -1 = no call any more), and the final result of ATD / ATA / AT+CHUP (kind = ECmdDial/Answer/HangUp)
	virtual void PheCall(TInt aStat, TBool aMt, const TDesC8& aNumber) = 0;
	virtual void PheCallCmdResult(TInt aKind, TInt aErr) = 0;
	// M4 SMS (PDU mode): AT+CMGS finished (TP-MR), a stored MT SMS read back (+CMTI -> AT+CMGR; TPDU without the SMSC)
	virtual void PheSmsSent(TInt aErr, TInt aRef) = 0;
	virtual void PheSmsReceived(const TDesC8& aTpdu, const TDesC& aSmsc) = 0;
	};

class CPheAt : public CActive
	{
public:
	static CPheAt* NewL(MPheAtObserver& aObs)
		{
		CPheAt* self = new (ELeave) CPheAt(aObs);
		CleanupStack::PushL(self);
		self->ConstructL();
		CleanupStack::Pop(self);
		return self;
		}
	~CPheAt()
		{
		Cancel();
		if (iOpen)
			{
			PowerDown();
			iPort.Close();					// the kernel finishes: STATUS high -> VBAT off (forced after 30 s)
			}
		}
	const TPheNet& Net() const { return iNet; }
	TPheSim Sim() const { return iSim; }
	const TDesC8& Imsi() const { return iImsi; }
	const TDesC8& Imei() const { return iImei; }
	TBool Ready() const { return iReady; }

	enum TCmdKind { ECmdPoll, ECmdCode, ECmdDial, ECmdAnswer, ECmdHangUp, ECmdClcc, ECmdCmgs, ECmdCmgr, ECmdCmgd };

	// PIN1 (aPuk empty) or PUK1 + new PIN: queued ahead of the poll; the result comes back through PheCodeResult
	TInt EnterCode(const TDesC& aCode, const TDesC& aPuk)
		{
		if (!iOpen || aCode.Length() > 8 || aPuk.Length() > 8)
			return KErrNotReady;
		TBuf8<48> c(_L8("AT+CPIN=\""));
		if (aPuk.Length())
			{
			c.Append(aPuk);
			c.Append(_L8("\",\""));
			}
		c.Append(aCode);
		c.Append('"');
		return Queue(ECmdCode, c);
		}
	// M4 calls: ATD<number>; / ATA / AT+CHUP; progress comes from AT+CLCC polling (1 s while a call exists) + RING/NO CARRIER
	TInt Dial(const TDesC& aNumber)
		{
		if (!iReady || aNumber.Length() == 0 || aNumber.Length() > 40)
			return KErrNotReady;
		TBuf8<48> c(_L8("ATD"));
		for (TInt i = 0; i < aNumber.Length(); ++i)		// dialable characters only
			{
			const TChar ch = aNumber[i];
			if (ch.IsDigit() || ch == '+' || ch == '*' || ch == '#')
				c.Append((TUint8)ch);
			}
		c.Append(';');
		iInCall = ETrue;
		return Queue(ECmdDial, c);
		}
	TInt Answer()	{ return Queue(ECmdAnswer, _L8("ATA")); }
	TInt HangUp()	{ return Queue(ECmdHangUp, _L8("AT+CHUP")); }
	// M4 SMS: AT+CMGS=<TPDU length>, "> " prompt, "00" (the SIM's SMSC) + the TPDU in hex + Ctrl-Z
	TInt SendSms(const TDesC8& aTpdu)
		{
		if (!iReady || aTpdu.Length() == 0 || aTpdu.Length() > 176 || iSmsHex.Length())
			return KErrNotReady;
		iSmsHex.Copy(_L8("00"));
		for (TInt i = 0; i < aTpdu.Length(); ++i)
			iSmsHex.AppendNumFixedWidthUC(aTpdu[i], EHex, 2);
		TBuf8<48> c(_L8("AT+CMGS="));
		c.AppendNum(aTpdu.Length());
		return Queue(ECmdCmgs, c);
		}
	const TDesC& Smsc() const { return iSmsc; }
	TBool AudioReady() const { return iAudioReady; }
	// M5: the call audio route (MODEM=1 kernel: Comm.A64 unit 3 DTR -> a64sound.cpp VoiceRoute, EG25-G PCM <-> earpiece/MIC1)
	void VoiceRoute(TBool aOn)
		{
		if (!iOpen)
			return;
		if (aOn) iPort.SetSignals(KSignalDTR, 0); else iPort.SetSignals(0, KSignalDTR);
		RDebug::Printf("[pheltsy] voice route %s", aOn ? "on" : "off");
		}

	// driven by the fake modem's 100 ms ticker
	void Tick()
		{
		if (!iOpen)
			return;
		++iTicks;
		if (iCmdActive)
			{
			const TInt limit = iKind == ECmdCmgs ? 600 : iKind == ECmdDial ? 300 : 50;	// CMGS 60 s, ATD 30 s, else 5 s
			if (iTicks - iCmdStart > limit)
				{
				RDebug::Printf("[pheltsy] timeout: %S", iKind == ECmdCode ? &KPheHidden() : &iCmd);
				CommandDone(EFalse);
				}
			return;
			}
		if (!iReady)
			{
			if (iTicks > 600 && !iRdy)			// 60 s without RDY: probe anyway (modem already up)
				iRdy = ETrue;
			if (iRdy)
				SendNext();
			return;
			}
		if (iQn || iTicks >= iNextPoll || (iInCall && iTicks - iLastClcc >= 10))
			SendNext();
		}

private:
	enum { KInit = 9, KPoll = 10, KQ = 6 };
	TInt Queue(TCmdKind aKind, const TDesC8& aCmd)
		{
		if (!iOpen || iQn >= KQ)
			return KErrServerBusy;
		iQ[iQn].iKind = aKind;
		iQ[iQn].iCmd.Copy(aCmd.Left(48));
		++iQn;
		return KErrNone;
		}
	CPheAt(MPheAtObserver& aObs) : CActive(EPriorityStandard), iObs(aObs) {}
	void ConstructL()
		{
		iNet.iReg = RMobilePhone::ERegistrationUnknown;
		iNet.iMode = RMobilePhone::ENetworkModeUnknown;
		iNet.iAccess = RMobilePhone::ENetworkAccessUnknown;
		CActiveScheduler::Add(this);
		TInt r = User::LoadLogicalDevice(_L("ECOMM"));
		r = iPort.Open(3);
		RDebug::Printf("[pheltsy] RBusDevComm::Open(3) -> %d", r);
		if (r != KErrNone)
			User::Leave(r);						// no MODEM=1 kernel: no engine, the scripted values stay
		iOpen = ETrue;
		TCommConfig cfg;
		iPort.Config(cfg);
		cfg().iRate = EBps115200;
		cfg().iDataBits = EData8;
		cfg().iStopBits = EStop1;
		cfg().iParity = EParityNone;
		cfg().iHandshake = KConfigFreeRTS | KConfigFreeDTR;
		cfg().iTerminatorCount = 0;
		iPort.SetConfig(cfg);
		Read();
		}
	void Read()
		{
		iRx.Zero();
		iPort.ReadOneOrMore(iStatus, iRx);
		SetActive();
		}
	void DoCancel() { iPort.ReadCancel(); }
	void RunL()
		{
		if (iStatus.Int() == KErrNone)
			{
			for (TInt i = 0; i < iRx.Length(); ++i)
				{
				const TUint8 c = iRx[i];
				if (c == '\r' || c == '\n')
					{
					if (iLine.Length())
						Line(iLine);
					iLine.Zero();
					}
				else if (iLine.Length() < iLine.MaxLength())
					iLine.Append(c);
				}
			if (iCmdActive && iKind == ECmdCmgs && !iPduSent && iLine.Length() && iLine[0] == '>')
				{
				iPduSent = ETrue;
				iLine.Zero();
				iSmsHex.Append(0x1A);				// Ctrl-Z ends the PDU
				TRequestStatus ws;
				iPort.Write(ws, iSmsHex);
				User::WaitForRequest(ws);
				RDebug::Printf("[pheltsy] CMGS: PDU written (%d hex chars) -> %d", iSmsHex.Length() - 1, ws.Int());
				}
			}
		else
			User::After(10000);				// a line error (break while unpowered): don't spin
		Read();
		}

	// AT+QPOWD=1 with a bounded synchronous wait for "POWERED DOWN" (3 tries x 5 s); the AO is already cancelled
	void PowerDown()
		{
		if (!iRdy)
			return;							// never answered: the kernel's PWRKEY off-pulse handles it
		_LIT8(KQpowd, "AT+QPOWD=1\r");
		TBuf8<256> acc;
		for (TInt attempt = 0; attempt < 3; ++attempt)
			{
			TRequestStatus ws;
			iPort.Write(ws, KQpowd);
			User::WaitForRequest(ws);
			acc.Zero();
			for (TInt n = 0; n < 25; ++n)		// 25 x 200 ms
				{
				TBuf8<64> buf;
				TRequestStatus rs, ts;
				RTimer t;
				if (t.CreateLocal() != KErrNone)
					break;
				iPort.ReadOneOrMore(rs, buf);
				t.After(ts, 200000);
				User::WaitForRequest(rs, ts);
				if (rs == KRequestPending) { iPort.ReadCancel(); User::WaitForRequest(rs); }
				else { t.Cancel(); User::WaitForRequest(ts); }
				t.Close();
				if (acc.Length() + buf.Length() > acc.MaxLength())
					acc.Delete(0, acc.Length() + buf.Length() - acc.MaxLength());
				acc.Append(buf);
				if (acc.Find(_L8("POWERED DOWN")) >= 0)
					{
					RDebug::Printf("[pheltsy] AT+QPOWD=1: POWERED DOWN (try %d)", attempt + 1);
					return;
					}
				if (acc.Find(_L8("ERROR")) >= 0)
					break;
				}
			RDebug::Printf("[pheltsy] AT+QPOWD=1 try %d: no POWERED DOWN", attempt + 1);
			}
		}

	void Send(const TDesC8& aCmd)
		{
		iCmd.Copy(aCmd.Left(iCmd.MaxLength()));
		TBuf8<64> out(aCmd.Left(63));
		out.Append('\r');
		TRequestStatus ws;
		iPort.Write(ws, out);
		User::WaitForRequest(ws);			// 115200 baud, < 64 bytes: ~6 ms
		iCmdActive = ETrue;
		iCmdStart = iTicks;
		}
	void SendNext()
		{
		iKind = ECmdPoll;
		if (!iReady)
			{
			// + M4: caller id, +CRING: VOICE, PDU mode, new SMS stored with a +CMTI index, the SMSC
			static const char* const KInitCmds[KInit] = { "ATE0", "AT+CMEE=2", "AT+CREG=2", "AT+CGSN", "AT+CLIP=1",
														  "AT+CRC=1", "AT+CMGF=0", "AT+CNMI=2,1,0,0,0", "AT+CSCA?" };
			Send(TPtrC8((const TUint8*)KInitCmds[iStep]));
			return;
			}
		if (iQn)								// queued one-shots (code, call control, SMS) go first
			{
			iKind = iQ[0].iKind;
			TBuf8<48> c(iQ[0].iCmd);
			for (TInt i = 1; i < iQn; ++i)
				iQ[i - 1] = iQ[i];
			--iQn;
			if (iKind == ECmdCmgs)
				iPduSent = EFalse;
			if (iKind == ECmdCmgr)
				iPdu.Zero();
			Send(c);
			return;
			}
		if (iInCall && iTicks - iLastClcc >= 10)	// a call exists: AT+CLCC every second
			{
			iKind = ECmdClcc;
			iLastClcc = iTicks;
			iClccStat = -1;
			Send(_L8("AT+CLCC"));
			return;
			}
		static const char* const KPollCmds[KPoll] = { "AT+CPIN?", "AT+CREG?", "AT+COPS=3,2", "AT+COPS?", "AT+COPS=3,0",
													  "AT+COPS?", "AT+CSQ", "AT+QSPN", "AT+CIMI", "AT+QDAI?" };
		// no usable SIM: skip AT+QSPN (it answers a bare ERROR) and AT+CIMI; IMSI known: skip AT+CIMI
		// the audio subsystem: AT+QDAI? errors until ~15-18 s after RDY (boot 983); polled until it answers, then never again
		if ((iStep == KPoll - 3 && iSim != EPheSimReady) || (iStep == KPoll - 2 && (iImsi.Length() || iSim != EPheSimReady)) ||
			(iStep == KPoll - 1 && iAudioReady))
			{
			CommandDone(ETrue);
			return;
			}
		Send(TPtrC8((const TUint8*)KPollCmds[iStep]));
		}
	void CommandDone(TBool aOk)
		{
		iCmdActive = EFalse;
		const TCmdKind kind = iKind;
		iKind = ECmdPoll;
		switch (kind)
			{
			case ECmdCode:
				RDebug::Printf("[pheltsy] AT+CPIN=.. -> %s", aOk ? "OK" : "rejected");
				iObs.PheCodeResult(aOk ? KErrNone : KErrAccessDenied);
				iNextPoll = iTicks;					// re-read +CPIN? at once
				return;
			case ECmdDial:
			case ECmdAnswer:
			case ECmdHangUp:
				RDebug::Printf("[pheltsy] %S -> %s", &iCmd, aOk ? "OK" : "failed");
				iObs.PheCallCmdResult(kind, aOk ? KErrNone : KErrGeneral);
				iInCall = ETrue;					// let AT+CLCC say what is left
				iLastClcc = iTicks - 10;
				return;
			case ECmdClcc:
				if (aOk)
					{
					iObs.PheCall(iClccStat, iClccMt, iClccNumber);
					if (iClccStat < 0)
						iInCall = EFalse;
					}
				return;
			case ECmdCmgs:
				RDebug::Printf("[pheltsy] CMGS -> %s, ref %d", aOk ? "OK" : "failed", iMr);
				iSmsHex.Zero();
				iObs.PheSmsSent(aOk ? KErrNone : KErrGeneral, iMr);
				return;
			case ECmdCmgr:
				{
				TBuf8<180> tpdu;
				TBuf<24> smsc;
				if (aOk && DecodePdu(iPdu, tpdu, smsc))
					{
					RDebug::Printf("[pheltsy] SMS %d read (%d byte TPDU)", iCmgrIndex, tpdu.Length());
					iObs.PheSmsReceived(tpdu, smsc);
					}
				TBuf8<24> del(_L8("AT+CMGD="));
				del.AppendNum(iCmgrIndex);
				Queue(ECmdCmgd, del);				// the SIM/ME store is small: delete once handed over
				return;
				}
			case ECmdCmgd:
				return;
			default:
				break;
			}
		++iStep;
		if (!iReady && iStep >= KInit)
			{
			iReady = ETrue;
			iStep = 0;
			RDebug::Printf("[pheltsy] modem ready (init done), IMEI %S", &iImei);
			}
		else if (iReady && iStep >= KPoll)
			{
			iStep = 0;
			iNextPoll = iTicks + 50;			// next round in 5 s
			if (iNetDirty)
				{
				iNetDirty = EFalse;
				RDebug::Print(_L("[pheltsy] net: reg %d mode %d '%S' spn '%S' %S-%S bars %d (%d dBm)"), (TInt)iNet.iReg,
					(TInt)iNet.iMode, &iNet.iName, &iNet.iSpn, &iNet.iMcc, &iNet.iMnc, iNet.iBars, iNet.iRssiDbm);
				iObs.PheNetChanged();
				}
			}
		}
	void SetSim(TPheSim aSim)
		{
		if (aSim == iSim)
			return;
		iSim = aSim;
		RDebug::Printf("[pheltsy] SIM -> %d (1 absent, 2 PIN, 3 PUK, 4 ready, 5 blocked)", (TInt)aSim);
		iObs.PheSimChanged();
		}

	// ---- the parser: final results end the current command; +CPIN/+CREG/+COPS/+CSQ/+QSPN update the state (solicited or URC)
	void Line(const TDesC8& aLine)
		{
		TBuf8<120> l(aLine.Left(120));
		RDebug::Printf("[pheltsy] << %S", &l);
		if (aLine == _L8("RDY"))
			{ iRdy = ETrue; return; }
		if (aLine == _L8("OK"))
			{ if (iCmdActive) CommandDone(ETrue); return; }
		if (aLine.Find(_L8("+CME ERROR")) == 0)
			{
			// AT+CPIN? on an empty slot: "+CME ERROR: 10" / "SIM not inserted" (CMEE=2); "SIM failure" (13) alike
			if (iCmdActive && iCmd == _L8("AT+CPIN?") &&
				(aLine.Find(_L8(": 10")) > 0 || aLine.Find(_L8(": 13")) > 0 || aLine.FindF(_L8("not inserted")) > 0 ||
				 aLine.FindF(_L8("SIM failure")) > 0))
				SetSim(EPheSimAbsent);
			if (iCmdActive) CommandDone(EFalse);
			return;
			}
		if (aLine.Find(_L8("ERROR")) == 0 || aLine == _L8("NO CARRIER") || aLine == _L8("BUSY") ||
			aLine == _L8("NO ANSWER") || aLine == _L8("NO DIALTONE"))
			{
			if (aLine.Find(_L8("ERROR")) != 0)	// a call ended / failed: re-read the call list now
				{
				iInCall = ETrue;
				iLastClcc = iTicks - 10;
				}
			if (iCmdActive) CommandDone(EFalse);
			return;
			}
		if (aLine == _L8("RING") || aLine.Find(_L8("+CRING: ")) == 0)
			{
			if (!iInCall) { iInCall = ETrue; iLastClcc = iTicks - 10; }
			return;
			}
		if (aLine.Find(_L8("+CLCC: ")) == 0)
			{ ParseClcc(aLine.Mid(7)); return; }
		if (aLine.Find(_L8("+CMGS: ")) == 0)
			{ iMr = Num(aLine.Mid(7)); return; }
		if (aLine.Find(_L8("+CMTI: ")) == 0)	// +CMTI: "SM",<index>
			{
			TPtrC8 f[2];
			if (Fields(aLine.Mid(7), f, 2) == 2)
				{
				TBuf8<24> c(_L8("AT+CMGR="));
				c.AppendNum(Num(f[1]));
				iCmgrPendingIndex = Num(f[1]);
				Queue(ECmdCmgr, c);
				}
			return;
			}
		if (aLine.Find(_L8("+CMGR: ")) == 0)
			{ iPduNext = ETrue; iCmgrIndex = iCmgrPendingIndex; return; }
		if (aLine.Find(_L8("+CSCA: ")) == 0)	// +CSCA: "<sca>",<tosca>
			{
			TPtrC8 f[2];
			if (Fields(aLine.Mid(7), f, 2) >= 1)
				iSmsc.Copy(f[0].Left(iSmsc.MaxLength()));
			return;
			}
		if (iPduNext && iCmdActive && iKind == ECmdCmgr)
			{
			iPduNext = EFalse;
			iPdu.Copy(aLine.Left(iPdu.MaxLength()));
			return;
			}
		if (aLine.Find(_L8("+QDAI: ")) == 0)	// the audio subsystem answers: the PCM config the codec route assumes?
			{
			iAudioReady = ETrue;
			const TBool ok = aLine.Mid(7) == _L8("1,1,0,1,0,0,1,1");
			RDebug::Printf("[pheltsy] audio ready, QDAI %S%s", &l, ok ? "" : " -- NOT the expected PCM slave config (phemodemtest ensures it)");
			return;
			}
		if (aLine.Find(_L8("+CPIN: ")) == 0)
			{ ParseCpin(aLine.Mid(7)); return; }
		if (aLine.Find(_L8("+CREG: ")) == 0)
			{ ParseCreg(aLine.Mid(7)); return; }
		if (aLine.Find(_L8("+COPS: ")) == 0)
			{ ParseCops(aLine.Mid(7)); return; }
		if (aLine.Find(_L8("+CSQ: ")) == 0)
			{ ParseCsq(aLine.Mid(6)); return; }
		if (aLine.Find(_L8("+QSPN: ")) == 0)
			{ ParseQspn(aLine.Mid(7)); return; }
		// a bare number: the answer of AT+CGSN (IMEI) / AT+CIMI (IMSI)
		if (iCmdActive && aLine.Length() >= 6 && aLine.Length() <= 16 && IsDigits(aLine))
			{
			if (iCmd == _L8("AT+CGSN")) iImei.Copy(aLine);
			else if (iCmd == _L8("AT+CIMI")) { iImsi.Copy(aLine); RDebug::Printf("[pheltsy] IMSI %S", &iImsi); }
			}
		}
	static TBool IsDigits(const TDesC8& a)
		{
		for (TInt i = 0; i < a.Length(); ++i)
			if (a[i] < '0' || a[i] > '9')
				return EFalse;
		return ETrue;
		}
	// +CPIN: READY | SIM PIN | SIM PUK | NOT INSERTED | SIM PIN2 ... (URC "+CPIN: NOT INSERTED" / "READY" too)
	void ParseCpin(const TDesC8& aArg)
		{
		if (aArg.FindF(_L8("READY")) == 0) SetSim(EPheSimReady);
		else if (aArg.FindF(_L8("SIM PIN")) == 0 && aArg.Length() == 7) SetSim(EPheSimPin);
		else if (aArg.FindF(_L8("SIM PUK")) == 0 && aArg.Length() == 7) SetSim(EPheSimPuk);
		else if (aArg.FindF(_L8("NOT INSERTED")) == 0 || aArg.FindF(_L8("NOT READY")) == 0) SetSim(EPheSimAbsent);
		}
	// comma-separated fields; quotes stripped
	static TInt Fields(const TDesC8& aIn, TPtrC8* aOut, TInt aMax)
		{
		TInt n = 0;
		TPtrC8 rest(aIn);
		while (n < aMax)
			{
			const TInt c = rest.Locate(',');
			TPtrC8 f = c < 0 ? rest : rest.Left(c);
			if (f.Length() >= 2 && f[0] == '"' && f[f.Length() - 1] == '"')
				f.Set(f.Mid(1, f.Length() - 2));
			aOut[n++].Set(f);
			if (c < 0)
				break;
			rest.Set(rest.Mid(c + 1));
			}
		return n;
		}
	static TInt Num(const TDesC8& aF, TRadix aRadix = EDecimal)
		{
		TLex8 lex(aF);
		TUint v = 0;
		return lex.Val(v, aRadix) == KErrNone ? (TInt)v : -1;
		}
	// +CREG: [<n>,]<stat>[,<lac>,<ci>[,<AcT>]] -- the URC form has no <n>; a solicited answer has it
	void ParseCreg(const TDesC8& aArgs)
		{
		TPtrC8 f[6];
		const TInt n = Fields(aArgs, f, 6);
		const TBool solicited = iCmdActive && iCmd == _L8("AT+CREG?");
		const TInt base = solicited ? 1 : 0;
		if (n <= base)
			return;
		RMobilePhone::TMobilePhoneRegistrationStatus reg;
		switch (Num(f[base]))
			{
			case 0: reg = RMobilePhone::ENotRegisteredNoService; break;
			case 1: reg = RMobilePhone::ERegisteredOnHomeNetwork; break;
			case 2: reg = RMobilePhone::ENotRegisteredSearching; break;
			case 3: reg = RMobilePhone::ERegistrationDenied; break;
			case 5: reg = RMobilePhone::ERegisteredRoaming; break;
			default: reg = RMobilePhone::ERegistrationUnknown; break;
			}
		if (n > base + 2)
			{
			iNet.iLac = Num(f[base + 1], EHex);
			iNet.iCellId = Num(f[base + 2], EHex);
			}
		if (reg != iNet.iReg)
			{
			iNet.iReg = reg;
			RDebug::Printf("[pheltsy] registration -> %d", (TInt)reg);
			iObs.PheRegChanged();
			}
		}
	// +COPS: <mode>[,<format>,<oper>[,<AcT>]]
	void ParseCops(const TDesC8& aArgs)
		{
		TPtrC8 f[4];
		const TInt n = Fields(aArgs, f, 4);
		if (n < 3)
			{
			if (n >= 1 && (iNet.iName.Length() || iNet.iMcc.Length()))	// "+COPS: 0": no operator any more
				{
				iNet.iName.Zero(); iNet.iMcc.Zero(); iNet.iMnc.Zero();
				iNetDirty = ETrue;
				}
			return;
			}
		const TInt fmt = Num(f[1]);
		if (fmt == 2 && f[2].Length() >= 5)
			{
			TBuf<4> mcc, mnc;
			mcc.Copy(f[2].Left(3));
			mnc.Copy(f[2].Mid(3).Left(3));
			if (mcc != iNet.iMcc || mnc != iNet.iMnc) { iNet.iMcc = mcc; iNet.iMnc = mnc; iNetDirty = ETrue; }
			}
		else if (fmt == 0)
			{
			TBuf<32> name;
			name.Copy(f[2].Left(32));
			if (name != iNet.iName) { iNet.iName = name; iNetDirty = ETrue; }
			}
		if (n >= 4)
			{
			RMobilePhone::TMobilePhoneNetworkMode mode = RMobilePhone::ENetworkModeUnknown;
			RMobilePhone::TMobilePhoneNetworkAccess acc = RMobilePhone::ENetworkAccessUnknown;
			switch (Num(f[3]))
				{
				case 0: case 1: case 3: mode = RMobilePhone::ENetworkModeGsm; acc = RMobilePhone::ENetworkAccessGsm; break;
				case 2: case 4: case 5: case 6: mode = RMobilePhone::ENetworkModeWcdma; acc = RMobilePhone::ENetworkAccessUtran; break;
				case 7: mode = RMobilePhone::ENetworkModeWcdma; acc = RMobilePhone::ENetworkAccessUtran; break;	// LTE: no 2010 enum; shown as 3G
				default: break;
				}
			if (mode != iNet.iMode) { iNet.iMode = mode; iNet.iAccess = acc; iNetDirty = ETrue; }
			}
		}
	// +CSQ: <rssi>,<ber>  (rssi 0..31 = -113..-51 dBm, 99 = unknown)
	void ParseCsq(const TDesC8& aArgs)
		{
		TPtrC8 f[2];
		if (Fields(aArgs, f, 2) < 1)
			return;
		const TInt rssi = Num(f[0]);
		TInt bars = 0, dbm = 0;
		if (rssi >= 0 && rssi <= 31)
			{
			dbm = -113 + 2 * rssi;
			bars = rssi >= 28 ? 7 : rssi / 4;		// 0..7
			}
		if (bars != iNet.iBars || dbm != iNet.iRssiDbm) { iNet.iBars = bars; iNet.iRssiDbm = dbm; iNetDirty = ETrue; }
		}
	// +QSPN: <FNN>,<SNN>,<SPN>,<alphabet>,<RPLMN>  -- the SPN is what the SIM wants shown (giffgaff on O2-UK)
	void ParseQspn(const TDesC8& aArgs)
		{
		TPtrC8 f[5];
		if (Fields(aArgs, f, 5) < 3)
			return;
		TBuf<32> spn;
		spn.Copy(f[2].Left(32));
		if (spn != iNet.iSpn) { iNet.iSpn = spn; iNetDirty = ETrue; }
		}

	// +CLCC: <id>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>] -- the first voice call is reported
	void ParseClcc(const TDesC8& aArgs)
		{
		TPtrC8 f[7];
		const TInt n = Fields(aArgs, f, 7);
		if (n < 5 || Num(f[3]) != 0 || iClccStat >= 0)
			return;
		iClccStat = Num(f[2]);
		iClccMt = Num(f[1]) == 1;
		iClccNumber.Copy(n >= 6 ? f[5].Left(iClccNumber.MaxLength()) : TPtrC8());
		}
	static TInt HexNibble(TUint8 c)
		{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'A' && c <= 'F') return c - 'A' + 10;
		if (c >= 'a' && c <= 'f') return c - 'a' + 10;
		return -1;
		}
	// the AT+CMGR PDU: <SMSC len><SMSC TOA><SMSC BCD...><TPDU>; returns the TPDU and the SMSC as "+digits"
	static TBool DecodePdu(const TDesC8& aHex, TDes8& aTpdu, TDes& aSmsc)
		{
		TBuf8<200> bin;
		for (TInt i = 0; i + 1 < aHex.Length() && bin.Length() < bin.MaxLength(); i += 2)
			{
			const TInt hi = HexNibble(aHex[i]), lo = HexNibble(aHex[i + 1]);
			if (hi < 0 || lo < 0)
				return EFalse;
			bin.Append((TUint8)(hi * 16 + lo));
			}
		if (bin.Length() < 2 || bin[0] + 1 >= bin.Length())
			return EFalse;
		const TInt scLen = bin[0];
		aSmsc.Zero();
		if (scLen >= 2)
			{
			if ((bin[1] & 0x70) == 0x10)
				aSmsc.Append('+');
			for (TInt i = 2; i <= scLen; ++i)
				{
				const TInt d1 = bin[i] & 0xF, d2 = bin[i] >> 4;
				if (d1 < 10) aSmsc.AppendNum(d1);
				if (d2 < 10) aSmsc.AppendNum(d2);
				}
			}
		aTpdu.Copy(bin.Mid(scLen + 1).Left(aTpdu.MaxLength()));
		return aTpdu.Length() > 0;
		}

	struct TQueued { TCmdKind iKind; TBuf8<48> iCmd; };
	TQueued iQ[KQ];
	TInt iQn;
	TCmdKind iKind;
	TBool iInCall, iClccMt, iPduSent, iPduNext, iAudioReady;
	TInt iLastClcc, iClccStat, iMr, iCmgrIndex, iCmgrPendingIndex;
	TBuf8<44> iClccNumber;
	TBuf8<360> iSmsHex;
	TBuf8<400> iPdu;
	TBuf<24> iSmsc;
	MPheAtObserver& iObs;
	RBusDevComm iPort;
	TBool iOpen, iRdy, iReady, iCmdActive, iNetDirty;
	TInt iTicks, iCmdStart, iNextPoll, iStep;
	TBuf8<48> iCmd;
	TBuf8<256> iRx;
	TBuf8<256> iLine;
	TBuf8<16> iImsi, iImei;
	TPheSim iSim;
	TPheNet iNet;
	};

#endif
