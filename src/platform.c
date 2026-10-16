#include <stdarg.h>
#include "platform.h"

#include <SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video.h"
#include "../third_party/tinf/tinf.h"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <dwmapi.h>
#else
#include <sys/stat.h>
#endif

static char g_game_dir[512] = "."; /* Ordner mit SOURCE1/2.RSC, falls nicht eingebettet */
static uint64_t g_last_present_us;
static uint32_t g_present_count;
static PlatformCursorFn g_cursor_fn;
static int g_quit;

/* Tastaturpuffer wie beim BIOS (16 Einträge) */
enum { KEYBUF = 16 };
static uint16_t g_keys[KEYBUF];
static int g_key_head, g_key_count;

/* Maus: Bruchteile aufheben, damit langsame Bewegungen nicht verloren gehen */
static float g_mouse_dx, g_mouse_dy;
static uint16_t g_mouse_buttons;
/* Die Maus wird nicht eingefangen: der Spielzeiger folgt der absoluten Position des
 * Windows-Zeigers (im Fenster ausgeblendet), so bleiben Titelleiste und Schließknopf erreichbar */
static int g_abs_x, g_abs_y, g_abs_new, g_abs_seen;
static float g_abs_fx, g_abs_fy; /* dieselbe Position mit Nachkommaanteil (für die Darstellung) */
/* Im Vollbild ist die Maus gefangen und meldet nur Bewegungen (relativ). Daraus wird ein
 * "virtueller Zeiger" in Spielpixeln geführt, der wie der Windows-Zeiger im Fenster behandelt
 * wird; die Grenzen des Spiels beschneiden ihn (platform_mouse_confine), so gibt es keine toten
 * Zonen. Beim Umschalten startet er an der aktuellen Position des Spielzeigers. */
static int g_rel_mode, g_rel_synced;
static float g_rel_x, g_rel_y, g_rel_pend_x, g_rel_pend_y;

static void mouse_mode_update(void)
{
    /* relativ nur im Vollbild und nicht, während die Menüleiste eingeblendet ist */
    int rel = video_is_fullscreen() && !video_menu_overlay();
    if (rel != g_rel_mode) {
        g_rel_mode = rel;
        g_rel_synced = 0;
        g_rel_pend_x = g_rel_pend_y = 0;
        SDL_SetRelativeMouseMode(rel ? SDL_TRUE : SDL_FALSE);
    }
}

/* Testläufe mit simulierten Klicks/Bewegungen: echte Maus über dem Fenster nicht einmischen */
static int scripted_mouse(void)
{
    static int scripted = -1;
    if (scripted < 0)
        scripted = (getenv("MADTV_CLICKS") != NULL || getenv("MADTV_MOVES") != NULL) && getenv("MADTV_ABSMOVES") == NULL;
    return scripted;
}

static uint64_t now_us(void)
{
    return SDL_GetPerformanceCounter() * 1000000ull / SDL_GetPerformanceFrequency();
}

void platform_set_cursor_provider(PlatformCursorFn fn)
{
    g_cursor_fn = fn;
}

/* Bild mit aktuellem Zeiger anzeigen */
static void present(void)
{
    if (g_cursor_fn) {
        /* Position in 1/256 Spielpixel: der Zeiger wird zwischen den Spielpixeln dargestellt */
        int w = 0, h = 0, stride = 0, x = (int)floorf(g_abs_fx * 256), y = (int)floorf(g_abs_fy * 256);
        const uint8_t *px = g_cursor_fn(&w, &h, &stride, &x, &y, g_abs_seen && !scripted_mouse());
        video_set_cursor(px, w, h, stride, x, y);
    }
    uint64_t t0 = now_us();
    video_present();
#ifdef _WIN32
    /* Unter dem Desktop-Compositor kehrt SDL_RenderPresent sofort zurück (das Bild wird nur
     * eingereiht). DwmFlush wartet auf den nächsten Compositor-Takt (= V-Sync des Monitors): so
     * fällt jedes Bild auf genau einen Monitortakt, und der Zeitpunkt danach ist der echte
     * Anzeigetakt, an dem sich die Zwischenschritte ausrichten. */
    DwmFlush();
#endif
    g_last_present_us = now_us();
    g_present_count++;
    if (getenv("MADTV_FRAMELOG"))
        platform_debug_log("P %llu %llu", (unsigned long long)t0, (unsigned long long)g_last_present_us);
}

/* Ist ein neues Bild fällig? Einmal pro Bildwiederholung des Monitors (Rate jede Sekunde neu
 * abfragen, falls das Fenster auf einen anderen Monitor wandert). Abgeschickt wird schon nach
 * einer halben Bilddauer, damit das Bild sicher vor dem nächsten Takt eingereiht ist; das Warten
 * auf den Takt übernimmt present(). */
static uint64_t g_frame_us = 16666;

static int g_present_hold; /* Anzeige kurz anhalten (platform_present_hold) */

void platform_present_hold(int on)
{
    g_present_hold = on;
}

