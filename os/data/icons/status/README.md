# Status-pane indicator glyphs

The status pane's indicator icons (`qgn_stat_*` in avkon.rss), drawn in a 23-26 x 15 px cell (nHD portrait).

* `tabler/` -- the outline icons from **Tabler Icons**, MIT licence (`tabler/LICENSE`), vendored verbatim: only the 48 files the
  indicators use. Source: <https://github.com/tabler/tabler-icons>, **tag v3.52.0**, `icons/outline/<name>.svg`
  (e.g. <https://raw.githubusercontent.com/tabler/tabler-icons/v3.52.0/icons/outline/wifi.svg>). To change the set, change
  pdk's `tools/mif_gen.py` `STAT_ALIASES` (the qgn_stat_* id -> Tabler name table) and fetch the file at the same tag.
* `material/` -- eight **Material Symbols** (Outlined, weight 500, 24 px; Apache-2.0, `material/LICENSE`) where Tabler has no good icon: infrared,
  phone_missed, voicemail, bluetooth, bluetooth_connected, bluetooth_searching (= discoverable), tab, memory. Vendored verbatim except the file
  name (`mat_<name>.svg`). Source: <https://github.com/google/material-design-icons>, commit **10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7**,
  `symbols/web/<name>/materialsymbolsoutlined/<name>_wght500_24px.svg`. Weight 500 has a stem of 2.5 of 24 units, like the Tabler stroke the
  build uses (2.6). They are filled outlines on a 960 grid; the build scales them to the same 21-unit box as the Tabler icons.
* `qgn_stat_*.svg` -- the four glyphs Tabler has no equivalent for (the three blank animation frames and secured WLAN), drawn in Tabler's
  conventions by `os/tools/statusicons/gen.py` (original artwork).

pdk's `tools/mif_gen.py` searches this directory first. Tabler draws with `currentColor` and a 2 px stroke on the 24 grid; the build stages
it as black, rewrites every path to absolute M/L/C/Z (the NVG encoder drops a path with an arc command, which is every rounded corner of
the set: `tools/svgnorm.py`), crops the 24 grid to its 21-unit live area (less padding, bigger glyph) and sets the stroke to `STAT_STROKE`
(2.6 = 1.9 px in the 15 px cell).
