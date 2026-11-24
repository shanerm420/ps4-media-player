/* ps_mark.h - the PlayStation mark as an alpha silhouette.
 *
 * Traced from the app's own emblem art (tools/make-ps-mark.py) and stored as
 * one byte of coverage per pixel, so the tesseract look can extrude the real
 * logo: every layer is the actual outline, not an approximation of it.
 */
#ifndef PS_MARK_ALPHA_H
#define PS_MARK_ALPHA_H

#define PS_MARK_W 160
#define PS_MARK_HEIGHT 128

extern const unsigned char ps_mark_alpha[PS_MARK_W * PS_MARK_HEIGHT];

#endif
