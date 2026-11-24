#!/usr/bin/env python3
"""Second pass: fixes found in the first render of the HD looks."""

import sys

PATH = "src/vis_art.c"
src = open(PATH, encoding="utf-8").read()
orig = len(src)


def func_span(t):
    i = t.index("static void art_flag(Cv *c, const UiModel *m)\n{")
    j = t.index("\n}\n", t.index("{", i))
    return i, j + 3


def sub(text, old, new):
    n = text.count(old)
    if n != 1:
        sys.exit("anchor count %d: %r" % (n, old[:70]))
    return text.replace(old, new)


# ------------------------------------------------------------------- liquid --
src = sub(src, """            double ca1 = sin((double)x * 0.031 + d * 9.0 - t * 1.9);
            double ca2 = sin((double)x * 0.019 - d * 12.5 + t * 1.3);
            double web = ca1 * ca2;
            double sh = 1.0 + 0.40 * web * web * web * web
                             * (1.0 - d) * (0.4 + lv);
""", """            double ca1 = sin((double)x * 0.062 + d * 21.0 - t * 2.1);
            double ca2 = sin((double)x * 0.041 - d * 26.0 + t * 1.5);
            double web = 1.0 - fabs(ca1 * ca2);
            double sh = 1.0 + 0.55 * pow(web, 12.0)
                             * (1.0 - d) * (0.4 + 0.9 * lv);
""")

src = sub(src, """            cv_set(c, x, y,
                   (int)((124.0 + 116.0 * light + face * 26.0) * sh),
                   (int)((198.0 +  48.0 * light + face * 28.0) * sh),
                   (int)((230.0 +  22.0 * light + face * 20.0) * sh), 255);
""", """            cv_set(c, x, y,
                   (int)(( 46.0 + 104.0 * light + face * 26.0) * sh),
                   (int)((140.0 +  66.0 * light + face * 28.0) * sh),
                   (int)((190.0 +  36.0 * light + face * 20.0) * sh), 255);
""")

src = sub(src, """    cv_soft(c, (double)c->w * 0.5, (double)c->h * 1.02, (double)c->w * 0.55,
            18, 52, 112, 70);
""", """    cv_soft(c, (double)c->w * 0.5, (double)c->h * 1.02, (double)c->w * 0.55,
            10, 30, 74, 55);
""")

src = sub(src, """    case VIS_STYLE_LIQUID:
        fx_reflect(&c, h * 0.50, 0.45, 1.30);
""", """    case VIS_STYLE_LIQUID:
        fx_reflect(&c, h * 0.47, 0.34, 1.05);
""")

