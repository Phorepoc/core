// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
//
// phebletest.cpp -- OURS (BLE plan rung 4): the LE scan test. Turns Bluetooth on (RBTPowerControl is not needed: the
// stack is up once Bluetooth is on in Settings, or SSM_TEST=btpower), connects to "!PheBle", scans actively for
// 20 s and logs every advert with a name, once per address. Exit proof on the PinePhone with the Pebble 2 HR in range:
// a line "phebletest: Pebble DE7F 54:a5:2e:b6:9e:f4 ..." and "phebletest: PASS". Argument: seconds (default 20).

#include <e32base.h>
#include <e32debug.h>
#include "pheble.h"

LOCAL_C TInt Run(TInt aSeconds)
	{
	RPheBle ble;
	TInt err = KErrNotFound;
	for (TInt i = 0; i < 30 && err != KErrNone; ++i)        // the server starts with the Bluetooth stack
		{
		err = ble.Connect();
		if (err != KErrNone)
			User::After(1000000);
		}
	RDebug::Printf("phebletest: connect %d", err);
	if (err != KErrNone)
		return err;
	err = ble.StartScan(ETrue, aSeconds);
	RDebug::Printf("phebletest: scan start %d", err);
	RArray<TInt64> seen;
	TBool pebble = EFalse;
	while (err == KErrNone)
		{
		TPheBleAdvertBuf adv;
		TRequestStatus st;
		ble.NextAdvert(adv, st);
		User::WaitForRequest(st);
		if (st.Int() != KErrNone)
			{
			RDebug::Printf("phebletest: scan end %d", st.Int());
			break;
			}
		const TPheBleAdvert& a = adv();
		TInt64 key = 0;
		for (TInt i = 5; i >= 0; --i)
			key = (key << 8) | a.iAddr.iAddr[i];
		if (a.iName.Length() == 0 || seen.Find(key) != KErrNotFound)
			continue;
		seen.Append(key);
		const TUint8* b = a.iAddr.iAddr;
		RDebug::Printf("phebletest: %S %02x:%02x:%02x:%02x:%02x:%02x type %d rssi %d", &a.iName,
			b[5], b[4], b[3], b[2], b[1], b[0], a.iAddr.iType, a.iRssi);
		if (a.iName.Find(_L8("Pebble")) == 0)
			pebble = ETrue;
		}
	_LIT8(KPass, "PASS");
	_LIT8(KNoPebble, "no Pebble");
	const TDesC8& verdict = pebble ? static_cast<const TDesC8&>(KPass) : static_cast<const TDesC8&>(KNoPebble);
	RDebug::Printf("phebletest: %d named devices, %S", seen.Count(), &verdict);
	seen.Close();
	ble.Close();
	return pebble ? KErrNone : KErrNotFound;
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	TInt seconds = 20;
	TBuf<16> arg;
	if (User::CommandLineLength() <= arg.MaxLength())
		{
		User::CommandLine(arg);
		TLex lex(arg);
		lex.Val(seconds);
		}
	TInt err = Run(seconds > 0 ? seconds : 20);
	delete cleanup;
	return err;
	}
