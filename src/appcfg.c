/* appcfg.c - reads and writes config.ini, the app's one settings file.
 *
 * The parser is deliberately small: lines of "key = value", sections in
 * brackets, '#' or ';' starts a comment. There is no dependency, no nesting and
 * no escaping. The one table below is the whole schema - it holds the default,
 * the accepted range and the comment that is written into the file - so a new
 * setting is one line here and one field in appcfg.h, and the file, the
 * defaults and the validation can never drift apart.
 *
 * Nothing here is fatal. A value that cannot be read is replaced by its default
 * and reported with its line number; a key the schema does not know is reported
 * once; a completely unreadable file means the defaults, and the next save will
 * write a good one.
 */
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "appcfg.h"
#include "applog.h"

#define CFG_DIR   "/data/ultimate-ps4-media-player"
#define CFG_FILE  CFG_DIR "/config.ini"

/* The three files this replaces. They are read once, when config.ini does not
 * exist yet, and never again. */
#define EQ_OLD    CFG_DIR "/eq.cfg"
#define PIC_OLD   CFG_DIR "/picture.cfg"
#define VIEW_OLD  CFG_DIR "/viewer.cfg"

AppCfg cfg;
static int s_ready;

typedef struct {
    const char *sec;
    const char *key;
    int         is_double;
    size_t      off;         /* byte offset of the field in AppCfg */
    int         idx;         /* element, for the ten EQ gains */
    double      def, lo, hi;
    const char *comment;
} CfgKey;

#define KI(s, k, f, d, lo, hi, c) \
    { s, k, 0, offsetof(AppCfg, f), 0, d, lo, hi, c }
#define KD(s, k, f, d, lo, hi, c) \
    { s, k, 1, offsetof(AppCfg, f), 0, d, lo, hi, c }
#define KDA(s, k, f, i, d, lo, hi, c) \
    { s, k, 1, offsetof(AppCfg, f), i, d, lo, hi, c }

