# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""ts_extras.py -- Symbian^4 S4.6 (docs/phorepoc/symbian4-build.md): the English texts newer than the release's
translation snapshot. oss.FCL.sf.mw.qttranslations (tsfiles.zip, 201037) lags the application code (201042): e.g. the
app library's "Applications & widgets (%L1)" (txt_applib_subtitle_applications_widgets_l1) is in no language there.
The packages keep their own source-language .ts next to the code (engineering English, the text the localisers
started from) -- the ids the official <name>_en.ts lacks are taken from it.

    py tools/phorepoc-s4/ts_extras.py   (build_translations.sh runs it after unpacking the _en set)
      -> .phorepoc-s4-build/qm/ts/<name>_extra.ts  (only the missing ids; lrelease keeps duplicates, so no overlap)
      -> .phorepoc-s4-build/qm/ts/<name>_en.ts     (the whole source .ts, when the snapshot has no <name>_en.ts)
"""
import os
import re
import sys

BUILD = os.environ["PHOREPOC_BUILD"]
S10 = os.path.join(os.environ["PHOREPOC_SYMBIAN"], "symbian-2010")
REPO = ""   # (absolute paths below; the private tree used repo-relative ones)
S4R = os.path.join(BUILD, "qt48")
B = S10
TS = os.path.join(REPO, S4R, "qm", "ts")
# (translation catalogue the code loads, source-language .ts in the package)
SOURCES = [
    ("hsapplibrary", f"{B}/oss.FCL.sf.app.homescreen/homescreenapp/stateplugins/hsapplibrarystateplugin/hsapplibrary.ts"),
    ("homescreen", f"{B}/oss.FCL.sf.app.homescreen/homescreenapp/hsapplication/resource/resource_win/homescreen.ts"),
    ("snsrbigclockscreensaverplugin",
     f"{B}/oss.FCL.sf.app.homescreen/screensaver/snsrplugins/snsrbigclockscreensaverplugin/snsrbigclockscreensaverplugin.ts"),
]
MESSAGE = re.compile(r"<message\b[^>]*\bid=\"([^\"]+)\"[^>]*>.*?</message>", re.S)


def main() -> int:
    for name, src in SOURCES:
        text = open(os.path.join(REPO, src), encoding="utf-8", errors="replace").read()
        official = os.path.join(TS, f"{name}_en.ts")
        if not os.path.isfile(official):
            with open(official, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)
            print(f"ts_extras: {name}_en.ts <- {os.path.basename(src)} (all {len(MESSAGE.findall(text))})")
            continue
        have = set(re.findall(r"\bid=\"([^\"]+)\"", open(official, encoding="utf-8", errors="replace").read()))
        missing = [m.group(0) for m in MESSAGE.finditer(text) if m.group(1) not in have]
        out = os.path.join(TS, f"{name}_extra.ts")
        if not missing:
            if os.path.exists(out):
                os.remove(out)
            continue
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write('<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n<TS version="3.0" language="en_GB">\n'
                    f"<context>\n<name>{name}</name>\n" + "\n".join(missing) + "\n</context>\n</TS>\n")
        print(f"ts_extras: {name}_extra.ts ({len(missing)} ids the snapshot lacks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
