#include "audio_core.h"

#include <math.h>

#define VOL_MIN_DB (-60.0f)
#define VOL_MAX_DB ( 12.0f)
#define LIMIT_KNEE ( 0.90f)

static float db_to_linear(float db)
{
    return powf(10.0f, db / 20.0f);
}

void vol_init(Volume *v)
{
    v->volume_db = -6.0f;
    v->muted     = 0;
    v->gain      = db_to_linear(v->volume_db);
}

void vol_set_db(Volume *v, float db)
{
    if (db < VOL_MIN_DB) db = VOL_MIN_DB;
    if (db > VOL_MAX_DB) db = VOL_MAX_DB;
    v->volume_db = db;
    v->gain      = v->muted ? 0.0f : db_to_linear(db);
}

void vol_add_db(Volume *v, float delta)
{
    vol_set_db(v, v->volume_db + delta);
}

float vol_current_db(const Volume *v)
{
    return v->volume_db;
}

void audio_apply_gain(float *buf, int samples, float gain)
{
    int i;
    if (!buf || gain == 1.0f)
        return;
    for (i = 0; i < samples; i++)
        buf[i] *= gain;
}

void audio_soft_limit(float *buf, int samples)
{
    int i;
    if (!buf)
        return;
    for (i = 0; i < samples; i++) {
        float x = buf[i];
        float a = fabsf(x);
        if (a > LIMIT_KNEE) {
            float sign  = (x < 0.0f) ? -1.0f : 1.0f;
            float over  = (a - LIMIT_KNEE) / (1.0f - LIMIT_KNEE);
            float shaped = LIMIT_KNEE + (1.0f - LIMIT_KNEE) * tanhf(over);
            buf[i] = sign * shaped;
        }
    }
}

void audio_to_s16(const float *in, int16_t *out, int samples)
{
    int i;
    for (i = 0; i < samples; i++) {
        float x = in[i];
        if (x >  1.0f) x =  1.0f;
        if (x < -1.0f) x = -1.0f;
        /* 32767 rather than 32768 keeps +1.0 from wrapping negative. */
        out[i] = (int16_t)lrintf(x * 32767.0f);
    }
}
