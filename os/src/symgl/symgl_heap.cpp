// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// symgl_heap.cpp -- OURS (2026-10-29): one process-wide heap for everything the GL implementation allocates. phgl.dll
// is linked with ld --wrap for the C allocators, the string functions that allocate and C++'s global new/delete
// (registry `link_wrap`), so Mesa's malloc/free/new/delete land here, whatever the calling thread.
//
// Why: P.I.P.S.' malloc is User::Alloc -- the CALLING THREAD's heap -- and so is the runtime's operator new. Threads
// of one process may have heaps of their own: in the window server the Alfred server thread runs on its own heap (its
// 2010 source: RThread::Create(..., 4 MB, KAlfServerMaxHeapSize, ...)), with the OpenVG renderer's GL worker on it,
// while the GPU composer runs on the window server's heap. Mesa's per-process objects (the display's screen, its
// buffer and shader caches) were allocated on one heap and freed or grown on the other: heap corruption, KERN-EXEC 3
// in RHybridHeap::DlMalloc under lima_get_compiled_fs (2026-10-29, GPU mode).
//
// The heap is the FIRST GL caller's own heap, shared the way EKA2 shares a heap between threads (RThread::Create with
// an allocator: RAllocator::Open, then use it from any thread): a thread heap is created thread-safe (CreateThreadHeap,
// aSingleThread = EFalse) and its chunk and lock handles are made process-relative, so with the access count held
// open here it outlives the thread that made it. It costs no address space: a heap of its own -- a fixed 256 MB
// maximum, its disconnected chunk reserving 512 MB of the process -- left Let's Golf 2 5 MB below KUserLocalDataEnd,
// too little to map its window surface (KErrNoMemory, then a panic; 2026-10-29). A chunk heap of its own remains the
// fallback for a first caller whose allocator is not a heap in a chunk.
//
// free/delete/realloc of a pointer outside the shared heap's chunk go to the original allocator: CBase objects come
// from User::AllocZ inline in e32base.inl and may be deleted through the global delete here.
#include <e32std.h>
#include <e32atomics.h>
#include <e32debug.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace
	{
	const TInt KFallbackMax = 64 << 20;		// the fallback chunk heap's maximum (its chunk reserves twice that)

	RAllocator* gHeap;
	TLinAddr gBase, gEnd;		// the shared heap's chunk: its reserved range

	// the chunk handle of a THREAD-SAFE chunk heap (RAllocator keeps it protected): such a heap holds two process-
	// relative handles, the chunk's then the lock's (heap_hybrid.cpp: iHandles = &iChunkHandle, iHandleCount = 2); a
	// single-threaded heap holds one, and a fixed heap in no chunk holds only its lock
	class TPeek : public RAllocator
		{
	public:
		static TInt ChunkHandle(const RAllocator& aA)
			{
			const TPeek& p = static_cast<const TPeek&>(aA);
			return p.iHandleCount == 2 && p.iHandles ? p.iHandles[0] : 0;
			}
		};

	// share the calling thread's heap if it is a heap in a chunk that holds it; sets gBase/gEnd
	RAllocator* ShareCallersHeap()
		{
		RAllocator& a = User::Allocator();
		const TInt handle = TPeek::ChunkHandle(a);
		if (!handle)
			return NULL;
		RChunk c;
		c.SetHandle(handle);
		const TLinAddr base = (TLinAddr)c.Base(), end = base + c.MaxSize();
		if ((TLinAddr)&a < base || (TLinAddr)&a >= end || a.Open() != KErrNone)
			return NULL;
		gBase = base;
		gEnd = end;
		RDebug::Printf("PH: phgl heap: the first GL thread's heap, shared (chunk %08x-%08x)", base, end);
		return &a;
		}

	RAllocator* OwnChunkHeap()
		{
		RChunk chunk;
		if (chunk.CreateDisconnectedLocal(0, 0, 2 * KFallbackMax, EOwnerProcess) != KErrNone)
			return NULL;
		RHeap* h = UserHeap::ChunkHeap(chunk, 0x10000, 0x10000, 0, 0, EFalse, UserHeap::EChunkHeapDuplicate);
		if (h)
			{
			gBase = (TLinAddr)chunk.Base();
			gEnd = gBase + chunk.MaxSize();
			RDebug::Printf("PH: phgl heap: %d MB chunk heap of its own at %08x", KFallbackMax >> 20, gBase);
			}
		chunk.Close();
		return h;
		}

	// (euser's atomics: GCC's __atomic builtins are library calls for this target, which the ROM does not provide)
	RAllocator* Heap()
		{
		RAllocator* h = (RAllocator*)__e32_atomic_load_acq_ptr(&gHeap);
		if (h)
			return h;
		static TUint32 lock;		// creation by the first caller (another thread waits for it)
		while (__e32_atomic_swp_ord32(&lock, 1))
			User::AfterHighRes(100);
		h = gHeap;
		if (!h)
			h = ShareCallersHeap();
		if (!h)
			h = OwnChunkHeap();
		if (!h)
			{
			RDebug::Printf("PH: phgl heap: none shared, the calling thread's heap is used");
			h = &User::Allocator();
			}
		__e32_atomic_store_rel_ptr(&gHeap, h);		// (after gBase/gEnd)
		__e32_atomic_store_rel32(&lock, 0);
		return h;
		}

	// a cell of the shared heap (a thread that never allocated may free one: the acquire load orders gBase/gEnd)
	inline TBool Ours(const TAny* aPtr)
		{
		return aPtr && __e32_atomic_load_acq_ptr(&gHeap) && (TLinAddr)aPtr >= gBase && (TLinAddr)aPtr < gEnd;
		}

	TAny* Alloc(size_t aSize)
		{
		if (aSize >= (size_t)(KMaxTInt / 2))
			{
			errno = ENOMEM;
			return NULL;
			}
		TAny* p = Heap()->Alloc(aSize);
		if (!p)
			errno = ENOMEM;
		return p;
		}
	}

