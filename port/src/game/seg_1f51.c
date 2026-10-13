/* Segment 1f51 – Grafik- und Oberflächenkern: Mauszeiger, Text, Rahmen, Fenster, Blit-Helfer,
 * Clipping, Aufzug-Warteschlange, Gebäude-/Raumgrafik, Eingabe und Fernsehbild.
 *
 * Sprites werden meist als "Bank + Byte-Offset" adressiert: sprite_ofs(n) = n*8-6 ist der
 * Offset des n-ten (1-basierten) Bitmap-Kopfs in einer SpriteBank. */
#include <string.h>

#include <stdlib.h>

#include "game.h"
#include "platform.h"

#define g_cursor_visible DS_I16(0x8aac)
#define g_cursor_shape   DS_I16(0x79b2)
#define g_cursor_old_x   DS_I16(0x7b06)
#define g_cursor_old_y   DS_I16(0x7b16)
#define g_cursor_bitmaps DS_ADDR(Bitmap *, 0x8e74)
#define g_cursor_grab    DS_PTR(Bitmap, 0x8e70)
#define g_cursor_bg_old  DS_PTR(Bitmap, 0x8ebe)
#define g_drag_bitmap    DS_PTR(Bitmap, 0x0390)
#define g_drag_w         DS_I16(0x0394)
#define g_drag_h         DS_I16(0x0396)
#define g_drag_hot_x     DS_I16(0x0398)
#define g_drag_hot_y     DS_I16(0x039a)
#define g_upper_screen   DS_PTR(Bitmap, 0x8e62)

/* aktuelle Zeichenquelle/-ziel für bank_draw* */
#define g_draw_bank      DS_PTR(uint8_t, 0x8f2e) /* SpriteBank als Bytezeiger (+ sprite_ofs) */
#define g_draw_target    DS_PTR(Bitmap, 0x8f2a)
#define g_draw_fixed_x   DS_I16(0x8f24)
#define g_draw_fixed_y   DS_I16(0x8f26)
#define g_draw_cur_ofs   DS_I16(0x8f28)

/* Rahmen */
#define g_frame_dst      DS_PTR(Bitmap, 0x8f32)
#define g_frame_tiles    DS_PTR(uint8_t, 0x8f36) /* erste Kachel; weitere in 8-Byte-Schritten */
#define g_frame_args     DS_ADDR(BlitShiftArgs, 0x8f16)

/* Fenster-Slots */
#define g_win_bitmap     DS_ADDR(Bitmap *, 0x8ec2)
#define g_win_x          DS_ADDR(int16_t, 0x7b08)
#define g_win_y          DS_ADDR(int16_t, 0x7b18)
#define g_win_w          DS_ADDR(int16_t, 0x8c1a)
#define g_win_h          DS_ADDR(int16_t, 0x8c24)
#define g_win_used       DS_ADDR(int16_t, 0x8c40)

/* vertikales Clipping der Blit-Helfer */
#define g_clip_on        DS_I16(0x0386)
#define g_clip_top       DS_I16(0x038a)
#define g_clip_bottom    DS_I16(0x038e)

#define BANK_BMP(bank, ofs) ((Bitmap *)((uint8_t *)(bank) + (ofs)))
static void tv_shutter_note(Bitmap *open); /* PORT, siehe tv_picture_update */

/* 1f51:0003 – Mauszeiger einblenden (Zähler: 1 = Zeiger sichtbar, 0 mit gezogenem Objekt =
 * Objekt am Zeiger sichtbar). PORT: Das Original sicherte hier den Hintergrund und zeichnete
 * Zeiger bzw. Objekt in den Bildschirmspeicher; das übernimmt im Port die Darstellungsschicht
 * (Zeiger-Overlay, rtlib.c), die den Zähler auswertet. */
void mouse_show(void)
{
    g_cursor_visible++;
}

/* 1f51:019e – Mauszeiger ausblenden (Zähler). PORT: ohne Zurückschreiben des Hintergrunds, siehe
 * mouse_show. */
void mouse_hide(void)
{
    g_cursor_visible--;
    DS_I32(0x7aa2) = g_cursor_visible;
}

/* 1f51:01f3 – value um ein Fünftel von delta (mindestens 1) bewegen, delta entsprechend verringern */
void ease_step(int16_t *delta, int16_t *value)
{
    int16_t step;
    if (*delta >= 1)
        step = *delta < 5 ? 1 : (int16_t)(*delta / 5);
    else if (*delta < 0)
        step = *delta < -4 ? (int16_t)(*delta / 5) : -1;
    else
        step = 0;
    *value = (int16_t)(*value + step);
    *delta = (int16_t)(*delta - step);
}

/* 1f51:0250 – Text mit 8-Pixel-Glyphen zeichnen; fixed_digits = 1: Ziffern auf 5 Pixel zentriert */
void draw_string(int16_t x, int16_t y, const char *str, Bitmap *dst, int16_t fixed_digits)
{
    int16_t col = (int16_t)(x >> 3);
    int16_t xpix = (int16_t)(x % 8);
    for (int16_t i = 0; str[i] != '\0'; i++) {
        uint16_t ch = (uint8_t)str[i];
        if (ch == ' ') {
            xpix += 3;
        } else {
            if (DS_U8(0x12e + ch) == 0)
                ch = '.'; /* Zeichen ohne Glyphe */
            uint8_t width = DS_U8(0x22e + ch);
            if (fixed_digits == 1 && ch >= '0' && ch <= '9' && width < 5) {
                xpix = (int16_t)(xpix + (5 - width));
                if (xpix > 8) {
                    col++;
                    xpix -= 8;
                }
            }
            uint8_t *font = DS_I16(0x8b86) == 0 ? (uint8_t *)DS_PTR(SpriteBank, 0x8366) : DS_PTR(uint8_t, 0x0108);
            BlitShiftArgs a = {BANK_BMP(font, DS_U8(0x12e + ch) * 8 - 6), xpix, dst, col, y};
            bmp_glyph_trans(&a);
            if (fixed_digits == 0 || ch < '0' || ch > '9' || width < 5)
                xpix = (int16_t)(xpix + width + 1);
            else
                xpix += 5;
        }
        if (xpix > 8) {
            col++;
            xpix -= 8;
        }
    }
}

/* 1f51:036f – Rahmen mit Koordinaten in 8-Pixel-Zellen */
void draw_frame_cells(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Bitmap *dst, int16_t style)
{
    draw_frame((int16_t)(x0 << 3), (int16_t)(y0 << 3), (int16_t)(x1 << 3), (int16_t)(y1 << 3), dst, style);
}

/* 1f51:03c1 – gefülltes Rechteck mit Rand aus 8x8-Kacheln (Stil 1 oder 2) */
void draw_frame(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Bitmap *dst, int16_t style)
{
    g_frame_dst = dst;
    g_frame_tiles = style == 1 ? DS_PTR(uint8_t, 0x7aea) : DS_PTR(uint8_t, 0x7aee);
    for (int16_t x = (int16_t)(x0 + 8); x < x1; x += 8)
        for (int16_t y = (int16_t)(y0 + 8); y < y1; y += 8)
            frame_piece_draw(0, x, y); /* Füllung */
    for (int16_t x = (int16_t)(x0 + 8); x < x1; x += 8) {
        frame_piece_draw(0x30, x, y0); /* oben */
        frame_piece_draw(0x28, x, y1); /* unten */
    }
    for (int16_t y = (int16_t)(y0 + 8); y < y1; y += 8) {
        frame_piece_draw(0x40, x0, y); /* links */
        frame_piece_draw(0x38, x1, y); /* rechts */
    }
    frame_piece_draw(8, x0, y0);
    frame_piece_draw(0x10, x1, y0);
    frame_piece_draw(0x18, x1, y1);
    frame_piece_draw(0x20, x0, y1);
}

/* 1f51:04cb – eine Rahmenkachel transparent zeichnen (Parameterblock im Datensegment) */
void frame_piece_draw(int16_t piece_ofs, int16_t x, int16_t y)
{
    g_frame_args->src = BANK_BMP(g_frame_tiles, piece_ofs);
    g_frame_args->xpix = (int16_t)(x % 8);
    g_frame_args->dst = g_frame_dst;
    g_frame_args->xcol = (int16_t)(x / 8);
    g_frame_args->y = y;
    bmp_glyph_trans(g_frame_args);
}

/* 1f51:0516 – Rahmen mit umbrochenem Text in die Spielszene zeichnen (nur wenn visible == 1);
 * gibt text frei */
void text_window_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1, char *text, int16_t visible)
{
    if (visible == 1) {
        draw_frame_cells(x0, y0, x1, y1, g_upper_screen, 1);
        text_box_to_free((int16_t)(x0 * 8 + 4), (int16_t)(y0 * 8 - 4), text, (int16_t)(x1 - x0), 1, g_upper_screen);
    }
}

/* 1f51:057c – Fenster öffnen: Hintergrund in einem Slot sichern (mode 1/3 freier Slot ab 1,
 * mode 2 Slot 0); mode 1 zeichnet zusätzlich Rahmen und Text. Rückgabe: Slot */
int16_t window_open(int16_t x0, int16_t y0, int16_t x1, int16_t y1, char *text, int16_t mode)
{
    int16_t slot = 1;
    if (mode == 1 || mode == 3) {
        while (g_win_bitmap[slot] != NULL)
            slot++;
        DS_I32(0x7aae) = slot;
    } else if (mode == 2) {
        slot = 0;
    }
    g_win_bitmap[slot] = bitmap_alloc((int16_t)((x1 - x0 + 1) * 8), (int16_t)((y1 - y0 + 1) * 8));
    if (mode == 1 || mode == 3) {
        g_win_x[slot] = x0;
        g_win_y[slot] = y0;
        g_win_w[slot] = (int16_t)(x1 - x0 + 1);
        g_win_h[slot] = (int16_t)(y1 - y0 + 1);
        g_win_used[slot] = 1;
        screen_grab(g_win_bitmap[slot], (int16_t)(x0 << 3), (int16_t)(y0 << 3));
        if (mode == 1) {
            text_window_draw(x0, y0, x1, y1, text, 1);
            screen_update();
        }
    }
    return slot;
}

/* 1f51:0686 – Tooltip-Kasten mit Text direkt auf den Bildschirm */
void popup_box_show(int16_t x0, int16_t y0, int16_t x1, int16_t y1, char *text)
{
    popup_box_create(x0, y0, x1, y1);
    text_box_to_free(4, -4, text, (int16_t)(x1 - x0), 1, g_win_bitmap[0]);
    popup_box_present(x0, y0);
}

