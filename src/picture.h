/* picture.h - still pictures for the picture viewer.
 *
 * Decodes JPEG / PNG / BMP (via the vendored stb_image) and keeps the decoded
 * image in memory; brightness, color saturation, sharpness and rotation are
 * applied into a second buffer when the viewer changes them, not per frame.
 * The viewer pans and zooms by moving the texture's source rectangle, so those
 * cost nothing.
 */
#ifndef PICTURE_H
#define PICTURE_H

#include <stdint.h>

/* Decodes path. Returns 0 on success, -1 on failure (picture_last_error()). */
int  picture_open(const char *path);
void picture_close(void);

/* Applies the viewing settings. brightness -100..100, saturation 0..250
 * (100 untouched), sharpness 0..100, rotation 0..3 (x90 degrees clockwise).
 * rb_swap 0 keeps the video byte order (b,g,r,a), 1 swaps red and blue - the
 * COLOR ORDER row in the picture settings, for files whose reds and blues
 * come out the wrong way round. Cheap enough for a slider, far too expensive
 * to do per frame. */
void picture_apply(int brightness, int saturation, int sharpness, int rotation,
                   int rb_swap);

/* Latest processed image as RGBA bytes in the byte order the renderer is fed
 * for video (b,g,r,a in memory), or NULL before the first picture_apply().
 * serial increments once per processed image, so the texture upload can be
 * skipped when nothing changed. */
const uint8_t *picture_pixels(int *w, int *h, int *serial);

/* The decoded picture's own size, before settings. */
int  picture_source_w(void);
int  picture_source_h(void);

const char *picture_last_error(void);

#endif /* PICTURE_H */
