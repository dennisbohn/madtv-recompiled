/* Segment 1581 – Klickauswertung: Verteilung nach Raum und die Klick-Handler der meisten Räume */
#include "game.h"

#define g_text(id)      PTR32(char, DS_ADDR(ptr32, 0x8370)[id])
#define g_click_hotspot DS_I16(0x8ee3) /* Bereich unter dem Mauszeiger (hotspot_at_mouse) */
#define g_click_lock    DS_I16(0x7a7a)
#define g_tv_player     DS_I16(0x79de) /* Spieler, dessen Programm der Fernseher zeigt */
#define g_tv_mode       DS_I16(0x79f0)

/* Pause (Raum -5) beginnen bzw. beenden; PORT: aus room_click herausgezogen, damit auch die Taste P
 * und die Menüleiste sie nutzen (game_menu_poll) */
void pause_begin(void)
{
    g_click_lock = 0;
    room_enter(-5);
    clock_suspend();
    DS_I32(0x728e) = DS_I32(0x6123); /* Uhrzeit merken */
    clock_resume();
    mouse_hide();
    bmp_copy(DS_PTR(Bitmap, 0x8e66), DS_PTR(Bitmap, 0x8e88));
    text_draw_centered_to(0x15, -6, g_text(0x121), DS_PTR(Bitmap, 0x8e88));
    scr_blit(DS_PTR(Bitmap, 0x8e88), 0x13, 0xb4);
    mouse_show();
}

void pause_end(void)
{
    /* Uhrzeit von vor der Pause wiederherstellen */
    g_click_lock = 0;
    clock_suspend();
    DS_I32(0x6123) = DS_I32(0x728e);
    clock_resume();
    room_reload();
    clock_update(1);
    DS_I16(0x79ae) = 1;
}

/* PORT: Pause möglich? Wie beim Klick auf den Start-Knopf: nicht in Pause und nicht während
 * DS:012c (Bildschirm "Neues Spiel", Weg zum Chef) */
int16_t pause_allowed(void)
{
    return g_cur_room != -5 && DS_I16(0x012c) == 0;
}

/* 1581:0009 – Linksklick auswerten */
void room_click(void)
{
    DS_SET_PTR(0x8ef9, DS_ADDR(uint8_t, 0x8bbc + g_local_player * 0x17)); /* eigener Spieler */
    g_click_hotspot = hotspot_at_mouse(0);

    if (g_cur_room == -5) {
        pause_end();
        return;
    }
    if (g_cur_room == 0x42 && g_click_lock == 0) {
        clock_start();
        room_leave();
        return;
    }
    if ((g_cur_room == -1 || g_cur_room == -3) && g_click_lock == 0) {
        room_leave(); /* Meldung/Info schließen */
        return;
    }

    if (mouse_y() < 0x78 && g_click_lock == 0) {
        switch (g_cur_room) {
        case 0x00: room_click_building(); break;
        case 0x0d: room_click_info_board(); break;
        case 0x3d: room_click_elevator_board(); break;
        case 0x01: room_click_office(); break;
        case 0x3a: room_click_save_menu(); break;
        case 0x1f: room_click_schedule(); break;
        case 0x02: room_click_news_studio(); break;
        case 0x19: room_click_station_map(); break;
        case 0x04: room_click_movie_agency(); break;
        case 0x08: room_click_auction(); break;
        case 0x0b: room_click_ad_agency(); break;
        case 0x09: room_click_screenplay_agency(); break;
        case 0x0a:
        case 0x3e: room_click_real_estate(); break;
        case 0x05: room_click_studio(); break;
        case 0x0c: room_click_supermarket(); break;
        case 0x3f: room_click_supermarket_shelves(); break;
        case 0x0e: room_click_betty(); break;
        case 0x03: room_click_boss(); break;
        case 0x40: room_click_porter(); break;
        case 0x41: room_click_new_game_setup(); break;
        default: break;
        }
        return;
    }

    /* untere Leiste: Spielerlämpchen */
    if (mouse_in_rect(0x17, 0xb9, 0x52, 0xc8) && g_click_lock == 0) {
        if (mouse_in_rect(0x17, 0xb9, 0x25, 0xc8)) {
            /* erstes Lämpchen: Fernseher an/aus */
            if (g_cur_room == 0x1f && g_cur_dialog == 0xa)
                panel_slide_toggle(DS_ADDR(int16_t, 0x79f0), 0x80, 0x3c);
            else
                g_tv_mode = (int16_t)(0x1e - g_tv_mode);
            tv_refresh();
        } else {
            int16_t p = (int16_t)((mouse_x() - 0x26) / 0xf + 1);
            if (DS_U8(0x8bd0 + p * 0x17) == 0) {
                g_tv_player = p;
                if (g_tv_mode == 0)
                    tv_show_current_programme();
                DS_I16(0x7a40) = 1;
                DS_I16(0x79ae) = 1;
            }
        }
        status_bar_draw();
        return;
    }

    /* Start-Knopf: Pause */
    if (!mouse_in_rect(0x99, 0xb5, 0xc1, 0xbd) && g_click_lock == 0)
        return;
    if (!pause_allowed())
        return;
    pause_begin();
}

#define RBANK(ofs)      DS_PTR(uint8_t, ofs)
#define BANK_BMP(bank, ofs) ((Bitmap *)((uint8_t *)(bank) + (ofs)))

#define g_dialog        g_cur_dialog
#define FIG(o)          DS_I16(0x7b22 + g_player_idx * 0x1e + (o)) /* Figur des Menschen */
#define FLOOR_B(f, o)   DS_I8(0x7c62 + (f) * 0x7a + (o))
#define FLOOR_U(f, o)   DS_U8(0x7c62 + (f) * 0x7a + (o))
#define BOARD_FLOOR(r, c) DS_I8(0x8a40 + ((r) - 1) * 8 + (c) * 2) /* Hinweistafel: Etage/Tür je Feld */
#define BOARD_DOOR(r, c)  DS_I8(0x8a41 + ((r) - 1) * 8 + (c) * 2)
#define g_drag_bitmap   DS_PTR(Bitmap, 0x0390)

/* 1581:19f0 – Gebäude: Klick auf Fahrstuhlschacht (0x1a) oder Etage (0x1b) setzt das Laufziel */
void room_click_building(void)
{
    if (FIG(0x16) >= 2 || FIG(0xe) >= 2) /* fährt gerade Fahrstuhl / ist beschäftigt */
        return;
    if (g_click_hotspot == 0x1a) {
        FIG(6) = (int16_t)((DS_I16(0x8c2e) - mouse_y() + 0x6d) / 0x1e + 1);
        FIG(4) = 0x99;
        FIG(0x12) = -2;
        DS_I16(0x79ca) = 0;
    }
    if (g_click_hotspot != 0x1b)
        return;
    int16_t fl = (int16_t)((DS_I16(0x8c2e) - mouse_y() + 0x6d) / 0x1e + 1);
    FIG(4) = -1;
    for (int16_t d = 0; FLOOR_B(fl, d + 0x14) != 0; d++) {
        DS_I16(0x8f8e) = FLOOR_U(fl, d + 0x14);
        int16_t t = DS_I16(0x8f8e);
        if (!((t != 0 && t < 4) || t == 6 || t == 10))
            continue;
        DS_I16(0x8f8e) = FLOOR_U(fl, d);
        if (mouse_x() < DS_I16(0x8f8e) || mouse_x() > DS_I16(0x8f8e) + 0x10)
            continue;
        FIG(6) = fl;
        FIG(0x12) = d;
        if (FIG(6) == FIG(2)) {
            FIG(4) = (int16_t)(FLOOR_U(fl, d) + 4);
            FIG(0x16) = 0;
        } else {
            FIG(4) = 0x99; /* erst zum Fahrstuhl */
        }
        FIG(8) = 0;
        DS_I16(0x79ca) = 0;
    }
    if (FIG(4) == -1) { /* freie Stelle auf der Etage */
        FIG(0x12) = -1;
        if (FIG(2) == fl) {
            FIG(4) = mouse_x();
            if (FIG(4) > 0xeb)
                FIG(4) = 0xec;
            FIG(6) = fl;
            FIG(8) = 0;
            FIG(0x16) = 0;
        } else {
            if (FIG(0x16) == 0)
                FIG(4) = 0x98;
            FIG(6) = fl;
            FIG(8) = mouse_x();
        }
        DS_I16(0x79ca) = 0;
    }
}

static int16_t board_hit(int16_t r, int16_t c, int16_t *x0, int16_t *y0)
{
    int16_t off = c < 2 ? 0 : 9;
    *x0 = (int16_t)(c * 0x4b + off + 6);
    *y0 = (int16_t)(0x6e - r * 8);
    return mouse_y() >= *y0 && mouse_y() <= 0x75 - r * 8 && mouse_x() >= *x0 && mouse_x() <= c * 0x4b + off + 0x50;
}

static int16_t sign_colour(int16_t floor, int16_t door)
{
    return FLOOR_B(floor, door + 0x46) == 0 ? 5 : (int16_t)(FLOOR_U(floor, door + 0x46) + 1);
}

