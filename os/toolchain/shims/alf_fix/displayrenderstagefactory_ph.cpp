// OURS: the REAL openwfcrs/displayrenderstagefactory.cpp with "PH:" lines at factory construction / stage creation.
#include <e32debug.h>
#include "displayrenderstagefactory.h"
#include "displayrenderstage.h"
#include "panic.h"
#define BaseConstructL(e) BaseConstructL(e); RDebug::Printf("PH: alf: alfopenwfrs factory ConstructL (compositor)")
#include "../../../baseline/symbian-2010/oss.FCL.sf.mw.uiaccelerator/uiacceltk/hitchcock/plugins/openwfcrs/src/displayrenderstagefactory.cpp"
