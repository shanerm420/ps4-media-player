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
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "audio_core.h"
#include "audio_out_orbis.h"
#include "eq.h"
#include "mp3_source.h"
#include "resampler.h"
#include "subtitles.h"
#include "ui.h"
#include "video_orbis.h"

#define OUT_RATE      48000
#define BLOCK_FRAMES  1024
/* How long the video transport bar stays up after the last button press. */
#define HUD_MS        3000
#define MAX_TRACKS    256
#define MAX_PATH_LEN  512
#define SCREEN_W      1920
#define SCREEN_H      1080

typedef struct {
    char path[MAX_PATH_LEN];
    char name[MAX_PATH_LEN];
    int  is_video;
} Track;

static Track   s_tracks[MAX_TRACKS];
static UiTrack s_uitracks[MAX_TRACKS];     /* what the UI draws from */
static int     s_track_count;
static int     s_selected;
static int     s_current = -1;
static volatile int s_playing;
static volatile int s_quit;
static volatile int s_track_finished;

static Volume       s_vol;
static Eq           s_eq;
static Mp3Source    s_src;
static Resampler    s_rs;
static pthread_t    s_thread;
static int          s_src_open;
static char         s_status[256] = "No track loaded";
static char         s_format[64];
static double       s_duration;
static int          s_show_eq;
static int          s_eq_band;
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
static int          s_video_settings;
static int          s_settings_row;
static int          s_sub_size = UI_SUB_NORMAL;
static char         s_sub_label[128] = "NONE FOUND";

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

static void scan_dir(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;

    if (!d)
        return;                         /* not mounted: normal, skip it */
    while ((e = readdir(d)) != NULL && s_track_count < MAX_TRACKS) {
        Track *t;
        char   path[MAX_PATH_LEN];
        int is_mp3   = has_suffix(e->d_name, ".mp3");
        int is_video = has_suffix(e->d_name, ".mp4") || has_suffix(e->d_name, ".mov");
        if (!is_mp3 && !is_video)
            continue;

        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        /* a drive root and its music/ subfolder can both be scanned, and
         * /usb0 may alias /mnt/usb0 on some setups */
        if (already_added(path))
            continue;

        t = &s_tracks[s_track_count];
        snprintf(t->path, sizeof(t->path), "%s", path);
        snprintf(t->name, sizeof(t->name), "%s", e->d_name);
        t->is_video = is_video ? 1 : 0;

        s_uitracks[s_track_count].name     = s_tracks[s_track_count].name;
        s_uitracks[s_track_count].is_video = t->is_video;
        s_track_count++;
    }
    closedir(d);
}

/* Every place media can live on an OrbisOS console:
 *   /data, /data/linux   internal hard drive (the player's own home is
 *                        /data, so a folder next to it is always reachable)
 *   /mnt/usb0 .. usb7    USB drives, in plug order: the two front ports and
 *                        the rear one all land here (8 devices is the limit)
 *   /mnt/ext0            a drive formatted as extended storage
 *   /usb0, /usb1         legacy Linux-install mounts
 * Each is tried both directly and in its music/ subfolder. Missing mount
 * points simply fail to open, so an unplugged port costs nothing. */
static void scan_all(void)
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

    s_track_count = 0;
    for (i = 0; i < sizeof(roots)/sizeof(roots[0]); i++)
        scan_dir(roots[i]);
}

/* ------------------------------------------------------------------ */
/* Playback                                                            */
/* ------------------------------------------------------------------ */

/* Fills and submits whole sceAudioOut blocks. Video packets can be larger than
 * one block (AAC frames arrive in bursts), so this loops instead of trimming:
 * the old truncating version quietly dropped whatever did not fit. */
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

