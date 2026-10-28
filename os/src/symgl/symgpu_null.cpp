// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// symgpu_null.cpp -- OURS: the GPU channel seam of symgpu.h for boards without a GPU
// driver: symgpu_open() answers NULL, so Mesa's DRM shim finds no device and phgl renders with softpipe. A SoC
// with a GPU replaces this file in phgl with its own channel, through its registry's EXTEND table.
#include <e32std.h>
#include <e32debug.h>
#include "symgpu.h"

extern "C" symgpu_t* symgpu_open(void) { return NULL; }
extern "C" void symgpu_close(symgpu_t*) {}
extern "C" void symgpu_info(symgpu_t*, symgpu_info_t* aOut) { Mem::FillZ(aOut, sizeof(*aOut)); }
extern "C" int symgpu_commit(symgpu_t*, uint32_t, uint32_t) { return KErrNotSupported; }
extern "C" int symgpu_import(symgpu_t*, int, uint32_t, uint32_t, uint32_t*) { return KErrNotSupported; }
extern "C" int symgpu_import_addr(symgpu_t*, const void*, uint32_t, uint32_t*) { return KErrNotSupported; }
extern "C" int symgpu_unimport(symgpu_t*, uint32_t) { return KErrNotSupported; }
extern "C" int symgpu_cache_sync(symgpu_t*, uint32_t, uint32_t, int) { return KErrNotSupported; }
extern "C" int symgpu_submit(symgpu_t*, int, const uint32_t*, unsigned) { return KErrNotSupported; }
extern "C" int symgpu_wait(symgpu_t*, int, int) { return KErrNotSupported; }

extern "C" void symgpu_log(const char* msg)
	{
	RDebug::Printf("%s", msg);
	}
