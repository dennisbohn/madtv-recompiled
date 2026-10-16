/* Segment 239a – Maus-Tooltips und Infokarten (Hover über Räume, Listen, Statuszeile) */
#include "game.h"

#define g_text(id)        PTR32(char, DS_ADDR(ptr32, 0x8370)[id])
#define g_upper_screen    DS_PTR(Bitmap, 0x8e62)
#define RBANK(ofs)        DS_PTR(uint8_t, ofs)
#define BANK_BMP(bank, ofs) ((Bitmap *)((uint8_t *)(bank) + (ofs)))
#define g_shows           DS_PTR(uint8_t, 0x713c)
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define CHANNEL_NAME(p)   DS_PTR(char, 0x8bc0 + (p) * 0x17)
#define g_view_player     DS_I16(0x7a16)
#define g_hotspot         DS_I16(0x8ee3)
#define g_card_dirty      DS_I16(0x79ae) /* Infokarte neu aufbauen */

/* Zustand des Tooltips; Positionen und Größe in 8-Pixel-Spalten / 8er-Zeilen */
#define g_hv_x            DS_I16(0x8f48)
#define g_hv_y            DS_I16(0x8f4a)
#define g_hv_w            DS_I16(0x8f8a)
#define g_hv_h            DS_I16(0x8f86)
#define g_hv_sel          DS_I16(0x8f88) /* Vertragsplatz / Besetzung, -1 = keiner */
#define g_hv_a            DS_I16(0x8f7e)
#define g_hv_i            DS_I16(0x8f80)
#define g_hv_j            DS_I16(0x8f46)
#define g_hv_show         DS_I16(0x8f76)
#define g_hv_ep           DS_I16(0x8f78)
#define g_hv_force        DS_I16(0x8f7c)
#define g_hv_mouse_hidden DS_I16(0x8f7a)
#define g_hv_val          DS_I32(0x8f3e)
#define g_hv_val2         DS_I32(0x8f42)
#define g_hv_n1           DS_I16(0x8f6c)
#define g_hv_n2           DS_I16(0x8f6e)
#define g_hv_spec         DS_PTR(char, 0x8f68) /* Layout-Beschreibung für hover_dispatch-Fall 5 */
#define HV_STR(k)         PTR32(char, DS_ADDR(ptr32, 0x8f4c)[k]) /* Textbausteine für layout_box */
#define HV_STR_SET(k, p)  (DS_ADDR(ptr32, 0x8f4c)[k] = P32(p))
#define g_hv_last         DS_I16(0x0e80) /* zuletzt angezeigtes Objekt */
#define g_hv_last_x       DS_I16(0x0e82)
#define g_hv_last_y       DS_I16(0x0e84)
#define g_popup_bmp       DS_PTR(Bitmap, 0x8ec2)

static void hv_at(int16_t x, int16_t y)
{
    g_hv_x = x;
    g_hv_y = y;
}

/* 239a:000a – was liegt unter der Maus? Ergebnis ist eine Objektnummer (Tausender = Art, siehe
 * hover_dispatch) samt Tooltip-Position/-Größe */
