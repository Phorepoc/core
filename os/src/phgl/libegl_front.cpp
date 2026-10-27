// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// libegl_front.cpp -- OURS: libEGL.dll, the frozen EGL 1.4 ABI (libegl14U.def) as a DISPATCHER over phgl.dll (Mesa):
//   - the display, initialisation, errors and EGL_KHR_reusable_sync are handled HERE, without phgl.dll: the window
//     server's openwfc render stage (openwfcwrapper.cpp: eglGetDisplay + eglInitialize + reusable syncs for its
//     compose jobs) must not pull a 15 MB GL stack into EWSRV.EXE -- the 2010 libegl_ref it replaces was exactly
//     this much;
//   - everything a GL client needs (configs, surfaces, contexts, swaps, fence syncs, extension functions) loads
//     phgl.dll on first use (phgl_front.cpp), initialises Mesa's display once and forwards with Mesa's EGLDisplay in
//     place of ours. Configs/surfaces/contexts are Mesa's handles passed through untouched.
// Two display handles exist: ours (KFrontDisplay) and Mesa's (iMesaDisplay); clients only ever see ours.
#include <e32std.h>
#include <e32debug.h>
#include <e32atomics.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "phgl_front.h"
#include "phgmem.h"
// EGL_NOK_resource_profiling (Nokia's eglext.h; not in the Khronos/Mesa header we build with)
#ifndef EGL_PROF_QUERY_GLOBAL_BIT_NOK
#define EGL_PROF_QUERY_GLOBAL_BIT_NOK 0x0001
#define EGL_PROF_QUERY_MEMORY_USAGE_BIT_NOK 0x0002
#define EGL_PROF_TOTAL_MEMORY_NOK 0x3070
#define EGL_PROF_USED_MEMORY_NOK 0x3071
#define EGL_PROF_PROCESS_ID_NOK 0x3072
#define EGL_PROF_PROCESS_USED_PRIVATE_MEMORY_NOK 0x3073
#define EGL_PROF_PROCESS_USED_SHARED_MEMORY_NOK 0x3074
#define EGL_PROF_TOTAL_MEMORY_USAGE_GT_NOK 0x3075
#define EGL_PROF_TOTAL_MEMORY_USAGE_LT_NOK 0x3076
#define EGL_PROF_MEMORY_USAGE_THRESHOLD_NOK 0x3077
#define EGL_SYNC_RESOURCE_PROFILING_NOK 0x3078
#endif
#ifndef EGL_SYNC_CONDITION_KHR
#define EGL_SYNC_CONDITION_KHR 0x30F8
#endif
#ifdef PH_ALF_TRACE		// ALF bring-up (components.py libegl_alf, staged as libEGL.dll at ALF>=s2): "PH:" lines on the OpenVG route
#define PH_VGT(...) RDebug::Printf("PH: alf: libEGL " __VA_ARGS__)
#else
#define PH_VGT(...)
#endif

