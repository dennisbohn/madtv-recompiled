/* PORT: selbst gezeichnete Menüleiste für Plattformen ohne natives Fenstermenü (Linux).
 * Inhalt aus menu.h; Aussehen nach KDE Breeze (Schrift Noto Sans, eingebettet). Gezeichnet wird in
 * Ausgabepixeln über dem Spielbild, Maus und Tastatur werden vor dem Spiel ausgewertet. */
#ifndef MADTV_MENUBAR_H
#define MADTV_MENUBAR_H

#include <SDL.h>

#include "menu.h"

void menubar_init(SDL_Window *win, SDL_Renderer *ren);
void menubar_shutdown(void);

/* Oberflächenskalierung des Desktops (1 = 100 %), z. B. 1.75 bei 168 dpi */
float menubar_ui_scale(void);
/* Höhe der Leiste in Fensterpunkten (zum Bemessen des Fensters) */
int menubar_height_points(void);
/* Höhe der Leiste in Ausgabepixeln; 0, wenn sie nicht sichtbar ist */
int menubar_height_px(void);
/* Leiste anzeigen? Im Fenster immer, im Vollbild nur, solange ein Menü offen ist (Alt) */
void menubar_set_fullscreen(int fullscreen);
/* ein Menü ist aufgeklappt */
int menubar_active(void);
/* Zeiger über Leiste oder Menü: dort den System-Zeiger zeigen */
int menubar_wants_cursor(void);

/* Ereignis auswerten; 1 = von der Leiste verbraucht (nicht ans Spiel weitergeben) */
int menubar_event(const SDL_Event *e);
/* gewählter Befehl (enum MenuCmd) seit dem letzten Aufruf oder MENU_NONE */
int menubar_take_command(void);

/* Programmsymbol für die Dialoge (RGBA-Bytes) */
void menubar_set_icon(const uint8_t *rgba, int w, int h);
/* Dialog öffnen ("Über" oder Rückfrage; Text wie menu_about_text) mit n Knöpfen (1..3, der erste hat
 * den Fokus) bzw. offen? Solange er offen ist, verbraucht menubar_event alle Eingaben. Ein Knopf,
 * Enter (Knopf mit Fokus) oder Esc schließen ihn; menubar_dialog_result liefert danach den Knopf
 * (0..n-1) oder -1 (Esc, geschlossen). */
void menubar_dialog_open(const char *title, const char *text, const char *const *buttons, int n);
int menubar_dialog_active(void);
void menubar_dialog_close(void);
int menubar_dialog_result(void);

/* Leiste und offene Menüs zeichnen (Renderziel: Fenster, ohne logische Größe); state bestimmt
 * Häkchen und gesperrte Einträge */
void menubar_draw(const MenuState *state);

#endif
