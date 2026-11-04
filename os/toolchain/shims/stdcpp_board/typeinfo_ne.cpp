// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// typeinfo_ne.cpp -- std::type_info::operator!= for libstdcppv5.dll. STLport's <typeinfo> declares the operator out of
// line (the release's runtime defined it in its own library); the GCC runtime this system links defines it inline in the
// header, so no object has the out-of-line symbol libstdcppv5's code calls (`_ZNKSt9type_infoneERKS_`). The function is
// the comparison the header spells: two type_infos are different when they are not equal.
#include <typeinfo>

extern "C" bool ph_typeinfo_ne(const std::type_info* aThis, const std::type_info* aOther) __asm__("_ZNKSt9type_infoneERKS_");

extern "C" bool ph_typeinfo_ne(const std::type_info* aThis, const std::type_info* aOther)
	{
	return !(*aThis == *aOther);
	}
