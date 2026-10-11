/* Segment 1dd5 – Spieltakt, Software-Mauszeiger, Bildaufbau, Texteingabe, Büro-Tetris und Rahmen.
 *
 * Der Mauszeiger ist ein Software-Sprite (3 Spalten x 11 Zeilen). Vor dem Zeichnen wird der
 * Hintergrund gesichert (DS:8e70 frisch gegriffen, 8eb6 sauberer Hintergrund, 8ebe Hintergrund
 * der alten Position); überlappen alte und neue Position, werden die Puffer gegenseitig
 * ausgebessert, damit beim Wiederherstellen nichts flackert. */
#include <string.h>

#include "game.h"

#define g_mouse_acc_x    DS_I16(0x7a28) /* aufgelaufene Mausbewegung (input_poll) */
#define g_mouse_acc_y    DS_I16(0x7a2a)
#define g_cursor_old_x   DS_I16(0x7b06) /* zuletzt gezeichnete Zeigerposition, -1 = keine */
#define g_cursor_old_y   DS_I16(0x7b16)
#define g_cursor_min_x   DS_I16(0x79cc)
#define g_cursor_min_y   DS_I16(0x79ce)
#define g_cursor_max_x   DS_I16(0x79d0)
#define g_cursor_max_y   DS_I16(0x79d2)
#define g_cursor_visible DS_I16(0x8aac)
#define g_cursor_shape   DS_I16(0x79b2) /* 2 = gezogenes Objekt, 3 = beschäftigt */
#define g_cursor_bitmaps DS_ADDR(Bitmap *, 0x8e74)
#define g_drag_bitmap    DS_PTR(Bitmap, 0x0390) /* am Zeiger hängendes Objekt (Ziehen) */
#define g_drag_w         DS_I16(0x0394)
#define g_drag_h         DS_I16(0x0396)
#define g_drag_hot_x     DS_I16(0x0398)
#define g_drag_hot_y     DS_I16(0x039a)
#define g_drag_x         DS_I16(0x8efe)
#define g_drag_y         DS_I16(0x8f00)
#define g_cursor_grab    DS_PTR(Bitmap, 0x8e70) /* Bildschirm unter neuer Position */
#define g_cursor_bg_old  DS_PTR(Bitmap, 0x8ebe) /* Hintergrund der alten Position */
#define g_cursor_bg_new  DS_PTR(Bitmap, 0x8eb6) /* sauberer Hintergrund der neuen Position */
#define g_upper_screen   DS_PTR(Bitmap, 0x8e62) /* 320x120 obere Bildhälfte (Spielszene) */
#define g_lower_panel    DS_PTR(Bitmap, 0x8eb2) /* 320x80 untere Bildhälfte */
#define g_clock_offset   DS_I16(0x6121)
#define g_intro_mode     DS_I16(0x012c) /* 0 = normales Spiel */

/* Tetris im Büro */
#define g_tetris_state   DS_I16(0x7a54) /* bis 0x2a hochzählen = läuft, 0x2b = vorbei */
#define g_tetris_score   DS_I16(0x7a56)
#define g_tetris_piece   DS_I16(0x7a58) /* 1..7, 0 = neuen Stein holen */
#define g_tetris_next    DS_I16(0x7a5a)
#define g_tetris_rot     DS_I16(0x7a5c)
#define g_tetris_level   DS_I16(0x7a5e) /* Fallverzögerung, sinkt alle 10 Reihen */
#define g_tetris_lines   DS_I16(0x7a60)
#define g_tetris_input   DS_I16(0x7a62) /* 1 links drehen, 2 rechts drehen, 3 links, 4 rechts, 5 fallen */
#define g_tetris_high    DS_I16(0x7a64)
#define g_tetris_col     DS_I16(0x8f0c)
#define g_tetris_row     DS_I16(0x8f0e)
#define g_tetris_old_col DS_I16(0x0d31) /* zuletzt gezeichnete Position, 999 = keine */
#define g_tetris_old_row DS_I16(0x8f10)
#define g_tetris_old_pc  DS_I16(0x8f12)
#define g_tetris_old_rot DS_I16(0x8f14)
#define g_tetris_board   DS_ADDR(uint8_t, 0x8ab4) /* 10 x 20, Index Spalte + Zeile*10 */
#define g_tetris_points  DS_ADDR(int8_t, 0x0d29)  /* Punkte je Stein (Index 1..7) */
#define g_tetris_frame   DS_I16(0x0d24)

