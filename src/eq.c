#include "eq.h"

#include <math.h>
#include <string.h>

#define EQ_PI 3.14159265358979323846f
#define EQ_Q  1.41f          /* ~1 octave bandwidth, matches 10 octave bands */
#define EQ_MAX_DB 12.0f

static const float k_band_hz[EQ_BANDS] = {
    31.0f, 62.0f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

float eq_band_hz(int band)
{
    if (band < 0 || band >= EQ_BANDS)
        return 0.0f;
    return k_band_hz[band];
}

static void eq_identity(EqBiquad *c)
{
    c->b0 = 1.0f;
    c->b1 = c->b2 = c->a1 = c->a2 = 0.0f;
}

static void eq_recompute(Eq *eq, int band)
{
    EqBiquad *c = &eq->coef[band];
    float f = k_band_hz[band];
    float db = eq->gain_db[band];

    /* A band that no longer fits under Nyquist passes through untouched. */
    if (eq->sample_rate <= 0 || f <= 0.0f || f >= eq->sample_rate * 0.49f) {
        eq_identity(c);
        return;
    }
    if (db > EQ_MAX_DB)  db = EQ_MAX_DB;
    if (db < -EQ_MAX_DB) db = -EQ_MAX_DB;

    {
        float A     = powf(10.0f, db / 40.0f);
        float w0    = 2.0f * EQ_PI * f / (float)eq->sample_rate;
        float cosw  = cosf(w0);
        float alpha = sinf(w0) / (2.0f * EQ_Q);
        float a0    = 1.0f + alpha / A;

        c->b0 = (1.0f + alpha * A) / a0;
        c->b1 = (-2.0f * cosw) / a0;
        c->b2 = (1.0f - alpha * A) / a0;
        c->a1 = (-2.0f * cosw) / a0;
        c->a2 = (1.0f - alpha / A) / a0;
    }
}

void eq_init(Eq *eq, int sample_rate, int channels)
{
    int b;
    memset(eq, 0, sizeof(*eq));
    eq->sample_rate = sample_rate;
    eq->channels    = (channels == 2) ? 2 : 1;
    eq->enabled     = 1;
    eq->preamp_db   = 0.0f;
    for (b = 0; b < EQ_BANDS; b++) {
        eq->gain_db[b] = 0.0f;
        eq_recompute(eq, b);
    }
}

void eq_set_band_db(Eq *eq, int band, float db)
{
    if (band < 0 || band >= EQ_BANDS)
        return;
    eq->gain_db[band] = db;
    eq_recompute(eq, band);
}

void eq_set_preamp_db(Eq *eq, float db)
{
    if (db > EQ_MAX_DB)  db = EQ_MAX_DB;
    if (db < -EQ_MAX_DB) db = -EQ_MAX_DB;
    eq->preamp_db = db;
}

void eq_reset(Eq *eq)
{
    memset(eq->z, 0, sizeof(eq->z));
}

void eq_process(Eq *eq, float *buf, int frames)
{
    int b, i, ch;
    float pre;

    if (!eq || !eq->enabled || !buf || frames <= 0)
        return;

    pre = powf(10.0f, eq->preamp_db / 20.0f);
    if (pre != 1.0f) {
        int n = frames * eq->channels;
        for (i = 0; i < n; i++)
            buf[i] *= pre;
    }

    for (b = 0; b < EQ_BANDS; b++) {
        const EqBiquad c = eq->coef[b];
        if (c.b0 == 1.0f && c.b1 == 0.0f && c.b2 == 0.0f &&
            c.a1 == 0.0f && c.a2 == 0.0f)
            continue;               /* bypassed band */

        for (ch = 0; ch < eq->channels; ch++) {
            float z1 = eq->z[b][ch][0];
            float z2 = eq->z[b][ch][1];
            if (eq->channels == 2) {
                for (i = 0; i < frames; i++) {
                    float x = buf[i * 2 + ch];
                    float y = c.b0 * x + z1;
                    z1 = c.b1 * x - c.a1 * y + z2;
                    z2 = c.b2 * x - c.a2 * y;
                    buf[i * 2 + ch] = y;
                }
            } else {
                for (i = 0; i < frames; i++) {
                    float x = buf[i];
                    float y = c.b0 * x + z1;
                    z1 = c.b1 * x - c.a1 * y + z2;
                    z2 = c.b2 * x - c.a2 * y;
                    buf[i] = y;
                }
            }
            eq->z[b][ch][0] = z1;
            eq->z[b][ch][1] = z2;
        }
    }
}

void eq_preset_flat(float *g)
{
    int b;
    for (b = 0; b < EQ_BANDS; b++)
        g[b] = 0.0f;
}

void eq_preset_bass_boost(float *g)
{
    static const float p[EQ_BANDS] = { 8, 7, 5, 3, 1, 0, 0, 1, 2, 2 };
    int b;
    for (b = 0; b < EQ_BANDS; b++)
        g[b] = p[b];
}

void eq_preset_vocal(float *g)
{
    static const float p[EQ_BANDS] = { -3, -2, 0, 3, 5, 5, 3, 1, 0, -1 };
    int b;
    for (b = 0; b < EQ_BANDS; b++)
        g[b] = p[b];
}