/* 1.6 s on screen: fully opaque for the first 0.8 s, then a fade. */
static void video_toast_show(const char *msg)
{
    snprintf(s_toast, sizeof(s_toast), "%s", msg);
    s_toast_start = SDL_GetTicks();
    s_toast_dur   = 1600;
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

static void video_setting_step(int dir)
{
    switch (s_settings_row) {
    case UI_VIDEO_ROW_ASPECT:
        s_video_aspect = (s_video_aspect + dir + UI_ASPECT_COUNT) % UI_ASPECT_COUNT;
        break;
    case UI_VIDEO_ROW_BRIGHTNESS:
        s_video_brightness += dir * 5;
        if (s_video_brightness >  100) s_video_brightness =  100;
        if (s_video_brightness < -100) s_video_brightness = -100;
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
}

static void play_track(int index)
{
    float decoded[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
    int   frames;

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

    if (s_tracks[index].is_video) {
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

    /* Decode one frame to learn the stream format, then rewind to the start. */
    frames = mp3_read(&s_src, decoded, 1152);
    if (frames > 0) {
        int ch = s_src.channels;
        eq_init(&s_eq, OUT_RATE, 2);
        eq_set_preamp_db(&s_eq, 0.0f);
        rs_init(&s_rs, s_src.hz > 0 ? s_src.hz : 44100, OUT_RATE, ch);
        snprintf(s_format, sizeof(s_format), "MP3   %d HZ   %s",
                 s_src.hz, ch == 2 ? "STEREO" : "MONO");
        snprintf(s_status, sizeof(s_status), "%s   %d Hz %s", s_tracks[index].name,
                 s_src.hz, ch == 2 ? "stereo" : "mono");
    }
    mp3_close(&s_src);
    (void)mp3_open(&s_src, s_tracks[index].path);
    s_duration = mp3_duration_est(&s_src);

    s_playing = 1;
}

/* ------------------------------------------------------------------ */
/* Drive browser                                                       */
/* ------------------------------------------------------------------ */

#define MAX_SOURCES  16
#define MAX_ENTRIES  512
#define BROWSER_PAGE 10

enum { EK_DIR, EK_MP3, EK_VIDEO, EK_RESCAN, EK_ALLMEDIA, EK_UP };

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
static int     s_screen = UI_SCREEN_BROWSER;

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
    case EK_VIDEO:    return 2;
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

    entry_add("ALL MEDIA (LIBRARY)", EK_ALLMEDIA, -1);
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
        if (!is_mp3 && !is_video)
            continue;
        entry_add(e->d_name, is_video ? EK_VIDEO : EK_MP3, -1);
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

static void browser_play(void)
{
    char        full[MAX_PATH_LEN];
    const char *name = s_entries[s_entry_sel].name;
    int         idx;

    if (join_path(full, sizeof(full), s_browser_path, name) != 0) {
        snprintf(s_browser_note, sizeof(s_browser_note), "PATH TOO LONG");
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
        snprintf(t->path, sizeof(t->path), "%s", full);
        snprintf(t->name, sizeof(t->name), "%s", name);
        t->is_video = (has_suffix(name, ".mp4") || has_suffix(name, ".mov")) ? 1 : 0;
        s_uitracks[s_track_count].name     = t->name;
        s_uitracks[s_track_count].is_video = t->is_video;
        idx = s_track_count++;
    }

    s_selected = idx;
    play_track(idx);
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

    scan_all();
    refresh_sources();

    if (had_current) {
        int idx = track_index_for(current);

        if (idx < 0 && s_track_count < MAX_TRACKS) {
            Track *t = &s_tracks[s_track_count];
            const char *nm = strrchr(current, '/');

            snprintf(t->path, sizeof(t->path), "%s", current);
            snprintf(t->name, sizeof(t->name), "%s", nm ? nm + 1 : current);
            t->is_video = was_video;
            s_uitracks[s_track_count].name     = t->name;
            s_uitracks[s_track_count].is_video = was_video;
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

static void *audio_thread(void *arg)
{
    (void)arg;

    while (!s_quit) {
        float decoded[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
        float resampled[4096 * 2];
        float stereo[4096 * 2];
        int   frames, outn, i;

        if (s_video_mode) {
            float vstereo[2048 * 2];
            int   vn;

            if (!s_playing) {
                usleep(10000);
                continue;
            }

            vn = video_read_audio(vstereo, 2048);
            if (vn <= 0) {
                usleep(2000);
                continue;
            }

            if (s_eq.enabled)
                eq_process(&s_eq, vstereo, vn);
            audio_apply_gain(vstereo, vn * 2, s_vol.gain);
            audio_soft_limit(vstereo, vn * 2);
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
        push_block(stereo, outn);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

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
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  return SDLK_q;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return SDLK_w;
    case SDL_CONTROLLER_BUTTON_BACK:          return SDLK_b;  /* SHARE */
    case SDL_CONTROLLER_BUTTON_START:         return SDLK_t;  /* OPTIONS */
    default:                                  return -1;
    }
}

static void stop_video(void)
{
    video_close();
    sub_close();
    refresh_sub_label();
    s_toast[0]       = '\0';
    s_video_mode     = 0;
    s_video_paused   = 0;
    s_video_settings = 0;
    s_playing        = 0;
    s_duration       = 0;
    s_format[0]      = '\0';
    snprintf(s_status, sizeof(s_status), "Stopped");
}

static void handle_key(int key)
{
    if (s_video_mode) {
        /* Any button brings the transport bar back, then does its job. */
        video_show_hud();

        /* Settings swallow input while open: the same buttons that seek must
         * not seek behind the panel. */
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
            s_video_paused = !s_video_paused;
            video_set_paused(s_video_paused);
            s_playing = s_video_paused ? 0 : 1;
            break;
        case SDLK_LEFT:
            video_seek(video_position_sec() - 10.0);
            break;
        case SDLK_RIGHT:
            video_seek(video_position_sec() + 10.0);
            break;
        case SDLK_e:
            s_video_settings = 1;
            s_settings_row   = 0;
            refresh_sub_label();
            break;
        case SDLK_t:
            toggle_subtitles();
            break;
        case SDLK_s:
        case SDLK_ESCAPE:
            stop_video();
            break;
        case SDLK_q:
            if (s_current > 0)
                play_track(s_current - 1);
            break;
        case SDLK_w:
            if (s_current + 1 < s_track_count)
                play_track(s_current + 1);
            break;
        default:
            break;
        }
        return;
    }

    if (s_show_eq) {
        if (key == SDLK_e || key == SDLK_ESCAPE) {
            s_show_eq = 0;
        } else if (key == SDLK_UP || key == SDLK_DOWN) {
            float d = (key == SDLK_UP) ? 1.0f : -1.0f;
            if (s_eq_band == EQ_BANDS)
                eq_set_preamp_db(&s_eq, s_eq.preamp_db + d);
            else
                eq_set_band_db(&s_eq, s_eq_band, s_eq.gain_db[s_eq_band] + d);
        } else if (key == SDLK_LEFT && s_eq_band > 0) {
            s_eq_band--;
        } else if (key == SDLK_RIGHT && s_eq_band < EQ_BANDS) {
            s_eq_band++;
        }
        return;
    }

    if (s_screen == UI_SCREEN_BROWSER) {
        switch (key) {
        case SDLK_UP:     browser_move(-1); break;
        case SDLK_DOWN:   browser_move(1); break;
        case SDLK_q:      browser_move(-BROWSER_PAGE); break;
        case SDLK_w:      browser_move(BROWSER_PAGE); break;
        case SDLK_x:      browser_activate(); break;
        case SDLK_s:      browser_refresh(); break;      /* Square rescan */
        case SDLK_ESCAPE:
            if (s_browser_level == 1)
                browser_go_up();
            else
                s_screen = UI_SCREEN_LIBRARY;
            break;
        case SDLK_e:      s_show_eq = 1; break;
        case SDLK_b:      s_screen = UI_SCREEN_LIBRARY; break;
        default:          break;
        }
        return;
    }

    switch (key) {
    case SDLK_b:    browser_open(); break;
    case SDLK_UP:   if (s_selected > 0) s_selected--; break;
    case SDLK_DOWN: if (s_selected + 1 < s_track_count) s_selected++; break;
    case SDLK_x:
        if (s_playing) {
            s_playing = 0;
        } else if (s_current == s_selected && s_src_open && !s_track_finished) {
            s_playing = 1;                       /* resume */
        } else {
            play_track(s_selected);
        }
        break;
    case SDLK_s:
        s_playing = 0;
        s_current = -1;
        s_format[0] = '\0';
        s_duration = 0;
        snprintf(s_status, sizeof(s_status), "Stopped");
        break;
    case SDLK_e: s_show_eq = 1; break;
    case SDLK_q: if (s_current > 0) play_track(s_current - 1); break;
    case SDLK_w: if (s_current + 1 < s_track_count) play_track(s_current + 1); break;
    case SDLK_LEFT:  vol_add_db(&s_vol, -3.0f); break;
    case SDLK_RIGHT: vol_add_db(&s_vol, +3.0f); break;
    case SDLK_ESCAPE: s_quit = 1; break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */

static void build_model(UiModel *m)
{
    int i;

    memset(m, 0, sizeof(*m));
    m->app_title   = s_app_title;
    m->tracks      = s_uitracks;
    m->track_count = s_track_count;
    m->selected    = s_selected;
    m->current     = s_current;
    m->playing     = s_playing;
    m->have_track  = (s_current >= 0 && s_format[0] != '\0');
    m->format_note = s_format;
    m->status      = s_status;
    m->duration_sec = s_duration;
    m->elapsed_sec  = (s_src_open && s_src.hz > 0)
                    ? (double)s_src.frames_decoded / s_src.hz : 0.0;
    if (m->elapsed_sec > m->duration_sec && m->duration_sec > 0)
        m->elapsed_sec = m->duration_sec;

    m->volume_db   = (int)s_vol.volume_db;
    m->volume_frac = (s_vol.volume_db + 60.0f) / 72.0f;

    m->screen         = s_screen;
    m->browser_title  = s_browser_path;
    m->browser_note   = s_browser_note;
    m->entries        = s_uientries;
    m->entry_count    = s_entry_count;
    m->entry_selected = s_entry_sel;

    m->eq_open    = s_show_eq;
    m->eq_band    = s_eq_band;
    m->eq_preamp_db = s_eq.preamp_db;
    for (i = 0; i < EQ_BANDS; i++)
        m->eq_gain_db[i] = s_eq.gain_db[i];

    m->video_aspect     = s_video_aspect;
    m->video_brightness = s_video_brightness;
    m->sub_size         = s_sub_size;
    m->sub_label        = s_sub_label;

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
         * seconds into playback and comes back on the next button press. */
        m->video_hud = (s_video_paused || s_video_settings ||
                        (Sint32)(s_video_hud_until - SDL_GetTicks()) > 0) ? 1 : 0;

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
    }
}

int main(int argc, char **argv)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_GameController *pad;
    UiModel model;
    int running = 1;
    int rc;

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    win = SDL_CreateWindow("Ultimate PS4 Media Player",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           SCREEN_W, SCREEN_H,
                           SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    pad = SDL_GameControllerOpen(0);

    vol_init(&s_vol);
    eq_init(&s_eq, OUT_RATE, 2);
    rs_init(&s_rs, 44100, OUT_RATE, 2);

    rc = aout_open(OUT_RATE, BLOCK_FRAMES);
    if (rc != 0) {
        snprintf(s_status, sizeof(s_status), "Audio out failed: 0x%08X", (unsigned)rc);
    }

    scan_all();
    if (s_track_count == 0)
        snprintf(s_status, sizeof(s_status),
                 "No media found - put files in /data/music or a USB drive");
    browser_open();                     /* start on the drive list */

    pthread_create(&s_thread, NULL, audio_thread, NULL);

    while (running && !s_quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                handle_key(ev.key.keysym.sym);
            } else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
                int key = controller_button_to_key(ev.cbutton.button);
                if (key >= 0)
                    handle_key(key);
            }
        }

        if (s_track_finished) {
            s_track_finished = 0;
            if (s_current + 1 < s_track_count)
                play_track(s_current + 1);
        }

        if (s_video_mode && video_step() <= 0) {
            stop_video();
            snprintf(s_status, sizeof(s_status), "Finished");
            if (s_current + 1 < s_track_count)
                play_track(s_current + 1);
        }

        build_model(&model);

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

        if (s_video_mode) {
            int fw = 0, fh = 0, serial = 0;
            const uint8_t *pix = video_frame(&fw, &fh, &serial);

            if (pix && fw > 0 && fh > 0) {
                if (!s_video_tex || s_video_tex_w != fw || s_video_tex_h != fh) {
                    if (s_video_tex)
                        SDL_DestroyTexture(s_video_tex);
                    s_video_tex = SDL_CreateTexture(ren,
                                                    SDL_PIXELFORMAT_ABGR8888,
                                                    SDL_TEXTUREACCESS_STREAMING,
                                                    fw, fh);
                    s_video_tex_w  = fw;
                    s_video_tex_h  = fh;
                    s_video_serial = -1;
                }
                if (s_video_tex && serial != s_video_serial) {
                    SDL_UpdateTexture(s_video_tex, NULL, pix, fw * 4);
                    s_video_serial = serial;
                }
            }
            ui_draw_video(ren, s_video_tex, fw, fh, &model, s_view_w, s_view_h);
        } else {
            ui_draw(ren, &model, s_view_w, s_view_h);
        }
        SDL_RenderPresent(ren);
        SDL_Delay(16);
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
        SDL_GameControllerClose(pad);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
