/* main.c - Ultimate PS4 Media Player.
 *
 * Plays MP3 through: minimp3 -> resampler -> 10-band EQ -> volume ->
 * soft limiter -> sceAudioOut (16-bit stereo, 48 kHz).
 *
 * Video (.mp4/.mov) goes through libSceAvPlayer instead: video_orbis.c decodes
 * it and hands back RGBA frames plus 48 kHz float stereo, which joins the same
 * EQ / volume / limiter chain as the MP3 path.
 *
 * Rendering is SDL2 and lives entirely in ui.c; this file only keeps the state
 * that ui.c draws (library, selection, playback position, EQ settings) and
 * turns controller buttons into actions.
 */
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

/* Fixed by tools/buildstamp.py so two builds of the same source are identical.
   The fallbacks keep a host/preview build that does not pass the flags working. */
#ifndef BUILD_DATE
#define BUILD_DATE __DATE__
#endif
#ifndef BUILD_TIME
#define BUILD_TIME __TIME__
#endif

/* One log file (app.log) and one settings file (config.ini); the two modules
 * below own both, and every part of the app goes through them. */
#include "appcfg.h"
#include "applog.h"

#include "audio_core.h"
#include "audio_out_orbis.h"
#include "eq.h"
#include "mp3_source.h"
#include "picture.h"
#include "resampler.h"
#include "spectrum.h"
#include "subtitles.h"
#include "tags.h"
#include "ui.h"
#include "netstat.h"
#include "video_orbis.h"
#include "vis_art.h"

#define BUILD_ID "v118"

#define OUT_RATE      48000
#define BLOCK_FRAMES  2048   /* ~43 ms per block: enough slack that a slow
                              * UI frame cannot starve the sound port */
/* How long the video transport bar stays up after the last button press. */
#define HUD_MS        3000
#define MAX_TRACKS    8192   /* the whole collection, not the first 256 */
#define MAX_PATH_LEN  512
#define SCREEN_W      1920
#define SCREEN_H      1080

/* A crash used to leave only the last line the app printed, which made every
 * teardown call look equally guilty. This writes the signal number into the
 * log straight away, with only write() and no buffered I/O, so the next one
 * names its site. */
/* Which part of the loop was running when the signal arrived. A bare
 * "CRASH signal=11" said nothing about whether the picture, the blit or the
 * player was at fault; with the stage name it rules two of the three out. */
static const char *const k_stage_names[] = {
    "boot", "events", "video-step", "model", "upload", "draw",
    "present", "delay"
};
#define STAGE_COUNT ((int)(sizeof(k_stage_names) / sizeof(k_stage_names[0])))

static volatile int s_stage;
static volatile int s_frame_no;