namespace
	{
	// phgl.def ordinals of the EGL entry points = libegl14U.def's
	enum TOrd
		{
		EOrdBindAPI = 2, EOrdBindTexImage, EOrdChooseConfig, EOrdCopyBuffers, EOrdCreateContext,
		EOrdCreatePbufferFromClientBuffer, EOrdCreatePbufferSurface, EOrdCreatePixmapSurface, EOrdCreateWindowSurface,
		EOrdDestroyContext, EOrdDestroySurface, EOrdGetConfigAttrib, EOrdGetConfigs, EOrdGetCurrentContext,
		EOrdGetCurrentDisplay, EOrdGetCurrentSurface, EOrdGetDisplay, EOrdGetError, EOrdGetProcAddress, EOrdInitialize,
		EOrdMakeCurrent, EOrdQueryAPI, EOrdQueryContext, EOrdQueryString, EOrdQuerySurface, EOrdReleaseTexImage,
		EOrdReleaseThread, EOrdSurfaceAttrib, EOrdSwapBuffers, EOrdSwapInterval, EOrdTerminate, EOrdWaitClient,
		EOrdWaitGL, EOrdWaitNative, EOrdCount
		};
	const EGLDisplay KFrontDisplay = (EGLDisplay)0x5047;	// what clients see (any non-zero value)

	TLibraryFunction gFn[EOrdCount];
	TBool gInitialized;				// our display
	EGLint gError = EGL_SUCCESS;	// the front's own last error (before Mesa is up)
	EGLDisplay gMesaDisplay = EGL_NO_DISPLAY;
	TBool gMesaInitialized;
	TBool gMesaFailed;

	inline TLibraryFunction Fn(TInt aOrd)
		{
		return gFn[aOrd] ? gFn[aOrd] : (gFn[aOrd] = PhglResolve(aOrd, NULL));
		}

	// Mesa's display, initialised once; EGL_NO_DISPLAY when phgl.dll is not there (every forwarded call then fails
	// with EGL_NOT_INITIALIZED)
	EGLDisplay Mesa()
		{
		if (gMesaInitialized)
			return gMesaDisplay;
		if (gMesaFailed)
			return EGL_NO_DISPLAY;
		typedef EGLDisplay (*TGetDisplay)(EGLNativeDisplayType);
		typedef EGLBoolean (*TInitialize)(EGLDisplay, EGLint*, EGLint*);
		TGetDisplay getDisplay = (TGetDisplay)Fn(EOrdGetDisplay);
		TInitialize initialize = (TInitialize)Fn(EOrdInitialize);
		EGLDisplay d = getDisplay ? getDisplay(EGL_DEFAULT_DISPLAY) : EGL_NO_DISPLAY;
		EGLint major = 0, minor = 0;
		if (d == EGL_NO_DISPLAY || !initialize || !initialize(d, &major, &minor))
			{
			RDebug::Printf("[libEGL] Mesa's display did not initialise (phgl.dll %s)", d ? "up" : "missing");
			gMesaFailed = ETrue;
			return EGL_NO_DISPLAY;
			}
		RDebug::Printf("[libEGL] Mesa EGL %d.%d up behind the front", major, minor);
		gMesaDisplay = d;
		gMesaInitialized = ETrue;
		return d;
		}

	// the EGLDisplay to hand Mesa for a client call on aDpy; EGL_NO_DISPLAY (+ our error) if it cannot be served
	EGLDisplay Forward(EGLDisplay aDpy)
		{
		if (aDpy != KFrontDisplay || !gInitialized)
			{
			gError = aDpy != KFrontDisplay ? EGL_BAD_DISPLAY : EGL_NOT_INITIALIZED;
			return EGL_NO_DISPLAY;
			}
		EGLDisplay d = Mesa();
		if (d == EGL_NO_DISPLAY)
			gError = EGL_NOT_INITIALIZED;
		return d;
		}

	// ---- EGL_KHR_reusable_sync, ours (after egl/eglrefimpl's CEglSync: a mutex + a condition variable); fence syncs
	// are Mesa's, told apart by the handle's low bit
	struct TSync
		{
		TUint32 iMagic;
		EGLenum iType;
		volatile EGLint iStatus;
		RMutex iMutex;
		RCondVar iCond;
		TInt iRefs;
		EGLint iCondition;		// EGL_SYNC_RESOURCE_PROFILING_NOK: EGL_PROF_TOTAL_MEMORY_USAGE_GT_NOK / _LT_NOK
		TInt iThreshold;		//   ... and EGL_PROF_MEMORY_USAGE_THRESHOLD_NOK
		};
	const TUint32 KSyncMagic = 0x53594e43;
	const TInt KMaxSyncs = 64;
	TSync* gSyncs[KMaxSyncs];
	RMutex gSyncLock;
	TBool gSyncLockMade;

	TBool SyncLock()
		{
		if (!gSyncLockMade)
			{
			RMutex m;
			if (m.CreateLocal() != KErrNone)
				return EFalse;
			// (two threads racing here would make two; the loser's is leaked -- the window server makes its first
			// sync from one thread)
			gSyncLock = m;
			gSyncLockMade = ETrue;
			}
		gSyncLock.Wait();
		return ETrue;
		}

	TSync* SyncOf(EGLSyncKHR aSync)
		{
		TSync* s = (TSync*)aSync;
		return s && !((TUint32)s & 1) && s->iMagic == KSyncMagic ? s : NULL;
		}

	// ---- EGL_NOK_resource_profiling over phgmem.h (the real GOOM monitor's only source of numbers) ----
	// Nokia's driver reported its fixed GPU pool; ours reports what is physically left: "total" = used + free, free =
	// min(system free RAM, headroom of the heap holding the most graphics memory) -- see PhGMem::Snapshot.

	// one summary line: totals and the three biggest holders
	void LogGMem(const char* aWhy, const PhGMem::TSnapshot& aSnap)
		{
		TInt top[3] = { -1, -1, -1 };
		for (TInt i = 0; i < aSnap.iCount; ++i)
			for (TInt k = 0; k < 3; ++k)
				if (top[k] < 0 || aSnap.iSlots[i].iBytes > aSnap.iSlots[top[k]].iBytes)
					{
					for (TInt m = 2; m > k; --m)
						top[m] = top[m - 1];
					top[k] = i;
					break;
					}
		TBuf8<200> procs;
		for (TInt k = 0; k < 3 && top[k] >= 0; ++k)
			{
			const PhGMem::TSlot& s = aSnap.iSlots[top[k]];
			procs.AppendFormat(_L8(" pid %u %dKB(peak %d, allocs %u, heap left %dKB)"), s.iPid, s.iBytes >> 10,
			                   s.iPeak >> 10, s.iAllocs, s.iHeadroom >> 10);
			}
		procs.Append(TChar(0));
		RDebug::Printf("PH: gmem %s used=%dKB free=%dKB sysfree=%dKB procs=%d%s", aWhy, aSnap.iUsed >> 10,
		               aSnap.iFree >> 10, aSnap.iSysFree >> 10, aSnap.iCount, (const char*)procs.Ptr());
		}

	EGLBoolean QueryProfilingDataNOK(EGLDisplay aDpy, EGLint aBits, EGLint* aData, EGLint aSize, EGLint* aCount)
		{
		if (aDpy != KFrontDisplay || !aCount)
			{
			gError = aDpy != KFrontDisplay ? EGL_BAD_DISPLAY : EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		PhGMem::TSnapshot snap;
		PhGMem::Snapshot(snap);
		EGLint buf[4 + PhGMem::KSlots * 7];
		TInt n = 0;
		if (aBits & EGL_PROF_QUERY_MEMORY_USAGE_BIT_NOK)
			{
			buf[n++] = EGL_PROF_TOTAL_MEMORY_NOK;
			buf[n++] = snap.iUsed + snap.iFree;
			buf[n++] = EGL_PROF_USED_MEMORY_NOK;
			buf[n++] = snap.iUsed;
			}
		const TUint32 me = (TUint32)RProcess().Id().Id();
		for (TInt i = 0; i < snap.iCount; ++i)
			{
			const PhGMem::TSlot& s = snap.iSlots[i];
			if (!(aBits & EGL_PROF_QUERY_GLOBAL_BIT_NOK) && s.iPid != me)
				continue;
			buf[n++] = EGL_PROF_PROCESS_ID_NOK;	// EGLNativeProcessIdTypeNOK is a TUint64: low word first
			buf[n++] = (EGLint)s.iPid;
			buf[n++] = 0;
			buf[n++] = EGL_PROF_PROCESS_USED_PRIVATE_MEMORY_NOK;
			buf[n++] = s.iBytes;
			buf[n++] = EGL_PROF_PROCESS_USED_SHARED_MEMORY_NOK;
			buf[n++] = 0;
			}
		if (!aData)
			{
			*aCount = n;
			return EGL_TRUE;
			}
		const TInt copy = n < aSize ? n : aSize;
		for (TInt i = 0; i < copy; ++i)
			aData[i] = buf[i];
		*aCount = copy;
		// GOOM queries in bursts (each check, each app list refresh): one line per 2 s at most
		static TUint32 lastLog;
		const TUint32 now = User::NTickCount();
		if (lastLog == 0 || now - lastLog >= 2000)
			{
			lastLog = now ? now : 1;
			LogGMem("query", snap);
			}
		return EGL_TRUE;
		}

	// GOOM (goomthresholdcrossedao.inl) encodes a FREE-memory threshold F as a usage threshold 32 MB - F (Nokia's pool).
	// Decode it back, so the condition agrees with the free figure the profiling query reports.
	TBool ProfilingConditionMet(const TSync& aSync, const PhGMem::TSnapshot& aSnap)
		{
		const TInt freeThreshold = 32 * 1024 * 1024 - aSync.iThreshold;
		return aSync.iCondition == EGL_PROF_TOTAL_MEMORY_USAGE_GT_NOK ? aSnap.iFree < freeThreshold
		                                                              : aSnap.iFree > freeThreshold;
		}

	EGLSyncKHR CreateProfilingSync(const EGLint* aAttribs)
		{
		EGLint condition = 0;
		TInt threshold = -1;
		for (const EGLint* a = aAttribs; a && a[0] != EGL_NONE; a += 2)
			{
			if (a[0] == EGL_SYNC_CONDITION_KHR)
				condition = a[1];
			else if (a[0] == EGL_PROF_MEMORY_USAGE_THRESHOLD_NOK)
				threshold = a[1];
			}
		if ((condition != EGL_PROF_TOTAL_MEMORY_USAGE_GT_NOK && condition != EGL_PROF_TOTAL_MEMORY_USAGE_LT_NOK) || threshold < 0)
			{
			gError = EGL_BAD_ATTRIBUTE;
			return EGL_NO_SYNC_KHR;
			}
		TSync* s = new TSync;
		if (!s || s->iMutex.CreateLocal() != KErrNone || s->iCond.CreateLocal() != KErrNone)
			{
			if (s)
				{
				s->iCond.Close();
				s->iMutex.Close();
				delete s;
				}
			gError = EGL_BAD_ALLOC;
			return EGL_NO_SYNC_KHR;
			}
		s->iMagic = KSyncMagic;
		s->iType = EGL_SYNC_RESOURCE_PROFILING_NOK;
		s->iStatus = EGL_UNSIGNALED_KHR;
		s->iRefs = 1;
		s->iCondition = condition;
		s->iThreshold = threshold;
		RDebug::Printf("PH: gmem sync %s free %dKB", condition == EGL_PROF_TOTAL_MEMORY_USAGE_GT_NOK ? "low below" : "good above",
		               (32 * 1024 * 1024 - threshold) >> 10);
		return (EGLSyncKHR)s;
		}

	// wait on a profiling sync: poll the table (GOOM's own thread blocks here); destroying the sync releases the wait.
	// The poll also writes the periodic summary -- the graphics memory trend over time, for leak hunting.
	EGLint WaitProfilingSync(TSync* aSync, EGLTimeKHR aTimeout)
		{
		const TInt KPollUs = 250000;
		TInt64 left = aTimeout == EGL_FOREVER_KHR ? -1 : (TInt64)(aTimeout / 1000);
		static TUint32 lastLog;
		static TInt lastUsed = -1;
		aSync->iMutex.Wait();
		EGLint ret = EGL_TIMEOUT_EXPIRED_KHR;
		for (;;)
			{
			PhGMem::TSnapshot snap;
			PhGMem::Snapshot(snap);
			const TUint32 now = User::NTickCount();
			const TInt delta = lastUsed < 0 ? KMaxTInt : Abs(snap.iUsed - lastUsed);
			if (now - lastLog >= 300000 || (now - lastLog >= 30000 && delta >= 256 * 1024))
				{
				lastLog = now;
				lastUsed = snap.iUsed;
				LogGMem("trend", snap);
				}
			if (aSync->iStatus == EGL_SIGNALED_KHR)
				{
				ret = EGL_CONDITION_SATISFIED_KHR;
				break;
				}
			if (ProfilingConditionMet(*aSync, snap))
				{
				LogGMem(aSync->iCondition == EGL_PROF_TOTAL_MEMORY_USAGE_GT_NOK ? "LOW (goom acts)" : "GOOD again", snap);
				aSync->iStatus = EGL_SIGNALED_KHR;
				ret = EGL_CONDITION_SATISFIED_KHR;
				break;
				}
			if (left == 0)
				break;
			const TInt slice = left < 0 || left > KPollUs ? KPollUs : (TInt)left;
			aSync->iCond.TimedWait(aSync->iMutex, slice);
			if (left > 0)
				left -= slice;
			}
		aSync->iMutex.Signal();
		return ret;
		}

	EGLSyncKHR CreateSyncKHR(EGLDisplay aDpy, EGLenum aType, const EGLint* aAttribs)
		{
		if (aDpy != KFrontDisplay || !gInitialized)
			{
			gError = EGL_BAD_DISPLAY;
			return EGL_NO_SYNC_KHR;
			}
		if (aType == EGL_SYNC_RESOURCE_PROFILING_NOK)
			return CreateProfilingSync(aAttribs);
		if (aType != EGL_SYNC_REUSABLE_KHR)
			{
			// a fence sync is Mesa's: forward, tag the handle
			EGLDisplay d = Mesa();
			typedef EGLSyncKHR (*TCreate)(EGLDisplay, EGLenum, const EGLint*);
			typedef void (*(*TGetProc)(const char*))();
			TGetProc getProc = (TGetProc)Fn(EOrdGetProcAddress);
			TCreate create = d != EGL_NO_DISPLAY && getProc ? (TCreate)getProc("eglCreateSyncKHR") : NULL;
			EGLSyncKHR s = create ? create(d, aType, aAttribs) : EGL_NO_SYNC_KHR;
			if (s == EGL_NO_SYNC_KHR)
				gError = EGL_BAD_ATTRIBUTE;
			return s == EGL_NO_SYNC_KHR ? EGL_NO_SYNC_KHR : (EGLSyncKHR)((TUint32)s | 1);
			}
		if (aAttribs && aAttribs[0] != EGL_NONE)
			{
			gError = EGL_BAD_ATTRIBUTE;
			return EGL_NO_SYNC_KHR;
			}
		TSync* s = new TSync;
		if (!s)
			{
			gError = EGL_BAD_ALLOC;
			return EGL_NO_SYNC_KHR;
			}
		s->iMagic = KSyncMagic;
		s->iType = EGL_SYNC_REUSABLE_KHR;
		s->iStatus = EGL_UNSIGNALED_KHR;
		s->iRefs = 1;
		if (s->iMutex.CreateLocal() != KErrNone || s->iCond.CreateLocal() != KErrNone)
			{
			s->iCond.Close();
			s->iMutex.Close();
			delete s;
			gError = EGL_BAD_ALLOC;
			return EGL_NO_SYNC_KHR;
			}
		if (SyncLock())
			{
			for (TInt i = 0; i < KMaxSyncs; ++i)
				if (!gSyncs[i])
					{
					gSyncs[i] = s;
					break;
					}
			gSyncLock.Signal();
			}
		static TInt made;
		if (made++ < 3)
			RDebug::Printf("[libEGL] reusable sync %d created", made);
		return (EGLSyncKHR)s;
		}

	void SyncRelease(TSync* aSync)
		{
		aSync->iMutex.Wait();
		const TBool last = --aSync->iRefs == 0;
		aSync->iMutex.Signal();
		if (last)
			{
			aSync->iMagic = 0;
			aSync->iCond.Close();
			aSync->iMutex.Close();
			delete aSync;
			}
		}

	EGLBoolean DestroySyncKHR(EGLDisplay aDpy, EGLSyncKHR aSync)
		{
		if ((TUint32)aSync & 1)
			{
			EGLDisplay d = Mesa();
			typedef EGLBoolean (*TDestroy)(EGLDisplay, EGLSyncKHR);
			typedef void (*(*TGetProc)(const char*))();
			TGetProc getProc = (TGetProc)Fn(EOrdGetProcAddress);
			TDestroy destroy = d != EGL_NO_DISPLAY && getProc ? (TDestroy)getProc("eglDestroySyncKHR") : NULL;
			return destroy ? destroy(d, (EGLSyncKHR)((TUint32)aSync & ~1u)) : EGL_FALSE;
			}
		TSync* s = SyncOf(aSync);
		if (aDpy != KFrontDisplay || !s)
			{
			gError = s ? EGL_BAD_DISPLAY : EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		if (SyncLock())
			{
			for (TInt i = 0; i < KMaxSyncs; ++i)
				if (gSyncs[i] == s)
					gSyncs[i] = NULL;
			gSyncLock.Signal();
			}
		// unblock every waiter, then drop the creator's reference (waiters hold their own)
		s->iMutex.Wait();
		s->iStatus = EGL_SIGNALED_KHR;
		s->iCond.Broadcast();
		s->iMutex.Signal();
		SyncRelease(s);
		return EGL_TRUE;
		}

	EGLint ClientWaitSyncKHR(EGLDisplay aDpy, EGLSyncKHR aSync, EGLint aFlags, EGLTimeKHR aTimeout)
		{
		if ((TUint32)aSync & 1)
			{
			EGLDisplay d = Mesa();
			typedef EGLint (*TWait)(EGLDisplay, EGLSyncKHR, EGLint, EGLTimeKHR);
			typedef void (*(*TGetProc)(const char*))();
			TGetProc getProc = (TGetProc)Fn(EOrdGetProcAddress);
			TWait wait = d != EGL_NO_DISPLAY && getProc ? (TWait)getProc("eglClientWaitSyncKHR") : NULL;
			return wait ? wait(d, (EGLSyncKHR)((TUint32)aSync & ~1u), aFlags, aTimeout) : EGL_FALSE;
			}
		TSync* s = SyncOf(aSync);
		if (aDpy != KFrontDisplay || !s)
			{
			gError = s ? EGL_BAD_DISPLAY : EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		(void)aFlags;	// EGL_SYNC_FLUSH_COMMANDS_BIT_KHR: no GL context of ours to flush
		if (s->iType == EGL_SYNC_RESOURCE_PROFILING_NOK)
			{
			s->iMutex.Wait();
			++s->iRefs;
			s->iMutex.Signal();
			const EGLint r = WaitProfilingSync(s, aTimeout);
			SyncRelease(s);
			return r;
			}
		s->iMutex.Wait();
		++s->iRefs;
		EGLint ret = EGL_CONDITION_SATISFIED_KHR;
		if (s->iStatus != EGL_SIGNALED_KHR)
			{
			if (aTimeout == 0)
				ret = EGL_TIMEOUT_EXPIRED_KHR;
			else if (aTimeout == EGL_FOREVER_KHR)
				{
				while (s->iStatus != EGL_SIGNALED_KHR)
					s->iCond.Wait(s->iMutex);
				}
			else
				{
				// RCondVar::TimedWait takes microseconds (a TInt): clamp a longer timeout to ~35 min slices
				TInt64 left = (TInt64)(aTimeout / 1000);
				while (s->iStatus != EGL_SIGNALED_KHR && left > 0)
					{
					const TInt slice = left > 2000000000LL ? 2000000000 : (TInt)left;
					const TInt r = s->iCond.TimedWait(s->iMutex, slice);
					if (r == KErrTimedOut)
						left -= slice;
					}
				if (s->iStatus != EGL_SIGNALED_KHR)
					ret = EGL_TIMEOUT_EXPIRED_KHR;
				}
			}
		s->iMutex.Signal();
		SyncRelease(s);
		return ret;
		}

	EGLBoolean SignalSyncKHR(EGLDisplay aDpy, EGLSyncKHR aSync, EGLenum aMode)
		{
		TSync* s = SyncOf(aSync);
		if (aDpy != KFrontDisplay || !s)
			{
			gError = s ? EGL_BAD_DISPLAY : EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		if (aMode != EGL_SIGNALED_KHR && aMode != EGL_UNSIGNALED_KHR)
			{
			gError = EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		s->iMutex.Wait();
		s->iStatus = aMode;
		if (aMode == EGL_SIGNALED_KHR)
			s->iCond.Broadcast();
		s->iMutex.Signal();
		return EGL_TRUE;
		}

	// The Nokia private extension libWFC REQUIRES (wfccontext.c: no context without it -> no render stage ->
	// WSERV-INTERNAL 112): eglSignalSyncKHR callable from any thread (its composer thread breaks the job fence
	// with it). The 2010 reference returns the signal's EGLBoolean; libWFC treats 0 as failure.
	EGLint PrivateSignalSyncNOK(EGLDisplay aDpy, EGLSyncKHR aSync, EGLenum aMode)
		{
		return SignalSyncKHR(aDpy, aSync, aMode);
		}

	EGLBoolean GetSyncAttribKHR(EGLDisplay aDpy, EGLSyncKHR aSync, EGLint aAttribute, EGLint* aValue)
		{
		if ((TUint32)aSync & 1)
			{
			EGLDisplay d = Mesa();
			typedef EGLBoolean (*TGet)(EGLDisplay, EGLSyncKHR, EGLint, EGLint*);
			typedef void (*(*TGetProc)(const char*))();
			TGetProc getProc = (TGetProc)Fn(EOrdGetProcAddress);
			TGet get = d != EGL_NO_DISPLAY && getProc ? (TGet)getProc("eglGetSyncAttribKHR") : NULL;
			return get ? get(d, (EGLSyncKHR)((TUint32)aSync & ~1u), aAttribute, aValue) : EGL_FALSE;
			}
		TSync* s = SyncOf(aSync);
		if (aDpy != KFrontDisplay || !s || !aValue)
			{
			gError = s ? (aValue ? EGL_BAD_DISPLAY : EGL_BAD_PARAMETER) : EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		switch (aAttribute)
			{
			case EGL_SYNC_TYPE_KHR: *aValue = (EGLint)s->iType; return EGL_TRUE;
			case EGL_SYNC_STATUS_KHR: *aValue = s->iStatus; return EGL_TRUE;
			default: gError = EGL_BAD_ATTRIBUTE; return EGL_FALSE;
			}
		}

	// GPU plan G6: EGLImages are Mesa's (egl_symbian.c: EGL_LINUX_DMA_BUF_EXT with RChunk handles as the plane fds);
	// the front only swaps its display for Mesa's. The image handle goes back and forth untouched.
	typedef void* TEGLImage;
	TEGLImage CreateImageKHR(EGLDisplay aDpy, EGLContext aCtx, EGLenum aTarget, EGLClientBuffer aBuffer,
		const EGLint* aAttribs)
		{
		EGLDisplay d = Mesa();
		if (aDpy != KFrontDisplay || !gInitialized || d == EGL_NO_DISPLAY)
			{
			gError = EGL_BAD_DISPLAY;
			return NULL;
			}
		typedef TEGLImage (*TCreate)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint*);
		typedef void (*(*TGetProc)(const char*))();
		TGetProc getProc = (TGetProc)Fn(EOrdGetProcAddress);
		TCreate create = getProc ? (TCreate)getProc("eglCreateImageKHR") : NULL;
		TEGLImage img = create ? create(d, aCtx, aTarget, aBuffer, aAttribs) : NULL;
		if (!img)
			{
			typedef EGLint (*TErr)();
			TErr err = (TErr)Fn(EOrdGetError);
			gError = err ? err() : EGL_BAD_ALLOC;
			}
		return img;
		}

	EGLBoolean DestroyImageKHR(EGLDisplay aDpy, TEGLImage aImage)
		{
		EGLDisplay d = Mesa();
		if (aDpy != KFrontDisplay || d == EGL_NO_DISPLAY)
			{
			gError = EGL_BAD_DISPLAY;
			return EGL_FALSE;
			}
		typedef EGLBoolean (*TDestroy)(EGLDisplay, TEGLImage);
		typedef void (*(*TGetProc)(const char*))();
		TGetProc getProc = (TGetProc)Fn(EOrdGetProcAddress);
		TDestroy destroy = getProc ? (TDestroy)getProc("eglDestroyImageKHR") : NULL;
		return destroy ? destroy(d, aImage) : EGL_FALSE;
		}

	TBool StrEq(const char* a, const char* b)
		{
		while (*a && *a == *b)
			{
			++a;
			++b;
			}
		return *a == *b;
		}

	// ---- OpenVG (milestone O1, openvg-plan.md): eglBindAPI(EGL_OPENVG_API) routes this THREAD's config/surface/context
	// calls to the VG-only EGL (libEGL_sw.dll = riMiniEGL over the Khronos reference implementation, sflibopenvgrefimpl),
	// so a client linking libEGL + libOpenVG (HWVG, rendervg10, nvgdecoder) works unchanged. Its handles never reach
	// Mesa: configs / surfaces / contexts the VG EGL returned are remembered (gVgSet) and any call on one goes back to it.
	// The GLES path is untouched, and a process that never binds OpenVG never loads libEGL_sw.dll.
	const TInt KMaxVgThreads = 16;
	volatile TUint32 gApiTid[KMaxVgThreads];		// thread id + 1 (0 = free slot); the VG-bound threads
	const TInt KMaxVg = 128;
	volatile TUint32 gVgSet[KMaxVg];			// the VG EGL's handles
	EGLDisplay gSwDisplay = EGL_NO_DISPLAY;

	TUint32 ThreadKey()
		{
		return (TUint32)RThread().Id().Id() + 1;
		}

	TBool VgBound()
		{
		const TUint32 k = ThreadKey();
		for (TInt i = 0; i < KMaxVgThreads; i++)
			if (gApiTid[i] == k)
				return ETrue;
		return EFalse;
		}

	void SetVgBound(TBool aOn)
		{
		const TUint32 k = ThreadKey();
		if (aOn)
			{
			if (VgBound())
				return;
			for (TInt i = 0; i < KMaxVgThreads; i++)
				{
				TUint32 e = 0;
				if (__e32_atomic_cas_ord32(&gApiTid[i], &e, k))
					return;
				}
			}
		else
			for (TInt i = 0; i < KMaxVgThreads; i++)
				{
				TUint32 e = k;
				if (__e32_atomic_cas_ord32(&gApiTid[i], &e, 0))
					return;
				}
		}

	TBool VgHas(const void* aHandle)
		{
		if (!aHandle)
			return EFalse;
		for (TInt i = 0; i < KMaxVg; i++)
			if (gVgSet[i] == (TUint32)aHandle)
				return ETrue;
		return EFalse;
		}

	void VgAdd(const void* aHandle)
		{
		if (!aHandle || VgHas(aHandle))
			return;
		for (TInt i = 0; i < KMaxVg; i++)
			{
			TUint32 e = 0;
			if (__e32_atomic_cas_ord32(&gVgSet[i], &e, (TUint32)aHandle))
				return;
			}
		RDebug::Printf("[libEGL] VG handle table full");
		}

	void VgDel(const void* aHandle)
		{
		for (TInt i = 0; i < KMaxVg; i++)
			{
			TUint32 e = (TUint32)aHandle;
			if (__e32_atomic_cas_ord32(&gVgSet[i], &e, 0))
				return;
			}
		}

	// the VG EGL's entry point (libegl13U.def ordinals == libegl14U.def's for the names it has)
	TLibraryFunction SwFn(TInt aOrd)
		{
		static RLibrary lib;
		static volatile TInt state;			// 0 = not tried, 1 = loading, 2 = loaded, 3 = failed
		TUint32 expect = 0;
		if (__e32_atomic_cas_ord32(&state, &expect, 1))
			{
			_LIT(KSw, "libEGL_sw.dll");
			PH_VGT("loading libEGL_sw.dll (thread %d)", (int)RThread().Id().Id());
			const TInt r = lib.Load(KSw);
			RDebug::Printf("[libEGL] OpenVG: libEGL_sw.dll %s (r=%d)", r == KErrNone ? "loaded" : "NOT loaded", r);
			__e32_atomic_store_ord32((TUint32*)&state, r == KErrNone ? 2 : 3);
			}
		while (state == 1)
			User::After(1000);
		return state == 2 ? lib.Lookup(aOrd) : NULL;
		}

	// the VG EGL's own (initialised) display
	EGLDisplay SwDisplay()
		{
		if (gSwDisplay != EGL_NO_DISPLAY)
			return gSwDisplay;
		typedef EGLDisplay (*TGetDisplay)(EGLNativeDisplayType);
		typedef EGLBoolean (*TInitialize)(EGLDisplay, EGLint*, EGLint*);
		TGetDisplay getDisplay = (TGetDisplay)SwFn(EOrdGetDisplay);
		TInitialize initialize = (TInitialize)SwFn(EOrdInitialize);
		EGLDisplay d = getDisplay ? getDisplay(EGL_DEFAULT_DISPLAY) : EGL_NO_DISPLAY;
		EGLint major = 0, minor = 0;
		if (d == EGL_NO_DISPLAY || !initialize || !initialize(d, &major, &minor))
			return EGL_NO_DISPLAY;
		RDebug::Printf("[libEGL] OpenVG EGL %d.%d up behind the front", major, minor);
		gSwDisplay = d;
		return d;
		}

	// the VG EGL's display for a client call on aDpy; EGL_NO_DISPLAY (+ our error) if it cannot be served
	EGLDisplay SwForward(EGLDisplay aDpy)
		{
		if (aDpy != KFrontDisplay || !gInitialized)
			{
			gError = aDpy != KFrontDisplay ? EGL_BAD_DISPLAY : EGL_NOT_INITIALIZED;
			return EGL_NO_DISPLAY;
			}
		EGLDisplay d = SwDisplay();
		if (d == EGL_NO_DISPLAY)
			gError = EGL_NOT_INITIALIZED;
		return d;
		}
	}

#define SWFWD(ord, ret, fail, sig, args) \
	{ \
	EGLDisplay d = SwForward(dpy); \
	typedef ret (*F) sig; \
	F f = d != EGL_NO_DISPLAY ? (F)SwFn(ord) : NULL; \
	return f ? f args : (fail); \
	}

extern "C"
{

EGLint eglGetError(void)
	{
	// A thread bound to OpenVG (Hitchcock's alfredserver, WSERV's icon rasteriser) must get the VG EGL's error, and must never enter Mesa: once ANOTHER
	// thread of the process has brought Mesa up (phvg's GL worker, ALF_PHVG), Mesa's per-thread state would be created for a thread that never used it
	// (KERN-EXEC 0 on the first eglGetError after an eglMakeCurrent, boot 803) and the VG error would be lost anyway.
	if (VgBound())
		{
		if (gSwDisplay != EGL_NO_DISPLAY)
			{
			typedef EGLint (*F)(void);
			F f = (F)SwFn(EOrdGetError);
			if (f)
				return f();
			}
		const EGLint e = gError;
		gError = EGL_SUCCESS;
		return e;
		}
	if (gMesaInitialized)
		{
		typedef EGLint (*F)(void);
		F f = (F)Fn(EOrdGetError);
		const EGLint e = f ? f() : EGL_SUCCESS;
		if (e != EGL_SUCCESS)
			{
			gError = EGL_SUCCESS;
			return e;
			}
		}
	if (gSwDisplay != EGL_NO_DISPLAY && gError == EGL_SUCCESS)
		{
		typedef EGLint (*F)(void);
		F f = (F)SwFn(EOrdGetError);
		return f ? f() : EGL_SUCCESS;
		}
	const EGLint e = gError;
	gError = EGL_SUCCESS;
	return e;
	}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
	{
	(void)display_id;
	return KFrontDisplay;
	}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor)
	{
	if (dpy != KFrontDisplay)
		{
		gError = EGL_BAD_DISPLAY;
		return EGL_FALSE;
		}
	if (!gInitialized)
		RDebug::Printf("[libEGL] display initialised (front)");
	gInitialized = ETrue;
	if (major)
		*major = 1;
	if (minor)
		*minor = 4;
	return EGL_TRUE;
	}

EGLBoolean eglTerminate(EGLDisplay dpy)
	{
	if (dpy != KFrontDisplay)
		{
		gError = EGL_BAD_DISPLAY;
		return EGL_FALSE;
		}
	// (Mesa's display stays up for the process: a re-initialise is cheap and surfaces of other threads survive)
	gInitialized = EFalse;
	return EGL_TRUE;
	}

const char* eglQueryString(EGLDisplay dpy, EGLint name)
	{
	if (dpy != KFrontDisplay || !gInitialized)
		{
		gError = dpy != KFrontDisplay ? EGL_BAD_DISPLAY : EGL_NOT_INITIALIZED;
		return NULL;
		}
	switch (name)
		{
		case EGL_VENDOR: return "Phorepoc (Mesa)";
		case EGL_VERSION: return "1.4 Phorepoc";
		case EGL_CLIENT_APIS: return "OpenGL_ES OpenVG";
		case EGL_EXTENSIONS:
			return "EGL_KHR_reusable_sync EGL_KHR_fence_sync EGL_KHR_wait_sync EGL_KHR_surfaceless_context "
			       "EGL_KHR_create_context EGL_KHR_image_base EGL_EXT_image_dma_buf_import EGL_KHR_image_pixmap "
			       "EGL_SYMBIAN_image_preserved EGL_SYMBIAN_PIXMAP_TYPE_RSGIMAGE EGL_NOK_pixmap_type_rsgimage "
			       "EGL_NOK_resource_profiling";
		default: gError = EGL_BAD_PARAMETER; return NULL;
		}
	}

void (*eglGetProcAddress(const char* procname))(...)
	{
	typedef void (*TProc)(...);
	if (!procname)
		return NULL;
	static TInt asked;
	if (asked++ < 8)
		RDebug::Printf("[libEGL] eglGetProcAddress('%s')", procname);
	if (StrEq(procname, "eglCreateSyncKHR")) return (TProc)CreateSyncKHR;
	if (StrEq(procname, "eglDestroySyncKHR")) return (TProc)DestroySyncKHR;
	if (StrEq(procname, "eglClientWaitSyncKHR")) return (TProc)ClientWaitSyncKHR;
	if (StrEq(procname, "eglSignalSyncKHR")) return (TProc)SignalSyncKHR;
	if (StrEq(procname, "eglGetSyncAttribKHR")) return (TProc)GetSyncAttribKHR;
	if (StrEq(procname, "eglCreateImageKHR")) return (TProc)CreateImageKHR;
	if (StrEq(procname, "eglDestroyImageKHR")) return (TProc)DestroyImageKHR;
	if (StrEq(procname, "egl_Private_SignalSyncNOK")) return (TProc)PrivateSignalSyncNOK;
	if (StrEq(procname, "eglQueryProfilingDataNOK")) return (TProc)QueryProfilingDataNOK;
	// other Nokia-private / 2010-debug names (egl_Private_*, egliDebug*): not ours, and not worth loading Mesa for
	if (procname[0] == 'e' && procname[1] == 'g' && procname[2] == 'l' && (procname[3] == '_' || procname[3] == 'i'))
		return NULL;
	// a GL extension or something else of Mesa's
	typedef void (*(*F)(const char*))(...);
	F f = (F)Fn(EOrdGetProcAddress);
	return f ? f(procname) : NULL;
	}

EGLBoolean eglBindAPI(EGLenum api)
	{
	if (api == EGL_OPENVG_API)
		{
		typedef EGLBoolean (*F)(EGLenum);
		F f = (F)SwFn(EOrdBindAPI);
		if (!f || !f(api))
			{
			gError = EGL_BAD_PARAMETER;
			return EGL_FALSE;
			}
		SetVgBound(ETrue);
		return EGL_TRUE;
		}
	if (api != EGL_OPENGL_ES_API)
		{
		gError = EGL_BAD_PARAMETER;
		return EGL_FALSE;
		}
	SetVgBound(EFalse);
	typedef EGLBoolean (*F)(EGLenum);
	F f = (F)Fn(EOrdBindAPI);
	return f ? f(api) : EGL_TRUE;
	}

EGLenum eglQueryAPI(void)
	{
	return VgBound() ? EGL_OPENVG_API : EGL_OPENGL_ES_API;
	}

EGLBoolean eglReleaseThread(void)
	{
	if (VgBound() && gSwDisplay != EGL_NO_DISPLAY)
		{
		typedef EGLBoolean (*F)(void);
		F f = (F)SwFn(EOrdReleaseThread);
		if (f)
			f();
		}
	SetVgBound(EFalse);
	typedef EGLBoolean (*F)(void);
	F f = gMesaInitialized ? (F)Fn(EOrdReleaseThread) : NULL;
	return f ? f() : EGL_TRUE;
	}

EGLBoolean eglWaitClient(void)
	{
	typedef EGLBoolean (*F)(void);
	F f = VgBound() ? (F)SwFn(EOrdWaitClient) : gMesaInitialized ? (F)Fn(EOrdWaitClient) : NULL;
	static TInt said;
	if (said++ < 4)
		RDebug::Printf("[libEGL] eglWaitClient vg %d mesa %d -> %08x", VgBound(), gMesaInitialized, (TUint)f);
	return f ? f() : EGL_TRUE;
	}

EGLBoolean eglWaitGL(void)
	{
	typedef EGLBoolean (*F)(void);
	F f = gMesaInitialized ? (F)Fn(EOrdWaitGL) : NULL;
	return f ? f() : EGL_TRUE;
	}

EGLBoolean eglWaitNative(EGLint engine)
	{
	typedef EGLBoolean (*F)(EGLint);
	F f = gMesaInitialized ? (F)Fn(EOrdWaitNative) : NULL;
	return f ? f(engine) : EGL_TRUE;
	}

EGLContext eglGetCurrentContext(void)
	{
	typedef EGLContext (*F)(void);
	F f = VgBound() ? (F)SwFn(EOrdGetCurrentContext) : gMesaInitialized ? (F)Fn(EOrdGetCurrentContext) : NULL;
	return f ? f() : EGL_NO_CONTEXT;
	}

EGLDisplay eglGetCurrentDisplay(void)
	{
	return eglGetCurrentContext() != EGL_NO_CONTEXT ? KFrontDisplay : EGL_NO_DISPLAY;
	}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
	{
	typedef EGLSurface (*F)(EGLint);
	F f = VgBound() ? (F)SwFn(EOrdGetCurrentSurface) : gMesaInitialized ? (F)Fn(EOrdGetCurrentSurface) : NULL;
	return f ? f(readdraw) : EGL_NO_SURFACE;
	}

// ---- forwarded with Mesa's display (or, for an OpenVG-bound thread / a VG handle, the VG EGL's) -----------------------

#define FORWARD(ord, ret, fail, sig, args) \
	{ \
	EGLDisplay d = Forward(dpy); \
	typedef ret (*F) sig; \
	F f = d != EGL_NO_DISPLAY ? (F)Fn(ord) : NULL; \
	return f ? f args : (fail); \
	}

// VG configs the VG EGL returned are remembered, so getConfigAttrib / createContext can tell them from Mesa's
EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig* configs, EGLint config_size, EGLint* num_config)
	{
	if (VgBound())
		{
		EGLDisplay d = SwForward(dpy);
		typedef EGLBoolean (*F)(EGLDisplay, EGLConfig*, EGLint, EGLint*);
		F f = d != EGL_NO_DISPLAY ? (F)SwFn(EOrdGetConfigs) : NULL;
		const EGLBoolean ok = f ? f(d, configs, config_size, num_config) : EGL_FALSE;
		if (ok && configs && num_config)
			for (EGLint i = 0; i < *num_config && i < config_size; i++)
				VgAdd(configs[i]);
		return ok;
		}
	FORWARD(EOrdGetConfigs, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLConfig*, EGLint, EGLint*),
	        (d, configs, config_size, num_config))
	}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size,
	EGLint* num_config)
	{
	if (VgBound())
		{
		EGLDisplay d = SwForward(dpy);
		typedef EGLBoolean (*F)(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
		F f = d != EGL_NO_DISPLAY ? (F)SwFn(EOrdChooseConfig) : NULL;
		const EGLBoolean ok = f ? f(d, attrib_list, configs, config_size, num_config) : EGL_FALSE;
		if (ok && configs && num_config)
			for (EGLint i = 0; i < *num_config && i < config_size; i++)
				VgAdd(configs[i]);
		return ok;
		}
	FORWARD(EOrdChooseConfig, EGLBoolean, EGL_FALSE, (EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*),
	        (d, attrib_list, configs, config_size, num_config))
	}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value)
	{
	if (VgHas(config))
		SWFWD(EOrdGetConfigAttrib, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLConfig, EGLint, EGLint*),
		      (d, config, attribute, value))
	FORWARD(EOrdGetConfigAttrib, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLConfig, EGLint, EGLint*),
	        (d, config, attribute, value))
	}