/* 1f51:06d5 – Fenster schließen: Hintergrund zurück, anzeigen, Slot freigeben */
void window_close(int16_t slot)
{
    if (g_win_used[slot] == 1 && g_win_bitmap[slot] != NULL) {
        bmp_draw(g_win_bitmap[slot], g_upper_screen, (int16_t)(g_win_x[slot] << 3), (int16_t)(g_win_y[slot] << 3));
        screen_update();
        far_free(g_win_bitmap[slot]);
        g_win_bitmap[slot] = NULL;
    }
}

/* 1f51:0762 – Hintergrund eines Fensters wiederherstellen, ohne es zu schließen */
void window_restore(int16_t slot)
{
    if (g_win_used[slot] == 1 && g_win_bitmap[slot] != NULL)
        bmp_draw(g_win_bitmap[slot], g_upper_screen, (int16_t)(g_win_x[slot] << 3), (int16_t)(g_win_y[slot] << 3));
}

/* 1f51:07c3 – Rahmen um die Raumansicht (Kacheln aus Bank 4) */
void room_frame_draw(void)
{
    g_clip_on = 0;
    g_draw_bank = (uint8_t *)DS_PTR(SpriteBank, 0x8b7c);
    g_draw_target = g_upper_screen;
    for (int16_t x = 8; x < 0x131; x += 8) {
        bank_draw_opaque(0x52, x, 0);
        bank_draw_opaque(10, x, 0x72);
    }
    for (int16_t y = 8; y < 0x6e; y += 6) {
        bank_draw_opaque(0x1a, 0, y);
        bank_draw_opaque(0x32, 0x138, y);
    }
    bank_draw_opaque(0x4a, 0, 0);
    bank_draw_opaque(0x5a, 0x138, 0);
    bank_draw_opaque(2, 0, 0x72);
    bank_draw_opaque(0x12, 0x138, 0x72);
    bank_draw_opaque(0x22, 0, 4);
    bank_draw_opaque(0x2a, 0, 0x6e);
    bank_draw_opaque(0x3a, 0x138, 4);
    bank_draw_opaque(0x42, 0x138, 0x6e);
}

/* 1f51:08d9 – Variante des Raumrahmens (Bank DS:0104) auf gelöschtem Hintergrund */
void room_frame_draw_alt(void)
{
    bmp_fill(g_upper_screen, 0, 0, 0x28, 0x78, 0);
    g_draw_bank = DS_PTR(uint8_t, 0x0104);
    g_draw_target = g_upper_screen;
    for (int16_t i = 1; i < 0x27; i++) {
        bank_draw_opaque(0x52, (int16_t)(i << 3), 0);
        bank_draw_opaque(10, (int16_t)(i << 3), 0x75);
    }
    for (int16_t y = 8; y < 0x6e; y += 4) {
        bank_draw_opaque(0x1a, 0, y);
        bank_draw_opaque(0x32, 0x138, y);
    }
    bank_draw_opaque(0x4a, 0, 0);
    bank_draw_opaque(0x5a, 0x138, 0);
    bank_draw_opaque(2, 0, 0x75);
    bank_draw_opaque(0x12, 0x138, 0x75);
    bank_draw_opaque(0x22, 0, 4);
    bank_draw_opaque(0x2a, 0, 0x71);
    bank_draw_opaque(0x3a, 0x138, 4);
    bank_draw_opaque(0x42, 0x138, 0x71);
}

/* 1f51:0a15 – Byte-Offset des 1-basierten Sprites idx in einer SpriteBank */
int16_t sprite_ofs(int16_t idx)
{
    return (int16_t)(idx * 8 - 6);
}

/* 1f51:0a26 – Offset des Animationsbilds frame der Figur fig in der Figurenbank */
int16_t figure_sprite_ofs(int16_t fig, int16_t frame)
{
    return (int16_t)((DS_ADDR(int16_t, 0x7b2c)[fig * 15] * 10 + frame) * 8 - 6);
}

/* 1f51:0a50 – Bild in das Zieh-Objekt am Mauszeiger kopieren */
void drag_image_draw(Bitmap *src)
{
    blit_sprite_masked(src, g_drag_bitmap, 0, 0, 0, 0, (int16_t)(src->wcols << 3), src->h);
}

/* 1f51:0a7f */
void bmp_draw_masked(Bitmap *src, Bitmap *dst, int16_t x, int16_t y)
{
    blit_sprite_masked(src, dst, x, y, 0, 0, (int16_t)(src->wcols << 3), src->h);
}

/* 1f51:0ab0 */
void bmp_draw(Bitmap *src, Bitmap *dst, int16_t x, int16_t y)
{
    blit_rect(src, dst, x, y, 0, 0, (int16_t)(src->wcols << 3), src->h);
}

/* 1f51:0ae1 – deckend in die Spielszene */
void draw_bitmap(Bitmap *src, int16_t x, int16_t y)
{
    blit_rect(src, g_upper_screen, x, y, 0, 0, (int16_t)(src->wcols << 3), src->h);
}

/* 1f51:0b14 – Ausschnitt der Spielszene in dst kopieren */
void screen_grab(Bitmap *dst, int16_t x, int16_t y)
{
    blit_rect(g_upper_screen, dst, 0, 0, x, y, (int16_t)(dst->wcols << 3), dst->h);
}

/* 1f51:0b47 */
void bank_draw_opaque(int16_t ofs, int16_t x, int16_t y)
{
    Bitmap *b = BANK_BMP(g_draw_bank, ofs);
    blit_rect(b, g_draw_target, x, y, 0, 0, (int16_t)(b->wcols << 3), b->h);
}

/* 1f51:0b8e */
void bank_draw(int16_t ofs, int16_t x, int16_t y)
{
    Bitmap *b = BANK_BMP(g_draw_bank, ofs);
    blit_sprite_masked(b, g_draw_target, x, y, 0, 0, (int16_t)(b->wcols << 3), b->h);
}

/* 1f51:0bd5 */
void target_draw_sprite(Bitmap *src, int16_t x, int16_t y)
{
    blit_sprite_masked(src, g_draw_target, x, y, 0, 0, (int16_t)(src->wcols << 3), src->h);
}

/* 1f51:0c08 */
void bank_draw_fixed_y(int16_t ofs, int16_t x)
{
    Bitmap *b = BANK_BMP(g_draw_bank, ofs);
    blit_sprite_masked(b, g_draw_target, x, g_draw_fixed_y, 0, 0, (int16_t)(b->wcols << 3), b->h);
}

/* 1f51:0c50 */
void bank_draw_fixed_x(int16_t ofs, int16_t y)
{
    Bitmap *b = BANK_BMP(g_draw_bank, ofs);
    blit_sprite_masked(b, g_draw_target, g_draw_fixed_x, y, 0, 0, (int16_t)(b->wcols << 3), b->h);
}

/* 1f51:0c98 */
void bank_draw_current(int16_t x, int16_t y)
{
    Bitmap *b = BANK_BMP(g_draw_bank, g_draw_cur_ofs);
    blit_sprite_masked(b, g_draw_target, x, y, 0, 0, (int16_t)(b->wcols << 3), b->h);
}

/* 1f51:0ce1 – transparent in die Spielszene */
void draw_sprite(Bitmap *src, int16_t x, int16_t y)
{
    blit_sprite_masked(src, g_upper_screen, x, y, 0, 0, (int16_t)(src->wcols << 3), src->h);
}

/* 1f51:0d14 – ganze Bitmap deckend in die Spielszene, x auf 8 Pixel abgerundet */
void draw_bitmap_fast(Bitmap *src, int16_t x, int16_t y)
{
    BlitArgs a = {src, g_upper_screen, (int16_t)(x >> 3), y};
    bmp_blit(&a);
}

/* vertikales Clipping wie in 1f51:0d4f/0ec4 */
static void clip_vertical(int16_t *dy, int16_t *sy, int16_t *h)
{
    if (g_clip_on != 1)
        return;
    if (*dy < g_clip_top) {
        *sy = (int16_t)(*sy + (g_clip_top - *dy));
        *h = (int16_t)(*h - (g_clip_top - *dy));
        *dy = g_clip_top;
    } else if (g_clip_bottom < *dy + *h) {
        *h = (int16_t)(g_clip_bottom - *dy);
    }
    if (*sy < 0) {
        *dy = (int16_t)(*dy - *sy);
        *h = (int16_t)(*h + *sy);
        *sy = 0;
    }
}

/* 1f51:0d4f – Ausschnitt transparent blitten; dx/sx/w in Pixeln (w auf Spalten aufgerundet) */
void blit_sprite_masked(Bitmap *src, Bitmap *dst, int16_t dx, int16_t dy, int16_t sx, int16_t sy, int16_t w, int16_t h)
{
    clip_vertical(&dy, &sy, &h);
    if (h <= 0 || w <= 0)
        return;
    if ((dx & 7) == 0) {
        BlitRectArgs a = {src, (int16_t)(sx >> 3), sy, (int16_t)((w + 7) >> 3), h, dst, (int16_t)(dx >> 3), dy};
        bmp_blit_rect_trans(&a);
    } else {
        BlitRectShiftArgs a = {src, (int16_t)(dx & 7), (int16_t)(sx >> 3), sy, (int16_t)((w + 7) >> 3), h,
                               dst, (int16_t)(dx >> 3), dy};
        bmp_blit_rect_trans_shift(&a);
    }
}

/* 1f51:0e6f – ganze Bitmap deckend nach dst, x auf 8 Pixel abgerundet */
void bmp_copy_at(Bitmap *src, Bitmap *dst, int16_t x, int16_t y)
{
    BlitArgs a = {src, dst, (int16_t)(x >> 3), y};
    bmp_blit(&a);
}

/* 1f51:0ea9 */
void bmp_copy(Bitmap *src, Bitmap *dst)
{
    bmp_copy_at(src, dst, 0, 0);
}

