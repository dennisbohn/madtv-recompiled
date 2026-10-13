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
static HWND g_hwnd;

/* PORT: Menüleiste "Spiel" (Beenden), "Einstellungen" (Sprache Deutsch/English, Vollbild) und
 * "Hilfe" (Über Mad TV Recompiled). Im Vollbild ist sie ausgeblendet; Alt blendet sie dort ein (wie
 * in Windows-Programmen üblich), eine Menüauswahl, erneutes Alt oder ein Klick ins Spielbild wieder
 * aus. Beim Ein- und Ausblenden im Fenster wird das Fenster um die Höhe der Leiste angepasst, damit
 * die Spielfläche gleich groß bleibt. */
#define MENU_LANG_DE 0x1100
#define MENU_LANG_EN 0x1110
#define MENU_FULLSCREEN 0x1120
#define MENU_QUIT 0x1130
#define MENU_ABOUT 0x1140
#ifndef MADTV_VERSION
#define MADTV_VERSION "?" /* setzt das Makefile aus res/version.rc */
#endif
static int g_lang_shown = -1, g_lang_request = -1, g_fullscreen_request, g_quit_request;
static HMENU g_menubar, g_menu_game, g_menu_settings, g_menu_lang, g_menu_help;
static int g_menu_overlay;      /* Leiste im Vollbild per Alt eingeblendet */
static int g_menu_mode_changed; /* Einblenden/Ausblenden seit der letzten Abfrage */
static int g_alt_alone;         /* Alt gedrückt, ohne weitere Taste */

/* Menütexte je Sprache */
static void menu_texts(int lang)
{
    int de = lang != 1;
    ModifyMenuW(g_menubar, 0, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)g_menu_game, de ? L"Spiel" : L"Game");
    ModifyMenuW(g_menu_game, MENU_QUIT, MF_BYCOMMAND | MF_STRING, MENU_QUIT,
                de ? L"Beenden\tAlt+F4" : L"Quit\tAlt+F4");
    ModifyMenuW(g_menubar, 1, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)g_menu_settings,
                de ? L"Einstellungen" : L"Settings");
    ModifyMenuW(g_menu_settings, 0, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)g_menu_lang,
                de ? L"Sprache" : L"Language");
    ModifyMenuW(g_menu_settings, MENU_FULLSCREEN, MF_BYCOMMAND | MF_STRING, MENU_FULLSCREEN,
                de ? L"Vollbild\tF11" : L"Fullscreen\tF11");
    ModifyMenuW(g_menubar, 2, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)g_menu_help, de ? L"Hilfe" : L"Help");
    ModifyMenuW(g_menu_help, MENU_ABOUT, MF_BYCOMMAND | MF_STRING, MENU_ABOUT,
                de ? L"Über Mad TV Recompiled" : L"About Mad TV Recompiled");
    if (GetMenuState(g_menu_settings, MENU_FULLSCREEN, MF_BYCOMMAND) != (UINT)-1)
        CheckMenuItem(g_menu_settings, MENU_FULLSCREEN, MF_BYCOMMAND | (video_is_fullscreen() ? MF_CHECKED : MF_UNCHECKED));
}

/* Fenster "Über Mad TV Recompiled" (modal; das Spiel steht so lange) */
static void about_show(void)
{
    int de = g_lang_shown != 1;
    wchar_t text[1024];
    swprintf(text, sizeof text / sizeof *text,
             de ? L"Mad TV Recompiled %hs\n\n"
                  L"Native Windows-Portierung von Mad TV (1991, Rainbow Arts). Der Code des "
                  L"Originalspiels wurde mit KI-Unterstützung (Claude von Anthropic) analysiert und "
                  L"nach C übersetzt.\n\n"
                  L"Entwicklung: Dennis Bohn\n"
                  L"https://github.com/dennisbohn/madtv-recompiled\n\n"
                  L"Tasten:\n"
                  L"F10\tSprache wechseln\n"
                  L"F11\tVollbild ein/aus (auch Alt+Enter)\n"
                  L"Alt\tMenüleiste im Vollbild\n\n"
                  L"Mad TV © 1991 Rainbow Arts."
                : L"Mad TV Recompiled %hs\n\n"
                  L"Native Windows port of Mad TV (1991, Rainbow Arts). The code of the original "
                  L"game was analysed and translated to C with AI assistance (Claude by "
                  L"Anthropic).\n\n"
                  L"Development: Dennis Bohn\n"
                  L"https://github.com/dennisbohn/madtv-recompiled\n\n"
                  L"Keys:\n"
                  L"F10\tSwitch language\n"
                  L"F11\tToggle fullscreen (also Alt+Enter)\n"
                  L"Alt\tMenu bar in fullscreen\n\n"
                  L"Mad TV © 1991 Rainbow Arts.",
             MADTV_VERSION);
    MessageBoxW(g_hwnd, text, de ? L"Über Mad TV Recompiled" : L"About Mad TV Recompiled",
                MB_OK | MB_ICONINFORMATION);
}

