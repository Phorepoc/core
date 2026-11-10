# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""s4_exports.py -- Symbian^4 (docs/phorepoc/symbian4-build.md): the header EXPORT step of the S^4 packages' own builds
(sbs's prj_exports), which their compiles rely on -- qthighway, for one, compiles against its own internal headers
exported to qthighway/inc and its public ones exported to /epoc32/include/mw.

Every qmake-generated bld.inf under the given source trees is read:
  `<src> /epoc32/include/<path>`  -> .phorepoc-s4-build/epoc32/include/<path>   (a separate S^4 export tree:
                                     the S^3 .epoc32 stays as it is; components_s4.py puts its dirs on the path)
  `<src> MW_LAYER_PLATFORM_EXPORT_PATH(x)` etc. -> the same, through platform_paths.hrh's layer macros
  `<src> |<rel path>`             -> <rel path> from the bld.inf's directory (the package's own inc/ dirs)
Other destinations (rom .iby, tools, mkspecs) are not headers and are skipped.

  py tools/phorepoc-s4/s4_exports.py [qtm-src qte-src ...]     (default: every S^4 qmake tree present)
"""
import os
import re
import shutil
import sys

BUILD = os.environ["PHOREPOC_BUILD"]
S10 = os.path.join(os.environ["PHOREPOC_SYMBIAN"], "symbian-2010")
REPO = ""   # (absolute paths below; the private tree used repo-relative ones)
S4R = os.path.join(BUILD, "qt48")
S4 = os.path.join(REPO, S4R)
OUT = os.path.join(S4, "epoc32", "include")
TREES = ["qtm-src", "qte-src", "hsrv-src", "hs-src"]
# platform_paths.hrh's layer export macros (the bld.infs name destinations with them)
LAYER = {"APP_LAYER_PUBLIC_EXPORT_PATH": "app", "APP_LAYER_PLATFORM_EXPORT_PATH": "platform/app",
         "MW_LAYER_PUBLIC_EXPORT_PATH": "mw", "MW_LAYER_PLATFORM_EXPORT_PATH": "platform/mw",
         "OS_LAYER_PUBLIC_EXPORT_PATH": "", "OS_LAYER_PLATFORM_EXPORT_PATH": "platform"}


def exports(bldinf: str):
    """(src, dest) pairs of a bld.inf's prj_exports section(s), in file order."""
    on = False
    for raw in open(bldinf, encoding="utf-8", errors="replace"):
        line = raw.split("//")[0].strip()
        if not line or line.startswith("#"):
            continue
        low = line.lower()
        if low.startswith("prj_"):
            on = low == "prj_exports"
            continue
        if not on:
            continue
        parts = re.findall(r'"[^"]*"|\S+', line)
        if len(parts) >= 2:
            yield parts[0].strip('"'), " ".join(parts[1:]).strip('"')


