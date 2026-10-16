/* Segment 19e6 – weitere Raum-Handler (Chef, Neues Spiel, Supermarkt, Betty), Hotspots,
 * Kauf-/Verkaufsaktionen und Bedienelemente der unteren Leiste */
#include "game.h"

#define g_click_hotspot DS_I16(0x8ee3)
#define g_tv_player     DS_I16(0x79de)
#define g_difficulty    DS_I16(0x7a74) /* 1 leicht, 2 normal, 3 schwer */

/* Spielerdatensatz (23 Byte ab DS:8bbc + p*0x17) */
#define PLAYER_NAME(p)  DS_PTR(char, 0x8bbc + (p) * 0x17)
#define PLAYER_MONEY(p) DS_I32(0x8bc4 + (p) * 0x17)

/* 19e6:09f7 – Klicks im Bildschirm "Neues Spiel" */
void room_click_new_game_setup(void)
{
    int16_t ai_count = 0;
    int16_t h = g_click_hotspot;
    if (h == 4 || h == 5 || h == 6) {
        /* Spieler wählen */
        g_tv_player = (int16_t)(h - 3);
        g_player_idx = (int16_t)(h - 4);
        g_local_player = g_tv_player;
        player_setup_screen(0);
        screen_update();
        status_bar_draw();
        return;
    }
    if (h == 7 || h == 8 || h == 9) {
        /* Namen eingeben */
        int16_t p = (int16_t)(h - 6);
        for (int16_t i = 0; i < 0x29; i++)
            PLAYER_NAME(p)[i] = 0;
        player_setup_screen(p);
        text_input((int16_t)(p * 0x60 - 0x40), 0x2e, 8, PLAYER_NAME(p));
    } else if (h >= 0x3f && h <= 0x41) {
        g_difficulty = (int16_t)(h - 0x3e);
    } else if (h == 0x4a) {
        /* Spiel starten: Startkapital, Computergegner, erster Tag */
        mouse_hide();
        for (int16_t p = 1; p < 4; p++) {
            if (p == g_local_player) {
                DS_ADDR(int16_t, 0x7b3c)[(p - 1) * 15] = 4;
                PLAYER_MONEY(p) = 200000;
                DS_I32(0x7abe) = 200000;
            } else {
                DS_U8(0xf2 + ai_count) = (uint8_t)p;
                ai_count++;
                DS_ADDR(int16_t, 0x7b3c)[(p - 1) * 15] = 3;
                PLAYER_MONEY(p) = g_difficulty < 3 ? 300000 : 750000;
                PLAYER_MONEY(p) *= g_opponent_stars + 1; /* Sterne aus der Kommandozeile */
            }
        }
        g_player_idx = (int16_t)(g_local_player - 1);
        DS_I16(0x8bd1 + g_local_player * 0x17) = 0x22;
        clock_suspend();
        DS_I32(0x8e8c) = DS_I32(0x6123); /* Zufallssalz */
        clock_resume();
        start_programmes_assign();
        ai_init_new_game();
        DS_U8(0x24d3) = 1;
        DS_U8(0x24d4) = 3;
        DS_U8(0x24d5) = 0;
        DS_U8(0x24d6) = 2;
        day_begin();
        DS_I16(0x7c58) = 2;
        return;
    } else if (h == 0x4b) {
        room_enter(0x3a); /* Spielstand laden */
        return;
    } else if (h == 0x24) {
        g_cur_room = 1000; /* Ende */
        return;
    } else {
        return;
    }
    player_setup_screen(0);
    screen_update();
}

/* 19e6:1f8c – Hotspot unter dem Mauszeiger (Tabelle DS:0636, 145 Einträge à 12 Byte:
 * x0, y0, x1, y1, Raum, Dialogzustand (-1 = jeder), nur für Besitzer, ID). 0 = keiner */
int16_t hotspot_at_mouse(int16_t any_owner)
{
    for (int16_t i = 0; i <= 0x90; i++) {
        uint16_t e = (uint16_t)(0x636 + i * 0xc);
        if (mouse_in_rect(DS_I16(e), DS_I16(e + 2), DS_I16(e + 4), DS_I16(e + 6)) &&
            DS_I8(e + 8) == g_cur_room && (DS_I8(e + 9) == g_cur_dialog || DS_I8(e + 9) == -1) &&
            (DS_I16(0x7a16) == g_local_player || DS_I8(e + 0xa) == 0 || any_owner == 1))
            return DS_I8(e + 0xb);
    }
    return 0;
}

#define g_dialog          g_cur_dialog
#define g_upper_screen    DS_PTR(Bitmap, 0x8e62)
#define g_me              DS_PTR(uint8_t, 0x8ef9)   /* Datensatz des Menschen */
#define ME_MONEY          (*(int32_t *)(g_me + 8))
#define ME_METER          (*(int16_t *)(g_me + 0xc))
#define ME_IMG            (*(int16_t *)(g_me + 0x15))
#define g_text(id)        PTR32(char, DS_ADDR(ptr32, 0x8370)[id])
#define g_credit          DS_I32(0x7abe)
#define g_talk_step       DS_I16(0x79ee)
#define g_boss_menu       DS_I16(0x7a6e)
#define g_boss_sel        DS_I16(0x7a70)
#define g_item            DS_I16(0x8eeb)            /* getragener Gegenstand */
#define g_item_prog       DS_I16(0x8ee9)
#define g_drag_bmp        DS_PTR(Bitmap, 0x0390)
#define RBANK(ofs)        DS_PTR(uint8_t, ofs)
#define BANK_BMP(bank, ofs) ((Bitmap *)((uint8_t *)(bank) + (ofs)))
#define GIFT(g)           DS_ADDR(uint8_t, 0x722c + (g) * 8)
#define g_present(p)      DS_I8(0x74c6 + (p))

