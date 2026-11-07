// Copyright (c) 1998-2009 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
// The Phorepoc Authors - every request rounded up to a multiple of 8 (8-byte-aligned blocks).
//
// Description:
// Open C's malloc/realloc/free (libc/src/ualloc.cpp). RHybridHeap's slab allocator serves requests up to
// 60 bytes from slabs whose cell sizes grow in 4-byte steps (RHybridHeap::SlabConfig), so a 12-byte cell
// sits at 4 mod 8 in every other slot. C code written for other C libraries relies on malloc() returning
// 8-byte-aligned blocks: GLib keeps lock bits in the low three bits of a GData pointer
// (G_DATALIST_FLAGS_MASK_INTERNAL), so a GData at 4 mod 8 is cut to the cell before it and g_datalist_*
// reads garbage. A size that is a multiple of 8 gives a slab cell size that is a multiple of 8, hence an
// 8-aligned address; larger requests come from the Doug Lea part, already 8-aligned.
//

#include <e32std.h>
#include <stdlib.h> //size_t
#include <errno.h>

static inline size_t Round8(size_t aBytes) { return (aBytes + 7) & ~(size_t)7; }

// We use the EPOC32 memory allocation which is already
// thread-safe, so we don't need the _malloc_r variants

extern "C" {

EXPORT_C void* malloc (size_t nbytes)
	{
	if (nbytes >= (KMaxTInt/2))
		{
		errno = ENOMEM;
		return NULL;
		}

	TAny *Tptr = User::Alloc(Round8(nbytes));
	if(Tptr == NULL)
		errno = ENOMEM;

	return Tptr;
	}

EXPORT_C void* realloc (void *p, size_t nbytes)
	{
	if (nbytes >= (KMaxTInt/2))
		{
		errno = ENOMEM;
		return NULL;
		}

	TAny *Tptr = User::ReAlloc(p, Round8(nbytes));
	if(Tptr == NULL)
		errno = ENOMEM;

	return Tptr;
	}

EXPORT_C void free (void *p)
	{
	User::Free(p);
	}

} // extern "C"
