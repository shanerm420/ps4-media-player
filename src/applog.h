/* applog.h - the one log file and the one place measurements are recorded.
 *
 * Everything the app has to say goes to
 *
 *     /data/ultimate-ps4-media-player/app.log
 *
 * as a line with uptime, level, area tag and message. When the file would grow
 * past 1 MB it is rotated once into app.old.log, so there are never more than
 * two files and the log can never fill the drive.
 *
 * The stats calls below are the measurement side: they are cheap per frame,
 * they summarise about once a second, and they answer "where does the time go"
 * without a second logging system or a profiling run. New code contributes with
 * one call - stats_note("name", value) - and appears in the next summary.
 */
#ifndef APPLOG_H
#define APPLOG_H

#include <stddef.h>
#include <stdint.h>

#define APP_LOG_PATH "/data/ultimate-ps4-media-player/app.log"
#define APP_LOG_OLD  "/data/ultimate-ps4-media-player/app.old.log"

enum { LOG_ERROR = 0, LOG_WARN, LOG_INFO, LOG_DEBUG };
enum { LOG_LEVEL_OFF = 0, LOG_LEVEL_SUMMARY, LOG_LEVEL_DETAILED };

/* Opens the log (rotating the previous run to app.old.log) and writes the run
 * separator. `build` is the build id, e.g. "v100". */
void log_open(const char *build);

/* Where the timestamp column comes from: a function returning microseconds.
 * The console's libc clock is not implemented (every line of every log before
 * v111 was stamped 0.000), so main() hands the log the kernel clock the frame
 * timing already uses. Without it the log falls back to clock_gettime, which
 * is what the host builds use. */
void log_set_uptime_us(uint64_t (*fn)(void));
void log_close(void);
void log_flush(void);
int  log_is_open(void);

/* 0 off, 1 summary, 2 detailed. Detailed is the only level that writes DEBUG
 * lines into the file. */
void log_set_level(int level);
int  log_level_budget(void);

/* One function every part of the app calls. Lines are rate-limited: the same
 * text repeated inside three seconds is counted and printed once with a count,
 * so a problem in the frame loop cannot flood the file. */
void log_msg(int level, const char *tag, const char *fmt, ...);

/* ---------------------------------------------------------------- stats --- */

/* Writes the schema line for this run, so a changed summary format is obvious. */
void stats_schema(void);

/* What the frame is doing, for the state line and for error context. Cheap:
 * called once a frame, copies the file name only when it changes. */
void stats_context(int style, int step, int frame, const char *file);

/* One per frame: total frame time, and the budget it was judged against. The
 * samples behind the percentiles come from here. */
void stats_frame(double total_ms, double budget_ms);

/* A named value for the next summary (last value wins). */
void stats_note(const char *name, double value);

/* Emits the ~1 second summary. `mode` is "menu" or "video". */
void stats_summary(const char *mode);

/* Counts one error for the end-of-run block. log_msg() calls this itself. */
void stats_error(void);

/* The end-of-run block: per style, the average and 99th percentile frame time,
 * the steps used, and the totals. `seconds` is the run length. */
void stats_end(double seconds);

/* ------------------------------------------------- compatibility entries -- */
/* The rest of the app calls these; they route into log_msg() with the area tag
 * taken from the message's own "boot:" / "video:" / "vis:" prefix, so there is
 * one file and one writer. New code should call log_msg() directly. */
void plog_open(void);
void plog(const char *fmt, ...);
void plog_once(const char *msg);
void plog_perf(const char *fmt, ...);
void plog_result(const char *fmt, ...);

#endif /* APPLOG_H */
