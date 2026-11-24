/* preview.c - renders src/ui.c to a BMP on a PC, with no PS4 involved.
 *
 * The point is to look at the real drawing code instead of guessing what it
 * does on a television. It builds fixed demo models (long file names, a video
 * entry, a playing track) and writes one frame per screen with SDL's dummy
 * video driver.
 *
 *   make -C preview run   -> out-home.bmp, out-music.bmp, out-videos.bmp,
 *                            out-video.bmp, out-settings.bmp, out-pictures.bmp,
 *                            out-picture-settings.bmp, out-browser.bmp,
 *                            out-eq.bmp, plus 720p and 1440p scaling checks
 *
 * The video and picture shots use synthetic images (a painted dusk scene and a
 * bright coast scene), since there is no decoder on the host. They are there to
 * judge the layout, the letterboxing and the overlays in place.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "../src/ui.h"

/* ui.c reports a picture geometry change through plog(); the host build links
 * src/applog.c, whose log is closed unless log_open() was called, so those
 * lines are dropped here exactly as before. */

#define W 1920
#define H 1080

enum {
    SHOT_HOME, SHOT_MUSIC, SHOT_VIDEOS, SHOT_VIDEO, SHOT_SETTINGS,
    SHOT_PICTURES, SHOT_PIC_SETTINGS, SHOT_BROWSER, SHOT_EQ,
    SHOT_PIC_FULL, SHOT_CONFIRM, SHOT_THEME, SHOT_VIS,
    SHOT_VIS_FULL, SHOT_EQ_PRESET, SHOT_PIC_FULL_SETTINGS
};

/* the visualizer style the SHOT_VIS frame uses */
static int g_vis_style;

static const UiTrack songs[] = {
    { "Midnight Drive (Extended Mix)",  "THE NEON DIVISION",  0, 0 },
    { "Neon Skyline",                   "THE NEON DIVISION",  0, 0 },
    { "Organ Suite in D minor",         "BASILICA",           0, 0 },
    { "Short",                          "VARIOUS ARTISTS",    0, 0 },
    { "A Really Long Track Name That Should Be Cut With An Ellipsis",
                                        "THE LONG PLAYERS",   0, 0 },
    { "Harbor Lights",                  "KITE ENSEMBLE",      0, 0 },
    { "Slow Motion",                    "KITE ENSEMBLE",      0, 0 },
    { "Encore",                         "BASILICA",           0, 0 },
};

/* the ARTISTS list, and one artist's albums */
static const UiTrack artists[] = {
    { "BASILICA",           "42 TRACKS",   0, 1 },
    { "KITE ENSEMBLE",      "18 TRACKS",   0, 1 },
    { "THE LONG PLAYERS",   "9 TRACKS",    0, 1 },
    { "THE NEON DIVISION",  "126 TRACKS",  0, 1 },
    { "VARIOUS ARTISTS",    "214 TRACKS",  0, 1 },
};

static const UiTrack albums[] = {
    { "Cathedral Works",  "BASILICA",         0, 1 },
    { "Harbor Sessions",  "KITE ENSEMBLE",    0, 1 },
    { "Midnight Drive",   "THE NEON DIVISION", 0, 1 },
    { "Night Roads",      "THE NEON DIVISION", 0, 1 },
    { "Slow Motion",      "KITE ENSEMBLE",    0, 1 },
};

static const UiTrack videos[] = {
    { "Concert Footage 2019.mp4",                 "VIDEO", 1, 0 },
    { "The Long Way Home (Director's Cut).mp4",   "VIDEO", 1, 0 },
    { "Slow Motion Test.mov",                     "VIDEO", 1, 0 },
    { "Holiday 2024.mp4",                         "VIDEO", 1, 0 },
    { "An Extremely Long Video File Name That Runs Out Of Room.mp4",
                                                   "VIDEO", 1, 0 },
};

static const UiTrack photos[] = {
    { "IMG_0001.JPG",              "PHOTO", 0, 0 },
    { "IMG_0002.JPG",              "PHOTO", 0, 0 },
    { "Sunset at the pier.jpg",    "PHOTO", 0, 0 },
    { "Family BBQ 2023.png",       "PHOTO", 0, 0 },
    { "IMG_0009.JPG",              "PHOTO", 0, 0 },
    { "Screenshot 2026-11-01.bmp", "PHOTO", 0, 0 },
};

