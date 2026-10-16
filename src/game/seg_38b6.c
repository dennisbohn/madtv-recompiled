/* Segment 38b6 – Sprechblasen, Dialoge, Meldungsfenster und Bilanz */
#include "game.h"

#define g_banner_save DS_PTR(Bitmap, 0x8fe4) /* Hintergrund unter dem Meldungsbanner */

/* 38b6:0b96 – zweizeiliges Banner in Bildschirmmitte (mode 0 zeigen, mode 1 entfernen).
 * rec: zwei Einträge à 20 Byte, Text jeweils ab dem zweiten Byte (Byte 0 = Tastencode) */
void banner_two_lines(int16_t mode, char *rec)
{
    if (mode == 0) {
        Bitmap *tmp = bitmap_alloc(0xa0, 0x28);
        DS_SET_PTR(0x8fe4, bitmap_alloc(0xa0, 0x28));
        scr_grab(tmp, 10, 0x50);
        bmp_copy_at(tmp, g_banner_save, 0, 0);
        draw_frame_cells(0, 0, 0x13, 4, tmp, 2);
        text_draw_centered_to(0x50, 4, rec + 1, tmp);
        text_draw_centered_to(0x50, 0xc, rec + 0x15, tmp);
        scr_blit(tmp, 10, 0x50);
        far_free(tmp);
    } else if (mode == 1) {
        scr_blit(g_banner_save, 10, 0x50);
        far_free(g_banner_save);
    }
}

/* 38b6:1179 – Spieleranzeige in der Statuszeile (Lämpchen je Spieler: aus, an, aktiv) */
void status_bar_draw(void)
{
    uint8_t *bank = (uint8_t *)DS_PTR(SpriteBank, 0x79a2);
    Bitmap *tmp = bitmap_alloc(0x48, 0xb);
    mouse_hide();
    scr_grab(tmp, 2, 0xbc);
    bmp_draw_masked((Bitmap *)(bank + (DS_I16(0x79f0) == 0 ? 0x6a : 0x72)), tmp, 7, 0);
    for (int16_t p = 1; p < 4; p++) {
        int16_t ofs;
        if (DS_U8(0x8bd0 + p * 0x17) == 0)
            ofs = p == DS_I16(0x79de) ? 0x6a : 0x72;
        else
            ofs = 0x7a;
        bmp_draw_masked((Bitmap *)(bank + ofs), tmp, (int16_t)(p * 0xf + 7), 0);
    }
    scr_blit(tmp, 2, 0xbc);
    far_free(tmp);
    mouse_show();
}

#define g_text(id) PTR32(char, DS_ADDR(ptr32, 0x8370)[id])

/* 38b6:1269 – Bildschirm "Neues Spiel": drei Spalten (Name, Sender, Farbe, Steuerung) und
 * drei Schaltflächen; skip_player bekommt kein Namensfeld (wird gerade bearbeitet) */
void player_setup_screen(int16_t skip_player)
{
    frame_draw(4, 4, 0x13c, 0x70, 0);
    frame_draw(0x18, 8, 0x128, 0x14, 1);
    text_draw_centered_to(0xa0, 2, g_text(0x16e), DS_PTR(Bitmap, 0x8e62));
    for (int16_t p = 1; p < 4; p++) {
        int16_t color = DS_I8(0x14ef + p);
        int16_t x = (int16_t)(p * 0x60);
        frame_draw((int16_t)(x - 0x48), 0x1a, (int16_t)(x + 8), 0x48, 1);
        frame_draw((int16_t)(x - 0x44), 0x1c, (int16_t)(x + 4), 0x28, color);
        frame_draw((int16_t)(x - 0x44), 0x2c, (int16_t)(x + 4), 0x38, color);
        frame_draw((int16_t)(x - 0x44), 0x3a, (int16_t)(x + 4), 0x46, g_local_player == p ? 6 : 0);
        text_draw((int16_t)(x - 0x40), 0x16, DS_PTR(char, 0x8bc0 + p * 0x17));          /* Sendername */
        text_draw((int16_t)(x - 0x40), 0x34, g_text(g_local_player == p ? 0x14b : 0x14a)); /* Mensch/Computer */
        if (p != skip_player)
            text_draw((int16_t)(p * 0x60 - 0x40), 0x26, DS_PTR(char, 0x8bbc + p * 0x17)); /* Spielername */
        frame_draw((int16_t)(x - 0x44), 0x4c, (int16_t)(x + 4), 0x58, DS_I16(0x7a74) == p ? 6 : 0);
        text_draw_centered((int16_t)(x - 0x20), 0x46, g_text(p + 0x1a3));
    }
    frame_draw(0x18, 0x5e, 0x68, 0x6c, 4);
    text_draw_centered(0x40, 0x59, g_text(0x14c));
    frame_draw(0x78, 0x5e, 200, 0x6c, 4);
    text_draw_centered(0xa0, 0x59, g_text(0x14d));
    frame_draw(0xd8, 0x5e, 0x128, 0x6c, 5);
    text_draw_centered(0x100, 0x59, g_text(0x16f));
}