static const CfgKey k_keys[] = {
/* ---- [player] ------------------------------------------------------------ */
    KI("player", "volume_db", player.volume_db, 0.0, -60.0, 12.0,
       "volume in dB"),
    KI("player", "muted", player.muted, 0, 0, 1,
       "1 = sound off, 0 = playing"),

/* ---- [audio] ------------------------------------------------------------- */
    KI("audio", "eq_enabled", audio.eq_enabled, 1, 0, 1,
       "1 = equaliser in the chain, 0 = flat"),
    KI("audio", "eq_preset", audio.eq_preset, 0, 0, 7,
       "0 FLAT, 1 BASS, 2 VOCAL, 3 ROCK, 4 POP, 5 JAZZ, 6 LOUD, 7 CUSTOM"),
    KD("audio", "preamp_db", audio.preamp_db, 0.0, -24.0, 12.0,
       "preamp in dB, applied before the bands"),
    KDA("audio", "gain_1",  audio.gain_db, 0, 0.0, -12.0, 12.0, "band 1 gain, dB (60 Hz)"),
    KDA("audio", "gain_2",  audio.gain_db, 1, 0.0, -12.0, 12.0, "band 2 gain, dB (120 Hz)"),
    KDA("audio", "gain_3",  audio.gain_db, 2, 0.0, -12.0, 12.0, "band 3 gain, dB (250 Hz)"),
    KDA("audio", "gain_4",  audio.gain_db, 3, 0.0, -12.0, 12.0, "band 4 gain, dB (500 Hz)"),
    KDA("audio", "gain_5",  audio.gain_db, 4, 0.0, -12.0, 12.0, "band 5 gain, dB (1 kHz)"),
    KDA("audio", "gain_6",  audio.gain_db, 5, 0.0, -12.0, 12.0, "band 6 gain, dB (2 kHz)"),
    KDA("audio", "gain_7",  audio.gain_db, 6, 0.0, -12.0, 12.0, "band 7 gain, dB (4 kHz)"),
    KDA("audio", "gain_8",  audio.gain_db, 7, 0.0, -12.0, 12.0, "band 8 gain, dB (6 kHz)"),
    KDA("audio", "gain_9",  audio.gain_db, 8, 0.0, -12.0, 12.0, "band 9 gain, dB (8 kHz)"),
    KDA("audio", "gain_10", audio.gain_db, 9, 0.0, -12.0, 12.0, "band 10 gain, dB (16 kHz)"),
    KD("audio", "beat_ratio", audio.beat_ratio, 1.22, 1.02, 4.0,
       "bass over its running average that counts as a beat; the bands are smoothed, so 1.2 here is about 1.45 on raw energy"),
    KD("audio", "beat_flux_ratio", audio.beat_flux_ratio, 1.60, 1.05, 6.0,
       "rise across the low half of the spectrum that also counts as a beat"),
    KD("audio", "beat_lockout_ms", audio.beat_lockout_ms, 220.0, 60.0, 1000.0,
       "minimum gap between two beats, ms"),
    KD("audio", "beat_history_s", audio.beat_history_s, 1.00, 0.10, 5.0,
       "length of the running average a beat is judged against, seconds"),

/* ---- [ui] ---------------------------------------------------------------- */
    KI("ui", "theme", ui.theme, 0, 0, 15,
       "skin index, SQUARE on the home screen"),
    KI("ui", "visualizer", ui.visualizer, 8, 0, 17,
       "style index, TRIANGLE on the music screen (8 = LIQUID, 16 = RAVE)"),
    KI("ui", "visualizer_full", ui.visualizer_full, 0, 0, 1,
       "1 = the visualizer fills the screen at start"),
    KI("ui", "video_aspect", ui.video_aspect, 0, 0, 2,
       "video fit: 0 FIT, 1 FILL, 2 STRETCH"),
    KI("ui", "video_brightness", ui.video_brightness, 0, -100, 100,
       "video brightness, -100..100"),
    KI("ui", "video_saturation", ui.video_saturation, 100, 0, 250,
       "video colour, percent"),
    KI("ui", "video_sharpness", ui.video_sharpness, 0, 0, 100,
       "video sharpening, 0..100"),
    KI("ui", "subtitle_size", ui.subtitle_size, 1, 0, 2,
       "subtitles: 0 SMALL, 1 NORMAL, 2 LARGE"),
    KI("ui", "picture_aspect", ui.picture_aspect, 0, 0, 2,
       "photo fit: 0 FIT, 1 FILL, 2 STRETCH"),
    KI("ui", "picture_brightness", ui.picture_brightness, 0, -100, 100,
       "photo brightness, -100..100"),
    KI("ui", "picture_saturation", ui.picture_saturation, 100, 0, 250,
       "photo colour, percent"),
    KI("ui", "picture_sharpness", ui.picture_sharpness, 0, 0, 100,
       "photo sharpening, 0..100"),
    KI("ui", "picture_rotation", ui.picture_rotation, 0, 0, 3,
       "photo rotation, quarter turns"),
    KI("ui", "zoom", ui.zoom, 100, 100, 400,
       "photo zoom, percent"),
    KI("ui", "color_order", ui.color_order, 1, 0, 1,
       "photo colour order: 1 = red and blue swapped, 0 = normal"),

/* ---- [quality] ----------------------------------------------------------- */
    KI("quality", "max_step", quality.max_step, 3, 1, 3,
       "3 = allow half and third resolution, 1 = always full"),
    KD("quality", "budget_step1_ms", quality.budget_step1_ms, 56.0, 10.0, 400.0,
       "a frame at full resolution over this drops one step"),
    KD("quality", "budget_ms", quality.budget_ms, 88.0, 10.0, 400.0,
       "a frame at step 2 over this drops to step 3"),
    KD("quality", "settle_seconds", quality.settle_seconds, 5.0, 0.0, 60.0,
       "how long a dropped step is held before it may climb back"),
    KI("quality", "soft_quarter", quality.soft_quarter, 0, 0, 1,
       "fog and light pools at a quarter of the canvas; measured slower on the host, so 0 is the default"),

/* ---- [log] --------------------------------------------------------------- */
    KI("log", "measure_level", log.measure_level, 1, 0, 2,
       "0 off, 1 summary (one line a second), 2 detailed (every phase)"),
    KI("log", "network_status", log.network_status, 1, 0, 1,
       "1 = log the connection state at startup and when it changes"),
    KI("log", "network_poll_seconds", log.network_poll_seconds, 20, 10, 120,
       "how often the connection state is checked, seconds"),

/* ---- [rave] -------------------------------------------------------------- */
    KI("rave", "crowd_back", rave.crowd_back, 14, 0, 40,
       "people in the far rank: small, dark, hazed"),
    KI("rave", "crowd_mid", rave.crowd_mid, 12, 0, 40,
       "people in the middle rank"),
    KI("rave", "crowd_front", rave.crowd_front, 11, 0, 40,
       "people in the front rank: largest, sharpest"),
    KI("rave", "near_pit", rave.near_pit, 3, 0, 16,
       "out-of-focus people against the lens"),
    KI("rave", "fleck", rave.fleck, 16, 0, 64,
       "disco-ball flecks on the walls and the crowd"),
    KI("rave", "dust", rave.dust, 24, 0, 128,
       "dust motes hanging in the beams"),
    KD("rave", "haze", rave.haze, 1.00, 0.0, 2.0,
       "laser haze and smoke; 0 = off"),
    KD("rave", "fog", rave.fog, 1.00, 0.0, 2.0,
       "low fog drifting over the floor; 0 = off"),
    KD("rave", "bloom_gain", rave.bloom_gain, 1.30, 0.0, 3.0,
       "extra bloom the club asks for in the finish pass"),
    KD("rave", "rim", rave.rim, 1.00, 0.0, 3.0,
       "rim light on the figures and the DJ"),
    KD("rave", "shake", rave.shake, 1.00, 0.0, 3.0,
       "bass shake; 0 = steady camera"),
    KD("rave", "floor_gloss", rave.floor_gloss, 0.55, 0.0, 1.5,
       "how hard the polished floor mirrors the room"),
    KD("rave", "laser_speed", rave.laser_speed, 1.00, 0.0, 4.0,
       "how fast the beams sweep"),
    KD("rave", "laser_width", rave.laser_width, 1.00, 0.2, 3.0,
       "how wide the beams open up"),
    KD("rave", "wall_y", rave.wall_y, 0.665, 0.30, 0.95,
       "where the wall meets the floor, as a frame fraction"),
    KD("rave", "reflect", rave.reflect, 0.78, 0.0, 1.0,
       "strength of the floor mirror; 0 = off"),
    KD("rave", "reflect_len", rave.reflect_len, 0.076, 0.0, 0.4,
       "how far a reflection runs below the feet"),
    KD("rave", "reflect_squash", rave.reflect_squash, 0.92, 0.1, 1.0,
       "vertical compression of the mirror"),
    KD("rave", "reflect_blur", rave.reflect_blur, 2.0, 0.0, 12.0,
       "horizontal blur radius of the mirror, px at 1920 wide"),
    KD("rave", "reflect_tint", rave.reflect_tint, 0.22, 0.0, 1.0,
       "how much the nearest light colours the mirror"),
    KD("rave", "pool_gain", rave.pool_gain, 1.00, 0.0, 3.0,
       "additive floor pools under every beam"),
    KD("rave", "pool_tint", rave.pool_tint, 0.65, 0.0, 1.0,
       "how hard a pool lights the legs standing in it"),
    KD("rave", "fog_knee", rave.fog_knee, 1.00, 0.0, 2.0,
       "knee-height fog thickness; 0 = off"),
    KD("rave", "fog_back", rave.fog_back, 1.45, 0.0, 4.0,
       "back rows are this much thicker than near ones"),
    KD("rave", "contact", rave.contact, 1.00, 0.0, 2.0,
       "contact shadow under every foot"),
    KD("rave", "shadow_side", rave.shadow_side, 0.55, 0.0, 1.0,
       "how hard the unlit side of a body goes"),
    KD("rave", "wall_dim", rave.wall_dim, 0.84, 0.0, 1.0,
       "spectral wall brightness against the DJ"),

/* ---- [liquid] ------------------------------------------------------------ */
    KD("liquid", "damp", liquid.damp, 0.999945, 0.99, 1.0,
       "velocity kept per 1/480 s substep"),
    KD("liquid", "break_slope", liquid.break_slope, 0.55, 0.05, 4.0,
       "steepness at which a crest spills"),
    KD("liquid", "shake_mul", liquid.shake_mul, 2.40, 0.0, 10.0,
       "how hard the music shakes the tank"),
    KD("liquid", "wall_delay", liquid.wall_delay, 0.055, 0.0, 0.5,
       "beat fraction the right-hand stack lags by"),
    KD("liquid", "self_limit", liquid.self_limit, 30.0, 0.0, 200.0,
       "how hard a big tank damps its own drive"),
    KD("liquid", "gain", liquid.gain, 2.20, 0.0, 12.0,
       "px per metre of surface height"),
    KD("liquid", "gain_level", liquid.gain_level, 1.60, 0.0, 12.0,
       "extra px per metre with the level"),
    KD("liquid", "crest_m", liquid.crest_m, 0.012, 0.0005, 0.2,
       "metres of height that count as a full crest"),
    KD("liquid", "glow", liquid.glow, 1.00, 0.0, 3.0,
       "light filling the gaps between the shafts"),
    KD("liquid", "sheen", liquid.sheen, 52.0, 0.0, 200.0,
       "sheen just under the surface"),
    KD("liquid", "reflect", liquid.reflect, 1.00, 0.0, 3.0,
       "the mirrored glow under the surface"),
    KD("liquid", "chop_mid", liquid.chop_mid, 1.00, 0.0, 3.0,
       "mid bands -> medium chop"),
    KD("liquid", "chop_high", liquid.chop_high, 1.00, 0.0, 3.0,
       "high bands -> fine shimmer"),
    KD("liquid", "wave_wobble", liquid.wave_wobble, 1.00, 0.0, 3.0,
       "the waveform -> a slow surface wobble"),
    KD("liquid", "foam", liquid.foam, 1.00, 0.0, 3.0,
       "white water"),
    KD("liquid", "air", liquid.air, 0.32, 0.05, 0.95,
       "the water line, as a frame fraction"),
    KD("liquid", "surface_lift", liquid.surface_lift, 0.08, 0.0, 0.30,
       "how far the water line rises above air, as a frame fraction"),
    KD("liquid", "floor_y", liquid.floor_y, 0.86, 0.05, 0.99,
       "the tank's floor, as a frame fraction"),
    KD("liquid", "glass", liquid.glass, 1.00, 0.0, 3.0,
       "side walls, front glass and floor lines"),
};