static int present_due(void)
{
    if (g_present_hold)
        return 0;
    static uint64_t checked_us;
    uint64_t frame_us = g_frame_us;
    uint64_t now = now_us();
    if (now - checked_us > 1000000) {
        checked_us = now;
        frame_us = g_frame_us = 1000000ull / (uint64_t)video_refresh_rate();
    }
    return now - g_last_present_us >= frame_us / 2;
}

/* Ordner für Spielstände: Windows-Ordner "Gespeicherte Spiele" (FOLDERID_SavedGames, auch wenn
 * umgeleitet) bzw. SDL-Benutzerordner, darin "Mad TV Recompiled". MADTV_SAVEDIR ersetzt ihn
 * (Testläufe). Der Ordner wird bei Bedarf angelegt. */
const char *platform_save_dir(void)
{
    static char dir[600];
    if (dir[0])
        return dir;
    const char *env = getenv("MADTV_SAVEDIR");
    if (env && *env)
        snprintf(dir, sizeof dir, "%s", env);
#ifdef _WIN32
    if (!dir[0]) {
        static const GUID saved_games = {0x4c5c32ff, 0xbb9d, 0x43b0, {0xb5, 0xb4, 0x2d, 0x72, 0xe5, 0x4e, 0xaa, 0xa4}};
        PWSTR wide = NULL;
        if (SHGetKnownFolderPath(&saved_games, 0, NULL, &wide) == S_OK) {
            char base[500];
            if (WideCharToMultiByte(CP_ACP, 0, wide, -1, base, sizeof base, NULL, NULL) > 0)
                snprintf(dir, sizeof dir, "%s\\Mad TV Recompiled", base);
        }
        CoTaskMemFree(wide);
    }
#endif
    if (!dir[0]) {
        char *pref = SDL_GetPrefPath("", "Mad TV Recompiled");
        if (pref) {
            snprintf(dir, sizeof dir, "%s", pref);
            size_t n = strlen(dir);
            if (n > 0 && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
                dir[n - 1] = 0;
            SDL_free(pref);
        } else {
            snprintf(dir, sizeof dir, "%s", g_game_dir);
        }
    }
#ifdef _WIN32
    CreateDirectoryA(dir, NULL);
#else
    mkdir(dir, 0755);
#endif
    return dir;
}

static void settings_path(char *path, size_t n)
{
    snprintf(path, n, "%s/settings.ini", platform_save_dir());
}

int platform_setting_get(const char *name, int def)
{
    char path[700], line[200];
    size_t len = strlen(name);
    int value = def;
    settings_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (!f)
        return def;
    while (fgets(line, sizeof line, f))
        if (strncmp(line, name, len) == 0 && line[len] == '=')
            value = atoi(line + len + 1);
    fclose(f);
    return value;
}

void platform_setting_set(const char *name, int value)
{
    char path[700], line[200], out[4096];
    size_t len = strlen(name), used = 0;
    settings_path(path, sizeof path);
    FILE *f = fopen(path, "r");
    if (f) { /* übrige Einträge übernehmen */
        while (fgets(line, sizeof line, f))
            if (!(strncmp(line, name, len) == 0 && line[len] == '=') && used + strlen(line) < sizeof out - 100)
                used += (size_t)snprintf(out + used, sizeof out - used, "%s", line);
        fclose(f);
    }
    snprintf(out + used, sizeof out - used, "%s=%d\n", name, value);
    f = fopen(path, "w");
    if (f) {
        fputs(out, f);
        fclose(f);
    }
}

int platform_system_language_german(void)
{
#ifdef _WIN32
    return PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
#else
    const char *l = getenv("LANG");
    return l && strncmp(l, "de", 2) == 0;
#endif
}

void platform_language_show(int lang)
{
    video_language_show(lang);
}

int platform_language_request(void)
{
    return video_language_request();
}

void platform_game_show(int game, int speed, int switches, int paused)
{
    video_game_show(game, speed, switches, paused);
}

int platform_pause_request(void)
{
    return video_pause_request();
}

int platform_new_game_request(void)
{
    return video_new_game_request();
}

int platform_speed_request(void)
{
    return video_speed_request();
}

int platform_switch_request(void)
{
    return video_switch_request();
}

/* Eingebettete Ressource name, noch gepackt: unter Windows aus den Programmressourcen (RCDATA),
 * unter Linux aus der Tabelle mtv_bundle (build/linux/bundle.S, per .incbin eingebunden). Ohne
 * Bündel (Entwicklungs-Build) fehlt die Tabelle; sie ist daher nur schwach referenziert. */
#ifndef _WIN32
struct BundleEntry { /* drei zeigergroße Felder (.dc.a in bundle.S) */
    const char *name;
    const uint8_t *data;
    uintptr_t len;
};
extern const struct BundleEntry mtv_bundle[] __attribute__((weak));
#endif

static const uint8_t *resource_raw(const char *name, size_t *len)
{
#ifdef _WIN32
    HRSRC r = FindResourceA(NULL, name, MAKEINTRESOURCEA(10)); /* RT_RCDATA */
    HGLOBAL g = r ? LoadResource(NULL, r) : NULL;
    const uint8_t *p = g ? LockResource(g) : NULL;
    if (p)
        *len = SizeofResource(NULL, r);
    return p;
#else
    for (const struct BundleEntry *e = mtv_bundle; e && e->name; e++)
        if (strcmp(e->name, name) == 0) {
            *len = e->len;
            return e->data;
        }
    return NULL;
#endif
}

/* Eingebettete Ressourcen sind gepackt ("MTVZ", Länge entpackt als u32, Deflate-Rohdaten; siehe
 * tools/make_bundle.py). Entpackt wird beim ersten Zugriff; der Speicher bleibt bis zum Ende. */
const uint8_t *platform_resource(const char *name, size_t *len)
{
    static struct {
        char name[16];
        uint8_t *data;
        size_t len;
    } cache[4];
    for (int i = 0; i < 4; i++)
        if (cache[i].data && strcmp(cache[i].name, name) == 0) {
            *len = cache[i].len;
            return cache[i].data;
        }
    size_t n = 0;
    const uint8_t *p = resource_raw(name, &n);
    if (p) {
        if (n < 8 || memcmp(p, "MTVZ", 4) != 0) { /* ungepackt */
            *len = n;
            return p;
        }
        unsigned int raw = (unsigned int)p[4] | (unsigned int)p[5] << 8 | (unsigned int)p[6] << 16 |
                           (unsigned int)p[7] << 24;
        uint8_t *out = malloc(raw ? raw : 1);
        unsigned int got = raw;
        if (out && tinf_uncompress(out, &got, p + 8, (unsigned int)(n - 8)) == TINF_OK && got == raw) {
            for (int i = 0; i < 4; i++)
                if (!cache[i].data) {
                    snprintf(cache[i].name, sizeof cache[i].name, "%s", name);
                    cache[i].data = out;
                    cache[i].len = raw;
                    break;
                }
            *len = raw;
            return out;
        }
        free(out); /* beschädigt: wie nicht vorhanden behandeln */
    }
    *len = 0;
    return NULL;
}

const char *platform_game_dir(void)
{
    return g_game_dir;
}

void platform_set_game_dir(const char *dir)
{
    snprintf(g_game_dir, sizeof g_game_dir, "%s", dir);
}

uint32_t platform_ms(void)
{
    return SDL_GetTicks();
}

/* Testhilfe MADTV_CLOCK=f[@ms][,f@ms...]: Spieluhr (Spielminuten) f-mal schneller, ab Zeitpunkt ms
 * der jeweils letzte fällige Eintrag; alles andere läuft in Echtzeit */
int platform_clock_factor(void)
{
    const char *p = getenv("MADTV_CLOCK");
    int f = 1;
    if (!p)
        return 1;
    uint32_t now = platform_ms();
    while (*p) {
        char *end;
        long v = strtol(p, &end, 10);
        unsigned long ms = 0;
        if (*end == '@')
            ms = strtoul(end + 1, &end, 10);
        if (now >= ms && v >= 1)
            f = (int)v;
        if (*end != ',')
            break;
        p = end + 1;
    }
    return f;
}

void platform_sleep_ms(uint32_t ms)
{
    SDL_Delay(ms);
}

int platform_quit_requested(void)
{
    return g_quit;
}

static void key_push(uint8_t scancode, uint8_t ascii)
{
    if (g_key_count >= KEYBUF)
        return; /* BIOS piept und verwirft */
    g_keys[(g_key_head + g_key_count) % KEYBUF] = (uint16_t)((scancode << 8) | ascii);
    g_key_count++;
}

/* PC-Scancodes (Satz 1) für Tasten ohne ASCII-Zeichen bzw. mit Sonderbedeutung. */
static uint8_t pc_scancode(SDL_Scancode s)
{
    switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER: return 0x1C;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_UP: return 0x48;
    case SDL_SCANCODE_DOWN: return 0x50;
    case SDL_SCANCODE_LEFT: return 0x4B;
    case SDL_SCANCODE_RIGHT: return 0x4D;
    case SDL_SCANCODE_HOME: return 0x47;
    case SDL_SCANCODE_END: return 0x4F;
    case SDL_SCANCODE_PAGEUP: return 0x49;
    case SDL_SCANCODE_PAGEDOWN: return 0x51;
    case SDL_SCANCODE_INSERT: return 0x52;
    case SDL_SCANCODE_DELETE: return 0x53;
    case SDL_SCANCODE_MINUS:
    case SDL_SCANCODE_KP_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_KP_PLUS: return 0x4E;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    default: break;
    }
    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10)
        return (uint8_t)(0x3B + (s - SDL_SCANCODE_F1));
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0)
        return (uint8_t)(0x02 + (s - SDL_SCANCODE_1));
    static const uint8_t letters[26] = {0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17,
                                        0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13,
                                        0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z)
        return letters[s - SDL_SCANCODE_A];
    return 0;
}

