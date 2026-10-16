/* Das Datensegment (DGROUP) des Originalspiels.
 *
 * MTV.EXE adressiert alle globalen Variablen über feste Offsets in DS (Segment 45d4).
 * Der Port behält dieses Layout bei: g_ds ist ein 64-KB-Block, in den dgroup_init beim Start die
 * vorbelegten Daten des Originals schreibt (Tabellen, Texte, Startwerte; src/game/ds_init.c).
 * Spielstände sind Abzüge dieses Blocks, daher bleiben sie mit dem Original kompatibel.
 *
 * Far-Pointer im Datensegment und in den Datenblöcken des Spiels belegen wie im Original 4 Byte.
 * Der Port legt dort native Adressen ab (keine Segment:Offset-Werte). Unter 64 Bit (Linux) liegt
 * aller Speicher, dessen Adresse so abgelegt wird, unterhalb von 4 GB: das Programm wird ohne PIE
 * gelinkt (Code und statische Daten liegen unten), und malloc verwendet nur den Heap direkt hinter
 * dem Programm (memory_init in main.c). P32 wandelt einen Zeiger in den 4-Byte-Wert und bricht ab,
 * falls er nicht hineinpasst; PTR32 wandelt zurück. Unter 32 Bit (Windows) sind beide reine
 * Typumwandlungen.
 *
 * Globale Variablen werden über benannte Makros in globals.h angesprochen; die DS_*-Makros
 * hier sind die Grundlage dafür und für noch unbenannte Variablen. */
#ifndef MADTV_DGROUP_H
#define MADTV_DGROUP_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t ptr32; /* Far-Pointer, wie er im Datensegment steht */

void ptr32_fail(uintptr_t addr);

static inline ptr32 P32(const void *p)
{
    uintptr_t a = (uintptr_t)p;
#if UINTPTR_MAX > 0xffffffffu
    if (a > 0xffffffffu)
        ptr32_fail(a);
#endif
    return (ptr32)a;
}

#define PTR32(T, v) ((T *)(uintptr_t)(ptr32)(v))
/* Far-Pointer an Adresse a (z. B. in einem Datenblock des Spiels) lesen bzw. setzen */
#define MEM_PTR(T, a) PTR32(T, *(const ptr32 *)(a))
#define MEM_SET_PTR(a, p) (*(ptr32 *)(a) = P32(p))

enum { DGROUP_SIZE = 0x10000 };

extern uint8_t g_ds[DGROUP_SIZE];

#define DS_U8(o)  (*(uint8_t *)(g_ds + (o)))
#define DS_I8(o)  (*(int8_t *)(g_ds + (o)))
#define DS_I16(o) (*(int16_t *)(g_ds + (o)))
#define DS_U16(o) (*(uint16_t *)(g_ds + (o)))
#define DS_I32(o) (*(int32_t *)(g_ds + (o)))
#define DS_U32(o) (*(uint32_t *)(g_ds + (o)))
/* Far-Pointer-Variable an Offset o lesen bzw. setzen, z.B. DS_PTR(Bitmap, 0x8e62) */
#define DS_PTR(T, o) PTR32(T, DS_U32(o))
#define DS_SET_PTR(o, p) (DS_U32(o) = P32(p))
/* Adresse einer globalen Variable bzw. eines Arrays (entspricht DS:o) */
#define DS_ADDR(T, o) ((T *)(g_ds + (o)))
/* Offset eines Zeigers in g_ds (Umkehrung von DS_ADDR), -1 wenn außerhalb */
int32_t ds_offset_of(const void *p);

/* Datensegment mit den Startwerten des Spiels füllen, alles übrige 0 */
void dgroup_init(void);
/* Startwerte an ihre Adressen schreiben (src/game/ds_init.c) */
void ds_init_data(void);

#endif
