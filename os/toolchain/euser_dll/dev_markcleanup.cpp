// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// dev_markcleanup.cpp -- OURS: the leave-by-throw bridge of the setjmp-TRAP device euser.
//
// Real-device SDK code (RVCT/GCCE builds, exceptions on: the real BrowserNG, Qt apps, our `device` mmp images) expands
//     TRAP(r, s)  ==>  { TTrapHandler* t = User::MarkCleanupStack(); try { s; User::UnMarkCleanupStack(t); }
//                        catch (XLeaveException& l) { r = l.GetReason(); } }
// and leaves with User::Leave == `throw XLeaveException`. Our euser is built TRAP=setjmp (User::Leave longjmps to the
// innermost TTrap), so before 2026-10-16 a leave inside such a TRAP (a) was caught by no `catch` and (b) skipped the cleanup
// level opened by MarkCleanupStack -> the next PopAndDestroy panicked E32USER-CBase 63 (EClnPopAcrossLevels; BrowserNG on the
// phone, boot 564). The bridge, both models in one euser:
//   * MarkCleanupStack pushes a MARKER onto the kernel's trap-frame list (a heap TTrap tagged iResult==KMarkTag, carrying the
//     caller's r4-r11/sp/lr as at the call) and opens the cleanup level like the real one.
//   * User::Leave (--wrap, see build_euser_device.py) pops the innermost frame: a setjmp TTrap -> the real longjmp Leave;
//     a marker -> the throw path below.
//   * the throw path cleans the cleanup stack to the level (pH->Leave), then RESUMES AS IF drtaeabi's __cxa_throw had been
//     called from the Mark call site: restore the marker's saved context and tail-jump (bx, lr = return address into the
//     TRAP's function) to __cxa_throw(XLeaveException). The unwinder then walks the device code's own tables to its catch --
//     no unwind tables are needed in euser. __cxa_* / the XLeaveException vtable+typeinfo are looked up in drtaeabi.dll
//     (RLibrary, once per thread; cached in the thread's Dll TLS) -- euser cannot import drtaeabi (it imports euser).
//   * XLeaveException::GetReason returns iR (the euser build has no __SUPPORT_CPP_EXCEPTIONS__ -> us_trp.cpp's returns 0).
// Our own setjmp-TRAP code never calls Mark/UnMark. Frames between a throwing Leave and its catch must have unwind tables
// (device-model code does; a setjmp-model/-fno-exceptions callee between them would terminate -- not seen yet).
#include <e32std.h>
#include <e32debug.h>
#include <u32exec.h>
#include <e32svr.h>
#include <u32std.h>

namespace
{
const TInt KMarkTag = 0x4B52414D;                 // "MARK" in TTrap::iResult (never a valid TInt* of a setjmp frame)
const TInt KTlsHandle = 0x50484C56;               // UserSvr::DllTls key (handle, uid) of the cached drtaeabi entry points
const TInt KOrdAllocEx = 177, KOrdThrow = 191, KOrdTypeInfo = 206, KOrdVtable = 207;   // drtaeabiu.def
const TInt KOrdCtorGlobals = 204, KOrdGetGlobals = 216, KOrdInit2 = 226;

struct SXLeave { const void* iVptr; TInt iR; };   // XLeaveException: RVCT 2.2+ shape (virtual key function), 8 bytes

struct SThrowFns
	{
	void* (*iAlloc)(TUint);
	void* iThrow;
	const void* iTypeInfo;
	const void* iVtable;
	void* (*iGetGlobals)();
	void (*iCtorGlobals)(void*);
	void (*iInit2)(void*);
	TInt iLogged;
	};

SThrowFns* ThrowFns()
	{
	SThrowFns* f = (SThrowFns*)UserSvr::DllTls(KTlsHandle, KDllUid_Special);
	if (f)
		return f;
	RLibrary lib;
	TInt r = lib.Load(_L("drtaeabi.dll"));
	_LIT(KP, "PH-LEAVE");
	if (r != KErrNone)
		User::Panic(KP, 1);
	f = (SThrowFns*)User::Alloc(sizeof(SThrowFns));
	if (!f)
		User::Panic(KP, 2);
	f->iAlloc = (void* (*)(TUint))lib.Lookup(KOrdAllocEx);
	f->iThrow = (void*)lib.Lookup(KOrdThrow);
	f->iTypeInfo = (const void*)lib.Lookup(KOrdTypeInfo);
	f->iVtable = (const void*)lib.Lookup(KOrdVtable);
	f->iGetGlobals = (void* (*)())lib.Lookup(KOrdGetGlobals);
	f->iCtorGlobals = (void (*)(void*))lib.Lookup(KOrdCtorGlobals);
	f->iInit2 = (void (*)(void*))lib.Lookup(KOrdInit2);
	f->iLogged = 0;
	// NOT lib.Close(): phebrctl links libsupc++ statically and does not import drtaeabi, so closing the only handle destroys
	// the code seg ("[cs] DESTRUCT drtaeabi.dll" right before LEAVE-THROW, boots 640/645). The unwinder then asks the kernel
	// (UserSvr::ExceptionDescriptor -> Kern::CodeSegFromAddress(addr, process)) for the descriptor of an address in drtaeabi,
	// gets 0 and InitialiseSymbianSpecificUnwinderCache abort()s -> USER-EXEC 3. The handle is process-owned: keep it open.
	if (!f->iAlloc || !f->iThrow || !f->iTypeInfo || !f->iVtable || !f->iGetGlobals || !f->iCtorGlobals || !f->iInit2)
		User::Panic(KP, 3);
	UserSvr::DllSetTls(KTlsHandle, KDllUid_Special, f);
	return f;
	}
}