/* Gegenstand an den Mauszeiger hängen (Standardgröße 32x24) */
static void attach_item_32x24(void)
{
    int16_t gy = mouse_y() - 0xc;
    int16_t gx = mouse_x() - 0x10;
    cursor_attach_item(0x20, 0x18, gx, gy, NULL, 0xb);
}

static void boss_credit(int32_t amount)
{
    PLAYER_MONEY(g_local_player) += amount;
    money_add(0xd, amount);
}

/* 19e6:0004 – Chefbüro: Begrüßung/Lagebericht (Dialog 1) und Kreditmenü (Dialog 2) */
void room_click_boss(void)
{
    int16_t img = DS_I16(0x8bd1 + g_local_player * 0x17), msg = 0, said = 0;
    char *part[3] = {NULL, NULL, NULL}; /* Original: nur die ersten beiden mit 0 vorbelegt */
    if (g_dialog == 1) {
        if (g_day == 1) {
            if (g_talk_step == 0) {
                said = 1;
                bubble_text_dialog(0, text_get_copy(0x48));
                g_talk_step = 1;
            } else if (g_talk_step == 1) {
                said = contest_tick() == 0;
                if (said) {
                    bubble_text_dialog(2, fmt(g_text(0xa4), g_text(DS_I16(0x7468) + 0x153)));
                    screen_update();
                }
                g_talk_step = 2;
            }
            if (said || g_talk_step != 2)
                goto done;
            bubble_text_dialog(1, NULL);
            g_talk_step = 0;
            g_dialog = 0;
            room_leave();
        } else {
            if (g_talk_step == 0) { /* Begrüßung nach Image */
                int16_t id;
                if (ME_IMG < 5) {
                    id = 0x9a;
                    DS_I16(0x7a34) = g_local_player;
                } else if (ME_IMG < 0xf) {
                    id = 0x9b;
                } else if (ME_IMG < 0x4b) {
                    id = 0x9c;
                } else {
                    id = 0x9d;
                }
                bubble_text_dialog(0, fmt(g_text(id), MEM_PTR(char, g_me)));
                g_talk_step = 1;
                said = 1;
                DS_I16(0x05e4) = 0;
            }
            if (!said && g_talk_step == 1) { /* Kritik: leere Sendeplätze, Wettbewerb, Image */
                if (DS_I16(0x7a34) != g_local_player) {
                    if (DS_I16(0x79e2) != 0) {
                        part[0] = fmt(g_text(0x9e), DS_I16(0x79e2));
                        DS_I16(0x05e4) = 1;
                    }
                    if (DS_I16(0x79e4) != 0) {
                        part[1] = fmt(g_text(0x9f), DS_I16(0x79e4));
                        DS_I16(0x05e4) = 1;
                    }
                    if (DS_I16(0x79e6) != 0 && DS_I16(0x79e6) != g_local_player) {
                        part[2] = text_get_copy(0xa0);
                        DS_I16(0x05e4) = 1;
                    }
                    if (ME_IMG < 0x14 && DS_I16(0x05e4) == 0) {
                        part[0] = text_get_copy(0xa1);
                        DS_I16(0x05e4) = 1;
                    }
                    if (DS_I16(0x05e4) == 0)
                        part[0] = text_get_copy(0xa2);
                    if (DS_I16(0x79e6) == g_local_player)
                        part[2] = text_get_copy(0xa3);
                    bubble_text_dialog(2, fmt(DS_ADDR(char, 0x605), part[0], part[1], part[2]));
                    for (int16_t i = 0; i < 3; i++)
                        if (part[i] != NULL) {
                            far_free(part[i]); /* far_free_ptr auf lokale Zeiger */
                            part[i] = NULL;
                        }
                    screen_update();
                    said = 1;
                }
                g_talk_step = 2;
            }
            if (!said && g_talk_step == 2) { /* Wettbewerb und Image */
                if (contest_tick() == 0 && DS_I16(0x7a34) != g_local_player)
                    part[0] = fmt(g_text(0xa4), g_text(DS_I16(0x7468) + 0x153));
                int16_t id = ME_IMG < 0x14 ? 0xa5 : ME_IMG < 0x3c ? 0xa6 : 0xa7;
                part[1] = fmt(g_text(id), MEM_PTR(char, g_me));
                bubble_text_dialog(2, fmt(DS_ADDR(char, 0x60c), part[0], part[1]));
                screen_update();
                if (part[0] != NULL) {
                    far_free(part[0]); /* far_free_ptr auf lokale Zeiger */
                    part[0] = NULL;
                }
                far_free(part[1]);
                part[1] = NULL;
                said = 1;
                g_talk_step = 3;
            }
            if (!said && g_talk_step == 3) {
                int16_t id = ME_IMG < 0x14 ? 0xa8 : ME_IMG < 0x4b ? 0xa9 : 0xaa;
                bubble_text_dialog(2, fmt(g_text(id), MEM_PTR(char, g_me)));
                screen_update();
                said = 1;
                g_talk_step = 4;
            }
            if (said || g_talk_step != 4)
                goto done;
            bubble_text_dialog(1, NULL);
            g_dialog = 0;
            if (DS_I16(0x7a34) == 0) {
                room_leave();
            } else { /* Spieler fliegt raus */
                music_on_leave(3, 0); /* Original: ohne Argumente aufgerufen */
                player_eliminated_sequence();
                if (DS_I16(0x7a34) != g_local_player)
                    room_leave();
            }
            g_talk_step = 0;
        }
        DS_I16(0x7a12) = 2;
        goto done;
    }
    if (g_dialog != 2)
        goto done;
    /* Kreditmenü */
    if (g_boss_menu == -1) {
        boss_dialog(1, 0);
        g_boss_menu = -2;
        goto done;
    }
    if (g_click_hotspot == 0x24 && g_boss_menu > 0) {
        if (--g_boss_sel < 1)
            g_boss_sel = DS_I16(0x5e8 + (g_boss_menu - 1) * 2);
        boss_dialog(2, -1);
    } else if (g_click_hotspot == 0x25 && g_boss_menu > 0) {
        if (++g_boss_sel > DS_I16(0x5e8 + (g_boss_menu - 1) * 2))
            g_boss_sel = 1;
        boss_dialog(2, -1);
    } else if (g_click_hotspot == 0x49) {
        int16_t s = g_boss_sel;
        switch (g_boss_menu) {
        case 0:
            g_boss_menu = 1;
            g_boss_sel = 1;
            msg = -1;
            break;
        case 1:
            if (s == 1) { /* Lage besprechen */
                if (DS_I16(0x8bd1 + g_local_player * 0x17) < 0x4b && (DS_I16(0x79e6) == 0 || DS_I16(0x79e6) != g_local_player)) {
                    g_boss_menu = -1;
                    DS_I16(0x05e6) = 1;
                    msg = 0xb2;
                } else {
                    g_boss_menu = 2;
                    g_boss_sel = 1;
                    msg = 0xb3;
                }
            } else if (s == 2) { /* Kredit */
                if (img < 10) {
                    g_boss_menu = -1;
                    msg = 0xb8;
                } else if (img < 0x32) {
                    if (g_credit == 0) {
                        msg = 0xb9;
                        g_credit = 200000;
                        g_boss_menu = -1;
                        boss_credit(200000);
                        status_money_update();
                    } else if (g_credit > 200000 || g_day < 2) {
                        g_boss_menu = -1;
                        msg = 0xc1;
                    } else {
                        g_boss_menu = 3;
                        g_boss_sel = 1;
                        msg = 0xba;
                    }
                } else if (g_credit == 0) {
                    msg = 0xc2;
                    g_credit = 400000;
                    g_boss_menu = -1;
                    boss_credit(400000);
                    status_money_update();
                } else if (g_credit <= 400000) {
                    g_boss_menu = 4;
                    g_boss_sel = 1;
                    msg = 0xc3;
                } else {
                    g_boss_menu = -1;
                    msg = 0xc1;
                }
            } else if (s == 3) { /* Rückzahlung */
                if (g_credit == 0) {
                    g_boss_menu = 1;
                    g_boss_sel = 1;
                    msg = 0x2c;
                } else {
                    g_boss_menu = 5;
                    g_boss_sel = 1;
                    msg = 0x32;
                }
            }
            break;
        case 2:
            if (s == 1) {
                g_boss_menu = -1;
                DS_I16(0x05e6) = 1;
                msg = 0xb6;
            } else if (s == 2) {
                g_boss_menu = -1;
                msg = 0xb7;
            }
            break;
        case 3: /* Erhöhung bis 400000 */
            if (s == 1 && DS_I16(0x79e6) == g_local_player) {
                g_boss_menu = -1;
                g_credit += 200000;
                money_add(0xd, 200000);
                msg = 0xbe;
                PLAYER_MONEY(g_local_player) += 200000;
                status_money_update();
            } else if (s == 1 || s == 3) {
                g_boss_menu = -1;
                msg = 0xbf;
            } else if (s == 2) {
                g_boss_menu = -1;
                g_credit += 100000;
                PLAYER_MONEY(g_local_player) += 100000;
                money_add(0xd, 100000);
                status_money_update();
                msg = 0xc0;
            }
            break;
        case 4: /* Erhöhung bis 800000 */
            if (s == 1) {
                if (DS_I16(0x79e6) == g_local_player) {
                    msg = 199;
                    g_credit += 400000;
                    g_boss_menu = -1;
                    boss_credit(400000);
                    status_money_update();
                } else {
                    g_boss_menu = -1;
                    msg = 200;
                }
            } else if (s == 2) {
                msg = 0xc9;
                g_credit += 200000;
                g_boss_menu = -1;
                boss_credit(200000);
                status_money_update();
            } else if (s == 3) {
                g_boss_menu = -1;
                msg = 0xca;
            }
            break;
        case 5: { /* zurückzahlen, in 50000er-Schritten */
            int32_t amount = s == 1 ? 100000 : s == 2 ? 200000 : g_credit;
            if (PLAYER_MONEY(g_local_player) <= amount)
                amount = PLAYER_MONEY(g_local_player);
            amount = amount / 50000 * 50000;
            msg = amount == 0 ? 0x3c : 0x3d;
            PLAYER_MONEY(g_local_player) -= amount;
            money_add(0x10, amount);
            g_credit -= amount;
            g_boss_menu = -1;
            status_money_update();
            break;
        }
        }
        boss_dialog(2, msg);
    } else {
        goto done;
    }
    screen_update();
done:
    if (DS_I16(0x05e6) != 0 && g_boss_menu == -2) { /* gefeuert */
        DS_I16(0x05e6) = 0;
        DS_I16(0x7a34) = g_local_player;
        music_on_leave(3, 0);
        g_dialog = 0;
        player_eliminated_sequence();
    } else if (g_boss_menu == -2) {
        g_dialog = 0;
        room_leave();
    }
}

