// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// phgmem.h -- OURS: system-wide graphics memory accounting, the data behind EGL_NOK_resource_profiling (libegl_front.cpp)
// and so behind the real GOOM monitor. Nokia's GPU driver answered from its own pool; here graphics memory is ordinary RAM
// held by whichever process renders (the OpenVG RI allocates its images and surfaces from the caller's heap: for Alf
// that is the window server's). Every renderer instance lives in its own process, while GOOM asks from AknCapServer --
// so the counters sit in one global chunk, one slot per process:
//   bytes     graphics memory this process holds now (RI image/surface pixels: riImage.cpp)
//   peak      the most it ever held
//   allocs    allocations made (with bytes falling back, a growing count is churn, not a leak)
//   headroom  how much more the heap those pixels come from can still give (max length - allocated)
// A slot whose process has died is taken back (its memory went with it).
#ifndef PHGMEM_H
#define PHGMEM_H
#include <e32std.h>
#include <e32atomics.h>
#include <e32hal.h>
#include <e32svr.h>

namespace PhGMem
	{
	const TInt KSlots = 64;
	const TUint32 KMagic = 0x4d454d47;		// "GMEM"
	struct TSlot
		{
		volatile TUint32 iPid;
		volatile TInt32 iBytes;
		volatile TInt32 iPeak;
		volatile TUint32 iAllocs;
		volatile TInt32 iHeadroom;
		};
	struct TTable
		{
		volatile TUint32 iMagic;
		TSlot iSlots[KSlots];
		};

	inline TBool Alive(TUint32 aPid)
		{
		RProcess p;
		if (p.Open(TProcessId(aPid)) != KErrNone)
			return EFalse;
		const TBool alive = p.ExitType() == EExitPending;
		p.Close();
		return alive;
		}

	// the table, mapped into this process (NULL if the chunk cannot be had)
	inline TTable* Table()
		{
		static TTable* table;
		static RChunk chunk;
		if (table)
			return table;
		_LIT(KName, "PhGfxMem");
		TInt r = chunk.CreateGlobal(KName, sizeof(TTable), sizeof(TTable));
		if (r == KErrAlreadyExists)
			r = chunk.OpenGlobal(KName, EFalse);
		if (r != KErrNone)
			return NULL;
		TTable* t = (TTable*)chunk.Base();
		TUint32 zero = 0;
		__e32_atomic_cas_ord32(&t->iMagic, &zero, KMagic);	// a new chunk is zero-filled
		table = t;
		return table;
		}

	// this process's slot
	inline TSlot* Mine()
		{
		static TSlot* mine;
		if (mine)
			return mine;
		TTable* t = Table();
		if (!t)
			return NULL;
		const TUint32 me = (TUint32)RProcess().Id().Id();
		for (TInt i = 0; i < KSlots; ++i)
			if (t->iSlots[i].iPid == me)
				return mine = &t->iSlots[i];
		for (TInt pass = 0; pass < 2; ++pass)
			for (TInt i = 0; i < KSlots; ++i)
				{
				TSlot& s = t->iSlots[i];
				TUint32 old = s.iPid;
				if (old && (pass == 0 || Alive(old)))
					continue;
				if (__e32_atomic_cas_ord32(&s.iPid, &old, me))
					{
					s.iBytes = s.iPeak = s.iHeadroom = 0;
					s.iAllocs = 0;
					return mine = &s;
					}
				}
		return NULL;
		}

	// The pixel pool: this process's graphics memory, apart from its ordinary heap -- what the GPU driver's own memory was
	// on Nokia's hardware. Without it the RI's pixels landed in the caller's heap, and for Alf that is the alfredserver
	// thread's, which Nokia capped at 16 MB (alfdecoderserverclient.cpp) for bookkeeping only. A chunk heap reserved at the
	// size of physical RAM (halved until the address space gives it), committed as used.
	inline RHeap* Pool()
		{
		static RHeap* volatile pool;
		if (pool)
			return pool;
		TMemoryInfoV1Buf info;
		UserHal::MemoryInfo(info);
		RHeap* h = NULL;
		for (TInt max = info().iTotalRamInBytes; !h && max >= 0x1000000; max /= 2)
			h = UserHeap::ChunkHeap(NULL, 0x10000, max, 0x100000);
		if (!h)
			return NULL;
		TAny* none = NULL;
		if (!__e32_atomic_cas_ord_ptr(&pool, &none, h))
			h->Close();		// another thread made it first
		return pool;
		}

	inline TBool InPool(RHeap* aPool, const TAny* aPtr)
		{
		const TUint8* b = aPool->Base();
		return (const TUint8*)aPtr >= b && (const TUint8*)aPtr < b + aPool->MaxLength();
		}

	// aBytes > 0: allocated; < 0: freed
	inline void Add(TInt aBytes)
		{
		TSlot* s = Mine();
		if (!s)
			return;
		const TInt now = (TInt)__e32_atomic_add_ord32(&s->iBytes, (TUint32)aBytes) + aBytes;
		if (aBytes > 0)
			{
			__e32_atomic_add_ord32(&s->iAllocs, 1);
			if (now > s->iPeak)
				s->iPeak = now;
			}
		RHeap* h = Pool();
		if (!h)
			h = &User::Heap();
		TInt alloc = 0;
		h->AllocSize(alloc);
		s->iHeadroom = h->MaxLength() - alloc;
		}

	// pixel storage for the RI (riImage.cpp): from the pool (the process heap if there is none), counted
	inline TAny* AllocPixels(TInt aBytes)
		{
		RHeap* h = Pool();
		TAny* p = h ? h->Alloc(aBytes) : User::Alloc(aBytes);
		if (p)
			Add(aBytes);
		return p;
		}

	inline void FreePixels(TAny* aPtr, TInt aBytes)
		{
		if (!aPtr)
			return;
		RHeap* h = Pool();
		if (h && InPool(h, aPtr))
			h->Free(aPtr);
		else
			User::Free(aPtr);
		Add(-aBytes);
		}

	struct TSnapshot
		{
		TInt iUsed;			// all processes
		TInt iFree;			// min(system free RAM, headroom of the heap holding the most graphics memory)
		TInt iSysFree;
		TInt iCount;
		TSlot iSlots[KSlots];	// live processes holding graphics memory (pid, bytes, peak, allocs, headroom)
		};

	inline void Snapshot(TSnapshot& aOut)
		{
		aOut.iUsed = 0;
		aOut.iCount = 0;
		TMemoryInfoV1Buf info;
		UserHal::MemoryInfo(info);
		aOut.iSysFree = info().iFreeRamInBytes;
		aOut.iFree = aOut.iSysFree;
		TTable* t = Table();
		if (!t)
			return;
		TInt biggest = -1;
		for (TInt i = 0; i < KSlots; ++i)
			{
			TSlot& s = t->iSlots[i];
			TUint32 pid = s.iPid;
			if (!pid)
				continue;
			if (!Alive(pid))
				{
				__e32_atomic_cas_ord32(&s.iPid, &pid, 0);	// its memory died with it
				continue;
				}
			if (s.iBytes <= 0 && s.iAllocs == 0)
				continue;
			TSlot& o = aOut.iSlots[aOut.iCount++];
			o.iPid = pid;
			o.iBytes = s.iBytes;
			o.iPeak = s.iPeak;
			o.iAllocs = s.iAllocs;
			o.iHeadroom = s.iHeadroom;
			aOut.iUsed += o.iBytes;
			if (biggest < 0 || o.iBytes > aOut.iSlots[biggest].iBytes)
				biggest = aOut.iCount - 1;
			}
		if (biggest >= 0 && aOut.iSlots[biggest].iHeadroom < aOut.iFree)
			aOut.iFree = aOut.iSlots[biggest].iHeadroom;
		}
	}
#endif