/* Vollbild umschalten; der Modus gilt auch beim nächsten Start (settings.ini) */
static void fullscreen_toggle(void)
{
    video_toggle_fullscreen();
    mouse_mode_update();
    platform_setting_set("fullscreen", video_is_fullscreen());
}

static void on_key(const SDL_KeyboardEvent *k)
{
    SDL_Scancode s = k->keysym.scancode;
    SDL_Keycode sym = k->keysym.sym;
    if ((s == SDL_SCANCODE_RETURN && (k->keysym.mod & KMOD_ALT)) || s == SDL_SCANCODE_F11) {
        fullscreen_toggle();
        return;
    }
#ifndef _WIN32
    /* unter Windows fängt die Fensterprozedur F10 ab (video.c); das Spiel bekommt die Taste nie */
    if (s == SDL_SCANCODE_F10) {
        video_language_toggle();
        return;
    }
#endif
    uint8_t sc = pc_scancode(s);
    uint8_t ascii = 0;
    int shift = (k->keysym.mod & KMOD_SHIFT) != 0;
    if (sym >= 'a' && sym <= 'z')
        ascii = (uint8_t)(shift ? sym - 32 : sym);
    else if (sym >= '0' && sym <= '9')
        ascii = (uint8_t)sym;
    else if (sym >= SDLK_KP_1 && sym <= SDLK_KP_9)
        ascii = (uint8_t)('1' + (sym - SDLK_KP_1));
    else if (sym == SDLK_KP_0)
        ascii = '0';
    else {
        switch (s) {
        case SDL_SCANCODE_ESCAPE: ascii = 27; break;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER: ascii = 13; break;
        case SDL_SCANCODE_BACKSPACE: ascii = 8; break;
        case SDL_SCANCODE_TAB: ascii = 9; break;
        case SDL_SCANCODE_SPACE: ascii = ' '; break;
        case SDL_SCANCODE_MINUS:
        case SDL_SCANCODE_KP_MINUS: ascii = '-'; break;
        case SDL_SCANCODE_KP_PLUS: ascii = '+'; break;
        case SDL_SCANCODE_EQUALS: ascii = (uint8_t)(shift ? '+' : '='); break;
        case SDL_SCANCODE_COMMA: ascii = ','; break;
        case SDL_SCANCODE_PERIOD: ascii = '.'; break;
        default: break;
        }
    }
    if (sym == '+' || sym == SDLK_PLUS)
        ascii = '+';
    if (sc || ascii)
        key_push(sc, ascii);
}

