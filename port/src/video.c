#include "video.h"

#include <SDL.h>
#include <string.h>

uint8_t g_screen[SCREEN_W * SCREEN_H];

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static uint32_t g_pal[256];

int video_init(const char *title, int scale)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        return -1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    /* 320x200 mit nicht-quadratischen Pixeln -> 4:3-Ausgabe (320x240 pro Faktor). */
    g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             320 * scale, 240 * scale, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_win)
        return -1;
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren)
        return -1;
    SDL_RenderSetLogicalSize(g_ren, 320 * 4, 240 * 4);
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                              SCREEN_W, SCREEN_H);
    if (!g_tex)
        return -1;
    memset(g_screen, 0, sizeof g_screen);
    for (int i = 0; i < 256; i++)
        g_pal[i] = 0xFF000000u;
    return 0;
}

void video_shutdown(void)
{
    if (g_tex)
        SDL_DestroyTexture(g_tex);
    if (g_ren)
        SDL_DestroyRenderer(g_ren);
    if (g_win)
        SDL_DestroyWindow(g_win);
    g_tex = NULL;
    g_ren = NULL;
    g_win = NULL;
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void video_set_palette(const uint8_t rgb[768])
{
    for (int i = 0; i < 256; i++)
        g_pal[i] = 0xFF000000u | ((uint32_t)rgb[i * 3] << 16) | ((uint32_t)rgb[i * 3 + 1] << 8) | rgb[i * 3 + 2];
}

void video_set_title(const char *title)
{
    SDL_SetWindowTitle(g_win, title);
}

void video_toggle_fullscreen(void)
{
    Uint32 fs = SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP;
    SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void video_logical_size(int *w, int *h)
{
    *w = 320 * 4;
    *h = 240 * 4;
}

void video_present(void)
{
    void *pixels;
    int pitch;
    if (SDL_LockTexture(g_tex, NULL, &pixels, &pitch) == 0) {
        for (int y = 0; y < SCREEN_H; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
            const uint8_t *src = g_screen + y * SCREEN_W;
            for (int x = 0; x < SCREEN_W; x++)
                row[x] = g_pal[src[x]];
        }
        SDL_UnlockTexture(g_tex);
    }
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
    SDL_RenderPresent(g_ren);
}

int video_save_bmp(const char *path)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(g_screen, SCREEN_W, SCREEN_H, 8, SCREEN_W,
                                                        SDL_PIXELFORMAT_INDEX8);
    if (!s)
        return -1;
    SDL_Color colors[256];
    for (int i = 0; i < 256; i++)
        colors[i] = (SDL_Color){(uint8_t)(g_pal[i] >> 16), (uint8_t)(g_pal[i] >> 8), (uint8_t)g_pal[i], 255};
    SDL_SetPaletteColors(s->format->palette, colors, 0, 256);
    int rc = SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
    return rc;
}

void video_clear(uint8_t color)
{
    memset(g_screen, color, sizeof g_screen);
}

void video_blit_trans(const uint8_t *px, int w, int h, int x, int y)
{
    for (int row = 0; row < h; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= SCREEN_H)
            continue;
        for (int col = 0; col < w; col++) {
            int sx = x + col;
            uint8_t c = px[row * w + col];
            if (c && sx >= 0 && sx < SCREEN_W)
                g_screen[sy * SCREEN_W + sx] = c;
        }
    }
}
