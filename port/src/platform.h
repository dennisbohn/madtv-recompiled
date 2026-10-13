/* Plattformschicht: SDL-Ereignisse, Zeit, Eingabe und Bildausgabe.
 * Der Spielcode sieht davon nichts direkt; er benutzt die Original-API aus rtlib.h. */
#ifndef MADTV_PLATFORM_H
#define MADTV_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

/* windowed = 0: Vollbild (Desktop-Auflösung) */
int platform_init(const char *game_dir, int windowed);
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
/* Position des Windows-Zeigers (im Vollbild: des virtuellen Zeigers) in Spielpixeln, falls er
 * sich seit dem letzten Aufruf bewegt hat (sonst 0). */
/* cur_x/cur_y: aktuelle Position des Spielzeigers (Startpunkt im Vollbild) */
int platform_mouse_abs(int16_t cur_x, int16_t cur_y, int16_t *x, int16_t *y);
/* Gemeldete Zeigerposition auf Spielpixel x/y beschneiden (Grenzen des Spiels). Im Vollbild
 * (relative Maus) wird so der virtuelle Zeiger begrenzt; im Fenster bleibt der Windows-Zeiger
 * frei, damit man das Fenster verlassen kann. */
void platform_mouse_confine(int16_t x, int16_t y);

/* Zeiger-Overlay: Das Spiel liefert über diese Funktion bei jedem angezeigten Bild das aktuelle
 * Zeigerbild (Palettenindizes, 0 = transparent; NULL = kein Zeiger). x und y enthalten beim Aufruf
 * die neueste Position des Windows-Zeigers in 1/256 Spielpixel, falls have_abs, und werden auf
 * die Zeichenposition gesetzt (ebenfalls in 1/256 Spielpixel). */
typedef const uint8_t *(*PlatformCursorFn)(int *w, int *h, int *stride, int *x, int *y, int have_abs);
void platform_set_cursor_provider(PlatformCursorFn fn);

/* Auf den nächsten (emulierten) VGA-Strahlrücklauf warten: 70 Hz wie im Original, unabhängig
 * von der Bildwiederholrate des Monitors. Zeigt dabei Bilder an, sobald der Monitor eines will. */
void platform_wait_retrace(void);

/* Bis zum nächsten angezeigten Bild warten (V-Sync); für flüssige Übergänge */
void platform_wait_frame(void);
/* Anzahl bisher angezeigter Bilder (ändert sich einmal pro Bildwiederholung) */
uint32_t platform_frame_count(void);
/* Anzeige vorübergehend anhalten (1) bzw. freigeben (0): während das Spiel einen Schritt rechnet,
 * soll kein Bild mit halbfertigem bzw. veraltetem Stand hinausgehen */
void platform_present_hold(int on);
/* Zeit in Mikrosekunden (hochauflösend) und voraussichtlicher Zeitpunkt, zu dem das nächste Bild
 * erscheint (letzter V-Sync + Bilddauer) – für gleichmäßig verteilte Zwischenschritte */
uint64_t platform_time_us(void);
uint64_t platform_next_frame_us(void);

/* Wurde das Fenster geschlossen? */
int platform_quit_requested(void);

const char *platform_game_dir(void);
/* Ins Programm eingebettete Daten (Windows-Ressource RCDATA "name"), sonst NULL */
const uint8_t *platform_resource(const char *name, size_t *len);
/* Ordner für Spielstände (Windows: "Gespeicherte Spiele\Mad TV Recompiled"; MADTV_SAVEDIR ersetzt ihn) */
const char *platform_save_dir(void);
void platform_set_game_dir(const char *dir);
/* PORT: Einstellungen als Zeilen "name=zahl" in settings.ini im Spielstandordner */
int platform_setting_get(const char *name, int def);
void platform_setting_set(const char *name, int value);
/* 1, wenn die Anzeigesprache des Systems Deutsch ist */
int platform_system_language_german(void);
/* Sprache (0 Deutsch, 1 Englisch) im Fenstermenü anzeigen; Wunsch aus Menü oder F10 abholen
 * (-1 = keiner) */
void platform_language_show(int lang);
int platform_language_request(void);
/* PORT: Menü "Spiel" und Schalter des Control-Centers: Zustand anzeigen (game 1 = Bildschirm
 * "Neues Spiel", 2 = Spiel läuft; Spieltempo 20/30/60; switches Bit i = Schalter i an: 0
 * Fernsehfamilie, 1 Fernsehbild, 2 Musik) und Wünsche abholen: neues Spiel (einmalig 1), Spieltempo
 * (sonst 0), Schalter (i * 2 + an, sonst -1) */
void platform_game_show(int game, int speed, int switches, int paused);
/* PORT: Menü "Pause" gewählt (einmalig 1: umschalten) */
int platform_pause_request(void);
int platform_new_game_request(void);
int platform_speed_request(void);
int platform_switch_request(void);

/* Testhilfe: idx-ter Raum aus MADTV_ROOM, sobald fällig (-1 = nicht/noch nicht) */
int platform_debug_room(int idx);
/* Testhilfe: Programm für 18:00 aus MADTV_PROG (-1 = nicht gesetzt) */
int platform_debug_prog(void);
/* Testhilfen (MADTV_SDLKEYS, MADTV_OUTSHOT ...) während modaler Dialoge weiterlaufen lassen */
void platform_modal_poll(void);
/* Testhilfe: Zeile nach stderr */
void platform_debug_log(const char *fmt, ...);
/* Testhilfe: Zahlenwert einer Umgebungsvariable (0 = nicht gesetzt) */
int platform_debug_env(const char *name);
/* Testhilfe: Faktor für die Spieluhr aus MADTV_CLOCK (1 = normal) */
int platform_clock_factor(void);

#endif
