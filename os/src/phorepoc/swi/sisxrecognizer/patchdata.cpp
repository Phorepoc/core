// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// patchdata.cpp -- OURS: the SISX recognizer's patchable constant at the value a phone ROM patches in.
//
// At 0 (the source default) recsisx is certain only of ".sis" and says EPossible for ".sisx"; any other recognizer that
// answers EProbable then wins (a document viewer's recognizer took ".sisx" as PowerPoint), the installer gets a non-SIS
// MIME type and refuses the package with KSWInstErrPackageNotSupported (-30474). Non-zero = certain of both extensions.
#include <e32def.h>
#include "patchdata.h"

EXPORT_C extern const TUint8 KSisxRecognizerConst = 0x01;
