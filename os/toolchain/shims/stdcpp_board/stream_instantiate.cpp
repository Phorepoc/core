// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// stream_instantiate.cpp (2026-10-31) -- the stream class instantiations libstdcppv5.dll exports. The library's own sources force them
// (stl/src/sstream.cpp, istream.cpp, fstream.cpp: "Force instantiation of ... classes") unless
// _STLP_NO_FORCE_INSTANTIATE is defined -- and STLport's GCC configuration (stl/config/_gcc.h) defines it
// unconditionally: a GCC-built library then has no typeinfo, vtables or VTTs for std::stringstream & co. and their
// .def slots fell to the unimplemented-export stub. Applications built against the SDK library import those slots, so
// the first std::stringstream they construct died EUSRNIMP (Stellarium Mobile). The lists below are the library's own.
#include "stlport_prefix.h"

#include <sstream>
#include <istream>
#include <fstream>

_STLP_BEGIN_NAMESPACE

// stl/src/sstream.cpp
template class basic_stringbuf<char, char_traits<char>, allocator<char> >;
template class basic_ostringstream<char, char_traits<char>, allocator<char> >;
template class basic_istringstream<char, char_traits<char>, allocator<char> >;
template class basic_stringstream<char, char_traits<char>, allocator<char> >;
#if !defined (_STLP_NO_WCHAR_T)
template class basic_stringbuf<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >;
template class basic_ostringstream<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >;
template class basic_istringstream<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >;
template class basic_stringstream<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >;
#endif

// stl/src/istream.cpp
template class basic_iostream<char, char_traits<char> >;
template class basic_istream<char, char_traits<char> >;
#if !defined (_STLP_NO_WCHAR_T)
template class basic_iostream<wchar_t, char_traits<wchar_t> >;
template class basic_istream<wchar_t, char_traits<wchar_t> >;
#endif

// stl/src/fstream.cpp
template class basic_filebuf<char, char_traits<char> >;
template class basic_ifstream<char, char_traits<char> >;
template class basic_ofstream<char, char_traits<char> >;
template class basic_fstream<char, char_traits<char> >;
#if !defined (_STLP_NO_WCHAR_T)
template class basic_filebuf<wchar_t, char_traits<wchar_t> >;
template class basic_ifstream<wchar_t, char_traits<wchar_t> >;
template class basic_ofstream<wchar_t, char_traits<wchar_t> >;
template class basic_fstream<wchar_t, char_traits<wchar_t> >;
#endif

_STLP_END_NAMESPACE
