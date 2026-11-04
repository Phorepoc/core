// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// xu_rtnames.cpp -- Phorepoc amd64/x32: the runtime names the toolchain's own archives (the distro libsupc++, libgcc)
// reference in an image that has no provider for them. build_mmp_x32.py links this object, with the -DRT_<name> of each
// such name, instead of a link stub: the name is a real function of the image.
//
//   __stack_chk_fail   the distro libsupc++ is built with stack protection: its members call this when the guard word
//                      of a frame was overwritten. User::Panic ends the thread -- the platform's reaction to a
//                      corrupted stack.
//   abort              what libsupc++'s terminate path calls: User::Panic. An image that imports a libc keeps that
//                      libc's abort.
#include <e32std.h>

#ifdef RT_stack_chk_fail
extern "C" __attribute__((visibility("hidden"), noreturn)) void __stack_chk_fail(void)
	{
	User::Panic(_L("stack smashing"), 0);
	for (;;) { }
	}
#endif

#ifdef RT_abort
extern "C" __attribute__((visibility("hidden"), noreturn)) void abort(void)
	{
	User::Panic(_L("abort"), 0);
	for (;;) { }
	}
#endif

#ifdef RT_nan
// C99 nan(): a quiet NaN; the tag selects the payload on platforms that keep one, and this one keeps none (the
// standard allows a tag that is not recognised to give the same NaN as strtod("NAN")). WebKit's trio_nan calls it.
extern "C" __attribute__((visibility("hidden"))) double nan(const char*)
	{
	return __builtin_nan("");
	}
#endif
