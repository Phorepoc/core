// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// stdcpp_x32.cpp -- OURS (Path C board, x32): what the RVCT/GCCE builds of Open C++ (ossrv/stdcpp, libstdcpp.dll)
// got from their toolchains and our ELF GCC does not provide.
//
// 1. num_get / num_put<char, stream iterators>::GetFacetLocaleId(): member templates defined only in the library's
//    libstdcppwsd.h, while the classes are explicitly instantiated in TUs that never include it -- so the two
//    members were never instantiated (RVCT emitted them through dllexport). Instantiated here, where it is visible.
// 2. std::type_info::operator!=: Symbian's <typeinfo.h> (the RVCT ABI) declares it out of line and exported; GCC's
//    runtime has it inline only. Same comparison as GCC's type_info::operator== (a leading '*' = a local type,
//    compared by address; else by name).
// (the includes of the library's own locale_impl.cpp, which includes libstdcppwsd.h the same way)
#include "stlport_prefix.h"

#include "locale_impl.h"
#include <locale>
#include <typeinfo>
#include <stdexcept>
#include "c_locale.h"
#include "aligned_buffer.h"
#include <exception>
#include <stl/_codecvt.h>
#include <stl/_collate.h>
#include <stl/_ctype.h>
#include <stl/_monetary.h>
#include "message_facets.h"
#include <string.h>

#include "libstdcppwsd.h"

_STLP_BEGIN_NAMESPACE
template locale::id& num_get<char, istreambuf_iterator<char, char_traits<char> > >::GetFacetLocaleId();
template locale::id& num_put<char, ostreambuf_iterator<char, char_traits<char> > >::GetFacetLocaleId();
_STLP_END_NAMESPACE

namespace std {
EXPORT_C bool type_info::operator!=(const type_info& aOther) const
    {
    const char* a = __type_name;
    const char* b = aOther.__type_name;
    if (a == b)
        return false;
    return a[0] == '*' || b[0] == '*' || strcmp(a, b) != 0;
    }
}
