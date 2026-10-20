/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-1.0 */

// OstTraceDefinitions.h -- OURS (not Symbian source). The OST trace compiler GENERATES this header
// (and <component>Traces.h) into each component's traces/ directory; that output is not part of the
// 2010 source release. The generated file's job is to pull in the trace API and, when tracing is
// compiled in, define OST_TRACE_COMPILER_IN_USE. We never compile traces in, so: the API only --
// opensystemtrace.h then defines every OstTrace*() macro as empty. Put on the include path of a
// component with `flags=[*OST_OFF]` (emitter/components.py).
#ifndef __OSTTRACEDEFINITIONS_H__
#define __OSTTRACEDEFINITIONS_H__
#include <opensystemtrace.h>
#endif