int video_fullscreen_request(void)
{
    int r = g_fullscreen_request;
    g_fullscreen_request = 0;
    return r;
}

int video_quit_request(void)
{
    return g_quit_request;
}

void video_language_show(int lang)
{
    g_lang_shown = lang;
    if (!g_menubar)
        return;
    menu_texts(lang);
    CheckMenuRadioItem(g_menu_lang, MENU_LANG_DE, MENU_LANG_EN, lang == 0 ? MENU_LANG_DE : MENU_LANG_EN,
                       MF_BYCOMMAND);
    if (GetMenu(g_hwnd))
        DrawMenuBar(g_hwnd);
}

static void menubar_show(int on)
{
    if (!g_menubar || on == (GetMenu(g_hwnd) != NULL))
        return;
    RECT win, cli;
    GetWindowRect(g_hwnd, &win);
    GetClientRect(g_hwnd, &cli);
    SetMenu(g_hwnd, on ? g_menubar : NULL);
    if (IsZoomed(g_hwnd) || (SDL_GetWindowFlags(g_win) & SDL_WINDOW_FULLSCREEN))
        return;
    RECT cli2; /* Spielfläche gleich groß halten: Fenster um die Höhe der Leiste ändern */
    GetClientRect(g_hwnd, &cli2);
    SetWindowPos(g_hwnd, NULL, 0, 0, win.right - win.left, (win.bottom - win.top) + (cli.bottom - cli2.bottom),
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* Leiste im Vollbild ein-/ausblenden (Spielfläche wird dabei kurz etwas kleiner) */
static void menu_overlay(int on)
{
    if (!g_menubar || on == g_menu_overlay)
        return;
    g_menu_overlay = on;
    g_menu_mode_changed = 1; /* Maus: solange die Leiste sichtbar ist, Windows-Zeiger statt relativ */
    SetMenu(g_hwnd, on ? g_menubar : NULL);
}

int video_menu_overlay(void)
{
    return g_menu_overlay;
}

int video_menu_overlay_changed(void)
{
    int r = g_menu_mode_changed;
    g_menu_mode_changed = 0;
    return r;
}

int video_language_request(void)
{
    int r = g_lang_request;
    g_lang_request = -1;
    return r;
}

static LRESULT CALLBACK aspect_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_COMMAND && HIWORD(wp) == 0 && (LOWORD(wp) == MENU_LANG_DE || LOWORD(wp) == MENU_LANG_EN)) {
        g_lang_request = LOWORD(wp) == MENU_LANG_DE ? 0 : 1;
        return 0;
    }
    if (msg == WM_COMMAND && HIWORD(wp) == 0 && LOWORD(wp) == MENU_ABOUT) {
        about_show(); /* direkt hier: die Maus ist nach der Menüauswahl noch frei */
        return 0;
    }
    if (msg == WM_COMMAND && HIWORD(wp) == 0 && LOWORD(wp) == MENU_QUIT) {
        g_quit_request = 1; /* beendet wird in platform_pump wie beim Schließen des Fensters */
        return 0;
    }
    if (msg == WM_COMMAND && HIWORD(wp) == 0 && LOWORD(wp) == MENU_FULLSCREEN) {
        g_fullscreen_request = 1; /* umgeschaltet wird in platform_pump, nicht hier im Fensterereignis */
        return 0;
    }
    /* F10 wechselt die Sprache; Windows würde damit sonst die Menüauswahl des Fensters starten */
    if ((msg == WM_SYSKEYDOWN || msg == WM_KEYDOWN || msg == WM_SYSKEYUP || msg == WM_KEYUP) && wp == VK_F10) {
        if ((msg == WM_SYSKEYDOWN || msg == WM_KEYDOWN) && !(lp & (1 << 30)))
            g_lang_request = g_lang_shown == 0 ? 1 : 0;
        return 0;
    }
    /* Alt allein (drücken und loslassen) blendet die Leiste im Vollbild ein bzw. aus */
    if (msg == WM_SYSKEYDOWN || msg == WM_KEYDOWN) {
        if (wp == VK_MENU) {
            if (!(lp & (1 << 30)))
                g_alt_alone = 1;
        } else {
            g_alt_alone = 0;
        }
    } else if (msg == WM_SYSKEYUP && wp == VK_MENU) {
        if (g_alt_alone && video_is_fullscreen())
            menu_overlay(!g_menu_overlay);
        g_alt_alone = 0;
    } else if (msg == WM_EXITMENULOOP && g_menu_overlay) {
        menu_overlay(0);
    } else if ((msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN) && g_menu_overlay) {
        menu_overlay(0); /* Klick ins Spielbild schließt nur die Leiste */
        return 0;
    }
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
        g_hwnd = hwnd;
        g_menu_lang = CreatePopupMenu();
        AppendMenuW(g_menu_lang, MF_STRING, MENU_LANG_DE, L"Deutsch\tF10");
        AppendMenuW(g_menu_lang, MF_STRING, MENU_LANG_EN, L"English\tF10");
        g_menu_settings = CreatePopupMenu();
        AppendMenuW(g_menu_settings, MF_POPUP | MF_STRING, (UINT_PTR)g_menu_lang, L"Sprache");
        AppendMenuW(g_menu_settings, MF_STRING, MENU_FULLSCREEN, L"Vollbild\tF11");
        g_menu_game = CreatePopupMenu();
        AppendMenuW(g_menu_game, MF_STRING, MENU_QUIT, L"Beenden\tAlt+F4");
        g_menubar = CreateMenu();
        AppendMenuW(g_menubar, MF_POPUP | MF_STRING, (UINT_PTR)g_menu_game, L"Spiel");
        AppendMenuW(g_menubar, MF_POPUP | MF_STRING, (UINT_PTR)g_menu_settings, L"Einstellungen");
        g_menu_help = CreatePopupMenu();
        AppendMenuW(g_menu_help, MF_STRING, MENU_ABOUT, L"Über Mad TV Recompiled");
        AppendMenuW(g_menubar, MF_POPUP | MF_STRING, (UINT_PTR)g_menu_help, L"Hilfe");
        menu_texts(g_lang_shown);
        menubar_show(1);
        if (g_lang_shown >= 0)
            video_language_show(g_lang_shown);
    }
}
#else
void video_language_show(int lang)
{
    (void)lang;
}

