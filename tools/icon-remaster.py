#!/usr/bin/env python3
"""Composite the icon remaster: generated vault frame + the project's own emblem + type."""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = "/workspace/chats/0c2b3a099dc1558e1719b468/files"
FRAME = os.path.join(ROOT, "assets/icon-frame.jpg")
EMBLEM = os.path.join(ROOT, "ps4-media-player/src/emblem.png")
FONT = "/usr/local/nvm/versions/node/v26.13.1/lib/node_modules/openclaw/dist/assets/session-card/Lato-%s.ttf"
S = 1024

def spaced(d, xy, text, font, fill, tracking=14, anchor="mm"):
    widths = [d.textlength(c, font=font) for c in text]
    total = sum(widths) + tracking * (len(text) - 1)
    x = xy[0] - total / 2
    for c, w in zip(text, widths):
        d.text((x, xy[1]), c, font=font, fill=fill, anchor="lm")
        x += w + tracking

def build():
    base = Image.open(FRAME).convert("RGB").resize((S, S), Image.LANCZOS)
    d = ImageDraw.Draw(base)
    em = Image.open(EMBLEM).convert("RGBA")
    ew = 520
    em = em.resize((ew, int(em.size[1] * ew / em.size[0])), Image.LANCZOS)
    # soft glow behind the emblem
    glow = Image.new("RGBA", base.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([S/2 - 340, S/2 - 150, S/2 + 340, S/2 + 150], fill=(40, 120, 220, 70))
    base = Image.alpha_composite(base.convert("RGBA"), glow.filter(ImageFilter.GaussianBlur(60)))
    base.paste(em, (int(S/2 - ew/2), int(S/2 - em.size[1]/2)), em)
    d = ImageDraw.Draw(base)
    big = ImageFont.truetype(FONT % "Bold", 78)
    small = ImageFont.truetype(FONT % "Bold", 52)
    spaced(d, (S/2, 268), "ULTIMATE", big, (238, 242, 250), 10)
    spaced(d, (S/2, 772), "MEDIA PLAYER", small, (210, 226, 246), 7)
    return base.convert("RGB")

def main():
    icon = build()
    out = os.path.join(ROOT, "ps4-media-player/design")
    icon.save(os.path.join(out, "icon-remaster-1024.png"))
    for px in (512, 160):
        icon.resize((px, px), Image.LANCZOS).save(os.path.join(out, "icon-remaster-%d.png" % px))
    # comparison sheet against the current master, at 512 and 160
    cur = Image.open(os.path.join(out, "icon0-master.png")).convert("RGB")
    sheet = Image.new("RGB", (512*2 + 120, 512 + 260), (10, 11, 16))
    sheet.paste(cur.resize((512, 512), Image.LANCZOS), (40, 40))
    sheet.paste(icon.resize((512, 512), Image.LANCZOS), (512 + 80, 40))
    sheet.paste(cur.resize((160, 160), Image.LANCZOS), (200, 600))
    sheet.paste(icon.resize((160, 160), Image.LANCZOS), (700, 600))
    sd = ImageDraw.Draw(sheet)
    f = ImageFont.truetype(FONT % "Bold", 26)
    sd.text((40, 566), "CURRENT", font=f, fill=(150, 160, 180))
    sd.text((512 + 80, 566), "REMASTER", font=f, fill=(210, 226, 246))
    sheet.save(os.path.join(ROOT, "mockups/icon-compare.png"))
    print("ok")

main()
