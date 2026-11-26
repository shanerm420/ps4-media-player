/* compat.h - what this console actually exposes, asked instead of assumed.
 *
 * The console axis that changes behaviour is the firmware version, not the
 * model: Sony ships one system-software image for the launch PS4, the Slim and
 * the Pro, and the per-firmware symbol dumps in ps4libdoc list the same
 * userland libraries for all of them. So a compatibility layer here is about
 * asking the running firmware what it has, and degrading when the answer is no
 * - not about branching on which console this is.
 *
 * Two things are checked:
 *
 *   1. The module is loaded. AvPlayer is not preloaded for a game process and
 *      must be brought up by the app (ORBIS_SYSMODULE_AV_PLAYER, documented in
 *      the OpenOrbis Sysmodule docs). VideoOut, AudioOut, Pad, SystemService,
 *      UserService, NetCtl and GnmDriver are in the loader's preload set, which
 *      is why the toolchain's own samples call sceVideoOutOpen without loading
 *      anything - ours adds GnmDriver only because the decoder thread needs it.
 *
 *   2. The entry points exist, resolved by name through sceKernelDlsym against
 *      the module handle. The handle comes from sceKernelLoadStartModule by
 *      path, never from a sysmodule id: ids are for sceSysmoduleLoadModule*,
 *      handles are for dlsym. This is the pattern PKGi, Apollo and the Piglet
 *      sample all use.
 *
 * AvPlayer's exports are stable across 5.05, 6.72 and 9.00 (identical NIDs in
 * every dump, both GetVideoData and GetVideoDataEx present in each), so nothing
 * here exists to paper over a known absence. It exists so that "absent" is
 * answered by a log line and a fallback instead of an unresolved import
 * trapping at the first call - the failure mode the kernel reports as
 * "Call to unpatched function is detected".
 */
#ifndef COMPAT_H
#define COMPAT_H

/* Frame getters, as pointers so the video path can pick one that exists.
 * NULL means "not resolved here"; callers then use the linked symbol, which is
 * exactly what every earlier build did. */
/* The handle is SceAvPlayerHandle, which this toolchain types as an unsigned
 * long, not a pointer - hence the explicit integer parameter here. */
typedef int (*compat_av_frame_fn)(unsigned long handle, void *frame);

extern compat_av_frame_fn compat_av_getvideodataex;
extern compat_av_frame_fn compat_av_getvideodata;

/* Asks the console what it has, once, before any video call. Writes one
 * "compat:" line per thing asked, so a log from any firmware says what was
 * found. Safe to call twice; the second call does nothing. */
void compat_probe(void);

#endif /* COMPAT_H */
