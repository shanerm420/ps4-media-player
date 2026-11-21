/* video_orbis.c - MP4/MOV playback through the system AvPlayer.
 *
 * libSceAvPlayer demuxes and decodes; everything around it is ours:
 *
 *   - the four allocators it refuses to initialise without,
 *   - file I/O, so a path straight out of /data or /usb0 can be played,
 *   - pulling decoded frames (8-bit YUV420, chroma interleaved as NV12),
 *   - converting them to RGBA for the SDL renderer,
 *   - turning the 16-bit PCM it returns into the same float stereo the MP3
 *     path feeds into the EQ / volume / limiter chain.
 *
 * The ABI is not published by OpenOrbis: include/orbis/AvPlayer.h declares
 * every entry point as `void sceAvPlayerInit();` - no arguments, no structs.
 * The layouts below are taken from two independent PS4 implementations,
 * shadPS4 (core/libraries/avplayer/avplayer.h) and fpPS4
 * (ps4_libsceavplayer.pas), which agree field for field. The symbols link
 * against the firmware stub library the toolchain ships
 * (lib/libSceAvPlayer.so), so the loader binds them at startup.
 *
 * This code has been compiled and linked, but has not yet been run on a
 * console. The two assumptions worth knowing about are marked below.
 */
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "resampler.h"
#include "video_orbis.h"

/* ------------------------------------------------------------------ */
/* The ABI, as recovered from shadPS4 and fpPS4                        */
/* ------------------------------------------------------------------ */

typedef uint64_t SceAvPlayerHandle;

typedef void *(*AvAlloc)(void *p, uint32_t align, uint32_t size);
typedef void  (*AvFree)(void *p, void *mem);

typedef struct {
    void   *object;
    AvAlloc allocate;
    AvFree  deallocate;
    AvAlloc allocate_texture;
    AvFree  deallocate_texture;
} AvMemAllocator;

typedef int32_t  (*AvOpenFile)(void *p, const char *name);
typedef int32_t  (*AvCloseFile)(void *p);
typedef int32_t  (*AvReadFile)(void *p, uint8_t *buf, uint64_t pos, uint32_t len);
typedef uint64_t (*AvSizeFile)(void *p);

typedef struct {
    void        *object;
    AvOpenFile   open;
    AvCloseFile  close;
    AvReadFile   read_offset;
    AvSizeFile   size;
} AvFileReplacement;

typedef void (*AvEventCallback)(void *p, int32_t event, int32_t source, void *data);

typedef struct {
    void            *object;
    AvEventCallback  callback;
} AvEventReplacement;

typedef struct {
    AvMemAllocator   memory;
    AvFileReplacement file;
    AvEventReplacement event;
    uint32_t         debug_level;
    uint32_t         base_priority;
    int32_t          video_frame_buffers;
    uint8_t          auto_start;
    uint8_t          reserved[3];
    const char      *default_language;
} AvInitData;

typedef struct {
    uint16_t channel_count;
    uint8_t  reserved[2];
    uint32_t sample_rate;
    uint32_t size;                 /* bytes of interleaved s16 */
    char     language[4];
} AvAudio;

typedef struct {
    uint32_t width;
    uint32_t height;
    float    aspect_ratio;
    char     language[4];
} AvVideo;

typedef struct {
    char     language[4];
    uint16_t text_size;
    uint16_t font_size;
    uint16_t top, left, bottom, right;
} AvTimedText;

typedef union {
    uint8_t     reserved[16];
    AvAudio     audio;
    AvVideo     video;
    AvTimedText text;
} AvStreamDetails;

typedef struct {
    uint8_t         *data;
    uint32_t         reserved;
    uint32_t         reserved2;
    uint64_t         timestamp_ms;
    AvStreamDetails  details;
} AvFrameInfo;

typedef struct {
    uint16_t channel_count;
    uint8_t  reserved[2];
    uint32_t sample_rate;
    uint32_t size;
    char     language[4];
    uint8_t  reserved1[64];
} AvAudioEx;

