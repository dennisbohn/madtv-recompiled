/* Segment 27bb – Gebäudeansicht: Etagen und Türen, Aufzug, Spielfiguren und Passanten,
 * Türschilder, Fassade und die Sequenz beim Ausscheiden eines Spielers.
 *
 * Das Gebäude wird in die Bitmap DS:8eba (208 x 150) gezeichnet, 5 Etagen à 30 Pixel; der
 * sichtbare Ausschnitt folgt der eigenen Figur (Scrollwert DS:8c2e). Etagen haben je 0x7a Byte
 * Türdaten ab DS:7c62 (x-Positionen, Typen, Schilder, Besitzer ...). */
#include "game.h"

#define g_text(id)        (DS_ADDR(char *, 0x8370)[id])
#define g_building        DS_PTR(Bitmap, 0x8eba)
#define g_upper_screen    DS_PTR(Bitmap, 0x8e62)
#define g_scroll          DS_I16(0x8c2e) /* Scrollposition des Gebäudes */
#define g_top_floor       DS_I16(0x8a3c) /* oberste sichtbare Etage */
#define g_elev_floor      DS_I16(0x8bba) /* Etage der Aufzugkabine */
#define g_elev_offset     DS_I16(0x0128) /* Pixelversatz der Kabine zwischen Etagen */
#define g_elev_door       DS_I16(0x012a) /* Aufzugtür: 0 zu, 1 halb, 2 offen */
#define g_elev_speed      DS_I16(0x7a2e)
#define g_elev_rider      DS_I16(0x7a1a) /* Figur, die gerade zusteigt */
#define g_force_redraw    DS_I16(0x8c4a)
#define g_clip_on         DS_I16(0x0386)
#define g_clip_top        DS_I16(0x038a)
#define g_clip_bottom     DS_I16(0x038e)

/* Etagen-/Türdaten (0x7a Byte je Etage ab DS:7c62) */
#define FLOOR_B(floor, ofs) DS_U8(0x7c62 + (floor) * 0x7a + (ofs))
#define FLOOR_W(floor, ofs) DS_I16(0x7c62 + (floor) * 0x7a + (ofs))
#define DOOR_X(f, d)      FLOOR_B(f, d)          /* x-Position */
#define DOOR_YOFS(f, d)   FLOOR_B(f, 0x0a + (d))
#define DOOR_TYPE(f, d)   FLOOR_B(f, 0x14 + (d)) /* 1..3 Tür, 6/10 Aufzug/Treppe, 12 Laterne ... */
#define DOOR_KIND(f, d)   FLOOR_B(f, 0x1e + (d)) /* Art des Schilds */
#define DOOR_TEXT(f, d)   FLOOR_B(f, 0x28 + (d))
#define DOOR_ALT(f, d)    FLOOR_B(f, 0x3c + (d))
#define DOOR_OWNER(f, d)  FLOOR_B(f, 0x46 + (d))
#define DOOR_W(f, d)      FLOOR_B(f, 0x50 + (d))
#define DOOR_H(f, d)      FLOOR_B(f, 0x5a + (d))
#define DOOR_ICON(f, d)   FLOOR_B(f, 0x64 + (d))
#define DOOR_ROOM(f, d)   FLOOR_B(f, 0x1e + (d))
#define DOOR_OPEN(f, d)   FLOOR_B(f, 0x3c + (d))
#define FLOOR_WALL(f)     FLOOR_W(f, 0x78)

/* Figuren (6 x 30 Byte ab DS:7b22) */
#define FIG(i, ofs)   DS_I16(0x7b22 + (i) * 0x1e + (ofs))
#define FIG_X(i)      FIG(i, 0x00)
#define FIG_FLOOR(i)  FIG(i, 0x02)
#define FIG_TX(i)     FIG(i, 0x04) /* Ziel-x */
#define FIG_TFLOOR(i) FIG(i, 0x06) /* Zieletage */
#define FIG_PEND(i)   FIG(i, 0x08) /* vorgemerktes Ziel-x */
#define FIG_ANIM(i)   FIG(i, 0x0c)
#define FIG_STATE(i)  FIG(i, 0x0e) /* 0 steht, 1 geht, 2 steigt ein, 3 fährt, 4 steigt aus, 5 wartet */
#define FIG_MODE(i)   FIG(i, 0x10) /* 0 im Raum, 1 im Gang, 2/3 Tür öffnet/schließt */
#define FIG_DOOR(i)   FIG(i, 0x12) /* Zieltür (-1 keine, -2 Aufzugtafel) */
#define FIG_CDOOR(i)  FIG(i, 0x14) /* aktuelle Tür */
#define FIG_ELEV(i)   FIG(i, 0x16) /* Aufzugphase 0..4 */
#define FIG_AI(i)     FIG(i, 0x18) /* 0/1 Passant, 2 Spieler */
#define FIG_SPEED(i)  FIG(i, 0x1a)
#define FIG_DOPEN(i)  FIG(i, 0x1c) /* Tür von der Figur geöffnet */

#define PLAYER_OUT(p) DS_U8(0x8bd0 + (p) * 0x17)

static Bitmap *bank_bmp(SpriteBank *bank, int16_t ofs)
{
    return (Bitmap *)((uint8_t *)bank + ofs);
}

/* 27bb:000b – sichtbare Etagen in die Gebäudebitmap zeichnen (beim Scrollen um eine Etage wird
 * der Rest verschoben und nur die neue Etage gezeichnet); Tag/Nacht-Wechsel der Grafiken */
