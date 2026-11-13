/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phe_ecamresolve.h -- OURS: the camera plug-in selection of ecampluginsupport.dll (EcamPluginSupport.cpp): ROM or
// installed implementations, the highest version_no wins (an installed one on a tie). Leaves KErrNotFound if none.
#ifndef PHE_ECAMRESOLVE_H
#define PHE_ECAMRESOLVE_H
#include <e32std.h>
TUid PheEcamSelectL(TUid aInterfaceUid);
#endif
