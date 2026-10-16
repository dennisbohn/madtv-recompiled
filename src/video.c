#include "video.h"

#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <string.h>
#include "menu.h"
#include "platform.h"
#ifdef _WIN32
#include <windows.h>
#else
#include "menubar.h"
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
    /* Desktopmodus des Monitors, auf dem das Fenster liegt (SDL_GetWindowDisplayMode liefert den
     * Modus für exklusives Vollbild, meist 60 Hz, auch wenn der Monitor schneller läuft) */
    SDL_DisplayMode m;
    int display = g_win ? SDL_GetWindowDisplayIndex(g_win) : -1;
    if (display >= 0 && SDL_GetDesktopDisplayMode(display, &m) == 0 && m.refresh_rate > 0)
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

/* PORT: Menüleiste "Spiel" (Neues Spiel, Pause, Geschwindigkeit, Fernsehfamilie, Fernsehbild, Musik,
 * Beenden), "Einstellungen" (Sprache, Vollbild) und "Hilfe" (Über Mad TV Recompiled), beschrieben in menu.c. Windows zeigt sie als
 * Win32-Menü, Linux zeichnet sie selbst (menubar.c). Gewählte Befehle werden als Wünsche abgelegt
 * und von platform_pump bzw. der Hauptschleife des Spiels abgeholt. */
static int g_lang_shown = -1, g_lang_request = -1, g_fullscreen_request, g_quit_request;
static int g_game_shown = MENU_GAME_NONE, g_speed_shown, g_switches_shown, g_speed_request, g_new_game_request;
static int g_switch_request = -1, g_paused_shown, g_pause_request;
static void about_show(void);
static int confirm_new_game(void);
static void menu_changed(void);

static MenuState menu_state(int fullscreen)
{
    MenuState s = {g_lang_shown, fullscreen, g_game_shown, g_speed_shown, g_switches_shown, g_paused_shown};
    return s;
}

static void menu_command(int cmd)
{
    switch (cmd) {
    case MENU_LANG_DE: g_lang_request = 0; break;
    case MENU_LANG_EN: g_lang_request = 1; break;
    case MENU_FULLSCREEN: g_fullscreen_request = 1; break; /* umgeschaltet wird in platform_pump */
    case MENU_QUIT: g_quit_request = 1; break; /* beendet wird in platform_pump wie beim Schließen */
    case MENU_ABOUT: about_show(); break;
    case MENU_PAUSE:
        if (g_game_shown == MENU_GAME_RUNNING)
            g_pause_request = 1;
        break;
    case MENU_NEW_GAME:
        if (g_game_shown == MENU_GAME_RUNNING && confirm_new_game())
            g_new_game_request = 1;
        break;
    default: {
        MenuState s = menu_state(0);
        if (!menu_enabled(cmd, &s))
            break;
        if (menu_speed(cmd))
            g_speed_request = menu_speed(cmd);
        else if (menu_switch(cmd) >= 0) /* umlegen */
            g_switch_request = menu_switch(cmd) * 2 + !menu_checked(cmd, &s);
        break;
    }
    }
}

void video_game_show(int game, int speed, int switches, int paused)
{
    if (game == g_game_shown && speed == g_speed_shown && switches == g_switches_shown && paused == g_paused_shown)
        return;
    g_paused_shown = paused;
    g_game_shown = game;
    g_speed_shown = speed;
    g_switches_shown = switches;
    menu_changed();
}

int video_new_game_request(void)
{
    int r = g_new_game_request;
    g_new_game_request = 0;
    return r;
}

int video_speed_request(void)
{
    int r = g_speed_request;
    g_speed_request = 0;
    return r;
}

int video_pause_request(void)
{
    int r = g_pause_request;
    g_pause_request = 0;
    return r;
}

