/* font_atlas.c - see font_atlas.h. */
#include "font_atlas.h"

#include <stdlib.h>
#include <string.h>

static SDL_Texture  *s_tex;
static SDL_Renderer *s_tex_owner;

/* Uploads the 8-bit coverage atlas once, as white pixels with that alpha. */
static SDL_Texture *atlas_texture(SDL_Renderer *r, const UiFontAtlas *f)
{
    SDL_Texture   *t;
    unsigned char *px;
    int i, n;

    if (s_tex && s_tex_owner == r)
        return s_tex;
    if (s_tex) {
        SDL_DestroyTexture(s_tex);
        s_tex = NULL;
        s_tex_owner = NULL;
    }

    t = SDL_CreateTexture(r, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                          f->aw, f->ah);
    if (!t)
        return NULL;

    n = f->aw * f->ah;
    px = (unsigned char *)malloc((size_t)n * 4);
    if (!px) {
        SDL_DestroyTexture(t);
        return NULL;
    }
    for (i = 0; i < n; i++) {
        px[i*4 + 0] = 255;             /* B */
        px[i*4 + 1] = 255;             /* G */
        px[i*4 + 2] = 255;             /* R */
        px[i*4 + 3] = f->alpha[i];     /* A */
    }
    SDL_UpdateTexture(t, NULL, px, f->aw * 4);
    free(px);

    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    s_tex = t;
    s_tex_owner = r;
    return t;
}

void ui_font_draw_a(SDL_Renderer *r, const UiFontAtlas *f, int x, int y, int px,
                    const char *s, int cr, int cg, int cb, int alpha)
{
    SDL_Texture *t;
    float k;
    int baseline;

    if (!f || !s || alpha <= 0)
        return;
    t = atlas_texture(r, f);
    if (!t)
        return;

    if (alpha > 255) alpha = 255;
    k = (float)px / (float)(f->base_h > 0 ? f->base_h : 1);
    baseline = y + (int)(f->ascent * k + 0.5f);
    SDL_SetTextureColorMod(t, (Uint8)cr, (Uint8)cg, (Uint8)cb);
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);

    for (; *s; s++) {
        int c = (unsigned char)*s;
        const UiGlyphAtlas *g;

        if (c < f->first || c >= f->first + f->count) {
            x += (int)(px * 0.5f);
            continue;
        }
        g = &f->glyphs[c - f->first];
        if (g->w && g->h) {
            SDL_Rect src, dst;
            src.x = g->x; src.y = g->y; src.w = g->w; src.h = g->h;
            dst.x = x;
            dst.y = baseline + (int)(g->oy * k);
            dst.w = (int)(g->w * k + 0.5f);
            dst.h = (int)(g->h * k + 0.5f);
            if (dst.w < 1) dst.w = 1;
            if (dst.h < 1) dst.h = 1;
            SDL_RenderCopy(r, t, &src, &dst);
        }
        x += (int)(g->adv * k + 0.5f);
    }
    SDL_SetTextureAlphaMod(t, 255);      /* later draws are opaque again */
}

void ui_font_draw(SDL_Renderer *r, const UiFontAtlas *f, int x, int y, int px,
                  const char *s, int cr, int cg, int cb)
{
    ui_font_draw_a(r, f, x, y, px, s, cr, cg, cb, 255);
}

int ui_font_width(const UiFontAtlas *f, int px, const char *s)
{
    float k;
    int w = 0;

    if (!f)
        return 0;
    k = (float)px / (float)(f->base_h > 0 ? f->base_h : 1);
    for (; s && *s; s++) {
        int c = (unsigned char)*s;
        if (c < f->first || c >= f->first + f->count) {
            w += (int)(px * 0.5f);
            continue;
        }
        w += (int)(f->glyphs[c - f->first].adv * k + 0.5f);
    }
    return w;
}
