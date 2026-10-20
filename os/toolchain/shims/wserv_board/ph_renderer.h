// ph_renderer.h -- OURS: the UI renderer switch (docs/phorepoc/devmenu.md, "UI renderer").
// WSERV (inifile_board.cpp) reads the developer-menu CenRep key and publishes the answer here; the alf clients that must work
// without an alf server (alf_fix/alfdecoderserverclient_ph.cpp) read it.
#ifndef PH_RENDERER_H
#define PH_RENDERER_H
#include <e32std.h>
// CenRep repository of the developer menu (src/phedevmenu/phedevmenu.h), key 4 = UI renderer (0 Classic, 1 Alfred)
#define PH_DEVMENU_REPO 0xE7015A01
#define PH_DEVMENU_KEY_RENDERER 4
// P&S: category = WSERV's own SID (UID3 0x10003B20), key 'PHR1'; value 0 = Classic, 1 = Alfred. Absent = the ROM has no switch.
#define PH_RENDERER_PS_CATEGORY 0x10003B20
#define PH_RENDERER_PS_KEY 0x50485231
#endif
