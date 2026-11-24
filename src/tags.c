/* tags.c - read the ID3 tag of an MP3.
 *
 * Everything here works on a file the library is scanning, so it must be
 * cheap and it must never fail loudly: a truncated tag, a wrong version or a
 * frame that lies about its own size ends the parse and leaves what was
 * already read in place.
 *
 * Both tag generations are read. ID3v2 sits at the front and is what every
 * modern ripper writes; ID3v1 is the fixed 128-byte block at the end and is
 * still the only tag in a lot of old MP3s. A file with a v2 tag that carries
 * no genre is allowed to take the genre from v1.
 */
#include "tags.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A tag that is bigger than this is almost always cover art. Text frames come
 * first in every real ripper's output, so reading this much finds the words
 * without pulling a 4 MB JPEG off the drive for every file. */
#define TAG_READ_CAP (64 * 1024)

static const char *const k_genres[] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge",
    "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B",
    "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska",
    "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient",
    "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
    "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
    "Alternative Rock", "Bass", "Soul", "Punk", "Space", "Meditative",
    "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave",
    "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream",
    "Southern Rock", "Comedy", "Cult", "Gangsta", "Top 40", "Christian Rap",
    "Pop/Funk", "Jungle", "Native US", "Cabaret", "New Wave", "Psychedelic",
    "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk",
    "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll", "Hard Rock",
    "Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebop",
    "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock",
    "Big Band", "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech",
    "Chanson", "Opera", "Chamber Music", "Sonata", "Symphony", "Booty Bass",
    "Primus", "Porn Groove", "Satire", "Slow Jam", "Club", "Tango", "Samba",
    "Folklore", "Ballad", "Power Ballad", "Rhythmic Soul", "Freestyle",
    "Duet", "Punk Rock", "Drum Solo", "A Cappella", "Euro-House",
    "Dance Hall"
};
#define GENRE_COUNT ((int)(sizeof(k_genres) / sizeof(k_genres[0])))

/* ------------------------------------------------------------- helpers -- */

static void put_utf8(char *out, int *n, int cap, unsigned cp)
{
    if (*n + 5 >= cap)
        return;
    if (cp < 0x80) {
        out[(*n)++] = (char)cp;
    } else if (cp < 0x800) {
        out[(*n)++] = (char)(0xC0 | (cp >> 6));
        out[(*n)++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out[(*n)++] = (char)(0xE0 | (cp >> 12));
        out[(*n)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*n)++] = (char)(0x80 | (cp & 0x3F));
    } else {
        out[(*n)++] = (char)(0xF0 | (cp >> 18));
        out[(*n)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[(*n)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*n)++] = (char)(0x80 | (cp & 0x3F));
    }
}

/* Trailing spaces and control characters are what a padded tag field looks
 * like once decoded; they would make two spellings of one artist two rows. */
static void tidy(char *s)
{
    size_t n = strlen(s), i;

    for (i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20)
            s[i] = ' ';
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
    while (*s == ' ')
        memmove(s, s + 1, strlen(s));
}

/* One text frame body: an encoding byte and then the string. */
static void decode_text(const unsigned char *p, int len, char *out, int cap)
{
    int enc, i, n = 0;

    out[0] = '\0';
    if (len <= 0)
        return;
    enc = p[0];
    p++;
    len--;

    if (enc == 1 || enc == 2) {
        int le = 1;

        if (enc == 1 && len >= 2) {
            if (p[0] == 0xFF && p[1] == 0xFE) {
                p += 2; len -= 2; le = 1;
            } else if (p[0] == 0xFE && p[1] == 0xFF) {
                p += 2; len -= 2; le = 0;
            }
        }
        if (enc == 2)
            le = 0;
        for (i = 0; i + 1 < len; i += 2) {
            unsigned cp = le ? ((unsigned)p[i] | ((unsigned)p[i + 1] << 8))
                             : (((unsigned)p[i] << 8) | (unsigned)p[i + 1]);

            if (cp == 0)
                break;
            put_utf8(out, &n, cap, cp);
        }
    } else if (enc == 3) {
        for (i = 0; i < len && p[i]; i++)
            if (n + 5 < cap)
                out[n++] = (char)p[i];
    } else {
        for (i = 0; i < len && p[i]; i++) {
            unsigned cp = p[i];

            if (cp < 0x80) {
                if (n + 5 < cap)
                    out[n++] = (char)cp;
            } else {
                put_utf8(out, &n, cap, cp);
            }
        }
    }
    out[n] = '\0';
    tidy(out);
}