// Called from the asm entry below with r0 = the 10 saved words {r4-r11, sp, lr}.
extern "C" TTrapHandler* PhMarkImpl(const TInt* aCtx)
	{
	TTrap* m = (TTrap*)User::Alloc(sizeof(TTrap));
	if (!m)
		User::Panic(_L("PH-LEAVE"), 4);
	for (TInt i = 0; i < 10; ++i)
		m->iState[i] = aCtx[i];
	m->iResult = (TInt*)KMarkTag;
	TTrapHandler* h = Exec::PushTrapFrame(m);     // sets m->iHandler / iNext
	if (h)
		h->Trap();                                 // opens the cleanup level
	return h;
	}

asm(
	".text\n"
	".arm\n"
	".global _ZN4User16MarkCleanupStackEv\n"
	".type _ZN4User16MarkCleanupStackEv, %function\n"
	"_ZN4User16MarkCleanupStackEv:\n"
	"	mov	r12, sp\n"
	"	stmfd	sp!, {r4-r12, lr}\n"          // ascending: r4..r11, caller sp, lr
	"	sub	sp, sp, #8\n"                     // keep 8-byte alignment (48 bytes)
	"	add	r0, sp, #8\n"
	"	bl	PhMarkImpl\n"
	"	add	sp, sp, #8\n"
	"	ldmfd	sp!, {r4-r12, lr}\n"
	"	bx	lr\n"
	".size _ZN4User16MarkCleanupStackEv, .-_ZN4User16MarkCleanupStackEv\n"
);

EXPORT_C void User::UnMarkCleanupStack(TTrapHandler* aHandler)
	{
	TTrap* t = Exec::PopTrapFrame();
	if (t)
		{
		if (t->iResult == (TInt*)KMarkTag)
			User::Free(t);
		else
			Exec::PushTrapFrame(t);                // not ours (unbalanced): leave the frame list untouched
		}
	if (aHandler)
		aHandler->UnTrap();
	}

// The throw path: never returns.
extern "C" void PhLeaveThrow(TTrap* aMarker, TInt aReason) __attribute__((noreturn));
extern "C" void PhLeaveThrow(TTrap* aMarker, TInt aReason)
	{
	TInt ctx[10];
	for (TInt i = 0; i < 10; ++i)
		ctx[i] = aMarker->iState[i];
	TTrapHandler* h = aMarker->iHandler;
	User::Free(aMarker);
	if (h)
		h->Leave(aReason);                         // cleanup stack back to the level Mark opened (and drops the level)
	SThrowFns* f = ThrowFns();
	if (f->iLogged < 20)
		{
		++f->iLogged;
		RDebug::Printf("PH: LEAVE-THROW %d -> catch frame lr=%08x", aReason, ctx[9]);
		}
	if (!f->iGetGlobals())
		{
		// this thread's EXE was built in the setjmp model (BrowserNG: no TCppRTExceptionsGlobals in its startup) but a
		// leave-by-throw DLL (phebrctl) TRAPs on it: give the thread the phone runtime's exception state now (the ctor
		// stores `this` in drtaeabi's Dll::Tls, where __cxa_get_globals reads it; the object lives as long as the thread).
		void* g = User::Alloc(1024);
		if (!g)
			User::Panic(_L("PH-LEAVE"), 5);
		f->iCtorGlobals(g);
		f->iInit2(g);
		}
	SXLeave* ex = (SXLeave*)f->iAlloc(sizeof(SXLeave));
	ex->iVptr = (const TUint8*)f->iVtable + 8;     // Itanium/EABI vtable: offset-to-top, typeinfo, then the slots
	ex->iR = aReason;
	// ctx[9] (the Mark call's return address) is used as is: the call-site table of the TRAP's function covers the Mark call
	// itself (CPhBrCtl::NetworkConnection: range 0x2d6ec..0x2d720 starts at the bl), so (lr-1) / (lr-2) lie inside it.
	register TInt* r12 asm("r12") = ctx;
	register void* r0 asm("r0") = ex;
	register const void* r1 asm("r1") = f->iTypeInfo;
	register TInt r2 asm("r2") = 0;                // no destructor
	register void* r3 asm("r3") = f->iThrow;
	asm volatile(
		"ldmia	r12, {r4-r11, sp, lr}\n"
		"bx	r3\n"
		: : "r"(r12), "r"(r0), "r"(r1), "r"(r2), "r"(r3) : "memory");
	__builtin_unreachable();
	}