static const UiEntry demo_entries[] = {
    { "INTERNAL DRIVE",      1, 0, 1, 0, 0, 0 },
    { "USB 1",               1, 0, 1, 0, 0, 0 },
    { "Holiday 2024.mp4",    0, 1, 0, 0, 0, 0 },
    { "Sunset at the pier.jpg", 0, 0, 0, 0, 1, 0 },
    { "track notes.txt",     0, 0, 0, 0, 0, 1 },
    { "REFRESH DRIVE SCAN",  0, 0, 0, 1, 0, 0 },
};

static void base(UiModel *m)
{
    int i;

    memset(m, 0, sizeof(*m));
    m->app_title     = "ULTIMATE PS4 MEDIA PLAYER";
    m->volume_db     = -9;
    m->volume_frac   = (-9.0f + 60.0f) / 72.0f;
    m->music_count   = (int)(sizeof(songs)/sizeof(songs[0]));
    m->video_count   = (int)(sizeof(videos)/sizeof(videos[0]));
    m->picture_count = (int)(sizeof(photos)/sizeof(photos[0]));
    m->music_count   = 35;
    m->home_selected = 1;
    m->sub_label     = "CONCERT.EN.SRT (1/2)";
    m->sub_size      = UI_SUB_NORMAL;

    /* a plausible spectrum, so the meter has shape instead of a flat row */
    for (i = 0; i < VIS_BANDS; i++) {
        float t = (float)i / (float)VIS_BANDS;

        m->vis_bands[i] = 0.28f + 0.52f * (float)fabs(sin((double)i * 0.55))
                        * (1.0f - 0.55f * t);
        if (i < 4)
            m->vis_bands[i] = 0.82f - (float)i * 0.07f;
    }
    m->vis_bands[30] = 0.2f;
    m->vis_bands[31] = 0.14f;
    m->vis_level = 0.7f;
    for (i = 0; i < VIS_WAVE; i++) {
        double t = (double)i / VIS_WAVE * 6.0 * 3.14159265;

        m->vis_wave[i] = (float)(sin(t * 7.0) * 0.35 * (0.4 + 0.6 * sin(t * 0.7)));
    }
    m->vis_style = VIS_STYLE_SPECTRUM;
    m->music_view      = 0;
    m->music_view_name = "TRACKS";
    m->music_crumb     = "";
    m->music_level     = 0;
}

static void music_model(UiModel *m)
{
    int i;

    base(m);
    m->screen       = UI_SCREEN_MUSIC;
    m->tracks       = songs;
    m->track_count  = (int)(sizeof(songs)/sizeof(songs[0]));
    m->selected     = 2;
    m->current      = 2;
    m->playing      = 1;
    m->have_track   = 1;
    m->elapsed_sec  = 97.0;
    m->duration_sec = 311.0;
    m->format_note  = "MP3   44100 HZ   STEREO";
    m->status       = "Basilica - Organ Suite in D minor.mp3   44100 Hz stereo";

    for (i = 0; i < EQ_BANDS; i++)
        m->eq_gain_db[i] = 0.0f;
    m->eq_gain_db[0] = 6.0f;
    m->eq_gain_db[9] = -2.0f;
    m->eq_preamp_db  = -4.0f;
    m->eq_band       = 3;
}

static void video_model(UiModel *m)
{
    base(m);
    m->tracks       = videos;
    m->track_count  = (int)(sizeof(videos)/sizeof(videos[0]));
    m->selected     = 1;
    m->current      = 1;
    m->playing      = 1;
    m->have_track   = 1;
    m->elapsed_sec  = 42.0;
    m->duration_sec = 137.0;
    m->format_note  = "H.264   1280 x 720";
    m->status       = "The Long Way Home (Director's Cut).mp4";
    m->video_aspect     = UI_ASPECT_FIT;
    m->video_brightness = 12;
    m->video_saturation = 120;
    m->video_sharpness  = 20;
    m->video_hud        = 1;
    m->sub_text         = "The hills were already dark when the\nlast train came through.";
}

