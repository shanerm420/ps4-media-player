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
#include <unistd.h>

#include <orbis/libkernel.h>
#include <orbis/_types/kernel.h>

#include "compat.h"
#include "resampler.h"
#include "video_orbis.h"

extern void plog(const char *fmt, ...);
extern void plog_result(const char *fmt, ...);
#include <emmintrin.h>

extern void plog_perf(const char *fmt, ...);

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
    int32_t          auto_start;      /* 32-bit: a byte here moved the two
                                       * fields after it and left the firmware
                                       * reading past the struct, so the
                                       * language was never set and no audio
                                       * track was selected. */
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
_Static_assert(sizeof(AvInitData)    == 128, "AvInitData layout");
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
extern int32_t  sceAvPlayerEnableStream(SceAvPlayerHandle h, uint32_t id);
extern int32_t  sceAvPlayerGetStreamInfo(SceAvPlayerHandle h, uint32_t id,
                                         AvStreamInfo *info);
extern _Bool    sceAvPlayerIsActive(SceAvPlayerHandle h);
/* These three return 0 on success and a Sce error code when no frame is
 * ready. They used to be declared as _Bool and tested for truth, which meant
 * every good frame was discarded and only failures were treated as data. */
extern int32_t  sceAvPlayerGetVideoDataEx(SceAvPlayerHandle h, AvFrameInfoEx *f);
extern int32_t  sceAvPlayerGetVideoData(SceAvPlayerHandle h, AvFrameInfo *f);
extern int32_t  sceAvPlayerGetAudioData(SceAvPlayerHandle h, AvFrameInfo *f);

/* ------------------------------------------------------------------ */
/* v118: what the container says about itself                          */
/* ------------------------------------------------------------------ */

/* libSceAvPlayer's AvStreamInfo carries no codec field and no nominal frame
 * rate, so the fourcc is read from the file's own header instead: the 'ftyp'
 * brand at offset 4, and the first sample entry inside 'stsd'. It is a report
 * only - nothing here is fed back into the player. */
static void src_probe(const char *path, char *brand, size_t bsz,
                      char *codec, size_t csz, uint64_t *bytes)
{
    static uint8_t buf[65536];
    FILE          *f;
    size_t         got = 0, i;

    if (brand && bsz) snprintf(brand, bsz, "?");
    if (codec && csz) snprintf(codec, csz, "?");
    if (bytes)
        *bytes = 0;
    if (!path)
        return;

    f = fopen(path, "rb");
    if (!f)
        return;
    got = fread(buf, 1, sizeof(buf), f);
    if (bytes) {
        long end;

        if (fseek(f, 0, SEEK_END) == 0 && (end = ftell(f)) > 0)
            *bytes = (uint64_t)end;
    }
    fclose(f);

    for (i = 4; i + 8 <= got; i++)
        if (!memcmp(buf + i, "ftyp", 4)) {
            if (brand && bsz)
                snprintf(brand, bsz, "%.4s", (const char *)(buf + i + 4));
            break;
        }
    for (i = 0; i + 24 <= got; i++)
        if (!memcmp(buf + i, "stsd", 4)) {
            if (codec && csz)
                snprintf(codec, csz, "%.4s", (const char *)(buf + i + 20));
            break;
        }

    /* A fourcc is four printable characters or it is junk; keep the log line
     * readable either way. */
    if (brand)
        for (i = 0; brand[i]; i++)
            if ((unsigned char)brand[i] < 32 || (unsigned char)brand[i] > 126)
                brand[i] = '.';
    if (codec)
        for (i = 0; codec[i]; i++)
            if ((unsigned char)codec[i] < 32 || (unsigned char)codec[i] > 126)
                codec[i] = '.';
}

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

    /* playback watchdog: the player's own auto start is trusted first, and
     * only a player that stays silent gets poked with an explicit Start. */
    int              start_tried;
    int              saw_frame;
    int              saw_active;
    int              first_frame_logged;
    int              retry_done;
    int              used_basic;
    int              a_logged;
    int              enable_tried;
    int              enable_mask;      /* streams that took EnableStream */
    int              enable_n;         /* how many streams the file has */
    int              enable_attempts;  /* retries already spent */
    unsigned         enable_rc[8];     /* last answer per stream */
    int              stream_type[8];   /* 0 video, 1 audio, -1 unknown */
    int              vfail_logged;     /* pull errors already logged */
    uint64_t         last_enable_try;
    int              total_frames;
    int              fps_frames;
    uint64_t         last_report_ms;
    uint64_t         conv_us;          /* time spent converting, per report */
    int              conv_n;
    uint64_t         dec_us;           /* time spent pulling frames, per report */
    int              dec_n;
    uint64_t         lock_us;          /* time spent waiting for this lock */
    int              lock_n;
    int              left_stopped;     /* players stopped but not closed */
    int              last_event;
    /* One engine per process, for the whole run. libSceAvPlayer behaves like
     * a singleton: every crash in the v43-v47 logs happens inside a player
     * created next to an older one that was never closed, and Close faults
     * whenever it is called. So the handle from the first file is kept and
     * every later file is queued onto it with AddSource instead. */
    SceAvPlayerHandle engine;
    int              engine_dead;      /* the one engine is stuck: no more video */
    int              engine_count;     /* sources opened on the engine */

    /* v46 measurement: every number the pipeline can produce, so the log
     * says where the time and the frames went instead of the frame rate
     * having to be inferred from a count. */
    int              pull_ok;          /* frames handed over by the decoder */
    int              pull_fail;        /* calls that returned nothing */
    int              pull_max;         /* most frames drained in one step */
    int              pull_rounds;      /* calls to video_step */
    uint64_t         conv_max_us;      /* slowest single conversion */
    uint64_t         conv_px;          /* pixels converted this file */
    int              audio_pkts;       /* audio packets pulled */
    int              audio_fail;       /* pulls that found nothing */
    int              saw_audio;
    int              audio_id;         /* stream index the audio lives in */
    int              audio_dead;       /* stream answered nothing, stop asking */
    int              basic_frames;     /* frames from GetVideoData (old ABI) */
    int              ex_frames;        /* frames from GetVideoDataEx */
    double           prev_pos_ms;      /* position at the last report */
    uint64_t         prev_pos_t;       /* clock at the last report */
    /* v118: the file's own frame rate, from the decoder's timestamps. The
     * only place it exists - AvStreamInfo has no frame_rate field. */
    double           ts_first;
    double           ts_last;
    int              ts_n;
    int              rate_logged;
    char             path[512];

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

/* The player reports what it is doing through this callback (2 ready,
 * 3 play, 5 buffering, 1 stop...). It runs on an AvPlayer thread with a
 * tiny stack, where logging crashed the app, so the id is only recorded and
 * the main loop writes it out. */
static void set_error(const char *what, int code)
{
    if (code)
        snprintf(s_av.err, sizeof(s_av.err), "%s (0x%08X)", what, (unsigned)code);
    else
        snprintf(s_av.err, sizeof(s_av.err), "%s", what);
}

/* The decoder hands its output frames to the GPU, so those blocks have to be
 * direct memory rather than libc heap. Asking for heap memory there is why
 * playback stopped the moment it started: the picture never appeared and the
 * player fell back to zero frames. Small control blocks stay on the heap. */

#define DIRECT_MAX     48
#define DIRECT_MIN     (256u * 1024u)

typedef struct {
    void  *addr;
    size_t size;
    off_t  phys;
} DirectBlock;

static DirectBlock     s_direct[DIRECT_MAX];
static pthread_mutex_t s_direct_lock = PTHREAD_MUTEX_INITIALIZER;