/* 1581:1bbd – Hinweistafel: Türschilder aufnehmen (Dialog 0) und vertauschen (Dialog 1) */
void room_click_info_board(void)
{
    int16_t x0, y0, colour = 5;
    if (g_dialog == 0) {
        for (int16_t r = 1; r < 0xe; r++) {
            for (int16_t c = 0; c < 4; c++) {
                if (board_hit(r, c, &x0, &y0)) {
                    DS_I16(0x8f8e) = BOARD_DOOR(r, c);
                    if (DS_I16(0x8f8e) < 100) {
                        DS_I16(0x8f8c) = BOARD_FLOOR(r, c);
                        if (FLOOR_B(DS_I16(0x8f8c), DS_I16(0x8f8e) + 0x28) != 0 ||
                            FLOOR_B(DS_I16(0x8f8c), DS_I16(0x8f8e) + 0x1e) == 5) {
                            DS_I16(0x79a8) = BOARD_FLOOR(r, c);
                            DS_I16(0x79aa) = BOARD_DOOR(r, c);
                            colour = sign_colour(DS_I16(0x79a8), DS_I16(0x79aa));
                            cursor_attach_item(0x48, 8, x0, y0, door_name_text(DS_I16(0x79a8), DS_I16(0x79aa)), colour);
                            BOARD_DOOR(r, c) += 100; /* Platz als leer markieren */
                            building_directory_panel();
                            screen_update();
                            g_dialog = 1;
                        }
                    }
                }
                if (g_drag_bitmap != NULL)
                    break;
            }
            if (g_drag_bitmap != NULL)
                return;
        }
    } else if (g_dialog == 1) {
        for (int16_t r = 1; r < 0xe; r++) {
            for (int16_t c = 0; c < 4; c++) {
                if (board_hit(r, c, &x0, &y0)) {
                    DS_I16(0x8f8e) = BOARD_FLOOR(r, c);
                    DS_I16(0x8f8c) = BOARD_DOOR(r, c);
                    if (FLOOR_B(DS_I16(0x8f8e), DS_I16(0x8f8c) + 0x28) != 0 ||
                        FLOOR_B(DS_I16(0x8f8e), DS_I16(0x8f8c) + 0x1e) == 5 || DS_I16(0x8f8c) > 99) {
                        if (BOARD_DOOR(r, c) < 100) { /* Schild tauschen */
                            DS_I16(0x79ac) = BOARD_FLOOR(r, c);
                            DS_I16(0x79b4) = BOARD_DOOR(r, c);
                            colour = sign_colour(DS_I16(0x79ac), DS_I16(0x79b4));
                        } else {
                            DS_I16(0x79ac) = 0;
                        }
                        BOARD_FLOOR(r, c) = (int8_t)DS_I16(0x79a8);
                        BOARD_DOOR(r, c) = (int8_t)DS_I16(0x79aa);
                        cursor_drop_item();
                        if (DS_I16(0x79ac) == 0) {
                            g_dialog = 0;
                        } else {
                            DS_I16(0x79a8) = DS_I16(0x79ac);
                            DS_I16(0x79aa) = DS_I16(0x79b4);
                            cursor_attach_item(0x48, 8, -1, 0, door_name_text(DS_I16(0x79ac), DS_I16(0x79b4)), colour);
                        }
                        building_directory_panel();
                        screen_update();
                    }
                }
                if (g_dialog == 0)
                    break;
            }
            if (g_dialog == 0)
                return;
        }
    }
}

/* 1581:201c – Tafel im Fahrstuhl: Ziel-Etage/-Tür wählen */
void room_click_elevator_board(void)
{
    static const int16_t door_x[4] = {0x50, 0x74, 0xb4, 0xd8};
    int16_t x0, y0;
    for (int16_t r = 1; r < 0xe; r++) {
        for (int16_t c = 0; c < 4; c++) {
            if (board_hit(r, c, &x0, &y0)) {
                DS_I16(0x8f8e) = BOARD_FLOOR(r, c);
                DS_I16(0x8f8c) = BOARD_DOOR(r, c);
                if (FLOOR_B(DS_I16(0x8f8e), DS_I16(0x8f8c) + 0x28) != 0 || FLOOR_B(DS_I16(0x8f8e), DS_I16(0x8f8c) + 0x1e) == 5 ||
                    (r == 1 && (c == 3 || c == 0))) {
                    int16_t d;
                    FIG(6) = r;
                    if (r == 1 && c == 3) {
                        d = 3;
                    } else if (r == 1 && c == 0) {
                        d = 0;
                    } else {
                        for (d = 0; d < 4 && FLOOR_U(r, d) != door_x[c]; d++)
                            ;
                    }
                    FIG(0x12) = d;
                    FIG(4) = (int16_t)(FLOOR_U(r, d) + 4);
                    room_reload();
                }
            }
            if (g_cur_room == 0)
                break;
        }
        if (g_cur_room == 0)
            return;
    }
}

/* 1581:2220 – eigenes Büro */
void room_click_office(void)
{
    if (g_dialog == 1) {
        screen_update();
        g_dialog = 0;
        return;
    }
    switch (g_click_hotspot) {
    case 0x1c:
        room_enter(0x19); /* Senderkarte */
        return;
    case 0x1d:
        room_enter(0x1f); /* Programmplaner */
        return;
    case 0x1e:
        draw_sprite(BANK_BMP(RBANK(0x100), 2), 0x3f, 0xc);
        screen_update();
        room_enter(0x3a); /* Spielstände */
        return;
    case 0x36:
        if (DS_I16(0x79a6 + (DS_I16(0x7a16) + 0x3a) * 2) == 0)
            return;
        player_info_popup(DS_I16(0x7a16));
        g_dialog = 1;
        return;
    case 0x44:
        room_leave();
        return;
    case 0x49: /* Fallblock-Spiel starten */
        if (DS_I16(0x7a7e) == 0)
            return;
        DS_I16(0x7a54) = 0;
        minigame_panel(0);
        screen_update();
        g_dialog = 2;
        return;
    case 0x45:
        DS_I16(0x7a62) = 2;
        draw_sprite(BANK_BMP(RBANK(0x100), 0x8a), 0xbd, 0x59);
        break;
    case 0x46:
        DS_I16(0x7a62) = 1;
        draw_sprite(BANK_BMP(RBANK(0x100), 0x92), 0xca, 0x59);
        break;
    case 0x47:
        DS_I16(0x7a62) = 3;
        draw_sprite(BANK_BMP(RBANK(0x100), 0x9a), 0xbd, 0x65);
        break;
    case 0x48:
        DS_I16(0x7a62) = 4;
        draw_sprite(BANK_BMP(RBANK(0x100), 0xa2), 0xca, 0x65);
        break;
    case 0x3e: /* schneller */
        if (DS_I16(0x7a5e) - 1 > 0)
            DS_I16(0x7a5e)--;
        else
            DS_I16(0x7a5e) = 1;
        return;
    case 0x4a:
        DS_I16(0x7a62) = 5; /* fallen lassen */
        return;
    default:
        return;
    }
    screen_update();
}

#define g_item_prog     DS_I16(0x8ee9) /* getragenes Programm */
#define g_me            DS_PTR(uint8_t, 0x8ef9)
#define ME_MONEY        (*(int32_t *)(g_me + 8))
#define FILM(n)         (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)       (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define CASE_ITEM(i)    DS_I16(0x2463 + (i) * 2) /* Koffer */
#define g_case_count    DS_I16(0x732a)

static void drag_cassette(int16_t grab_x, int16_t grab_y)
{
    cursor_attach_item(0x10, 0x1d, grab_x, grab_y, NULL, 0xb);
    drag_image_draw(BANK_BMP(RBANK(0x104), 2));
}

static int32_t prog_price(int16_t prog)
{
    return prog < 1000 ? film_price(prog) : series_price(prog - 1000);
}

/* 1581:0345 – Filmhändler (Raum 8): Angebote kaufen, Koffer, Auktion */
void room_click_auction(void)
{
    int16_t i, c;
    switch (g_dialog) {
    case 0:
        for (i = 1; i < 0xb; i++) {
            int16_t o = i * 8;
            if (!mouse_in_rect(DS_I16(0x32e + o), DS_I16(0x330 + o), DS_I16(0x332 + o), DS_I16(0x334 + o)) ||
                DS_I8(0x19d4 + o) < 0)
                continue;
            if (money_check(g_local_player, DS_I32(0x19cf + o)) == 0) { /* Angebot vom Regal nehmen */
                room_animate(2);
                DS_I8(0x19d4 + o) -= 100;
                g_item_prog = (int16_t)(DS_I16(0x19cd + o) + 10000);
                drag_cassette(DS_I16(0x32e + o), DS_I16(0x330 + o));
                room8_items_panel(1);
                screen_update();
                int32_t price = g_item_prog < 11000 ? film_price(g_item_prog - 10000) : series_price(g_item_prog - 11000);
                ME_MONEY -= price;
                money_add(g_item_prog < 11000 ? 1 : 2, price);
                g_dialog = 1;
                status_money_update();
            }
            break;
        }
        if (g_click_hotspot == 0x21) { /* aus dem Koffer nehmen */
            i = (int16_t)((mouse_x() - 0xe0) / 9 + ((mouse_y() - 0x1c) / 0x1e) * 5);
            if (CASE_ITEM(i) != 0) {
                g_item_prog = CASE_ITEM(i);
                case_remove(i);
                drag_cassette((int16_t)(mouse_x() - 5), (int16_t)(mouse_y() - 0x14));
                dialog_frame(2, 2);
                screen_update();
                for (i = 1; i <= 10; i++) {
                    if (DS_I16(0x19cd + i * 8) == g_item_prog) { /* stammt vom Regal: zurücklegbar */
                        g_dialog = 5;
                        g_item_prog = (int16_t)(g_item_prog + 10000);
                        return;
                    }
                }
                g_dialog = 5;
                return;
            }
        }
        if (g_click_hotspot == 0x23) { /* zur Auktion */
            if (image_at_least(g_local_player, 10) == 0) {
                show_message(0xcb);
            } else {
                dialog_frame(1, 2);
                music_on_leave(8, 0);
                auction_panel(0);
                screen_update();
                g_dialog = 4;
            }
        }
        break;
    case 1:
    case 5:
        if (g_click_hotspot == 0x21) {
            if (g_case_count < 10)
                offer_list_add_selected();
            else
                show_message(0x66);
        } else if (g_click_hotspot == 0x18 || (g_click_hotspot == 0x23 && g_item_prog > 10000)) {
            if (g_item_prog < 10001)
                show_message(0x67);
            else
                auction_withdraw_item();
        } else if (g_click_hotspot == 0x23) { /* eigenes Programm versteigern */
            cursor_drop_item();
            int16_t p = g_item_prog;
            if ((p < 1000 && (*(uint16_t *)(FILM(p) + 6) & 0x40)) ||
                (p > 1000 && (*(uint16_t *)(SERIES(p - 1000) + 0x25) & 0x40))) {
                for (i = 1; DS_I16(0x1aed + i * 9) != 0 && i <= 10; i++)
                    ;
                if (i < 0xb) {
                    uint8_t *lot = DS_ADDR(uint8_t, 0x1aed + i * 9);
                    *(int16_t *)lot = p;
                    lot[7] = (uint8_t)g_local_player;
                    lot[6] = 0;
                    lot[8] = 0;
                    *(int32_t *)(lot + 2) = prog_price(p);
                    ME_MONEY += prog_price(p);
                    money_add(p < 1000 ? 0xb : 0xc, prog_price(p));
                    /* Release-Fassung: zusätzlich Bit 0x20 löschen (in der Auktion, nicht auf dem
                     * Markt; auction_day_end setzt es zurück, wenn das Los unverkauft bleibt) */
                    if (p < 1000) {
                        FILM(p)[5] = 0;
                        *(uint16_t *)(FILM(p) + 6) &= ~0x20;
                    } else {
                        SERIES(p - 1000)[0x24] = 0;
                        *(uint16_t *)(SERIES(p - 1000) + 0x25) &= ~0x20;
                    }
                    g_dialog = 0;
                    status_money_update();
                    return;
                }
            }
            programme_sell_selected();
        }
        break;
    case 4: /* Auktion: Gebot abgeben */
        for (c = 0; c < 2; c++) {
            for (i = 1; i < 6; i++) {
                int16_t lot = c * 5 + i;
                if (mouse_in_rect((int16_t)(c * 0x8e + 0x14), (int16_t)(i * 0x13 - 7), (int16_t)(c * 0x8e + 0x9c),
                                  (int16_t)(i * 0x13 + 0xb)) &&
                    DS_I16(0x1aed + lot * 9) != 0 && trade_item_toggle(lot) == 1) {
                    auction_panel((int16_t)(lot + 100));
                    screen_update();
                    status_money_update();
                    DS_I16(0x79ae) = 1;
                }
            }
        }
        break;
    default:
        break;
    }
}