/* 19e6:0c29 – Supermarkt: Einkaufslisten und Geschenke */
void room_click_supermarket(void)
{
    int16_t p = g_local_player;
    if (g_dialog == 0) {
        if (g_click_hotspot == 0x23) {
            g_dialog = 6;
            room_animate(2);
        }
        return;
    }
    if (g_dialog == 1) {
        if (g_click_hotspot == 0x2c) { /* Geschenke */
            g_dialog = 3;
            confirm_bubble_dialog(1);
            people_select_dialog(0);
            dialog_frame(0, 6);
        } else if (g_click_hotspot == 0x2d) { /* Requisiten für die Produktion */
            if (DS_I16(0x7460 + p * 2) < 1) {
                room_animate(2);
                show_message(0xec);
                return;
            }
            int16_t i = 0;
            while (i < DS_I16(0x7460 + p * 2) &&
                   episode_cast_complete(DS_I16(0x732c + p * 0x4b + i * 0xf), DS_I16(0x732e + p * 0x4b + i * 0xf)) != 0)
                i++;
            if (i >= DS_I16(0x7460 + p * 2))
                i = 0;
            DS_I16(0x79c2) = 1;
            DS_I16(0x79c4) = 0;
            DS_I16(0x79c6) = 0;
            DS_I16(0x79c0) = i;
            confirm_bubble_dialog(1);
            g_dialog = 0;
            room_enter(0x3f);
            return;
        } else {
            return;
        }
    } else if (g_dialog == 3) {
        if (g_click_hotspot == 0x21 && g_present(p) != 0) { /* gekauftes Geschenk wieder aufnehmen */
            attach_item_32x24();
            int16_t n = g_present(p);
            if (n > 100)
                n -= 100;
            g_item = g_present(p);
            bmp_copy(BANK_BMP(RBANK(0x10c), sprite_ofs(n)), g_drag_bmp);
            g_dialog = 5;
            DS_I16(0x79ae) = 1;
            g_present(p) = 0;
            dialog_frame(2, 6);
        } else {
            for (int16_t r = 0; r < 2; r++) {
                for (int16_t c = 1; c < 6; c++) {
                    if (mouse_in_rect(c * 0x28 + 0x38, r * 0x24 + 0x10, c * 0x28 + 0x57, r * 0x24 + 0x27) == 0)
                        continue;
                    int16_t g = r * 5 + c;
                    if (money_check(p, *(int32_t *)GIFT(g)) == 0) {
                        g_item = (int16_t)(g + 100);
                        ME_MONEY -= *(int32_t *)GIFT(g);
                        attach_item_32x24();
                        bmp_copy(BANK_BMP(RBANK(0x10c), sprite_ofs(g)), g_drag_bmp);
                        g_dialog = 4;
                        DS_I16(0x79ae) = 1;
                        status_money_update();
                    }
                }
            }
            return;
        }
    } else if (g_dialog == 4 || g_dialog == 5) {
        if (g_click_hotspot == 0x21) {
            if (g_present(p) == 0)
                supermarket_present_take();
            else
                show_message(0x46);
        } else if (g_click_hotspot == 0x35) {
            supermarket_checkout();
        }
        return;
    } else {
        return;
    }
    screen_update();
}

