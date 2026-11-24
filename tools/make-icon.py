#!/usr/bin/env python3
"""make-icon.py - render sce_sys/icon0.png, the XMB tile for the player.

Three lines, top to bottom: ULTIMATE, the PS4 emblem the app draws in its
header (read straight from the span table in src/ps4logo.h, so the icon and
the app match), and MEDIA PLAYER. All of it chrome over a steel-blue plate:
the metal is a vertical stop gradient, the gloss is a white ramp mixed into
the top of each shape, plus a white lip on the top edge, a dark bevel and a
drop shadow.

    python3 tools/make-icon.py

Writes 512x512 sce_sys/icon0.png.
"""
import os
import re
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

W = H = 512
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FONT = ("/opt/freestyle/python/lib/python3.12/site-packages/"
        "matplotlib/mpl-data/fonts/ttf/DejaVuSans-Bold.ttf")

# chrome: bright lip, soft grey, a bright band through the middle, darker below
CHROME = [
    (0.00, (255, 255, 255)),
    (0.16, (226, 238, 250)),
    (0.36, (118, 137, 160)),
    (0.48, (248, 252, 255)),
    (0.60, (166, 184, 205)),
    (0.78, (82, 100, 124)),
    (0.90, (206, 224, 242)),
    (1.00, (250, 253, 255)),
]

STEEL = [                       # the plate the whole icon sits on
    (0.00, (36, 64, 98)),
    (0.35, (18, 34, 55)),
    (0.70, (9, 18, 30)),
    (1.00, (4, 9, 16)),
]

# the wall of the extrusion: lit steel right under the face, nearly black at
# the back, so the letters read as solid blocks rather than flat type
SIDE = [
    (0.00, (96, 122, 152)),
    (0.30, (48, 66, 90)),
    (0.65, (18, 30, 46)),
    (1.00, (5, 10, 18)),
]


def lerp_stops(stops, t):
    t = min(max(t, 0.0), 1.0)
    for i in range(len(stops) - 1):
        t0, c0 = stops[i]
        t1, c1 = stops[i + 1]
        if t <= t1 or i == len(stops) - 2:
            k = 0.0 if t1 <= t0 else (t - t0) / (t1 - t0)
            k = min(max(k, 0.0), 1.0)
            return tuple(int(c0[j] + (c1[j] - c0[j]) * k) for j in range(3))
    return stops[-1][1]


def gradient(size, stops):
    """A vertical gradient image of `size`."""
    w, h = size
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        px[0, y] = lerp_stops(stops, y / max(h - 1, 1))
    for y in range(h):
        for x in range(1, w):
            px[x, y] = px[0, y]
    return img


def ramp(size, y0, y1, peak, span, power=0.8):
    """An L image: `peak` at the top of the shape, fading out by `span`."""
    w, h = size
    out = Image.new("L", (w, h), 0)
    px = out.load()
    height = max(y1 - y0 - 1, 1)
    for y in range(h):
        t = (y - y0) / height
        v = int(peak * (1.0 - t / span) ** power) if t < span else 0
        if v:
            for x in range(w):
                px[x, y] = v
    return out