#define g_text(id)        PTR32(char, DS_ADDR(ptr32, 0x8370)[id])
#define g_upper_screen    DS_PTR(Bitmap, 0x8e62)
#define RBANK(ofs)        DS_PTR(uint8_t, ofs)
#define BANK_BMP(bank, ofs) ((Bitmap *)((uint8_t *)(bank) + (ofs)))
#define PLAYER_NAME(p)    DS_PTR(char, 0x8bbc + (p) * 0x17)
#define CHANNEL_NAME(p)   DS_PTR(char, 0x8bc0 + (p) * 0x17)
#define g_shows           DS_PTR(uint8_t, 0x713c)

static void rsprite(int16_t bank, int16_t ofs, int16_t x, int16_t y)
{
    draw_sprite(BANK_BMP(RBANK(bank), ofs), x, y);
}

/* 38b6:0003 – Liste der eigenen Produktionen (Haken = Besetzung komplett) und Titel der gewählten */
void production_episode_panel(void)
{
    int16_t p = g_local_player, sel = DS_I16(0x79c0);
    uint8_t *e = DS_ADDR(uint8_t, 0x732c + p * 0x4b + sel * 0xf);
    int16_t show = *(int16_t *)e, ep = *(int16_t *)(e + 2);
    int16_t prog = *(int16_t *)(g_shows + show * 0x1b3 + 6);
    int16_t done = episode_cast_complete(show, ep);
    for (int16_t i = 0; i < DS_I16(0x7460 + p * 2); i++) {
        uint8_t *q = DS_ADDR(uint8_t, 0x732c + p * 0x4b + i * 0xf);
        int16_t ok = episode_cast_complete(*(int16_t *)q, *(int16_t *)(q + 2));
        rsprite(0x108, sprite_ofs(ok ? 0x14 : 0x13), 0x35, (int16_t)(i * 0xc + 6));
        text_draw_free(0x37, (int16_t)(i * 0xc), fmt(DS_ADDR(char, 0x14f3), i + 1));
    }
    draw_frame(0x3c, 3, 0x132, 0x3e, g_upper_screen, 1);
    rsprite(0x108, sprite_ofs(done ? 0x14 : 0x13), 0x35, (int16_t)(sel * 0xc + 6));
    text_draw_free(0x37, (int16_t)(sel * 0xc), fmt(DS_ADDR(char, 0x14f6), sel + 1));
    char *s;
    if (*(uint16_t *)(g_shows + show * 0x1b3 + 5) & 1)
        s = fmt(g_text(0x176), series_title_get(prog), episode_title_get(prog, ep), episode_descr_get(prog, ep));
    else
        s = fmt(g_text(0x175), film_title_get(prog), show_text_title_get(show));
    text_box_wrap_free(0x46, 3, s, 0x1d, 1);
}

