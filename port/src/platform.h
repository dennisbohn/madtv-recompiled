/* Plattformschicht: SDL-Ereignisse, Zeit, Eingabe und Bildausgabe.
 * Der Spielcode sieht davon nichts direkt; er benutzt die Original-API aus rtlib.h. */
#ifndef MADTV_PLATFORM_H
#define MADTV_PLATFORM_H

#include <stdint.h>

int platform_init(const char *game_dir);
void platform_shutdown(void);

/* Ereignisse verarbeiten und den Bildschirm höchstens ~70-mal pro Sekunde anzeigen.
 * Wird von allen Eingabe-, Zeit- und VSync-Funktionen aufgerufen, weil das Original
 * direkt in den Bildspeicher schreibt und das Bild nie explizit "anzeigt". */
void platform_pump(void);
/* Bild sofort anzeigen (z.B. beim Warten auf den Strahlrücklauf). */
void platform_present_now(void);

uint32_t platform_ms(void);
void platform_sleep_ms(uint32_t ms);

/* Tastatur wie BIOS INT 16h: (Scancode << 8) | ASCII, -1 wenn leer. */
int32_t platform_key_peek(void);
int32_t platform_key_get(void);
void platform_key_flush(void);

/* Maus: aufgelaufene Bewegung seit dem letzten Aufruf in Spielpixeln, Tasten (Bit0 links, Bit1 rechts). */
void platform_mouse_take(int16_t *dx, int16_t *dy, uint16_t *buttons);
/* Absolute Position des Windows-Zeigers in Spielpixeln, falls er sich seit dem letzten Aufruf
 * bewegt hat (sonst 0). */
int platform_mouse_abs(int16_t *x, int16_t *y);

/* Wurde das Fenster geschlossen? */
int platform_quit_requested(void);

const char *platform_game_dir(void);
void platform_set_game_dir(const char *dir);

/* Testhilfe: idx-ter Raum aus MADTV_ROOM, sobald fällig (-1 = nicht/noch nicht) */
int platform_debug_room(int idx);
/* Testhilfe: Programm für 18:00 aus MADTV_PROG (-1 = nicht gesetzt) */
int platform_debug_prog(void);
/* Testhilfe: Zeile nach stderr */
void platform_debug_log(const char *fmt, ...);
/* Testhilfe: Zahlenwert einer Umgebungsvariable (0 = nicht gesetzt) */
int platform_debug_env(const char *name);
/* Testhilfe: Faktor für die Spieluhr aus MADTV_CLOCK (1 = normal) */
int platform_clock_factor(void);

#endif