void room_hover(void)
{
    int16_t id = 0, k;
    g_hv_sel = 0;
    g_hv_x = g_hv_y = g_hv_w = g_hv_h = 0;
    DS_I16(0x8f3a) = DS_I16(0x8f3c) = 0;
    g_hv_a = g_hv_i = 0;
    g_hv_mouse_hidden = 0;
    g_hv_j = 0;
    g_hv_show = g_hv_ep = 0;
    g_hv_force = 0;
    g_hotspot = hotspot_at_mouse(1);
    if (mouse_y() < 0x78 && DS_I16(0x8aac) != 1 && DS_PTR(Bitmap, 0x0390) == NULL)
        return;
    if (DS_I16(0x012c) != 0)
        return;
    if (DS_I16(0x0e86) != g_cur_room || DS_I16(0x0e88) != g_cur_dialog) {
        g_hv_last = 0;
        DS_I16(0x0e86) = g_cur_room;
        DS_I16(0x0e88) = g_cur_dialog;
        DS_I16(0x79f8) = 0;
    }
    if (mouse_y() > 0x78 && DS_I16(0x8aac) == 1) { /* Statusleiste unten */
        if (mouse_x() < 0x78 && hour_index() > 0 && DS_I16(0x012c) == 0) {
            id = 7000; /* Fernseher: Senderkarte */
            hv_at(1, 6);
            g_hv_w = 0xf;
            g_hv_h = 7;
        } else if (mouse_in_rect(0x99, 0x80, 0xc2, 0x89)) {
            id = 17000; /* Geld */
            hv_at(0x11, 9);
            g_hv_w = 8;
            g_hv_h = 4;
        } else if (mouse_in_rect(0x99, 0x94, 0xc2, 0x9e)) {
            id = 18000; /* Quote */
            hv_at(0xc, 7);
            g_hv_w = 0xe;
            g_hv_h = 6;
        } else if (mouse_in_rect(0x99, 0xb5, 0xc2, 0xbe)) {
            DS_SET_PTR(0x8f68, DS_ADDR(char, 0x0e8e)); /* Tag */
            id = 5000;
            if (g_hv_last != 5000 || g_card_dirty != 0)
                HV_STR_SET(0, fmt(g_text(0x51), g_day));
            hv_at(0x10, 0xc);
            g_hv_w = 6;
            g_hv_h = 1;
        }
        goto dispatch;
    }
    if (g_cur_room == 0x1f) { /* Programmplaner */
        if (g_cur_dialog == 0 || g_cur_dialog == 4) {
            if (g_hotspot == 0xf) {
                g_hv_a = (int16_t)((mouse_y() - 4) / 0x10 + DS_I16(0x79a6) + 1);
                if (DS_I16(0x43f7 + g_view_player * 0x5a + g_hv_a * 2) != 0) {
                    id = schedule_get_programme(g_view_player, g_hv_a);
                    g_hv_x = mouse_x() < 0xa0 ? 0x15 : 1;
                    g_hv_y = 1;
                }
            }
            if (g_hotspot != 0x10)
                goto dispatch;
            g_hv_a = (int16_t)((g_cursor_y - 4) / 0x10 + DS_I16(0x79a6) + 1);
            int16_t ad = DS_I16(0x4433 + g_view_player * 0x5a + g_hv_a * 2);
            if (ad == 0)
                goto dispatch;
            ad %= 1000;
            id = (int16_t)(ad + 4000);
            g_hv_sel = -1;
            for (g_hv_j = 0; g_hv_j < DS_I16(0x7688 + g_view_player * 2); g_hv_j++) {
                if (DS_I16(0x1b50 + g_view_player * 0x46 + g_hv_j * 7) == ad) {
                    g_hv_sel = g_hv_j;
                    break;
                }
            }
            goto x2_y1;
        }
        if (g_cur_dialog == 2) { /* Archiv */
            if (g_hotspot != 0x18)
                goto dispatch;
            /* nur belegte Plätze des Fachs */
            if ((g_cursor_y - 0x48) / 10 >= DS_I8(0x6f59 + g_view_player * 0x6c + DS_I16(0x79aa)))
                goto dispatch;
            int16_t v = DS_I16(0x6ef9 + g_view_player * 0x6c + DS_I16(0x79aa) * 8 + ((g_cursor_y - 0x48) / 10) * 2);
            if (DS_I16(0x79aa) < 0xb) {
                id = v;
            } else {
                if (v == 0)
                    goto dispatch;
                id = (int16_t)(v + 1000);
            }
            g_hv_x = 1;
            goto y1;
        }
        if (g_cur_dialog == 5) { /* Werbeverträge */
            if (g_hotspot != 0x19 || DS_I16(0x1b50 + g_view_player * 0x46 + ((g_cursor_y - 0x12) / 10) * 7) == 0)
                goto dispatch;
            g_hv_sel = (int16_t)((g_cursor_y - 0x12) / 10);
            id = (int16_t)(DS_I16(0x1b50 + g_view_player * 0x46 + g_hv_sel * 7) + 4000);
            goto x2_y1;
        }
        if (g_cur_dialog == 7 && mouse_y() > 0x5d && mouse_y() < 0x69 && DS_I16(0x7a4e) == 0) {
            /* Statistik: Balken einer Stunde */
            DS_I16(0x8f70) = g_hv_n1;
            DS_I16(0x8f72) = g_hv_n2;
            int16_t ch = DS_I16(0x79da);
            for (g_hv_i = 1; g_hv_i < 8; g_hv_i++) {
                if (mouse_x() < g_hv_i * 0x1e + 0x3f || mouse_x() > g_hv_i * 0x1e + 0x5a)
                    continue;
                if (DS_I16(0x79d8) == 0) {
                    id = DS_I16(0x3919 + ch * 0x5a + g_hv_i * 2);
                    g_hv_j = g_hv_i;
                    if (id == 9999)
                        while ((id = DS_I16(0x3919 + ch * 0x5a + g_hv_j * 2)) == 9999)
                            g_hv_j--;
                    g_hv_n2 = DS_I16(0x3937 + ch * 0x5a + g_hv_j * 2);
                    g_hv_val = DS_I32(0x789e + ch * 0x20 + g_hv_i * 4);
                    g_hv_val2 = DS_I16(0x1c68 + ch * 0x1e + g_hv_i * 2);
                } else if (DS_I16(0x79d8) == 1 &&
                           (g_hv_i < hour_index() || (hour_index() == g_hv_i && DS_I16(0x71cc) > 4))) {
                    g_hv_j = g_hv_i;
                    while ((id = DS_I16(0x43f7 + ch * 0x5a + g_hv_j * 2)) == 9999)
                        g_hv_j--;
                    g_hv_n2 = DS_I16(0x4415 + ch * 0x5a + g_hv_j * 2);
                    g_hv_val = DS_I32(0x74ca + ch * 0x20 + g_hv_i * 4);
                    g_hv_val2 = DS_I16(0x1c68 + ch * 0x1e + (g_hv_i + 7) * 2);
                }
                if (id > 0) {
                    g_hv_n1 = (int16_t)(g_hv_i - g_hv_j + 1); /* Stunde n des Programms */
                    id = (int16_t)(id + 14000);
                    hv_at(0xc, 1);
                    if (id == g_hv_last) {
                        if (g_hv_n2 == DS_I16(0x8f72) && g_hv_n1 == DS_I16(0x8f70))
                            continue;
                    } else {
                        g_hv_force = 1;
                    }
                    g_card_dirty = 1;
                }
            }
        }
        goto dispatch;
    }
    if (g_cur_room == 0x19) { /* Senderkarte */
        if (g_cur_dialog == 0) {
            if (g_hotspot == 1)
                id = 0x9c5;
            else if (g_hotspot == 2)
                id = 0x9c6;
            hv_at(0xc, 4);
        }
        if (g_cur_dialog == 1) {
            id = 2000;
            hv_at(0x1d, 4);
            if (mouse_x() != DS_I16(0x0e8a) || mouse_y() != DS_I16(0x0e8c))
                g_card_dirty = 1;
            DS_I16(0x0e8a) = mouse_x();
            DS_I16(0x0e8c) = mouse_y();
        } else if (g_cur_dialog == 3) {
            id = (int16_t)(DS_I16(0x79a8) + 2000);
            hv_at(0x1d, 9);
        }
        goto dispatch;
    }
    if (g_cur_room == 8) { /* Filmhändler */
        if (g_cur_dialog == 0) {
            for (g_hv_i = 1; g_hv_i < 0xb; g_hv_i++) {
                int16_t o = g_hv_i * 8;
                if (mouse_in_rect(DS_I16(0x32e + o), DS_I16(0x330 + o), DS_I16(0x332 + o), DS_I16(0x334 + o)) &&
                    DS_I8(0x19d4 + o) >= 0) {
                    g_hv_x = g_hv_i < 9 ? 0x15 : 1;
                    g_hv_y = 1;
                    id = DS_I16(0x19cd + o);
                    break;
                }
            }
            if (g_hotspot == 0x21) { /* Koffer */
                int16_t c = (int16_t)((mouse_x() - 0xe0) / 9);
                g_hv_a = DS_I16(0x2463 + (c + ((mouse_y() - 0x1c) / 0x1e) * 5) * 2);
                if (g_hv_a != 0) {
                    hv_at(1, 1);
                    id = g_hv_a;
                }
            }
        } else if (g_cur_dialog == 4) { /* Auktion */
            for (g_hv_i = 0; g_hv_i < 2; g_hv_i++) {
                for (g_hv_j = 1; g_hv_j < 6; g_hv_j++) {
                    int16_t lot = g_hv_i * 5 + g_hv_j;
                    if (mouse_in_rect((int16_t)(g_hv_i * 0x8e + 0x14), (int16_t)(g_hv_j * 0x13 - 7),
                                      (int16_t)(g_hv_i * 0x8e + 0x9e), (int16_t)(g_hv_j * 0x13 + 0xb)) &&
                        DS_I16(0x1aed + lot * 9) != 0) {
                        id = DS_I16(0x1aed + lot * 9);
                        g_hv_y = 1;
                        g_hv_x = g_hv_i == 0 ? 0x15 : 1;
                    }
                }
            }
        }
        goto dispatch;
    }
    if (g_cur_room == 0xb) { /* Werbeagentur */
        int16_t slot = 0; /* im Original ohne passenden Hotspot uninitialisiert */
        if (g_hotspot == 0x3b) {
            slot = (int16_t)((g_cursor_x - 0x67) / 8);
            id = DS_I16(0x1980 + slot * 7);
        } else if (g_hotspot == 0x3c) {
            slot = (int16_t)((g_cursor_x - 0x7b) / 8 + 3);
            id = DS_I16(0x1980 + slot * 7);
        } else if (g_hotspot == 0x3d) {
            slot = (int16_t)((g_cursor_x - 0x95) / 8 + 7);
            id = DS_I16(0x1980 + slot * 7);
        }
        if (DS_I8(0x1982 + slot * 7) < 0 || DS_I16(0x1980 + slot * 7) == 0)
            id = 0;
        if (id > 0) {
            id = (int16_t)(id + 3000);
            g_hv_x = 0x17;
            goto y1;
        }
        if (g_hotspot != 0x21 || g_cur_dialog != 0)
            goto dispatch;
        k = (int16_t)((mouse_x() - 0xe4) / 9 + ((mouse_y() - 0x20) / 0x20) * 5);
        int16_t ad = DS_I16(0x1b50 + g_local_player * 0x46 + k * 7);
        if (ad == 0 || k < 0 || k > 9) {
            id = 0;
            goto dispatch;
        }
        id = (int16_t)(ad + 3000);
        goto x2_y1;
    }
    if (g_cur_room == 4) { /* Archiv */
        if (g_cur_dialog == 0 && g_hotspot == 0x21) {
            g_hv_a = (int16_t)((mouse_x() - 0x18) / 9);
            if (g_hv_a < DS_I16(0x732a)) {
                id = DS_I16(0x2463 + g_hv_a * 2);
                hv_at(0x15, 1);
            }
        }
        if (g_cur_dialog == 0 && DS_I16(0x79aa) != 0 && g_hotspot == 0x20) {
            g_hv_a = (int16_t)((mouse_x() - 0xa0) / 9);
            if (g_hv_a < DS_I8(0x6f59 + g_view_player * 0x6c + DS_I16(0x79aa))) {
                id = DS_I16(0x6ef9 + g_view_player * 0x6c + DS_I16(0x79aa) * 8 + g_hv_a * 2);
                if (id != 0 && DS_I16(0x79aa) > 10)
                    id = (int16_t)(id + 1000);
                hv_at(1, 1);
            }
        }
        if (g_cur_dialog != 2 || !mouse_in_rect(0x1c, 0x14, 0x9a, 199) ||
            (int8_t)SERIES(DS_I16(0x79b4))[0x22] <= (mouse_y() - 0x14) / 8)
            goto dispatch;
        id = (int16_t)((mouse_y() - 0x14) / 8 + (DS_I16(0x79b4) + 1000) * 10); /* Serienfolge */
        g_hv_x = 0x13;
    } else if (g_cur_room == 9) { /* Drehbuchagentur */
        if (g_cur_dialog != 0)
            goto dispatch;
        for (g_hv_i = 1; g_hv_i < 6; g_hv_i++) {
            if (mouse_in_rect((int16_t)(g_hv_i * 7 + 99), 0x23, (int16_t)(g_hv_i * 7 + 0x69), (int16_t)(0x40 - g_hv_i)) &&
                DS_I8(0x194c + g_hv_i * 9) >= 0 && DS_I16(0x194a + g_hv_i * 9) != 0) {
                id = (int16_t)(DS_I16(0x194a + g_hv_i * 9) + 6000);
                hv_at(0x14, 2);
                break;
            }
        }
        if (g_hotspot != 0x21)
            goto dispatch;
        int16_t sh = DS_I16(0x1a25 + g_local_player * 0x32 + ((mouse_x() - 0x1a) / 9) * 5);
        if (sh == 0) {
            id = 0;
            goto dispatch;
        }
        id = (int16_t)(sh + 6000);
        g_hv_x = 0xf;
    } else {
        if (g_cur_room == 0x3e && g_cur_dialog == 0) { /* Studio-Belegungsplan */
            for (g_hv_i = 1; g_hv_i < 0xe; g_hv_i++) {
                for (g_hv_j = 0; DS_I8(0x7c76 + g_hv_i * 0x7a + g_hv_j) != 0 && DS_U8(0x7c76 + g_hv_i * 0x7a + g_hv_j) < 4;
                     g_hv_j++) {
                    building_map_door_pos(g_hv_i, g_hv_j, DS_ADDR(int16_t, 0x8f3a), DS_ADDR(int16_t, 0x8f3c));
                    if (!mouse_in_rect(DS_I16(0x8f3a), DS_I16(0x8f3c), (int16_t)(DS_I16(0x8f3a) + 9), (int16_t)(DS_I16(0x8f3c) + 10)))
                        continue;
                    id = (int16_t)(g_hv_i * 10 + g_hv_j + 5000);
                    g_hv_x = g_hv_i < 8 ? 2 : 0x16;
                    g_hv_y = 1;
                    g_hv_w = 0x10;
                    g_hv_h = 7;
                    DS_SET_PTR(0x8f68, DS_ADDR(char, 0x0e92));
                    if (id != g_hv_last || g_card_dirty != 0) {
                        g_hv_a = DS_U8(0x7cd0 + g_hv_i * 0x7a + g_hv_j);
                        uint8_t *st = DS_ADDR(uint8_t, 0x6145 + g_hv_a * 0x10);
                        int8_t size = (int8_t)st[2], owner = (int8_t)st[3], pend = (int8_t)st[0xf];
                        if (g_hv_a == 0)
                            HV_STR_SET(0, text_get_copy(0x6c));
                        else if (pend >= 1)
                            HV_STR_SET(0, pend == g_local_player ? fmt(g_text(0x71), size)
                                                               : fmt(g_text(0x72), CHANNEL_NAME(pend), size));
                        else if (owner == -1)
                            HV_STR_SET(0, fmt(g_text(0x6d), *(int32_t *)(st + 6) * 3 / 2, size));
                        else if (owner == 0 || owner != g_local_player)
                            HV_STR_SET(0, text_get_copy(0x6c));
                        else
                            HV_STR_SET(0, fmt(g_text(0x6f), *(int32_t *)(st + 6), size));
                    }
                    break;
                }
                if (id != 0)
                    break;
            }
            goto dispatch;
        }
        if (g_cur_room != 5) {
            if (g_cur_room == 0xe && contest_tick() < 2) { /* Chefbüro: Wettbewerb */
                if (g_hotspot == 0x4a) {
                    DS_SET_PTR(0x8f68, DS_ADDR(char, 0x0eae));
                    if (g_hv_last != 5000 || g_card_dirty != 0)
                        HV_STR_SET(0, fmt(g_text(0x177), g_text(DS_I16(0x7468) + 0x153)));
                    hv_at(0x15, 2);
                    g_hv_w = 0x11;
                    g_hv_h = 3;
                    id = 5000;
                }
            } else if (g_cur_room == 0xc && g_cur_dialog == 3) { /* Geschenke */
                for (g_hv_i = 0; g_hv_i < 2; g_hv_i++) {
                    for (g_hv_j = 1; g_hv_j < 6; g_hv_j++) {
                        if (!mouse_in_rect((int16_t)(g_hv_j * 0x28 + 0x38), (int16_t)(g_hv_i * 0x24 + 0x10),
                                           (int16_t)(g_hv_j * 0x28 + 0x57), (int16_t)(g_hv_i * 0x24 + 0x27)))
                            continue;
                        id = (int16_t)(g_hv_i * 5 + g_hv_j + 16000);
                        g_hv_x = (int16_t)((g_hv_j * 0x28 + 0x20) >> 3);
                        if (g_hv_x > 0x1e)
                            g_hv_x = 0x1e;
                        g_hv_y = (int16_t)((g_hv_i * 0x24 + 0x28) >> 3);
                        g_hv_w = 9;
                        g_hv_h = 5;
                    }
                }
                if (g_hotspot == 0x21 && DS_I8(0x74c6 + g_local_player) != 0) {
                    hv_at(1, 2);
                    g_hv_w = 9;
                    g_hv_h = 5;
                    id = (int16_t)(DS_I8(0x74c6 + g_local_player) + 16000);
                }
            } else if (g_cur_room == 2) { /* Bank: Kredit */
                if (g_cur_dialog == 0 && g_hotspot > 0x3e && g_hotspot < 0x42) {
                    id = (int16_t)(g_hotspot + 0x146b);
                    DS_SET_PTR(0x8f68, DS_ADDR(char, 0x0eba));
                    if (id != g_hv_last || g_card_dirty == 1)
                        HV_STR_SET(0, fmt(DS_ADDR(char, 0x0ec2), g_text(g_hotspot + 0xe3), g_hotspot == 0x3f ? 1000 : 10000));
                    hv_at(2, 6);
                    g_hv_w = 9;
                    g_hv_h = 2;
                }
            } else if (g_cur_room == 0x3f) { /* Requisiten */
                g_hv_show = DS_I16(0x732c + g_local_player * 0x4b + DS_I16(0x79c0) * 0xf);
                g_hv_ep = DS_I16(0x732e + g_local_player * 0x4b + DS_I16(0x79c0) * 0xf);
                uint8_t *e = g_shows + g_hv_show * 0x1b3 + (g_hv_ep - 1) * 0x23;
                if (mouse_y() < 0x4e || mouse_y() > 0x65) {
                    if (DS_I16(0x79c2) < 8 && mouse_y() > 6 && mouse_y() < 0x1f && g_cur_dialog == 0) {
                        for (g_hv_i = 1; g_hv_i < 6; g_hv_i++) {
                            if (mouse_x() >= g_hv_i * 0x24 + 0x54 && mouse_x() <= g_hv_i * 0x24 + 0x73) {
                                id = (int16_t)(g_hv_i + DS_I16(0x79c4) + 13000);
                                hv_at(1, 1);
                            }
                        }
                    }
                } else {
                    for (g_hv_i = 1; g_hv_i < 4; g_hv_i++) {
                        if (mouse_x() < g_hv_i * 0x58 - 0x14 || mouse_x() > g_hv_i * 0x58 + 0xb)
                            continue;
                        int16_t r = g_hv_i + DS_I16(0x79c6);
                        int8_t type = (int8_t)e[r * 3 + 9], cast = (int8_t)e[r * 3 + 0xb];
                        int16_t staff = 0;
                        if (type < 0x10) {
                            if (g_cur_dialog == 1) {
                                if ((int8_t)g_shows[g_hv_show * 0x1b3 + type + 0x1a3] == DS_I16(0x79c2)) {
                                    id = (int16_t)(r + 11000);
                                    g_hv_sel = DS_I16(0x79b6);
                                }
                            } else {
                                id = (int16_t)(r + 11000);
                                g_hv_sel = cast;
                            }
                            if (id != 0)
                                g_hv_w = 0x12;
                        } else {
                            if (g_cur_dialog == 1) {
                                if (type == DS_I16(0x79c2)) {
                                    g_hv_sel = DS_I16(0x79b6);
                                    staff = 1;
                                }
                            } else if (cast != 0) {
                                g_hv_sel = cast;
                                staff = 1;
                            }
                            if (staff) {
                                id = (int16_t)(r + 12000);
                                g_hv_w = 0xd;
                            }
                        }
                        if (id != 0) {
                            g_hv_y = 1;
                            g_hv_x = 8;
                            g_hv_h = 7;
                        }
                    }
                }
            }
            goto dispatch;
        }
        /* Raum 5: Studio */
        if (g_cur_dialog != 0)
            goto dispatch;
        int16_t sh = 0;
        if (g_view_player == g_local_player && g_hotspot == 0x21)
            sh = DS_I16(0x1a25 + g_local_player * 0x32 + ((mouse_x() - 0xfa) / 9) * 5);
        if (sh != 0) {
            id = (int16_t)(sh + 6000);
        } else {
            if (g_hotspot != 0x29 || DS_I16(0x6145 + DS_I16(0x79be) * 0x10) == 0)
                goto dispatch;
            id = (int16_t)(DS_I16(0x6145 + DS_I16(0x79be) * 0x10) + 9000);
        }
        g_hv_x = 2;
    }
    g_hv_y = 2;
    goto dispatch;
x2_y1:
    g_hv_x = 2;
y1:
    g_hv_y = 1;
dispatch:
    hover_dispatch(id);
}