static void picture_model(UiModel *m)
{
    base(m);
    m->screen       = UI_SCREEN_PICTURES;
    m->tracks       = photos;
    m->track_count  = (int)(sizeof(photos)/sizeof(photos[0]));
    m->selected     = 2;
    m->current      = 2;
    m->pic_count    = m->track_count;
    m->pic_selected = 2;
    m->pic_name     = "Sunset at the pier.jpg";
    m->pic_note     = "1600 x 1000";
    m->pic_loaded   = 1;
    m->pic_fit      = UI_ASPECT_FIT;
    m->pic_brightness = 8;
    m->pic_saturation = 130;
    m->pic_sharpness  = 15;
    m->pic_rotation   = 0;
    m->pic_zoom       = 100;
    m->status         = "Sunset at the pier.jpg   1600x1000";
}

/* The video stand-in: dusk sky, sun, hills, so the picture and the transport
 * bar can be judged against dark and bright areas. */
static void paint_video(SDL_Surface *surf)
{
    uint32_t *px = surf->pixels;
    int       stride = surf->pitch / 4;
    int x, y;

    for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
            int r, g, b;

            if (y < surf->h * 3 / 5) {
                float t = (float)y / (float)(surf->h * 3 / 5);

                r = (int)(26 + 190 * t * t);
                g = (int)(34 + 96 * t * t);
                b = (int)(76 + 40 * (1.0f - t));
            } else {
                float t = (float)(y - surf->h * 3 / 5) / (float)(surf->h * 2 / 5);

                r = (int)(30 - 16 * t);
                g = (int)(24 - 12 * t);
                b = (int)(32 - 12 * t);
            }

            {
                int dx = x - surf->w * 72 / 100;
                int dy = y - surf->h * 42 / 100;
                int d2 = dx*dx + dy*dy;
                int rad = surf->h / 9;

                if (d2 < rad*rad) {
                    float k = 1.0f - (float)d2 / (float)(rad*rad);

                    r = (int)(r + 210 * k);
                    g = (int)(g + 150 * k);
                    b = (int)(b + 70 * k);
                }
            }

            {
                int h1 = surf->h * 62 / 100 + (int)(28 * sin(x * 0.004));
                int h2 = surf->h * 74 / 100 + (int)(18 * sin(x * 0.009 + 2.0));

                if (y > h1) { r = 22; g = 20; b = 30; }
                if (y > h2) { r = 12; g = 12; b = 20; }
            }

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            px[(size_t)y * stride + x] = SDL_MapRGBA(surf->format,
                                                     (Uint8)r, (Uint8)g,
                                                     (Uint8)b, 255);
        }
    }
}

/* The photo stand-in: bright sky, sea, a shoreline and a pier, with enough
 * contrast to show what sharpening and saturation do. */
static void paint_photo(SDL_Surface *surf)
{
    uint32_t *px = surf->pixels;
    int       stride = surf->pitch / 4;
    int x, y;

    for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
            int r, g, b;

            if (y < surf->h * 45 / 100) {
                float t = (float)y / (float)(surf->h * 45 / 100);

                r = (int)(92 + 140 * t);
                g = (int)(150 + 80 * t);
                b = (int)(214 + 30 * t);
            } else if (y < surf->h * 72 / 100) {
                float t = (float)(y - surf->h * 45 / 100) /
                          (float)(surf->h * 27 / 100);

                r = (int)(24 + 40 * t);
                g = (int)(96 + 50 * t);
                b = (int)(140 + 40 * t);
            } else {
                float t = (float)(y - surf->h * 72 / 100) /
                          (float)(surf->h * 28 / 100);

                r = (int)(214 - 40 * t);
                g = (int)(196 - 40 * t);
                b = (int)(150 - 30 * t);
            }

            /* a pier running out to the right */
            {
                int dy = y - surf->h * 70 / 100;

                if (x > surf->w * 40 / 100 && dy > 0 && dy < surf->h / 26 &&
                    (x / 12) % 3 != 0) {
                    r = 78; g = 60; b = 44;
                }
            }

            /* a couple of posts, for a sharp edge to grade */
            {
                int px1 = surf->w * 24 / 100, px2 = surf->w * 62 / 100;

                if ((abs(x - px1) < 9 || abs(x - px2) < 9) &&
                    y > surf->h * 55 / 100 && y < surf->h * 82 / 100) {
                    r = 50; g = 38; b = 30;
                }
            }

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            px[(size_t)y * stride + x] = SDL_MapRGBA(surf->format,
                                                     (Uint8)r, (Uint8)g,
                                                     (Uint8)b, 255);
        }
    }
}