/* 19e6:1b54 */
void supermarket_present_take(void)
{
    g_present(g_local_player) = (int8_t)g_item;
    cursor_drop_item();
    g_dialog = 3;
    dialog_frame(2, 6);
    screen_update();
}

/* 19e6:1b7f */
void betty_present_give_close(void)
{
    g_present(g_local_player) = (int8_t)g_item;
    cursor_drop_item();
    g_dialog = 0;
    dialog_frame(2, 7);
    screen_update();
}

/* 19e6:1baa – Geschenk zurück ins Regal: Geld zurück */
void supermarket_checkout(void)
{
    if (g_item > 100)
        ME_MONEY += *(int32_t *)GIFT(g_item - 100);
    cursor_drop_item();
    g_dialog = 3;
    status_money_update();
}

/* 19e6:1be9 – Bettys Büro: Gespräch, Geschenke, Heiratsantrag (Spielende) */
void room_click_betty(void)
{
    int16_t p = g_local_player, end = 0;
    if (g_dialog == 0) {
        if (g_click_hotspot == 0x36) {
            if (ME_METER < 0x14 || player_flag(p, 0) != 0) {
                player_status_bubble(0);
                g_dialog = 1;
            } else {
                bubble_text_overlay(0, text_get_copy(0x15d));
                player_flag(p, 1);
            }
        }
        if (g_click_hotspot == 0x21 && g_present(p) != 0) {
            g_item = g_present(p);
            g_present(p) = 0;
            attach_item_32x24();
            bmp_copy(BANK_BMP(RBANK(0x100), sprite_ofs(g_item)), g_drag_bmp);
            dialog_frame(2, 7);
            screen_update();
            g_dialog = 2;
        }
        return;
    }
    if (g_dialog == 1) {
        player_status_bubble(1);
    } else {
        if (g_dialog != 2)
            return;
        if (g_click_hotspot == 0x21) {
            betty_present_give_close();
            return;
        }
        if (g_click_hotspot != 0x36)
            return;
        if (g_item != 10) { /* Geschenk überreichen */
            cursor_drop_item();
            g_dialog = 0;
            if (g_item == 2) {
                ME_METER = 0;
            } else {
                ME_METER += (int8_t)GIFT(g_item)[4];
                GIFT(g_item)[4] = (uint8_t)((int8_t)GIFT(g_item)[4] * 0x46 / 100);
            }
            GIFT(g_item)[5]++;
            if (ME_IMG < ME_METER)
                ME_METER = ME_IMG;
            dialog_frame(1, 7);
            player_ranking_panel();
            screen_update();
            status_meter_draw();
            bubble_text_overlay(0, text_get_copy(g_item == 2 ? 0x108 : 0x107));
            return;
        }
        if (ME_METER < 100) { /* Ring: noch nicht genug Zuneigung */
            bubble_text_overlay(0, text_get_copy(0x106));
            return;
        }
        end = 1;
        cursor_drop_item();
    }
    g_dialog = 0;
    if (end)
        game_end_sequence();
}

/* 19e6:1e4f – Requisit des Einkaufszettels in die Zieh-Bitmap kopieren */
void shelf_item_draw(void)
{
    int16_t n = DS_I16(0x4ffd + (DS_I16(0x79c2) - 1) * 0xc0 + g_item * 0xc);
    if (n < 0x10)
        bmp_copy_at(BANK_BMP(RBANK(0x100), sprite_ofs(n)), g_drag_bmp, 0, 2);
    else
        bmp_copy_at(BANK_BMP(RBANK(0x10c), sprite_ofs((int16_t)(n - 0xf))), g_drag_bmp, 0, 2);
}