void building_draw(int16_t scroll_y)
{
    SpriteBank *tiles = DS_PTR(SpriteBank, 0x8b80);
    int16_t night = hour_index() >= 4;
    if ((!night && DS_I16(0x117c) != 1) || (night && DS_I16(0x117c) != 2)) {
        if (DS_PTR(SpriteBank, 0x00f4))
            far_free_ptr(DS_ADDR(void *, 0x00f4));
        DS_PTR(SpriteBank, 0x00f4) = font_get(DS_I16(0x117c) != 2, 0);
        DS_I16(0x117c) = (int16_t)(3 - DS_I16(0x117c));
        g_force_redraw = 1;
        DS_I16(0x7a24) = 9999;
        building_walls_draw(DS_I16(0x117c));
    } else if (g_force_redraw == 1) {
        building_walls_draw(DS_I16(0x117c));
    }

    if (scroll_y / 0x1e + 5 != DS_I16(0x11a2) || g_force_redraw == 1) {
        int16_t first, last;
        DS_I16(0x7a26) = 1;
        g_top_floor = (int16_t)(scroll_y / 0x1e + 5);
        DS_I16(0x0388) = 0;
        DS_I16(0x038c) = 0xd0;
        if (DS_I16(0x11a2) - 1 == g_top_floor && g_force_redraw == 0) {
            /* eine Etage nach unten: Inhalt nach oben schieben, unterste neu */
            g_clip_on = 0;
            for (int16_t y = 0x1e; y < 0x96; y += 0x1e)
                blit_rect(g_building, g_building, 0, (int16_t)(y - 0x1e), 0, y, 0xd0, 0x1e);
            g_clip_top = 0x78;
            g_clip_bottom = 0x96;
            first = last = (int16_t)(g_top_floor - 4);
        } else {
            if (DS_I16(0x11a2) + 1 == g_top_floor && g_force_redraw == 0) {
                g_clip_on = 0;
                for (int16_t y = 0x5a; y > -0x1e; y -= 0x1e)
                    blit_rect(g_building, g_building, 0, (int16_t)(y + 0x1e), 0, y, 0xd0, 0x1e);
                g_clip_bottom = 0x1e;
                first = g_top_floor;
            } else {
                g_clip_bottom = 0x96;
                first = (int16_t)(scroll_y / 0x1e + 1);
            }
            g_clip_top = 0;
            last = g_top_floor;
        }
        g_clip_on = 1;
        DS_I16(0x11a2) = g_top_floor;

        if (last > 0xd) {
            /* Himmel über dem Dach */
            int16_t sky = (int16_t)((g_top_floor - 0xd) * 0x1e);
            if (sky > g_clip_bottom)
                sky = g_clip_bottom;
            int16_t tile = hour_index() < 4 ? 1 : 9;
            for (int16_t x = 0; x < 0xd0; x += 8)
                for (int16_t y = 0; y < sky; y += 8) {
                    if (g_clip_bottom < y + 7)
                        blit_rect(bank_bmp(tiles, sprite_ofs(tile)), g_building, x, y, 0, 0, 8, (int16_t)(g_clip_bottom - y));
                    else
                        bmp_copy_at(bank_bmp(tiles, sprite_ofs(tile)), g_building, x, y);
                }
            bmp_draw_masked(&DS_PTR(SpriteBank, 0x00fc)->bmp[0], g_building, 0,
                            (int16_t)((g_top_floor - 0xd) * 0x1e - 0x25));
        }
        /* Stützpfeiler links und rechts */
        for (int16_t i = 1; i < 0x1f; i++) {
            int16_t y = (int16_t)(i * -0xd + 0x1c2 + (0xf - g_top_floor) * -0x1e);
            if (y > -0xd && y < 0x97) {
                bmp_draw(bank_bmp(tiles, 0x32), g_building, 0, y);
                bmp_draw(bank_bmp(tiles, 0x32), g_building, 0xc0, y);
            }
        }
        for (int16_t floor = first; floor <= last; floor++) {
            if (floor < 0xe) {
                int16_t base = (int16_t)(((floor - g_top_floor) + 4) * -0x1e);
                int16_t y = (int16_t)(base + 0x93);
                if (y > -3 && y < 0x97)
                    for (int16_t x = 0x10; x < 0xc0; x += 8)
                        bmp_copy_at(bank_bmp(tiles, 0x3a), g_building, x, y); /* Boden */
                /* Tapete */
                Bitmap *wall = bank_bmp(tiles, sprite_ofs((int16_t)(FLOOR_WALL(floor) * 2 - 1)));
                Bitmap *edge = bank_bmp(tiles, sprite_ofs((int16_t)(FLOOR_WALL(floor) << 1)));
                for (int16_t x = 0x10; x < 0xc0; x += 8) {
                    for (int16_t yy = (int16_t)(base + 0x7b); yy < base + 0x93; yy += 8) {
                        bmp_copy_at(wall, g_building, x, yy);
                        if (x == 0x10)
                            bmp_copy_at(edge, g_building, 0x10, yy);
                    }
                    blit_rect(edge, g_building, x, (int16_t)(base + 0x78), 0, 0, 8, 5);
                }
                /* Türen und Objekte */
                for (int16_t d = 0; d < 0xb && DOOR_TYPE(floor, d) != 0; d++) {
                    int16_t yadd = 0;
                    int16_t type = (int16_t)(DOOR_TYPE(floor, d) - 1);
                    Bitmap *spr = bank_bmp(DS_PTR(SpriteBank, 0x8c36), sprite_ofs(DS_I8(0x117e + type * 3)));
                    int16_t w = DS_I8(0x117f + type * 3), h = DS_I8(0x1180 + type * 3);
                    if (type < 3 && DOOR_ALT(floor, d) != 0)
                        spr = bank_bmp(DS_PTR(SpriteBank, 0x8c36), sprite_ofs((int16_t)(DS_I8(0x117e + type * 3) + 1)));
                    if (type == 0xb) { /* Laterne: tagsüber aus */
                        if (hour_index() < 4) {
                            yadd = 3;
                            spr = bank_bmp(DS_PTR(SpriteBank, 0x8362), 0x2a);
                            w = 8;
                            h = 4;
                        } else {
                            spr = bank_bmp(DS_PTR(SpriteBank, 0x8362), 0x32);
                        }
                    }
                    int16_t yofs = (int16_t)(DOOR_YOFS(floor, d) + yadd);
                    if (yofs == 0)
                        yofs = (int16_t)(0x1c - h);
                    blit_sprite_masked(spr, g_building, (int16_t)(DOOR_X(floor, d) - 0x38),
                                       (int16_t)(yofs + base + 0x78), 0, 0, w, h);
                    if (DOOR_OWNER(floor, d) != 0 && PLAYER_OUT(DOOR_OWNER(floor, d)) == 0)
                        bmp_draw_masked(bank_bmp(DS_PTR(SpriteBank, 0x8362), sprite_ofs(DOOR_OWNER(floor, d))), g_building,
                                        (int16_t)(DOOR_X(floor, d) - 0x26), (int16_t)(base + 0x7c));
                    if (DOOR_KIND(floor, d) == 5)
                        bmp_draw_masked(bank_bmp(DS_PTR(SpriteBank, 0x8362), 0x1a), g_building,
                                        (int16_t)(DOOR_X(floor, d) - 0x3c), (int16_t)(base + 0x7c));
                }
                /* Etagennummer */
                bmp_draw_masked(bank_bmp(DS_PTR(SpriteBank, 0x7b12), 0x22), g_building, 2, (int16_t)(base + 0x80));
                char *s = fmt(DS_ADDR(char, 0x11c4), floor);
                text_box_centered(8, (int16_t)(base + 0x7a), s, 0, 0, g_building, -1);
                far_free(s);
            }
            if (floor < 0xe)
                building_elevator_draw(floor);
        }
        DS_I16(0x7a26) = 0;
    }
    g_clip_top = 0;
    g_clip_bottom = 0x96;
    for (int16_t floor = (int16_t)(g_top_floor - 4); floor <= g_top_floor; floor++)
        if (floor < 0xe)
            building_elevator_draw(floor);
}

