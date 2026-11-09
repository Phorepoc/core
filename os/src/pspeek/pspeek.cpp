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

TInt E32Main()
	{
	for (TInt n = 0; n < 18; ++n)
		{
		RDebug::Printf("[pspeek] t+%ds global %d simstatus %d bootoffline %d uiphase %d startupappstate %d idlephase1 %d phonepwr %d", n * 10,
			Peek(0x101F8766, 0x41), Peek(0x101F8766, 0x31), Peek(0x101F8766, 0x11), Peek(0x101F8766, 0x42),
			Peek(0x100058F4, 0x1), Peek(0x101F8766, 0x43), Peek(0x101f75b6, 0x100052C5));
		User::After(10 * 1000000);
		}
	return 0;
	}