/* 1f51:0ec4 – Ausschnitt deckend blitten; dx/sx/w in Pixeln (w auf Spalten abgerundet) */
void blit_rect(Bitmap *src, Bitmap *dst, int16_t dx, int16_t dy, int16_t sx, int16_t sy, int16_t w, int16_t h)
{
    clip_vertical(&dy, &sy, &h);
    if (h <= 0 || w <= 0)
        return;
    if ((dx & 7) == 0) {
        BlitRectArgs a = {src, (int16_t)(sx >> 3), sy, (int16_t)(w >> 3), h, dst, (int16_t)(dx >> 3), dy};
        bmp_blit_rect(&a);
    } else {
        BlitRectShiftArgs a = {src, (int16_t)(dx & 7), (int16_t)(sx >> 3), sy, (int16_t)(w >> 3), h,
                               dst, (int16_t)(dx >> 3), dy};
        bmp_blit_rect_shift(&a);
    }
}

/* 1f51:1628 – Eingabe abfragen: Maus, Enter/Esc als Klick, Pfeiltasten bewegen den Zeiger
 * mit Beschleunigung, übrige Tasten an hotkey_handle. Rückgabe: Klick (1 links, 2 rechts) */
int16_t input_poll(void)
{
    mouse_update();
    int16_t key = kbd_getkey();
    if (key != -1 && DS_I16(0x7a2c) == 0) {
        if (key == 0x1f)
            DS_I16(0x7a2c) = 1;
        else if (key == 0x14)
            DS_I16(0x7a2c) = 2;
    }
    if (key < 0x15 || key > 0x1c) {
        hotkey_handle(key);
    } else {
        int16_t dir = (int16_t)(key - (key < 0x19 ? 0x14 : 0x18));
        if (dir == DS_I16(0x0d36)) {
            if (DS_I16(0x0d38) < 0x10)
                DS_I16(0x0d38) += 2;
        } else {
            DS_I16(0x0d38) = 2;
            DS_I16(0x0d36) = dir;
        }
        switch (DS_I16(0x0d36)) {
        case 1: DS_I16(0x7a2a) -= DS_I16(0x0d38); break; /* hoch */
        case 2: DS_I16(0x7a2a) += DS_I16(0x0d38); break; /* runter */
        case 3: DS_I16(0x7a28) -= DS_I16(0x0d38); break; /* links */
        case 4: DS_I16(0x7a28) += DS_I16(0x0d38); break; /* rechts */
        default: break;
        }
    }
    return DS_I16(0x7a2c);
}

/* 1f51:16dd – Mausbewegung aufsummieren und Tastendruck merken */
void mouse_update(void)
{
    int16_t buttons = mouse_poll();
    DS_I16(0x7a28) += g_mouse_dx;
    DS_I16(0x7a2a) += g_mouse_dy;
    if (buttons != 0 && buttons != DS_I16(0x7a2c))
        DS_I16(0x7a2c) = buttons;
}

/* 1f51:0fd8 – Etage in die Aufzug-Warteschlange einreihen (falls noch nicht drin) */
void elevator_request_add(int16_t floor)
{
    int16_t *queue = DS_ADDR(int16_t, 0x8c4c);
    int16_t i;
    for (i = 0; queue[i] != floor && i < DS_I16(0x8c34); i++)
        ;
    if (i >= DS_I16(0x8c34)) {
        queue[DS_I16(0x8c34)] = floor;
        DS_I16(0x8c34)++;
    }
}

/* 1f51:100e – Etage aus der Aufzug-Warteschlange entfernen */
void elevator_request_remove(int16_t floor)
{
    int16_t *queue = DS_ADDR(int16_t, 0x8c4c);
    int16_t i;
    for (i = 0; queue[i] != floor && i < DS_I16(0x8c34); i++)
        ;
    if (i < DS_I16(0x8c34)) {
        /* nachrücken; das Original kopiert dabei auch das Element hinter dem Ende */
        for (;;) {
            queue[i] = queue[i + 1];
            if (!(i < DS_I16(0x8c34)))
                break;
            i++;
        }
        DS_I16(0x8c34)--;
    }
}

/* Türtyp (1..3 = begehbar) von Tür slot auf Etage floor */
#define DOOR_TYPE(floor, slot) DS_U8(0x7c76 + (floor) * 0x7a + (slot))

/* 1f51:105c – *slot rückwärts (mit Umlauf auf 3) bis zur nächsten begehbaren Tür ungleich exclude */
void floor_door_prev(int16_t floor, int16_t *slot, int16_t exclude)
{
    for (;;) {
        uint8_t t = DOOR_TYPE(floor, *slot);
        if (t != 0 && t < 4 && (exclude == -1 || *slot != exclude))
            return;
        (*slot)--;
        if (*slot < 0)
            *slot = 3;
    }
}

/* 1f51:10cf – Anzahl begehbarer Türen am Anfang der Türliste einer Etage */
int16_t floor_door_count(int16_t floor)
{
    int16_t n = 0;
    while (DOOR_TYPE(floor, n) != 0 && DOOR_TYPE(floor, n) < 4)
        n++;
    return n;
}

static void clip_set(int16_t right, int16_t bottom)
{
    g_clip_on = 1;
    DS_I16(0x0388) = 0;
    g_clip_top = 0;
    DS_I16(0x038c) = right;
    g_clip_bottom = bottom;
}

/* 1f51:110f – Clipping auf die Größe des Mauszeigers */
void clip_set_small(void)
{
    clip_set(0x18, 0xb);
}

/* 1f51:112e – Clipping auf die Spielszene (320x120) */
void clip_reset(void)
{
    clip_set(0x140, 0x78);
}

/* 1f51:114d – Clipping auf die oberen 80 Zeilen */
void clip_set_upper(void)
{
    clip_reset();
    g_clip_bottom = 0x50;
}

/* 1f51:1158 – Mauszeiger-x für Treffertests (Zeigerform 2 bzw. gezogenes Objekt berücksichtigt) */
int16_t mouse_x(void)
{
    if (g_cursor_shape == 2 && g_drag_bitmap == NULL)
        return (int16_t)(g_cursor_x + 5);
    if (g_drag_bitmap != NULL)
        return (int16_t)(g_cursor_x - g_drag_hot_x);
    return g_cursor_x;
}

/* 1f51:1186 */
int16_t mouse_y(void)
{
    if (g_cursor_shape == 2 && g_drag_bitmap == NULL)
        return (int16_t)(g_cursor_y + 5);
    if (g_drag_bitmap != NULL)
        return (int16_t)(g_cursor_y - g_drag_hot_y);
    return g_cursor_y;
}

/* 1f51:11ef – Seitenwände und Schachtsäulen des Gebäudes (Bank 13) zeichnen */
void building_walls_draw(int16_t variant)
{
    SpriteBank *bank = DS_PTR(SpriteBank, 0x8362);
    int16_t middle, left, right;
    clip_reset();
    if (variant == 1) {
        middle = 10;
        left = 0xd;
        right = 0xc;
    } else {
        middle = 0xb;
        left = 0xf;
        right = 0xe;
    }
    for (int16_t y = 4; y < 0x71; y++) {
        draw_sprite(BANK_BMP(bank, sprite_ofs(left)), 0, y);
        draw_sprite(BANK_BMP(bank, sprite_ofs(right)), 0x138, y);
        for (int16_t x = 8; x < 0x38; x += 8)
            draw_sprite(BANK_BMP(bank, sprite_ofs(middle)), x, y);
        for (int16_t x = 0x108; x < 0x138; x += 8)
            draw_sprite(BANK_BMP(bank, sprite_ofs(middle)), x, y);
    }
}

/* 1f51:12ca – Tür im Gebäudebild (DS:8eba) zeichnen; state 1 = offen, 2 = geschlossen */
void door_draw(int16_t floor, int16_t slot, int16_t state)
{
    if (g_cur_room != 0)
        return;
    clip_set(0xd0, 0x96);
    int16_t y = (int16_t)(((floor - DS_I16(0x8a3c)) + 4) * -0x1e + 0x7c);
    uint8_t *bank = (uint8_t *)DS_PTR(SpriteBank, 0x8c36);
    /* ORIGINAL-BUG: bei anderen Türtypen bleiben die Sprites uninitialisiert */
    Bitmap *closed = NULL, *open = NULL;
    switch (DOOR_TYPE(floor, slot)) {
    case 1: closed = BANK_BMP(bank, 0x42); open = BANK_BMP(bank, 0x4a); break;
    case 2: closed = BANK_BMP(bank, 0x52); open = BANK_BMP(bank, 0x5a); break;
    case 3: closed = BANK_BMP(bank, 0x62); open = BANK_BMP(bank, 0x6a); break;
    default: break;
    }
    int16_t x = (int16_t)(DS_U8(0x7c62 + floor * 0x7a + slot) - 0x38);
    if (state == 1 && open)
        blit_sprite_masked(open, DS_PTR(Bitmap, 0x8eba), x, y, 0, 0, 0x18, 0x18);
    else if (state == 2 && closed)
        blit_sprite_masked(closed, DS_PTR(Bitmap, 0x8eba), x, y, 0, 0, 0x18, 0x18);
}

/* 1f51:1419 – untere Bildleiste (DS:8eb2, 320x80) aus Bank 9, 15 und 10 zusammensetzen */
void panel_build(void)
{
    Bitmap *panel = DS_PTR(Bitmap, 0x8eb2);
    uint8_t *bank9 = (uint8_t *)DS_PTR(SpriteBank, 0x8c58);
    bmp_fill(panel, 0, 0, 0x28, 0x50, 0);
    bmp_copy_at(BANK_BMP(bank9, 2), panel, 0, 2);
    bmp_draw_masked(BANK_BMP(DS_PTR(SpriteBank, 0x79a2), 2), panel, 0xd, 9);
    bmp_copy_at(BANK_BMP(bank9, 10), panel, 0x78, 2);
    g_draw_target = panel;
    g_draw_bank = (uint8_t *)DS_PTR(SpriteBank, 0x799e);
    bank_draw(2, 0xc9, 2);
    bank_draw(10, 0xc9, 0x3a);
    bank_draw(0x12, 0xc9, 0xb);
    bank_draw(0x1a, 0x131, 0xb);
}

/* 1f51:4095 – -1, wenn Mauszeiger (bzw. gezogenes Objekt) das Rechteck berührt, sonst 0 */
int16_t mouse_overlaps_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    if (g_drag_bitmap == NULL) {
        if (mouse_x() + 0x18 >= x0 && mouse_x() <= x1 && mouse_y() + 10 >= y0 && mouse_y() <= y1)
            return -1;
        return 0;
    }
    if (x0 <= mouse_x() + g_drag_w + 8 && mouse_x() <= x1 && y0 <= mouse_y() + g_drag_h && mouse_y() <= y1)
        return -1;
    return 0;
}

