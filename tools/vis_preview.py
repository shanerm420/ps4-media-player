#!/usr/bin/env python3
"""Reference implementation of the Stage-2 effects: plate + motion + light.

Each effect here is the thing that gets ported into src/vis_art.c once it looks
right on the host. Nothing here ships; it is the look and the maths.
"""
import math, os
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

ROOT = "/workspace/chats/0c2b3a099dc1558e1719b468/files"
A = os.path.join(ROOT, "assets")
W, H = 1920, 1080
FONT = "/usr/local/nvm/versions/node/v26.13.1/lib/node_modules/openclaw/dist/assets/session-card/Lato-%s.ttf"


def f(sz, bold=False):
    return ImageFont.truetype(FONT % ("Bold" if bold else "Regular"), sz)


def plate(name):
    im = Image.open(os.path.join(A, name)).convert("RGB").resize((W, H), Image.LANCZOS)
    return Image.blend(im, Image.new("RGB", (W, H), (4, 6, 12)), 0.22)


def reflection(im, waterline=0.62, strength=0.45):
    """F3 — vertical flip below the waterline, ripple warped, faded with depth."""
    y = int(H * waterline)
    src = im.crop((0, 0, W, y)).transpose(Image.FLIP_TOP_BOTTOM).resize((W, H - y), Image.LANCZOS)
    out = im.copy()
    for yy in range(H - y):
        off = int(4 * math.sin(yy * 0.045) + 2 * math.sin(yy * 0.16))
        out.paste(src.crop((0, yy, W, yy + 1)), (off, y + yy))
    out = Image.blend(im, out, strength)
    return out


def cone(im, cx=0.5, top=0, bottom=0.72, spread=0.26, strength=0.5):
    """F2 — volumetric spotlight cone, additive, soft edged, noisy."""
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    x0, y1 = int(W * cx), int(H * bottom)
    for i in range(60):
        t = i / 59
        yy = int(y1 * t)
        half = int(W * spread * (0.18 + t))
        a = int(255 * (1 - t) ** 1.4)
        d.rectangle([x0 - half, yy, x0 + half, yy + y1 // 59 + 2], fill=(a // 8, a // 4, a // 2))
    layer = layer.filter(ImageFilter.GaussianBlur(28))
    return ImageChops.add(im, layer.point(lambda v: int(v * strength)))


def vignette_grain(im, vig=0.35, grain=6):
    """F7 — matched vignette and fine grain."""
    mask = Image.new("L", (W, H), 255)
    md = ImageDraw.Draw(mask)
    for i in range(40):
        t = i / 39
        md.ellipse([-W * 0.35 * (1 - t) + 0, -H * 0.2 * (1 - t),
                    W + W * 0.35 * (1 - t), H + H * 0.2 * (1 - t)],
                   outline=int(255 - vig * 255 * (1 - t)))
    mask = mask.filter(ImageFilter.GaussianBlur(120))
    im = Image.composite(im, Image.new("RGB", (W, H), (0, 0, 0)), mask)
    # fine grain only: sigma scaled down, applied as an offset around mid grey
    n = Image.effect_noise((W, H), grain).convert("L")
    n = n.point(lambda v: int(128 + (v - 128) * 0.22))
    return ImageChops.add(im, Image.merge("RGB", (n, n, n)), 1.0, -128)


def hud(im, title, sub, right, keys):
    d = ImageDraw.Draw(im)
    d.text((96, 74), title, font=f(44, True), fill=(240, 244, 252))
    d.text((96, 132), sub, font=f(26), fill=(150, 168, 196))
    d.text((W - 96, 88), right, font=f(26), fill=(150, 168, 196), anchor="ra")
    for i, (k, lab) in enumerate(keys):
        cx = W - 96 - (len(keys) - 1 - i) * 190
        d.ellipse([cx - 22, H - 118, cx + 22, H - 74], outline=(190, 205, 225), width=2)
        d.text((cx, H - 96), k, font=f(18, True), fill=(235, 240, 250), anchor="mm")
        d.text((cx, H - 52), lab, font=f(17), fill=(150, 168, 196), anchor="mm")


def spectrum_bars(im):
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    n, base = 64, 812
    slot = W / n
    for i in range(n):
        t = i / (n - 1)
        env = math.exp(-((t - 0.5) ** 2) / 0.055) * 0.85 + 0.15
        hgt = int((60 + 300 * env) * (0.55 + 0.45 * math.sin(i * 1.7) ** 2))
        x = i * slot + slot * 0.22
        col = (int(60 + 170 * (1 - t)), int(120 + 90 * (1 - abs(t - 0.5) * 2)), 235)
        d.rectangle([x, base - hgt, x + slot * 0.56, base], fill=col)
    im = ImageChops.add(im, layer.filter(ImageFilter.GaussianBlur(26)).point(lambda v: int(v * 0.95)))
    im = ImageChops.add(im, layer)
    d = ImageDraw.Draw(im)
    d.rectangle([0, base + 26, W, base + 27], fill=(90, 150, 235))
    return im


def mirror_rings(im):
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(9):
        r = 120 + i * 78
        a = int(210 * (1 - i / 10))
        d.arc([W/2 - r, H*0.42 - r * 0.55, W/2 + r, H*0.42 + r * 0.55],
              200, 340, fill=(a // 3, a // 2, a), width=6)
    return ImageChops.add(im, layer.filter(ImageFilter.GaussianBlur(18)))


def still_face(im):
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    pts = []
    for i in range(220):
        x = 200 + i * 7
        y = H * 0.44 + 120 * math.sin(i * 0.14) * math.exp(-i / 400)
        pts.append((x, y))
    d.line(pts, fill=(120, 200, 255), width=5, joint="curve")
    return ImageChops.add(im, layer.filter(ImageFilter.GaussianBlur(14)).point(lambda v: int(v * 0.8)))


JOBS = [
    ("vis-spectrum.jpg", "SPECTRUM", "prism on a night sea", spectrum_bars, True, 0.66),
    ("vis-mirror.jpg", "MIRROR", "flooded colonnade", mirror_rings, True, 0.60),
    ("vis-flag.jpg", "FLAG", "backlit silk in a spotlight", None, False, 0.72),
    ("vis-scope.jpg", "SCOPE", "phosphor tube", still_face, False, 0.70),
]

for name, title, sub, motion, refl, wl in JOBS:
    im = plate(name)
    if refl:
        im = reflection(im, waterline=wl, strength=0.5)
    if title == "FLAG":
        im = cone(im, cx=0.5, bottom=0.74, spread=0.22, strength=0.85)
    if motion:
        im = motion(im)
    im = vignette_grain(im)
    hud(im, title, "Blue in Green  ·  Miles Davis", "2:14 / 5:37",
        [("O", "BACK"), ("T", "STYLE"), ("\u25a1", "NEXT")])
    out = os.path.join(ROOT, "mockups/08-vis-%s.png" % title.lower().replace(" ", "-"))
    im.save(out)
    print("wrote", os.path.basename(out))
