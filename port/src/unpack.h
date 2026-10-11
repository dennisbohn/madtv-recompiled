/* Entpacker für LZW-Ressourcen (Musik, Intro) und RLE-Delta-Frames (Intro-Animation). */
#ifndef MADTV_UNPACK_H
#define MADTV_UNPACK_H

#include <stddef.h>
#include <stdint.h>

/* LZW-Ressource: +0 u16le entpackte Länge, +2 u16le gepackte Länge, Daten ab +8.
 * Ersetzt lzw_decode (MTV.EXE 438d:0002). Rückgabe: malloc-Puffer oder NULL. */
uint8_t *lzw_unpack(const uint8_t *src, size_t src_size, size_t *out_size);

/* Ein Delta-Frame auf ein Pixelfeld anwenden (rle_delta_frame, MTV.EXE 5052:0000):
 *   0x62          Frame-Ende
 *   0x63 n.. v    Lauf: n wird über Folgebytes 0xFF aufsummiert; v = 0x64 -> n Pixel überspringen,
 *                 sonst n-mal v schreiben
 *   0x64          1 Pixel überspringen
 *   sonst         Literal-Pixel
 * *stream zeigt danach hinter das 0x62. Rückgabe 0, -1 bei Über-/Unterlauf. */
int rle_delta_frame(const uint8_t **stream, const uint8_t *end, uint8_t *dst, size_t dst_size);

#endif
