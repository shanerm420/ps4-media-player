/* preview.c - renders src/ui.c to a BMP on a PC, with no PS4 involved.
 *
 * The point is to look at the real drawing code instead of guessing what it
 * does on a television. It builds fixed demo models (long file names, a video
 * entry, a playing track) and writes one frame per screen with SDL's dummy
 * video driver.
 *
 *   make -C preview run      -> out-browser.bmp, out-main.bmp, out-eq.bmp,
 *                               out-video.bmp, out-settings.bmp,
 *                               out-fullscreen.bmp, plus out-main-720,
 *                               out-video-720 and out-main-1440 for the
 *                               scaling check
 *
 * out-video.bmp uses a synthetic frame (a painted dusk scene) as the picture
 * under the transport bar, since there is no decoder on the host.
 * out-settings.bmp opens the video settings card over a subtitle, so the fit /
 * brightness / subtitle rows can be judged in place.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>

#include "../src/ui.h"

#define W 1920
#define H 1080

static const UiTrack demo_tracks[] = {
    { "Midnight Drive (Extended Mix).mp3",           0 },
    { "Neon Skyline.mp3",                            0 },
    { "Basilica - Organ Suite in D minor.mp3",       0 },
    { "Short",                                       0 },
    { "A Really Long Track Name That Should Be Cut With An Ellipsis.mp3", 0 },
    { "Harbour Lights.mp3",                          0 },
    { "Concert Footage 2019.mp4",                    1 },
    { "Slow Motion.mp3",                             0 },
    { "Encore.mp3",                                  0 },
    { "Hidden Track.mp3",                            0 },
    { "Bonus.mp3",                                   0 },
};

/* Drive list, as it looks with the internal drive and two sticks plugged in. */
static const UiEntry demo_entries[] = {
    { "ALL MEDIA (LIBRARY)", 0, 0, 0, 1 },
    { "INTERNAL DRIVE",      1, 0, 1, 0 },
    { "USB 1",               1, 0, 1, 0 },
    { "USB 2",               1, 0, 1, 0 },
    { "REFRESH DRIVE SCAN",  0, 0, 0, 1 },
};

static void fill(UiModel *m, int eq_open, int eq_band, int current)
{
    int i;

    memset(m, 0, sizeof(*m));
    m->app_title   = "ULTIMATE PS4 MEDIA PLAYER";
    m->tracks      = demo_tracks;
    m->track_count = (int)(sizeof(demo_tracks)/sizeof(demo_tracks[0]));
    m->selected    = 4;
    m->current     = current;
    m->playing     = 1;
    m->have_track  = 1;
    m->elapsed_sec  = 97.0;
    m->duration_sec = 311.0;
    m->format_note  = "MP3   44100 HZ   STEREO";
    m->status       = "Basilica - Organ Suite in D minor.mp3   44100 Hz stereo";
    m->volume_db    = -9;
    m->volume_frac  = (-9.0f + 60.0f) / 72.0f;
    m->eq_open      = eq_open;
    m->eq_band      = eq_band;

    for (i = 0; i < EQ_BANDS; i++)
        m->eq_gain_db[i] = 0.0f;
    m->eq_gain_db[0] = 6.0f;
    m->eq_gain_db[1] = 4.0f;
    m->eq_gain_db[2] = 1.0f;
    m->eq_gain_db[3] = -2.0f;
    m->eq_gain_db[4] = -3.0f;
    m->eq_gain_db[5] = 0.0f;
    m->eq_gain_db[6] = 2.0f;
    m->eq_gain_db[7] = 3.0f;
    m->eq_gain_db[8] = 1.0f;
    m->eq_gain_db[9] = -1.0f;
    m->eq_preamp_db  = -4.0f;
}

/* A 1280x720 stand-in for a decoded frame: dusk sky, sun, hills, so the
 * transport bar can be judged against something with dark and bright areas. */
