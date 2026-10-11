/* Das Datensegment (DGROUP) des Originalspiels.
 *
 * MTV.EXE adressiert alle globalen Variablen über feste Offsets in DS (Segment 45d4).
 * Der Port behält dieses Layout bei: g_ds ist ein 64-KB-Block, der beim Start mit den
 * initialisierten Daten aus Gamefiles/MTV.EXE gefüllt wird (Tabellen, Texte, Startwerte).
 * Spielstände sind Abzüge dieses Blocks, daher bleiben sie mit dem Original kompatibel.
 *
 * Far-Pointer im Datensegment belegen 4 Byte; der Port wird deshalb als 32-Bit-Programm
 * gebaut, sodass dort native Zeiger gespeichert werden können.
 *
 * Globale Variablen werden über benannte Makros in globals.h angesprochen; die DS_*-Makros
 * hier sind die Grundlage dafür und für noch unbenannte Variablen. */
#ifndef MADTV_DGROUP_H
#define MADTV_DGROUP_H

#include <stdint.h>

_Static_assert(sizeof(void *) == 4, "MadTV-Port muss als 32-Bit-Programm gebaut werden");

enum { DGROUP_SIZE = 0x10000 };

extern uint8_t g_ds[DGROUP_SIZE];

#define DS_U8(o)  (*(uint8_t *)(g_ds + (o)))
#define DS_I8(o)  (*(int8_t *)(g_ds + (o)))
#define DS_I16(o) (*(int16_t *)(g_ds + (o)))
#define DS_U16(o) (*(uint16_t *)(g_ds + (o)))
#define DS_I32(o) (*(int32_t *)(g_ds + (o)))
#define DS_U32(o) (*(uint32_t *)(g_ds + (o)))
/* Far-Pointer-Variable an Offset o, z.B. DS_PTR(Bitmap, 0x8e62) */
#define DS_PTR(T, o) (*(T **)(g_ds + (o)))
/* Adresse einer globalen Variable bzw. eines Arrays (entspricht DS:o) */
#define DS_ADDR(T, o) ((T *)(g_ds + (o)))
/* Offset eines Zeigers in g_ds (Umkehrung von DS_ADDR), -1 wenn außerhalb */
int32_t ds_offset_of(const void *p);

/* Lädt die initialisierten Daten aus MTV.EXE im angegebenen Ordner. Rückgabe 0 bei Erfolg. */
int dgroup_load(const char *game_dir);

#endif