/* 38b6:0253 – Genre-Menü mit Anzahl je Genre (5 = Koffer enthält eins) */
void genre_menu_dialog(int16_t xcol, int16_t yrow, int16_t mode)
{
    int16_t *cnt = DS_ADDR(int16_t, 0x8fc2);
    for (int16_t g = 1; g <= 0xb; g++) {
        cnt[g] = DS_I8(0x6f59 + DS_I16(0x7a16) * 0x6c + g);
        if (g_local_player == DS_I16(0x7a16)) {
            for (int16_t i = 0; i < DS_I16(0x732a); i++) {
                int16_t prog = DS_I16(0x2463 + i * 2);
                /* nur ein volles Fach (4) zeigt 5 */
                if (((g < 0xb && prog <= 999 && (int8_t)DS_PTR(uint8_t, 0x2489)[prog * 0x17] == g) ||
                     (g == 0xb && prog > 1000)) && cnt[g] == 4) {
                    cnt[g] = 5;
                    break;
                }
            }
        }
    }
    clip_reset();
    if (mode == 0) {
        char *s = fmt(DS_ADDR(char, 0x14f9), cnt[1], g_text(0x1b), cnt[2], g_text(0x1c), cnt[3], g_text(0x1d), cnt[4],
                      g_text(0x1e), cnt[5], g_text(0x1f), cnt[6], g_text(0x20), cnt[7], g_text(0x21), cnt[8],
                      g_text(0x22), cnt[9], g_text(0x23), cnt[10], g_text(0x24), cnt[11], g_text(0x25));
        DS_I16(0x8fc0) = window_open(xcol, yrow, xcol + 0xb, yrow + 0xb, s, 1);
        DS_I16(0x79b0) = 1;
    } else if (mode == 1) {
        window_close(DS_I16(0x8fc0));
        DS_I16(0x79b0) = 0;
    }
}

/* 38b6:03ca – Sprechblase (Betty) mit Text; mode 0 wechselt danach in den Gesprächsraum -6 */
void bubble_text_overlay(int16_t mode, char *text)
{
    speech_bubble(mode, 0x15, 2, 0x26, 0xd, 1);
    if (mode == 0) {
        text_box_wrap_free(0xb0, 0x10, text, 0xf, 1);
        screen_update();
        room_enter(-6);
    }
}

/* 38b6:041f – Chefgespräch: obere Blase mit Satz, untere mit Menüauswahl */
void boss_dialog(int16_t mode, int16_t text_sel)
{
    int16_t img = DS_I16(0x8bd1 + g_local_player * 0x17), id = 0;
    if (text_sel == -1)
        text_sel = DS_I16(0x14ea);
    if (mode == 0)
        DS_I16(0x14ec) = 0;
    speech_bubble(mode, 0x13, 1, 0x26, 8, 1);
    int16_t lower = DS_I16(0x7a6e) == -1 ? 1 : mode;
    if (DS_I16(0x14ec) == 0) {
        speech_bubble(lower, 0x13, 9, 0x26, 0xd, 2);
        if (lower == 1)
            DS_I16(0x14ec) = 1;
    }
    if (mode == 1)
        return;
    if (DS_I16(0x7a6e) > 0) {
        rsprite(0x100, 0x2a, 0x9c, 0x4c);
        rsprite(0x100, 0x32, 0x9c, 0x5c);
    }
    if (DS_I16(0x7a6e) == 0) {
        text_box_wrap(0xb0, 0x4c, g_text(0xab), 0xf, 1);
        id = img < 10 ? 0xac : img < 0x4b ? 0xad : 0xae;
        text_box_wrap_free(0x9c, 0xc, fmt(g_text(id), PLAYER_NAME(g_local_player)), 0x12, 1);
        DS_I16(0x14ea) = id;
    }
    if (DS_I16(0x7a6e) > 0) {
        for (int16_t i = 0; i < 0xf; i++) {
            if (DS_I16(0x1490 + i * 6) == DS_I16(0x7a6e) && DS_I16(0x1492 + i * 6) == DS_I16(0x7a70)) {
                id = DS_I16(0x1494 + i * 6);
                break;
            }
        }
        text_box_wrap(0xb0, 0x4c, g_text(id), 0xf, 1);
    }
    if (DS_I16(0x7a6e) != 0) {
        text_box_wrap_free(0x9c, 0xc, fmt(g_text(text_sel), PLAYER_NAME(g_local_player)), 0x12, 1);
        DS_I16(0x14ea) = text_sel;
    }
}