/* 1581:088c – Archiv (Raum 4): Regal nach Genre, Koffer, Serienfolgen */
void room_click_movie_agency(void)
{
    uint8_t *arch = DS_ADDR(uint8_t, 0x6ef9 + DS_I16(0x7a16) * 0x6c);
    int16_t i;
#define ARCH(g, k) (*(int16_t *)(arch + (g) * 8 + (k) * 2))
#define ARCH_N(g)  ((int8_t)arch[0x60 + (g)])
    if (g_dialog == 0) {
        if (g_click_hotspot == 0x1f && DS_I16(0x79aa) == 0xb) { /* Serie: Folgen ansehen */
            mouse_hide();
            DS_I16(0x79b2) = 2;
            g_dialog = 2;
            mouse_show();
        }
        if (DS_I16(0x79aa) != 0 && g_click_hotspot == 0x20) { /* Kassette aus dem Regal */
            i = (int16_t)((mouse_x() - 0xa0) / 9);
            if (ARCH(DS_I16(0x79aa), i) > 0 && i < ARCH_N(DS_I16(0x79aa)) &&
                programme_check_sellable(i, DS_I16(0x79aa)) == 1) {
                g_item_prog = ARCH(DS_I16(0x79aa), i);
                if (DS_I16(0x79aa) == 0xb)
                    g_item_prog = (int16_t)(g_item_prog + 1000);
                DS_I16(0x79a8) = i;
                programme_remove_from_archives(g_item_prog);
                archive_shelf_panel(DS_I16(0x7a16));
                screen_update();
                drag_cassette((int16_t)(DS_I16(0x79a8) * 9 + 0xa0), 0x30);
                g_dialog = 3;
            }
        }
        if (g_click_hotspot == 0x22 && DS_I16(0x79b0) == 0) { /* Genre wählen */
            genre_menu_dialog(0x12, 1, 0);
            g_dialog = 1;
            screen_update();
        }
        if (g_click_hotspot == 0x21) { /* Koffer */
            i = (int16_t)((mouse_x() - 0x18) / 9);
            if (i < g_case_count) {
                g_item_prog = CASE_ITEM(i);
                DS_I16(0x79ac) = i;
                case_remove(i);
                DS_I16(0x79aa) = g_item_prog < 1000 ? (int8_t)FILM(g_item_prog)[0] : 0xb;
                archive_shelf_panel(DS_I16(0x7a16));
                dialog_frame(2, 1);
                screen_update();
                drag_cassette((int16_t)(DS_I16(0x79ac) * 9 + 0x18), 0x2c);
                g_dialog = 4;
            }
        }
    } else if (g_dialog == 1) {
        if (g_click_hotspot == 0x17) {
            DS_I16(0x79aa) = (int16_t)((mouse_y() - 0xc) / 8 + 1);
            genre_menu_dialog(0, 0, 1);
            g_dialog = 0;
            archive_shelf_panel(DS_I16(0x7a16));
            screen_update();
            DS_I16(0x79b0) = 0;
        }
    } else if (g_dialog == 2) {
        if (mouse_in_rect(0xa0, 0x30, 0x140, 0x4d)) {
            i = (int16_t)((mouse_x() - 0xa0) / 9);
            if (ARCH(DS_I16(0x79aa), i) > 0) {
                DS_I16(0x79b4) = ARCH(DS_I16(0x79aa), i);
                mouse_hide();
                DS_I16(0x79b2) = 1;
                mouse_show();
                series_episode_dialog(0);
            }
        }
    } else if (g_dialog == 3 || g_dialog == 4) {
        if (g_click_hotspot == 0x21) {
            if (g_case_count < 10)
                dealer_offer_return();
            else
                show_message(0x66);
        } else if (g_click_hotspot == 0x20) { /* ins Regal stellen */
            if (g_item_prog < 1000) {
                DS_I16(0x79aa) = (int8_t)FILM(g_item_prog)[0];
                DS_I16(0x79a8) = ARCH_N(DS_I16(0x79aa));
                if (DS_I16(0x79a8) > 3) { /* Fach voll */
                    char *s = fmt(g_text(0x64), g_text((int8_t)FILM(g_item_prog)[0] + 0x1a));
                    show_message_text(s);
                    far_free(s);
                    return;
                }
            } else {
                DS_I16(0x79aa) = 0xb;
                DS_I16(0x79a8) = ARCH_N(0xb);
                if (DS_I16(0x79a8) > 3) {
                    show_message(0x65);
                    return;
                }
            }
            archive_add_selected();
        }
    }
#undef ARCH
#undef ARCH_N
}

#define g_view_player   DS_I16(0x7a16)
#define g_sched_scroll  DS_I16(0x79a6)
#define SCHED(p, h)     DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define SCHED_AD(p, h)  DS_I16(0x4433 + (p) * 0x5a + (h) * 2)
#define CONTRACT(p, i)  DS_I16(0x1b50 + (p) * 0x46 + (i) * 7)
#define AD_NAME(n)      (MEM_PTR(char, DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)))

static int16_t prog_hours(int16_t prog)
{
    return prog < 1000 ? (int8_t)FILM(prog)[4] : (int8_t)SERIES(prog - 1000)[0x23];
}

/* Index des Werbevertrags mit Kunde ad (im Original ohne Grenze gesucht) */
static int16_t contract_index(int16_t p, int16_t ad)
{
    int16_t i;
    for (i = 0; i < 10 && CONTRACT(p, i) != ad; i++)
        ;
    return i;
}

static void drag_ad(int16_t ad, int16_t x, int16_t y)
{
    cursor_attach_item(0x40, 0x10, x, y, fmt(DS_ADDR(char, 0x05c0), AD_NAME(ad)), 0);
}

/* Maßstab der Statistik aus dem größten Zuschauerwert (Vielfache von 4 Mio., gerundet) */
static void statistics_open(void)
{
    uint32_t best = 0;
    for (int16_t p = 1; p < 4; p++) {
        if (DS_U8(0x8bd0 + p * 0x17) != 0)
            continue;
        for (int16_t h = 1; h < 8; h++) {
            uint32_t v;
            if ((v = DS_U32(0x74ca + p * 0x20 + h * 4)) > best)
                best = v;
            if ((v = DS_U32(0x789e + p * 0x20 + h * 4)) > best)
                best = v;
            if ((v = DS_U32(0x6d89 + p * 0x20 + h * 4)) > best)
                best = v;
        }
    }
    uint32_t q = (best + 0x3d08ff) / 0x3d0900;
    int16_t s = max_i16((int16_t)q, (int16_t)(q >> 16));
    if (s < 0 || s > 4) {
        if (s >= 0 && s <= 9)
            s = 10;
        else if (s >= 0 && s <= 0x13)
            s = 0x14;
        else if (s >= 0 && s <= 0x18)
            s = 0x19;
        else if (s >= 0 && s <= 0x1d)
            s = 0x1e;
        else if (s >= 0 && s <= 0x27)
            s = 0x28;
        else if (s >= 0 && s <= 0x31)
            s = 0x32;
        else
            s = 0x3c;
    }
    if (s == 0)
        s = 1;
    DS_I16(0x79dc) = s;
    set_cursor_shape(3);
    text_cache_titles_free(0);
    text_cache_descr_free(0);
    text_cache_titles(99, DS_I16(0x79da));
    text_cache_descr(99, DS_I16(0x79da));
    statistics_panel();
    set_cursor_shape(0);
}