/* "(17)" and "17" both mean Rock in a TCON frame; "17Rock" means the same. */
static void genre_from_text(const char *raw, char *out, int cap)
{
    int n = 0;

    if (raw[0] == '(') {
        int v = 0;

        while (raw[1] >= '0' && raw[1] <= '9') {
            v = v * 10 + (raw[1] - '0');
            raw++;
        }
        if (v >= 0 && v < GENRE_COUNT) {
            snprintf(out, (size_t)cap, "%s", k_genres[v]);
            return;
        }
        if (raw[0] == ')')
            raw++;
    } else {
        while (raw[0] >= '0' && raw[0] <= '9') {
            n = n * 10 + (raw[0] - '0');
            raw++;
        }
        if (n > 0) {
            if (n >= 0 && n < GENRE_COUNT) {
                snprintf(out, (size_t)cap, "%s", k_genres[n]);
                return;
            }
        }
    }
    snprintf(out, (size_t)cap, "%s", raw);
    tidy(out);
}

static int synchsafe(const unsigned char *p)
{
    return ((int)(p[0] & 0x7F) << 21) | ((int)(p[1] & 0x7F) << 14) |
           ((int)(p[2] & 0x7F) << 7)  |  (int)(p[3] & 0x7F);
}

/* --------------------------------------------------------------- ID3v2 -- */

static void parse_v2(const unsigned char *d, int len, Tags *t)
{
    int ver = d[3];
    int flags = d[5];
    int pos = 10;

    if (len < 11)
        return;
    if (flags & 0x40) {                 /* extended header, skip it */
        int skip = (ver >= 4) ? synchsafe(d + 10) : 0;

        if (ver >= 4) {
            pos += skip > 0 ? skip : 6;
        } else {
            pos += 4 + (((int)d[10] << 24) | ((int)d[11] << 16) |
                        ((int)d[12] << 8) | d[13]);
        }
        if (pos < 10 || pos >= len)
            return;
    }

    while (pos + (ver == 2 ? 6 : 10) <= len) {
        const unsigned char *h = d + pos;
        char  id[5];
        int   fsz, hdr;

        if (ver == 2) {
            id[0] = (char)h[0]; id[1] = (char)h[1]; id[2] = (char)h[2]; id[3] = 0;
            fsz = ((int)h[3] << 16) | ((int)h[4] << 8) | h[5];
            hdr = 6;
        } else {
            memcpy(id, h, 4);
            id[4] = 0;
            fsz = (ver >= 4) ? synchsafe(h + 4)
                             : (((int)h[4] << 24) | ((int)h[5] << 16) |
                                ((int)h[6] << 8) | h[7]);
            if (fsz <= 0 && ver >= 4)       /* some writers store plain sizes */
                fsz = ((int)h[4] << 24) | ((int)h[5] << 16) |
                      ((int)h[6] << 8) | h[7];
            hdr = 10;
        }
        if (id[0] < 'A' || id[0] > 'Z' || fsz <= 0 || pos + hdr + fsz > len)
            break;

        if (!strcmp(id, "TIT2") || !strcmp(id, "TT2")) {
            decode_text(h + hdr, fsz, t->title, TAG_LEN);
            t->have_id3 = 1;
        } else if (!strcmp(id, "TPE1") || !strcmp(id, "TP1") ||
                   (!t->artist[0] && (!strcmp(id, "TPE2") || !strcmp(id, "TP2")))) {
            decode_text(h + hdr, fsz, t->artist, TAG_LEN);
            t->have_id3 = 1;
        } else if (!strcmp(id, "TALB") || !strcmp(id, "TAL")) {
            decode_text(h + hdr, fsz, t->album, TAG_LEN);
            t->have_id3 = 1;
        } else if (!strcmp(id, "TCON") || !strcmp(id, "TCO")) {
            char raw[TAG_LEN];

            decode_text(h + hdr, fsz, raw, sizeof(raw));
            genre_from_text(raw, t->genre, TAG_LEN);
            t->have_id3 = 1;
        } else if (!strcmp(id, "TRCK") || !strcmp(id, "TRK")) {
            char raw[32];
            int  v = 0, i;

            decode_text(h + hdr, fsz, raw, sizeof(raw));
            for (i = 0; raw[i] >= '0' && raw[i] <= '9'; i++)
                v = v * 10 + (raw[i] - '0');
            t->track = v;
            t->have_id3 = 1;
        }
        pos += hdr + fsz;

        if (t->title[0] && t->artist[0] && t->album[0] && t->genre[0])
            return;                     /* everything wanted is in */
    }
}