static int shot(const char *path, int mode, int vw, int vh)
{
    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Surface  *surf = NULL;
    SDL_Surface  *img = NULL;
    SDL_Texture  *tex = NULL;
    UiModel       m;
    int rc = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    win  = SDL_CreateWindow("preview", 0, 0, vw, vh, SDL_WINDOW_HIDDEN);
    ren  = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    surf = ren ? SDL_CreateRGBSurfaceWithFormat(0, vw, vh, 32,
                                                SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (!win || !ren || !surf) {
        fprintf(stderr, "setup: %s\n", SDL_GetError());
        goto done;
    }

    ui_theme_set(0);

    switch (mode) {
    case SHOT_HOME:
        base(&m);
        m.screen     = UI_SCREEN_HOME;
        m.theme_name = "ORIGINAL";
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_THEME:
        base(&m);
        ui_theme_set(2);
        m.screen     = UI_SCREEN_HOME;
        m.theme_name = "EMERALD";
        m.home_selected = 1;
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_VIS:
        music_model(&m);
        m.vis_style = g_vis_style;
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_MUSIC:
        music_model(&m);
        ui_draw(ren, &m, vw, vh);
        break;

    case 20:                            /* the ARTISTS list */
        music_model(&m);
        m.tracks         = artists;
        m.track_count    = (int)(sizeof(artists)/sizeof(artists[0]));
        m.music_view     = 1;
        m.music_view_name = "ARTISTS";
        m.music_level    = 0;
        m.selected       = 2;
        ui_draw(ren, &m, vw, vh);
        break;

    case 21:                            /* one artist's albums */
        music_model(&m);
        m.tracks          = albums;
        m.track_count     = (int)(sizeof(albums)/sizeof(albums[0]));
        m.music_view      = 1;
        m.music_view_name = "ARTISTS";
        m.music_crumb     = "THE NEON DIVISION";
        m.music_level     = 1;
        m.selected        = 0;
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_EQ:
        music_model(&m);
        m.eq_open = 1;
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_EQ_PRESET:
        music_model(&m);
        m.eq_open        = 1;
        m.eq_preset      = 3;
        m.eq_preset_open = 1;
        m.eq_preset_sel  = 4;
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_VIS_FULL:
        music_model(&m);
        m.vis_fullscreen = 1;
        m.vis_style      = g_vis_style;
        ui_draw_visualizer_full(ren, &m, vw, vh);
        break;

    case SHOT_BROWSER:
        base(&m);
        m.screen         = UI_SCREEN_BROWSER;
        m.browser_title  = "DRIVES";
        m.browser_note   = "3 DRIVES";
        m.entries        = demo_entries;
        m.entry_count    = (int)(sizeof(demo_entries)/sizeof(demo_entries[0]));
        m.entry_selected = 1;
        m.usb_present    = 1;
        m.usb_count      = 1;
        m.clip_note      = "COPIED";
        m.clip_name      = "The Long Way Home (Director's Cut).mp4";
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_CONFIRM:
        base(&m);
        m.screen         = UI_SCREEN_BROWSER;
        m.browser_title  = "/data/Video";
        m.browser_note   = "24 ITEMS";
        m.entries        = demo_entries;
        m.entry_count    = (int)(sizeof(demo_entries)/sizeof(demo_entries[0]));
        m.entry_selected = 3;
        m.usb_present    = 0;
        m.confirm_delete = 1;
        m.confirm_name   = "Holiday 2024.mp4";
        ui_draw(ren, &m, vw, vh);
        break;

    case SHOT_VIDEOS:
    case SHOT_VIDEO:
    case SHOT_SETTINGS:
        video_model(&m);
        m.screen = UI_SCREEN_VIDEOS;
        img = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32,
                                             SDL_PIXELFORMAT_ABGR8888);
        if (!img) {
            fprintf(stderr, "image: %s\n", SDL_GetError());
            goto done;
        }
        paint_video(img);
        tex = SDL_CreateTextureFromSurface(ren, img);
        if (!tex) {
            fprintf(stderr, "texture: %s\n", SDL_GetError());
            goto done;
        }
        if (mode == SHOT_VIDEOS) {
            ui_draw_video_section(ren, tex, img->w, img->h, &m, vw, vh);
        } else if (mode == SHOT_SETTINGS) {
            m.video_settings_open = 1;
            m.video_settings_row  = 2;
            ui_draw_video_section(ren, tex, img->w, img->h, &m, vw, vh);
        } else {
            m.video_fullscreen = 1;
            ui_draw_video(ren, tex, img->w, img->h, &m, vw, vh);
        }
        break;

    case SHOT_PICTURES:
    case SHOT_PIC_SETTINGS:
    case SHOT_PIC_FULL:
    case SHOT_PIC_FULL_SETTINGS:
        picture_model(&m);
        img = SDL_CreateRGBSurfaceWithFormat(0, 1600, 1000, 32,
                                             SDL_PIXELFORMAT_ABGR8888);
        if (!img) {
            fprintf(stderr, "image: %s\n", SDL_GetError());
            goto done;
        }
        paint_photo(img);
        tex = SDL_CreateTextureFromSurface(ren, img);
        if (!tex) {
            fprintf(stderr, "texture: %s\n", SDL_GetError());
            goto done;
        }
        if (mode == SHOT_PIC_SETTINGS || mode == SHOT_PIC_FULL_SETTINGS) {
            m.pic_settings_open = 1;
            m.pic_settings_row  = 2;
            m.pic_rb_swap       = 1;   /* the shipped default */
        }
        if (mode == SHOT_PIC_FULL || mode == SHOT_PIC_FULL_SETTINGS)
            m.pic_fullscreen = 1;
        ui_draw_pictures(ren, tex, img->w, img->h, &m, vw, vh);
        break;
    }

    if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surf->pixels, surf->pitch) != 0) {
        fprintf(stderr, "read: %s\n", SDL_GetError());
        goto done;
    }
    if (SDL_SaveBMP(surf, path) != 0) {
        fprintf(stderr, "save: %s\n", SDL_GetError());
        goto done;
    }
    printf("wrote %s\n", path);
    rc = 0;