/* 27bb:0718 – sichtbaren Ausschnitt der Gebäudebitmap in die Spielszene */
void building_view_present(void)
{
    blit_rect(g_building, g_upper_screen, 0x38, 4, 0, (int16_t)(g_top_floor * 0x1e - (g_scroll + 0x6d)), 0xd0, 0x6d);
}

/* 27bb:0756 – Aufzugschacht, Kabine (mit Fahrgästen) und Aufzugtür einer Etage */
void building_elevator_draw(int16_t floor)
{
    int16_t cab_y = 0, cab_sy = 0, cab_h = 0;
    if (floor > g_top_floor || floor < g_top_floor - 4)
        return;
    int16_t base = (int16_t)(((floor - g_top_floor) + 4) * -0x1e);
    SpriteBank *b7b12 = DS_PTR(SpriteBank, 0x7b12), *b8c36 = DS_PTR(SpriteBank, 0x8c36);
    bmp_draw_masked(bank_bmp(b7b12, 0x72), g_building, 0x61, (int16_t)(base + 0x7a));
    bmp_draw_masked(bank_bmp(b7b12, 0x7a), g_building, (int16_t)(g_elev_floor + 0x60), (int16_t)(base + 0x7a));
    if (floor == g_elev_floor) {
        if (g_elev_offset >= 1) {
            cab_y = (int16_t)(base + 0x78 + g_elev_offset + 7);
            cab_sy = 0;
            cab_h = (int16_t)(0x14 - g_elev_offset);
        } else if (g_elev_offset < 0 && g_elev_offset > -0x15) {
            cab_y = (int16_t)(base + 0x7f);
            cab_sy = (int16_t)-g_elev_offset;
            cab_h = (int16_t)(g_elev_offset + 0x14);
        } else if (g_elev_offset == 0) {
            cab_y = (int16_t)(base + 0x7f);
            cab_sy = 0;
            cab_h = 0x14;
        }
    } else if (g_elev_floor + 1 == floor && g_elev_offset < 0) {
        cab_y = (int16_t)(g_elev_offset + base + 0x78 + 0x25);
        cab_sy = 0;
        cab_h = (int16_t)(0x14 - (g_elev_offset + 0x1e));
    } else if (g_elev_floor - 1 == floor && g_elev_offset > 10) {
        cab_y = (int16_t)(base + 0x7f);
        cab_sy = (int16_t)(0x1e - g_elev_offset);
        cab_h = (int16_t)(0x14 - (0x1e - g_elev_offset));
    }
    if (cab_h < 1) {
        bmp_draw_masked(bank_bmp(b8c36, 0xea), g_building, 0x5d, (int16_t)(base + 0x7c));
    } else {
        bmp_draw_masked(bank_bmp(b8c36, 0x72), g_building, 0x5d, (int16_t)(base + 0x7c));
        blit_rect(bank_bmp(DS_PTR(SpriteBank, 0x8b80), 0x4a), g_building, 0x60, cab_y, 0, cab_sy, 0x10, cab_h);
        for (int16_t i = 5; i >= 0; i--)
            if (FIG_STATE(i) == 3)
                blit_sprite_masked(bank_bmp(DS_PTR(SpriteBank, 0x00f8), figure_sprite_ofs(i, 7)), g_building, 0x61,
                                   (int16_t)(cab_y + 1), 0, cab_sy, 0x10, (int16_t)(cab_h - 1));
    }
    if (floor != g_elev_floor || g_elev_door != 2) {
        if (floor == g_elev_floor && g_elev_door == 1) {
            bmp_draw_masked(bank_bmp(b8c36, 0x82), g_building, 0x60, (int16_t)(base + 0x7f));
            bmp_draw_masked(bank_bmp(b7b12, 0x1a), g_building, 0x6a, (int16_t)(base + 0x7f));
        } else if (cab_h > 0) {
            bmp_draw_masked(bank_bmp(b8c36, 0x8a), g_building, 0x60, (int16_t)(base + 0x7f));
        }
    }
}

/* 27bb:0a3b – Seitenteile neben dem Gebäude (bei Scroll-Änderung) neu zeichnen */
void building_sides_draw(void)
{
    if (g_scroll != DS_I16(0x11a4) || DS_I16(0x7a0c) == 1 || g_force_redraw != 0) {
        SpriteBank *b00f4 = DS_PTR(SpriteBank, 0x00f4), *b7b12 = DS_PTR(SpriteBank, 0x7b12);
        g_force_redraw = 0;
        int16_t s = (int16_t)(g_scroll / g_elev_speed);
        blit_rect(bank_bmp(b00f4, 10), g_upper_screen, 0, (int16_t)(s + 4), 0, 0, 0x38, (int16_t)(0x6d - s));
        blit_rect(bank_bmp(b00f4, 2), g_upper_screen, 0x108, (int16_t)(s + 4), 0, 0, 0x38, (int16_t)(0x6d - s));
        if (g_scroll < 0x17)
            blit_sprite_masked(bank_bmp(b7b12, 10), g_upper_screen, 0x108, (int16_t)(g_scroll + 0x5b), 0, 0, 0x38,
                               (int16_t)(0x16 - g_scroll));
        if (g_scroll < 0xc)
            blit_sprite_masked(bank_bmp(b7b12, 0x12), g_upper_screen, 4, (int16_t)(g_scroll + 0x66), 0, 0, 0x38,
                               (int16_t)(0xb - g_scroll));
        if (g_scroll < 0x22)
            blit_sprite_masked(bank_bmp(DS_PTR(SpriteBank, 0x8c36), 0xd2), g_upper_screen, 0x1d,
                               (int16_t)(g_scroll + 0x50), 0, 0, 0x20, (int16_t)(0x21 - g_scroll));
    }
    DS_I16(0x11a4) = g_scroll;
}

