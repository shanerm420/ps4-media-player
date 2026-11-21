/* ui.h - all screen drawing for the player, in one place.
 *
 * The renderer is plain SDL2 with no assets: backgrounds, panels, bars and text
 * are drawn from primitives plus an 8x8 bitmap font, so the look is identical
 * on a PS4 and in the host preview (preview/preview.c renders this exact code
 * to a PNG on a PC).
 *
 * Everything is laid out for a 1920x1080 surface and scaled by ui_draw()'s
 * width argument, so a different output size still fills the screen.
 */
#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>

#include "eq.h"

typedef struct {
    const char *name;
    int         is_video;
} UiTrack;

/* One row of the drive browser: a mount, a folder, a media file or an action
 * such as "refresh". */
typedef struct {
    const char *name;
    int         is_dir;
    int         is_video;
    int         is_drive;      /* a mounted drive, not a folder inside one */
    int         is_action;     /* refresh / library row */
} UiEntry;

#define UI_SCREEN_LIBRARY 0
#define UI_SCREEN_BROWSER 1

typedef struct {
    const char *app_title;

    const UiTrack *tracks;
    int            track_count;
    int            selected;      /* highlighted row */
    int            current;       /* playing row, -1 for none */

    int            playing;       /* 1 = audio is advancing */
    int            have_track;    /* a track is loaded (even if paused) */

    double         elapsed_sec;
    double         duration_sec;  /* <= 0 when unknown */

    const char    *format_note;   /* "MP3  44100 HZ  STEREO" */
    const char    *status;        /* one-line message under the title */

    /* drive / folder browser */
    int             screen;          /* UI_SCREEN_LIBRARY or _BROWSER */
    const char     *browser_title;   /* "DRIVES" or the current path */
    const char     *browser_note;    /* "3 MOUNTED", "12 ITEMS", ... */
    const UiEntry  *entries;
    int             entry_count;
    int             entry_selected;

    int            volume_db;
    float          volume_frac;   /* 0..1 across the volume bar */

    int            eq_open;
    int            eq_band;       /* 0..EQ_BANDS-1, EQ_BANDS = preamp */
    float          eq_gain_db[EQ_BANDS];
    float          eq_preamp_db;

    /* video picture and subtitles */
    int            video_aspect;     /* UI_ASPECT_FIT / FILL / STRETCH */
    int            video_brightness; /* -100..100, 0 leaves the picture alone */
    int            video_hud;        /* 1 = draw the transport bar */
    const char    *sub_text;         /* cue on screen now, '\n'-separated */
    int            sub_size;         /* UI_SUB_SMALL / NORMAL / LARGE */

    /* video settings overlay (Triangle while a video plays) */
    int            video_settings_open;
    int            video_settings_row;   /* UI_VIDEO_ROW_* */
    const char    *sub_label;        /* "OFF", "NONE FOUND", or a file name */

    /* short confirmation drawn over the video ("SUBTITLES ON"), faded by the
     * caller so the UI needs no clock of its own */
    const char    *video_toast;
    int            video_toast_alpha;   /* 0..255 */
} UiModel;

#define UI_ASPECT_FIT      0
#define UI_ASPECT_FILL     1
#define UI_ASPECT_STRETCH  2
#define UI_ASPECT_COUNT    3

#define UI_SUB_SMALL       0
#define UI_SUB_NORMAL      1
#define UI_SUB_LARGE       2
#define UI_SUB_SIZE_COUNT  3

#define UI_VIDEO_ROW_ASPECT     0
#define UI_VIDEO_ROW_BRIGHTNESS 1
#define UI_VIDEO_ROW_SUBTITLES  2
#define UI_VIDEO_ROW_SUB_SIZE   3
#define UI_VIDEO_ROWS           4

/* Draws the whole frame. w/h are the surface size; design size is 1920x1080. */
void ui_draw(SDL_Renderer *r, const UiModel *m, int w, int h);

/* Full-screen video: the frame letterboxed under a slim transport bar.
 * frame may be NULL (nothing decoded yet), in which case the overlay is drawn
 * over black. */
void ui_draw_video(SDL_Renderer *r, SDL_Texture *frame, int fw, int fh,
                   const UiModel *m, int w, int h);

#endif /* UI_H */
