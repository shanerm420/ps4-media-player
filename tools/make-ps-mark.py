"""Cut the PlayStation mark out of the header emblem and bake it into an alpha
map, so the tesseract look can extrude the real logo instead of a copy of it.

The emblem art is a metal rendering with a dark keyline, so the mark is taken
from the lit body of the art (alpha plus brightness), which leaves a clean
silhouette - the counters of the P and the gap under the S included - and drops
the drop shadow, the keyline and the little (r) mark.
"""
from PIL import Image
import numpy as np

SRC = 'src/emblem.png'
W, H = 160, 128

im = Image.open(SRC).convert('RGBA')
a = np.array(im).astype(float)
crop = a[2:186, 2:231, :]
rgb, al = crop[..., :3], crop[..., 3]
luma = 0.299 * rgb[..., 0] + 0.587 * rgb[..., 1] + 0.114 * rgb[..., 2]
mask = ((al > 128) & (luma > 75)).astype(np.uint8)

# drop specks: the (r) beside the mark must not become part of the badge
try:
    from scipy import ndimage

    lab, n = ndimage.label(mask)
    sizes = ndimage.sum(mask, lab, range(1, n + 1))
    keep = np.zeros(n + 1, dtype=np.uint8)
    for i, sz in enumerate(sizes, start=1):
        if sz >= 200:
            keep[i] = 1
    mask = keep[lab]
except ImportError:
    pass

small = Image.fromarray(mask * 255).resize((W, H), Image.BOX)
m = np.where(np.array(small) < 24, 0, np.array(small)).astype(np.uint8)

rows = []
for y in range(H):
    rows.append('    ' + ','.join('%3d' % v for v in m[y]) + ',')
body = '\n'.join(rows)

open('src/ps_mark.h', 'w', encoding='utf-8').write(f'''/* ps_mark.h - the PlayStation mark as an alpha silhouette.
 *
 * Traced from the app's own emblem art (tools/make-ps-mark.py) and stored as
 * one byte of coverage per pixel, so the tesseract look can extrude the real
 * logo: every layer is the actual outline, not an approximation of it.
 */
#ifndef PS_MARK_ALPHA_H
#define PS_MARK_ALPHA_H

#define PS_MARK_W {W}
#define PS_MARK_HEIGHT {H}

extern const unsigned char ps_mark_alpha[PS_MARK_W * PS_MARK_HEIGHT];

#endif
''')

open('src/ps_mark.c', 'w', encoding='utf-8').write(f'''/* ps_mark.c - coverage map of the PlayStation mark. Generated, do not edit. */
#include "ps_mark.h"

const unsigned char ps_mark_alpha[PS_MARK_W * PS_MARK_HEIGHT] = {{
{body}
}};
''')

print('ps_mark %dx%d coverage=%.3f' % (W, H, (m > 128).mean()))
