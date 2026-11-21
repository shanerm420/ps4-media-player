/* mp3_source.h - minimp3 based MP3 file source. */
#ifndef MP3_SOURCE_H
#define MP3_SOURCE_H

#include <stdint.h>
#include <stddef.h>

#include "minimp3.h"

typedef struct {
    uint8_t *data;
    size_t   size;
    size_t   pos;
    mp3dec_t dec;
    int      hz;            /* sample rate of the last decoded frame */
    int      channels;      /* 1 or 2 */
    int      bitrate_kbps;
    long long frames_decoded;
    int      finished;
} Mp3Source;

/* Reads the whole file into memory. Returns 0 on success, negative errno-ish. */
int  mp3_open(Mp3Source *s, const char *path);
void mp3_close(Mp3Source *s);

/* Decodes the next MPEG frame. Writes at most max_frames per channel into
 * out (interleaved floats), returns frames written, 0 at end of stream. */
int  mp3_read(Mp3Source *s, float *out, int max_frames);

/* Byte-rate estimate of total length; good enough for a progress bar. */
double mp3_duration_est(const Mp3Source *s);

#endif /* MP3_SOURCE_H */
