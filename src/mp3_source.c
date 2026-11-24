#define MINIMP3_IMPLEMENTATION
#include "mp3_source.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The audio thread reads frames while the main thread seeks, and the two used
 * to race: a decode that had already read the file position wrote it back
 * afterwards, so the seek was undone a fraction of a millisecond after it was
 * made. The clock jumped and the music did not, which is exactly what "L2 and
 * R2 do not rewind" looked like. Every entry point takes this lock now, so a
 * seek can never be overwritten by an in-flight frame. */
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

int mp3_open(Mp3Source *s, const char *path)
{
    FILE *f;
    long  len;

    pthread_mutex_lock(&s_lock);
    memset(s, 0, sizeof(*s));

    f = fopen(path, "rb");
    if (!f) {
        pthread_mutex_unlock(&s_lock);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        pthread_mutex_unlock(&s_lock);
        return -2;
    }
    len = ftell(f);
    if (len <= 0) {
        fclose(f);
        pthread_mutex_unlock(&s_lock);
        return -3;
    }
    rewind(f);

    s->data = (uint8_t *)malloc((size_t)len);
    if (!s->data) {
        fclose(f);
        pthread_mutex_unlock(&s_lock);
        return -4;
    }
    if (fread(s->data, 1, (size_t)len, f) != (size_t)len) {
        free(s->data);
        s->data = NULL;
        fclose(f);
        pthread_mutex_unlock(&s_lock);
        return -5;
    }
    fclose(f);

    s->size = (size_t)len;
    mp3dec_init(&s->dec);
    pthread_mutex_unlock(&s_lock);
    return 0;
}

void mp3_close(Mp3Source *s)
{
    pthread_mutex_lock(&s_lock);
    if (s->data)
        free(s->data);
    s->data = NULL;
    s->size = s->pos = 0;
    pthread_mutex_unlock(&s_lock);
}

int mp3_read(Mp3Source *s, float *out, int max_frames)
{
    mp3dec_frame_info_t info;
    short pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int   samples;
    int   i, n;

    pthread_mutex_lock(&s_lock);
    if (!s->data || s->pos >= s->size || !out || max_frames <= 0) {
        s->finished = 1;
        pthread_mutex_unlock(&s_lock);
        return 0;
    }

    memset(&info, 0, sizeof(info));
    samples = mp3dec_decode_frame(&s->dec, s->data + s->pos,
                                  (int)(s->size - s->pos), pcm, &info);

    if (info.frame_bytes == 0) {
        /* No sync word here: move straight to the next byte that could start
         * a frame and try again. Advancing one byte per call meant a file
         * with an ID3v2 tag (cover art) needed tens of thousands of calls
         * before it reached its first frame - and the playback loop sleeps a
         * millisecond on each empty read, so such a file looked like it would
         * not play at all and skipped to the next song. */
        size_t p = s->pos + 1;

        while (p + 1 < s->size &&
               !(s->data[p] == 0xFF && (s->data[p + 1] & 0xE0) == 0xE0))
            p++;
        s->pos = p;
        if (s->pos >= s->size)
            s->finished = 1;
        pthread_mutex_unlock(&s_lock);
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
        pthread_mutex_unlock(&s_lock);
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
    pthread_mutex_unlock(&s_lock);
    return n;
}

/* ------------------------------------------------------------------ */
/* Diagnostics                                                         */
/* ------------------------------------------------------------------ */

size_t mp3_id3_size(const Mp3Source *s)
{
    if (!s->data || s->size < 10)
        return 0;
    if (s->data[0] != 'I' || s->data[1] != 'D' || s->data[2] != '3')
        return 0;
    /* Syncsafe integer: seven bits per byte, which is what keeps the size
     * from ever containing a false frame sync. */
    return ((size_t)(s->data[6] & 0x7F) << 21)
         | ((size_t)(s->data[7] & 0x7F) << 14)
         | ((size_t)(s->data[8] & 0x7F) << 7)
         |  (size_t)(s->data[9] & 0x7F);
}

size_t mp3_first_sync(const Mp3Source *s)
{
    size_t p;

    if (!s->data || s->size < 2)
        return 0;
    p = mp3_id3_size(s);
    if (p)
        p += 10;                        /* 'ID3' + version + flags + size */
    for (; p + 1 < s->size; p++)
        if (s->data[p] == 0xFF && (s->data[p + 1] & 0xE0) == 0xE0)
            return p;
    return s->size;                     /* no frame header anywhere */
}

const char *mp3_container(const Mp3Source *s)
{
    if (!s->data || s->size < 12)
        return "short";
    if (s->data[0] == 'I' && s->data[1] == 'D' && s->data[2] == '3') {
        size_t sync = mp3_first_sync(s);

        return sync >= s->size ? "id3-only" : "id3";
    }
    if (!memcmp(s->data + 4, "ftyp", 4))
        return "m4a";
    if (!memcmp(s->data, "RIFF", 4))
        return "wav";
    if (!memcmp(s->data, "OggS", 4))
        return "ogg";
    if (s->data[0] == 0xFF && (s->data[1] & 0xE0) == 0xE0)
        /* Layer bits 00 are ADTS AAC, 01/10/11 are MPEG audio. */
        return (s->data[1] & 0x06) == 0 ? "aac-adts" : "mp3";
    return "unknown";
}

int mp3_seek_seconds(Mp3Source *s, double sec)
{
    size_t start, p, size_bytes;
    int    kbps;
    double rate;                 /* bytes per second, measured on this file */

    if (!s || !s->data || s->size < 4 || s->hz <= 0)
        return -1;

    pthread_mutex_lock(&s_lock);

    start = mp3_first_sync(s);
    if (start + 2 >= s->size) {
        pthread_mutex_unlock(&s_lock);
        return -1;
    }

    /* How fast this file actually is, in bytes per second. The header's
     * bitrate is only right for a constant-bitrate file; a VBR track has a
     * different one on almost every frame, and seeking by the first header's
     * number landed most of a collection somewhere other than the second it
     * asked for. Once half a second has been decoded the bytes the file has
     * given per second of audio is a fact, not an estimate. */
    kbps = s->bitrate_kbps > 0 ? s->bitrate_kbps : 128;
    rate = (double)kbps * 1000.0 / 8.0;
    if (s->frames_decoded > s->hz / 2) {
        double secs = (double)s->frames_decoded / (double)s->hz;
        double measured = (double)(s->pos > start ? s->pos - start : 0) / secs;

        if (measured > 1000.0)
            rate = measured;
    }

    size_bytes = sec > 0.0 ? (size_t)(sec * rate) : 0;
    p = start + size_bytes;
    if (p + 2 >= s->size)
        p = s->size - 3;

    while (p + 1 < s->size &&
           !(s->data[p] == 0xFF && (s->data[p + 1] & 0xE0) == 0xE0))
        p++;
    if (p + 1 >= s->size) {
        pthread_mutex_unlock(&s_lock);
        return -1;
    }

    s->pos = p;
    mp3dec_init(&s->dec);
    /* The clock is the decoded-frame counter, so it has to move with the
     * jump or the progress bar would restart from zero. */
    if (sec < 0.0)
        sec = 0.0;
    s->frames_decoded = (long long)(sec * (double)s->hz);
    s->finished = 0;
    pthread_mutex_unlock(&s_lock);
    return 0;
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