done:
    if (tex)   SDL_DestroyTexture(tex);
    if (img)   SDL_FreeSurface(img);
    if (surf)  SDL_FreeSurface(surf);
    if (ren)   SDL_DestroyRenderer(ren);
    if (win)   SDL_DestroyWindow(win);
    SDL_Quit();
    return rc;
}

/* bench <mode> <style> [frames]: how long a frame of that screen takes here,
 * so a performance change is measured instead of guessed. */
static int bench(int mode, int style, int n)
{
    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    UiModel       m;
    Uint64        freq, d_sum = 0, f_sum = 0;
    double        ms;
    int           i;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    win = SDL_CreateWindow("bench", 0, 0, W, H, SDL_WINDOW_HIDDEN);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!ren) {
        fprintf(stderr, "setup: %s\n", SDL_GetError());
        return 1;
    }
    ui_theme_set(0);
    g_vis_style = style;
    music_model(&m);
    if (mode == SHOT_VIS_FULL) {
        m.vis_fullscreen = 1;
        m.vis_style      = style;
    }

    /* one frame first: bakes the background and warms every cache */
    if (mode == SHOT_VIS_FULL)
        ui_draw_visualizer_full(ren, &m, W, H);
    else
        ui_draw(ren, &m, W, H);

    /* The draw calls only *queue* work in the software renderer; every blend
     * and copy actually runs inside SDL_RenderFlush. Timing the queue alone is
     * what made these numbers read far faster than the console, so the flush is
     * inside the timed loop and reported on its own line. */
    freq = SDL_GetPerformanceFrequency();
    for (i = 0; i < n; i++) {
        Uint64 a, b, c;

        m.elapsed_sec += 0.016;
        a = SDL_GetPerformanceCounter();
        if (mode == SHOT_VIS_FULL)
            ui_draw_visualizer_full(ren, &m, W, H);
        else
            ui_draw(ren, &m, W, H);
        b = SDL_GetPerformanceCounter();
        SDL_RenderFlush(ren);
        c = SDL_GetPerformanceCounter();
        d_sum += b - a;
        f_sum += c - b;
    }
    ms = (double)(d_sum + f_sum) * 1000.0 / ((double)freq * (double)n);
    printf("mode=%d style=%d %.2f ms/frame (%.1f fps)  draw=%.2f flush=%.2f\n",
           mode, style, ms, 1000.0 / ms,
           (double)d_sum * 1000.0 / ((double)freq * (double)n),
           (double)f_sum * 1000.0 / ((double)freq * (double)n));

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