// a surface the VG EGL creates is remembered (VgAdd)
#define VG_CREATE_SURFACE(ord, sig, args) \
	{ \
	PH_VGT("VG create-surface ord %d thread %d: SwForward", (int)(ord), (int)RThread().Id().Id()); \
	EGLDisplay d = SwForward(dpy); \
	PH_VGT("SwForward -> %x", (unsigned)d); \
	typedef EGLSurface (*F) sig; \
	F f = d != EGL_NO_DISPLAY ? (F)SwFn(ord) : NULL; \
	PH_VGT("SwFn -> %x, calling the OpenVG EGL", (unsigned)f); \
	EGLSurface s = f ? f args : EGL_NO_SURFACE; \
	PH_VGT("the OpenVG EGL returned %x", (unsigned)s); \
	VgAdd(s); \
	return s; \
	}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win,
	const EGLint* attrib_list)
	{
	if (VgHas(config))
		VG_CREATE_SURFACE(EOrdCreateWindowSurface, (EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*),
		                  (d, config, win, attrib_list))
	FORWARD(EOrdCreateWindowSurface, EGLSurface, EGL_NO_SURFACE,
	        (EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint*), (d, config, win, attrib_list))
	}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list)
	{
	if (VgHas(config))
		VG_CREATE_SURFACE(EOrdCreatePbufferSurface, (EGLDisplay, EGLConfig, const EGLint*), (d, config, attrib_list))
	FORWARD(EOrdCreatePbufferSurface, EGLSurface, EGL_NO_SURFACE, (EGLDisplay, EGLConfig, const EGLint*),
	        (d, config, attrib_list))
	}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap,
	const EGLint* attrib_list)
	{
	if (VgHas(config))
		VG_CREATE_SURFACE(EOrdCreatePixmapSurface, (EGLDisplay, EGLConfig, EGLNativePixmapType, const EGLint*),
		                  (d, config, pixmap, attrib_list))
	FORWARD(EOrdCreatePixmapSurface, EGLSurface, EGL_NO_SURFACE,
	        (EGLDisplay, EGLConfig, EGLNativePixmapType, const EGLint*), (d, config, pixmap, attrib_list))
	}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
	EGLConfig config, const EGLint* attrib_list)
	{
	if (VgHas(config) || buftype == EGL_OPENVG_IMAGE)
		VG_CREATE_SURFACE(EOrdCreatePbufferFromClientBuffer, (EGLDisplay, EGLenum, EGLClientBuffer, EGLConfig, const EGLint*),
		                  (d, buftype, buffer, config, attrib_list))
	FORWARD(EOrdCreatePbufferFromClientBuffer, EGLSurface, EGL_NO_SURFACE,
	        (EGLDisplay, EGLenum, EGLClientBuffer, EGLConfig, const EGLint*), (d, buftype, buffer, config, attrib_list))
	}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
	{
	if (VgHas(surface))
		{
		EGLDisplay d = SwForward(dpy);
		typedef EGLBoolean (*F)(EGLDisplay, EGLSurface);
		F f = d != EGL_NO_DISPLAY ? (F)SwFn(EOrdDestroySurface) : NULL;
		const EGLBoolean ok = f ? f(d, surface) : EGL_FALSE;
		if (ok)
			VgDel(surface);
		return ok;
		}
	FORWARD(EOrdDestroySurface, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface), (d, surface))
	}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value)
	{
	if (VgHas(surface))
		SWFWD(EOrdQuerySurface, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint, EGLint*),
		      (d, surface, attribute, value))
	FORWARD(EOrdQuerySurface, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint, EGLint*),
	        (d, surface, attribute, value))
	}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
	{
	if (VgHas(surface))
		SWFWD(EOrdSurfaceAttrib, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint, EGLint),
		      (d, surface, attribute, value))
	FORWARD(EOrdSurfaceAttrib, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint, EGLint),
	        (d, surface, attribute, value))
	}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
	{
	if (VgHas(surface))
		SWFWD(EOrdBindTexImage, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint), (d, surface, buffer))
	FORWARD(EOrdBindTexImage, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint), (d, surface, buffer))
	}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
	{
	if (VgHas(surface))
		SWFWD(EOrdReleaseTexImage, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint), (d, surface, buffer))
	FORWARD(EOrdReleaseTexImage, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLint), (d, surface, buffer))
	}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
	{
	if (VgBound())
		SWFWD(EOrdSwapInterval, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLint), (d, interval))
	FORWARD(EOrdSwapInterval, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLint), (d, interval))
	}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list)
	{
	if (VgHas(config))
		{
		EGLDisplay d = SwForward(dpy);
		typedef EGLContext (*F)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
		F f = d != EGL_NO_DISPLAY ? (F)SwFn(EOrdCreateContext) : NULL;
		EGLContext c = f ? f(d, config, share_context, attrib_list) : EGL_NO_CONTEXT;
		VgAdd(c);
		return c;
		}
	FORWARD(EOrdCreateContext, EGLContext, EGL_NO_CONTEXT, (EGLDisplay, EGLConfig, EGLContext, const EGLint*),
	        (d, config, share_context, attrib_list))
	}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
	{
	if (VgHas(ctx))
		{
		EGLDisplay d = SwForward(dpy);
		typedef EGLBoolean (*F)(EGLDisplay, EGLContext);
		F f = d != EGL_NO_DISPLAY ? (F)SwFn(EOrdDestroyContext) : NULL;
		const EGLBoolean ok = f ? f(d, ctx) : EGL_FALSE;
		if (ok)
			VgDel(ctx);
		return ok;
		}
	FORWARD(EOrdDestroyContext, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLContext), (d, ctx))
	}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
	{
	if (ctx == EGL_NO_CONTEXT && draw == EGL_NO_SURFACE && read == EGL_NO_SURFACE)
		{
		// a release: whichever API this thread has bound (the VG EGL only if it was ever brought up)
		if (VgBound() && gSwDisplay != EGL_NO_DISPLAY)
			SWFWD(EOrdMakeCurrent, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLSurface, EGLContext),
			      (d, draw, read, ctx))
		}
	else if (VgHas(ctx) || VgHas(draw))
		SWFWD(EOrdMakeCurrent, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLSurface, EGLContext),
		      (d, draw, read, ctx))
	FORWARD(EOrdMakeCurrent, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLSurface, EGLContext),
	        (d, draw, read, ctx))
	}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value)
	{
	if (VgHas(ctx))
		SWFWD(EOrdQueryContext, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLContext, EGLint, EGLint*),
		      (d, ctx, attribute, value))
	FORWARD(EOrdQueryContext, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLContext, EGLint, EGLint*),
	        (d, ctx, attribute, value))
	}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
	{
	if (VgHas(surface))
		SWFWD(EOrdSwapBuffers, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface), (d, surface))