static void pump_events(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (video_menu_event(&e)) /* von der Menüleiste verbraucht (Linux) */
            continue;
        switch (e.type) {
        case SDL_QUIT:
            /* Fenster geschlossen: beenden wie das Original (atexit-Handler schalten Musik ab) */
            g_quit = 1;
            exit(0);
        case SDL_KEYDOWN: on_key(&e.key); break;
        case SDL_MOUSEMOTION: {
            /* Fensterkoordinaten -> Spielpixel (video_mouse_to_game). Nicht begrenzen: Werte außerhalb
             * (schwarze Ränder) erkennt mouse_poll und holt den Zeiger zurück. */
            if (g_rel_mode) { /* Vollbild: nur die Bewegung zählt */
                float sx, sy;
                video_mouse_rel_scale(&sx, &sy);
                float dx = (float)e.motion.xrel * sx, dy = (float)e.motion.yrel * sy;
                if (!g_rel_synced) { /* Startpunkt noch unbekannt: Bewegung sammeln */
                    g_rel_pend_x += dx;
                    g_rel_pend_y += dy;
                    break;
                }
                g_rel_x += dx;
                g_rel_y += dy;
                g_abs_fx = g_rel_x;
                g_abs_fy = g_rel_y;
            } else {
                video_mouse_to_game(e.motion.x, e.motion.y, &g_abs_fx, &g_abs_fy);
            }
            g_abs_x = (int)floorf(g_abs_fx);
            g_abs_y = (int)floorf(g_abs_fy);
            g_abs_new = 1;
            g_abs_seen = 1;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            uint16_t bit = e.button.button == SDL_BUTTON_LEFT ? 1 : e.button.button == SDL_BUTTON_RIGHT ? 2 : 0;
            if (e.type == SDL_MOUSEBUTTONDOWN)
                g_mouse_buttons |= bit;
            else
                g_mouse_buttons &= (uint16_t)~bit;
            break;
        }
        default: break;
        }
    }
    if (video_quit_request()) { /* Menü "Spiel > Beenden": wie Fenster schließen */
        g_quit = 1;
        exit(0);
    }
    if (video_fullscreen_request()) /* Menü "Einstellungen > Vollbild" (wie F11) */
        fullscreen_toggle();
    if (video_menu_overlay_changed()) /* Menüleiste im Vollbild ein-/ausgeblendet */
        mouse_mode_update();
}

void platform_present_now(void)
{
    pump_events();
    present();
}

uint32_t platform_frame_count(void)
{
    return g_present_count;
}

uint64_t platform_time_us(void)
{
    return now_us();
}

uint64_t platform_next_frame_us(void)
{
    return g_last_present_us + g_frame_us;
}

void platform_wait_frame(void)
{
    uint32_t n = g_present_count;
    for (;;) {
        platform_pump();
        if (g_present_count != n)
            break;
        SDL_Delay(1);
    }
}

void platform_wait_retrace(void)
{
    static const uint64_t period_us = 14268; /* 1/70,086 Hz – VGA-Modus 13h */
    uint64_t next = (now_us() / period_us + 1) * period_us;
    for (;;) {
        platform_pump();
        uint64_t now = now_us();
        if (now >= next)
            break;
        if (next - now > 2000)
            SDL_Delay(1);
    }
}

