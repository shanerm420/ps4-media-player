/* subtitles.c - SRT / WebVTT sidecar parsing for the video player.
 *
 * Deliberately small: a cue list per file, timing lookup by binary search, and
 * a text cleaner that drops markup the renderer cannot draw (<i>, <b>,
 * {\an8}, <c.yellow>). A cue that cannot be timed is skipped rather than
 * guessed at, so a malformed file degrades to "no subtitles" instead of
 * flashing text at the wrong moment.
 */
#include "subtitles.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MAX_CUES    8192
#define MAX_CUE_TXT 1024

typedef struct {
    long  start_ms;
    long  end_ms;
    char *text;
} Cue;

typedef struct {
    char  name[96];
    Cue  *cues;
    int   count;
    int   cap;
} SubSource;

static SubSource s_src[SUB_MAX_SOURCES];
static int       s_count;
static int       s_index = -1;
static int       s_enabled;

/* ---------------------------------------------------------------- helpers -- */

static int ends_with_ci(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);

    return ls >= lx && strcasecmp(s + ls - lx, suffix) == 0;
}

static void free_source(SubSource *s)
{
    int i;

    for (i = 0; i < s->count; i++)
        free(s->cues[i].text);
    free(s->cues);
    memset(s, 0, sizeof(*s));
}

/* "00:01:02,500" / "01:02.500" / "62.5" -> milliseconds. Returns -1 when the
 * text does not begin with a number, which is what an unparsable cue line
 * looks like. */
static long parse_ts(const char *p, const char **end)
{
    long parts[3] = { 0, 0, 0 };
    long frac = 0;
    int  n = 0, f = 0;

    while (*p == ' ' || *p == '\t')
        p++;

    for (;;) {
        long v = 0;
        int  digits = 0;

        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            p++;
            digits++;
        }
        if (!digits)
            break;
        if (n < 3)
            parts[n++] = v;
        if (*p == ':') {
            p++;
            continue;
        }
        break;
    }

    if (*p == ',' || *p == '.') {
        p++;
        while (*p >= '0' && *p <= '9' && f < 3) {
            frac = frac * 10 + (*p - '0');
            p++;
            f++;
        }
        while (f < 3) {                 /* ".5" is 500 ms, not 5 */
            frac *= 10;
            f++;
        }
        while (*p >= '0' && *p <= '9')   /* sub-millisecond precision */
            p++;
    }

    if (end)
        *end = p;
    if (n == 0)
        return -1;
    if (n == 1)
        return parts[0] * 1000 + frac;
    if (n == 2)
        return (parts[0] * 60 + parts[1]) * 1000 + frac;
    return ((parts[0] * 60 + parts[1]) * 60 + parts[2]) * 1000 + frac;
}

/* Copies src into dst while dropping tag-ish markup. Keeps newlines, trims
 * runs of spaces to one, and collapses the whole result's edges. */
static void clean_text(char *dst, size_t dstsz, const char *src)
{
    size_t o = 0;
    int in_angle = 0, in_brace = 0, space = 0;

    for (; src && *src && o + 1 < dstsz; src++) {
        unsigned char c = (unsigned char)*src;

        if (c == '<') { in_angle = 1; continue; }
        if (in_angle) {
            if (c == '>')
                in_angle = 0;
            continue;
        }
        if (c == '{') { in_brace = 1; continue; }
        if (in_brace) {
            if (c == '}')
                in_brace = 0;
            continue;
        }
        if (c == '\r')
            continue;
        if (c == '\n') {
            /* drop a trailing space before the break, then the break itself if
             * we already have one */
            while (o > 0 && dst[o - 1] == ' ')
                o--;
            if (o > 0 && dst[o - 1] != '\n')
                dst[o++] = '\n';
            space = 0;
            continue;
        }
        if (c == ' ' || c == '\t') {
            space = 1;
            continue;
        }
        if (space && o > 0 && dst[o - 1] != '\n')
            dst[o++] = ' ';
        space = 0;
        dst[o++] = (char)c;
    }

    while (o > 0 && (dst[o - 1] == ' ' || dst[o - 1] == '\n'))
        o--;
    dst[o] = '\0';
}

/* own copy: strdup is not in C11 without a feature-test macro, and the PS4
 * libc does not reliably expose it either */
static char *dup_string(const char *s)
{
    size_t n = strlen(s) + 1;
    char  *p = malloc(n);

    if (p)
        memcpy(p, s, n);
    return p;
}

