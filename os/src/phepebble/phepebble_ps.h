// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// phepebble_ps.h -- OURS: the Publish & Subscribe keys between the Pebble app (phepebble.exe, the UI) and the Pebble
// service (phepebblesrv.exe, the background session + notification bridge). The service defines them; category = its SID.

#ifndef PHEPEBBLE_PS_H
#define PHEPEBBLE_PS_H

#include <e32std.h>

const TUid KPhePebbleAppUid = { 0xE7017D00 };
const TUid KPhePebbleSrvUid = { 0xE7017D01 };
_LIT(KPhePebbleSrvExe, "phepebblesrv.exe");
_LIT(KPhePebbleSrvMatch, "phepebblesrv*");

// key 1 (int): the service state, TPhePebbleState
const TUint32 KPhePebbleKeyState = 1;
// key 2 (text, <= 128 chars): a human line for the UI: the watch's name, the last error or result
const TUint32 KPhePebbleKeyInfo = 2;
// key 3 (int): a command from the UI: (sequence << 8) | TPhePebbleCommand; the sequence makes a repeat a new value
const TUint32 KPhePebbleKeyCommand = 3;
// key 4 (int): notifications delivered since the service started
const TUint32 KPhePebbleKeyDelivered = 4;

enum TPhePebbleState
	{
	EPhePebbleNoWatch = 0,      // nothing bonded
	EPhePebbleIdle,             // bonded, not connected (waiting to reconnect)
	EPhePebbleScanning,
	EPhePebbleConnecting,
	EPhePebblePairing,
	EPhePebbleOpening,          // connected, PPoGATT / version exchange running
	EPhePebbleConnected,        // session ready: notifications go to the watch
	EPhePebbleNoBluetooth       // the LE transport is not available in this image
	};

enum TPhePebbleCommand
	{
	EPhePebbleCmdPair = 1,      // scan for a "Pebble" and pair with it (replaces the bonded watch)
	EPhePebbleCmdTest = 2,      // send a test notification
	EPhePebbleCmdForget = 3,    // disconnect and forget the watch
	EPhePebbleCmdReconnect = 4
	};

#endif