/* 38b6:066a – Fallblöcke-Minispiel (Spielfeld 10x20 bei DS:8ab4) */
void minigame_panel(int16_t mode)
{
    clip_reset();
    if (mode == 0 || mode == 2) {
        if (mode == 0)
            DS_I16(0x8fda) = window_open(9, 0, 0x1f, 0xe, NULL, 3);
        rsprite(0x100, 0x52, 0x67, 4);
        DS_I16(0x038a) = 8;
        DS_I16(0x038e) = 0x6e;
        text_draw_free(0x6c, 0x31, fmt(DS_ADDR(char, 0x153b), DS_I16(0x7a60)));
        text_draw_free(0x6c, 0x1d, fmt(DS_ADDR(char, 0x1540), 9 - DS_I16(0x7a5e)));
        text_draw_free(0x6c, 9, fmt(DS_ADDR(char, 0x1545), DS_I16(0x7a56)));
        text_draw_free(0x6c, 0x45, fmt(DS_ADDR(char, 0x154a), DS_I16(0x7a64)));
        uint8_t *grid = DS_ADDR(uint8_t, 0x8ab4);
        for (int16_t r = 0; r < 0x14; r++)
            for (int16_t c = 0; c < 10; c++)
                if (grid[c + r * 10] != 0)
                    rsprite(0x100, 0x82, (int16_t)(c * 5 + 0x88), (int16_t)(r * 5 + 9));
        for (int16_t r = 0; r < 0x14; r++)
            for (int16_t c = 0; c < 10; c++)
                if (grid[c + r * 10] != 0)
                    rsprite(0x100, sprite_ofs((int16_t)(grid[c + r * 10] + 0xb)), (int16_t)(c * 5 + 0x89),
                            (int16_t)(r * 5 + 8));
        for (int16_t i = 0; i < 4; i++) { /* nächster Stein */
            for (int16_t j = 0; j < 4; j++) {
                uint8_t b = DS_U8(0x39c + (DS_I16(0x7a5a) - 1) * 0x40 + i * 4 + j);
                if (b != 0)
                    rsprite(0x100, sprite_ofs((int16_t)(b + 0xb)), (int16_t)(i * 5 + 0x6a), (int16_t)(j * 5 + 0x5a));
            }
        }
        int16_t t = DS_I16(0x7a54);
        if (t < 0xe)
            rsprite(0x100, 0xaa, 0x8f, 0x1e);
        else if (t < 0x1c)
            rsprite(0x100, 0xb2, 0x8c, 0x1e);
        else if (t < 0x2a)
            rsprite(0x100, 0xba, 0x98, 0x1e);
        if (t > 0x2a) { /* Spiel vorbei */
            rsprite(0x100, 0xc2, 0x8f, 0x1e);
            rsprite(0x100, 0xca, 0x92, 0x32);
            if (++DS_I16(0x7a54) > 0x38) {
                DS_I16(0x7a54) = 0;
                far_memset(DS_ADDR(uint8_t, 0x8ab4), 200);
                DS_I16(0x7a5e) = 8;
                DS_I16(0x7a60) = 0;
                DS_I16(0x7a54) = 0;
                DS_I16(0x7a56) = 0;
                DS_I16(0x7a5c) = 0;
                DS_I16(0x7a62) = 0;
                DS_I16(0x7a58) = 0;
                DS_I16(0x7a5a) = rand_range(7, 1);
            }
        }
    } else if (mode == 1) {
        window_close(DS_I16(0x8fda));
    }
}

/* 38b6:09b3 – Namen der 10 Spielstände */
void savegame_slots_draw(void)
{
    for (int16_t col = 0; col < 2; col++) {
        for (int16_t r = 1; r < 6; r++) {
            rsprite(0x104, 0x22, (int16_t)(col * 0x76 + 0x40), (int16_t)(r * 0x11 + 8));
            text_draw((int16_t)(col * 0x76 + 0x40), (int16_t)(r * 0x11), PTR32(char, DS_ADDR(ptr32, 0x8b92)[col * 5 + r - 1]));
        }
    }
    screen_update();
}

/* 38b6:0a2f */
void info_bubble_dialog(int16_t mode)
{
    speech_bubble(mode, 10, 5, 0x1d, 9, 2);
    if (mode != 1) {
        text_box(0x54, 0x24, g_text(0x11e), 0x12, 1, g_upper_screen, -1);
        screen_update();
    }
}

