// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// symgl_tls.cpp -- OURS (2026-10-29): the per-thread storage behind Mesa's thread-locals (symgl_native.h,
// symgl_thread_slot). The ARM toolchain's ELF TLS (__aeabi_read_tp + a TLS block per thread) has no runtime on
// Symbian, so mesa_symbian.h compiles `thread_local` away and routes the variables whose per-thread value matters to
// these slots. They were per-process globals before: one GL thread's eglMakeCurrent replaced the current context of
// every other GL thread in the process (the OpenVG renderer's GL threads and WSERV's GPU composer share the window
// server process) -- the composer's GL calls then ran in the renderer's context and failed.
//
// One Dll::Tls block per thread: SYMGL_SLOTS pointers, each slot allocated (zeroed) on its first use from the calling
// thread's heap. symgl_thread_release frees the block (eglReleaseThread).
#include <e32std.h>

#include "symgl_native.h"

namespace
	{
	struct TThreadSlots
		{
		TAny* iSlot[SYMGL_SLOTS];
		};

	TThreadSlots* Slots(TBool aCreate)
		{
		TThreadSlots* s = static_cast<TThreadSlots*>(Dll::Tls());
		if (!s && aCreate)
			{
			s = static_cast<TThreadSlots*>(User::AllocZ(sizeof(TThreadSlots)));
			if (s && Dll::SetTls(s) != KErrNone)
				{
				User::Free(s);
				s = NULL;
				}
			}
		return s;
		}
	}

extern "C" void* symgl_thread_slot(int aId, unsigned aSize)
	{
	if (aId < 0 || aId >= SYMGL_SLOTS)
		return NULL;
	TThreadSlots* s = Slots(ETrue);
	if (!s)
		return NULL;
	TAny*& slot = s->iSlot[aId];
	if (!slot)
		slot = User::AllocZ(aSize);
	return slot;
	}

extern "C" void symgl_thread_release(void)
	{
	TThreadSlots* s = Slots(EFalse);
	if (!s)
		return;
	for (TInt i = 0; i < SYMGL_SLOTS; i++)
		User::Free(s->iSlot[i]);
	User::Free(s);
	Dll::FreeTls();
	}
