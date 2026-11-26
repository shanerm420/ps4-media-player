/* compat.c - see compat.h for why this exists and what it is asking.
 *
 * On the host build there is no Orbis kernel, so every pointer stays NULL and
 * compat_probe() says nothing; the console-only code is compiled behind
 * __FreeBSD__, which is what the x86_64-pc-freebsd12-elf target defines (there
 * is no project-wide PS4 macro, and inventing one would mean a compiler-flag
 * change that invalidates every object for no gain).
 */
#include <stdio.h>
#include <string.h>

#include "applog.h"
#include "compat.h"

#if defined(__FreeBSD__)
#include <orbis/Sysmodule.h>
#include <orbis/libkernel.h>
#endif

compat_av_frame_fn compat_av_getvideodataex = 0;
compat_av_frame_fn compat_av_getvideodata = 0;

#if defined(__FreeBSD__)

/* Load a system module by path and keep the handle for dlsym.
 *
 * The sandbox word path is what the Piglet sample uses and what the GnmDriver
 * load in main.c already does; the /system path is the fallback for a build
 * whose sandbox does not expose common/lib. A module already loaded through
 * sceSysmoduleLoadModule can answer either call with its existing handle, and a
 * refusal is not fatal: the pointers stay NULL and the video path keeps using
 * the linked symbols, which is what every earlier build did. */
static int load_by_name(const char *name)
{
    char path[256];
    int  h;

    snprintf(path, sizeof(path), "/%s/common/lib/%s",
             sceKernelGetFsSandboxRandomWord(), name);
    h = (int)sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    if (h < 0) {
        snprintf(path, sizeof(path), "/system/common/lib/%s", name);
        h = (int)sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    }
    return h;
}

/* Resolve one symbol by name. Returns 1 when a pointer came back, 0 when the
 * module or the export is missing - never an error the caller has to handle. */
static int resolve(int handle, const char *symbol, void **out)
{
    void *p = 0;

    if (handle < 0 || !out)
        return 0;
    if (sceKernelDlsym(handle, symbol, &p) != 0 || !p)
        return 0;
    *out = p;
    return 1;
}

#else

static int load_by_name(const char *name)
{
    (void)name;
    return -1;
}

static int resolve(int handle, const char *symbol, void **out)
{
    (void)handle;
    (void)symbol;
    (void)out;
    return 0;
}

#endif

static int s_probed = 0;

void compat_probe(void)
{
    int h;
    int ex;
    int plain;

    if (s_probed)
        return;
    s_probed = 1;

    /* AvPlayer: the one module this app must ask for by name, and the only one
     * whose absence would otherwise be discovered by calling into it. */
    h = load_by_name("libSceAvPlayer.sprx");
    plog("compat: avplayer module handle=%d", h);

    ex    = resolve(h, "sceAvPlayerGetVideoDataEx", (void **)&compat_av_getvideodataex);
    plain = resolve(h, "sceAvPlayerGetVideoData", (void **)&compat_av_getvideodata);
    plog("compat: avplayer GetVideoDataEx=%s GetVideoData=%s",
         ex ? "yes" : "no", plain ? "yes" : "no");

#if defined(__FreeBSD__)
    /* The rest of the set this app calls into, asked for by internal id. On a
     * normal game process every one of these is already up - the loader
     * preloads them, which is why the toolchain's samples call sceVideoOutOpen
     * and scePadInit without loading anything - so a refusal is logged and
     * ignored rather than acted on. Asking anyway is what makes the same binary
     * survive a context that does not preload them. System service and video
     * out are asked for in main.c, next to the GnmDriver load. */
    {
        static const struct {
            const char *name;
            unsigned    id;
        } rest[] = {
            { "audio out",    ORBIS_SYSMODULE_INTERNAL_AUDIOOUT },
            { "pad",          ORBIS_SYSMODULE_INTERNAL_PAD },
            { "user service", ORBIS_SYSMODULE_INTERNAL_USER_SERVICE },
            { "net ctl",      ORBIS_SYSMODULE_INTERNAL_NETCTL },
        };
        size_t i;

        for (i = 0; i < sizeof(rest) / sizeof(rest[0]); i++)
            plog("compat: %s rc=0x%08X", rest[i].name,
                 (unsigned)sceSysmoduleLoadModuleInternal(rest[i].id));
    }
#endif
}
