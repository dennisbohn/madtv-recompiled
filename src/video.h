/* Ersatz für VGA-Modus 13h: 320x200-Framebuffer mit 256-Farben-Palette,
 * dargestellt über SDL2 im Seitenverhältnis 4:3 (wie auf einem CRT). */
#ifndef MADTV_VIDEO_H
#define MADTV_VIDEO_H

#include <stdint.h>

enum { SCREEN_W = 320, SCREEN_H = 200 };

/* Entspricht dem Bildschirmspeicher bei A000:0000. */
extern uint8_t g_screen[SCREEN_W * SCREEN_H];

int video_init(const char *title, int scale);
void video_shutdown(void);
void video_set_palette(const uint8_t rgb[768]);
void video_set_title(const char *title);
void video_toggle_fullscreen(void);
int video_is_fullscreen(void);
/* PORT: Sprachwahl im Systemmenü des Fensters ("Deutsch"/"English", Häkchen auf lang) und F10
 * (wechselt). video_language_request liefert einmalig den Wunsch (0 Deutsch, 1 Englisch) oder -1. */
void video_language_show(int lang);
int video_language_request(void);
#ifndef _WIN32
/* PORT: F10 ohne Menüleiste (Linux): Sprache wechseln */
void video_language_toggle(void);
/* PORT: Fenstersymbol (Linux; unter Windows aus der Programmressource) */
void video_set_icon(const uint8_t *rgba, int w, int h);
#endif
/* PORT: Menüeintrag "Vollbild" gewählt (einmalig 1, sonst 0) */
int video_fullscreen_request(void);
/* PORT: Menüeintrag "Beenden" gewählt */
int video_quit_request(void);
/* PORT: Spielzustand fürs Menü (wie platform_game_show) und Wünsche aus dem Menü: neues Spiel
 * (einmalig 1), Spieltempo (20/30/60, sonst 0), Schalter (i * 2 + an, sonst -1) */
void video_game_show(int game, int speed, int switches, int paused);
int video_pause_request(void);
int video_new_game_request(void);
int video_speed_request(void);
int video_switch_request(void);
/* PORT: Menüleiste im Vollbild per Alt eingeblendet? / seit der letzten Abfrage geändert? */
int video_menu_overlay(void);
int video_menu_overlay_changed(void);
uint32_t video_window_id(void);
/* Mausposition aus SDL-Ereignissen (Fensterkoordinaten) in Spielpixel umrechnen; Werte außerhalb
 * von 0..319/0..199 liegen neben dem Spielbild */
void video_mouse_to_game(int x, int y, float *gx, float *gy);
/* Spielpixel je Einheit einer relativen Mausbewegung (Vollbild) */
void video_mouse_rel_scale(float *sx, float *sy);
/* PORT: Ereignis zuerst der Menüleiste geben (Linux, selbst gezeichnet); 1 = dort verbraucht */
union SDL_Event;
int video_menu_event(const union SDL_Event *e);
/* Framebuffer anzeigen (ersetzt das Warten auf den vertikalen Strahlrücklauf). */
void video_present(void);
/* Mauszeiger als Overlay: w x h Palettenindizes (0 = transparent, Zeilenabstand stride), oben
 * links an Position x/y in 1/256 Spielpixel. px = NULL blendet ihn aus. Er wird bei jedem video_present über das Bild gelegt, liegt also
 * immer auf dem 320x200-Raster, wird aber so oft gezeichnet, wie der Monitor Bilder anzeigt. */
void video_set_cursor(const uint8_t *px, int w, int h, int stride, int x, int y);
/* Bildwiederholrate des Monitors, auf dem das Fenster liegt (Hz; 60, falls unbekannt) */
int video_refresh_rate(void);
/* aktuelles Bild mit Palette als 8-Bit-BMP speichern (Testhilfe) */
int video_save_bmp(const char *path);
/* Bis zu 4 Bereiche (zwischen video_layers_begin und _commit): Bereich (Spielpixel) um frac256/256 Pixel
 * nach unten versetzt darstellen – die Pixel bleiben ganze, scharfe 320x200-Pixel, nur ihre Lage wird fein
 * verschoben. row_above (w Palettenindizes) ist die Zeile, die oben in den Bereich nachrückt. */
void video_set_subpixel_shift(int x, int y, int w, int h, int frac256, const uint8_t *row_above);
/* Bis zu 6 Ebenen (in dieser Reihenfolge übereinander, zwischen begin und commit): eigene Ebene
 * über dem Bild, Bereich x/y/w/h (Spielpixel), um frac256/256 Pixel nach unten versetzt. px: h+1
 * Zeilen Palettenindizes (Zeilenabstand stride), beginnend mit der Zeile über dem Bereich.
 * Deckung: mask (gleicher Aufbau, != 0 = deckend) oder ohne mask: transparent -> Farbe 0 frei. */
void video_add_layer(int x, int y, int w, int h, int frac256, const uint8_t *px, int stride, int transparent,
                     const uint8_t *mask);
/* Ebenen/Feinverschiebungen neu beginnen bzw. abschließen: commit merkt sich das Grundbild
 * (oberer Bereich von g_screen); sie gelten, bis neue kommen oder sich das Grundbild ändert. */
void video_layers_begin(void);
void video_layers_commit(void);
/* Testhilfe: das fertig skalierte Fensterbild beim nächsten Anzeigen als BMP speichern */
void video_request_output_shot(const char *path);

/* Sprite mit Farbschlüssel 0 an Pixelposition zeichnen, abgeschnitten am Rand. */
void video_blit_trans(const uint8_t *px, int w, int h, int x, int y);
void video_clear(uint8_t color);

#endif