/* 38b6:0a88 – Diskette einlegen: Kasten direkt auf dem Bildschirm */
void disk_request_dialog(int16_t mode, int16_t text_id)
{
    if (mode == 0) {
        Bitmap *box = bitmap_alloc(0xa0, 0x28);
        DS_SET_PTR(0x8fdc, box);
        DS_SET_PTR(0x8fe0, bitmap_alloc(0xa0, 0x28));
        scr_grab(box, 10, 0x28);
        bmp_copy_at(box, DS_PTR(Bitmap, 0x8fe0), 0, 0);
        draw_frame_cells(0, 0, 0x13, 4, box, 2);
        text_box(4, 4, g_text(text_id), 0x12, 1, box, -1);
        scr_blit(box, 10, 0x28);
        far_free(box);
    } else if (mode == 1) {
        scr_blit(DS_PTR(Bitmap, 0x8fe0), 10, 0x28);
        far_free(DS_PTR(Bitmap, 0x8fe0));
    }
}

/* 38b6:0cb4 – große Sprechblase oben rechts */
void bubble_text_dialog(int16_t mode, char *text)
{
    speech_bubble(mode, 0x13, 1, 0x26, 0xd, 2);
    if (mode != 1)
        text_box_wrap_free(0x9c, 0xc, text, 0x12, 1);
}

/* 38b6:0cfa */
void bubble_text_dialog_small(int16_t mode, char *text)
{
    speech_bubble(mode, 0x1a, 2, 0x26, 0xc, 1);
    if (mode != 1) {
        text_box_wrap_free(0xd4, 0x14, text, 0xb, 1);
        screen_update();
    }
}

/* 38b6:0d45 – Sprechblase zeichnen; mode 0 rettet den Hintergrund (Stapel DS:8fe8), 1 stellt ihn her */
void speech_bubble(int16_t mode, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t style)
{
    clip_reset();
    if (mode == 0 || mode == 2) {
        if (mode == 0) {
            DS_I16(0x8fe8 + DS_I16(0x14ee) * 2) = window_open(x0, y0, x1, y1, NULL, 3);
            DS_I16(0x14ee)++;
        }
        draw_frame_cells(x0, y0, x1, y1, g_upper_screen, style);
    } else if (mode == 1) {
        DS_I16(0x14ee)--;
        window_close(DS_I16(0x8fe8 + DS_I16(0x14ee) * 2));
    }
}

/* 38b6:0dc5 */
void speech_bubble_text52(int16_t mode)
{
    speech_bubble(mode, 2, 2, 0x12, 7, 1);
    if (mode != 1)
        text_box_wrap_free(0x14, 0x14, text_get_copy(0x52), 0xd, 1);
}

/* 38b6:0e0a – Optionen: Spieltempo (20/30/60) und drei Schalter */
void options_panel(void)
{
    frame_draw(0x58, 8, 200, 0x72, 0);
    rsprite(0x10c, 0xa2, 0x61, 0xc);
    frame_draw(0x60, 0x1a, 0xc0, 0x30, 1);
    frame_draw(0x60, 0x32, 0xc0, 0x48, 1);
    frame_draw(0x60, 0x4a, 0xc0, 0x60, 1);
    DS_SET_PTR(0x8f2e, RBANK(0x10c));
    DS_SET_PTR(0x8f2a, g_upper_screen);
    bank_draw(sprite_ofs(DS_I16(0x7282) == 0x14 ? 0x18 : 0x19), 0x60, 0x62);
    bank_draw(sprite_ofs(DS_I16(0x7282) == 0x1e ? 0x18 : 0x19), 0x82, 0x62);
    bank_draw(sprite_ofs(DS_I16(0x7282) == 0x3c ? 0x18 : 0x19), 0xa4, 0x62);
    bank_draw(0xca, 0x61, 99);
    bank_draw(0xd2, 0x83, 99);
    bank_draw(0xda, 0xa5, 99);
    text_draw_centered(0x90, 0x14, g_text(0x53));
    text_draw_centered(0x90, 0x2c, g_text(0x54));
    text_draw_centered(0x90, 0x44, g_text(0x58));
    static const int16_t opt[3] = {0x7a10, 0x79f0, 0x79f2};
    for (int16_t i = 0; i < 3; i++) {
        option_switch_draw(DS_I16(opt[i]));
        blit_sprite_masked(DS_PTR(Bitmap, 0x110), g_upper_screen, 0x80, (int16_t)(0x24 + i * 0x18), 0x20, 0, 0x20, 10);
    }
    screen_update();
}

