# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""service_descriptors.py -- the Qt Mobility service descriptors of the Symbian^4 packages, as files to INSTALL.

Qt Mobility's service framework registers a service from its XML descriptor. The release's build ran the host tool `servicedbgen` over every
descriptor (`START EXTENSION qt/qtservice` in a package's bld.inf) into a pre-built system database that went into the ROM
(build_servicedb.sh): a database made by the 1.0.3 tool, with the 1.0.3 schema. The stock Qt Mobility 1.2.x database manager server has a
mechanism for packages that are installed afterwards: it registers every `*.xml` it finds in `\\private\\2002ac7f\\import\\` when it starts
(DiscoverServices, autoimport.ini). That folder is writable by any package, so Orbit installs ITS OWN descriptors there and the QtMobility
package carries no database.

    py tools/phorepoc-s4/service_descriptors.py        -> .phorepoc-qt48-build/sfw/import/*.xml + .phorepoc-qt48-build/sfw/import.list
                                                          (one "private\\2002ac7f\\import\\<name>.xml=<host path>" per descriptor; orbit_sis.py adds them)

The descriptors are found as build_servicedb.sh does: every bld.inf of hs-src / hsrv-src / qte-src with a qt/qtservice extension names them
(QTSERVICE_DESCRIPTOR), the file is looked up in that package's tree. qmake 4.8 drops some of the extension blocks from generated bld.infs (the
command handlers' caapphandlerplugin / catapphandlerplugin / caurlhandlerplugin), so the Qt 4.7 tree's bld.inf of the same package is consulted too.
"""
import os
import re
import shutil
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
R48 = os.path.join(REPO, ".phorepoc-qt48-build")
R47 = os.path.join(REPO, ".phorepoc-s4-build")
OUT = os.path.join(R48, "sfw", "import")
LIST = os.path.join(R48, "sfw", "import.list")
ROOTS = ("hs-src", "hsrv-src", "qte-src")
BS = chr(92)


def infs(root: str):
    for dp, _dn, fn in os.walk(root):
        if "/tsrc" in dp.replace(BS, "/") or "/examples" in dp.replace(BS, "/"):
            continue
        for f in fn:
            if f.lower().startswith("bld.inf"):
                yield os.path.join(dp, f)


def descriptors(inf: str):
    """the QTSERVICE_DESCRIPTOR names of the `START EXTENSION qt/qtservice` blocks of one bld.inf"""
    text = open(inf, encoding="utf-8", errors="replace").read()
    out = []
    for blk in re.findall(r"START\s+EXTENSION\s+qt/qtservice(.*?)^\s*END\b", text, re.S | re.M | re.I):
        m = re.search(r"QTSERVICE_DESCRIPTOR\s+(\S+)", blk)
        if m:
            out.append(m.group(1))
    return out


def find(d: str, name: str):
    for dp, _dn, fn in os.walk(d):
        if "/tsrc" in dp.replace(BS, "/"):
            continue
        if name in fn:
            return os.path.join(dp, name)
    return None


def main() -> int:
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(OUT)
    got = {}
    for tree, base in ((R48, R48), (R47, R48)):
        for root in ROOTS:
            r = os.path.join(tree, root)
            if not os.path.isdir(r):
                continue
            for inf in infs(r):
                rel = os.path.relpath(os.path.dirname(inf), tree)
                d = os.path.join(base, rel)
                if not os.path.isdir(d):
                    continue
                for des in descriptors(inf):
                    if des in got:
                        continue
                    src = find(d, des)
                    if src:
                        shutil.copyfile(src, os.path.join(OUT, des))
                        got[des] = src
                    else:
                        print(f"service_descriptors: {des} ({os.path.relpath(inf, tree)}) not found in the 4.8 tree")
    with open(LIST, "w", newline="\n") as fh:
        for des in sorted(got):
            fh.write("private" + BS + "2002ac7f" + BS + "import" + BS + des + "=" + os.path.join(OUT, des).replace(BS, "/") + "\n")
    print(f"service_descriptors: {len(got)} descriptors -> {OUT}")
    for des in sorted(got):
        print("  +", des)
    return 0


if __name__ == "__main__":
    sys.exit(main())