static void paint_frame(SDL_Surface *surf)
{
    uint32_t *px = surf->pixels;
    int       stride = surf->pitch / 4;
    int x, y;

    for (y = 0; y < surf->h; y++) {
        for (x = 0; x < surf->w; x++) {
            int r, g, b;

            if (y < surf->h * 3 / 5) {
                /* sky: deep blue at the top, warm near the horizon */
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

            /* sun disc */
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

            /* two hill ridges */
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

static int shot(const char *path, int eq_open, int eq_band, int video,
                int settings, int browser, int vw, int vh)
{
    SDL_Window   *win = NULL;
    SDL_Renderer *ren = NULL;
    SDL_Surface  *surf = NULL;
    SDL_Surface  *frame = NULL;
    SDL_Texture  *tex = NULL;
    UiModel       m;
    int rc = 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    win = SDL_CreateWindow("preview", 0, 0, vw, vh, SDL_WINDOW_HIDDEN);
    ren = win ? SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE) : NULL;
    surf = ren ? SDL_CreateRGBSurfaceWithFormat(0, vw, vh, 32,
                                                SDL_PIXELFORMAT_ARGB8888) : NULL;

    if (!win || !ren || !surf) {
        fprintf(stderr, "setup: %s\n", SDL_GetError());
        goto done;
    }

    if (browser) {
        fill(&m, 0, 0, 4);
        m.screen         = UI_SCREEN_BROWSER;
        m.browser_title  = "DRIVES";
        m.browser_note   = "3 DRIVES";
        m.entries        = demo_entries;
        m.entry_count    = (int)(sizeof(demo_entries)/sizeof(demo_entries[0]));
        m.entry_selected = 1;
        ui_draw(ren, &m, vw, vh);
    } else if (video) {
        fill(&m, 0, 0, 6);
        m.format_note = "MP4   1280 x 720";
        m.status      = "Concert Footage 2019.mp4   MP4   1280 x 720";
        m.sub_text    = "The hills were already dark when the\nlast train came through.";
        m.sub_size    = 1;
        m.sub_label   = "CONCERT.EN.SRT (1/2)";
        m.video_aspect     = 0;
        m.video_brightness = 12;
        m.video_hud        = 1;
        if (settings == 1) {
            m.video_settings_open = 1;
            m.video_settings_row  = 2;
        } else if (settings == 2) {
            m.video_hud = 0;            /* OSD timed out: picture only */
            m.video_toast       = "SUBTITLES ON";
            m.video_toast_alpha = 215;
        }

        frame = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32,
                                               SDL_PIXELFORMAT_ABGR8888);
        if (!frame) {
            fprintf(stderr, "frame: %s\n", SDL_GetError());
            goto done;
        }
        paint_frame(frame);
        tex = SDL_CreateTextureFromSurface(ren, frame);
        if (!tex) {
            fprintf(stderr, "texture: %s\n", SDL_GetError());
            goto done;
        }
        ui_draw_video(ren, tex, frame->w, frame->h, &m, vw, vh);
    } else {
        fill(&m, eq_open, eq_band, 4);
        ui_draw(ren, &m, vw, vh);
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
    if (frame) SDL_FreeSurface(frame);
    if (surf)  SDL_FreeSurface(surf);
    if (ren)   SDL_DestroyRenderer(ren);
    if (win)   SDL_DestroyWindow(win);
    SDL_Quit();
    return rc;
}

int main(int argc, char **argv)
{
    const char *prefix = argc > 1 ? argv[1] : "out";
    char path[256];

    snprintf(path, sizeof(path), "%s-browser.bmp", prefix);
    if (shot(path, 0, 0, 0, 0, 1, W, H) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-main.bmp", prefix);
    if (shot(path, 0, 0, 0, 0, 0, W, H) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-eq.bmp", prefix);
    if (shot(path, 1, 3, 0, 0, 0, W, H) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-video.bmp", prefix);
    if (shot(path, 0, 0, 1, 0, 0, W, H) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-settings.bmp", prefix);
    if (shot(path, 0, 0, 1, 1, 0, W, H) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-fullscreen.bmp", prefix);
    if (shot(path, 0, 0, 1, 2, 0, W, H) != 0)
        return 1;

    /* the same interface at other output sizes: text must keep its place
     * relative to the panels, not just the rectangles */
    snprintf(path, sizeof(path), "%s-main-720.bmp", prefix);
    if (shot(path, 0, 0, 0, 0, 0, 1280, 720) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-video-720.bmp", prefix);
    if (shot(path, 0, 0, 1, 0, 0, 1280, 720) != 0)
        return 1;
    snprintf(path, sizeof(path), "%s-main-1440.bmp", prefix);
    if (shot(path, 0, 0, 0, 0, 0, 2560, 1440) != 0)
        return 1;
    return 0;
}
