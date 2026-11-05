# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
"""icongen.py -- app icons in the Etesian design language (https://language.etesian-design.com, "Iconography",
"Shape: the squircle", "Icon lighting"). Original artwork; nothing here is copied from any vendor's icons.

The rules it implements:
  * the tile is the superellipse |x|^2.8 + |y|^2.8 = 1 (97.5 % of the canvas), never a rounded rectangle;
  * the glyph is drawn on a 24 x 24 grid with ONE 2 px round stroke, outline only, scaled as a whole (60 % of the tile);
  * the tile is lit from straight above: a vertical gradient (starts at 10 %, ends at 72 % of the height, hue leaning
    to the brighter neighbour, +-12 % lightness), a 2.5 % top rim crescent (+10 % lightness, 55 % opacity),
    the glyph off-white (#F6F8ED -> #EAE9E3 -> #D0C9C6) with a 1 px white top highlight and a cast shadow straight down.
  * one accent colour per app, taken from the language's accent table (ACCENTS).

A glyph is a list of primitives in the 24 grid:  ("line", [(x, y), ...])  an open polyline,  ("poly", [...]) a closed one,
("circle", cx, cy, r),  ("ellipse", cx, cy, rx, ry),  ("rrect", x, y, w, h, r),  ("dot", cx, cy)  a filled dot.
render_png() needs Pillow; svg() needs nothing.
"""
import colorsys
import math

N = 2.8                      # the squircle exponent (language: Shape)
TILE = 0.975                 # tile size as a fraction of the canvas
GLYPH = 0.60                 # glyph box (24 grid) as a fraction of the tile
STROKE = 2.0                 # in grid units

# the language's accent table (Colour -> Accent colours)
ACCENTS = {"default": "#4591FF", "phone": "#62B700", "maps": "#3D890C", "conversations": "#09A7CC", "contacts": "#007DC9",
           "email": "#0067BC", "store": "#1C51AF", "photos": "#821DDE", "videos": "#870CBA", "music": "#CC09BA",
           "calendar": "#EF5500", "clock": "#F77219", "calculator": "#ED9507", "notes": "#F2B111", "browser": "#2EA8C7"}

LIGHT = dict(gradient_start=0.10, gradient_end=0.72, delta=0.12, hue_shift=12.0, rim=0.025, rim_light=0.10, rim_opacity=0.55,
             glyph=("#F6F8ED", "#EAE9E3", "#D0C9C6"), highlight="#FFFFFF", cast_offset=0.015, cast_blur=0.012,
             cast_opacity=0.34)


def _hex(c):
    c = c.lstrip("#")
    return tuple(int(c[i:i + 2], 16) for i in (0, 2, 4))


def _tohex(rgb):
    return "#%02X%02X%02X" % tuple(max(0, min(255, int(round(v)))) for v in rgb)


def _hsl(rgb):
    r, g, b = [v / 255.0 for v in rgb]
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    return h * 360.0, s, l


def _rgb(h, s, l):
    r, g, b = colorsys.hls_to_rgb((h % 360.0) / 360.0, max(0.0, min(1.0, l)), max(0.0, min(1.0, s)))
    return (r * 255, g * 255, b * 255)


def _dh(a, b):
    return (b - a + 540.0) % 360.0 - 180.0


def tile_colours(accent):
    """top and bottom of the lit tile (the language's rule: the top leans towards yellow or cyan, the bottom the other way)"""
    h, s, l = _hsl(_hex(accent))
    d = LIGHT["delta"]
    toward = 60.0 if abs(_dh(h, 60.0)) < abs(_dh(h, 190.0)) else 190.0
    f = _dh(h, toward)
    shift = math.copysign(min(abs(f), LIGHT["hue_shift"]), f)
    k = 0.85 if toward == 60.0 else 0.4
    top = _rgb(h + shift * k, s if toward == 60.0 else s * 0.82, l + d * 0.55)
    bottom = _rgb(h - shift * (1.6 - k), min(1.0, s * 1.05), l - d * 0.45)
    return top, bottom


def squircle_points(cx, cy, r, count=360):
    pts = []
    for i in range(count):
        t = 2 * math.pi * i / count
        c, s = math.cos(t), math.sin(t)
        pts.append((cx + r * math.copysign(abs(c) ** (2.0 / N), c), cy + r * math.copysign(abs(s) ** (2.0 / N), s)))
    return pts


