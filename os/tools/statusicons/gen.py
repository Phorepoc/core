# Copyright The Phorepoc Authors
# SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
"""gen.py -- Phorepoc's own status-pane indicator glyphs (the ones no open icon set covers) as SVG-Tiny.

    py os/tools/statusicons/gen.py            writes os/data/icons/status/<qgn name>.svg

The status pane's universal indicator pane draws these in a 23-26 x 15 px cell (nHD portrait), so the art is small
and simple. Etesian design language (Iconography): a 24 x 24 grid, ONE 2 px round stroke, outline only, no gradients,
monochrome black (the indicator recolours it by its mask / skin colour). Original artwork; nothing here is copied from
any vendor's icons. The files are named by the icon id avkon.rss asks for (qgn_stat_*), which is how pdk's
tools/mif_gen.py finds them (the first art directory it searches).
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "data", "icons", "status")

HEAD = ('<?xml version="1.0" encoding="UTF-8"?>\n'
        '<!-- Phorepoc status glyph (original artwork), EPL-2.0 OR GPL-2.0-or-later -->\n'
        '<svg xmlns="http://www.w3.org/2000/svg" version="1.1" baseProfile="tiny" width="24" height="24" viewBox="0 0 24 24">\n'
        '<rect x="0" y="0" width="24" height="24" fill="none"/>\n')
S = 'fill="none" stroke="#000000" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
F = 'fill="#000000"'


def p(d, fill=False):
    return f'<path d="{d}" {F if fill else S}/>'


def c(cx, cy, r, fill=False):
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" {F if fill else S}/>'


def r(x, y, w, h, rx=0, fill=False):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" {F if fill else S}/>'


def slash():
    return p("M4 4L20 20")


FAN = [p("M3.5 11A12 12 0 0 1 20.5 11"), p("M7 14.5A7 7 0 0 1 17 14.5")]
EAR = p("M9 21c0-3.5 4-4.5 4-9a4.5 4.5 0 0 0-9 0")
GLYPHS = {
    # the three frames that are blank on purpose (the animated indicators blink between a glyph and nothing)
    "qgn_stat_message_uni_blank": [],
    "qgn_stat_bt_uni_blank": [],
    "qgn_stat_ir_uni_blank": [],
    "qgn_stat_wlan_avail_uni": FAN + [c(12, 19, 1.5, True)],
    "qgn_stat_wlan_active_secure_uni": [p("M2.5 9A12 12 0 0 1 17 5"), p("M6 13A7 7 0 0 1 13 10.5"), c(8, 18, 1.5, True),
                                        r(13, 14, 9, 7, 1, True), p("M15.5 14v-2a2.5 2.5 0 0 1 5 0v2")],
    "qgn_stat_ir_uni": [c(5, 12, 1.5, True), p("M9.5 8A5.5 5.5 0 0 1 9.5 16"), p("M14 5A10 10 0 0 1 14 19"), p("M18.5 2.5A14 14 0 0 1 18.5 21.5")],
    "qgn_stat_fmtx_on_uni": [c(12, 6, 1.5, True), p("M12 8v12"), p("M8 20h8"), p("M7.5 3A6 6 0 0 0 7.5 9"), p("M16.5 3A6 6 0 0 1 16.5 9")],
    "qgn_stat_fmtx_off_uni": [c(12, 6, 1.5, True), p("M12 8v12"), p("M8 20h8"), p("M7.5 3A6 6 0 0 0 7.5 9"), p("M16.5 3A6 6 0 0 1 16.5 9"), slash()],
    "qgn_stat_servtab_uni": [p("M4 18V7a1 1 0 0 1 1-1h5l2 2h7a1 1 0 0 1 1 1v9a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1Z")],
    "qgn_stat_cdma_roaming_uni": [c(12, 12, 8.5), p("M12 3.5c-4 4-4 13 0 17"), p("M12 3.5c4 4 4 13 0 17"), p("M3.5 12h17")],
    "qgn_stat_sync_uni": [p("M18.5 9A7 7 0 0 0 6 7.5"), p("M18.5 4v5h-5"), p("M5.5 15A7 7 0 0 0 18 16.5"), p("M5.5 20v-5h5")],
    "qgn_stat_message_fax_uni": [r(7, 3, 10, 6), r(3, 9, 18, 8, 2), r(7, 14, 10, 7)],
    "qgn_stat_carkit": [p("M3 16v-4l2-5h14l2 5v4Z"), c(7.5, 17.5, 2, True), c(16.5, 17.5, 2, True), p("M3 12h18")],
    "qgn_stat_headset": [p("M5 14v-2a7 7 0 0 1 14 0v2"), r(3, 13, 4, 7, 1.5, True), r(17, 13, 4, 7, 1.5, True)],
    "qgn_stat_headset_unavailable": [p("M5 14v-2a7 7 0 0 1 14 0v2"), r(3, 13, 4, 7, 1.5, True), r(17, 13, 4, 7, 1.5, True), slash()],
    "qgn_stat_loopset": [EAR, p("M14.5 14.5A5 5 0 0 1 14.5 20"), p("M18 12A9 9 0 0 1 18 22.5")],
    "qgn_stat_hac": [EAR, p("M17 5v6"), p("M14 8h6")],
    "qgn_stat_hdmi_connect": [p("M4 7h16v6l-3 3H7l-3-3Z"), p("M9 10v2"), p("M12 10v2"), p("M15 10v2")],
    "qgn_stat_poc_on": [r(8, 9, 8, 12, 2), p("M14.5 9V3"), p("M10.5 13h3")],
    "qgn_stat_poc_dnd": [r(8, 9, 8, 12, 2), p("M14.5 9V3"), p("M10.5 13h3"), slash()],
    "qgn_stat_tarm": [r(6, 6, 12, 12, 2, True), p("M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3")],
    "qgn_stat_mod_tarm": [r(6, 6, 12, 12, 2), p("M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3")],
    "qgn_stat_tty": [r(3, 7, 18, 12, 2), p("M7 11h.01M11 11h.01M15 11h.01M7 15h10")],
    "qgn_stat_tvout": [r(3, 4, 18, 12, 2), p("M8 20h8"), p("M12 16v4")],
    "qgn_stat_indi_mtv_on": [r(3, 8, 18, 12, 2), p("M8 3l4 5 4-5")],
    "qgn_stat_indi_mtv_rec": [r(3, 8, 18, 12, 2), p("M8 3l4 5 4-5"), c(12, 14, 2.2, True)],
    "qgn_stat_voip_readiness": [p("M6 4h3l2 5-2 1.5a11 11 0 0 0 4.5 4.5L15 13l5 2v3a2 2 0 0 1-2 2C10 20 4 14 4 6a2 2 0 0 1 2-2Z")],
}


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, shapes in sorted(GLYPHS.items()):
        with open(os.path.join(OUT, name + ".svg"), "w", encoding="utf-8", newline="\n") as fh:
            fh.write(HEAD + "".join(s + "\n" for s in shapes) + "</svg>\n")
    print(len(GLYPHS), "glyphs ->", os.path.normpath(OUT))


if __name__ == "__main__":
    main()
