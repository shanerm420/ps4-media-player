#!/usr/bin/env python3
"""Icon remaster, second direction: structure and detail come from the v100
design (drawn here), the generated plate supplies material and light only."""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter, ImageChops

ROOT = "/workspace/chats/0c2b3a099dc1558e1719b468/files"
PLATE = os.path.join(ROOT, "assets/icon-frame.jpg")
EMBLEM = os.path.join(ROOT, "ps4-media-player/src/emblem.png")
FONT = "/usr/local/nvm/versions/node/v26.13.1/lib/node_modules/openclaw/dist/assets/session-card/Lato-%s.ttf"
S = 1024


def chrome_type(img, xy, text, size, tracking, top=True):
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT % "Bold", size)
    widths = [d.textlength(c, font=font) for c in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x = xy[0] - total / 2
    pts = []
    for c, w in zip(text, widths):
        pts.append((x, c, w))
        x += w + tracking
    # deep shadow, then body, then top bevel light, then blue rim underneath
    for px, c, w in pts:
        d.text((px + 2, xy[1] + 5), c, font=font, fill=(4, 8, 18))
    for px, c, w in pts:
        d.text((px, xy[1]), c, font=font, fill=(150, 165, 188))
    for px, c, w in pts:
        d.text((px, xy[1] - 3), c, font=font, fill=(233, 240, 252))
    mask = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(mask)
    for px, c, w in pts:
        md.text((px, xy[1] - 3), c, font=font, fill=255)
    # blue underglow spilling from the letters
    glow = Image.new("RGB", img.size, (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for px, c, w in pts:
        gd.text((px, xy[1] - 2), c, font=font, fill=(30, 90, 200))
    glow = glow.filter(ImageFilter.GaussianBlur(9))
    img.paste(ImageChops.add(img, glow), (0, 0))
    return img


def build():
    img = Image.new("RGB", (S, S), (5, 7, 14))
    d = ImageDraw.Draw(img)
    # 1. the lit interior: deep blue volume, brighter toward the middle-top
    for r in range(760, 0, -8):
        t = r / 760
        col = (int(8 + 26 * (1 - t)), int(14 + 46 * (1 - t)), int(30 + 88 * (1 - t)))
        d.ellipse([S/2 - r, S/2 - r * 0.95, S/2 + r, S/2 + r * 0.95], fill=col)
    # 2. material only: the plate, desaturated and multiplied in at low weight
    if os.path.exists(PLATE):
        plate = Image.open(PLATE).convert("L").resize((S, S), Image.LANCZOS)
        plate = plate.filter(ImageFilter.GaussianBlur(1))
        mat = Image.merge("RGB", (plate, plate, plate)).point(lambda v: int(v * 0.55))
        img = Image.blend(img, ImageChops.multiply(img, mat), 0.55)
    d = ImageDraw.Draw(img)

    # 3. chamfered hex frame, chrome, with a lit inner edge
    m, c = 54, 120                      # margin, chamfer
    outer = [(m + c, m), (S - m - c, m), (S - m, m + c), (S - m, S - m - c),
             (S - m - c, S - m), (m + c, S - m), (m, S - m - c), (m, m + c)]
    d.polygon(outer, outline=None, fill=None)
    for k, col in ((10, (0, 0, 0)), (7, (26, 34, 48)), (4, (96, 112, 132)),
                   (2, (198, 212, 232)), (0, (120, 140, 168))):
        d.line(outer + [outer[0]], fill=col, width=(6 if k else 4), joint="curve")
    inner = [(x + 26 if x < S/2 else x - 26, y + 26 if y < S/2 else y - 26) for x, y in outer]
    d.line(inner + [inner[0]], fill=(10, 16, 30), width=14, joint="curve")

    # 4. neon seams across the top and bottom rails, and light bleeding in
    for y, a in ((m + 40, 255), (S - m - 40, 255)):
        seam = Image.new("RGB", (S, S), (0, 0, 0))
        sd = ImageDraw.Draw(seam)
        sd.rectangle([m + 46, y - 4, S - m - 46, y + 4], fill=(70, 175, 255))
        img.paste(ImageChops.add(img, seam), (0, 0))
        halo = seam.filter(ImageFilter.GaussianBlur(26))
        img.paste(ImageChops.add(img, halo.point(lambda v: int(v * 0.8))), (0, 0))

    # 5. the emblem, large, on its own dark plate with a blue border
    em = Image.open(EMBLEM).convert("RGBA")
    ew = 640
    em = em.resize((ew, int(em.size[1] * ew / em.size[0])), Image.LANCZOS)
    ex, ey = int(S/2 - ew/2), int(S/2 - em.size[1]/2)
    plate_box = [ex - 46, ey - 34, ex + ew + 46, ey + em.size[1] + 34]
    d.rounded_rectangle(plate_box, radius=22, fill=(9, 14, 26), outline=(48, 120, 220), width=3)
    d.rounded_rectangle([plate_box[0] + 8, plate_box[1] + 8, plate_box[2] - 8, plate_box[1] + 11],
                        radius=2, fill=(70, 130, 220))
    img.paste(em, (ex, ey), em)

    # 6. the wordmark above and below, chrome, as the original had them
    img = chrome_type(img, (S/2, 214), "ULTIMATE", 84, 12)
    img = chrome_type(img, (S/2, 742), "MEDIA PLAYER", 54, 9)
    return img


def main():
    icon = build()
    out = os.path.join(ROOT, "ps4-media-player/design")
    icon.save(os.path.join(out, "icon-v2-1024.png"))
    for px in (512, 160):
        icon.resize((px, px), Image.LANCZOS).save(os.path.join(out, "icon-v2-%d.png" % px))
    print("ok")


main()
