# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
"""make_icon.py -- renders the Camera launcher icon (the "camera" glyph on the "photos" accent) to the Etesian design
language (https://language.etesian-design.com; see ../../../tools/icongen/icongen.py). Writes phecamera_icon.svg,
phecamera_icon_tiny.svg (-> phecamera_icon.mif with the pdk's app_icon_mif.py), phecamera_icon.mbm and
phecamera_icon_preview.png. Run: py make_icon.py (needs Pillow)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "icongen"))
import icongen  # noqa: E402
import mbm  # noqa: E402

if __name__ == "__main__":
    mbm.make_app_icon(HERE, "phecamera", "camera", icongen.ACCENTS["photos"])