#ifdef PHGL_TRACE
	// TRACE (Let's Golf 2 black frames, 2026-10-27): what the GL back buffer holds at every 30th swap (capped)
	static TInt swaps;
	if (++swaps <= 5)
		RDebug::Printf("PH: phgl swap %d (glReadPixels %x)", swaps, (TUint)PhglResolve(0, "glReadPixels"));
	if (swaps % 30 == 0 && swaps <= 3000)
		{
		typedef void (*TReadPixels)(int, int, int, int, unsigned, unsigned, void*);
		typedef void (*TGetIv)(unsigned, int*);
		TReadPixels readPixels = (TReadPixels)PhglResolve(0, "glReadPixels");
		TGetIv getIv = (TGetIv)PhglResolve(0, "glGetIntegerv");
		if (readPixels && getIv)
			{
			int vp[4] = {0, 0, 0, 0};
			getIv(0x0BA2, vp);
			TUint32 px[3] = {0, 0, 0};
			readPixels(vp[2] / 2, vp[3] / 2, 1, 1, 0x1908, 0x1401, &px[0]);
			readPixels(vp[2] / 4, vp[3] / 4, 1, 1, 0x1908, 0x1401, &px[1]);
			readPixels(3 * vp[2] / 4, 3 * vp[3] / 4, 1, 1, 0x1908, 0x1401, &px[2]);
			RDebug::Printf("PH: phgl swap %d viewport %dx%d back buffer %08x %08x %08x", swaps, vp[2], vp[3], px[0], px[1], px[2]);
			}
		}
#endif
	FORWARD(EOrdSwapBuffers, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface), (d, surface))
	}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
	{
	if (VgHas(surface))
		SWFWD(EOrdCopyBuffers, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLNativePixmapType),
		      (d, surface, target))
	FORWARD(EOrdCopyBuffers, EGLBoolean, EGL_FALSE, (EGLDisplay, EGLSurface, EGLNativePixmapType),
	        (d, surface, target))
	}

}