/* 1581:0c78 – Programmplaner: Programme/Werbung verschieben, Listen, Statistik, Optionen */
void room_click_schedule(void)
{
    int16_t p = g_view_player, k;
    int16_t h = (int16_t)((mouse_y() - 4) / 0x10 + g_sched_scroll + 1);
    int16_t y = (int16_t)((h - g_sched_scroll - 1) * 0x10 + 4);
    if (g_click_hotspot == 0xb && g_sched_scroll != 0 && (g_dialog == 0 || g_dialog == 4)) {
        g_sched_scroll = 0;
    } else if (g_click_hotspot == 0xc && g_sched_scroll == 0 && (g_dialog == 0 || g_dialog == 4)) {
        g_sched_scroll = 7;
    } else if (g_dialog == 0) {
        if (g_click_hotspot == 0x37) {
            if (DS_I8(0x7227) > 0) { /* Briefkasten */
                player_message_dialog(p);
                g_dialog = 8;
            }
        } else if (g_click_hotspot == 0x38) {
            DS_I16(0x79d8) = 1;
            balance_sheet();
            g_dialog = 9;
        }
        if (g_click_hotspot == 0xd) {
            g_dialog = 5;
            ad_contract_list_dialog(0x16, 1, 0, p);
            screen_update();
        }
        if (g_click_hotspot == 0xe) {
            g_dialog = 1;
            genre_menu_dialog(0x19, 1, 0);
        }
        if (g_click_hotspot == 0x45) {
            g_dialog = 10;
            options_panel();
        }
        if (g_click_hotspot == 0x33) {
            g_dialog = 7;
            DS_I16(0x79d8) = 1;
            statistics_open();
        }
        if (g_click_hotspot == 10) { /* Papierkorb */
            clip_reset();
            draw_sprite(BANK_BMP(RBANK(0x100), 0x52), 0xc, 0x5d);
            screen_update();
            mouse_hide();
            DS_I16(0x79b2) = 2;
            g_dialog = 4;
            mouse_show();
        }
        if (g_click_hotspot == 0xf && schedule_hover_editable() == 1) { /* Programm aufnehmen */
            for (k = 0; (DS_I16(0x79b6) = SCHED(p, h + k)) == 9999; k--)
                ;
            h = (int16_t)(h + k);
            y = (int16_t)((h - 1 - g_sched_scroll) * 0x10 + 4);
            archive_find_selected();
            if (DS_I16(0x79aa) != 0) {
                int16_t prog = DS_I16(0x79b6);
                DS_I16(0x79ae) = 1;
                if (prog < 1000)
                    cursor_attach_item(0x90, (int16_t)((int8_t)FILM(prog)[4] << 4), 0x48, y, schedule_slot_title(h, p), 0);
                else
                    cursor_attach_item(0x90, (int16_t)((int8_t)SERIES(prog - 1000)[0x23] << 4), 0x48, y,
                                       archive_title_by_id(SCHED(p, h)), 0);
                schedule_remove_hour(h);
                g_dialog = 3;
            }
        }
        if (g_click_hotspot != 0x10 || schedule_hover_editable() != 1)
            return;
        DS_I16(0x79b6) = SCHED_AD(p, h); /* Werbung aufnehmen */
        if (DS_I16(0x79b6) != 0) {
            DS_I16(0x79b8) = contract_index(p, DS_I16(0x79b6));
            drag_ad(DS_I16(0x79b6), 0xd8, y);
            SCHED_AD(p, h) = 0;
            schedule_view(p);
            screen_update();
            g_dialog = 6;
        }
        return;
    } else if (g_dialog == 1) { /* Genre gewählt */
        if (g_click_hotspot != 0x17)
            return;
        DS_I16(0x79aa) = (int16_t)((mouse_y() - 0xc) / 8 + 1);
        g_dialog = 2;
        archive_list_dialog(0x13, 8, 0, (uint8_t)DS_I8(0x7ca8 + FIG(2) * 0x7a + FIG(0x12)));
        screen_update();
        return;
    } else if (g_dialog == 2) { /* Programm aus der Archivliste */
        if (g_click_hotspot != 0x18)
            return;
        DS_I16(0x79a8) = (int16_t)((mouse_y() - 0x48) / 10);
        DS_I16(0x79b6) = DS_I16(0x6ef9 + p * 0x6c + DS_I16(0x79aa) * 8 + DS_I16(0x79a8) * 2);
        int16_t prog = DS_I16(0x79b6);
        if (prog != 0) {
            if (prog < 1000 && DS_I16(0x79aa) == 8 && (*(uint16_t *)(FILM(prog) + 7) & 4) &&
                (*(uint16_t *)(FILM(prog) + 7) & 2)) {
                show_message(0x2f); /* Live-Sendung schon gelaufen */
                DS_I16(0x79ae) = 1;
                return;
            }
            clip_reset();
            archive_list_dialog(0, 0, 1, p);
            genre_menu_dialog(0, 0, 1);
            schedule_view(p);
            screen_update();
            char *t = archive_item_title(DS_I16(0x79aa), DS_I16(0x79a8), p);
            int16_t len = DS_I16(0x79aa) < 0xb ? (int8_t)FILM(prog)[4] : (int8_t)SERIES(prog)[0x23];
            cursor_attach_item(0x90, (int16_t)(len << 4), 0x9c, (int16_t)(DS_I16(0x79a8) * 10 + 0x48), t, 0);
            g_dialog = 3;
        }
        DS_I16(0x79ae) = 1;
        return;
    } else if (g_dialog == 3) { /* Programm ablegen */
        if (g_click_hotspot != 0xf || schedule_hover_editable() != 1)
            return;
        for (k = 0; k < 0x18 && (DS_I16(0x79b6) = SCHED(p, h + k)) == 0; k++) /* PORT: Grenze */
            ;
        int16_t len = 0;
        if (DS_I16(0x79aa) < 0xb && DS_I16(0x79aa) > 0)
            len = (int8_t)FILM(DS_I16(0x6ef9 + p * 0x6c + DS_I16(0x79aa) * 8 + DS_I16(0x79a8) * 2))[4];
        else if (DS_I16(0x79aa) == 0xb)
            len = (int8_t)SERIES(DS_I16(0x6f51 + p * 0x6c + DS_I16(0x79a8) * 2))[0x23];
        if (len <= k)
            DS_I16(0x79b6) = 0; /* passt ohne Verdrängen */
        if (schedule_place_programme(DS_I16(0x79a8), DS_I16(0x79aa), h, p) != 1)
            return;
        DS_I16(0x79ae) = 1;
        cursor_drop_item();
        schedule_view(p);
        screen_update();
        archive_find_selected();
        if (DS_I16(0x79aa) == 0) {
            g_dialog = 0;
            return;
        }
        char *t = archive_item_title(DS_I16(0x79aa), DS_I16(0x79a8), p);
        cursor_attach_item(0x90, (int16_t)(prog_hours(DS_I16(0x79b6)) << 4), 0x48, y, t, 0);
        g_dialog = 3;
        return;
    } else if (g_dialog == 4) { /* Papierkorb aktiv */
        if (g_click_hotspot == 0xf && schedule_hover_editable() == 1) {
            for (k = h; SCHED(p, k) == 9999; k--)
                ;
            schedule_remove_hour(k);
        }
        if (g_click_hotspot != 0x10 || schedule_hover_editable() != 1)
            return;
        SCHED_AD(p, h) = 0;
    } else if (g_dialog == 5) { /* Vertrag aus der Liste */
        if (g_click_hotspot != 0x19)
            return;
        int16_t i = (int16_t)((mouse_y() - 0x12) / 10);
        if (CONTRACT(p, i) == 0)
            return;
        DS_I16(0x79b8) = i;
        DS_I16(0x79b6) = CONTRACT(p, i);
        clip_reset();
        ad_contract_list_dialog(0, 0, 1, 0);
        schedule_view(p);
        screen_update();
        cursor_attach_item(0x40, 0x10, 0xb0, (int16_t)(i * 10 + 0x12), fmt(DS_ADDR(char, 0x05c6), AD_NAME(DS_I16(0x79b6))), 0);
        g_dialog = 6;
        return;
    } else if (g_dialog == 6) { /* Werbung ablegen */
        if (g_click_hotspot != 0x10 || schedule_hover_editable() != 1)
            return;
        DS_I16(0x79b6) = SCHED_AD(p, h);
        if (schedule_place_ad(DS_I16(0x79b8), h, p) != 1)
            return;
        DS_I16(0x79ae) = 1;
        cursor_drop_item();
        schedule_view(p);
        screen_update();
        if (DS_I16(0x79b6) != 0) { /* verdrängten Spot weitertragen */
            DS_I16(0x79b8) = contract_index(p, DS_I16(0x79b6));
            cursor_attach_item(0x40, 0x10, 0xd8, y, fmt(DS_ADDR(char, 0x05cc), AD_NAME(DS_I16(0x79b6))), 0);
            return;
        }
        g_dialog = 0;
        return;
    } else if (g_dialog == 7) { /* Statistik */
        if (DS_I16(0x7a4e) == 0) {
            int16_t hs = g_click_hotspot;
            if (hs >= 4 && hs <= 6 && hs - 3 != DS_I16(0x79da) && DS_U8(0x8bd0 + (hs - 3) * 0x17) == 0) {
                DS_I16(0x79da) = (int16_t)(hs - 3);
                statistics_panel();
            } else if (hs == 0x34) {
                if (++DS_I16(0x79d8) > 2)
                    DS_I16(0x79d8) = 0;
                if (g_day == 1 && DS_I16(0x79d8) == 0)
                    DS_I16(0x79d8) = 1;
                statistics_panel();
            }
        }
        if (g_click_hotspot == 0x4c) {
            DS_I16(0x7a4e) = (int16_t)(1 - DS_I16(0x7a4e));
            statistics_panel();
        }
        return;
    } else if (g_dialog == 8) { /* Nachricht gelesen */
        screen_update();
        g_dialog = 0;
        return;
    } else if (g_dialog == 9) { /* Bilanz heute/gestern */
        if (g_click_hotspot == 0x34) {
            DS_I16(0x79d8) = (int16_t)(1 - DS_I16(0x79d8));
            if (g_day == 1)
                DS_I16(0x79d8) = 1;
            balance_sheet();
        }
        return;
    } else if (g_dialog == 10) { /* Optionen */
        switch (g_click_hotspot) {
        case 0x3f:
        case 0x40:
        case 0x41: /* Spieltempo */
            options_speed_set(g_click_hotspot == 0x3f ? 0x14 : g_click_hotspot == 0x40 ? 0x1e : 0x3c);
            return;
        case 0x46:
        case 0x47:
        case 0x48: /* Schalter Fernsehfamilie, Fernsehbild, Musik */
            options_switch_set((int16_t)(g_click_hotspot - 0x46), options_switch_on((int16_t)(g_click_hotspot - 0x46)) == 0);
            return;
        default:
            return;
        }
    } else {
        return;
    }
    schedule_view(p);
    screen_update();
}