def main() -> int:
    trees = sys.argv[1:] or [t for t in TREES if os.path.isdir(os.path.join(S4, t))]
    n = 0
    for t in trees:
        root = os.path.join(S4, t)
        for d, _, files in os.walk(root):
            for f in files:
                if not f.startswith("bld.inf"):
                    continue
                rules = list(exports(os.path.join(d, f)))
                # Qt 4.8's qmake drops the .pri files' BLD_INF_RULES.prj_exports header rules (qte xqservice: 39-line bld.inf vs
                # 113 under 4.7): the same package's Qt 4.7 bld.inf names the same source headers -> take its rules, copy from here
                old = os.path.join(REPO, ".phorepoc-s4-build", os.path.relpath(os.path.join(d, f), S4))
                if S4R != ".phorepoc-s4-build" and os.path.isfile(old):
                    have = {r[0] for r in rules}
                    rules += [r for r in exports(old) if r[0] not in have]
                for src, dest in rules:
                    s = os.path.normpath(os.path.join(d, src))
                    lm = re.match(r"(\w+)\((.+)\)$", dest)
                    if lm and lm.group(1) in LAYER:
                        dest = "/epoc32/include/" + "/".join(x for x in (LAYER[lm.group(1)], lm.group(2)) if x)
                    if dest.startswith("|"):
                        target = os.path.normpath(os.path.join(d, dest[1:].strip()))
                    else:
                        m = re.match(r"/?epoc32/include/(.+)$", dest.replace("\\", "/").lstrip("/"))
                        if not m:
                            continue
                        target = os.path.join(OUT, m.group(1))
                    if not os.path.isfile(s):
                        continue
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    shutil.copyfile(s, target)
                    n += 1
    # Qt 4.8's qmake writes app registration resources S^3 cannot compile or use (2026-10-18, a phone boot: hsapplication
    # died KErrNotFound in qt_init -- no hsapplication_reg.rsc): a `graphics_memory=` field the S^3 appinfo.rh lacks ("Label not
    # found"), and service TBUFs without their body. The registration is Qt-version independent -> the 4.7 tree's file where
    # there is one, else the 4.8 one without that field
    regs = 0
    if S4R != ".phorepoc-s4-build":
        for t in trees:
            for d, _, files in os.walk(os.path.join(S4, t)):
                for f in files:
                    if not f.endswith("_reg.rss"):
                        continue
                    p = os.path.join(d, f)
                    old = os.path.join(REPO, ".phorepoc-s4-build", os.path.relpath(p, S4))
                    if os.path.isfile(old):
                        shutil.copyfile(old, p)
                    else:
                        txt = open(p, encoding="utf-8", errors="replace").read()
                        open(p, "w", encoding="utf-8", newline="\n").write(re.sub(r"^\s*graphics_memory\s*=.*\n", "", txt, flags=re.M))
                    regs += 1
    if regs:
        print(f"s4_exports: {regs} _reg.rss files made S^3-compatible (Qt 4.8 qmake)")
    # Qt 4.8's qmake also drops the .pro files' MMP_RULES blocks that carry EPOCHEAPSIZE / EPOCSTACKSIZE (hsapplication:
    # 8 MB default instead of the release's 32 MB -> QString::realloc on a NULL block after an app round trip, boot 1063).
    # Take the DEVICE value (the #else branch of an #ifdef WINSCW block, else the last one) from the Qt 4.7 mmp.
    sizes = 0
    if S4R != ".phorepoc-s4-build":
        for t in trees + ["hb-src"]:
            for d, _, files in os.walk(os.path.join(S4, t)):
                for f in files:
                    if not f.endswith(".mmp"):
                        continue
                    p = os.path.join(d, f)
                    old = os.path.join(REPO, ".phorepoc-s4-build", os.path.relpath(p, S4))
                    if not os.path.isfile(old):
                        continue
                    oldtxt = open(old, encoding="utf-8", errors="replace").read()
                    txt = open(p, encoding="utf-8", errors="replace").read()
                    new = txt
                    for kw in ("EPOCHEAPSIZE", "EPOCSTACKSIZE"):
                        vals = re.findall(r"^\s*" + kw + r"\s+([^\n]+?)\s*$", oldtxt, flags=re.M)
                        if not vals:
                            continue
                        want = vals[-1]
                        if re.search(r"^\s*" + kw + r"\b", new, flags=re.M):
                            new = re.sub(r"^\s*" + kw + r"\s+[^\n]*$", kw + "\t\t" + want, new, count=1, flags=re.M)
                        else:
                            new = new.rstrip("\n") + "\n" + kw + "\t\t" + want + "\n"
                    if new != txt:
                        open(p, "w", encoding="utf-8", newline="\n").write(new)
                        sizes += 1
    if sizes:
        print(f"s4_exports: {sizes} mmps got their Qt 4.7 EPOCHEAPSIZE/EPOCSTACKSIZE back (qmake 4.8 drops MMP_RULES)")
    print(f"s4_exports: {n} headers exported ({', '.join(trees)}) -> {OUT} + package inc/ dirs")
    return 0


if __name__ == "__main__":
    sys.exit(main())