/* Zelle (c, r) des 4x4-Rasters von Stein piece (1..7) in Drehung rot */
static int8_t piece_cell(int16_t piece, int16_t rot, int16_t c, int16_t r)
{
    return DS_I8(0x39c + (piece - 1) * 0x40 + rot * 0x10 + c * 4 + r);
}

static Bitmap *bank_sprite(SpriteBank *bank, int16_t idx)
{
    return (Bitmap *)((uint8_t *)bank + sprite_ofs(idx));
}

/* 1dd5:000f – ein Takt der Spielschleife; liefert die Tür-ID unter der Spielfigur oder 0 */
int16_t game_tick(void)
{
    int16_t door = 0;

    g_mouse_buttons = input_poll();
    DS_I16(0x7a2c) = 0;

    if (DS_ADDR(int16_t, 0x7b30)[g_player_idx * 15] != 0x45 &&
        (g_mouse_acc_x != 0 || g_mouse_acc_y != 0 || g_cursor_old_x == -1)) {
        while (g_mouse_acc_x != 0 || g_mouse_acc_y != 0 || g_cursor_old_x == -1) {
            /* Bewegung anwenden (normal geglättet, beim Ziehen direkt) */
            if (g_drag_bitmap == NULL) {
                ease_step(&g_mouse_acc_x, &g_cursor_x);
            } else {
                g_cursor_x += g_mouse_acc_x;
                g_mouse_acc_x = 0;
            }
            if (g_cursor_max_x < g_cursor_x)
                g_cursor_x = g_cursor_max_x;
            if (g_cursor_x < g_cursor_min_x)
                g_cursor_x = g_cursor_min_x;
            if (g_drag_bitmap == NULL) {
                ease_step(&g_mouse_acc_y, &g_cursor_y);
            } else {
                g_cursor_y += g_mouse_acc_y;
                g_mouse_acc_y = 0;
            }
            if (g_cursor_max_y < g_cursor_y)
                g_cursor_y = g_cursor_max_y;
            if (g_cursor_y < g_cursor_min_y)
                g_cursor_y = g_cursor_min_y;

            if (g_cursor_visible == 1) {
                scr_grab(g_cursor_grab, (int16_t)(g_cursor_x >> 3), g_cursor_y);
                int16_t ncol = (int16_t)(g_cursor_x >> 3), ocol = (int16_t)(g_cursor_old_x >> 3);
                if (ncol < ocol + 3 && ocol < ncol + 3 && g_cursor_y < g_cursor_old_y + 0xb &&
                    g_cursor_old_y < g_cursor_y + 0xb) {
                    int16_t dcol, scol, dy, sy, w, h;
                    if (ncol < ocol) {
                        dcol = (int16_t)(ocol - ncol);
                        scol = 0;
                        w = dcol;
                    } else {
                        dcol = 0;
                        scol = (int16_t)(ncol - ocol);
                        w = scol;
                    }
                    w = (int16_t)(3 - w);
                    if (g_cursor_y < g_cursor_old_y) {
                        dy = (int16_t)(g_cursor_old_y - g_cursor_y);
                        sy = 0;
                        h = dy;
                    } else {
                        dy = 0;
                        sy = (int16_t)(g_cursor_y - g_cursor_old_y);
                        h = sy;
                    }
                    h = (int16_t)(0xb - h);
                    blit_rect(g_cursor_bg_old, g_cursor_grab, (int16_t)(dcol << 3), dy, (int16_t)(scol << 3), sy,
                              (int16_t)(w * 8), h);
                    bmp_copy(g_cursor_grab, g_cursor_bg_new);
                    bmp_draw_masked(g_cursor_bitmaps[g_cursor_shape], g_cursor_grab, (int16_t)(g_cursor_x & 7), 0);
                    BlitRectArgs back = {g_cursor_grab, dcol, dy, w, h, g_cursor_bg_old, scol, sy};
                    bmp_blit_rect(&back);
                } else {
                    bmp_copy(g_cursor_grab, g_cursor_bg_new);
                    blit_sprite_masked(g_cursor_bitmaps[g_cursor_shape], g_cursor_grab, (int16_t)(g_cursor_x & 7),
                                       0, 0, 0, 0x10, 10);
                }
                scr_blit(g_cursor_bg_old, (int16_t)(g_cursor_old_x >> 3), g_cursor_old_y);
                scr_blit(g_cursor_grab, (int16_t)(g_cursor_x >> 3), g_cursor_y);
                bmp_copy(g_cursor_bg_new, g_cursor_bg_old);
                g_cursor_old_x = g_cursor_x;
                g_cursor_old_y = g_cursor_y;
            } else if (g_cursor_shape == 2) {
                /* gezogenes Objekt am Zeiger, auf den oberen Bildbereich begrenzt */
                g_drag_x = (int16_t)(g_cursor_x - g_drag_hot_x);
                g_drag_y = (int16_t)(g_cursor_y - g_drag_hot_y);
                if (g_drag_x < 0)
                    g_drag_x = 0;
                else if (g_drag_x + g_drag_w > 0x13f)
                    g_drag_x = (int16_t)(0x13f - g_drag_w);
                if (g_drag_y < 0)
                    g_drag_y = 0;
                else if (g_drag_y + g_drag_h > 0x78)
                    g_drag_y = (int16_t)(0x78 - g_drag_h);
                if (g_drag_x != g_cursor_old_x || g_drag_y != g_cursor_old_y) {
                    scr_grab(g_cursor_grab, (int16_t)(g_drag_x >> 3), g_drag_y);
                    int16_t ocol = (int16_t)(g_cursor_old_x >> 3), ncol = (int16_t)(g_drag_x >> 3);
                    int16_t span = (int16_t)((g_drag_w + 0xf) >> 3);
                    if (ncol < ocol + span && ocol < ncol + span && g_drag_y < g_cursor_old_y + g_drag_h &&
                        g_cursor_old_y < g_drag_y + g_drag_h && g_cursor_old_x != -1) {
                        int16_t dcol, scol, dy, sy, w, h;
                        if (g_drag_x < g_cursor_old_x) {
                            dcol = (int16_t)(ocol - ncol);
                            scol = 0;
                            w = (int16_t)(((g_drag_w + 7) >> 3) - dcol);
                        } else {
                            dcol = 0;
                            scol = (int16_t)(ncol - ocol);
                            w = (int16_t)(((g_drag_w + 7) >> 3) - scol);
                        }
                        w++;
                        if (g_drag_y < g_cursor_old_y) {
                            dy = (int16_t)(g_cursor_old_y - g_drag_y);
                            sy = 0;
                            h = dy;
                        } else {
                            dy = 0;
                            sy = (int16_t)(g_drag_y - g_cursor_old_y);
                            h = sy;
                        }
                        h = (int16_t)(g_drag_h - h);
                        blit_rect(g_cursor_bg_old, g_cursor_grab, (int16_t)(dcol << 3), dy, (int16_t)(scol << 3), sy,
                                  (int16_t)(w * 8), h);
                        blit_rect(g_cursor_grab, g_cursor_bg_new, 0, 0, 0, 0, (int16_t)(g_drag_w + 8), g_drag_h);
                        blit_sprite_masked(g_drag_bitmap, g_cursor_grab, (int16_t)(g_drag_x & 7), 0, 0, 0, g_drag_w,
                                           g_drag_h);
                        BlitRectArgs back = {g_cursor_grab, dcol, dy, w, h, g_cursor_bg_old, scol, sy};
                        bmp_blit_rect(&back);
                    } else {
                        blit_rect(g_cursor_grab, g_cursor_bg_new, 0, 0, 0, 0, (int16_t)(g_drag_w + 8), g_drag_h);
                        blit_sprite_masked(g_drag_bitmap, g_cursor_grab, (int16_t)(g_drag_x & 7), 0, 0, 0, g_drag_w,
                                           g_drag_h);
                    }
                    if (g_cursor_old_x != -1)
                        scr_blit(g_cursor_bg_old, (int16_t)(g_cursor_old_x >> 3), g_cursor_old_y);
                    scr_blit(g_cursor_grab, (int16_t)(g_drag_x >> 3), g_drag_y);
                    blit_rect(g_cursor_bg_new, g_cursor_bg_old, 0, 0, 0, 0, (int16_t)(g_drag_w + 8), g_drag_h);
                    g_cursor_old_x = g_drag_x;
                    g_cursor_old_y = g_drag_y;
                }
            }
        }
    }

    if (g_cur_room != -5) {
        if (g_intro_mode == 0)
            ai_tick();
        uint16_t interval = (uint16_t)(50 / (int32_t)g_game_speed_setting);
        if (g_cur_room == 0) {
            /* Gebäudeansicht */
            if ((uint16_t)(interval + g_clock_offset) <= ticks_get()) {
                g_clock_offset = (int16_t)ticks_get();
                if (DS_I16(0x7a3c) != 0)
                    random_events();
                DS_I16(0x7a24) = 0;
                building_animate();
                if (DS_I16(0x7a24) != 9999)
                    DS_I16(0x7a24) = 1;
                screen_update();
                DS_I16(0x7a24) = 0;
                tv_picture_tick();
                if (DS_ADDR(int16_t, 0x7b32)[g_player_idx * 15] == 0)
                    door = DS_U8(0x7c80 + DS_ADDR(int16_t, 0x7b24)[g_player_idx * 15] * 0x7a +
                                 DS_ADDR(int16_t, 0x7b34)[g_player_idx * 15]);
            }
        } else {
            if ((uint16_t)(interval + g_clock_offset) <= ticks_get()) {
                g_clock_offset = (int16_t)ticks_get();
                if (DS_I16(0x7a3c) != 0)
                    random_events();
                building_animate();
                room_animate(1);
                tv_picture_tick();
                if (g_cur_room == 1 && g_cur_dialog == 2 && DS_I16(0x79f8) == 0) {
                    /* Tetris auf dem Bürocomputer */
                    if (g_tetris_state < 0x2a)
                        g_tetris_state++;
                    g_tetris_frame++;
                    if (g_tetris_frame < g_tetris_level) {
                        if (g_tetris_input == 5)
                            tetris_tick(1);
                        else if (g_tetris_input != 0)
                            tetris_tick(0);
                    } else {
                        g_tetris_frame = 0;
                        tetris_tick(1);
                    }
                    minigame_panel(2);
                    screen_update();
                }
            }
            room_hover();
        }
    }

    if (g_intro_mode == 0 && g_cur_room != -5 && DS_I16(0x7a6a) == 0 && DS_I16(0x7a72) == 0)
        clock_update(0);
    return door;
}