extern "C" void __real__ZN4User5LeaveEi(TInt aReason);

extern "C" void __wrap__ZN4User5LeaveEi(TInt aReason)
	{
	TTrap* t = Exec::PopTrapFrame();
#ifdef PH_LEAVE_TRACE
	// bring-up trace (build with PH_LEAVE_TRACE): the first 300 leaves of each thread (count kept as the TLS value
	// itself). Off by default: printed synchronously, it stalled threads that leave as they work -- an MP3 controller
	// thread decoding while its app waited on it (the app then missed a view switch: ViewSrv 14).
	TInt n = (TInt)UserSvr::DllTls(KTlsHandle + 1, KDllUid_Special);
	if (n < 300)
		{
		UserSvr::DllSetTls(KTlsHandle + 1, KDllUid_Special, (TAny*)(n + 1));
		// + the first ROM code addresses on the stack: lr is often euser's own LeaveIfError, the caller is above it
		TUint ra[4] = {0, 0, 0, 0};
		const TUint* sp;
		asm volatile("mov %0, sp" : "=r"(sp));
		TThreadStackInfo si;
		const TInt words = RThread().StackInfo(si) == KErrNone ? Min(48, TInt((si.iBase - TLinAddr(sp)) / 4)) : 0;
		for (TInt i = 0, k = 0; i < words && k < 4; ++i)
			if (sp[i] >= 0x80000000u && sp[i] < 0x90000000u)
				ra[k++] = sp[i];
		RDebug::Printf("PH: LEAVE %d lr=%08x top=%s st=%08x %08x %08x %08x", aReason, (TUint)__builtin_return_address(0),
			!t ? "NONE" : (t->iResult == (TInt*)KMarkTag ? "marker" : "setjmp"), ra[0], ra[1], ra[2], ra[3]);
		}
#endif
	if (t)
		{
		if (t->iResult == (TInt*)KMarkTag)
			PhLeaveThrow(t, aReason);
		Exec::PushTrapFrame(t);                    // a setjmp frame: the real Leave pops it again
		}
	__real__ZN4User5LeaveEi(aReason);
	}

// XLeaveException::GetReason (extern "C": the exact mangled name; the class has no member data we need to declare).
extern "C" TInt _ZNK15XLeaveException9GetReasonEv(const SXLeave* aThis)
	{
	return aThis->iR;
	}

// bring-up: who calls User::Panic? category, reason, caller lr + the ROM-code-looking words near the top of the stack.
extern "C" void __real__ZN4User5PanicERK7TDesC16i(const TDesC16& aCat, TInt aReason);
extern "C" void __wrap__ZN4User5PanicERK7TDesC16i(const TDesC16& aCat, TInt aReason)
	{
	TUint lr = (TUint)__builtin_return_address(0);
	TUint* sp = (TUint*)&lr;
	TUint bt[12];
	TInt n = 0;
	for (TInt i = 1; i < 120 && n < 12; i++)
		if (sp[i] >= 0x80000000u && sp[i] < 0x8C000000u && !(sp[i] & 2))
			bt[n++] = sp[i];
	for (; n < 12; n++)
		bt[n] = 0;
	TBuf8<40> cat8;
	cat8.Copy(aCat.Left(32));
	cat8.Append(TChar(0));
	RDebug::Printf("PH: PANIC %s %d lr=%08x", cat8.Ptr(), aReason, lr);
	RDebug::Printf("PH: bt %08x %08x %08x %08x %08x %08x", bt[0], bt[1], bt[2], bt[3], bt[4], bt[5]);
	RDebug::Printf("PH: bt %08x %08x %08x %08x %08x %08x", bt[6], bt[7], bt[8], bt[9], bt[10], bt[11]);
	__real__ZN4User5PanicERK7TDesC16i(aCat, aReason);
	}

extern "C" void __real__ZN4User9InvariantEv();
extern "C" void __wrap__ZN4User9InvariantEv()
	{
	TUint lr = (TUint)__builtin_return_address(0);
	TUint* sp = (TUint*)&lr;
	TUint bt[12];
	TInt n = 0;
	for (TInt i = 1; i < 120 && n < 12; i++)
		if (sp[i] >= 0x80000000u && sp[i] < 0x8C000000u && !(sp[i] & 2))
			bt[n++] = sp[i];
	for (; n < 12; n++)
		bt[n] = 0;
	RDebug::Printf("PH: INVARIANT lr=%08x", lr);
	RDebug::Printf("PH: bt %08x %08x %08x %08x %08x %08x", bt[0], bt[1], bt[2], bt[3], bt[4], bt[5]);
	RDebug::Printf("PH: bt %08x %08x %08x %08x %08x %08x", bt[6], bt[7], bt[8], bt[9], bt[10], bt[11]);
	__real__ZN4User9InvariantEv();
	}
