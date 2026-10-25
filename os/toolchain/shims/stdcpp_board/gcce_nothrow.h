// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// gcce_nothrow.h -- OURS (ARM, pre-included by the components built with C++ exceptions on: libstdcpp).
// Our ARM prefix leaves __NO_THROW / __THROW empty, which fits code built without exceptions. With them on,
// GCC declares placement new throw() and e32cmn.h's empty __NO_THROW no longer matches it. GCCE's gcce.h
// defines both as below.
#undef __NO_THROW
#define __NO_THROW throw ()
#undef __THROW
#define __THROW(t) throw (t)
