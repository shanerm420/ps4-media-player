/* applog.c - one file, four levels, one writer, and the frame measurements.
 *
 * Design notes that matter on the console:
 *
 *   * WARN and ERROR are flushed immediately, because the line that explains a
 *     crash is worthless in a buffer that dies with the process. INFO is
 *     flushed as it is written (it is rare). DEBUG goes to a small text buffer
 *     that is flushed on the next WARN/ERROR, on the next summary and at exit,
 *     so per-frame chatter costs a memcpy and never an fwrite.
 *   * Identical text inside three seconds is counted, not repeated.
 *   * The file is capped at 1 MB and rotated into app.old.log, so the app
 *     cannot grow a log without bound and the previous run survives a crash.
 *   * The timestamp is uptime in seconds with three decimals: two events 40 ms
 *     apart can be told apart even when the console clock has whole seconds.
 */
#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>

#include "applog.h"

#define LOG_DIR   "/data/ultimate-ps4-media-player"
#define LOG_CAP   (1024 * 1024)
#define DBG_BUF   4096
#define DUP_SLOTS 16
#define STATS_SAMPLES 256
#define STATS_NOTES   32
#define STYLE_MAX     18

static FILE           *s_f;
static pthread_mutex_t s_mtx = PTHREAD_MUTEX_INITIALIZER;
static uint64_t        s_t0;
static int             s_opened;
static int             s_level = LOG_LEVEL_SUMMARY;
static int             s_bytes;

static char s_dbg[DBG_BUF];
static int  s_dbg_n;

typedef struct {
    unsigned hash;
    int      count;
    uint64_t last_ns;
} Dup;

static Dup s_dup[DUP_SLOTS];
static int s_dup_i;
static int s_dup_pending;     /* repeats suppressed since the last line */

/* ---- stats state --------------------------------------------------------- */

static double s_smp[STATS_SAMPLES];
static int    s_smp_n;
static int    s_smp_i;
static double s_budget;
static long   s_frames;
static long   s_over;
static int    s_errors;

static const char *s_note_name[STATS_NOTES];
static double      s_note_val[STATS_NOTES];
static int         s_note_n;

static int    s_ctx_style = -1;
static int    s_ctx_step;
static int    s_ctx_frame;
static char   s_ctx_file[256];
static const char *s_ctx_file_src;

typedef struct {
    long   n;
    double sum;
    double worst;
    double p99;          /* the worst window p99 seen for this style */
    double dwell;        /* seconds spent at the current step */
    unsigned steps;      /* bit per step used */
    double bpm_sum;      /* tempo samples, for the per-style average */
    long   bpm_n;
    double canvas_peak_kb;
} StyleRun;

static StyleRun s_run[STYLE_MAX];

static uint64_t (*s_uptime_us)(void);

void log_set_uptime_us(uint64_t (*fn)(void))
{
    s_uptime_us = fn;
}

