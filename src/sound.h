/* Ersatz für den PALLADIX-PSM-Soundtreiber (095.HES/098.HES).
 * Das Spiel ruft den Treiber über 13 Slots auf (MTV.EXE 43ae:0083 + 5*k). Bis der native
 * Musik-Player steht, sind die Slots Stubs, die wie ein fehlender Treiber -1 liefern.
 * Achtung: Slot 3 registriert den Tick-Zähler; die Zeit läuft im Port unabhängig davon
 * (siehe ticks_get in rtlib.c). */
#ifndef MADTV_SOUND_H
#define MADTV_SOUND_H

#include <stdint.h>

int32_t psm_slot_0(int16_t p1, int16_t p2, int16_t p3, int16_t p4); /* Init */
int32_t psm_slot_1(void);                                           /* Shutdown */
int32_t psm_slot_2(void);
int32_t psm_slot_3(void *tick_block);                               /* Tick-Zähler registrieren */
int32_t psm_slot_4(void);
int32_t psm_slot_5(int16_t on);                                     /* Musik an/aus */
int32_t psm_slot_6(void *song, uint16_t len);                       /* Song setzen */
int32_t psm_slot_7(void);
int32_t psm_slot_8(void);
int32_t psm_slot_9(int16_t cmd, int16_t arg);                       /* Kommando */
int32_t psm_slot_10(void);
int32_t psm_slot_11(void);
int32_t psm_slot_12(void);

#endif