/* ---- Testhilfen (nur über Umgebungsvariablen aktiv) ----
 * MADTV_SCREENSHOT=datei.bmp:ms[,datei2.bmp:ms2...]  Spielbild nach ms Millisekunden speichern
 * MADTV_KEYS=ms:ZEICHEN[,ms:ZEICHEN...]               Taste zum Zeitpunkt ms einspeisen
 *   ZEICHEN: Buchstabe/Ziffer, oder ENTER, ESC, SPACE, UP, DOWN, LEFT, RIGHT, F1..F10;
 *   Zusatz "*n" liefert die Taste n-mal hintereinander */
static void debug_hooks(void)
{
    static int shots_done, outshot_done;
    uint32_t now = platform_ms();
    const char *outshot = getenv("MADTV_OUTSHOT"); /* datei.bmp:ms[,...] – fertiges Fensterbild */
    if (outshot) {
        int idx = 0;
        for (const char *p = outshot; *p; idx++) {
            const char *colon = strchr(p, ':'), *comma = strchr(p, ',');
            if (!colon)
                break;
            if (!(outshot_done & (1 << idx)) && now >= (uint32_t)strtoul(colon + 1, NULL, 10)) {
                char path[260];
                size_t n = (size_t)(colon - p) < sizeof path - 1 ? (size_t)(colon - p) : sizeof path - 1;
                memcpy(path, p, n);
                path[n] = 0;
                video_request_output_shot(path);
                outshot_done |= 1 << idx;
                break; /* eins pro angezeigtem Bild */
            }
            if (!comma)
                break;
            p = comma + 1;
        }
    }
    const char *shots = getenv("MADTV_SCREENSHOT");
    if (shots) {
        int idx = 0;
        for (const char *p = shots; *p; idx++) {
            char path[260];
            unsigned ms = 0;
            const char *colon = strchr(p, ':');
            const char *comma = strchr(p, ',');
            if (!colon)
                break;
            size_t n = (size_t)(colon - p) < sizeof path - 1 ? (size_t)(colon - p) : sizeof path - 1;
            memcpy(path, p, n);
            path[n] = 0;
            ms = (unsigned)strtoul(colon + 1, NULL, 10);
            if (!(shots_done & (1 << idx)) && now >= ms) {
                shots_done |= 1 << idx;
                video_save_bmp(path);
            }
            if (!comma)
                break;
            p = comma + 1;
        }
    }
}

/* Nächste simulierte Taste (MADTV_KEYS), sobald ihr Zeitpunkt erreicht ist. Die Tasten werden
 * der Reihe nach erst beim Abholen ausgeliefert, damit kbd_flush des Spiels sie nicht löscht. */
static int32_t debug_next_key(void)
{
    static int delivered;
    const char *keys = getenv("MADTV_KEYS");
    if (!keys)
        return -1;
    int idx = 0;
    for (const char *p = keys; *p; idx++) {
        const char *colon = strchr(p, ':');
        const char *comma = strchr(p, ',');
        if (!colon)
            return -1;
        if (idx == delivered) {
            unsigned ms = (unsigned)strtoul(p, NULL, 10);
            if (platform_ms() < ms)
                return -1;
            char name[16];
            size_t n = comma ? (size_t)(comma - colon - 1) : strlen(colon + 1);
            if (n > sizeof name - 1)
                n = sizeof name - 1;
            memcpy(name, colon + 1, n);
            name[n] = 0;
            /* "E*3": Taste dreimal hintereinander liefern (z. B. Sprachwahl, wo auch game_tick
             * Tasten abholt und eine einzelne verschlucken kann) */
            static int repeated;
            char *star = strchr(name, '*');
            int times = star ? atoi(star + 1) : 1;
            if (star)
                *star = 0;
            if (++repeated >= times) {
                repeated = 0;
                delivered++;
            }
            static const struct { const char *n; uint8_t sc, ascii; } named[] = {
                {"ENTER", 0x1C, 13}, {"ESC", 0x01, 27}, {"SPACE", 0x39, ' '}, {"UP", 0x48, 0},
                {"DOWN", 0x50, 0},   {"LEFT", 0x4B, 0}, {"RIGHT", 0x4D, 0},
            };
            for (size_t i = 0; i < sizeof named / sizeof *named; i++)
                if (strcmp(name, named[i].n) == 0)
                    return (named[i].sc << 8) | named[i].ascii;
            if (name[0] == 'F' && name[1] >= '1' && name[1] <= '9')
                return (0x3A + atoi(name + 1)) << 8;
            return (uint8_t)name[0];
        }
        if (!comma)
            return -1;
        p = comma + 1;
    }
    return -1;
}

static void debug_sdlclicks(void);
static void debug_sdlkeys(void);

/* Testhilfen auch während modaler Dialoge der Menüleiste (video.c), die selbst Ereignisse holen */
void platform_modal_poll(void)
{
    debug_sdlclicks();
    debug_sdlkeys();
    debug_hooks();
}

void platform_pump(void)
{
    debug_sdlclicks();
    debug_sdlkeys();
    pump_events();
    debug_hooks();
    if (present_due())
        present();
}

