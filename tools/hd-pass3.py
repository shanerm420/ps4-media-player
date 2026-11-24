#!/usr/bin/env python3
"""Third pass: tone the caustics, join the worm's tail, fix dancer proportions."""

import sys

PATH = "src/vis_art.c"
src = open(PATH, encoding="utf-8").read()
orig = len(src)


def sub(text, old, new):
    n = text.count(old)
    if n != 1:
        sys.exit("anchor count %d: %r" % (n, old[:70]))
    return text.replace(old, new)


# liquid: finer, weaker net, darker deep water
src = sub(src, """            double ca1 = sin((double)x * 0.062 + d * 21.0 - t * 2.1);
            double ca2 = sin((double)x * 0.041 - d * 26.0 + t * 1.5);
            double web = 1.0 - fabs(ca1 * ca2);
            double sh = 1.0 + 0.55 * pow(web, 12.0)
                             * (1.0 - d) * (0.4 + 0.9 * lv);
""", """            double ca1 = sin((double)x * 0.085 + d * 30.0 - t * 2.1);
            double ca2 = sin((double)x * 0.055 - d * 37.0 + t * 1.5);
            double web = 1.0 - fabs(ca1 * ca2);
            double sh = 1.0 + 0.30 * pow(web, 14.0)
                             * (1.0 - d) * (0.3 + 0.9 * lv);
""")

src = sub(src, """                   (int)(( 46.0 + 104.0 * light + face * 26.0) * sh),
                   (int)((140.0 +  66.0 * light + face * 28.0) * sh),
                   (int)((190.0 +  36.0 * light + face * 20.0) * sh), 255);
""", """                   (int)(( 28.0 + 106.0 * light + face * 26.0) * sh),
                   (int)((104.0 +  74.0 * light + face * 28.0) * sh),
                   (int)((160.0 +  44.0 * light + face * 20.0) * sh), 255);
""")

# worm: the body no longer shrinks to nothing at its start, so the tail taper
# can actually meet it
src = sub(src, """        if (u < 0.10)
            bulge *= u / 0.10;          /* the body grows out of the tail */
""", "")

src = sub(src, """            double rr = rmax * (0.16 + 0.56 * u);
""", """            double rr = rmax * (0.13 + 0.53 * u);
""")

# dancer: a short neck and a smaller head, so the figure has human proportions
src = sub(src, """    p->th = H * 0.058;
    p->sw = H * 0.105;
    p->hw = H * 0.085;
    p->hr = H * 0.062;
    p->px = cx;
    p->py = ground - H * 0.47;
    p->cx = cx;
    p->cy = ground - H * 0.655;
    p->nx = cx;
    p->ny = ground - H * 0.775;
    p->hx = cx;
    p->hy = ground - H * 0.885;
""", """    p->th = H * 0.058;
    p->sw = H * 0.108;
    p->hw = H * 0.088;
    p->hr = H * 0.055;
    p->px = cx;
    p->py = ground - H * 0.47;
    p->cx = cx;
    p->cy = ground - H * 0.660;
    p->nx = cx;
    p->ny = ground - H * 0.790;
    p->hx = cx;
    p->hy = ground - H * 0.872;
""")

# the neck follows the new joints
src = sub(src, """    cv_taper(c, p->nx, p->ny + t * 0.2, p->hx, p->hy + p->hr * 0.6,
             t * 0.62, t * 0.56, 188, 156, 136, alpha);
""", """    cv_taper(c, p->nx, p->ny + t * 0.3, p->hx, p->hy + p->hr * 0.5,
             t * 0.58, t * 0.50, 176, 144, 126, alpha);
""")

open(PATH, "w", encoding="utf-8").write(src)
print("patched %s: %d -> %d" % (PATH, orig, len(src)))
