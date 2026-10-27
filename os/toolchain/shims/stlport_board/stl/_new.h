// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// stl/_new.h -- OURS: STLport's <stl/_new.h> for a module built with C++ exceptions (PH_EH_NEW, set by the x86 builder for
// eh=True components). Its first two operator new declarations (the `__SYMBIAN32__ && !__GCCE__` branch) carry no exception
// specification, but with exceptions on the declaration seen first (symcpp.h, e32cmn.h) says throw () and GCC refuses the
// mismatch. The GCCE branch of the same header omits them, so it is selected for this one include. Other modules see the
// header unchanged.
#ifdef PH_EH_NEW
#  ifndef __GCCE__
#    define __GCCE__ 1
#    define PH_EH_NEW_UNDEF_GCCE
#  endif
#endif
#include_next <stl/_new.h>
#ifdef PH_EH_NEW_UNDEF_GCCE
#  undef __GCCE__
#  undef PH_EH_NEW_UNDEF_GCCE
#endif