int video_switch_request(void)
{
    int r = g_switch_request;
    g_switch_request = -1;
    return r;
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

int video_language_request(void)
{
    int r = g_lang_request;
    g_lang_request = -1;
    return r;
}

#ifdef _WIN32
/* Fenstergröße beim Ziehen auf 4:3 (Innenfläche ohne Rahmen) halten: Windows meldet die neue
 * Größe vor dem Übernehmen (WM_SIZING); das Rechteck wird hier angepasst. SDL bietet dafür keine
 * Einstellung, daher wird die Fensterprozedur von SDL umschlossen. */
static WNDPROC g_sdl_wndproc;
static HWND g_hwnd;

/* Win32-Menü aus menu.c. Im Vollbild ist es ausgeblendet; Alt blendet es dort ein (wie in
 * Windows-Programmen üblich), eine Menüauswahl, erneutes Alt oder ein Klick ins Spielbild wieder
 * aus. Beim Ein- und Ausblenden im Fenster wird das Fenster um die Höhe der Leiste angepasst, damit
 * die Spielfläche gleich groß bleibt. Befehls-IDs: MENU_ID_BASE + enum MenuCmd. */
#define MENU_ID_BASE 0x1100
static HMENU g_menubar;
static int g_menu_overlay;      /* Leiste im Vollbild per Alt eingeblendet */
static int g_menu_mode_changed; /* Einblenden/Ausblenden seit der letzten Abfrage */
static int g_alt_alone;         /* Alt gedrückt, ohne weitere Taste */

/* UTF-8 (menu.c) nach UTF-16 für die W-Funktionen */
static void widen(const char *s, wchar_t *out, int n)
{
    if (MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) <= 0)
        out[0] = 0;
}

/* Menü mit Untermenüs anlegen; die Texte setzt menu_update */
static HMENU menu_build(const MenuItem *items, int popup)
{
    HMENU m = popup ? CreatePopupMenu() : CreateMenu();
    for (const MenuItem *it = items; it->text[0]; it++) {
        if (it->cmd == MENU_SEPARATOR)
            AppendMenuW(m, MF_SEPARATOR, 0, NULL);
        else if (it->sub)
            AppendMenuW(m, MF_POPUP | MF_STRING, (UINT_PTR)menu_build(it->sub, 1), L"");
        else
            AppendMenuW(m, MF_STRING, MENU_ID_BASE + it->cmd, L"");
    }
    return m;
}

/* Texte in der angezeigten Sprache (Tastenkürzel nach "\t"), Häkchen, Auswahlpunkte und
 * gesperrte Einträge */
static void menu_update_items(HMENU m, const MenuItem *items, const MenuState *s)
{
    UINT pos = 0;
    for (const MenuItem *it = items; it->text[0]; it++, pos++) {
        if (it->cmd == MENU_SEPARATOR)
            continue;
        char utf8[160];
        wchar_t text[160];
        if (it->key)
            snprintf(utf8, sizeof utf8, "%s\t%s", menu_text(it, g_lang_shown), it->key);
        else
            snprintf(utf8, sizeof utf8, "%s", menu_text(it, g_lang_shown));
        widen(utf8, text, (int)(sizeof text / sizeof *text));
        if (it->sub) {
            HMENU sub = GetSubMenu(m, (int)pos);
            ModifyMenuW(m, pos, MF_BYPOSITION | MF_POPUP | MF_STRING, (UINT_PTR)sub, text);
            menu_update_items(sub, it->sub, s);
            continue;
        }
        ModifyMenuW(m, pos, MF_BYPOSITION | MF_STRING, MENU_ID_BASE + it->cmd, text);
        EnableMenuItem(m, pos, MF_BYPOSITION | (menu_enabled(it->cmd, s) ? MF_ENABLED : MF_GRAYED));
        int on = menu_checked(it->cmd, s);
        if (it->mark == MENU_MARK_RADIO && on)
            CheckMenuRadioItem(m, pos, pos, pos, MF_BYPOSITION);
        else if (it->mark != MENU_MARK_NONE)
            CheckMenuItem(m, pos, MF_BYPOSITION | (on ? MF_CHECKED : MF_UNCHECKED));
    }
}

static void menu_update(int fullscreen)
{
    if (!g_menubar)
        return;
    MenuState s = menu_state(fullscreen);
    menu_update_items(g_menubar, menu_bar, &s);
    if (GetMenu(g_hwnd))
        DrawMenuBar(g_hwnd);
}

/* Fenster "Über Mad TV Recompiled" (modal; das Spiel steht so lange) */
static void about_show(void)
{
    static wchar_t text[1200], title[128];
    widen(menu_about_text(g_lang_shown, "Windows"), text, (int)(sizeof text / sizeof *text));
    widen(menu_about_title(g_lang_shown), title, (int)(sizeof title / sizeof *title));
    MessageBoxW(g_hwnd, text, title, MB_OK | MB_ICONINFORMATION);
}

/* Rückfrage vor "Neues Spiel" (modal) */
static int confirm_new_game(void)
{
    static wchar_t text[400], title[128];
    widen(menu_new_game_text(g_lang_shown), text, (int)(sizeof text / sizeof *text));
    widen(menu_text(&menu_bar[0].sub[0], g_lang_shown), title, (int)(sizeof title / sizeof *title));
    return MessageBoxW(g_hwnd, text, title, MB_YESNO | MB_ICONQUESTION) == IDYES;
}