typedef struct {
    uint32_t width;
    uint32_t height;
    float    aspect_ratio;
    char     language[4];
    uint32_t framerate;
    uint32_t crop_left;
    uint32_t crop_right;
    uint32_t crop_top;
    uint32_t crop_bottom;
    uint32_t pitch;                /* bytes per row of the luma plane */
    uint8_t  luma_bits;
    uint8_t  chroma_bits;
    uint8_t  full_range;
    uint8_t  reserved[37];
} AvVideoEx;

typedef struct {
    char     language[4];
    uint8_t  reserved[76];
} AvTimedTextEx;

typedef union {
    AvAudioEx    audio;
    AvVideoEx    video;
    AvTimedTextEx text;
    uint8_t      reserved[80];
} AvStreamDetailsEx;

typedef struct {
    uint8_t            *data;
    uint32_t            reserved;
    uint32_t            reserved2;
    uint64_t            timestamp_ms;
    AvStreamDetailsEx   details;
} AvFrameInfoEx;

typedef struct {
    uint32_t         type;          /* 0 video, 1 audio, 2 text */
    uint32_t         reserved;
    AvStreamDetails  details;
    uint64_t         duration_ms;
    uint64_t         start_ms;
} AvStreamInfo;

/* The layouts below are the contract with the firmware. If a future edit gets
 * one of these sizes wrong, the call would read the wrong offsets at runtime. */
_Static_assert(sizeof(AvInitData)    == 120, "AvInitData layout");
_Static_assert(sizeof(AvFrameInfo)   == 40,  "AvFrameInfo layout");
_Static_assert(sizeof(AvFrameInfoEx) == 104, "AvFrameInfoEx layout");
_Static_assert(sizeof(AvStreamInfo)  == 40,  "AvStreamInfo layout");

