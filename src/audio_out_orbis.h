/* audio_out_orbis.h - thin wrapper over the PS4 audio out port. */
#ifndef AUDIO_OUT_ORBIS_H
#define AUDIO_OUT_ORBIS_H

#include <stdint.h>

/* Opens the main audio port as 16-bit stereo at the given rate.
 * frames_per_block is the port block size; sceAudioOutOutput must be given
 * exactly this many frames. Returns 0 on success, a Sce error code otherwise. */
int  aout_open(int sample_rate, int frames_per_block);
int  aout_push_s16(const int16_t *pcm);
int  aout_block_frames(void);
/* Non-zero once a real port is open. The port already blocks for the length of
 * a block, so callers must not add their own delay on top of it. */
int  aout_is_open(void);
void aout_close(void);

#endif /* AUDIO_OUT_ORBIS_H */