#define STATION(p, i)     DS_ADDR(uint8_t, 0x1ce9 + (p) * 0x1ce + (i) * 0xe)
#define REACH_LEVEL(l)    DS_I32(0xa8 + ((l) - 1) * 4)
#define AD_REC(n)         DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)
#define SCHED_AD(p, h)    DS_I16(0x4433 + (p) * 0x5a + (h) * 2)

static void hv_free(int16_t k)
{
    far_free(HV_STR(k));
}

/* Staffelung der Stabsymbole: Stufe 1..3 -> Bild 3, 1, 2 */
static int16_t hv_staff_icon(int16_t k, int16_t keep)
{
    return k == 1 ? 3 : k == 2 ? 1 : k == 3 ? 2 : keep;
}

static Bitmap *hv_actor_image(int16_t img)
{
    return img < 0x10 ? BANK_BMP(RBANK(0x100), sprite_ofs(img)) : BANK_BMP(RBANK(0x10c), sprite_ofs((int16_t)(img - 0xf)));
}

/* Fall 2000..2600: Senderkarte (Reichweite, Preise der Sendemasten) */
static void hover_station(int16_t id)
{
    if (id == 2000) { /* freie Stelle unter der Maus */
        g_hv_w = 10;
        g_hv_h = 4;
        g_hv_val = station_reach_gain((int16_t)(mouse_x() - 0x30), (int16_t)(mouse_y() - 4)) * 0x9a0;
        int32_t price = price_step_next(station_reach_at((int16_t)(mouse_x() - 0x30), (int16_t)(mouse_y() - 4)) * 0x268 / 10);
        HV_STR_SET(1, fmt_money(g_hv_val, 3));
        HV_STR_SET(0, fmt(g_text(0x17a), HV_STR(1), price));
        hv_free(1);
        layout_box(DS_ADDR(char, 0x0f31));
    } else if (id > 2000 && id <= 0x9c4) { /* eigener Mast */
        uint8_t *st = STATION(g_view_player, id - 2000);
        g_hv_w = 10;
        g_hv_h = 4;
        g_hv_val = *(int32_t *)(st + 4) * 0x9a0;
        HV_STR_SET(1, fmt_money(g_hv_val, 3));
        HV_STR_SET(0, fmt(g_text(0x17b), HV_STR(1), *(int32_t *)st));
        hv_free(1);
        layout_box(DS_ADDR(char, 0x0f41));
    } else if (id > 0x9c4 && id < 0xa29) { /* Satellit / Kabel */
        int16_t k = id - 0x9c5;
        uint8_t *st = STATION(g_view_player, id - 0x9c4);
        g_hv_w = 10;
        g_hv_val = DS_I32(0x7a96 + k * 4) * 0x9a0;
        g_hv_val2 = DS_I32(0x7a96 + (k + 7) * 4) * 0x9a0;
        if (*(int32_t *)(STATION(DS_I16(0x79ac), id - 0x9c4) + 4) == 0) {
            g_hv_h = 6;
            HV_STR_SET(2, fmt_money(g_hv_val - g_hv_val2, 3));
            HV_STR_SET(1, fmt_money(g_hv_val, 3));
            HV_STR_SET(0, fmt(g_text(0x17c), HV_STR(1), HV_STR(2), *(int32_t *)st));
            hv_free(1);
            hv_free(2);
            layout_box(DS_ADDR(char, 0x0f51));
        } else {
            g_hv_h = 4;
            HV_STR_SET(1, fmt_money(g_hv_val, 3));
            HV_STR_SET(0, fmt(g_text(0x17d), HV_STR(1), *(int32_t *)st));
            hv_free(1);
            layout_box(DS_ADDR(char, 0x0f69));
        }
    } else {
        return;
    }
    tooltip_present();
    hv_free(0);
}