#define KEY_COUNT ((int)(sizeof(k_keys) / sizeof(k_keys[0])))

const char *cfg_path(void)
{
    return CFG_FILE;
}

static double *dptr(const CfgKey *k)
{
    return (double *)((char *)&cfg + k->off + (size_t)k->idx * sizeof(double));
}

static int *iptr(const CfgKey *k)
{
    return (int *)((char *)&cfg + k->off);
}

static double value_of(const CfgKey *k)
{
    return k->is_double ? *dptr(k) : (double)*iptr(k);
}

static void store(const CfgKey *k, double v)
{
    if (k->is_double)
        *dptr(k) = v;
    else
        *iptr(k) = (int)(v < 0.0 ? v - 0.5 : v + 0.5);
}

void cfg_ready(void)
{
    int i;

    if (s_ready)
        return;
    s_ready = 1;
    /* The table owns the defaults: one place to change a value, and the file,
     * the comments and the fallback all follow. */
    for (i = 0; i < KEY_COUNT; i++)
        store(&k_keys[i], k_keys[i].def);
}

/* ---------------------------------------------------------------- parsing -- */

static char *trim(char *s)
{
    char *e;

    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' ||
                     e[-1] == '\r'))
        e--;
    *e = '\0';
    return s;
}

static int key_index(const char *sec, const char *key)
{
    int i;

    for (i = 0; i < KEY_COUNT; i++)
        if (strcasecmp(k_keys[i].sec, sec) == 0 &&
            strcasecmp(k_keys[i].key, key) == 0)
            return i;
    return -1;
}

