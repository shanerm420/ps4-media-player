/* bench-blit.c - what a full-screen background blit costs each way.
 *
 * The perf log showed flush=45..55 ms on every frame, in every screen. That is
 * a full-screen texture copy, and the question is whether it is the scaled
 * path (960x540 stretched to 1920x1080) or the unscaled one. This measures
 * both on the same renderer.
 *
 * Build:  cc -O2 -o build/bench-blit tools/bench-blit.c $(sdl2-config --cflags --libs)
 */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 1920
#define H 1080

static double now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(void)
{
    SDL_Window   *win;
    SDL_Renderer *ren;
    SDL_Texture  *small, *native;
    unsigned char *pix;
    int i, n = 60;
    double t0, t1, scaled, plain;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "init: %s\n", SDL_GetError());
        return 1;
    }
    win = SDL_CreateWindow("bench", 0, 0, W, H, 0);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "renderer: %s\n", SDL_GetError());
        return 1;
    }

    pix = (unsigned char *)malloc((size_t)W * H * 4);
    memset(pix, 40, (size_t)W * H * 4);

    small  = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                               SDL_TEXTUREACCESS_STATIC, 960, 540);
    native = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888,
                               SDL_TEXTUREACCESS_STATIC, W, H);
    SDL_UpdateTexture(small, NULL, pix, 960 * 4);
    SDL_UpdateTexture(native, NULL, pix, W * 4);

    /* warm the caches and the present path */
    for (i = 0; i < 4; i++) {
        SDL_RenderCopy(ren, small, NULL, NULL);
        SDL_RenderPresent(ren);
    }

    t0 = now_ms();
    for (i = 0; i < n; i++) {
        SDL_RenderCopy(ren, small, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    t1 = now_ms();
    scaled = (t1 - t0) / (double)n;

    t0 = now_ms();
    for (i = 0; i < n; i++) {
        SDL_RenderCopy(ren, native, NULL, NULL);
        SDL_RenderPresent(ren);
    }
    t1 = now_ms();
    plain = (t1 - t0) / (double)n;

    printf("scaled 960x540 -> 1920x1080 : %.2f ms/frame\n", scaled);
    printf("1:1    1920x1080 -> same    : %.2f ms/frame\n", plain);
    printf("saved per frame             : %.2f ms\n", scaled - plain);

    SDL_DestroyTexture(small);
    SDL_DestroyTexture(native);
    free(pix);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