# --------------------------------------------------------------------- flag --
FLAG = r'''static void art_flag(Cv *c, const UiModel *m)
{
    const int stripes = 13;
    double    t = m->elapsed_sec;
    double    lv = (double)m->vis_level;
    double    sh = (double)c->h / (double)stripes;
    double    canton_w = (double)c->w * 0.42;
    double    pole = (double)c->w * 0.014 + 2.0;
    double   *rip;
    int       x, k, i;

    rip = (double *)malloc(sizeof(double) * (size_t)c->w);
    if (!rip)
        return;

    for (x = 0; x < c->w; x++) {
        double u = (double)x / (double)c->w;
        double big = (double)c->h * (0.030 + 0.060 * lv);
        double small_ = (double)c->h * 0.012;

        rip[x] = big * sin(u * 7.0 - t * 2.3)
               + big * 0.40 * sin(u * 15.0 + t * 1.6)
               + small_ * sin(u * 33.0 - t * 3.1);
    }

    /* Cloth, per pixel. The bands are not stacked rectangles any more: one
     * continuous strip of cloth is displaced by the beat and the stripe index
     * is read from the displaced position, so the bands can never separate and
     * show background through the flag. */
    for (x = 0; x < c->w; x++) {
        double crease = cos((double)x * 0.052 + t * 1.7) * 0.64
                      + cos((double)x * 0.021 - t * 0.90) * 0.36;
        double slope = 0.0;
        double shade = 0.80 + 0.24 * crease + 0.12 * crease * crease * crease;
        double top = rip[x];
        int    y, yy0 = (int)top, yy1 = (int)(top + sh * (double)stripes);

        if (x > 0 && x < c->w - 1)
            slope = (rip[x + 1] - rip[x - 1]) * 0.5;
        shade -= slope * 0.035;
        if (shade < 0.46)
            shade = 0.46;
        if (shade > 1.20)
            shade = 1.20;

        for (y = yy0; y < yy1 && y < c->h; y++) {
            double v = ((double)y - top) / (sh * (double)stripes);
            double vd = v
                      + 0.055 * lv * sin(v * 9.0 - t * 2.4 + (double)x * 0.004)
                      + 0.022 * lv * sin(v * 21.0 + t * 1.3);
            double pos = vd * (double)stripes;
            double frac, s;
            int    b, red;
            double br, bg, bb;

            k = (int)floor(pos);
            if (k < 0)
                k = 0;
            if (k > stripes - 1)
                k = stripes - 1;
            frac = pos - floor(pos);

            b = k * VIS_BANDS / stripes;
            {
                double lit = (double)m->vis_bands[b];

                if (k % 2 == 0) {
                    br = 168.0 + lit * 74.0;
                    bg =  24.0 + lit * 44.0;
                    bb =  44.0 + lit * 50.0;
                } else {
                    br = 234.0 + lit * 20.0;
                    bg = 234.0 + lit * 17.0;
                    bb = 236.0 + lit * 16.0;
                }
            }
            s = shade * (0.93 + 0.07 * sin((double)y * 0.055 + t * 1.2));
            if (frac < 0.045 || frac > 0.955)     /* the stitched seam */
                s *= 0.84;
            cv_set(c, x, y, (int)(br * s), (int)(bg * s), (int)(bb * s), 255);
        }
    }

    for (x = 0; x < (int)canton_w; x++) {
        double crease = cos((double)x * 0.055 + t * 1.7) * 0.70 + 0.30;
        double shade = 0.78 + 0.30 * crease;
        int    y0 = (int)rip[x], y1 = y0 + (int)(sh * 7.0);
        int    y;

        for (y = y0; y < y1 && y < c->h; y++) {
            double d = (double)(y - y0) / (double)(y1 - y0 + 1);
            double s = shade * (0.94 + 0.10 * d);

            cv_set(c, x, y, (int)(30 * s), (int)(48 * s), (int)(120 * s), 255);
        }
    }
    cv_line(c, canton_w - 1.0, rip[(int)canton_w - 1],
            canton_w - 1.0, rip[(int)canton_w - 1] + sh * 7.0,
            2.0, 200, 210, 240, 55);

    for (i = 0; i < 50; i++) {
        int    row = 0, count, index = i, col;
        double rowstep = 1.0 / 9.0;
        int    long_row = 1;

        while (index >= (long_row ? 6 : 5)) {
            index -= long_row ? 6 : 5;
            long_row = !long_row;
            row++;
        }
        count = long_row ? 6 : 5;
        col = index;
        {
            int    b = i * VIS_BANDS / 50;
            double lit = (double)m->vis_bands[b];
            double ux = (double)c->w * 0.42;
            double cx = ux * (0.10 + 0.80 * (double)(col + (long_row ? 0 : 0.5))
                             / (double)count);
            double cy = rip[(int)cx] + sh * 7.0 *
                        (rowstep + (double)row * (0.93 / 9.0))
                      - lit * lit * (double)c->h * 0.045;
            double rad = sh * 0.26 * (1.0 + lit * 0.55);

            cv_soft(c, cx, cy, rad * 2.8, 150, 200, 255, (int)(26 + lit * 60));
            star_shape(c, cx, cy, rad, t * 0.25 + (double)i, 238, 246, 255, 255);
            cv_soft(c, cx, cy, rad * 0.9, 255, 255, 255, (int)(120 + lit * 100));
        }
    }

    {
        int yy;

        for (yy = 0; yy < c->h; yy++) {
            double s = 1.0 - 0.10 * (double)yy / (double)c->h;

            cv_set(c, (int)(pole * 0.30), yy,
                   (int)(54 * s), (int)(60 * s), (int)(78 * s), 255);
            cv_set(c, (int)(pole * 0.55), yy,
                   (int)(124 * s), (int)(132 * s), (int)(156 * s), 255);
            cv_set(c, (int)(pole * 0.80), yy,
                   (int)(34 * s), (int)(38 * s), (int)(52 * s), 255);
        }
        cv_ball(c, pole * 0.55, (double)c->h * 0.02, pole * 0.45,
                180, 176, 150, 255, 240, 210);
        for (yy = 0; yy < 6; yy++)
            cv_ring(c, pole * 0.55, (double)c->h * (0.10 + 0.150 * (double)yy),
                    pole * 0.42, 2.0, 172, 168, 146, 180);
    }
    free(rip);
}
'''

