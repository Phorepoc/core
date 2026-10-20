/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// opensystemtrace.h -- OURS (not Symbian source): a DEBUG-ONLY stand-in that turns the OST trace macros into RDebug::Printf lines
// (the serial log). The real header makes them empty without the trace compiler's output, which the 2010 release does not
// contain; ost_off/ keeps that default. Put this directory first on a component's include path (flags=[*OST_RDEBUG]) to see
// the component's own trace points (e.g. the SMS stack's state machine: telephony T1 debugging). Only the trace TEXT is printed, not
// its arguments: they may be descriptors passed by value for %S, which a printf-style call mis-reads (USER 19 panic).
#ifndef PH_OPENSYSTEMTRACE_RDEBUG_H
#define PH_OPENSYSTEMTRACE_RDEBUG_H
#include_next <opensystemtrace.h>
#include <e32debug.h>

#undef OstTraceDef0
#undef OstTraceDef1
#undef OstTraceDefExt1
#undef OstTraceDefExt2
#undef OstTraceDefExt3
#undef OstTraceDefExt4
#undef OstTraceDefExt5
#undef OstTrace0
#undef OstTrace1
#undef OstTraceExt1
#undef OstTraceExt2
#undef OstTraceExt3
#undef OstTraceExt4
#undef OstTraceExt5

#define OstTraceDef0( c, g, n, t )                              RDebug::Printf( "[ost] " t )
#define OstTraceDef1( c, g, n, t, a )                           RDebug::Printf( "[ost] %s", t )
#define OstTraceDefExt1( c, g, n, t, a )                        RDebug::Printf( "[ost] %s", t )
#define OstTraceDefExt2( c, g, n, t, a, b )                     RDebug::Printf( "[ost] %s", t )
#define OstTraceDefExt3( c, g, n, t, a, b, d )                  RDebug::Printf( "[ost] %s", t )
#define OstTraceDefExt4( c, g, n, t, a, b, d, e )               RDebug::Printf( "[ost] %s", t )
#define OstTraceDefExt5( c, g, n, t, a, b, d, e, f )            RDebug::Printf( "[ost] %s", t )
#define OstTrace0( g, n, t )                                    RDebug::Printf( "[ost] " t )
#define OstTrace1( g, n, t, a )                                 RDebug::Printf( "[ost] %s", t )
#define OstTraceExt1( g, n, t, a )                              RDebug::Printf( "[ost] %s", t )
#define OstTraceExt2( g, n, t, a, b )                           RDebug::Printf( "[ost] %s", t )
#define OstTraceExt3( g, n, t, a, b, d )                        RDebug::Printf( "[ost] %s", t )
#define OstTraceExt4( g, n, t, a, b, d, e )                     RDebug::Printf( "[ost] %s", t )
#define OstTraceExt5( g, n, t, a, b, d, e, f )                  RDebug::Printf( "[ost] %s", t )
#endif
