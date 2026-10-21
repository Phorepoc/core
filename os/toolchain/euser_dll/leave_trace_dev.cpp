// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// leave_trace_dev.cpp — OURS: opt-in bring-up diagnostic for the DEVICE euser.
//
// Built only with LEAVE_TRACE=1 (emitter/build_euser_device.py), which links euser
// with `--wrap=_ZN4User5LeaveEi` and points the User::Leave EXPORT ordinal at this
// wrapper. Every Leave in the whole system (euser-internal LeaveIfError/AllocL/...
// and every client DLL/EXE that imports User::Leave) then prints
//     [LEAVE] <code> lr=<caller return address> <process name>
// on the debug port before leaving for real. Feed the lr to tools/romsym.py to get
// image+function. Servers TRAP most failures silently; this makes them visible
// without touching a line of the 2010 source.
#include <e32std.h>
#include <e32debug.h>

extern "C" void __real__ZN4User5LeaveEi(TInt aReason);

// first characters of the process name, so a leave can be attributed without guessing
static void ProcTag(char* aTag, TInt aMax)
	{
	TName n(RProcess().Name());
	TInt i = 0;
	for (; i < n.Length() && i < aMax - 1 && n[i] != '['; i++)
		aTag[i] = (char)n[i];
	aTag[i] = 0;
	}

extern "C" void __wrap__ZN4User5LeaveEi(TInt aReason)
	{
	TUint lr = (TUint)__builtin_return_address(0);
	char tag[20];
	ProcTag(tag, sizeof(tag));
	RDebug::Printf("[LEAVE] %d lr=%08x %s", aReason, lr, tag);
	__real__ZN4User5LeaveEi(aReason);
	}

// User::LeaveIfError hides the interesting caller behind its own frame: trace it too
// (only when it is actually going to leave), so the lr names the real failing site.
extern "C" TInt __real__ZN4User12LeaveIfErrorEi(TInt aReason);

extern "C" TInt __wrap__ZN4User12LeaveIfErrorEi(TInt aReason)
	{
	if (aReason < 0)
		{
		TUint lr = (TUint)__builtin_return_address(0);
		char tag[20];
		ProcTag(tag, sizeof(tag));
		RDebug::Printf("[LEAVE-IF] %d lr=%08x %s", aReason, lr, tag);
			// poor man's backtrace: the first few stack words that look like ROM code addresses
			TUint* sp = (TUint*)&lr;
			TUint bt[24]; TInt n = 0;
			TThreadStackInfo si; TInt lim = 0;   // never read past the top of this stack
			if (RThread().StackInfo(si) == KErrNone) lim = (TInt)(((TUint*)si.iBase) - sp);
			for (TInt i = 1; i < 600 && i < lim && n < 24; i++)
				if (sp[i] >= 0x80000000u && sp[i] < 0x81000000u && !(sp[i] & 1) && sp[i] != lr
				    && !(sp[i] >= (TUint)&__wrap__ZN4User5LeaveEi - 0x70000 && sp[i] < (TUint)&__wrap__ZN4User5LeaveEi + 0x70000)
				    && (n == 0 || bt[n-1] != sp[i]))
					bt[n++] = sp[i];
			for (; n < 24; n++) bt[n] = 0;
			RDebug::Printf("  bt %08x %08x %08x %08x %08x %08x", bt[0], bt[1], bt[2], bt[3], bt[4], bt[5]);
			RDebug::Printf("  bt %08x %08x %08x %08x %08x %08x", bt[6], bt[7], bt[8], bt[9], bt[10], bt[11]);
			RDebug::Printf("  bt %08x %08x %08x %08x %08x %08x", bt[12], bt[13], bt[14], bt[15], bt[16], bt[17]);
			RDebug::Printf("  bt %08x %08x %08x %08x %08x %08x", bt[18], bt[19], bt[20], bt[21], bt[22], bt[23]);
		}
	return __real__ZN4User12LeaveIfErrorEi(aReason);
	}