static void crash_handler(int sig)
{
    char msg[96];
    int  n, fd, stage = s_stage;

    if (stage < 0 || stage >= STAGE_COUNT)
        stage = 0;
    n = snprintf(msg, sizeof(msg),
                 "\n!!! CRASH signal=%d stage=%s loop=%d (last state above)\n",
                 sig, k_stage_names[stage], s_frame_no);
    if (n > 0) {
        /* Straight into app.log with write(): no buffer, no allocator, nothing
         * a dying process can get wrong. */
        fd = open(APP_LOG_PATH, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (fd >= 0) {
            ssize_t w = write(fd, msg, (size_t)n);
            (void)w;
            close(fd);
        }
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

static void install_crash_logging(void)
{
    signal(SIGSEGV, crash_handler);
    signal(SIGBUS,  crash_handler);
    signal(SIGILL,  crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGFPE,  crash_handler);
}

typedef struct {
    char path[MAX_PATH_LEN];
    char name[MAX_PATH_LEN];
    char title[TAG_LEN];        /* from the tag, or the file name */
    char artist[TAG_LEN];
    char album[TAG_LEN];
    char genre[TAG_LEN];
    int  track_no;
    int  is_video;
} Track;

typedef struct {
    char path[MAX_PATH_LEN];
    char name[MAX_PATH_LEN];
} Pic;

static Track   s_tracks[MAX_TRACKS];
static int     s_track_count;
static int     s_selected;
static int     s_current = -1;

static Pic     s_pics[MAX_TRACKS];         /* the picture section's list */
static int     s_pic_count;
static int     s_pic_sel;

/* The section list the UI draws from, rebuilt each frame: songs in the music
 * section, videos in the video section, photos in the picture section. */
static UiTrack s_view[MAX_TRACKS];
static int     s_view_count;

/* set whenever the collection or the browsing state changes, so the music
 * list is rebuilt once instead of on every frame */
static int     s_mv_dirty = 1;

static int     s_home_sel;                 /* card on the home screen, 0..3 */
static int     s_sel_music;                /* last selection in each section */
static int     s_sel_video;
static volatile int s_playing;
static volatile int s_quit;
static volatile int s_track_finished;

static Volume       s_vol;
static Eq           s_eq;
static Mp3Source    s_src;
static Resampler    s_rs;
static pthread_t    s_thread;
static int          s_thread_started;
static int          s_src_open;
static char         s_status[256] = "No track loaded";
static char         s_format[64];
static double       s_duration;
static int          s_show_eq;
static int          s_eq_band;
static int          s_eq_preset;         /* EQ_PRESET_* */
static int          s_eq_preset_open;    /* the drop-down is down */
static int          s_eq_preset_sel;     /* row highlighted in it */

static int          s_hat_last;   /* previous d-pad hat state */
static int          s_hat_dir;    /* direction currently held, 0 if none */
static uint64_t     s_hat_next;   /* when the held direction repeats */
static uint64_t     s_hat_t0;     /* when the current press started */
static unsigned     s_btn_logged; /* button numbers already traced once */
static int          s_gnm_loaded;

/* Only callable once libSceGnmDriver is loaded (see the module block in
 * main). With it loaded the system will not suspend or close the app until
 * sceGnmSubmitDone is called, so the loop calls it whenever the system UI is
 * over the app; calling it every frame instead stalls the decoder. */
extern int32_t sceGnmSubmitDone(void);
extern int32_t sceSystemServiceGetStatus(void *status);

typedef struct {
    int32_t event_count;
    uint8_t system_ui_overlaid;
    uint8_t in_background;
    uint8_t rest[510];
} SysStatus;
static const char  *s_app_title = "ULTIMATE PS4 MEDIA PLAYER";

/* video playback */
static int          s_video_mode;
static int          s_video_paused;
static SDL_Texture *s_video_tex;
static int          s_video_tex_w, s_video_tex_h;
static int          s_video_serial = -1;
static int          s_view_w = SCREEN_W;   /* real renderer output size */
static int          s_view_h = SCREEN_H;

/* video settings: picture fit, display brightness and subtitles. They are
 * display-side on purpose - the decoder's output is untouched, so changing
 * brightness costs nothing per frame and seeking never re-decodes. */
static Uint32       s_video_hud_until;   /* SDL tick the transport bar hides at */
static char         s_toast[64];         /* "SUBTITLES ON", fades out */
static Uint32       s_toast_start;
static Uint32       s_toast_dur;
static int          s_video_aspect = UI_ASPECT_FIT;
static int          s_video_brightness;
static int          s_video_saturation = 100;
static int          s_video_sharpness;
static int          s_video_settings;
static int          s_settings_row;
static int          s_sub_size = UI_SUB_NORMAL;
static char         s_sub_label[128] = "NONE FOUND";
static int          s_vid_dirty = 1;     /* something on screen changed */
static int          s_vid_visible;       /* the bar or a toast was showing */
static Uint32       s_vid_painted;       /* SDL tick of the last present */
static int          s_video_fullscreen;  /* the preview fills the screen */

/* Picture viewer state. Its grade is the viewer's, like the equalizer, and is
 * stored next to the logs so a setting made once stays set. */
static int  s_pic_fit = UI_ASPECT_FIT;
static int  s_pic_brightness;
static int  s_pic_saturation = 100;
static int  s_pic_sharpness;
static int  s_pic_rotation;
static int  s_pic_zoom = 100;
static int  s_pic_pan_x, s_pic_pan_y;
static int  s_pic_zoom_mode;
static int  s_pic_settings;
static int  s_pic_row;
static int  s_pic_loaded;
static int  s_pic_fullscreen;            /* the photo fills the screen */
/* Photos arrive with red and blue the other way round from the video frames
 * (the v55 log shows every picture byte order as b,g,r,a while the decoder's
 * own frames draw correct skin), so the swap is on unless the viewer says
 * otherwise. COLOR ORDER in the picture settings flips it back. */
static int  s_pic_rb_swap = 1;           /* COLOR ORDER: red and blue reversed */
static char s_pic_note[64] = "NO PICTURE";

static SDL_Texture *s_pic_tex;
static int          s_pic_tex_w, s_pic_tex_h, s_pic_serial = -1;

static int  s_vis_style;                 /* VIS_STYLE_*, TRIANGLE cycles it */
static int  s_vis_full;                  /* L3: the visualiser fills the screen */
static int  s_theme;                     /* UI theme/skin, SQUARE on home */

/* The settings live in config.ini now. These two are the join between the file
 * and the running state: apply on boot (and on a reload), capture and save
 * whenever the user changes something. */
static void settings_apply(void)
{
    int b, n;

    cfg_ready();

    /* A reload can change the skin, the palette or a quality limit, and the
     * baked layers were built from those: throw them away with the rest. */
    vis_art_cache_invalidate();

    vol_set_db(&s_vol, (float)cfg.player.volume_db);
    s_vol.muted = cfg.player.muted ? 1 : 0;

    s_eq.enabled = cfg.audio.eq_enabled ? 1 : 0;
    for (b = 0; b < EQ_BANDS; b++)
        eq_set_band_db(&s_eq, b, (float)cfg.audio.gain_db[b]);
    eq_set_preamp_db(&s_eq, (float)cfg.audio.preamp_db);
    n = cfg.audio.eq_preset;
    s_eq_preset = (n >= 0 && n < EQ_PRESET_COUNT) ? n : eq_preset_match(&s_eq);

    n = cfg.ui.theme;
    s_theme = (n >= 0 && n < ui_theme_count()) ? n : 0;
    n = cfg.ui.visualizer;
    s_vis_style = (n >= 0 && n < VIS_STYLE_COUNT) ? n : VIS_STYLE_LIQUID;
    s_vis_full  = cfg.ui.visualizer_full ? 1 : 0;

    s_video_aspect     = cfg.ui.video_aspect;
    s_video_brightness = cfg.ui.video_brightness;
    s_video_saturation = cfg.ui.video_saturation;
    s_video_sharpness  = cfg.ui.video_sharpness;
    s_sub_size         = cfg.ui.subtitle_size;

    s_pic_fit        = cfg.ui.picture_aspect;
    s_pic_brightness = cfg.ui.picture_brightness;
    s_pic_saturation = cfg.ui.picture_saturation;
    s_pic_sharpness  = cfg.ui.picture_sharpness;
    s_pic_rotation   = cfg.ui.picture_rotation;
    s_pic_zoom       = cfg.ui.zoom;
    s_pic_rb_swap    = cfg.ui.color_order ? 1 : 0;

    /* The beat detector's thresholds are [audio] settings like the rest. */
    vis_beat_config((float)cfg.audio.beat_ratio,
                    (float)cfg.audio.beat_flux_ratio,
                    (float)(cfg.audio.beat_lockout_ms / 1000.0),
                    (float)cfg.audio.beat_history_s);
}

static void settings_save(void)
{
    int b;

    cfg.player.volume_db = (double)vol_current_db(&s_vol);
    cfg.player.muted     = s_vol.muted ? 1 : 0;

    cfg.audio.eq_enabled = s_eq.enabled ? 1 : 0;
    for (b = 0; b < EQ_BANDS; b++)
        cfg.audio.gain_db[b] = (double)s_eq.gain_db[b];
    cfg.audio.preamp_db = (double)s_eq.preamp_db;
    cfg.audio.eq_preset = s_eq_preset;

    cfg.ui.theme           = s_theme;
    cfg.ui.visualizer      = s_vis_style;
    cfg.ui.visualizer_full = s_vis_full ? 1 : 0;

    cfg.ui.video_aspect     = s_video_aspect;
    cfg.ui.video_brightness = s_video_brightness;
    cfg.ui.video_saturation = s_video_saturation;
    cfg.ui.video_sharpness  = s_video_sharpness;
    cfg.ui.subtitle_size    = s_sub_size;

    cfg.ui.picture_aspect     = s_pic_fit;
    cfg.ui.picture_brightness = s_pic_brightness;
    cfg.ui.picture_saturation = s_pic_saturation;
    cfg.ui.picture_sharpness  = s_pic_sharpness;
    cfg.ui.picture_rotation   = s_pic_rotation;
    cfg.ui.zoom               = s_pic_zoom;
    cfg.ui.color_order        = s_pic_rb_swap ? 1 : 0;

    cfg_save();
}

/* ------------------------------------------------------------------ */
/* File scanning                                                       */
/* ------------------------------------------------------------------ */

static int has_suffix(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    return ls >= lx && strcasecmp(s + ls - lx, suffix) == 0;
}

static int already_added(const char *path)
{
    int i;

    for (i = 0; i < s_track_count; i++)
        if (strcmp(s_tracks[i].path, path) == 0)
            return 1;
    return 0;
}

static int pic_already(const char *path)
{
    int i;

    for (i = 0; i < s_pic_count; i++)
        if (strcmp(s_pics[i].path, path) == 0)
            return 1;
    return 0;
}

static int pic_index_for(const char *path)
{
    int i;

    for (i = 0; i < s_pic_count; i++)
        if (strcmp(s_pics[i].path, path) == 0)
            return i;
    return -1;
}

/* ---------------------------------------------------------- library cache -- */

/* Reading a tag means opening the file, and a collection of three thousand
 * songs would mean three thousand opens on every boot. The tags are therefore
 * kept in one index file next to the logs, keyed by path and invalidated by
 * the file's own size and modification time, so a rescan only opens what is
 * new or has changed. */
#define LIB_CACHE "/data/ultimate-ps4-media-player/library.idx"

typedef struct {
    char path[MAX_PATH_LEN];
    char title[TAG_LEN];
    char artist[TAG_LEN];
    char album[TAG_LEN];
    char genre[TAG_LEN];
    int  track_no;
    long mtime;
    long size;
} CacheRec;

static CacheRec *s_cache;
static int       s_cache_n;
static int       s_cache_dirty;
static int       s_cache_loaded;

static int cmp_cache(const void *a, const void *b)
{
    return strcmp(((const CacheRec *)a)->path, ((const CacheRec *)b)->path);
}

static void cache_load(void)
{
    FILE *f;
    long  n, i;

    if (s_cache_loaded)
        return;
    s_cache_loaded = 1;
    f = fopen(LIB_CACHE, "rb");
    if (!f)
        return;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return;
    }
    {
        long bytes = ftell(f);

        /* a file that is not a whole number of records is not one of ours */
        if (bytes <= 0 || (bytes % (long)sizeof(CacheRec)) != 0) {
            fclose(f);
            return;
        }
        n = bytes / (long)sizeof(CacheRec);
    }
    if (n <= 0 || n > 400000) {
        fclose(f);
        return;
    }
    s_cache = (CacheRec *)calloc((size_t)n, sizeof(CacheRec));
    if (!s_cache) {
        fclose(f);
        return;
    }
    rewind(f);
    s_cache_n = (int)fread(s_cache, sizeof(CacheRec), (size_t)n, f);
    fclose(f);
    for (i = 0; i < s_cache_n; i++) {
        s_cache[i].path[MAX_PATH_LEN - 1]   = '\0';
        s_cache[i].title[TAG_LEN - 1]       = '\0';
        s_cache[i].artist[TAG_LEN - 1]      = '\0';
        s_cache[i].album[TAG_LEN - 1]       = '\0';
        s_cache[i].genre[TAG_LEN - 1]       = '\0';
    }
    qsort(s_cache, (size_t)s_cache_n, sizeof(s_cache[0]), cmp_cache);
    plog("library: %d cached entries", s_cache_n);
}

static CacheRec *cache_find(const char *path)
{
    CacheRec key;

    if (!s_cache || s_cache_n <= 0)
        return NULL;
    memset(&key, 0, sizeof(key));
    snprintf(key.path, sizeof(key.path), "%s", path);
    return (CacheRec *)bsearch(&key, s_cache, (size_t)s_cache_n,
                               sizeof(s_cache[0]), cmp_cache);
}

static void cache_save(void)
{
    FILE *f;
    int   i;

    if (!s_cache_dirty)
        return;
    f = fopen(LIB_CACHE, "wb");
    if (!f)
        return;
    s_cache_dirty = 0;
    for (i = 0; i < s_track_count; i++) {
        CacheRec    r;
        struct stat st;

        if (s_tracks[i].is_video)
            continue;
        memset(&r, 0, sizeof(r));
        snprintf(r.path, sizeof(r.path), "%s", s_tracks[i].path);
        snprintf(r.title, TAG_LEN, "%s", s_tracks[i].title);
        snprintf(r.artist, TAG_LEN, "%s", s_tracks[i].artist);
        snprintf(r.album, TAG_LEN, "%s", s_tracks[i].album);
        snprintf(r.genre, TAG_LEN, "%s", s_tracks[i].genre);
        r.track_no = s_tracks[i].track_no;
        if (stat(r.path, &st) == 0) {
            r.mtime = (long)st.st_mtime;
            r.size  = (long)st.st_size;
        }
        if (fwrite(&r, sizeof(r), 1, f) != 1)
            break;
    }
    fclose(f);
    plog("library: index saved");
}

/* The words a row shows: the tag when the file has one, "Artist - Title" out
 * of the file name when it does not, and the folder's name for an album that
 * names nothing. Artist, album and genre always end up non-empty, so an
 * untagged file still lands in a browsable place instead of vanishing. */
static void track_fill_tags(Track *t, const char *path)
{
    struct stat st;
    CacheRec   *hit;
    int         have_st = (stat(path, &st) == 0);

    cache_load();
    hit = cache_find(path);
    if (hit && have_st && hit->mtime == (long)st.st_mtime &&
        hit->size == (long)st.st_size) {
        snprintf(t->title, TAG_LEN, "%s", hit->title);
        snprintf(t->artist, TAG_LEN, "%s", hit->artist);
        snprintf(t->album, TAG_LEN, "%s", hit->album);
        snprintf(t->genre, TAG_LEN, "%s", hit->genre);
        t->track_no = hit->track_no;
        return;
    }

    {
        Tags tg;
        char folder[TAG_LEN];
        const char *slash_from;
        int  i, n;

        tags_read(path, &tg);
        snprintf(t->title, TAG_LEN, "%s", tg.title);
        snprintf(t->artist, TAG_LEN, "%s", tg.artist);
        snprintf(t->album, TAG_LEN, "%s", tg.album);
        snprintf(t->genre, TAG_LEN, "%s", tg.genre);
        t->track_no = tg.track;

        if (!t->title[0] || !t->artist[0]) {
            char a[TAG_LEN], ti[TAG_LEN];

            if (tags_split_name(t->name, a, ti, TAG_LEN)) {
                if (!t->artist[0])
                    snprintf(t->artist, TAG_LEN, "%s", a);
                if (!t->title[0])
                    snprintf(t->title, TAG_LEN, "%s", ti);
            }
        }
        if (!t->title[0]) {
            n = (int)strlen(t->name);
            if (n > 4 && !strcasecmp(t->name + n - 4, ".mp3"))
                n -= 4;
            if (n > TAG_LEN - 1)
                n = TAG_LEN - 1;
            for (i = 0; i < n; i++)
                t->title[i] = (t->name[i] == '_') ? ' ' : t->name[i];
            t->title[n] = '\0';
        }
        if (!t->album[0]) {
            /* the folder the file sits in is usually the album */
            snprintf(folder, sizeof(folder), "%s", path);
            slash_from = strrchr(folder, '/');
            if (slash_from && slash_from != folder) {
                int cut = (int)(slash_from - folder);

                folder[cut] = '\0';
                slash_from = strrchr(folder, '/');
                snprintf(t->album, TAG_LEN, "%s",
                         slash_from ? slash_from + 1 : folder);
            }
        }
        if (!t->artist[0])
            snprintf(t->artist, TAG_LEN, "UNKNOWN ARTIST");
        if (!t->album[0])
            snprintf(t->album, TAG_LEN, "UNKNOWN ALBUM");
        if (!t->genre[0])
            snprintf(t->genre, TAG_LEN, "UNKNOWN GENRE");
    }
    s_cache_dirty = 1;
}

/* One directory entry: the suffix rules, the duplicate check, the tag read. */
static void scan_entry(const char *dir, const char *name)
{
    Track *t;
    char   path[MAX_PATH_LEN];
    int    n;
    int is_mp3   = has_suffix(name, ".mp3");
    int is_video = has_suffix(name, ".mp4") || has_suffix(name, ".mov");
    int is_pic   = has_suffix(name, ".jpg")  || has_suffix(name, ".jpeg") ||
                   has_suffix(name, ".png")  || has_suffix(name, ".bmp");

    if (s_track_count >= MAX_TRACKS)
        return;
    n = snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (n < 0 || n >= (int)sizeof(path))
        return;

    /* pictures have their own list; they are not tracks */
    if (is_pic) {
        if (s_pic_count < MAX_TRACKS && !pic_already(path)) {
            snprintf(s_pics[s_pic_count].path, MAX_PATH_LEN, "%s", path);
            snprintf(s_pics[s_pic_count].name, MAX_PATH_LEN, "%s", name);
            s_pic_count++;
        }
        return;
    }
    if (!is_mp3 && !is_video)
        return;
    /* a drive root and its music/ subfolder can both be scanned, and /usb0
     * may alias /mnt/usb0 on some setups */
    if (already_added(path))
        return;

    t = &s_tracks[s_track_count];
    memset(t, 0, sizeof(*t));
    snprintf(t->path, sizeof(t->path), "%s", path);
    snprintf(t->name, sizeof(t->name), "%s", name);
    t->is_video = is_video ? 1 : 0;
    if (t->is_video) {
        /* a video carries no ID3 tag: its file name is all there is */
        snprintf(t->title, TAG_LEN, "%s", name);
        snprintf(t->artist, TAG_LEN, "VIDEOS");
        snprintf(t->album, TAG_LEN, "VIDEOS");
        snprintf(t->genre, TAG_LEN, "VIDEO");
    } else {
        track_fill_tags(t, path);
    }
    s_track_count++;
}

/* ---------------------------------------------------- the walk over time -- */

/* A collection of three thousand songs means three thousand file opens to read
 * their tags, and doing all of that before the first frame is what left the
 * console on a black screen. The walk is therefore a queue of folders plus a
 * slice of work per frame: the app draws at once and the library fills in
 * behind it. */
#define SCAN_Q 4096

typedef struct {
    char path[MAX_PATH_LEN];
    int  depth;
} ScanDir;

static ScanDir s_scanq[SCAN_Q];
static int     s_scan_head, s_scan_tail;
static DIR    *s_scan_d;
static char    s_scan_cur[MAX_PATH_LEN];
static int     s_scan_depth;
static int     s_scanning;
static int     s_scan_files;
static uint64_t s_scan_t0;

static uint64_t scan_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void scan_push(const char *path, int depth)
{
    if (s_scan_tail >= SCAN_Q)
        return;
    snprintf(s_scanq[s_scan_tail].path, MAX_PATH_LEN, "%s", path);
    s_scanq[s_scan_tail].depth = depth;
    s_scan_tail++;
}

/* Every place media can live on an OrbisOS console. Missing mount points
 * simply fail to open, so an unplugged port costs nothing. */
static void scan_begin(void)
{
    static const char *const roots[] = {
        "/data/music",
        "/data/linux/music",
        "/mnt/usb0/music", "/mnt/usb0",
        "/mnt/usb1/music", "/mnt/usb1",
        "/mnt/usb2/music", "/mnt/usb2",
        "/mnt/usb3/music", "/mnt/usb3",
        "/mnt/usb4/music", "/mnt/usb4",
        "/mnt/usb5/music", "/mnt/usb5",
        "/mnt/usb6/music", "/mnt/usb6",
        "/mnt/usb7/music", "/mnt/usb7",
        "/mnt/ext0/music", "/mnt/ext0",
        "/usb0/music", "/usb1/music",
    };
    size_t i;

    if (s_scan_d) {
        closedir(s_scan_d);
        s_scan_d = NULL;
    }
    s_track_count = 0;
    s_pic_count   = 0;
    s_scan_head   = s_scan_tail = 0;
    s_scan_files  = 0;
    s_scanning    = 1;
    s_scan_t0     = scan_now_ms();
    /* Four levels under /data reaches /data/music/Artist/Album/song.mp3, and
     * three under a USB root reaches /mnt/usb0/Music/Artist/Album/song.mp3.
     * The old walk stopped at two and one, which is why most of a real
     * collection never appeared at all. */
    scan_push("/data", 4);
    for (i = 0; i < sizeof(roots)/sizeof(roots[0]); i++)
        scan_push(roots[i], 3);
    s_mv_dirty = 1;
    plog("library: scan started");
}

/* The scan summary, in the one log: the numbers a library problem is read
 * from, plus the first tracks as the scan read them at DEBUG level. */
static void log_scan_summary(unsigned ms)
{
    int i, videos = 0;

    for (i = 0; i < s_track_count; i++)
        if (s_tracks[i].is_video)
            videos++;

    log_msg(LOG_INFO, "library",
            "%d tracks (%d music, %d video), %d pictures, %d entries read in %u ms",
            s_track_count, s_track_count - videos, videos, s_pic_count,
            s_scan_files, ms);
    for (i = 0; i < s_track_count && i < 12; i++)
        log_msg(LOG_DEBUG, "library", "track %d: %s | %s | %s | %s", i,
                s_tracks[i].title, s_tracks[i].artist, s_tracks[i].album,
                s_tracks[i].genre);

    /* v118: a name the list cannot draw is a name whose bytes matter. One line
     * per such file - the raw bytes, the string the model holds, and whether
     * that string is valid UTF-8. The atlas covers ASCII 32-126 only
     * (src/font_atlas.h, font_inter.h), so any non-ASCII byte is a glyph the
     * font does not have; this says whether the bytes arrived mangled first. */
    for (i = 0; i < s_track_count; i++) {
        const unsigned char *p = (const unsigned char *)s_tracks[i].title;
        int nonascii = 0, bad = 0;

        if (!p)
            continue;
        for (; *p; p++)
            if (*p >= 0x80)
                nonascii++;
        if (nonascii == 0)
            continue;

        p = (const unsigned char *)s_tracks[i].title;
        while (*p) {
            int need;

            if (*p < 0x80) {
                p++;
                continue;
            }
            need = (*p >= 0xC2 && *p <= 0xDF) ? 1
                 : (*p >= 0xE0 && *p <= 0xEF) ? 2
                 : (*p >= 0xF0 && *p <= 0xF4) ? 3 : -1;
            if (need < 0) {
                bad++;
                p++;
                continue;
            }
            {
                int k;

                for (k = 1; k <= need; k++)
                    if ((p[k] & 0xC0) != 0x80) {
                        bad++;
                        break;
                    }
            }
            p += need + 1;
        }
        log_msg(LOG_INFO, "library",
                "name non-ascii=%d utf8_bad=%d glyphs=ascii32-126-only bytes=%02X %02X %02X %02X %02X %02X %02X %02X stored=%s",
                nonascii, bad,
                (unsigned char)s_tracks[i].title[0],
                (unsigned char)s_tracks[i].title[1],
                (unsigned char)s_tracks[i].title[2],
                (unsigned char)s_tracks[i].title[3],
                (unsigned char)s_tracks[i].title[4],
                (unsigned char)s_tracks[i].title[5],
                (unsigned char)s_tracks[i].title[6],
                (unsigned char)s_tracks[i].title[7],
                s_tracks[i].title);
    }
}

/* Works for at most budget_ms, then hands the frame back. */
static int scan_slice(int budget_ms)
{
    uint64_t t0    = scan_now_ms();
    int      guard = 0;

    if (!s_scanning)
        return 0;

    while (scan_now_ms() - t0 < (uint64_t)budget_ms && guard++ < 4000) {
        struct dirent *e;

        if (!s_scan_d) {
            if (s_scan_head >= s_scan_tail) {
                unsigned ms = (unsigned)(scan_now_ms() - s_scan_t0);

                s_scanning = 0;
                cache_save();
                s_mv_dirty = 1;
                log_scan_summary(ms);
                snprintf(s_status, sizeof(s_status),
                         "Library ready   %d track%s   %d picture%s   %.1f s",
                         s_track_count, s_track_count == 1 ? "" : "s",
                         s_pic_count, s_pic_count == 1 ? "" : "s",
                         (double)ms / 1000.0);
                return 0;
            }
            snprintf(s_scan_cur, sizeof(s_scan_cur), "%s",
                     s_scanq[s_scan_head].path);
            s_scan_depth = s_scanq[s_scan_head].depth;
            s_scan_head++;
            s_scan_d = opendir(s_scan_cur);
            if (!s_scan_d)
                continue;               /* not mounted: normal, skip it */
        }

        e = readdir(s_scan_d);
        if (!e) {
            closedir(s_scan_d);
            s_scan_d = NULL;
            continue;
        }
        s_scan_files++;
        if (e->d_name[0] == '.')
            continue;                   /* . and .. and hidden folders */

        if (s_scan_depth > 0 &&
            strcmp(e->d_name, "linux") != 0 &&
            strcmp(e->d_name, "ultimate-ps4-media-player") != 0) {
            char        sub[MAX_PATH_LEN];
            struct stat st;
            int         n = snprintf(sub, sizeof(sub), "%s/%s",
                                     s_scan_cur, e->d_name);

            if (n > 0 && n < (int)sizeof(sub) && stat(sub, &st) == 0 &&
                S_ISDIR(st.st_mode)) {
                scan_push(sub, s_scan_depth - 1);
                continue;
            }
        }
        scan_entry(s_scan_cur, e->d_name);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Playback                                                            */
/* ------------------------------------------------------------------ */

/* Fills and submits whole sceAudioOut blocks. Video packets can be larger than
 * one block (AAC frames arrive in bursts), so this loops instead of trimming:
 * the old truncating version quietly dropped whatever did not fit. */
static uint64_t s_pace_t0;      /* ms when the clock below was armed */
static uint64_t s_pace_frames;  /* frames handed to the port since then */

static uint64_t pace_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static void audio_pace_reset(void)
{
    s_pace_t0     = 0;
    s_pace_frames = 0;
}

/* The clock behind the progress bar. Wall time, not the decoder's frame
 * counter: the decoder runs ahead of what is actually heard and advances in
 * bursts, so a bar read straight off it jumps and stalls on the beat. */
static double   s_clock_pos;
static uint64_t s_clock_ms;

static void play_clock_arm(double pos)
{
    s_clock_pos = pos < 0.0 ? 0.0 : pos;
    s_clock_ms  = pace_now_ms();
}

static double play_clock_advance(void)
{
    uint64_t now = pace_now_ms();

    s_clock_pos += (double)(now - s_clock_ms) / 1000.0;
    s_clock_ms = now;
    return s_clock_pos;
}

/* The console's player decodes only as fast as its audio is consumed. Feeding
 * it faster (which is what happens when the sound port never opened) drains
 * its buffer and it goes inactive about two seconds in, which is exactly what
 * the logs showed. So sound is released at 1x wall-clock speed here, whether
 * or not a port is open. */
static void push_block(const float *stereo, int frames)
{
    static int16_t block[BLOCK_FRAMES * 2];
    static int     fill;

    while (frames > 0) {
        int n = BLOCK_FRAMES - fill;
        if (n > frames)
            n = frames;

        audio_to_s16(stereo, block + fill * 2, n * 2);
        stereo += n * 2;
        fill   += n;
        frames -= n;

        if (fill == BLOCK_FRAMES) {
            /* With a real port open, sceAudioOutOutput already waits exactly
             * one block, which is the clock. Adding our own sleep on top made
             * every block late, which is what made MP3s stutter. */
            if (!aout_is_open()) {
                uint64_t now = pace_now_ms();
                uint64_t due;

                if (s_pace_t0 == 0)
                    s_pace_t0 = now;
                due = s_pace_t0 + (s_pace_frames + (uint64_t)BLOCK_FRAMES) * 1000ULL
                                / (uint64_t)OUT_RATE;
                if (due > now)
                    usleep((useconds_t)((due - now) * 1000ULL));
                s_pace_frames += (uint64_t)BLOCK_FRAMES;
            }
            aout_push_s16(block);
            fill = 0;
        }
    }
}

static void refresh_sub_label(void)
{
    if (sub_count() == 0)
        snprintf(s_sub_label, sizeof(s_sub_label), "NONE FOUND");
    else if (!sub_enabled())
        snprintf(s_sub_label, sizeof(s_sub_label), "OFF");
    else
        snprintf(s_sub_label, sizeof(s_sub_label), "%s (%d/%d)",
                 sub_name(sub_index()), sub_index() + 1, sub_count());
}

static void video_show_hud(void)
{
    s_video_hud_until = SDL_GetTicks() + HUD_MS;
}

/* Fully opaque for the first 0.8 s of its life, then a fade. */
static void toast_show_for(const char *msg, Uint32 ms)
{
    snprintf(s_toast, sizeof(s_toast), "%s", msg);
    s_toast_start = SDL_GetTicks();
    s_toast_dur   = ms;
}

/* 1.6 s on screen: the length every video confirmation uses. */
static void video_toast_show(const char *msg)
{
    toast_show_for(msg, 1600);
}

static void toggle_subtitles(void)
{
    if (sub_count() == 0) {
        video_toast_show("NO SUBTITLE FILES");
        return;
    }
    if (sub_enabled()) {
        sub_set_enabled(0);
        video_toast_show("SUBTITLES OFF");
    } else {
        sub_set_enabled(1);
        video_toast_show("SUBTITLES ON");
    }
    refresh_sub_label();
}

/* Every slider moves in single units. Cross-country runs are made bearable by
 * the d-pad repeat speeding up while a direction is held, not by jumping the
 * value in tens - a one-step change is the only kind that can be undone with
 * one press. */
static void video_setting_step(int dir)
{
    switch (s_settings_row) {
    case UI_VIDEO_ROW_ASPECT:
        s_video_aspect = (s_video_aspect + dir + UI_ASPECT_COUNT) % UI_ASPECT_COUNT;
        break;
    case UI_VIDEO_ROW_BRIGHTNESS:
        s_video_brightness += dir;
        if (s_video_brightness >  100) s_video_brightness =  100;
        if (s_video_brightness < -100) s_video_brightness = -100;
        break;
    case UI_VIDEO_ROW_SATURATION:
        s_video_saturation += dir;
        if (s_video_saturation > 250) s_video_saturation = 250;
        if (s_video_saturation <   0) s_video_saturation =   0;
        video_set_picture(s_video_saturation, s_video_sharpness);
        break;
    case UI_VIDEO_ROW_SHARPNESS:
        s_video_sharpness += dir;
        if (s_video_sharpness > 100) s_video_sharpness = 100;
        if (s_video_sharpness <   0) s_video_sharpness =   0;
        video_set_picture(s_video_saturation, s_video_sharpness);
        break;
    case UI_VIDEO_ROW_SUBTITLES:
        if (sub_count() > 0) {
            sub_cycle();
            refresh_sub_label();
        }
        break;
    case UI_VIDEO_ROW_SUB_SIZE:
        s_sub_size = (s_sub_size + dir + UI_SUB_SIZE_COUNT) % UI_SUB_SIZE_COUNT;
        break;
    default:
        break;
    }
    /* Kept for the next boot, and every change is logged with the value it
     * landed on: a setting that "did not stick" can then be read back. */
    settings_save();
    plog("video: setting aspect=%d bright=%d color=%d%% sharp=%d%%",
         s_video_aspect, s_video_brightness,
         s_video_saturation, s_video_sharpness);
}

static void play_track(int index)
{
    float decoded[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
    int   frames, tries;

    if (index < 0 || index >= s_track_count)
        return;

    s_playing = 0;
    if (s_src_open) {
        mp3_close(&s_src);
        s_src_open = 0;
    }

    if (s_video_mode) {
        video_close();
        sub_close();
        s_video_mode     = 0;
        s_video_paused   = 0;
        s_video_settings = 0;
    }

    /* the list that started this file follows it, whichever section it is in */
    s_selected = index;
    if (s_tracks[index].is_video)
        s_sel_video = index;
    else
        s_sel_music = index;

    plog("play: index=%d video=%d %s", index, s_tracks[index].is_video,
         s_tracks[index].path);

    if (s_tracks[index].is_video) {
        audio_pace_reset();
        if (video_open(s_tracks[index].path) != 0) {
            snprintf(s_status, sizeof(s_status), "%s", video_last_error());
            s_current = index;
            return;
        }
        /* Sidecar .srt/.vtt next to the file; none is a normal case. */
        sub_open(s_tracks[index].path);
        refresh_sub_label();
        s_video_mode     = 1;
        s_video_paused   = 0;
        s_video_settings = 0;
        s_settings_row   = 0;
        s_toast[0]       = '\0';
        video_show_hud();
        s_video_serial   = -1;              /* force the first upload */
        s_current        = index;
        s_track_finished = 0;
        s_duration       = video_duration_sec();
        snprintf(s_format, sizeof(s_format), "%s", video_format_note());
        snprintf(s_status, sizeof(s_status), "%s   %s",
                 s_tracks[index].name, video_format_note());
        s_playing = 1;
        return;
    }

    if (mp3_open(&s_src, s_tracks[index].path) != 0) {
        snprintf(s_status, sizeof(s_status), "Cannot open %s", s_tracks[index].name);
        return;
    }
    s_src_open = 1;
    s_current  = index;
    s_track_finished = 0;
    s_duration = mp3_duration_est(&s_src);

    /* Decode one frame to learn the stream format, then rewind to the start.
     * Several attempts, because the first frame of a real file is often behind
     * an ID3 tag: one attempt was enough to refuse files that play fine. */
    frames = 0;
    for (tries = 0; tries < 24 && frames <= 0 && !s_src.finished; tries++)
        frames = mp3_read(&s_src, decoded, 1152);
    plog("song: %s size=%lu first=%d tries=%d%s",
         s_tracks[index].name, (unsigned long)s_src.size, frames, tries,
         s_src.finished ? " (reached end)" : "");
    /* What the bytes actually are. A file that refuses to play is either a
     * different container renamed .mp3, an ID3 tag with no audio behind it, or
     * a real MP3 with broken frames - and the header says which. */
    {
        char hex[3 * 17];
        int  k, hn = s_src.size < 16 ? (int)s_src.size : 16;

        for (k = 0; k < hn; k++)
            snprintf(hex + k * 3, 4, "%02X ", s_src.data[k]);
        hex[hn * 3] = '\0';
        plog("song: %s container=%s id3=%lu firstsync=%lu head=%s",
             s_tracks[index].name, mp3_container(&s_src),
             (unsigned long)mp3_id3_size(&s_src),
             (unsigned long)mp3_first_sync(&s_src), hex);
    }
    if (frames > 0) {
        int ch = s_src.channels;
        /* Only the filter state is cleared, never the settings: the whole
         * equaliser used to be reset here on every song change. */
        eq_reset(&s_eq);
        rs_init(&s_rs, s_src.hz > 0 ? s_src.hz : 44100, OUT_RATE, ch);
        snprintf(s_format, sizeof(s_format), "MP3   %d HZ   %s",
                 s_src.hz, ch == 2 ? "STEREO" : "MONO");
        plog("song: %s hz=%d ch=%d bitrate=%dkbps est=%.1fs",
             s_tracks[index].name, s_src.hz, ch, s_src.bitrate_kbps,
             mp3_duration_est(&s_src));
        snprintf(s_status, sizeof(s_status), "%s   %d Hz %s", s_tracks[index].name,
                 s_src.hz, ch == 2 ? "stereo" : "mono");
    }
    /* Rewind instead of reopening: mp3_open() clears the parsed header, and
     * that is why the duration came out as "--:--" (no bitrate to estimate
     * from, no exact frame count yet). */
    mp3dec_init(&s_src.dec);
    s_src.pos            = 0;
    s_src.frames_decoded = 0;
    s_src.finished       = 0;
    s_duration = mp3_duration_est(&s_src);
    play_clock_arm(0.0);

    /* The probe above decoded nothing: the file is not an MP3 (AAC or WAV
     * renamed, say) or its frames are corrupt. Starting playback anyway made
     * it run out instantly and jump to the next song, over and over. */
    if (frames <= 0) {
        plog("song: %s gave no audio on the first frame (%lu bytes)",
             s_tracks[index].name, (unsigned long)s_src.size);
        mp3_close(&s_src);
        s_src_open = 0;
        s_playing  = 0;
        snprintf(s_status, sizeof(s_status), "Cannot decode %s",
                 s_tracks[index].name);
        return;
    }

    s_playing = 1;
}

/* ------------------------------------------------------------------ */
/* Drive browser                                                       */
/* ------------------------------------------------------------------ */

#define MAX_SOURCES  16
#define MAX_ENTRIES  512
#define BROWSER_PAGE 10

/* EK_FILE is any file the player cannot open: the browser lists everything in
 * a folder so it can be copied, moved or deleted, but only the four kinds
 * above can be played, shown or opened. */
enum { EK_DIR, EK_MP3, EK_VIDEO, EK_PICTURE, EK_FILE,
       EK_RESCAN, EK_ALLMEDIA, EK_UP };

typedef struct {
    char name[256];
    int  kind;
    int  source;                /* index into s_sources for drive rows */
} Entry;

typedef struct {
    char label[40];
    char path[MAX_PATH_LEN];
} Source;

static Source  s_sources[MAX_SOURCES];
static int     s_source_count;
static Entry   s_entries[MAX_ENTRIES];
static UiEntry s_uientries[MAX_ENTRIES];
static int     s_entry_count;
static int     s_entry_sel;
static char    s_browser_path[MAX_PATH_LEN];
static char    s_browser_root[MAX_PATH_LEN];
static int     s_browser_level;     /* 0 = drive list, 1 = inside a drive */
static char    s_browser_note[64];
static int     s_screen = UI_SCREEN_HOME;

/* The file manager: one clipboard slot, and a delete that has to be said
 * twice. Every operation touches files only - a recursive folder delete is
 * exactly the accident a confirm prompt is there to prevent, so folders are
 * refused with a note instead. */
static char s_clip_path[MAX_PATH_LEN];
static char s_clip_name[256];
static int  s_clip_mode;         /* 0 empty, 1 copied, 2 cut */
static int  s_browser_confirm;   /* 1 = the DELETE prompt is on screen */
static int  s_usb_count;         /* USB mounts found by the last scan */
static int  s_usb_present;       /* 1 = a storage device is in a USB slot */

/* Which mount points exist right now. opendir either opens or fails, so this
 * costs one syscall per candidate and needs no mount table. */
static void refresh_sources(void)
{
    static const struct { const char *label; const char *path; } mounts[] = {
        { "INTERNAL DRIVE",   "/data" },
        { "INTERNAL MUSIC",   "/data/music" },
        { "USB 1",            "/mnt/usb0" },
        { "USB 2",            "/mnt/usb1" },
        { "USB 3",            "/mnt/usb2" },
        { "USB 4",            "/mnt/usb3" },
        { "USB 5",            "/mnt/usb4" },
        { "USB 6",            "/mnt/usb5" },
        { "USB 7",            "/mnt/usb6" },
        { "USB 8",            "/mnt/usb7" },
        { "EXTENDED STORAGE", "/mnt/ext0" },
        { "LEGACY USB 1",     "/usb0" },
        { "LEGACY USB 2",     "/usb1" },
    };
    size_t i;

    s_source_count = 0;
    for (i = 0; i < sizeof(mounts)/sizeof(mounts[0]); i++) {
        DIR *d = opendir(mounts[i].path);

        if (!d)
            continue;
        closedir(d);
        if (s_source_count >= MAX_SOURCES)
            break;
        snprintf(s_sources[s_source_count].label,
                 sizeof(s_sources[s_source_count].label), "%s", mounts[i].label);
        snprintf(s_sources[s_source_count].path,
                 sizeof(s_sources[s_source_count].path), "%s", mounts[i].path);
        s_source_count++;
    }

    /* A plugged-in stick is a USB mount that exists right now, so the same
     * scan that fills the drive list also drives the "USB CONNECTED" light. */
    s_usb_count = 0;
    for (i = 0; i < (size_t)s_source_count; i++)
        if (strncmp(s_sources[i].path, "/mnt/usb", 8) == 0 ||
            strncmp(s_sources[i].path, "/usb", 4) == 0)
            s_usb_count++;
    s_usb_present = s_usb_count > 0;
}

/* Joins dir/name without the silently-truncating behaviour of snprintf. */
static int join_path(char *dst, size_t dstsz, const char *dir, const char *name)
{
    size_t dl = strlen(dir), nl = strlen(name);

    if (dl + 1 + nl + 1 > dstsz)
        return -1;
    memcpy(dst, dir, dl);
    dst[dl] = '/';
    memcpy(dst + dl + 1, name, nl + 1);
    return 0;
}

static void entry_add(const char *name, int kind, int source)
{
    if (s_entry_count >= MAX_ENTRIES)
        return;
    snprintf(s_entries[s_entry_count].name,
             sizeof(s_entries[s_entry_count].name), "%.255s", name);
    s_entries[s_entry_count].kind   = kind;
    s_entries[s_entry_count].source = source;
    s_entry_count++;
}

static int entry_group(int kind)
{
    switch (kind) {
    case EK_UP:       return 0;
    case EK_DIR:      return 1;
    case EK_MP3:
    case EK_VIDEO:
    case EK_PICTURE:
    case EK_FILE:     return 2;
    default:          return 3;
    }
}

static int cmp_entry(const void *a, const void *b)
{
    const Entry *x = a, *y = b;
    int gx = entry_group(x->kind), gy = entry_group(y->kind);

    if (gx != gy)
        return gx < gy ? -1 : 1;
    return strcasecmp(x->name, y->name);
}

/* Entries are sorted in place, so the UI view is rebuilt after every sort
 * rather than holding pointers into the array as it moves. */
static void sync_ui_entries(void)
{
    int i;

    for (i = 0; i < s_entry_count; i++) {
        s_uientries[i].name      = s_entries[i].name;
        s_uientries[i].is_dir    = (s_entries[i].kind == EK_DIR ||
                                    s_entries[i].kind == EK_UP);
        s_uientries[i].is_video  = (s_entries[i].kind == EK_VIDEO);
        s_uientries[i].is_drive  = (s_entries[i].source >= 0);
        s_uientries[i].is_action = (s_entries[i].kind == EK_RESCAN ||
                                    s_entries[i].kind == EK_ALLMEDIA);
        s_uientries[i].is_picture = (s_entries[i].kind == EK_PICTURE);
        s_uientries[i].is_other   = (s_entries[i].kind == EK_FILE);
    }
}

static void browser_list_drives(void)
{
    int i;

    s_browser_level = 0;
    s_entry_count   = 0;
    s_entry_sel     = 0;
    s_browser_root[0] = '\0';
    snprintf(s_browser_path, sizeof(s_browser_path), "DRIVES");

    /* No "ALL MEDIA" row here: the browser is the file manager, and the
     * library is its own section. The kind stays in the enum so the guards
     * that refuse to act on it keep their meaning. */
    for (i = 0; i < s_source_count; i++)
        entry_add(s_sources[i].label, EK_DIR, i);
    entry_add("REFRESH DRIVE SCAN", EK_RESCAN, -1);

    sync_ui_entries();
    snprintf(s_browser_note, sizeof(s_browser_note), "%d DRIVE%s",
             s_source_count, s_source_count == 1 ? "" : "S");
}

static void browser_list_dir(const char *path)
{
    DIR *d;
    struct dirent *e;

    s_browser_level = 1;
    s_entry_count   = 0;
    s_entry_sel     = 0;
    snprintf(s_browser_path, sizeof(s_browser_path), "%s", path);
    snprintf(s_browser_note, sizeof(s_browser_note), "EMPTY");

    d = opendir(path);
    if (!d) {
        sync_ui_entries();
        snprintf(s_browser_note, sizeof(s_browser_note), "CANNOT OPEN");
        return;
    }

    if (strcmp(path, s_browser_root) != 0)
        entry_add("..", EK_UP, -1);

    while ((e = readdir(d)) != NULL && s_entry_count < MAX_ENTRIES) {
        char        full[MAX_PATH_LEN];
        struct stat st;
        int         is_mp3, is_video;

        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (join_path(full, sizeof(full), path, e->d_name) != 0)
            continue;
        if (stat(full, &st) != 0)
            continue;
        if (S_ISDIR(st.st_mode)) {
            entry_add(e->d_name, EK_DIR, -1);
            continue;
        }
        is_mp3   = has_suffix(e->d_name, ".mp3");
        is_video = has_suffix(e->d_name, ".mp4") || has_suffix(e->d_name, ".mov");
        if (is_video)
            entry_add(e->d_name, EK_VIDEO, -1);
        else if (is_mp3)
            entry_add(e->d_name, EK_MP3, -1);
        else if (has_suffix(e->d_name, ".jpg")  ||
                 has_suffix(e->d_name, ".jpeg") ||
                 has_suffix(e->d_name, ".png")  ||
                 has_suffix(e->d_name, ".bmp"))
            entry_add(e->d_name, EK_PICTURE, -1);
        else
            /* Everything else is listed too, so a stray file can still be
             * moved or deleted from the pad; Cross says it cannot play it. */
            entry_add(e->d_name, EK_FILE, -1);
    }
    closedir(d);

    qsort(s_entries, (size_t)s_entry_count, sizeof(s_entries[0]), cmp_entry);
    sync_ui_entries();
    snprintf(s_browser_note, sizeof(s_browser_note), "%d ITEM%s",
             s_entry_count, s_entry_count == 1 ? "" : "S");
}

static void browser_go_up(void)
{
    char  parent[MAX_PATH_LEN];
    char *slash;
    size_t root_len = strlen(s_browser_root);

    if (s_browser_level == 0)
        return;

    /* at a drive's own root (or somehow outside it) go back to the list */
    if (root_len == 0 || strncmp(s_browser_path, s_browser_root, root_len) != 0 ||
        s_browser_path[root_len] != '/') {
        browser_list_drives();
        return;
    }

    snprintf(parent, sizeof(parent), "%s", s_browser_path);
    slash = strrchr(parent, '/');
    if (!slash || strlen(parent) <= root_len) {
        browser_list_drives();
        return;
    }
    *slash = '\0';
    browser_list_dir(parent);
}

static int track_index_for(const char *path)
{
    int i;

    for (i = 0; i < s_track_count; i++)
        if (strcmp(s_tracks[i].path, path) == 0)
            return i;
    return -1;
}

static void picture_select(int index);

static void browser_play(void)
{
    char        full[MAX_PATH_LEN];
    const char *name = s_entries[s_entry_sel].name;
    int         idx;

    if (join_path(full, sizeof(full), s_browser_path, name) != 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "PATH TOO LONG");
        return;
    }

    /* The list shows every file; only these four kinds can actually be
     * opened, and saying so is better than loading a file nothing can play. */
    if (!has_suffix(name, ".mp3")  && !has_suffix(name, ".mp4") &&
        !has_suffix(name, ".mov")  && !has_suffix(name, ".jpg") &&
        !has_suffix(name, ".jpeg") && !has_suffix(name, ".png") &&
        !has_suffix(name, ".bmp")) {
        snprintf(s_browser_note, sizeof(s_browser_note), "CANNOT PLAY THIS FILE");
        return;
    }

    /* a photo picked in the browser opens in the picture section */
    if (has_suffix(name, ".jpg") || has_suffix(name, ".jpeg") ||
        has_suffix(name, ".png") || has_suffix(name, ".bmp")) {
        int pi = pic_index_for(full);

        if (pi < 0 && s_pic_count < MAX_TRACKS) {
            snprintf(s_pics[s_pic_count].path, MAX_PATH_LEN, "%s", full);
            snprintf(s_pics[s_pic_count].name, MAX_PATH_LEN, "%s", name);
            pi = s_pic_count++;
        }
        if (pi >= 0) {
            s_screen = UI_SCREEN_PICTURES;
            picture_select(pi);
        }
        return;
    }

    idx = track_index_for(full);
    if (idx < 0) {
        Track *t;

        if (s_track_count >= MAX_TRACKS) {
            snprintf(s_browser_note, sizeof(s_browser_note), "LIBRARY FULL");
            return;
        }
        t = &s_tracks[s_track_count];
        memset(t, 0, sizeof(*t));
        snprintf(t->path, sizeof(t->path), "%s", full);
        snprintf(t->name, sizeof(t->name), "%s", name);
        t->is_video = (has_suffix(name, ".mp4") || has_suffix(name, ".mov")) ? 1 : 0;
        if (t->is_video) {
            snprintf(t->title, TAG_LEN, "%s", name);
            snprintf(t->artist, TAG_LEN, "VIDEOS");
            snprintf(t->album, TAG_LEN, "VIDEOS");
            snprintf(t->genre, TAG_LEN, "VIDEO");
        } else {
            track_fill_tags(t, full);
        }
        s_mv_dirty = 1;
        idx = s_track_count++;
    }

    s_selected = idx;
    play_track(idx);

    /* a file opened from the browser lands in its own section */
    if (s_tracks[idx].is_video) {
        s_screen           = UI_SCREEN_VIDEOS;
        s_video_fullscreen = 1;
    } else {
        s_screen = UI_SCREEN_MUSIC;
    }
}

/* Re-check the mounts and the library, keeping whatever is playing. A file
 * picked from a subfolder is not in the flat scan, so it is put back if the
 * rescan drops it. */
static void browser_refresh(void)
{
    char current[MAX_PATH_LEN];
    int  had_current = (s_current >= 0);
    int  was_video   = had_current ? s_tracks[s_current].is_video : 0;

    if (had_current)
        snprintf(current, sizeof(current), "%s", s_tracks[s_current].path);

    scan_begin();
    refresh_sources();

    if (had_current) {
        int idx = track_index_for(current);

        if (idx < 0 && s_track_count < MAX_TRACKS) {
            Track *t = &s_tracks[s_track_count];
            const char *nm = strrchr(current, '/');

            memset(t, 0, sizeof(*t));
            snprintf(t->path, sizeof(t->path), "%s", current);
            snprintf(t->name, sizeof(t->name), "%s", nm ? nm + 1 : current);
            t->is_video = was_video;
            if (was_video) {
                snprintf(t->title, TAG_LEN, "%s", t->name);
                snprintf(t->artist, TAG_LEN, "VIDEOS");
                snprintf(t->album, TAG_LEN, "VIDEOS");
                snprintf(t->genre, TAG_LEN, "VIDEO");
            } else {
                track_fill_tags(t, current);
            }
            s_mv_dirty = 1;
            idx = s_track_count++;
        }
        s_current = idx;
    }

    if (s_selected >= s_track_count)
        s_selected = s_track_count > 0 ? s_track_count - 1 : 0;

    if (s_browser_level == 0) {
        browser_list_drives();
    } else {
        DIR *d = opendir(s_browser_path);

        if (d) {
            closedir(d);
            browser_list_dir(s_browser_path);
        } else {
            browser_list_drives();      /* drive pulled out under us */
        }
    }
}

static void browser_activate(void)
{
    const Entry *e;

    if (s_entry_count == 0)
        return;
    e = &s_entries[s_entry_sel];

    switch (e->kind) {
    case EK_ALLMEDIA:
        s_screen = UI_SCREEN_LIBRARY;
        break;
    case EK_RESCAN:
        browser_refresh();
        break;
    case EK_UP:
        browser_go_up();
        break;
    case EK_DIR:
        if (s_browser_level == 0) {
            snprintf(s_browser_root, sizeof(s_browser_root), "%s",
                     s_sources[e->source].path);
            browser_list_dir(s_browser_root);
        } else {
            char full[MAX_PATH_LEN];

            if (join_path(full, sizeof(full), s_browser_path, e->name) == 0)
                browser_list_dir(full);
        }
        break;
    default:
        browser_play();
        break;
    }
}

static void browser_move(int delta)
{
    if (s_entry_count == 0)
        return;
    s_entry_sel += delta;
    if (s_entry_sel < 0)
        s_entry_sel = 0;
    if (s_entry_sel >= s_entry_count)
        s_entry_sel = s_entry_count - 1;
}

static void browser_open(void)
{
    refresh_sources();
    browser_list_drives();
    s_screen = UI_SCREEN_BROWSER;
}

/* ------------------------------------------------------- file operations -- */

static const Entry *browser_selected(void)
{
    if (s_entry_count == 0 || s_entry_sel < 0 || s_entry_sel >= s_entry_count)
        return NULL;
    return &s_entries[s_entry_sel];
}

/* Resolves the highlighted row to a full path. Actions and folders are
 * refused here, not at the call sites, so every operation says the same
 * thing about them. */
static int browser_selected_path(char *dst, size_t dstsz, const Entry **out)
{
    const Entry *e = browser_selected();

    if (!e || e->kind == EK_UP || e->kind == EK_RESCAN ||
        e->kind == EK_ALLMEDIA || s_browser_level == 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "PICK A FILE FIRST");
        return -1;
    }
    if (e->kind == EK_DIR) {
        snprintf(s_browser_note, sizeof(s_browser_note), "FOLDERS ARE NOT CHANGED");
        return -1;
    }
    if (join_path(dst, dstsz, s_browser_path, e->name) != 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "PATH TOO LONG");
        return -1;
    }
    if (out)
        *out = e;
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    FILE   *in = fopen(src, "rb");
    FILE   *out;
    char    buf[64 * 1024];
    size_t  n;
    int     ok = 0;

    if (!in)
        return -1;
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = -1;
            break;
        }
    }
    if (ferror(in))
        ok = -1;
    if (fclose(out) != 0)
        ok = -1;
    fclose(in);
    if (ok != 0)
        remove(dst);            /* never leave half a file behind */
    return ok;
}