/* 1f51:416f – 1, wenn der Mauspunkt im Rechteck (inklusive Rand) liegt */
int16_t mouse_in_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    return x0 <= mouse_x() && mouse_x() <= x1 && y0 <= mouse_y() && mouse_y() <= y1;
}

/* ---------------------------------------------------------------- Figuren und Raumanimationen */
#define RBANK(ofs)        DS_PTR(uint8_t, ofs)   /* Sprite-Bank der Raumressourcen DS:0100.. */
#define RBMP(ofs)         DS_PTR(Bitmap, ofs)    /* Arbeits-Bitmaps der Raumressourcen */
#define g_anim_frame      DS_I16(0x0d3a)         /* Animationszähler des Raums */
#define g_anim_sub        DS_I16(0x0d3c)
#define g_anim_last       DS_I16(0x0d40)

/* Sprite n (1-basiert) einer Raum-Bank maskiert zeichnen. ORIGINAL-BUG: n = 0 liest 6 Byte vor
 * dem Bankanfang (sprite_ofs(0) = -6); das wird hier übersprungen. */
static void room_sprite(int16_t bank_ofs, int16_t n, Bitmap *dst, int16_t x, int16_t y)
{
    if (n < 1)
        return;
    bmp_draw_masked(BANK_BMP(RBANK(bank_ofs), sprite_ofs(n)), dst, x, y);
}

/* 1f51:1503 – Figur in der Gebäudeansicht zeichnen, wenn ihre Etage sichtbar ist */
void figure_draw(int16_t fig, int16_t frame)
{
    uint8_t *f = DS_ADDR(uint8_t, 0x7b22 + fig * 0x1e);
    int16_t floor = *(int16_t *)(f + 2);
    if (floor > DS_I16(0x8a3c) || floor < DS_I16(0x8a3c) - 4)
        return;
    int16_t y = floor * -0x1e + DS_I16(0x8c2e) - 0xb + 0x85;
    int16_t anim = *(int16_t *)(f + 0xc), state = *(int16_t *)(f + 0xe);
    if ((state == 2 && anim > 3) || (state == 4 && anim < 3))
        y--; /* Schrittversatz */
    blit_sprite_masked(BANK_BMP(DS_PTR(uint8_t, 0x00f8), figure_sprite_ofs(fig, frame)), g_upper_screen,
                       *(int16_t *)f, y, 0, 0, 0x10, 0x13);
}

/* 1f51:2c36 – Teiler: *frame alle period Aufrufe weiterzählen */
void anim_step_every(int16_t period, int16_t *frame)
{
    if (++DS_I16(0x0d9a) == period) {
        DS_I16(0x0d9a) = 0;
        (*frame)++;
    }
}

/* fliegender Gegenstand in der Drehbuchagentur (Raum 9): Hintergrund retten, Sprite zeigen */
static void room9_flyer(int16_t bmp_ofs, int16_t w, int16_t h, int16_t work_ofs, int16_t x, int16_t y,
                        int16_t sprite)
{
    Bitmap **save = DS_ADDR(Bitmap *, bmp_ofs);
    int16_t *sx = DS_ADDR(int16_t, bmp_ofs + 4), *sy = DS_ADDR(int16_t, bmp_ofs + 6);
    mouse_hide();
    if (*save == NULL) {
        *save = bitmap_alloc(w, h);
    } else {
        draw_bitmap_fast(*save, (int16_t)(*sx & 0xfff8), *sy);
        scr_blit(*save, (int16_t)(*sx >> 3), *sy);
    }
    *sx = x;
    *sy = y;
    blit_rect(g_upper_screen, *save, 0, 0, x, y, w, h);
    bmp_copy(*save, RBMP(work_ofs));
    room_sprite(0x10c, sprite, RBMP(work_ofs), (int16_t)((x + 2) % 8), 0);
    draw_bitmap_fast(RBMP(work_ofs), x, y);
    scr_blit(RBMP(work_ofs), (int16_t)((x + 2) >> 3), y);
}

static void room9_flyer_end(int16_t bmp_ofs)
{
    Bitmap **save = DS_ADDR(Bitmap *, bmp_ofs);
    int16_t sx = DS_I16(bmp_ofs + 4), sy = DS_I16(bmp_ofs + 6);
    mouse_hide();
    draw_bitmap_fast(*save, (int16_t)(sx & 0xfff8), sy);
    scr_blit(*save, (int16_t)(sx >> 3), sy);
    far_free_ptr((void **)save);
}