static void menu_changed(void)
{
    menu_update(video_is_fullscreen());
}

void video_language_show(int lang)
{
    g_lang_shown = lang;
    menu_update(video_is_fullscreen());
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

static LRESULT CALLBACK aspect_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_COMMAND && HIWORD(wp) == 0 && LOWORD(wp) > MENU_ID_BASE &&
        LOWORD(wp) < MENU_ID_BASE + MENU_CMD_COUNT) {
        menu_command(LOWORD(wp) - MENU_ID_BASE); /* "Über" direkt hier: die Maus ist noch frei */
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
        g_menubar = menu_build(menu_bar, 0);
        menu_update(video_is_fullscreen());
        menubar_show(1);
    }
}

int video_menu_event(const SDL_Event *e)
{
    (void)e; /* das Win32-Menü läuft über die Fensterprozedur */
    return 0;
}
#else
/* PORT: F10 wechselt die Sprache (platform.c), außerdem die selbst gezeichnete Menüleiste */

void video_language_show(int lang)
{
    g_lang_shown = lang;
}

void video_language_toggle(void)
{
    if (g_lang_shown >= 0)
        g_lang_request = g_lang_shown == 0 ? 1 : 0;
}

void video_set_icon(const uint8_t *rgba, int w, int h)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom((void *)rgba, w, h, 32, w * 4, SDL_PIXELFORMAT_RGBA32);
    if (s) {
        SDL_SetWindowIcon(g_win, s);
        SDL_FreeSurface(s);
    }
    menubar_set_icon(rgba, w, h);
}

static int g_menu_was_active;

/* Dialog im Fenster gezeichnet (menubar.c). Modal wie unter Windows: das Spiel steht, bis er
 * geschlossen ist; nur Bild und Ereignisse laufen weiter. Schließen des Fensters beendet wie sonst
 * (Ereignis zurück in die Warteschlange für platform.c). Rückgabe: gewählter Knopf oder -1. */
static int dialog_run(const char *title, const char *text, const char *const *buttons, int n)
{
    menubar_dialog_open(title, text, buttons, n);
    SDL_ShowCursor(SDL_ENABLE);
    while (menubar_dialog_active()) {
        platform_modal_poll();
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                SDL_PushEvent(&e);
                menubar_dialog_close();
                break;
            }
            menubar_event(&e);
        }
        if (!menubar_dialog_active())
            break;
        video_present();
    }
    return menubar_dialog_result();
}

/* Dialog "Über Mad TV Recompiled" */
static void about_show(void)
{
    const char *ok[] = {"OK"};
    dialog_run(menu_about_title(g_lang_shown), menu_about_text(g_lang_shown, "Linux"), ok, 1);
}

/* Rückfrage vor "Neues Spiel" (Fokus auf "Ja" wie bei MB_YESNO unter Windows) */
static int confirm_new_game(void)
{
    const char *yes_no[] = {menu_yes(g_lang_shown), menu_no(g_lang_shown)};
    return dialog_run(menu_text(&menu_bar[0].sub[0], g_lang_shown), menu_new_game_text(g_lang_shown), yes_no, 2) == 0;
}

static void menu_changed(void)
{
    /* die Leiste liest den Zustand bei jedem Zeichnen */
}

/* Ereignis zuerst an die Menüleiste; ausgewählte Befehle wie unter Windows als Wünsche ablegen */
int video_menu_event(const SDL_Event *e)
{
    int used = menubar_event(e);
    menu_command(menubar_take_command());
    /* über Leiste und Menüs den System-Zeiger zeigen, sonst nur den des Spiels */
    SDL_ShowCursor(menubar_wants_cursor() ? SDL_ENABLE : SDL_DISABLE);
    return used;
}

/* im Vollbild: Leiste per Alt eingeblendet (solange ein Menü offen ist) */
int video_menu_overlay(void)
{
    return video_is_fullscreen() && menubar_active();
}

int video_menu_overlay_changed(void)
{
    int a = video_menu_overlay();
    int r = a != g_menu_was_active;
    g_menu_was_active = a;
    return r;
}
#endif

#ifndef _WIN32
/* Fläche des Spielbilds in Ausgabepixeln: 4:3, mittig unter der Menüleiste (unter Windows liegt die
 * Leiste außerhalb der Fensterfläche und SDL rechnet über die logische Größe) */