/* seq <mode> <style> <frames> <path>: run the screen for that many frames and
 * save the last one. The liquid solver, the foam and the spray all need time to
 * happen, so one frame says nothing about them - this is how the water is
 * looked at between builds. */
static int seq(int mode, int style, int n, const char *path)
{
    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Surface  *surf = NULL;
    UiModel       m;
    int           i;
    int           rc = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    win  = SDL_CreateWindow("seq", 0, 0, W, H, SDL_WINDOW_HIDDEN);
    ren  = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    surf = ren ? SDL_CreateRGBSurfaceWithFormat(0, W, H, 32,
                                                SDL_PIXELFORMAT_ARGB8888) : NULL;
    if (!win || !ren || !surf) {
        fprintf(stderr, "setup: %s\n", SDL_GetError());
        goto done;
    }
    ui_theme_set(0);
    music_model(&m);
    m.vis_fullscreen = 1;
    m.vis_style      = style;
    if (mode != SHOT_VIS_FULL)
        m.vis_fullscreen = 0;

    for (i = 0; i < n; i++) {
        m.elapsed_sec += 1.0 / 60.0;
        if (m.vis_fullscreen)
            ui_draw_visualizer_full(ren, &m, W, H);
        else
            ui_draw(ren, &m, W, H);
        SDL_RenderFlush(ren);
    }

    if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888,
                             surf->pixels, surf->pitch) != 0) {
        fprintf(stderr, "read: %s\n", SDL_GetError());
        goto done;
    }
    if (SDL_SaveBMP(surf, path) != 0) {
        fprintf(stderr, "save: %s\n", SDL_GetError());
        goto done;
    }
    printf("wrote %s after %d frames\n", path, n);
    rc = 0;

done:
    if (surf) SDL_FreeSurface(surf);
    if (ren)  SDL_DestroyRenderer(ren);
    if (win)  SDL_DestroyWindow(win);
    SDL_Quit();
    return rc;
}

/* skin [frames] [switch_frame]: the home screen across a skin change, every
 * frame timed. A skin change used to rebuild the room in the frame that paid
 * for the key press (435 ms on the console, the 467 ms freeze in the logs); it
 * is now built a slice per frame. This prints the worst frame on each side and
 * how long the new room took to arrive. */
static int skin(int n, int at, const char *path)
{
    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    UiModel       m;
    Uint64        freq;
    double        worst_before = 0.0, worst_after = 0.0, total = 0.0;
    int           i, over = 0, arrived = -1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
    win = SDL_CreateWindow("skin", 0, 0, W, H, SDL_WINDOW_HIDDEN);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    if (!ren) {
        fprintf(stderr, "setup: %s\n", SDL_GetError());
        return 1;
    }
    ui_theme_set(0);
    base(&m);
    m.screen     = UI_SCREEN_HOME;
    m.theme_name = (char *)ui_theme_name(0);

    freq = SDL_GetPerformanceFrequency();
    /* the startup bake, exactly as the first frame pays for it */
    {
        Uint64 a = SDL_GetPerformanceCounter();

        ui_draw(ren, &m, W, H);
        SDL_RenderFlush(ren);
        printf("startup first frame: %.1f ms\n",
               (double)(SDL_GetPerformanceCounter() - a) * 1000.0 / (double)freq);
    }

    for (i = 0; i < n; i++) {
        Uint64 a, b;
        double ms;

        if (i == at) {
            ui_theme_set(1);
            m.theme_name = (char *)ui_theme_name(1);
        }
        a = SDL_GetPerformanceCounter();
        ui_draw(ren, &m, W, H);
        SDL_RenderFlush(ren);
        b = SDL_GetPerformanceCounter();
        ms = (double)(b - a) * 1000.0 / (double)freq;
        total += ms;
        if (i < at) {
            if (ms > worst_before) worst_before = ms;
        } else {
            if (ms > worst_after) worst_after = ms;
            if (ms > 20.0) over++;
            if (arrived < 0 && ms < 6.0 && i > at + 2)
                arrived = i - at;
        }
    }
    printf("skin: frames=%d switch_at=%d  worst before=%.1f  worst after=%.1f"
           " ms  frames>20ms after=%d  settled ~%d frames after the press\n",
           n, at, worst_before, worst_after, over, arrived);
    printf("skin: average %.1f ms over the run\n", total / (double)n);

    if (path && path[0]) {
        SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32,
                                                          SDL_PIXELFORMAT_ARGB8888);

        if (!out)
            printf("save: no surface: %s\n", SDL_GetError());
        else if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888,
                                      out->pixels, out->pitch) != 0)
            printf("save: read: %s\n", SDL_GetError());
        else if (SDL_SaveBMP(out, path) != 0)
            printf("save: write: %s\n", SDL_GetError());
        else
            printf("wrote %s\n", path);
        if (out)
            SDL_FreeSurface(out);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}

