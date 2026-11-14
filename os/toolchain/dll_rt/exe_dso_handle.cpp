// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// __dso_handle for EXEs (build_mmp.py, 2026-10-06): GCC registers a global object's destructor with
// __aeabi_atexit(obj, dtor, &__dso_handle); DLLs get the symbol from dll_entry.S, EXEs had none (CED's static
// globals failed to link). Process exit reclaims everything, so the handle is only an address.
extern "C" { __attribute__((visibility("hidden"))) void* __dso_handle = 0; }