static uint64_t mono_ns(void)
{
    struct timespec ts;

    /* The caller's clock wins. On the console CLOCK_MONOTONIC returns an error
     * and leaves the struct untouched, which is why every line of the v105 and
     * v109 logs read [    0.000]: the stamp was the never-filled stack value
     * minus itself. main() passes the kernel's process clock instead. */
    if (s_uptime_us)
        return s_uptime_us() * 1000ull;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static double uptime_s(uint64_t now)
{
    return s_t0 ? (double)(now - s_t0) / 1e9 : 0.0;
}

/* ------------------------------------------------------------- file I/O --- */

static void rotate(void)
{
    if (!s_f)
        return;
    fclose(s_f);
    s_f = NULL;
    remove(APP_LOG_OLD);
    rename(APP_LOG_PATH, APP_LOG_OLD);
    s_f = fopen(APP_LOG_PATH, "w");
    s_bytes = 0;
}

static void flush_debug(void)
{
    if (!s_dbg_n)
        return;
    if (s_f)
        fwrite(s_dbg, 1, (size_t)s_dbg_n, s_f);
    s_dbg_n = 0;
}

/* Caller holds the lock. */
static void put_line(int level, const char *tag, const char *msg, uint64_t now)
{
    char  line[1024];
    int   n;
    double up = uptime_s(now);

    n = snprintf(line, sizeof(line), "[%9.3f] %-5s %-6s %s\n", up,
                 level == LOG_ERROR ? "ERROR" : level == LOG_WARN ? "WARN"
                 : level == LOG_INFO ? "INFO" : "DEBUG",
                 tag, msg);
    if (n <= 0)
        return;
    if (n >= (int)sizeof(line))
        n = (int)sizeof(line) - 1;

    if (level == LOG_DEBUG) {
        if (s_dbg_n + n > DBG_BUF)
            flush_debug();
        if (n > DBG_BUF)            /* a single line longer than the buffer */
            n = DBG_BUF;
        memcpy(s_dbg + s_dbg_n, line, (size_t)n);
        s_dbg_n += n;
        return;
    }

    flush_debug();
    if (s_f) {
        fwrite(line, 1, (size_t)n, s_f);
        s_bytes += n;
    }
    /* Warnings and errors are the lines a crash report needs, so they are on
     * the card before the next frame runs. INFO is rare enough to do the same. */
    if (s_f)
        fflush(s_f);
    if (s_bytes >= LOG_CAP)
        rotate();
}

static unsigned hash_text(const char *s)
{
    unsigned h = 2166136261u;

    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}

/* ---------------------------------------------------------------- public -- */

int log_is_open(void)
{
    return s_opened;
}

void log_set_level(int level)
{
    if (level < LOG_LEVEL_OFF)
        level = LOG_LEVEL_OFF;
    if (level > LOG_LEVEL_DETAILED)
        level = LOG_LEVEL_DETAILED;
    s_level = level;
}

int log_level_budget(void)
{
    return s_level;
}

void log_open(const char *build)
{
    time_t     now = time(NULL);
    struct tm *tm  = localtime(&now);
    char       sep[160];

    mkdir(LOG_DIR, 0777);
    /* The previous run becomes app.old.log; the two files together hold the
     * crash you are investigating and the run before it. */
    remove(APP_LOG_OLD);
    rename(APP_LOG_PATH, APP_LOG_OLD);

    s_f      = fopen(APP_LOG_PATH, "w");
    s_opened = (s_f != NULL);
    s_t0     = mono_ns();
    s_bytes  = 0;
    s_dbg_n  = 0;
    if (!s_f)
        return;

    if (tm)
        snprintf(sep, sizeof(sep),
                 "========================= run %04d-%02d-%02d %02d:%02d:%02d  build %s =========================",
                 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour,
                 tm->tm_min, tm->tm_sec, build ? build : "?");
    else
        snprintf(sep, sizeof(sep), "========================= run  build %s =========================",
                 build ? build : "?");
    fprintf(s_f, "%s\n", sep);
    fprintf(s_f, "; %s - one file, one run.  previous run: %s\n",
            APP_LOG_PATH, APP_LOG_OLD);
    fflush(s_f);
    s_bytes = (int)strlen(sep) + 1;
    stats_schema();
}

void log_flush(void)
{
    pthread_mutex_lock(&s_mtx);
    flush_debug();
    if (s_f)
        fflush(s_f);
    pthread_mutex_unlock(&s_mtx);
}

void log_close(void)
{
    pthread_mutex_lock(&s_mtx);
    flush_debug();
    if (s_f) {
        fflush(s_f);
        fclose(s_f);
        s_f = NULL;
    }
    s_opened = 0;
    pthread_mutex_unlock(&s_mtx);
}

void log_msg(int level, const char *tag, const char *fmt, ...)
{
    char    msg[768];
    unsigned h;
    uint64_t now;
    int     slot = -1, i;

    if (!s_opened)
        return;
    if (level == LOG_DEBUG && s_level < LOG_LEVEL_DETAILED)
        return;
    if (level == LOG_DEBUG && !s_f)
        return;

    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    now = mono_ns();
    h   = hash_text(msg) ^ hash_text(tag) ^ (unsigned)level;

    pthread_mutex_lock(&s_mtx);

    /* Say how much was swallowed, on the first line that is not a repeat. */
    if (s_dup_pending) {
        s_dup_pending = 0;
        for (i = 0; i < DUP_SLOTS; i++) {
            if (s_dup[i].count > 1) {
                char note[96];

                snprintf(note, sizeof(note),
                         "(the line above repeated %d more time%s in under 3 s)",
                         s_dup[i].count - 1,
                         s_dup[i].count == 2 ? "" : "s");
                put_line(LOG_INFO, "log", note, now);
            }
            s_dup[i].count = 0;
        }
    }

    /* Rate limit: the same line inside three seconds is a repeat, not news.
     * Only warnings and errors: INFO is deliberate and DEBUG is already
     * summed up rather than repeated. */
    if (level <= LOG_WARN) {
        for (i = 0; i < DUP_SLOTS; i++) {
            if (s_dup[i].hash == h) {
                slot = i;
                break;
            }
        }
        if (slot >= 0 && now - s_dup[slot].last_ns < 3000000000ull) {
            s_dup[slot].last_ns = now;
            s_dup[slot].count++;
            s_dup_pending = 1;
            pthread_mutex_unlock(&s_mtx);
            return;
        }
        if (slot < 0) {
            slot = s_dup_i;
            s_dup_i = (s_dup_i + 1) % DUP_SLOTS;
        }
        s_dup[slot].hash    = h;
        s_dup[slot].count   = 1;
        s_dup[slot].last_ns = now;
    }

    put_line(level, tag, msg, now);

    pthread_mutex_unlock(&s_mtx);

    if (level == LOG_ERROR)
        stats_error();
}

/* ---------------------------------------------------------------- stats --- */

void stats_schema(void)
{
    log_msg(LOG_INFO, "stats",
            "schema 2: uptime level tag message; summary keys: mode n avg min max p95 p99 over style step canvas");
    log_msg(LOG_INFO, "stats", "log level=%d (0 off, 1 summary, 2 detailed)",
            s_level);
}

void stats_context(int style, int step, int frame, const char *file)
{
    s_ctx_style = style;
    s_ctx_step  = step;
    s_ctx_frame = frame;
    if (file != s_ctx_file_src) {
        s_ctx_file_src = file;
        if (file)
            snprintf(s_ctx_file, sizeof(s_ctx_file), "%s", file);
        else
            s_ctx_file[0] = '\0';
    }
}

void stats_error(void)
{
    s_errors++;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;

    return x < y ? -1 : (x > y ? 1 : 0);
}

void stats_frame(double total_ms, double budget_ms)
{
    s_budget = budget_ms;
    s_frames++;
    if (total_ms > budget_ms)
        s_over++;
    if (s_level == LOG_LEVEL_OFF)
        return;

    s_smp[s_smp_i] = total_ms;
    s_smp_i = (s_smp_i + 1) % STATS_SAMPLES;
    if (s_smp_n < STATS_SAMPLES)
        s_smp_n++;

    if (s_ctx_style >= 0 && s_ctx_style < STYLE_MAX) {
        StyleRun *r = &s_run[s_ctx_style];

        r->n++;
        r->sum += total_ms;
        if (total_ms > r->worst)
            r->worst = total_ms;
        if (s_ctx_step >= 0 && s_ctx_step < 8)
            r->steps |= 1u << s_ctx_step;
    }

    /* A frame twice over budget is a visible hitch; say what was happening,
     * rate-limited by log_msg() so a stuck loop cannot flood the file. */
    if (total_ms > 2.0 * budget_ms)
        log_msg(LOG_WARN, "perf",
                "long frame %.1f ms (budget %.0f) style=%d step=%d frame=%d file=%s",
                total_ms, budget_ms, s_ctx_style, s_ctx_step, s_ctx_frame,
                s_ctx_file[0] ? s_ctx_file : "-");
}

void stats_note(const char *name, double value)
{
    int i;

    for (i = 0; i < s_note_n; i++) {
        if (s_note_name[i] == name || strcmp(s_note_name[i], name) == 0) {
            s_note_val[i] = value;
            return;
        }
    }
    if (s_note_n < STATS_NOTES) {
        s_note_name[s_note_n] = name;
        s_note_val[s_note_n]  = value;
        s_note_n++;
    }

    /* Two of the named values are per style in the end-of-run block: the tempo
     * it was played at, and the paint buffer it needed. */
    if (s_ctx_style >= 0 && s_ctx_style < STYLE_MAX) {
        StyleRun *r = &s_run[s_ctx_style];

        if (strcmp(name, "bpm") == 0 && value > 0.0) {
            r->bpm_sum += value;
            r->bpm_n++;
        } else if (strcmp(name, "canvas_kb") == 0 &&
                   value > r->canvas_peak_kb) {
            r->canvas_peak_kb = value;
        }
    }
}

void stats_summary(const char *mode)
{
    double tmp[STATS_SAMPLES];
    double avg = 0.0, mn = 0.0, mx = 0.0, p95 = 0.0, p99 = 0.0;
    double cw = 0.0, ch = 0.0;
    char   buf[768];
    int    n, i, at;
    long   over_win, over_pct;

    if (s_level == LOG_LEVEL_OFF) {
        s_smp_n = 0;
        s_smp_i = 0;
        s_note_n = 0;
        return;
    }
    n = s_smp_n;
    if (n <= 0) {
        s_note_n = 0;
        return;
    }
    memcpy(tmp, s_smp, sizeof(double) * (size_t)s_smp_n);
    qsort(tmp, (size_t)n, sizeof(double), cmp_double);
    mn = tmp[0];
    mx = tmp[n - 1];
    for (i = 0; i < n; i++)
        avg += tmp[i];
    avg /= (double)n;
    p95 = tmp[(int)((double)(n - 1) * 0.95)];
    p99 = tmp[(int)((double)(n - 1) * 0.99)];

    over_win = 0;
    for (i = 0; i < s_smp_n; i++)
        if (s_smp[i] > s_budget)
            over_win++;
    over_pct = n ? over_win * 100 / n : 0;

    if (s_ctx_style >= 0 && s_ctx_style < STYLE_MAX &&
        p99 > s_run[s_ctx_style].p99)
        s_run[s_ctx_style].p99 = p99;

    /* The canvas is in the line itself rather than in the trailing notes: it is
     * the first thing to check when a frame time changes. */
    for (i = 0; i < s_note_n; i++) {
        if (strcmp(s_note_name[i], "canvas_w") == 0)
            cw = s_note_val[i];
        else if (strcmp(s_note_name[i], "canvas_h") == 0)
            ch = s_note_val[i];
    }

    at = snprintf(buf, sizeof(buf),
                  "stats mode=%s style=%d step=%d canvas=%dx%d file=%s n=%d "
                  "avg=%.1f min=%.1f p95=%.1f p99=%.1f max=%.1f over=%ld(%ld%%)",
                  mode, s_ctx_style, s_ctx_step, (int)cw, (int)ch,
                  s_ctx_file[0] ? s_ctx_file : "-", n, avg, mn, p95, p99, mx,
                  over_win, over_pct);
    for (i = 0; i < s_note_n && at > 0 && at < (int)sizeof(buf); i++) {
        if (strcmp(s_note_name[i], "canvas_w") == 0 ||
            strcmp(s_note_name[i], "canvas_h") == 0)
            continue;
        at += snprintf(buf + at, sizeof(buf) - (size_t)at, " %s=%.1f",
                       s_note_name[i], s_note_val[i]);
    }
    if (at > 0)
        log_msg(LOG_INFO, "stats", "%s", buf);

    s_smp_n  = 0;
    s_smp_i  = 0;
    s_note_n = 0;
}

void stats_end(double seconds)
{
    int i;
    long steps_used = 0;

    log_msg(LOG_INFO, "stats", "=== end of run ===");
    log_msg(LOG_INFO, "stats",
            "run seconds=%.0f frames=%ld over_budget=%ld errors=%d", seconds,
            s_frames, s_over, s_errors);
    for (i = 0; i < STYLE_MAX; i++) {
        StyleRun *r = &s_run[i];

        if (r->n <= 0)
            continue;
        steps_used++;
        log_msg(LOG_INFO, "stats",
                "style %d: frames=%ld avg=%.1f worst=%.1f p99max=%.1f steps=%s%s%s",
                i, r->n, r->sum / (double)r->n, r->worst, r->p99,
                (r->steps & 2u) ? "1" : "",
                (r->steps & 4u) ? ",2" : "",
                (r->steps & 8u) ? ",3" : "");
        if (!r->steps)
            log_msg(LOG_INFO, "stats", "style %d: no frames at a known step", i);
        if (r->bpm_n > 0)
            log_msg(LOG_INFO, "stats",
                    "style %d: bpm avg=%.1f over %ld samples, canvas peak=%.0f KB",
                    i, r->bpm_sum / (double)r->bpm_n, r->bpm_n,
                    r->canvas_peak_kb);
    }
    log_msg(LOG_INFO, "stats", "%ld styles seen", steps_used);
    log_flush();
}

/* ---------------------------------------------------------- compatibility -- */

/* "boot: ..." -> "boot", "loop[3]: ..." -> "loop", and anything without that
 * shape is "app". The message text keeps its prefix: several of them carry a
 * format argument inside it ("loop[%d]"), so the string must not be rewritten
 * here - only the tag is taken from it. */
static const char *tag_of(const char *fmt, char *scratch, size_t n)
{
    size_t i;

    for (i = 0; i < n - 1 && fmt[i]; i++) {
        char c = fmt[i];

        if (c == ':' || c == '[') {
            if (i == 0)
                return "app";
            memcpy(scratch, fmt, i);
            scratch[i] = '\0';
            return scratch;
        }
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == ' '))
            return "app";
    }
    return "app";
}

void plog(const char *fmt, ...)
{
    char    msg[512];
    char    tag[16];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    log_msg(LOG_INFO, tag_of(msg, tag, sizeof(tag)), "%s", msg);
}

void plog_open(void)
{
    if (!s_opened)
        log_open("?");
}

void plog_once(const char *msg)
{
    static unsigned seen;

    if (seen & 1u)
        return;
    seen |= 1u;
    log_msg(LOG_INFO, "app", "%s", msg);
}

void plog_perf(const char *fmt, ...)
{
    char    msg[512];
    char    tag[16];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    log_msg(LOG_DEBUG, tag_of(msg, tag, sizeof(tag)), "%s", msg);
}

/* Per-file verdicts used to have their own never-rotated file. They are INFO
 * lines in the one log now; a START with no PASS/FAIL after it still names the
 * file that killed the app. */
void plog_result(const char *fmt, ...)
{
    char    msg[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    log_msg(LOG_INFO, "media", "%s", msg);
}