/* 1f51:1703 – Raumanimation (mode 1/3 ein Schritt, 2 aufräumen) */
void room_animate(int16_t mode)
{
    int16_t rnd_seed = 0, k, c = 0, a = 0, s;
    Bitmap *work;
    if (mode != 1 && mode != 3) {
        if (mode != 2)
            return;
        g_anim_frame = 0;
        g_anim_sub = 0;
        g_anim_last = -1;
        /* ORIGINAL-BUG: prüft vorher den Eintrag [SI] mit zufälligem SI; hier alle freigeben */
        for (k = 0; k <= 2; k++)
            if (DS_PTR(void, 0xd42 + k * 8) != NULL)
                far_free_ptr(DS_ADDR(void *, 0xd42 + k * 8));
        return;
    }
    if (DS_I16(0x79f8) != 0)
        return;
    int16_t room = g_cur_room, dlg = g_cur_dialog;
    if (room == 10) { /* Immobilienmakler */
        DS_I16(0x7a18) = 1;
        if (g_anim_frame++ > 0x2d)
            g_anim_frame = 1;
        bmp_copy(RBMP(0x108), RBMP(0x10c));
        if (g_anim_frame > 0x28)
            bmp_draw_masked(BANK_BMP(RBANK(0x104), 0xa), RBMP(0x10c), DS_I8(0xd7a + DS_I16(0x7a32) * 2),
                            DS_I8(0xd7b + DS_I16(0x7a32) * 2));
        room_region_restore(9);
        draw_bitmap(RBMP(0x10c), 0xf8, 0x28);
        return;
    }
    if (room == 0xc) { /* Supermarkt */
        work = RBMP(0x118);
        DS_I16(0x7a18) = 1;
        if (dlg == 1 || dlg == 2) {
            if (g_anim_frame++ > 0x1e)
                g_anim_frame = 1;
            bmp_copy(RBMP(0x114), work);
            room_sprite(0x110, 1, work, 0xc, 0x36);
            room_sprite(0x110, 9, work, 6, 1);
            k = g_anim_frame / 10 % 3;
            room_sprite(0x110, (int16_t)(k + 10), work, DS_I8(0xd66 + k * 2), DS_I8(0xd67 + k * 2));
        } else if (dlg == 6) {
            g_anim_frame++;
            bmp_copy(RBMP(0x114), work);
            k = g_anim_frame / 4;
            room_sprite(0x110, 1, work, 0xc, 0x36);
            room_sprite(0x110, (int16_t)(k + 3), work, DS_I8(0xd6c + k * 2), DS_I8(0xd6d + k * 2));
            room_region_restore(8);
            draw_bitmap(work, 0, 0x14);
            if (g_anim_frame == 0x1b) {
                g_cur_dialog = 1;
                confirm_bubble_dialog(0);
            }
            return;
        } else if (dlg == 0) {
            if (g_anim_frame++ > 0x28)
                g_anim_frame = 1;
            bmp_copy(RBMP(0x114), work);
            k = g_anim_frame / 8 % 5;
            room_sprite(0x110, 1, work, 0xc, 0x36);
            room_sprite(0x110, 2, work, 6, 1);
            room_sprite(0x104, k, work, 0x11, 0);
        } else {
            return;
        }
        room_region_restore(8);
        draw_bitmap(work, 0, 0x14);
        return;
    }
    if (room == 1 && DS_I16(0x7a16) == g_local_player && dlg == 0) { /* eigenes Büro */
        if (++g_anim_frame > 0x13f)
            g_anim_frame = 1;
        if (g_anim_frame > 0x117) {
            bmp_copy(RBMP(0x114), RBMP(0x118));
            if (g_anim_frame % 2 == 0)
                room_sprite(0x100, 8, RBMP(0x118), 0xb, 2);
            room_region_restore(7);
        }
        return;
    }
    if (room == 5 || (room == -3 && DS_I16(0x8a28 + DS_I16(0x8e6e) * 2) == 5)) {
        work = RBMP(0x118);
        if (room == 5) { /* Studio */
            DS_I16(0x7a18) = 1;
            if (++g_anim_frame > 0x78)
                g_anim_frame = 1;
            bmp_copy(RBMP(0x114), work);
            room_sprite(0x110, 1, work, 4, 0);
            room_sprite(0x110, 2, work, 4, 0x31);
            if (g_anim_frame > 0x61)
                room_sprite(0x110, 6, work, 0x18, 8);
            if (dlg == 3) {
                if (g_anim_frame % 6 == 0)
                    g_anim_sub = rand_range(5, g_anim_frame) - 1;
                if (g_anim_sub != 0)
                    room_sprite(0x110, (int16_t)(g_anim_sub + 6), work, 0x18, 0x11);
            }
            room_sprite(0x110, (int16_t)(g_anim_frame / 0xc % 3 + 0xf), work, 0x29, 4);
        } else { /* Fernsehbild: Studio */
            if (++g_anim_frame > 0x5c)
                g_anim_frame = 0xc;
            bmp_copy(RBMP(0x114), work);
            if (g_anim_frame < 0xc) {
                k = g_anim_frame / 3 * 3;
                s = DS_I8(0xd5a + k);
                room_sprite(0x110, s, work, DS_I8(0xd5b + k), DS_I8(0xd5c + k));
                if (s < 4)
                    room_sprite(0x110, 1, work, 4, 0);
            } else {
                room_sprite(0x110, 5, work, 7, 1);
                if (g_anim_frame % 4 == 0)
                    g_anim_sub = rand_range(5, g_anim_frame) - 1;
                if (g_anim_sub != 0)
                    room_sprite(0x110, (int16_t)(g_anim_sub + 10), work, 0x1a, 0x10);
            }
        }
        room_region_restore(6);
        draw_bitmap(work, 0, 0x1e);
        return;
    }
    if (room == 8 && (dlg == 0 || dlg == 1 || dlg == 5)) { /* Auktion */
        work = RBMP(0x118);
        g_anim_frame++;
        bmp_draw(RBMP(0x114), work, 0, 0);
        blit_rect(g_upper_screen, work, 0x38, 0x30, 0x48, 0x42, 0x10, 5);
        if (g_anim_frame % 3 == 0)
            g_anim_sub = rand_range(3, g_anim_frame) - 1;
        if (g_anim_sub != 0)
            room_sprite(0x110, g_anim_sub, work, 4, 0x16);
        k = g_anim_frame % 0x38 / 2 + 1;
        if (k > 0x19)
            room_sprite(0x110, (int16_t)((k == 0x1a || k == 0x1c) ? 3 : 4), work, 0xe, 0xd);
        draw_bitmap(work, 0x10, 0x12);
        room_region_restore(5);
        if (g_anim_frame > 500) { /* Auktionator schlägt zu (blockierende Sequenz) */
            g_anim_frame = 1;
            DS_U16(0x6121) = ticks_get();
            int32_t speed = DS_I16(0x7a30) != 0 ? DS_I16(0x7a30) : 1; /* Original: Division durch 0 möglich */
            while (g_anim_frame < 9) {
                if ((uint16_t)((int16_t)(0x32 / speed) * 3 + DS_U16(0x6121)) < ticks_get()) {
                    DS_U16(0x6121) = ticks_get();
                    screen_grab(work, 0x10, 0x12);
                    blit_rect(g_upper_screen, work, 0x38, 0x30, 0x48, 0x42, 0x10, 5);
                    static const int8_t pos[6][2] = {{0x10, 0x18}, {0x10, 0x15}, {0x10, 0x13},
                                                     {0x12, 0x10}, {0x10, 0xe}, {4, 0}};
                    if ((uint16_t)(g_anim_frame - 1) < 6) {
                        c = pos[g_anim_frame - 1][0];
                        a = pos[g_anim_frame - 1][1];
                    } else {
                        c = 4;
                        a = 0;
                        DS_I16(0x7a18) = 1;
                    }
                    room_sprite(0x110, (int16_t)(g_anim_frame < 7 ? g_anim_frame + 4 : 0xb), work, c, a);
                    room_region_restore(5);
                    g_anim_frame++;
                }
            }
            g_anim_frame = 1;
        }
        return;
    }
    if ((room == 2 && dlg == 0) || (room == -3 && DS_I16(0x8a28 + DS_I16(0x8e6e) * 2) == 2)) { /* Nachrichten */
        work = RBMP(0x110);
        DS_I16(0x7a18) = 1;
        if (g_anim_frame++ > 999)
            g_anim_frame = 1;
        bmp_copy(RBMP(0x10c), work);
        k = g_anim_frame % 0x23 + 1;
        if (k > 0x15 && k < 0x1a)
            room_sprite(0x108, 1, work, 6, 0);
        if (room == -3) {
            int16_t v = g_anim_last;
            if (g_anim_frame % 4 == 0 || g_anim_last == -1) {
                do
                    v = rand_range(7, rnd_seed++) - 1;
                while (v == g_anim_last);
            }
            g_anim_last = v;
            if (g_anim_last != 0)
                room_sprite(0x108, (int16_t)(g_anim_last + 1), work, 6, 6);
        }
        draw_bitmap(work, 0xe8, 0x31);
        room_region_restore(4);
        return;
    }
    if (room == 4 && (dlg == 0 || dlg == 3 || dlg == 4)) { /* Filmagentur */
        work = RBMP(0x114);
        int16_t *fr = &g_anim_frame;
        DS_I16(0x0d3e) = 1;
        if (*fr == 0 && rand_range(500, 1) == 1) {
            mouse_hide();
            mouse_show();
            *fr = 1;
        } else if (*fr >= 1 && *fr <= 5) {
            room_sprite(0x10c, (int16_t)(*fr + 8), work, 2, 0);
            anim_step_every(5, fr);
        } else if (*fr >= 6 && *fr <= 9) {
            room_sprite(0x10c, (int16_t)(*fr + 0x10), work, 10, 0x11);
            anim_step_every(3, fr);
        } else if (*fr >= 10 && *fr < 0x1a) {
            room_sprite(0x10c, (int16_t)((*fr - 9) % 8 + 1), work, 10, 0x11);
            room_sprite(0x10c, (int16_t)((*fr - 9) % 8 + 0xe), work, 0x16, 0x25);
            if (++*fr > 0x19) {
                *fr = 10;
                if (++g_anim_sub > 10) {
                    g_anim_sub = 0;
                    *fr = 0x12;
                }
            }
        } else if (*fr >= 0x1a && *fr <= 0x1d) {
            if (*fr == 0x1a)
                room_sprite(0x10c, 13, work, 2, 0);
            room_sprite(0x10c, (int16_t)(0x33 - *fr), work, 10, 0x11);
            anim_step_every(3, fr);
        } else if (*fr >= 0x1e && *fr <= 0x22) {
            room_sprite(0x10c, (int16_t)(0x2b - *fr), work, 2, 0);
            anim_step_every(5, fr);
        } else if (*fr > 0x22) {
            *fr = 0;
            DS_I16(0x7a18) = 1;
        }
        draw_bitmap(work, 0xe8, 0xc);
        room_region_restore(1);
        return;
    }
    if (room != 9) {
        DS_I16(0x7a18) = 1;
        return;
    }
    /* Drehbuchagentur */
    work = RBMP(0x118);
    if (g_anim_frame > 9999 && dlg < 3)
        g_anim_frame = 1;
    if ((dlg == 3 || dlg == 4) && (g_anim_frame < 800 || g_anim_frame > 9999)) {
        if (g_anim_frame < 10000)
            g_anim_frame = 10000;
        int16_t t = g_anim_frame - 9999;
        g_anim_frame++;
        bmp_copy(RBMP(0x114), work);
        room_sprite(0x10c, 23, work, 6, 0);
        room_sprite(0x10c, 3, work, 0x18, 0x1c);
        room_sprite(0x10c, 4, work, 0x18, 0x12);
        uint16_t u = (uint16_t)(t / 3 % 0x14 - 1);
        s = 0;
        if (u < 5)
            s = (u == 0 || u == 4) ? 0x18 : 0x19;
        if (s != 0)
            room_sprite(0x10c, s, work, 0x11, 10);
        room_region_restore(3);
        draw_bitmap_fast(work, 0xd0, 0x20);
        if (g_anim_frame == 0x2716) {
            speech_bubble_text52(0);
            screen_update();
            g_cur_dialog = 4;
        }
        return;
    }
    if (++g_anim_frame > 0x35c)
        g_anim_frame = 1;
    if (g_anim_frame < 800 || g_anim_frame > 0x33f) {
        if (g_anim_frame % 2 == 0) {
            static const int8_t pose[3][3] = {{4, 0x18, 0x12}, {5, 0x19, 0x11}, {6, 0x19, 0x13}};
            k = g_anim_frame / 2 % 3;
            bmp_copy(RBMP(0x114), work);
            room_sprite(0x10c, 3, work, 0x18, 0x1c);
            room_sprite(0x10c, pose[k][0], work, pose[k][1], pose[k][2]);
            room_region_restore(3);
            draw_bitmap_fast(work, 0xd0, 0x20);
        }
    } else if (g_anim_frame % 4 == 0) {
        int16_t u = (g_anim_frame - 800) / 4;
        bmp_copy(RBMP(0x114), work);
        room_sprite(0x10c, 2, work, 0x19, 0xc);
        if (u >= 0 && u < 3)
            room_sprite(0x10c, 3, work, 0x18, 0x1c);
        room_sprite(0x10c, DS_I8(0xd82 + u * 3), work, DS_I8(0xd83 + u * 3), DS_I8(0xd84 + u * 3));
        room_region_restore(3);
        draw_bitmap_fast(work, 0xd0, 0x20);
    }
    /* zwei geworfene Gegenstände fliegen nach links */
    int16_t i = (g_anim_frame - 0x32c) * 4;
    int16_t x = (g_anim_frame - 0x32c) * -8 + 0xe8, y;
    if (i >= 0 && i <= 0x44) {
        if (i < 10)
            y = (g_anim_frame - 0x32c) * -8 + 0x23;
        else if (i < 0x1c)
            y = 0xf;
        else if (i < 0x2c)
            y = i - 0xc;
        else if (i < 0x3c)
            y = (i - 0x2b) * 2 + 0x1f;
        else
            y = (i - 0x3b) * 4 + 0x3f;
        room9_flyer(0xd42, 0x18, 0xc, 0x11c, x, y, (int16_t)((0x5d - i / 4) % 8 + 10));
        mouse_show();
    } else if (i == 0x48) {
        room9_flyer_end(0xd42);
        mouse_show();
    }
    i = (g_anim_frame - 0x338) * 4;
    x = (g_anim_frame - 0x338) * -8 + 0xe8;
    if (i >= 0 && i < 0x41) {
        if (i < 10)
            y = (g_anim_frame - 0x338) * -8 + 0x1e;
        else if (i < 0x1c)
            y = 10;
        else if (i < 0x2c)
            y = i - 0x11;
        else if (i < 0x3c)
            y = (i - 0x2b) * 2 + 0x1a;
        else
            y = (i - 0x3b) * 4 + 0x3a;
        room9_flyer(0xd4a, 0x20, 0x14, 0x120, x, y, (int16_t)((0x5e - i / 4) % 5 + 0x12));
        mouse_show();
        return;
    }
    if (i != 0x44)
        return;
    room9_flyer_end(0xd4a);
    for (k = 0; k <= 4; k++) { /* geworfener Gegenstand landet im Regal */
        if (DS_I16(0x79a6 + (k + 0x2a) * 2) < 0) {
            DS_I16(0x79a6 + (k + 0x2a) * 2) = (int16_t)-DS_I16(0x79a6 + (k + 0x2a) * 2);
            break;
        }
    }
    room_items_draw();
    mouse_show();
    screen_update();
    DS_I16(0x7a18) = 1;
}

