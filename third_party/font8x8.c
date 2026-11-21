/* font8x8.c - 8x8 bitmap font used for the on-screen text.
 *
 * Table: dhepper/font8x8 (public domain). One byte per row, bit 0 = leftmost
 * pixel, rows top to bottom. Only printable ASCII 0x20..0x7E is meaningful.
 */
#include "font8x8_basic.h"

const unsigned char *media_font_char(unsigned char c)
{
    if (c < 0x20 || c > 0x7E)
        c = '?';
    return font8x8_basic[c];
}
