// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phetelprobe.cpp -- OURS: the telephony T1 smoke test (T1). Not Symbian source.
//
// Walks ETel the way the Phone app / Messaging / Contacts do, against whatever TSY the ROM carries (today: the
// licensee stub with the fake modem, toolchain/shims/licenseetsy_board), and logs every step to the debug port:
//   (a) RTelServer -> RMobilePhone: registration status, current network (operator), signal, IMSI, IMEI
//   (c) RMobileLine/RMobileCall: Dial -> status changes until connected -> HangUp
//   (d) RMobileSmsMessaging: SendMessage (the fake modem loops it back) and ReceiveMessage
// Every wait is bounded, so a request the TSY never completes is reported, not hung on.
// Run:  BOOTLIST="!phetelprobe" ./ph boot   (SSM ROM; the line "[phetelprobe] DONE" ends it)
#include <e32base.h>
#include <e32debug.h>
#include <etel.h>
#include <etelmm.h>
#include <e32property.h>

static const TInt KStepTimeout = 15 * 1000000;
static TInt Fails = 0;
static TBool Fast = EFalse;			// "fast" on the command line: no UI pauses (the regression run)
static TInt Secs(TInt aSlow) { return Fast ? 1 : aSlow; }
static const TUid KStimCat = { 0xE7015730 };	// the fake modem's stimulus properties (key 1 = ring, key 2 = SMS text)

static TInt Await(TRequestStatus& aStatus, const char* aWhat, TInt aTimeout = KStepTimeout)
	{
	RTimer t;
	t.CreateLocal();
	TRequestStatus ts;
	t.After(ts, aTimeout);
	User::WaitForRequest(aStatus, ts);
	TInt r;
	if (aStatus == KRequestPending)
		{
		RDebug::Printf("[phetelprobe] %s: TIMED OUT", aWhat);
		t.Close();
		return KErrTimedOut;
		}
	t.Cancel();
	User::WaitForRequest(ts);
	t.Close();
	r = aStatus.Int();
	RDebug::Printf("[phetelprobe] %s: %d", aWhat, r);
	return r;
	}

static void Check(TInt aErr, const char* aWhat)
	{
	if (aErr != KErrNone)
		{
		Fails++;
		RDebug::Printf("[phetelprobe] FAIL %s (%d)", aWhat, aErr);
		}
	}