/* 27bb:0b9e – ein Animationsschritt im Gebäude (Aufzug, Passanten, Bild, Figuren, Hinweise) */
void building_animate(void)
{
    if (DS_I16(0x012c) == 2)
        return;
    if (g_cur_room == 0)
        mouse_update();
    elevator_update();
    if (g_cur_room == 0)
        mouse_update();
    npc_update();
    if (g_cur_room == 0)
        mouse_update();
    if (g_cur_room == 0) {
        mouse_update();
        building_draw(g_scroll);
        mouse_update();
        building_sides_draw();
        mouse_update();
        building_view_present();
        mouse_update();
    }
    figures_update();
    if (g_cur_room == 0)
        mouse_update();
    if (g_cur_room == 0) {
        building_hover_draw();
        mouse_update();
        if (DS_I16(0x7a0c) != 0) {
            /* Absperrung beim Gang zum Chef */
            for (int16_t x = 8; x < 0x134; x += 0x14)
                draw_sprite(bank_bmp(DS_PTR(SpriteBank, 0x835e), 10), x, 0x69);
            draw_sprite(bank_bmp(DS_PTR(SpriteBank, 0x835e), 2), 0x106, 0x5c);
        }
        if (DS_I16(0x7a42) > 999) {
            g_clip_on = 1;
            g_clip_top = 4;
            g_clip_bottom = 0x70;
            draw_sprite(bank_bmp(DS_PTR(SpriteBank, 0x8362), 0x8a), (int16_t)(DOOR_X(DS_I16(0x7a46), DS_I16(0x7a44)) - 5),
                        (int16_t)((DS_I16(0x7a46) * -0x1e - (0x155 - g_scroll)) + 0x1c6));
            g_clip_on = 0;
        }
    }
}

/* Animationsbild beim Gehen: Schritt-Zyklus aus Position und Geschwindigkeit */
static int16_t walk_frame(int16_t fig, int16_t base_frame, int16_t mirror)
{
    int16_t speed = FIG_SPEED(fig);
    int16_t f = (int16_t)((FIG_X(fig) % (speed * 3)) / speed + base_frame);
    if (f > mirror / 2)
        f = (int16_t)(mirror - f);
    return f;
}

/* 27bb:0ce1 – Figuren bewegen und zeichnen (zuerst die im Aufzug/an Türen, dann die gehenden) */
void figures_update(void)
{
    static int16_t frame; /* Original: Register DI bleibt zwischen den Figuren erhalten */
    int16_t order[6], count = 0;
    for (int16_t i = 0; i < 6; i++) {
        DS_I16(0x8f8e) = FIG_STATE(i);
        if (FIG_STATE(i) > 1 && FIG_STATE(i) != 3)
            order[count++] = i;
    }
    for (int16_t i = 0; i < 6; i++)
        if (FIG_STATE(i) < 2)
            order[count++] = i;

    g_clip_on = 1;
    g_clip_top = 4;
    g_clip_bottom = 0x71;
    for (int16_t n = 0; n < count; n++) {
        int16_t f = order[n];
        int16_t dtype = 0;
        if (FIG_MODE(f) != 1) {
            dtype = FLOOR_B(FIG_FLOOR(f), 0x14 + FIG_DOOR(f));
            DS_I16(0x8f8e) = dtype;
        }
        if (!(FIG_MODE(f) == 1 || dtype == 6 || dtype == 10))
            continue;
        int16_t speed = FIG_SPEED(f);
        if (FIG_X(f) % speed > 0)
            FIG_X(f) = (int16_t)(FIG_X(f) / speed * speed);
        if (FIG_TX(f) % speed > 0)
            FIG_TX(f) = (int16_t)(FIG_TX(f) / speed * speed);

        int16_t dir = 0;
        if (FIG_STATE(f) == 1) {
            if (FIG_TX(f) == FIG_X(f)) {
                dir = 0;
                FIG_STATE(f) = 0;
            } else {
                dir = FIG_TX(f) < FIG_X(f) ? -1 : 1;
            }
        }

        /* Blickrichtung/Haltung umstellen, bevor gegangen wird */
        int16_t *anim = &FIG_ANIM(f);
        if (dir < 0 && *anim >= 0) {
            *anim = (int16_t)(*anim - (*anim == 2 ? 3 : 1));
            dir = 0;
        } else if (dir > 0 && *anim < 1) {
            (*anim)++;
            dir = 0;
        } else if (dir > 0 && *anim == 2) {
            *anim = 1;
            dir = 0;
        } else if (FIG_STATE(f) == 2 || FIG_STATE(f) == 5) {
            if (*anim == 0)
                *anim = 1;
            else if (*anim < 2)
                *anim = 2;
            else
                (*anim)++;
            dir = 0;
        } else if (FIG_STATE(f) == 4) {
            if (*anim < 2)
                *anim = 2;
            else
                (*anim)++;
        }
        FIG_X(f) = (int16_t)(FIG_X(f) + speed * dir);

        if (FIG_STATE(f) == 3)
            continue;
        if (FIG_STATE(f) < 2) {
            static const int8_t tbl[4] = {-2, 7, -1, 9}; /* -1: Schritt 1-3, -2: Schritt 4-6 */
            uint16_t idx = (uint16_t)(*anim + 1);
            if (idx < 4)
                frame = tbl[idx] == -1 ? walk_frame(f, 1, 6) : tbl[idx] == -2 ? walk_frame(f, 4, 12) : tbl[idx];
        } else if (FIG_STATE(f) == 2 || FIG_STATE(f) == 5) {
            static const int8_t tbl[7] = {4, 9, 10, 9, 10, 4, 7};
            uint16_t idx = (uint16_t)(*anim - 1);
            if (idx < 7)
                frame = tbl[idx];
        } else if (FIG_STATE(f) == 4) {
            static const int8_t tbl[4] = {8, 7, 8, 7};
            uint16_t idx = (uint16_t)(*anim - 2);
            if (idx < 4)
                frame = tbl[idx];
        }
        DS_I16(0x8f8e) = FIG_FLOOR(f);
        if (FIG_FLOOR(f) <= g_top_floor && g_top_floor - 4 <= FIG_FLOOR(f) && g_cur_room == 0)
            figure_draw(f, frame);
        int16_t a = *anim, st = FIG_STATE(f);
        if ((a > 6 && st == 2) || (a > 4 && st == 4) || (a > 2 && st == 5)) {
            *anim = (int16_t)(st == 5 ? 2 : 0);
            FIG_STATE(f) = 0;
        }
    }
    g_clip_on = 0;
}

/* zufällige Tür auf einer Etage wählen (Seed wie im Original) */
static int16_t random_door(int16_t floor, int16_t seed)
{
    int16_t door = (int16_t)(rand_range(floor_door_count(floor), seed) - 1);
    floor_door_prev(floor, &door, -1);
    return door;
}