#define NEWS_SEL(p, i)  DS_ADDR(uint8_t, 0x3d4b + (p) * 0x78 + (i) * 0x1e) /* gewählte Nachricht 1..3 */
#define g_news_carry    DS_ADDR(uint8_t, 0x6127) /* getragene Nachricht (0x1e Byte) */
#define g_news_bought   DS_I16(0x8eda)           /* getragene Nachricht ist neu (kostet) */

/* 1581:23ca – Nachrichtenstudio: Abos, Nachrichtenarchiv, drei Sendeplätze */
void room_click_news_studio(void)
{
    int16_t p = g_view_player, own = g_view_player == g_local_player, i, n;
    if (g_dialog == 0) {
        if (own) {
            int16_t abo = g_click_hotspot == 0x3f ? 1 : g_click_hotspot == 0x40 ? 2 : g_click_hotspot == 0x41 ? 3 : 0;
            if (abo != 0) {
                int32_t price = 0;
                int16_t need = 0;
                if (g_me[abo + 0x10] == 0) { /* abonnieren */
                    price = abo == 1 ? 1000 : 10000;
                    need = abo == 1 ? 5 : abo == 2 ? 10 : 15;
                }
                if (money_check(g_local_player, price) == 0) {
                    if (image_at_least(g_local_player, need) == 0) {
                        char *s = fmt(g_text(0xcc), g_text(abo + 0x121));
                        show_message_text(s);
                        far_free(s);
                    } else {
                        player_toggle_flag(p, abo);
                        if (g_me[abo + 0x10] != 0) {
                            ME_MONEY -= price;
                            money_add(10, price);
                        }
                        player_flags_draw(p);
                        screen_update();
                        status_money_update();
                    }
                }
            }
        }
        if (g_click_hotspot == 0x42 || (g_click_hotspot == 0x3e && own)) {
            news_slots_panel(p, 0);
            if (own) {
                for (i = 1; i < 4 && news_count_by_category(i) == 0; i++)
                    ;
                if (i < 4)
                    DS_I16(0x7a52) = i;
                DS_I16(0x7a50) = 0;
                player_tabs_panel(0);
            }
            screen_update();
            g_dialog = 1;
        }
    } else if (g_dialog == 1) {
        static const int16_t tab_hot[4] = {0, 0x3f, 0x40, 0x41};
        for (i = 1; i < 4; i++) {
            if (g_click_hotspot == tab_hot[i] && news_count_by_category(i) != 0 && DS_I16(0x7a52) != i) {
                DS_I16(0x7a52) = i;
                if (i != 1)
                    DS_I16(0x7a50) = 0;
                player_tabs_panel(2);
                screen_update();
            }
        }
        if (g_click_hotspot == 0x2f && DS_I16(0x7a50) > 0) {
            DS_I16(0x7a50) -= 2;
            if (DS_I16(0x7a50) < 0)
                DS_I16(0x7a50) = 0;
            player_tabs_panel(2);
            screen_update();
        }
        if (g_click_hotspot == 0x31 && news_count_by_category(DS_I16(0x7a52)) > DS_I16(0x7a50) + 2) {
            DS_I16(0x7a50) += 2;
            if (news_count_by_category(DS_I16(0x7a52)) < DS_I16(0x7a50) + 2)
                DS_I16(0x7a50) = (int16_t)(news_count_by_category(DS_I16(0x7a52)) - 2);
            player_tabs_panel(2);
            screen_update();
        }
        if (g_click_hotspot == 0x44 && own) { /* Nachricht aus dem Archiv nehmen */
            int16_t sel = -1;
            n = (int16_t)((mouse_y() - 0x29) / 0x22 + DS_I16(0x7a50) + 1);
            while (n != 0) {
                sel = news_find_next(DS_I16(0x7a52), sel);
                n--;
                if (sel == -1)
                    break;
            }
            if (sel >= 0) {
                g_news_bought = 1;
                far_memcpy(g_news_carry, DS_ADDR(uint8_t, 0x455f + g_local_player * 600 + sel * 0x1e), 0x1e);
                player_news_remove(g_local_player, sel);
                player_tabs_panel(2);
                screen_update();
                news_item_draw(g_news_carry, 0xc, (int16_t)(((mouse_y() - 0x29) / 0x22) * 0x22 + 0x29));
                g_dialog = 2;
            }
        }
        if (g_click_hotspot == 0x43 && own) { /* Nachricht vom Sendeplatz nehmen */
            i = (int16_t)((mouse_y() - 7) / 0x22);
            if (*(int16_t *)NEWS_SEL(p, i + 1) != 0) {
                far_memcpy(g_news_carry, NEWS_SEL(p, i + 1), 0x1e);
                news_item_draw(g_news_carry, 0xa4, (int16_t)(i * 0x1a + 0x12));
                *(int16_t *)NEWS_SEL(p, i + 1) = 0;
                news_slots_panel(p, 2);
                player_tabs_panel(2);
                screen_update();
                g_dialog = 2;
                g_news_bought = 0;
                DS_I16(0x7a76) = 0;
            }
        }
    } else if (g_dialog == 2 && g_click_hotspot == 0x43 && own) { /* auf einen Sendeplatz legen */
        int32_t price = genre_value_get(DS_I8(0x6144));
        if (g_news_bought == 0 || money_check(g_local_player, price) == 0) {
            uint8_t tmp[0x1e];
            if (g_news_bought != 0) {
                ME_MONEY -= price;
                money_add(10, price);
                status_money_update();
            }
            i = (int16_t)((mouse_y() - 7) / 0x22);
            if (*(int16_t *)NEWS_SEL(p, i + 1) == 0)
                *(int16_t *)tmp = 0;
            else
                far_memcpy(tmp, NEWS_SEL(p, i + 1), 0x1e);
            far_memcpy(NEWS_SEL(p, i + 1), g_news_carry, 0x1e);
            cursor_drop_item();
            if (*(int16_t *)tmp == 0) {
                g_dialog = 1;
            } else { /* verdrängte Nachricht weitertragen */
                far_memcpy(g_news_carry, tmp, 0x1e);
                news_item_draw(g_news_carry, 0x9c, (int16_t)(i * 0x1a + 0x12));
            }
            news_slots_panel(p, 2);
            player_tabs_panel(2);
            screen_update();
            DS_I16(0x79ae) = 1;
            DS_I16(0x7a76) = 1;
            g_news_bought = 0;
        }
    }
}

#define g_station_player DS_I16(0x79ac)
#define STATION(p, i)   DS_ADDR(uint8_t, 0x1ce9 + (p) * 0x1ce + (i) * 0xe) /* +0 Preis, +4 Reichweite, +8 x, +0xa y, +0xc Zustand */
#define ST_PRICE(s)     (*(int32_t *)(s))
#define ST_REACH(s)     (*(int32_t *)((s) + 4))
#define ST_STATE(s)     (*(int16_t *)((s) + 0xc))
#define g_station_bids  DS_ADDR(int32_t, 0x8eed) /* Gebote der beiden KI-Sender */

static void station_map_redraw(int16_t sel)
{
    DS_I16(0x79a8) = sel;
    station_map_panel(sel);
    screen_update();
}

