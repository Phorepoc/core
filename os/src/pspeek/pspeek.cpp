// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0
#include <e32base.h>
#include <e32debug.h>
#include <e32property.h>

static TInt Peek(TUint32 aCategory, TUint32 aKey)
	{
	TInt v = 0;
	TInt r = RProperty::Get(TUid::Uid(aCategory), aKey, v);
	return r == KErrNone ? v : -100000 - r;		// an error shows as -1000xx
	}

// the Hb device dialog server's published system info (QDataStream: big-endian ints mode, status, signal, power, battery)
static void HbInfo(TInt aSecs)
	{
	TBuf8<64> b;
	TInt r = RProperty::Get(TUid::Uid(0x20022FC5), 0x496e666f, b);
	if (r != KErrNone || b.Length() < 20)
		{
		RDebug::Printf("[pspeek] t+%ds hbsysinfo: %d (len %d)", aSecs, r, b.Length());
		return;
		}
	TInt v[5];
	for (TInt i = 0; i < 5; ++i)
		v[i] = (b[i * 4] << 24) | (b[i * 4 + 1] << 16) | (b[i * 4 + 2] << 8) | b[i * 4 + 3];
	RDebug::Printf("[pspeek] t+%ds hbsysinfo mode %d status %d signal %d power %d battery %d", aSecs, v[0], v[1], v[2], v[3], v[4]);
	}

TInt E32Main()
	{
	for (TInt n = 0; n < 18; ++n)
		{
		RDebug::Printf("[pspeek] t+%ds global %d simstatus %d bootoffline %d uiphase %d startupappstate %d idlephase1 %d activeidleuid %x phonepwr %d", n * 10,
			Peek(0x101F8766, 0x41), Peek(0x101F8766, 0x31), Peek(0x101F8766, 0x11), Peek(0x101F8766, 0x42),
			Peek(0x100058F4, 0x1), Peek(0x101F8766, 0x43), Peek(0x101FD657, 1), Peek(0x101f75b6, 0x100052C5));
		HbInfo(n * 10);
		User::After(10 * 1000000);
		}
	return 0;
	}