/* Fall 3000/4000: Werbekunde (Angebot bzw. laufender Vertrag) */
static void hover_ad(int16_t id)
{
    int16_t ad = id % 1000;
    uint8_t *rec = AD_REC(ad);
    ad_requirements_text_build(ad);
    g_hv_w = 0xf;
    g_hv_h = 0xb;
    HV_STR_SET(1, MEM_PTR(char, rec));
    HV_STR_SET(2, MEM_PTR(char, rec + 4));
    int16_t p = g_cur_room == 0xb ? g_local_player : g_view_player;
    int32_t reach = REACH_LEVEL((int8_t)rec[0xa]);
    int16_t rating = (int16_t)rating_from_viewers(p, reach);
    HV_STR_SET(4, fmt_money(reach, 3));
    HV_STR_SET(3, fmt(g_text(0x17e), *(int32_t *)(rec + 0x10), *(int32_t *)(rec + 0x14), HV_STR(4), rating));
    hv_free(4);
    if (id < 4001) {
        HV_STR_SET(4, fmt(g_text(0x181), (int16_t)(int8_t)rec[9], (int16_t)(int8_t)rec[8]));
    } else if (g_hv_sel < 0) {
        HV_STR_SET(4, text_get_copy(0x180));
    } else {
        uint8_t *c = DS_ADDR(uint8_t, 0x1b50 + g_view_player * 0x46 + g_hv_sel * 7);
        int16_t days = (int16_t)(*(int16_t *)(c + 5) - g_day);
        const char *f = days == 0 ? g_text(0x1a7) : days == 1 ? g_text(0x1a8) : g_text(0x17f);
        HV_STR_SET(4, fmt(f, (int16_t)((int8_t)rec[9] - (int8_t)c[4]), (int16_t)(int8_t)rec[9], days));
    }
    layout_box(DS_ADDR(char, 0x0f79));
    hv_free(0);
    hv_free(3);
    hv_free(4);
    tooltip_present();
}

/* Fall 6000: Drehbuch / eigene Produktion */
static void hover_show(int16_t id)
{
    int16_t s = id % 1000;
    uint8_t *sh = g_shows + s * 0x1b3;
    int16_t prog = *(int16_t *)(sh + 6), i;
    g_hv_w = 0x12;
    g_hv_h = 8;
    for (i = 0; i < DS_I16(0x7652 + g_view_player * 2) && DS_I16(0x1a25 + g_view_player * 0x32 + i * 5) != s; i++)
        ;
    g_hv_i = i;
    uint8_t *rec = DS_ADDR(uint8_t, 0x1a25 + g_view_player * 0x32 + i * 5);
    if (*(uint16_t *)(sh + 5) & 1) {
        uint8_t *ser = SERIES(prog);
        HV_STR_SET(0, series_title_get(prog));
        HV_STR_SET(1, g_text((int8_t)ser[0x21] + 0x124));
        g_hv_val = (int8_t)ser[0x23];
        if (g_cur_room == 5)
            HV_STR_SET(2, fmt(g_text(0x183), (int16_t)(*(int16_t *)(rec + 3) - 1), (int16_t)(int8_t)ser[0x22]));
        else
            HV_STR_SET(2, fmt(g_text(0x184), (int16_t)(int8_t)ser[0x22]));
    } else {
        uint8_t *f = FILM(prog);
        HV_STR_SET(0, film_title_get(prog));
        HV_STR_SET(1, g_text((int8_t)f[0] + 0x1a));
        g_hv_val = (int8_t)f[4];
        if ((*(uint16_t *)(f + 7) & 4) || g_cur_room != 5)
            HV_STR_SET(2, fmt(DS_ADDR(char, 0x0fa5)));
        else
            HV_STR_SET(2, fmt(g_text(0x182), (int16_t)(*(int16_t *)(rec + 3) - 1)));
    }
    HV_STR_SET(3, show_text_title_get(s));
    HV_STR_SET(4, fmt(g_text(0x185), show_role_count(s), (int16_t)g_hv_val));
    layout_box(DS_ADDR(char, 0x0fa7));
    text_draw_right_to_free(0x88, 0x24, fmt(DS_ADDR(char, 0x0fc7), *(int32_t *)sh), g_popup_bmp);
    tooltip_present();
    hv_free(2);
    hv_free(4);
}

