/* vis_art.h - the painted visualizations (see vis_art.c). */
#ifndef VIS_ART_H
#define VIS_ART_H

#include <SDL2/SDL.h>
#include "ui.h"

/* Draws the style into a pixel canvas and blits it. Returns 1 if this module
 * owns the style, so the caller knows not to draw it again. */
int vis_art_draw(SDL_Renderer *r, const UiModel *m, int style,
                 int x, int y, int w, int h);

/* The resolution step this module is painting at: 1 full, 2 half, 3 a third.
 * The measurement line reports it, and the canvas is the frame size divided by
 * it, so the log says what a frame time was actually bought with. */
int vis_art_step(void);

/* What the whole frame cost, called once per frame by main. The ladder judges
 * this rather than the look's own drawing, because on the console the flush and
 * the flip are more than half of a frame. Ignored on frames that drew no look. */
void vis_art_frame_ms(double frame_ms);

/* High-water mark of the paint canvas, in bytes: the largest single buffer the
 * visualizers allocate, and the number the end-of-run memory line reports. */
size_t vis_art_mem_peak(void);

/* Throws away every baked layer: called when something the caches were built
 * from has changed - a config reload (R3), the skin, or a quality limit. The
 * canvas size is watched by the caches themselves, so a quality step that
 * changes it rebakes on its own. */
void vis_art_cache_invalidate(void);

#endif
