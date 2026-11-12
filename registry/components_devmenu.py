# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

"""components_devmenu -- OURS: the hidden developer menu. Merged into components.COMPONENTS."""
import os

from paths import OS_ROOT, SYMBIAN, BUILD, EPOC32_ROOT  # noqa: E402

COMPONENTS = {
    # the Avkon settings list (UI shell S^3|S^4, Control Panel shortcut, boot guard timeout) in CenRep e7015a01; hidden app,
    # opened by the dialer code *#3386633# (the Phone app's secret-code path: toolchain/shims/phone_devmenu) or the plug-in below
    "phedevmenu": dict(mmp=os.path.join(OS_ROOT, "src/phedevmenu/phedevmenu.mmp")),
    # the Control Panel (Device) entry "Developer menu", visible while the shortcut setting is on
    "gsphedevmenu": dict(mmp=os.path.join(OS_ROOT, "src/phedevmenu/gsphedevmenu.mmp")),
}

COMPONENTS["gsphshell"] = dict(mmp=os.path.join(OS_ROOT, "src/phedevmenu/gsphshell.mmp"))   # Control Panel (next to Display) "User interface": the shells there are + a restart
COMPONENTS["gsphorient"] = dict(mmp=os.path.join(OS_ROOT, "src/phedevmenu/gsphorient.mmp"))   # Control Panel (next to Display) "Screen orientation": Automatic / Portrait / Landscape
COMPONENTS["gsphbrowser"] = dict(mmp=os.path.join(OS_ROOT, "src/phedevmenu/gsphbrowser.mmp"))   # Control Panel (next to Display) "Default browser": apparc's app for x-scheme-handler/http