/* Fall 9000: laufende Produktion im Studio */
static void hover_studio(int16_t id)
{
    uint8_t *st = DS_ADDR(uint8_t, 0x6145 + DS_I16(0x79be) * 0x10);
    uint8_t *sh = g_shows + (id - 9000) * 0x1b3;
    int16_t prog = *(int16_t *)(sh + 6), k = 2;
    int8_t first = (int8_t)st[4], last = (int8_t)st[5];
    g_hv_w = 0x14;
    g_hv_h = 9;
    for (int16_t i = 2; i < 7; i++)
        HV_STR_SET(i, NULL);
    if (*(uint16_t *)(sh + 5) & 1) {
        HV_STR_SET(0, series_title_get(prog));
        HV_STR_SET(1, fmt(g_text(0x187), (int16_t)(first - 1), (int16_t)(int8_t)SERIES(prog)[0x22]));
        for (int16_t e = first; e <= last && k < 7; e++) { /* PORT: k < 7, Original ohne Grenze */
            const char *mark = episode_cast_complete(*(int16_t *)st, e) == 1 ? g_text(0x16a) : DS_ADDR(char, 0x0fe5);
            HV_STR_SET(k++, fmt(DS_ADDR(char, 0x0feb), e, episode_title_get(prog, e), mark));
        }
    } else {
        HV_STR_SET(0, film_title_get(prog));
        if (*(uint16_t *)(FILM(prog) + 7) & 4) { /* mehrteilig */
            HV_STR_SET(1, fmt(g_text(0x186), (int16_t)(first - 1)));
            for (int16_t e = first; e <= last && k < 7; e++) {
                const char *mark = episode_cast_complete(*(int16_t *)st, e) == 1 ? g_text(0x16a) : DS_ADDR(char, 0x0fcc);
                HV_STR_SET(k++, fmt(DS_ADDR(char, 0x0fd2), g_text(0x8e), e, mark));
            }
        } else {
            HV_STR_SET(1, fmt(DS_ADDR(char, 0x0fde)));
            if (last == 1) {
                const char *mark = episode_cast_complete(*(int16_t *)st, first) == 1 ? g_text(0x16a) : DS_ADDR(char, 0x0fdf);
                HV_STR_SET(2, fmt(g_text(0x8f), mark));
            } else {
                HV_STR_SET(2, text_get_copy(0x90));
            }
        }
    }
    layout_box(DS_ADDR(char, 0x0ff8));
    tooltip_present();
    hv_free(1);
    for (int16_t i = 2; i < 7; i++)
        if (HV_STR(i) != NULL)
            hv_free(i);
}

/* Fall 11000: Rolle (Darsteller) der Folge im Requisitenregal */
static void hover_role(int16_t id)
{
    uint8_t *sh = g_shows + g_hv_show * 0x1b3;
    uint8_t *e = sh + (g_hv_ep - 1) * 0x23;
    int16_t r = id - 11000;
    popup_box_create(g_hv_x, g_hv_y, (int16_t)(g_hv_x + g_hv_w), (int16_t)(g_hv_y + g_hv_h));
    g_hv_a = (int8_t)sh[(int8_t)e[r * 3 + 9] + 0x1a3];
    uint8_t *actor = DS_ADDR(uint8_t, 0x4ff3 + (g_hv_a - 1) * 0xc0 + g_hv_sel * 0xc);
    g_hv_j = (int8_t)e[0xa]; /* Folgenfaktor */
    g_hv_n1 = (int16_t)((int8_t)e[r * 3 + 9] - 1);
    const char *name = g_hv_sel == 0 ? g_text(0x188) : name_table_get(*(int16_t *)actor, 2);
    if (*(uint16_t *)(sh + 5) & 1)
        text_box_to_free(8, -4, fmt(DS_ADDR(char, 0x1034), show_text_get(g_hv_show, g_hv_n1), g_text(g_hv_a + 0x91), name),
                         0xc, 1, g_popup_bmp);
    else
        text_box_to_free(8, -4, fmt(DS_ADDR(char, 0x1040), g_text(g_hv_a + 0x91), name), 7, 1, g_popup_bmp);
    if (g_hv_sel > 0) {
        text_box_to_free(8, 0x1d, fmt(g_text(0x189), *(int32_t *)(actor + 2) * g_hv_j), 7, 1, g_popup_bmp);
        text_draw_to(0x3d, 0x1d, g_text(0x16b), g_popup_bmp);
    } else {
        text_box_to_free(8, 0x1d, fmt(DS_ADDR(char, 0x1047), g_text(0x14e)), 0xf, 1, g_popup_bmp);
    }
    if (g_hv_sel > 0) {
        blit_rect(hv_actor_image(*(int16_t *)(actor + 0xa)), g_popup_bmp, 0x69, 6, 0, 0, 0x20, 0x14);
        blit_sprite_masked(BANK_BMP(RBANK(0x108), 0x4a), g_popup_bmp, 0x69, 4, 0, 0, 0x20, 0x18);
        text_draw_to_free(0x69, 0x1d,
                          fmt(DS_ADDR(char, 0x104a), ((int8_t)actor[6] << 3) / 5, ((int8_t)actor[8] << 3) / 5,
                              ((int8_t)actor[9] << 3) / 5),
                          g_popup_bmp);
    } else {
        bmp_draw_masked(BANK_BMP(RBANK(0x108), sprite_ofs(g_hv_a)), g_popup_bmp, 0x69, 6);
        bmp_draw_masked(BANK_BMP(RBANK(0x108), 0x4a), g_popup_bmp, 0x69, 4);
    }
    tooltip_present();
}

/* Fall 12000: Rolle (Stab) */
static void hover_staff(int16_t id)
{
    uint8_t *e = g_shows + g_hv_show * 0x1b3 + (g_hv_ep - 1) * 0x23;
    g_hv_a = (int8_t)e[(id - 12000) * 3 + 9];
    popup_box_create(g_hv_x, g_hv_y, (int16_t)(g_hv_x + g_hv_w), (int16_t)(g_hv_y + g_hv_h));
    bmp_draw(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(g_hv_a - 0xf))), g_popup_bmp, 0x27, 6);
    g_hv_n2 = hv_staff_icon(g_hv_sel, g_hv_n2);
    bmp_draw_masked(BANK_BMP(RBANK(0x108), sprite_ofs((int16_t)(g_hv_n2 + 9))), g_popup_bmp, 0x27, 4);
    bmp_draw_masked(BANK_BMP(RBANK(0x108), 0x82), g_popup_bmp, 0xc, 0x1c);
    text_draw_centered_to(0x38, 0x15, g_text(g_hv_a + 0xc0), g_popup_bmp);
    int32_t fee = DS_I32(0x6215 + (g_hv_a - 0xf) * 0x10 + g_hv_sel * 4) * (int8_t)e[0xa];
    text_draw_to_free(0xe, 0x1f, fmt(g_text(0x18a), g_text(g_hv_sel + 0xec), fee), g_popup_bmp);
    tooltip_present();
}

/* Fall 18000: Quote der laufenden Stunde gegen die Anforderung des Werbespots */
static void hover_rating(void)
{
    int16_t p = g_local_player, h = hour_index(), ad = SCHED_AD(p, h), c;
    int16_t prog = schedule_get_programme(p, h);
    if (prog > 0 && prog < 1000)
        g_hv_a = (int8_t)FILM(prog)[0];
    else if (prog > 1000)
        g_hv_a = (int16_t)((int8_t)SERIES(prog - 1000)[0x21] + 10); /* ORIGINAL-BUG: Original indiziert mit prog statt prog-1000 */
    else
        g_hv_a = 0;
    g_hv_val2 = 0;
    if (ad != 0 && DS_I16(0x71cc) < 0x37) {
        for (g_hv_i = 0; g_hv_i < 10 && DS_I16(0x1b50 + p * 0x46 + g_hv_i * 7) != ad; g_hv_i++) /* PORT: Grenze */
            ;
        g_hv_val2 = REACH_LEVEL((int8_t)AD_REC(DS_I16(0x1b50 + p * 0x46 + g_hv_i * 7))[0xa]);
    }
    popup_box_create(g_hv_x, g_hv_y, (int16_t)(g_hv_x + 0xe), (int16_t)(g_hv_y + 6));
    card_band_fill(3, 9, 2);
    card_band_fill(0xc, 8, 1);
    card_band_fill(0x14, 8, 2);
    card_band_fill(0x1c, 8, 1);
    card_band_fill(0x24, 8, 2);
    int32_t viewers = DS_I32(0x74ca + p * 0x20 + h * 4);
    int16_t ok = 0;
    if (DS_I16(0x43f7 + p * 0x5a + h * 2) != 0 && (viewers >= g_hv_val2 || DS_I16(0x71cc) < 5) && ad != 0 &&
        ad_genre_allowed(ad, g_hv_a) != 0)
        ok = 1;
    if (ok || DS_I16(0x71cc) >= 0x37 || hour_index() <= 0)
        c = 1;
    else
        c = 3; /* Anforderung verfehlt */
    card_band_fill(0x2c, 9, c);
    if (hour_index() > 0 && DS_I16(0x71cc) >= 5) {
        HV_STR_SET(1, fmt_money(viewers, 2));
        HV_STR_SET(0, fmt(g_text(0x18f), HV_STR(1), (int16_t)rating_from_viewers(p, viewers)));
        hv_free(1);
    } else {
        HV_STR_SET(0, text_get_copy(0x190));
    }
    if (g_hv_val2 != 0 || ad != 0) {
        HV_STR_SET(2, fmt_money(g_hv_val2, 2));
        HV_STR_SET(1, fmt(g_text(0x191), HV_STR(2), (int16_t)rating_from_viewers(p, g_hv_val2)));
        hv_free(2);
    } else if (hour_index() > 0 && ad == 0) {
        HV_STR_SET(1, text_get_copy(0x192));
    } else {
        HV_STR_SET(1, text_get_copy(0x190));
    }
    HV_STR_SET(2, fmt_money(*(int32_t *)(STATION(p, 0) + 4) * 0x9a0, 1));
    text_draw_to_free(8, -4, fmt(g_text(0x193), HV_STR(2), HV_STR(0), HV_STR(1)), g_popup_bmp);
    hv_free(2);
    tooltip_present();
    hv_free(0);
    hv_free(1);
}