/* 1dd5:0884 – obere Bildhälfte (DS:8e62) mit Mauszeiger auf den Bildschirm bringen */
void screen_update(void)
{
    vga_wait_vsync();
    int16_t cursor_in_scene = g_cur_room == 0 || (g_cur_room == 1 && g_cur_dialog == 2);
    if (cursor_in_scene) {
        if (g_cursor_y < 0x78 && g_cursor_visible == 1) {
            clip_reset();
            blit_rect(g_upper_screen, g_cursor_bg_new, 0, 0, (int16_t)(g_cursor_x & 0xfff8), g_cursor_y, 0x18, 0xb);
            draw_sprite(g_cursor_bitmaps[g_cursor_shape], g_cursor_x, g_cursor_y);
            DS_I16(0x0386) = 0;
        }
    } else {
        mouse_hide();
    }

    if (DS_I16(0x7a24) == 1) {
        /* nur den Gebäudeausschnitt (ggf. ohne Aufzugschacht) übertragen */
        ScrRectArgs a;
        a.src = g_upper_screen;
        a.sy = 4;
        a.h = 0x6d;
        a.dy = 4;
        if (DS_I16(0x8c2e) == DS_I16(0x0d26) && DS_I16(0x7a0c) != 1) {
            a.dx = 7;
            a.sx = 7;
            a.w = 0x1a;
        } else {
            a.dx = 0;
            a.sx = 0;
            a.w = 0x28;
        }
        scr_blit_rect(&a);
    } else {
        scr_blit(g_upper_screen, 0, 0);
    }

    if (cursor_in_scene) {
        if (g_cursor_y < 0x78 && g_cursor_visible == 1) {
            clip_reset();
            draw_bitmap_fast(g_cursor_bg_new, (int16_t)(g_cursor_x & 0xfff8), g_cursor_y);
            DS_I16(0x0386) = 0;
            int16_t h = g_cursor_y < 0x6e ? 0xb : (int16_t)(0x78 - g_cursor_y);
            blit_rect(g_upper_screen, g_cursor_bg_old, 0, 0, (int16_t)(g_cursor_x & 0xfff8), g_cursor_y, 0x18, h);
            bmp_copy(g_cursor_bg_old, g_cursor_grab);
        }
    } else {
        mouse_show();
    }
    DS_I16(0x0d26) = DS_I16(0x8c2e);
}

