#!/usr/bin/env python3
"""HD pass for the looks that were still drawn on the old half-resolution grid."""

import sys

PATH = "src/vis_art.c"

src = open(PATH, encoding="utf-8").read()
orig_len = len(src)


def func_span(text, header):
    i = text.index(header)
    j = text.index("\n}\n", text.index("{", i))
    return i, j + 3


def replace_func(text, header, new):
    i, j = func_span(text, header)
    return text[:i] + new + text[j:]


def sub(text, old, new):
    n = text.count(old)
    if n != 1:
        sys.exit("anchor count %d: %r" % (n, old[:80]))
    return text.replace(old, new)


# ------------------------------------------------------------- forward decl --
src = sub(src,
          "#define ART_PI 3.14159265358979\n",
          "#define ART_PI 3.14159265358979\n\n"
          "static double band_at(const UiModel *m, double u);\n")

# ------------------------------------------------------------------- liquid --
LIQUID = r'''static void art_liquid(Cv *c, const UiModel *m)
{
    double  t = m->elapsed_sec;
    double  lv = (double)m->vis_level;
    double  base = (double)c->h * 0.46;
    double *surf;
    int     x, y;

    surf = (double *)malloc(sizeof(double) * (size_t)c->w);
    if (!surf)
        return;

    /* Per column, with the bands read continuously instead of per cell: the
     * cells were what made the old surface a staircase. */
    for (x = 0; x < c->w; x++) {
        double u = (double)x / (double)(c->w > 1 ? c->w - 1 : 1);
        double bi = u * (double)(VIS_BANDS - 1);
        int    b0 = (int)bi, b1 = b0 + 1;
        double bf, band, wv, amp;

        if (b1 >= VIS_BANDS)
            b1 = VIS_BANDS - 1;
        bf = bi - b0;
        band = (double)m->vis_bands[b0] * (1.0 - bf) + (double)m->vis_bands[b1] * bf;
        wv = sin(u * 8.0 + t * 1.6) * 0.46
           + sin(u * 19.0 - t * 2.2) * 0.30
           + sin(u * 34.0 + t * 3.4) * 0.16;
        amp = (double)c->h * (0.05 + 0.24 * band) * (0.55 + 0.75 * lv);
        surf[x] = base - wv * amp - band * amp * 0.7;
        if (surf[x] < (double)c->h * 0.10)
            surf[x] = (double)c->h * 0.10;
        if (surf[x] > (double)c->h * 0.90)
            surf[x] = (double)c->h * 0.90;
    }

    /* the room above the tank */
    cv_vgrad(c, 0, 0, c->w, (int)base + 6, 6, 9, 18, 14, 22, 44);

    for (x = 0; x < c->w; x++) {
        int    y0 = (int)surf[x];
        double span = (double)(c->h - y0);
        double slope = 0.0, face;

        if (span < 1.0)
            span = 1.0;
        /* the far face of a crest turns toward the light, the near face away */
        if (x > 0 && x < c->w - 1)
            slope = (surf[x + 1] - surf[x - 1]) * 0.5;
        face = -slope / (double)c->h * 3.2;
        if (face < -0.55)
            face = -0.55;
        if (face > 0.55)
            face = 0.55;

        for (y = y0; y < c->h; y++) {
            double d = ((double)y - (double)y0) / span;
            double light = 1.0 - 0.74 * d;      /* light dies as it goes down */
            double ca1 = sin((double)x * 0.031 + d * 9.0 - t * 1.9);
            double ca2 = sin((double)x * 0.019 - d * 12.5 + t * 1.3);
            double web = ca1 * ca2;
            double sh = 1.0 + 0.40 * web * web * web * web
                             * (1.0 - d) * (0.4 + lv);

            /* Two crossing waves interfere into the bright web a pool throws
             * on its own floor; raising the product to the fourth sharpens it
             * into caustics instead of a stripe. */
            cv_set(c, x, y,
                   (int)((124.0 + 116.0 * light + face * 26.0) * sh),
                   (int)((198.0 +  48.0 * light + face * 28.0) * sh),
                   (int)((230.0 +  22.0 * light + face * 20.0) * sh), 255);
        }

        /* the crest: hot water, then the mist it throws into the air */
        cv_set(c, x, y0,     246, 253, 255, 255);
        cv_set(c, x, y0 - 1, 206, 238, 255, 210);
        cv_set(c, x, y0 - 2, 152, 202, 246, 118);
        cv_set(c, x, y0 - 3,  92, 142, 212,  66);
        cv_set(c, x, y0 - 4,  52,  92, 162,  34);
    }

    /* glints riding the crest, and spray thrown off the peaks */
    for (x = 4; x < c->w - 4; x++) {
        double g  = 0.5 + 0.5 * sin(t * 0.9 + (double)x * 0.031);
        double g2 = 0.5 + 0.5 * sin(t * 1.7 - (double)x * 0.047);

        if (g > 0.982)
            cv_soft(c, (double)x, surf[x] - 2.0, 7.0, 215, 240, 255, 130);
        if (g2 > 0.995)
            cv_soft(c, (double)x + 3.0, surf[x] - (9.0 + 16.0 * lv), 2.4,
                    235, 250, 255, (int)(90 + 90 * lv));
    }

    /* the bottom of the tank: no light reaches it, and its own color returns */
    cv_soft(c, (double)c->w * 0.5, (double)c->h * 1.02, (double)c->w * 0.55,
            18, 52, 112, 70);
    free(surf);
}
'''

