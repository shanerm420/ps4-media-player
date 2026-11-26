#!/usr/bin/env python3
"""Render the 1080p mockups from the tokens in docs/DESIGN-PROCESS.md."""
import os
from PIL import Image, ImageDraw, ImageFont, ImageChops, ImageFilter

W, H = 1920, 1080
SAFE, GUT = 48, 32
ACCENT = (58, 160, 255)
WHITE = (238, 242, 250)
DIM = (150, 160, 180)
FAINT = (90, 98, 118)
SLAB = (22, 26, 38)
SLAB2 = (30, 35, 50)
FONT = "/usr/local/nvm/versions/node/v26.13.1/lib/node_modules/openclaw/dist/assets/session-card/Lato-%s.ttf"
def f(size, bold=False):
    return ImageFont.truetype(FONT % ("Bold" if bold else "Regular"), size)

def bg():
    im = Image.new("RGB", (W, H), (7, 8, 14))
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / H
        d.line([(0, y), (W, y)], fill=(int(6 + 10 * t), int(7 + 12 * t), int(13 + 24 * t)))
    # accent bloom top-left
    for r in range(900, 0, -30):
        a = int(10 * (1 - r / 900))
        d.ellipse([260 - r, 60 - r, 260 + r, 60 + r], outline=None,
                  fill=(6 + a, 8 + a * 2, 14 + a * 3))
    return im

def cap(d, x, y, s, size=22, col=DIM, bold=False):
    d.text((x, y), " ".join(s.upper()), font=f(size, bold), fill=col)

def slab(d, box, r=16, fill=SLAB, edge=None, focus=False):
    d.rounded_rectangle(box, radius=r, fill=fill)
    if edge:
        d.rounded_rectangle(box, radius=r, outline=edge, width=2)
    if focus:
        d.rounded_rectangle(box, radius=r, outline=WHITE, width=3)
        x0, y0, x1, y1 = box
        for k in range(1, 14):
            d.rounded_rectangle([x0 - k, y0 - k, x1 + k, y1 + k], radius=r + k,
                                outline=(30 + 8 * (14 - k), 60 + 9 * (14 - k), 110 + 9 * (14 - k)))

