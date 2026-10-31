// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// stdnew_nothrow.cpp -- std::nothrow for stdnew.dll. operator_new.cpp passes it to the nothrow overloads (new[] -> new);
// the GCCE toolchain's libsupc++ defines it in new_handler.o, the same member that defines std::set_new_handler, which
// stdnew's own set_new_handler.cpp defines (so that member is left out of the link, `supcxx_drop`). The Standard C++
// library itself (stdcpp_support.cpp) exports its own copy, as in the release.
namespace std
	{
	struct nothrow_t {};
	extern const nothrow_t nothrow;
	}

const std::nothrow_t std::nothrow = std::nothrow_t();
