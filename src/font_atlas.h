/* font_atlas.h - drawing text from a pre-rendered glyph atlas.
 *
 * The console build cannot rasterise a font at runtime, so tools/make-font-atlas.py
 * renders a TrueType face into an 8-bit alpha atlas on a PC and emits it as a C
 * header. This module blits those glyphs as one texture, which keeps the
 * anti-aliasing but costs one draw call per glyph instead of ~64 rectangles.
 *
 * The atlas header defines a single `ui_font` object of type UiFontAtlas.
 */
#ifndef FONT_ATLAS_H
#define FONT_ATLAS_H

#include <SDL2/SDL.h>

typedef struct {
    unsigned short x, y, w, h;   /* glyph rectangle inside the atlas */
    unsigned short adv;          /* advance width, in atlas pixels */
    short          oy;           /* bitmap top, relative to the baseline */
} UiGlyphAtlas;

typedef struct {
    const unsigned char      *alpha;   /* atlas, 8-bit coverage, row major */
    int                       aw, ah;
    const UiGlyphAtlas       *glyphs;
    int                       first;   /* first code point (usually 32) */
    int                       count;
    int                       base_h;  /* pixel size the atlas was rendered at */
    int                       ascent;
} UiFontAtlas;

/* Draws s with its top-left at (x, y); px is the line height to render at. */
void ui_font_draw(SDL_Renderer *r, const UiFontAtlas *f, int x, int y, int px,
                  const char *s, int cr, int cg, int cb);

/* Same, with 0..255 opacity, for things that fade out (the subtitle toast). */
void ui_font_draw_a(SDL_Renderer *r, const UiFontAtlas *f, int x, int y, int px,
                    const char *s, int cr, int cg, int cb, int alpha);

/* Width the string will occupy at that line height. */
int  ui_font_width(const UiFontAtlas *f, int px, const char *s);

#endif /* FONT_ATLAS_H */