/* 27bb:11fb – Figuren steuern: Passanten wählen Ziele, Türen auf/zu, Aufzug ein-/aussteigen */
void npc_update(void)
{
    for (int16_t i = 0; i <= 5; i++) {
        int16_t idle = FIG_AI(i) == 2 && FIG_MODE(i) == 0 && FIG_TX(i) == FIG_X(i) && FIG_FLOOR(i) == FIG_TFLOOR(i) &&
                       FIG_PEND(i) == 0;
        if (idle || !(i > 2 || PLAYER_OUT(i + 1) == 0))
            continue;

        if (FIG_MODE(i) == 0) {
            if (FIG_AI(i) == 1) {
                /* Passant: andere Etage */
                int16_t floor = (int16_t)(rand_range(0xc, i) + 1);
                int16_t door = (int16_t)(rand_range(floor_door_count(floor), i) - 1);
                floor_door_prev(floor, &door, -1);
                FIG_FLOOR(i) = floor;
                FIG_X(i) = (int16_t)(DOOR_X(floor, door) + 4);
                FIG_CDOOR(i) = door;
                do
                    floor = (int16_t)(rand_range(0xc, floor) + 1);
                while (floor == FIG_FLOOR(i));
                door = random_door(floor, floor);
                FIG_DOOR(i) = door;
                FIG_TFLOOR(i) = floor;
            } else if (FIG_AI(i) == 0) {
                /* Passant: gleiche Etage */
                int16_t floor = (int16_t)(rand_range(0xb, i) + 1);
                int16_t door = (int16_t)(rand_range(floor_door_count(floor), i) - 1);
                floor_door_prev(floor, &door, -1);
                FIG_FLOOR(i) = floor;
                FIG_X(i) = (int16_t)(DOOR_X(floor, door) + 4);
                FIG_CDOOR(i) = door;
                door = random_door(floor, floor);
                FIG_DOOR(i) = door;
                FIG_TFLOOR(i) = floor;
            }
            if (FIG_TFLOOR(i) == FIG_FLOOR(i))
                FIG_TX(i) = (int16_t)(DOOR_X(FIG_TFLOOR(i), FIG_DOOR(i)) + 4);
            else
                FIG_TX(i) = 0x99; /* zum Aufzug */
            int16_t t = DOOR_TYPE(FIG_FLOOR(i), FIG_CDOOR(i));
            DS_I16(0x8f8e) = t;
            if (t == 6 || t == 10) {
                FIG_MODE(i) = 1;
            } else {
                DOOR_OPEN(FIG_FLOOR(i), FIG_CDOOR(i)) = 1;
                door_draw(FIG_FLOOR(i), FIG_CDOOR(i), 1);
                FIG_MODE(i) = 2;
            }
        } else if (FIG_MODE(i) == 2) {
            FIG_MODE(i) = 3;
        } else if (FIG_MODE(i) == 3) {
            int16_t t = DOOR_TYPE(FIG_FLOOR(i), FIG_CDOOR(i));
            DS_I16(0x8f8e) = t;
            if (t != 6 && t != 10) {
                DOOR_OPEN(FIG_FLOOR(i), FIG_CDOOR(i)) = 0;
                door_draw(FIG_FLOOR(i), FIG_CDOOR(i), 2);
            }
            FIG_MODE(i) = 1;
        } else if (FIG_MODE(i) == 1 && FIG_STATE(i) == 0 && FIG_DOOR(i) == -1 && FIG_PEND(i) != 0 &&
                   FIG_FLOOR(i) == FIG_TFLOOR(i)) {
            FIG_STATE(i) = 1;
            FIG_DOPEN(i) = 0;
            FIG_TX(i) = FIG_PEND(i);
            FIG_PEND(i) = 0;
        } else if (FIG_MODE(i) == 1 && FIG_TX(i) != FIG_X(i) && FIG_STATE(i) != 1 && FIG_ELEV(i) == 0) {
            FIG_STATE(i) = 1;
            FIG_DOPEN(i) = 0;
        } else if (FIG_STATE(i) == 0 && (FIG_FLOOR(i) != FIG_TFLOOR(i) || FIG_DOOR(i) == -2) && FIG_ELEV(i) == 0) {
            FIG_STATE(i) = 5; /* am Aufzug warten */
            elevator_request_add(i);
            FIG_ELEV(i) = 1;
        }

        /* Aufzugphasen */
        if (FIG_FLOOR(i) == g_elev_floor && g_elev_door == 2 && FIG_ELEV(i) == 1 && i == g_elev_rider) {
            FIG_STATE(i) = 2;
            FIG_ELEV(i) = 2;
        } else if (FIG_STATE(i) == 0 && FIG_ELEV(i) == 2) {
            FIG_STATE(i) = 3;
            FIG_ELEV(i) = 3;
        } else if (FIG_STATE(i) == 3 && FIG_TFLOOR(i) == g_elev_floor && FIG_DOOR(i) != -2 && g_elev_door == 2) {
            FIG_STATE(i) = 4;
            FIG_ELEV(i) = 4;
        } else if (FIG_STATE(i) == 0 && FIG_ELEV(i) == 4) {
            if (FIG_DOOR(i) != -1) {
                FIG_STATE(i) = 1;
                FIG_TX(i) = (int16_t)(DOOR_X(FIG_FLOOR(i), FIG_DOOR(i)) + 4);
            }
            FIG_ELEV(i) = 0;
        } else if (FIG_STATE(i) == 0 && FIG_TX(i) == FIG_X(i) && FIG_ELEV(i) == 0 && FIG_DOOR(i) != -1) {
            /* an der Zieltür angekommen */
            int16_t kind = FLOOR_B(FIG_FLOOR(i), 0x1e + FIG_DOOR(i));
            DS_I16(0x8f8e) = kind;
            if (i == 0 && kind >= 1 && kind <= 5 && FLOOR_B(FIG_FLOOR(i), 0x46 + FIG_DOOR(i)) != 1 &&
                player_flag(1, 0) == 0) {
                DS_I16(0x79ca) = 0x15c; /* fremdes Büro: Zutritt verweigert */
                FIG_DOOR(i) = -1;
            } else {
                int16_t t = DOOR_TYPE(FIG_FLOOR(i), FIG_DOOR(i));
                DS_I16(0x8f8e) = t;
                if (t == 6 || t == 10) {
                    if (FIG_STATE(i) == 0 && FIG_DOPEN(i) == 1) {
                        FIG_DOPEN(i) = 0;
                        FIG_MODE(i) = 0;
                        FIG_CDOOR(i) = FIG_DOOR(i);
                    } else if (FIG_STATE(i) == 0) {
                        FIG_STATE(i) = 5;
                        FIG_DOPEN(i) = 1;
                    }
                } else if (FIG_DOPEN(i) == 0) {
                    if (DOOR_OPEN(FIG_FLOOR(i), FIG_DOOR(i)) == 0) {
                        DOOR_OPEN(FIG_FLOOR(i), FIG_DOOR(i)) = 1;
                        door_draw(FIG_FLOOR(i), FIG_DOOR(i), 1);
                    }
                    FIG_DOPEN(i) = 1;
                    FIG_STATE(i) = 5;
                } else if (FIG_DOPEN(i) == 1 && FIG_STATE(i) == 0) {
                    DOOR_OPEN(FIG_FLOOR(i), FIG_DOOR(i)) = 0;
                    door_draw(FIG_FLOOR(i), FIG_DOOR(i), 2);
                    FIG_MODE(i) = 0;
                    FIG_DOPEN(i) = 0;
                    FIG_CDOOR(i) = FIG_DOOR(i);
                }
            }
        } else if (FIG_STATE(i) == 0 && FIG_TX(i) == FIG_X(i) && FIG_DOOR(i) == -1 && FIG_ELEV(i) == 0) {
            FIG_ANIM(i) = 0;
        }
    }
}

