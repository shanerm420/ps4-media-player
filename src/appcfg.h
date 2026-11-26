/* appcfg.h - the one configuration file.
 *
 * Every setting the app keeps between runs lives in one plain-text file,
 *
 *     /data/ultimate-ps4-media-player/config.ini
 *
 * with a section per area ([player], [audio], [ui], [quality], [log], [rave],
 * [liquid]). appcfg.c writes a complete commented file on first run and reads
 * it back by name, so an old value can never be read as a new one, and a line
 * the parser does not know is a warning in the log instead of a silent fail.
 *
 * The visualizer settings are in here too: vis_art.c reads RV_* and FL_* out of
 * `cfg`, which start as the compiled-in defaults and are then whatever the file
 * says, so a look can be tuned without a rebuild.
 */
#ifndef APP_CFG_H
#define APP_CFG_H

#include <stddef.h>

typedef struct {
    double volume_db;      /* -60 .. +12 */
    int    muted;
} CfgPlayer;

typedef struct {
    int    eq_enabled;
    int    eq_preset;      /* 0..7, see EQ_PRESET_* */
    double preamp_db;
    double gain_db[10];    /* the ten bands, low to high */

    /* beat detection (see spectrum.c): what counts as a beat, how close two
     * beats may be, and how long the average it is judged against runs */
    double beat_ratio;
    double beat_flux_ratio;
    double beat_lockout_ms;
    double beat_history_s;
} CfgAudio;

typedef struct {
    int theme;
    int visualizer;        /* VIS_STYLE_* */
    int visualizer_full;

    /* video playback */
    int video_aspect;
    int video_brightness;
    int video_saturation;
    int video_sharpness;
    int subtitle_size;

    /* picture viewer (kept apart from the video grade, as before) */
    int picture_aspect;
    int picture_brightness;
    int picture_saturation;
    int picture_sharpness;
    int picture_rotation;
    int zoom;
    int color_order;       /* 0 normal, 1 red/blue swapped */
} CfgUi;

typedef struct {
    int    max_step;          /* 1..3 */
    int    tier;              /* 0 auto, 1 base, 2 pro - see cfg_apply_tier() */
    double budget_step1_ms;   /* a frame over this drops to step 2 */
    double budget_ms;         /* ... and this drops to step 3 */
    double settle_seconds;    /* stay at a dropped step this long */
    int    soft_quarter;      /* 1 = fog and light pools at a quarter of the canvas */
} CfgQuality;

typedef struct {
    int measure_level;        /* 0 off, 1 summary, 2 detailed */
    int network_status;       /* 1 = log the connection state */
    int network_poll_seconds; /* how often it is checked */
} CfgLog;

/* The club (VIS_STYLE_RAVE). Counts are people and motes, the rest are gains
 * where 1.0 is the tuned look and 0.0 turns the layer off. */
typedef struct {
    int    crowd_back, crowd_mid, crowd_front, near_pit;
    int    fleck, dust;

    double haze, fog, bloom_gain, rim, shake, floor_gloss;
    double laser_speed, laser_width, wall_y;

    double reflect, reflect_len, reflect_squash, reflect_blur, reflect_tint;
    double pool_gain, pool_tint;
    double fog_knee, fog_back;
    double contact, shadow_side, wall_dim;
} CfgRave;

/* The water tank (VIS_STYLE_LIQUID): physics constants first, then the look. */
typedef struct {
    double damp, break_slope, shake_mul, wall_delay, self_limit;
    double gain, gain_level, crest_m;
    double glow, sheen, reflect;
    double chop_mid, chop_high, wave_wobble, foam;
    double air, surface_lift, floor_y, glass;
} CfgLiquid;

typedef struct {
    CfgPlayer  player;
    CfgAudio   audio;
    CfgUi      ui;
    CfgQuality quality;
    CfgLog     log;
    CfgRave    rave;
    CfgLiquid  liquid;
} AppCfg;

extern AppCfg cfg;

/* Fills cfg with the compiled-in defaults. Idempotent, and called by cfg_load()
 * and by anything that reads cfg before a load (the PC preview). */
void cfg_ready(void);

/* Reads config.ini, creating it on first run and migrating the old eq.cfg /
 * picture.cfg / viewer.cfg once. Always leaves a usable cfg: a missing or
 * broken value falls back to its default with a warning in the log. Returns 0
 * when the file was read, 1 when it was created. */
int cfg_load(void);

/* Applies the stated hardware tier to the quality budgets. tier 0 leaves the
 * ladder free to measure this console itself; 1 states base PS4; 2 states PS4
 * Pro and tightens the budgets so the extra headroom is spent on the picture. */
void cfg_apply_tier(void);

/* Writes the whole file back with the current values. Returns 0 on success. */
int cfg_save(void);

const char *cfg_path(void);

/* One INFO line naming the values that are not at their defaults. */
void cfg_log_diffs(void);

#endif /* APP_CFG_H */
