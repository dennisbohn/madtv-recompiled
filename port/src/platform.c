#include <stdarg.h>
#include "platform.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video.h"

static char g_game_dir[512] = "Gamefiles";
static uint32_t g_last_present;
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
static int g_abs_x, g_abs_y, g_abs_new;

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

static void on_key(const SDL_KeyboardEvent *k)
{
    SDL_Scancode s = k->keysym.scancode;
    SDL_Keycode sym = k->keysym.sym;
    if ((s == SDL_SCANCODE_RETURN && (k->keysym.mod & KMOD_ALT)) || s == SDL_SCANCODE_F11) {
        video_toggle_fullscreen();
        return;
    }
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
        switch (e.type) {
        case SDL_QUIT:
            /* Fenster geschlossen: beenden wie das Original (atexit-Handler schalten Musik ab) */
            g_quit = 1;
            exit(0);
        case SDL_KEYDOWN: on_key(&e.key); break;
        case SDL_MOUSEMOTION: {
            int w, h;
            video_logical_size(&w, &h);
            /* logische Renderpixel (SDL rechnet Fensterskalierung und Ränder heraus) -> Spielpixel */
            g_abs_x = e.motion.x * SCREEN_W / w;
            g_abs_y = e.motion.y * SCREEN_H / h;
            g_abs_x = g_abs_x < 0 ? 0 : g_abs_x >= SCREEN_W ? SCREEN_W - 1 : g_abs_x;
            g_abs_y = g_abs_y < 0 ? 0 : g_abs_y >= SCREEN_H ? SCREEN_H - 1 : g_abs_y;
            g_abs_new = 1;
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
}

void platform_present_now(void)
{
    pump_events();
    video_present();
    g_last_present = SDL_GetTicks();
}

/* ---- Testhilfen (nur über Umgebungsvariablen aktiv) ----
 * MADTV_SCREENSHOT=datei.bmp:ms[,datei2.bmp:ms2...]  Spielbild nach ms Millisekunden speichern
 * MADTV_KEYS=ms:ZEICHEN[,ms:ZEICHEN...]               Taste zum Zeitpunkt ms einspeisen
 *   ZEICHEN: Buchstabe/Ziffer, oder ENTER, ESC, SPACE, UP, DOWN, LEFT, RIGHT, F1..F10;
 *   Zusatz "*n" liefert die Taste n-mal hintereinander */
static void debug_hooks(void)
{
    static int shots_done;
    uint32_t now = platform_ms();
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

void platform_pump(void)
{
    pump_events();
    debug_hooks();
    uint32_t now = SDL_GetTicks();
    if (now - g_last_present >= 14) {
        video_present();
        g_last_present = now;
    }
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

void platform_mouse_take(int16_t *dx, int16_t *dy, uint16_t *buttons)
{
    platform_pump();
    debug_moves();
    int ix = (int)g_mouse_dx, iy = (int)g_mouse_dy;
    g_mouse_dx -= (float)ix;
    g_mouse_dy -= (float)iy;
    *dx = (int16_t)ix;
    *dy = (int16_t)iy;
    *buttons = (uint16_t)(g_mouse_buttons | debug_clicks());
}

int platform_mouse_abs(int16_t *x, int16_t *y)
{
    /* Testläufe mit simulierten Klicks/Bewegungen: echte Maus über dem Fenster nicht einmischen */
    static int scripted = -1;
    if (scripted < 0)
        scripted = getenv("MADTV_CLICKS") != NULL || getenv("MADTV_MOVES") != NULL;
    if (!g_abs_new || scripted)
        return 0;
    g_abs_new = 0;
    *x = (int16_t)g_abs_x;
    *y = (int16_t)g_abs_y;
    return 1;
}

int platform_init(const char *game_dir)
{
    platform_set_game_dir(game_dir);
    /* auch der Klick, der das Fenster erst aktiviert, zählt (sonst muss man z. B. die Sprache
     * zweimal anklicken) */
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return -1;
    if (video_init("MadTV", 3) != 0)
        return -1;
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
