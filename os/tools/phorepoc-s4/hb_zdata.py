# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""hb_zdata -- Orbit's ROM data (Symbian^4 S4.4, docs/phorepoc/symbian4-build.md), for the S^4 ROM.

Orbit's .pro files export their Z: data with hbPrjExport("<src>", "$${EPOCROOT}epoc32/data/z/<rom path>") -- the
central-repository files HbCore opens at start (theme 20022E82, sensors 2002C384, ...), the i18n tables, the display
definition. qmake's Symbian generator only passes some of them to bld.inf, so they are read from the .pro files.

    py tools/phorepoc-s4/hb_zdata.py     -> .phorepoc-s4-build/hb-zdata.txt  (`<rom path>=<repo path>` lines)

tools/phorepoc-x86/x32_rom_args.py adds the list to the ROM when PHOREPOC_S4=1.
"""
import os
import re
import sys

BUILD = os.environ["PHOREPOC_BUILD"]
S10 = os.path.join(os.environ["PHOREPOC_SYMBIAN"], "symbian-2010")
REPO = ""   # (absolute paths below; the private tree used repo-relative ones)
S4R = os.path.join(BUILD, "qt48")
HB = os.path.join(REPO, S4R, "hb-src")
OUT = os.path.join(REPO, S4R, "hb-zdata.txt")
EXPORT = re.compile(r'hbPrjExport\(\s*"([^"]+)"\s*,\s*"[^"]*epoc32/data/z/([^"]+)"\s*\)')


def main() -> int:
    found = {}
    for d, _, files in os.walk(os.path.join(HB, "src")):
        for f in files:
            if not f.endswith((".pro", ".pri")):
                continue
            text = open(os.path.join(d, f), encoding="utf-8", errors="replace").read()
            for src, rom in EXPORT.findall(text):
                host = os.path.normpath(os.path.join(d, src.replace("$${PWD}", "").lstrip("/")))
                if os.path.isfile(host):
                    # lower case: rombuild's directory tree is case-sensitive (z:\private\10202be9 exists already)
                    found[rom.lower()] = (rom.lower().replace("/", "\\"),
                                          host.replace("\\", "/"))
    # ... and the ones that did reach a generated bld.inf (`<src rel. to the .pro dir> /epocroot/epoc32/data/z/<rom>`)
    gen = os.path.join(REPO, S4R, "hb-symbian")
    for d, _, files in os.walk(gen):
        if "bld.inf" not in files:
            continue
        srcdir = os.path.join(HB, os.path.relpath(d, gen))
        for line in open(os.path.join(d, "bld.inf"), encoding="utf-8", errors="replace"):
            m = re.match(r'\s*"?(\S+?)"?\s+"?\S*epoc32/data/z/(\S+?)"?\s*$', line)
            if m:
                host = os.path.normpath(os.path.join(srcdir, m.group(1)))
                if os.path.isfile(host):
                    found[m.group(2).lower()] = (m.group(2).lower().replace("/", "\\"),
                                                 host.replace("\\", "/"))
    # the binary stylesheet (hb.pro's cssbinmaker rule) -- the target-made one (./ph s4 cssbin); build_hb_host_tools.sh's LP64
    # output is named .cssbin.lp64 and stays out (its layout crashes the x32 HbSharedCache)
    cssbin = os.path.join(REPO, S4R, "hb-themes", "hbdefault.cssbin")
    # (Qt 4.8 tree, 2026-10-18: until a 4.8 target run makes its own, the 4.7 target-made one -- 32-bit, qreal = float on both)
    if not os.path.isfile(cssbin):
        cssbin = os.path.join(REPO, ".phorepoc-s4-build", "hb-themes", "hbdefault.cssbin")
    if os.path.isfile(cssbin):
        found["resource/hb/themes/hbdefault.cssbin"] = ("resource\\hb\\themes\\hbdefault.cssbin",
                                                        cssbin.replace("\\", "/"))
    # the typeface metrics (text height -> font pixel size per family) HbTypefaceInfo reads from z:\resource\fonts
    # (TYPEFACE_RESOURCE_FOLDER): the release keeps them in hbcore/resources/variant, the device's font package put
    # them in the ROM. Without them Orbit auto-generates a table whose height-0 entry has an UNSET pixel size -- a
    # `textheight="0"` font (the app library's title group boxes) came out millions of pixels high and fbserv died
    variant = os.path.join(HB, "src", "hbcore", "resources", "variant")
    for f in sorted(os.listdir(variant)):
        if f.startswith("typeface_metrics_") and f.endswith(".xml"):
            found["resource/fonts/" + f] = ("resource\\fonts\\" + f,
                                            os.path.join(variant, f).replace("\\", "/"))
    with open(OUT, "w", newline="\n") as fh:
        for rom, host in sorted(found.values()):
            fh.write(f"{rom}={host}\n")
    print(f"hb_zdata: {len(found)} ROM data files -> {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
