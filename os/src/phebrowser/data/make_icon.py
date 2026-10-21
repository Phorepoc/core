# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""make_icon.py -- renders the Phorepoc Browser icon (a globe with a "P") and writes
   phebrowser_icon.svg  the SVG source (original artwork: a globe disc, meridians, a bold P whose bowl sits on the globe,
                        an orbit arc);
   phebrowser_icon.mbm  the launcher's bitmap icon (64x64 colour bitmap + 8-bit mask) from the same geometry.
Run: py make_icon.py   (needs Pillow; the uid checksum comes from the repository's emitter/e32image.py when present).
"""
import os
import struct
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
BLUE, WHITE, ORANGE = (0x2F, 0x5B, 0xEA), (255, 255, 255), (0xFF, 0x8A, 0x3D)
S = 64          # icon size
K = 8           # supersampling

SVG = """<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64" viewBox="0 0 64 64">
  <!-- Phorepoc Browser icon: original artwork, Apache-2.0 (The Phorepoc Authors) -->
  <circle cx="32" cy="32" r="28" fill="#2F5BEA"/>
  <g fill="none" stroke="#FFFFFF" stroke-opacity="0.35" stroke-width="1.5">
    <ellipse cx="32" cy="32" rx="12" ry="28"/>
    <path d="M4 32H60M8.5 20H55.5M8.5 44H55.5"/>
  </g>
  <rect x="21" y="16" width="7" height="32" rx="1.5" fill="#FFFFFF"/>
  <ellipse cx="33" cy="26" rx="12" ry="10" fill="#FFFFFF"/>
  <ellipse cx="33" cy="26" rx="5" ry="4.2" fill="#2F5BEA"/>
  <path d="M5 40A31 10 0 0 0 59 24" transform="rotate(-25 32 32)" fill="none" stroke="#FF8A3D" stroke-width="3" stroke-linecap="round"/>
</svg>
"""


def render():
    n = S * K
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    k = K
    d.ellipse([4 * k, 4 * k, 60 * k, 60 * k], fill=BLUE + (255,))
    # meridians and parallels, clipped to the globe
    layer = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    w = int(1.5 * k)
    ld.ellipse([20 * k, 4 * k, 44 * k, 60 * k], outline=WHITE + (90,), width=w)
    for y in (20, 32, 44):
        ld.line([4 * k, y * k, 60 * k, y * k], fill=WHITE + (90,), width=w)
    clip = Image.new("L", (n, n), 0)
    ImageDraw.Draw(clip).ellipse([4 * k, 4 * k, 60 * k, 60 * k], fill=255)
    layer.putalpha(Image.composite(layer.getchannel("A"), Image.new("L", (n, n), 0), clip))
    img.alpha_composite(layer)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([21 * k, 16 * k, 28 * k, 48 * k], radius=int(1.5 * k), fill=WHITE + (255,))
    d.ellipse([21 * k, 16 * k, 45 * k, 36 * k], fill=WHITE + (255,))
    d.ellipse([28 * k, 21.8 * k, 38 * k, 30.2 * k], fill=BLUE + (255,))
    # the orbit: the lower-left to upper-right arc, tilted
    orb = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    od = ImageDraw.Draw(orb)
    od.arc([1 * k, 22 * k, 63 * k, 42 * k], start=10, end=170, fill=ORANGE + (255,), width=int(3 * k))
    orb = orb.rotate(25, center=(32 * k, 32 * k), resample=Image.BICUBIC)
    img.alpha_composite(orb)
    return img.resize((S, S), Image.LANCZOS)


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
    open(os.path.join(HERE, "phebrowser_icon.svg"), "w", newline="\n").write(SVG)
    im = render()
    im.save(os.path.join(HERE, "phebrowser_icon_preview.png"))
    open(os.path.join(HERE, "phebrowser_icon.mbm"), "wb").write(mbm(im))
    print("wrote phebrowser_icon.svg, .mbm, _preview.png")
