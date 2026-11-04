// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// rt_names.cpp -- the runtime names the GCC archives (libsupc++, libgcc) reference in an image that has no provider for
// them. build_mmp.py links this object, with the -DRT_<name> of each such name, instead of a link stub: the name is a
// real function (or datum) of the image, not a jump to the unimplemented-export panic.
//
//   abort                       User::Panic -- the process-ending call the C++ runtime's terminate path makes (libstdc++'s
//                               std::terminate -> abort). An image that imports a libc keeps that libc's abort.
//   __gnu_Unwind_Find_exidx     the exception index lookup over the image's own .ARM.exidx (the kernel's descriptor of the
//                               code segment holding the pc), as eh_find_exidx.cpp
//   __gnu_cxx::__verbose_terminate_handler
//                               libsupc++'s vterminate.o prints through newlib's `stderr` (_impure_ptr->_stderr), a data
//                               structure this system's libc does not have; the handler here ends the thread the way the
//                               system's own runtime does on an uncaught exception: User::Panic. build_mmp.py leaves
//                               vterminate.o out of the link when it sets RT_vterminate.
#include <e32std.h>
#include <e32svr.h>

#ifdef RT_find_exidx
#include "eh_find_exidx.cpp"
#endif

#ifdef RT_abort
extern "C" __attribute__((visibility("hidden"), noreturn)) void abort(void)
    {
    User::Panic(_L("abort"), 0);
    for (;;) { }
    }
#endif

#ifdef RT_vterminate
namespace __gnu_cxx
    {
    __attribute__((visibility("hidden"), noreturn)) void __verbose_terminate_handler()
        {
        User::Panic(_L("C++ terminate"), 0);
        for (;;) { }
        }
    }
#endif