/* 19e6:1eb8 */
void shoplist_redraw(void)
{
    production_plan_panel();
    screen_update();
}

/* 19e6:1ec3 – Nachricht als Zettel an den Mauszeiger hängen */
void news_item_draw(void *item, int16_t x, int16_t y)
{
    uint8_t *n = item;
    int16_t style = DS_I8(0x601 + (int8_t)n[0x1c]);
    char *s = news_format_text(n, *(int16_t *)n);
    cursor_attach_item(0x90, 0x22, x, y, fmt(DS_ADDR(char, 0x611), s), style);
    far_free(s);
    char *when = *(int16_t *)(n + 0x1a) == g_day ? DS_PTR(char, 0x877c) : DS_PTR(char, 0x87a0);
    text_draw_to_free(2, 0x11, fmt(DS_ADDR(char, 0x617), *(int16_t *)(n + 0x18), when, genre_value_get((int8_t)n[0x1d])),
                      g_drag_bmp);
}

/* 19e6:2022 – nächster gültiger Sender (rundum) */
int16_t station_next_valid(int16_t idx)
{
    for (;; idx++) {
        if (idx > 0x20)
            idx = 1;
        uint8_t *st = DS_ADDR(uint8_t, 0x1ce9 + DS_I16(0x7a16) * 0x1ce + idx * 0xe);
        if (*(int32_t *)(st + 4) != 0 && *(int16_t *)(st + 0xc) != 2)
            return idx;
    }
}

/* 19e6:207c */
int16_t station_prev_valid(int16_t idx)
{
    for (;; idx--) {
        if (idx < 1)
            idx = 0x20;
        uint8_t *st = DS_ADDR(uint8_t, 0x1ce9 + DS_I16(0x7a16) * 0x1ce + idx * 0xe);
        if (*(int32_t *)(st + 4) != 0 && *(int16_t *)(st + 0xc) != 2)
            return idx;
    }
}

/* 19e6:20d6 – getragenes Programm verkaufen */
void programme_sell_selected(void)
{
    if (g_item_prog < 1000) {
        ME_MONEY += film_price(g_item_prog);
        money_add(0xb, film_price(g_item_prog));
        DS_PTR(uint8_t, 0x2489)[g_item_prog * 0x17 + 5] = 0;
    } else {
        g_item_prog -= 1000;
        ME_MONEY += series_price(g_item_prog);
        money_add(0xc, series_price(g_item_prog));
        DS_PTR(uint8_t, 0x2626)[g_item_prog * 0x39 + 0x24] = 0;
    }
    status_money_update();
    g_dialog = 0;
}

/* 19e6:2174 – beim Verlassen der Werbeagentur reservierte Verträge vergeben */
void adcontract_offers_commit(void)
{
    for (int16_t i = 1; i < 0xb; i++) {
        uint8_t *e = DS_ADDR(uint8_t, 0x1980 + i * 7);
        if ((int8_t)e[2] < 0) {
            DS_U8(0x3f3e + *(int16_t *)e * 0x18) = (uint8_t)g_local_player;
            far_memset(e, 7);
        }
    }
    dialog_frame(1, 3);
    screen_update();
    ad_market_update();
}

/* 19e6:21f1 – beim Verlassen der Drehbuchagentur gekaufte Angebote entfernen */
void screenplay_offers_commit(void)
{
    for (int16_t i = 1; i < 0xb; i++)
        if (DS_I8(0x194c + i * 9) < 0)
            far_memset(DS_ADDR(uint8_t, 0x194a + i * 9), 9);
    dialog_frame(1, 4);
    screen_update();
    production_market_update();
}

/* 19e6:2246 – Programm in den Koffer */
void dealer_offer_return(void)
{
    DS_I16(0x2463 + DS_I16(0x732a) * 2) = g_item_prog;
    DS_I16(0x732a)++;
    cursor_drop_item();
    g_dialog = 0;
    dialog_frame(2, 1);
    screen_update();
}

/* 19e6:2277 – Programm ins Archiv des gewählten Genres legen */
void archive_add_selected(void)
{
    int16_t owner = DS_I16(0x7a16), g = DS_I16(0x79aa);
    int16_t prog = g_item_prog > 999 ? g_item_prog - 1000 : g_item_prog;
    int8_t *count = DS_ADDR(int8_t, 0x6f59 + owner * 0x6c + g);
    DS_I16(0x6ef9 + owner * 0x6c + g * 8 + *count * 2) = prog;
    (*count)++;
    cursor_drop_item();
    g_dialog = 0;
    archive_shelf_panel(owner);
    screen_update();
}

/* 19e6:22f7 – Programm (aus der Auktion) in den Koffer */
void offer_list_add_selected(void)
{
    if (g_item_prog > 10000)
        g_item_prog -= 10000;
    DS_I16(0x2463 + DS_I16(0x732a) * 2) = g_item_prog;
    DS_I16(0x732a)++;
    cursor_drop_item();
    g_dialog = 0;
    dialog_frame(2, 2);
    screen_update();
}

/* 19e6:2333 – gekauftes Händlerangebot zurückgeben (Geld zurück) */
void auction_withdraw_item(void)
{
    int16_t v = g_item_prog;
    if (g_item_prog > 10000) {
        v = g_item_prog - 10000;
        for (int16_t i = 1; i < 0xb; i++) {
            if (DS_I16(0x19cd + i * 8) != v)
                continue;
            if (v < 1000) {
                g_item_prog = v;
                ME_MONEY += film_price(v);
                money_add(1, -film_price(g_item_prog));
            } else {
                g_item_prog -= 11000;
                ME_MONEY += series_price(g_item_prog);
                money_add(2, -series_price(g_item_prog));
            }
            DS_I8(0x19d4 + i * 8) += 100;
            cursor_drop_item();
            g_dialog = 0;
            room8_items_panel(1);
            screen_update();
            status_money_update();
            return;
        }
    }
    g_item_prog = v;
}

