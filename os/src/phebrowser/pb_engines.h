// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// pb_engines.h -- the engines the frontend can host. The built-in dummy engine is compiled in (PHEBROWSER_DUMMY);
// real engines are ECom implementations of PHE_WEBENGINE_ECOM_IFACE_UID found at runtime (docs/phorepoc/browser-frontend.md 3.1).
#ifndef PB_ENGINES_H
#define PB_ENGINES_H

#include <phorepoc/phewebengine.h>

extern "C" const PheWebEngineApi* PheDummyGetApi(unsigned aWantedVersion);

#endif
