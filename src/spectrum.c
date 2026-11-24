/* spectrum.c - FFT and band extraction for the music visualizations.
 *
 * A 1024-point radix-2 FFT of the newest samples, folded into 32 log-spaced
 * bands from 40 Hz to 16 kHz. The bands are smoothed with a fast attack and a
 * slow release, which is what makes a bar meter look musical instead of
 * flickering, and the waveform is the newest stretch of the same buffer, so it
 * lines up with the beat.
 *
 * The audio thread writes, the render thread reads: one mutex around a small
 * copy, no allocation after init.
 */
#include "spectrum.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#define FFT_N   1024
#define RING    16384
#define RATE    48000.0f
#define F_LO    40.0
#define F_HI    16000.0

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static float    s_ring[RING];
static unsigned s_wpos;
static int      s_have;

static float    s_re[FFT_N], s_im[FFT_N];
static float    s_cos[FFT_N / 2], s_sin[FFT_N / 2];
static unsigned short s_rev[FFT_N];
static float    s_win[FFT_N];
static float    s_band[VIS_BANDS];
static int      s_lo[VIS_BANDS], s_hi[VIS_BANDS];
static int      s_tables;

/* ---- beat detection state ------------------------------------------------
 * All of this is written by whichever thread calls vis_fill() (one, in
 * practice: the render thread) and read by vis_beat() from the same thread.
 * The only locked field is the frame counter, which the audio thread owns. */
#define BEAT_BANDS 6        /* the bottom of the spectrum: kick and bass */
#define BEAT_FLUX_BANDS 12  /* flux over the low half only: a hat is not a beat */
#define BEAT_IVALS 8        /* intervals kept for the tempo estimate */
#define BEAT_HZ    48000.0

/* The bands are already smoothed (fast attack, slow release), which flattens
 * a kick against the music around it: a real kick peaks at about 1.3x its own
 * running average here, not the 1.4-1.5x raw energy would show. The default
 * thresholds are calibrated for the smoothed values; the config comments say
 * so, since that is where they are meant to be adjusted. */
static float    s_b_ratio  = 1.22f;   /* bass over its average = a beat */
static float    s_b_fluxr  = 1.60f;   /* rise over its average = a beat  */
static float    s_b_lock   = 0.22f;   /* seconds between two beats      */
static float    s_b_hist   = 1.00f;   /* seconds of running average     */
static float    s_b_bass_avg;
static float    s_b_flux_avg;
static float    s_b_prev[VIS_BANDS];
static float    s_b_prev_bass;
static int      s_b_seeded;
static uint64_t s_frames_pushed;      /* audio frames into the ring      */
static uint64_t s_frames_seen;        /* ... accounted for here          */
static double   s_b_t;                /* analysis time, seconds         */
static double   s_b_last;             /* time of the last beat          */
static double   s_b_ivals[BEAT_IVALS];
static int      s_b_n, s_b_head;
static long     s_b_count;
static int      s_b_pending;
static float    s_b_strength;
static float    s_b_bpm;

static void make_tables(void)
{
    int i, b;

    if (s_tables)
        return;

    for (i = 0; i < FFT_N; i++) {
        unsigned v = 0;
        unsigned x = (unsigned)i;

        for (b = 0; b < 10; b++) {      /* log2(FFT_N) = 10 */
            v = (v << 1) | (x & 1u);
            x >>= 1;
        }
        s_rev[i] = (unsigned short)v;
        /* Hann window: without it every transient smears across the spectrum */
        s_win[i] = 0.5f - 0.5f * cosf(6.28318530718f * (float)i / (float)(FFT_N - 1));
    }

    for (i = 0; i < FFT_N / 2; i++) {
        double a = -2.0 * 3.14159265358979323846 * (double)i / (double)FFT_N;

        s_cos[i] = (float)cos(a);
        s_sin[i] = (float)sin(a);
    }

    /* log-spaced band edges, computed once: f = F_LO * (F_HI/F_LO)^(b/32) */
    for (b = 0; b < VIS_BANDS; b++) {
        double f0 = F_LO * pow(F_HI / F_LO, (double)b / (double)VIS_BANDS);
        double f1 = F_LO * pow(F_HI / F_LO, (double)(b + 1) / (double)VIS_BANDS);
        int    a  = (int)(f0 * FFT_N / RATE);
        int    c  = (int)(f1 * FFT_N / RATE);

        if (a < 1)
            a = 1;
        if (c <= a)
            c = a + 1;
        if (c > FFT_N / 2)
            c = FFT_N / 2;
        s_lo[b] = a;
        s_hi[b] = c;
    }
    s_tables = 1;
}

