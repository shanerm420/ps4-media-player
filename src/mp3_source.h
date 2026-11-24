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

/* Jumps to sec seconds in: sets the read position to the next frame header at
 * the byte the file's own bitrate says that time lands on, resets the decoder
 * and moves the frame counter so the clock and the bar follow the jump.
 * Returns 0 when the seek happened, negative when there is nothing to seek. */
int mp3_seek_seconds(Mp3Source *s, double sec);

/* Diagnostics for a file that will not decode: the ID3v2 tag length, the
 * offset of the first MPEG frame header, and the container guessed from the
 * first bytes ("mp3", "id3", "aac-adts", "m4a", "wav", "ogg"). */
size_t      mp3_id3_size(const Mp3Source *s);
size_t      mp3_first_sync(const Mp3Source *s);
const char *mp3_container(const Mp3Source *s);

#endif /* MP3_SOURCE_H */
