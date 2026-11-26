#!/usr/bin/env python3
"""Stage 2, part two: F4 particles, F5 parallax, F6 plasma on UI seams.

Reference look for the port into src/vis_art.c. Renders DANCE, WORM and PULSE
over their generated plates.
"""
import math, os, random
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

ROOT = "/workspace/chats/0c2b3a099dc1558e1719b468/files"
A = os.path.join(ROOT, "assets")
W, H = 1920, 1080
FONT = "/usr/local/nvm/versions/node/v26.13.1/lib/node_modules/openclaw/dist/assets/session-card/Lato-%s.ttf"
random.seed(7)


def f(sz, bold=False):
    return ImageFont.truetype(FONT % ("Bold" if bold else "Regular"), sz)


def plate(name):
    im = Image.open(os.path.join(A, name)).convert("RGB").resize((W, H), Image.LANCZOS)
    return Image.blend(im, Image.new("RGB", (W, H), (4, 6, 12)), 0.20)


def parallax(im, shift=18):
    """F5 — the plate split into a far and a near layer, offset against each other.

    The far layer is the whole plate, blurred and scaled up slightly; the near
    layer is a darkened copy of the lower third with its edges feathered. Moving
    them by different amounts is what reads as depth when the camera pans.
    """
    far = im.filter(ImageFilter.GaussianBlur(3))
    far = far.resize((int(W * 1.03), int(H * 1.03)), Image.LANCZOS).crop((0, 0, W, H))
    near = im.crop((0, int(H * 0.55), W, H)).point(lambda v: int(v * 0.82))
    near = near.resize((W, H - int(H * 0.55)), Image.LANCZOS)
    out = far.copy()
    ramp = Image.linear_gradient("L").resize((W, H - int(H * 0.55)))
    ramp = ramp.point(lambda v: min(255, int(v * 1.6)))
    out.paste(near, (shift, int(H * 0.55)), ramp)
    return out


