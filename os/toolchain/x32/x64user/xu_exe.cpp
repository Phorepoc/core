// Portions Copyright (c) Nokia Corporation and/or its subsidiary(-ies)
// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// xu_exe.cpp -- Phorepoc amd64/x32: the EXE entry stub (eexe.lib). Sibling of
// euser/epoc/x86/uc_exe.cia (_E32Startup) + uc_exe.cpp (RunThread; the original is MSVC-CRT flavoured).
// Entry contract = IA-32's (kernel side: x64ekern/xe_kernel.cpp X64_EnterUser):
//   RBX = entry reason: 0 first thread of the process, 1 any other thread, 4 exception
//   RSP -> the thread create-info block the kernel copied to the top of the user stack

#include "u32std.h"
#include <u32exec.h>
#include <e32svr.h>

GLREF_C TInt E32Main();

extern "C" {
extern void (*const __init_array_start[])();		// uexe.lds
extern void (*const __init_array_end[])();

void globalDestructorFunc()
	{
	// X64_TODO: .fini_array walk; nothing registers static destructors yet
	}

void X64U_RunThread(TBool aNotFirst, SThreadCreateInfo& aInfo)
	{
	SStdEpocThreadCreateInfo& cinfo = (SStdEpocThreadCreateInfo&)aInfo;
	TInt r = UserHeap::SetupThreadHeap(aNotFirst, cinfo);
	if (r==KErrNone)
		r = UserSvr::DllSetTls(KGlobalDestructorTlsKey, KDllUid_Special, (TAny*)globalDestructorFunc);
	if (r==KErrNone)
		{
		if (aNotFirst)
			r = (*cinfo.iFunction)(cinfo.iPtr);
		else
			{
			// Init statics for implicitly linked DLLs
			User::InitProcess();
			// Init statics for EXE
			for (void (*const* f)() = __init_array_start; f < __init_array_end; ++f)
				(**f)();
			r = E32Main();
			}
		}
	User::Exit(r);
	}

// THE 16-BYTE ENTRY HEADER, exactly as the real x86 uc_exe.cia writes it: a jump over the
// header, then zeros -- "need zero word at offset 12 from entry point". An EXE's +12 is where
// the user-side LOADER (sf_ldr.cpp) stores the code-segment unique ID for RAM-loaded code, the
// same slot uc_dll.cia reserves; the DLL form differs only in also carrying UID3 there
// (xu_dll.cpp). For an XIP ROM image nothing writes it, but the layout is a platform contract,
// so an image the loader may later handle must already have the room.
//
// Compiled with -fcf-protection=none (make_uexe.sh, the efile builder): GCC otherwise opens
// every function -- naked ones included -- with a 4-byte endbr64, which would shift all of this
// along. build_uhello_x32/build_efile_x32 assert the bytes landed where intended.
//
// Entry reason 4 = a hardware exception this thread handles (uc_exe.cia's not_thread_entry): the kernel
// (ekern/xk_irq.cpp PushExcInfoOnUserStack) left a 24-word record at RSP -- type, vector, error,
// fault address, the 15 general registers, then an iretq frame (rip cs rflags rsp ss). Run
// User::HandleException(record) (a TRAP around the thread's handler; a leave unwinds from there), then
// restore every register and iretq back to the faulting instruction's continuation, as uc_exe.cia's pops + retf.
void X64U_HandleException(TAny* aRecord)
	{
	User::HandleException(aRecord);
	}
__attribute__((naked, section(".text.entry"))) TInt _E32Startup()
	{
	__asm__(".byte 0xeb, 0x0e     \n"	// +0  jmp -> +16
			".byte 0,0,0,0,0,0    \n"	// +2  pad
			".long 0              \n"	// +8
			".long 0              \n"	// +12 loader: RAM code-segment unique ID
			"cmp ebx, 2\n"				// +16
			"jae 1f\n"
			"mov edi, ebx\n"
			"mov esi, esp\n"
			"and rsp, -16\n"
			"call X64U_RunThread\n"
			"1:\n"
			"cmp ebx, 4\n"
			"jne 2f\n"
			"mov rdi, rsp\n"			// the record (x32: rsp's upper half is 0)
			"call X64U_HandleException\n"
			"add rsp, 32\n"			// type, vector, error, fault address
			"pop rax\npop rcx\npop rdx\npop rbx\npop rbp\npop rsi\npop rdi\n"
			"pop r8\npop r9\npop r10\npop r11\npop r12\npop r13\npop r14\npop r15\n"
			"iretq\n"					// rip cs rflags rsp ss: same privilege, 64-bit
			"2:\n"
			"ud2\n");
	}
}