/* 19e6:241e – Werbeangebot reservieren/freigeben */
void adcontract_offer_reserve(void)
{
    int16_t i = 1;
    while (DS_I16(0x1980 + i * 7) != DS_I16(0x8edc) && i < 0xb)
        i++;
    cursor_drop_item();
    DS_I8(0x1982 + i * 7) += 100;
    ad_offers_panel();
    screen_update();
    g_dialog = 0;
}

/* 19e6:246d – Werbevertrag unterschreiben */
void adcontract_sign(void)
{
    int16_t ad = DS_I16(0x8edc), p = g_local_player;
    uint8_t *c = DS_ADDR(uint8_t, 0x1b50 + p * 0x46 + DS_I16(0x7688 + p * 2) * 7);
    *(int16_t *)c = ad;
    *(int16_t *)(c + 5) = (int16_t)(DS_I8(0x3f37 + ad * 0x18) + g_day);
    c[4] = 0;
    DS_I16(0x7688 + p * 2)++;
    cursor_drop_item();
    dialog_frame(2, 3);
    screen_update();
    g_dialog = 0;
}

/* 19e6:24f3 – Drehbuch kaufen */
void screenplay_buy_selected(void)
{
    cursor_drop_item();
    int16_t i = 1;
    while (DS_I16(0x194a + i * 9) != g_item)
        i++;
    DS_I8(0x194c + i * 9) += 100;
    int32_t price = DS_I32(0x194e + i * 9);
    ME_MONEY += price;
    money_add(4, -price);
    production_offers_panel();
    status_money_update();
    screen_update();
    g_dialog = 0;
}

/* 19e6:2591 – Drehbuch in die Mappe */
void screenplay_add_to_bag(void)
{
    int16_t p = g_local_player;
    uint8_t *e = DS_ADDR(uint8_t, 0x1a25 + p * 0x32 + DS_I16(0x7652 + p * 2) * 5);
    *(int16_t *)e = g_item;
    e[2] = DS_U8(0x8ee5);
    *(int16_t *)(e + 3) = 1;
    DS_I16(0x7652 + p * 2)++;
    cursor_drop_item();
    dialog_frame(2, 4);
    screen_update();
    g_dialog = 0;
}

/* 19e6:2602 – Drehbuch aus dem Studio zurück in die Mappe */
void studio_screenplay_to_bag(void)
{
    int16_t p = g_local_player;
    uint8_t *e = DS_ADDR(uint8_t, 0x1a25 + p * 0x32 + DS_I16(0x7652 + p * 2) * 5);
    *(int16_t *)e = g_item;
    *(int16_t *)(e + 3) = DS_I16(0x8ee5);
    DS_I16(0x7652 + p * 2)++;
    cursor_drop_item();
    dialog_frame(2, 5);
    screen_update();
}

/* 19e6:2667 – Drehbuch ins Studio legen */
void studio_screenplay_insert(void)
{
    uint8_t *st = DS_ADDR(uint8_t, 0x6145 + DS_I16(0x79be) * 0x10);
    *(int16_t *)st = g_item;
    st[4] = DS_U8(0x8ee5);
    st[5] = (uint8_t)(DS_U8(0x8ee5) - 1);
    cursor_drop_item();
    studio_panel_draw();
    dialog_frame(2, 5);
    screen_update();
}

/* 19e6:26b9 – Drehbuch aus dem Studio nehmen */
void studio_screenplay_remove(void)
{
    int16_t *st = DS_ADDR(int16_t, 0x6145 + DS_I16(0x79be) * 0x10);
    g_item = *st;
    DS_I16(0x8ee5) = DS_I8(0x6149 + DS_I16(0x79be) * 0x10);
    if (DS_I16(0x8ee5) == 0)
        DS_I16(0x8ee5) = 1;
    studio_slot_release(DS_I16(0x79be));
    int16_t gy = mouse_y() - 4, gx = mouse_x() - 0xf;
    cursor_attach_item(0x10, 0x1d, gx, gy, NULL, 0xb);
    drag_image_draw(BANK_BMP(RBANK(0x100), 10));
    *st = 0;
    clip_reset();
    draw_bitmap_fast(DS_PTR(Bitmap, 0x10c), 0xf8, 0x53);
    screen_update();
}

/* 19e6:2760 – getragenes Drehbuch gegen das im Studio tauschen */
void studio_screenplay_swap(void)
{
    int16_t *st = DS_ADDR(int16_t, 0x6145 + DS_I16(0x79be) * 0x10);
    if (*st == 0) {
        DS_I16(0x8ef7) = 0;
    } else {
        DS_I16(0x8ef7) = *st;
        DS_I16(0x8ee7) = DS_I8(0x6149 + DS_I16(0x79be) * 0x10);
        studio_slot_release(DS_I16(0x79be));
    }
    studio_screenplay_insert();
    if (DS_I16(0x8ef7) == 0) {
        g_dialog = 0;
    } else {
        g_item = DS_I16(0x8ef7);
        DS_I16(0x8ee5) = DS_I16(0x8ee7);
        int16_t gy = mouse_y() - 4, gx = mouse_x() - 0xf;
        cursor_attach_item(0x10, 0x1d, gx, gy, NULL, 0xb);
        drag_image_draw(BANK_BMP(RBANK(0x100), 10));
        g_dialog = 2;
    }
}