extern SceAvPlayerHandle sceAvPlayerInit(AvInitData *data);
extern int32_t  sceAvPlayerAddSource(SceAvPlayerHandle h, const char *path);
extern int32_t  sceAvPlayerStart(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerStop(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerClose(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerPause(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerResume(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerJumpToTime(SceAvPlayerHandle h, uint64_t ms);
extern uint64_t sceAvPlayerCurrentTime(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerStreamCount(SceAvPlayerHandle h);
extern int32_t  sceAvPlayerGetStreamInfo(SceAvPlayerHandle h, uint32_t id,
                                         AvStreamInfo *info);
extern _Bool    sceAvPlayerIsActive(SceAvPlayerHandle h);
extern _Bool    sceAvPlayerGetVideoDataEx(SceAvPlayerHandle h, AvFrameInfoEx *f);
extern _Bool    sceAvPlayerGetAudioData(SceAvPlayerHandle h, AvFrameInfo *f);

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

#define AB_FRAMES    8192          /* audio ring, stereo frames */
#define AB_PULL_MAX  4096          /* frames we handle from one packet */

typedef struct {
    FILE           *file;
    pthread_mutex_t lock;
} AvFileCtx;

static struct {
    int              inited;
    int              paused;
    int              finished;
    SceAvPlayerHandle handle;

    AvFileCtx        file;
    pthread_mutex_t  lock;

    int              width, height;
    double           duration_ms;
    double           position_ms;

    uint8_t         *rgba;
    size_t           rgba_cap;
    int              serial;
    int              frame_w, frame_h;

    Resampler        rs;
    int              rs_rate;
    float            abuf[AB_FRAMES * 2];
    int              ahead, acount;

    uint64_t         opened_ms;
    char             format[64];
    char             err[192];
} s_av;

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

#define LOCK()   pthread_mutex_lock(&s_av.lock)
#define UNLOCK() pthread_mutex_unlock(&s_av.lock)

static uint64_t now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000);
}

static void set_error(const char *what, int code)
{
    if (code)
        snprintf(s_av.err, sizeof(s_av.err), "%s (0x%08X)", what, (unsigned)code);
    else
        snprintf(s_av.err, sizeof(s_av.err), "%s", what);
}

/* Allocation with an alignment the decoder asks for. The base pointer is
 * stashed immediately before the aligned block so free() gets it back. */
static void *aligned_alloc_like(uint32_t align, uint32_t size)
{
    void     *base;
    uintptr_t p;

    if (align < 16)
        align = 16;
    base = malloc((size_t)size + align + 16);
    if (!base)
        return NULL;

    p = ((uintptr_t)base + 16 + align - 1) & ~(uintptr_t)(align - 1);
    ((void **)p)[-1] = base;
    return (void *)p;
}

static void aligned_free_like(void *mem)
{
    if (mem)
        free(((void **)mem)[-1]);
}

static void *av_alloc(void *p, uint32_t align, uint32_t size)
{
    (void)p;
    return aligned_alloc_like(align, size);
}

static void av_free(void *p, void *mem)
{
    (void)p;
    aligned_free_like(mem);
}

/* ------------------------------------------------------------------ */
/* File I/O behind the decoder                                         */
/* ------------------------------------------------------------------ */

static int32_t av_file_open(void *p, const char *name)
{
    AvFileCtx *c = p;
    FILE *f;

    if (!name)
        return -1;
    f = fopen(name, "rb");
    if (!f)
        return -1;

    pthread_mutex_lock(&c->lock);
    if (c->file)
        fclose(c->file);
    c->file = f;
    pthread_mutex_unlock(&c->lock);
    return 0;
}

static int32_t av_file_close(void *p)
{
    AvFileCtx *c = p;

    pthread_mutex_lock(&c->lock);
    if (c->file) {
        fclose(c->file);
        c->file = NULL;
    }
    pthread_mutex_unlock(&c->lock);
    return 0;
}

static int32_t av_file_read(void *p, uint8_t *buf, uint64_t pos, uint32_t len)
{
    AvFileCtx *c = p;
    size_t got = 0;

    if (!buf)
        return -1;

    pthread_mutex_lock(&c->lock);
    if (c->file && fseek(c->file, (long)pos, SEEK_SET) == 0) {
        got = fread(buf, 1, len, c->file);
        if (got < (size_t)len && ferror(c->file))
            got = 0;
    } else {
        pthread_mutex_unlock(&c->lock);
        return -1;
    }
    pthread_mutex_unlock(&c->lock);

    /* A short read at end of file is normal; only a hard failure is -1. */
    return got == 0 ? 0 : (int32_t)got;
}

static uint64_t av_file_size(void *p)
{
    AvFileCtx *c = p;
    uint64_t size = 0;

    pthread_mutex_lock(&c->lock);
    if (c->file && fseek(c->file, 0, SEEK_END) == 0) {
        long end = ftell(c->file);
        if (end > 0)
            size = (uint64_t)end;
        fseek(c->file, 0, SEEK_SET);
    }
    pthread_mutex_unlock(&c->lock);
    return size;
}

/* ------------------------------------------------------------------ */
/* Video: YUV420 (NV12) -> RGBA                                        */
/* ------------------------------------------------------------------ */

static uint8_t clamp_u8(int v)
{
    if (v < 0)
        return 0;
    if (v > 255)
        return 255;
    return (uint8_t)v;
}

/* ASSUMPTION 1: the decoded frame is 8-bit YUV420 with the chroma planes
 * interleaved (U,V,U,V - NV12), laid out as one luma plane of pitch*height
 * bytes followed by the chroma plane at the same pitch. Both reference
 * implementations pack frames this way. If a title shows correct geometry
 * but wrong colours, this is the line to revisit: swap the two chroma reads
 * for separate planes and it becomes planar I420. */
static void convert_nv12(const AvFrameInfoEx *fi)
{
    const AvVideoEx *v = &fi->details.video;
    const int      w = (int)v->width;
    const int      h = (int)v->height;
    const uint32_t pitch = v->pitch ? v->pitch : (uint32_t)w;
    const uint8_t *y  = fi->data;
    const uint8_t *uv = fi->data + (size_t)pitch * (size_t)h;
    uint8_t       *dst;
    int x, row;

    if (!y || w <= 0 || h <= 0)
        return;
    if (v->luma_bits > 8 || v->chroma_bits > 8)
        return;                             /* 10-bit: reported elsewhere */

    if ((size_t)w * (size_t)h > s_av.rgba_cap / 4) {
        uint8_t *n = realloc(s_av.rgba, (size_t)w * (size_t)h * 4);
        if (!n)
            return;
        s_av.rgba    = n;
        s_av.rgba_cap = (size_t)w * (size_t)h * 4;
    }

    dst = s_av.rgba;

    for (row = 0; row < h; row++) {
        const uint8_t *yr  = y + (size_t)row * pitch;
        const uint8_t *uvr = uv + (size_t)(row >> 1) * pitch;
        uint8_t       *out = dst + (size_t)row * (size_t)w * 4;

        for (x = 0; x < w; x += 2) {
            int u = uvr[x & ~1] - 128;
            int vv = uvr[(x & ~1) + 1] - 128;
            int c0 = 298 * (yr[x] - 16);
            int c1 = (x + 1 < w) ? 298 * (yr[x + 1] - 16) : c0;
            uint8_t *o = out + (size_t)x * 4;

            /* BT.601 studio range, 8-bit fixed point */
            o[0] = clamp_u8((c0 + 409 * vv + 128) >> 8);
            o[1] = clamp_u8((c0 - 100 * u - 208 * vv + 128) >> 8);
            o[2] = clamp_u8((c0 + 516 * u + 128) >> 8);
            o[3] = 255;

            if (x + 1 < w) {
                o[4] = clamp_u8((c1 + 409 * vv + 128) >> 8);
                o[5] = clamp_u8((c1 - 100 * u - 208 * vv + 128) >> 8);
                o[6] = clamp_u8((c1 + 516 * u + 128) >> 8);
                o[7] = 255;
            }
        }
    }

    s_av.frame_w = w;
    s_av.frame_h = h;
    s_av.serial++;
}

/* ------------------------------------------------------------------ */
/* Audio: interleaved s16 -> float stereo at 48 kHz                    */
/* ------------------------------------------------------------------ */

static void ab_push(const float *in, int frames)
{
    int i, c;

    if (frames <= 0)
        return;
    if (frames > AB_FRAMES - s_av.acount)
        frames = AB_FRAMES - s_av.acount;

    for (i = 0; i < frames; i++) {
        int slot = (s_av.ahead + s_av.acount) % AB_FRAMES;
        for (c = 0; c < 2; c++)
            s_av.abuf[slot * 2 + c] = in[i * 2 + c];
        s_av.acount++;
    }
}

static void pull_audio(void)
{
    AvFrameInfo fi;
    int ch, rate, frames, i, n;
    static float raw[AB_PULL_MAX * 2];
    static float conv[AB_PULL_MAX * 2 * 4];

    memset(&fi, 0, sizeof(fi));
    if (!sceAvPlayerGetAudioData(s_av.handle, &fi) || !fi.data)
        return;

    ch    = fi.details.audio.channel_count;
    rate  = (int)fi.details.audio.sample_rate;
    if (ch <= 0 || rate <= 0 || fi.details.audio.size < (uint32_t)(ch * 2))
        return;

    frames = (int)(fi.details.audio.size / (uint32_t)(ch * 2));
    if (frames > AB_PULL_MAX)
        frames = AB_PULL_MAX;

    /* s16 -> float, down to stereo, duplicating mono */
    {
        const int16_t *s = (const int16_t *)fi.data;
        for (i = 0; i < frames; i++) {
            if (ch == 1) {
                float v = s[i] / 32768.0f;
                raw[i * 2] = v;
                raw[i * 2 + 1] = v;
            } else {
                raw[i * 2]     = s[i * ch] / 32768.0f;
                raw[i * 2 + 1] = s[i * ch + 1] / 32768.0f;
            }
        }
    }

    if (rate != s_av.rs_rate) {
        rs_init(&s_av.rs, rate, 48000, 2);
        s_av.rs_rate = rate;
    }

    n = rs_process(&s_av.rs, raw, frames, conv, AB_PULL_MAX * 4);
    if (n > 0)
        ab_push(conv, n);
}

/* ------------------------------------------------------------------ */
/* Open / close                                                        */
/* ------------------------------------------------------------------ */

int video_open(const char *path)
{
    AvInitData data;
    AvStreamInfo info;
    int32_t rc, streams, i;
    int aud_rate = 0;

    video_close();

    if (!path)
        return -1;

    if (!s_av.inited) {
        pthread_mutex_init(&s_av.lock, NULL);
        pthread_mutex_init(&s_av.file.lock, NULL);
        s_av.rs_rate = 0;
        s_av.inited  = 1;
    }

    memset(&data, 0, sizeof(data));
    data.memory.object             = NULL;
    data.memory.allocate           = av_alloc;
    data.memory.deallocate         = av_free;
    data.memory.allocate_texture   = av_alloc;
    data.memory.deallocate_texture = av_free;

    data.file.object      = &s_av.file;
    data.file.open        = av_file_open;
    data.file.close       = av_file_close;
    data.file.read_offset = av_file_read;
    data.file.size        = av_file_size;

    data.debug_level         = 0;
    data.base_priority       = 0;
    data.video_frame_buffers = 4;
    data.auto_start          = 1;
    data.default_language    = "ENG";

    LOCK();
    s_av.handle = sceAvPlayerInit(&data);
    if (!s_av.handle) {
        UNLOCK();
        set_error("AvPlayer refused to start", 0);
        return -1;
    }

    rc = sceAvPlayerAddSource(s_av.handle, path);
    if (rc < 0) {
        sceAvPlayerClose(s_av.handle);
        s_av.handle = 0;
        UNLOCK();
        set_error("Cannot open video", rc);
        return -1;
    }

    /* Ask the demuxer what is in the file before playback starts. */
    s_av.width = s_av.height = 0;
    s_av.duration_ms = 0;
    streams = sceAvPlayerStreamCount(s_av.handle);
    for (i = 0; i < streams; i++) {
        memset(&info, 0, sizeof(info));
        if (sceAvPlayerGetStreamInfo(s_av.handle, (uint32_t)i, &info) != 0)
            continue;
        if (info.type == 0 && s_av.width == 0) {
            s_av.width  = (int)info.details.video.width;
            s_av.height = (int)info.details.video.height;
            s_av.duration_ms = (double)info.duration_ms;
        } else if (info.type == 1 && aud_rate == 0) {
            if (info.details.audio.sample_rate > 0)
                aud_rate = (int)info.details.audio.sample_rate;
        }
    }

    s_av.position_ms = 0;
    s_av.paused      = 0;
    s_av.finished    = 0;
    s_av.serial      = 0;
    s_av.frame_w     = 0;
    s_av.frame_h     = 0;
    s_av.ahead       = 0;
    s_av.acount      = 0;
    s_av.opened_ms   = now_ms();

    s_av.rs_rate = aud_rate;
    if (aud_rate > 0)
        rs_init(&s_av.rs, aud_rate, 48000, 2);

    if (s_av.width > 0 && s_av.height > 0)
        snprintf(s_av.format, sizeof(s_av.format), "MP4   %d x %d",
                 s_av.width, s_av.height);
    else
        snprintf(s_av.format, sizeof(s_av.format), "MP4   H.264");

    rc = sceAvPlayerStart(s_av.handle);
    if (rc < 0) {
        sceAvPlayerClose(s_av.handle);
        s_av.handle = 0;
        UNLOCK();
        set_error("Playback would not start", rc);
        return -1;
    }
    UNLOCK();

    s_av.err[0] = '\0';
    return 0;
}

void video_close(void)
{
    if (!s_av.inited || !s_av.handle)
        return;

    LOCK();
    sceAvPlayerStop(s_av.handle);
    sceAvPlayerClose(s_av.handle);
    s_av.handle   = 0;
    s_av.paused   = 0;
    s_av.finished = 0;
    s_av.acount   = 0;
    UNLOCK();
    av_file_close(&s_av.file);
}

/* ------------------------------------------------------------------ */
/* Playback                                                            */
/* ------------------------------------------------------------------ */

int video_step(void)
{
    int pulled;

    if (!s_av.inited || !s_av.handle)
        return -1;
    if (s_av.finished)
        return 0;

    LOCK();

    for (pulled = 0; pulled < 4; pulled++) {
        AvFrameInfoEx fi;

        memset(&fi, 0, sizeof(fi));
        if (!sceAvPlayerGetVideoDataEx(s_av.handle, &fi) || !fi.data)
            break;
        if (fi.details.video.luma_bits > 8) {
            snprintf(s_av.err, sizeof(s_av.err),
                     "10-bit video is not supported");
        } else {
            convert_nv12(&fi);
        }
    }

    s_av.position_ms = (double)sceAvPlayerCurrentTime(s_av.handle);

    if (!s_av.paused && !sceAvPlayerIsActive(s_av.handle) &&
        now_ms() - s_av.opened_ms > 1500) {
        s_av.finished = 1;
        UNLOCK();
        return 0;
    }

    UNLOCK();
    return 1;
}

const uint8_t *video_frame(int *w, int *h, int *serial)
{
    const uint8_t *p;

    LOCK();
    p = (s_av.rgba && s_av.frame_w > 0) ? s_av.rgba : NULL;
    if (w)      *w = s_av.frame_w;
    if (h)      *h = s_av.frame_h;
    if (serial) *serial = s_av.serial;
    UNLOCK();
    return p;
}

int video_read_audio(float *stereo, int max_frames)
{
    int n, i;

    if (!s_av.inited || !s_av.handle || !stereo || max_frames <= 0)
        return 0;
    if (s_av.paused)
        return 0;

    LOCK();

    for (i = 0; i < 4 && s_av.acount + AB_PULL_MAX <= AB_FRAMES; i++)
        pull_audio();

    n = s_av.acount < max_frames ? s_av.acount : max_frames;
    for (i = 0; i < n; i++) {
        stereo[i * 2]     = s_av.abuf[s_av.ahead * 2];
        stereo[i * 2 + 1] = s_av.abuf[s_av.ahead * 2 + 1];
        s_av.ahead = (s_av.ahead + 1) % AB_FRAMES;
        s_av.acount--;
    }
    UNLOCK();
    return n;
}

double video_position_sec(void)
{
    double ms;

    LOCK();
    ms = s_av.position_ms;
    UNLOCK();
    return ms / 1000.0;
}

double video_duration_sec(void)
{
    double ms;

    LOCK();
    ms = s_av.duration_ms;
    UNLOCK();
    return ms / 1000.0;
}

const char *video_format_note(void)
{
    return s_av.format;
}

void video_set_paused(int paused)
{
    if (!s_av.handle)
        return;

    LOCK();
    if (paused) {
        sceAvPlayerPause(s_av.handle);
        s_av.paused = 1;
    } else {
        sceAvPlayerResume(s_av.handle);
        s_av.paused = 0;
        s_av.finished = 0;
        s_av.opened_ms = now_ms();
    }
    UNLOCK();
}

void video_seek(double seconds)
{
    uint64_t ms;

    if (!s_av.handle)
        return;
    if (seconds < 0)
        seconds = 0;
    if (s_av.duration_ms > 0 && seconds * 1000.0 > s_av.duration_ms)
        seconds = s_av.duration_ms / 1000.0;

    ms = (uint64_t)(seconds * 1000.0);
    LOCK();
    if (sceAvPlayerJumpToTime(s_av.handle, ms) == 0) {
        s_av.position_ms = (double)ms;
        s_av.acount = 0;                    /* drop audio from before the jump */
        s_av.ahead  = 0;
        rs_reset(&s_av.rs);
    }
    UNLOCK();
}

const char *video_last_error(void)
{
    return s_av.err[0] ? s_av.err : "Video playback failed";
}