int video_language_request(void)
{
    return -1;
}

int video_fullscreen_request(void)
{
    return 0;
}

int video_quit_request(void)
{
    return 0;
}

int video_menu_overlay(void)
{
    return 0;
}

int video_menu_overlay_changed(void)
{
    return 0;
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
    /* Direct3D 11 bevorzugen (SDL nähme sonst Direct3D 9); fehlt es, wählt SDL selbst. Die
     * Umgebungsvariable SDL_RENDER_DRIVER hat Vorrang (Ausweichmöglichkeit bei Treiberproblemen). */
    const char *forced = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
    for (int i = 0; !g_ren && !(forced && *forced) && i < SDL_GetNumRenderDrivers(); i++) {
        SDL_RendererInfo ri;
        if (SDL_GetRenderDriverInfo(i, &ri) == 0 && strcmp(ri.name, "direct3d11") == 0)
            g_ren = SDL_CreateRenderer(g_win, i, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    }
    if (!g_ren)
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
#ifdef _WIN32
    if (!fs)
        menubar_show(0); /* Menüleiste im Vollbild nur per Alt */
    else
        menu_overlay(0);
#endif
    SDL_SetWindowFullscreen(g_win, fs ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
#ifdef _WIN32
    if (fs)
        menubar_show(1);
    if (g_menu_settings)
        CheckMenuItem(g_menu_settings, MENU_FULLSCREEN, MF_BYCOMMAND | (fs ? MF_UNCHECKED : MF_CHECKED));
#endif
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