/* Re-reads the folder that is on screen, keeping the highlight where it was
 * so a delete does not jump the list back to the top. */
static void browser_reload(void)
{
    int keep = s_entry_sel;

    refresh_sources();
    if (s_browser_level == 0) {
        browser_list_drives();
    } else {
        DIR *d = opendir(s_browser_path);

        if (!d) {
            browser_list_drives();
            return;
        }
        closedir(d);
        browser_list_dir(s_browser_path);
        s_entry_sel = keep;
        if (s_entry_sel >= s_entry_count)
            s_entry_sel = s_entry_count > 0 ? s_entry_count - 1 : 0;
    }
}

/* Square asks, Cross answers. Nothing is removed until the second press. */
static void browser_ask_delete(void)
{
    const Entry *e = browser_selected();

    if (!e || e->kind == EK_UP || e->kind == EK_RESCAN ||
        e->kind == EK_ALLMEDIA || s_browser_level == 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "PICK A FILE FIRST");
        return;
    }
    if (e->kind == EK_DIR) {
        snprintf(s_browser_note, sizeof(s_browser_note), "FOLDERS ARE NOT CHANGED");
        return;
    }
    s_browser_confirm = 1;
}

static void browser_delete_confirmed(void)
{
    char   full[MAX_PATH_LEN];
    const Entry *e = NULL;

    s_browser_confirm = 0;
    if (browser_selected_path(full, sizeof(full), &e) != 0)
        return;
    if (remove(full) != 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "DELETE FAILED");
        plog("files: delete failed %s", full);
        return;
    }
    snprintf(s_browser_note, sizeof(s_browser_note), "DELETED %.32s",
             e ? e->name : "FILE");
    plog("files: deleted %s", full);
    browser_reload();
}

static void browser_clip(int cut)
{
    char        full[MAX_PATH_LEN];
    const Entry *e = NULL;

    if (browser_selected_path(full, sizeof(full), &e) != 0)
        return;
    snprintf(s_clip_path, sizeof(s_clip_path), "%s", full);
    snprintf(s_clip_name, sizeof(s_clip_name), "%s", e->name);
    s_clip_mode = cut ? 2 : 1;
    snprintf(s_browser_note, sizeof(s_browser_note), "%s %.32s",
             cut ? "CUT" : "COPIED", s_clip_name);
    plog("files: %s %s", cut ? "cut" : "copied", full);
}

static void browser_paste(void)
{
    char dst[MAX_PATH_LEN];

    if (s_clip_mode == 0 || !s_clip_name[0]) {
        snprintf(s_browser_note, sizeof(s_browser_note), "NOTHING COPIED");
        return;
    }
    if (s_browser_level == 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "OPEN A FOLDER FIRST");
        return;
    }
    if (join_path(dst, sizeof(dst), s_browser_path, s_clip_name) != 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "PATH TOO LONG");
        return;
    }
    if (strcmp(dst, s_clip_path) == 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "ALREADY HERE");
        return;
    }
    {
        struct stat st;

        if (stat(dst, &st) == 0) {
            snprintf(s_browser_note, sizeof(s_browser_note), "NAME ALREADY EXISTS");
            return;
        }
    }
    if (copy_file(s_clip_path, dst) != 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "COPY FAILED");
        plog("files: copy failed %s -> %s", s_clip_path, dst);
        return;
    }
    if (s_clip_mode == 2) {
        remove(s_clip_path);            /* a cut removes the original */
        s_clip_mode = 0;
        s_clip_name[0] = '\0';
    }
    plog("files: pasted %s", dst);
    snprintf(s_browser_note, sizeof(s_browser_note), "PASTED %.28s",
             s_clip_name[0] ? s_clip_name : strrchr(dst, '/') + 1);
    browser_reload();
}

/* Mix buffers live here rather than on the audio thread's stack: the three of
 * them together are around 75 KB, which overflows the small default pthread
 * stack on the console and kills the process the moment the thread starts. */
/* Mixer health, reported in the perf line: how often the audio thread found
 * nothing ready (a gap the listener hears) and how often an MP3 read had to
 * skip over bytes that were not a frame. */