static void fft(void)
{
    int i, len;

    for (i = 0; i < FFT_N; i++) {
        unsigned j = s_rev[i];

        if (j > (unsigned)i) {
            float tr = s_re[i], ti = s_im[i];

            s_re[i] = s_re[j]; s_im[i] = s_im[j];
            s_re[j] = tr;      s_im[j] = ti;
        }
    }

    for (len = 2; len <= FFT_N; len <<= 1) {
        int half = len >> 1;
        int step = FFT_N / len;
        int start;

        for (start = 0; start < FFT_N; start += len) {
            int k;

            for (k = 0; k < half; k++) {
                float wr = s_cos[k * step], wi = s_sin[k * step];
                int   a  = start + k, b = a + half;
                float tr = s_re[b] * wr - s_im[b] * wi;
                float ti = s_re[b] * wi + s_im[b] * wr;

                s_re[b] = s_re[a] - tr;
                s_im[b] = s_im[a] - ti;
                s_re[a] += tr;
                s_im[a] += ti;
            }
        }
    }
}

void vis_push(const float *stereo, int frames)
{
    int i;

    if (!stereo || frames <= 0)
        return;

    pthread_mutex_lock(&s_lock);
    for (i = 0; i < frames; i++) {
        float m = 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);

        s_ring[s_wpos] = m;
        s_wpos = (s_wpos + 1) & (RING - 1);
    }
    s_have = 1;
    s_frames_pushed += (uint64_t)frames;
    pthread_mutex_unlock(&s_lock);
}

/* One step of the detector, on the bands the caller has just computed. The
 * clock is the audio itself: the frames that reached the ring since the last
 * look are the seconds that passed, so a frame the app skipped does not
 * stretch a beat interval and silence is never mistaken for a long gap. */
static void beat_step(void)
{
    uint64_t pushed;
    float    bass = 0.0f, flux = 0.0f, over;
    double   dt;
    int      i;

    pthread_mutex_lock(&s_lock);
    pushed = s_frames_pushed;
    pthread_mutex_unlock(&s_lock);

    if (pushed == s_frames_seen)
        return;                        /* no new audio since the last look */
    dt = (double)(pushed - s_frames_seen) / BEAT_HZ;
    s_frames_seen = pushed;
    if (dt > 2.0)                      /* a stalled UI is not a beat     */
        dt = 2.0;
    s_b_t += dt;

    for (i = 0; i < BEAT_BANDS; i++)
        bass += s_band[i];
    bass /= (float)BEAT_BANDS;

    for (i = 0; i < VIS_BANDS; i++) {
        float d = s_band[i] - s_b_prev[i];

        /* Only a rise is flux, and only in the low half: a hi-hat is broadband
         * and would otherwise count as a beat all by itself. */
        if (i < BEAT_FLUX_BANDS && d > 0.0f)
            flux += d;
        s_b_prev[i] = s_band[i];
    }
    flux /= (float)BEAT_FLUX_BANDS;

    if (!s_b_seeded) {
        s_b_bass_avg  = bass;
        s_b_flux_avg  = flux;
        s_b_prev_bass = bass;
        s_b_seeded    = 1;
        return;
    }

    /* The average a beat is judged against: about a second of music. */
    {
        float k = (float)(dt / (s_b_hist > 0.05f ? s_b_hist : 0.05f));

        if (k > 1.0f)
            k = 1.0f;
        s_b_bass_avg += (bass - s_b_bass_avg) * k;
        s_b_flux_avg += (flux - s_b_flux_avg) * k;
    }

    /* Both votes need the bass to be *rising*: a beat is an onset, and a
     * sustained loud passage would otherwise fire on every frame it lasted. */
    over = s_b_bass_avg > 0.02f ? bass / s_b_bass_avg : 0.0f;
    {
        int   rising = bass > s_b_prev_bass + 0.015f;
        float fnorm  = s_b_flux_avg > 0.002f
                       ? flux / (s_b_flux_avg * s_b_fluxr) : 0.0f;
        int   vote_a = rising && over > s_b_ratio;
        int   vote_b = rising && fnorm > 1.0f;

        s_b_prev_bass = bass;
        /* 1.0 is the threshold whichever vote fired, so the number is
         * comparable between a kick-heavy and a mid-heavy track. */
        s_b_strength = over / s_b_ratio;
        if (fnorm > s_b_strength)
            s_b_strength = fnorm;
        if (!vote_a && !vote_b)
            return;
    }
    if (s_b_t - s_b_last < (double)s_b_lock)
        return;

    if (s_b_last > 0.0) {
        double iv = s_b_t - s_b_last;

        if (iv > 0.25 && iv < 2.0) {
            s_b_ivals[s_b_head] = iv;
            s_b_head = (s_b_head + 1) % BEAT_IVALS;
            if (s_b_n < BEAT_IVALS)
                s_b_n++;
        }
    }
    s_b_last = s_b_t;
    s_b_count++;
    s_b_pending = 1;

    /* Tempo from the median interval, so one missed or doubled beat cannot
     * move it: half the intervals are still correct on either side. */
    if (s_b_n >= 4) {
        double tmp[BEAT_IVALS];
        int    j;

        memcpy(tmp, s_b_ivals, sizeof(tmp));
        for (i = 1; i < s_b_n; i++) {
            double v = tmp[i];

            for (j = i - 1; j >= 0 && tmp[j] > v; j--)
                tmp[j + 1] = tmp[j];
            tmp[j + 1] = v;
        }
        s_b_bpm = (float)(60.0 / tmp[s_b_n / 2]);
        /* Fold into a musical range: 40 counted as 80, 240 as 120.
         * The ceiling is 160 rather than 200 because above it the detector has
         * usually counted the off-beat too, which made an 88 BPM track read
         * 175.8. The cost is that a genuine 170 BPM track reads 85. */
        while (s_b_bpm > 0.0f && s_b_bpm < 60.0f)
            s_b_bpm *= 2.0f;
        while (s_b_bpm > 160.0f)
            s_b_bpm *= 0.5f;
    }
}

