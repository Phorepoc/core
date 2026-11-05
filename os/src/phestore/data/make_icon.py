# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""make_icon.py -- renders the Catalogue launcher icon (download glyph on the "store" accent) to the Etesian design language
(https://language.etesian-design.com; see ../../../tools/icongen/icongen.py). Writes phestore_icon.svg (the source),
phestore_icon.mbm (64x64 bitmap + mask, the registration's icon_file) and phestore_icon_preview.png. Run: py make_icon.py (needs Pillow)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "tools", "icongen"))
import icongen  # noqa: E402
import mbm  # noqa: E402

if __name__ == "__main__":
    mbm.make_app_icon(HERE, "phestore", "download", icongen.ACCENTS["store"])