/* 19e6:2806 – Schieberegler animiert zwischen 0 und 30 bewegen */
void panel_slide_toggle(int16_t *pos, int16_t xcol, int16_t y)
{
    int16_t target = *pos == 0 ? 0x1e : 0, step = *pos == 0 ? 2 : -2;
    mouse_hide();
    while (*pos != target) {
        *pos += step;
        option_switch_draw(*pos);
        blit_sprite_masked(DS_PTR(Bitmap, 0x110), g_upper_screen, xcol, y, 0x20, 0, 0x20, 10);
        screen_update();
    }
    mouse_show();
}

/* 19e6:287c */
void tv_refresh(void)
{
    if (hour_index() > 0) {
        if (DS_I16(0x79f0) == 0)
            tv_show_current_programme();
        else
            tv_show_picture(0xdc);
    }
}

/* 19e6:289f – nächsten Sender in der Senderkarte wählen */
void station_select_next(void)
{
    DS_I16(0x79ae) = 1;
    DS_I16(0x79a8) = station_next_valid(DS_I16(0x79a8) + 1);
    station_bid_dialog(1, DS_ADDR(int32_t, 0x8eed));
    g_dialog = 3;
    station_map_panel(DS_I16(0x79a8));
    stations_reach_update(g_local_player);
    screen_update();
    status_money_update();
}

#define g_shop_list       DS_I16(0x79c0) /* gewählte Produktion */
#define g_shop_group      DS_I16(0x79c2) /* Darstellergruppe 1..7 bzw. Stabtyp >= 0x10 */
#define g_shop_scroll     DS_I16(0x79c4) /* Darsteller-Scroll */
#define g_shop_roles      DS_I16(0x79c6) /* Rollen-Scroll */
#define ACTOR_FEE(g, a)   DS_I32(0x4ff5 + ((g) - 1) * 0xc0 + (a) * 0xc)
#define STAFF_FEE(t, k)   DS_I32(0x6215 + ((t) - 0xf) * 0x10 + (k) * 4)

static void attach_item_at_mouse(void)
{
    int16_t y = mouse_y(), x = mouse_x();
    cursor_attach_item(0x20, 0x18, x, y, NULL, 0xb);
}

static int16_t staff_icon(int16_t k)
{
    return k == 1 ? 3 : k == 2 ? 1 : k == 3 ? 2 : 0; /* im Original sonst uninitialisiert */
}

static void staff_item_draw(int16_t k)
{
    bmp_copy_at(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(g_shop_group - 0xf))), g_drag_bmp, 0, 2);
    bmp_draw_masked(BANK_BMP(RBANK(0x108), sprite_ofs((int16_t)(staff_icon(k) + 9))), g_drag_bmp, 0, 0);
}

