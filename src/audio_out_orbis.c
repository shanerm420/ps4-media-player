/* audio_out_orbis.c - PS4 audio output.
 *
 * Signatures and enumerators here are taken from the OpenOrbis headers:
 *   int32_t sceAudioOutOpen(OrbisUserServiceUserId, enum OrbisAudioOutPort,
 *                           int32_t index, uint32_t length,
 *                           uint32_t sample_rate, uint32_t param_type);
 *   int32_t sceAudioOutOutput(int32_t handle, const void *ptr);
 *   ORBIS_AUDIO_OUT_PORT_TYPE_MAIN = 0
 *   ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO = 1
 */
#include "audio_out_orbis.h"

#include <orbis/AudioOut.h>
#include <orbis/UserService.h>

static int s_handle = -1;
static int s_frames = 0;

int aout_open(int sample_rate, int frames_per_block)
{
    OrbisUserServiceUserId uid = 0;
    int rc;

    rc = sceUserServiceGetInitialUser(&uid);
    if (rc != 0)
        uid = 0;   /* keep going with user 0; output still opens on most FWs */

    rc = sceAudioOutInit();
    if (rc != 0)
        return rc;

    s_handle = sceAudioOutOpen(uid, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                               (uint32_t)frames_per_block,
                               (uint32_t)sample_rate,
                               ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (s_handle < 0)
        return s_handle;

    s_frames = frames_per_block;
    return 0;
}

int aout_push_s16(const int16_t *pcm)
{
    if (s_handle < 0)
        return -1;
    /* Blocks until the port has consumed the previous block, which is what
     * paces playback at exactly the sample rate. */
    return sceAudioOutOutput(s_handle, pcm);
}

int aout_block_frames(void)
{
    return s_frames;
}

void aout_close(void)
{
    if (s_handle >= 0)
        sceAudioOutClose(s_handle);
    s_handle = -1;
}
