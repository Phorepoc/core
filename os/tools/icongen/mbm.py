# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""mbm.py -- an app icon the way the launcher's registration resource wants it: a multi-bitmap file with one 64 x 64
32-bit colour bitmap and its 8-bit mask (icon_file in the app's LOCALISABLE_APP_INFO), drawn by icongen.py in the
Etesian design language. make_app_icon(dir, name, glyph, accent) writes <name>_icon.svg, .mbm and _preview.png."""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import icongen  # noqa: E402

S = 64


def uid_checksum(u1, u2, u3):
    # the checksum comes from the repository's emitter (e32image.py), as it did for the earlier icons
    root = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
    for base in (os.path.join(root, "..", "Symbian-ReverseEngineered", "emitter"), os.path.join(root, "tools", "emitter")):
        if os.path.isfile(os.path.join(base, "e32image.py")):
            sys.path.insert(0, base)
            import e32image
            return e32image.uid_checksum(u1, u2, u3)
    raise SystemExit("no uid_checksum available (needs emitter/e32image.py)")


def _bitmap(width, height, bpp, color, data):
    return struct.pack("<10i", 40 + len(data), 40, width, height, width * 15, height * 15, bpp, color, 0, 0) + data


def mbm(img):
    rgba = img.convert("RGBA")
    px, mask = bytearray(), bytearray()
    for y in range(S):
        for x in range(S):
            r, g, b, a = rgba.getpixel((x, y))
            px += bytes((b, g, r, 0xFF))
            mask.append(a)
    blobs = [_bitmap(S, S, 32, 1, bytes(px)), _bitmap(S, S, 8, 0, bytes(mask))]
    off, offsets = 20, []
    for b in blobs:
        offsets.append(off)
        off += len(b)
    uids = (0x10000037, 0x10000042, 0)
    head = struct.pack("<5I", uids[0], uids[1], uids[2], uid_checksum(*uids), off)
    trailer = struct.pack("<I", len(blobs)) + b"".join(struct.pack("<I", o) for o in offsets)
    return head + b"".join(blobs) + trailer


def make_app_icon(folder, name, glyph, accent):
    g = icongen.GLYPHS[glyph]
    with open(os.path.join(folder, name + "_icon.svg"), "w", newline="\n") as f:
        f.write(icongen.svg(g, accent, name))
    icongen.render_png(g, accent, 256).save(os.path.join(folder, name + "_icon_preview.png"))
    with open(os.path.join(folder, name + "_icon.mbm"), "wb") as f:
        f.write(mbm(icongen.render_png(g, accent, S, ss=8)))
    print("wrote %s_icon.svg, .mbm, _preview.png" % name)
