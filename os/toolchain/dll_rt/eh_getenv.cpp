// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// getenv for the in-image C++ exception runtime (build_mmp.py `cxx_eh`, 2026-10-26): libsupc++'s emergency exception
// pool (eh_alloc.cc) reads GLIBCXX_TUNABLES from a static constructor. Nothing else in these images links a C
// library, so the call fell to the unimpl stub and the app panicked EUSRNIMP as it loaded (Photos opening a picture
// -> glxdataprovider). There is no environment here: no variable is ever set, the pool keeps its defaults.
extern "C" __attribute__((visibility("hidden"))) char* getenv(const char*)
    {
    return 0;
    }