/* 239a:1568 – Tooltip/Infokarte für Objekt id zeigen (0 = ausblenden). Tausenderstelle = Art:
 * 0 Film, 1 Serie, 2 Senderkarte, 3/4 Werbung, 5 fertiger Text, 6 Drehbuch, 7 Fernseher,
 * 9 Studio, 10 Serienfolge, 11/12 Rolle, 13 Darsteller, 14/15 Statistik, 16 Geschenk, 17 Geld, 18 Quote */
/* PORT: Text des Geld-Kastens (englisch "Money:~ $#_9l ~Credit:~ $#_9l", deutsch
 * "Geld:~ #_9l $~Kredit:~ #_9l $", 8 Spalten breit). Das Original setzt ihn unverändert ein; bei
 * mehr als 9 Stellen passt eine Zeile samt Leerzeichen nicht mehr, wird umbrochen, und die
 * zusätzliche Zeile schiebt "Credit" unter den Kasten (im Flur auch über den Rahmen hinaus, wo sie
 * stehen bleibt). Dann werden schrittweise, bis der Text wieder 4 Zeilen ergibt, die Leerzeichen
 * vor einem Umbruch, nach einem Umbruch und vor dem '$' entfernt. Normale Beträge bleiben
 * unverändert. */
static char *money_text(void)
{
    char *t = fmt(g_text(0x18e), money_available(g_local_player), DS_I32(0x7abe));
    for (int pass = 0; pass < 3 && text_line_count(t, 8, 1) > 4; pass++) {
        char *w = t;
        for (const char *r = t; *r; r++)
            if (!(*r == ' ' && (pass == 0   ? r[1] == '~'
                                : pass == 1 ? r > t && r[-1] == '~'
                                            : r[1] == '$')))
                *w++ = *r;
        *w = '\0';
    }
    return t;
}

void hover_dispatch(int16_t id)
{
    clip_reset();
    if (id == 0) {
        if (g_hv_last == 0 && g_card_dirty != 1)
            return;
        screen_update();
        g_hv_last = g_hv_last_x = g_hv_last_y = 0;
        g_card_dirty = 0;
        DS_I16(0x79f8) = 0;
        return;
    }
    if ((g_hv_last != 0 && (g_hv_y != g_hv_last_y || g_hv_x != g_hv_last_x)) || g_hv_force != 0) {
        screen_update();
        DS_I16(0x79f8) = 0;
        g_hv_force = 0;
    }
    if (id == g_hv_last && g_hv_y == g_hv_last_y && g_hv_x == g_hv_last_x && g_card_dirty != 1) {
        g_hv_last_x = g_hv_x;
        g_hv_last_y = g_hv_y;
        return;
    }
    DS_I16(0x79f8) = 1;
    g_card_dirty = 0;
    if (g_cur_room != 0x19 || g_cur_dialog != 1) {
        mouse_hide();
        g_hv_mouse_hidden = 1;
    }
    int16_t n = id % 1000;
    switch ((uint16_t)id / 1000) {
    case 0:
        info_card_film_small(id);
        break;
    case 1: { /* Serie */
        uint8_t *ser = SERIES(n);
        g_hv_w = 0x11;
        g_hv_h = 0xc;
        HV_STR_SET(0, series_title_get(n));
        HV_STR_SET(1, series_descr_get(n));
        HV_STR_SET(2, g_text((int8_t)ser[0x21] + 0x124));
        HV_STR_SET(3, fmt(g_text(0x178), (int8_t)ser[0x35] << 2, (int8_t)ser[0x36] << 2));
        if (*(int16_t *)(ser + 0x31) == 0)
            *(int16_t *)(ser + 0x31) = 1;
        HV_STR_SET(4, fmt(g_text(0x179), (int16_t)(int8_t)ser[0x22], (int16_t)(int8_t)ser[0x23], *(int16_t *)(ser + 0x31),
                        episode_title_get(n, *(int16_t *)(ser + 0x31))));
        HV_STR_SET(5, (*(uint16_t *)(ser + 0x26) & 2) ? g_text(0x27) : DS_ADDR(char, 0x0eca));
        layout_box(text_line_count(series_title_get(n), 0x10, 1) == 0 ? DS_ADDR(char, 0x0efc) : DS_ADDR(char, 0x0ecc));
        text_draw_right_to_free(0x88, 0x3c, fmt(DS_ADDR(char, 0x0f2c), series_price(n)), g_popup_bmp);
        tooltip_present();
        hv_free(3);
        hv_free(4);
        break;
    }
    case 2:
        hover_station(id);
        break;
    case 3:
    case 4:
        hover_ad(id);
        break;
    case 5: /* Text schon in room_hover vorbereitet */
        layout_box(g_hv_spec);
        tooltip_present();
        hv_free(0);
        break;
    case 6:
        hover_show(id);
        break;
    case 7: /* Fernseher: Sender-Infokarte */
        if (DS_I16(0x7a40) == 1)
            channel_programme_title_load();
        g_hv_w = 0x10;
        popup_box_create(g_hv_x, g_hv_y, (int16_t)(g_hv_x + 0x10), (int16_t)(g_hv_y + 6));
        channel_info_card_draw(g_popup_bmp);
        tooltip_present();
        break;
    case 9:
        hover_studio(id);
        break;
    case 10: /* Serienfolge: id = (Serie+1000)*10 + Folge-1 */
        g_hv_w = 0x13;
        g_hv_h = 6;
        g_hv_a = (int16_t)(id / 10 - 1000);
        g_hv_i = (int16_t)(id - g_hv_a * 10 - 9999);
        HV_STR_SET(0, episode_title_get(g_hv_a, g_hv_i));
        HV_STR_SET(1, episode_descr_get(g_hv_a, g_hv_i));
        layout_box(DS_ADDR(char, 0x101c));
        tooltip_present();
        break;
    case 11:
        hover_role(id);
        break;
    case 12:
        hover_staff(id);
        break;
    case 13: /* Darsteller im Regal */
        g_hv_w = 0xc;
        g_hv_h = 7;
        g_hv_a = DS_I16(0x4ff3 + (DS_I16(0x79c2) - 1) * 0xc0 + (id - 13000) * 0xc);
        HV_STR_SET(0, fmt(DS_ADDR(char, 0x1059), name_table_get(g_hv_a, 2), name_table_get(g_hv_a, 1)));
        layout_box(DS_ADDR(char, 0x1061));
        tooltip_present();
        hv_free(0);
        break;
    case 14: { /* Statistik: Film */
        uint8_t *f = FILM(n);
        g_hv_w = 0x14;
        g_hv_h = 4;
        DS_I16(0x8f74) = (int8_t)f[0];
        HV_STR_SET(0, film_title_get(n));
        HV_STR_SET(2, fmt_money(g_hv_val, 3));
        HV_STR_SET(1, fmt(g_text(0x18b), g_text(DS_I16(0x8f74) + 0x1a), g_hv_n1, (int16_t)(int8_t)f[4], HV_STR(2),
                        (int16_t)g_hv_val2));
        hv_free(2);
        layout_box(DS_ADDR(char, 0x107d));
        tooltip_present();
        hv_free(1);
        DS_I16(0x8f70) = g_hv_n1;
        break;
    }
    case 15: { /* Statistik: Serie */
        uint8_t *ser = SERIES(n);
        g_hv_w = 0x14;
        g_hv_h = 6;
        DS_I16(0x8f74) = (int8_t)ser[0x21];
        HV_STR_SET(0, series_title_get(n));
        HV_STR_SET(1, fmt(DS_ADDR(char, 0x108d), g_hv_n2, episode_title_get(n, g_hv_n2)));
        HV_STR_SET(3, fmt_money(g_hv_val, 3));
        HV_STR_SET(2, fmt(g_text(0x18c), g_text(DS_I16(0x8f74) + 0x124), g_hv_n1, (int16_t)(int8_t)ser[0x23], HV_STR(3),
                        (int16_t)g_hv_val2));
        hv_free(3);
        layout_box(DS_ADDR(char, 0x1094));
        tooltip_present();
        hv_free(2);
        hv_free(1);
        DS_I16(0x8f72) = g_hv_n2;
        DS_I16(0x8f70) = g_hv_n1;
        break;
    }
    case 16: { /* Geschenk; > 100 = schon gekauft */
        g_hv_n1 = (int16_t)(id - 16000);
        if (g_hv_n1 > 100)
            g_hv_n1 = (int16_t)(id - 16100);
        int16_t g = g_hv_n1;
        tooltip_show_text(fmt(g_text(0x18d), g_text(DS_I16(0x722a + g * 8)), DS_I32(0x722c + g * 8),
                              (int16_t)(DS_I8(0x7231 + g * 8) + 1)));
        break;
    }
    case 17:
        tooltip_show_text(money_text());
        break;
    case 18:
        hover_rating();
        break;
    default:
        break;
    }
    if (g_hv_mouse_hidden == 1)
        mouse_show();
    g_hv_last_y = g_hv_y;
    g_hv_last_x = g_hv_x;
    g_hv_last = id;
}