/* 1f51:2c53 – Raumbereich (Arbeits-Bitmap) auf den Bildschirm bringen; Maus ggf. verstecken */
void room_region_restore(int16_t region)
{
    static const struct { int16_t res, xcol, y, x0, y0, x1, y1; } r[10] = {
        {0, 0, 0, 0, 0, 0, 0},
        {0x114, 0x1d, 0x0c, 0xe8, 0x0c, 0x120, 0x39},
        {0, 0, 0, 0, 0, 0, 0},
        {0x118, 0x1a, 0x20, 0xd0, 0x20, 0x100, 0x48},
        {0x110, 0x1d, 0x31, 0xe8, 0x31, 0x100, 0x3b},
        {0x118, 0x02, 0x12, 0x10, 0x12, 0x58, 0x47},
        {0x118, 0x00, 0x1e, 0x00, 0x1e, 0x50, 0x71},
        {0x118, 0x0f, 0x45, 0x78, 0x45, 0xa0, 0x55},
        {0x118, 0x00, 0x14, 0x00, 0x14, 0x30, 0x72},
        {0x10c, 0x1f, 0x28, 0xf8, 0x28, 0x118, 0x32},
    };
    if (region < 1 || region > 9 || r[region].res == 0)
        return; /* im Original: Blit mit uninitialisierten Werten */
    int16_t hidden = 0;
    if (mouse_overlaps_rect(r[region].x0, r[region].y0, r[region].x1, r[region].y1) != 0) {
        mouse_hide();
        hidden = 1;
    }
    scr_blit(RBMP(r[region].res), r[region].xcol, r[region].y);
    if (hidden)
        mouse_show();
}

/* 1f51:2e46 – Rahmen-Bitmap DS:8ec2 aus dem aktuellen Bildschirmrand aufbauen (Zellen à 8 px) */
void popup_box_create(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int16_t w = x1 - x0 + 1, h = y1 - y0 + 1;
    uint8_t *stat = DS_PTR(uint8_t, 0x0124);
    Bitmap *box;
    if (stat == NULL) {
        box = bitmap_alloc((int16_t)(w * 8), (int16_t)(h * 8));
    } else {
        box = (Bitmap *)stat;
        box->wcols = w;
        box->h = (int16_t)(h * 8);
        box->pixels = stat + 8;
    }
    DS_PTR(Bitmap, 0x8ec2) = box;
    int16_t n = w > y1 - y0 - 1 ? w : y1 - y0 - 1;
    uint8_t *mem = far_calloc((uint16_t)(n * 0x40 + 8), 1);
    Bitmap *tmp = (Bitmap *)mem;
    tmp->wcols = w;
    tmp->h = 8;
    tmp->pixels = mem + 8;
    scr_grab(tmp, x0, (int16_t)(y0 << 3));
    bmp_copy(tmp, box);
    if (y0 < y1) {
        scr_grab(tmp, x0, (int16_t)(y1 << 3));
        bmp_copy_at(tmp, box, 0, (int16_t)((y1 - y0) * 8));
    }
    if (y1 - y0 > 1) {
        tmp->wcols = 1;
        tmp->h = (int16_t)((y1 - y0 - 1) * 8);
        tmp->pixels = mem + 8;
        scr_grab(tmp, x0, (int16_t)((y0 + 1) * 8));
        bmp_copy_at(tmp, box, 0, 8);
        scr_grab(tmp, x1, (int16_t)((y0 + 1) * 8));
        bmp_copy_at(tmp, box, (int16_t)((x1 - x0) * 8), 8);
    }
    far_free(mem);
    draw_frame_cells(0, 0, x1 - x0, y1 - y0, box, 1);
}

/* 1f51:3060 */
void popup_box_present(int16_t xcol, int16_t yrow)
{
    scr_blit(DS_PTR(Bitmap, 0x8ec2), xcol, (int16_t)(yrow << 3));
    if (DS_PTR(uint8_t, 0x0124) == NULL)
        far_free(DS_PTR(Bitmap, 0x8ec2));
}

/* 1f51:3099 – kleinen Fernseher der unteren Leiste neu zeichnen */
/* ---- PORT: Rollladen des Fernsehbilds flüssig ----
 * Das Original öffnet/schließt den Rollladen (Bänder zu 5 Pixeln von oben und unten, Zähler
 * DS:79d4) um eine Stufe je Aufruf von tv_picture_compose, also je Spielschritt. Hier wird das
 * Bild zusätzlich ohne Rollladen aufbewahrt und bei jedem angezeigten Bild der Rollladen
 * pixelgenau mit einem Zwischenstand zwischen vorletzter und letzter Stufe darübergelegt
 * (gleiche Grafik, gleiche Dauer). */
static Bitmap *g_tv_open; /* letztes Fernsehbild ohne Rollladen und Rahmen */
/* letzte Stufen (Anzahl Bänder) mit Zeitpunkt, [0] = neueste; dargestellt wird eine Stufe plus
 * Reserve in der Vergangenheit, damit etwas zu spät kommende Spielschritte nicht stocken */
static int16_t g_sh_level[3];
static uint64_t g_sh_time[3];
static int g_sh_count, g_shutter_active;
#define SHUTTER_STEP_US 80000
#define SHUTTER_RESERVE_US 16000

static void tv_frame_draw(Bitmap *b, int coverage)
{
    /* Rollladen: coverage Pixel von oben und von unten, aus 5-Pixel-Bändern (Sprite 0x3a) */
    int16_t clip_on = g_clip_on, clip_top = g_clip_top, clip_bottom = g_clip_bottom;
    g_draw_target = b;
    g_draw_bank = DS_PTR(uint8_t, 0x79a2);
    g_clip_on = 1;
    g_clip_top = 0;
    g_clip_bottom = 0x3a;
    for (int y = coverage - 5; y > -5; y -= 5)
        bank_draw(0x3a, 0, (int16_t)y);
    for (int y = 0x3a - coverage; y < 0x3a && coverage > 0; y += 5)
        bank_draw(0x3a, 0, (int16_t)y);
    g_clip_on = clip_on;
    g_clip_top = clip_top;
    g_clip_bottom = clip_bottom;
    bank_draw(0x42, 0, 0);
    bank_draw(0x4a, 0x5e, 0);
    bank_draw(0x52, 0, 0x33);
}

/* Nach tv_picture_compose: Bild ohne Rollladen merken, neue Stufe mit Zeitpunkt festhalten */
static void tv_shutter_note(Bitmap *open)
{
    if (!g_tv_open)
        g_tv_open = bitmap_alloc(0x68, 0x3a);
    bmp_copy(open, g_tv_open);
    int16_t level = DS_I16(0x79d4);
    uint64_t now = time_us();
    if (g_sh_count == 0 || level != g_sh_level[0]) {
        int jump = g_sh_count == 0 || (level - g_sh_level[0] != 1 && g_sh_level[0] - level != 1);
        if (jump) /* neue Sendung oder Abschalten: ohne Übergang übernehmen */
            g_sh_count = 0;
        else if (now > g_sh_time[0] + SHUTTER_STEP_US * 3 / 2) {
            /* die bisherige Stufe stand länger: sie gilt bis einen Spielschritt vor der neuen */
            g_sh_time[0] = now - SHUTTER_STEP_US;
            g_sh_count = 1;
        }
        for (int k = 2; k > 0; k--) {
            g_sh_level[k] = g_sh_level[k - 1];
            g_sh_time[k] = g_sh_time[k - 1];
        }
        /* Zeitpunkt auf das Raster der Spielschritte legen und sanft nachführen */
        uint64_t t = now;
        if (!jump && now >= g_sh_time[1] + SHUTTER_STEP_US / 2 && now <= g_sh_time[1] + SHUTTER_STEP_US * 3 / 2)
            t = g_sh_time[1] + SHUTTER_STEP_US + ((int64_t)now - (int64_t)(g_sh_time[1] + SHUTTER_STEP_US)) / 8;
        g_sh_level[0] = level;
        g_sh_time[0] = t;
        if (jump) { /* Ausgangsstand gilt schon eine Stufe vorher */
            g_sh_level[1] = level;
            g_sh_time[1] = t - SHUTTER_STEP_US;
            g_sh_count = 2;
        } else if (g_sh_count < 3) {
            g_sh_count++;
        }
        g_shutter_active = 1;
        if (platform_debug_env("MADTV_FRAMELOG"))
            platform_debug_log("SHUTTER %d bei %u ms", level, platform_ms());
    }
}

/* Zwischenstand des Rollladens für das nächste angezeigte Bild zeichnen */
void tv_shutter_render_smooth(void)
{
    /* läuft weiter, bis der Endstand dargestellt ist – auch wenn das Spiel den Rollladen schon
     * für fertig erklärt hat (DS:79d6 = 0), denn die Darstellung liegt eine Stufe zurück */
    if (!g_shutter_active || !g_tv_open || g_sh_count < 2 || DS_I16(0x7a4c) != 0)
        return;
    uint64_t when = display_next_frame_us() - SHUTTER_STEP_US - SHUTTER_RESERVE_US;
    int a = 1, b = 0;
    for (int k = 1; k < g_sh_count; k++) {
        a = k;
        b = k - 1;
        if (g_sh_time[a] <= when)
            break;
    }
    uint32_t den = g_sh_time[b] > g_sh_time[a] ? (uint32_t)(g_sh_time[b] - g_sh_time[a]) : 1;
    uint32_t num = when <= g_sh_time[a] ? 0 : when - g_sh_time[a] >= den ? den : (uint32_t)(when - g_sh_time[a]);
    int coverage = (g_sh_level[a] * 5 * 256 + (g_sh_level[b] - g_sh_level[a]) * 5 * 256 * (int)num / (int)den + 128) >> 8;
    if (b == 0 && num >= den && (g_sh_level[0] == 0 || g_sh_level[0] >= 6))
        g_shutter_active = 0; /* Endstand erreicht */
    Bitmap *bm = bitmap_alloc(0x68, 0x3a);
    bmp_copy(g_tv_open, bm);
    tv_frame_draw(bm, coverage);
    scr_blit(bm, 0x1a, 0x7f);
    far_free(bm);
}

void tv_picture_update(void)
{
    int16_t hidden = 0;
    clip_reset();
    if (mouse_y() > 0x6d && mouse_x() > 0xbd && g_cursor_visible == 1) {
        mouse_hide();
        hidden = 1;
    }
    if (DS_I16(0x7a4c) == 0) {
        Bitmap *b = tv_picture_compose();
        scr_blit(b, 0x1a, 0x7f);
        far_free(b);
        tv_shutter_render_smooth(); /* PORT: sofort Zwischenstand statt der neuen Stufe */
    }
    if (hidden)
        mouse_show();
}

