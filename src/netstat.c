/* netstat.c - connected or not, on a thread of its own.
 *
 * libSceNetCtl is linked (-lSceNetCtl) and loaded by the system with the app;
 * the internal module load below is the same belt-and-braces call main.c makes
 * for the other system libraries, and its result is logged once. After that the
 * thread initialises netctl, reads the state, logs only when it changes, and
 * sleeps for the configured interval in between.
 *
 * What is deliberately absent: sockets, DNS, addresses, MACs, SSIDs, host
 * names. The whole useful answer for the log is "connected" or "not
 * connected", plus the link type.
 *
 * If the module or the init fails, one warning is written and the thread ends.
 * Nothing waits for it and nothing retries in a loop.
 */
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <orbis/NetCtl.h>
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>

#include "netstat.h"
#include "applog.h"

/* Exported by libSceNetCtl but not declared by this toolchain's header. The
 * shape is the public one: the call fills an int with the connection state. */
extern int32_t sceNetCtlGetState(int32_t *state);

/* The state values follow the public netctl convention (0 disconnected,
 * 1 connecting, 2 obtaining an address, 3 connected): the Vita SDK documents
 * the same four states, and PS4 research notes report 3 as the connected value.
 * The number is logged next to the label, so it can be checked on hardware.
 *
 * The device code from ORBIS_NET_CTL_INFO_DEVICE is not documented anywhere
 * public, so it is never guessed at: code 0 is what a known-wired console
 * reported, that one is named, and every other value is logged as a number. */
#define NET_STATE_CONNECTED 3

static int s_started;
static int s_poll = 20;

static const char *state_name(int32_t st)
{
    switch (st) {
    case 0:  return "not connected";
    case 1:  return "connecting";
    case 2:  return "obtaining an address";
    case 3:  return "connected";
    default: return NULL;
    }
}

static void *net_thread(void *arg)
{
    int32_t last_state = -2, last_dev = -2;
    int32_t rc;

    (void)arg;

    rc = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL);
    log_msg(LOG_INFO, "net", "netctl module rc=0x%08X", (unsigned)rc);

    rc = sceNetCtlInit();
    log_msg(LOG_INFO, "net", "netctl init rc=0x%08X", (unsigned)rc);
    if (rc < 0) {
        log_msg(LOG_WARN, "net", "connection state unavailable (init failed)");
        return NULL;
    }

    for (;;) {
        int32_t        st = -1, dev = -1;
        int32_t        src = sceNetCtlGetState(&st);
        const char    *name;

        if (src == 0 && st == NET_STATE_CONNECTED) {
            OrbisNetCtlInfo info;

            memset(&info, 0, sizeof(info));
            if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_DEVICE, &info) == 0)
                dev = (int32_t)info.device;
        } else if (src != 0) {
            /* A failed query is not a state change: keep the last answer and
             * say so once per change of the error code, not per poll. */
            static int32_t last_rc;

            if (src != last_rc) {
                last_rc = src;
                log_msg(LOG_WARN, "net", "state query failed rc=0x%08X",
                        (unsigned)src);
            }
            st = last_state;
        }

        if (st != last_state || dev != last_dev) {
            name = state_name(st);
            if (name == NULL)
                log_msg(LOG_INFO, "net", "not connected");
            else if (st == NET_STATE_CONNECTED) {
                /* 0 is wired: the v109 log came from a console on a cable and
                 * reported 0. Anything else stays a bare number, with the
                 * state kept beside it so the pair can still be checked. */
                if (dev == 0)
                    log_msg(LOG_INFO, "net",
                            "connected (state %d, device %d wired)", st, dev);
                else
                    log_msg(LOG_INFO, "net",
                            "connected (state %d, device code %d)", st, dev);
            }
            else
                log_msg(LOG_INFO, "net", "%s (state %d)", name, st);
            last_state = st;
            last_dev   = dev;
        }
        sleep((unsigned)s_poll);
    }
    return NULL;
}

void net_start(int enabled, int poll_seconds)
{
    pthread_t      th;
    pthread_attr_t attr;

    if (s_started)
        return;
    s_started = 1;

    if (!enabled) {
        log_msg(LOG_INFO, "net", "not polled ([log] network_status = 0)");
        return;
    }
    if (poll_seconds < 10)
        poll_seconds = 10;
    if (poll_seconds > 120)
        poll_seconds = 120;
    s_poll = poll_seconds;

    log_msg(LOG_INFO, "net", "monitoring the connection every %d s", s_poll);

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 128 * 1024);
    if (pthread_create(&th, &attr, net_thread, NULL) == 0)
        pthread_detach(th);
    else
        log_msg(LOG_WARN, "net", "monitor thread did not start");
    pthread_attr_destroy(&attr);
}