/* 27bb:198f – Aufzug steuern: Ziel aus Fahrgast oder Warteschlange, Tür, Fahrt; danach den
 * Gebäude-Scroll an die eigene Figur anpassen */
void elevator_update(void)
{
    int16_t target = 0, rider = 0;
    if (g_elev_rider >= 0)
        g_elev_rider = -1;
    for (int16_t i = 0; i < 7; i++)
        if (FIG_STATE(i) == 3) {
            target = FIG_TFLOOR(i);
            rider = (int16_t)(i + 1);
        }
    if (target == 0 && DS_I16(0x8c34) > 0)
        target = FIG_FLOOR(DS_I16(0x8c4c)); /* erste Anforderung der Warteschlange */
    for (int16_t i = 0; i < 7; i++) {
        DS_I16(0x8f8e) = FIG_STATE(i);
        if (FIG_STATE(i) == 2 || FIG_STATE(i) == 4 || (FIG_STATE(i) == 0 && FIG_ELEV(i) == 2)) {
            target = 0; /* jemand steigt gerade ein/aus */
            rider = (int16_t)(i + 1);
        }
    }
    if (target == 0 && g_elev_offset != 0)
        target = (int16_t)(g_elev_offset < 1 ? g_elev_floor + 1 : g_elev_floor - 1);

    if (target != 0) {
        if (g_elev_floor == target) {
            int16_t take = 0;
            if (g_elev_door < 2) {
                g_elev_door++;
                take = g_elev_door == 2 && rider == 0;
            } else {
                take = rider == 0 && DS_I16(0x8c34) >= 1 && FIG_FLOOR(DS_I16(0x8c4c)) == g_elev_floor;
            }
            if (take) {
                g_elev_rider = DS_I16(0x8c4c);
                elevator_request_remove(DS_I16(0x8c4c));
            }
        } else {
            if (g_elev_door < 1) {
                if (g_elev_floor < target) {
                    if (g_elev_speed - 0x1e < g_elev_offset) {
                        g_elev_offset = (int16_t)(g_elev_offset - g_elev_speed);
                    } else {
                        g_elev_floor++;
                        g_elev_offset = 0;
                    }
                } else if (0x1e - g_elev_speed <= g_elev_offset) {
                    g_elev_floor--;
                    g_elev_offset = 0;
                } else {
                    g_elev_offset = (int16_t)(g_elev_offset + g_elev_speed);
                }
            } else {
                g_elev_door--;
            }
            if (rider != 0)
                FIG_FLOOR(rider - 1) = g_elev_floor;
        }
    }

    /* Scroll folgt der eigenen Figur; beim Aufzugfahren pixelgenau mit der Kabine */
    int16_t base = (int16_t)(FIG_FLOOR(g_player_idx) * 0x1e);
    g_scroll = (int16_t)(base - 0x3c);
    if (FIG_STATE(g_player_idx) == 3) {
        int16_t pos = (int16_t)(FIG_FLOOR(g_player_idx) * 0x1e - g_elev_offset);
        DS_I16(0x8f8e) = pos;
        if (pos != base && pos % g_elev_speed == 0)
            g_scroll = (int16_t)((FIG_FLOOR(g_player_idx) * 0x1e - 0x3c) - g_elev_offset);
    }
    if (g_scroll < 0)
        g_scroll = 0;
    else if (g_scroll > 0x141)
        g_scroll = 0x141;
}

/* 27bb:1be9 – Hinweise im Gebäude: Grüßen bei Begegnungen, Türschild unter dem Mauszeiger,
 * Meldung DS:79ca und Tooltips der unteren Leiste */
