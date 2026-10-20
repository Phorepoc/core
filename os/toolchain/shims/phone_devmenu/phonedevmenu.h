// phonedevmenu.h -- OURS: the developer-menu secret code *#3386633# ("DEVMODE" on T9), through the Phone app's real
// secret-code path (docs/phorepoc/devmenu.md). The three files tools/patch_phone_devmenu.py patches share these.
#ifndef PHONEDEVMENU_H
#define PHONEDEVMENU_H

#include <e32std.h>

// the dialled code, as the manufacturer parser (phoneparser) matches it
_LIT( KPheDevMenuCode, "*#3386633#" );
// CPhoneGsmManufacturerHandler::ProcessCommandL's command (the enum ends at EBTDebugMode = 12)
const TUint KPheDevMenuCommand = 0x1000;
// MEngineMonitor message engine -> Phone UI (parser messages are 6000..6999; EPEMessageBTDebugMode is the last used)
const TInt KPheDevMenuMessage = 6990;
// the hidden developer settings application (src/phedevmenu)
const TUid KPheDevMenuAppUid = { 0xE7015A00 };

#endif