/* 1dd5:0a44 – untere Bildhälfte (DS:8eb2) mit Einblendeffekt anzeigen, Mauszeiger erhalten */
void lower_screen_present(void)
{
    if (g_cursor_y > 0x6d && g_cursor_visible == 1) {
        clip_set_small();
        blit_rect(g_lower_panel, g_cursor_bg_new, 0, 0, (int16_t)(g_cursor_x & 0xfff8), (int16_t)(g_cursor_y - 0x78),
                  0x18, 0xb);
        clip_set_upper();
        draw_sprite(g_cursor_bitmaps[g_cursor_shape], g_cursor_x, (int16_t)(g_cursor_y - 0x78));
    }
    screen_wipe_in(g_lower_panel, 0, 0x78, 0x140, 0x50, 0, 1);
    scr_blit(g_lower_panel, 0, 0x78);
    if (g_cursor_y > 0x6d && g_cursor_visible == 1) {
        clip_set_upper();
        bmp_copy_at(g_cursor_bg_new, g_lower_panel, (int16_t)(g_cursor_x & 0xfff8), (int16_t)(g_cursor_y - 0x78));
        int16_t h = 0xb;
        if (g_cursor_y < 0x78)
            h = (int16_t)(0x78 - g_cursor_y);
        clip_set_small();
        blit_rect(g_upper_screen, g_cursor_bg_old, 0, h, (int16_t)(g_cursor_x & 0xfff8),
                  (int16_t)(g_cursor_y - 0x78 - h), 0x18, h);
    }
}