/* 239a:3132 – Senderinfo im Fernseher (Bitmap DS:8e6a, bei Bedarf neu aufbauen) */
void channel_info_show(void)
{
    if (g_card_dirty != 0) {
        if (DS_I16(0x7a40) != 0)
            channel_programme_title_load();
        g_card_dirty = 0;
        bmp_fill(DS_PTR(Bitmap, 0x8e6a), 0, 0, 0x11, 0x38, 0);
        draw_frame_cells(0, 0, 0x10, 6, DS_PTR(Bitmap, 0x8e6a), 1);
        channel_info_card_draw(DS_PTR(Bitmap, 0x8e6a));
    }
    draw_sprite(DS_PTR(Bitmap, 0x8e6a), 0x40, 0x30);
}

/* 239a:31b2 */
void money_tooltip_show(void)
{
    text_window_draw(0x11, 9, 0x19, 0xd, money_text(), 1);
}

/* 239a:31f5 */
void day_tooltip_show(void)
{
    text_window_draw(0x10, 0xc, 0x16, 0xd, fmt(g_text(0x51), g_day), 1);
}

/* 239a:3228 – Infokarte des Senders DS:79de: laufendes Programm/Werbung, Bewertung, Zuschauer */
void channel_info_card_draw(Bitmap *dst)
{
    int16_t ch = DS_I16(0x79de), h = hour_index();
    int16_t ad = SCHED_AD(ch, h), prog;
    int32_t viewers = DS_I32(0x248f + ch * 10);
    char *s = NULL;
    g_hv_w = 0x10;
    DS_SET_PTR(0x8ec2, dst);
    card_band_fill(3, 9, 3);
    card_band_fill(0xc, 0x20, 2);
    card_band_fill(0x2c, 9, 1);
    text_draw_to(8, -4, CHANNEL_NAME(ch), dst);
    int16_t rating = (int16_t)rating_from_viewers(ch, viewers);
    for (g_hv_j = h; (prog = DS_I16(0x43f7 + ch * 0x5a + g_hv_j * 2)) == 9999; g_hv_j--)
        ;
    if (DS_I16(0x71cc) < 5) {
        text_draw_to(8, 5, g_text(0x194), dst);
    } else if ((DS_I16(0x71cc) < 0x37 && prog == 0) || (DS_I16(0x71cc) > 0x36 && ad == 0)) {
        text_draw_to(8, 5, g_text(0x195), dst);
    } else if (DS_I16(0x71cc) < 0x37) { /* Programm */
        if (prog < 1000) {
            uint8_t *f = FILM(prog);
            s = (int8_t)f[4] < 2 ? fmt(DS_ADDR(char, 0x10ac)) : fmt(g_text(0x196), (int16_t)(h - g_hv_j + 1));
            text_box_to_free(8, 5, fmt(DS_ADDR(char, 0x10ae), g_text((int8_t)f[0] + 0x1a), DS_ADDR(char, 0x775c), s), 0xf, 1,
                             dst);
            far_free(s);
            if (*(uint16_t *)(f + 7) & 8)
                text_draw_to(0x40, -4, g_text(0x1a3), dst);
            s = NULL;
        } else {
            uint8_t *ser = SERIES(prog - 1000);
            s = (int8_t)ser[0x23] < 2 ? fmt(DS_ADDR(char, 0x10b6)) : fmt(g_text(0x196), (int16_t)(h - g_hv_j + 1));
            text_box_to_free(8, 5,
                             fmt(DS_ADDR(char, 0x10b8), g_text((int8_t)ser[0x21] + 0x124), DS_ADDR(char, 0x775c),
                                 DS_I16(0x4415 + ch * 0x5a + g_hv_j * 2), DS_ADDR(char, 0x77f2), s),
                             0xf, 1, dst);
            far_free(s);
            s = NULL;
        }
    } else { /* Werbeblock */
        int16_t n = ad % 1000;
        text_draw_to(8, 5, MEM_PTR(char, AD_REC(n)), dst);
        switch ((uint16_t)ad / 1000) {
        case 0:
            s = fmt(DS_ADDR(char, 0x10c6));
            break;
        case 1:
        case 3:
            for (g_hv_j = 0; g_hv_j < DS_I16(0x7688 + ch * 2) && DS_I16(0x1b50 + ch * 0x46 + g_hv_j * 7) != n; g_hv_j++)
                ;
            if ((uint16_t)ad / 1000 == 1)
                s = fmt(g_text(0x197), (int16_t)(DS_I8(0x1b54 + ch * 0x46 + g_hv_j * 7) + 1), (int16_t)AD_REC(n)[9]);
            else
                s = fmt(g_text(0x199), (int16_t)DS_I8(0x1b54 + ch * 0x46 + g_hv_j * 7), (int16_t)AD_REC(n)[9]);
            break;
        case 2:
            s = text_get_copy(0x198);
            break;
        }
        if (s != NULL)
            text_draw_to(8, 0x15, s, dst);
        far_free(s);
        s = NULL;
    }
    if (DS_I16(0x71cc) > 4 && DS_I16(0x71cc) < 0x37) { /* Erfolg der Stunde */
        int8_t res = DS_I8(0x5533 + ch * 8 + hour_index());
        const char *t = res == 0 ? g_text(0x19a) : res == 2 ? g_text(0x19b) : NULL;
        if (t != NULL)
            text_draw_to(0x68, -4, t, dst);
    }
    char *money;
    if (viewers < 1000000)
        s = fmt(DS_ADDR(char, 0x10e6), (money = fmt_money(viewers, 2)), rating);
    else
        s = fmt(DS_ADDR(char, 0x10ca), (money = fmt_money(viewers, 1)), rating);
    text_draw_to_free(8, 0x25, s, dst);
    far_free(money);
}

