#define MINIMP3_IMPLEMENTATION
#include "mp3_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int mp3_open(Mp3Source *s, const char *path)
{
    FILE *f;
    long  len;

    memset(s, 0, sizeof(*s));

    f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -2;
    }
    len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return -3;
    }
    rewind(f);

    s->data = (uint8_t *)malloc((size_t)len);
    if (!s->data) {
        fclose(f);
        return -4;
    }
    if (fread(s->data, 1, (size_t)len, f) != (size_t)len) {
        free(s->data);
        s->data = NULL;
        fclose(f);
        return -5;
    }
    fclose(f);

    s->size = (size_t)len;
    mp3dec_init(&s->dec);
    return 0;
}

void mp3_close(Mp3Source *s)
{
    if (s->data)
        free(s->data);
    s->data = NULL;
    s->size = s->pos = 0;
}

int mp3_read(Mp3Source *s, float *out, int max_frames)
{
    mp3dec_frame_info_t info;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int   samples;
    int   i, n;

    if (!s->data || s->pos >= s->size || !out || max_frames <= 0) {
        s->finished = 1;
        return 0;
    }

    memset(&info, 0, sizeof(info));
    samples = mp3dec_decode_frame(&s->dec, s->data + s->pos,
                                  (int)(s->size - s->pos), pcm, &info);

    if (info.frame_bytes == 0) {
        /* No sync word here: skip one byte and try again, like the reference
         * player does, so ID3 tags and junk between frames are tolerated. */
        s->pos++;
        if (s->pos >= s->size)
            s->finished = 1;
        return 0;
    }
    s->pos += (size_t)info.frame_bytes;

    /* Only trust a header that actually parsed. Trailing/padding "frames"
     * report frame_bytes > 0 with hz == 0, and must not wipe the format we
     * already know - that bug made the duration come out as 0. */
    if (info.hz > 0 && info.channels > 0) {
        s->hz           = info.hz;
        s->channels     = info.channels;
        s->bitrate_kbps = info.bitrate_kbps;
    }

    if (samples <= 0) {
        if (s->pos >= s->size)
            s->finished = 1;
        return 0;
    }

    n = samples;
    if (n > max_frames)
        n = max_frames;

    for (i = 0; i < n * info.channels; i++)
        out[i] = (float)pcm[i] / 32768.0f;

    s->frames_decoded += n;
    if (s->pos >= s->size)
        s->finished = 1;
    return n;
}

double mp3_duration_est(const Mp3Source *s)
{
    /* Once the file has been decoded the frame count is exact, which beats the
     * byte/bitrate guess (ID3 tags and padding skew that badly). */
    if (s->finished && s->hz > 0 && s->frames_decoded > 0)
        return (double)s->frames_decoded / (double)s->hz;
    if (s->bitrate_kbps <= 0)
        return 0.0;
    return ((double)s->size * 8.0) / ((double)s->bitrate_kbps * 1000.0);
}
