/* Zugriff auf die NNN.HES-Ressourcendateien von MadTV.
 * Ersetzt hes_load/hes_read/hes_filesize (MTV.EXE 4500/4510/44ce) und
 * hes_unpack_sprites (4444:04ac). */
#ifndef MADTV_HES_H
#define MADTV_HES_H

#include <stddef.h>
#include <stdint.h>

/* Wie Bitmap im Original, aber mit nativer Breite statt 8-Pixel-Spalten-Padding. */
typedef struct {
    int w, h;
    uint8_t *pixels; /* w*h Bytes, zeilenweise, Index 0 = transparent */
} HesSprite;

typedef struct {
    int count;
    HesSprite *sprites;
    uint8_t *storage; /* gemeinsamer Pixelspeicher aller Sprites */
} HesSpriteSet;

/* Ordner mit den Spieldateien setzen (Standard: "Gamefiles"). */
void hes_set_dir(const char *dir);

/* Ganze Datei laden. Rückgabe: malloc-Puffer (Aufrufer gibt frei) oder NULL. */
uint8_t *hes_file_load(int no, size_t *size);

int hes_exists(int no);

/* 1, wenn die Daten eine Huffman-Sprite-Bank sind (Header-Prüfsumme passt). */
int hes_is_sprite_bank(const uint8_t *data, size_t size);

/* Sprite-Bank entpacken. Rückgabe 0 bei Erfolg. */
int hes_decode_sprites(const uint8_t *data, size_t size, HesSpriteSet *out);
void hes_free_sprites(HesSpriteSet *bank);

/* Palettendatei (z.B. 1 oder 299): u16 Anzahl, danach RGB-Tripel mit 6 Bit.
 * Schreibt 256*3 Bytes mit 8 Bit pro Kanal. Rückgabe 0 bei Erfolg. */
int hes_load_palette(int no, uint8_t rgb[768]);

#endif