/* 38b6:1059 – Ein/Aus-Schieber in die Bitmap DS:0110 zeichnen (pos 0..30) */
void option_switch_draw(int16_t pos)
{
    Bitmap *dst = DS_PTR(Bitmap, 0x110);
    bmp_draw_masked(BANK_BMP(RBANK(0x10c), 0xb2), dst, (int16_t)(0x21 - pos), 1);
    text_draw_centered_to((int16_t)(0x31 - pos), -6, g_text(0x5a), dst);
    text_draw_centered_to((int16_t)(0x4f - pos), -6, g_text(0x5b), dst);
    bmp_draw_masked(BANK_BMP(RBANK(0x10c), 0xaa), dst, 0x20, 0);
}

/* 38b6:10f0 – ohne Aufrufer im Original */
void bubble_dialog_8820(int16_t mode)
{
    speech_bubble(mode, 0x19, 1, 0x26, 0xc, 1);
    if (mode != 1) {
        text_box(0xcc, 4, g_text(0x12c), 0xc, 1, g_upper_screen, -1);
        rsprite(0x100, 0x12, 0xe4, 0x50);
        rsprite(0x100, 0x1a, 0x10c, 0x50);
    }
}

/* 38b6:14e5 – Senderverkauf: Wert und Gebote der beiden KI-Sender */
void station_bid_dialog(int16_t mode, int32_t *bids)
{
    if (mode == 0 || mode == 2) {
        if (mode == 0)
            DS_I16(0x8ff2) = window_open(10, 3, 0x1d, 0xc, NULL, 3);
        draw_frame_cells(10, 3, 0x1d, 0xc, g_upper_screen, 2);
        int32_t value = DS_I32(0x1ce9 + g_local_player * 0x1ce + DS_I16(0x79a8) * 0xe);
        text_box_wrap_free(0x58, 0x18, fmt(g_text(0x170), value), 0x12, 1);
        draw_tile_box(10, 0x28, 0x13, 10, 3);
        text_draw(0x58, 0x21, g_text(0x171));
        for (int16_t i = 0; i < 2; i++) {
            if (bids[i] == -1)
                continue;
            int8_t ai = DS_I8(0xf2 + i);
            rsprite(0x104, sprite_ofs((int16_t)(ai + 0x12)), 0x58, (int16_t)(i * 0xb + 0x33));
            text_draw(0x5a, (int16_t)(i * 0xb + 0x2d), CHANNEL_NAME(ai));
            if (bids[i] > 0)
                text_draw_free(0x80, (int16_t)(i * 0xb + 0x2d), fmt(g_text(0x172), bids[i]));
        }
        draw_tile_box(10, 0x49, 0x13, 10, 3);
        text_draw(0x5a, 0x42, g_text(0x173));
        rsprite(0x104, 0xaa, 0x58, 0x54);
        text_draw(0x5a, 0x4d, g_text(0x174));
    } else if (mode == 1) {
        window_close(DS_I16(0x8ff2));
    }
}

/* 38b6:1721 – Bettys Einschätzung des Spielers */
void player_status_bubble(int16_t mode)
{
    speech_bubble(mode, 0x15, 2, 0x26, 0xd, 1);
    if (mode == 1)
        return;
    int16_t m = DS_I16(0x8bc8 + g_local_player * 0x17);
    int16_t id = m == 0 ? 0x17 : m < 0x1e ? 0x18 : 0x19;
    if (DS_U8(0x8bd0 + g_local_player * 0x17) != 0)
        id = 0x105;
    text_box_wrap_free(0xb0, 0x10, fmt(DS_ADDR(char, 0x154f), g_text(0x16), PLAYER_NAME(g_local_player), g_text(id)), 0xf, 1);
    screen_update();
}

static char *topic_name(int32_t v)
{
    return v < 0xb ? g_text(v + 0x1a) : g_text(v + 0x11a);
}