static int cmp_cue(const void *a, const void *b)
{
    const Cue *x = a, *y = b;

    if (x->start_ms != y->start_ms)
        return x->start_ms < y->start_ms ? -1 : 1;
    return 0;
}

static int add_cue(SubSource *s, long start, long end, const char *text)
{
    char clean[MAX_CUE_TXT];

    if (end <= start || s->count >= MAX_CUES)
        return 0;
    clean_text(clean, sizeof(clean), text);
    if (!clean[0])
        return 0;

    if (s->count == s->cap) {
        int   cap = s->cap ? s->cap * 2 : 64;
        Cue  *n;

        if (cap > MAX_CUES)
            cap = MAX_CUES;
        n = realloc(s->cues, (size_t)cap * sizeof(*n));
        if (!n)
            return 0;
        s->cues = n;
        s->cap  = cap;
    }

    s->cues[s->count].start_ms = start;
    s->cues[s->count].end_ms   = end;
    s->cues[s->count].text     = dup_string(clean);
    if (!s->cues[s->count].text)
        return 0;
    s->count++;
    return 1;
}

/* One pass over the file. Every line carrying "-->" opens a cue whose text is
 * whatever follows until the next blank line; that holds for SRT (with its
 * numeric line above) and for VTT (with its optional cue id), so neither
 * format needs a separate branch. */
static int parse_buffer(SubSource *s, const char *buf, size_t len)
{
    const char *p = buf, *end = buf + len;
    char text[MAX_CUE_TXT];
    size_t tlen = 0;

    text[0] = '\0';

    while (p < end) {
        const char *line = p, *le;
        const char *arrow;
        size_t      llen;

        while (p < end && *p != '\n')
            p++;
        le = p;
        if (p < end)
            p++;
        llen = (size_t)(le - line);
        if (llen && line[llen - 1] == '\r')
            llen--;

        arrow = memchr(line, '>', llen);
        if (arrow && arrow > line && arrow[-1] == '-') {
            const char *q;
            long start, stop;

            /* a cue line always follows a blank line; flush any text that was
             * collected before it (defensive: some files omit the blank) */
            tlen = 0;
            text[0] = '\0';

            start = parse_ts(line, &q);
            if (start < 0)
                continue;
            while (q < line + llen && *q != '-')
                q++;
            if (q + 2 < line + llen)
                q += 3;                     /* skip "--> " */
            stop = parse_ts(q, NULL);
            if (stop < 0)
                continue;

            /* gather the following lines as this cue's text */
            while (p < end) {
                const char *tl = p, *te;
                size_t      n;

                while (p < end && *p != '\n')
                    p++;
                te = p;
                if (p < end)
                    p++;
                n = (size_t)(te - tl);
                if (n && tl[n - 1] == '\r')
                    n--;
                if (n == 0)
                    break;                  /* blank line ends the cue */
                if (tlen + n + 2 < sizeof(text)) {
                    if (tlen)
                        text[tlen++] = '\n';
                    memcpy(text + tlen, tl, n);
                    tlen += n;
                    text[tlen] = '\0';
                }
            }

            add_cue(s, start, stop, text);
            tlen = 0;
            text[0] = '\0';
        }
    }

    if (s->count > 1)
        qsort(s->cues, (size_t)s->count, sizeof(*s->cues), cmp_cue);
    return s->count;
}

static int load_file(SubSource *s, const char *path, const char *name)
{
    FILE  *f;
    char  *buf;
    long   size;
    size_t got;

    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    size = ftell(f);
    if (size <= 0 || size > 4 * 1024 * 1024) {
        fclose(f);
        return 0;
    }
    rewind(f);
    buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return 0;
    }
    got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';

    /* A BOM would sit in front of "WEBVTT" or the first timestamp. */
    {
        const char *start = buf;
        if (got >= 3 && (unsigned char)start[0] == 0xEF &&
            (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
            start += 3;
            got   -= 3;
        }
        parse_buffer(s, start, got);
    }
    free(buf);

    if (s->count == 0) {
        free_source(s);
        return 0;
    }
    snprintf(s->name, sizeof(s->name), "%.95s", name);
    return 1;
}

/* ---------------------------------------------------------------- public -- */