static int s_mix_empty;
static int s_mp3_skips;

static float s_adec[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
static float s_ares[4096 * 2];
static float s_astereo[4096 * 2];
static float s_avideo[2048 * 2];

static void *audio_thread(void *arg)
{
    (void)arg;

    plog("audio: thread started");

    while (!s_quit) {
        float *decoded   = s_adec;
        float *resampled = s_ares;
        float *stereo    = s_astereo;
        int   frames, outn, i;

        if (s_video_mode) {
            float *vstereo = s_avideo;
            int   vn;

            if (!s_playing) {
                usleep(10000);
                continue;
            }

            vn = video_read_audio(vstereo, 2048);
            if (vn <= 0) {
                s_mix_empty++;
                /* 10 ms, not 2: the empty pulls were hitting the player a
                 * thousand times a second (4832 in one five-second file),
                 * and each one takes the lock the picture needs. A 43 ms
                 * block of already-buffered audio covers the wait. */
                usleep(10000);
                continue;
            }

            if (s_eq.enabled)
                eq_process(&s_eq, vstereo, vn);
            audio_apply_gain(vstereo, vn * 2, s_vol.gain);
            audio_soft_limit(vstereo, vn * 2);
            vis_push(vstereo, vn);      /* what is heard is what is drawn */
            push_block(vstereo, vn);
            continue;
        }

        if (!s_playing || !s_src_open) {
            usleep(10000);
            continue;
        }

        frames = mp3_read(&s_src, decoded, 1152);
        if (frames == 0) {
            if (s_src.finished) {
                s_playing = 0;
                s_track_finished = 1;
            } else {
                /* Skipping a stray byte between frames: do not spin, the
                 * main loop needs the CPU to keep drawing. */
                s_mp3_skips++;
                usleep(1000);
            }
            continue;
        }

        outn = rs_process(&s_rs, decoded, frames, resampled, 4096);
        if (outn <= 0)
            continue;

        /* upmix mono onto the stereo port */
        if (s_src.channels == 1) {
            for (i = 0; i < outn; i++) {
                stereo[i * 2]     = resampled[i];
                stereo[i * 2 + 1] = resampled[i];
            }
        } else {
            memcpy(stereo, resampled, (size_t)outn * 2 * sizeof(float));
        }

        if (s_eq.enabled)
            eq_process(&s_eq, stereo, outn);
        audio_apply_gain(stereo, outn * 2, s_vol.gain);
        audio_soft_limit(stereo, outn * 2);
        vis_push(stereo, outn);         /* feeds the music visualizations */
        push_block(stereo, outn);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Music library: artists, albums, genres                              */
/* ------------------------------------------------------------------ */

/* A row of the music list. Song rows carry the index of the track they play;
 * artist, album and genre rows are folders, and picking one opens the level
 * under it instead of starting anything. */
typedef struct {
    char name[128];
    char sub[48];
    int  track;
    int  count;
    int  is_category;
} MvRow;

#define MV_TRACKS   0
#define MV_ARTISTS  1
#define MV_ALBUMS   2
#define MV_GENRES   3

static const char *const k_mv_name[] = { "TRACKS", "ARTISTS", "ALBUMS", "GENRES" };

static MvRow s_mv_rows[MAX_TRACKS];
static int   s_mv_count;
static int   s_mv;              /* MV_* */
static int   s_mv_level;        /* 0 = the list, 1 = inside, 2 = album's songs */
static char  s_mv_key[TAG_LEN]; /* artist / album / genre being looked at */
static char  s_mv_key2[TAG_LEN];/* the album inside an artist, or the artist */
static int   s_mv_row;
/* The row each level was left on. Backing out of an album used to drop you at
 * the top of the artist list, which is the one place nobody wants to be after
 * they have found the artist they were looking at. */
static int   s_mv_row_mem[4][3];

static void mv_push(const char *name, const char *sub, int track, int cat)
{
    MvRow *r;

    if (s_mv_count >= MAX_TRACKS)
        return;
    r = &s_mv_rows[s_mv_count++];
    snprintf(r->name, sizeof(r->name), "%s", (name && name[0]) ? name : "?");
    snprintf(r->sub, sizeof(r->sub), "%s", sub ? sub : "");
    r->track       = track;
    r->is_category = cat;
    r->count       = 1;
}

static int mv_cmp_name(const void *a, const void *b)
{
    return strcasecmp(((const MvRow *)a)->name, ((const MvRow *)b)->name);
}

static int mv_cmp_album(const void *a, const void *b)
{
    const MvRow *x = a, *y = b;
    int          c = strcasecmp(x->name, y->name);

    return c ? c : strcasecmp(x->sub, y->sub);
}

/* Inside an album the track number is the order the artist intended; files
 * with no number fall back to their name. */
static int mv_cmp_trackno(const void *a, const void *b)
{
    const MvRow *x = a, *y = b;
    int nx = (x->track >= 0) ? s_tracks[x->track].track_no : 0;
    int ny = (y->track >= 0) ? s_tracks[y->track].track_no : 0;

    if (nx != ny)
        return nx < ny ? -1 : 1;
    return strcasecmp(x->name, y->name);
}

/* Collapses runs of equal names into one row and counts what it swallowed. */
static void mv_merge_dupes(void)
{
    int i, n = 0;

    for (i = 0; i < s_mv_count; i++) {
        if (n > 0 && !strcasecmp(s_mv_rows[n - 1].name, s_mv_rows[i].name) &&
            !strcasecmp(s_mv_rows[n - 1].sub, s_mv_rows[i].sub)) {
            s_mv_rows[n - 1].count++;
        } else {
            s_mv_rows[n] = s_mv_rows[i];
            n++;
        }
    }
    s_mv_count = n;
}

/* mode 0 = every song, 1 = an artist's songs (a = artist, b = album when it
 * is set), 2 = an album's songs, 3 = a genre's songs. */
static void mv_tracks_of(int mode, const char *a, const char *b, int by_track_no)
{
    int i;

    s_mv_count = 0;
    for (i = 0; i < s_track_count; i++) {
        Track *t = &s_tracks[i];
        int    match = 0;

        if (t->is_video)
            continue;
        switch (mode) {
        case 0: match = 1; break;
        case 1: match = !strcasecmp(t->artist, a) &&
                        (!b || !b[0] || !strcasecmp(t->album, b)); break;
        case 2: match = !strcasecmp(t->album, a) &&
                        (!b || !b[0] || !strcasecmp(t->artist, b)); break;
        case 3: match = !strcasecmp(t->genre, a); break;
        default: break;
        }
        if (!match)
            continue;
        mv_push(t->title[0] ? t->title : t->name, t->artist, i, 0);
    }
    qsort(s_mv_rows, (size_t)s_mv_count, sizeof(s_mv_rows[0]),
          by_track_no ? mv_cmp_trackno : mv_cmp_name);
}

static void mv_artists(void)
{
    int i;

    s_mv_count = 0;
    for (i = 0; i < s_track_count; i++)
        if (!s_tracks[i].is_video)
            mv_push(s_tracks[i].artist, "", -1, 1);
    qsort(s_mv_rows, (size_t)s_mv_count, sizeof(s_mv_rows[0]), mv_cmp_name);
    mv_merge_dupes();
    for (i = 0; i < s_mv_count; i++)
        snprintf(s_mv_rows[i].sub, sizeof(s_mv_rows[i].sub), "%d TRACK%s",
                 s_mv_rows[i].count, s_mv_rows[i].count == 1 ? "" : "S");
}

static void mv_artist_albums(const char *artist)
{
    int i;

    s_mv_count = 0;
    for (i = 0; i < s_track_count; i++) {
        if (s_tracks[i].is_video)
            continue;
        if (strcasecmp(s_tracks[i].artist, artist))
            continue;
        mv_push(s_tracks[i].album, "", -1, 1);
    }
    qsort(s_mv_rows, (size_t)s_mv_count, sizeof(s_mv_rows[0]), mv_cmp_name);
    mv_merge_dupes();
    for (i = 0; i < s_mv_count; i++)
        snprintf(s_mv_rows[i].sub, sizeof(s_mv_rows[i].sub), "%d TRACK%s",
                 s_mv_rows[i].count, s_mv_rows[i].count == 1 ? "" : "S");
}

/* One row per album, with the artist on the right, so two bands that both
 * called a record "Greatest Hits" stay two records. */
static void mv_albums(void)
{
    int i;

    s_mv_count = 0;
    for (i = 0; i < s_track_count; i++) {
        if (s_tracks[i].is_video)
            continue;
        mv_push(s_tracks[i].album, s_tracks[i].artist, -1, 1);
    }
    qsort(s_mv_rows, (size_t)s_mv_count, sizeof(s_mv_rows[0]), mv_cmp_album);
    mv_merge_dupes();
}

static void mv_genres(void)
{
    int i;

    s_mv_count = 0;
    for (i = 0; i < s_track_count; i++)
        if (!s_tracks[i].is_video)
            mv_push(s_tracks[i].genre, "", -1, 1);
    qsort(s_mv_rows, (size_t)s_mv_count, sizeof(s_mv_rows[0]), mv_cmp_name);
    mv_merge_dupes();
    for (i = 0; i < s_mv_count; i++)
        snprintf(s_mv_rows[i].sub, sizeof(s_mv_rows[i].sub), "%d TRACK%s",
                 s_mv_rows[i].count, s_mv_rows[i].count == 1 ? "" : "S");
}

static void music_view_build(void)
{
    if (!s_mv_dirty)
        return;
    s_mv_dirty = 0;

    switch (s_mv) {
    case MV_ARTISTS:
        if (s_mv_level == 0)
            mv_artists();
        else if (s_mv_level == 1)
            mv_artist_albums(s_mv_key);
        else
            mv_tracks_of(1, s_mv_key, s_mv_key2, 1);
        break;
    case MV_ALBUMS:
        if (s_mv_level == 0)
            mv_albums();
        else
            mv_tracks_of(2, s_mv_key, s_mv_key2, 1);
        break;
    case MV_GENRES:
        if (s_mv_level == 0)
            mv_genres();
        else
            mv_tracks_of(3, s_mv_key, NULL, 0);
        break;
    default:
        mv_tracks_of(0, NULL, NULL, 0);
        break;
    }
    if (s_mv_row >= s_mv_count)
        s_mv_row = s_mv_count > 0 ? s_mv_count - 1 : 0;
}

/* The four headings, and how deep into one of them we are. */
static void music_view_reset(int view)
{
    int l;

    s_mv      = view;
    s_mv_level = 0;
    s_mv_key[0] = s_mv_key2[0] = '\0';
    s_mv_row   = 0;
    /* A different list is a different set of rows, so nothing is remembered
     * across a view change. */
    for (l = 0; l < 3; l++)
        s_mv_row_mem[view][l] = 0;
    s_mv_dirty = 1;
}

static void music_view_up(void)
{
    if (s_mv_level > 0) {
        /* Leave a bookmark on the level being left, come back to the one this
         * level was opened from: out of an album returns to its artist, and
         * out of the artist returns to the artist list where you left it. */
        s_mv_row_mem[s_mv][s_mv_level] = s_mv_row;
        s_mv_level--;
        if (s_mv_level < 2)
            s_mv_key2[0] = '\0';
        s_mv_row   = s_mv_row_mem[s_mv][s_mv_level];
        s_mv_dirty = 1;
    } else {
        s_screen = UI_SCREEN_HOME;
    }
}

/* 1 when the highlighted row was a folder and has been opened. */
static int music_row_open(void)
{
    MvRow *r;

    music_view_build();
    if (s_mv_row < 0 || s_mv_row >= s_mv_count)
        return 0;
    r = &s_mv_rows[s_mv_row];
    if (!r->is_category)
        return 0;

    /* Remember where this folder was in the list above it. */
    s_mv_row_mem[s_mv][s_mv_level] = s_mv_row;

    if (s_mv == MV_ARTISTS) {
        if (s_mv_level == 0) {
            snprintf(s_mv_key, TAG_LEN, "%s", r->name);
            s_mv_level = 1;
        } else if (s_mv_level == 1) {
            snprintf(s_mv_key2, TAG_LEN, "%s", r->name);
            s_mv_level = 2;
        } else {
            return 0;
        }
    } else if (s_mv == MV_ALBUMS && s_mv_level == 0) {
        snprintf(s_mv_key, TAG_LEN, "%s", r->name);
        snprintf(s_mv_key2, TAG_LEN, "%s", r->sub);
        s_mv_level = 1;
    } else if (s_mv == MV_GENRES && s_mv_level == 0) {
        snprintf(s_mv_key, TAG_LEN, "%s", r->name);
        s_mv_level = 1;
    } else {
        return 0;
    }
    s_mv_row   = 0;
    s_mv_dirty = 1;
    return 1;
}

static int music_row_for_track(int track)
{
    int i;

    music_view_build();
    for (i = 0; i < s_mv_count; i++)
        if (s_mv_rows[i].track == track)
            return i;
    return -1;
}

/* L1 / R1 walk the list that is on screen, not the whole drive: inside an
 * album the next track is the next song of that album. */
static void music_neighbour(int dir)
{
    int i;

    music_view_build();
    for (i = s_mv_row + dir; i >= 0 && i < s_mv_count; i += dir) {
        if (s_mv_rows[i].track >= 0) {
            s_mv_row   = i;
            s_selected = s_mv_rows[i].track;
            play_track(s_selected);
            return;
        }
    }
    snprintf(s_status, sizeof(s_status), "%s",
             dir > 0 ? "Last track in this list" : "First track in this list");
}

/* Position of a track in the view a section draws, and the way back: the
 * music list numbers only songs, the video list only videos, so a row index
 * and a track index are different numbers and must never be swapped. */
static int track_to_view(int track, int want_video)
{
    int i, n = 0;

    if (track < 0 || track >= s_track_count)
        return -1;
    for (i = 0; i < s_track_count; i++)
        if (s_tracks[i].is_video == want_video) {
            if (i == track)
                return n;
            n++;
        }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

/* L1 / R1 step through the library one file at a time, staying with the kind
 * of thing that is playing: a video moves to the next video, a song to the
 * next song. Stopping at the end of the list is said out loud rather than
 * wrapping, so a press that does nothing is never mistaken for a dead key. */
static void play_neighbour(int dir)
{
    int base = s_current >= 0 ? s_current : s_selected;
    int kind = (base >= 0 && base < s_track_count) ? s_tracks[base].is_video
                                                   : s_video_mode;
    int i;

    for (i = base + dir; i >= 0 && i < s_track_count; i += dir) {
        if (s_tracks[i].is_video == kind) {
            play_track(i);
            return;
        }
    }
    if (s_video_mode)
        video_toast_show(dir > 0 ? "NO NEXT FILE" : "NO PREVIOUS FILE");
    else
        snprintf(s_status, sizeof(s_status), "%s",
                 dir > 0 ? "Last file in this list" : "First file in this list");
}

static int controller_button_to_key(Uint8 b)
{
    switch (b) {
    case SDL_CONTROLLER_BUTTON_A:             return SDLK_x;
    case SDL_CONTROLLER_BUTTON_X:             return SDLK_s;
    case SDL_CONTROLLER_BUTTON_Y:             return SDLK_e;
    case SDL_CONTROLLER_BUTTON_B:             return SDLK_ESCAPE;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       return SDLK_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     return SDLK_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     return SDLK_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    return SDLK_RIGHT;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return SDLK_q;  /* L1 */
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return SDLK_w;  /* R1 */
    /* SHARE belongs to the system: on real hardware it opens the share menu
     * and captures screenshots and clips, so the player must never act on it. */
    case SDL_CONTROLLER_BUTTON_BACK:          return -1;
    case SDL_CONTROLLER_BUTTON_START:         return SDLK_t;  /* OPTIONS */
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     return SDLK_m;  /* L3 mute  */
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    return SDLK_r;  /* R3 EQ    */
    default:                                  return -1;
    }
}

static void stop_video(void)
{
    audio_pace_reset();
    video_close();
    sub_close();
    refresh_sub_label();
    s_toast[0]       = '\0';
    s_video_mode       = 0;
    s_video_paused     = 0;
    s_video_settings   = 0;
    s_video_fullscreen = 0;
    s_playing          = 0;
    s_duration         = 0;
    s_format[0]        = '\0';
    snprintf(s_status, sizeof(s_status), "Stopped");
}

/* One step of the volume, in 1 dB increments, with the mute released: turning
 * the volume up while muted and hearing nothing reads as a broken key. */
static void volume_step(int dir)
{
    s_vol.muted = 0;
    vol_add_db(&s_vol, dir > 0 ? 1.0f : -1.0f);
    s_vid_dirty = 1;
}

/* Moves the video list one row. The highlight alone never opens a file: the
 * preview starts only when the row is picked with Cross, so walking down a
 * list no longer decodes every video it passes. */
static void video_select_step(int dir)
{
    int i;

    for (i = s_selected + dir; i >= 0 && i < s_track_count; i += dir) {
        if (s_tracks[i].is_video) {
            s_selected   = i;
            s_sel_video  = i;
            s_vid_dirty  = 1;
            return;
        }
    }
    video_toast_show(dir > 0 ? "END OF LIST" : "START OF LIST");
}

/* ---------------------------------------------------------- video section - */

/* OPTIONS is the options button while watching, TRIANGLE is the full-screen
 * button for the preview, and up/down on the d-pad is the volume once the
 * picture fills the screen. */
static void video_keys(int key)
{
    video_show_hud();
    s_vid_dirty = 1;

    if (s_video_settings) {
        switch (key) {
        case SDLK_UP:
            s_settings_row = (s_settings_row + UI_VIDEO_ROWS - 1) % UI_VIDEO_ROWS;
            break;
        case SDLK_DOWN:
            s_settings_row = (s_settings_row + 1) % UI_VIDEO_ROWS;
            break;
        case SDLK_LEFT:  video_setting_step(-1); break;
        case SDLK_RIGHT: video_setting_step(+1); break;
        case SDLK_e:
        case SDLK_t:
        case SDLK_ESCAPE:
            s_video_settings = 0;
            break;
        default:
            break;
        }
        return;
    }

    switch (key) {
    case SDLK_x:
        /* Windowed: Cross plays the highlighted file, always - the row you
         * are looking at is the row that opens, even while another video is
         * loaded. Pause/resume only exists in the full-screen player, where
         * Cross has nothing else to do. */
        if (!s_video_fullscreen) {
            if (s_selected >= 0 && s_selected < s_track_count &&
                s_tracks[s_selected].is_video)
                play_track(s_selected);
            break;
        }
        if (!s_video_mode)
            break;
        s_video_paused = !s_video_paused;
        video_set_paused(s_video_paused);
        s_playing = s_video_paused ? 0 : 1;
        audio_pace_reset();
        break;

    case SDLK_UP:
    case SDLK_DOWN:
        if (s_video_fullscreen && s_video_mode)
            volume_step((key == SDLK_UP) ? +1 : -1);
        else
            video_select_step((key == SDLK_UP) ? -1 : +1);
        break;

    case SDLK_LEFT:
        if (s_video_mode)
            video_seek(video_position_sec() - 10.0);
        break;
    case SDLK_RIGHT:
        if (s_video_mode)
            video_seek(video_position_sec() + 10.0);
        break;

    case SDLK_e:                        /* TRIANGLE: full screen / window */
        if (s_video_mode) {
            s_video_fullscreen = !s_video_fullscreen;
            s_vid_dirty        = 1;
        }
        break;

    case SDLK_t:                        /* OPTIONS: picture settings */
        s_video_settings = 1;
        s_settings_row   = 0;
        refresh_sub_label();
        break;

    case SDLK_s:                        /* SQUARE: subtitles */
        if (s_video_mode)
            toggle_subtitles();
        break;

    case SDLK_ESCAPE:                   /* CIRCLE: back one level */
        if (s_video_fullscreen) {
            s_video_fullscreen = 0;
        } else {
            stop_video();
            s_screen = UI_SCREEN_HOME;
        }
        break;

    case SDLK_q:
        if (s_video_fullscreen) play_neighbour(-1);
        else                    video_select_step(-1);  /* windowed: move */
        break;
    case SDLK_w:
        if (s_video_fullscreen) play_neighbour(+1);
        else                    video_select_step(+1);
        break;
    default: break;
    }
}

/* ---------------------------------------------------------- music section - */

/* L2 and R2 have nothing to do with music any more: they jumped the decoder
 * and left the sound behind. Seeking moved to the d-pad, and only where it
 * cannot collide with anything else - the full-screen visualization, where
 * left and right are otherwise unused. The library view keeps left and right
 * for the volume, exactly as before. */

static uint64_t s_seek_ms;

/* Ten seconds per press, the step the video player uses. The position comes
 * from the frames the decoder has actually produced, not from the clock, so a
 * seek that the decoder refuses is reported instead of faked. */
static void music_seek(double delta)
{
    double   pos, want;
    uint64_t now = scan_now_ms();

    if (!s_playing || !s_src_open || s_current < 0)
        return;
    if (now - s_seek_ms < 250)          /* one press does not seek twice */
        return;
    s_seek_ms = now;

    pos  = (s_src.hz > 0) ? (double)s_src.frames_decoded / (double)s_src.hz : 0.0;
    want = pos + delta;
    if (want < 0.0)
        want = 0.0;
    if (s_duration > 1.0 && want > s_duration - 1.0)
        want = s_duration - 1.0;

    if (mp3_seek_seconds(&s_src, want) != 0) {
        snprintf(s_status, sizeof(s_status), "Seek not available for this file");
        plog("music: seek %.0fs refused", want);
        return;
    }
    play_clock_arm(want);
    snprintf(s_status, sizeof(s_status), "Seek  %d:%02d  /  %d:%02d",
             (int)want / 60, (int)want % 60,
             (int)s_duration / 60, (int)s_duration % 60);
    plog("music: seek %.0fs (full screen), now %.1fs", delta, want);
}

/* The rows the library panel shows at once (track_list draws seven), so one
 * page jump lands exactly where the next screen of rows begins. */
#define MUSIC_PAGE_ROWS 7

/* Moves the library highlight by delta rows, clamped to the list, and follows
 * the row's track so Cross still plays whatever is highlighted. */
static void music_row_step(int delta)
{
    music_view_build();
    if (s_mv_count <= 0)
        return;
    s_mv_row += delta;
    if (s_mv_row < 0)
        s_mv_row = 0;
    if (s_mv_row >= s_mv_count)
        s_mv_row = s_mv_count - 1;
    if (s_mv_rows[s_mv_row].track >= 0) {
        s_selected  = s_mv_rows[s_mv_row].track;
        s_sel_music = s_selected;
    }
}

static void music_keys(int key)
{
    switch (key) {
    case SDLK_UP:
    case SDLK_DOWN:
        /* Full screen there is no list to walk, so up and down are the volume,
         * in the same 1 dB steps as left and right are in the windowed view,
         * with a short readout because no volume bar is drawn here. */
        if (s_vis_full) {
            char msg[48];

            volume_step(key == SDLK_UP ? +1 : -1);
            snprintf(msg, sizeof(msg), "VOLUME %+.0f dB",
                     (double)vol_current_db(&s_vol));
            toast_show_for(msg, 1200);
        } else {
            music_row_step(key == SDLK_UP ? -1 : 1);
        }
        break;
    case SDLK_PAGEUP:
    case SDLK_PAGEDOWN:
        /* L2 / R2: a screen at a time, but only while the list is on show -
         * in the full-screen visualization there is no list to page. */
        if (!s_vis_full)
            music_row_step(key == SDLK_PAGEUP ? -MUSIC_PAGE_ROWS : MUSIC_PAGE_ROWS);
        break;
    case SDLK_x:
        /* Cross opens an artist, album or genre row; on a song it starts it,
         * and on the song already playing it pauses and resumes. */
        if (music_row_open())
            break;
        if (s_mv_row >= 0 && s_mv_row < s_mv_count &&
            s_mv_rows[s_mv_row].track >= 0) {
            int tr = s_mv_rows[s_mv_row].track;

            s_selected  = tr;
            s_sel_music = tr;
            if (s_playing && s_current == tr) {
                s_playing = 0;
            } else if (s_current == tr && s_src_open && !s_track_finished) {
                s_playing = 1;                   /* resume */
            } else {
                play_track(tr);
            }
        }
        break;
    case SDLK_s:                                 /* SQUARE: next list */
        music_view_reset((s_mv + 1) % 4);
        snprintf(s_status, sizeof(s_status), "Library: %s", k_mv_name[s_mv]);
        plog("library: view %s", k_mv_name[s_mv]);
        break;
    case SDLK_b:                                 /* keyboard only: stop */
        s_playing   = 0;
        s_current   = -1;
        s_format[0] = '\0';
        s_duration  = 0;
        play_clock_arm(0.0);
        snprintf(s_status, sizeof(s_status), "Stopped");
        break;
    case SDLK_e:                                 /* TRIANGLE: next visual */
        s_vis_style = (s_vis_style + 1) % VIS_STYLE_COUNT;
        settings_save();
        snprintf(s_status, sizeof(s_status), "Visualization: %s",
                 vis_style_name(s_vis_style));
        break;
    case SDLK_t: browser_open(); break;          /* OPTIONS: drives */
    case SDLK_q: music_neighbour(-1); break;
    case SDLK_w: music_neighbour(+1); break;
    /* Left and right seek only while the visualization fills the screen, so
     * walking the library never jumps a song by accident. */
    case SDLK_LEFT:
        if (s_vis_full)
            music_seek(-10.0);
        else
            volume_step(-1);
        break;
    case SDLK_RIGHT:
        if (s_vis_full)
            music_seek(+10.0);
        else
            volume_step(+1);
        break;
    case SDLK_ESCAPE:
        if (s_vis_full)
            s_vis_full = 0;                     /* back to the library */
        else
            music_view_up();                    /* out of a folder, then home */
        break;
    default: break;
    }
}

/* -------------------------------------------------------- picture section - */

static void picture_grade(int save)
{
    picture_apply(s_pic_brightness, s_pic_saturation, s_pic_sharpness,
                  s_pic_rotation, s_pic_rb_swap);
    if (save)
        settings_save();
}

static void picture_select(int index)
{
    if (index < 0 || index >= s_pic_count)
        return;
    s_pic_sel = index;
    if (picture_open(s_pics[index].path) != 0) {
        s_pic_loaded = 0;
        snprintf(s_status, sizeof(s_status), "%s", picture_last_error());
        snprintf(s_pic_note, sizeof(s_pic_note), "%s", picture_last_error());
        if (s_pic_tex) {
            SDL_DestroyTexture(s_pic_tex);
            s_pic_tex = NULL;
            s_pic_tex_w = s_pic_tex_h = 0;
        }
        plog("picture: %s failed (%s)", s_pics[index].name,
             picture_last_error());
        return;
    }

    s_pic_loaded = 1;
    s_pic_zoom   = 100;                 /* a new photo starts unzoomed */
    s_pic_pan_x  = 0;
    s_pic_pan_y  = 0;
    s_pic_serial = -1;                  /* force the texture upload */
    snprintf(s_pic_note, sizeof(s_pic_note), "%dx%d", picture_source_w(),
             picture_source_h());
    snprintf(s_status, sizeof(s_status), "%s   %s", s_pics[index].name,
             s_pic_note);
    picture_grade(0);
    plog("picture: %s %s", s_pics[index].name, s_pic_note);
}

static void picture_step(int dir)
{
    int i = s_pic_sel + dir;

    if (i < 0 || i >= s_pic_count) {
        snprintf(s_status, sizeof(s_status), "%s",
                 dir > 0 ? "Last picture" : "First picture");
        return;
    }
    picture_select(i);
}

static void picture_zoom_step(int dir)
{
    s_pic_zoom += dir;
    if (s_pic_zoom > 400) s_pic_zoom = 400;
    if (s_pic_zoom < 100) s_pic_zoom = 100;
    if (s_pic_zoom == 100) {
        s_pic_pan_x = 0;
        s_pic_pan_y = 0;
    }
}

static void picture_setting_step(int dir)
{
    int grade = 0;

    switch (s_pic_row) {
    case UI_PIC_ROW_FIT:
        s_pic_fit = (s_pic_fit + dir + UI_ASPECT_COUNT) % UI_ASPECT_COUNT;
        break;
    case UI_PIC_ROW_BRIGHTNESS:
        s_pic_brightness += dir;
        if (s_pic_brightness >  100) s_pic_brightness =  100;
        if (s_pic_brightness < -100) s_pic_brightness = -100;
        grade = 1;
        break;
    case UI_PIC_ROW_SATURATION:
        s_pic_saturation += dir;
        if (s_pic_saturation > 250) s_pic_saturation = 250;
        if (s_pic_saturation <   0) s_pic_saturation =   0;
        grade = 1;
        break;
    case UI_PIC_ROW_SHARPNESS:
        s_pic_sharpness += dir;
        if (s_pic_sharpness > 100) s_pic_sharpness = 100;
        if (s_pic_sharpness <   0) s_pic_sharpness =   0;
        grade = 1;
        break;
    case UI_PIC_ROW_ROTATION:
        s_pic_rotation = (s_pic_rotation + dir + 4) & 3;
        grade = 1;
        break;
    case UI_PIC_ROW_ZOOM:
        picture_zoom_step(dir * 5);
        break;
    default:
        /* COLOR ORDER: the one swap that fixes a file whose reds and blues
         * arrive the other way round. The next photo keeps it. */
        s_pic_rb_swap = !s_pic_rb_swap;
        grade = 1;
        break;
    }

    if (grade) {
        picture_grade(1);
        plog("picture: fit=%d bright=%d color=%d%% sharp=%d%% rot=%d zoom=%d%% order=%s",
             s_pic_fit, s_pic_brightness, s_pic_saturation, s_pic_sharpness,
             s_pic_rotation * 90, s_pic_zoom,
             s_pic_rb_swap ? "swap" : "normal");
    } else {
        settings_save();
    }
}

/* R3 (or Cross) is the zoom key here: with the zoom on, the right stick pans
 * in all eight directions and up/down on the d-pad changes the magnification.
 * SQUARE rotates the photo, OPTIONS opens the full picture settings. */
static void picture_keys(int key)
{
    if (s_pic_settings) {
        switch (key) {
        case SDLK_UP:
            s_pic_row = (s_pic_row + UI_PIC_ROWS - 1) % UI_PIC_ROWS;
            break;
        case SDLK_DOWN:
            s_pic_row = (s_pic_row + 1) % UI_PIC_ROWS;
            break;
        case SDLK_LEFT:  picture_setting_step(-1); break;
        case SDLK_RIGHT: picture_setting_step(+1); break;
        case SDLK_e:
        case SDLK_t:
        case SDLK_ESCAPE:
            s_pic_settings = 0;
            break;
        default:
            break;
        }
        return;
    }

    switch (key) {
    case SDLK_UP:
        if (s_pic_zoom_mode) picture_zoom_step(-5);
        else                 picture_step(-1);
        break;
    case SDLK_DOWN:
        if (s_pic_zoom_mode) picture_zoom_step(+5);
        else                 picture_step(+1);
        break;
    case SDLK_LEFT:
        if (s_pic_zoom_mode) picture_zoom_step(-5);
        break;
    case SDLK_RIGHT:
        if (s_pic_zoom_mode) picture_zoom_step(+5);
        break;
    case SDLK_x:
    case SDLK_r:
        s_pic_zoom_mode = !s_pic_zoom_mode;
        if (!s_pic_zoom_mode) {
            s_pic_zoom  = 100;
            s_pic_pan_x = 0;
            s_pic_pan_y = 0;
        }
        break;
    case SDLK_s:
        s_pic_rotation = (s_pic_rotation + 1) & 3;
        picture_grade(1);
        break;
    case SDLK_e:                        /* TRIANGLE: full-screen picture */
        s_pic_fullscreen = !s_pic_fullscreen;
        break;
    case SDLK_t:
        s_pic_settings = 1;
        s_pic_row      = 0;
        break;
    case SDLK_q: picture_step(-1); break;
    case SDLK_w: picture_step(+1); break;
    case SDLK_ESCAPE:
        if (s_pic_fullscreen)
            s_pic_fullscreen = 0;       /* back leaves full screen first */
        else
            s_screen = UI_SCREEN_HOME;
        break;
    default: break;
    }
}

/* ---------------------------------------------------------------- sections */

/* Entering a section sets up exactly one engine: songs stop when the video
 * section starts, the video engine stops when music or pictures start, and the
 * picture viewer decodes the highlighted photo. */
static void enter_section(int screen)
{
    int i;

    s_show_eq        = 0;
    s_eq_preset_open = 0;
    s_vis_full       = 0;      /* a section always opens in its normal view */

    switch (screen) {
    case UI_SCREEN_MUSIC:
        if (s_video_mode)
            stop_video();
        s_screen = UI_SCREEN_MUSIC;
        s_selected = s_sel_music;
        if (s_selected < 0 || s_selected >= s_track_count ||
            s_tracks[s_selected].is_video) {
            s_selected = -1;
            for (i = 0; i < s_track_count; i++)
                if (!s_tracks[i].is_video) { s_selected = i; break; }
            s_sel_music = s_selected;
        }
        /* the highlight follows the playing song into whichever list is up */
        {
            int row = music_row_for_track(s_selected);

            if (row >= 0)
                s_mv_row = row;
            else if (s_mv_row >= s_mv_count)
                s_mv_row = s_mv_count > 0 ? s_mv_count - 1 : 0;
        }
        break;

    case UI_SCREEN_VIDEOS:
        if (s_src_open) {
            s_playing  = 0;
            mp3_close(&s_src);
            s_src_open = 0;
        }
        s_screen     = UI_SCREEN_VIDEOS;
        s_video_settings = 0;
        s_selected   = s_sel_video;
        if (s_selected < 0 || s_selected >= s_track_count ||
            !s_tracks[s_selected].is_video) {
            s_selected = -1;
            for (i = 0; i < s_track_count; i++)
                if (s_tracks[i].is_video) { s_selected = i; break; }
            s_sel_video = s_selected;
        }
        /* Nothing opens on entry: the section shows the list and a black
         * preview until a file is picked with Cross. */
        break;

    case UI_SCREEN_PICTURES:
        if (s_video_mode)
            stop_video();
        s_screen         = UI_SCREEN_PICTURES;
        s_pic_settings   = 0;
        s_pic_fullscreen = 0;
        if (s_pic_count > 0)
            picture_select(s_pic_sel < 0 ? 0 : s_pic_sel);
        break;

    default:
        browser_open();
        break;
    }
}

static void handle_key(int key)
{
    /* In the music section L3 is the full-screen visualization; everywhere
     * else it is still the mute key. */
    if (key == SDLK_m && s_screen == UI_SCREEN_MUSIC && !s_show_eq) {
        s_vis_full = !s_vis_full;
        settings_save();
        snprintf(s_status, sizeof(s_status), "Visualization: %s",
                 vis_style_name(s_vis_style));
        plog("music: full-screen visualization %s",
             s_vis_full ? "on" : "off");
        return;
    }

    /* L3 mutes, R3 opens the equalizer - in every section, including over a
     * playing video, which is what the sticks are for. */
    if (key == SDLK_m) {
        /* In the file browser L3 is the rescan key: the list is the one place
         * where re-reading the drives matters. This used to sit in the
         * browser's own key switch, where the mute branch below returned first
         * and made it unreachable. */
        if (s_screen == UI_SCREEN_BROWSER && !s_show_eq) {
            browser_refresh();
            return;
        }
        s_vol.muted = !s_vol.muted;
        vol_set_db(&s_vol, s_vol.volume_db);       /* recomputes the gain */
        if (s_video_mode)
            video_toast_show(s_vol.muted ? "MUTED" : "SOUND ON");
        else
            snprintf(s_status, sizeof(s_status), "%s",
                     s_vol.muted ? "Muted" : "Sound on");
        s_vid_dirty = 1;
        return;
    }
    if (key == SDLK_r) {
        /* R3 is the equalizer everywhere except the picture viewer, where the
         * same button is the zoom key the viewer needs. */
        if (s_screen == UI_SCREEN_PICTURES)
            picture_keys(SDLK_r);
        else {
            /* The settings screen is also the place config.ini is picked up
             * again: edit the file over FTP, press R3, and the EQ, the skin and
             * the visualizer's numbers are what the file now says.
             *
             * Volume and mute are the exception. They live in the same file,
             * but the file is only written when something saves, so a volume
             * turned down with the pad is not in it yet - and re-applying it
             * snapped the sound back to the saved level every time the
             * equalizer opened. Keep the live pair across the reload. */
            float live_db    = vol_current_db(&s_vol);
            int   live_muted = s_vol.muted;

            if (cfg_load() == 0) {
                settings_apply();
                log_msg(LOG_INFO, "config", "reloaded before opening the equaliser");
            }
            cfg.player.volume_db = (double)live_db;
            cfg.player.muted     = live_muted ? 1 : 0;
            s_vol.muted          = live_muted;
            vol_set_db(&s_vol, live_db);
            s_show_eq = 1;
        }
        return;
    }

    /* The equalizer swallows input wherever it is open, so the same keys do
     * not also seek, skip, or change the picture behind it. */
    if (s_show_eq) {
        /* The preset list owns the pad while it is down: up/down choose, Cross
         * applies, anything that closes a panel closes it unchanged. */
        if (s_eq_preset_open) {
            if (key == SDLK_UP)
                s_eq_preset_sel = (s_eq_preset_sel + EQ_PRESET_COUNT - 1) % EQ_PRESET_COUNT;
            else if (key == SDLK_DOWN)
                s_eq_preset_sel = (s_eq_preset_sel + 1) % EQ_PRESET_COUNT;
            else if (key == SDLK_x) {
                s_eq_preset = s_eq_preset_sel;
                eq_preset_apply(&s_eq, s_eq_preset);
                eq_reset(&s_eq);
                settings_save();
                s_eq_preset_open = 0;
                snprintf(s_status, sizeof(s_status), "Equalizer preset: %s",
                         eq_preset_name(s_eq_preset));
                plog("eq: preset %s applied", eq_preset_name(s_eq_preset));
            } else if (key == SDLK_ESCAPE || key == SDLK_e || key == SDLK_t ||
                       key == SDLK_s) {
                s_eq_preset_open = 0;
            }
            return;
        }

        if (key == SDLK_s) {           /* SQUARE: the preset drop-down */
            s_eq_preset_open = 1;
            s_eq_preset_sel  = s_eq_preset;
            return;
        }
        if (key == SDLK_e || key == SDLK_t || key == SDLK_ESCAPE) {
            s_show_eq = 0;
            settings_save();                  /* kept for the next boot */
        } else if (key == SDLK_UP || key == SDLK_DOWN) {
            float d = (key == SDLK_UP) ? 1.0f : -1.0f;

            if (s_eq_band == EQ_BANDS)
                eq_set_preamp_db(&s_eq, s_eq.preamp_db + d);
            else
                eq_set_band_db(&s_eq, s_eq_band, s_eq.gain_db[s_eq_band] + d);
            /* moving a band by hand makes the curve the user's own */
            s_eq_preset = EQ_PRESET_CUSTOM;
        } else if (key == SDLK_LEFT && s_eq_band > 0) {
            s_eq_band--;
        } else if (key == SDLK_RIGHT && s_eq_band < EQ_BANDS) {
            s_eq_band++;
        }
        return;
    }

    switch (s_screen) {
    case UI_SCREEN_HOME:
        switch (key) {
        case SDLK_LEFT:
            if (s_home_sel & 1)
                s_home_sel--;
            break;
        case SDLK_RIGHT:
            if (!(s_home_sel & 1) && s_home_sel < 3)
                s_home_sel++;
            break;
        case SDLK_UP:
        case SDLK_DOWN:
            s_home_sel ^= 2;
            break;
        case SDLK_x:
            if (s_home_sel == 0)      enter_section(UI_SCREEN_MUSIC);
            else if (s_home_sel == 1) enter_section(UI_SCREEN_VIDEOS);
            else if (s_home_sel == 2) enter_section(UI_SCREEN_PICTURES);
            else                      browser_open();
            break;
        case SDLK_t:
        case SDLK_b:
            browser_open();
            break;
        case SDLK_s:                     /* SQUARE: next theme / skin */
            s_theme = (s_theme + 1) % ui_theme_count();
            settings_save();
            snprintf(s_status, sizeof(s_status), "Theme: %s",
                     ui_theme_name(s_theme));
            break;
        default:
            break;
        }
        return;

    case UI_SCREEN_VIDEOS:
        video_keys(key);
        return;

    case UI_SCREEN_PICTURES:
        picture_keys(key);
        return;

    case UI_SCREEN_BROWSER:
        /* The delete prompt owns the pad while it is up: Cross says yes,
         * Circle says no, and nothing else can sneak in behind it. */
        if (s_browser_confirm) {
            if (key == SDLK_x) {
                browser_delete_confirmed();
            } else if (key == SDLK_ESCAPE) {
                s_browser_confirm = 0;
                snprintf(s_browser_note, sizeof(s_browser_note), "NOT DELETED");
            }
            return;
        }

        switch (key) {
        case SDLK_UP:     browser_move(-1); break;
        case SDLK_DOWN:   browser_move(1); break;
        /* L1 is CUT. It used to page the list, and cut sat on a keyboard key
         * that no pad button sends since SHARE was unbound. */
        case SDLK_q:      browser_clip(1); break;        /* L1      cut   */
        case SDLK_w:      browser_move(BROWSER_PAGE); break;
        case SDLK_x:      browser_activate(); break;
        case SDLK_s:      browser_ask_delete(); break;   /* SQUARE  delete */
        case SDLK_e:      browser_clip(0); break;        /* TRIANGLE copy  */
        case SDLK_b:      browser_clip(1); break;        /* keyboard cut  */
        case SDLK_t:      browser_paste(); break;        /* OPTIONS  paste */
        case SDLK_ESCAPE:
            if (s_browser_level == 1)
                browser_go_up();
            else
                s_screen = UI_SCREEN_HOME;
            break;
        default:          break;
        }
        return;

    default:
        music_keys(key);
        return;
    }
}
/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

static void build_model(UiModel *m)
{
    int i, audio_n = 0, video_n = 0;

    memset(m, 0, sizeof(*m));
    m->app_title = s_app_title;
    m->screen    = s_screen;

    for (i = 0; i < s_track_count; i++) {
        if (s_tracks[i].is_video) video_n++;
        else audio_n++;
    }

    /* One view per frame, in the units the current screen draws: songs, then
     * videos, then photos. The selection indices follow, so "row 4" means the
     * same row in the list and in the list of files it points at. */
    if (s_screen == UI_SCREEN_PICTURES) {
        for (i = 0; i < s_pic_count && i < MAX_TRACKS; i++) {
            s_view[i].name        = s_pics[i].name;
            s_view[i].sub         = "PHOTO";
            s_view[i].is_video    = 0;
            s_view[i].is_category = 0;
        }
        s_view_count = s_pic_count;
        m->selected  = s_pic_sel;
        m->current   = s_pic_sel;
    } else if (s_screen == UI_SCREEN_MUSIC) {
        /* The music list is the library's own: songs, or the artists, albums
         * and genres the collection is made of. */
        music_view_build();
        for (i = 0; i < s_mv_count && i < MAX_TRACKS; i++) {
            s_view[i].name        = s_mv_rows[i].name;
            s_view[i].sub         = s_mv_rows[i].sub;
            s_view[i].is_video    = 0;
            s_view[i].is_category = s_mv_rows[i].is_category;
        }
        s_view_count = s_mv_count;
        m->selected  = s_mv_row;
        if (m->selected < 0)
            m->selected = s_view_count > 0 ? 0 : -1;
        m->current   = music_row_for_track(s_current);
    } else {
        int want_video = (s_screen == UI_SCREEN_VIDEOS);
        int n = 0;

        for (i = 0; i < s_track_count; i++) {
            if (s_tracks[i].is_video == want_video && n < MAX_TRACKS) {
                s_view[n].name        = s_tracks[i].name;
                s_view[n].sub         = want_video ? "VIDEO" : "MP3";
                s_view[n].is_video    = want_video;
                s_view[n].is_category = 0;
                n++;
            }
        }
        s_view_count = n;
        m->selected  = track_to_view(s_selected, want_video);
        if (m->selected < 0)
            m->selected = s_view_count > 0 ? 0 : -1;
        m->current   = track_to_view(s_current, want_video);
    }

    m->music_view      = s_mv;
    m->music_view_name = k_mv_name[s_mv];
    m->music_crumb     = (s_mv_level <= 0) ? ""
                       : (s_mv_level >= 2 ? s_mv_key2 : s_mv_key);
    m->music_level     = s_mv_level;
    m->tracks      = s_view;
    m->track_count = s_view_count;

    m->playing      = s_playing;
    m->have_track   = (s_current >= 0 && s_format[0] != '\0');
    m->format_note  = s_format;
    m->status       = s_status;
    m->duration_sec = s_duration;
    /* Wall clock, re-anchored only when the position really moved (a seek or
     * a new file). Never backwards, so the bar cannot bounce. */
    if (s_src_open && s_src.hz > 0) {
        double raw = (double)s_src.frames_decoded / (double)s_src.hz;

        if (raw < s_clock_pos - 0.40 || raw > s_clock_pos + 2.50)
            play_clock_arm(raw);
        if (s_playing) {
            m->elapsed_sec = play_clock_advance();
        } else {
            play_clock_arm(raw);       /* paused: freeze at what was decoded */
            m->elapsed_sec = raw;
        }
    } else {
        play_clock_arm(0.0);
        m->elapsed_sec = 0.0;
    }
    /* The length is only known once a header has been parsed, so it is asked
     * for again here rather than once when playback starts. Small drift is
     * ignored: moving the denominator is what made the bar wobble. */
    if (!s_video_mode && s_src_open) {
        double d = mp3_duration_est(&s_src);

        if (d > 0 && (s_duration <= 0.5 ||
                      d > s_duration * 1.05 || d < s_duration * 0.95))
            s_duration = d;
        m->duration_sec = s_duration;
    }
    if (m->elapsed_sec > m->duration_sec && m->duration_sec > 0)
        m->elapsed_sec = m->duration_sec;

    m->volume_db    = (int)s_vol.volume_db;
    m->volume_frac  = (s_vol.volume_db + 60.0f) / 72.0f;
    m->volume_muted = s_vol.muted;

    m->home_selected = s_home_sel;
    m->music_count   = audio_n;
    m->video_count   = video_n;
    m->picture_count = s_pic_count;

    m->browser_title  = s_browser_path;
    m->browser_note   = s_browser_note;
    m->entries        = s_uientries;
    m->entry_count    = s_entry_count;
    m->entry_selected = s_entry_sel;

    m->eq_open      = s_show_eq;
    m->eq_band      = s_eq_band;
    m->eq_preset    = s_eq_preset;
    m->eq_preset_open = s_eq_preset_open;
    m->eq_preset_sel  = s_eq_preset_sel;
    m->eq_preamp_db = s_eq.preamp_db;
    for (i = 0; i < EQ_BANDS; i++)
        m->eq_gain_db[i] = s_eq.gain_db[i];

    m->vis_style      = s_vis_style;
    m->vis_fullscreen = s_vis_full;
    vis_fill(m->vis_bands, VIS_BANDS, m->vis_wave, VIS_WAVE, &m->vis_level);

    m->video_aspect     = s_video_aspect;
    m->video_brightness = s_video_brightness;
    m->video_saturation = s_video_saturation;
    m->video_sharpness  = s_video_sharpness;
    m->video_fullscreen = s_video_fullscreen;
    m->sub_size         = s_sub_size;
    m->sub_label        = s_sub_label;

    m->pic_count         = s_pic_count;
    m->pic_selected      = s_pic_sel;
    m->pic_name          = (s_pic_sel >= 0 && s_pic_sel < s_pic_count)
                         ? s_pics[s_pic_sel].name : "";
    m->pic_note          = s_pic_note;
    m->pic_loaded        = s_pic_loaded;
    m->pic_fit           = s_pic_fit;
    m->pic_brightness    = s_pic_brightness;
    m->pic_saturation    = s_pic_saturation;
    m->pic_sharpness     = s_pic_sharpness;
    m->pic_rotation      = s_pic_rotation;
    m->pic_zoom          = s_pic_zoom;
    m->pic_pan_x         = s_pic_pan_x;
    m->pic_pan_y         = s_pic_pan_y;
    m->pic_zoom_mode     = s_pic_zoom_mode;
    m->pic_settings_open = s_pic_settings;
    m->pic_settings_row  = s_pic_row;
    m->pic_fullscreen    = s_pic_fullscreen;
    m->pic_rb_swap       = s_pic_rb_swap;

    m->theme_name    = ui_theme_name(s_theme);
    m->usb_present   = s_usb_present;
    m->usb_count     = s_usb_count;
    m->confirm_delete = s_browser_confirm;
    m->confirm_name  = (s_browser_confirm && s_entry_count > 0 &&
                        s_entry_sel < s_entry_count)
                     ? s_entries[s_entry_sel].name : "";
    m->clip_note     = (s_clip_mode == 1) ? "COPIED"
                     : (s_clip_mode == 2) ? "CUT" : "";
    m->clip_name     = s_clip_name;

    if (s_video_mode) {
        /* the decoder's clock, not the MP3 frame counter */
        m->elapsed_sec  = video_position_sec();
        m->duration_sec = video_duration_sec();
        m->have_track   = 1;
        m->format_note  = video_format_note();
        m->video_settings_open = s_video_settings;
        m->video_settings_row  = s_settings_row;
        if (sub_enabled())
            m->sub_text = sub_text_at(m->elapsed_sec);

        /* The bar is an overlay, not part of the picture: it fades out a few
         * seconds into playback and comes back on the next button press, and
         * it stays while a panel is open over it. */
        m->video_hud = (s_video_paused || s_video_settings || s_show_eq ||
                        (Sint32)(s_video_hud_until - SDL_GetTicks()) > 0) ? 1 : 0;

    }

    /* The toast is not a video thing any more: the full-screen visualizer
     * shows the volume in the same pill, so every screen fills it here. */
    if (s_toast[0]) {
        Uint32 age = SDL_GetTicks() - s_toast_start;

        if (age >= s_toast_dur) {
            s_toast[0] = '\0';
        } else {
            m->video_toast = s_toast;
            m->video_toast_alpha = (age <= 800)
                ? 255
                : (int)(255u * (s_toast_dur - age) / (s_toast_dur - 800));
        }
    }

    if (s_screen == UI_SCREEN_PICTURES)
        m->have_track = 0;
}
/* ------------------------------------------------------------------ */
/* Measurement                                                         */
/* ------------------------------------------------------------------ */
/* One struct for every stage of the loop, so the numbers in perf.log line
 * up with the sections of the code that produced them, and a frame that
 * takes 80 ms says which of the five pieces took the time. */
typedef struct {
    Uint64   freq;
    Uint64   win_start;      /* performance counter when this window opened */
    uint64_t cpu_start;      /* process CPU time (us) when it opened */
    Uint64   s_ev,    max_ev;
    Uint64   s_step,  max_step;
    Uint64   s_model, max_model;
    Uint64   s_up,    max_up;
    Uint64   s_draw,  max_draw;
    Uint64   s_flush, max_flush;
    Uint64   s_pres,  max_pres;
    Uint64   s_surf,  max_surf;
    Uint64   s_rest,  max_rest;
    Uint64   s_loop,  max_loop;
    int      n;
    int      tex_w, tex_h;
} Perf;

static Perf s_perf;

/* Milliseconds in a performance-counter delta. */
static double perf_ms(Uint64 t)
{
    return s_perf.freq ? (double)t * 1000.0 / (double)s_perf.freq : 0.0;
}

/* Time the section that just ran and start the next one. */
#define LAP_ADD(f) do {                                            \
        Uint64 n_ = SDL_GetPerformanceCounter();                    \
        Uint64 d_ = n_ - t_lap;                                     \
        s_perf.s_##f += d_;                                         \
        if (d_ > s_perf.max_##f) s_perf.max_##f = d_;               \
        t_lap = n_;                                                 \
    } while (0)

/* The phase split, about once a second, into the one log. The frame times
 * themselves (average, min, p95, p99, worst, over-budget count) come from the
 * stats module, which samples every frame; this only fills in what it cannot
 * see from outside - which part of the loop the milliseconds went to, what the
 * decoder is holding, and how loud the music is. */
static void perf_report(int video_mode)
{
    Uint64             now = SDL_GetPerformanceCounter();
    double             span = perf_ms(now - s_perf.win_start);
    uint64_t           cpu_now;
    int                blocks = 0;
    unsigned long long direct = 0;
    double             per;
    static Uint32      step_t0;
    static int         step_last = -1;

    if (span < 1000.0 || s_perf.n <= 0)
        return;

    cpu_now = sceKernelGetProcessTime();
    per     = 1.0 / (double)s_perf.n;
    video_mem_stats(&blocks, &direct);

    stats_note("ev",      perf_ms(s_perf.s_ev) * per);
    stats_note("sim",     perf_ms(s_perf.s_step) * per);
    stats_note("model",   perf_ms(s_perf.s_model) * per);
    stats_note("upload",  perf_ms(s_perf.s_up) * per);
    stats_note("paint",   perf_ms(s_perf.s_draw) * per);
    stats_note("flush",   perf_ms(s_perf.s_flush) * per);
    stats_note("present", perf_ms(s_perf.s_pres) * per);
    stats_note("surf",    perf_ms(s_perf.s_surf) * per);
    stats_note("rest",    perf_ms(s_perf.s_rest) * per);
    stats_note("worst_ev",     perf_ms(s_perf.max_ev));
    stats_note("worst_paint",  perf_ms(s_perf.max_draw));
    stats_note("worst_present", perf_ms(s_perf.max_pres));
    stats_note("cpu", (double)(cpu_now - s_perf.cpu_start) / (span * 10.0));

    stats_note("canvas_w", (double)s_view_w);
    stats_note("canvas_h", (double)s_view_h);
    stats_note("tex_w", (double)s_perf.tex_w);
    stats_note("tex_h", (double)s_perf.tex_h);
    stats_note("dec_mb", (double)direct / (1024.0 * 1024.0));
    stats_note("dec_blocks", (double)blocks);
    stats_note("rgba_kb", (double)video_rgba_bytes() / 1024.0);
    stats_note("mix_empty", (double)s_mix_empty);
    stats_note("mp3_skips", (double)s_mp3_skips);

    if (step_last != vis_art_step()) {
        step_last = vis_art_step();
        step_t0   = SDL_GetTicks();
    }
    stats_note("dwell_s", (double)(SDL_GetTicks() - step_t0) / 1000.0);

    /* What the visualizer is being fed, so a visual fault can be told apart
     * from an audio-mapping one without another log. */
    {
        float  bands[VIS_BANDS], lvl = 0.0f;
        double bass = 0.0, mid = 0.0, tre = 0.0;
        int    i;

        vis_fill(bands, VIS_BANDS, NULL, 0, &lvl);
        for (i = 0; i < 6; i++)
            bass += bands[i];
        for (i = 6; i < 18; i++)
            mid += bands[i];
        for (i = 18; i < VIS_BANDS; i++)
            tre += bands[i];
        stats_note("level",  (double)lvl * 100.0);
        stats_note("bass",   bass / 6.0 * 100.0);
        stats_note("mid",    mid / 12.0 * 100.0);
        stats_note("treble", tre / (double)(VIS_BANDS - 18) * 100.0);
    }

    /* The detector's own verdict, next to the levels it was read from, so a
     * tempo that looks wrong can be judged against the spectrum that produced
     * it in the same line. Nothing in the visualizers reads these yet. */
    {
        static float strength_peak;
        int   beat = 0;
        float strength = 0.0f, bpm = 0.0f;
        long  beats = 0;

        vis_beat(&beat, &strength, &bpm, &beats);
        if (strength > strength_peak)
            strength_peak = strength;
        stats_note("bpm", (double)bpm);
        stats_note("beats", (double)beats);
        /* the strongest moment of the last second, not one sample of it */
        stats_note("beat_str", (double)strength_peak * 100.0);
        stats_note("canvas_kb", (double)vis_art_mem_peak() / 1024.0);
        strength_peak = 0.0f;
    }

    stats_summary(video_mode ? "video" : "menu");

    s_perf.win_start = now;
    s_perf.cpu_start = cpu_now;
    s_perf.n         = 0;
    s_perf.s_ev = s_perf.s_step = s_perf.s_model = s_perf.s_up = 0;
    s_perf.s_draw = s_perf.s_flush = s_perf.s_pres = s_perf.s_surf = 0;
    s_perf.s_rest = s_perf.s_loop = 0;
    s_perf.max_ev = s_perf.max_step = s_perf.max_model = s_perf.max_up = 0;
    s_perf.max_draw = s_perf.max_flush = s_perf.max_pres = s_perf.max_surf = 0;
    s_perf.max_rest = s_perf.max_loop = 0;
}

#define PAD_UNSET (-1000000)

#define PAD_UNSET (-1000000)

/* --------------------------------------------------------- pad triggers -- */

/* L2 and R2 are analog triggers, and SDL's game-controller layer delivers them
 * as axes, never as buttons. The page keys were first mapped in the raw
 * joystick button switch, which only runs when the pad fails to open as a
 * controller - so on the console, where the pad opens as a game controller,
 * the triggers produced no key at all and the music list never paged. They are
 * read here instead, once a frame, with a threshold and an edge so one pull is
 * one page and a held trigger does not run away.
 *
 * Which path the pad took is logged at boot ("pad game controller opened" or
 * "joystick only"), and the boot block also logs the axis count and rest
 * values, so the trigger axes can be checked against the hardware. */
static int s_trig_l_was, s_trig_r_was;

/* ---- uptime for the log -------------------------------------------------- */

/* Every log line is stamped with this. The console's libc has no working
 * CLOCK_MONOTONIC - clock_gettime() fails there and left every line of the
 * v105 and v109 logs stamped [    0.000] - so the log is given the kernel's
 * own process clock instead: microseconds from launch, no initialisation
 * needed, and the same clock the frame timing is derived from. */
static uint64_t log_uptime_us(void)
{
    return sceKernelGetProcessTime();
}

/* ---- screenshots --------------------------------------------------------- */

/* Save the frame that was just presented as an uncompressed BMP, so a real
 * full-resolution frame can be copied off the console over FTP instead of
 * photographed from the TV. The renderer is software, so by the time this runs
 * the window surface already holds the finished frame and reading it back is a
 * copy; the BMP header is written here rather than pulling in a new library.
 *
 * Triggered by the touchpad click (button 13), the one button on the pad that
 * nothing else in the player uses, so pressing it cannot page, seek, mute or
 * change a setting. */
#define SHOT_DIR  "/data/ultimate-ps4-media-player/shots"
#define SHOT_MAX  20
#define SHOT_PATH 320

static int s_shot_want;

static void shot_request(void)
{
    s_shot_want = 1;
}

static void shot_put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void shot_put32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

/* Keep the folder at SHOT_MAX files: the oldest goes, by its exact name, so a
 * long session cannot fill the card. Nothing outside shots/ is touched. */
static void shot_make_room(void)
{
    DIR           *d;
    struct dirent *e;
    struct stat    st;
    char           path[SHOT_PATH], oldest[SHOT_PATH];
    time_t         old_t = 0;
    int            n = 0;

    d = opendir(SHOT_DIR);
    if (!d)
        return;
    oldest[0] = '\0';
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", SHOT_DIR, e->d_name);
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
            continue;
        n++;
        if (!oldest[0] || st.st_mtime < old_t) {
            old_t = st.st_mtime;
            snprintf(oldest, sizeof(oldest), "%s", path);
        }
    }
    closedir(d);
    if (n >= SHOT_MAX && oldest[0])
        unlink(oldest);
}

static void shot_save(SDL_Renderer *ren)
{
    SDL_Surface   *s;
    FILE          *f;
    unsigned char  hdr[54];
    unsigned char *line;
    const char    *nm;
    char           style[40], path[SHOT_PATH];
    unsigned       row, bmp;
    int            w = 0, h = 0, x, y, i, ok;

    if (!ren) {
        plog("shot: no renderer to read back");
        return;
    }
    if (SDL_GetRendererOutputSize(ren, &w, &h) != 0 || w <= 0 || h <= 0) {
        plog("shot: renderer has no size (%s)", SDL_GetError());
        return;
    }
    s = SDL_CreateRGBSurface(0, w, h, 32,
                             0x00FF0000u, 0x0000FF00u, 0x000000FFu, 0xFF000000u);
    if (!s) {
        plog("shot: surface failed (%s)", SDL_GetError());
        return;
    }
    if (SDL_RenderReadPixels(ren, NULL, s->format->format, s->pixels,
                             s->pitch) != 0) {
        plog("shot: read failed (%s)", SDL_GetError());
        SDL_FreeSurface(s);
        return;
    }

    /* version, style and quality step in the name, so three captures can be
     * told apart before any of them is opened */
    nm = vis_style_name(s_vis_style);
    for (i = 0; i < (int)sizeof(style) - 1 && nm && nm[i]; i++) {
        char ch = nm[i];

        style[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch - 'A' + 'a')
                 : ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))
                   ? ch : '-';
    }
    style[i] = '\0';

    mkdir(SHOT_DIR, 0777);
    shot_make_room();
    snprintf(path, sizeof(path), "%s/%s-%s-step%d-%04us.bmp", SHOT_DIR,
             BUILD_ID, style[0] ? style : "ui", vis_art_step(),
             (unsigned)(SDL_GetTicks() / 1000));

    /* 24-bit BMP, bottom-up, rows padded to four bytes */
    row = ((unsigned)w * 3u + 3u) & ~3u;
    bmp = 54u + row * (unsigned)h;
    hdr[0] = 'B';
    hdr[1] = 'M';
    shot_put32(hdr + 2, bmp);
    shot_put32(hdr + 6, 0);
    shot_put32(hdr + 10, 54);
    shot_put32(hdr + 14, 40);
    shot_put32(hdr + 18, (unsigned long)w);
    shot_put32(hdr + 22, (unsigned long)h);
    shot_put16(hdr + 26, 1);
    shot_put16(hdr + 28, 24);
    shot_put32(hdr + 30, 0);
    shot_put32(hdr + 34, row * (unsigned)h);
    shot_put32(hdr + 38, 2835);
    shot_put32(hdr + 42, 2835);
    shot_put32(hdr + 46, 0);
    shot_put32(hdr + 50, 0);

    line = (unsigned char *)malloc(row);
    f    = line ? fopen(path, "wb") : NULL;
    if (!f) {
        plog("shot: cannot write %s", path);
        free(line);
        SDL_FreeSurface(s);
        return;
    }
    fwrite(hdr, 1, sizeof(hdr), f);
    memset(line + (size_t)w * 3u, 0, row - (unsigned)w * 3u);
    /* One row at a time: the read-back pixels are turned into BGR in place and
     * written, so no second copy of the frame is needed. */
    for (y = h - 1; y >= 0; y--) {
        const uint32_t *src = (const uint32_t *)((const char *)s->pixels +
                                                 (size_t)y * (size_t)s->pitch);

        for (x = 0; x < w; x++) {
            uint32_t p = src[x];

            line[x * 3 + 0] = (unsigned char)(p & 0xFF);
            line[x * 3 + 1] = (unsigned char)((p >> 8) & 0xFF);
            line[x * 3 + 2] = (unsigned char)((p >> 16) & 0xFF);
        }
        if (fwrite(line, 1, row, f) != row)
            break;
    }
    ok = (y < 0 && ferror(f) == 0);
    fclose(f);
    free(line);
    SDL_FreeSurface(s);

    if (ok)
        plog("shot: saved %s (%dx%d, %u KB)", path, w, h, bmp / 1024u);
    else
        plog("shot: write failed on %s", path);
}

static void pad_triggers(SDL_GameController *gc, SDL_Joystick *js)
{
    const int ON  = 16384;      /* half travel: a click, not a graze */
    const int OFF = 8192;       /* releasing has to come back further */
    int l = 0, r = 0;
    int l_on, r_on;

    if (!gc && !js)
        return;

    if (gc) {
        l = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        r = SDL_GameControllerGetAxis(gc, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    } else if (SDL_JoystickNumAxes(js) >= 6) {
        /* Opened as a plain joystick the triggers come after the four stick
         * axes. A pad that reports only four axes has no analog trigger, and
         * its digital buttons 6 and 7 are already mapped to the page keys. */
        l = SDL_JoystickGetAxis(js, 4);
        r = SDL_JoystickGetAxis(js, 5);
    } else {
        return;
    }

    l_on = s_trig_l_was ? (l > OFF) : (l > ON);
    r_on = s_trig_r_was ? (r > OFF) : (r > ON);
    if (l_on && !s_trig_l_was) {
        plog("pad: L2 axis %d -> page up", l);
        handle_key(SDLK_PAGEUP);
    }
    if (r_on && !s_trig_r_was) {
        plog("pad: R2 axis %d -> page down", r);
        handle_key(SDLK_PAGEDOWN);
    }
    s_trig_l_was = l_on;
    s_trig_r_was = r_on;
}

int main(int argc, char **argv)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Surface  *wsurf;
    SDL_GameController *pad;
    SDL_Joystick *joy;
    UiModel model;
    int running = 1;
    int rc;

    (void)argc; (void)argv;

    log_set_uptime_us(log_uptime_us);
    log_open(BUILD_ID);
    log_msg(LOG_INFO, "boot", "main entered (build %s)", BUILD_ID);
    install_crash_logging();
    /* One file holds every setting. It is created here on the first run, and
     * the old eq.cfg / picture.cfg / viewer.cfg are migrated into it once and
     * then ignored. A broken value can never stop the app: it falls back to
     * its default and says so in the log. */
    cfg_load();
    log_set_level(cfg.log.measure_level);
    log_msg(LOG_INFO, "log", "measurement level %d (0 off, 1 summary, 2 detailed)",
            cfg.log.measure_level);

    /* The toolchain's SDL2 port is driven the way its own sample drives it:
     * joystick events rather than the game-controller layer, a plain window
     * with no flags, and a software renderer created from the window surface. */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        plog("boot: SDL_Init failed: %s", SDL_GetError());
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    plog("boot: SDL_Init ok (video=%s)", SDL_GetCurrentVideoDriver());

    /* ---- graphics library ------------------------------------------- */
    /* SDL only offers its GLES2 renderer once a GL library has been loaded.
     * Until then it reports "Couldn't find matching render driver", which is
     * what every build so far logged while running on the CPU. Both the
     * module load and SDL's own loader are attempted, and each answer is
     * logged so the console says which one it gave us. */
    {
        char     glpath[256];
        uint32_t grc;

        snprintf(glpath, sizeof(glpath), "/%s/common/lib/libGLESv2.sprx",
                 sceKernelGetFsSandboxRandomWord());
        grc = sceKernelLoadStartModule(glpath, 0, NULL, 0, NULL, NULL);
        plog("boot: libGLESv2 rc=0x%08X", (unsigned)grc);
        if ((int)grc < 0) {
            grc = sceKernelLoadStartModule("/system/common/lib/libGLESv2.sprx",
                                           0, NULL, 0, NULL, NULL);
            plog("boot: libGLESv2 fallback rc=0x%08X", (unsigned)grc);
        }
    }
    plog("boot: SDL_GL_LoadLibrary %s",
         SDL_GL_LoadLibrary(NULL) == 0 ? "ok" : SDL_GetError());

    win = SDL_CreateWindow("Ultimate PS4 Media Player",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           SCREEN_W, SCREEN_H,
                           SDL_WINDOW_OPENGL);
    if (!win) {
        plog("boot: GL window failed: %s", SDL_GetError());
        win = SDL_CreateWindow("Ultimate PS4 Media Player",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               SCREEN_W, SCREEN_H, 0);
    } else {
        plog("boot: GL window ok");
    }
    if (!win) {
        plog("boot: SDL_CreateWindow failed: %s", SDL_GetError());
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    plog("boot: window ok");

    /* The GPU comes first. Every working PS4 player renders through it, the
     * console's H.264 decoder writes into GPU memory, and an app that only
     * draws on the CPU never submits the work the decoder waits on - which is
     * what the two-second stall has looked like all along. The software paths
     * stay as the fallback, and whichever one is used is logged by name. */
    const char *kind = "accelerated (GPU)";

    wsurf = NULL;
    /* A3b: A3 hands the scaling to the renderer, so the filter stops being
     * implicit. Set before the renderer exists - backends read this once -
     * and read it back below: a hint silently ignored must not be allowed
     * to read as "linear". */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) {
        plog("boot: accelerated renderer failed: %s", SDL_GetError());
        kind = "sdl software";
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren) {
        plog("boot: software renderer failed: %s", SDL_GetError());
        kind = "surface software";
        plog("boot: getting window surface");
        wsurf = SDL_GetWindowSurface(win);
        ren = wsurf ? SDL_CreateSoftwareRenderer(wsurf) : NULL;
    }
    if (!ren) {
        plog("boot: no renderer: %s", SDL_GetError());
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }
    plog("boot: renderer ok (%s)", kind);
    {
        const char *sq = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);

        plog("boot: scale quality hint '%s'", sq ? sq : "(unset)");
    }

    /* Everything about the output path that decides what a frame costs: the
     * renderer, the surface format the software blit has to convert into, and
     * the mode the console is really displaying. A 720p output with a 1080p
     * window is a very different bill from the reverse. */
    {
        SDL_RendererInfo ri;
        SDL_DisplayMode  dm;
        int              ww = 0, wh = 0, ow = 0, oh = 0;

        plog("boot: SDL %d.%d.%d cpu=%d ram=%dMB",
             SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL,
             SDL_GetCPUCount(), SDL_GetSystemRAM());
        if (SDL_GetRendererInfo(ren, &ri) == 0) {
            plog("boot: renderer '%s' flags=0x%X max_tex=%dx%d formats=%d first=%s",
                 ri.name ? ri.name : "?", (unsigned)ri.flags,
                 ri.max_texture_width, ri.max_texture_height,
                 ri.num_texture_formats,
                 ri.num_texture_formats > 0
                    ? SDL_GetPixelFormatName(ri.texture_formats[0]) : "?");
        }
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetRendererOutputSize(ren, &ow, &oh);
        plog("boot: window %dx%d renderer-output %dx%d", ww, wh, ow, oh);
        if (wsurf && wsurf->format)
            plog("boot: window surface %dx%d pitch=%d fmt=%s",
                 wsurf->w, wsurf->h, wsurf->pitch,
                 SDL_GetPixelFormatName(wsurf->format->format));
        else
            plog("boot: window surface not used (renderer owns the buffer)");
        if (SDL_GetCurrentDisplayMode(0, &dm) == 0)
            plog("boot: display %dx%d @%dHz fmt=%s",
                 dm.w, dm.h, dm.refresh_rate, SDL_GetPixelFormatName(dm.format));
        plog("boot: perf counter freq=%llu",
             (unsigned long long)SDL_GetPerformanceFrequency());
        plog("boot: direct memory pool %.0fMB",
             (double)sceKernelGetDirectMemorySize() / (1024.0 * 1024.0));
    }

    /* The game-controller layer is the one with a DualShock mapping: it knows
     * which physical button is OPTIONS. Raw joystick indices do not, which is
     * how OPTIONS ended up doing nothing and L1/R1 doing the wrong thing. */
    joy = NULL;
    pad = NULL;
    pad = SDL_GameControllerOpen(0);
    if (pad)
        joy = SDL_GameControllerGetJoystick(pad);
    else
        joy = SDL_JoystickOpen(0);
    plog("boot: pad %s", pad ? "game controller opened"
                             : (joy ? "joystick only" : "not found"));
    /* What the pad actually exposes is logged once, because the trigger
     * question cannot be settled without it: if this pad reports four axes
     * there is no analog trigger to read and the digital buttons have to be
     * used instead. */
    if (joy) {
        int ki;

        plog("boot: pad name=%s axes=%d buttons=%d hats=%d",
             SDL_JoystickName(joy), SDL_JoystickNumAxes(joy),
             SDL_JoystickNumButtons(joy), SDL_JoystickNumHats(joy));
        for (ki = 0; ki < SDL_JoystickNumAxes(joy) && ki < 8; ki++)
            plog("boot: pad axis %d rest=%d", ki,
                 SDL_JoystickGetAxis(joy, ki));
    }

    /* The firmware keeps libSceAvPlayer out of memory until an application
     * asks for it. Calling sceAvPlayerInit on an unloaded module jumps into
     * unmapped code, which is exactly where the first video attempt died. */
    {
        static const struct {
            const char      *name;
            enum OrbisSysModule id;
        } mods[] = {
            /* Only AvPlayer is loaded. It brings up its own decoder libraries
             * (libSceVideodec2 / libSceVdecwrap are in its dependency list),
             * and pre-loading them here is the one structural difference left
             * from the player that works: doing so leaves the decoder claimed
             * before AvPlayer asks for it, and nothing is ever delivered. */
            { "AV_PLAYER",  ORBIS_SYSMODULE_AV_PLAYER },
        };
        size_t mi;

        for (mi = 0; mi < sizeof(mods) / sizeof(mods[0]); mi++) {
            int mrc = sceSysmoduleIsLoaded(mods[mi].id);

            if (mrc != 0)
                mrc = sceSysmoduleLoadModule(mods[mi].id);
            else
                mrc = 0;
            plog("boot: module %s rc=0x%08X", mods[mi].name, (unsigned)mrc);
        }
    }

    /* The H.264 decoder behind AvPlayer (libSceVdecSavc) calls into
     * libSceGnmDriver, which a homebrew app does not get loaded for free.
     * Without it the decoder thread dies on an unresolved import and the
     * player parses the file, reports PLAY, and never produces one frame -
     * which is exactly what the console showed. */
    {
        char path[256];
        uint32_t grc;

        snprintf(path, sizeof(path), "/%s/common/lib/libSceGnmDriver.sprx",
                 sceKernelGetFsSandboxRandomWord());
        grc = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
        plog("boot: gnm driver rc=0x%08X", (unsigned)grc);
        if ((int)grc < 0) {
            grc = sceKernelLoadStartModule(
                "/system/common/lib/libSceGnmDriver.sprx", 0, NULL, 0, NULL, NULL);
            plog("boot: gnm driver fallback rc=0x%08X", (unsigned)grc);
        }
        s_gnm_loaded = ((int)grc >= 0);
    }
    plog("boot: system service module rc=0x%08X",
         (unsigned)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE));
    plog("boot: video out module rc=0x%08X",
         (unsigned)sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_VIDEO_OUT));

    vol_init(&s_vol);
    eq_init(&s_eq, OUT_RATE, 2);
    /* The EQ, the theme, the viewer's grade and the visualizer's settings are
     * all in config.ini now, so one call restores the lot. */
    settings_apply();
    video_set_picture(s_video_saturation, s_video_sharpness);
    rs_init(&s_rs, 44100, OUT_RATE, 2);
    plog("boot: audio graph ready");

    rc = aout_open(OUT_RATE, BLOCK_FRAMES);
    plog("boot: aout_open rc=0x%08X", (unsigned)rc);
    if (rc != 0) {
        snprintf(s_status, sizeof(s_status), "Audio out failed: 0x%08X", (unsigned)rc);
    }


    /* ---- what this run is, once, in one block --------------------------
     * A report should not need questions: the build, the firmware, the output
     * mode, the memory, the audio path, where settings and logs are, and which
     * settings are not at their defaults are all here. */
    {
        int                  ow = SCREEN_W, oh = SCREEN_H;
        int                  ram;
        struct statvfs       sv;
        unsigned long long   direct;
        OrbisKernelSwVersion sw;

        /* The real layout, as the kernel writes it: a size field first, then
         * the printable version, then the numeric one. The size is set before
         * the call, and a failure is a log line rather than anything else. */
        memset(&sw, 0, sizeof(sw));
        sw.Size = sizeof(sw);
        {
            int32_t frc = sceKernelGetSystemSwVersion(&sw);

            if (frc == 0) {
                sw.VersionString[sizeof(sw.VersionString) - 1] = '\0';
                log_msg(LOG_INFO, "boot", "firmware %s (0x%08X) - note: a spoofed system reports a different version",
                        sw.VersionString, (unsigned)sw.Version);
            } else {
                log_msg(LOG_INFO, "boot", "firmware: not available (0x%08X)",
                        (unsigned)frc);
            }
        }

        if (SDL_GetRendererOutputSize(ren, &ow, &oh) == 0)
            log_msg(LOG_INFO, "display",
                    "output %dx%d, canvas %dx%d, quality step %d",
                    ow, oh, SCREEN_W, SCREEN_H, vis_art_step());
        else
            log_msg(LOG_INFO, "display", "output: not available");

        direct = (unsigned long long)sceKernelGetDirectMemorySize();
        /* SDL answers 0 here on this platform, and "system 0 MB" reads like a
         * fault. Say it is not reported instead of printing a number. */
        ram = SDL_GetSystemRAM();
        if (ram > 0)
            log_msg(LOG_INFO, "mem",
                    "system %d MB, direct pool %.0f MB, video rgba %.0f KB",
                    ram, (double)direct / (1024.0 * 1024.0),
                    (double)video_rgba_bytes() / 1024.0);
        else
            log_msg(LOG_INFO, "mem",
                    "system: not reported on this platform, direct pool %.0f MB, "
                    "video rgba %.0f KB",
                    (double)direct / (1024.0 * 1024.0),
                    (double)video_rgba_bytes() / 1024.0);

        log_msg(LOG_INFO, "build", "%s %s, compiler %s, optimised %d, build %s",
                BUILD_DATE, BUILD_TIME, __VERSION__,
#ifdef __OPTIMIZE__
                1,
#else
                0,
#endif
                BUILD_ID);

        log_msg(LOG_INFO, "audio",
                "%d Hz, 2 channels, %d-frame blocks (%.0f ms each)",
                OUT_RATE, BLOCK_FRAMES,
                1000.0 * (double)BLOCK_FRAMES / (double)OUT_RATE);

        /* statvfs here has answered 0 without writing the struct: one run
         * printed a 32-digit MB figure taken from whatever was on the stack.
         * Only numbers that could describe a real disk are believed, and the
         * unit grows so the line stays readable. */
        memset(&sv, 0, sizeof(sv));
        if (statvfs("/data", &sv) == 0) {
            double unit  = sv.f_frsize ? (double)sv.f_frsize
                                       : (double)sv.f_bsize;
            double bytes = (double)sv.f_bavail * unit;
            double total = (double)sv.f_blocks * unit;

            if (unit >= 512.0 && unit <= 65536.0 && total > 0.0 &&
                total <= 16.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0 &&
                bytes <= total) {
                if (bytes >= 1024.0 * 1024.0 * 1024.0)
                    log_msg(LOG_INFO, "storage", "%s, %.1f GB free there",
                            cfg_path(), bytes / (1024.0 * 1024.0 * 1024.0));
                else if (bytes >= 1024.0 * 1024.0)
                    log_msg(LOG_INFO, "storage", "%s, %.0f MB free there",
                            cfg_path(), bytes / (1024.0 * 1024.0));
                else
                    log_msg(LOG_INFO, "storage", "%s, %.0f KB free there",
                            cfg_path(), bytes / 1024.0);
            } else
                log_msg(LOG_INFO, "storage",
                        "%s (free space not readable: the filesystem returned "
                        "numbers that cannot describe a disk)", cfg_path());
        } else
            log_msg(LOG_INFO, "storage", "%s (free space not available)",
                    cfg_path());

        /* Connection state lives on its own thread: boot never waits for it,
         * and a console that is offline just logs "not connected". */
        net_start(cfg.log.network_status, cfg.log.network_poll_seconds);
        cfg_log_diffs();
    }

    plog("boot: scanning media");
    scan_begin();
    plog("boot: scan done, %d tracks", s_track_count);
    /* The index printed by the play: lines means nothing on its own; this is
     * the table that turns it back into a file name. */
    {
        int ti;

        for (ti = 0; ti < s_track_count; ti++) {
            struct stat st;
            long        sz = (stat(s_tracks[ti].path, &st) == 0) ? (long)st.st_size : -1;

            plog("scan: %d kind=%s size=%ld %s", ti,
                 s_tracks[ti].is_video ? "video" : "audio", sz,
                 s_tracks[ti].name);
        }
    }
    if (s_track_count == 0)
        snprintf(s_status, sizeof(s_status),
                 "No media found - put files in /data/music or a USB drive");
    /* first song and first video in each section, so a section always opens on
     * something instead of an empty list */
    s_sel_music = -1;
    s_sel_video = -1;
    {
        int ti;

        for (ti = 0; ti < s_track_count; ti++) {
            if (s_tracks[ti].is_video && s_sel_video < 0)
                s_sel_video = ti;
            if (!s_tracks[ti].is_video && s_sel_music < 0)
                s_sel_music = ti;
        }
    }
    s_selected = s_sel_music;
    browser_open();                     /* fills the drive list */
    s_screen   = UI_SCREEN_HOME;        /* the app opens on the sections */
    plog("boot: home ready (%d media, %d pictures)", s_track_count, s_pic_count);

    {
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 512 * 1024);
        if (pthread_create(&s_thread, &attr, audio_thread, NULL) == 0)
            s_thread_started = 1;
        pthread_attr_destroy(&attr);
    }
    plog("boot: entering main loop");

    {
    int frame = 0;

    s_perf.freq      = SDL_GetPerformanceFrequency();
    s_perf.win_start = SDL_GetPerformanceCounter();
    s_perf.cpu_start = sceKernelGetProcessTime();

    while (running && !s_quit) {
        SDL_Event ev;
        Uint64    t_start = SDL_GetPerformanceCounter();
        Uint64    t_lap   = t_start;
        int       painted = 1;    /* a menu frame is always painted */

        s_stage    = 1;
        s_frame_no = frame;
        /* Everything measured this frame is attributed to this style, step and
         * file, and that is also the line that says what a crash interrupted. */
        stats_context(s_vis_style, vis_art_step(), frame,
                      (s_current >= 0 && s_current < s_track_count)
                          ? s_tracks[s_current].path : NULL);

        if (frame < 3)
            plog("loop[%d]: poll start", frame);

        /* The PS4 SDL video driver is happy to deliver events only once a
         * frame has been submitted, so the first iteration presents before
         * it polls. */
        if (frame > 0) {
        while (SDL_PollEvent(&ev)) {
            if (frame < 3)
                plog("loop[%d]: event type %u", frame, (unsigned)ev.type);

            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                handle_key(ev.key.keysym.sym);
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                int key = controller_button_to_key((Uint8)ev.cbutton.button);

                plog("pad: controller button %d -> key %d",
                     ev.cbutton.button, key);
                if (key >= 0)
                    handle_key(key);
            } else if (!pad && ev.type == SDL_JOYBUTTONDOWN) {
                int key = -1;

                /* The d-pad is delivered as a hat on this SDL port, and the
                 * same presses also arrive as numbered buttons in an order
                 * that does not match the hat bits, so only the face and
                 * shoulder buttons are mapped here and the hat below owns all
                 * four directions.
                 *
                 * The numbers follow the pad's own report order - Cross,
                 * Circle, Square, Triangle, L1, R1, L2, R2, SHARE, OPTIONS,
                 * L3, R3, PS - not the order they were guessed in before.
                 * The old guesses put L1 on the SHARE key (so it backed out of
                 * a screen), OPTIONS on the previous-track key (so it looked
                 * like a play button that walked up the list) and left R1
                 * unbound. */
                switch (ev.jbutton.button) {
                case 0:  key = SDLK_x;      break;  /* Cross    */
                case 1:  key = SDLK_ESCAPE; break;  /* Circle   */
                case 2:  key = SDLK_s;      break;  /* Square   */
                case 3:  key = SDLK_e;      break;  /* Triangle */
                case 4:  key = SDLK_q;      break;  /* L1       */
                case 5:  key = SDLK_w;      break;  /* R1       */
                /* The triggers page the music library a screen at a time.
                 * They used to seek a song and broke playback, so they only
                 * send a page key now and every screen decides what to do
                 * with it (the library pages, the rest ignores it). */
                case 6:  key = SDLK_PAGEUP;   break;  /* L2 */
                case 7:  key = SDLK_PAGEDOWN; break;  /* R2 */
                /* SHARE is the console's own button: it opens the share menu
                 * and takes screenshots and clips, so it is never mapped. */
                case 8:  key = -1;  break;  /* SHARE    */
                case 9:  key = SDLK_t;      break;  /* OPTIONS  */
                case 10: key = SDLK_m;      break;  /* L3 mute  */
                /* The v55 log settles both sticks: pressing L3 reported
                 * button 11 and pressing R3 reported 12. 11 was left mapped
                 * to the equalizer in v55, which is why L3 opened it. */
                case 11: key = SDLK_m;      break;  /* L3       */
                case 12: key = SDLK_r;      break;  /* R3 EQ    */
                /* The touchpad click is the only button left that nothing
                 * else uses: it saves the finished frame as a BMP instead of
                 * sending a key, so a capture cannot page or change a view. */
                case 13:
                    plog("pad: touchpad click -> screenshot");
                    shot_request();
                    break;
                /* L2, R2, the stick clicks and PS/SHARE extras stay
                 * unmapped: acting on them started files on their own. */
                default: break;
                }

                /* The pad also reports trigger and stick jitter as button
                 * events; logging each one buried the useful lines in the
                 * boot log, so every button number is logged only once. */
                if (key >= 0 || ev.jbutton.button >= 32 ||
                    !(s_btn_logged & (1u << ev.jbutton.button))) {
                    plog("joy: button %d -> key %d", ev.jbutton.button, key);
                    if (ev.jbutton.button < 32)
                        s_btn_logged |= 1u << ev.jbutton.button;
                }

                if (key >= 0)
                    handle_key(key);
            }
        }
        }
        LAP_ADD(ev);

        if (s_track_finished) {
            s_track_finished = 0;
            /* next song, never a video from the music list */
            play_neighbour(+1);
        }

        if (s_video_mode) {
            int vrc;

            s_stage = 2;
            vrc = video_step();

            if (vrc <= 0) {
                char why[192];

                snprintf(why, sizeof(why), "%s",
                         vrc < 0 ? video_last_error() : "Finished");
                stop_video();
                snprintf(s_status, sizeof(s_status), "%s", why);
            }
        }

        /* The d-pad is read straight from the stick's hat instead of from
         * hat events: the buttons numbered 11-14 on this SDL port are the two
         * stick clicks (L3/R3), not directions, and acting on those made the
         * sticks adjust the equaliser. Edge detection keeps one press to one
         * step, exactly like a button event would. */
        if (!pad && joy) {
            int hat = SDL_JoystickGetHat(joy, 0);

            if (hat != s_hat_last) {
                plog("joy: hat %d", hat);
                if (hat & SDL_HAT_UP)    { handle_key(SDLK_UP);    s_hat_dir = SDLK_UP; }
                if (hat & SDL_HAT_DOWN)  { handle_key(SDLK_DOWN);  s_hat_dir = SDLK_DOWN; }
                if (hat & SDL_HAT_LEFT)  { handle_key(SDLK_LEFT);  s_hat_dir = SDLK_LEFT; }
                if (hat & SDL_HAT_RIGHT) { handle_key(SDLK_RIGHT); s_hat_dir = SDLK_RIGHT; }
                if (hat == SDL_HAT_CENTERED)
                    s_hat_dir = 0;
                s_hat_last = hat;
                s_hat_t0   = pace_now_ms();
                s_hat_next = s_hat_t0 + 300;        /* then repeat */
            }

            /* Holding the d-pad repeats, and the repeat speeds up the longer
             * it is held: one step every 200 ms, then 90, then 45, then 22.
             * The step stays 1, so a slider still moves in single increments -
             * it just does not take a minute to cross the whole range. */
            if (s_hat_dir) {
                uint64_t now  = pace_now_ms();
                uint64_t held = now - s_hat_t0;

                if (now >= s_hat_next) {
                    handle_key(s_hat_dir);
                    s_hat_next = now + (held < 600  ? 200
                                     : held < 1600 ? 90
                                     : held < 3200 ? 45 : 22);
                }
            }
        }

        /* L2 and R2: the page keys, read as the analog axes they are. */
        pad_triggers(pad, joy);

        /* R3 zoom in the picture viewer: the right stick pans in all eight
         * directions, so both axes are read together and the diagonals work
         * like the cardinal directions do. */
        if (s_screen == UI_SCREEN_PICTURES && s_pic_zoom_mode && joy) {
            int ax = SDL_JoystickGetAxis(joy, 2);
            int ay = SDL_JoystickGetAxis(joy, 3);
            const int dead = 6000;

            if (s_pic_zoom <= 100)
                s_pic_zoom = 110;           /* zoom mode shows some zoom */
            if (ax > dead || ax < -dead) {
                s_pic_pan_x += ax / 2000;
                if (s_pic_pan_x >  1000) s_pic_pan_x =  1000;
                if (s_pic_pan_x < -1000) s_pic_pan_x = -1000;
            }
            if (ay > dead || ay < -dead) {
                s_pic_pan_y += ay / 2000;
                if (s_pic_pan_y >  1000) s_pic_pan_y =  1000;
                if (s_pic_pan_y < -1000) s_pic_pan_y = -1000;
            }
        }

        LAP_ADD(step);

        if (frame < 3)
            plog("loop[%d]: events done", frame);
        plog_once("loop: first iteration");

        build_model(&model);

        if (frame < 3)
            plog("loop[%d]: model built", frame);

        /* The UI is authored at 1080p but drawn at whatever the console is
         * really outputting (720p, 1080p, 1440p), so text shrinks and grows
         * with the panels instead of drifting off them. */
        {
            int out_w = SCREEN_W, out_h = SCREEN_H;

            if (SDL_GetRendererOutputSize(ren, &out_w, &out_h) != 0 ||
                out_w <= 0 || out_h <= 0) {
                out_w = SCREEN_W;
                out_h = SCREEN_H;
            }

            s_view_w = out_w;
            s_view_h = out_h;
        }
        LAP_ADD(model);

        s_stage = 4;
        ui_theme_set(s_theme);      /* the skin is a draw-time choice */
        if (s_screen == UI_SCREEN_VIDEOS) {
            int fw = 0, fh = 0, serial = 0;
            const uint8_t *pix = video_frame(&fw, &fh, &serial);
            int fresh = 0;

            painted = 0;

            if (pix && fw > 0 && fh > 0) {
                if (!s_video_tex || s_video_tex_w != fw || s_video_tex_h != fh) {
                    if (s_video_tex)
                        SDL_DestroyTexture(s_video_tex);
                    plog("video: creating texture %dx%d", fw, fh);
                    s_video_tex = SDL_CreateTexture(ren,
                                                    SDL_PIXELFORMAT_ABGR8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    fw, fh);
                    plog("video: texture %s", s_video_tex ? "ok" : SDL_GetError());
                    s_video_tex_w  = fw;
                    s_video_tex_h  = fh;
                    s_video_serial = -1;
                }
                if (s_video_tex && serial != s_video_serial) {
                    /* The cost of handing a frame to the renderer is measured,
                     * not assumed: together with the conversion cost the
                     * player logs, this says which side of the pipeline is
                     * holding the frame rate down. */
                    static Uint64 up_freq, up_start, up_sum;
                    static int    up_n;
                    Uint64        c0, c1;

                    plog_once("video: first frame uploaded");
                    if (!up_freq) {
                        up_freq  = SDL_GetPerformanceFrequency();
                        up_start = SDL_GetPerformanceCounter();
                    }
                    c0 = SDL_GetPerformanceCounter();
                    SDL_UpdateTexture(s_video_tex, NULL, pix, fw * 4);
                    c1 = SDL_GetPerformanceCounter();
                    up_sum += c1 - c0;
                    up_n++;
                    if (c1 - up_start >= up_freq * 2) {
                        plog("video: upload=%.2fms/frame (%d frames)",
                             (double)up_sum * 1000.0 / ((double)up_freq * up_n),
                             up_n);
                        up_sum   = 0;
                        up_n     = 0;
                        up_start = c1;
                    }
                    s_video_serial = serial;
                    s_perf.tex_w   = fw;
                    s_perf.tex_h   = fh;
                    fresh          = 1;
                }
            }
            LAP_ADD(up);
            s_stage = 5;
            /* The picture is painted only when something changed - a new
             * frame, a button, or the bar and toasts while they are on screen
             * (at 10 Hz, which is all their fade needs). */
            {
                const int visible = model.video_hud || model.video_toast_alpha > 0 ||
                                    s_video_settings || s_show_eq;
                Uint32    vnow    = SDL_GetTicks();

                if (visible != s_vid_visible) {
                    s_vid_visible = visible;
                    s_vid_dirty   = 1;
                }
                /* Ten times a second at the very least, even with nothing to
                 * show: this driver hands over pad events around a present,
                 * and a button must never wait on a frame that is not
                 * coming. */
                if (vnow - s_vid_painted >= 100)
                    s_vid_dirty = 1;

                painted = fresh || s_vid_dirty;
                if (painted) {
                    static Uint64 dr_freq, dr_start, dr_sum;
                    static int    dr_n;
                    Uint64        d0, d1;

                    if (!dr_freq) {
                        dr_freq  = SDL_GetPerformanceFrequency();
                        dr_start = SDL_GetPerformanceCounter();
                    }
                    d0 = SDL_GetPerformanceCounter();
                    if (s_video_fullscreen && s_video_mode)
                        ui_draw_video(ren, s_video_tex, fw, fh, &model,
                                      s_view_w, s_view_h);
                    else
                        ui_draw_video_section(ren, s_video_tex, fw, fh, &model,
                                              s_view_w, s_view_h);
                    d1 = SDL_GetPerformanceCounter();
                    dr_sum += d1 - d0;
                    dr_n++;
                    if (d1 - dr_start >= dr_freq * 2) {
                        plog("video: draw=%.2fms/frame (%d frames)",
                             (double)dr_sum * 1000.0 / ((double)dr_freq * dr_n), dr_n);
                        dr_sum   = 0;
                        dr_n     = 0;
                        dr_start = d1;
                    }
                    s_vid_painted = vnow;
                    s_vid_dirty   = 0;
                }
            }
        } else if (s_screen == UI_SCREEN_PICTURES) {
            int            pw = 0, ph = 0, ps = 0;
            const uint8_t *pp = picture_pixels(&pw, &ph, &ps);

            LAP_ADD(up);
            if (pp && pw > 0 && ph > 0) {
                if (!s_pic_tex || s_pic_tex_w != pw || s_pic_tex_h != ph) {
                    if (s_pic_tex)
                        SDL_DestroyTexture(s_pic_tex);
                    s_pic_tex = SDL_CreateTexture(ren,
                                                  SDL_PIXELFORMAT_ABGR8888,
                                                  SDL_TEXTUREACCESS_STREAMING,
                                                  pw, ph);
                    if (s_pic_tex) {
                        s_pic_tex_w  = pw;
                        s_pic_tex_h  = ph;
                        s_pic_serial = -1;
                        /* Photos are opaque: blending them would let the
                         * alpha byte tint the picture towards the dark
                         * plate behind it. */
                        SDL_SetTextureBlendMode(s_pic_tex, SDL_BLENDMODE_NONE);
                        {
                            int fmt = 0;

                            SDL_QueryTexture(s_pic_tex, (Uint32 *)&fmt, NULL,
                                             NULL, NULL);
                            plog("picture: texture %dx%d px=%d,%d,%d,%d fmt=%s",
                                 pw, ph, pp[0], pp[1], pp[2], pp[3],
                                 SDL_GetPixelFormatName((Uint32)fmt));
                        }
                    } else {
                        plog("picture: texture failed: %s", SDL_GetError());
                        s_pic_tex_w = s_pic_tex_h = 0;
                    }
                }
                if (s_pic_tex && ps != s_pic_serial) {
                    SDL_UpdateTexture(s_pic_tex, NULL, pp, pw * 4);
                    s_pic_serial = ps;
                }
            }
            s_stage = 5;
            ui_draw_pictures(ren, s_pic_tex, s_pic_tex_w, s_pic_tex_h, &model,
                             s_view_w, s_view_h);
        } else {
            LAP_ADD(up);
            if (s_screen == UI_SCREEN_MUSIC && s_vis_full)
                ui_draw_visualizer_full(ren, &model, s_view_w, s_view_h);
            else
                ui_draw(ren, &model, s_view_w, s_view_h);
        }
        LAP_ADD(draw);
        /* The console parks a GNM app that never answers while its system UI
         * (the PS menu used to close apps) is over it. Ten times a second is
         * enough to answer and rare enough not to stall decoding. */
        if (s_gnm_loaded && (frame % 6) == 0) {
            SysStatus st;

            memset(&st, 0, sizeof(st));
            if (sceSystemServiceGetStatus(&st) == 0 &&
                (st.system_ui_overlaid || st.in_background))
                sceGnmSubmitDone();
        }

        if (frame < 3)
            plog("loop[%d]: presenting", frame);

        /* Presenting was the only part of a frame nobody had timed. The
         * renderer is software, so this pushes a whole 1920x1080 surface, and
         * it happens twice: once to the renderer's target and once to the
         * window surface. Timed separately because they are separate costs.
         * Skipped whole on a loop that painted nothing: its numbers would be
         * zeros that pull every average down. */
        if (painted) {
            static Uint64 p_freq, p_start, p_sum, w_sum, f_sum;
            static Uint64 f_max, p_max, w_max;
            static int    p_n;
            Uint64        f0, f1, p0, p1, w1;

            if (!p_freq) {
                p_freq  = SDL_GetPerformanceFrequency();
                p_start = SDL_GetPerformanceCounter();
            }
            /* The software renderer queues its commands and runs them at
             * present time, which is why draw= read 0.01 ms while present=
             * read 50 ms: they were the same work. Flushing first splits the
             * two - flush is the clear plus the scaled blit of the picture,
             * present is the flip the display driver waits on. */
            s_stage = 6;
            f0 = SDL_GetPerformanceCounter();
            SDL_RenderFlush(ren);
            f1 = SDL_GetPerformanceCounter();
            LAP_ADD(flush);

            p0 = SDL_GetPerformanceCounter();
            SDL_RenderPresent(ren);
            p1 = SDL_GetPerformanceCounter();
            LAP_ADD(pres);

            if (wsurf)
                SDL_UpdateWindowSurface(win);
            w1 = SDL_GetPerformanceCounter();
            LAP_ADD(surf);

            /* The frame is finished and on screen, so a requested capture
             * reads it back now. One frame pays for it and the log says so. */
            if (s_shot_want) {
                s_shot_want = 0;
                shot_save(ren);
            }

            f_sum += f1 - f0;
            p_sum += p1 - p0;
            w_sum += w1 - p1;
            if (f1 - f0 > f_max) f_max = f1 - f0;
            if (p1 - p0 > p_max) p_max = p1 - p0;
            if (w1 - p1 > w_max) w_max = w1 - p1;
            p_n++;
            if (w1 - p_start >= p_freq * 2) {
                plog("video: flush=%.2fms/frame present=%.2fms/frame surface=%.2fms/frame (%d frames, worst flush=%.2f present=%.2f)",
                     (double)f_sum * 1000.0 / ((double)p_freq * p_n),
                     (double)p_sum * 1000.0 / ((double)p_freq * p_n),
                     (double)w_sum * 1000.0 / ((double)p_freq * p_n), p_n,
                     (double)f_max * 1000.0 / (double)p_freq,
                     (double)p_max * 1000.0 / (double)p_freq);
                plog_perf("present flush=%.2f present=%.2f surface=%.2f worst=%.2f/%.2f/%.2f n=%d",
                         (double)f_sum * 1000.0 / ((double)p_freq * p_n),
                         (double)p_sum * 1000.0 / ((double)p_freq * p_n),
                         (double)w_sum * 1000.0 / ((double)p_freq * p_n),
                         (double)f_max * 1000.0 / (double)p_freq,
                         (double)p_max * 1000.0 / (double)p_freq,
                         (double)w_max * 1000.0 / (double)p_freq, p_n);
                f_sum = p_sum = w_sum = 0;
                f_max = p_max = w_max = 0;
                p_n     = 0;
                p_start = w1;
            }
        }

        /* The library walk gets a slice after the picture, never before it:
         * whatever is left of it runs while the user is already looking at
         * the home screen. */
        if (s_scanning) {
            scan_slice(12);
            if (s_scanning && !s_playing && !s_video_mode)
                snprintf(s_status, sizeof(s_status),
                         "Scanning media   %d track%s", s_track_count,
                         s_track_count == 1 ? "" : "s");
        }

        if (frame < 3)
            plog("loop[%d]: presented", frame);
        else if ((frame % 120) == 0)
            plog("frame %d, %d tracks, video=%d", frame, s_track_count, s_video_mode);
        frame++;
        /* A fixed 16 ms sleep per loop is 16 ms added to every displayed
         * frame. Video is paced by the decoder already; only the menus need
         * to be throttled. */
        s_stage = 7;
        if (!s_video_mode) {
            /* Sleep only what is left of the frame, never a fixed 16 ms on top
             * of it. The fixed sleep was paid on every menu frame whether the
             * frame had already missed its budget or not, and the console log
             * read rest=16.50 on frames that took 100 ms: a sixth of a frame
             * thrown away for nothing. */
            double spent = perf_ms(SDL_GetPerformanceCounter() - t_start);

            if (spent < 16.0)
                SDL_Delay((Uint32)(16.0 - spent));
        }

        LAP_ADD(rest);
        {
            Uint64 t_end = SDL_GetPerformanceCounter();

            s_perf.s_loop += t_end - t_start;
            if (t_end - t_start > s_perf.max_loop)
                s_perf.max_loop = t_end - t_start;
            s_perf.n++;
            /* The whole frame, flush and present included: this is what the
             * quality ladder has to judge, because the look's own drawing is
             * barely half of what a frame on the console costs. */
            vis_art_frame_ms(perf_ms(t_end - t_start));
            stats_frame(perf_ms(t_end - t_start),
                        vis_art_step() == 1 ? cfg.quality.budget_step1_ms
                                            : cfg.quality.budget_ms);
        }
        perf_report(s_video_mode);
    }

    s_quit = 1;
    pthread_join(s_thread, NULL);
    if (s_video_mode)
        video_close();
    sub_close();
    if (s_src_open)
        mp3_close(&s_src);
    aout_close();
    if (s_video_tex)
        SDL_DestroyTexture(s_video_tex);
    if (pad)
        if (joy)
        SDL_JoystickClose(joy);
    if (pad)
        SDL_GameControllerClose(pad);
    plog("boot: exited main loop");
    }

    settings_save();

    /* Shut down in a fixed order and touch as little of SDL as possible on the
     * way out: the audio thread must be gone before the process ends, and the
     * console's driver did not survive tearing down a window whose surface is
     * still owned by a software renderer. */
    s_quit = 1;
    if (s_thread_started)
        pthread_join(s_thread, NULL);
    aout_close();
    stats_end((double)SDL_GetTicks() / 1000.0);
    log_msg(LOG_INFO, "boot", "clean exit");
    log_close();
    /* Leave through _exit so no atexit handler gets a chance to touch a driver
     * that has already been handed back; returning from main crashed here. */
    _exit(0);
}