#define TV_LAYER(i)       DS_ADDR(uint8_t, 0x830e + (i) * 0x10) /* +0 Bank, +4 x, +6 Pose, +8 Takt, +0xa, +0xc, +0xe Blinzeln */
#define TVL_W(i, ofs)     (*(int16_t *)(TV_LAYER(i) + (ofs)))
#define g_tv_bg           DS_PTR(Bitmap, 0x7bdc)

static void use_bank(uint8_t *bank) { g_draw_bank = bank; }
static void bank_draw_n(int16_t n, int16_t x, int16_t y) { bank_draw(sprite_ofs(n), x, y); }

/* 1f51:3105 – Fernsehbild aus Hintergrund und bis zu fünf animierten Ebenen zusammensetzen */
Bitmap *tv_picture_compose(void)
{
    Bitmap *b = bitmap_alloc(0x68, 0x3a);
    bmp_fill(b, 0, 0, 1, 0x3a, 0);
    g_draw_target = b;
    bmp_copy(g_tv_bg, b);
    if (DS_I16(0x79d6) != 0) {
        int16_t x, k;
        if (TVL_W(0, 8) != 0) { /* Ebene 0: Moderator */
            use_bank(DS_PTR(uint8_t, 0x830e));
            x = TVL_W(0, 4);
            TVL_W(0, 8)++;
            TVL_W(0, 0xc)++;
            if (TVL_W(0, 0xc) > 0x2b) {
                bank_draw(0x22, x + 7, 9);
                if (TVL_W(0, 0xc) == 0x2d)
                    TVL_W(0, 0xc) = 1;
            }
            if (TVL_W(0, 8) > 8)
                TVL_W(0, 8) = 1;
            k = (TVL_W(0, 8) + 1) / 2;
            if (k == 1 || k == 3)
                k = 2;
            else if (k == 2)
                k = 3;
            bank_draw_n(k, x + 10, 0x10);
            anim_blink_draw(TV_LAYER(0), 4, 8, 7, 0xd);
            anim_blink_draw(TV_LAYER(0), 6, 10, 7, 0xd);
        }
        use_bank(DS_PTR(uint8_t, 0x831e)); /* Ebene 1 */
        x = TVL_W(1, 4);
        if (TVL_W(1, 8) == 1) {
            TVL_W(1, 0xa)++;
            if (TVL_W(1, 0xa) % 0x32 > 0x2f)
                bank_draw(0x52, x + 8, 0xb);
            anim_blink_draw(TV_LAYER(1), 2, 10, 8, 0x11);
            anim_blink_draw(TV_LAYER(1), 4, 0xc, 8, 0x11);
        }
        if (TVL_W(1, 8) == 2) {
            if (++TVL_W(1, 0xa) > 0xb9)
                TVL_W(1, 0xa) = 0x7e;
            int16_t t = TVL_W(1, 0xa);
            if (t < 0x56)
                k = 2;
            else if (t < 0x7e || (t - 0x60) / 0x1e != 1)
                k = 3;
            else
                k = 0xe;
            bank_draw_n(k, x + 8, 7);
            if (t > 0x4b && t < 0x51)
                bank_draw(0x1a, x + 7, 0x18);
            if (t > 0x50 && t < 0x56)
                bank_draw(0x22, x + 7, 0x18);
            if (t > 0x55)
                bank_draw(0x2a, x + 7, 0x18);
            if (t > 0x5a)
                bank_draw(0x32, x + 0xc, 0x2c);
            if (t > 0x9b)
                bank_draw(0x3a, x + 7, 0x18);
        }
        use_bank(DS_PTR(uint8_t, 0x832e)); /* Ebene 2 */
        x = TVL_W(2, 4);
        if (TVL_W(2, 8) == 1 && TVL_W(2, 0xa)++ > 100)
            TVL_W(2, 0xa) = 1;
        if (TVL_W(2, 8) == 2) {
            if (++TVL_W(2, 0xa) > 0x87)
                TVL_W(2, 0xa) = 1;
            bank_draw(0x72, x + 6, 5);
            static const int8_t mouth[8] = {2, 3, 2, 3, 0, 2, 0, 3};
            uint16_t u = (uint16_t)((TVL_W(2, 0xa) + 0xe) / 0xf - 2);
            k = u < 8 ? mouth[u] : 0;
            if (k != 0)
                bank_draw_n(k, x + 8, 0xe);
        } else if (TVL_W(2, 8) == 3) {
            if (++TVL_W(2, 0xa) > 0x11d)
                TVL_W(2, 0xa) = 0xe2;
            if (TVL_W(2, 0xa) < 0xe2)
                bank_draw_n((int16_t)((TVL_W(2, 0xa) + 0x4a) / 0x4b + 3), x + 6, 4);
            else
                bank_draw_n((int16_t)((TVL_W(2, 0xa) - 0xc4) / 0x1e + 6), x + 6, 4);
        }
        if (TVL_W(2, 8) == 1) {
            if (TVL_W(2, 0xa) % 0x2d > 0x2a)
                bank_draw(0x4a, x + 6, 9);
            anim_blink_draw(TV_LAYER(2), 0xb, 0xb, 6, 8);
            anim_blink_draw(TV_LAYER(2), 0xc, 0xc, 6, 8);
        }
        if (TVL_W(3, 8) != 0) { /* Ebene 3 */
            use_bank(DS_PTR(uint8_t, 0x833e));
            x = TVL_W(3, 4);
            TVL_W(3, 8)++;
            int16_t pose = TVL_W(3, 6);
            k = (pose == 7 || pose == 6) ? 1 : pose == 5 ? 2 : 3;
            if (TVL_W(3, 8) > k * 9 + 0x4b)
                TVL_W(3, 8) = 1;
            if (TVL_W(3, 8) % 0x28 > 0x24)
                bank_draw(0x4a, x + 7, 9);
            anim_blink_draw(TV_LAYER(3), 6, 6, 7, 0);
            anim_blink_draw(TV_LAYER(3), 7, 7, 7, 0);
            anim_blink_draw(TV_LAYER(3), 9, 9, 7, 0);
            if (TVL_W(3, 8) > 0x4e)
                bank_draw_n((int16_t)(((TVL_W(3, 8) - 0x49) / 3 - 1) % 3 + 2), x + 2, 0xc);
        }
        if (TVL_W(4, 8) != 0) { /* Ebene 4 */
            use_bank(DS_PTR(uint8_t, 0x834e));
            x = TVL_W(4, 4);
            if (++TVL_W(4, 0xa) > 0x28)
                TVL_W(4, 0xa) = 1;
            if (TVL_W(4, 0xa) > 0x24)
                bank_draw(0x52, x + 7, 0x10);
            k = (TVL_W(4, 0xa) % 0x14 + 5) / 5;
            if (k == 4)
                k = 2;
            bank_draw_n((int16_t)(k + 7), x + 2, 0x26);
            bank_draw_n((int16_t)(k * 2 + 0xc), x, 0xb);
            bank_draw_n((int16_t)(k * 2 + 0xd), x + 0x10, 0xb);
            if (TVL_W(4, 8) == 2) {
                if (++TVL_W(4, 0xc) > 0x33)
                    TVL_W(4, 0xc) = 1;
                bank_draw_n(DS_I8(0xd9b + (TVL_W(4, 0xc) + 2) / 3), x + 1, 0x18);
            }
        }
    }
    use_bank(DS_PTR(uint8_t, 0x79a2)); /* Rahmen und Rollladen */
    bank_draw(10, 0x54, 4);
    Bitmap *open = b; /* PORT: Stand ohne Rollladen merken (nach der Stufenänderung unten) */
    if (DS_I16(0x79d6) == 1) {
        if (++DS_I16(0x79d4) == 6)
            DS_I16(0x79d6) = 0;
    } else if (DS_I16(0x79d6) == 2 && --DS_I16(0x79d4) == 0) {
        DS_I16(0x79d6) = 3;
    }
    Bitmap *snap = bitmap_alloc(0x68, 0x3a);
    bmp_copy(open, snap);
    tv_shutter_note(snap);
    far_free(snap);
    g_draw_target = b;
    use_bank(DS_PTR(uint8_t, 0x79a2));
    for (int16_t i = 0; i < DS_I16(0x79d4); i++) {
        bank_draw(0x3a, 0, (int16_t)(i * 5));
        bank_draw(0x3a, 0, (int16_t)(i * -5 + 0x35));
    }
    bank_draw(0x42, 0, 0);
    bank_draw(0x4a, 0x5e, 0);
    bank_draw(0x52, 0, 0x33);
    return b;
}

/* 1f51:38b1 – statischer Hintergrund des Fernsehbilds (Studio) */
void tv_picture_bg_draw(void)
{
    g_draw_target = g_tv_bg;
    use_bank(DS_PTR(uint8_t, 0x79a2));
    for (int16_t x = 0; x < 0x61; x += 8) {
        bank_draw(0x2a, x, 0);
        bank_draw(0x2a, x, 8);
        bank_draw(0x32, x, 0x32);
    }
    for (int16_t x = 0x1f; x < 0x44; x += 8)
        bank_draw(0x22, x, 0xe);
    bank_draw(0x12, 0, 0xe);
    bank_draw(0x1a, 0x43, 0xe);
}

/* 1f51:3946 – Ebene 0 (Moderator) fest ins Hintergrundbild */
void tv_layer0_draw(void)
{
    int16_t x = TVL_W(0, 4), pose = TVL_W(0, 6), deco = TVL_W(0, 0xa);
    use_bank(DS_PTR(uint8_t, 0x830e));
    g_draw_target = g_tv_bg;
    bank_draw(2, x, 4);
    if (pose != 0) {
        int16_t k = pose == 4 ? 2 : pose;
        if (k == 6)
            k = 3;
        bank_draw_n((int16_t)(k + 4), x + 7, 0xd);
    }
    if (deco > 1)
        bank_draw(0x52, x, 0x1c);
    if (deco > 2)
        bank_draw(0x5a, x, 0x25);
    if (deco > 3 && deco != 7)
        bank_draw(0x62, x, 0x2d);
    if (deco == 5)
        bank_draw(0x6a, x + 0xc, 0x2f);
    if (deco == 6)
        bank_draw(0x72, x + 1, 0x2e);
    if (deco == 7)
        bank_draw(0x7a, x, 0x2d);
}

