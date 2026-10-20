// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-1.0

// OURS: the REAL openwfcrs/displayrenderstagefactory.cpp with "PH:" lines at factory construction / stage creation.
#include <e32debug.h>
#include "displayrenderstagefactory.h"
#include "displayrenderstage.h"
#include "panic.h"
#define BaseConstructL(e) BaseConstructL(e); RDebug::Printf("PH: alf: alfopenwfrs factory ConstructL (compositor)")
#include <symbian-2010/oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/plugins/openwfcrs/src/displayrenderstagefactory.cpp>