/* 38b6:17fd – älteste Nachricht im Briefkasten anzeigen und entfernen */
void player_message_dialog(uint16_t player)
{
    uint8_t *m = DS_ADDR(uint8_t, 0x609f);
    int16_t id = *(int16_t *)m;
    int16_t a1 = *(int16_t *)(m + 2), a2 = *(int16_t *)(m + 6);
    char *msg;
    draw_frame_cells(5, 2, 0x22, 0xc, g_upper_screen, 1);
    text_box_wrap_free(0x2c, 0xc, fmt(g_text(0x16c), PLAYER_NAME(player)), 0x13, 1);
    if (id == 0x10e) { /* Produktion fertig */
        uint8_t *sh = g_shows + a2 * 0x1b3;
        int16_t prog = *(int16_t *)(sh + 6);
        char *title, *mid, *ep;
        if (*(uint16_t *)(sh + 5) & 1) {
            text_cache_descr(0, prog);
            title = series_title_get(prog);
            ep = episode_title_get(prog, *(int16_t *)(m + 0xa));
            mid = g_text(0x16d);
        } else {
            text_cache_titles(0, prog);
            title = film_title_get(prog);
            mid = DS_ADDR(char, 0x1559);
            ep = DS_ADDR(char, 0x155a);
        }
        char *s = fmt(DS_ADDR(char, 0x155b), title, mid, ep);
        if (*(uint16_t *)(sh + 5) & 1)
            text_cache_descr_free(-1);
        else
            text_cache_titles_free(-1);
        msg = fmt(g_text(0x10e), a1, s, *(int16_t *)(m + 0xe));
        far_free(s);
    } else if (id == 0x10f || id == 0x110) {
        msg = fmt(g_text(id), MEM_PTR(char, DS_ADDR(uint8_t, 0x3f2f + a1 * 0x18)), *(int32_t *)(m + 6));
    } else if (id == 0x133 || id == 0x136) {
        msg = fmt(g_text(id), a1);
    } else if (id == 0x152) {
        msg = fmt(g_text(0x152), g_text(a1 + 0x121));
    } else if (id == 0x158) {
        msg = fmt(g_text(0x158), g_text(a1 + 0x153), PLAYER_NAME(a2));
    } else if (id == 0x159) {
        msg = fmt(g_text(0x159), PLAYER_NAME(player), g_text(a1 + 0x153));
    } else if (id == 0x2b) {
        msg = fmt(g_text(0x2b), g_text(a1 + 0x153));
    } else if (id == 0x15f) {
        msg = fmt(g_text(0x15f), topic_name(*(int32_t *)(m + 2)), topic_name(*(int32_t *)(m + 6)),
                  topic_name(*(int32_t *)(m + 0xa)));
    } else if (id == 0x113 || id == 0x15b) {
        text_cache_titles(0, a1);
        if (id == 0x113)
            msg = fmt(g_text(0x113), film_title_get(a1));
        else
            msg = fmt(g_text(0x15b), PLAYER_NAME(player), film_title_get(a1));
        text_cache_titles_free(-1);
    } else {
        msg = text_get_copy(id);
    }
    text_box_wrap_free(0x2c, 0x1c, msg, 0x1d, 1);
    for (int16_t i = 0; i < 4; i++)
        far_memcpy(DS_ADDR(uint8_t, 0x609f + i * 0x1a), DS_ADDR(uint8_t, 0x609f + (i + 1) * 0x1a), 0x1a);
    DS_I8(0x7227)--;
    screen_update();
}

/* 38b6:1d91 – kleine Info zu einem Spieler (Text nach DS:79a6[0x3a+p]) */
void player_info_popup(uint16_t player)
{
    Bitmap *b = bitmap_alloc(0xf0, 0x58);
    scr_grab(b, 5, 0x10);
    draw_frame_cells(0, 0, 0x1d, 10, b, 1);
    text_box_to_free(4, 4, fmt(g_text(DS_I16(0x79a6 + (player + 0x3a) * 2)), PLAYER_NAME(player)), 0x1d, 1, b);
    mouse_hide();
    scr_blit(b, 5, 0x10);
    mouse_show();
    far_free(b);
}