extern "C" {

void* __real_realloc(void* aPtr, size_t aSize);
void __real_free(void* aPtr);
void __real__ZdlPv(void* aPtr);
void __real__ZdaPv(void* aPtr);

void* __wrap_malloc(size_t aSize)
	{
	return Alloc(aSize);
	}

void* __wrap_calloc(size_t aCount, size_t aSize)
	{
	if (aSize && aCount > (size_t)(KMaxTInt / 2) / aSize)
		{
		errno = ENOMEM;
		return NULL;
		}
	TAny* p = Alloc(aCount * aSize);
	if (p)
		Mem::FillZ(p, aCount * aSize);
	return p;
	}

void __wrap_free(void* aPtr)
	{
	if (Ours(aPtr))
		gHeap->Free(aPtr);
	else if (aPtr)
		__real_free(aPtr);
	}

void* __wrap_realloc(void* aPtr, size_t aSize)
	{
	if (aPtr && !Ours(aPtr))
		return __real_realloc(aPtr, aSize);
	if (!aPtr)
		return Alloc(aSize);
	if (!aSize)
		{
		gHeap->Free(aPtr);
		return NULL;
		}
	if (aSize >= (size_t)(KMaxTInt / 2))
		{
		errno = ENOMEM;
		return NULL;
		}
	TAny* p = gHeap->ReAlloc(aPtr, aSize);
	if (!p)
		errno = ENOMEM;
	return p;
	}

char* __wrap_strdup(const char* aStr)
	{
	const size_t n = strlen(aStr) + 1;
	char* p = (char*)Alloc(n);
	if (p)
		memcpy(p, aStr, n);
	return p;
	}

char* __wrap_strndup(const char* aStr, size_t aMax)
	{
	size_t n = 0;
	while (n < aMax && aStr[n])
		n++;
	char* p = (char*)Alloc(n + 1);
	if (p)
		{
		memcpy(p, aStr, n);
		p[n] = 0;
		}
	return p;
	}

int __wrap_vasprintf(char** aOut, const char* aFmt, va_list aArgs)
	{
	va_list copy;
	va_copy(copy, aArgs);
	const int n = vsnprintf(NULL, 0, aFmt, copy);
	va_end(copy);
	*aOut = NULL;
	if (n < 0)
		return -1;
	char* p = (char*)Alloc(n + 1);
	if (!p)
		return -1;
	vsnprintf(p, n + 1, aFmt, aArgs);
	*aOut = p;
	return n;
	}

int __wrap_asprintf(char** aOut, const char* aFmt, ...)
	{
	va_list args;
	va_start(args, aFmt);
	const int n = __wrap_vasprintf(aOut, aFmt, args);
	va_end(args);
	return n;
	}

// C++: operator new / new[] / delete / delete[] / sized delete (the runtime's are User::Alloc / User::Free)
void* __wrap__Znwj(unsigned aSize)
	{
	return Alloc(aSize);
	}

void* __wrap__Znaj(unsigned aSize)
	{
	return Alloc(aSize);
	}

void __wrap__ZdlPv(void* aPtr)
	{
	if (Ours(aPtr))
		gHeap->Free(aPtr);
	else if (aPtr)
		__real__ZdlPv(aPtr);
	}

void __wrap__ZdaPv(void* aPtr)
	{
	if (Ours(aPtr))
		gHeap->Free(aPtr);
	else if (aPtr)
		__real__ZdaPv(aPtr);
	}

void __wrap__ZdlPvj(void* aPtr, unsigned)
	{
	__wrap__ZdlPv(aPtr);
	}

}