i, j = func_span(src)
src = src[:i] + FLAG + src[j:]

# --------------------------------------------------------------------- worm --
src = sub(src, """            cr = (int)((198 + 24 * lift) * lam * mott + 255 * spec);
            cg = (int)((124 + 22 * lift) * lam * mott + 252 * spec);
            cb = (int)((150 + 28 * lift) * lam * mott + 255 * spec);
""", """            cr = (int)((168 + 26 * lift) * lam * mott + 255 * spec);
            cg = (int)(( 96 + 20 * lift) * lam * mott + 250 * spec);
            cb = (int)((124 + 26 * lift) * lam * mott + 255 * spec);
""")

src = sub(src, """        /* the wet back: a stroked curve, so it is smooth at any width */
        if (x > (int)x0)
            cv_line(c, (double)x - 1.0, cy - rad * 0.52, (double)x + 0.5,
                    cy - rad * 0.50, 1.6, 255, 246, 255, 95);
""", """        /* the wet back: a soft sheen laid on the tube, not a drawn line */
        if (x > (int)x0)
            cv_soft(c, (double)x, cy - rad * 0.40, rad * 0.34,
                    255, 238, 250, 52);
""")

src = sub(src, """    /* tail, thinning out behind the body */
    for (i = 0; i < 46; i++) {
        double u = (double)i / 46.0;
        double yy = gy - h * 0.09 - sin(t * 2.3 + u * 1.5) * h * 0.035;
        double xx = x0 - u * w * 0.07;

        cv_ball(c, xx, yy, rmax * (0.62 - 0.44 * u), 198, 126, 150, 250, 232, 250);
    }
""", """    /* tail: one continuous taper out of the body's start, so the two are the
     * same animal instead of a body with a blob beside it */
    {
        double ty0 = gy - h * 0.09 - sin(t * 2.3) * h * 0.055;
        double ty1 = gy - 0.30 * (h * 0.30) - sin(t * 2.3) * h * 0.055;

        for (i = 0; i < 48; i++) {
            double u = (double)i / 47.0;
            double yy = ty0 + (ty1 - ty0) * u * u;
            double xx = x0 - (1.0 - u) * w * 0.075;
            double rr = rmax * (0.16 + 0.56 * u);

            cv_soft(c, xx, gy + 14.0, rr * 2.0, 4, 3, 8, 60);
            cv_ball(c, xx, yy, rr, 164, 100, 124, 246, 226, 244);
        }
    }
""")

src = sub(src, """    case VIS_STYLE_WORM:
        fx_reflect(&c, h * 0.78, 0.34, 0.22);
""", """    case VIS_STYLE_WORM:
        fx_reflect(&c, h * 0.78, 0.22, 0.12);
""")

# ----------------------------------------------------------------- windmill --
src = sub(src, """        cv_poly(c, gx, gy, 4, 12, 16, 20, 255);
        cv_soft(c, w * 0.70, h * 0.90, w * 0.45, 120, 92, 60, 60);
        cv_soft(c, w * 0.10, h * 1.00, w * 0.35, 6, 8, 14, 90);
""", """        cv_poly(c, gx, gy, 4, 24, 27, 30, 255);
        cv_soft(c, w * 0.70, h * 0.92, w * 0.48, 148, 108, 62, 86);
        cv_soft(c, w * 0.10, h * 1.00, w * 0.35, 8, 10, 16, 80);
""")

src = sub(src, """        cv_line(c, gx, h, gx + 1.5, h - gh, 2.2, 14, 18, 22, 235);
""", """        cv_line(c, gx, h, gx + 1.5, h - gh, 2.2, 26, 30, 30, 235);
""")

src = sub(src, """        cv_poly(c, (double[]){ hx - bw, hx, hx, hx - tw },
                   (double[]){ by, by, ty, ty }, 4, 132, 112, 96, 230);
""", """        cv_poly(c, (double[]){ hx - bw, hx, hx, hx - tw },
                   (double[]){ by, by, ty, ty }, 4, 112, 96, 84, 230);
""")

# ------------------------------------------------------------------ dancers --
src = sub(src, "    p->th = H * 0.050;\n", "    p->th = H * 0.058;\n")

src = sub(src, "    const int dark_r = 52, dark_g = 64, dark_b = 96;\n",
          "    const int dark_r = 22, dark_g = 25, dark_b = 40;\n")