int32_t platform_key_peek(void)
{
    platform_pump();
    return g_key_count ? g_keys[g_key_head] : -1;
}

int32_t platform_key_get(void)
{
    platform_pump();
    if (!g_key_count)
        return debug_next_key();
    uint16_t k = g_keys[g_key_head];
    g_key_head = (g_key_head + 1) % KEYBUF;
    g_key_count--;
    return k;
}

void platform_key_flush(void)
{
    pump_events();
    g_key_count = 0;
}

/* Testhilfe MADTV_CLICKS=ms[:R],...: Klick (L Standard, R rechts), sobald die Spieluhr ms erreicht.
 * Die Klicks werden der Reihe nach ausgeliefert: je 80 ms Echtzeit gedrückt, dann 80 ms losgelassen,
 * damit das Spiel sie auch im Zeitraffer (MADTV_SPEED) sicher einzeln erkennt. */
static uint16_t debug_clicks(void)
{
    static int cur;          /* nächster auszuliefernder Klick */
    static uint32_t phase_t; /* Echtzeit-Beginn der laufenden Phase, 0 = wartet */
    static int pressed, polls;
    const char *clicks = getenv("MADTV_CLICKS");
    if (!clicks)
        return 0;
    uint32_t now = platform_ms(), real = SDL_GetTicks();
    int idx = 0;
    for (const char *p = clicks; *p; idx++) {
        char *end;
        unsigned ms = (unsigned)strtoul(p, &end, 10);
        uint16_t bit = 1;
        if (*end == ':') {
            bit = end[1] == 'R' ? 2 : 1;
            end += 2;
        }
        if (idx == cur) {
            if (phase_t == 0) {
                if (now < ms)
                    return 0;
                phase_t = real;
                pressed = 1;
                polls = 0;
            }
            polls++; /* jede Phase dauert mind. 80 ms und 2 Abfragen durch das Spiel */
            if (pressed && real - phase_t >= 80 && polls > 2) {
                pressed = 0;
                phase_t = real;
                polls = 0;
            } else if (!pressed && real - phase_t >= 80 && polls > 2) {
                cur++;
                phase_t = 0;
                return 0;
            }
            return pressed ? bit : 0;
        }
        if (*end != ',')
            break;
        p = end + 1;
    }
    return 0;
}

/* Testhilfe MADTV_MOVES=ms:dx:dy,...: Mausbewegung (Spielpixel) zum Zeitpunkt ms einspeisen */
static void debug_moves(void)
{
    static unsigned done;
    const char *moves = getenv("MADTV_MOVES");
    if (!moves)
        return;
    uint32_t now = platform_ms();
    int idx = 0;
    for (const char *p = moves; *p; idx++) {
        char *end;
        unsigned ms = (unsigned)strtoul(p, &end, 10);
        long dx = 0, dy = 0;
        if (*end == ':')
            dx = strtol(end + 1, &end, 10);
        if (*end == ':')
            dy = strtol(end + 1, &end, 10);
        if (!(done & (1u << idx)) && now >= ms) {
            done |= 1u << idx;
            g_mouse_dx += (float)dx;
            g_mouse_dy += (float)dy;
        }
        if (*end != ',')
            break;
        p = end + 1;
    }
}

/* Testhilfe MADTV_ABSMOVES=ms:x:y[,...]: Windows-Zeiger zum Zeitpunkt ms auf Spielpixel x/y setzen
 * (wie ein echtes Bewegungsereignis) */
/* Testhilfe MADTV_SDLCLICKS=ms:wx:wy[,...]: Linksklick als echte SDL-Ereignisse in
 * Fensterkoordinaten (Bewegung und Drücken, 100 ms später Loslassen); läuft wie die echte Maus
 * zuerst durch die Menüleiste */
static void debug_sdlclicks(void)
{
    static unsigned done, released;
    const char *clicks = getenv("MADTV_SDLCLICKS");
    if (!clicks)
        return;
    uint32_t now = platform_ms();
    int idx = 0;
    for (const char *p = clicks; *p; idx++) {
        char *end;
        unsigned ms = (unsigned)strtoul(p, &end, 10);
        long x = 0, y = 0;
        if (*end == ':')
            x = strtol(end + 1, &end, 10);
        if (*end == ':')
            y = strtol(end + 1, &end, 10);
        for (int k = 0; k < 2; k++) {
            unsigned *flags = k == 0 ? &done : &released;
            if ((*flags & (1u << idx)) || now < ms + (k == 0 ? 0u : 100u) || (k == 1 && !(done & (1u << idx))))
                continue;
            *flags |= 1u << idx;
            SDL_Event ev;
            if (k == 0) {
                memset(&ev, 0, sizeof ev);
                ev.type = SDL_MOUSEMOTION;
                ev.motion.windowID = video_window_id();
                ev.motion.x = (int)x;
                ev.motion.y = (int)y;
                SDL_PushEvent(&ev);
            }
            memset(&ev, 0, sizeof ev);
            ev.type = k == 0 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            ev.button.windowID = video_window_id();
            ev.button.button = SDL_BUTTON_LEFT;
            ev.button.state = k == 0 ? SDL_PRESSED : SDL_RELEASED;
            ev.button.clicks = 1;
            ev.button.x = (int)x;
            ev.button.y = (int)y;
            SDL_PushEvent(&ev);
        }
        if (*end != ',')
            break;
        p = end + 1;
    }
}

