#include "video.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

uint8_t g_screen[SCREEN_W * SCREEN_H];

static SDL_Window *g_win;
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static SDL_Texture *g_big; /* ganzzahlig vergrößertes Bild für die "scharf bilineare" Ausgabe */
static int g_big_w, g_big_h;
/* Zeiger als eigene Ebene (wird nicht mit verschoben) */
static SDL_Texture *g_cur_tex;
static uint32_t g_cur_argb[SCREEN_W * SCREEN_H];
/* Feinverschiebung von Bildbereichen um Bruchteile eines Spielpixels nach unten (jeweils für
 * ein Bild): Bereich, Anteil in 1/256 Pixel und die Zeile, die oben nachrückt */
enum { SUB_MAX = 4 };
static struct {
    int x, y, w, h, frac;
    uint8_t row[SCREEN_W];
} g_sub[SUB_MAX];
static int g_sub_count;
static SDL_Texture *g_row_tex; /* eine Zeile je Bereich */
/* Eigene Ebenen (z. B. Parallax-Kulisse, Straßenobjekte, Hinweise getrennt): Pixel einschließlich
 * der Zeile über dem Bereich, Feinanteil und Deckung je Pixel */
enum { LAYER_MAX = 6, LAYER_W = SCREEN_W, LAYER_H = SCREEN_H + 1 };
static struct {
    int x, y, w, h, frac;
    uint8_t px[LAYER_W * LAYER_H];
    uint8_t opaque[LAYER_W * LAYER_H];
} g_layer[LAYER_MAX];
static int g_layer_count;
static SDL_Texture *g_layer_tex[LAYER_MAX];
/* Feinverschiebungen und Ebenen gelten, bis neue gesetzt werden – aber nur, solange der obere
 * Bildbereich noch das Grundbild ist, zu dem sie gehören (sonst Raumwechsel, Übergang, Dialog) */
enum { BASE_Y0 = 4, BASE_Y1 = 0x71 };
static uint8_t g_layer_base[SCREEN_W * (BASE_Y1 - BASE_Y0)];
static int g_layer_valid;

void video_layers_begin(void)
{
    g_sub_count = 0;
    g_layer_count = 0;
    g_layer_valid = 0;
}

void video_layers_commit(void)
{
    memcpy(g_layer_base, g_screen + BASE_Y0 * SCREEN_W, sizeof g_layer_base);
    g_layer_valid = 1;
}
static uint32_t g_layer_argb[LAYER_W * LAYER_H];
static char g_outshot[260]; /* Testhilfe: fertiges Ausgabebild beim nächsten Anzeigen speichern */

void video_set_subpixel_shift(int x, int y, int w, int h, int frac256, const uint8_t *row_above)
{
    if (frac256 <= 0 || !row_above || w <= 0 || x < 0 || x + w > SCREEN_W || h <= 0 || g_sub_count >= SUB_MAX)
        return;
    g_sub[g_sub_count].x = x;
    g_sub[g_sub_count].y = y;
    g_sub[g_sub_count].w = w;
    g_sub[g_sub_count].h = h;
    g_sub[g_sub_count].frac = frac256 > 255 ? 255 : frac256;
    memcpy(g_sub[g_sub_count].row, row_above, (size_t)w);
    g_sub_count++;
}

void video_add_layer(int x, int y, int w, int h, int frac256, const uint8_t *px, int stride, int transparent,
                     const uint8_t *mask)
{
    if (g_layer_count >= LAYER_MAX || w <= 0 || w > LAYER_W || h <= 0 || h + 1 > LAYER_H || x < 0 || x + w > SCREEN_W)
        return;
    g_layer[g_layer_count].x = x;
    g_layer[g_layer_count].y = y;
    g_layer[g_layer_count].w = w;
    g_layer[g_layer_count].h = h;
    g_layer[g_layer_count].frac = frac256 < 0 ? 0 : frac256 > 255 ? 255 : frac256;
    for (int r = 0; r < h + 1; r++)
        for (int c = 0; c < w; c++) {
            uint8_t v = px[r * stride + c];
            g_layer[g_layer_count].px[r * w + c] = v;
            g_layer[g_layer_count].opaque[r * w + c] = mask ? mask[r * stride + c] != 0 : (!transparent || v != 0);
        }
    g_layer_count++;
}

