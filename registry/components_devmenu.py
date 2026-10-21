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