/* 239a:3849 – Titel des laufenden Programms von Sender DS:79de in DS:775c (Folge: DS:77f2) kopieren */
void channel_programme_title_load(void)
{
    int16_t loaded = 0;
    if (DS_I16(0x71cc) > 4 && DS_I16(0x71cc) < 0x37) {
        int16_t ch = DS_I16(0x79de);
        for (g_hv_j = hour_index(); (g_hv_a = DS_I16(0x43f7 + ch * 0x5a + g_hv_j * 2)) == 9999; g_hv_j--)
            ;
        if (g_hv_a >= 1 && g_hv_a <= 999) {
            if (film_title_get(g_hv_a) == NULL) { /* nicht im Cache: nachladen */
                loaded = -1;
                set_cursor_shape(3);
                text_cache_titles(0, g_hv_a);
                set_cursor_shape(0);
            }
            const char *t = film_title_get(g_hv_a);
            far_memcpy(DS_ADDR(char, 0x775c), t, (uint16_t)str_length(t));
            DS_U8(0x775c + str_length(t)) = 0;
            if (loaded)
                text_cache_titles_free(-1);
        } else if (g_hv_a > 1000) {
            g_hv_a -= 1000;
            if (series_title_get(g_hv_a) == NULL) {
                loaded = -1;
                set_cursor_shape(3);
                text_cache_descr(0, g_hv_a);
                set_cursor_shape(0);
            }
            const char *t = series_title_get(g_hv_a);
            far_memcpy(DS_ADDR(char, 0x775c), t, (uint16_t)str_length(t));
            DS_U8(0x775c + str_length(t)) = 0;
            const char *e = episode_title_get(g_hv_a, DS_I16(0x4415 + ch * 0x5a + g_hv_j * 2));
            far_memcpy(DS_ADDR(char, 0x77f2), e, (uint16_t)str_length(e));
            DS_U8(0x77f2 + str_length(e)) = 0;
            if (loaded)
                text_cache_descr_free(-1);
        }
    }
    DS_I16(0x7a40) = 0;
}

/* 239a:3a2f – Zielgruppen-Bedingung des Werbekunden als Text nach HV_STR(0) */
void ad_requirements_text_build(int16_t ad)
{
    int8_t g = (int8_t)AD_REC(ad)[0xc];
    if (g == 0) {
        HV_STR_SET(0, fmt(DS_ADDR(char, 0x1101)));
        return;
    }
    if (g > 0) {
        ad_targets_text_build(ad, 1);
        HV_STR_SET(0, fmt(g_text(0x4e), HV_STR(4)));
    } else {
        ad_targets_text_build(ad, -1);
        HV_STR_SET(0, fmt(g_text(0x4d), HV_STR(4)));
    }
    hv_free(4);
}

/* 239a:3af8 – bis zu drei Genres (+0xc..+0xe des Werbekunden, Vorzeichen sign) als Text HV_STR(4) */
void ad_targets_text_build(int16_t ad, int16_t sign)
{
    uint8_t *rec = AD_REC(ad);
    for (int16_t i = 1; i < 4; i++) {
        int8_t v = (int8_t)rec[0xb + i];
        char *s;
        if (v == 0) {
            s = fmt(DS_ADDR(char, 0x1108));
        } else {
            int16_t n = (int16_t)(v * sign);
            const char *name = n < 0xb ? g_text(n + 0x1a) : g_text(n + 0x11a);
            if (i == 1 && rec[0xe] != 0)
                s = fmt(DS_ADDR(char, 0x1103), name);
            else if (i < 3 && rec[0xc + i] != 0)
                s = fmt(g_text(0x50), name);
            else
                s = fmt(name);
        }
        HV_STR_SET(i, s);
    }
    HV_STR_SET(4, fmt(DS_ADDR(char, 0x1109), HV_STR(1), HV_STR(2), HV_STR(3)));
    hv_free(1);
    hv_free(2);
    hv_free(3);
}

/* 239a:3cb1 – einfacher Text-Tooltip an der Hover-Position (Text wird freigegeben) */
void tooltip_show_text(const char *text)
{
    popup_box_show(g_hv_x, g_hv_y, (int16_t)(g_hv_x + g_hv_w), (int16_t)(g_hv_y + g_hv_h), (char *)text);
}

/* 239a:3cdb – waagerechtes Farbband in der Infokarte */
void card_band_fill(int16_t y, int16_t h, int16_t style)
{
    draw_tile_box_to(0, y, g_hv_w, h, style, g_popup_bmp);
}

/* 239a:3cff – Infokarte nach Beschreibung aufbauen: je Zeile 4 Zeichen "BTM ":
 * B = Farbband 1..3 (sonst keins), T = Textbaustein HV_STR(T), M = maximale Zeilen */
void layout_box(const char *spec)
{
    int16_t y = 3;
    popup_box_create(g_hv_x, g_hv_y, (int16_t)(g_hv_x + g_hv_w), (int16_t)(g_hv_y + g_hv_h));
    for (int16_t i = 0; i < g_hv_h; i++) {
        int16_t hh = (i == 0 || i == g_hv_h - 1) ? 9 : 8;
        int8_t c = (int8_t)(spec[i * 4] - '0');
        if (c > 0 && c < 4)
            card_band_fill(y, hh, c);
        y += hh;
    }
    y = -4;
    for (int16_t i = 0; i < g_hv_h; i++) {
        int8_t c = (int8_t)(spec[i * 4 + 1] - '0');
        if (c >= 0 && c < 10)
            text_box(8, y, HV_STR(c), (int16_t)(g_hv_w - 1), 1, g_popup_bmp, (int16_t)(int8_t)(spec[i * 4 + 2] - '0'));
        y += 8;
    }
}

/* 239a:3dff */
void tooltip_present(void)
{
    popup_box_present(g_hv_x, g_hv_y);
}

/* 239a:3e0f – Infokarte eines Films */
void info_card_film_small(int16_t film)
{
    uint8_t *f = FILM(film);
    int8_t genre = (int8_t)f[0];
    g_hv_w = 0x11;
    g_hv_h = 0xc;
    HV_STR_SET(0, film_title_get(film));
    HV_STR_SET(1, film_descr_get(film));
    HV_STR_SET(2, g_text(genre + 0x1a));
    if (genre < 6 || genre > 8)
        HV_STR_SET(3, fmt(g_text(0x19c), g_text((int8_t)f[0x13] + 0x13e), (int16_t)(int8_t)f[0x14], (int8_t)f[0x15] << 2,
                        (int8_t)f[0x16] << 2));
    else if (genre == 6)
        HV_STR_SET(3, fmt(g_text(0x19d), g_text((int8_t)f[0x13] + 0x13e), (int16_t)(int8_t)f[0x14], (int8_t)f[0x15] << 2,
                        (int8_t)f[0x16] << 2));
    else if (genre == 8)
        HV_STR_SET(3, fmt(g_text(0x19e), (int8_t)f[0x13] << 2, (int8_t)f[0x14] << 2));
    else
        HV_STR_SET(3, fmt(DS_ADDR(char, 0x1110)));
    HV_STR_SET(4, NULL);
    if (*(uint16_t *)(f + 7) & 2)
        HV_STR_SET(4, g_text(0x27));
    if (g_cur_room == 8 && DS_I8(0x6f59 + g_local_player * 0x6c + genre) > 3)
        HV_STR_SET(4, g_text(0x28)); /* Genre im Archiv schon reichlich vorhanden */
    int16_t live_day = *(int16_t *)(f + 0xf);
    if (genre == 7 && live_day != 0) { /* Live-Sport mit Termin */
        if (film == 0x1d)
            HV_STR_SET(6, fmt(g_text(0x19f), 3 - DS_I16(0x7a22), 3));
        else if (film == 0xbb)
            HV_STR_SET(6, fmt(g_text(0x19f), 5 - DS_I16(0x7a7c), 5));
        else
            HV_STR_SET(6, fmt(DS_ADDR(char, 0x1112)));
        if (live_day - g_day < 2)
            HV_STR_SET(5, fmt(g_text(0x1a9), live_day == g_day ? g_text(0xce) : g_text(0xcf),
                            (int16_t)(*(int16_t *)(f + 0x11) + 0x11), HV_STR(6)));
        else
            HV_STR_SET(5, fmt(g_text(0x1a0), (int16_t)(live_day - g_day), HV_STR(6)));
        hv_free(6);
    } else if (genre == 7 || genre == 8) {
        HV_STR_SET(5, fmt(DS_ADDR(char, 0x1114)));
    } else {
        /* ohne Qualitätswert 40 */
        int16_t v = (int8_t)f[1] != 0 ? ((int8_t)f[3] * 0x28) / (int8_t)f[1] : 0x28;
        HV_STR_SET(5, fmt(g_text(0x1a1), v));
    }
    HV_STR_SET(6, fmt(g_text(0x1a2), (int16_t)(int8_t)f[4]));
    layout_box(text_line_count(film_title_get(film), 0x10, 1) < 2 ? DS_ADDR(char, 0x1116) : DS_ADDR(char, 0x1146));
    text_draw_right_to_free(0x88, 0x44, fmt(DS_ADDR(char, 0x1176), film_price(film)), g_popup_bmp);
    if (*(uint16_t *)(f + 7) & 8)
        text_draw_right_to_free(0x88, 0x4c, text_get_copy(0x1a3), g_popup_bmp);
    tooltip_present();
    hv_free(3);
    hv_free(5);
    hv_free(6);
}