/* 1f51:3a78 – Ebene 1 */
void tv_layer1_draw(void)
{
    int16_t x = TVL_W(1, 4);
    use_bank(DS_PTR(uint8_t, 0x831e));
    g_draw_target = g_tv_bg;
    bank_draw(2, x, 0x12);
    if (TVL_W(1, 8) == 1) {
        bank_draw(0x42, x + 8, 7);
        if (TVL_W(1, 6) == 5)
            bank_draw_n((int16_t)(TVL_W(1, 6) + 8), x + 8, 0x11);
    }
}

/* 1f51:3b02 – Ebene 2 */
void tv_layer2_draw(void)
{
    int16_t x = TVL_W(2, 4), pose = TVL_W(2, 6);
    use_bank(DS_PTR(uint8_t, 0x832e));
    g_draw_target = g_tv_bg;
    bank_draw(2, x, 0xb);
    if (TVL_W(2, 8) == 1) {
        bank_draw(0x42, x + 6, 5);
        if (pose != 9 && pose != 0xb && pose != 0xc)
            bank_draw_n(pose, x + 6, 8);
    } else if (TVL_W(2, 8) == 2) {
        bank_draw(0x72, x + 6, 5);
    }
}

/* 1f51:3bb7 – Ebene 3 */
void tv_layer3_draw(void)
{
    int16_t x = TVL_W(3, 4), pose = TVL_W(3, 6);
    use_bank(DS_PTR(uint8_t, 0x833e));
    g_draw_target = g_tv_bg;
    bank_draw(2, x, 0);
    if (pose == 6 || pose == 7)
        pose = 5;
    if (pose == 9)
        pose = 8;
    bank_draw_n(pose, x + 7, 0);
}

/* 1f51:3c2a – Ebene 4 */
void tv_layer4_draw(void)
{
    int16_t x = TVL_W(4, 4);
    use_bank(DS_PTR(uint8_t, 0x834e));
    g_draw_target = g_tv_bg;
    bank_draw(2, x + 7, 0xb);
    if (TVL_W(4, 6) != 0)
        bank_draw_n(TVL_W(4, 6), x + 7, 0x13);
    bank_draw(10, x + 1, 0x23);
    if (TVL_W(4, 8) == 1)
        bank_draw(0x12, x + 1, 0x18);
}

/* 1f51:3cc6 */
void tv_picture_tick(void)
{
    if (DS_I16(0x79d6) != 0)
        tv_picture_update();
}

/* 1f51:3cd2 – Gegenstände im Regal (Drehbuchagentur) */
void room_items_draw(void)
{
    clip_reset();
    g_clip_bottom = 0x71;
    for (int16_t i = 0; i < 5; i++) {
        int16_t x = DS_I16(0x79a6 + (i + 0x2a) * 2);
        if (x > 0)
            draw_sprite(BANK_BMP(RBANK(0x10c), sprite_ofs((int16_t)(i + 0x12))), x, 100);
    }
}

/* 1f51:3d24 – Papierstapel (DS:7a04 Blätter, max 36) */
void room_stack_overlay_draw(void)
{
    if (DS_I16(0x7a04) > 0x24)
        DS_I16(0x7a04) = 0x24;
    if (DS_I16(0x7a04) > 0)
        draw_sprite(BANK_BMP(RBANK(0x100), 10), 0x7a, 0x45);
    if (DS_I16(0x7a04) < 2 || DS_I16(0x7a04) > 6) {
        for (int16_t i = DS_I16(0x7a04) - 6; i >= 0; i--)
            draw_sprite(BANK_BMP(RBANK(0x100), 0x32), DS_I16(0xdad + i * 4), DS_I16(0xdaf + i * 4));
    } else {
        for (int16_t i = 0; i <= DS_I16(0x7a04) - 2; i++)
            draw_sprite(BANK_BMP(RBANK(0x100), sprite_ofs(DS_I16(0xe2d + i * 6))),
                        (int16_t)(DS_I16(0xe29 + i * 6) + DS_I16(0xdad)),
                        (int16_t)(DS_I16(0xe2b + i * 6) + DS_I16(0xdaf)));
    }
}

/* 1f51:3e0f – Markierung des Sonderfilms im eigenen Büro */
void room_owner_marker_draw(void)
{
    int16_t v = DS_I16(0x79a6 + (DS_I16(0x7a16) + 0x3a) * 2);
    if (v != 0) {
        draw_sprite(BANK_BMP(RBANK(0x100), 0x42), 0xb0, 0x4c);
        if (v == 0x29)
            draw_sprite(BANK_BMP(RBANK(0x100), 0x4a), 0xc4, 0x58);
    }
}

/* 1f51:3e66 – Auktion: Bild je nach Tageszeit */
void room_variant_overlay_draw(void)
{
    static const int16_t pos[4][2] = {{0xa1, 0x1b}, {0xa0, 0x22}, {0xa0, 0x26}, {0xa0, 0x1c}};
    uint16_t v = (uint16_t)(DS_I16(0x7a06) - 0xbc);
    if (v >= 4)
        return; /* im Original uninitialisierte Position */
    draw_sprite(BANK_BMP(RBANK(0x110), 2), pos[v][0], pos[v][1]);
}

/* 1f51:3eb9 – Immobilienmakler: Häuserbild und Menschenmenge nach Belegung DS:7a0e */
void room_crowd_draw(void)
{
    static const int8_t level[11] = {0, 0, 0, 1, 1, 2, 2, 2, 3, 3, 3};
    int16_t seed = 0;
    clip_reset();
    g_clip_bottom = 0x71;
    if ((uint16_t)DS_I16(0x7a0e) < 0xb)
        DS_I16(0x7a32) = level[DS_I16(0x7a0e)];
    draw_sprite(BANK_BMP(RBANK(0x104), 2), DS_U8(0xe5f + DS_I16(0x7a32) * 2), DS_U8(0xe60 + DS_I16(0x7a32) * 2));
    for (int16_t i = 3; i >= 0; i--) {
        int16_t n = DS_I16(0x7a0e) * 5 / 10 - (3 - i);
        if (n <= 0)
            continue;
        int16_t dy = 0;
        for (int16_t j = 1; j <= n; j++) {
            dy += (j + 1) % 2 + 1 == 1 ? 7 : 5;
            int16_t y = DS_U8(0xe48 + i * 2) - dy;
            int16_t x = rand_range(3, seed++) + DS_U8(0xe47 + i * 2) - 2;
            draw_sprite(BANK_BMP(RBANK(0x100), sprite_ofs((int16_t)((j + 1) % 2 + 1))), x, y);
        }
    }
    for (int16_t i = 0; i <= 7; i++) {
        int16_t n = DS_I16(0x7a0e) * 6 / 10 - i / 2;
        if (n <= 0)
            continue;
        for (int16_t j = 1; j <= n; j++)
            draw_sprite(BANK_BMP(RBANK(0x100), sprite_ofs((int16_t)(i % 2 + 3))), DS_U8(0xe4f + i * 2),
                        (int16_t)(DS_U8(0xe50 + i * 2) - j * 4 + 4));
    }
}

/* 1f51:410c – Blinzeln einer Fernsehfigur in Pose state */
void anim_blink_draw(void *anim, int16_t state, int16_t sprite, int16_t dx, int16_t y)
{
    uint8_t *a = anim;
    int16_t *blink = (int16_t *)(a + 0xe);
    if (*(int16_t *)(a + 6) != state)
        return;
    if (*blink == 0) {
        if (rand_range(0x32, 1) == 1)
            *blink = 10;
    } else {
        (*blink)--;
        target_draw_sprite(BANK_BMP(*(uint8_t **)a, sprite_ofs(sprite)), (int16_t)(*(int16_t *)(a + 4) + dx), y);
    }
}

/* 1f51:41a1 – Spielende: Abschlussanimation, auf Taste/Klick warten, Programm beenden */
void game_end_sequence(void)
{
    room_free_resources();
    music_on_leave(0x42, 0);
    music_on_enter(0x42, 0);
    far_free(g_upper_screen);
    g_upper_screen = bitmap_alloc(0x140, 200);
    bmp_fill(g_upper_screen, 0, 0, 0x28, 200, 0);
    RBANK(0x100) = (uint8_t *)hes_load_unpack0(0x7d);
    draw_bitmap_fast(BANK_BMP(RBANK(0x100), 2), 0, 1);
    far_free_ptr(DS_ADDR(void *, 0x100));
    RBANK(0x100) = (uint8_t *)hes_load_unpack0(0x4b);
    draw_bitmap_fast(BANK_BMP(RBANK(0x100), 2), 0xd0, 1);
    far_free_ptr(DS_ADDR(void *, 0x100));
    RBANK(0x100) = (uint8_t *)hes_load_unpack0(0xef);
    draw_bitmap_fast(BANK_BMP(RBANK(0x100), 2), 0x70, 1);
    far_free_ptr(DS_ADDR(void *, 0x100));
    RBANK(0x100) = (uint8_t *)hes_load_unpack(0xf0);
    RBMP(0x104) = bitmap_alloc(0x80, 0x8e);
    g_clip_on = 0;
    blit_rect(g_upper_screen, RBMP(0x104), 0, 0, 0x60, 0x23, 0x80, 0x8e);
    draw_sprite(BANK_BMP(RBANK(0x100), 2), 0x60, 0x23);
    screen_wipe_in(g_upper_screen, 0, 0, 0x140, 200, 2, 1);
    for (int16_t f = 1; f < 0x6a; f++) {
        ticks_set(0);
        int16_t hit = 0, spr = 0, x = 0, y = 0;
        for (int16_t i = 0; i < 6; i++) {
            if (DS_U8(0xe67 + i * 4) == f) {
                hit = 1;
                spr = DS_U8(0xe68 + i * 4);
                x = DS_U8(0xe69 + i * 4);
                y = DS_U8(0xe6a + i * 4);
            }
        }
        if (hit) {
            draw_bitmap_fast(RBMP(0x104), 0x60, 0x23);
            if (spr != 0)
                draw_sprite(BANK_BMP(RBANK(0x100), sprite_ofs(spr)), x, y);
            ScrRectArgs a = {g_upper_screen, 0xc, 0x23, 0x10, 0x8e, 0xc, 0x23};
            scr_blit_rect(&a);
        }
        while (ticks_get() < 3)
            ;
    }
    while (kbd_getkey() == -1 && mouse_poll() == 0)
        ;
    bmp_fill(g_upper_screen, 0, 0, 0x28, 200, 0);
    screen_wipe_in(g_upper_screen, 0, 0, 0x140, 200, 1, 1);
    tc_exit(0);
}
