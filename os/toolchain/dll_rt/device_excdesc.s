/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

@ device_excdesc.s -- OURS: the TExceptionDescriptor of a `device` image (e32def_private.h), the GCC twin of
@ uc_exe.cia's Symbian$$CPP$$Exception$$Descriptor. build_mmp.py writes (this - code base) | 1 into the header's
@ iExceptionDescriptor; the kernel adds the code's run address, the phone's unwinder reads these four words (the
@ loader relocates them) to find .ARM.exidx for a return address inside this image.
	.section .rodata.symbian_exception_descriptor,"a"
	.align 2
	.globl Symbian_CPP_Exception_Descriptor
Symbian_CPP_Exception_Descriptor:
	.word __exidx_start
	.word __exidx_end
	.word __symbian_ro_base + 1	@ + 1: EHABI v2 tables (PREL31 offsets), as ARMCC > 2.2 marks them
	.word __exidx_end
