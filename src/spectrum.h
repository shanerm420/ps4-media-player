/* spectrum.h - audio analysis for the music visualizations.
 *
 * The audio thread pushes the samples it is about to hear; the UI asks for a
 * band spectrum and a waveform once a frame. Everything is in sync with the
 * music by construction: the data is the music.
 */
#ifndef SPECTRUM_H
#define SPECTRUM_H

#define VIS_BANDS 32
#define VIS_WAVE  160

#define VIS_STYLE_SPECTRUM 0
#define VIS_STYLE_WAVE     1
#define VIS_STYLE_RADIAL   2
#define VIS_STYLE_PULSE    3
#define VIS_STYLE_MIRROR   4
#define VIS_STYLE_BLOCKS   5
#define VIS_STYLE_SCOPE    6
#define VIS_STYLE_ORBIT    7
#define VIS_STYLE_LIQUID   8   /* shallow water tank in glass, driven by the music */
#define VIS_STYLE_NEON     9   /* the icon's blue neon tube */
#define VIS_STYLE_FLAG    10   /* stars and stripes, lit by the music */
#define VIS_STYLE_DANCE   11   /* hip-hop dancer, painted */
#define VIS_STYLE_WORM    12   /* the worm, painted */
#define VIS_STYLE_WINDMILL 13  /* the windmill, painted */
#define VIS_STYLE_DANCE2  14   /* robot dancer */
#define VIS_STYLE_DANCE3  15   /* spinning dancer */
#define VIS_STYLE_RAVE    16   /* DJ seen from behind, crowd in front */
#define VIS_STYLE_TESSERACT 17 /* cage that blooms, emblem trapped in it */
#define VIS_STYLE_COUNT   18

/* Called from the audio thread with interleaved stereo at 48 kHz. */
void vis_push(const float *stereo, int frames);

/* Copies up to VIS_BANDS band levels (0..1) and VIS_WAVE waveform samples
 * (-1..1), plus the overall level (0..1). Safe to call every frame. */
void vis_fill(float *bands, int nbands, float *wave, int nwave, float *level);

const char *vis_style_name(int style);

/* ---- beat ---------------------------------------------------------------
 * A cheap detector on the bands vis_fill() already computes: bass energy
 * against its own running average, with the rise across the low half of the
 * spectrum as a second vote. It runs inside vis_fill(), costs one pass over 32 floats, and
 * changes nothing that already reads these bands - a visualizer opts in by
 * calling vis_beat(). */

/* Thresholds, in the order they appear in config.ini. Defaults are the values
 * below, so a caller that never configures it still gets working detection. */
void vis_beat_config(float ratio, float flux_ratio, float lockout_s,
                     float history_s);

/* beat is cleared by the call: 1 means a beat landed since the last read.
 * strength is how far over the threshold the strongest of the two signals is at
 * this moment, so 1.0 means "a beat is happening" and 2.0 is twice that. bpm
 * is 0 until there are enough intervals to judge, and
 * count is the number of beats since the app started. Any pointer may be NULL. */
void vis_beat(int *beat, float *strength, float *bpm, long *count);

#endif /* SPECTRUM_H */
