/* resampler.h - streaming linear resampler.
 *
 * MP3 files are commonly 44.1 kHz while the PS4 audio out port runs at
 * 48 kHz, so decoded audio has to be converted before it is submitted.
 * One frame of history is kept between calls so block boundaries are clean.
 */
#ifndef RESAMPLER_H
#define RESAMPLER_H

typedef struct {
    int    in_rate;
    int    out_rate;
    int    channels;     /* 1 or 2 */
    double pos;          /* fractional read position in the current block */
    float  hist[2];      /* last input frame of the previous block */
    int    primed;
} Resampler;

void rs_init(Resampler *rs, int in_rate, int out_rate, int channels);
void rs_reset(Resampler *rs);

/* Returns the number of output frames written to out (interleaved). */
int  rs_process(Resampler *rs, const float *in, int in_frames,
                float *out, int out_capacity);

#endif /* RESAMPLER_H */