/* "off" / "summary" / "detailed" are friendlier than 0/1/2 for the one setting
 * that has named values; everything else is a number. */
static int parse_value(const char *sec, const char *key, const char *text,
                       double *out)
{
    char  *end;
    double v;

    if (strcasecmp(sec, "log") == 0 && strcasecmp(key, "measure_level") == 0) {
        if (strcasecmp(text, "off") == 0)      { *out = 0; return 1; }
        if (strcasecmp(text, "summary") == 0)  { *out = 1; return 1; }
        if (strcasecmp(text, "detailed") == 0) { *out = 2; return 1; }
    }
    v = strtod(text, &end);
    if (end == text || *end != '\0')
        return 0;
    *out = v;
    return 1;
}

/* -------------------------------------------------------------- migration -- */

/* The three files the app used before, read once. Their layouts are fixed
 * binary records, so each one is accepted only at an exact size. */
static unsigned try_migrate(void)
{
    unsigned  read_mask = 0;
    struct stat st;
    FILE *f;

    /* The sizes are written by hand, not computed from the current structs:
     * they are what v55 and later actually wrote to disk. */
    if (stat(EQ_OLD, &st) == 0 && st.st_size == (off_t)(10 * 4 + 4 + 4)) {
        float gains[10], pre = 0.0f;
        int   preset = -1;

        f = fopen(EQ_OLD, "rb");
        if (f) {
            if (fread(gains, sizeof(gains), 1, f) == 1 &&
                fread(&pre, sizeof(pre), 1, f) == 1) {
                int b;

                for (b = 0; b < 10; b++)
                    cfg.audio.gain_db[b] = (double)gains[b];
                cfg.audio.preamp_db = (double)pre;
                if (fread(&preset, sizeof(preset), 1, f) == 1 &&
                    preset >= 0 && preset <= 7)
                    cfg.audio.eq_preset = preset;
                read_mask |= 1u;
            }
            fclose(f);
        }
    }

    if (stat(PIC_OLD, &st) == 0 && st.st_size == (off_t)(5 * sizeof(int))) {
        int v[5];

        f = fopen(PIC_OLD, "rb");
        if (f) {
            if (fread(v, sizeof(v), 1, f) == 1) {
                cfg.ui.video_aspect     = v[0];
                cfg.ui.video_brightness = v[1];
                cfg.ui.video_saturation = v[2];
                cfg.ui.video_sharpness  = v[3];
                cfg.ui.subtitle_size    = v[4];
                read_mask |= 2u;
            }
            fclose(f);
        }
    }

    if (stat(VIEW_OLD, &st) == 0 &&
        (st.st_size == (off_t)(7 * sizeof(int)) ||
         st.st_size == (off_t)(8 * sizeof(int)) ||
         st.st_size == (off_t)(11 * sizeof(int)))) {
        int    v[11];
        size_t n;

        memset(v, 0, sizeof(v));
        f = fopen(VIEW_OLD, "rb");
        if (f) {
            n = fread(v, sizeof(v[0]), 11, f);
            fclose(f);
            if (n >= 7) {
                cfg.ui.picture_aspect     = v[0];
                cfg.ui.picture_brightness = v[1];
                cfg.ui.picture_saturation = v[2];
                cfg.ui.picture_sharpness  = v[3];
                cfg.ui.picture_rotation   = v[4];
                cfg.ui.visualizer         = v[5];
                cfg.ui.zoom               = v[6];
                if (n >= 8)
                    cfg.ui.theme = v[7];
                if (n >= 11)
                    cfg.ui.color_order = v[10] ? 1 : 0;
                read_mask |= 4u;
            }
        }
    }
    return read_mask;
}

