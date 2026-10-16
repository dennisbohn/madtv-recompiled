/* Laufzeitbibliothek mit der API des Originals.
 *
 * Ersetzt die Assembler-Hilfsbibliothek von MTV.EXE (Segmente 43c6-4510: VGA, Blitter, Maus,
 * Tastatur, HES-Dateien), die Treiber-Anbindung (Ticks) und die benutzten Teile der Turbo-C-
 * Laufzeitbibliothek. Namen und Semantik entsprechen dem Original (siehe re/notes/library_map.md),
 * damit der übersetzte Spielcode sie 1:1 aufrufen kann.
 *
 * Koordinaten der Blitter: x in 8-Pixel-Spalten ("xcol"), y in Zeilen; die *_shift-Varianten
 * haben zusätzlich einen Pixelversatz 0..7. Farbe 0 ist bei den *_trans-Varianten durchsichtig. */
#ifndef MADTV_RTLIB_H
#define MADTV_RTLIB_H

#include <stddef.h>
#include <stdint.h>

#include "dgroup.h"

#pragma pack(push, 1)

/* 8-Byte-Bitmap-Kopf; die Pixel folgen bei bitmap_alloc direkt dahinter. */
typedef struct Bitmap {
    int16_t wcols; /* Breite in 8-Pixel-Spalten */
    int16_t h;     /* Höhe in Zeilen */
    ptr32 pixels;  /* zeilenweise, Zeilenlänge wcols*8 (BMP_PX) */
} Bitmap;
#define BMP_PX(b) PTR32(uint8_t, (b)->pixels)

/* Ergebnis von hes_unpack_sprites: Anzahl, Bitmap-Köpfe, danach die Pixeldaten. */
typedef struct SpriteBank {
    uint16_t count;
    Bitmap bmp[];
} SpriteBank;

/* Parameterblöcke der Blitter (das Spiel legt sie teils im Datensegment ab); src/dst sind
 * 4-Byte-Zeiger auf Bitmap (P32 beim Füllen, BLIT_BMP beim Lesen). */
typedef struct BlitArgs { ptr32 src; ptr32 dst; int16_t xcol, y; } BlitArgs;                     /* bmp_blit */
typedef struct BlitRectArgs {
    ptr32 src; int16_t sx, sy, w, h; ptr32 dst; int16_t dx, dy;
} BlitRectArgs;                                                                               /* bmp_blit_rect(_trans) */
typedef struct BlitRectShiftArgs {
    ptr32 src; int16_t xpix, sx, sy, w, h; ptr32 dst; int16_t dx, dy;
} BlitRectShiftArgs;                                                                          /* *_rect_shift */
typedef struct BlitShiftArgs { ptr32 src; int16_t xpix; ptr32 dst; int16_t xcol, y; } BlitShiftArgs;
typedef struct ScrRectArgs { ptr32 src; int16_t sx, sy, w, h, dx, dy; } ScrRectArgs;              /* scr_blit_rect */
#define BLIT_BMP(v) PTR32(Bitmap, v)

#pragma pack(pop)

_Static_assert(sizeof(Bitmap) == 8, "Bitmap-Kopf muss 8 Byte haben");
_Static_assert(sizeof(BlitRectShiftArgs) == 0x16, "Layout BlitRectShiftArgs");

/* ---- Video (Bildschirm = 320x200 Mode-13h-Ersatz) ---- */
int16_t vga_detect(void);
void vga_set_mode13(void);
void vga_restore_mode(void);
void vga_init_segs(void);
void vga_wait_vsync(void);
/* pal[0] = Anzahl Farben, danach RGB-Tripel mit 6 Bit (Format von 001.HES/299.HES) */
void vga_set_palette(const void *pal);

/* ---- Bitmaps und Blitter ---- */
Bitmap *bitmap_alloc(int16_t w_px, int16_t h);
uint16_t spritebank_size(const SpriteBank *b);
void scr_grab(Bitmap *dst, int16_t xcol, int16_t y);
void scr_blit(const Bitmap *src, int16_t xcol, int16_t y);
void bmp_fill(Bitmap *dst, int16_t xcol, int16_t y, int16_t wcols, int16_t h, uint8_t color);
void bmp_blit(const BlitArgs *a);
void bmp_blit_rect(const BlitRectArgs *a);
void bmp_blit_rect_shift(const BlitRectShiftArgs *a);
void bmp_glyph_trans(const BlitShiftArgs *a);
void bmp_blit_rect_trans(const BlitRectArgs *a);
void bmp_blit_trans_shift(const BlitShiftArgs *a);
void bmp_blit_rect_trans_shift(const BlitRectShiftArgs *a);
void scr_blit_rect(const ScrRectArgs *a);
/* PORT: pixelgenaues Rechteck aus src auf den Bildschirm */
void scr_blit_px(const Bitmap *src, int sx, int sy, int w, int h, int dx, int dy);

