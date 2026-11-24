#!/usr/bin/env python3
"""Renders every look and lays the results out as one contact sheet."""

import os
import subprocess
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PREV = os.path.join(ROOT, "preview")
OUT = os.path.join(os.path.dirname(ROOT), "v84-sheet.png")

NAMES = ["SPECTRUM", "WAVE", "RADIAL", "PULSE", "MIRROR", "BLOCKS",
         "SCOPE", "ORBIT", "LIQUID", "NEON", "FLAG", "DANCE",
         "WORM", "WINDMILL", "DANCE 2", "DANCE 3", "RAVE", "TESSERACT"]

env = dict(os.environ)
env["SDL_VIDEODRIVER"] = "dummy"
home = os.path.expanduser("~")
lib = home + "/hostdeps/root/usr/lib/x86_64-linux-gnu"
env["LD_LIBRARY_PATH"] = lib + ":" + lib + "/pulseaudio"

CW, CH = 560, 193
sheet = Image.new("RGB", (CW * 3, (CH + 18) * 6), (6, 7, 12))
draw = ImageDraw.Draw(sheet)

for i, name in enumerate(NAMES):
    subprocess.run(["./build/preview", "one", "13", str(i)], cwd=PREV, env=env,
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    im = Image.open(os.path.join(PREV, "one-shot.bmp")).convert("RGB")
    im = im.crop((120, 250, 1800, 830)).resize((CW, CH), Image.LANCZOS)
    cx, cy = (i % 3) * CW, (i // 3) * (CH + 18)
    sheet.paste(im, (cx, cy))
    draw.text((cx + 6, cy + CH + 3), name, fill=(150, 180, 210))

sheet.save(OUT)
print("wrote", OUT)