/* Testhilfe MADTV_SDLKEYS=ms:TASTE[,...]: Taste als echte SDL-Ereignisse (Drücken, Loslassen);
 * TASTE: ALT, ESC, ENTER, UP, DOWN, LEFT, RIGHT, F10, F11 oder ein Buchstabe A..Z */
static void debug_sdlkeys(void)
{
    static unsigned done;
    const char *keys = getenv("MADTV_SDLKEYS");
    if (!keys)
        return;
    static const struct {
        const char *n;
        SDL_Scancode sc;
    } named[] = {{"ALT", SDL_SCANCODE_LALT},   {"ESC", SDL_SCANCODE_ESCAPE}, {"ENTER", SDL_SCANCODE_RETURN},
                 {"UP", SDL_SCANCODE_UP},      {"DOWN", SDL_SCANCODE_DOWN},  {"LEFT", SDL_SCANCODE_LEFT},
                 {"RIGHT", SDL_SCANCODE_RIGHT}, {"F10", SDL_SCANCODE_F10},    {"F11", SDL_SCANCODE_F11}};
    uint32_t now = platform_ms();
    int idx = 0;
    for (const char *p = keys; *p; idx++) {
        char *end;
        unsigned ms = (unsigned)strtoul(p, &end, 10);
        const char *name = *end == ':' ? end + 1 : end;
        const char *comma = strchr(name, ',');
        size_t len = comma ? (size_t)(comma - name) : strlen(name);
        if (!(done & (1u << idx)) && now >= ms) {
            done |= 1u << idx;
            SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
            if (len == 1 && name[0] >= 'A' && name[0] <= 'Z')
                sc = (SDL_Scancode)(SDL_SCANCODE_A + (name[0] - 'A'));
            for (size_t i = 0; i < sizeof named / sizeof *named; i++)
                if (strlen(named[i].n) == len && strncmp(name, named[i].n, len) == 0)
                    sc = named[i].sc;
            for (int k = 0; k < 2 && sc != SDL_SCANCODE_UNKNOWN; k++) {
                SDL_Event ev;
                memset(&ev, 0, sizeof ev);
                ev.type = k == 0 ? SDL_KEYDOWN : SDL_KEYUP;
                ev.key.windowID = video_window_id();
                ev.key.state = k == 0 ? SDL_PRESSED : SDL_RELEASED;
                ev.key.keysym.scancode = sc;
                ev.key.keysym.sym = SDL_GetKeyFromScancode(sc);
                SDL_PushEvent(&ev);
            }
        }
        if (!comma)
            break;
        p = comma + 1;
    }
}

/* Testhilfe MADTV_SDLMOVES=ms:wx:wy[,...]: echtes SDL-Bewegungsereignis in Fensterkoordinaten
 * einspeisen (läuft wie die echte Maus durch die Umrechnung des Renderers) */
static void debug_sdlmoves(void)
{
    static unsigned done;
    const char *moves = getenv("MADTV_SDLMOVES");
    if (!moves)
        return;
    uint32_t now = platform_ms();
    int idx = 0;
    for (const char *p = moves; *p; idx++) {
        char *end;
        unsigned ms = (unsigned)strtoul(p, &end, 10);
        long x = 0, y = 0;
        if (*end == ':')
            x = strtol(end + 1, &end, 10);
        if (*end == ':')
            y = strtol(end + 1, &end, 10);
        if (!(done & (1u << idx)) && now >= ms) {
            done |= 1u << idx;
            SDL_Event ev;
            memset(&ev, 0, sizeof ev);
            ev.type = SDL_MOUSEMOTION;
            ev.motion.windowID = video_window_id();
            ev.motion.x = (int)x;
            ev.motion.y = (int)y;
            ev.motion.xrel = (int)x; /* im Vollbild (relative Maus) als Bewegung gedeutet */
            ev.motion.yrel = (int)y;
            SDL_PushEvent(&ev);
        }
        if (*end != ',')
            break;
        p = end + 1;
    }
}

static void debug_absmoves(void)
{
    static unsigned done;
    const char *moves = getenv("MADTV_ABSMOVES");
    if (!moves)
        return;
    uint32_t now = platform_ms();
    int idx = 0;
    for (const char *p = moves; *p; idx++) {
        char *end;
        unsigned ms = (unsigned)strtoul(p, &end, 10);
        long x = 0, y = 0;
        if (*end == ':')
            x = strtol(end + 1, &end, 10);
        if (*end == ':')
            y = strtol(end + 1, &end, 10);
        if (!(done & (1u << idx)) && now >= ms) {
            done |= 1u << idx;
            g_abs_x = (int)x;
            g_abs_y = (int)y;
            g_abs_fx = (float)x + 0.5f;
            g_abs_fy = (float)y + 0.5f;
            g_abs_new = g_abs_seen = 1;
        }
        if (*end != ',')
            break;
        p = end + 1;
    }
}