static void *direct_alloc(uint32_t alignment, uint32_t size)
{
    size_t align = alignment > 0x10000 ? alignment : 0x10000;
    size_t sz    = ((size_t)size + 0xFFFF) & ~(size_t)0xFFFF;
    off_t  phys  = 0;
    void  *addr  = NULL;
    int    i, rc;

    rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), sz,
                                       align, ORBIS_KERNEL_WB_ONION, &phys);
    if (rc != 0)
        return NULL;
    rc = sceKernelMapDirectMemory(&addr, sz,
                                  ORBIS_KERNEL_PROT_CPU_RW | ORBIS_KERNEL_PROT_GPU_RW,
                                  0, phys, align);
    if (rc != 0) {
        sceKernelReleaseDirectMemory(phys, sz);
        return NULL;
    }

    pthread_mutex_lock(&s_direct_lock);
    for (i = 0; i < DIRECT_MAX; i++) {
        if (!s_direct[i].addr) {
            s_direct[i].addr = addr;
            s_direct[i].size = sz;
            s_direct[i].phys = phys;
            break;
        }
    }
    pthread_mutex_unlock(&s_direct_lock);
    return addr;
}

/* Returns 1 if the pointer was direct memory, 0 if it came from the heap. */
static int direct_free(void *p)
{
    int i;

    if (!p)
        return 1;
    pthread_mutex_lock(&s_direct_lock);
    for (i = 0; i < DIRECT_MAX; i++) {
        if (s_direct[i].addr == p) {
            sceKernelMunmap(p, s_direct[i].size);
            sceKernelReleaseDirectMemory(s_direct[i].phys, s_direct[i].size);
            s_direct[i].addr = NULL;
            pthread_mutex_unlock(&s_direct_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&s_direct_lock);
    return 0;
}

static void *heap_alloc(uint32_t align, uint32_t size)
{
    void *mem = NULL;
    size_t a = align < sizeof(void *) ? sizeof(void *) : (size_t)align;

    if (posix_memalign(&mem, a, size) != 0)
        return NULL;
    return mem;
}

static void *av_alloc(void *p, uint32_t align, uint32_t size)
{
    void *mem;

    (void)p;
    /* Small blocks are control data; big ones feed the decoder. */
    if (size < DIRECT_MIN) {
        mem = heap_alloc(align, size);
        if (mem)
            return mem;
    }
    mem = direct_alloc(align, size);
    if (!mem)
        mem = heap_alloc(align, size);
    return mem;
}

static void av_free(void *p, void *mem)
{
    (void)p;
    if (!mem)
        return;
    if (!direct_free(mem))
        free(mem);
}

static void *av_alloc_texture(void *p, uint32_t align, uint32_t size)
{
    void *mem;

    (void)p;
    mem = direct_alloc(align, size);
    if (!mem)
        mem = heap_alloc(align, size);
    return mem;
}

static void av_free_texture(void *p, void *mem)
{
    (void)p;
    if (!mem)
        return;
    if (!direct_free(mem))
        free(mem);
}

/* ------------------------------------------------------------------ */
/* File I/O behind the decoder                                         */
/* ------------------------------------------------------------------ */

__attribute__((unused)) static int32_t av_file_open(void *p, const char *name)
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

__attribute__((unused)) static int32_t av_file_close(void *p)
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

__attribute__((unused)) static int32_t av_file_read(void *p, uint8_t *buf, uint64_t pos, uint32_t len)
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

__attribute__((unused)) static uint64_t av_file_size(void *p)
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
 * but wrong colors, this is the line to revisit: swap the two chroma reads
 * for separate planes and it becomes planar I420. */
/* Pictures are converted straight to at most this width: the screen is 1280
 * wide and drawn by the CPU, so scaling here costs nothing extra and saves
 * the renderer from rescaling a 1080p image on every single frame. */
#define CV_MAX_W 1920
static int s_xmap[CV_MAX_W];

/* Conversion tables: the multiplies are done once per possible byte value
 * instead of once per pixel, which is most of what a 1080p frame cost. */
static int s_tab_y[256], s_tab_r[256], s_tab_u[256], s_tab_v[256], s_tab_b[256];
static int s_tab_ready, s_tab_hd, s_tab_sat = 100;
static int s_ky6, s_rv6, s_gu6, s_gv6, s_bu6;   /* same maths, 6-bit fixed point */

/* Picture settings the viewer owns, applied while converting: saturation is
 * folded into the chroma coefficients (free), sharpening is a three-tap
 * unsharp on the luma row (only paid while it is switched on). */
static int s_sat   = 100;   /* 0 greyscale, 100 untouched, 250 vivid */
static int s_sharp = 0;     /* 0 off, 100 strong */

void video_set_picture(int saturation, int sharpness)
{
    if (saturation < 0)   saturation = 0;
    if (saturation > 250) saturation = 250;
    if (sharpness < 0)    sharpness  = 0;
    if (sharpness > 100)  sharpness  = 100;
    if (saturation != s_sat) {
        s_sat        = saturation;
        s_tab_ready  = 0;       /* the coefficients carry the saturation */
    }
    s_sharp = sharpness;
}

/* Full-range and BT.709 handling follows both working PS4 players: BT.601 for
 * SD, BT.709 for 720 lines and up. Using 601 on HD puts a yellow-green cast
 * into skin tones. */
static void cv_tables(int hd)
{
    int i, ky = 298, rv = 409, gu = 100, gv = 208, bu = 516;

    if (s_tab_ready && s_tab_hd == hd && s_tab_sat == s_sat)
        return;
    if (hd) {
        rv = 459;
        gu = 55;
        gv = 136;
        bu = 541;
    }
    /* Saturation is a gain on every chroma difference, so it belongs in the
     * coefficients: scaling the four chroma terms scales the color and costs
     * nothing per pixel. 0 therefore has to give a clean grey picture, which
     * scaling to zero does exactly. */
    if (s_sat != 100) {
        rv = rv * s_sat / 100;
        gu = gu * s_sat / 100;
        gv = gv * s_sat / 100;
        bu = bu * s_sat / 100;
    }
    for (i = 0; i < 256; i++) {
        s_tab_y[i] = ky * (i - 16);
        s_tab_r[i] = rv * (i - 128) + 128;
        s_tab_u[i] = -gu * (i - 128);
        s_tab_v[i] = -gv * (i - 128);
        s_tab_b[i] = bu * (i - 128) + 128;
    }
    s_ky6 = (ky + 2) / 4;
    s_rv6 = (rv + 2) / 4;
    s_gu6 = (gu + 2) / 4;
    s_gv6 = (gv + 2) / 4;
    s_bu6 = (bu + 2) / 4;
    s_tab_hd  = hd;
    s_tab_sat = s_sat;
    s_tab_ready = 1;
}

/* --- SSE2 conversion ---------------------------------------------------
 * 16 pixels of two rows per step, 6-bit fixed point with saturating 16-bit
 * maths. The scalar loop below is the reference and still handles any width
 * that is not a multiple of 16. Stremio-PS4 reports ~4 ms per 1080p frame
 * for this shape of code on a PS4; the scalar loop was costing 28 ms. */
static inline void cv_store16(uint8_t *dst, __m128i r_lo, __m128i r_hi,
                              __m128i g_lo, __m128i g_hi,
                              __m128i b_lo, __m128i b_hi)
{
    const __m128i r = _mm_packus_epi16(_mm_srai_epi16(r_lo, 6), _mm_srai_epi16(r_hi, 6));
    const __m128i g = _mm_packus_epi16(_mm_srai_epi16(g_lo, 6), _mm_srai_epi16(g_hi, 6));
    const __m128i b = _mm_packus_epi16(_mm_srai_epi16(b_lo, 6), _mm_srai_epi16(b_hi, 6));
    const __m128i a = _mm_set1_epi8((char)0xFF);
    /* Byte order has to match the scalar path, which writes
     * 0xFF000000 | r << 16 | g << 8 | b into a uint32. On this little-endian
     * target that lands in memory as b, g, r, a - and that is what the
     * renderer has always been fed. Writing r, g, b, a here instead is what
     * turned skin and hair blue in v50. */
    const __m128i bg_lo = _mm_unpacklo_epi8(b, g), bg_hi = _mm_unpackhi_epi8(b, g);
    const __m128i ra_lo = _mm_unpacklo_epi8(r, a), ra_hi = _mm_unpackhi_epi8(r, a);

    _mm_storeu_si128((__m128i *)dst,        _mm_unpacklo_epi16(bg_lo, ra_lo));
    _mm_storeu_si128((__m128i *)(dst + 16), _mm_unpackhi_epi16(bg_lo, ra_lo));
    _mm_storeu_si128((__m128i *)(dst + 32), _mm_unpacklo_epi16(bg_hi, ra_hi));
    _mm_storeu_si128((__m128i *)(dst + 48), _mm_unpackhi_epi16(bg_hi, ra_hi));
}

/* Three-tap unsharp on 16 luma samples: c + k * (2c - l - r) / 4, clamped.
 * k is 0..8 for a sharpness of 0..100, so the strongest setting doubles the
 * edge without inverting anything, and the middle tap always dominates. */
static inline __m128i cv_sharp16(const uint8_t *row, int col, int k)
{
    const __m128i zero = _mm_setzero_si128();
    const __m128i kv   = _mm_set1_epi16((short)k);
    const __m128i hi   = _mm_set1_epi16(255);
    const __m128i c    = _mm_loadu_si128((const __m128i *)(row + col));
    const __m128i l    = _mm_loadu_si128((const __m128i *)(row + col - 1));
    const __m128i r    = _mm_loadu_si128((const __m128i *)(row + col + 1));
    __m128i c_lo, c_hi, d_lo, d_hi;

    c_lo = _mm_unpacklo_epi8(c, zero);
    c_hi = _mm_unpackhi_epi8(c, zero);
    d_lo = _mm_unpacklo_epi8(l, zero);
    d_hi = _mm_unpackhi_epi8(l, zero);
    d_lo = _mm_sub_epi16(_mm_slli_epi16(c_lo, 1), _mm_add_epi16(d_lo, _mm_unpacklo_epi8(r, zero)));
    d_hi = _mm_sub_epi16(_mm_slli_epi16(c_hi, 1), _mm_add_epi16(d_hi, _mm_unpackhi_epi8(r, zero)));
    d_lo = _mm_srai_epi16(_mm_mullo_epi16(d_lo, kv), 2);
    d_hi = _mm_srai_epi16(_mm_mullo_epi16(d_hi, kv), 2);
    c_lo = _mm_adds_epi16(c_lo, d_lo);
    c_hi = _mm_adds_epi16(c_hi, d_hi);
    c_lo = _mm_max_epi16(_mm_min_epi16(c_lo, hi), zero);
    c_hi = _mm_max_epi16(_mm_min_epi16(c_hi, hi), zero);
    return _mm_packus_epi16(c_lo, c_hi);
}

static void cv_rows_sse2(const uint8_t *y, const uint8_t *uv, int pitch,
                         int w, int h, uint8_t *dst)
{
    /* 0..8 taps of the unsharp above; 0 leaves the load below untouched. */
    const int sharp = s_sharp > 0 ? (s_sharp * 8 + 50) / 100 : 0;
    const __m128i zero  = _mm_setzero_si128();
    const __m128i mask  = _mm_set1_epi16(0x00FF);
    const __m128i y_off = _mm_set1_epi16(16), c128 = _mm_set1_epi16(128);
    const __m128i ky = _mm_set1_epi16((short)s_ky6), rv = _mm_set1_epi16((short)s_rv6);
    const __m128i gu = _mm_set1_epi16((short)s_gu6), gv = _mm_set1_epi16((short)s_gv6);
    const __m128i bu = _mm_set1_epi16((short)s_bu6), round = _mm_set1_epi16(32);
    int row, col;

    for (row = 0; row + 1 < h; row += 2) {
        const uint8_t *y0  = y + (size_t)row * pitch;
        const uint8_t *y1  = y0 + pitch;
        const uint8_t *uvr = uv + (size_t)(row >> 1) * pitch;
        uint8_t       *o0  = dst + (size_t)row * w * 4;
        uint8_t       *o1  = o0 + (size_t)w * 4;

        for (col = 0; col < (w & ~15); col += 16) {
            const __m128i uvv = _mm_loadu_si128((const __m128i *)(uvr + col));
            const __m128i v   = _mm_sub_epi16(_mm_and_si128(uvv, mask), c128);
            const __m128i u   = _mm_sub_epi16(_mm_srli_epi16(uvv, 8), c128);
            const __m128i rt  = _mm_add_epi16(_mm_mullo_epi16(v, rv), round);
            const __m128i gt  = _mm_sub_epi16(round, _mm_add_epi16(_mm_mullo_epi16(u, gu),
                                                                  _mm_mullo_epi16(v, gv)));
            const __m128i bt  = _mm_add_epi16(_mm_mullo_epi16(u, bu), round);
            const __m128i r_lo = _mm_unpacklo_epi16(rt, rt), r_hi = _mm_unpackhi_epi16(rt, rt);
            const __m128i g_lo = _mm_unpacklo_epi16(gt, gt), g_hi = _mm_unpackhi_epi16(gt, gt);
            const __m128i b_lo = _mm_unpacklo_epi16(bt, bt), b_hi = _mm_unpackhi_epi16(bt, bt);
            int k;

            for (k = 0; k < 2; k++) {
                const uint8_t *yr = k ? y1 : y0;
                /* The left and right loads of the unsharp read one byte past
                 * the block, so the first and last blocks of a row keep the
                 * unfiltered pixels instead. */
                const __m128i yy = (sharp && col > 0 && col + 16 < w)
                                 ? cv_sharp16(yr, col, sharp)
                                 : _mm_loadu_si128((const __m128i *)(yr + col));
                const __m128i yl = _mm_mullo_epi16(
                        _mm_sub_epi16(_mm_unpacklo_epi8(yy, zero), y_off), ky);
                const __m128i yh = _mm_mullo_epi16(
                        _mm_sub_epi16(_mm_unpackhi_epi8(yy, zero), y_off), ky);

                cv_store16((k ? o1 : o0) + (size_t)col * 4,
                           _mm_adds_epi16(yl, r_lo), _mm_adds_epi16(yh, r_hi),
                           _mm_adds_epi16(yl, g_lo), _mm_adds_epi16(yh, g_hi),
                           _mm_adds_epi16(yl, b_lo), _mm_adds_epi16(yh, b_hi));
            }
        }
    }
}

static void convert_nv12(const AvFrameInfoEx *fi)
{
    const AvVideoEx *v = &fi->details.video;
    const int      w = (int)v->width;
    const int      h = (int)v->height;
    /* The decoder pads rows to 64 bytes and the frame height to 16, with the
     * chroma plane directly after that padded luma plane. */
    const uint32_t pitch = v->pitch ? v->pitch : (uint32_t)((w + 63) & ~63);
    const int      vh    = (h + 15) & ~15;
    const uint8_t *y  = fi->data;
    const uint8_t *uv = fi->data + (size_t)pitch * (size_t)vh;
    uint8_t       *dst;
    int x, row, tw, th, sharp;

    if (!y || w <= 0 || h <= 0)
        return;
    /* A frame whose geometry or pitch is nonsense is skipped rather than
     * walked: reading past the buffer is how some files took the app down. */
    if (w > 4096 || h > 2304 || pitch < (uint32_t)w || pitch > 8192)
        return;
    if (v->luma_bits > 8 || v->chroma_bits > 8)
        return;                             /* 10-bit: reported elsewhere */

    tw = w > CV_MAX_W ? CV_MAX_W : w;
    th = (int)(((long)h * tw) / w) & ~1;
    if (tw <= 0 || th <= 0)
        return;

    if ((size_t)tw * (size_t)th * 4 > s_av.rgba_cap) {
        uint8_t *n = realloc(s_av.rgba, (size_t)tw * (size_t)th * 4);
        if (!n)
            return;
        s_av.rgba    = n;
        s_av.rgba_cap = (size_t)tw * (size_t)th * 4;
    }

    /* BT.709 for 720 lines and up, BT.601 below: the same rule both working
     * PS4 players use. */
    cv_tables(h >= 720);
    for (x = 0; x < tw; x++)
        s_xmap[x] = (x * w) / tw;
    /* Same three-tap unsharp as the SSE2 path, weighted so that both paths
     * bend the luma edge by about the same amount for a given setting. */
    sharp = s_sharp > 0 ? s_sharp : 0;

    dst = s_av.rgba;

    {
    struct timespec t0, t1;
    uint64_t        us;

    clock_gettime(CLOCK_MONOTONIC, &t0);

    /* The fast path only fits an unscaled, even-height, 16-aligned picture,
     * which is what every file tested so far gives. It also stops above 200%
     * saturation: its chroma terms are 16-bit fixed point, and at 300% the
     * product of a full-scale chroma value and the coefficient no longer fits,
     * which would wrap instead of clip. The scalar path below is 32-bit, so
     * the vivid end of the slider is exact - it just costs CPU while it is
     * set, and only while it is set. */
    if (tw == w && th == h && !(w & 15) && !(h & 1) && s_sat <= 200) {
        cv_rows_sse2(y, uv, (int)pitch, w, h, dst);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        us = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000u
           + (uint64_t)((t1.tv_nsec - t0.tv_nsec) / 1000);
        s_av.conv_us += us;
        s_av.conv_n++;
        if (us > s_av.conv_max_us)
            s_av.conv_max_us = us;
        s_av.conv_px += (uint64_t)tw * (uint64_t)th;
        s_av.frame_w = tw;
        s_av.frame_h = th;
        s_av.serial++;
        return;
    }

    for (row = 0; row < th; row++) {
        int            sy  = (row * h) / th;
        const uint8_t *yr  = y + (size_t)sy * pitch;
        const uint8_t *uvr = uv + (size_t)(sy >> 1) * pitch;
        uint32_t      *orow = (uint32_t *)dst + (size_t)row * (size_t)tw;

        if (tw == w) {
            /* No scaling: the usual case, since the picture is converted to
             * screen width. Skipping the map lookup keeps the inner loop to a
             * table read and a shift per channel. */
            for (x = 0; x < tw; x++) {
                int p  = yr[x];
                /* The chroma pair is stored V first. Reading it the other way
                 * round is what made skin tones come out blue. */
                int vv = uvr[x & ~1];
                int u  = uvr[(x & ~1) + 1];

                if (sharp) {
                    int l = x > 0 ? yr[x - 1] : p;
                    int r = x + 1 < tw ? yr[x + 1] : p;

                    p = clamp_u8(p + (sharp * (2 * p - l - r)) / 128);
                }
                {
                int c  = s_tab_y[p];

                orow[x] = 0xFF000000u
                        | ((uint32_t)clamp_u8((c + s_tab_r[vv]) >> 8) << 16)
                        | ((uint32_t)clamp_u8((c + s_tab_u[u] + s_tab_v[vv] + 128) >> 8) << 8)
                        |  (uint32_t)clamp_u8((c + s_tab_b[u]) >> 8);
                }
            }
        } else {
            for (x = 0; x < tw; x++) {
                int sx = s_xmap[x];
                int p  = yr[sx];
                int vv = uvr[sx & ~1];
                int u  = uvr[(sx & ~1) + 1];

                if (sharp) {
                    int l = sx > 0 ? yr[sx - 1] : p;
                    int r = sx + 1 < w ? yr[sx + 1] : p;

                    p = clamp_u8(p + (sharp * (2 * p - l - r)) / 128);
                }
                {
                int c  = s_tab_y[p];

                orow[x] = 0xFF000000u
                        | ((uint32_t)clamp_u8((c + s_tab_r[vv]) >> 8) << 16)
                        | ((uint32_t)clamp_u8((c + s_tab_u[u] + s_tab_v[vv] + 128) >> 8) << 8)
                        |  (uint32_t)clamp_u8((c + s_tab_b[u]) >> 8);
                }
            }
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    us = (uint64_t)(t1.tv_sec - t0.tv_sec) * 1000000u
       + (uint64_t)((t1.tv_nsec - t0.tv_nsec) / 1000);
    s_av.conv_us += us;
    s_av.conv_n++;
    if (us > s_av.conv_max_us)
        s_av.conv_max_us = us;
    s_av.conv_px += (uint64_t)tw * (uint64_t)th;
    }

    s_av.frame_w = tw;
    s_av.frame_h = th;
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

    if (s_av.audio_dead)
        return;

    memset(&fi, 0, sizeof(fi));
    /* The call reports success either as "true" or as zero depending on the
     * caller's reading of the ABI, so the data pointer is what is trusted and
     * only a negative (error) return rejects the packet. */
    if (sceAvPlayerGetAudioData(s_av.handle, &fi) < 0 || !fi.data) {
        s_av.audio_fail++;
        /* A file whose audio stream never answers was pulled about four
         * hundred times a second for the whole run - 18796 failures in one
         * file, each one taking the lock the picture needs. Once a second of
         * video exists with no packet at all, the stream is asked for itself
         * once (the one call the reference players never make, and the only
         * lever left) and then left alone: the file plays silently. */
        if (!s_av.audio_dead && s_av.audio_pkts == 0 && s_av.audio_fail > 200 &&
            s_av.saw_frame) {
            int32_t erc = -1;

            if (s_av.audio_id >= 0)
                erc = sceAvPlayerEnableStream(s_av.handle, (uint32_t)s_av.audio_id);
            if (erc == 0) {
                plog("video: audio stream %d enabled after %d empty pulls (rc=0x%08X), still trying",
                     s_av.audio_id, s_av.audio_fail, (unsigned)erc);
            } else {
                s_av.audio_dead = 1;
                plog("video: audio gives nothing after %d pulls (stream=%d EnableStream rc=0x%08X) - playing without sound",
                     s_av.audio_fail, s_av.audio_id, (unsigned)erc);
            }
        }
        return;
    }

    ch    = fi.details.audio.channel_count;
    rate  = (int)fi.details.audio.sample_rate;
    if (ch <= 0 || rate <= 0 || fi.details.audio.size < (uint32_t)(ch * 2))
        return;

    frames = (int)(fi.details.audio.size / (uint32_t)(ch * 2));
    if (frames > AB_PULL_MAX)
        frames = AB_PULL_MAX;

    s_av.audio_pkts++;
    s_av.saw_audio = 1;

    if (!s_av.a_logged) {
        s_av.a_logged = 1;
        plog("video: first audio packet %d ch %d Hz %d frames",
             ch, rate, frames);
    }

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

    if (s_av.engine_dead) {
        plog("video: the last file delivered no frame - the next one gets a new player");
        s_av.engine_dead = 0;
    }

    video_close();

    if (!path)
        return -1;

    plog("video: open %s", path);

    if (!s_av.inited) {
        pthread_mutex_init(&s_av.lock, NULL);
        pthread_mutex_init(&s_av.file.lock, NULL);
        s_av.rs_rate = 0;
        s_av.inited  = 1;
    }

    /* One player per file. Up to v56 the first player was kept for the whole
     * session and every later file was handed to it as the next source; once
     * one file delivered no picture, that player refused every new source
     * with OPERATION_FAILED (0x806A0002) and the video section was dead for
     * the rest of the run - exactly the loop in the v56 video-results log.
     * A fresh player per file cannot inherit that state, so one stuck file
     * can no longer touch the ones after it.
     *
     * A player that never delivered a frame is not stopped or closed (Stop
     * faulted in v37, Pause in v38): it is simply forgotten, and its memory
     * comes back when the app exits. */
    if (s_av.engine) {
        plog("video: an unusable player is still held - forgetting it, starting a new one");
        s_av.engine = 0;
    }

    memset(&data, 0, sizeof(data));
    data.memory.object             = NULL;
    data.memory.allocate           = av_alloc;
    data.memory.deallocate         = av_free;
    data.memory.allocate_texture   = av_alloc_texture;
    data.memory.deallocate_texture = av_free_texture;

    /* No file-replacement callbacks and no event callback, exactly like the
     * SDL2 PS4 player this now mirrors. The player opens the path itself
     * (it runs inside this process, with the same file access), and it starts
     * on its own. Intercepting its reads and events is what kept it from
     * ever leaving the first frame: it parsed the header, reported ready,
     * and then stopped. */

    data.debug_level         = 0;
    data.base_priority       = 160;
    data.video_frame_buffers = 4;
    /* The player starts itself once the source is ready and reports PLAY
     * through the callback. This is how PS4 media players actually run it:
     * calling Start by hand, or selecting streams before the player is
     * running, only returns success/OPERATION_FAILED and then nothing
     * decodes. */
    data.auto_start          = 1;
    data.default_language    = "en";

    plog("video: calling sceAvPlayerInit");

    LOCK();
    s_av.handle = sceAvPlayerInit(&data);
    plog("video: init handle=0x%llX", (unsigned long long)s_av.handle);
    if (!s_av.handle) {
        UNLOCK();
        plog_result("FAIL %s reason=player-init", path);
        set_error("AvPlayer refused to start", 0);
        return -1;
    }
    s_av.engine = s_av.handle;    /* kept until the app exits */

    rc = sceAvPlayerAddSource(s_av.handle, path);
    plog("video: AddSource rc=0x%08X", (unsigned)rc);
    if (rc < 0) {
        sceAvPlayerClose(s_av.handle);
        s_av.handle = 0;
        UNLOCK();
        plog_result("FAIL %s reason=add-source rc=0x%08X", path, (unsigned)rc);
        set_error("Cannot open video", rc);
        return -1;
    }

    /* AddSource returns as soon as the source is accepted; the demuxer keeps
     * opening the file in the background. Starting before it has parsed makes
     * sceAvPlayerStart fail with OPERATION_FAILED (0x806A0002), which is what
     * every attempt did. Wait for the stream list to appear. */
    s_av.width = s_av.height = 0;
    s_av.duration_ms = 0;
    for (i = 0; i < 150; i++) {
        streams = sceAvPlayerStreamCount(s_av.handle);
        if (streams > 0)
            break;
        usleep(20000);   /* 20 ms, so at most about three seconds */
    }
    plog("video: demux ready after %d waits, streams=%d", i, streams);
    /* A stopped playlist player does not necessarily restart itself for the
     * next source, so ask it to, and record the answer. */
    if (s_av.engine_count > 0 && !sceAvPlayerIsActive(s_av.handle)) {
        rc = sceAvPlayerStart(s_av.handle);
        plog("video: reused source was idle - Start rc=0x%08X", (unsigned)rc);
    }
    s_av.engine_count++;
    {
        int                blocks = 0;
        unsigned long long bytes  = 0;

        video_mem_stats(&blocks, &bytes);
        plog("video: engine %d holds %d blocks / %.1fMB of the %.0fMB direct pool",
             s_av.engine_count, blocks, (double)bytes / (1024.0 * 1024.0),
             (double)sceKernelGetDirectMemorySize() / (1024.0 * 1024.0));
    }
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

    plog("video: streams=%d %dx%d aud_rate=%d dur=%.0fms",
         streams, s_av.width, s_av.height, aud_rate, s_av.duration_ms);

    /* v118: one line that says what the file is, not what we did with it. */
    {
        char     brand[8], codec[8];
        uint64_t fbytes = 0;
        double   kbps   = 0.0;

        src_probe(path, brand, sizeof(brand), codec, sizeof(codec), &fbytes);
        if (fbytes > 0 && s_av.duration_ms > 0.0)
            kbps = (double)fbytes * 8.0 / (s_av.duration_ms / 1000.0) / 1000.0;
        plog("video: SRC brand=%s codec=%s bytes=%llu %dx%d dur=%.0fms bitrate=%.0fkbps streams=%d aud_rate=%d",
             brand, codec, (unsigned long long)fbytes, s_av.width, s_av.height,
             s_av.duration_ms, kbps, streams, aud_rate);
    }

    /* Every stream the container advertises, with its own details: the two
     * calls that refuse streams (EnableStream / GetVideoData) only make sense
     * next to what the file claims to contain. */
    for (i = 0; i < streams && i < 8; i++)
        s_av.stream_type[i] = -1;
    for (i = 0; i < streams; i++) {
        memset(&info, 0, sizeof(info));
        if (sceAvPlayerGetStreamInfo(s_av.handle, (uint32_t)i, &info) != 0) {
            plog("video: stream %d info failed - it will not be enabled", i);
            continue;
        }
        if (i < 8)
            s_av.stream_type[i] = (int)info.type;
        if (info.type == 1 && s_av.audio_id < 0)
            s_av.audio_id = i;
        if (info.type == 0)
            plog("video: stream %d type=video %ux%u aspect=%.3f lang=%.4s dur=%.0fms",
                 i, (unsigned)info.details.video.width,
                 (unsigned)info.details.video.height,
                 (double)info.details.video.aspect_ratio,
                 info.details.video.language, (double)info.duration_ms);
        else if (info.type == 1)
            plog("video: stream %d type=audio ch=%u rate=%u size=%u lang=%.4s dur=%.0fms",
                 i, (unsigned)info.details.audio.channel_count,
                 (unsigned)info.details.audio.sample_rate,
                 (unsigned)info.details.audio.size,
                 info.details.audio.language, (double)info.duration_ms);
        else
            plog("video: stream %d type=%u dur=%.0fms", i,
                 (unsigned)info.type, (double)info.duration_ms);
    }

    /* No EnableStream and no Start: the working player does neither, and both
     * answered OPERATION_FAILED here and left the player inactive from the
     * first millisecond. autoStart is the supported path. */

    /* Streams are listed but not selected: until each one is enabled the
     * decoder hands out nothing, which is exactly what the console showed
     * (source ready, clock at zero, no frames, ever). Enabling is refused
     * with OPERATION_FAILED while the player is still stopped, so it is tried
     * again after Start, once a second, until every stream takes. */

    s_av.position_ms = 0;
    s_av.paused      = 0;
    s_av.finished    = 0;
    s_av.serial      = 0;
    s_av.frame_w     = 0;
    s_av.frame_h     = 0;
    s_av.ahead       = 0;
    s_av.acount      = 0;
    s_av.start_tried = 0;
    s_av.saw_frame   = 0;
    s_av.saw_active  = 0;
    s_av.first_frame_logged = 0;
    s_av.a_logged    = 0;
    s_av.enable_tried = 0;
    s_av.enable_mask     = 0;
    s_av.enable_n        = streams > 8 ? 8 : streams;
    s_av.enable_attempts = 0;
    s_av.last_enable_try = 0;
    memset(s_av.enable_rc, 0, sizeof(s_av.enable_rc));
    s_av.total_frames = 0;
    s_av.conv_us      = 0;
    s_av.conv_n       = 0;
    s_av.dec_us       = 0;
    s_av.dec_n        = 0;
    s_av.pull_ok      = 0;
    s_av.pull_fail    = 0;
    s_av.pull_max     = 0;
    s_av.pull_rounds  = 0;
    s_av.conv_max_us  = 0;
    s_av.conv_px      = 0;
    s_av.audio_pkts   = 0;
    s_av.audio_fail   = 0;
    s_av.saw_audio    = 0;
    s_av.audio_dead   = 0;
    s_av.audio_id     = -1;
    s_av.basic_frames = 0;
    s_av.ex_frames    = 0;
    s_av.vfail_logged = 0;
    s_av.prev_pos_ms  = 0.0;
    s_av.prev_pos_t   = now_ms();
    s_av.ts_first     = 0.0;
    s_av.ts_last      = 0.0;
    s_av.ts_n         = 0;
    s_av.rate_logged  = 0;
    /* The picture buffer still holds the last frame of whatever played
     * before. Left alone, the new file showed that old frame - the previous
     * video in the list - until its own first frame arrived. */
    if (s_av.rgba && s_av.rgba_cap >= 4) {
        memset(s_av.rgba, 0, s_av.rgba_cap < 4096 ? s_av.rgba_cap : 4096);
        s_av.frame_w = 0;
        s_av.frame_h = 0;
        s_av.serial  = 1;   /* tell the renderer the buffer changed */
    }
    s_av.fps_frames  = 0;
    s_av.retry_done  = 0;
    snprintf(s_av.path, sizeof(s_av.path), "%s", path);
    s_av.opened_ms   = now_ms();
    s_av.last_report_ms = s_av.opened_ms;
    /* One line into the results file before anything can go wrong: a START
     * without a verdict afterwards is the file that crashed the app. */
    plog_result("START %s", path);
    plog("video: RESULT start");

    s_av.rs_rate = aud_rate;
    if (aud_rate > 0)
        rs_init(&s_av.rs, aud_rate, 48000, 2);

    if (s_av.width > 0 && s_av.height > 0)
        snprintf(s_av.format, sizeof(s_av.format), "MP4   %d x %d",
                 s_av.width, s_av.height);
    else
        snprintf(s_av.format, sizeof(s_av.format), "MP4   H.264");

    /* No Start and no stream selection: autoStart drives it. The picture
     * only flows while audio is pulled at real-time pace, which the audio
     * thread already does through video_read_audio(). */
    plog("video: opened with autoStart, waiting for the play event");
    plog("video: open stage took %.0fms", (double)(now_ms() - s_av.opened_ms));
    UNLOCK();

    s_av.err[0] = '\0';
    return 0;
}

void video_close(void)
{
    if (!s_av.inited || !s_av.handle)
        return;

    plog("video: closing handle");
    {
        uint64_t close_t0 = now_ms();

    LOCK();
    /* Whether a teardown call faults depends on what the player is doing when
     * it is asked, so the state it is in is recorded next to the call. */
    plog("video: teardown active=%d frames=%d",
         sceAvPlayerIsActive(s_av.handle) ? 1 : 0, s_av.total_frames);
    /* A player that never produced a single picture does not survive Stop:
     * every crash in the logs ends on this line for a file whose video stream
     * could not even be selected (enable answered 0x806A0002 and no frame ever
     * arrived). Such a player is paused and closed instead, which is the same
     * teardown without the call that faults. */
    if (s_av.saw_frame) {
        /* Stop + Close, exactly like both reference players: they tear the
         * player down for every file and start a fresh one for the next. The
         * part we had wrong was the order. They first join their pull threads
         * and only then call Stop and Close; ours called them while the audio
         * thread was still pulling, and that is where the signal 11s came
         * from. So the handle is dropped here, the lock is released long
         * enough for any pull in flight to return, and only then is the
         * player torn down. */
        SceAvPlayerHandle eng = s_av.handle;
        int32_t           rc_stop, rc_close;

        s_av.handle   = 0;
        s_av.paused   = 0;
        s_av.finished = 0;
        s_av.acount   = 0;
        /* no frame belongs to a closed file: the section draws its placeholder
         * instead of a stale last picture */
        s_av.frame_w  = 0;
        s_av.frame_h  = 0;
        UNLOCK();
        usleep(200000);
        LOCK();

        /* v118: the 250-383 ms frames around a close happen in here, so
         * each call is timed rather than the whole teardown guessed at. */
        {
            uint64_t t_stop = now_ms();

            rc_stop = sceAvPlayerStop(eng);
            plog("video: Stop rc=0x%08X took=%.0fms", (unsigned)rc_stop,
                 (double)(now_ms() - t_stop));
        }
        {
            uint64_t t_close = now_ms();

            rc_close = sceAvPlayerClose(eng);
            plog("video: Close rc=0x%08X took=%.0fms", (unsigned)rc_close,
                 (double)(now_ms() - t_close));
        }
        if (s_av.engine == eng)
            s_av.engine = 0;      /* the next file starts a fresh player */
        plog("video: player torn down");
        plog("video: teardown took %.0fms total",
             (double)(now_ms() - close_t0));
    } else {
        /* Teardown on a player that never delivered a picture faults whichever
         * call is used: Stop died in v37, Pause in v38, both right here. The
         * handle is therefore abandoned instead of torn down. The decoder
         * keeps its memory until the app exits, which is a few megabytes in
         * exchange for not crashing, and only ever happens on files that could
         * not be played at all. */
        plog("video: abandoned (no frames were delivered)");
        /* The player is forgotten, not torn down, and the next file builds a
         * new one: v56 handed every later file to this same dead player and
         * none of them could ever play. */
        s_av.engine_dead = 1;
        s_av.engine      = 0;
        plog("video: player forgotten - the next file starts a new one");
        s_av.handle   = 0;
        s_av.paused   = 0;
        s_av.finished = 0;
        s_av.acount   = 0;
        /* no frame belongs to a closed file: the section draws its placeholder
         * instead of a stale last picture */
        s_av.frame_w  = 0;
        s_av.frame_h  = 0;
        plog_result("FAIL %s reason=no-frames enable0=0x%08X enable1=0x%08X active=%d pos=%.0f audio=%d/%d basic=%d",
                    s_av.path, s_av.enable_rc[0], s_av.enable_rc[1],
                    s_av.saw_active, s_av.position_ms,
                    s_av.audio_pkts, s_av.audio_fail, s_av.basic_frames);
        plog("video: RESULT fail: no frames (streams=%d mask=0x%X attempts=%d audio=%d/%d pulls=%d/%d)",
             s_av.enable_n, s_av.enable_mask, s_av.enable_attempts,
             s_av.audio_pkts, s_av.audio_fail,
             s_av.pull_ok, s_av.pull_fail);
        plog_perf("file FAIL mask=0x%X attempts=%d active=%d pos=%.0f audio=%d/%d",
                 s_av.enable_mask, s_av.enable_attempts, s_av.saw_active,
                 s_av.position_ms, s_av.audio_pkts, s_av.audio_fail);
        av_file_close(&s_av.file);
        UNLOCK();
        return;
    }
    /* No Close. Every v43 run that called it died with signal 11 right here
     * (boot.1: 'stopped' then CRASH; boot.2: the same), and the one run that
     * got through did not survive its next file either - the crash landed a
     * second after the close, inside the newly opened player, which is a
     * thread the close had freed memory under and left running.
     *
     * So the player is stopped and then left alone. A stopped player decodes
     * nothing; it only holds the memory it was given. That is the price of not
     * faulting, and it is written into the log so the cost stays visible. */
    s_av.left_stopped++;
    plog("video: closed (%d closed this session)", s_av.left_stopped);
    plog("video: close stage took %.0fms", (double)(now_ms() - close_t0));
    }
    {
        uint64_t span = now_ms() - s_av.opened_ms;
        int      fps  = span ? (int)((uint64_t)s_av.total_frames * 1000u / span) : 0;
        double   conv = s_av.conv_n ? (double)s_av.conv_us / (1000.0 * s_av.conv_n) : 0.0;

        plog_result("PASS %s frames=%d fps=%d pos=%.0fms conv=%.2fms convmax=%.2fms pull=%d/%d/%d audio=%d/%d basic=%d ex=%d",
                    s_av.path, s_av.total_frames, fps, s_av.position_ms,
                    conv, (double)s_av.conv_max_us / 1000.0,
                    s_av.pull_ok, s_av.pull_fail, s_av.pull_max,
                    s_av.audio_pkts, s_av.audio_fail,
                    s_av.basic_frames, s_av.ex_frames);
        plog("video: RESULT pass frames=%d fps=%d", s_av.total_frames, fps);
        /* One line per file that sums up the whole pipeline: the picture size
         * and the conversion cost per megapixel are what decide whether the
         * frame rate can be raised at all. */
        plog("video: FILE frames=%d played=%.1fs fps=%d pos=%.0fms conv_avg=%.2fms conv_max=%.2fms px=%llu pull_ok=%d pull_fail=%d pull_max=%d pull_rounds=%d rounds_per_frame=%.2f audio_pkt=%d audio_fail=%d basic=%d ex=%d left=%d",
             s_av.total_frames, (double)span / 1000.0, fps, s_av.position_ms,
             conv, (double)s_av.conv_max_us / 1000.0,
             (unsigned long long)s_av.conv_px,
             s_av.pull_ok, s_av.pull_fail, s_av.pull_max,
             s_av.pull_rounds,
             s_av.pull_ok ? (double)s_av.pull_rounds / (double)s_av.pull_ok : 0.0,
             s_av.audio_pkts, s_av.audio_fail,
             s_av.basic_frames, s_av.ex_frames, s_av.left_stopped);
        plog_perf("file PASS frames=%d fps=%d played=%.1fs conv=%.2f/%.2fms pull=%d/%d audio=%d/%d",
                 s_av.total_frames, fps, (double)span / 1000.0,
                 conv, (double)s_av.conv_max_us / 1000.0,
                 s_av.pull_ok, s_av.pull_fail,
                 s_av.audio_pkts, s_av.audio_fail);
    }
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
    int      pulled, active;
    uint64_t now = now_ms();
    struct timespec w0, w1;

    if (!s_av.inited || !s_av.handle)
        return -1;
    if (s_av.finished)
        return 0;

    /* The audio thread takes this same lock to pull packets, so any time it
     * spends holding it is time the picture waits. Counted, not assumed. */
    clock_gettime(CLOCK_MONOTONIC, &w0);
    LOCK();
    clock_gettime(CLOCK_MONOTONIC, &w1);
    s_av.lock_us += (uint64_t)(w1.tv_sec - w0.tv_sec) * 1000000u
                  + (uint64_t)((w1.tv_nsec - w0.tv_nsec) / 1000);
    s_av.lock_n++;

    /* No EnableStream call. Neither working PS4 player makes one:
     * PS4-IPTV-Player adds the source and pulls frames, and Stremio-PS4 waits
     * for the player's own PLAY event and pulls. AvPlayer selects the first
     * video and audio stream itself. Ours was the odd one out, and the logs
     * show why that mattered - the file that plays refuses every enable with
     * OPERATION_FAILED, while the files that show nothing accept it and then
     * never deliver a frame. */

    /* Drain what the decoder has, but convert only the newest frame. Each
     * 1080p conversion costs milliseconds, and doing all eight of them made
     * playback crawl: the picture was correct but only a few frames a second
     * could ever be shown. Skipping the stale ones is what makes it smooth. */
    {
    AvFrameInfoEx   newest;
    int             have = 0;
    struct timespec d0, d1;

    memset(&newest, 0, sizeof(newest));
    clock_gettime(CLOCK_MONOTONIC, &d0);

    for (pulled = 0; pulled < 8; pulled++) {
        AvFrameInfoEx fi;
        int           got = 0;
        int32_t       vrc;

        memset(&fi, 0, sizeof(fi));
        /* The pointer is the console answering "does this firmware export it?",
         * asked once at boot by compat_probe(). NULL means the linked symbol is
         * the only answer available, which is every build before v144. */
        vrc = compat_av_getvideodataex
                  ? compat_av_getvideodataex(s_av.handle, &fi)
                  : sceAvPlayerGetVideoDataEx(s_av.handle, &fi);
        if (vrc >= 0 && fi.data) {
            got = 1;
            s_av.ex_frames++;
        } else {
            /* Older firmware answers the basic call even when the extended
             * one declines; the frame layout it returns is the same NV12. */
            AvFrameInfo f40;

            memset(&f40, 0, sizeof(f40));
            if ((compat_av_getvideodata
                     ? compat_av_getvideodata(s_av.handle, &f40)
                     : sceAvPlayerGetVideoData(s_av.handle, &f40)) >= 0 &&
                f40.data) {
                fi.data                    = f40.data;
                fi.timestamp_ms            = f40.timestamp_ms;
                fi.details.video.width     = f40.details.video.width;
                fi.details.video.height    = f40.details.video.height;
                fi.details.video.aspect_ratio = f40.details.video.aspect_ratio;
                got = 1;
                s_av.basic_frames++;
                if (!s_av.used_basic) {
                    s_av.used_basic = 1;
                    plog("video: frames come from the basic GetVideoData");
                }
            }
        }
        if (!got) {
            s_av.pull_fail++;
            /* What the decoder actually answers when it has no picture. The
             * share of these against successful pulls is the whole story of
             * the frame rate, and the code says whether it has nothing ready
             * or is refusing outright. */
            if (s_av.vfail_logged < 3) {
                s_av.vfail_logged++;
                plog("video: pull empty rc=0x%08X active=%d pos=%.0f",
                     (unsigned)vrc, sceAvPlayerIsActive(s_av.handle) ? 1 : 0,
                     s_av.position_ms);
            }
            break;
        }
        s_av.pull_ok++;
        newest = fi;
        have    = 1;
    }
    s_av.pull_rounds++;
    if (pulled > s_av.pull_max)
        s_av.pull_max = pulled;

    if (have) {
        s_av.saw_frame = 1;
        s_av.engine_dead = 0;      /* this engine still plays pictures */
        s_av.fps_frames++;
        s_av.total_frames++;
        /* v118: the file's rate, from the decoder's own timestamps. Thirty
         * frames separate 23.976 from 25 from 29.97; nothing else in the
         * API reports it. */
        if (s_av.ts_n == 0) {
            s_av.ts_first = (double)newest.timestamp_ms;
            s_av.ts_last  = s_av.ts_first;
            s_av.ts_n     = 1;
        } else if ((double)newest.timestamp_ms > s_av.ts_last) {
            s_av.ts_last = (double)newest.timestamp_ms;
            s_av.ts_n++;
            if (s_av.ts_n == 31 && !s_av.rate_logged) {
                double span = s_av.ts_last - s_av.ts_first;

                s_av.rate_logged = 1;
                plog("video: RATE nominal=%.3ffps from 30 timestamps span=%.0fms",
                     span > 0.0 ? 30000.0 / span : 0.0, span);
            }
        }
        if (newest.details.video.luma_bits > 8) {
            snprintf(s_av.err, sizeof(s_av.err),
                     "10-bit video is not supported");
        } else {
            convert_nv12(&newest);
        }
        if (!s_av.first_frame_logged) {
            s_av.first_frame_logged = 1;
            plog("video: first frame %ux%u pitch=%u bits=%u/%u",
                 (unsigned)newest.details.video.width,
                 (unsigned)newest.details.video.height,
                 (unsigned)newest.details.video.pitch,
                 (unsigned)newest.details.video.luma_bits,
                 (unsigned)newest.details.video.chroma_bits);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &d1);
    s_av.dec_us += (uint64_t)(d1.tv_sec - d0.tv_sec) * 1000000u
                 + (uint64_t)((d1.tv_nsec - d0.tv_nsec) / 1000);
    s_av.dec_n++;
    }

    active = sceAvPlayerIsActive(s_av.handle) ? 1 : 0;
    if (active) {
        uint64_t t = sceAvPlayerCurrentTime(s_av.handle);

        s_av.saw_active = 1;
        /* A stopped player returns junk (values around 2e17 ms), which is
         * where the absurd clock in the corner came from. */
        if (t <= 86400000ull)
            s_av.position_ms = (double)t;
    }


    /* Still nothing six seconds in: say so instead of spinning on
     * BUFFERING forever, and leave the file behind rather than pretending it
     * played to the end. */
    if (!s_av.saw_frame && now - s_av.opened_ms > 10000) {
        plog("video: no frames after 10s (active=%d pos=%.0f)",
             active, s_av.position_ms);
        /* Everything the player was willing to say, at the moment it gave up:
         * a file that reports success on both streams and still delivers
         * nothing is a different fault from one that refuses a stream. */
        plog("video: nogiveup streams=%d mask=0x%X attempts=%d rc0=0x%08X rc1=0x%08X audio=%d/%d basic=%d ex=%d pulls_ok=%d pulls_fail=%d",
             s_av.enable_n, s_av.enable_mask, s_av.enable_attempts,
             s_av.enable_rc[0], s_av.enable_rc[1],
             s_av.audio_pkts, s_av.audio_fail,
             s_av.basic_frames, s_av.ex_frames,
             s_av.pull_ok, s_av.pull_fail);
        plog_perf("giveup streams=%d mask=0x%X attempts=%d audio=%d/%d pulls=%d/%d elapsed=%.0fms",
                 s_av.enable_n, s_av.enable_mask, s_av.enable_attempts,
                 s_av.audio_pkts, s_av.audio_fail,
                 s_av.pull_ok, s_av.pull_fail,
                 (double)(now - s_av.opened_ms));
        set_error("THIS VIDEO GAVE NO PICTURE - CHOOSE ANOTHER FILE", 0);
        s_av.finished = 1;
        UNLOCK();
        return -1;
    }

    /* End of file: only after a picture has actually been delivered. The
     * player's clock can creep forward (pos=66, pos=1466) while it is still
     * buffering and no frame has ever arrived; treating that as the end of the
     * file made a slow starter look like a finished one, and closing it marked
     * the decoder unusable for the rest of the run - the "restart the app"
     * message in the v54 log. A file that never plays now waits for the
     * ten-second give-up above instead. */
    if (!active && !s_av.paused && s_av.saw_frame &&
        now - s_av.opened_ms > 1500) {
        s_av.finished = 1;
        UNLOCK();
        return 0;
    }

    /* Reported on elapsed time, not on a modulo window: at ten frames a
     * second the old check could miss the window entirely and print nothing,
     * which left the frame rate unmeasurable exactly when it mattered. */
    if (now - s_av.last_report_ms >= 2000) {
        uint64_t dt = now - s_av.last_report_ms;
        double   rate = 0.0;

        /* Playback speed: how far the player's own clock moved against how
         * much real time passed. 1.00 is real time; 0.30 would be the slow
         * motion the earlier builds had. */
        if (s_av.prev_pos_t) {
            double dpos = s_av.position_ms - s_av.prev_pos_ms;
            double dms  = (double)(now - s_av.prev_pos_t);

            if (dms > 0.0)
                rate = dpos / dms;
        }
        s_av.prev_pos_ms = s_av.position_ms;
        s_av.prev_pos_t  = now;

        plog("video: t=%.0fms active=%d frames=%d pos=%.0f fps=%d conv=%.2fms dec=%.2fms",
             (double)(now - s_av.opened_ms), active, s_av.saw_frame,
             s_av.position_ms, (int)((uint64_t)s_av.fps_frames * 1000u / dt),
             s_av.conv_n ? (double)s_av.conv_us / (1000.0 * s_av.conv_n) : 0.0,
             s_av.dec_n ? (double)s_av.dec_us / (1000.0 * s_av.dec_n) : 0.0);
        plog("video: pulls ok=%d fail=%d max=%d rounds=%d conv_max=%.2fms px=%llu audio=%d pkt/%d fail basic=%d ex=%d rate=%.2f",
             s_av.pull_ok, s_av.pull_fail, s_av.pull_max, s_av.pull_rounds,
             (double)s_av.conv_max_us / 1000.0,
             (unsigned long long)s_av.conv_px,
             s_av.audio_pkts, s_av.audio_fail,
             s_av.basic_frames, s_av.ex_frames, rate);
        plog("video: lockw=%.2fms/frame audio_q=%d",
             s_av.lock_n ? (double)s_av.lock_us / (1000.0 * s_av.lock_n) : 0.0,
             s_av.acount);
        plog_perf("video t=%.0fms active=%d frames=%d fps=%d pos=%.0f rate=%.2f conv=%.2f/%.2f dec=%.2f pull=%d/%d/%d audio=%d/%d q=%d lock=%.3f",
                 (double)(now - s_av.opened_ms), active, s_av.saw_frame,
                 (int)((uint64_t)s_av.fps_frames * 1000u / dt),
                 s_av.position_ms, rate,
                 s_av.conv_n ? (double)s_av.conv_us / (1000.0 * s_av.conv_n) : 0.0,
                 (double)s_av.conv_max_us / 1000.0,
                 s_av.dec_n ? (double)s_av.dec_us / (1000.0 * s_av.dec_n) : 0.0,
                 s_av.pull_ok, s_av.pull_fail, s_av.pull_max,
                 s_av.audio_pkts, s_av.audio_fail, s_av.acount,
                 s_av.lock_n ? (double)s_av.lock_us / (1000.0 * s_av.lock_n) : 0.0);
        s_av.last_report_ms = now;
        s_av.fps_frames     = 0;
        s_av.conv_us        = 0;
        s_av.conv_n         = 0;
        s_av.dec_us         = 0;
        s_av.dec_n          = 0;
        s_av.lock_us        = 0;
        s_av.lock_n         = 0;
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

    if (!stereo || max_frames <= 0)
        return 0;

    LOCK();
    /* The handle is checked under the lock. Checking it first left a window
     * where the audio thread could call into a player that the main thread
     * had just closed - a use-after-free that ended the app on some files. */
    if (!s_av.inited || !s_av.handle || s_av.paused) {
        UNLOCK();
        return 0;
    }

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

/* Counters the main loop reports next to its own timings: how many players
 * were left allocated (the price of never calling Close), and how much
 * memory the decoder's own allocations are still holding. */
int video_left_players(void)
{
    return s_av.left_stopped;
}

unsigned long long video_rgba_bytes(void)
{
    return (unsigned long long)s_av.rgba_cap;
}

void video_mem_stats(int *blocks, unsigned long long *bytes)
{
    int    i, n = 0;
    size_t total = 0;

    pthread_mutex_lock(&s_direct_lock);
    for (i = 0; i < DIRECT_MAX; i++) {
        if (s_direct[i].addr) {
            n++;
            total += s_direct[i].size;
        }
    }
    pthread_mutex_unlock(&s_direct_lock);

    if (blocks)
        *blocks = n;
    if (bytes)
        *bytes = (unsigned long long)total;
}