void building_hover_draw(void)
{
    if (g_cur_room != 0)
        return;
    SpriteBank *b8362 = DS_PTR(SpriteBank, 0x8362);

    /* Begegnungen im Gang: Sprechblase "Hallo" */
    for (int16_t i = 0; i < 6; i++) {
        if (FIG_MODE(i) != 1)
            continue;
        int16_t y = (int16_t)(0x75 - (FIG_FLOOR(i) * 0x1e - g_scroll));
        for (int16_t j = 0; j < 6; j++) {
            if (!(FIG_FLOOR(i) == FIG_FLOOR(j) && j != i && FIG_STATE(i) != 3 && FIG_STATE(j) != 3 && FIG_MODE(j) == 1))
                continue;
            int16_t xi = FIG_X(i), xj = FIG_X(j);
            int16_t ai = FIG_ANIM(i), aj = FIG_ANIM(j);
            int16_t facing = !(xj <= xi || xi + 0x32 <= xj || (ai != 0 && ai != 1) || (aj != 0 && aj != -1));
            if (!facing) {
                DS_I16(0x8f8e) = xi;
                DS_I16(0x8f8c) = xj;
                if (xi <= xj || xj <= xi - 0x32 || (ai != 0 && ai != -1) || (aj != 0 && aj != 1))
                    continue;
            }
            if (y > -0xf && y < 0x72) {
                if (y < 4)
                    y = 4;
                if (y > 0x6a)
                    y = 0x6a;
                int16_t players = i < 3 && j < 3;
                if (xj < xi)
                    draw_sprite((Bitmap *)((uint8_t *)b8362 + sprite_ofs(players ? 0x11 : 9)), (int16_t)(xi + 0xd), y);
                else
                    draw_sprite((Bitmap *)((uint8_t *)b8362 + sprite_ofs(players ? 0x10 : 8)),
                                (int16_t)(xi - (players ? 0xb : 9)), y);
            }
            break;
        }
    }

    /* Türschild unter dem Mauszeiger */
    if ((FIG_STATE(g_player_idx) != 3 || FIG_DOOR(g_player_idx) != -2) && g_cursor_y > 3 && g_cursor_y < 0x72 &&
        DS_I16(0x8aac) == 1) {
        int16_t rel = (int16_t)((g_scroll - g_cursor_y) + 0x70);
        int16_t floor = (int16_t)(rel / 0x1e + 1);
        if (floor < 0xe) {
            int16_t top = (int16_t)(floor * 0x1e - g_scroll);
            for (int16_t d = 0; d < 4; d++) {
                int16_t t = DOOR_TYPE(floor, d);
                DS_I16(0x8f8e) = t;
                if (!(((t != 0 && t < 4) || t == 6 || t == 10) && DOOR_X(floor, d) <= g_cursor_x &&
                      g_cursor_x <= DOOR_X(floor, d) + 0x10))
                    continue;
                char *label = door_label_text(d, floor);
                if (label == NULL)
                    continue;
                int16_t wcols = (int16_t)(DOOR_W(floor, d) + 1);
                int16_t hrows = (int16_t)(DOOR_H(floor, d) + 1);
                int16_t col = (int16_t)(((DOOR_X(floor, d) + 0x10) >> 3) + 1);
                int16_t y = FIG_STATE(g_player_idx) == 3 ? (int16_t)(mouse_y() - 8) : (int16_t)(0x79 - top);
                if (y < 4)
                    y = 4;
                if (0x70 < hrows * 8 + y)
                    y = (int16_t)(hrows * -8 + 0x70);
                if (0x20 - wcols < col)
                    col = (int16_t)(((DOOR_X(floor, d) + 0x10) >> 3) - wcols - 2);
                Bitmap *icon = NULL;
                if (DOOR_ICON(floor, d) != 0)
                    icon = (Bitmap *)((uint8_t *)DS_PTR(SpriteBank, 0x7b12) + sprite_ofs((int16_t)(DOOR_ICON(floor, d) + 5)));
                draw_frame((int16_t)(col << 3), y, (int16_t)((col + wcols - 1) * 8), (int16_t)(DOOR_H(floor, d) * 8 + y),
                           g_upper_screen, 2);
                int16_t indent;
                if (icon == NULL) {
                    indent = 1;
                } else {
                    draw_sprite(icon, (int16_t)((col + 1) * 8 - 5), (int16_t)(y + 4));
                    indent = 3;
                }
                text_box_to_free((int16_t)((col + 1) * 8 + indent * 8 - 0xc), (int16_t)(y - 4), label,
                                 (int16_t)(wcols - indent), 1, g_upper_screen);
            }
        }
    }

    /* Meldung (z. B. Zutritt verweigert) */
    if (DS_I16(0x79ca) > 0) {
        char *s;
        if (DS_I16(0x79ca) == 0x12a || DS_I16(0x79ca) == 0x12e)
            s = fmt(g_text(DS_I16(0x79ca)), DS_PTR(char, 0x8bbc + DS_I16(0x79f6) * 0x17), DS_I32(0x7ad2));
        else
            s = fmt(g_text(DS_I16(0x79ca)));
        draw_frame_cells(8, 1, 0x16, 6, g_upper_screen, 1);
        text_box(0x44, 4, s, 0xd, 1, g_upper_screen, 5);
        far_free(s);
    }

    /* Tooltips der unteren Leiste */
    if (DS_I16(0x8aac) == 1) {
        if (mouse_y() < 0x78 || mouse_x() > 0x78 || hour_index() < 1) {
            if (mouse_in_rect(0x99, 0x80, 0xc1, 0x88))
                money_tooltip_show();
            else if (mouse_in_rect(0x99, 0xb5, 0xc1, 0xbd))
                day_tooltip_show();
        } else {
            channel_info_show();
        }
    }
}

/* 27bb:21dd – Türschild-Text (neuer Puffer) oder NULL */
char *door_label_text(int16_t slot, int16_t floor)
{
    int16_t owner = DOOR_OWNER(floor, slot);
    int16_t text = DOOR_TEXT(floor, slot);
    if (owner != 0 && PLAYER_OUT(owner) != 0)
        return NULL; /* ausgeschiedener Spieler */
    switch (DOOR_KIND(floor, slot)) {
    case 0:
        /* ORIGINAL-BUG: liefert eine nicht initialisierte Variable zurück */
        return NULL;
    case 1: /* Raumname + Spielername */
        return fmt(DS_ADDR(char, 0x11c7), g_text(text), DS_PTR(char, 0x8bbc + owner * 0x17));
    case 2:
    case 3:
    case 4: /* Raumname + Sendername */
        return fmt(DS_ADDR(char, 0x11cd), g_text(text), DS_PTR(char, 0x8bc0 + owner * 0x17));
    case 5:
        return fmt(DS_ADDR(char, 0x11d4), g_text(0x8b), DS_PTR(char, 0x8bc0 + owner * 0x17));
    default:
        return text_get_copy(text);
    }
}

/* 27bb:231e – Spieler DS:7a34 scheidet aus: Figur fällt an der Fassade herab; beim
 * menschlichen Spieler Spielende, sonst werden seine Räume und Programme freigegeben */
