# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""make_icon.py -- renders the Catalogue launcher icon: a blue squircle tile (#4d8ecc at the top to #1080dd at the
   bottom) with a white box and a down arrow ("get something into the phone"). Writes
   phestore_icon.svg  the SVG source (original artwork);
   phestore_icon.mbm  the launcher's bitmap icon (64x64 colour bitmap + 8-bit mask) from the same geometry;
   phestore_icon_preview.png  the same at 256x256.
Run: py make_icon.py   (needs Pillow; the uid checksum comes from the repository's emitter/e32image.py when present).
"""
import math
import os
import struct
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
TOP, BOTTOM, WHITE = (0x4D, 0x8E, 0xCC), (0x10, 0x80, 0xDD), (255, 255, 255)
S = 64
K = 8

# the shapes, in a 64x64 box
N = 4.6                                                     # the tile is a squircle: |x|^N + |y|^N = 1
CX, CY, R = 32.0, 32.0, 30.0


def squircle(points=240):
    pts = []
    for i in range(points):
        t = 2 * math.pi * i / points
        c, s = math.cos(t), math.sin(t)
        pts.append((CX + R * math.copysign(abs(c) ** (2 / N), c), CY + R * math.copysign(abs(s) ** (2 / N), s)))
    return pts


SHAFT = (27.5, 9, 36.5, 29)                                 # the arrow's shaft
HEAD = [(18, 27), (46, 27), (32, 42)]                       # the arrow's head
TRAY = [(12, 36), (19, 36), (19, 46), (45, 46), (45, 36), (52, 36), (52, 53), (12, 53)]   # the box (open top)

def svg():
    path = "M" + "L".join("%.2f %.2f" % p for p in squircle()) + "Z"
    return """<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
  <!-- Catalogue launcher icon: original artwork, Apache-2.0 (The Phorepoc Authors). The tile is the squircle
       |x|^4.6 + |y|^4.6 = 1, written out as a path by make_icon.py -->
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#4d8ecc"/>
      <stop offset="1" stop-color="#1080dd"/>
    </linearGradient>
  </defs>
  <path d="%s" fill="url(#g)"/>
  <rect x="27.5" y="9" width="9" height="20" rx="1.5" fill="#FFFFFF"/>
  <path d="M18 27H46L32 42Z" fill="#FFFFFF" stroke="#FFFFFF" stroke-width="2" stroke-linejoin="round"/>
  <path d="M12 36H19V46H45V36H52V53H12Z" fill="#FFFFFF"/>
</svg>
""" % path


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t + 0.5) for i in range(3))


def render():
    n = S * K
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    grad = Image.new("RGBA", (n, n))
    gd = ImageDraw.Draw(grad)
    for y in range(n):
        gd.line([(0, y), (n, y)], fill=lerp(TOP, BOTTOM, y / (n - 1)) + (255,))
    mask = Image.new("L", (n, n), 0)
    ImageDraw.Draw(mask).polygon([(x * K, y * K) for x, y in squircle(720)], fill=255)
    img.paste(grad, (0, 0), mask)
    d = ImageDraw.Draw(img)
    sx0, sy0, sx1, sy1 = [v * K for v in SHAFT]
    d.rounded_rectangle([sx0, sy0, sx1, sy1], radius=int(1.5 * K), fill=WHITE + (255,))
    d.polygon([(x * K, y * K) for x, y in HEAD], fill=WHITE + (255,))
    d.line([(x * K, y * K) for x, y in HEAD + [HEAD[0]]], fill=WHITE + (255,), width=2 * K, joint="curve")
    d.polygon([(x * K, y * K) for x, y in TRAY], fill=WHITE + (255,))
    return img.resize((S, S), Image.LANCZOS), img.resize((256, 256), Image.LANCZOS)


def uid_checksum(u1, u2, u3):
    here = os.path.abspath(os.path.join(HERE, "..", "..", "..", "..", ".."))
    for base in (os.path.join(here, "..", "Symbian-ReverseEngineered", "emitter"), os.path.join(here, "tools", "emitter")):
        if os.path.isfile(os.path.join(base, "e32image.py")):
            sys.path.insert(0, base)
            import e32image
            return e32image.uid_checksum(u1, u2, u3)
    raise SystemExit("no uid_checksum available")


def bitmap(width, height, bpp, color, data):
    return struct.pack("<10i", 40 + len(data), 40, width, height, width * 15, height * 15, bpp, color, 0, 0) + data


def mbm(img):
    rgba = img.convert("RGBA")
    px, mask = bytearray(), bytearray()
    for y in range(S):
        for x in range(S):
            r, g, b, a = rgba.getpixel((x, y))
            px += bytes((b, g, r, 0xFF))
            mask.append(a)
    blobs = [bitmap(S, S, 32, 1, bytes(px)), bitmap(S, S, 8, 0, bytes(mask))]
    off, offsets = 20, []
    for b in blobs:
        offsets.append(off)
        off += len(b)
    uids = (0x10000037, 0x10000042, 0)
    head = struct.pack("<5I", uids[0], uids[1], uids[2], uid_checksum(*uids), off)
    trailer = struct.pack("<I", len(blobs)) + b"".join(struct.pack("<I", o) for o in offsets)
    return head + b"".join(blobs) + trailer


if __name__ == "__main__":
    open(os.path.join(HERE, "phestore_icon.svg"), "w", newline="\n").write(svg())
    small, big = render()
    big.save(os.path.join(HERE, "phestore_icon_preview.png"))
    open(os.path.join(HERE, "phestore_icon.mbm"), "wb").write(mbm(small))
    print("wrote phestore_icon.svg, .mbm, _preview.png")
