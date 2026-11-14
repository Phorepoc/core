// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// dev_exe_startup.cpp — the DEVICE EXE thread bootstrap (Track B / M7).
//
// The real exexp startup is uc_exe.cia (_E32Startup, REAL source, compiled
// as-is into the EXE) + uc_exe.cpp's RunThread. uc_exe.cpp only has bodies for
// __GCC32__ (2004 GCC: __CTOR_LIST__ walk) and __ARMCC__; our modern-GCC build
// defines neither and hits its #error. This TU is the modern-GCC RunThread:
// identical logic to the __GCC32__ branch, with the static-ctor walk over the
// .init_array table (emitter/efile.ld brackets it with __init_array_start/
// __init_array_end) instead of the extinct __CTOR_LIST__ format.
//
// Static DTORS are not run: globalDestructorFunc is registered exactly like
// the real one (DllSetTls) but is a no-op — the file server never exits, and
// process teardown reclaims everything. (The real __GCC32__ branch walks
// __DTOR_LIST__; wiring .fini_array is a when-needed follow-up.)

#include "u32std.h"
#include <u32exec.h>
#include <e32svr.h>
#include <e32panic.h>

GLREF_C TInt E32Main();

#ifdef __LEAVE_EQUALS_THROW__
// `device` images (build_mmp.py: -fexceptions -D__LEAVE_EQUALS_THROW__) on the REAL phone: a Leave is a C++ throw,
// and the phone's runtime (drtaeabi.dll, compsupp/symaehabi) keeps each thread's exception state -- the
// __cxa_eh_globals + the OOM emergency buffer -- in a TCppRTExceptionsGlobals that the thread's entry puts on its
// own stack; its constructor stores `this` in drtaeabi's Dll::Tls (__cxa_get_globals reads it). RVCT EXEs get that
// from callfirstprocessfn.cpp (CallThrdProcEntry, in their static startup lib). Without it the first Leave of the
// process faulted in __cxa_throw (KERN-EXEC 3; nsquery_arm, 2026-09-29). Opaque here: only drtaeabi touches the
// fields (symbian_support.h: __cxa_eh_globals + emergency_buffer, ~0x90 bytes) -- the space is generous.
class TCppRTExceptionsGlobals
	{
public:
	IMPORT_C TCppRTExceptionsGlobals();		// drtaeabi.dll 204
	IMPORT_C void Init2ndEmergencyBuffer();
	IMPORT_C void Kill2ndEmergencyBuffer();
private:
	TUint64 iSpace[64];
	};
_LIT(KLitUser, "USER");
#endif

typedef void (*TCtorFn)();
extern TCtorFn __init_array_start[];   // efile.ld
extern TCtorFn __init_array_end[];

extern "C" {

void globalDestructorFunc()
	{
	}

void RunThread(TBool aNotFirst, SThreadCreateInfo& aInfo)
	{
	SStdEpocThreadCreateInfo& cinfo = (SStdEpocThreadCreateInfo&)aInfo;

	TInt r = UserHeap::SetupThreadHeap(aNotFirst, cinfo);

	if (r==KErrNone)
		r = UserSvr::DllSetTls(KGlobalDestructorTlsKey, KDllUid_Special, (TAny*)globalDestructorFunc);

#ifdef __LEAVE_EQUALS_THROW__
	TCppRTExceptionsGlobals exceptionGlobals;
	exceptionGlobals.Init2ndEmergencyBuffer();
	try {
#endif
	if (r==KErrNone)
		{
		if (aNotFirst)
			r = (*cinfo.iFunction)(cinfo.iPtr);
		else
			{
			// Init statics for implicitly linked DLLs
			User::InitProcess();

			// Init statics for EXE (.init_array, oldest first)
			for (TCtorFn* f = __init_array_start; f < __init_array_end; ++f)
				(**f)();

			r = E32Main();
			}
		}
#ifdef __LEAVE_EQUALS_THROW__
		}
	catch (XLeaveException&)
		{
		User::Panic(KLitUser, EUserLeaveWithoutTrap);
		}
	exceptionGlobals.Kill2ndEmergencyBuffer();
#endif
	User::Exit(r);
	}
}