/* 1581:29fc – Senderkarte: Masten setzen/verkaufen, Satellit/Kabel, Karten anderer Sender */
void room_click_station_map(void)
{
    int16_t hs = g_click_hotspot, i;
    switch (g_dialog) {
    case 0:
        if (hs == 0x16) { /* neuen Mast nehmen */
            for (i = 3; ST_REACH(STATION(g_view_player, i)) != 0 && i < 0x21; i++)
                ;
            if (i > 0x20) {
                show_message(0x62);
                return;
            }
            if (g_station_player != g_view_player) {
                g_station_player = g_view_player;
                station_map_panel(0);
                screen_update();
                stations_reach_update(g_local_player);
            }
            cursor_attach_item(0x10, 0x10, mouse_x(), mouse_y(), NULL, 0xb);
            drag_image_draw(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(g_station_player + 2))));
            DS_I16(0x79cc) = 0x30; /* Zeiger auf die Karte beschränken */
            DS_I16(0x79ce) = 4;
            DS_I16(0x79d0) = 200;
            DS_I16(0x79d2) = 0x61;
            g_dialog = 1;
            return;
        }
        if (hs == 0x11) { /* Mast verkaufen: ersten auswählen */
            g_dialog = 3;
            g_station_player = g_view_player;
            for (i = 1; i < 0x21; i++) {
                uint8_t *st = STATION(g_view_player, i);
                if (ST_REACH(st) > 0 && ST_STATE(st) != 2)
                    break;
            }
            if (i > 0x20)
                i = DS_I16(0x79a8);
            station_map_redraw(i);
            return;
        }
        if (hs > 3 && hs < 7) { /* Masten eines Senders zeigen */
            if (DS_U8(0x8bd0 + (hs - 3) * 0x17) != 0)
                return;
            g_station_player = (int16_t)(hs - 3);
            station_map_panel(0);
            screen_update();
            stations_reach_update(g_station_player);
            return;
        }
        if (hs > 6 && hs < 10) { /* Reichweitenkarte */
            if (DS_U8(0x8bd0 + (hs - 6) * 0x17) != 0)
                return;
            g_station_player = (int16_t)(hs - 6);
            station_map_panel(0);
            screen_update();
            station_map_view(g_station_player, 0);
            g_dialog = 2;
            return;
        }
        if (hs == 1 || hs == 2) { /* Satellit / Kabel umschalten */
            uint8_t *st = STATION(g_view_player, hs);
            if (ST_REACH(st) == 0 && ST_STATE(st) != 2 && money_check(g_view_player, ST_PRICE(st)) != 0)
                return;
            if (ST_REACH(st) == 0) {
                ST_REACH(st) = map_population_sum((int16_t)((hs - 1) * 0x54), 0, (int16_t)((hs - 1) * 0x54 + 0x53), 0x6d,
                                                  DS_PTR(uint16_t, 0x7c5e));
                ST_STATE(st) = 1;
            } else if (ST_STATE(st) == 1) {
                ST_REACH(st) = 0;
                ST_STATE(st) = 0;
            } else if (ST_STATE(st) == 2) {
                ST_STATE(st) = 0;
            } else if (ST_STATE(st) == 0) {
                ST_STATE(st) = 2;
            }
            g_station_player = g_local_player;
            stations_reach_update(g_local_player);
            station_map_panel(0);
            screen_update();
            status_money_update();
        }
        return;
    case 1: /* Mast auf die Karte setzen */
        if (hs != 3)
            return;
        {
            int16_t x = (int16_t)(mouse_x() - 0x30), y = (int16_t)(mouse_y() - 4);
            if (station_reach_gain(x, y) <= 0)
                return;
            if (money_check(g_view_player, round_to_step(station_reach_gain(x, y) * 0x268 / 10, 1000)) != 0)
                return;
            for (i = 3; ST_REACH(STATION(g_view_player, i)) != 0 && i <= 0x20; i++)
                ;
            if (i >= 0x21)
                return;
            uint8_t *st = STATION(g_view_player, i);
            ST_REACH(st) = station_reach_gain(x, y);
            ST_PRICE(st) = price_step_next(station_reach_at(x, y) * 0x268 / 10);
            *(int16_t *)(st + 8) = x;
            *(int16_t *)(st + 0xa) = y;
            ST_STATE(st) = 1;
            g_dialog = 0;
            cursor_drop_item();
            station_map_panel(0);
            screen_update();
            status_money_update();
            stations_reach_update(g_local_player);
        }
        return;
    case 2:
        if (hs > 6 && hs < 10) {
            g_station_player = (int16_t)(hs - 6);
            station_map_view(g_station_player, 1);
            g_dialog = 0;
        }
        return;
    case 3: /* Mast zum Verkauf auswählen */
        if (hs == 0x12) {
            i = station_prev_valid((int16_t)(DS_I16(0x79a8) - 1));
        } else if (hs == 0x13) {
            i = station_next_valid((int16_t)(DS_I16(0x79a8) + 1));
        } else if (hs == 0x14) { /* verkaufen */
            if (station_next_valid((int16_t)(DS_I16(0x79a8) + 1)) == DS_I16(0x79a8)) {
                show_message(99); /* letzter Mast */
                return;
            }
            uint8_t *st = STATION(g_view_player, DS_I16(0x79a8));
            if (ST_STATE(st) != 1) { /* Gebote der Computergegner einholen */
                uint8_t *own = STATION(g_local_player, DS_I16(0x79a8));
                g_station_bids[0] = ai_calc_bid(0, *(int16_t *)(own + 8), *(int16_t *)(own + 0xa));
                g_station_bids[1] = ai_calc_bid(1, *(int16_t *)(own + 8), *(int16_t *)(own + 0xa));
                station_bid_dialog(0, g_station_bids);
                screen_update();
                g_dialog = 4;
                return;
            }
            ST_REACH(st) = 0; /* heute gesetzt: einfach zurücknehmen */
            ST_STATE(st) = 0;
            DS_I16(0x79a8) = station_next_valid((int16_t)(DS_I16(0x79a8) + 1));
            DS_I16(0x79ae) = 1;
            station_map_panel(DS_I16(0x79a8));
            stations_reach_update(g_local_player);
            screen_update();
            status_money_update();
            return;
        } else {
            return;
        }
        DS_I16(0x79ae) = 1;
        station_map_redraw(i);
        return;
    case 4: { /* Gebot annehmen oder ablehnen */
        uint8_t *st = STATION(g_local_player, DS_I16(0x79a8));
        if (hs == 4 || hs == 5) {
            int16_t k = (int16_t)(hs - 4);
            int32_t bid = g_station_bids[k];
            if (bid <= 0)
                return;
            ME_MONEY += bid;
            money_add(0xd, bid);
            int16_t buyer = DS_I8(0xf2 + k);
            for (i = 3; i < 0x21 && ST_REACH(STATION(buyer, i)) != 0; i++) /* PORT: Grenze */
                ;
            if (i < 0x21)
                far_memcpy(STATION(buyer, i), st, 0xe);
            ST_REACH(st) = 0;
            ST_STATE(st) = 2;
        } else if (hs == 0x15) { /* ohne Käufer abreißen */
            ST_STATE(st) = 2;
            ST_REACH(st) = 0;
        } else {
            return;
        }
        station_select_next();
        return;
    }
    default:
        return;
    }
}

#define g_item          DS_I16(0x8eeb)
#define OFFER_AD(i)     DS_ADDR(uint8_t, 0x1980 + (i) * 7) /* Werbeangebote: +0 Kunde, +2 Zustand */
#define OFFER_SP(i)     DS_ADDR(uint8_t, 0x194a + (i) * 9) /* Drehbuchangebote: +0 Show, +2 Zustand, +4 Preis, +8 Folgen */
#define REACH_LEVEL(l)  DS_I32(0xa8 + ((l) - 1) * 4)
#define STUDIO(s)       DS_ADDR(uint8_t, 0x6145 + (s) * 0x10)
#define g_shows         DS_PTR(uint8_t, 0x713c)
#define PLAYER_NAME(p)  DS_PTR(char, 0x8bbc + (p) * 0x17)

static void drag_folder(int16_t sprite_ofs_, uint8_t *bank, int16_t gx, int16_t gy)
{
    cursor_attach_item(0x10, 0x1e, gx, gy, NULL, 0xb);
    drag_image_draw(BANK_BMP(bank, sprite_ofs_));
}

/* 1581:3156 – Werbeagentur: Angebote vom Regal nehmen, eigene Verträge zurückgeben */
void room_click_ad_agency(void)
{
    if (g_dialog == 0) {
        int16_t slot = 0;
        if (mouse_in_rect(0x6f, 0x4c, 0x84, 0x6a))
            slot = (int16_t)((mouse_x() - 0x68) / 8);
        else if (mouse_in_rect(0x83, 0x3d, 0x9f, 0x5b))
            slot = (int16_t)((mouse_x() - 0x7c) / 8 + 3);
        else if (mouse_in_rect(0x9d, 0x2b, 0xb2, 0x49))
            slot = (int16_t)((mouse_x() - 0x96) / 8 + 7);
        if ((int8_t)OFFER_AD(slot)[2] < 0 || *(int16_t *)OFFER_AD(slot) == 0)
            slot = 0;
        if (slot < 1) {
            if (mouse_in_rect(0xe4, 0x20, 0x111, 0x57)) { /* eigener Vertrag im Koffer */
                int16_t i = (int16_t)((mouse_x() - 0xe4) / 9 + ((mouse_y() - 0x20) / 0x20) * 5);
                int16_t ad = CONTRACT(g_local_player, i), j;
                if (ad != 0) {
                    for (j = 1; *(int16_t *)OFFER_AD(j) != ad && j < 0xb; j++)
                        ;
                    if (j > 10) {
                        show_message(0x69); /* schon begonnen, nicht mehr rückgebbar */
                        return;
                    }
                    DS_I16(0x8edc) = CONTRACT(g_local_player, i);
                    list_remove(g_local_player, i);
                    drag_folder(10, RBANK(0x104), (int16_t)(mouse_x() - 4), (int16_t)(mouse_y() - 0xf));
                    dialog_frame(2, 3);
                    screen_update();
                    g_dialog = 2;
                    return;
                }
            }
            if (g_click_hotspot == 0x23)
                adcontract_offers_commit();
        } else {
            int16_t ad = *(int16_t *)OFFER_AD(slot);
            uint8_t *rec = DS_ADDR(uint8_t, 0x3f2f + ad * 0x18);
            int16_t need = (int16_t)rating_from_viewers(g_local_player, REACH_LEVEL((int8_t)rec[0xa]));
            if ((rec[0xc] == 0 || image_at_least(g_local_player, 0x14) != 0) && image_at_least(g_local_player, need) != 0) {
                DS_I16(0x8edc) = ad;
                OFFER_AD(slot)[2] -= 100;
                drag_folder(10, RBANK(0x104), (int16_t)(mouse_x() - 4), (int16_t)(mouse_y() - 0xf));
                ad_offers_panel();
                screen_update();
                g_dialog = 1;
            } else { /* Image reicht nicht */
                char *s = fmt(g_text(0xcd), AD_NAME(ad));
                show_message_text(s);
                far_free(s);
            }
        }
    } else if (g_dialog == 1 || g_dialog == 2) {
        if (g_click_hotspot == 0x26) {
            adcontract_offer_reserve();
        } else if (g_click_hotspot == 0x21) {
            if (DS_I16(0x7688 + g_local_player * 2) < 10)
                adcontract_sign();
            else
                show_message(0x66);
        }
    }
}

