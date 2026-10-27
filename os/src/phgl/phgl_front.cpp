// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phgl_front.cpp -- OURS: the lazy binding of a Khronos ABI front (libEGL.dll / libGLESv2.dll / libGLESv1_CM.dll)
// to the Mesa backend, phgl.dll (the vendor backend is bound at load). phgl.dll is loaded once per process, on the first call of a front function that
// needs it (that thread -- the process's GL thread -- keeps the library handle for the life of the process).
#include <e32std.h>
#include <e32debug.h>
#include <e32atomics.h>
#include "phgl_front.h"

namespace
	{
	_LIT(KPhgl, "phgl.dll");
	const TInt KOrdEglGetProcAddress = 20;		// libegl14U.def (gen/phgl.def keeps the EGL ordinals)

	RLibrary gLib;
	volatile TInt gState;		// 0 = not tried, 1 = loading, 2 = loaded, 3 = failed

	TBool Load(TInt aOrdinal, const char* aName)
		{
		TUint32 expect = 0;
		if (!__e32_atomic_cas_ord32(&gState, &expect, 1))
			{
			while (gState == 1)
				User::After(1000);		// another thread is loading
			return gState == 2;
			}
		// the handle stays with the loading thread (the process's main / GL thread) for the life of the process
		const TInt r = gLib.Load(KPhgl);
		if (r != KErrNone)
			RDebug::Printf("[phgl] phgl.dll not loaded (r=%d): no OpenGL ES / EGL in this process", r);
		else
			RDebug::Printf("[phgl] phgl.dll loaded (first call: ordinal %d '%s')", aOrdinal, aName ? aName : "");
		__e32_atomic_store_ord32((TUint32*)&gState, r == KErrNone ? 2 : 3);
		return r == KErrNone;
		}
	}

TLibraryFunction PhglResolve(TInt aOrdinal, const char* aName)
	{
	if (!Load(aOrdinal, aName))
		return NULL;
	TLibraryFunction f = aOrdinal > 0 ? gLib.Lookup(aOrdinal) : NULL;
	if (!f && aName)
		{
		typedef TLibraryFunction (*TGetProc)(const char*);
		TGetProc getProc = (TGetProc)gLib.Lookup(KOrdEglGetProcAddress);
		if (getProc)
			f = getProc(aName);
		}
	return f;
	}

TLibraryFunction PhglResolveEgl(TInt aOrdinal, const char* aName)
	{
	// libEGL.dll is in ROM and normally already loaded by the app's own EGL imports; one handle per process, kept for
	// its life (as phgl.dll's above). Its exports are the frozen libegl14U.def ordinals.
	static RLibrary egl;
	static volatile TInt state;			// 0 = not tried, 1 = loading, 2 = loaded, 3 = failed
	TUint32 expect = 0;
	if (__e32_atomic_cas_ord32(&state, &expect, 1))
		{
		_LIT(KEgl, "libEGL.dll");
		const TInt r = egl.Load(KEgl);
		if (r != KErrNone)
			RDebug::Printf("[phgl] libEGL.dll not loaded (r=%d) for '%s'", r, aName ? aName : "");
		__e32_atomic_store_ord32((TUint32*)&state, r == KErrNone ? 2 : 3);
		}
	while (state == 1)
		User::After(1000);
	return state == 2 ? egl.Lookup(aOrdinal) : NULL;
	}

void PhglCheckStatus(const char* aWhat, unsigned aObj, unsigned aStatusEnum, const char* aQuery, const char* aLog)
	{
	static TInt traced;
	if (traced >= 24)
		return;
	typedef void (*TGetIv)(unsigned, unsigned, int*);
	typedef void (*TGetLog)(unsigned, int, int*, char*);
	TGetIv getIv = (TGetIv)PhglResolve(0, aQuery);
	TGetLog getLog = (TGetLog)PhglResolve(0, aLog);
	if (!getIv)
		return;
	int ok = 1;
	getIv(aObj, aStatusEnum, &ok);
	if (ok)
		return;
	++traced;
	char buf[200];
	int len = 0;
	buf[0] = 0;
	if (getLog)
		getLog(aObj, sizeof(buf) - 1, &len, buf);
	buf[len >= 0 && len < (int)sizeof(buf) ? len : sizeof(buf) - 1] = 0;
	for (char* p = buf; *p; ++p)
		if (*p == '\n' || *p == '\r')
			*p = ' ';
	RDebug::Printf("PH: phgl %s %u FAILED: %s", aWhat, aObj, buf);
	}