/* 38b6:1e63 – Bilanz (heute DS:71d1 oder gestern DS:7472, 21 Konten) */
void balance_sheet(void)
{
    int32_t bids = 0, exp = 0, inc = 0;
    DS_I16(0x0386) = 1;
    DS_I16(0x0388) = 4;
    DS_I16(0x038c) = 0x13b;
    DS_I16(0x038a) = 4;
    DS_I16(0x038e) = 0x74;
    for (int16_t y = 0; y < 0x71; y += 8) {
        for (int16_t x = 1; x < 0x27; x++)
            rsprite(0x10c, 10, (int16_t)(x << 3), y);
        rsprite(0x10c, 2, 4, y);
        rsprite(0x10c, 2, 0x138, y);
    }
    clip_reset();
    int32_t *f;
    if (DS_I16(0x79d8) == 0) {
        f = DS_ADDR(int32_t, 0x7472);
    } else {
        f = DS_ADDR(int32_t, 0x71d1);
        for (int16_t i = 0; i < 0xb; i++) /* laufende Gebote */
            if (DS_I16(0x1aed + i * 9) != 0 && DS_I8(0x1af3 + i * 9) == g_local_player)
                bids += DS_I32(0x1aef + i * 9);
    }
    draw_frame(8, 2, 0x98, 0x60, g_upper_screen, 1);
    text_draw(0xc, -2, g_text(0x114));
    hline_tiles_draw(0xc, 0x9c, 0xe);
    text_draw(0xc, 8, g_text(0x115));
    text_draw_free(0x6c, 8, fmt(DS_ADDR(char, 0x1564), f[2] + f[3] + bids, f[4], f[5], f[8], f[6], f[9], f[11], f[10],
                                f[7] + f[17]));
    hline_tiles_draw(0xc, 0x9c, 0x58);
    text_draw(0xc, 0x52, g_text(0x119));
    for (int16_t i = 1; i <= 10; i++)
        exp += f[i + 1];
    exp += f[17] + bids;
    text_draw_free(0x6c, 0x52, fmt(DS_ADDR(char, 0x15a3), exp));
    draw_frame(0xa0, 2, 0x130, 0x2c, g_upper_screen, 1);
    draw_frame(0xa0, 0x32, 0x130, 0x60, g_upper_screen, 1);
    text_draw(0xa4, -2, g_text(0x10d));
    hline_tiles_draw(0xa4, 0x134, 0xe);
    text_draw(0xa4, 8, g_text(0x117));
    text_draw_free(0x104, 8, fmt(DS_ADDR(char, 0x15aa), f[12] + f[13], f[15], f[14]));
    hline_tiles_draw(0xa4, 0x134, 0x27);
    text_draw(0xa4, 0x21, g_text(0x119));
    for (int16_t i = 0xb; i < 0xf; i++)
        inc += f[i + 1];
    text_draw_free(0x104, 0x21, fmt(DS_ADDR(char, 0x15bf), inc));
    text_draw(0xa4, 0x2e, g_text(0x118));
    hline_tiles_draw(0xa4, 0x134, 0x3e);
    text_draw(0xa4, 0x38, g_text(0x11a));
    hline_tiles_draw(0xa4, 0x134, 0x58);
    text_draw(0xa4, 0x52, g_text(0x11b));
    text_draw_free(0x104, 0x2e, fmt(DS_ADDR(char, 0x15c6), f[0]));
    text_draw_free(0x104, 0x38, fmt(DS_ADDR(char, 0x15cd), inc, exp, f[16]));
    text_draw_free(0x104, 0x52, fmt(DS_ADDR(char, 0x15e2), f[0] + inc - exp - f[16]));
    rsprite(0x10c, 0x2a, 0x88, 0x68);
    text_draw(0x8f, 0x62, g_text(DS_I16(0x79d8) + 0x102));
    screen_update();
}

/* 38b6:2396 – waagerechte Linie aus 8-px-Kacheln */
void hline_tiles_draw(int16_t x0, int16_t x1, int16_t y)
{
    for (; x0 <= x1 - 8; x0 += 8)
        rsprite(0x10c, 0x9a, x0, y);
    rsprite(0x10c, 0x9a, (int16_t)(x1 - 8), y);
}

/* 38b6:23e5 */
void yes_no_dialog(int16_t mode)
{
    speech_bubble(mode, 10, 3, 0x1d, 0xb, 1);
    if (mode != 1) {
        text_box(0x54, 0x14, g_text(0x1aa), 0x12, 1, g_upper_screen, -1);
        rsprite(0x104, 0x12, 0x80, 0x48);
        rsprite(0x104, 0x1a, 0xb8, 0x48);
        screen_update();
    }
}

/* 38b6:2473 */
void yes_no_dialog2(int16_t mode)
{
    speech_bubble(mode, 10, 3, 0x1d, 0xb, 2);
    if (mode != 1) {
        text_box(0x54, 0x14, g_text(0x11d), 0x12, 1, g_upper_screen, -1);
        rsprite(0x104, 2, 0x7b, 0x40);
        rsprite(0x104, 10, 0xb5, 0x40);
        screen_update();
    }
}