/* ------------------------------------------------------------------- load -- */

int cfg_load(void)
{
    FILE *f;
    char  buf[512];
    char  sec[32] = "";
    int   lineno = 0, created = 0;
    int   seen[KEY_COUNT];
    int   missing = 0, bad = 0, unknown = 0;
    char  missing_names[192] = "";

    cfg_ready();

    f = fopen(CFG_FILE, "rb");
    if (!f) {
        unsigned migrated;

        memset(seen, 0, sizeof(seen));
        migrated = try_migrate();
        created  = 1;
        if (cfg_save() == 0) {
            if (migrated)
                log_msg(LOG_INFO, "config",
                        "created %s, values carried over from %s%s%s",
                        CFG_FILE,
                        (migrated & 1u) ? "eq.cfg " : "",
                        (migrated & 2u) ? "picture.cfg " : "",
                        (migrated & 4u) ? "viewer.cfg" : "");
            else
                log_msg(LOG_INFO, "config", "created %s with defaults", CFG_FILE);
        } else {
            log_msg(LOG_WARN, "config",
                    "could not write %s - running on defaults", CFG_FILE);
        }
        (void)seen;
        (void)missing_names;
        return 1;
    }

    memset(seen, 0, sizeof(seen));
    while (fgets(buf, sizeof(buf), f)) {
        char *line, *eq, *key, *val;
        int   ki;

        lineno++;
        line = trim(buf);
        if (!*line || *line == '#' || *line == ';')
            continue;

        if (*line == '[') {
            char *close = strchr(line, ']');

            if (!close) {
                log_msg(LOG_WARN, "config", "line %d: unterminated section",
                        lineno);
                continue;
            }
            *close = '\0';
            snprintf(sec, sizeof(sec), "%s", trim(line + 1));
            continue;
        }

        eq = strchr(line, '=');
        if (!eq) {
            log_msg(LOG_WARN, "config", "line %d: not 'key = value'", lineno);
            bad++;
            continue;
        }
        *eq  = '\0';
        key  = trim(line);
        val  = trim(eq + 1);
        ki   = key_index(sec, key);
        if (ki < 0) {
            if (unknown < 10)
                log_msg(LOG_WARN, "config", "line %d: unknown key [%s] %s",
                        lineno, sec, key);
            unknown++;
            continue;
        }
        {
            double v;

            if (!parse_value(sec, key, val, &v)) {
                log_msg(LOG_WARN, "config",
                        "line %d: [%s] %s = '%s' is not a number, using %g",
                        lineno, sec, key, val, k_keys[ki].def);
                bad++;
                continue;
            }
            if (v < k_keys[ki].lo || v > k_keys[ki].hi) {
                double clamped = v < k_keys[ki].lo ? k_keys[ki].lo : k_keys[ki].hi;

                log_msg(LOG_WARN, "config",
                        "line %d: [%s] %s = %g is outside %g..%g, using %g",
                        lineno, sec, key, v, k_keys[ki].lo, k_keys[ki].hi,
                        clamped);
                v = clamped;
                bad++;
            }
            store(&k_keys[ki], v);
            seen[ki] = 1;
        }
    }
    fclose(f);

    for (int i = 0; i < KEY_COUNT; i++) {
        if (seen[i])
            continue;
        missing++;
        if (strlen(missing_names) < sizeof(missing_names) - 24)
            snprintf(missing_names + strlen(missing_names),
                     sizeof(missing_names) - strlen(missing_names),
                     "%s%s.%s", missing_names[0] ? " " : "",
                     k_keys[i].sec, k_keys[i].key);
    }
    if (missing)
        log_msg(LOG_WARN, "config", "%d key%s missing, defaults used: %s",
                missing, missing == 1 ? "" : "s", missing_names);
    if (bad)
        log_msg(LOG_WARN, "config", "%d value%s replaced", bad,
                bad == 1 ? "" : "s");
    if (unknown)
        log_msg(LOG_WARN, "config", "%d unknown key%s ignored", unknown,
                unknown == 1 ? "" : "s");
    log_msg(LOG_INFO, "config", "loaded %s (%d keys, %d missing, %d bad, %d unknown)",
            CFG_FILE, KEY_COUNT, missing, bad, unknown);
    (void)created;
    return 0;
}

