/* subtitles.h - sidecar subtitle files (.srt / .vtt) for videos.
 *
 * libSceAvPlayer can report embedded timed-text streams but the OpenOrbis
 * stubs expose no way to pull their frames, so subtitles come from text files
 * sitting next to the video, which is what most downloaded releases ship
 * anyway. Everything here is plain C: no SDL, no PS4, so the parser is covered
 * by the host tests.
 */
#ifndef SUBTITLES_H
#define SUBTITLES_H

#define SUB_MAX_SOURCES 8

/* Loads every subtitle file that belongs to this video:
 *   movie.mp4 + movie.srt / movie.vtt / movie.en.srt / movie.en.vtt
 * Names are matched case-insensitively. Any previously loaded files are
 * released first. Returns the number of usable files (0 if none). */
int  sub_open(const char *video_path);
void sub_close(void);

int         sub_count(void);
const char *sub_name(int index);        /* file name only, "" if out of range */
int         sub_index(void);            /* selected source, -1 = none */

/* Rendering switch. Turning it on with nothing loaded does nothing. */
void sub_set_enabled(int on);
int  sub_enabled(void);

/* Next source; after the last one it wraps to "off". Returns 1 if subtitles
 * are showing afterwards. */
int  sub_cycle(void);

/* The cue that should be on screen at this playback position, or NULL.
 * Lines inside one cue are separated by '\n'. */
const char *sub_text_at(double seconds);

/* Format detection is by content, not extension: SRT, WebVTT and plain
 * "MM:SS --> MM:SS" text all go through the same parser. */
#endif /* SUBTITLES_H */
