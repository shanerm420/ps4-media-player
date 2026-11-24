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
#include "spectrum.h"

typedef struct {
    const char *name;
    const char *sub;        /* right-hand tag: "12 TRACKS", an artist, "MP3" */
    int         is_video;
    int         is_category;/* an artist / album / genre row, not a song */
} UiTrack;

/* One row of the drive browser: a mount, a folder, a media file or an action
 * such as "refresh". */
typedef struct {
    const char *name;
    int         is_dir;
    int         is_video;
    int         is_drive;      /* a mounted drive, not a folder inside one */
    int         is_action;     /* refresh / library row */
    int         is_picture;
    int         is_other;      /* a file the player cannot open, listed so it
                                * can still be copied, moved or deleted */
} UiEntry;

#define UI_SCREEN_HOME     0
#define UI_SCREEN_MUSIC    1
#define UI_SCREEN_VIDEOS   2
#define UI_SCREEN_PICTURES 3
#define UI_SCREEN_BROWSER  4
#define UI_SCREEN_LIBRARY  UI_SCREEN_MUSIC   /* old name, same screen */

typedef struct {
    const char *app_title;

    const UiTrack *tracks;        /* the current section's list */
    int            track_count;
    int            selected;      /* highlighted row */
    int            current;       /* playing row, -1 for none */

    /* music browsing: TRACKS / ARTISTS / ALBUMS / GENRES, and how far into
     * one of them the list has been opened */
    int            music_view;
    const char    *music_view_name;
    const char    *music_crumb;   /* the artist or album being looked at */
    int            music_level;

    int            playing;       /* 1 = audio is advancing */
    int            have_track;    /* a track is loaded (even if paused) */

    double         elapsed_sec;
    double         duration_sec;  /* <= 0 when unknown */

    const char    *format_note;   /* "MP3  44100 HZ  STEREO" */
    const char    *status;        /* one-line message under the title */

    /* drive / folder browser */
    int             screen;          /* UI_SCREEN_* */
    const char     *browser_title;   /* "DRIVES" or the current path */
    const char     *browser_note;    /* "3 MOUNTED", "12 ITEMS", ... */
    const UiEntry  *entries;
    int             entry_count;
    int             entry_selected;

    int            volume_db;
    float          volume_frac;   /* 0..1 across the volume bar */
    int            volume_muted;

    int            eq_open;
    int            eq_band;       /* 0..EQ_BANDS-1, EQ_BANDS = preamp */
    float          eq_gain_db[EQ_BANDS];
    float          eq_preamp_db;
    int            eq_preset;     /* EQ_PRESET_*, CUSTOM when hand-tuned */
    int            eq_preset_open;/* the header's preset list is dropped down */
    int            eq_preset_sel; /* highlighted row while it is open */

    /* home screen */
    int            home_selected;
    int            music_count;
    int            video_count;
    int            picture_count;

    /* music section: the visualizer, fed from the audio thread */
    int            vis_style;
    int            vis_fullscreen;   /* L3: the visualiser fills the screen */
    float          vis_bands[VIS_BANDS];
    float          vis_wave[VIS_WAVE];
    float          vis_level;

    /* video picture and subtitles */
    int            video_aspect;     /* UI_ASPECT_FIT / FILL / STRETCH */
    int            video_brightness; /* -100..100, 0 leaves the picture alone */
    int            video_saturation; /* 0..250, 100 is the decoder's own color */
    int            video_sharpness;  /* 0..100, 0 leaves the luma alone */
    int            video_hud;        /* 1 = draw the transport bar */
    int            video_fullscreen; /* 1 = preview fills the screen */
    const char    *sub_text;         /* cue on screen now, '\n'-separated */
    int            sub_size;         /* UI_SUB_SMALL / NORMAL / LARGE */

    /* video settings overlay (OPTIONS while a video plays) */
    int            video_settings_open;
    int            video_settings_row;   /* UI_VIDEO_ROW_* */
    const char    *sub_label;        /* "OFF", "NONE FOUND", or a file name */

    /* short confirmation drawn over the video ("SUBTITLES ON"), faded by the
     * caller so the UI needs no clock of its own */
    const char    *video_toast;
    int            video_toast_alpha;

    /* picture viewer */
    int            pic_count;
    int            pic_selected;
    const char    *pic_name;
    const char    *pic_note;         /* "1920 x 1080" */
    int            pic_loaded;       /* a texture is available */
    int            pic_fit;          /* UI_ASPECT_FIT / FILL / STRETCH */
    int            pic_brightness;   /* -100..100 */
    int            pic_saturation;   /* 0..250 */
    int            pic_sharpness;    /* 0..100 */
    int            pic_rotation;     /* 0..3, x90 degrees */
    int            pic_zoom;         /* 100..400 (%) */
    int            pic_pan_x;        /* -1000..1000, across the zoomed image */
    int            pic_pan_y;
    int            pic_zoom_mode;    /* 1 = the right stick pans */
    int            pic_settings_open;
    int            pic_settings_row; /* UI_PIC_ROW_* */
    int            pic_fullscreen;   /* the photo fills the screen */
    int            pic_rb_swap;      /* red and blue reversed for this file */

    /* theme / skin, and the file manager's state */
    const char    *theme_name;
    int            usb_present;     /* a storage device is in a USB slot */
    int            usb_count;
    int            confirm_delete;  /* the delete prompt is on screen */
    const char    *confirm_name;
    const char    *clip_note;       /* "COPIED" / "CUT" / "" */
    const char    *clip_name;
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
#define UI_VIDEO_ROW_SATURATION 2
#define UI_VIDEO_ROW_SHARPNESS  3
#define UI_VIDEO_ROW_SUBTITLES  4
#define UI_VIDEO_ROW_SUB_SIZE   5
#define UI_VIDEO_ROWS           6

#define UI_PIC_ROW_FIT        0
#define UI_PIC_ROW_BRIGHTNESS 1
#define UI_PIC_ROW_SATURATION 2
#define UI_PIC_ROW_SHARPNESS  3
#define UI_PIC_ROW_ROTATION   4
#define UI_PIC_ROW_ZOOM       5
#define UI_PIC_ROW_COLOR      6
#define UI_PIC_ROWS           7

/* Themes (skins): ORIGINAL is the palette the player shipped with, the rest
 * recolour the same layout. The chosen index is kept in viewer.cfg. */
int         ui_theme_count(void);
const char *ui_theme_name(int index);
void        ui_theme_set(int index);

/* Draws the whole frame for the menu screens (HOME, MUSIC, BROWSER). */
void ui_draw(SDL_Renderer *r, const UiModel *m, int w, int h);

/* Full-screen video: the frame under a transport bar. frame may be NULL
 * (nothing decoded yet), in which case the overlay is drawn over black. */
void ui_draw_video(SDL_Renderer *r, SDL_Texture *frame, int fw, int fh,
                   const UiModel *m, int w, int h);

/* The video section: the file list on the left, the preview player on the
 * right, with the OSD and the settings panel in place. */
void ui_draw_video_section(SDL_Renderer *r, SDL_Texture *frame, int fw, int fh,
                           const UiModel *m, int w, int h);

/* The picture section: the file list on the left, the picture on the right. */
void ui_draw_pictures(SDL_Renderer *r, SDL_Texture *tex, int tw, int th,
                      const UiModel *m, int w, int h);

/* L3 in the music section: the visualiser on the whole screen, with the track
 * name and clock above it. The equalizer draws over it as usual. */
void ui_draw_visualizer_full(SDL_Renderer *r, const UiModel *m, int w, int h);

#endif /* UI_H */