static void Run()
	{
	RTelServer srv;
	TInt r = srv.Connect();
	RDebug::Printf("[phetelprobe] RTelServer::Connect: %d", r);
	Check(r, "connect");
	if (r != KErrNone)
		return;
	TInt phones = 0;
	for (TInt i = 0; i < 20 && phones < 1; i++)
		{
		srv.EnumeratePhones(phones);
		if (phones < 1)
			User::After(500000);
		}
	RDebug::Printf("[phetelprobe] phones: %d", phones);
	RTelServer::TPhoneInfo info;
	r = srv.GetPhoneInfo(0, info);
	RDebug::Print(_L("[phetelprobe] GetPhoneInfo: %d name %S"), r, &info.iName);
	Check(r, "phone info");

	RMobilePhone phone;
	r = phone.Open(srv, info.iName);
	RDebug::Printf("[phetelprobe] RMobilePhone::Open: %d", r);
	Check(r, "phone open");
	if (r != KErrNone)
		return;
	TRequestStatus st;

	// ---- (a) registration, network, signal, identity
	RMobilePhone::TMobilePhoneRegistrationStatus reg = RMobilePhone::ERegistrationUnknown;
	phone.GetNetworkRegistrationStatus(st, reg);
	r = Await(st, "GetNetworkRegistrationStatus");
	if (r == KErrTimedOut) phone.CancelAsyncRequest(EMobilePhoneGetNetworkRegistrationStatus);
	RDebug::Printf("[phetelprobe] registration status = %d (5 = home, 7 = roaming)", (TInt)reg);
	Check(reg == RMobilePhone::ERegisteredOnHomeNetwork || reg == RMobilePhone::ERegisteredRoaming ? KErrNone : KErrGeneral, "registered");

	RMobilePhone::TMobilePhoneNetworkInfoV1 net;
	RMobilePhone::TMobilePhoneNetworkInfoV1Pckg netP(net);
	RMobilePhone::TMobilePhoneLocationAreaV1 area;
	phone.GetCurrentNetwork(st, netP, area);
	r = Await(st, "GetCurrentNetwork");
	if (r == KErrTimedOut) phone.CancelAsyncRequest(EMobilePhoneGetCurrentNetwork);
	RDebug::Print(_L("[phetelprobe] network: long '%S' short '%S' mcc %S mnc %S mode %d status %d"),
		&net.iLongName, &net.iShortName, &net.iCountryCode, &net.iNetworkId, (TInt)net.iMode, (TInt)net.iStatus);
	Check(r, "current network");

	TInt32 rssi = 0;
	TInt8 bars = 0;
	phone.GetSignalStrength(st, rssi, bars);
	r = Await(st, "GetSignalStrength");
	if (r == KErrTimedOut) phone.CancelAsyncRequest(EMobilePhoneGetSignalStrength);
	RDebug::Printf("[phetelprobe] signal: rssi %d dBm, bars %d of 7", (TInt)rssi, (TInt)bars);
	Check(r, "signal");

	RMobilePhone::TMobilePhoneSubscriberId imsi;
	phone.GetSubscriberId(st, imsi);
	r = Await(st, "GetSubscriberId");
	if (r == KErrTimedOut) phone.CancelAsyncRequest(EMobilePhoneGetSubscriberId);
	RDebug::Print(_L("[phetelprobe] IMSI: %S"), &imsi);
	Check(r, "imsi");

	RMobilePhone::TMobilePhoneIdentityV1 id;
	phone.GetPhoneId(st, id);
	r = Await(st, "GetPhoneId");
	if (r == KErrTimedOut) phone.CancelAsyncRequest(EMobilePhoneGetPhoneId);
	RDebug::Print(_L("[phetelprobe] IMEI: %S (%S %S)"), &id.iSerialNumber, &id.iManufacturer, &id.iModel);
	Check(r, "imei");

	RDebug::Printf("[phetelprobe] STAGE-A %s", Fails ? "FAIL" : "PASS");

	// ---- (c) a mobile-originated call
	RMobileLine line;
	r = line.Open(phone, _L("Voice1"));
	RDebug::Printf("[phetelprobe] RMobileLine::Open: %d", r);
	Check(r, "line open");
	if (r == KErrNone)
		{
		RMobileCall call;
		r = call.OpenNewCall(line);
		RDebug::Printf("[phetelprobe] OpenNewCall: %d", r);
		Check(r, "open call");
		if (r == KErrNone)
			{
			RMobileCall::TMobileCallStatus cs = RMobileCall::EStatusUnknown;
			TRequestStatus nst;
			call.NotifyMobileCallStatusChange(nst, cs);
			RMobileCall::TMobileCallParamsV1 params;
			RMobileCall::TMobileCallParamsV1Pckg paramsP(params);
			TRequestStatus dst;
			call.Dial(dst, paramsP, _L("+358401234567"));
			TBool connected = EFalse;
			for (TInt n = 0; n < 12 && !connected; n++)
				{
				RTimer t;
				t.CreateLocal();
				TRequestStatus ts;
				t.After(ts, 2000000);
				User::WaitForRequest(nst, ts);
				if (nst != KRequestPending)
					{
					t.Cancel();
					User::WaitForRequest(ts);
					RDebug::Printf("[phetelprobe] call status -> %d (3 dialling, 5 connecting, 6 connected)", (TInt)cs);
					if (cs == RMobileCall::EStatusConnected)
						connected = ETrue;
					else
						call.NotifyMobileCallStatusChange(nst, cs);
					}
				else
					{
					User::WaitForRequest(ts);
					}
				t.Close();
				}
			if (dst == KRequestPending)
				Await(dst, "Dial", 3000000);
			else
				RDebug::Printf("[phetelprobe] Dial completed: %d", dst.Int());
			RDebug::Printf("[phetelprobe] STAGE-C %s", connected ? "PASS" : "FAIL");
			if (!connected)
				Fails++;
			User::After(Secs(20) * 1000000);			// the Phone app shows "Call connected" meanwhile
			TRequestStatus hst;
			call.HangUp(hst);
			Await(hst, "HangUp");
			if (nst == KRequestPending)
				{
				call.CancelAsyncRequest(EMobileCallNotifyMobileCallStatusChange);
				User::WaitForRequest(nst);
				}
			call.Close();
			}
		line.Close();
		}

	// ---- (d) SMS: send one, expect it back (the fake modem's loopback)
	RMobileSmsMessaging sms;
	r = sms.Open(phone);
	RDebug::Printf("[phetelprobe] RMobileSmsMessaging::Open: %d", r);
	Check(r, "sms open");
	if (r == KErrNone)
		{
		RMobileSmsMessaging::TMobileSmsReceiveAttributesV1 rattr;
		RMobileSmsMessaging::TMobileSmsReceiveAttributesV1Pckg rattrP(rattr);
		TBuf8<RMobileSmsMessaging::KGsmTpduSize> rpdu;
		TRequestStatus rst;
		sms.ReceiveMessage(rst, rpdu, rattrP);

		// SMS-SUBMIT to +358401234567, 7-bit "hello"
		static const TUint8 pdu[] = {0x01,0x00,0x0A,0x91,0x53,0x48,0x10,0x32,0x54,0x76,0x00,0x00,0x05,0xE8,0x32,0x9B,0xFD,0x06};
		TBuf8<RMobileSmsMessaging::KGsmTpduSize> spdu;
		spdu.Append(pdu, sizeof(pdu));
		RMobileSmsMessaging::TMobileSmsSendAttributesV1 sattr;
		sattr.iFlags = RMobileSmsMessaging::KSmsDataFormat | RMobileSmsMessaging::KGsmServiceCentre | RMobileSmsMessaging::KRemotePartyInfo;
		sattr.iDataFormat = RMobileSmsMessaging::EFormatGsmTpdu;
		sattr.iGsmServiceCentre.iTelNumber = _L("+358508771010");
		sattr.iGsmServiceCentre.iTypeOfNumber = RMobilePhone::EInternationalNumber;
		sattr.iDestination.iTelNumber = _L("+358401234567");
		sattr.iDestination.iTypeOfNumber = RMobilePhone::EInternationalNumber;
		RMobileSmsMessaging::TMobileSmsSendAttributesV1Pckg sattrP(sattr);
		TRequestStatus sst;
		sms.SendMessage(sst, spdu, sattrP);
		r = Await(sst, "SendMessage");
		Check(r, "sms send");
		if (r == KErrNone)
			RDebug::Printf("[phetelprobe] sent, message ref %d", (TInt)sattr.iMsgRef);
		r = Await(rst, "ReceiveMessage");
		if (r == KErrTimedOut)
			{
			sms.CancelAsyncRequest(EMobileSmsMessagingReceiveMessage);
			User::WaitForRequest(rst);
			}
		else if (r == KErrNone)
			RDebug::Printf("[phetelprobe] received a %d byte TPDU, first bytes %02x %02x %02x", rpdu.Length(),
				rpdu.Length() > 0 ? rpdu[0] : 0, rpdu.Length() > 1 ? rpdu[1] : 0, rpdu.Length() > 2 ? rpdu[2] : 0);
		Check(r, "sms receive");
		sms.Close();
		}
	// ---- (e) an incoming call and an incoming SMS, triggered through the fake modem's P&S stimuli
	RProperty::Define(KStimCat, 1, RProperty::EText, TSecurityPolicy(TSecurityPolicy::EAlwaysPass), TSecurityPolicy(TSecurityPolicy::EAlwaysPass));
	RProperty::Define(KStimCat, 2, RProperty::EText, TSecurityPolicy(TSecurityPolicy::EAlwaysPass), TSecurityPolicy(TSecurityPolicy::EAlwaysPass));
	RProperty::Set(KStimCat, 1, KNullDesC);
	RProperty::Set(KStimCat, 2, KNullDesC);
	RMobileLine vline;
	if (vline.Open(phone, _L("Voice1")) == KErrNone)
		{
		TName callName;
		TRequestStatus ist;
		vline.NotifyIncomingCall(ist, callName);
		RProperty::Set(KStimCat, 1, _L("+358407654321"));
		r = Await(ist, "NotifyIncomingCall", 8000000);
		if (r == KErrNone)
			{
			RMobileCall in;
			r = in.OpenExistingCall(vline, callName);
			RDebug::Print(_L("[phetelprobe] incoming call '%S' open: %d"), &callName, r);
			if (r == KErrNone)
				{
				RMobileCall::TMobileCallInfoV1 ci;
				RMobileCall::TMobileCallInfoV1Pckg ciP(ci);
				in.GetMobileCallInfo(ciP);
				RDebug::Print(_L("[phetelprobe] caller %S, status %d"), &ci.iRemoteParty.iRemoteNumber.iTelNumber, (TInt)ci.iStatus);
				User::After(Secs(20) * 1000000);			// ringing: the Phone app shows the incoming call
				TRequestStatus ast;
				in.AnswerIncomingCall(ast);
				r = Await(ast, "AnswerIncomingCall");
				RDebug::Printf("[phetelprobe] STAGE-E-call %s", r == KErrNone ? "PASS" : "FAIL");
				Check(r, "answer");
				User::After(Secs(10) * 1000000);
				TRequestStatus hst;
				in.HangUp(hst);
				Await(hst, "HangUp(incoming)");
				in.Close();
				}
			}
		else
			Check(r, "incoming call");
		vline.Close();
		}
	// an SMS the network sends (not a loopback): the messaging stack above ETel is what normally takes it
	RMobileSmsMessaging sms2;
	if (sms2.Open(phone) == KErrNone)
		{
		RMobileSmsMessaging::TMobileSmsReceiveAttributesV1 rattr;
		RMobileSmsMessaging::TMobileSmsReceiveAttributesV1Pckg rattrP(rattr);
		TBuf8<RMobileSmsMessaging::KGsmTpduSize> rpdu;
		TRequestStatus rst;
		sms2.ReceiveMessage(rst, rpdu, rattrP);
		RProperty::Set(KStimCat, 2, _L("Hello from the fake network"));
		r = Await(rst, "ReceiveMessage(network)", 8000000);
		RDebug::Printf("[phetelprobe] STAGE-E-sms %s (%d byte TPDU)", r == KErrNone ? "PASS" : "FAIL", rpdu.Length());
		Check(r, "network sms");
		if (r == KErrTimedOut)
			{
			sms2.CancelAsyncRequest(EMobileSmsMessagingReceiveMessage);
			User::WaitForRequest(rst);
			}
		sms2.Close();
		}

	// ---- (g) the SIM: PIN1 lock info (card present, no PIN needed) and the ADN phonebook (3 contacts in the fake modem)
	RMobilePhone::TMobilePhoneLockInfoV1 lockInfo;
	RMobilePhone::TMobilePhoneLockInfoV1Pckg lockP(lockInfo);
	phone.GetLockInfo(st, RMobilePhone::ELockICC, lockP);
	r = Await(st, "GetLockInfo(ICC)");
	if (r == KErrTimedOut) phone.CancelAsyncRequest(EMobilePhoneGetLockInfo);
	RDebug::Printf("[phetelprobe] PIN1 lock info: %d status %d setting %d (0 = unlocked / disabled expected)", r, (TInt)lockInfo.iStatus, (TInt)lockInfo.iSetting);
	TInt simOk = r;
	RMobilePhoneBookStore pb;
	r = pb.Open(phone, KETelIccAdnPhoneBook);
	RDebug::Printf("[phetelprobe] ADN phonebook open: %d", r);
	if (r == KErrNone)
		{
		User::After(4000000);		// the store initialises and caches in the background (a client waits for the cache-ready notification)
		RMobilePhoneBookStore::TMobilePhoneBookInfoV1 pbInfo;
		RMobilePhoneBookStore::TMobilePhoneBookInfoV1Pckg pbInfoP(pbInfo);
		pb.GetInfo(st, pbInfoP);
		r = Await(st, "PhoneBook GetInfo", 25000000);
		if (r == KErrTimedOut) pb.CancelAsyncRequest(EMobilePhoneStoreGetInfo);
		RDebug::Printf("[phetelprobe] ADN GetInfo: %d total %d used %d name len max %d number len max %d", r, pbInfo.iTotalEntries, pbInfo.iUsedEntries,
			pbInfo.iMaxTextLength, pbInfo.iMaxNumLength);
		simOk |= r;
		TBuf8<400> pbData;
		pb.Read(st, 1, 3, pbData);
		r = Await(st, "PhoneBook Read", 25000000);
		if (r == KErrTimedOut) pb.CancelAsyncRequest(EMobilePhoneStoreRead);
		RDebug::Printf("[phetelprobe] ADN Read(1,3): %d, %d bytes", r, pbData.Length());
		simOk |= r;
		if (pbData.Length() > 0)
			{
			TBuf<64> shown;
			for (TInt i = 0; i < pbData.Length() && shown.Length() < 60; i++)
				shown.Append(pbData[i] >= 32 && pbData[i] < 127 ? (TChar)pbData[i] : (TChar)'.');
			RDebug::Print(_L("[phetelprobe] ADN data: %S"), &shown);
			}
		pb.Close();
		}
	else
		simOk = r;
	RDebug::Printf("[phetelprobe] STAGE-G-sim %s", simOk == KErrNone ? "PASS" : "FAIL");
	Check(simOk, "sim phonebook");
	phone.Close();
	srv.Close();
	}

TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TBuf<160> cmd;
	User::CommandLine(cmd);
	// "stim sms <text>" / "stim ring <number>": only set the fake modem's stimulus (an SMS arrives / the phone rings), then exit --
	// for watching the real UI (Messaging, the Phone app): BOOTLIST="!phetelprobe|stim|sms|Hello"
	TInt stim = cmd.Find(_L("stim "));
	if (stim >= 0)
		{
		TPtrC rest = cmd.Mid(stim + 5);
		TBool ring = rest.Left(4).Compare(_L("ring")) == 0;
		TBool sms = rest.Left(3).Compare(_L("sms")) == 0;
		TPtrC arg = rest.Mid(ring ? 5 : 4);
		RDebug::Print(_L("[phetelprobe] stimulus: %S '%S'"), ring ? &_L("ring") : &_L("sms"), &arg);
		User::After(35 * 1000000);
		RProperty::Define(KStimCat, 1, RProperty::EText, TSecurityPolicy(TSecurityPolicy::EAlwaysPass), TSecurityPolicy(TSecurityPolicy::EAlwaysPass));
		RProperty::Define(KStimCat, 2, RProperty::EText, TSecurityPolicy(TSecurityPolicy::EAlwaysPass), TSecurityPolicy(TSecurityPolicy::EAlwaysPass));
		if (ring || sms)
			RProperty::Set(KStimCat, ring ? 1 : 2, arg.Left(100));
		delete cleanup;
		return KErrNone;
		}
	Fast = cmd.Find(_L("fast")) >= 0;
	RDebug::Printf("[phetelprobe] start%s", Fast ? " (fast)" : "");
	if (!Fast)
		User::After(30 * 1000000);			// let the UI come up first (started as ssmtest.exe by SSM, early)
	Run();
	RDebug::Printf("[phetelprobe] DONE %s", Fails ? "FAIL" : "PASS");
	delete cleanup;
	return Fails;
	}
