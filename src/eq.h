/* eq.h - 10-band peaking equalizer for the PS4 media player.
 *
 * Pure C, no platform dependencies, so it can be unit-tested on a PC.
 * Bands follow the usual ISO octave centres. Each band is an RBJ peaking
 * biquad; bands above Nyquist for the current sample rate are bypassed.
 */
#ifndef EQ_H
#define EQ_H

#define EQ_BANDS 10

typedef struct {
    float b0, b1, b2, a1, a2;
} EqBiquad;

typedef struct {
    int   sample_rate;
    int   channels;      /* 1 or 2 */
    int   enabled;
    float gain_db[EQ_BANDS];
    float preamp_db;
    EqBiquad coef[EQ_BANDS];
    float z[EQ_BANDS][2][2];   /* [band][channel][z1|z2] */
} Eq;

void  eq_init(Eq *eq, int sample_rate, int channels);
void  eq_set_band_db(Eq *eq, int band, float db);
void  eq_set_preamp_db(Eq *eq, float db);
void  eq_reset(Eq *eq);
void  eq_process(Eq *eq, float *interleaved, int frames);
float eq_band_hz(int band);
void  eq_preset_flat(float *gain_db);
void  eq_preset_bass_boost(float *gain_db);
void  eq_preset_vocal(float *gain_db);

#endif /* EQ_H */
