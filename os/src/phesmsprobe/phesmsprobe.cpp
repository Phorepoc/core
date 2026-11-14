// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phesmsprobe.cpp -- OURS: telephony T1, the SMS stack over the fake modem.
//
// The same walk Messaging's SMS server (smss) does, minus the message store: an SMS socket (the SMS protocol module smsprot.prt,
// ESock) bound to "receive any", a CSmsMessage built with the real GSM 03.40 code (gsmu), streamed to the socket and sent
// (KIoctlSendSmsMessage); the stack hands the PDU to ETel (RMobileSmsMessaging::SendMessage -> CTSY -> the fake modem), the
// fake modem loops it back as an incoming SMS and the stack reassembles + delivers it to the same socket. Logged to the debug
// port; every wait is bounded. Run: SSM_TEST=phesmsprobe ./ph rom ; ./ph boot (or BOOTLIST="!phesmsprobe").
#include <e32base.h>
#include <e32debug.h>
#include <es_sock.h>
#include <f32file.h>
#include <smsuaddr.h>
#include <gsmumsg.h>
#include <gsmubuf.h>
#include <gsmuset.h>
#include <smsustrm.h>

static TInt Fails = 0;
static const TInt KWait = 40 * 1000000;

static void Check(TInt aErr, const char* aWhat)
	{
	if (aErr != KErrNone)
		{
		Fails++;
		RDebug::Printf("[phesmsprobe] FAIL %s (%d)", aWhat, aErr);
		}
	}

static TInt Wait(TRequestStatus& aStatus, const char* aWhat, TInt aTimeout)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, aTimeout);
	User::WaitForRequest(aStatus, ts);
	TInt r;
	if (aStatus == KRequestPending)
		{
		RDebug::Printf("[phesmsprobe] %s: TIMED OUT", aWhat);
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	r = aStatus.Int();
	RDebug::Printf("[phesmsprobe] %s: %d", aWhat, r);
	return r;
	}

static void RunL()
	{
	RFs fs;
	User::LeaveIfError(fs.Connect());
	CleanupClosePushL(fs);
	RSocketServ ss;
	User::LeaveIfError(ss.Connect(8));
	CleanupClosePushL(ss);
	RSocket sock;
	TInt r = sock.Open(ss, KSMSAddrFamily, KSockDatagram, KSMSDatagramProtocol);
	RDebug::Printf("[phesmsprobe] SMS socket open: %d", r);
	User::LeaveIfError(r);
	CleanupClosePushL(sock);
	TSmsAddr addr;
	addr.SetSmsAddrFamily(ESmsAddrRecvAny);
	r = sock.Bind(addr);
	RDebug::Printf("[phesmsprobe] bind ESmsAddrRecvAny: %d", r);
	User::LeaveIfError(r);

	// the message: 7-bit text to a number, through the service centre
	_LIT(KText, "Hello from Phorepoc over the fake modem");
	CSmsBuffer* buffer = CSmsBuffer::NewL();
	CSmsMessage* msg = CSmsMessage::NewL(fs, CSmsPDU::ESmsSubmit, buffer);
	CleanupStack::PushL(msg);
	TSmsUserDataSettings uds;
	uds.SetAlphabet(TSmsDataCodingScheme::ESmsAlphabet7Bit);
	uds.SetTextCompressed(EFalse);
	msg->SetUserDataSettingsL(uds);
	msg->SetToFromAddressL(_L("+358401234567"));
	msg->SmsPDU().SetServiceCenterAddressL(_L("+358508771010"));
	buffer->InsertL(0, KText);

	RSmsSocketWriteStream out(sock);
	out << *msg;
	out.CommitL();
	TPckgBuf<TUint> sbuf;
	TRequestStatus sendSt;
	sock.Ioctl(KIoctlSendSmsMessage, sendSt, &sbuf, KSolSmsProv);
	r = Wait(sendSt, "send (KIoctlSendSmsMessage)", KWait);
	Check(r, "sms send");
	RDebug::Printf("[phesmsprobe] STAGE-F-send %s", r == KErrNone ? "PASS" : "FAIL");

	// one Ioctl at a time per socket (ESock_client 8): select-for-read only after the send completed
	TPckgBuf<TUint> sel;
	sel() = KSockSelectRead;
	TRequestStatus selSt;
	sock.Ioctl(KIOctlSelect, selSt, &sel, KSOLSocket);
	r = Wait(selSt, "wait for the looped-back SMS", KWait);
	if (r == KErrNone)
		{
		CSmsBuffer* rbuf = CSmsBuffer::NewL();
		CSmsMessage* in = CSmsMessage::NewL(fs, CSmsPDU::ESmsSubmit, rbuf);
		CleanupStack::PushL(in);
		RSmsSocketReadStream rd(sock);
		TRAPD(rr, rd >> *in);
		Check(rr, "read stream");
		TPckgBuf<TUint> rbuf2;
		TRequestStatus ack;
		sock.Ioctl(KIoctlReadMessageSucceeded, ack, &rbuf2, KSolSmsProv);
		User::WaitForRequest(ack);
		TBuf<160> text;
		rbuf->Extract(text, 0, Min(rbuf->Length(), 160));
		RDebug::Print(_L("[phesmsprobe] received: type %d from '%S' text '%S'"), (TInt)in->Type(), &in->ToFromAddress(), &text);
		TBool same = text.Compare(KText) == 0;
		RDebug::Printf("[phesmsprobe] STAGE-F-recv %s", same ? "PASS" : "FAIL");
		if (!same)
			Fails++;
		CleanupStack::PopAndDestroy(in);
		}
	else
		{
		Check(r, "sms receive");
		sock.CancelIoctl();
		User::WaitForRequest(selSt);
		}
	CleanupStack::PopAndDestroy(msg);
	CleanupStack::PopAndDestroy(&sock);
	CleanupStack::PopAndDestroy(&ss);
	CleanupStack::PopAndDestroy(&fs);
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TBuf<64> cmd;
	User::CommandLine(cmd);
	RDebug::Printf("[phesmsprobe] start");
	if (cmd.Find(_L("fast")) < 0)
		User::After(45 * 1000000);			// started as ssmtest.exe early: let ETel + the modem come up first
	TRAPD(err, RunL());
	if (err != KErrNone)
		{
		Fails++;
		RDebug::Printf("[phesmsprobe] leave %d", err);
		}
	RDebug::Printf("[phesmsprobe] DONE %s", Fails ? "FAIL" : "PASS");
	delete cleanup;
	return Fails;
	}