/* ---- Maus (relativ: mouse_poll legt die Bewegung in DS:16a2/16a4 ab) ---- */
int16_t mouse_init(void);
void mouse_setup(void);
int16_t mouse_poll(void); /* Rückgabe: Tasten, Bit0 links, Bit1 rechts */

/* ---- Tastatur ---- */
int16_t kbd_init(void);
void kbd_flush(void);
int16_t kbd_getkey(void); /* Spiel-Tastencode oder -1 */
void kbd_shutdown(void);

/* ---- Zeit (Tick-Zähler DS:9014, im Original vom Soundtreiber-Interrupt erhöht) ---- */
int16_t timer_init(void);
void timer_shutdown(void);
void ticks_reset(void);
uint16_t ticks_get(void);
/* PORT: Spieltakte in 1/256 Takt (nur Differenzen nutzen) */
uint32_t ticks_fine(void);
/* PORT: Anzahl angezeigter Bilder */
uint32_t display_frame_count(void);
/* PORT: Anzeige während eines Spielschritts anhalten (1) bzw. freigeben (0) */
void display_hold(int on);
/* PORT: Zeitpunkt des nächsten angezeigten Bilds und aktuelle Zeit (µs), Dauer von n Takten (µs) */
uint64_t display_next_frame_us(void);
/* PORT: Bereich des nächsten Bilds um frac256/256 Spielpixel nach unten versetzt darstellen;
 * row_above = Zeile (w Palettenindizes), die oben nachrückt */
void display_subpixel_shift(int x, int y, int w, int h, int frac256, const uint8_t *row_above);
/* PORT: eigene Ebene x/y/w/h, um frac256/256 Pixel nach unten versetzt; bmp enthält h+1 Zeilen,
 * beginnend mit der Zeile über dem Bereich; transparent: Farbe 0 frei */
void display_layer(int x, int y, int w, int h, int frac256, const Bitmap *bmp, int transparent);
/* PORT: Ebenen/Feinverschiebungen neu beginnen; nach dem Zeichnen ins Grundbild abschließen */
void display_layers_begin(void);
void display_layers_commit(void);
/* PORT: unverschobene Ebene mit Deckungsmaske (px/mask: h+1 Zeilen ab der Zeile über dem Bereich) */
void display_overlay(int x, int y, int w, int h, const uint8_t *px, const uint8_t *mask, int stride);
uint64_t time_us(void);
uint32_t ticks_to_us(uint32_t n);
uint16_t ticks_peek(void);
void ticks_set(uint16_t n);

/* ---- NNN.HES-Dateien ---- */
/* PORT: beide Archive SOURCE1/SOURCE2.RSC vorhanden (eingebettet oder im Spielordner)? */
int rsc_available(void);
int32_t hes_filesize(int16_t no);
int32_t hes_load(int16_t no, void *buf);
int32_t hes_read(int16_t no, void *buf, uint16_t ofs, uint16_t len);
int32_t hes_write(int16_t no, const void *buf, uint16_t len);
int32_t hes_write_dgroup(int16_t no, uint16_t len);
void hes_sprite_info(int16_t no, int16_t *filesize, int16_t *unpacked_size, int16_t mode);
int16_t hes_list(int16_t *out);
void hes_unpack_sprites(const uint8_t *src, SpriteBank *dst);
void lzw_decode(uint8_t *dst, const uint8_t *src, void *work);
/* Delta-Frame auf eine Bitmap mit direkt folgenden Pixeln anwenden (Original 5052:0000). */
void rle_delta_frame_bmp(uint8_t **stream, Bitmap *dst);
int16_t dos_get_drive(int16_t *out);
int16_t dos_set_drive(int16_t drive);

/* ---- Turbo-C-Laufzeit, wo sich das Verhalten von der C-Bibliothek unterscheidet ---- */
void tc_srand(uint16_t seed);
int16_t tc_rand(void);
void *tc_malloc(uint16_t n);
void *tc_farmalloc(uint32_t n);
void *tc_calloc(uint16_t n, uint16_t size);
void *tc_farcalloc(uint32_t n, uint32_t size);
void tc_free(void *p);
char *tc_itoa(int16_t v, char *buf, int16_t radix);
char *tc_ltoa(int32_t v, char *buf, int16_t radix);
char *tc_ultoa(uint32_t v, char *buf, int16_t radix);
int16_t tc_atoi(const char *s);
int16_t tc_atexit(void (*fn)(void));
void tc_exit(int16_t code);

#endif
