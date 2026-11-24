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

#include <string.h>

#include <orbis/AudioOut.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/_types/user.h>

extern void plog(const char *fmt, ...);

static int s_handle = -1;
static int s_frames = 0;
static int s_ready  = 0;

/* sceAudioOutInit goes through the user-service layer, so both modules have
 * to be in memory first. Called without them it fails with a user-service
 * error (0x809B0001), which is exactly what every boot logged. */
static void ensure_modules(void)
{
    OrbisUserServiceInitializeParams param;

    if (s_ready)
        return;
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT);
    memset(&param, 0, sizeof(param));
    param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    sceUserServiceInitialize(&param);
    s_ready = 1;
}

int aout_open(int sample_rate, int frames_per_block)
{
    OrbisUserServiceUserId uid = 0;
    int rc;

    ensure_modules();

    rc = sceUserServiceGetInitialUser(&uid);
    /* 0 is not a user: opening a port as user 0 is refused. The system user
     * (0xFF) is what the working PS4 players pass, so fall back to it. */
    if (rc != 0 || uid == 0)
        uid = ORBIS_USER_SERVICE_USER_ID_SYSTEM;

    rc = sceAudioOutInit();
    plog("audio: init rc=0x%08X", (unsigned)rc);
    /* 0x8026000E is "already initialised", which is fine. */
    if (rc != 0 && (uint32_t)rc != 0x8026000E)
        return rc;

    /* 0x809B0001 is "invalid user", so the id matters more than the format.
     * Both the console's logged-in user and the system user are tried, and
     * each attempt is logged so the next boot says which one the firmware
     * accepts. */
    {
        int32_t ids[2];
        int     i;

        ids[0] = uid;
        ids[1] = ORBIS_USER_SERVICE_USER_ID_SYSTEM;
        for (i = 0; i < 2; i++) {
            s_handle = sceAudioOutOpen(ids[i], ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                                       (uint32_t)frames_per_block,
                                       (uint32_t)sample_rate,
                                       ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
            plog("audio: open uid=0x%X rc=0x%08X", (unsigned)ids[i],
                 (unsigned)s_handle);
            if (s_handle >= 0)
                break;
            if (frames_per_block != 256) {
                s_handle = sceAudioOutOpen(ids[i], ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0,
                                           256, (uint32_t)sample_rate,
                                           ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
                plog("audio: open uid=0x%X grain=256 rc=0x%08X",
                     (unsigned)ids[i], (unsigned)s_handle);
                if (s_handle >= 0) {
                    frames_per_block = 256;
                    break;
                }
            }
        }
    }
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

int aout_is_open(void)
{
    return s_handle >= 0;
}

void aout_close(void)
{
    if (s_handle >= 0)
        sceAudioOutClose(s_handle);
    s_handle = -1;
}
