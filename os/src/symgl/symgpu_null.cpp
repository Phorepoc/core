// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// symgpu_null.cpp -- OURS: the GPU channel seam of symmali.h for boards without a GPU
// driver: symmali_open() answers NULL, so Mesa's DRM shim finds no device and phgl renders with softpipe. A board
// with a GPU replaces this file in phgl with its own channel (adaptation-a64: src/symgl/symmali.cpp, the Mali-400),
// through its registry's EXTEND table.
#include <e32std.h>
#include <e32debug.h>
#include "symmali.h"

extern "C" symmali_t* symmali_open(void) { return NULL; }
extern "C" void symmali_close(symmali_t*) {}
extern "C" void symmali_info(symmali_t*, symmali_info_t* aOut) { Mem::FillZ(aOut, sizeof(*aOut)); }
extern "C" int symmali_commit(symmali_t*, uint32_t, uint32_t) { return KErrNotSupported; }
extern "C" int symmali_import(symmali_t*, int, uint32_t, uint32_t, uint32_t*) { return KErrNotSupported; }
extern "C" int symmali_import_addr(symmali_t*, const void*, uint32_t, uint32_t*) { return KErrNotSupported; }
extern "C" int symmali_unimport(symmali_t*, uint32_t) { return KErrNotSupported; }
extern "C" int symmali_cache_sync(symmali_t*, uint32_t, uint32_t, int) { return KErrNotSupported; }
extern "C" int symmali_submit(symmali_t*, int, const uint32_t*, unsigned) { return KErrNotSupported; }
extern "C" int symmali_wait(symmali_t*, int, int) { return KErrNotSupported; }

extern "C" void symmali_log(const char* msg)
	{
	RDebug::Printf("%s", msg);
	}