void sub_close(void)
{
    int i;

    for (i = 0; i < SUB_MAX_SOURCES; i++)
        free_source(&s_src[i]);
    s_count   = 0;
    s_index   = -1;
    s_enabled = 0;
}

int sub_open(const char *video_path)
{
    char  dir[512], stem[256], path[768];
    const char *slash, *base, *dot;
    DIR   *d;
    struct dirent *e;
    char  names[SUB_MAX_SOURCES][96];
    int   found = 0, i, j;
    size_t stem_len;

    sub_close();
    if (!video_path || !video_path[0])
        return 0;

    slash = strrchr(video_path, '/');
    if (slash) {
        size_t n = (size_t)(slash - video_path);
        if (n >= sizeof(dir))
            return 0;
        memcpy(dir, video_path, n);
        dir[n] = '\0';
        base = slash + 1;
    } else {
        snprintf(dir, sizeof(dir), ".");
        base = video_path;
    }

    dot = strrchr(base, '.');
    if (dot && dot != base)
        stem_len = (size_t)(dot - base);
    else
        stem_len = strlen(base);
    if (stem_len == 0 || stem_len >= sizeof(stem))
        return 0;
    memcpy(stem, base, stem_len);
    stem[stem_len] = '\0';

    d = opendir(dir[0] ? dir : ".");
    if (!d)
        return 0;

    while ((e = readdir(d)) != NULL && found < SUB_MAX_SOURCES) {
        const char *nm = e->d_name;
        int is_sub = ends_with_ci(nm, ".srt") || ends_with_ci(nm, ".vtt");

        if (!is_sub || strncasecmp(nm, stem, stem_len) != 0)
            continue;
        /* "movie.srt" or "movie.en.srt", never "movie2.srt" */
        if (nm[stem_len] != '.' && nm[stem_len] != '\0')
            continue;
        snprintf(names[found], sizeof(names[found]), "%s", nm);
        found++;
    }
    closedir(d);

    /* directory order is arbitrary; keep the list stable */
    for (i = 1; i < found; i++) {
        char key[96];
        snprintf(key, sizeof(key), "%s", names[i]);
        for (j = i; j > 0 && strcasecmp(names[j - 1], key) > 0; j--)
            snprintf(names[j], sizeof(names[j]), "%s", names[j - 1]);
        snprintf(names[j], sizeof(names[j]), "%s", key);
    }

    {
        int loaded = 0;
        for (i = 0; i < found; i++) {
            snprintf(path, sizeof(path), "%s/%.95s", dir[0] ? dir : ".", names[i]);
            if (load_file(&s_src[loaded], path, names[i]))
                loaded++;
        }
        s_count = loaded;
    }

    if (s_count > 0) {
        s_index   = 0;
        s_enabled = 1;
    }
    return s_count;
}

int sub_count(void)
{
    return s_count;
}

const char *sub_name(int index)
{
    if (index < 0 || index >= s_count)
        return "";
    return s_src[index].name;
}

int sub_index(void)
{
    return s_index;
}

void sub_set_enabled(int on)
{
    s_enabled = (on && s_count > 0) ? 1 : 0;
    if (s_enabled && s_index < 0)
        s_index = 0;
}

int sub_enabled(void)
{
    return s_enabled;
}

int sub_cycle(void)
{
    if (s_count == 0) {
        s_enabled = 0;
        return 0;
    }
    if (!s_enabled) {
        s_enabled = 1;
        if (s_index < 0)
            s_index = 0;
        return 1;
    }
    if (s_index + 1 < s_count) {
        s_index++;
        return 1;
    }
    s_enabled = 0;
    return 0;
}

const char *sub_text_at(double seconds)
{
    const SubSource *s;
    long  t;
    int   lo, hi, best;
    int   i;

    if (!s_enabled || s_index < 0 || s_index >= s_count)
        return NULL;
    s = &s_src[s_index];
    if (s->count == 0)
        return NULL;

    t = (long)(seconds * 1000.0);
    if (t < 0)
        t = 0;

    /* rightmost cue that has started */
    lo = 0;
    hi = s->count - 1;
    best = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (s->cues[mid].start_ms <= t) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    if (best < 0)
        return NULL;

    /* overlapping cues are legal; show the most recent one that still runs */
    for (i = best; i >= 0 && i > best - 8; i--) {
        if (t >= s->cues[i].start_ms && t < s->cues[i].end_ms)
            return s->cues[i].text;
    }
    return NULL;
}