void video_request_output_shot(const char *path)
{
    snprintf(g_outshot, sizeof g_outshot, "%s", path);
}
static uint32_t g_pal[256];

static const uint8_t *g_cur_px;
static int g_cur_w, g_cur_h, g_cur_stride, g_cur_x, g_cur_y;

void video_set_cursor(const uint8_t *px, int w, int h, int stride, int x, int y)
{
    g_cur_px = px;
    g_cur_w = w;
    g_cur_h = h;
    g_cur_stride = stride;
    g_cur_x = x;
    g_cur_y = y;
}

int video_refresh_rate(void)
{
    SDL_DisplayMode m;
    if (g_win && SDL_GetWindowDisplayMode(g_win, &m) == 0 && m.refresh_rate > 0)
        return m.refresh_rate;
    return 60;
}

/* Zeiger in ein 8-Bit-Bild (320 Pixel breit) kopieren */
static void cursor_overlay(uint8_t *img)
{
    if (!g_cur_px)
        return;
    for (int r = 0; r < g_cur_h; r++) {
        int sy = (g_cur_y >> 8) + r;
        if (sy < 0 || sy >= SCREEN_H)
            continue;
        for (int c = 0; c < g_cur_w; c++) {
            int sx = (g_cur_x >> 8) + c;
            uint8_t v = g_cur_px[r * g_cur_stride + c];
            if (v && sx >= 0 && sx < SCREEN_W)
                img[sy * SCREEN_W + sx] = v;
        }
    }
}

#ifdef _WIN32
/* Fenstergröße beim Ziehen auf 4:3 (Innenfläche ohne Rahmen) halten: Windows meldet die neue
 * Größe vor dem Übernehmen (WM_SIZING); das Rechteck wird hier angepasst. SDL bietet dafür keine
 * Einstellung, daher wird die Fensterprozedur von SDL umschlossen. */
static WNDPROC g_sdl_wndproc;

static LRESULT CALLBACK aspect_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SIZING) {
        RECT *r = (RECT *)lp, win, cli;
        GetWindowRect(hwnd, &win);
        GetClientRect(hwnd, &cli);
        int bw = (win.right - win.left) - cli.right, bh = (win.bottom - win.top) - cli.bottom; /* Rahmen */
        int w = r->right - r->left - bw, h = r->bottom - r->top - bh;
        if (wp == WMSZ_TOP || wp == WMSZ_BOTTOM) { /* Höhe gezogen: Breite folgt */
            r->right = r->left + h * 4 / 3 + bw;
        } else { /* Seite oder Ecke: Höhe folgt der Breite */
            int nh = w * 3 / 4 + bh;
            if (wp == WMSZ_TOPLEFT || wp == WMSZ_TOPRIGHT)
                r->top = r->bottom - nh;
            else
                r->bottom = r->top + nh;
        }
        return TRUE;
    }
    return CallWindowProcW(g_sdl_wndproc, hwnd, msg, wp, lp);
}

static void aspect_lock_install(void)
{
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(g_win, &info) && info.subsystem == SDL_SYSWM_WINDOWS) {
        HWND hwnd = info.info.win.window;
        g_sdl_wndproc = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)aspect_wndproc);
    }
}
#endif

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
    SDL_SetTextureScaleMode(g_tex, SDL_ScaleModeNearest);
    memset(g_screen, 0, sizeof g_screen);
    for (int i = 0; i < 256; i++)
        g_pal[i] = 0xFF000000u;
#ifdef _WIN32
    aspect_lock_install();
#endif
    return 0;
}

