/* video_orbis.h - MP4/MOV playback through the system AvPlayer. */
#ifndef VIDEO_ORBIS_H
#define VIDEO_ORBIS_H

#include <stdint.h>

/* Opens a container file and starts playback. Returns 0 on success, -1 on
 * failure (video_last_error() says why). A previously open file is closed. */
int  video_open(const char *path);
void video_close(void);

/* Pulls whatever the decoder has ready. Call once per rendered frame.
 * Returns 1 while playing, 0 when the file ended, <0 on error. */
int  video_step(void);

/* Latest decoded frame as RGBA bytes (R,G,B,A in memory), or NULL if no frame
 * has arrived yet. The pointer stays valid until the next video_step().
 * serial increments once per new frame, so the caller can skip re-uploading. */
const uint8_t *video_frame(int *w, int *h, int *serial);

/* Decoded audio for the mixer: stereo floats at 48 kHz, the same format the
 * MP3 path produces. Returns the number of frames written (0 if none ready). */
int  video_read_audio(float *stereo, int max_frames);

double      video_position_sec(void);
double      video_duration_sec(void);
const char *video_format_note(void);      /* "H.264   1920 x 1080" */

void video_set_paused(int paused);
void video_seek(double seconds);

/* Picture settings applied while converting: saturation is a percentage (0
 * greyscale, 100 untouched, 200 vivid) and sharpness 0..100 (0 off). Both
 * take effect on the next frame, cost nothing when at their defaults, and are
 * display-side: the decoder's output is never touched. */
void video_set_picture(int saturation, int sharpness);

const char *video_last_error(void);

/* Measurement helpers for the main loop's perf line: players stopped but not
 * closed, the size of the RGBA frame buffer, and the decoder's direct-memory
 * blocks (count and bytes). */
int                video_left_players(void);
unsigned long long video_rgba_bytes(void);
void               video_mem_stats(int *blocks, unsigned long long *bytes);

#endif /* VIDEO_ORBIS_H */
