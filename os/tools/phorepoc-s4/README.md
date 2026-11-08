# phorepoc-s4

The configure steps of the Symbian^4 UI set (Orbit = Hb, the Qt homescreen, the Qt service framework and its friends), ported from
the private tree's `tools/phorepoc-s4/`. They run inside the toolchain image, like `../phorepoc-qt48/`, with `PHOREPOC_BUILD` and
`PHOREPOC_SYMBIAN` set (`s4env.sh`) and after Qt 4.8 has been built (`../phorepoc-qt48/build_qt_sis.sh`: `qt-host`,
`qt-symbian`, `qt-src` under `<build>/qt48`). Everything is written to `<build>/qt48`; the release checkouts are never written.

Order (the orbit-ui package's `recipes/prebuild.sh` runs it):

| step | script | out |
|---|---|---|
| Orbit | `hb_configure_symbian.sh`, then `hb_zdata.py` | `hb-src`, `hb-symbian`, `hb-zdata.txt` |
| Qt Mobility, Qt extensions | `qtm_configure_symbian.sh`, `qte_configure_symbian.sh` | `qtm-src`, `qte-src` |
| homescreensrv, the homescreen | `hsrv_configure_symbian.sh`, `hs_configure_symbian.sh` | `hsrv-src`, `hs-src` |
| header exports | `s4_exports.py` | `epoc32/include` (the S^4 export tree) |
| host tools, data | `build_hb_host_tools.sh`, `build_servicedb.sh`, `build_translations.sh`, `s4_zdata.py` | `hb-host`, `sfw`, `qm`, `s4-zdata.txt` |

`ts_extras.py` adds the task switcher's texts, `hb_noexcept.py` the no-exceptions rewrite the generated sources need.

Status: ported mechanically from scripts that ran in the private layout; the public path (this folder driven by the pdk's
package.yml) has not been run end to end. The old Qt 4.7 route, and `make_cssbin.sh` (Hb's binary stylesheet made on the target:
a booted ROM is needed) are not ported; `hb_zdata.py` ships without `hbdefault.cssbin` then and the theme server keeps the release's
stylesheet.