def _path(pts):
    return "M" + "L".join("%.2f %.2f" % p for p in pts) + "Z"


def _rrect_pts(x, y, w, h, r, per=10):
    pts = []
    for (cx, cy, a0) in ((x + w - r, y + r, -90), (x + w - r, y + h - r, 0), (x + r, y + h - r, 90), (x + r, y + r, 180)):
        for i in range(per + 1):
            a = math.radians(a0 + 90.0 * i / per)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def _ellipse_pts(cx, cy, rx, ry, count=96):
    return [(cx + rx * math.cos(2 * math.pi * i / count), cy + ry * math.sin(2 * math.pi * i / count)) for i in range(count)]


def strokes(glyph):
    """every primitive as (points, closed) polylines (for the raster) and the dots"""
    out, dots = [], []
    for p in glyph:
        k = p[0]
        if k == "line":
            out.append((list(p[1]), False))
        elif k == "poly":
            out.append((list(p[1]), True))
        elif k == "circle":
            out.append((_ellipse_pts(p[1], p[2], p[3], p[3]), True))
        elif k == "ellipse":
            out.append((_ellipse_pts(p[1], p[2], p[3], p[4]), True))
        elif k == "rrect":
            out.append((_rrect_pts(p[1], p[2], p[3], p[4], p[5]), True))
        elif k == "dot":
            dots.append((p[1], p[2]))
    return out, dots


def _svg_glyph(glyph, attrs):
    parts = []
    for p in glyph:
        k = p[0]
        if k in ("line", "poly"):
            d = "M" + "L".join("%g %g" % q for q in p[1]) + ("Z" if k == "poly" else "")
            parts.append('<path d="%s"/>' % d)
        elif k == "circle":
            parts.append('<circle cx="%g" cy="%g" r="%g"/>' % p[1:4])
        elif k == "ellipse":
            parts.append('<ellipse cx="%g" cy="%g" rx="%g" ry="%g"/>' % p[1:5])
        elif k == "rrect":
            parts.append('<rect x="%g" y="%g" width="%g" height="%g" rx="%g"/>' % p[1:6])
        elif k == "dot":
            parts.append('<circle cx="%g" cy="%g" r="1.2" fill="currentColor" stroke="none"/>' % p[1:3])
    return "<g %s>%s</g>" % (attrs, "".join(parts))


def svg(glyph, accent, title="icon"):
    """the icon as an SVG (100 x 100, no scripts, no external references)"""
    top, bottom = tile_colours(accent)
    L = LIGHT
    half = 50 * TILE
    tile = squircle_points(50, 50, half)
    shifted = squircle_points(50, 50 + 100 * L["rim"], half)
    gs = 100 * TILE * GLYPH / 24.0
    place = "translate(50 50) scale(%.4f) translate(-12 -12)" % gs
    y0, y1 = 50 - 12 * gs, 50 + 12 * gs
    rim = _tohex(_rgb(*(_hsl(top)[0], _hsl(top)[1], _hsl(top)[2] + L["rim_light"])))
    shadow = _tohex(tuple(v * 0.55 for v in bottom))
    stroke = 'fill="none" stroke-width="%g" stroke-linecap="round" stroke-linejoin="round"' % STROKE
    return """<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100" viewBox="0 0 100 100" role="img">
  <title>%(title)s</title>
  <!-- Original artwork, Apache-2.0 (The Phorepoc Authors), drawn to the Etesian design language: squircle |x|^2.8 + |y|^2.8 = 1,
       24-grid glyph with one 2 px round stroke, lit from above (icongen.py) -->
  <defs>
    <linearGradient id="tile" gradientUnits="userSpaceOnUse" x1="0" y1="%(g0).2f" x2="0" y2="%(g1).2f">
      <stop offset="0" stop-color="%(top)s"/><stop offset="1" stop-color="%(bottom)s"/>
    </linearGradient>
    <linearGradient id="glyph" gradientUnits="userSpaceOnUse" x1="0" y1="%(y0).2f" x2="0" y2="%(y1).2f">
      <stop offset="0" stop-color="%(c0)s"/><stop offset="0.5" stop-color="%(c1)s"/><stop offset="1" stop-color="%(c2)s"/>
    </linearGradient>
    <clipPath id="clip"><path d="%(tile)s"/></clipPath>
    <filter id="blur" x="-20%%" y="-20%%" width="140%%" height="140%%"><feGaussianBlur stdDeviation="%(blur).2f"/></filter>
  </defs>
  <path d="%(tile)s" fill="url(#tile)"/>
  <path d="%(tile)s %(shifted)s" fill="%(rim)s" fill-opacity="%(rimop)s" fill-rule="evenodd" clip-path="url(#clip)"/>
  <g clip-path="url(#clip)"><g transform="translate(0 %(cast).2f)" filter="url(#blur)" opacity="%(castop)s" color="%(shadow)s" stroke="%(shadow)s">%(glyph_shadow)s</g></g>
  <g color="%(hi)s" stroke="%(hi)s" transform="translate(0 -0.6)">%(glyph_hi)s</g>
  <g color="#EAE9E3" stroke="url(#glyph)">%(glyph_main)s</g>
</svg>
""" % dict(title=title, g0=100 * L["gradient_start"], g1=100 * L["gradient_end"], top=_tohex(top), bottom=_tohex(bottom),
           y0=y0, y1=y1, c0=L["glyph"][0], c1=L["glyph"][1], c2=L["glyph"][2], tile=_path(tile), shifted=_path(shifted),
           blur=100 * L["cast_blur"], rim=rim, rimop=L["rim_opacity"], cast=100 * L["cast_offset"], castop=L["cast_opacity"],
           shadow=shadow, hi=L["highlight"],
           glyph_shadow=_svg_glyph(glyph, 'transform="%s" %s' % (place, stroke)),
           glyph_hi=_svg_glyph(glyph, 'transform="%s" %s' % (place, stroke)),
           glyph_main=_svg_glyph(glyph, 'transform="%s" %s' % (place, stroke)))