void player_eliminated_sequence(void)
{
    int16_t player = DS_I16(0x7a34);
    int16_t fall_y = 0x3f;
    clock_stop();
    if (player == g_local_player) {
        status_text(g_text(0xf5), 0xb4);
        tv_show_picture(0xdc);
        if (DS_I16(0x79d6) != 0)
            DS_I16(0x79d6) = 1;
        status_text(DS_ADDR(char, 0x11db), 0x7f);
    } else {
        char *s = fmt(g_text(0x12d), DS_PTR(char, 0x8bbc + player * 0x17));
        show_message_text(s);
        far_free(s);
        do
            game_tick();
        while (g_mouse_buttons != 0);
        do
            game_tick();
        while (g_mouse_buttons == 0);
        room_leave();
    }
    status_text(DS_ADDR(char, 0x11de), 0xb4);
    DS_PTR(SpriteBank, 0x00fc) = font_get(2, 1);
    DS_PTR(SpriteBank, 0x00f4) = font_get(0, 0);
    mouse_hide();
    g_scroll = (int16_t)((player * 3 + 2) * 0x1e - 0x3c);
    DS_PTR(SpriteBank, 0x104) = hes_load_unpack0(0x1c);
    DS_PTR(SpriteBank, 0x108) = hes_load_unpack(0x1d);
    DS_PTR(SpriteBank, 0x10c) = hes_load_unpack((int16_t)(player + 0x18));
    if (player == g_local_player) {
        DS_PTR(SpriteBank, 0x110) = hes_load_unpack(0x1e);
        DS_PTR(Bitmap, 0x114) = bitmap_alloc(0x128, 0x3d);
    }
    clip_reset();
    g_clip_top = 4;
    g_clip_bottom = 0x71;
    building_walls_draw(1);
    facade_draw();
    screen_transition(2, 1, 4);
    DS_I16(0x7a24) = 0;

    int16_t frame = 1, x = 0xa5;
    uint16_t last = ticks_get();
    while (g_scroll >= 0) {
        if (last + 3u <= ticks_get()) {
            last = ticks_get();
            facade_draw();
            draw_sprite(&DS_PTR(SpriteBank, 0x108)->bmp[0], 0xa6, fall_y);
            draw_sprite((Bitmap *)((uint8_t *)DS_PTR(SpriteBank, 0x10c) + sprite_ofs(frame)), x, 0x3e);
            screen_update();
            g_scroll -= 3;
            fall_y -= 3;
            frame++;
            if (frame > 8)
                frame = 1;
            /* Ausweichbewegung an Vorsprüngen (Tabelle DS:11a6, 5 Paare je Spieler) */
            int16_t k;
            for (k = 0; x <= DS_U8(0x11a6 + (player - 1) * 10 + k * 2) && k < 5; k++)
                ;
            if (k < 5 && DS_U8(0x11a6 + (player - 1) * 10 + k * 2) != 0)
                x = (int16_t)(x - DS_U8(0x11a7 + (player - 1) * 10 + k * 2));
        }
    }
    g_scroll = 0;
    for (int16_t y = 0x3b; y < 0x5e;) {
        if (last + 3u <= ticks_get()) {
            last = ticks_get();
            facade_draw();
            draw_sprite((Bitmap *)((uint8_t *)DS_PTR(SpriteBank, 0x10c) + sprite_ofs(frame)), x, y);
            screen_update();
            frame++;
            if (frame > 8)
                frame = 1;
            if (x > 0x14)
                x--;
            y += 3;
        }
    }
    draw_sprite(&DS_PTR(SpriteBank, 0x108)->bmp[1], 6, 0x53);
    screen_update();

    if (player == g_local_player) {
        /* Spielende */
        g_clip_on = 0;
        uint16_t t0 = ticks_get();
        while (ticks_get() < t0 + 100u)
            ;
        SpriteBank *go = DS_PTR(SpriteBank, 0x110);
        draw_sprite(&go->bmp[0], 0xe, 0x1d);
        draw_sprite(&go->bmp[1], 0x54, 0x1d);
        draw_sprite(&go->bmp[2], 0x9f, 0x1d);
        draw_sprite(&go->bmp[3], 0xec, 0x1d);
        screen_transition(2, 1, 4);
        g_mouse_buttons = 0;
        while (g_mouse_buttons == 0)
            game_tick();
        far_free_ptr(DS_ADDR(void *, 0x110));
        far_free_ptr(DS_ADDR(void *, 0x114));
        music_on_leave(1000, 0);
        savegame_load(0x2b);
        DS_I16(0x7c58) = 0;
        g_cur_room = 0;
        DS_I16(0x8e6e) = 0;
        mouse_hide();
        status_text(g_text(0xf6), 0xb4);
    } else {
        /* Computerspieler: Besitz freigeben */
        PLAYER_OUT(player) = 1;
        for (int16_t i = 0; i < DS_I16(0x7688 + player * 2); i++)
            DS_U8(0x3f3e + DS_I16(0x1b50 + player * 0x46 + i * 7) * 0x18) = 0; /* Werbeverträge */
        DS_I16(0x7688 + player * 2) = 0;
        for (int16_t g = 1; g < 0xb; g++)
            for (int16_t k = 0; k < DS_I8(0x6f59 + player * 0x6c + g); k++)
                DS_PTR(uint8_t, 0x2489)[DS_I16(0x6ef9 + player * 0x6c + g * 8 + k * 2) * 0x17 + 5] = 0; /* Filme */
        DS_I16(0x8bc8 + player * 0x17) = 0;
        for (int16_t s = 1; s < 0xd; s++) { /* Studios */
            int16_t o = (int16_t)(s * 0x10);
            if (DS_I8(0x6148 + o) == player) {
                DS_I16(0x6145 + o) = 0;
                DS_U8(0x6153 + o) = 0;
                DS_I8(0x6148 + o) = -1;
                studio_release_request(s);
            }
        }
        FIG_FLOOR(player - 1) = 0;
        FIG_DOOR(player - 1) = -1;
        elevator_request_remove((int16_t)(player - 1));
        g_clip_on = 0;
        status_bar_draw();
        DS_I16(0x7a34) = 0;
        room_load_background(0x69);
        screen_transition(1, 1, 4);
    }
    far_free_ptr(DS_ADDR(void *, 0x10c));
    far_free_ptr(DS_ADDR(void *, 0x108));
    far_free_ptr(DS_ADDR(void *, 0x104));
    DS_I16(0x7a34) = 0;
    mouse_show();
    clock_start();
}

/* 27bb:2951 – Gebäudefassade (Bank DS:0104) passend zum Scroll in die Spielszene */
void facade_draw(void)
{
    SpriteBank *bank = DS_PTR(SpriteBank, 0x0104);
    g_force_redraw = 1;
    building_sides_draw();
    g_clip_top = 4;
    g_clip_bottom = 0x71;
    for (int16_t row = 1; row < 6; row++) {
        int16_t extra = g_scroll < 0x1e && row == 5 ? 3 : 0;
        int16_t y = (int16_t)((row * 0x1e - (0x1e - g_scroll % 0x1e)) - 0x25);
        bmp_draw((Bitmap *)((uint8_t *)bank + sprite_ofs((int16_t)(extra + 2))), g_upper_screen, 0x38, y);
        bmp_draw((Bitmap *)((uint8_t *)bank + sprite_ofs((int16_t)(extra + 3))), g_upper_screen, 0xf0, y);
        for (int16_t i = 1; i < 5; i++)
            bmp_draw((Bitmap *)((uint8_t *)bank + sprite_ofs((int16_t)(extra + 1))), g_upper_screen,
                     (int16_t)(i * 0x28 + 0x28), y);
    }
}