static int save(const char *prefix, const char *name, int mode, int w, int h)
{
    char path[256];

    snprintf(path, sizeof(path), "%s-%s.bmp", prefix, name);
    return shot(path, mode, w, h);
}

int main(int argc, char **argv)
{
    const char *p = argc > 1 ? argv[1] : "out";

    /* "one <mode>" renders a single shot, for checking a screen in a fresh
     * process instead of behind every earlier frame. */
    if (argc > 3 && strcmp(argv[1], "bench") == 0)
        return bench(atoi(argv[2]), atoi(argv[3]), argc > 4 ? atoi(argv[4]) : 20);

    if (argc > 5 && strcmp(argv[1], "seq") == 0)
        return seq(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), argv[5]);

    if (argc > 1 && strcmp(argv[1], "skin") == 0)
        return skin(argc > 2 ? atoi(argv[2]) : 60,
                    argc > 3 ? atoi(argv[3]) : 10,
                    argc > 4 ? argv[4] : NULL);

    if (argc > 2 && strcmp(argv[1], "one") == 0) {
        int mode = atoi(argv[2]);
        int w = W, h = H;

        if (argc > 3)
            g_vis_style = atoi(argv[3]);
        /* one <mode> [style [w h]]: one screen in a fresh process at any
         * size, without the state the earlier shots of a full run leave
         * behind - that is what makes a before/after comparison exact. */
        if (argc > 5) {
            w = atoi(argv[4]);
            h = atoi(argv[5]);
            if (w <= 0 || h <= 0) {
                w = W;
                h = H;
            }
        }
        return save("one", "shot", mode, w, h);
    }

    if (save(p, "home",          SHOT_HOME,         W, H) != 0 ||
        save(p, "music",         SHOT_MUSIC,        W, H) != 0 ||
        save(p, "videos",        SHOT_VIDEOS,       W, H) != 0 ||
        save(p, "video",         SHOT_VIDEO,        W, H) != 0 ||
        save(p, "settings",      SHOT_SETTINGS,     W, H) != 0 ||
        save(p, "pictures",      SHOT_PICTURES,     W, H) != 0 ||
        save(p, "picsettings",   SHOT_PIC_SETTINGS, W, H) != 0 ||
        save(p, "browser",       SHOT_BROWSER,      W, H) != 0 ||
        save(p, "eq",            SHOT_EQ,           W, H) != 0)
        return 1;

    if (save(p, "picsettings", SHOT_PIC_SETTINGS, W, H) != 0 ||
        save(p, "picfull",     SHOT_PIC_FULL,     W, H) != 0 ||
        save(p, "confirm",     SHOT_CONFIRM,      W, H) != 0 ||
        save(p, "theme",       SHOT_THEME,        W, H) != 0 ||
        save(p, "visfull",     SHOT_VIS_FULL,     W, H) != 0 ||
        save(p, "eqpresets",   SHOT_EQ_PRESET,    W, H) != 0)
        return 1;

    {
        static const char *const names[] = {
            "vis-spectrum", "vis-wave", "vis-radial", "vis-pulse",
            "vis-mirror", "vis-blocks", "vis-scope", "vis-orbit"
        };
        int i;

        for (i = 0; i < 8; i++) {
            g_vis_style = i;
            if (save(p, names[i], SHOT_VIS, W, H) != 0)
                return 1;
        }
    }

    if (save(p, "home-720",      SHOT_HOME,   1280, 720) != 0 ||
        save(p, "videos-720",    SHOT_VIDEOS, 1280, 720) != 0 ||
        save(p, "music-1440",    SHOT_MUSIC,  2560, 1440) != 0)
        return 1;
    return 0;
}