def render_png(glyph, accent, size=256, ss=4):
    """the icon as an RGBA Pillow image of size x size (transparent outside the squircle)"""
    from PIL import Image, ImageChops, ImageDraw, ImageFilter
    L = LIGHT
    n = size * ss
    top, bottom = tile_colours(accent)

    def mask_of(pts_list, dy=0.0):
        m = Image.new("L", (n, n), 0)
        d = ImageDraw.Draw(m)
        for pts in pts_list:
            d.polygon([(x * n / 100.0, (y + dy) * n / 100.0) for x, y in pts], fill=255)
        return m

    half = 50 * TILE
    tile_m = mask_of([squircle_points(50, 50, half, 720)])
    # tile gradient: flat above 10 %, flat below 72 %
    grad = Image.new("RGB", (n, n))
    gd = ImageDraw.Draw(grad)
    for y in range(n):
        t = (y / n - L["gradient_start"]) / (L["gradient_end"] - L["gradient_start"])
        t = max(0.0, min(1.0, t))
        gd.line([(0, y), (n, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    img = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    img.paste(grad, (0, 0), tile_m)
    # the rim: the tile minus itself shifted down
    crescent = ImageChops.subtract(tile_m, mask_of([squircle_points(50, 50, half, 720)], 100 * L["rim"]))
    h, s, l = _hsl(top)
    rim_col = _rgb(h, s, l + L["rim_light"])
    rim_layer = Image.new("RGBA", (n, n), tuple(int(v) for v in rim_col) + (0,))
    rim_layer.putalpha(crescent.point(lambda v: int(v * L["rim_opacity"])))
    img = Image.alpha_composite(img, rim_layer)
    # the glyph mask (one 2 px round stroke)
    gs = 100 * TILE * GLYPH / 24.0 * n / 100.0          # px per grid unit
    w = max(1, int(round(STROKE * gs)))
    st, dots = strokes(glyph)

    def glyph_mask(dy=0.0):
        m = Image.new("L", (n, n), 0)
        d = ImageDraw.Draw(m)
        ox, oy = n / 2 - 12 * gs, n / 2 - 12 * gs + dy * n / 100.0
        for pts, closed in st:
            xy = [(ox + x * gs, oy + y * gs) for x, y in pts]
            if closed:
                xy = xy + [xy[0], xy[1]]
            d.line(xy, fill=255, width=w, joint="curve")
            for q in (xy if not closed else xy[:-2]):
                d.ellipse([q[0] - w / 2, q[1] - w / 2, q[0] + w / 2, q[1] + w / 2], fill=255)
        for (x, y) in dots:
            r = 1.2 * gs
            d.ellipse([ox + x * gs - r, oy + y * gs - r, ox + x * gs + r, oy + y * gs + r], fill=255)
        return m

    g = glyph_mask()
    # the cast shadow straight down, soft, inside the tile only
    sh = glyph_mask(100 * L["cast_offset"]).filter(ImageFilter.GaussianBlur(L["cast_blur"] * n))
    sh = ImageChops.multiply(sh, tile_m).point(lambda v: int(v * L["cast_opacity"]))
    shadow_layer = Image.new("RGBA", (n, n), tuple(int(v * 0.55) for v in bottom) + (0,))
    shadow_layer.putalpha(sh)
    img = Image.alpha_composite(img, shadow_layer)
    # the white top-edge highlight, then the glyph in its own off-white gradient
    hi = glyph_mask(-0.6)
    hi_layer = Image.new("RGBA", (n, n), _hex(L["highlight"]) + (0,))
    hi_layer.putalpha(hi)
    img = Image.alpha_composite(img, hi_layer)
    c0, c1, c2 = [_hex(c) for c in L["glyph"]]
    y0, y1 = n / 2 - 12 * gs, n / 2 + 12 * gs
    gg = Image.new("RGB", (n, n))
    gd = ImageDraw.Draw(gg)
    for y in range(n):
        t = max(0.0, min(1.0, (y - y0) / (y1 - y0)))
        a, b, u = (c0, c1, t * 2) if t < 0.5 else (c1, c2, t * 2 - 1)
        gd.line([(0, y), (n, y)], fill=tuple(int(a[i] + (b[i] - a[i]) * u) for i in range(3)))
    gl = gg.convert("RGBA")
    gl.putalpha(g)
    img = Image.alpha_composite(img, gl)
    img.putalpha(ImageChops.multiply(img.getchannel("A"), tile_m))
    return img.resize((size, size), Image.LANCZOS)


# ---- the glyphs (24 x 24 grid, one 2 px stroke). Original shapes.

def _cog():
    teeth = []
    for i in range(8):
        a = math.radians(45 * i)
        teeth.append(("line", [(12 + 5.2 * math.cos(a), 12 + 6.6 * math.sin(a)), (12 + 9.4 * math.cos(a), 12 + 9.4 * math.sin(a))]))
    return [("circle", 12, 12, 5.2), ("circle", 12, 12, 1.8)] + teeth


GLYPHS = {
    # a globe: the browser
    "globe": [("circle", 12, 12, 9), ("ellipse", 12, 12, 4, 9), ("line", [(3, 12), (21, 12)]),
              ("line", [(4.6, 7.2), (19.4, 7.2)]), ("line", [(4.6, 16.8), (19.4, 16.8)])],
    # an engine: a cog
    "cog": _cog(),
    # a compass: navigate (a small, fast engine)
    "compass": [("circle", 12, 12, 9), ("poly", [(15.8, 8.2), (13.4, 13.4), (8.2, 15.8), (10.6, 10.6)])],
    # a library / package: an isometric cube
    "cube": [("poly", [(12, 2.8), (20, 7.4), (20, 16.6), (12, 21.2), (4, 16.6), (4, 7.4)]),
             ("line", [(4, 7.4), (12, 12), (20, 7.4)]), ("line", [(12, 12), (12, 21.2)])],
    # a codec: a screen with a play triangle and a sound wave under it
    "play": [("rrect", 3, 4, 18, 11, 2), ("poly", [(10, 7), (10, 12), (14.5, 9.5)]),
             ("line", [(4, 19), (6, 19)]), ("line", [(8.5, 17.5), (8.5, 20.5)]), ("line", [(12, 16.5), (12, 21.5)]),
             ("line", [(15.5, 17.5), (15.5, 20.5)]), ("line", [(18, 19), (20, 19)])],
    # a tray with a down arrow: get something onto the device (Catalogue)
    "download": [("line", [(12, 3), (12, 14)]), ("line", [(7.5, 9.5), (12, 14), (16.5, 9.5)]),
                 ("line", [(4, 14.5), (4, 20), (20, 20), (20, 14.5)])],
    # a prompt: the terminal
    "prompt": [("rrect", 3, 4, 18, 16, 2.5), ("line", [(7, 9.5), (10.5, 12.5), (7, 15.5)]), ("line", [(12.5, 15.5), (17, 15.5)])],
}
