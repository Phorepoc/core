# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
"""gen.py -- the few status-pane indicator glyphs the Tabler Icons set (os/data/icons/status/tabler) has no equivalent for.

    py os/tools/statusicons/gen.py            writes os/data/icons/status/<qgn name>.svg

Tabler's own conventions, so they sit in the set: 24 x 24 grid, outline, stroke-width 2 with round caps and joins, `currentColor`
(pdk's tools/mif_gen.py turns that into black masks and sets the indicator stroke). Original artwork. Files are named by the icon
id avkon.rss asks for (qgn_stat_*), which is how mif_gen finds them.
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "data", "icons", "status")

HEAD = ("<!--\n"
        "Phorepoc status glyph (original artwork, in the style of Tabler Icons), EPL-2.0 OR GPL-2.0-or-later\n"
        "-->\n"
        '<svg\n  xmlns="http://www.w3.org/2000/svg"\n  width="24"\n  height="24"\n  viewBox="0 0 24 24"\n  fill="none"\n'
        '  stroke="currentColor"\n  stroke-width="2"\n  stroke-linecap="round"\n  stroke-linejoin="round"\n>\n')
GLYPHS = {
    # blank on purpose: the animated indicators blink between their glyph and nothing
    "qgn_stat_message_uni_blank": [],
    "qgn_stat_bt_uni_blank": [],
    "qgn_stat_ir_uni_blank": [],
    # WLAN connected with encryption: the Tabler wifi arcs (cut short) and a padlock
    "qgn_stat_wlan_active_secure_uni": [
        '<path d="M3.515 9.515c3.2 -3.2 7.6 -4.1 11.485 -2.7" />',
        '<path d="M6.343 12.343a8 8 0 0 1 3.657 -2.1" />',
        '<path d="M8 18l.01 0" />',
        '<path d="M13 14.5a1.5 1.5 0 0 1 1.5 -1.5h5a1.5 1.5 0 0 1 1.5 1.5v4a1.5 1.5 0 0 1 -1.5 1.5h-5a1.5 1.5 0 0 1 -1.5 -1.5z" />',
        '<path d="M15 13v-1.5a2 2 0 1 1 4 0v1.5" />',
    ],
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, shapes in sorted(GLYPHS.items()):
        with open(os.path.join(OUT, name + ".svg"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(HEAD + "".join("  " + x + "\n" for x in shapes) + "</svg>\n")
    print(len(GLYPHS), "glyphs ->", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