def particles(im, count=140, region=(0.0, 1.0), size=1.5, tint=(150, 200, 255)):
    """F4 — depth-sorted points: near ones bigger, brighter and softer-edged."""
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    pts = []
    for _ in range(count):
        depth = random.random()
        x = random.uniform(0, W)
        y = random.uniform(H * region[0], H * region[1])
        pts.append((depth, x, y))
    for depth, x, y in sorted(pts, reverse=True):
        r = size * (0.4 + 1.6 * depth)
        a = int(35 + 130 * (1 - depth))
        d.ellipse([x - r, y - r, x + r, y + r],
                  fill=(tint[0] * a // 255, tint[1] * a // 255, tint[2] * a // 255))
    near = layer.filter(ImageFilter.GaussianBlur(7))
    return ImageChops.add(im, ImageChops.add(layer, near.point(lambda v: int(v * 0.7))))


def plasma(im, strength=1.0):
    """F6 — the icon's arc language, small and sparse, on the UI seams only."""
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    seams = (34, H - 34)                      # the two chrome rails of the frame
    for sy in seams:
        for i in range(4):
            x0 = random.uniform(160, W - 420)
            y = sy
            pts = [(x0, y)]
            for k in range(6):
                x0 += random.uniform(20, 40)
                y += random.uniform(-5, 5)    # stays on the seam
                pts.append((x0, y))
            d.line(pts, fill=(215, 238, 255), width=2, joint="curve")
            d.line(pts, fill=(80, 150, 250), width=4, joint="curve")
    core = ImageChops.add(im, layer.point(lambda v: int(min(255, v * strength))))
    halo = layer.filter(ImageFilter.GaussianBlur(22))
    return ImageChops.add(core, halo.point(lambda v: int(min(255, v * 0.9))))



def performer(im, sheet, cell, cx=0.5, base=0.94, height=0.62, gain=1.0, warm=False,
              trim_bottom=0.0):
    """A real performer from a generated pose sheet, keyed by its own light.

    The sheets are shot on pure black with a rim, so an additive composite leaves
    the background untouched and the body reads as lit by the world it stands in.
    Phase 1 of the motion: one pose. The port cross-fades adjacent cells on the
    beat; the cell index is the thing the audio drives at runtime.
    """
    sh = Image.open(os.path.join(A, sheet)).convert("RGB")
    if "solo" in sheet:                      # one figure, one frame, with headroom
        body = sh
    else:
        c = sh.size[0] // 3
        gutter = int(c * 0.05)
        r, k = divmod(cell % 9, 3)
        body = sh.crop((k * c + gutter, r * c + gutter,
                        (k + 1) * c - gutter, (r + 1) * c - gutter))
    if "chroma" in sheet or "solo" in sheet:                     # stay inside the cell, away from its edges
        m = int(body.size[0] * 0.03)
        body = body.crop((m, m, body.size[0] - m, body.size[1] - m))
    # chroma sheets: green is the matte, so dark clothing survives the key
    if "chroma" in sheet or "solo" in sheet:
        r_, g_, b_ = body.split()
        key = ImageChops.subtract(g_, ImageChops.lighter(r_, b_))
        matte = key.point(lambda v: 255 if v < 26 else max(0, 255 - int((v - 26) * 3.2)))
        matte = matte.filter(ImageFilter.MinFilter(5))     # erode the green halo
        matte = matte.filter(ImageFilter.GaussianBlur(1.1))
        # despill: green is a matte, never a colour in the shot
        r_, g_, b_ = body.split()
        g_clean = ImageChops.darker(g_, ImageChops.lighter(r_, b_).point(lambda v: int(v * 0.82)))
        body = Image.merge("RGB", (r_, g_clean, b_))
        body = Image.composite(body, Image.new("RGB", body.size, (0, 0, 0)), matte)
        body_alpha = matte
    tw = int(W * height * body.size[0] / body.size[1])
    th = int(H * height)
    if trim_bottom:
        body = body.crop((0, 0, body.size[0], int(body.size[1] * (1 - trim_bottom))))
    ba = body_alpha.resize((tw, th), Image.LANCZOS) if ("chroma" in sheet or "solo" in sheet) else None
    body_alpha_resized = ba
    body = body.resize((tw, th), Image.LANCZOS)
    # contact shadow: the body must sit on the floor, not hover over it
    sh_layer = Image.new("L", (W, H), 255)
    sd = ImageDraw.Draw(sh_layer)
    sw_, sh_ = int(tw * 0.62), int(th * 0.045)
    sd.ellipse([int(W * cx - sw_ / 2), int(H * base - sh_), int(W * cx + sw_ / 2), int(H * base + sh_)],
               fill=70)
    sh_layer = sh_layer.filter(ImageFilter.GaussianBlur(18))
    im = Image.composite(Image.new("RGB", (W, H), (0, 0, 0)), im, sh_layer.point(lambda v: 255 - v))
    im = ImageChops.multiply(im, Image.merge("RGB", (sh_layer, sh_layer, sh_layer)))
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    layer.paste(body, (int(W * cx - tw / 2), int(H * base - th)))
    layer = layer.point(lambda v: min(255, int(v * gain)))
    alpha = layer.convert("L").point(lambda v: 0 if v < 12 else min(255, int((v - 12) * 1.35)))
    alpha = alpha.filter(ImageFilter.GaussianBlur(1.2))
    if warm:
        layer = Image.merge("RGB", (layer.split()[0].point(lambda v: min(255, int(v * 1.05))),
                                    layer.split()[1], layer.split()[2]))
    halo = layer.filter(ImageFilter.GaussianBlur(30))
    if "chroma" in sheet or "solo" in sheet:
        am = Image.new("L", (W, H), 0)
        am.paste(body_alpha_resized, (int(W * cx - tw / 2), int(H * base - th)))
        im = Image.composite(layer, im, am)
    else:
        im = Image.composite(layer, im, alpha)                # solid body
    return ImageChops.add(im, halo.point(lambda v: int(v * 0.30)))

def dancer(im, cx=0.5, base=0.86, h=0.46):
    """A wireframe figure: torso, head, four limbs, additive with a soft core."""
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    X = W * cx
    B = H * base
    T = B - H * h
    hip = (X, T + H * h * 0.45)
    shoulders = (X, T + H * h * 0.14)
    d.line([shoulders, hip], fill=(90, 160, 230), width=16)
    d.line([shoulders, hip], fill=(150, 210, 255), width=9)
    d.line([shoulders, hip], fill=(235, 246, 255), width=4)
    hr = H * h * 0.075
    d.ellipse([shoulders[0] - hr, T - hr, shoulders[0] + hr, T + hr], fill=(24, 44, 76), outline=(205, 235, 255), width=5)
    for sgn in (-1, 1):
        elbow = (X + sgn * W * 0.085, T + H * h * 0.30)
        hand = (X + sgn * W * 0.135, T + H * h * 0.02)
        d.line([shoulders, elbow], fill=(85, 155, 225), width=13)
        d.line([shoulders, elbow], fill=(200, 232, 255), width=5)
        d.line([elbow, hand], fill=(85, 155, 225), width=11)
        d.line([elbow, hand], fill=(200, 232, 255), width=4)
        knee = (X + sgn * W * 0.055, T + H * h * 0.72)
        foot = (X + sgn * W * 0.095, B)
        d.line([hip, knee], fill=(85, 155, 225), width=15)
        d.line([hip, knee], fill=(200, 232, 255), width=5)
        d.line([knee, foot], fill=(85, 155, 225), width=12)
        d.line([knee, foot], fill=(200, 232, 255), width=4)
    core = ImageChops.add(im, layer)
    halo = layer.filter(ImageFilter.GaussianBlur(26))
    return ImageChops.add(core, halo.point(lambda v: int(v * 0.85)))


def worm(im, phase=0.0):
    """An undulating tube crossing the frame, lit from its own leading edge."""
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    pts = []
    for i in range(90):
        t = i / 89
        x = -100 + t * (W + 200)
        y = H * 0.55 + math.sin(t * 6.2 + phase) * H * 0.11 * (0.6 + 0.4 * t)
        pts.append((x, y))
    for wdt, col in ((46, (18, 34, 60)), (30, (40, 74, 120)), (14, (150, 205, 255)),
                     (6, (235, 246, 255))):
        d.line(pts, fill=col, width=wdt, joint="curve")
    core = ImageChops.add(im, layer)
    halo = layer.filter(ImageFilter.GaussianBlur(34))
    return ImageChops.add(core, halo.point(lambda v: int(v * 0.9)))


def pulse(im, rings=7):
    layer = Image.new("RGB", (W, H), (0, 0, 0))
    d = ImageDraw.Draw(layer)
    cx, cy = W / 2, H * 0.5
    for i in range(rings):
        r = 90 + i * 76
        a = int(230 * (1 - i / (rings + 1)))
        d.ellipse([cx - r, cy - r * 0.62, cx + r, cy + r * 0.62],
                  outline=(a // 3, a // 2, a), width=max(2, 9 - i))
    d.ellipse([cx - 54, cy - 54, cx + 54, cy + 54], fill=(210, 235, 255))
    core = ImageChops.add(im, layer)
    halo = layer.filter(ImageFilter.GaussianBlur(30))
    return ImageChops.add(core, halo.point(lambda v: int(v * 0.95)))


def vignette_grain(im, vig=0.35, grain=6):
    mask = Image.new("L", (W, H), 255)
    md = ImageDraw.Draw(mask)
    for i in range(40):
        t = i / 39
        md.ellipse([-W * 0.35 * (1 - t), -H * 0.2 * (1 - t),
                    W + W * 0.35 * (1 - t), H + H * 0.2 * (1 - t)],
                   outline=int(255 - vig * 255 * (1 - t)))
    mask = mask.filter(ImageFilter.GaussianBlur(120))
    im = Image.composite(im, Image.new("RGB", (W, H), (0, 0, 0)), mask)
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


def build(name, title, sub, fn):
    im = parallax(plate(name))
    im = fn(im)
    im = plasma(im)
    im = vignette_grain(im)
    hud(im, title, "Blue in Green  ·  Miles Davis", "2:14 / 5:37",
        [("O", "BACK"), ("T", "STYLE"), ("\u25a1", "NEXT")])
    out = os.path.join(ROOT, "mockups/09-%s.png" % title.lower())
    im.save(out)
    im.resize((960, 540), Image.LANCZOS).save(out.replace(".png", "-small.png"))
    print("wrote", os.path.basename(out))


build("vis-dance.jpg", "DANCE", "club stage",
      lambda im: particles(performer(im, "dancers-a.jpg", 1, cx=0.5, base=0.83,
                                     height=0.44, gain=1.1), count=150,
                           region=(0.5, 1.0), size=1.6))
build("vis-dance2.jpg", "DANCE 2", "basement lab",
      lambda im: particles(performer(im, "dancers-b.jpg", 6, cx=0.5, base=0.92,
                                     height=0.46, gain=1.05, warm=True), count=120,
                           region=(0.55, 1.0), size=1.4))
build("vis-dance3.jpg", "DANCE 3", "theatre proscenium",
      lambda im: particles(performer(im, "soloist-solo.jpg", 0, cx=0.5, base=0.95,
                                     height=0.62, gain=1.06, warm=True), count=110,
                           region=(0.55, 1.0), size=1.3))
build("vis-rave.jpg", "RAVE DJ", "booth from behind",
      lambda im: particles(performer(im, "dj-solo.jpg", 0, cx=0.5, base=0.94,
                                     height=0.66, gain=1.0, trim_bottom=0.0), count=160,
                           region=(0.2, 0.9), size=1.5))
build("vis-worm.jpg", "WORM", "deep trench",
      lambda im: particles(worm(im), count=230, region=(0.0, 1.0), size=1.8, tint=(170, 215, 255)))
build("vis-pulse.jpg", "PULSE", "flooded tunnel",
      lambda im: particles(pulse(im), count=120, region=(0.45, 1.0), size=1.5))