/* 1dd5:0b74 – Mauszeiger auf den ganzen Bildschirm begrenzen */
void cursor_reset(void)
{
    g_cursor_min_x = 0;
    g_cursor_min_y = 0;
    g_cursor_max_x = 0x12f;
    g_cursor_max_y = 0xbd;
}

/* 1dd5:0b8d – einzeilige Texteingabe in buf (Enter = 0, Esc = -1) */
int16_t text_input(int16_t x, int16_t y, int16_t max_cols, char *buf)
{
    mouse_hide();
    Bitmap *save = bitmap_alloc((int16_t)((max_cols + 2) * 8), 8);
    blit_rect(g_upper_screen, save, 0, 0, (int16_t)(x & 0xfff8), y, (int16_t)((max_cols + 2) * 8), 8);
    int16_t key;
    while ((key = kbd_getkey()) != 0x1f && key != 0x14) {
        blit_rect(save, g_upper_screen, (int16_t)(x & 0xfff8), y, 0, 0, (int16_t)((max_cols + 2) * 8), 8);
        draw_string(x, y, buf, g_upper_screen, 0);
        draw_string((int16_t)(text_width(buf) + x), y, DS_ADDR(char, 0x0d33), g_upper_screen, 0); /* Eingabemarke */
        screen_update();
        if ((text_width(buf) >> 3) < max_cols - 1) {
            char ch = (char)key;
            if (key >= 0x26 && key <= 0x3f) {
                /* Buchstaben: am Wortanfang groß, sonst klein */
                ch = (char)(strlen(buf) == 0 ? ch + 0x1b : ch + 0x3b);
                buf[strlen(buf)] = ch;
            } else if (key >= 10 && key <= 0x13) {
                buf[strlen(buf)] = (char)(ch + '&'); /* Ziffern */
            } else if (key == 0x20) {
                buf[strlen(buf)] = ' ';
            }
        }
        if (key == 0x21 && strlen(buf) != 0)
            buf[strlen(buf) - 1] = 0; /* Rückschritt */
    }
    far_free(save);
    mouse_show();
    return key == 0x1f ? 0 : -1;
}

