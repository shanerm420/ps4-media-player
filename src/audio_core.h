/* audio_core.h - volume, limiting and sample conversion.
 *
 * Everything here is platform independent and unit-tested on a PC.
 */
#ifndef AUDIO_CORE_H
#define AUDIO_CORE_H

#include <stdint.h>

typedef struct {
    float volume_db;   /* -60 .. +12 */
    int   muted;
    float gain;        /* linear, derived from volume_db */
} Volume;

void  vol_init(Volume *v);
void  vol_set_db(Volume *v, float db);
void  vol_add_db(Volume *v, float delta);
float vol_current_db(const Volume *v);

/* Applies the volume gain in place. */
void  audio_apply_gain(float *buf, int samples, float gain);

/* Soft-limits in place: transparent below the knee, tanh above it, so an
 * EQ boost cannot hard-clip the output. */
void  audio_soft_limit(float *buf, int samples);

/* float [-1,1] -> signed 16-bit, with clamping. */
void  audio_to_s16(const float *in, int16_t *out, int samples);

#endif /* AUDIO_CORE_H */
