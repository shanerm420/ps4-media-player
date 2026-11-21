#include "resampler.h"

#include <string.h>

void rs_init(Resampler *rs, int in_rate, int out_rate, int channels)
{
    memset(rs, 0, sizeof(*rs));
    rs->in_rate  = (in_rate  > 0) ? in_rate  : 48000;
    rs->out_rate = (out_rate > 0) ? out_rate : 48000;
    rs->channels = (channels == 2) ? 2 : 1;
}

void rs_reset(Resampler *rs)
{
    rs->pos    = 0.0;
    rs->primed = 0;
    rs->hist[0] = rs->hist[1] = 0.0f;
}

static float sample_at(const Resampler *rs, const float *in, int idx, int ch)
{
    if (idx == 0)
        return rs->primed ? rs->hist[ch] : 0.0f;
    return in[(idx - 1) * rs->channels + ch];
}

int rs_process(Resampler *rs, const float *in, int in_frames,
               float *out, int out_capacity)
{
    const double step = (double)rs->in_rate / (double)rs->out_rate;
    int n = 0;
    int ch;

    if (!rs || !in || !out || in_frames <= 0 || out_capacity <= 0)
        return 0;

    /* With one frame of history the conceptual input spans [0 .. in_frames]. */
    while (rs->pos <= (double)(in_frames - 1) && n < out_capacity) {
        int    i0 = (int)rs->pos;
        double f  = rs->pos - (double)i0;

        for (ch = 0; ch < rs->channels; ch++) {
            float a = sample_at(rs, in, i0, ch);
            float b = sample_at(rs, in, i0 + 1, ch);
            out[n * rs->channels + ch] = (float)(a * (1.0 - f) + b * f);
        }
        rs->pos += step;
        n++;
    }

    /* The frame we consumed last becomes the history for the next call. */
    rs->hist[0] = in[(in_frames - 1) * rs->channels];
    rs->hist[1] = (rs->channels == 2) ? in[(in_frames - 1) * rs->channels + 1]
                                      : rs->hist[0];
    rs->primed = 1;
    rs->pos -= (double)in_frames;
    if (rs->pos < 0.0)
        rs->pos = 0.0;

    return n;
}