/* 1dd5:0d83 – Zeigerform setzen (shape != 0, auf Stapel legen) bzw. vorige wiederherstellen (0) */
void set_cursor_shape(int16_t shape)
{
    if (g_drag_bitmap != NULL)
        return;
    mouse_hide();
    if (shape == 0) {
        DS_I16(0x0d28)--;
        g_cursor_shape = DS_ADDR(int16_t, 0x8f02)[DS_I16(0x0d28)];
    } else {
        DS_ADDR(int16_t, 0x8f02)[DS_I16(0x0d28)] = g_cursor_shape;
        DS_I16(0x0d28)++;
        g_cursor_shape = shape;
    }
    mouse_show();
}

/* Kollision des Steins (piece, rot) bei (col, row) mit Rand oder Feld; Prüfung wie im Original */
static int16_t tetris_overlaps(int16_t rot, int16_t col, int16_t row)
{
    int16_t hit = 0;
    for (int16_t c = col; c < col + 4; c++)
        for (int16_t r = row; r < row + 4; r++)
            if (c >= 0 && c < 10 && r >= 0 && r < 20 && piece_cell(g_tetris_piece, rot, c - col, r - row) != 0 &&
                g_tetris_board[c + r * 10] != 0)
                hit = 1;
    return hit;
}

/* Drehung prüfen: Spalten links (<0) und rechts (>9) des Felds müssen leer sein */
static int16_t tetris_rotation_blocked(int16_t rot)
{
    int16_t hit = 0;
    if (g_tetris_col < 0)
        for (int16_t c = g_tetris_col; c < 0; c++)
            for (int16_t r = 0; r < 4; r++)
                if (piece_cell(g_tetris_piece, rot, c - g_tetris_col, r) != 0)
                    hit = 1;
    if (g_tetris_col > 6)
        for (int16_t c = (int16_t)(g_tetris_col + 3); c > 9; c--)
            for (int16_t r = 0; r < 4; r++)
                if (piece_cell(g_tetris_piece, rot, c - g_tetris_col, r) != 0)
                    hit = 1;
    return hit || tetris_overlaps(rot, g_tetris_col, g_tetris_row);
}