/* --------------------------------------------------------------- ID3v1 -- */

static void parse_v1(FILE *f, Tags *t)
{
    unsigned char b[128];

    if (fseek(f, -128, SEEK_END) != 0)
        return;
    if (fread(b, 1, sizeof(b), f) != sizeof(b))
        return;
    if (memcmp(b, "TAG", 3) != 0)
        return;

    if (!t->title[0])  { memcpy(t->title, b + 3, 30);  t->title[30] = 0; tidy(t->title); }
    if (!t->artist[0]) { memcpy(t->artist, b + 33, 30); t->artist[30] = 0; tidy(t->artist); }
    if (!t->album[0])  { memcpy(t->album, b + 63, 30);  t->album[30] = 0; tidy(t->album); }
    if (!t->genre[0] && b[127] < GENRE_COUNT)
        snprintf(t->genre, TAG_LEN, "%s", k_genres[b[127]]);
    t->have_id3 = 1;
}

/* --------------------------------------------------------------- entry -- */

void tags_read(const char *path, Tags *t)
{
    FILE          *f;
    unsigned char *buf = NULL;
    unsigned char  head[10];
    size_t         got;

    memset(t, 0, sizeof(*t));
    f = fopen(path, "rb");
    if (!f)
        return;

    got = fread(head, 1, sizeof(head), f);
    if (got == sizeof(head) && head[0] == 'I' && head[1] == 'D' &&
        head[2] == '3') {
        int size = ((int)(head[6] & 0x7F) << 21) | ((int)(head[7] & 0x7F) << 14) |
                   ((int)(head[8] & 0x7F) << 7)  |  (int)(head[9] & 0x7F);
        int want = size + 10;

        if (want > TAG_READ_CAP)
            want = TAG_READ_CAP;
        if (want < 11)
            want = 11;
        buf = (unsigned char *)malloc((size_t)want);
        if (buf) {
            size_t n;

            rewind(f);
            n = fread(buf, 1, (size_t)want, f);
            if (n >= 11)
                parse_v2(buf, (int)n, t);
            free(buf);
        }
    }
    parse_v1(f, t);
    fclose(f);
}

int tags_split_name(const char *name, char *artist, char *title, int sz)
{
    char  base[TAG_LEN];
    char *sep;
    int   i = 0, n;

    artist[0] = title[0] = '\0';
    n = (int)strlen(name);
    if (n >= 4 && !strcasecmp(name + n - 4, ".mp3"))
        n -= 4;
    if (n >= (int)sizeof(base))
        n = (int)sizeof(base) - 1;
    memcpy(base, name, (size_t)n);
    base[n] = '\0';

    sep = strstr(base, " - ");
    if (!sep)
        return 0;
    *sep = '\0';
    /* "01 Artist - Title" numbers the artist by accident: drop the track no */
    while (base[i] >= '0' && base[i] <= '9')
        i++;
    if (i > 0 && (base[i] == ' ' || base[i] == '.' || base[i] == '-'))
        while (base[i] == ' ' || base[i] == '.' || base[i] == '-')
            i++;
    snprintf(artist, (size_t)sz, "%s", base + (i > 0 ? i : 0));
    snprintf(title, (size_t)sz, "%s", sep + 3);
    tidy(artist);
    tidy(title);
    return artist[0] && title[0];
}
