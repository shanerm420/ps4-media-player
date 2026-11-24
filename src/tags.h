/* tags.h - the words stored inside a music file.
 *
 * The library used to be a list of file names, which is fine for a handful of
 * songs and useless for a real collection. This reads the tags out of an MP3
 * (ID3v2.2/2.3/2.4, then ID3v1 as a fallback) so artists, albums and genres
 * can be browsed.
 */
#ifndef TAGS_H
#define TAGS_H

#define TAG_LEN 96

typedef struct {
    char title[TAG_LEN];
    char artist[TAG_LEN];
    char album[TAG_LEN];
    char genre[TAG_LEN];
    int  track;         /* track number, 0 when the file does not say */
    int  have_id3;      /* 1 = a tag was found and at least one field read */
} Tags;

/* Clears t, then fills what the file actually carries. Never fails to return:
 * a file with no tag at all just leaves the fields empty for the caller to
 * fill from the file name. */
void tags_read(const char *path, Tags *t);

/* "Artist - Title" out of a file name. Returns 1 when a split was found. */
int tags_split_name(const char *name, char *artist, char *title, int sz);

#endif /* TAGS_H */
