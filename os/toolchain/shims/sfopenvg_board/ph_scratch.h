// Copyright The Phorepoc Authors
// SPDX-License-Identifier: BSD-2-Clause
//
// ph_scratch.h -- one cached scratch block per translation unit for the OpenVG upload staging buffers (w*h*4).
// Those buffers are usually >= 64 KB, which the hybrid heap serves from its paged allocator: every User::Alloc committed
// pages (Exec::ChunkAdjust via RHybridHeap::Map) and every User::Free decommitted them again, several times per frame in
// alfredserver. PhScratchAlloc hands out the cached block when it is large enough and free; PhScratchFree keeps the
// block (the larger of the two) instead of freeing it. The slot is taken and returned with atomic swaps, so a second
// thread just falls back to the heap.
#ifndef PH_SCRATCH_H
#define PH_SCRATCH_H
#include <e32std.h>
#include <e32atomics.h>

namespace PhScratch
	{
	static TAny* volatile gBlock = NULL;	// the cached block, or NULL while in use / none
	static const TInt KMaxKeep = 8 * 1024 * 1024;	// never keep more than this (one full-screen layer is ~0.9 MB)
	static RAllocator* volatile gHeap = NULL;	// the heap the cached block belongs to: other heaps bypass the cache
	}

static inline TAny* PhScratchAlloc(TInt aBytes)
	{
	if (&User::Allocator() != PhScratch::gHeap)
		return User::Alloc(aBytes);
	TAny* b = __e32_atomic_swp_acq_ptr(&PhScratch::gBlock, NULL);
	if (b)
		{
		if (User::AllocLen(b) >= aBytes)
			return b;
		User::Free(b);			// too small: drop it, the larger block replaces it on PhScratchFree
		}
	return User::Alloc(aBytes);
	}

static inline void PhScratchFree(TAny* aPtr)
	{
	if (!aPtr)
		return;
	RAllocator* heap = &User::Allocator();
	if (!PhScratch::gHeap)
		PhScratch::gHeap = heap;		// the first heap to free a block owns the cache (alfredserver's)
	if (heap != PhScratch::gHeap || User::AllocLen(aPtr) > PhScratch::KMaxKeep)
		{
		User::Free(aPtr);
		return;
		}
	TAny* old = __e32_atomic_swp_rel_ptr(&PhScratch::gBlock, aPtr);
	if (old)
		{
		if (User::AllocLen(old) > User::AllocLen(aPtr))	// keep the larger one
			{
			TAny* mine = __e32_atomic_swp_rel_ptr(&PhScratch::gBlock, old);
			if (mine)
				User::Free(mine);
			}
		else
			User::Free(old);
		}
	}

#endif // PH_SCRATCH_H