void video_shutdown(void)
{
    if (g_big)
        SDL_DestroyTexture(g_big);
    g_big = NULL;
    if (g_cur_tex)
        SDL_DestroyTexture(g_cur_tex);
    g_cur_tex = NULL;
    if (g_row_tex)
        SDL_DestroyTexture(g_row_tex);
    g_row_tex = NULL;
    for (int k = 0; k < LAYER_MAX; k++) {
        if (g_layer_tex[k])
            SDL_DestroyTexture(g_layer_tex[k]);
        g_layer_tex[k] = NULL;
    }
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

uint32_t video_window_id(void)
{
    return SDL_GetWindowID(g_win);
}

int video_is_fullscreen(void)
{
    return (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
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
    static uint8_t frame[SCREEN_W * SCREEN_H];
    memcpy(frame, g_screen, sizeof frame);
    /* Zeiger normalerweise als eigene Ebene im vergrößerten Bild (unten); nur ohne Zwischenbild
     * direkt ins 320x200-Bild */
    int cursor_layer = g_big != NULL;
    if (!cursor_layer)
        cursor_overlay(frame);
    if (SDL_LockTexture(g_tex, NULL, &pixels, &pitch) == 0) {
        for (int y = 0; y < SCREEN_H; y++) {
            uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
            const uint8_t *src = frame + y * SCREEN_W;
            for (int x = 0; x < SCREEN_W; x++)
                row[x] = g_pal[src[x]];
        }
        SDL_UnlockTexture(g_tex);
    }
    /* "Scharf bilinear": das 320x200-Bild zuerst mit harten Pixelkanten auf das kleinste ganze
     * Vielfache vergrößern, das mindestens so groß ist wie die Ausgabefläche, und diese Bitmap
     * dann bilinear auf die Ausgabegröße (4:3) verkleinern. Pixel bleiben scharf; nur wo zwei
     * Spielpixel in einem Bildschirmpixel zusammentreffen, wird gemischt. Das gleicht auch die
     * ungleich hohen Zeilen der 200->240-Streckung aus. */
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    double s = (double)ow / (320 * 4) < (double)oh / (240 * 4) ? (double)ow / (320 * 4) : (double)oh / (240 * 4);
    int fx = (int)((320 * 4 * s + SCREEN_W - 1) / SCREEN_W), fy = (int)((240 * 4 * s + SCREEN_H - 1) / SCREEN_H);
    if (fx < 1)
        fx = 1;
    if (fy < 1)
        fy = 1;
    if (!g_big || g_big_w != fx * SCREEN_W || g_big_h != fy * SCREEN_H) {
        if (g_big)
            SDL_DestroyTexture(g_big);
        g_big_w = fx * SCREEN_W;
        g_big_h = fy * SCREEN_H;
        g_big = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, g_big_w, g_big_h);
        if (g_big)
            SDL_SetTextureScaleMode(g_big, SDL_ScaleModeLinear);
    }
    if (g_layer_valid && memcmp(g_layer_base, g_screen + BASE_Y0 * SCREEN_W, sizeof g_layer_base) != 0)
        video_layers_begin(); /* Grundbild gewechselt: Ebenen gehören nicht mehr dazu */
    if (!g_layer_valid) {
        g_sub_count = 0;
        g_layer_count = 0;
    }
    SDL_Texture *out = g_tex;
    if (g_big && SDL_SetRenderTarget(g_ren, g_big) == 0) {
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL); /* ganzzahlig, nächster Nachbar */
        /* Bereiche im vergrößerten Bild um Bruchteile eines Spielpixels nach unten versetzen
         * (Pixel bleiben ganze, scharfe 320x200-Pixel); oben rückt die Zeile darüber nach */
        if (g_sub_count > 0 && !g_row_tex) {
            g_row_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_W,
                                          SUB_MAX);
            if (g_row_tex)
                SDL_SetTextureScaleMode(g_row_tex, SDL_ScaleModeNearest);
        }
        for (int k = 0; k < g_sub_count && g_row_tex; k++) {
            int shift = (g_sub[k].frac * fy + 128) >> 8;
            if (shift <= 0)
                continue;
            SDL_Rect clip = {g_sub[k].x * fx, g_sub[k].y * fy, g_sub[k].w * fx, g_sub[k].h * fy};
            SDL_Rect src = {g_sub[k].x, g_sub[k].y, g_sub[k].w, g_sub[k].h};
            SDL_Rect dst = {g_sub[k].x * fx, g_sub[k].y * fy + shift, g_sub[k].w * fx, g_sub[k].h * fy};
            uint32_t row[SCREEN_W];
            for (int i = 0; i < g_sub[k].w; i++)
                row[i] = g_pal[g_sub[k].row[i]];
            SDL_Rect rsrc = {0, k, g_sub[k].w, 1};
            SDL_UpdateTexture(g_row_tex, &rsrc, row, (int)sizeof row);
            SDL_Rect rdst = {g_sub[k].x * fx, g_sub[k].y * fy + shift - fy, g_sub[k].w * fx, fy};
            SDL_RenderSetClipRect(g_ren, &clip);
            SDL_RenderCopy(g_ren, g_tex, &src, &dst);
            SDL_RenderCopy(g_ren, g_row_tex, &rsrc, &rdst);
            SDL_RenderSetClipRect(g_ren, NULL);
        }
        /* eigene Ebenen in Reihenfolge: um den Feinanteil nach unten versetzt, auf den Bereich begrenzt */
        for (int k = 0; k < g_layer_count; k++) {
            if (!g_layer_tex[k]) {
                g_layer_tex[k] = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                                   LAYER_W, LAYER_H);
                if (!g_layer_tex[k])
                    continue;
                SDL_SetTextureScaleMode(g_layer_tex[k], SDL_ScaleModeNearest);
                SDL_SetTextureBlendMode(g_layer_tex[k], SDL_BLENDMODE_BLEND);
            }
            int w = g_layer[k].w, h = g_layer[k].h;
            for (int i = 0; i < w * (h + 1); i++)
                g_layer_argb[i] = g_layer[k].opaque[i] ? g_pal[g_layer[k].px[i]] : 0;
            SDL_Rect lsrc = {0, 0, w, h + 1};
            SDL_UpdateTexture(g_layer_tex[k], &lsrc, g_layer_argb, w * 4);
            int shift = (g_layer[k].frac * fy + 128) >> 8;
            SDL_Rect clip = {g_layer[k].x * fx, g_layer[k].y * fy, w * fx, h * fy};
            SDL_Rect ldst = {g_layer[k].x * fx, (g_layer[k].y - 1) * fy + shift, w * fx, (h + 1) * fy};
            SDL_RenderSetClipRect(g_ren, &clip);
            SDL_RenderCopy(g_ren, g_layer_tex[k], &lsrc, &ldst);
            SDL_RenderSetClipRect(g_ren, NULL);
        }
        if (g_cur_px) { /* Zeiger als eigene Ebene, auf ganzen Spielpixeln */
            if (!g_cur_tex) {
                g_cur_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                              SCREEN_W, SCREEN_H);
                if (g_cur_tex) {
                    SDL_SetTextureScaleMode(g_cur_tex, SDL_ScaleModeNearest);
                    SDL_SetTextureBlendMode(g_cur_tex, SDL_BLENDMODE_BLEND);
                }
            }
            int w = g_cur_w < SCREEN_W ? g_cur_w : SCREEN_W, h = g_cur_h < SCREEN_H ? g_cur_h : SCREEN_H;
            if (g_cur_tex && w > 0 && h > 0) {
                for (int r = 0; r < h; r++)
                    for (int c = 0; c < w; c++) {
                        uint8_t v = g_cur_px[r * g_cur_stride + c];
                        g_cur_argb[r * w + c] = v ? g_pal[v] : 0;
                    }
                SDL_Rect csrc = {0, 0, w, h};
                SDL_UpdateTexture(g_cur_tex, &csrc, g_cur_argb, w * 4);
                /* zwischen den Spielpixeln (1/256), die Zeigerpixel selbst bleiben ganze Pixel */
                SDL_Rect cdst = {g_cur_x * fx / 256, g_cur_y * fy / 256, w * fx, h * fy};
                SDL_RenderCopy(g_ren, g_cur_tex, &csrc, &cdst);
            }
        }
        SDL_SetRenderTarget(g_ren, NULL);
        out = g_big;
    }
    (void)0; /* Ebenen bleiben bis zum nächsten video_layers_begin bzw. bis das Grundbild wechselt */
    SDL_SetRenderDrawColor(g_ren, 0, 0, 0, 255);
    SDL_RenderClear(g_ren);
    SDL_RenderCopy(g_ren, out, NULL, NULL); /* bilinear auf die 4:3-Fläche */
    if (g_outshot[0]) {
        SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, ow, oh, 32, SDL_PIXELFORMAT_ARGB8888);
        if (shot && SDL_RenderReadPixels(g_ren, NULL, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch) == 0)
            SDL_SaveBMP(shot, g_outshot);
        SDL_FreeSurface(shot);
        g_outshot[0] = 0;
    }
    SDL_RenderPresent(g_ren);
}

int video_save_bmp(const char *path)
{
    static uint8_t frame[SCREEN_W * SCREEN_H];
    memcpy(frame, g_screen, sizeof frame);
    cursor_overlay(frame);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(frame, SCREEN_W, SCREEN_H, 8, SCREEN_W,
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