def buttons(d, items, y=H - 128):
    """The Pictures-screen button treatment: 48 px circle, label beneath, 104 px slot."""
    n = len(items)
    total = n * 104 + (n - 1) * 24
    x = (W - total) // 2
    for i, (glyph, label) in enumerate(items):
        cx = x + i * 128 + 24
        focus = i == 0
        d.ellipse([cx - 24, y - 24, cx + 24, y + 24],
                  outline=WHITE if focus else DIM, width=2,
                  fill=(ACCENT[0] // 6, ACCENT[1] // 6, ACCENT[2] // 6) if focus else None)
        d.text((cx, y), glyph, font=f(20, True), fill=WHITE, anchor="mm")
        d.text((cx, y + 40), label, font=f(18), fill=WHITE if focus else DIM, anchor="ma")

def header(d, title, sub=""):
    cap(d, SAFE, SAFE, title, 44, WHITE, True)
    if sub:
        cap(d, SAFE, SAFE + 56, sub, 18, FAINT)
    d.line([(SAFE, SAFE + 96), (W - SAFE, SAFE + 96)], fill=(40, 46, 62), width=2)

def tile(d, x, y, w, h, hue, label, sub="", focus=False):
    slab(d, [x, y, x + w, y + h], 16, SLAB, focus=focus)
    inner = [x + 10, y + 10, x + w - 10, y + h - 68]
    for k in range(28):
        t = k / 27
        col = (int(hue[0] * (0.55 + 1.15 * t)), int(hue[1] * (0.50 + 1.00 * t)),
               int(hue[2] * (0.60 + 1.05 * t)))
        d.rectangle([inner[0] + (inner[2] - inner[0]) * t, inner[1],
                     inner[0] + (inner[2] - inner[0]) * (t + 1 / 28), inner[3]], fill=col)
    d.text((x + 14, y + h - 54), label, font=f(20, True), fill=WHITE)
    if sub:
        d.text((x + 14, y + h - 30), sub, font=f(15), fill=DIM)

def screen_home():
    im = bg(); d = ImageDraw.Draw(im)
    cap(d, SAFE, 64, "ultimate", 92, WHITE, True)
    cap(d, SAFE, 176, "media player", 26, ACCENT, True)
    doors = [("PICTURES", (26, 42, 62), "1,284 items"), ("MUSIC", (52, 28, 70), "2,921 tracks"),
             ("VIDEOS", (18, 52, 64), "312 files"), ("FILES", (30, 34, 60), "3 drives"),
             ("SETTINGS", (34, 34, 44), "v125"), ("DEDICATION", (44, 26, 46), "credits")]
    w, gap = 252, 24
    x0 = (W - (6 * w + 5 * gap)) // 2
    for i, (name, hue, sub) in enumerate(doors):
        tile(d, x0 + i * (w + gap), 300 if i else 284, w, 380, hue, name, sub, focus=(i == 0))
    d.text((SAFE, 726), "Pictures", font=f(34, True), fill=WHITE)
    d.text((SAFE, 772), "1,284 photos across three drives \u00b7 newest first", font=f(22), fill=DIM)
    buttons(d, [("X", "OPEN"), ("O", "BACK"), ("T", "OPTIONS"), ("L1", "SECTIONS")])
    return im

def screen_pictures():
    im = bg(); d = ImageDraw.Draw(im)
    header(d, "PICTURES", "hdd:/data/Pictures  ·  1,284 items")
    hues = [(20, 40, 70), (60, 30, 40), (20, 55, 50), (50, 40, 70), (25, 45, 65)]
    w, h, gap = 340, 220, 24
    for r in range(3):
        for c in range(5):
            i = r * 5 + c
            tile(d, SAFE + c * (w + gap), 180 + r * (h + 44), w, h,
                 hues[i % 5], "IMG_%04d.JPG" % (2041 + i), "%d × %d" % (4000, 3000),
                 focus=(i == 0))
    buttons(d, [("X", "OPEN"), ("O", "BACK"), ("T", "INFO"), ("L2", "ZOOM"), ("\u25a1", "SLIDESHOW")])
    return im

def screen_music():
    im = bg(); d = ImageDraw.Draw(im)
    header(d, "MUSIC")
    tabs = ["FOLDER", "ARTIST", "ALBUM", "PLAYLIST"]
    x = SAFE
    for i, t in enumerate(tabs):
        col = WHITE if i == 0 else FAINT
        cap(d, x, 168, t, 20, col, i == 0)
        if i == 0:
            d.line([(x, 196), (x + 96, 196)], fill=ACCENT, width=3)
        x += 160
    tracks = [("Organ Suite in D minor", "Basilica", "5:11"), ("Lil Big Pac", "Kodak Black", "3:04"),
              ("Wet Riding", "POV Amateur", "4:38"), ("Nocturne Op.9 No.2", "Chopin", "4:33"),
              ("Redbone", "Childish Gambino", "5:26"), ("Blue in Green", "Miles Davis", "5:37")]
    for i, (title, artist, dur) in enumerate(tracks):
        y = 220 + i * 96
        focus = i == 0
        slab(d, [SAFE, y, W - SAFE, y + 84], 8, SLAB2 if focus else SLAB, focus=focus)
        album_cell(im, [SAFE + 12, y + 12, SAFE + 72, y + 72], i)
        d.text((SAFE + 92, y + 20), title, font=f(26, True), fill=WHITE)
        d.text((SAFE + 92, y + 50), artist, font=f(18), fill=DIM)
        d.text((W - SAFE - 24, y + 30), dur, font=f(18), fill=DIM, anchor="ra")
    buttons(d, [("X", "PLAY"), ("O", "BACK"), ("T", "QUEUE"), ("\u25a1", "VISUAL")])
    return im

def screen_videos():
    im = bg(); d = ImageDraw.Draw(im)
    header(d, "VIDEOS", "hdd:/data/Video  ·  312 files")
    cap(d, SAFE, 168, "continue", 20, FAINT, True)
    for i in range(3):
        slab(d, [SAFE + i * 380, 200, SAFE + i * 380 + 356, 320], 16, SLAB)
        _stills(im, [SAFE + 10 + i * 380, 210, SAFE + 346 + i * 380, 270], i + 3)
        d.rectangle([SAFE + 10 + i * 380, 278, SAFE + 10 + i * 380 + 200, 284], fill=ACCENT)
        d.text((SAFE + 18 + i * 380, 292), ["Doc.S01E02.mkv", "Concert 2019.mp4", "Home.Movie.mkv"][i],
               font=f(18, True), fill=WHITE)
    for i in range(4):
        y = 360 + i * 132
        slab(d, [SAFE, y, W - SAFE, y + 116], 8, SLAB2 if i == 0 else SLAB, focus=(i == 0))
        _stills(im, [SAFE + 12, y + 12, SAFE + 176, y + 104], i + 3)
        d.text((SAFE + 200, y + 24), ["XxX.mp4", "Road.Trip.mkv", "Live.Set.mp4", "Family.2018.mkv"][i],
               font=f(26, True), fill=WHITE)
        d.text((SAFE + 200, y + 58), ["1080p · 42:11 · resumed 12:04", "720p · 1:48:20",
                                      "1080p · 58:02", "1080p · 22:40"][i], font=f(18), fill=DIM)
    buttons(d, [("X", "PLAY"), ("O", "BACK"), ("T", "OPTIONS"), ("R2", "SEEK")])
    return im

def screen_files():
    im = bg(); d = ImageDraw.Draw(im)
    header(d, "FILES", "copy 2 of 3  ·  412 MB/s  ·  38%")
    d.rectangle([SAFE, 172, W - SAFE, 180], fill=(34, 40, 56))
    d.rectangle([SAFE, 172, SAFE + int((W - 2 * SAFE) * 0.38), 180], fill=ACCENT)
    panes = [(SAFE, 660), (SAFE + 700, W - SAFE)]
    names = [["hdd:/", "usb0:/", "usb1:/", "/data", "/data/Pictures", "/data/Music"],
             ["..", "Doc.S01E02.mkv", "Concert 2019.mp4", "Home.Movie.mkv", "notes.txt"]]
    for p, (x0, x1) in enumerate(panes):
        slab(d, [x0, 210, x1, 860], 16, SLAB)
        cap(d, x0 + 20, 226, "locations" if p == 0 else "contents", 18, FAINT, True)
        for i, n in enumerate(names[p]):
            y = 262 + i * 92
            focus = (p == 1 and i == 1)
            if focus:
                slab(d, [x0 + 12, y, x1 - 12, y + 76], 8, SLAB2, focus=True)
            d.text((x0 + 28, y + 18), n, font=f(24, True), fill=WHITE)
            if p == 1 and i:
                d.text((x1 - 28, y + 24), ["", "1.4 GB", "980 MB", "2.1 GB", "2 KB"][i],
                       font=f(17), fill=DIM, anchor="ra")
    buttons(d, [("X", "OPEN"), ("O", "BACK"), ("\u25a1", "SELECT"), ("T", "ACTIONS")])
    return im

def screen_settings():
    im = bg(); d = ImageDraw.Draw(im)
    header(d, "SETTINGS")
    rows = [("Safe area", "48 px"), ("Theme", "Vault (default)"), ("Effects quality", "High"),
            ("Audio output", "HDMI · stereo"), ("Resume playback", "On"),
            ("Scroll speed", "Normal"), ("Language", "English"), ("Storage", "3 drives · 1.2 TB free"),
            ("About", "v125"), ("Reset", "Settings only")]
    for i, (k, v) in enumerate(rows):
        y = 176 + i * 86
        focus = i == 3
        slab(d, [SAFE, y, W - SAFE, y + 72], 8, SLAB2 if focus else SLAB, focus=focus)
        d.text((SAFE + 24, y + 20), k, font=f(24, True), fill=WHITE)
        d.text((W - SAFE - 24, y + 24), v, font=f(22), fill=ACCENT if focus else DIM, anchor="ra")
    buttons(d, [("X", "CHANGE"), ("O", "BACK"), ("T", "RESET")])
    return im

def screen_dedication():
    im = bg(); d = ImageDraw.Draw(im)
    header(d, "DEDICATION", "everything that went into this")
    cols = [("LIBRARIES", ["SDL2 · znullptr PS4 port", "minimp3", "stb_image", "font8x8", "Inter"]),
            ("TOOLS", ["OpenOrbis PS4Toolchain", "clang 18.1.8", "Piglet / GLES2", "GoldHEN"]),
            ("THANKS", ["Shane — design and direction", "flatz — Piglet research",
                        "the PS4 homebrew scene"])]
    for c, (head, items) in enumerate(cols):
        x = SAFE + c * 610
        cap(d, x, 200, head, 20, ACCENT, True)
        for i, it in enumerate(items):
            d.text((x, 244 + i * 54), it, font=f(22), fill=WHITE)
    slab(d, [SAFE, 620, W - SAFE, 780], 16, SLAB)
    cap(d, SAFE + 28, 644, "github", 20, FAINT, True)
    d.text((SAFE + 28, 682), "github.com/shanerm420/ps4-media-player", font=f(34, True), fill=WHITE)
    d.text((SAFE + 28, 730), "source, docs, manifest, tools and every package", font=f(20), fill=DIM)
    buttons(d, [("O", "BACK"), ("T", "COPY LINK")])
    return im


ASSETS = "/workspace/chats/0c2b3a099dc1558e1719b468/files/assets"
ART = {"PICTURES": "door-pictures", "MUSIC": "door-music", "VIDEOS": "door-videos",
       "FILES": "door-files", "SETTINGS": "door-settings", "DEDICATION": "door-dedication"}

def _cover(src_path, box):
    im = Image.open(src_path).convert("RGB")
    bw, bh = box[2] - box[0], box[3] - box[1]
    sw, sh = im.size
    s = max(bw / sw, bh / sh)
    im = im.resize((max(1, int(sw * s)), max(1, int(sh * s))), Image.LANCZOS)
    ox = (im.size[0] - bw) // 2
    oy = (im.size[1] - bh) // 3
    return im.crop((ox, oy, ox + bw, oy + bh))

def bg():
    from PIL import ImageFilter
    im = Image.new("RGB", (W, H), (7, 8, 14))
    path = os.path.join(ASSETS, "room-floor.jpg")
    if os.path.exists(path):
        plate = Image.open(path).convert("RGB").resize((W, H), Image.LANCZOS)
        plate = plate.filter(ImageFilter.GaussianBlur(2))
        im.paste(plate, (0, 0))
        veil = Image.new("RGB", (W, H), (5, 7, 13))
        im = Image.blend(im, veil, 0.45)
        hp = os.path.join(ASSETS, "haze.jpg")
        if os.path.exists(hp):
            hz = Image.open(hp).convert("RGB").resize((W, H), Image.LANCZOS)
            hz = hz.filter(ImageFilter.GaussianBlur(24))
            im = Image.blend(im, ImageChops.add(im, hz.point(lambda v: int(v * 0.35))), 0.5)
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = max(0.0, (y - H * 0.55) / (H * 0.45))
        d.line([(0, y), (W, y)], fill=(int(6 + 8 * t), int(7 + 10 * t), int(13 + 20 * t))) if y > H * 0.55 else None
    return im

def tile(d_in, x, y, w, h, hue, label, sub="", focus=False):
    d = d_in
    slab(d, [x, y, x + w, y + h], 16, SLAB, focus=focus)
    inner = [x + 10, y + 10, x + w - 10, y + h - 68]
    path = os.path.join(ASSETS, ART.get(label.strip().upper(), "") + ".jpg")
    img = d._image
    if os.path.exists(path):
        img.paste(_cover(path, inner), (inner[0], inner[1]))
        # caption band and a lift off the art
        d.rectangle([inner[0], inner[3] - 52, inner[2], inner[3]], fill=(12, 15, 24))
        for k in range(24):
            a = 1 - k / 24
            d.rectangle([inner[0], inner[3] - 52 - k, inner[2], inner[3] - 51 - k],
                        fill=(int(12 * a), int(15 * a), int(24 * a)))
    else:
        for k in range(28):
            t = k / 27
            d.rectangle([inner[0] + (inner[2] - inner[0]) * t, inner[1],
                         inner[0] + (inner[2] - inner[0]) * (t + 1 / 28), inner[3]],
                        fill=(int(hue[0] * (0.55 + 1.15 * t)), int(hue[1] * (0.5 + t)),
                              int(hue[2] * (0.6 + 1.05 * t))))
    d.text((x + 14, y + h - 54), label, font=f(20, True), fill=WHITE)
    if sub:
        d.text((x + 14, y + h - 30), sub, font=f(15), fill=DIM)


def _cell(sheet_name, idx, box):
    path = os.path.join(ASSETS, sheet_name)
    if not os.path.exists(path):
        return False
    sh = Image.open(path).convert("RGB")
    c = sh.size[0] // 3
    r, k = divmod(idx % 9, 3)
    pad = int(c * 0.04)
    t = sh.crop((k * c + pad, r * c + pad, (k + 1) * c - pad, (r + 1) * c - pad))
    bw, bh = box[2] - box[0], box[3] - box[1]
    sc = max(bw / t.size[0], bh / t.size[1])
    t = t.resize((max(1, int(t.size[0] * sc)), max(1, int(t.size[1] * sc))), Image.LANCZOS)
    ox, oy = (t.size[0] - bw) // 2, (t.size[1] - bh) // 2
    return t.crop((ox, oy, ox + bw, oy + bh))

def _stills(im, box, i):
    # the middle cell of the generated sheet merged two frames, so row and card
    # slots draw from a fixed set of clean cells instead of a running index
    cells = (0, 2, 6, 8, 3, 7, 1, 5, 4)
    t = _cell("sheet-stills.jpg", cells[i % len(cells)], box)
    im.paste(t if t else Image.new("RGB", (box[2] - box[0], box[3] - box[1]), (34, 48, 74)), (box[0], box[1]))

def album_cell(im, box, i):
    t = _cell("sheet-albums.jpg", i, box)
    im.paste(t if t else Image.new("RGB", (box[2] - box[0], box[3] - box[1]), (40, 60, 100)), (box[0], box[1]))

def tile(d_in, x, y, w, h, hue, label, sub="", focus=False):
    d = d_in; im = d._image
    slab(d, [x, y, x + w, y + h], 16, SLAB, focus=focus)
    inner = [x + 10, y + 10, x + w - 10, y + h - 68]
    up = label.strip().upper()
    art = None
    if up in ART:
        art = _cover(os.path.join(ASSETS, ART[up] + ".jpg"), inner)
    elif up.endswith(".JPG"):
        n = int("".join(ch for ch in up if ch.isdigit()) or 0)
        art = _cell("sheet-photos.jpg", n % 9, inner)
    elif up.endswith(".MP4"):
        n = int("".join(ch for ch in up if ch.isdigit()) or 0)
        art = _cell("sheet-albums.jpg", n % 9, inner)
    if art:
        im.paste(art, (inner[0], inner[1]))
        d.rectangle([inner[0], inner[3] - 46, inner[2], inner[3]], fill=(11, 14, 22))
        for k in range(20):
            a = 1 - k / 20
            d.rectangle([inner[0], inner[3] - 46 - k, inner[2], inner[3] - 45 - k],
                        fill=(int(11 * a), int(14 * a), int(22 * a)))
    else:
        for k in range(28):
            t = k / 27
            d.rectangle([inner[0] + (inner[2] - inner[0]) * t, inner[1],
                         inner[0] + (inner[2] - inner[0]) * (t + 1 / 28), inner[3]],
                        fill=(int(hue[0] * (0.55 + 1.15 * t)), int(hue[1] * (0.5 + t)),
                              int(hue[2] * (0.6 + 1.05 * t))))
    cp = os.path.join(ASSETS, "chrome.jpg")
    if os.path.exists(cp):
        strip = Image.open(cp).convert("RGB").resize((w - 20, 4), Image.LANCZOS)
        strip = strip.point(lambda v: int(v * 0.55))
        im.paste(Image.blend(im.crop((x + 10, y + 8, x + w - 10, y + 12)), strip, 0.5), (x + 10, y + 8))
    if focus:
        gp = os.path.join(ASSETS, "glass.jpg")
        if os.path.exists(gp):
            gl = Image.open(gp).convert("RGB").resize((w + 60, h + 60), Image.LANCZOS)
            gl = gl.point(lambda v: int(v * 0.30))
            halo = Image.new("RGB", (W, H), (0, 0, 0))
            halo.paste(gl, (x - 30, y - 30))
            im.paste(ImageChops.add(im, halo.filter(ImageFilter.GaussianBlur(30))), (0, 0))
    d = ImageDraw.Draw(im)
    d.text((x + 14, y + h - 52), label, font=f(19, True), fill=WHITE)
    if sub:
        d.text((x + 14, y + h - 28), sub, font=f(14), fill=DIM)

out = "/workspace/chats/0c2b3a099dc1558e1719b468/files/mockups"
os.makedirs(out, exist_ok=True)
for name, fn in [("00-home", screen_home), ("01-pictures", screen_pictures),
                 ("02-music", screen_music), ("03-videos", screen_videos),
                 ("04-files", screen_files), ("05-settings", screen_settings),
                 ("06-dedication", screen_dedication)]:
    fn().save(os.path.join(out, name + ".png"))
    print("wrote", name)