/* 1dd5:0dcb – ein Schritt des versteckten Tetris; drop = um eine Zeile fallen lassen */
void tetris_tick(int16_t drop)
{
    if (g_tetris_state != 0x2a)
        return;

    if (g_tetris_piece == 0) {
        /* neuen Stein holen; Startzeile so, dass die unterste belegte Reihe in Zeile 0 liegt */
        g_tetris_piece = g_tetris_next;
        g_tetris_rot = 0;
        g_tetris_next = rand_range(7, 1);
        int16_t r;
        for (r = 3; r >= 0; r--) {
            int16_t empty = 1;
            for (int16_t c = 0; c < 4; c++)
                if (piece_cell(g_tetris_piece, g_tetris_rot, c, r) != 0)
                    empty = 0;
            if (!empty)
                break;
        }
        g_tetris_row = (int16_t)-r;
        g_tetris_col = 3;
        g_tetris_old_col = 999;
    }

    /* zuletzt gezeichneten Stein aus dem Feld entfernen */
    if (g_tetris_old_col != 999) {
        for (int16_t c = g_tetris_old_col; c < g_tetris_old_col + 4; c++)
            for (int16_t r = g_tetris_old_row; r < g_tetris_old_row + 4; r++)
                if (piece_cell(g_tetris_old_pc, g_tetris_old_rot, c - g_tetris_old_col, r - g_tetris_old_row) != 0 &&
                    r >= 0 && r < 20 && c >= 0 && c < 10)
                    g_tetris_board[c + r * 10] = 0;
    }

    if (g_tetris_input == 1 || g_tetris_input == 2) {
        int16_t rot;
        if (g_tetris_input == 1)
            rot = g_tetris_rot < 1 ? 3 : (int16_t)(g_tetris_rot - 1);
        else
            rot = g_tetris_rot + 1 > 3 ? 0 : (int16_t)(g_tetris_rot + 1);
        if (!tetris_rotation_blocked(rot))
            g_tetris_rot = rot;
    } else if (g_tetris_input == 3 || g_tetris_input == 4) {
        int16_t col = (int16_t)(g_tetris_col + (g_tetris_input == 3 ? -1 : 1));
        int16_t hit = 0;
        if (g_tetris_input == 3 && col < 0) {
            for (int16_t c = col; c < 0; c++)
                for (int16_t r = 0; r < 4; r++)
                    if (piece_cell(g_tetris_piece, g_tetris_rot, c - col, r) != 0)
                        hit = 1;
        }
        if (g_tetris_input == 4 && col > 6) {
            for (int16_t c = (int16_t)(g_tetris_col + 4); c > 9; c--)
                for (int16_t r = 0; r < 4; r++)
                    if (piece_cell(g_tetris_piece, g_tetris_rot, c - col, r) != 0)
                        hit = 1;
        }
        int16_t last = (int16_t)(g_tetris_input == 3 ? g_tetris_col + 3 : g_tetris_col + 5);
        for (int16_t c = col; c < last; c++)
            for (int16_t r = g_tetris_row; r < g_tetris_row + 4; r++)
                if (c >= 0 && c < 10 && r >= 0 && r < 20 &&
                    piece_cell(g_tetris_piece, g_tetris_rot, c - col, r - g_tetris_row) != 0 &&
                    g_tetris_board[c + r * 10] != 0)
                    hit = 1;
        if (!hit)
            g_tetris_col = col;
    }
    if (g_tetris_input != 5)
        g_tetris_input = 0;

    /* gelandet? unter dem Feldboden belegte Reihen oder Überlappung */
    int16_t landed = 0;
    if (g_tetris_row > 0x10) {
        for (int16_t r = g_tetris_row; r < g_tetris_row + 4; r++) {
            if (r > 0x13) {
                int16_t empty = 1;
                for (int16_t c = g_tetris_col; c < g_tetris_col + 4; c++)
                    if (piece_cell(g_tetris_piece, g_tetris_rot, c - g_tetris_col, r - g_tetris_row) != 0)
                        empty = 0;
                if (!empty)
                    landed = 1;
            }
        }
    }
    if (tetris_overlaps(g_tetris_rot, g_tetris_col, g_tetris_row))
        landed = 1;

    if (landed) {
        g_tetris_input = 0;
        g_tetris_score = (int16_t)(g_tetris_score + g_tetris_points[g_tetris_piece] * (9 - g_tetris_level));
        if (g_tetris_score > 9999) {
            g_tetris_state = 0x2b;
            g_tetris_score = 9999;
        }
        if (g_tetris_high < g_tetris_score)
            g_tetris_high = g_tetris_score;
        g_tetris_piece = 0;
        /* an der zuletzt gültigen Position einrasten */
        for (int16_t c = g_tetris_old_col; c < g_tetris_old_col + 4; c++)
            for (int16_t r = g_tetris_old_row; r < g_tetris_old_row + 4; r++) {
                int8_t v = piece_cell(g_tetris_old_pc, g_tetris_old_rot, c - g_tetris_old_col, r - g_tetris_old_row);
                if (v != 0 && r >= 0 && r < 20 && c >= 0 && c < 10)
                    g_tetris_board[c + r * 10] = (uint8_t)v;
            }
        /* volle Reihen entfernen */
        for (int16_t r = 0x13; r >= 0; r--) {
            int8_t free_cells = 10;
            for (int16_t c = 0; c < 10; c++)
                if (g_tetris_board[c + r * 10] != 0)
                    free_cells--;
            if (free_cells == 0) {
                for (int16_t rr = r; rr > 0; rr--)
                    for (int16_t c = 0; c < 10; c++)
                        g_tetris_board[c + rr * 10] = g_tetris_board[c + (rr - 1) * 10];
                for (int16_t c = 0; c < 10; c++)
                    g_tetris_board[c] = 0;
                r++;
                g_tetris_lines++;
                if (g_tetris_lines % 10 == 0 && g_tetris_level > 1)
                    g_tetris_level--;
            }
            int16_t top_used = 0;
            for (int16_t c = 0; c < 10; c++)
                if (g_tetris_board[c] != 0)
                    top_used = 1;
            if (top_used)
                g_tetris_state = 0x2b; /* Spiel vorbei */
        }
    } else {
        for (int16_t c = g_tetris_col; c < g_tetris_col + 4; c++)
            for (int16_t r = g_tetris_row; r < g_tetris_row + 4; r++) {
                int8_t v = piece_cell(g_tetris_piece, g_tetris_rot, c - g_tetris_col, r - g_tetris_row);
                if (v != 0 && r >= 0 && r < 20 && c >= 0 && c < 10)
                    g_tetris_board[c + r * 10] = (uint8_t)v;
            }
        g_tetris_old_pc = g_tetris_piece;
        g_tetris_old_rot = g_tetris_rot;
        g_tetris_old_col = g_tetris_col;
        g_tetris_old_row = g_tetris_row;
        g_tetris_row = (int16_t)(g_tetris_row + drop);
    }
}