void platform_mouse_take(int16_t *dx, int16_t *dy, uint16_t *buttons)
{
    platform_pump();
    debug_moves();
    debug_absmoves();
    debug_sdlmoves();
    int ix = (int)g_mouse_dx, iy = (int)g_mouse_dy;
    g_mouse_dx -= (float)ix;
    g_mouse_dy -= (float)iy;
    *dx = (int16_t)ix;
    *dy = (int16_t)iy;
    *buttons = (uint16_t)(g_mouse_buttons | debug_clicks());
}

void platform_mouse_confine(int16_t x, int16_t y)
{
    /* nur den gemeldeten/virtuellen Zeiger beschneiden; der echte Zeiger wird nie versetzt */
    if (g_rel_mode || getenv("MADTV_ABSMOVES")) {
        g_abs_x = x;
        g_abs_y = y;
        g_abs_fx = g_rel_x = (float)x; /* genau auf der Grenze (ganzes Pixel) */
        g_abs_fy = g_rel_y = (float)y;
    }
}

void platform_mouse_detach(void)
{
    g_abs_new = g_abs_seen = 0;
    if (g_rel_mode) {
        g_rel_synced = 0;
        g_rel_pend_x = g_rel_pend_y = 0;
    }
}

int platform_mouse_abs(int16_t cur_x, int16_t cur_y, int16_t *x, int16_t *y)
{
    if (g_rel_mode && !g_rel_synced) { /* virtuellen Zeiger am Spielzeiger ansetzen */
        g_rel_synced = 1;
        g_rel_x = (float)cur_x + 0.5f + g_rel_pend_x;
        g_rel_y = (float)cur_y + 0.5f + g_rel_pend_y;
        g_abs_fx = g_rel_x;
        g_abs_fy = g_rel_y;
        g_abs_x = (int)floorf(g_rel_x);
        g_abs_y = (int)floorf(g_rel_y);
        g_abs_new = g_abs_seen = 1;
    }
    if (!g_abs_new || scripted_mouse())
        return 0;
    g_abs_new = 0;
    *x = (int16_t)g_abs_x;
    *y = (int16_t)g_abs_y;
    return 1;
}

int platform_init(int windowed)
{
    /* auch der Klick, der das Fenster erst aktiviert, zählt (sonst muss man z. B. die Sprache
     * zweimal anklicken) */
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    /* Fenster- und Taskleistensymbol aus der Programmressource 1 (res/madtv.rc) */
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON, "1");
    SDL_SetHint(SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL, "1");
    /* in voller Monitorauflösung rendern (sonst skaliert Windows bei 150/200 % das fertige Bild
     * noch einmal unscharf hoch); Fenstergröße und Mauskoordinaten bleiben in Punkten */
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#ifndef _WIN32
    /* relative Maus im Vollbild aus den normalen Zeigerbewegungen (Zeiger wird in die Mitte
     * zurückgesetzt) statt aus den rohen Gerätedaten: so gelten Zeigergeschwindigkeit und
     * -beschleunigung des Desktops wie außerhalb des Spiels */
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return -1;
    if (video_init("Mad TV Recompiled", 3) != 0)
        return -1;
#ifndef _WIN32
    size_t icon_len = 0;
    const uint8_t *icon = platform_resource("ICON", &icon_len);
    if (icon && icon_len == 64 * 64 * 4)
        video_set_icon(icon, 64, 64);
#endif
    /* PORT: erster Start im Fenster; danach der zuletzt gewählte Modus (settings.ini) */
    if (!windowed && platform_setting_get("fullscreen", 0) == 1)
        video_toggle_fullscreen(); /* Vollbild in Desktop-Auflösung, Bild im Seitenverhältnis 4:3 */
    mouse_mode_update();
    SDL_ShowCursor(SDL_DISABLE); /* nur über dem Fenster; das Spiel zeichnet seinen eigenen Zeiger */
    atexit(platform_shutdown); /* auch bei exit() aus dem Spiel oder beim Schließen des Fensters */
    return 0;
}

void platform_shutdown(void)
{
    video_shutdown();
    SDL_Quit();
}

/* MADTV_ROOM=raum[@ms][,raum@ms...]: Eintrag idx liefern, sobald die Uhr ms erreicht hat;
 * -1 = noch nicht so weit oder kein solcher Eintrag */
int platform_debug_room(int idx)
{
    const char *p = getenv("MADTV_ROOM");
    if (!p)
        return -1;
    for (int i = 0; *p; i++) {
        char *end;
        long room = strtol(p, &end, 0);
        unsigned long ms = 0;
        if (*end == '@')
            ms = strtoul(end + 1, &end, 10);
        if (i == idx)
            return platform_ms() >= ms ? (int)room : -1;
        if (*end != ',')
            break;
        p = end + 1;
    }
    return -1;
}

int platform_debug_prog(void)
{
    const char *r = getenv("MADTV_PROG");
    return r ? (int)strtol(r, NULL, 0) : -1;
}

void platform_debug_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc(10, stderr);
    fflush(stderr);
}

int platform_debug_env(const char *name)
{
    const char *r = getenv(name);
    return r ? (int)strtol(r, NULL, 0) : 0;
}