static void game_rect(SDL_Rect *r)
{
    int ow = 0, oh = 0, bar = menubar_height_px();
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    int ah = oh - bar > 1 ? oh - bar : 1;
    if ((long)ow * 3 > (long)ah * 4) {
        r->h = ah;
        r->w = ah * 4 / 3;
    } else {
        r->w = ow;
        r->h = ow * 3 / 4;
    }
    r->x = (ow - r->w) / 2;
    r->y = bar + (ah - r->h) / 2;
}
#endif

int video_init(const char *title, int scale)
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        return -1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    /* 320x200 mit nicht-quadratischen Pixeln -> 4:3-Ausgabe (320x240 pro Faktor). */
    int extra_h = 0;
#ifndef _WIN32
    extra_h = menubar_height_points(); /* Platz für die Menüleiste, Spielfläche bleibt 4:3 */
#endif
    g_win = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             320 * scale, 240 * scale + extra_h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
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
#ifdef _WIN32
    SDL_RenderSetLogicalSize(g_ren, 320 * 4, 240 * 4);
#else
    menubar_init(g_win, g_ren); /* ohne logische Größe: Leiste in Ausgabepixeln, Spielbild in game_rect */
    /* Startgröße in Punkten der Desktop-Skalierung (unter Windows rechnet das System sie ein); nicht
     * größer als der freie Bildschirmbereich */
    float f = menubar_ui_scale();
    if (f < 0.99f || f > 1.01f) {
        SDL_Rect area;
        int s = scale, w = (int)(320 * s * f + 0.5f), h = (int)(240 * s * f + 0.5f);
        if (SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(g_win), &area) == 0)
            while (s > 1 && (w > area.w || h + menubar_height_points() > area.h)) {
                s--;
                w = (int)(320 * s * f + 0.5f);
                h = (int)(240 * s * f + 0.5f);
            }
        SDL_SetWindowSize(g_win, w, h + menubar_height_points());
        SDL_SetWindowPosition(g_win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
#endif
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
#ifndef _WIN32
    menubar_shutdown();
#endif
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
#ifndef _WIN32
    menubar_set_fullscreen(!fs); /* im Vollbild nur per Alt */
#else
    if (fs)
        menubar_show(1);
    menu_update(!fs);
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

/* Mausposition (Fensterkoordinaten von SDL) in Spielpixel. Unter Windows rechnet SDL über die
 * logische Größe 1280x960 schon heraus; unter Linux zählt die Fläche unter der Menüleiste. */
void video_mouse_to_game(int x, int y, float *gx, float *gy)
{
#ifdef _WIN32
    *gx = (float)x * SCREEN_W / (320 * 4);
    *gy = (float)y * SCREEN_H / (240 * 4);
#else
    SDL_Rect r;
    int ww = 0, wh = 0, ow = 0, oh = 0;
    game_rect(&r);
    SDL_GetWindowSize(g_win, &ww, &wh);
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    float d = ww > 0 && ow > 0 ? (float)ow / (float)ww : 1.0f; /* Fensterpunkte -> Ausgabepixel */
    *gx = ((float)x * d - (float)r.x) * SCREEN_W / (float)r.w;
    *gy = ((float)y * d - (float)r.y) * SCREEN_H / (float)r.h;
#endif
}

/* Spielpixel je Einheit einer relativen Mausbewegung */
void video_mouse_rel_scale(float *sx, float *sy)
{
#ifdef _WIN32
    *sx = (float)SCREEN_W / (320 * 4);
    *sy = (float)SCREEN_H / (240 * 4);
#else
    float x0, y0, x1, y1;
    video_mouse_to_game(0, 0, &x0, &y0);
    video_mouse_to_game(100, 100, &x1, &y1);
    *sx = (x1 - x0) / 100;
    *sy = (y1 - y0) / 100;
#endif
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
#ifdef _WIN32
    double s = (double)ow / (320 * 4) < (double)oh / (240 * 4) ? (double)ow / (320 * 4) : (double)oh / (240 * 4);
#else
    SDL_Rect game;
    game_rect(&game);
    double s = (double)game.w / (320 * 4);
#endif
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
#ifdef _WIN32
    SDL_RenderCopy(g_ren, out, NULL, NULL); /* bilinear auf die 4:3-Fläche (logische Größe) */
#else
    SDL_RenderCopy(g_ren, out, NULL, &game); /* bilinear auf die 4:3-Fläche unter der Leiste */
    MenuState ms = menu_state(video_is_fullscreen());
    menubar_draw(&ms);
#endif
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