/* 1dd5:169a – Rahmen in die obere Bildhälfte zeichnen */
void draw_tile_box(int16_t xcol, int16_t y, int16_t wcols, int16_t h, int16_t style)
{
    draw_tile_box_to(xcol, y, wcols, h, style, g_upper_screen);
}

/* 1dd5:16bd – Rahmen aus 8x8-Kacheln: Ecken aus Bank 2 (maskiert), Kanten aus Bank 7 */
void draw_tile_box_to(int16_t xcol, int16_t y, int16_t wcols, int16_t h, int16_t style, Bitmap *dst)
{
    int16_t corner = (int16_t)(style * 3 + 0x13);
    /* ORIGINAL-BUG: Ist h kein Vielfaches von 8 und bleibt nach dem ersten Abzug ein negativer
     * Rest ungleich -1, endet die Schleife nie (die Aufrufer vermeiden das). */
    h -= 8;
    while (h != -1) {
        bmp_draw_masked(bank_sprite(DS_PTR(SpriteBank, 0x8c3c), (int16_t)(corner + 2)), dst,
                        (int16_t)(xcol * 8 + 2), y);
        bmp_draw_masked(bank_sprite(DS_PTR(SpriteBank, 0x8c3c), corner), dst, (int16_t)(xcol * 8 + wcols * 8), y);
        for (int16_t i = 1; i < wcols; i++)
            bmp_copy_at(bank_sprite(DS_PTR(SpriteBank, 0x8b80), (int16_t)(style + 0xe)), dst,
                        (int16_t)(xcol * 8 + i * 8), y);
        if (h >= 8) {
            y += 8;
            h -= 8;
        } else if (h > 0) {
            y += h;
            h = 0;
        } else if (h == 0) {
            h = -1;
        }
    }
}
