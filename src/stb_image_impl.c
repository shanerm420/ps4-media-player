/* One definition of stb_image for the whole player.
 *
 * It used to live inside picture.c, but the header emblem art is decoded from
 * memory in ui.c, and the preview build does not compile picture.c. Keeping the
 * implementation in its own file means both builds link the same decoder. */

#define STB_IMAGE_IMPLEMENTATION
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