void vis_fill(float *bands, int nbands, float *wave, int nwave, float *level)
{
    float window[FFT_N];
    int   i, b;

    make_tables();

    /* newest FFT_N samples, oldest first */
    pthread_mutex_lock(&s_lock);
    for (i = 0; i < FFT_N; i++) {
        unsigned idx = (s_wpos + RING - FFT_N + (unsigned)i) & (RING - 1);

        window[i] = s_have ? s_ring[idx] : 0.0f;
    }
    pthread_mutex_unlock(&s_lock);

    for (i = 0; i < FFT_N; i++) {
        s_re[i] = window[i] * s_win[i];
        s_im[i] = 0.0f;
    }
    fft();

    for (b = 0; b < VIS_BANDS; b++) {
        float mag = 0.0f;
        int   k;

        for (k = s_lo[b]; k < s_hi[b]; k++) {
            float m = sqrtf(s_re[k] * s_re[k] + s_im[k] * s_im[k]);

            if (m > mag)
                mag = m;
        }
        /* normalise to the FFT's own scale, then to a 56 dB window: quiet
         * passages still move, loud ones top out */
        {
            float db = 20.0f * log10f(mag / (FFT_N / 2) + 1e-7f);
            float v  = (db + 56.0f) / 56.0f;

            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;

            /* fast attack, slow release */
            if (v > s_band[b])
                s_band[b] = s_band[b] * 0.35f + v * 0.65f;
            else
                s_band[b] = s_band[b] * 0.86f + v * 0.14f;
        }
    }

    if (bands) {
        if (nbands > VIS_BANDS)
            nbands = VIS_BANDS;
        for (i = 0; i < nbands; i++)
            bands[i] = s_band[i];
    }

    if (wave && nwave > 0) {
        if (nwave > VIS_WAVE)
            nwave = VIS_WAVE;
        for (i = 0; i < nwave; i++) {
            int s = (int)((long)i * (FFT_N - 512) / (nwave > 1 ? nwave - 1 : 1));

            wave[i] = window[FFT_N - 512 + s];
        }
    }

    if (level) {
        float sum = 0.0f;

        for (i = 0; i < 8; i++)
            sum += s_band[i];
        *level = sum / 8.0f;
    }

    /* After the bands, so it sees this frame's spectrum. */
    beat_step();
}

void vis_beat_config(float ratio, float flux_ratio, float lockout_s,
                     float history_s)
{
    if (ratio > 1.0f && ratio < 10.0f)
        s_b_ratio = ratio;
    if (flux_ratio > 1.0f && flux_ratio < 20.0f)
        s_b_fluxr = flux_ratio;
    if (lockout_s > 0.02f && lockout_s < 3.0f)
        s_b_lock = lockout_s;
    if (history_s > 0.05f && history_s < 10.0f)
        s_b_hist = history_s;
}

void vis_beat(int *beat, float *strength, float *bpm, long *count)
{
    if (beat) {
        *beat = s_b_pending;
        s_b_pending = 0;
    }
    if (strength)
        *strength = s_b_strength;
    if (bpm)
        *bpm = s_b_bpm;
    if (count)
        *count = s_b_count;
}

const char *vis_style_name(int style)
{
    switch (style) {
    case VIS_STYLE_SPECTRUM: return "SPECTRUM";
    case VIS_STYLE_WAVE:     return "WAVEFORM";
    case VIS_STYLE_RADIAL:   return "RADIAL";
    case VIS_STYLE_PULSE:    return "PULSE";
    case VIS_STYLE_MIRROR:   return "MIRROR";
    case VIS_STYLE_BLOCKS:   return "BLOCKS";
    case VIS_STYLE_SCOPE:    return "SCOPE";
    case VIS_STYLE_ORBIT:    return "ORBIT";
    case VIS_STYLE_LIQUID:   return "LIQUID";
    case VIS_STYLE_NEON:     return "NEON";
    case VIS_STYLE_FLAG:     return "FLAG";
    case VIS_STYLE_DANCE:    return "DANCE";
    case VIS_STYLE_WORM:     return "WORM";
    case VIS_STYLE_WINDMILL: return "WINDMILL";
    case VIS_STYLE_DANCE2:   return "DANCE 2";
    case VIS_STYLE_DANCE3:   return "DANCE 3";
    case VIS_STYLE_RAVE:     return "RAVE DJ";
    case VIS_STYLE_TESSERACT: return "TESSERACT";
    default:                 return "SPECTRUM";
    }
}