src = replace_func(src, "static void art_liquid(Cv *c, const UiModel *m)\n{", LIQUID)

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

    /* One ripple shared by stripes, canton and stars: everything waves on the
     * same piece of cloth. */
    for (x = 0; x < c->w; x++) {
        double u = (double)x / (double)c->w;
        double big = (double)c->h * (0.030 + 0.060 * lv);
        double small_ = (double)c->h * 0.012;

        rip[x] = big * sin(u * 7.0 - t * 2.3)
               + big * 0.40 * sin(u * 15.0 + t * 1.6)
               + small_ * sin(u * 33.0 - t * 3.1);
    }

    /* Cloth, per pixel: creases run down the flag, the far face of each wave
     * catches the light and the near face falls away, so the thing has a body
     * instead of thirteen flat bands. */
    for (x = 0; x < c->w; x++) {
        double crease = cos((double)x * 0.052 + t * 1.7) * 0.64
                      + cos((double)x * 0.021 - t * 0.90) * 0.36;
        double slope = 0.0;
        double shade = 0.80 + 0.24 * crease + 0.12 * crease * crease * crease;
        double top = rip[x];

        if (x > 0 && x < c->w - 1)
            slope = (rip[x + 1] - rip[x - 1]) * 0.5;
        shade -= slope * 0.035;
        if (shade < 0.46)
            shade = 0.46;
        if (shade > 1.20)
            shade = 1.20;

        for (k = 0; k < stripes; k++) {
            int    b = k * VIS_BANDS / stripes;
            double lit = (double)m->vis_bands[b];
            double jump = lit * lit * (double)c->h * 0.055
                        * sin(t * 7.0 - (double)k * 0.9);
            int    y0 = (int)(top + (double)k * sh + jump);
            int    y1 = (int)(top + (double)(k + 1) * sh + jump);
            int    red = (k % 2 == 0);
            int    y;
            double br, bg, bb;

            if (red) {
                br = 168.0 + lit * 74.0;
                bg =  24.0 + lit * 44.0;
                bb =  44.0 + lit * 50.0;
            } else {
                br = 234.0 + lit * 20.0;
                bg = 234.0 + lit * 17.0;
                bb = 236.0 + lit * 16.0;
            }

            for (y = y0; y <= y1 && y < c->h; y++) {
                double s = shade * (0.93 + 0.07 * sin((double)y * 0.055 + t * 1.2));

                if (y1 - y < 1)             /* the stitched seam between bands */
                    cv_set(c, x, y, (int)(br * s * 0.62), (int)(bg * s * 0.62),
                           (int)(bb * s * 0.62), 255);
                else
                    cv_set(c, x, y, (int)(br * s), (int)(bg * s), (int)(bb * s), 255);
            }
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

    /* Fifty stars, in the real nine-row pattern, pulsing and hopping on the
     * upper bands - the part of the music you feel in your chest. */
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

    /* the pole: a metal cylinder with a lit edge, a finial, and the rings the
     * flag hangs from */
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

src = replace_func(src, "static void art_flag(Cv *c, const UiModel *m)\n{", FLAG)

# --------------------------------------------------------------------- worm --
src = sub(src, """            double d, lam, cov, spec;
            int    cr, cg, cb;
""", """            double d, lam, cov, spec, mott;
            int    cr, cg, cb;
""")

src = sub(src, """            /* light from the upper left: the top of the tube is brightest */
            lam = 0.34 + 0.66 * (d * 0.85 - ty * 0.55);
            if (lam < 0.0)
                lam = 0.0;
            spec = pow(d * (ty < -0.2 ? 1.0 : 0.0) * 1.4, 8.0) * 0.55;
            cr = (int)((196 + 22 * lift) * lam + 255 * spec);
            cg = (int)((122 + 20 * lift) * lam + 250 * spec);
            cb = (int)((148 + 26 * lift) * lam + 255 * spec);
            cv_set(c, x, y, cr, cg, cb, (int)(255 * cov));
""", """            /* A cylinder, not a stripe: the visible surface normal at this
             * height is (0, ty, sqrt(1-ty*ty)). Lighting it properly is what
             * gives the body its roundness, and the half vector gives the wet
             * highlight a place to live instead of a hard band. */
            lam = ty * -0.78 + d * 0.44;
            if (lam < 0.0)
                lam = 0.0;
            spec = pow(ty * -0.60 + d * 0.72, 24.0) * 0.85;
            mott = 0.95 + 0.05 * sin((double)x * 0.21 + ty * 5.0)
                        + 0.03 * sin((double)x * 0.07 - ty * 2.1);
            cr = (int)((198 + 24 * lift) * lam * mott + 255 * spec);
            cg = (int)((124 + 22 * lift) * lam * mott + 252 * spec);
            cb = (int)((150 + 28 * lift) * lam * mott + 255 * spec);
            cv_set(c, x, y, cr, cg, cb, (int)(255 * cov));
""")

src = sub(src, """        if (x % 3 == 0)
            cv_soft(c, (double)x, gy + 14, rad * 2.2, 6, 5, 10, 70);
""", """        /* two shadows: the broad one the body throws, and the tight dark
         * where it actually touches the floor */
        if (x % 2 == 0) {
            cv_soft(c, (double)x, gy + 16, rad * 2.4, 6, 5, 10, 74);
            cv_soft(c, (double)x, gy + 13, rad * 1.1, 4, 3, 7, 96);
        }
""")

# ----------------------------------------------------------------- windmill --
src = sub(src, """    cv_soft(c, w * 0.76, h * 0.58, h * 0.07, 255, 236, 200, 150);
""", """    cv_soft(c, w * 0.76, h * 0.58, h * 0.07, 255, 236, 200, 150);

    /* long cloud bands lit from underneath by that sun: the layer that turns a
     * gradient into an evening */
    for (i = 0; i < 7; i++) {
        double cy = h * (0.16 + 0.052 * (double)i);
        double cw = w * (0.22 + 0.10 * (double)(i % 4));
        double cx = w * (0.18 + 0.62 * fmod((double)i * 0.37 + t * 0.006, 1.0));
        double d = 1.0 - (double)i / 8.0;

        cv_soft(c, cx, cy + 6.0, cw, 236, 148, 126, (int)(16 + 24 * d));
        cv_soft(c, cx, cy, cw * 0.9, 96, 84, 128, (int)(22 + 26 * d));
        cv_soft(c, cx + cw * 0.5, cy + 3.0, cw * 0.30, 255, 190, 150, (int)(20 * d));
    }
""")

src = sub(src, """    /* sails: two ghosts for motion, then the live one */
""", """    /* ambient shadow where the mill meets the field, and the sun catching
     * its right-hand edge */
    cv_soft(c, hx + w * 0.030, h * 0.905, w * 0.085, 6, 6, 10, 130);
    cv_soft(c, hx + w * 0.050, h * 0.895, w * 0.055, 4, 4, 8, 80);
    cv_line(c, hx + w * 0.060, h * 0.895, hx + w * 0.042, h * 0.395,
            2.4, 255, 196, 132, 120);
    cv_line(c, hx - w * 0.060, h * 0.895, hx - w * 0.042, h * 0.395,
            1.6, 120, 160, 220, 46);

    /* sails: two ghosts for motion, then the live one */
""")

src = sub(src, """    for (i = 0; i < 90; i++) {
        double gx = (double)i * w / 90.0;
        double gh = h * (0.025 + 0.018 * sin((double)i * 1.7));

        cv_line(c, gx, h, gx + 1.5, h - gh, 2.2, 14, 18, 22, 235);
    }
""", """    /* the field itself, warmer near the sun and falling into shadow at the
     * edges, with grass that catches the last light on its tips */
    {
        double gx[4], gy[4];

        gx[0] = 0.0;      gy[0] = h * 0.86;
        gx[1] = w;        gy[1] = h * 0.87;
        gx[2] = w;        gy[2] = h;
        gx[3] = 0.0;      gy[3] = h;
        cv_poly(c, gx, gy, 4, 12, 16, 20, 255);
        cv_soft(c, w * 0.70, h * 0.90, w * 0.45, 120, 92, 60, 60);
        cv_soft(c, w * 0.10, h * 1.00, w * 0.35, 6, 8, 14, 90);
    }
    for (i = 0; i < 130; i++) {
        double gx = (double)i * w / 130.0;
        double gh = h * (0.022 + 0.020 * sin((double)i * 1.7));
        double tip = 0.4 + 0.6 * ((double)i / 130.0);

        cv_line(c, gx, h, gx + 1.5, h - gh, 2.2, 14, 18, 22, 235);
        cv_line(c, gx + 1.5, h - gh, gx + 2.0, h - gh - 2.0, 1.2,
                (int)(80 + 70 * tip), (int)(84 + 56 * tip), (int)(60 + 30 * tip), 180);
    }
""")

# ------------------------------------------------------------------ dancers --
src = sub(src, """    cv_vgrad(c, 0, 0, c->w, c->h, 7, 8, 18, 15, 16, 30);
""", """    cv_vgrad(c, 0, 0, c->w, c->h, 7, 8, 18, 15, 16, 30);

    /* the back wall: a grid of LED panels, each answering its own slice of the
     * spectrum, so the room is lit by the music and not only by the rig */
    {
        int    cols = 16, rows = 6, px_, py_;
        double pw = w / (double)cols, ph = h * 0.66 / (double)rows;

        for (px_ = 0; px_ < cols; px_++) {
            for (py_ = 0; py_ < rows; py_++) {
                double lit = band_at(m, ((double)px_ + 0.5) / (double)cols)
                           * (1.0 - 0.50 * (double)py_ / (double)rows);
                double x0 = (double)px_ * pw, y0 = (double)py_ * ph;

                if (lit < 0.02)
                    continue;
                cv_rect(c, (int)x0 + 1, (int)y0 + 1, (int)(pw - 2.0), (int)(ph - 2.0),
                        (int)(12 + 96 * lit), (int)(14 + 44 * lit),
                        (int)(26 + 150 * lit), (int)(36 + 92 * lit));
            }
        }
        cv_soft(c, w * 0.5, h * 0.30, w * 0.42, 70, 60, 160, (int)(10 + 26 * lv));
    }
""")

src = sub(src, """    cv_rect(c, 0, (int)(h * 0.865), c->w, 2, 120, 170, 230, (int)(90 + 80 * lv));
""", """    cv_rect(c, 0, (int)(h * 0.865), c->w, 2, 120, 170, 230, (int)(90 + 80 * lv));

    /* the floor in perspective: lines running to a vanishing point below the
     * frame, and boards closing up as they recede */
    {
        int k;

        for (k = -6; k <= 6; k++) {
            cv_line(c, w * 0.5 + (double)k * w * 0.055, h * 0.865,
                    w * 0.5 + (double)k * w * 0.30, h, 1.0, 70, 110, 180, 46);
        }
        for (k = 1; k <= 5; k++) {
            double u = (double)k / 5.0;
            double yy = h * 0.865 + h * 0.135 * u * u;

            cv_line(c, 0.0, yy, w, yy, 1.0, 60, 96, 160, (int)(20 + 24 * u));
        }
    }
""")

# the costume catches the stage light
src = sub(src, """        cv_poly(c, xs, ys, 6, 34, 28, 44, alpha);
    }

    if (!rims)
""", """        cv_poly(c, xs, ys, 6, 34, 28, 44, alpha);
    }

    /* the costume: a sheen across the chest and a belt, so the figure has a
     * material rather than a silhouette */
    cv_poly(c, (double[]){ p->cx - p->sw * 0.92, p->cx + p->sw * 0.30,
                           p->px + p->hw * 0.30, p->px - p->hw * 0.82 },
               (double[]){ p->cy - t * 0.55, p->cy - t * 0.65,
                           p->py - t * 0.10, p->py - t * 0.34 }, 4,
            86, 112, 158, alpha * 2 / 3);
    cv_line(c, p->px - p->hw * 0.92, p->py - t * 0.10,
            p->px + p->hw * 0.92, p->py - t * 0.10, t * 0.30,
            236, 120, 214, alpha);

    if (!rims)
""")

# --------------------------------------------------------------------- rave --
src = sub(src, """                          h * (0.150 + 0.045 * r1), phase, lv, 235, (i % 3) != 1);
    }
""", """                          h * (0.150 + 0.045 * r1), phase, lv, 235, (i % 3) != 1);
    }

    /* smoke between the ranks: without it the far crowd reads as being the
     * same distance away as the front one */
    cv_rect(c, 0, (int)(h * 0.62), c->w, (int)(h * 0.13), 34, 20, 62, 44);
    cv_rect(c, 0, (int)(h * 0.71), c->w, (int)(h * 0.06), 20, 12, 40, 26);
""")

src = sub(src, """    cv_soft(c, w * 0.5, h * 0.40, w * 0.52, 120, 40, 170, 30);
""", """    cv_soft(c, w * 0.5, h * 0.40, w * 0.52, 120, 40, 170, 30);

    /* the air over the floor: three layers of haze, densest where the light
     * enters, so the beams have volume */
    cv_soft(c, w * 0.30, h * 0.20, w * 0.42, 90, 60, 170, 22);
    cv_soft(c, w * 0.72, h * 0.26, w * 0.38, 60, 120, 190, 20);
    cv_soft(c, w * 0.50, h * 0.55, w * 0.60, 70, 40, 120, 26);
""")

# ---------------------------------------------------------------- tesseract --
src = sub(src, """    tess_cell(c, px_, py_, pz_, 1, lv);
}
""", """    tess_cell(c, px_, py_, pz_, 1, lv);

    /* the corners themselves: small glass nodes with their own light, sized by
     * depth, so the far cube reads as further away instead of merely thinner */
    {
        int n;

        for (n = 0; n < 16; n++) {
            double rad = sc * (0.019 + 0.007 * (pz_[n] + 1.0));
            double lit = 0.55 + 0.45 * lv + 0.35 * bloom;

            cv_soft(c, px_[n], py_[n], rad * 5.5, 90, 170, 255,
                    clampi((int)(18 + 40 * lit + 18 * bloom), 0, 120));
            cv_ball(c, px_[n], py_[n], rad, 110, 180, 240, 255, 255, 255);
        }
    }
}
""")

open(PATH, "w", encoding="utf-8").write(src)
print("patched %s: %d -> %d bytes" % (PATH, orig_len, len(src)))
