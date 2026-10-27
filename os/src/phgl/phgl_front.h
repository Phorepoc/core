/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phgl_front.h -- OURS: what the generated ABI fronts (gen/lib*_front.cpp: libEGL.dll, libGLESv2.dll,
// libGLESv1_CM.dll) share -- the lazy binding to phgl.dll (phgl_front.cpp).
#ifndef PHGL_FRONT_H
#define PHGL_FRONT_H

#include <e32std.h>

// the backend's entry point at aOrdinal of phgl.dll, or (aOrdinal 0 / not exported) phgl's eglGetProcAddress(aName);
// NULL when there is no phgl.dll (the front then fails every call quietly)
TLibraryFunction PhglResolve(TInt aOrdinal, const char* aName);

// libEGL.dll's function at aOrdinal (libegl14U.def) -- for a front that re-exports egl* (libGLES_CM.dll); NULL without it
TLibraryFunction PhglResolveEgl(TInt aOrdinal, const char* aName);
// TRACE (2026-10-27): after glCompileShader / glLinkProgram -- a failure (status 0) is logged with its info log ("PH:" line)
void PhglCheckStatus(const char* aWhat, unsigned aObj, unsigned aStatusEnum, const char* aQuery, const char* aLog);

#endif
