#!/usr/bin/env python3
"""icon-from-art.py - turn the badge artwork into sce_sys/icon0.png.

The PS4 wants icon0.png at 512x512, 24-bit PNG, non-interlaced, no alpha
(psdevwiki, OpenOrbis), and the XMB draws it inside a rounded-corner tile, so
the picture has to bleed to all four edges: a badge floating in the middle of
a big empty square is exactly what we do not want.

The source sheet holds the badge twice; this takes one of them, finds its
outline, squares it around the centre, zooms just past the edges so the plate
bleeds, downsamples with Lanczos and finishes with a light unsharp mask so the
rivets and the lettering edges survive the trip.

    python3 tools/icon-from-art.py --src ~/Downloads/badges.png --split left
    python3 tools/icon-from-art.py --src art.png --box 60,50,700,710

Writes sce_sys/icon0.png (and a 176px preview beside it).
"""
import argparse
import os

from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SIZE = 512


def find_badge(img, side):
    """Bounding box of the bright badge on the dark sheet, left or right."""
    lum = img.convert("L")
    w, h = lum.size
    px = lum.load()

    # column and row profiles of "bright" pixels: the badge is the only thing
    # in the frame that is lit
    cols = [0] * w
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if px[x, y] > 92:
                cols[x] += 1
    rows = [0] * h
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if px[x, y] > 92:
                rows[y] += 1

    # the two badges are the two widest runs of lit columns
    runs, start = [], None
    for x in range(w):
        lit = cols[x] > (h / 2) * 0.06
        if lit and start is None:
            start = x
        elif not lit and start is not None:
            if x - start > w * 0.05:
                runs.append((start, x))
            start = None
    if start is not None:
        runs.append((start, w))
    if not runs:
        raise SystemExit("could not find the badge in that image")
    x0, x1 = runs[0] if side == "left" else runs[-1]

    ys = [y for y in range(h) if rows[y] > (x1 - x0) / 2 * 0.06]
    if not ys:
        raise SystemExit("found the badge columns but no rows")
    return x0, min(ys), x1, max(ys)


def square(box, img_size, zoom):
    """Grow the box to a square around its centre, then `zoom` past the edge."""
    x0, y0, x1, y1 = box
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    side = max(x1 - x0, y1 - y0) * zoom
    w, h = img_size
    side = min(side, min(w, h))
    box = (round(cx - side / 2), round(cy - side / 2),
           round(cx + side / 2), round(cy + side / 2))
    return tuple(max(0, v) for v in box[:1]) + tuple(
        max(0, v) for v in box[1:2]) + (min(w, box[2]), min(h, box[3]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--split", choices=["left", "right"], default="left")
    ap.add_argument("--box", help="explicit x0,y0,x1,y1 in source pixels")
    ap.add_argument("--zoom", type=float, default=1.08,
                    help="how far past the edges the plate bleeds (1.0 = exact)")
    args = ap.parse_args()

    img = Image.open(os.path.expanduser(args.src)).convert("RGB")
    box = (tuple(int(v) for v in args.box.split(",")) if args.box
           else find_badge(img, args.split))
    print("badge box", box, "of", img.size)

    crop = img.crop(square(box, img.size, args.zoom))
    icon = crop.resize((SIZE, SIZE), Image.LANCZOS)
    icon = icon.filter(ImageFilter.UnsharpMask(radius=1.4, percent=55,
                                               threshold=2))
    out = os.path.join(ROOT, "sce_sys", "icon0.png")
    icon.save(out, "PNG")                     # RGB -> 24-bit, no alpha
    icon.resize((176, 176), Image.LANCZOS).save(
        os.path.join(ROOT, "sce_sys", "icon0-preview.png"), "PNG")
    print("wrote", out, icon.size, icon.mode)


if __name__ == "__main__":
    main()
