# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0

"""mk_qt_stubs.py <outdir> -- the ROM stub SIS files for the Qt we ship (docs/phorepoc/qt48-build.md), as qt.iby's
System\\Install\\qt_stub.sis: a SIS of a Qt app declares "(0x2001E61C), 4, 7, 3, {"Qt"}" and SWI refuses it unless that package is
registered -- on a phone by the ROM stub. Package UIDs from Qt's own deployment (src/s60installs/s60installs.pro TARGET.UID3).

Only what the ROM really carries is registered: QtMobility (0x2002AC89) and Qt Quick Components (0x200346DE) come with phase Q4 --
a stub without the libraries would let such apps install and then fail at start.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.environ.get("PHOREPOC_PDK", "/pdk"), "tools", "emitter"))
from sis import build_rom_stub  # noqa: E402

STUBS = {
    "qt_stub.sis": dict(pkg_uid=0x2001E61C, vendor="Qt", name="Qt", version=(4, 8, 7)),
}


def main() -> int:
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for fn, kw in STUBS.items():
        with open(os.path.join(out, fn), "wb") as fh:
            fh.write(build_rom_stub(**kw))
        print(f"mk_qt_stubs: {fn} = package 0x{kw['pkg_uid']:08X} v{'.'.join(map(str, kw['version']))}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
