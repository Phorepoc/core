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

void PhglTraceCall(const char* aName)
	{
	static const char* names[192];
	static TUint8 counts[192];
	static TInt lines, errors;
	static TBool inside;
	if (inside || (lines >= 1500 && errors >= 64))
		return;
	inside = ETrue;
	typedef unsigned (*TGetError)();
	static TGetError getError;
	if (!getError)
		getError = (TGetError)PhglResolve(0, "glGetError");
	const unsigned err = getError ? getError() : 0;
	if (err && errors < 64)
		{
		++errors;
		RDebug::Printf("PH: phgl GL error 0x%x pending on entry to %s", err, aName);
		}
	TInt i = 0;
	while (i < 192 && names[i] && names[i] != aName)
		++i;
	if (i < 192)
		{
		names[i] = aName;
		if (counts[i] < 3 && lines < 1500)
			{
			++counts[i];
			++lines;
			RDebug::Printf("PH: phgl call %s #%d", aName, counts[i]);
			}
		}
	inside = EFalse;
	}

void PhglTraceArgs(const char* aName, unsigned aA, unsigned aB, int aC, int aD)
	{
	static const char* names[32];
	static TInt counts[32];
	typedef unsigned (*TGetError)();
	static TGetError getError;
	if (!getError)
		getError = (TGetError)PhglResolve(0, "glGetError");
	const unsigned err = getError ? getError() : 0;
	TInt i = 0;
	while (i < 31 && names[i] && names[i] != aName)
		++i;
	names[i] = aName;
	if (counts[i] < 60)
		{
		++counts[i];
		RDebug::Printf("PH: phgl args %s(0x%x, 0x%x, %d, %d) -> error 0x%x", aName, aA, aB, aC, aD, err);
		}
	}

void PhglTraceSource(unsigned aShader, int aCount, const char** aStrings, const int* aLengths)
	{
	static TInt shaders;
	if (shaders >= 16 || !aStrings)
		return;
	++shaders;
	for (TInt s = 0; s < aCount; ++s)
		{
		const char* p = aStrings[s];
		if (!p)
			continue;
		TInt len = aLengths && aLengths[s] >= 0 ? aLengths[s] : User::StringLength((const TUint8*)p);
		TInt line = 0;
		while (len > 0 && *p && line < 120)
			{
			TInt n = 0;
			while (n < len && p[n] && p[n] != '\n' && n < 200)
				++n;
			TBuf8<204> b;
			b.Copy(TPtrC8((const TUint8*)p, n));
			b.TrimAll();
			if (b.Length())
				RDebug::Printf("PH: phgl src %u.%d.%d: %S", aShader, s, line++, &b);
			if (n < len && p[n] == '\n')
				++n;
			p += n;
			len -= n;
			}
		}
	}

void PhglTraceProbe(const char* aName, int aArg)
	{
	static TInt probes;
	if (probes >= 120)
		return;
	++probes;
	typedef void (*TReadPixels)(int, int, int, int, unsigned, unsigned, void*);
	typedef void (*TGetIv)(unsigned, int*);
	static TReadPixels readPixels;
	static TGetIv getIv;
	if (!readPixels)
		readPixels = (TReadPixels)PhglResolve(0, "glReadPixels");
	if (!getIv)
		getIv = (TGetIv)PhglResolve(0, "glGetIntegerv");
	if (!readPixels || !getIv)
		return;
	int vp[4] = {0, 0, 0, 0}, prog = 0, tex = 0, unit = 0, blend = 0, cmask[4] = {0, 0, 0, 0};
	getIv(0x0BA2, vp);
	getIv(0x8B8D, &prog);
	getIv(0x8069, &tex);
	getIv(0x84E0, &unit);
	getIv(0x0BE2, &blend);
	getIv(0x0C23, cmask);
	TUint32 px = 0;
	readPixels(vp[2] / 2, vp[3] / 2, 1, 1, 0x1908, 0x1401, &px);
	RDebug::Printf("PH: phgl probe %s(0x%x): centre %08x prog %d tex %d unit 0x%x blend %d colormask %d%d%d%d", aName, aArg, px, prog,
	               tex, unit, blend, cmask[0], cmask[1], cmask[2], cmask[3]);
	typedef void (*TGetFv)(unsigned, float*);
	static TGetFv getFv;
	if (!getFv)
		getFv = (TGetFv)PhglResolve(0, "glGetFloatv");
	int depthTest = 0, depthFunc = 0, depthMask = 0, cull = 0, cullMode = 0, front = 0, scissor = 0, sbox[4] = {0, 0, 0, 0}, stencil = 0;
	float clearDepth = -1, range[2] = {-1, -1};
	getIv(0x0B71, &depthTest);
	getIv(0x0B74, &depthFunc);
	getIv(0x0B72, &depthMask);
	getIv(0x0B44, &cull);
	getIv(0x0B45, &cullMode);
	getIv(0x0B46, &front);
	getIv(0x0C11, &scissor);
	getIv(0x0C10, sbox);
	getIv(0x0B90, &stencil);
	if (getFv)
		{
		getFv(0x0B73, &clearDepth);
		getFv(0x0B70, range);
		}
	RDebug::Printf("PH: phgl probe   depth test %d func 0x%x mask %d clear x1000 %d range x1000 %d..%d cull %d 0x%x front 0x%x scissor %d %d,%d %dx%d stencil %d",
	               depthTest, depthFunc, depthMask, (int)(clearDepth * 1000), (int)(range[0] * 1000), (int)(range[1] * 1000), cull, cullMode,
	               front, scissor, sbox[0], sbox[1], sbox[2], sbox[3], stencil);
	}
