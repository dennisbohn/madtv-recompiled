/* Benannte globale Variablen im Datensegment (Offsets wie in MTV.EXE, DS = 45d4).
 * Wird beim Übersetzen schrittweise ergänzt; Quellen: re/notes/architecture.md, Übersetzungs-Reports. */
#ifndef MADTV_GLOBALS_H
#define MADTV_GLOBALS_H

#include "dgroup.h"

/* Hardware-Bibliothek */
#define g_mouse_dx      DS_I16(0x16a2) /* Mausbewegung seit letzter Abfrage (mouse_poll) */
#define g_mouse_dy      DS_I16(0x16a4)
#define g_sound_is_adlib DS_I16(0x1602) /* 1 = AdLib-Treiber geladen */
#define g_tick_counter  DS_U16(0x9014) /* Spieltakt (ticks_get) */

/* Spielzustand */
#define g_local_player  DS_I16(0x8b88) /* Nummer des menschlichen Spielers (1..3) */
#define g_player_idx    DS_I16(0x7af2) /* Index des menschlichen Spielers (0..2) */
#define g_mouse_buttons DS_I16(0x7af4) /* entprellte Maustasten: 1 links, 2 rechts */
#define g_game_speed_setting DS_I16(0x7a30) /* Spieltempo */
#define g_cursor_x      DS_I16(0x7bd6) /* Mauszeiger-Position (Software-Cursor) */
#define g_cursor_y      DS_I16(0x7bd8)
#define g_language      DS_I16(0x055c) /* 0 = Deutsch, 1 = Englisch */
#define g_day           DS_I16(0x71c4) /* Spieltag */
#define g_hour          DS_I16(0x71ca) /* Uhrzeit im Spiel */
#define g_minute        DS_I16(0x71cc)
#define g_second        DS_I16(0x71ce)
#define g_cur_room      DS_I16(0x836c) /* aktueller Raum; 0 = Gebäude, 0x41 = Titel/Neues Spiel */
#define g_cur_dialog    DS_I16(0x8b84) /* offener Unterdialog im Raum */

#endif