/* 19e6:0f78 – Requisitenregal: Darsteller und Stab für die Rollen der Folge buchen */
void room_click_supermarket_shelves(void)
{
    int16_t p = g_local_player;
    uint8_t *prod = DS_ADDR(uint8_t, 0x732c + p * 0x4b + g_shop_list * 0xf);
    int16_t show = *(int16_t *)prod;
    uint8_t *sh = DS_PTR(uint8_t, 0x713c) + show * 0x1b3;
    int8_t *e = (int8_t *)(sh + (*(int16_t *)(prod + 2) - 1) * 0x23 + 8); /* Folge: +2 Faktor, Rollen ab +r*3+1 */
    int16_t c;
    if (g_dialog == 2 && mouse_y() > 0x48 && mouse_y() < 0x78) {
        g_dialog = 0;
        shoplist_redraw();
    }
    if (g_click_hotspot == 0x2f && g_shop_scroll > 0) {
        g_shop_scroll -= 5;
        if (g_shop_scroll < 0)
            g_shop_scroll = 0;
        shoplist_redraw();
    }
    if (g_click_hotspot == 0x31 && DS_I16(0x4ff3 + (g_shop_group - 1) * 0xc0 + (g_shop_scroll + 6) * 0xc) > 0) {
        for (g_shop_scroll += 5;
             DS_I16(0x4ff3 + (g_shop_group - 1) * 0xc0 + (g_shop_scroll + 5) * 0xc) == 0 || g_shop_scroll > 0xb;
             g_shop_scroll--)
            ;
        shoplist_redraw();
    }
    if (g_click_hotspot == 0x30 && g_shop_roles > 0) {
        g_shop_roles -= 3;
        if (g_shop_roles < 0)
            g_shop_roles = 0;
        shoplist_redraw();
    }
    if (g_click_hotspot == 0x32 && e[(g_shop_roles + 4) * 3 + 1] != 0) {
        for (g_shop_roles += 3; e[(g_shop_roles + 3) * 3 + 1] == 0 || g_shop_roles > 8; g_shop_roles--)
            ;
        shoplist_redraw();
    }
    if (g_dialog == 0) {
        if (mouse_y() >= 0x66 && mouse_y() <= 0x6f) { /* Rollenreiter */
            for (c = 1; c < 4 && (mouse_x() < c * 0x58 - 0x30 || mouse_x() > c * 0x58 + 0x26); c++)
                ;
            if (c < 4) {
                int8_t type = e[(c + g_shop_roles) * 3 + 1];
                if (type < 0x10) {
                    int16_t g = (int8_t)sh[type + 0x1a3];
                    if (g != g_shop_group)
                        g_shop_scroll = 0;
                    g_shop_group = g;
                } else {
                    g_shop_group = type;
                }
                shoplist_redraw();
            }
        } else if (g_shop_group < 8 && mouse_y() > 6 && mouse_y() < 0x1f) { /* Darsteller im Regal */
            for (c = 1; c < 6; c++) {
                if (mouse_x() < c * 0x24 + 0x54 || mouse_x() > c * 0x24 + 0x73)
                    continue;
                DS_I16(0x79b6) = c + g_shop_scroll;
                int16_t a = DS_I16(0x79b6);
                int32_t price = ACTOR_FEE(g_shop_group, a);
                g_item = a;
                if (money_check(p, price) != 0)
                    continue;
                if ((price > 10000 && ME_IMG < 0x14) || (price > 5000 && ME_IMG < 0xf) || ME_IMG < 10) {
                    /* zu berühmt für das eigene Image */
                    char *s = fmt(g_text(0x99), name_table_get(DS_I16(0x4ff3 + (g_shop_group - 1) * 0xc0 + a * 0xc), 2));
                    show_message_text(s);
                    far_free(s);
                } else {
                    attach_item_at_mouse();
                    shelf_item_draw();
                    bmp_draw_masked(BANK_BMP(RBANK(0x108), 0x4a), g_drag_bmp, 0, 0);
                    g_dialog = 1;
                    DS_I16(0x79ae) = 1;
                }
            }
        } else if (g_shop_group >= 0x10 && mouse_y() >= 0x17 && mouse_y() <= 0x2e) { /* Stab in drei Preisstufen */
            for (c = 1; c < 4; c++) {
                if (mouse_x() < c * 0x47 + 0x14 || mouse_x() > c * 0x47 + 0x33)
                    continue;
                g_item = c;
                DS_I16(0x79b6) = c;
                if (money_check(p, STAFF_FEE(g_shop_group, c)) != 0)
                    continue;
                attach_item_at_mouse();
                staff_item_draw(c);
                g_dialog = 1;
                DS_I16(0x79ae) = 1;
            }
        }
        if (mouse_y() >= 0x4e && mouse_y() <= 0x65) { /* Rollenzeile: Besetzung wieder abnehmen */
            for (c = 1; c <= 3; c++) {
                int16_t idx = c + g_shop_roles;
                int8_t *type = &e[idx * 3 + 1], *cast = &e[idx * 3 + 3];
                if (mouse_x() < c * 0x58 - 0x14 || mouse_x() > c * 0x58 + 0xb)
                    continue;
                if (*type <= 0xf) {
                    int16_t g = (int8_t)sh[*type + 0x1a3];
                    if (g != g_shop_group)
                        g_shop_scroll = 0;
                    g_shop_group = g;
                    if (*cast == 0 || (int8_t)prod[idx + 4] == *cast) {
                        shoplist_redraw();
                    } else {
                        g_item = *cast;
                        DS_I16(0x79b6) = g_item;
                        *cast = (int8_t)prod[idx + 4];
                        ME_MONEY += ACTOR_FEE(g, g_item) * e[2];
                        shoplist_redraw();
                        attach_item_at_mouse();
                        shelf_item_draw();
                        bmp_draw_masked(BANK_BMP(RBANK(0x108), 0x4a), g_drag_bmp, 0, 0);
                        g_dialog = 1;
                    }
                } else {
                    g_shop_group = *type;
                    if (*cast == 0 || (int8_t)prod[idx + 4] == *cast) {
                        shoplist_redraw();
                    } else {
                        g_item = *cast;
                        DS_I16(0x79b6) = g_item;
                        *cast = (int8_t)prod[idx + 4];
                        ME_MONEY += STAFF_FEE(g_shop_group, g_item) * e[2];
                        shoplist_redraw();
                        attach_item_at_mouse();
                        staff_item_draw(g_item);
                        g_dialog = 1;
                    }
                }
                DS_I16(0x79ae) = 1;
                status_money_update();
            }
        }
        if (g_click_hotspot == 0x23) {
            production_episode_panel();
            screen_update();
            g_dialog = 2;
        }
        return;
    }
    if (g_dialog == 1) { /* getragenen Darsteller/Stab in eine Rolle legen */
        if (mouse_y() < 0x4e || mouse_y() > 0x65)
            return;
        for (c = 1; c <= 3; c++) {
            int16_t idx = c + g_shop_roles;
            int8_t *type = &e[idx * 3 + 1], *cast = &e[idx * 3 + 3];
            if (mouse_x() < c * 0x58 - 0x14 || mouse_x() > c * 0x58 + 0xb)
                continue;
            int16_t staff;
            if (g_shop_group <= 7 && (int8_t)sh[*type + 0x1a3] == g_shop_group)
                staff = 0;
            else if (g_shop_group >= 0x10 && *type == g_shop_group)
                staff = 1;
            else
                continue;
            if (*cast == 0 || (int8_t)prod[idx + 4] == *cast) {
                *cast = (int8_t)g_item;
                ME_MONEY -= (staff ? STAFF_FEE(g_shop_group, g_item) : ACTOR_FEE(g_shop_group, g_item)) * e[2];
                cursor_drop_item();
                g_dialog = 0;
            } else { /* tauschen */
                g_item_prog = *cast;
                ME_MONEY += (staff ? STAFF_FEE(g_shop_group, g_item_prog) : ACTOR_FEE(g_shop_group, g_item_prog)) * e[2];
                *cast = (int8_t)g_item;
                ME_MONEY -= (staff ? STAFF_FEE(g_shop_group, g_item) : ACTOR_FEE(g_shop_group, g_item)) * e[2];
                g_item = g_item_prog;
                DS_I16(0x79b6) = g_item;
                if (staff) {
                    bmp_copy_at(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(g_shop_group - 0xf))), g_drag_bmp, 0, 2);
                    drag_image_draw(BANK_BMP(RBANK(0x108), sprite_ofs((int16_t)(staff_icon(g_item) + 9))));
                } else {
                    shelf_item_draw();
                    drag_image_draw(BANK_BMP(RBANK(0x108), 0x4a));
                }
                DS_I16(0x79ae) = 1;
            }
            shoplist_redraw();
            status_money_update();
        }
        return;
    }
    if (g_dialog == 2 && g_click_hotspot == 0x24) { /* andere Produktion wählen */
        int16_t r = (mouse_y() - 6) / 0xc;
        if (r < DS_I16(0x7460 + p * 2)) {
            g_shop_list = r;
            g_shop_roles = 0;
            production_plan_panel();
            production_episode_panel();
            screen_update();
        }
    }
}
