// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// pheexctest.cpp -- OURS: the user-side exception delivery test. A software exception (User::RaiseException: the
// handler runs, user side only), then a hardware one: a write to an unmapped page under a TRAP. The kernel pushes the
// register record on the user stack and re-enters the process at _E32Startup with reason 4 (xu_exe.cpp);
// User::HandleException runs the handler, which leaves, and the TRAP catches it. On x86 a page fault is reported as
// EExcPageFault (kernel/x86/ckernel.cpp VectorToType). The thread then carries on normally.
#include <e32std.h>
#include <e32debug.h>

static volatile TInt LastExc = -1;

static void ExcHandler(TExcType aType)
	{
	LastExc = aType;
	if (aType == EExcPageFault || aType == EExcAccessViolation)
		User::Leave(KErrAbort);
	}

GLDEF_C TInt E32Main()
	{
	User::SetExceptionHandler(ExcHandler, 0xFFFFFFFFu);
	User::RaiseException(EExcUserInterrupt);			// software: the handler runs and returns here
	const TInt sw = LastExc;
	LastExc = -1;
	TRAPD(r, *(volatile TInt*)0x1000u = 1);				// hardware: page 1 is never mapped for user code
	const TInt hw = LastExc;
	TInt sum = 0;										// registers, stack and the TRAP chain are sound afterwards
	for (TInt i = 0; i < 100; ++i)
		sum += i * (hw + 1);
	User::SetExceptionHandler(NULL, 0);
	if (sw == EExcUserInterrupt && hw == EExcPageFault && r == KErrAbort && sum == 4950 * (EExcPageFault + 1))
		RDebug::Printf("PHEEXCTEST PASS: raised %d, fault %d, left %d", sw, hw, r);
	else
		RDebug::Printf("PHEEXCTEST FAIL: raised %d, fault %d, left %d, sum %d", sw, hw, r, sum);
	return KErrNone;
	}
