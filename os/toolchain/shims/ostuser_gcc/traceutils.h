// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// traceutils.h (ostuser) -- the release's header, with GET_PC for GCC. The release spells the caller's address as the RVCT
// inline-assembler intrinsic `asm("mov pc, __return_address()")` (ARMv5 only; every other target gets 0), which no GCC
// accepts. The same thing is the builtin: the address this function returns to.
#include_next "traceutils.h"

#undef GET_PC
#define GET_PC(pc) TUint32 pc = (TUint32)(TLinAddr)__builtin_return_address(0)