/* 1581:353d – Drehbuchagentur */
void room_click_screenplay_agency(void)
{
    if (g_click_hotspot == 0x44 && g_dialog == 0) {
        screenplay_offers_commit();
        room_leave();
        return;
    }
    if (g_dialog == 0) {
        for (int16_t i = 1; i < 6; i++) {
            if (!mouse_in_rect((int16_t)(i * 7 + 99), 0x23, (int16_t)(i * 7 + 0x69), (int16_t)(0x40 - i)))
                continue;
            uint8_t *o = OFFER_SP(i);
            if ((int8_t)o[2] < 0 || *(int16_t *)o == 0 || money_check(g_local_player, *(int32_t *)(o + 4)) != 0)
                continue;
            g_item = *(int16_t *)o;
            DS_I16(0x8ee5) = (int8_t)o[8];
            cursor_attach_item(0x10, 0x1d, (int16_t)(i * 7 + 99), 0x23, NULL, 0xb);
            drag_image_draw(BANK_BMP(RBANK(0x104), 0x2a));
            ME_MONEY -= *(int32_t *)(o + 4);
            money_add(4, *(int32_t *)(o + 4));
            o[2] -= 100;
            production_offers_panel();
            status_money_update();
            screen_update();
            g_dialog = 1;
            break;
        }
        if (g_click_hotspot == 0x21) { /* eigenes Drehbuch zurückgeben */
            int16_t i = (int16_t)((mouse_x() - 0x1a) / 9), j;
            g_item = DS_I16(0x1a25 + g_local_player * 0x32 + i * 5);
            if (g_item >= 0) {
                for (j = 1; *(int16_t *)OFFER_SP(j) != g_item && j < 6; j++)
                    ;
                if (j < 6) {
                    DS_I16(0x8ee5) = DS_I8(0x1a27 + g_local_player * 0x32 + i * 5);
                    cursor_attach_item(0x10, 0x1d, (int16_t)(i * 9 + 0x1a), 0x41, NULL, 0xb);
                    drag_image_draw(BANK_BMP(RBANK(0x104), 0x2a));
                    list5_remove(g_local_player, i);
                    dialog_frame(2, 4);
                    screen_update();
                    g_dialog = 2;
                } else {
                    show_message(0x2a);
                }
            }
        }
        if (g_click_hotspot == 0x23)
            g_dialog = 3;
    } else if (g_dialog == 1 || g_dialog == 2) {
        if (g_click_hotspot == 0x26) {
            screenplay_buy_selected();
        } else if (g_click_hotspot == 0x21) {
            if (DS_I16(0x7652 + g_local_player * 2) < 5)
                screenplay_add_to_bag();
            else
                show_message(0x66);
        }
    }
}

/* Auslastung der Studios: (Summe der Mieten - Grundmiete) im Verhältnis zur Grundmiete, 1..10 */
static void studio_demand_update(void)
{
    int32_t sum = 0;
    for (int16_t i = 1; i < 0xd; i++)
        sum += *(int32_t *)(STUDIO(i) + 6);
    int32_t base = DS_I32(0x7aba) / 10;
    if (base == 0) /* PORT: Original teilt ggf. durch 0 */
        base = 1;
    DS_I16(0x7a0e) = (int16_t)((sum - DS_I32(0x7aba) + 1) / base + 1);
    if (DS_I16(0x7a0e) > 10)
        DS_I16(0x7a0e) = 10;
}

/* 1581:37d8 – Immobilienmakler: Belegungsplan, Studios mieten/kündigen */
void room_click_real_estate(void)
{
    if (g_cur_room == 10) {
        if (g_click_hotspot == 0x27)
            room_enter(0x3e);
        return;
    }
    if (g_cur_room != 0x3e)
        return;
    if (g_dialog == 0) {
        for (int16_t f = 1; f < 0xe; f++) {
            for (int16_t d = 0; (DS_I16(0x8f8e) = FLOOR_U(f, d + 0x14)) != 0 && DS_I16(0x8f8e) < 4; d++) {
                int16_t xy[2];
                building_map_door_pos(f, d, &xy[0], &xy[1]);
                if (!mouse_in_rect(xy[0], xy[1], (int16_t)(xy[0] + 9), (int16_t)(xy[1] + 10)))
                    continue;
                DS_I16(0x8ef5) = FLOOR_U(f, d + 0x6e);
                uint8_t *st = STUDIO(DS_I16(0x8ef5));
                if (DS_I16(0x8ef5) == 0 || (int8_t)st[0xf] >= 1)
                    continue;
                if ((int8_t)st[3] == g_local_player) { /* eigenes Studio: kündigen? */
                    message_box2_draw(fmt(g_text(0x74), (int16_t)(int8_t)st[2]), 0);
                    screen_update();
                    g_dialog = 1;
                } else if ((int8_t)st[3] == -1) { /* frei: mieten? */
                    message_box2_draw(fmt(g_text(0x76), *(int32_t *)(st + 6) * 3 / 2, (int16_t)(int8_t)st[2]), 0);
                    screen_update();
                    g_dialog = 2;
                }
            }
        }
        studio_demand_update();
        return;
    }
    if (g_dialog == 1 && g_click_hotspot == 0x24) {
        message_box2_draw(NULL, 1);
        studio_release_request(DS_I16(0x8ef5));
        studio_booking_panel();
        screen_update();
        if ((int8_t)STUDIO(DS_I16(0x8ef5))[0xf] == -1)
            show_message(0x77);
    } else if (g_dialog == 2 && g_click_hotspot == 0x24) {
        message_box2_draw(NULL, 1);
        studio_rent_request(g_local_player, DS_I16(0x8ef5));
        studio_booking_panel();
        screen_update();
        status_money_update();
        if ((int8_t)STUDIO(DS_I16(0x8ef5))[0xf] == g_local_player)
            show_message(0x78);
    } else if ((g_dialog == 1 || g_dialog == 2) && g_click_hotspot == 0x25) {
        message_box2_draw(NULL, 1);
        screen_update();
    } else {
        studio_demand_update();
        return;
    }
    g_dialog = 0;
    studio_demand_update();
}

/* Einschätzung der Besetzung der nächsten Folge (Hotspot 0x4b im Studio) */
static void studio_cast_rating(void)
{
    uint8_t *st = STUDIO(DS_I16(0x79be));
    int16_t show = *(int16_t *)st, ep = (int8_t)st[4], r = 1;
    int16_t act_w = 0, staff_w = 0, act_q = 0, staff_q = 0;
    char *t;
    speech_bubble(0, 0xb, 3, 0x1f, 9, 2);
    if (ep <= (int8_t)st[5]) {
        int8_t *e = (int8_t *)(g_shows + show * 0x1b3 + (ep - 1) * 0x23 + 8);
        for (; r < 0xb && (e[r * 3 + 1] == 0 || e[r * 3 + 3] != 0); r++) {
            int8_t type = e[r * 3 + 1];
            if (type >= 1 && type <= 0xf) {
                act_q = (int16_t)(act_q + (actor_suitability(show, ep, r, e[r * 3 + 3]) * e[r * 3 + 2]) / 100);
                act_w = (int16_t)(act_w + e[r * 3 + 2]);
            } else if (type > 0xf) {
                staff_q = (int16_t)(staff_q + min_i16(e[r * 3 + 2], DS_I8(0x266e + (type - 0xf) * 4 + e[r * 3 + 3])));
                staff_w = (int16_t)(staff_w + e[r * 3 + 2]);
            }
        }
        if (r > 10) { /* alle Rollen besetzt */
            int16_t f = e[0] != 0 ? e[0] : 1; /* PORT: Division durch 0 vermeiden */
            int16_t q = act_w + staff_w == 0 ? 0 : (int16_t)((e[0] * (act_q + staff_q)) / (act_w + staff_w));
            int16_t v = (int16_t)(((q * 0x7d) / 100) * 100 / f);
            int16_t id = v >= 0x51 ? 0x161 : v >= 0x3d ? 0x162 : v >= 0x29 ? 0x163 : 0x164;
            char *s;
            if (id == 0x161) {
                s = text_get_copy(0x168);
            } else { /* schwächeren Teil nennen */
                int16_t sq = staff_w != 0 ? (int16_t)((staff_q * 100) / staff_w) : 0;
                int16_t aq = act_w != 0 ? (int16_t)((act_q * 100) / act_w) : 0;
                s = fmt(g_text(0x165), sq < aq ? g_text(0x166) : g_text(0x167));
            }
            t = fmt(DS_ADDR(char, 0x05d2), g_text(id), s);
            far_free(s);
            goto show;
        }
    }
    t = text_get_copy(0x160);
show:
    text_box_to_free(0x60, 0x18, t, 0x13, 1, DS_PTR(Bitmap, 0x8e62));
    screen_update();
    g_dialog = 4;
}

/* 1581:3aaf – Studio: Drehbuch einlegen/tauschen, Folgen vorbereiten und produzieren */
void room_click_studio(void)
{
    uint8_t *st = STUDIO(DS_I16(0x79be));
    if (g_dialog == 0) {
        if (g_click_hotspot == 0x28 && *(int16_t *)st != 0) {
            studio_production_dialog(0);
            g_dialog = 3;
            return;
        }
        if (g_click_hotspot == 0x21) {
            int16_t i = (int16_t)((mouse_x() - 0xfa) / 9);
            g_item = DS_I16(0x1a25 + g_local_player * 0x32 + i * 5);
            if (g_item != 0) {
                DS_I16(0x8ee5) = DS_I16(0x1a28 + g_local_player * 0x32 + i * 5);
                if (DS_I16(0x8ee5) == 0)
                    DS_I16(0x8ee5) = 1;
                cursor_attach_item(0x10, 0x1d, (int16_t)(i * 9 + 0xfa), 0x19, NULL, 0xb);
                drag_image_draw(BANK_BMP(RBANK(0x100), 10));
                list5_remove(g_local_player, i);
                dialog_frame(2, 5);
                screen_update();
                g_dialog = 1;
                return;
            }
        }
        if (g_click_hotspot != 0x29 || *(int16_t *)st == 0)
            return;
        studio_screenplay_remove();
        g_dialog = 2;
        return;
    }
    if (g_dialog == 1 || g_dialog == 2) {
        if (g_click_hotspot == 10) { /* Papierkorb */
            g_shows[g_item * 0x1b3 + 4] = 99;
            cursor_drop_item();
        } else if (g_click_hotspot == 0x21) {
            studio_screenplay_to_bag();
        } else if (g_click_hotspot == 0x29) {
            if (show_role_count(g_item) <= (int8_t)st[2])
                studio_screenplay_swap();
            else
                show_message(0x3e); /* Studio zu klein */
            return;
        } else {
            return;
        }
        g_dialog = 0;
        return;
    }
    if (g_dialog != 3)
        return;
    switch (g_click_hotspot) {
    case 0x2a:
        if (studio_prepare_episode(DS_I16(0x79be)) == 1)
            studio_production_dialog(2);
        return;
    case 0x4b:
        studio_cast_rating();
        return;
    case 0x2b: { /* produzieren */
        if (*(int16_t *)st == 0) {
            show_message(0x42);
            return;
        }
        if ((int8_t)st[5] < (int8_t)st[4]) {
            show_message(0xf3);
            return;
        }
        if (episode_cast_complete(*(int16_t *)st, (int8_t)st[4]) == 0) {
            show_message(0xf1);
            return;
        }
        if (studio_produce_episode(DS_I16(0x79be)) != 1)
            return;
        studio_production_dialog(1);
        int16_t day = *(int16_t *)(st + 0xa);
        const char *when = day == g_day ? g_text(0xce) : day - g_day == 1 ? g_text(0xcf) : g_text(0x112);
        char *s = fmt(g_text(0xf2), when, *(int16_t *)(st + 0xc));
        show_info_overlay(s);
        far_free(s);
        g_cur_room = -4;
        g_dialog = 0;
        room_animate(2);
        return;
    }
    case 0x24:
        /* Release-Fassung: nur bei vorbereiteter Folge; den Eintrag dieser Show aus der
         * Produktionsliste nehmen (Suche ab dem Index "Anzahl", wie im Original) und eine Folge
         * weniger vorbereitet. Die Beta zählte nur die Liste herunter. */
        if ((int8_t)st[5] >= (int8_t)st[4]) {
            for (int16_t i = DS_I16(0x7460 + g_local_player * 2); i >= 0; i--)
                if (DS_I16(0x732c + g_local_player * 0x4b + i * 0xf) == *(int16_t *)st) {
                    production_remove(g_local_player, i);
                    break;
                }
            st[5]--;
            studio_production_dialog(2);
        }
        return;
    default:
        return;
    }
}

