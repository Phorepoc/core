// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// __gnu_Unwind_Find_exidx for the in-image C++ exception runtime (build_mmp.py `cxx_eh`, 2026-10-26). libgcc's ARM
// unwinder calls it once per frame to find the .ARM.exidx table that covers a return address. On Linux it walks the
// loaded objects; here the kernel already knows: every image carries a TExceptionDescriptor (device_excdesc.s, its
// offset in the E32 header), and UserSvr::ExceptionDescriptor(pc) returns the one of the code segment holding pc --
// the lookup drtaeabi's unwinder does for `device` images. So frames in other DLLs unwind too. Before this the symbol
// was the unimpl stub: the first throw panicked EUSRNIMP (Photos opening a picture, alfwidgetmodel).
#include <e32std.h>
#include <e32svr.h>

namespace
    {
    struct TExcDesc     // e32def_private.h TExceptionDescriptor
        {
        TLinAddr iExIdxBase;
        TLinAddr iExIdxLimit;
        TLinAddr iROSegmentBase;
        TLinAddr iROSegmentLimit;
        };
    }

extern "C" __attribute__((visibility("hidden"))) TLinAddr __gnu_Unwind_Find_exidx(TLinAddr aPc, int* aCount)
    {
    const TExcDesc* d = reinterpret_cast<const TExcDesc*>(UserSvr::ExceptionDescriptor(aPc) & ~1u);   // bit 0 = the EHABI v2 mark (symbian_support.cpp masks it too)
    if (!d)
        {
        *aCount = 0;
        return 0;
        }
    *aCount = (d->iExIdxLimit - d->iExIdxBase) / 8;     // two words per entry
    return d->iExIdxBase;
    }
