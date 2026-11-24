#!/usr/bin/env python3
"""make-emblem-h.py - turn the clean emblem artwork into src/emblem.h.

The console build has no image loader: everything it draws is either a
primitive or a table in a header (see ps4logo.h). So the emblem is reduced to
horizontal runs of four tones and written out as data. Drawing it is then a
couple of thousand one-pixel rectangles, which the software renderer handles,
and which needs no file at runtime.

Input: a PNG with a real alpha channel, cropped or not. If the file turns out
to be a JPEG with a painted transparency checkerboard instead, the border
flood fill below still removes it.

    python3 tools/make-emblem-h.py ../PS4_emblem_transparent_1000px.png
"""
import os
import sys
from collections import deque

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_H = os.path.join(os.path.dirname(HERE), "src", "emblem.h")
OUT_PNG = os.path.join(os.path.dirname(HERE), "..", "v66-emblem-source.png")
TARGET_W = 300

# The four tones the artwork is made of: black keyline, brushed steel face,
# the blue glow along the left edge, and the white highlight.
PALETTE = [(12, 16, 26), (172, 194, 216), (86, 140, 198), (242, 249, 255)]


def is_checker(c):
    mx, mn = max(c), min(c)
    return mn > 150 and (mx - mn) <= 24


def cut_background(im):
    """Remove a painted checkerboard, keeping everything the fill cannot reach."""
    w, h = im.size
    px = im.load()
    seen = [[False] * w for _ in range(h)]
    q = deque()

    for x in range(w):
        for y in (0, h - 1):
            if not seen[y][x] and is_checker(px[x, y]):
                seen[y][x] = True
                q.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not seen[y][x] and is_checker(px[x, y]):
                seen[y][x] = True
                q.append((x, y))

    while q:
        x, y = q.popleft()
        for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] \
                    and is_checker(px[nx, ny]):
                seen[ny][nx] = True
                q.append((nx, ny))

    out = Image.new("RGBA", (w, h))
    po = out.load()
    for y in range(h):
        for x in range(w):
            if not seen[y][x]:
                r, g, b = px[x, y]
                po[x, y] = (r, g, b, 255)
    return out


def lum_of(c):
    return (c[0] * 299 + c[1] * 587 + c[2] * 114) // 1000


def tone(c, blue_ok):
    lum = lum_of(c)
    if lum < 50:
        return 0                    # black keyline
    if blue_ok and c[2] - c[0] > 55:
        return 2                    # the blue glow, only along the edge
    if lum > 205:
        return 3                    # highlight
    return 1                        # steel face


def rim_mask(im):
    """Where the blue is allowed: within a few pixels of the dark keyline.

    The artwork's blue sits along the glyph edges, but the same hue also fills
    the lower part of the PS and the wedges of the letters. Classifying by hue
    alone therefore painted blue patches across the faces, which is what read as
    "the blue problem" on the console. Blue is kept only near the keyline, so
    the emblem stays chrome with a blue edge."""
    w, h = im.size
    px = im.load()
    dark = Image.new("L", (w, h), 0)
    dp = dark.load()

    for y in range(h):
        for x in range(w):
            c = px[x, y]
            if c[3] >= 128 and lum_of(c) < 62:
                dp[x, y] = 255
    return dark.filter(ImageFilter.MaxFilter(3))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "../PS4_emblem_transparent_1000px.png"
    im = Image.open(src).convert("RGBA")

    if im.getchannel("A").getextrema()[0] >= 250:
        print("no alpha in the source - keying out the checkerboard")
        im = cut_background(im.convert("RGB"))

    bbox = im.getchannel("A").getbbox()
    im = im.crop(bbox)
    w, h = im.size
    nw = TARGET_W
    nh = max(1, int(round(h * nw / w)))
    im = im.resize((nw, nh), Image.LANCZOS)
    print("emblem", im.size, "from", w, "x", h)

    px = im.load()
    rim = rim_mask(im).load()
    runs = []
    for y in range(nh):
        x = 0
        while x < nw:
            if px[x, y][3] < 128:
                x += 1
                continue
            lv = tone(px[x, y], rim[x, y] > 0)
            x0 = x
            while x < nw and px[x, y][3] >= 128 and \
                    tone(px[x, y], rim[x, y] > 0) == lv:
                x += 1
            runs.append((y, x0, x - x0, lv))
    # Merge identical stretches stacked on consecutive rows into one taller run:
    # a letter stem then costs one rect instead of ninety. This is what keeps
    # the emblem cheap to draw every frame on the console's software renderer.
    open_runs = {}
    merged = []
    for y, x0, ln, lv in runs:
        key = (x0, ln, lv)
        prev = open_runs.get(key)
        if prev is not None and merged[prev][0] + merged[prev][3] == y:
            merged[prev][3] += 1
        else:
            merged.append([y, x0, ln, 1, lv])
            open_runs[key] = len(merged) - 1
    # a key may be open from an older row; only the latest row counts
    merged = [r for r in merged if r[3] > 0]
    print("runs", len(runs), "merged", len(merged), "worst row",
          max(sum(1 for r in runs if r[0] == y) for y in range(nh)))

    # a look at what the table will draw, over the player's dark panel color
    prev = Image.new("RGB", (nw, nh), (18, 22, 34))
    pp = prev.load()
    for y, x0, ln, hh, lv in merged:
        for yy in range(y, y + hh):
            for x in range(x0, x0 + ln):
                pp[x, yy] = PALETTE[lv]
    prev.save(OUT_PNG)

    with open(OUT_H, "w", encoding="utf-8") as f:
        f.write("/* emblem.h - the clean PS4 emblem as drawing data.\n"
                " *\n"
                " * Generated by tools/make-emblem-h.py from the artwork: alpha\n"
                " * cropped, scaled to %d px wide and reduced to horizontal runs of\n"
                " * four tones (keyline, steel, blue glow, highlight). No image file\n"
                " * is loaded at runtime, exactly like ps4logo.h.\n"
                " */\n"
                "#ifndef PS4_EMBLEM_H\n#define PS4_EMBLEM_H\n\n"
                "#define EMBLEM_W %d\n#define EMBLEM_H %d\n\n"
                "static const unsigned short emblem_runs[][5] = {\n"
                % (nw, nw, nh))
        for i, (y, x0, ln, hh, lv) in enumerate(merged):
            f.write("{%d,%d,%d,%d,%d}," % (y, x0, ln, hh, lv))
            if i % 8 == 7:
                f.write("\n")
        f.write("\n};\n\n#define EMBLEM_RUNS "
                "(sizeof(emblem_runs)/sizeof(emblem_runs[0]))\n\n"
                "#endif /* PS4_EMBLEM_H */\n")
    print("wrote", OUT_H, os.path.getsize(OUT_H), "bytes")


if __name__ == "__main__":
    main()
