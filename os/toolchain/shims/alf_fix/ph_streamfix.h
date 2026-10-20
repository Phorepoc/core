/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// ph_streamfix.h -- OURS: prepended to the generated copy of ServerCore/Src/alfstreamerserver.cpp (tools/alf_instrument.py).
// The release's logging macros are EMPTY in a build without _ALF_LOGGING and the file ends a switch with
// `default: __ALFLOGSTRING1(...)` then `}`: g++ rejects a label before `}`. alflogger.h is include-guarded, so define the macros as `;` first
// (unless ph_alflog.h already made them "PH:" prints).
#include <alflogger.h>
#if !defined(_ALF_LOGGING) && !defined(PH_ALFLOG)
#undef __ALFLOGSTRING
#undef __ALFLOGSTRING1
#undef __ALFLOGSTRING2
#undef __ALFLOGSTRING3
#undef __ALFLOGSTRING4
#define __ALFLOGSTRING(C) ;
#define __ALFLOGSTRING1(C, X) ;
#define __ALFLOGSTRING2(C, X, Y) ;
#define __ALFLOGSTRING3(C, X, Y, Z) ;
#define __ALFLOGSTRING4(C, X, Y, Z, W) ;
#endif