src = sub(src, """    cv_taper(c, p->nx, p->ny + t * 0.2, p->hx, p->hy + p->hr * 0.6,
             t * 0.60, t * 0.55, 196, 168, 148, alpha);
    cv_soft(c, p->hx, p->hy, p->hr + 0.6, 206, 176, 152, alpha);
    {
        double xs[6], ys[6];

        xs[0] = p->hx - p->hr * 1.02;  ys[0] = p->hy + p->hr * 0.20;
        xs[1] = p->hx - p->hr * 0.88;  ys[1] = p->hy - p->hr * 0.90;
        xs[2] = p->hx + p->hr * 0.10;  ys[2] = p->hy - p->hr * 1.18;
        xs[3] = p->hx + p->hr * 1.02;  ys[3] = p->hy - p->hr * 0.55;
        xs[4] = p->hx + p->hr * 0.96;  ys[4] = p->hy + p->hr * 0.15;
        xs[5] = p->hx - p->hr * 0.15;  ys[5] = p->hy - p->hr * 0.25;
        cv_poly(c, xs, ys, 6, 34, 28, 44, alpha);
    }
""", """    cv_taper(c, p->nx, p->ny + t * 0.2, p->hx, p->hy + p->hr * 0.6,
             t * 0.62, t * 0.56, 188, 156, 136, alpha);
    /* a head, not a wig: a lit sphere with hair over the top and back only */
    cv_ball(c, p->hx, p->hy, p->hr, 196, 160, 138, 255, 244, 228);
    {
        double xs[6], ys[6];

        xs[0] = p->hx - p->hr * 1.04;  ys[0] = p->hy + p->hr * 0.26;
        xs[1] = p->hx - p->hr * 1.00;  ys[1] = p->hy - p->hr * 0.92;
        xs[2] = p->hx;                 ys[2] = p->hy - p->hr * 1.20;
        xs[3] = p->hx + p->hr * 1.00;  ys[3] = p->hy - p->hr * 0.78;
        xs[4] = p->hx + p->hr * 0.58;  ys[4] = p->hy - p->hr * 0.30;
        xs[5] = p->hx - p->hr * 0.72;  ys[5] = p->hy - p->hr * 0.06;
        cv_poly(c, xs, ys, 6, 28, 22, 38, alpha);
    }
    cv_soft(c, p->hx + p->hr * 0.36, p->hy - p->hr * 0.44, p->hr * 0.55,
            255, 236, 214, 70);
""")

src = sub(src, "                t * 1.00, -t * 0.18, rim_r, rim_g, rim_b, 105);\n",
          "                t * 1.00, -t * 0.18, rim_r, rim_g, rim_b, 155);\n")
src = sub(src, "rim_b, 80);", "rim_b, 118);")
src = sub(src, "rim_b, 95);", "rim_b, 140);")
src = sub(src, "rim_b, 70);", "rim_b, 104);")

src = sub(src, """                cv_rect(c, (int)x0 + 1, (int)y0 + 1, (int)(pw - 2.0), (int)(ph - 2.0),
                        (int)(12 + 96 * lit), (int)(14 + 44 * lit),
                        (int)(26 + 150 * lit), (int)(36 + 92 * lit));
""", """                cv_rect(c, (int)x0 + 1, (int)y0 + 1, (int)(pw - 2.0), (int)(ph - 2.0),
                        (int)(16 + 140 * lit), (int)(18 + 52 * lit),
                        (int)(34 + 190 * lit), (int)(46 + 120 * lit));
""")

# --------------------------------------------------------------------- rave --
src = sub(src, """        cv_taper(c, dx + w * 0.088, shy + h * 0.01,
                 dx + w * 0.155, h * 0.700, h * 0.030, h * 0.024, 16, 16, 24, 255);
        cv_taper(c, dx + w * 0.155, h * 0.700, handx, handy,
                 h * 0.024, h * 0.018, 16, 16, 24, 255);
""", """        cv_taper(c, dx + w * 0.088, shy + h * 0.01,
                 dx + w * 0.155, h * 0.700, h * 0.026, h * 0.021, 16, 16, 24, 255);
        cv_taper(c, dx + w * 0.155, h * 0.700, handx, handy,
                 h * 0.021, h * 0.016, 16, 16, 24, 255);
        /* the wall light running down the outside of that raised arm */
        cv_line(c, dx + w * 0.095, shy - h * 0.005,
                handx - w * 0.004, handy + h * 0.004, 2.4, 150, 220, 255, 105);
""")

open(PATH, "w", encoding="utf-8").write(src)
print("patched %s: %d -> %d" % (PATH, orig, len(src)))