def metal(mask, size, stops=CHROME, gloss=0.62, gloss_span=0.46,
          shadow=(0, 5), shadow_alpha=120, bevel=3, depth=0, dx=2,
          shine=0.0):
    """Turn an L mask into a shiny metal shape, on a transparent layer.

    depth extrudes the shape towards the viewer, one row at a time, so it
    reads as a solid block of metal standing off the plate. shine adds a
    soft specular blob across the upper part of the face."""
    w, h = size
    bb = mask.getbbox()
    if not bb:
        return Image.new("RGBA", (size), (0, 0, 0, 0))
    y0, y1 = bb[1], bb[3]

    # the metal: the stop gradient across the shape's own height
    grad = Image.new("RGBA", (w, h))
    src = grad.load()
    for y in range(h):
        c = lerp_stops(stops, (y - y0) / max(y1 - y0 - 1, 1))
        for x in range(w):
            src[x, y] = (c[0], c[1], c[2], 255)

    # the gloss: the top of the shape lifts towards white
    body = Image.composite(Image.new("RGBA", (w, h), (255, 255, 255, 255)),
                           grad, ramp((w, h), y0, y1, int(255 * gloss),
                                      gloss_span))

    # the specular blob that makes the face look wet rather than painted
    if shine:
        hl = Image.new("L", (w, h), 0)
        hh = max(y1 - y0, 1)
        ImageDraw.Draw(hl).ellipse([-w * 0.15, y0 - hh * 0.55,
                                    w * 1.15, y0 + hh * 0.6], fill=255)
        hl = hl.filter(ImageFilter.GaussianBlur(max(hh // 3, 3)))
        hl = ImageChops.multiply(hl, mask)
        body = Image.composite(Image.new("RGBA", (w, h), (255, 255, 255, 255)),
                               body, hl.point(lambda v: int(v * shine)))

    # a white lip on the top edge, a dark one on the bottom edge
    down = Image.new("L", (w, h), 0)
    down.paste(mask, (0, 3))
    top = ImageChops.subtract(mask, down).filter(ImageFilter.GaussianBlur(1))
    body = Image.composite(Image.new("RGBA", (w, h), (255, 255, 255, 255)),
                           body, top.point(lambda v: int(v * 0.85)))
    up = Image.new("L", (w, h), 0)
    up.paste(mask, (0, -2))
    bottom = ImageChops.subtract(mask, up).filter(ImageFilter.GaussianBlur(1))
    body = Image.composite(Image.new("RGBA", (w, h), (12, 22, 36, 255)),
                           body, bottom.point(lambda v: int(v * 0.40)))
    left = Image.new("L", (w, h), 0)
    left.paste(mask, (3, 0))
    lrim = ImageChops.subtract(mask, left)
    body = Image.composite(Image.new("RGBA", (w, h), (255, 255, 255, 255)),
                           body, lrim.point(lambda v: int(v * 0.45)))

    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))

    # the drop shadow, thrown back and down from the raised face
    if shadow_alpha:
        grown = mask.filter(ImageFilter.MaxFilter(bevel * 2 + 1))
        sm = grown.filter(ImageFilter.GaussianBlur(10 + depth // 2))
        sm = sm.point(lambda v: int(v * shadow_alpha / 255))
        sh = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        sh.paste(Image.new("RGBA", (w, h), (2, 5, 10, 255)),
                 (shadow[0] + depth // 2, shadow[1] + depth), sm)
        out = Image.alpha_composite(out, sh)

    # the dark bevel, so the shape reads against the plate
    if bevel:
        grown = mask.filter(ImageFilter.MaxFilter(bevel * 2 + 1))
        out.paste(Image.new("RGBA", (w, h), (5, 10, 18, 255)), (0, 0), grown)

    # the extrusion: the side wall of the letter, back to front
    for k in range(depth, 0, -1):
        c = lerp_stops(SIDE, k / max(depth, 1))
        out.paste(Image.new("RGBA", (w, h), (c[0], c[1], c[2], 255)),
                  (int(round(dx * k / max(depth, 1))), k), mask)

    body.putalpha(mask)          # the metal exists only where the shape is
    return Image.alpha_composite(out, body)


def spaced_text(text, font, tracking):
    """Render `text` with extra letter spacing, returning an L mask."""
    d = ImageDraw.Draw(Image.new("L", (4, 4)))
    widths = []
    for ch in text:
        bb = d.textbbox((0, 0), ch, font=font)
        widths.append(bb[2] - bb[0])
    total = sum(widths) + tracking * (len(text) - 1)
    asc, desc = font.getmetrics()
    mask = Image.new("L", (total + 8, asc + desc + 12), 0)
    md = ImageDraw.Draw(mask)
    x = 2
    for ch, wd in zip(text, widths):
        bb = md.textbbox((0, 0), ch, font=font)
        md.text((x - bb[0], 2), ch, font=font, fill=255)
        x += wd + tracking
    return mask


def fit_text(text, target_w, start, tracking_ratio=0.14):
    size = start
    while size > 12:
        font = ImageFont.truetype(FONT, size)
        tracking = max(int(size * tracking_ratio), 1)
        m = spaced_text(text, font, tracking)
        bb = m.getbbox()
        if bb[2] - bb[0] <= target_w:
            return m.crop(bb)
        size -= 2
    raise SystemExit("could not fit " + text)


def ps4_emblem(scale):
    """The app's wordmark, from the span table in src/ps4logo.h."""
    src = open(os.path.join(ROOT, "src", "ps4logo.h"), encoding="utf-8").read()
    runs = [tuple(int(v) for v in m) for m in
            re.findall(r"\{(\d+),(\d+),(\d+)\}", src)]
    base = Image.new("L", (192, 40), 0)
    d = ImageDraw.Draw(base)
    for x, y, w in runs:
        d.rectangle([x, y, x + w - 1, y], fill=255)
    big = base.resize((192 * 8, 40 * 8), Image.NEAREST)
    return big.resize((int(192 * scale), int(40 * scale)), Image.LANCZOS)


def plate():
    img = Image.new("RGBA", (W, H))
    img.paste(gradient((W, H), STEEL), (0, 0))

    # a soft sheen from the top left, so the plate is not flat
    glow = Image.new("L", (W, H), 0)
    ImageDraw.Draw(glow).ellipse([-140, -230, 470, 300], fill=105)
    glow = glow.filter(ImageFilter.GaussianBlur(70))
    img = Image.composite(Image.new("RGBA", (W, H), (150, 195, 235, 255)),
                          img, glow)

    # faint brushed lines
    lines = Image.new("L", (W, H), 0)
    ld = ImageDraw.Draw(lines)
    for y in range(0, H, 4):
        ld.line([(0, y), (W, y)], fill=9)
    img = Image.composite(Image.new("RGBA", (W, H), (205, 228, 246, 255)),
                          img, lines)

    # a wide specular streak across the plate: the light that makes steel
    # look polished rather than painted
    band = Image.new("L", (W * 2, H * 2), 0)
    bd = ImageDraw.Draw(band)
    bd.polygon([(0, 470), (W * 2, 250), (W * 2, 400), (0, 620)], fill=70)
    band = band.rotate(-18, resample=Image.BICUBIC, center=(W, H))
    band = band.crop((W // 2, H // 2, W // 2 + W, H // 2 + H))
    band = band.filter(ImageFilter.GaussianBlur(26))
    img = Image.composite(Image.new("RGBA", (W, H), (188, 219, 248, 255)),
                          img, band)

    # the chrome frame around the tile
    frame = Image.new("L", (W, H), 0)
    ImageDraw.Draw(frame).rounded_rectangle([14, 14, W - 15, H - 15],
                                            radius=58, outline=255, width=9)
    return Image.alpha_composite(img, metal(frame, (W, H), gloss=0.5,
                                            shadow=(0, 0), shadow_alpha=0,
                                            bevel=2))


def stamp(img, mask, x, y, depth, **kw):
    """Place a metal shape at (x, y), with canvas room for its extrusion."""
    p = depth + 34
    big = Image.new("L", (mask.width + 2 * p, mask.height + 2 * p), 0)
    big.paste(mask, (p, p))
    layer = metal(big, big.size, depth=depth, **kw)
    img.alpha_composite(layer, (x - p, y - p))


def main():
    img = plate()

    # The emblem is the centerpiece: as wide as the frame comfortably allows,
    # with the two type lines pulled in tight above and below it. Everything is
    # extruded towards the viewer, so the plate stays behind the type.
    gap = 12
    d_u, d_e, d_m = 15, 19, 15

    um = fit_text("ULTIMATE", 372, 96, 0.07)
    em = ps4_emblem(2.2)
    em = em.crop(em.getbbox())
    mm = fit_text("MEDIA PLAYER", 344, 68, 0.10)

    total = (um.height + d_u + gap + em.height + d_e + gap
             + mm.height + d_m)
    top = (H - total) // 2 + 10

    stamp(img, um, W // 2 - um.width // 2, top, d_u,
          gloss=0.72, shine=0.42, shadow_alpha=180)
    top += um.height + d_u + gap
    stamp(img, em, W // 2 - em.width // 2, top, d_e,
          gloss=0.66, shine=0.38, shadow_alpha=180)
    top += em.height + d_e + gap
    stamp(img, mm, W // 2 - mm.width // 2, top, d_m,
          gloss=0.72, shine=0.42, shadow_alpha=180)

    out = os.path.join(ROOT, "sce_sys", "icon0.png")
    img.convert("RGB").save(out, "PNG")
    print("wrote", out, img.size)


if __name__ == "__main__":
    main()
