// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// ph_vfpvec.cpp -- OURS: mark the VFP short-vector blocks of a RAM-loaded ARM image for the kernel (2026-10-27).
// ARM11 (VFPv2) runs VFP data-processing instructions over FPSCR.LEN+1 registers; RVCT-built games enter that mode with
// `bic/orr Rt, Rt, #LEN<<16 ; vmsr fpscr, Rt` around their matrix maths. ARMv8's AArch32 has no short vectors: the
// Cortex-A53 drops the LEN write and computes lane 0 only. After relocation (efile's patched sf_lepoc.cpp calls this)
// each such `vmsr fpscr, Rt` (cond AL, an ORR of LEN/STRIDE bits into Rt within the 4 instructions before it) becomes
// UDF #0x560<Rt>; the kernel's undefined-instruction seam (ekern_seam.cpp EmulateVfpVector) interprets the block from
// there with vector semantics. Only code the loader copies is touched (not ROM / XIP, not demand-paged code).
#include <e32std.h>
#include <e32debug.h>

static TBool PhSetsVectorBits(TUint32 aInstr, TUint aRt)
	{
	if ((aInstr >> 28) != 0xE || (aInstr & 0x0FE00000u) != 0x03800000u || ((aInstr >> 12) & 0xF) != aRt)
		return EFalse;		// ORR Rt, Rn, #imm (cond AL)
	const TUint rot = ((aInstr >> 8) & 0xF) * 2, imm8 = aInstr & 0xFF;
	const TUint32 imm = rot ? (imm8 >> rot) | (imm8 << (32 - rot)) : imm8;
	return (imm & 0x00370000u) != 0;
	}

void PhPatchVfpVectors(TUint32* aCode, TInt aSize)
	{
	if (!aCode || aSize < 8)
		return;
	const TInt words = aSize / 4;
	TInt patched = 0;
	for (TInt i = 1; i < words; ++i)
		{
		const TUint32 w = aCode[i];
		if ((w & 0xFFFF0FFFu) != 0xEEE10A10u)			// vmsr fpscr, Rt (cond AL)
			continue;
		const TUint rt = (w >> 12) & 0xF;
		if (rt == 15)
			continue;
		for (TInt j = i - 1; j >= 0 && j >= i - 4; --j)
			{
			if (PhSetsVectorBits(aCode[j], rt))
				{
				aCode[i] = 0xE7F560F0u | rt;				// UDF #0x560<Rt>
				++patched;
				break;
				}
			}
		}
	if (patched)
		RDebug::Printf("PH: [ldr] VFP short-vector blocks marked for the kernel: %d", patched);
	}