/* ------------------------------------------------------------------- save -- */

static void write_value(FILE *f, const CfgKey *k)
{
    double v = value_of(k);

    if (k->is_double)
        fprintf(f, "%s = %.6g\n", k->key, v);
    else
        fprintf(f, "%s = %d\n", k->key, (int)v);
}

int cfg_save(void)
{
    FILE       *f;
    const char *sec = NULL;
    int         i;

    mkdir(CFG_DIR, 0777);
    f = fopen(CFG_FILE, "w");
    if (!f)
        return -1;

    fprintf(f,
        "; Ultimate PS4 Media Player - all settings, in one file.\n"
        "; Edit and save, then restart the app (or it reloads when you open\n"
        "; the equaliser screen). A missing or impossible value falls back to\n"
        "; the default shown in its comment and is reported in app.log.\n"
        "; Numbers only, except measure_level = off | summary | detailed.\n");

    for (i = 0; i < KEY_COUNT; i++) {
        if (!sec || strcmp(sec, k_keys[i].sec) != 0) {
            sec = k_keys[i].sec;
            fprintf(f, "\n[%s]\n", sec);
        }
        fprintf(f, "# %s (default %.6g)\n", k_keys[i].comment, k_keys[i].def);
        write_value(f, &k_keys[i]);
    }
    fclose(f);
    return 0;
}

void cfg_log_diffs(void)
{
    char list[256] = "";
    int  n = 0, i;

    for (i = 0; i < KEY_COUNT; i++) {
        double v = value_of(&k_keys[i]);

        if (v == k_keys[i].def)
            continue;
        n++;
        if (n <= 6) {
            size_t at = strlen(list);

            if (at < sizeof(list) - 48)
                snprintf(list + at, sizeof(list) - at, "%s%s.%s=%g",
                         list[0] ? " " : "", k_keys[i].sec, k_keys[i].key, v);
        }
    }
    if (n == 0)
        log_msg(LOG_INFO, "config", "every value is at its default");
    else
        log_msg(LOG_INFO, "config", "%d value%s away from default: %s%s", n,
                n == 1 ? "" : "s", list, n > 6 ? " ..." : "");
}
