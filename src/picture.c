/* picture.c - JPEG / PNG / BMP decoding and still-picture grading.
 *
 * One decoded copy stays in memory at the picture's own resolution (capped so
 * a 50 MP photo cannot eat the console's heap). Every settings change grades
 * that copy into the output buffer: sharpness first (it needs the unmodified
 * neighbours), then brightness, then color saturation, then rotation. The
 * result is stored in the same byte order the renderer is fed for video
 * frames (b,g,r,a), with COLOR ORDER in the picture settings able to reverse
 * the red and blue bytes for a file that comes out the wrong way round.
 */
#include "picture.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 2560 x 1600 keeps a 4 MP photo at full detail, a 12 MP phone photo at half,
 * and the two buffers under 40 MB together. */
#define PIC_MAX_W 2560
#define PIC_MAX_H 1600

/* the implementation lives in stb_image_impl.c, shared with the emblem */
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb_image.h"
#ifdef __clang__
#pragma clang diagnostic pop
#endif

static uint8_t *s_base;         /* source, r,g,b,a */
static int      s_bw, s_bh;
static uint8_t *s_out;          /* processed, b,g,r,a */
static int      s_ow, s_oh;
static int      s_serial;
static char     s_err[160] = "NO PICTURE";

static int clamp_i(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static uint8_t clamp_u8(int v)
{
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

void picture_close(void)
{
    free(s_base);
    free(s_out);
    s_base = NULL;
    s_out  = NULL;
    s_bw = s_bh = s_ow = s_oh = 0;
    s_serial++;
}

int picture_open(const char *path)
{
    int            w = 0, h = 0, n = 0;
    unsigned char *data;

    picture_close();
    s_err[0] = '\0';
    if (!path || !path[0]) {
        snprintf(s_err, sizeof(s_err), "NO PICTURE");
        return -1;
    }

    data = stbi_load(path, &w, &h, &n, 4);
    if (!data) {
        const char *why = stbi_failure_reason();

        snprintf(s_err, sizeof(s_err), "CANNOT DECODE %s", why ? why : "FILE");
        return -1;
    }
    if (w <= 0 || h <= 0 || (size_t)w * (size_t)h > 64u * 1024u * 1024u) {
        stbi_image_free(data);
        snprintf(s_err, sizeof(s_err), "PICTURE TOO LARGE");
        return -1;
    }

    /* Halve until it fits: a photo viewer shows 1080p, and a 48 MB texture is
     * the fastest way to exhaust the console's memory. */
    while (w > PIC_MAX_W * 2 || h > PIC_MAX_H * 2) {
        int          nw = w / 2, nh = h / 2;
        unsigned char *small = malloc((size_t)nw * (size_t)nh * 4);
        int          x, y;

        if (!small)
            break;
        for (y = 0; y < nh; y++) {
            for (x = 0; x < nw; x++) {
                const unsigned char *a = data + ((size_t)(y * 2) * w + x * 2) * 4;
                const unsigned char *b = a + (size_t)w * 4;
                const unsigned char *c = a + 4;
                const unsigned char *d = b + 4;
                unsigned char       *o = small + ((size_t)y * nw + x) * 4;
                int                  k;

                for (k = 0; k < 4; k++)
                    o[k] = (unsigned char)((a[k] + b[k] + c[k] + d[k] + 2) / 4);
            }
        }
        stbi_image_free(data);
        data = small;
        w = nw;
        h = nh;
    }
    if (w > PIC_MAX_W) {
        int            nw = PIC_MAX_W;
        int            nh = (int)((long)h * nw / w);
        unsigned char *small = malloc((size_t)nw * (size_t)nh * 4);
        int            x, y;

        if (small) {
            for (y = 0; y < nh; y++) {
                int sy = (int)((long)y * h / nh);

                for (x = 0; x < nw; x++) {
                    int sx = (int)((long)x * w / nw);

                    memcpy(small + ((size_t)y * nw + x) * 4,
                           data + ((size_t)sy * w + sx) * 4, 4);
                }
            }
            stbi_image_free(data);
            data = small;
            w = nw;
            h = nh;
        }
    }

    s_base = data;
    s_bw   = w;
    s_bh   = h;
    return 0;
}

void picture_apply(int brightness, int saturation, int sharpness, int rotation,
                   int rb_swap)
{
    uint8_t *adj;
    int      x, y, r, g, b;

    if (!s_base || s_bw <= 0 || s_bh <= 0)
        return;

    brightness = clamp_i(brightness, -100, 100);
    saturation = clamp_i(saturation, 0, 250);
    sharpness  = clamp_i(sharpness, 0, 100);
    rotation   = ((rotation % 4) + 4) % 4;

    adj = malloc((size_t)s_bw * (size_t)s_bh * 4);
    if (!adj)
        return;

    for (y = 0; y < s_bh; y++) {
        const uint8_t *row = s_base + (size_t)y * s_bw * 4;
        uint8_t       *orow = adj + (size_t)y * s_bw * 4;

        for (x = 0; x < s_bw; x++) {
            const uint8_t *p = row + (size_t)x * 4;

            r = p[0]; g = p[1]; b = p[2];

            if (sharpness > 0) {
                const uint8_t *l = x > 0 ? p - 4 : p;
                const uint8_t *rr = x + 1 < s_bw ? p + 4 : p;
                const uint8_t *u = y > 0 ? p - (size_t)s_bw * 4 : p;
                const uint8_t *d = y + 1 < s_bh ? p + (size_t)s_bw * 4 : p;

                /* Laplacian unsharp on luma only. 0..100 maps to half the
                 * edge difference, which is strong but never inverts. */
                {
                    int yc = (77 * r + 150 * g + 29 * b) >> 8;
                    int yl = (77 * l[0] + 150 * l[1] + 29 * l[2]) >> 8;
                    int yr = (77 * rr[0] + 150 * rr[1] + 29 * rr[2]) >> 8;
                    int yu = (77 * u[0] + 150 * u[1] + 29 * u[2]) >> 8;
                    int yd = (77 * d[0] + 150 * d[1] + 29 * d[2]) >> 8;
                    int delta = (4 * yc - yl - yr - yu - yd) * sharpness / 200;

                    r += delta; g += delta; b += delta;
                }
            }

            if (brightness) {
                int add = brightness * 255 / 100;

                r += add; g += add; b += add;
            }

            if (saturation != 100) {
                int lum = (77 * r + 150 * g + 29 * b) >> 8;

                r = lum + (r - lum) * saturation / 100;
                g = lum + (g - lum) * saturation / 100;
                b = lum + (b - lum) * saturation / 100;
            }

            orow[x * 4 + 0] = clamp_u8(rb_swap ? r : b);
            orow[x * 4 + 1] = clamp_u8(g);
            orow[x * 4 + 2] = clamp_u8(rb_swap ? b : r);
            /* Photos are opaque. Carrying the source alpha through let a
             * transparent PNG tint the picture towards the dark plate the
             * viewer draws behind it, which reads as a blue cast. */
            orow[x * 4 + 3] = 255;
        }
    }

    {
        int ow = (rotation & 1) ? s_bh : s_bw;
        int oh = (rotation & 1) ? s_bw : s_bh;

        if (ow != s_ow || oh != s_oh || !s_out) {
            uint8_t *n = realloc(s_out, (size_t)ow * (size_t)oh * 4);

            if (!n) {
                free(adj);
                return;
            }
            s_out = n;
            s_ow  = ow;
            s_oh  = oh;
        }

        for (y = 0; y < oh; y++) {
            uint8_t *orow = s_out + (size_t)y * s_ow * 4;

            for (x = 0; x < ow; x++) {
                int sx, sy;

                switch (rotation) {
                case 1:  sx = y;              sy = s_bh - 1 - x; break; /* 90 CW  */
                case 2:  sx = s_bw - 1 - x;   sy = s_bh - 1 - y; break; /* 180    */
                case 3:  sx = s_bw - 1 - y;   sy = x;            break; /* 90 CCW */
                default: sx = x;              sy = y;            break;
                }
                if (sx < 0) sx = 0;
                if (sy < 0) sy = 0;
                if (sx >= s_bw) sx = s_bw - 1;
                if (sy >= s_bh) sy = s_bh - 1;

                memcpy(orow + (size_t)x * 4,
                       adj + ((size_t)sy * s_bw + (size_t)sx) * 4, 4);
            }
        }
    }

    free(adj);
    s_serial++;
}

const uint8_t *picture_pixels(int *w, int *h, int *serial)
{
    if (w)      *w = s_ow;
    if (h)      *h = s_oh;
    if (serial) *serial = s_serial;
    return s_out;
}

int picture_source_w(void) { return s_bw; }
int picture_source_h(void) { return s_bh; }

const char *picture_last_error(void)
{
    return s_err;
}