#define SAVE_NAME(n)    PTR32(char, DS_ADDR(ptr32, 0x8b92)[n])

/* 1581:4112 */
void save_menu_refresh(void)
{
    set_cursor_shape(3);
    clock_resume();
    savegame_names_load();
    savegame_slots_draw();
    set_cursor_shape(0);
}

static int16_t save_slot_hit(int16_t c, int16_t r)
{
    return mouse_in_rect((int16_t)(c * 0x76 + 0x40), (int16_t)(r * 0x11 + 8), (int16_t)(c * 0x76 + 0x8e), (int16_t)(r * 0x11 + 0xf));
}

/* PORT: Spieltempo setzen wie die drei Knöpfe im Optionen-Monitor (Programmplaner, Dialog 10);
 * auch aus der Menüleiste. Ist der Monitor offen, wird er neu gezeichnet. */
void options_speed_set(int16_t speed)
{
    clock_suspend();
    DS_I16(0x7282) = speed;
    clock_resume();
    if (g_cur_room == 0x1f && g_dialog == 10) {
        options_panel();
        screen_update();
    }
}

/* Schalter des Control-Centers: 0 Fernsehfamilie (DS:7a10), 1 Fernsehbild (DS:79f0), 2 Musik
 * (DS:79f2); Wert 0 = an, 0x1e = aus (Stellung des Schiebers) */
static const uint16_t g_switch_var[3] = {0x7a10, 0x79f0, 0x79f2};

int16_t options_switch_on(int16_t which)
{
    return DS_I16(g_switch_var[which]) == 0;
}

/* PORT: Schalter which wie im Control-Center umlegen (Klick dort oder Menüleiste). Bei offenem
 * Control-Center gleitet der Schieber wie beim Anklicken, sonst springt er um. */
void options_switch_set(int16_t which, int16_t on)
{
    int16_t *var = DS_ADDR(int16_t, g_switch_var[which]);
    if ((*var == 0) == (on != 0))
        return;
    if (g_cur_room == 0x1f && g_dialog == 10)
        panel_slide_toggle(var, 0x80, (int16_t)(0x24 + which * 0x18));
    else
        *var = (int16_t)(on ? 0 : 0x1e);
    if (which == 0) { /* Fernsehfamilie: Animation beenden bzw. starten */
        if (DS_I16(0x7a10) != 0) {
            if (DS_I16(0x79d6) > 0)
                DS_I16(0x79d6) = 1;
        } else if (DS_I16(0x71cc) >= 5 && DS_I16(0x71cc) <= 0x36 && hour_index() > 0) {
            tv_anim_setup(1);
            DS_I16(0x79d6) = 2;
        }
    } else if (which == 1) { /* Fernsehbild: Fernseher und Lämpchen neu zeichnen */
        tv_refresh();
        status_bar_draw();
    } else { /* Musik: sofort (Original: erst beim nächsten Raumwechsel) */
        music_switch_apply();
    }
}

/* Spielstand slot (1..10) laden, wie ein Klick im Ladedialog (auch für die Testhilfe MADTV_LOAD) */
void save_menu_load(int16_t slot)
{
    int16_t secs[1];
    set_cursor_shape(3);
    if (DS_I16(0x79d6) != 0) { /* laufende Fernsehanimation beenden */
        DS_I16(0x79d6) = 1;
        while (DS_I16(0x79d6) != 0)
            game_tick();
    }
    savegame_load((int16_t)(slot + 0xd1));
    g_player_idx = (int16_t)(g_local_player - 1);
    DS_I16(0x7a16) = DS_U8(0x7ca8 + FIG(2) * 0x7a + FIG(0x12));
    clock_get_seconds(secs);
    if (DS_I16(0x7c58) == 1) { /* aus dem Startmenü geladen */
        g_dialog = 0;
        DS_I16(0x8a2a) = 0;
        DS_I16(0x8a2c) = 1;
        DS_I16(0x8e6e) = 2;
        room_leave();
        DS_I16(0x7c58) = 3;
        DS_I16(0x012c) = 0;
    }
    char *s = fmt(DS_ADDR(char, 0x05d8), g_hour, g_minute);
    status_text(s, 0xb4);
    far_free(s);
    DS_I16(0x7a40) = 1;
    if (DS_I16(0x7a10) == 0 && hour_index() > 0) {
        if (g_minute > 4 && g_minute < 0x37)
            tv_anim_setup(1);
        else
            tv_anim_setup(2); /* Release-Fassung: auch außerhalb der Sendung */
    }
    tv_show_current_programme();
    figure_sprites_load_all();
    set_cursor_shape(0);
    status_money_update();
    status_ratings_update();
    status_meter_draw();
    status_bar_draw();
}

/* 1581:4135 – Spielstände: 1 Diskettenfrage, 2 Laden/Speichern, 3 Speichern, 4 Laden, 5 Start */
void room_click_save_menu(void)
{
    int16_t c, r;
    if (g_dialog == 1) {
        if (g_click_hotspot == 0x25) {
            room_leave();
        } else if (g_click_hotspot == 0x24) { /* Spielstandsverzeichnis anlegen */
            set_cursor_shape(3);
            hes_write_dgroup(0xd1, 400);
            savegame_names_save();
            set_cursor_shape(0);
            yes_no_dialog(1);
            if (DS_I16(0x7c58) == 1) {
                g_dialog = 4;
            } else {
                g_dialog = 2;
                yes_no_dialog2(0);
            }
        }
    } else if (g_dialog == 2) {
        if (g_click_hotspot == 0x39)
            g_dialog = 3;
        else if (g_click_hotspot == 0x3a)
            g_dialog = 4;
        else
            return;
        yes_no_dialog2(1);
    } else if (g_dialog == 3) { /* speichern: Namen eingeben */
        for (c = 0; c < 2; c++) {
            for (r = 1; r < 6; r++) {
                if (!save_slot_hit(c, r))
                    continue;
                draw_sprite(BANK_BMP(RBANK(0x104), 0x22), (int16_t)(c * 0x76 + 0x40), (int16_t)(r * 0x11 + 8));
                clock_suspend();
                int16_t n = c * 5 + r - 1;
                if (text_input((int16_t)(c * 0x76 + 0x40), (int16_t)(r * 0x11 + 8), 10, SAVE_NAME(n)) == 0 &&
                    str_length(SAVE_NAME(n)) != 0) {
                    set_cursor_shape(3);
                    clock_resume();
                    savegame_slots_draw();
                    savegame_names_save();
                    savegame_save((int16_t)(c * 5 + r + 0xd1));
                    set_cursor_shape(0);
                    if (DS_I16(0x7a78) != 0) { /* Schreibfehler */
                        save_menu_refresh();
                        show_message(0x1ab);
                    } else { /* Release-Fassung: nach dem Speichern zurück ins Spiel */
                        g_dialog = 0;
                        room_leave();
                    }
                } else {
                    save_menu_refresh();
                }
            }
        }
    } else if (g_dialog == 4) { /* laden */
        for (c = 0; c < 2; c++) {
            for (r = 1; r < 6; r++) {
                if (!save_slot_hit(c, r) || str_length(SAVE_NAME(c * 5 + r - 1)) == 0)
                    continue;
                save_menu_load(c * 5 + r);
            }
        }
    } else if (g_dialog == 5) {
        info_bubble_dialog(1);
        if (hes_filesize(0xd1) == -1) { /* noch kein Spielstandsverzeichnis */
            yes_no_dialog(0);
            g_dialog = 1;
        } else {
            yes_no_dialog2(0);
            g_dialog = 2;
        }
    }
}

/* 1581:454e – Portier: Hinweis, wer zuletzt im Büro war */
void room_click_porter(void)
{
    if (g_dialog != 0) {
        bubble_text_dialog_small(1, NULL);
        g_dialog = 0;
        return;
    }
    char *me = MEM_PTR(char, g_me), *s;
    int16_t who = DS_I16(0x7a3e);
    if (DS_I16(0x7a3c) != 3)
        s = fmt(g_text(0x13d), me);
    else if (who == g_local_player)
        s = fmt(g_text(0x13a), me);
    else if (who < 4)
        s = fmt(g_text(0x13b), me, PLAYER_NAME(who));
    else if (who == 8)
        s = fmt(g_text(0x13c), me);
    else
        s = fmt(g_text(0x13d), me); /* PORT: Original übergibt hier einen uninitialisierten Text */
    bubble_text_dialog_small(0, s);
    g_dialog = 1;
}
