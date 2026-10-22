// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_stkdump.h -- a diagnostic (PHEBROWSER_STKDUMP builds only): STKDUMP_DELAY_S seconds after start, log every thread of this
// process with its registers and every word of its stack that looks like a ROM code address ("PH: [stk] ..." lines), so the
// call chain of a hung thread can be resolved offline against the images' link maps. Nothing is written or changed.
#ifndef PB_STKDUMP_H
#define PB_STKDUMP_H
#include <e32base.h>
#include <e32debug.h>

#ifndef STKDUMP_DELAY_S
#define STKDUMP_DELAY_S 40
#endif

static TInt PbStkDumpTick(TAny*)
	{
	RProcess me;
	TFullName pattern = me.Name();
	pattern.Append(_L("::*"));
	TFindThread find(pattern);
	TFullName name;
	const TUint32 myId = (TUint32)RThread().Id();
	while (find.Next(name) == KErrNone)
		{
		RThread t;
		if (t.Open(find) != KErrNone)
			continue;
		const TBool self = ((TUint32)t.Id() == myId);
		TUint32 sp = 0, pc = 0, lr = 0;
		if (!self)
			{
			t.Suspend();
			TUint32 regs[18];                  // TArmRegSet: r0..r15, flags, dacr
			TPtr8 pk((TUint8*)regs, sizeof(regs), sizeof(regs));
			t.Context(pk);
			sp = regs[13]; pc = regs[15]; lr = regs[14];
			}
		else
			{
			TUint32 here = 0;
			sp = (TUint32)&here;
			}
		TThreadStackInfo si;
		TInt e = t.StackInfo(si);
		TBuf8<200> n8;
		n8.Copy(name.Left(180));
		RDebug::Printf("PH: [stk] thread %S sp=%08x pc=%08x lr=%08x stack base=%08x limit=%08x (%d)",
			&n8, sp, pc, lr, (TUint32)si.iBase, (TUint32)si.iLimit, e);
		if (e == KErrNone && sp >= (TUint32)si.iLimit && sp < (TUint32)si.iBase)
			{
			TInt words = 0, line = 0;
			TBuf8<160> row;
			for (TUint32 a = sp & ~3u; a < (TUint32)si.iBase && words < 6000; a += 4, ++words)
				{
				const TUint32 v = *(const TUint32*)a;
				if (v >= 0x80000000u && v < 0x90000000u)
					{
					row.AppendFormat(_L8(" %x:%08x"), (a - sp) / 4, v);
					if (++line == 6)
						{
						RDebug::Printf("PH: [stk] %S", &row);
						row.Zero();
						line = 0;
						}
					}
				}
			if (row.Length())
				RDebug::Printf("PH: [stk] %S", &row);
			}
		if (!self)
			t.Resume();
		t.Close();
		}
	RDebug::Printf("PH: [stk] done");
	return EFalse;      // one shot
	}

static void PbStartStackDump()
	{
	CPeriodic* p = CPeriodic::New(CActive::EPriorityLow);
	if (p)
		p->Start(STKDUMP_DELAY_S * 1000000, STKDUMP_DELAY_S * 1000000, TCallBack(PbStkDumpTick, NULL));
	}
#endif
