/* Segment 340d – Meldungen, Statuszeile, Programmplaner und Programmlisten */
#include <string.h>

#include "game.h"

#define g_text(id) PTR32(char, DS_ADDR(ptr32, 0x8370)[id])

/* 340d:0002 – Meldung (Text in Vorlage Nr. 89 eingesetzt) als Sprechblase zeigen */
void show_message_text(char *text)
{
    char *s = fmt(g_text(0x59), text);
    room_enter(-1);
    message_box_draw(0, s);
    far_free(s);
}

/* 340d:0050 */
void show_message(int16_t text_id)
{
    show_message_text(g_text(text_id));
}

/* 340d:006a – Info-Einblendung (Raum -3) */
void show_info_overlay(char *text)
{
    room_enter(-3);
    message_box_draw(0, text);
}

/* 340d:0089 – Sprechblasen-Fenster öffnen (mode 0) bzw. schließen (1) und Text zeigen */
void message_box_draw(int16_t mode, char *text)
{
    speech_bubble(mode, 10, 3, 0x1e, 8, 1);
    if (mode != 1)
        text_box_wrap(0x54, 0x1c, text, 0x13, 1);
    screen_update();
}

/* 340d:00d4 – Kontostand (in Tausend) in der Statuszeile */
void status_money_update(void)
{
    char *s = fmt(DS_ADDR(char, 0x1344), money_available(g_local_player) / 1000); /* "#_6l" */
    status_text(s, 0x7f);
    far_free(s);
}

/* 340d:0128 – Text zentriert in das Statusfeld (DS:8e88) und an Zeile y anzeigen */
void status_text(char *text, int16_t y)
{
    int16_t hidden = 0;
    clip_reset();
    bmp_copy(DS_PTR(Bitmap, 0x8e66), DS_PTR(Bitmap, 0x8e88));
    text_draw_centered_to(0x15, -6, text, DS_PTR(Bitmap, 0x8e88));
    if (y - 9 <= g_cursor_y && g_cursor_y <= y + 10 && g_cursor_y > 0x87 && g_cursor_x < 0xc9 &&
        DS_I16(0x8aac) == 1) {
        mouse_hide();
        hidden = 1;
    }
    if (DS_I16(0x7a4c) == 0)
        scr_blit(DS_PTR(Bitmap, 0x8e88), 0x13, y);
    if (hidden)
        mouse_show();
}

/* 340d:01cb – Balkenanzeige (46 Segmente) für den Prozentwert des Spielers */
void status_meter_draw(void)
{
    clip_set_upper();
    Bitmap *tmp = bitmap_alloc(0x30, 2);
    scr_grab(tmp, 0x10, 0xab);
    for (int16_t i = 1; i < 0x2f; i++) {
        int16_t sprite = DS_I16(0x8bc8 + g_local_player * 0x17) < (i * 100) / 0x2e ? 0xd : 0xc;
        bmp_draw_masked((Bitmap *)((uint8_t *)DS_PTR(SpriteBank, 0x79a2) + sprite_ofs(sprite)), tmp, i, 0);
    }
    scr_blit(tmp, 0x10, 0xab);
    far_free(tmp);
}

/* 340d:1911 – Kasten in die Spielszene */
void frame_draw(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t style)
{
    frame_draw_to(x0, y0, x1, y1, style, DS_PTR(Bitmap, 0x8e62));
}

/* 340d:1934 – Kasten aus 8x2-Kacheln der Raum-Bank DS:0100; Stil wählt den Kachelsatz (DS:132e) */
void frame_draw_to(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t style, Bitmap *dst)
{
    int16_t base = DS_I8(0x132e + style);
    DS_I16(0x0386) = 0; /* ohne Clipping */
    DS_SET_PTR(0x8f2e, (uint8_t *)DS_PTR(SpriteBank, 0x0100));
    DS_SET_PTR(0x8f2a, dst);
    for (int16_t x = (int16_t)(x0 + 8); x < x1 - 8; x += 8) {
        bank_draw(sprite_ofs((int16_t)(base + 1)), x, y0);           /* oben */
        bank_draw(sprite_ofs((int16_t)(base + 7)), x, (int16_t)(y1 - 2)); /* unten */
        for (int16_t y = (int16_t)(y0 + 2); y < y1 - 2; y += 2)
            bank_draw(sprite_ofs((int16_t)(base + 4)), x, y);       /* Füllung */
    }
    bank_draw(sprite_ofs(base), x0, y0);
    bank_draw(sprite_ofs((int16_t)(base + 2)), (int16_t)(x1 - 8), y0);
    bank_draw(sprite_ofs((int16_t)(base + 6)), x0, (int16_t)(y1 - 2));
    bank_draw(sprite_ofs((int16_t)(base + 8)), (int16_t)(x1 - 8), (int16_t)(y1 - 2));
    for (int16_t y = (int16_t)(y0 + 2); y < y1 - 2; y += 2) {
        bank_draw(sprite_ofs((int16_t)(base + 3)), x0, y);
        bank_draw(sprite_ofs((int16_t)(base + 5)), (int16_t)(x1 - 8), y);
    }
}

#define g_upper_screen  DS_PTR(Bitmap, 0x8e62)
#define g_draw_bank     DS_PTR(uint8_t, 0x8f2e)
#define g_draw_target   DS_PTR(Bitmap, 0x8f2a)
#define g_draw_fixed_y  DS_I16(0x8f26)
#define g_font_select   DS_I16(0x8b86)
#define g_view_player   DS_I16(0x7a16) /* Spieler, dessen Planer/Archiv gezeigt wird */
#define g_sched_scroll  DS_I16(0x79a6) /* 0 oder 7 Stunden */
#define g_arch_genre    DS_I16(0x79aa)
#define RBANK(ofs)      DS_PTR(uint8_t, ofs)
#define BANK_BMP(bank, ofs) ((Bitmap *)((uint8_t *)(bank) + (ofs)))
#define SCHED(p, h)     DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define SCHED_EP(p, h)  DS_I16(0x4415 + (p) * 0x5a + (h) * 2)
#define SCHED_AD(p, h)  DS_I16(0x4433 + (p) * 0x5a + (h) * 2)
#define AD_NAME(n)      (MEM_PTR(char, DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)))
#define AD_SPOTS(n)     DS_I8(0x3f38 + (n) * 0x18)

/* 340d:0287 – Programmplaner: Hintergrund, Reiter, Briefkasten-Symbol, dann Stundenliste */
void schedule_panel_open(int16_t player)
{
    room_frame_draw_alt();
    DS_I16(0x0386) = 1;
    DS_I16(0x0388) = 4;
    DS_I16(0x038c) = 0x13b;
    DS_I16(0x038a) = 4;
    DS_I16(0x038e) = 0x74;
    DS_SET_PTR(0x8f2a, g_upper_screen);
    DS_SET_PTR(0x8f2e, RBANK(0x100));
    for (int16_t y = 0; y < 0x71; y += 8) {
        g_draw_fixed_y = y;
        for (int16_t x = 8; x < 0x138; x += 8)
            bank_draw_fixed_y(0x3a, x);
        bank_draw_fixed_y(0x32, 4);
        bank_draw_fixed_y(0x32, 0x138);
    }
    for (int16_t i = 1; i < 7; i++)
        bank_draw(sprite_ofs(i), 0x128, (int16_t)(i * 0x11 - 9));
    if (DS_I8(0x7227) > 0 && g_view_player == g_local_player)
        bank_draw(0x2ba, 0x128, 0x5d);
    frame_draw(8, 8, 0x20, 0x72, 1);
    DS_SET_PTR(0x8f2a, g_upper_screen);
    bank_draw(0x102, 0xc, 0x5d);
    schedule_view(player);
}

/* 340d:03a3 – sieben Stunden ab DS:79a6: Uhrzeit, Programm, Werbung */
void schedule_view(int16_t player)
{
    clip_reset();
    schedule_draw_rows(player);
    DS_SET_PTR(0x8f2e, RBANK(0x100));
    DS_SET_PTR(0x8f2a, g_upper_screen);
    bank_draw(0x42, 0xc, 0xc);
    bank_draw(0x4a, 0xc, 0x1e);
    bank_draw(0x52, 0xc, g_sched_scroll == 0 ? 0xc : 0x1e);
    g_font_select = 1;
    for (int16_t h = g_sched_scroll + 1; h <= g_sched_scroll + 7; h++) {
        int16_t y = (int16_t)((h - g_sched_scroll) * 0x10 - 0x12);
        int16_t clock = h;
        if (clock > 7)
            clock -= 7;
        clock += 0x11;
        if (clock >= 0x18)
            clock -= 0x18;
        text_box_wrap_free(0x2a, y, fmt(DS_ADDR(char, 0x1349), clock), 0, 2); /* "#02d:00" */
        text_box_free(0x4a, y, schedule_slot_title(h, player), 0x11, 1, g_upper_screen, 2);
        int16_t ad = SCHED_AD(g_view_player, h);
        if (ad == 0)
            continue;
        char *s;
        if (ad < 1000) { /* laufender Vertrag: Spot n von m */
            int16_t sent = 0, i, n = 1;
            for (i = 0; i < DS_I16(0x7688 + g_view_player * 2); i++) {
                if (DS_I16(0x1b50 + g_view_player * 0x46 + i * 7) == ad) {
                    sent = DS_I8(0x1b54 + g_view_player * 0x46 + i * 7);
                    break;
                }
            }
            if (i >= DS_I16(0x7688 + g_view_player * 2))
                sent = AD_SPOTS(ad);
            for (int16_t k = hour_index_after_min55(); k < h; k++)
                if (SCHED_AD(g_view_player, k) == ad)
                    n++;
            s = fmt(DS_ADDR(char, 0x1351), AD_NAME(ad), sent + n, (int16_t)AD_SPOTS(ad));
        } else if (ad < 2000) {
            s = fmt(DS_ADDR(char, 0x135c), AD_NAME(ad - 1000));
        } else if (ad < 3000) {
            s = fmt(g_text(0x11c), AD_NAME(ad - 2000));
        } else {
            s = fmt(DS_ADDR(char, 0x1365), AD_NAME(ad - 3000), ad_spots_remaining(g_view_player, h),
                    (int16_t)AD_SPOTS(ad - 3000));
        }
        text_box_free(0xda, y, s, 8, 0, g_upper_screen, 2);
    }
    g_font_select = 0;
}

static int16_t row_style(int16_t past)
{
    return past && g_sched_scroll == 0 ? 2 : 0;
}

/* 340d:06dd – Rahmen je Stunde; vergangene Stunden dunkel, Länge nach Programmdauer */
void schedule_draw_rows(int16_t player)
{
    for (int16_t r = 1; r < 8; r++) {
        int16_t y0 = (int16_t)((r - 1) * 0x10 + 4), y1 = (int16_t)((r - 1) * 0x10 + 0x14);
        frame_draw(0x28, y0, 0x48, y1, (hour_index() < r || g_sched_scroll != 0) ? 0 : 2);
        int16_t st = row_style(r < hour_index() || (hour_index() == r && DS_I16(0x71cc) > 4) || DS_I16(0x012c) != 0);
        int16_t prog = SCHED(player, r + g_sched_scroll), len = 0x10;
        if (prog >= 1 && prog <= 2000) {
            if (prog < 1000)
                len = (int16_t)(DS_PTR(int8_t, 0x2489)[prog * 0x17 + 4] << 4);
            else
                len = (int16_t)(DS_PTR(int8_t, 0x2626)[(prog - 1000) * 0x39 + 0x23] << 4);
        } else if (prog == 0) {
            st++;
        } /* sonst im Original uninitialisiert */
        if (prog != 9999)
            frame_draw(0x48, y0, 0xd8, (int16_t)(y0 + len), st);
        st = row_style(r < hour_index() || (hour_index() == r && DS_I16(0x71cc) > 0x36));
        if (SCHED_AD(player, r + g_sched_scroll) == 0)
            st++;
        frame_draw(0xd8, y0, 0x118, y1, st);
    }
}

/* 340d:085f – Programmtitel einer Stunde (neuer String) */
char *schedule_slot_title(int16_t hour, int16_t player)
{
    int16_t prog = SCHED(player, hour);
    if (prog == 0)
        return fmt(DS_ADDR(char, 0x1370));
    if (prog < 1000)
        return fmt(DS_ADDR(char, 0x1371), film_title_get(prog));
    if (prog < 2000) {
        int16_t s = prog - 1000, ep = SCHED_EP(player, hour);
        if (ep == 0) {
            ep = series_next_episode(player, s, hour);
            return fmt(DS_ADDR(char, 0x1377), series_title_get(s), ep, episode_title_get(s, ep));
        }
        return fmt(DS_ADDR(char, 0x1387), series_title_get(s), ep, episode_title_get(s, ep));
    }
    if (prog == 9999) /* Fortsetzung eines langen Films */
        return fmt(DS_ADDR(char, 0x1397));
    return fmt(DS_ADDR(char, 0x1398)); /* "Fehler aufgetreten!" */
}

/* 340d:09a2 */
char *archive_item_title(int16_t genre, int16_t idx, int16_t player)
{
    return archive_title_by_id(
        (int16_t)(DS_I16(0x6ef9 + player * 0x6c + genre * 8 + idx * 2) + (genre == 0xb ? 1000 : 0)));
}

/* 340d:09e4 */
char *archive_title_by_id(int16_t prog_id)
{
    if (prog_id == 0)
        return text_get_copy(0x5c);
    if (prog_id < 1000)
        return fmt(DS_ADDR(char, 0x13ac), film_title_get(prog_id));
    return fmt(g_text(0x5d), series_title_get(prog_id - 1000));
}

/* 340d:0a56 – Archivliste des Genres DS:79aa als Fenster */
void archive_list_dialog(int16_t xcol, int16_t yrow, int16_t mode, int16_t player)
{
    int16_t x = xcol * 8, y = yrow * 8;
    clip_reset();
    if (mode == 0) {
        DS_I16(0x8fb2) = window_open(xcol, yrow, xcol + 0x14, yrow + 6, NULL, 3);
        frame_draw(x, y, (int16_t)((xcol + 0x14) * 8), (int16_t)((yrow + 6) * 8 + 2), 0);
        g_font_select = 1;
        text_draw_free((int16_t)(x + 4), (int16_t)(y - 6), fmt(DS_ADDR(char, 0x13af), g_text(g_arch_genre + 0x1a)));
        for (int16_t i = 0; i < DS_I8(0x6f59 + player * 0x6c + g_arch_genre); i++) {
            int16_t dy = (int16_t)(i * 10);
            if (DS_I16(0x6ef9 + player * 0x6c + g_arch_genre * 8 + i * 2) == 0)
                break;
            frame_draw((int16_t)(x + 4), (int16_t)(y + dy + 8), (int16_t)((xcol + 0x14) * 8 - 4), (int16_t)(y + dy + 0x12), 0);
            char *t = archive_item_title(g_arch_genre, i, player);
            text_draw_free((int16_t)(x + 5), (int16_t)(y + dy + 3), fmt(DS_ADDR(char, 0x13b3), t));
            far_free(t);
        }
        g_font_select = 0;
    } else if (mode == 1) {
        window_close(DS_I16(0x8fb2));
    }
}

#define g_station_player DS_I16(0x79ac)
#define STATION(p, i)    DS_ADDR(uint8_t, 0x1ce9 + (p) * 0x1ce + (i) * 0xe) /* +4 Preis, +8 x, +0xa y, +0xc Zustand */
#define g_drag_bitmap    DS_PTR(Bitmap, 0x0390)
#define g_drag_w         DS_I16(0x0394)
#define g_drag_h         DS_I16(0x0396)
#define g_drag_hot_x     DS_I16(0x0398)
#define g_drag_hot_y     DS_I16(0x039a)
#define g_cursor_grab    DS_PTR(Bitmap, 0x8e70)
#define g_cursor_bg_old  DS_PTR(Bitmap, 0x8ebe)
#define g_cursor_bg_new  DS_PTR(Bitmap, 0x8eb6)
#define g_cursor_old_x   DS_I16(0x7b06)
#define g_cursor_old_y   DS_I16(0x7b16)
#define g_cursor_shape   DS_I16(0x79b2)

static int16_t station_visible(int16_t p, int16_t i, int16_t part)
{
    uint8_t *st = STATION(p, i);
    return *(int32_t *)(st + 4) != 0 && *(int16_t *)(st + 0xc) != 2 && (part == 0 || part == i);
}

/* 340d:0c07 – Senderkarte: Rahmen, Spielerliste, Sendemasten des Spielers DS:79ac.
 * part 0 = alles, sonst nur Mast Nr. part */
void station_map_panel(int16_t part)
{
    int16_t p = g_station_player;
    clip_reset();
    DS_I16(0x038e) = 0x71;
    DS_SET_PTR(0x8f2e, RBANK(0x104));
    DS_SET_PTR(0x8f2a, g_upper_screen);
    for (int16_t y = 4; y < 0x71; y += 8)
        for (int16_t x = 4; x < 0x28; x += 8)
            bank_draw(0x62, x, y);
    draw_bitmap_fast(BANK_BMP(RBANK(0x100), 2), 0x28, 4);
    for (int16_t y = 4; y < 0x71; y += 8)
        for (int16_t x = 0xd8; x < 0x138; x += 8)
            bank_draw(0x62, x, y);
    for (int16_t y = 4; y < 0x71; y += 8)
        bank_draw(0x62, 0x134, y);
    bank_draw(0x32, 5, 0x46);
    bank_draw(0x2a, 0xdc, 0x28);
    for (int16_t i = 1; i < 4; i++) {
        if (DS_U8(0x8bd0 + i * 0x17) == 0) {
            bank_draw(sprite_ofs((int16_t)(i + 7)), 0x100, (int16_t)(i * 0xd + 0x37));
            text_draw(0x102, (int16_t)(i * 0xd + 0x31), DS_PTR(char, 0x8bbc + i * 0x17));
        }
    }
    if (p > 0)
        bank_draw(0x6a, 0x100, (int16_t)(p * 0xd + 0x37));
    if (g_view_player == g_local_player) {
        bank_draw(0x52, 0x108, 0x17);
        bank_draw(0x52, 0x108, 0x24);
        text_draw(0x10a, 0x11, g_text(0x139));
        text_draw(0x10a, 0x1e, g_text(0x153));
        if (DS_I16(0x8b84) == 3)
            bank_draw(0x5a, 0x108, 0x31);
    }
    if (p == 0)
        return;
    if (station_visible(p, 1, part)) /* Satellit */
        bank_draw(10, 5, 0x46);
    if (station_visible(p, 2, part)) /* Kabel */
        bank_draw(2, 0xdc, 0x28);
    for (int16_t i = 3; i < 0x21; i++) {
        if (station_visible(p, i, part)) {
            uint8_t *st = STATION(p, i);
            bank_draw(sprite_ofs((int16_t)(p + 2)), (int16_t)(*(int16_t *)(st + 8) + 0x30),
                      (int16_t)(*(int16_t *)(st + 0xa) + 4));
        }
    }
}

/* 340d:0f5b – Filmhändler-Regal: 10 Angebote (DS:19cd, 8 B); 0 = Hintergrund sichern, sonst herstellen */
void room8_items_panel(int16_t redraw)
{
    clip_reset();
    if (redraw == 0) {
        DS_SET_PTR(0x011c, bitmap_alloc(0x70, 0x2c));
        mouse_hide();
        screen_grab(DS_PTR(Bitmap, 0x011c), 0x48, 0x3b);
    } else {
        mouse_hide();
        draw_bitmap_fast(DS_PTR(Bitmap, 0x011c), 0x48, 0x3b);
    }
    mouse_show();
    for (int16_t i = 10; i > 0; i--)
        if (DS_I8(0x19d4 + i * 8) >= 0)
            draw_sprite(BANK_BMP(RBANK(0x10c), sprite_ofs(i)), DS_I16(0x32e + i * 8), DS_I16(0x330 + i * 8));
}

/* 340d:1022 – Rahmen aus Tabelle DS:12e6 (Spalten/Zeilen) mit Inhalt je nach Nummer:
 * 1/2 Koffer, 3 Werbeverträge, 4/5 Produktionen, 6/7 Geschenk */
void dialog_frame(int16_t mode, int16_t frame_id)
{
    int16_t c0 = 0, r0 = 0, c1 = 0, r1 = 0; /* im Original bei frame_id <= 0 uninitialisiert */
    if (frame_id > 0) {
        c0 = DS_I8(0x12e6 + (frame_id - 1) * 4);
        r0 = DS_I8(0x12e7 + (frame_id - 1) * 4);
        c1 = DS_I8(0x12e8 + (frame_id - 1) * 4);
        r1 = DS_I8(0x12e9 + (frame_id - 1) * 4);
    }
    int16_t x0 = c0 * 8, y0 = r0 * 8, x1 = c1 * 8, y1 = r1 * 8;
    clip_reset();
    if (mode == 1) {
        window_close(DS_I16(0x8fb4));
        return;
    }
    if (mode != 0 && mode != 2)
        return;
    if (mode == 0)
        DS_I16(0x8fb4) = window_open(c0, r0, c1, r1, NULL, 3);
    DS_SET_PTR(0x8f2e, RBANK(0x108));
    DS_SET_PTR(0x8f2a, g_upper_screen);
    bank_draw(2, x0, y0);
    bank_draw(10, x1, y0);
    bank_draw(0x12, x0, y1);
    bank_draw(0x1a, x1, y1);
    for (int16_t c = c0 + 1; c < c1; c++) {
        bank_draw(0x22, (int16_t)(c * 8), y0);
        bank_draw(0x3a, (int16_t)(c * 8), y1);
    }
    for (int16_t r = r0 + 1; r < r1; r++) {
        bank_draw(0x32, x0, (int16_t)(r * 8));
        bank_draw(0x2a, x1, (int16_t)(r * 8));
    }
    for (int16_t c = c0 + 1; c < c1; c++)
        for (int16_t r = r0 + 1; r < r1; r++)
            bank_draw(0x42, (int16_t)(c * 8), (int16_t)(r * 8));
    bank_draw(0x4a, (int16_t)(((c1 - c0) / 2 + c0) * 8), (int16_t)(r0 << 3));
    int16_t p = g_local_player, i, n;
    switch (frame_id) {
    case 1:
        for (i = 0; i < DS_I16(0x732a); i++)
            draw_sprite(BANK_BMP(RBANK(0x104), 2), (int16_t)(i * 9 + x0 + 8), (int16_t)(y0 + 8));
        break;
    case 2:
        for (i = 0; i < DS_I16(0x732a); i++)
            draw_sprite(BANK_BMP(RBANK(0x104), 2), (int16_t)(i * 9 + x0 + 8 - (i / 5) * 0x2d),
                        (int16_t)((i / 5) * 0x1e + y0 + 8));
        break;
    case 3:
        for (i = 0; i < DS_I16(0x7688 + p * 2); i++)
            draw_sprite(BANK_BMP(RBANK(0x104), 10), (int16_t)(i * 9 + x0 + 0xc - (i / 5) * 0x2d),
                        (int16_t)((i / 5) * 0x20 + y0 + 8));
        break;
    case 4:
        for (i = 0; i < DS_I16(0x7652 + p * 2); i++)
            draw_sprite(BANK_BMP(RBANK(0x104), 0x2a), (int16_t)(i * 9 + x0 + 10), (int16_t)(y0 + 9));
        break;
    case 5:
        for (i = 0; i < DS_I16(0x7652 + DS_I8(0x6148 + DS_I16(0x79be) * 0x10) * 2); i++)
            draw_sprite(BANK_BMP(RBANK(0x100), 10), (int16_t)(i * 9 + x0 + 10), (int16_t)(y0 + 9));
        break;
    case 6:
        n = DS_I8(0x74c6 + p);
        if (n != 0) {
            if (n > 100)
                n -= 100;
            draw_bitmap_fast(BANK_BMP(RBANK(0x10c), sprite_ofs(n)), (int16_t)(x0 + 8), (int16_t)(y0 + 8));
        }
        break;
    case 7:
        if (DS_I8(0x74c6 + p) != 0)
            draw_bitmap_fast(BANK_BMP(RBANK(0x100), sprite_ofs(DS_I8(0x74c6 + p))), (int16_t)(x0 + 8),
                             (int16_t)(y0 + 8));
        break;
    }
}

/* 340d:144f – Werbeagentur: bis zu 10 Angebote (DS:1980, 7 B) als Mappen im Regal */
void ad_offers_panel(void)
{
    clip_reset();
    if (DS_PTR(Bitmap, 0x0110) == NULL) {
        DS_SET_PTR(0x0110, bitmap_alloc(0x88, 0x46));
        mouse_hide();
        screen_grab(DS_PTR(Bitmap, 0x0110), 0x48, 0x2b);
    } else {
        mouse_hide();
        draw_bitmap_fast(DS_PTR(Bitmap, 0x0110), 0x48, 0x2b);
    }
    mouse_show();
    for (int16_t i = 10; i > 0; i--) {
        if (DS_I8(0x1982 + i * 7) < 0 || DS_I16(0x1980 + i * 7) == 0)
            continue;
        int16_t x, y, k;
        if (i >= 8) {
            x = 0x99, y = 0x2b, k = i - 8;
        } else if (i >= 4) {
            x = 0x7f, y = 0x3d, k = i - 4;
        } else {
            x = 0x6b, y = 0x4c, k = i - 1;
        }
        draw_sprite(BANK_BMP(RBANK(0x104), 2), (int16_t)(k * 8 + x), y);
    }
}

/* 340d:155a – Archivregal: Genre DS:79aa als Schild, je Programm eine Kassette */
void archive_shelf_panel(int16_t player)
{
    clip_reset();
    if (DS_PTR(Bitmap, 0x0110) == NULL) {
        DS_SET_PTR(0x0110, bitmap_alloc(0x50, 0x38));
        mouse_hide();
        screen_grab(DS_PTR(Bitmap, 0x0110), 0xa0, 0x30);
    } else {
        mouse_hide();
        draw_bitmap_fast(DS_PTR(Bitmap, 0x0110), 0xa0, 0x30);
    }
    mouse_show();
    const char *name = g_arch_genre < 0xb ? g_text(g_arch_genre + 0x1a) : g_text(0x25);
    text_window_draw(0x14, 10, 0x1d, 0xb, fmt(name), 1);
    for (int16_t i = 0; i < DS_I8(0x6f59 + player * 0x6c + g_arch_genre); i++)
        if (DS_I16(0x6ef9 + player * 0x6c + g_arch_genre * 8 + i * 2) > 0)
            draw_sprite(BANK_BMP(RBANK(0x104), 2), (int16_t)(i * 9 + 0xa0), 0x31);
}

/* 340d:167b – Folgenliste und Beschreibung der Serie DS:79b4; mode 0 öffnen, sonst schließen */
void series_episode_dialog(int16_t mode)
{
    char *ep[11] = {0};
    int16_t sid = DS_I16(0x79b4);
    uint8_t *ser = DS_PTR(uint8_t, 0x2626) + sid * 0x39;
    clip_reset();
    if (mode != 0) {
        window_close(DS_I16(0x8fb6));
        return;
    }
    for (int16_t i = 1; i <= (int8_t)ser[0x22] && i <= 10; i++)
        ep[i] = fmt(DS_ADDR(char, 0x13b9), i, episode_title_get(sid, i));
    DS_I16(0x8fb6) = window_open(1, 1, 0x26, 0xd, NULL, 3);
    draw_frame_cells(1, 1, 0x26, 0xd, g_upper_screen, 1);
    draw_tile_box(1, 0xb, 0x25, 9, 3);
    draw_tile_box(1, 0x14, 0x12, 0x59, 1);
    text_draw_to_free(0xc, 4,
                      fmt(DS_ADDR(char, 0x13c5), series_title_get(sid), ep[1], ep[2], ep[3], ep[4], ep[5], ep[6],
                          ep[7], ep[8], ep[9], ep[10]),
                      g_upper_screen);
    draw_tile_box(0x13, 0x14, 0x13, 0x38, 2);
    draw_tile_box(0x13, 0x4c, 0x13, 0x10, 1);
    draw_tile_box(0x13, 0x5c, 0x13, 0x11, 2);
    text_box(0x9c, 0xc, series_descr_get(sid), 0x12, 1, g_upper_screen, 7);
    text_draw_to_free(0x9c, 0x44,
                      fmt(g_text(0x169), (int16_t)(int8_t)ser[0x23], g_text((int8_t)ser[0x21] + 0x124),
                          (int16_t)((int8_t)ser[0x35] << 2), (int16_t)((int8_t)ser[0x36] << 2)),
                      g_upper_screen);
    screen_update();
    for (int16_t i = 1; i < 0xb; i++)
        if (ep[i] != NULL)
            far_free(ep[i]);
}

/* 340d:1aa6 – Gegenstand an den Mauszeiger hängen: Bitmap DS:0390 (Rahmen, Symbol oder Text je
 * nach style) und eigene Hintergrundpuffer; die alten Zeigerpuffer werden in DS:7af8.. gemerkt */
void cursor_attach_item(int16_t w, int16_t h, int16_t grab_x, int16_t grab_y, char *text, int16_t style)
{
    if (g_drag_bitmap != NULL)
        return;
    mouse_hide();
    Bitmap *b = bitmap_alloc(w, h);
    DS_SET_PTR(0x0390, b);
    if (style == 0 || style == 10)
        frame_draw_to(0, 0, w, h, 0, b);
    if (style >= 0x15 && style <= 0x18) {
        frame_draw_to(0, 0, w, h, (int16_t)(style - 0x11), b);
    } else if (style >= 1 && style <= 9) {
        bmp_fill(b, 0, 0, 9, 8, 0);
        bmp_draw_masked(BANK_BMP(RBANK(0x100), sprite_ofs(style)), b, 0, 0);
    } else if (style == 0xb) {
        bmp_fill(b, 0, 0, (int16_t)(w >> 3), h, 0);
    }
    if (style >= 0 && style <= 9) {
        DS_I16(0x8b86) = 1;
        if (text != NULL)
            text_box_free(style == 0 ? 1 : 2, -6, text, (int16_t)(w / 8 - 1), 1, b, (int16_t)((h - 2) / 7));
        DS_I16(0x8b86) = 0;
    } else if (style == 10 || (style > 0x14 && style < 0x19)) {
        if (text != NULL)
            text_box_free(style == 10 ? 1 : 2, -7, text, (int16_t)(w / 8 - 1), 1, b, (int16_t)((h - 2) / 8));
    }
    g_drag_w = w;
    g_drag_h = h;
    if (grab_x != -1) {
        g_drag_hot_x = min_i16((int16_t)(g_cursor_x - grab_x), (int16_t)(w - 0x10));
        g_drag_hot_y = (int16_t)(g_cursor_y - grab_y);
    }
    /* PORT: Das Original legte hier objektgroße Puffer für die Hintergrundsicherung an; das
     * gezogene Objekt zeichnet im Port die Darstellungsschicht (Zeiger-Overlay, rtlib.c). */
    g_cursor_shape = 2;
    DS_I16(0x79d2) = (int16_t)((0x78 - g_drag_h) + g_drag_hot_y);
}

/* 340d:1d02 – gezogenen Gegenstand loslassen */
void cursor_drop_item(void)
{
    if (g_drag_bitmap == NULL)
        return;
    /* PORT: ohne Zurückschreiben des Hintergrunds und Puffertausch, siehe cursor_attach_item */
    far_free_ptr(DS_ADDR(ptr32, 0x0390));
    g_cursor_shape = 1;
    cursor_reset();
    mouse_show();
}

/* 340d:1d9a – Liste der 10 Werbeverträge eines Spielers */
void ad_contract_list_dialog(int16_t xcol, int16_t yrow, int16_t mode, int16_t player)
{
    int16_t x = xcol * 8, y = yrow * 8;
    clip_reset();
    if (mode == 0) {
        DS_I16(0x8fb8) = window_open(xcol, yrow, xcol + 0x11, yrow + 0xd, NULL, 3);
        frame_draw(x, y, (int16_t)((xcol + 0x11) * 8), (int16_t)((yrow + 0xe) * 8), 0);
        text_draw((int16_t)(x + 4), (int16_t)(y - 7), g_text(0x5f));
        for (int16_t i = 0; i < 10; i++) {
            int16_t dy = (int16_t)(i * 10), ad = DS_I16(0x1b50 + player * 0x46 + i * 7);
            frame_draw((int16_t)(x + 4), (int16_t)(y + dy + 10), (int16_t)((xcol + 0x11) * 8 - 4),
                       (int16_t)(y + dy + 0x14), ad == 0 ? 1 : 0);
            if (ad != 0)
                text_draw_free((int16_t)(x + 5), (int16_t)(y + dy + 3), fmt(DS_ADDR(char, 0x13ea), AD_NAME(ad)));
        }
    } else if (mode == 1) {
        window_close(DS_I16(0x8fb8));
    }
}

#define FLOOR_B(f, o)    DS_I8(0x7c62 + (f) * 0x7a + (o))

/* 340d:1f43 – Hinweistafel im Foyer: 13 Etagen x 4 Türen (Plan DS:8a40), Farbe nach Besitzer */
void building_directory_panel(void)
{
    bmp_fill(g_upper_screen, 0, 0, 0x28, 0x78, 0);
    room_frame_draw();
    clip_reset();
    g_font_select = 1;
    for (int16_t f = 1; f < 0xe; f++) {
        draw_sprite(BANK_BMP(RBANK(0x100), 0x2a), 0x9a, (int16_t)(f * 8 - 3));
        for (int16_t k = 0; k < 4; k++) {
            int16_t dx = k > 1 ? 9 : 0, spr;
            int16_t floor = DS_I8(0x8a40 + (f - 1) * 8 + k * 2), door = DS_I8(0x8a41 + (f - 1) * 8 + k * 2);
            if (door >= 100)
                spr = 7;
            else if (FLOOR_B(floor, door + 0x1e) == 0)
                spr = 1;
            else if (FLOOR_B(floor, door + 0x46) == 0)
                spr = 5;
            else
                spr = (int16_t)((uint8_t)FLOOR_B(floor, door + 0x46) + 1);
            draw_sprite(BANK_BMP(RBANK(0x100), sprite_ofs(spr)), (int16_t)(k * 0x4b + dx + 6), (int16_t)(0x6e - f * 8));
            if (door < 100 && FLOOR_B(floor, door + 0x28) != 0 && FLOOR_B(floor, door + 0x1e) != 0)
                text_draw_free((int16_t)(k * 0x4b + dx + 8), (int16_t)(0x68 - f * 8), door_name_text(floor, door));
        }
    }
    draw_sprite(BANK_BMP(RBANK(0x100), 0x22), 0xf0, 0x66);
    text_draw(0xf2, 0x60, g_text(0x60));
    draw_sprite(BANK_BMP(RBANK(0x100), 0x22), 6, 0x66);
    text_draw(8, 0x60, g_text(0x88));
    g_font_select = 0;
}

/* 340d:21a4 – drei Nachrichten-Abos (Spielerbytes +0x11..0x13) und Anzeige DS:7286 */
void player_flags_draw(int16_t player)
{
    clip_reset();
    for (int16_t i = 1; i < 4; i++)
        draw_sprite(BANK_BMP(RBANK(0x104), sprite_ofs(DS_I8(0x8bcc + player * 0x17 + i) == 0 ? 1 : 2)),
                    DS_I8(0x1335 + i), 0x5b);
    draw_sprite(BANK_BMP(RBANK(0x104), sprite_ofs(DS_I16(0x7286 + player * 2) < 1 ? 4 : 3)), 0x50, 0x5c);
}

/* Datumszeile einer Nachricht: Uhrzeit, heute/gestern, Preis */
static void news_date_line(int16_t x, int16_t y, uint8_t *item)
{
    const char *day = *(int16_t *)(item + 0x1a) == g_day ? g_text(0x103) : g_text(0x10c);
    text_box_free(x, y,
                  fmt(DS_ADDR(char, 0x13f6), *(int16_t *)(item + 0x18), day, genre_value_get((int8_t)item[0x1d])),
                  0x11, 0, g_upper_screen, 1);
}

/* 340d:2246 – die drei ausgewählten Nachrichten des Spielers (DS:3d4b) */
void news_slots_panel(int16_t player, int16_t mode)
{
    clip_reset();
    if (mode == 1) {
        window_close(DS_I16(0x8fba));
        return;
    }
    if (mode != 0 && mode != 2)
        return;
    if (mode == 0)
        DS_I16(0x8fba) = window_open(0x14, 0, 0x26, 0xe, NULL, 3);
    frame_draw(0xa0, 4, 0x138, 0x70, 0);
    for (int16_t u = 1; u < 4; u++) {
        uint8_t *item = DS_ADDR(uint8_t, 0x3d4b + player * 0x78 + u * 0x1e);
        if (*(int16_t *)item == 0) {
            frame_draw(0xa4, (int16_t)(u * 0x22 - 0x1b), 0x134, (int16_t)(u * 0x22 + 7), 1);
            continue;
        }
        frame_draw(0xa4, (int16_t)(u * 0x22 - 0x1b), 0x134, (int16_t)(u * 0x22 + 7), DS_I8(0x1338 + (int8_t)item[0x1c]));
        char *s = news_format_text(item, *(int16_t *)item);
        text_box_free(0xa6, (int16_t)(u * 0x22 - 0x22), fmt(DS_ADDR(char, 0x13f0), s), 0x11, 1, g_upper_screen, 3);
        far_free(s);
        news_date_line(0xa6, (int16_t)(u * 0x22 - 10), item);
    }
}

/* 340d:24a1 – Nachrichtenarchiv: Reiter je Kategorie (DS:7a52), zwei Nachrichten ab Position DS:7a50 */
void player_tabs_panel(int16_t mode)
{
    int16_t sel = -1;
    clip_reset();
    if (mode != 0 && mode != 2) {
        window_close(DS_I16(0x8fbc));
        return;
    }
    if (mode == 0)
        DS_I16(0x8fbc) = window_open(1, 0, 0x13, 0xe, NULL, 3);
    frame_draw(8, 4, 0xa0, 0x70, 0);
    if (news_count_by_category(1) == 0 && news_count_by_category(2) == 0 && news_count_by_category(3) == 0)
        return;
    while (news_count_by_category(DS_I16(0x7a52)) == 0)
        if (++DS_I16(0x7a52) > 3)
            DS_I16(0x7a52) = 1;
    for (int16_t i = 1; i < 4; i++)
        if (news_count_by_category(i) != 0)
            draw_sprite(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(i + 4))), (int16_t)(i * 0x18 - 0xc), 8);
    draw_sprite(BANK_BMP(RBANK(0x104), 0x5a), (int16_t)(DS_I16(0x7a52) * 0x18 - 0xc), 8);
    if (DS_I16(0x7a50) != 0)
        draw_sprite(BANK_BMP(RBANK(0x104), 0x3a), 100, 8);
    if (news_count_by_category(DS_I16(0x7a52)) > DS_I16(0x7a50) + 2)
        draw_sprite(BANK_BMP(RBANK(0x104), 0x42), 0x78, 8);
    for (int16_t i = 0; i < DS_I16(0x7a50); i++)
        sel = news_find_next(DS_I16(0x7a52), sel);
    for (int16_t i = 1; i < 3 && (sel = news_find_next(DS_I16(0x7a52), sel)) != -1; i++) {
        uint8_t *item = DS_ADDR(uint8_t, 0x455f + g_local_player * 600 + sel * 0x1e);
        frame_draw(0xc, (int16_t)(i * 0x22 + 7), 0x9c, (int16_t)(i * 0x22 + 0x29), DS_I8(0x133b + (int8_t)item[0x1c]));
        char *s = news_format_text(item, *(int16_t *)item);
        text_box_free(0xe, (int16_t)(i * 0x22), fmt(DS_ADDR(char, 0x1415), s), 0x11, 1, g_upper_screen, 3);
        far_free(s);
        news_date_line(0xe, (int16_t)(i * 0x22 + 0x18), item);
    }
}

/* 340d:27e9 – Nachrichtentext: Vorlage aus DS:729a (20 x {id, Text}); "#n" wird durch das Wort
 * aus Tabelle DS:5573 ersetzt, das Feldpaar n der Nachricht (+2+4(n-1) Liste, +4+4(n-1) Eintrag) angibt */
char *news_format_text(void *news_item, int16_t text_id)
{
    uint8_t *item = news_item;
    int8_t idx[5] = {0, 0, 0, 0, 0};
    int16_t n = 0, k;
    for (k = 0; k < 0x14 && DS_I16(0x729a + k * 6) != text_id; k++)
        ;
    if (k >= 0x14) /* PORT: das Original liest hier hinter die Tabelle */
        return fmt(DS_ADDR(char, 0x1370));
    const char *tpl = MEM_PTR(char, DS_ADDR(uint8_t, 0x729c + k * 6));
    if (tpl == NULL)
        return fmt(DS_ADDR(char, 0x1370));
    uint16_t len = (uint16_t)strlen(tpl);
    char *t = far_calloc((uint16_t)(len + 1), 1);
    far_memcpy(t, tpl, (uint16_t)(len + 1));
    for (int16_t i = 0; t[i] != 0; i++) {
        if (t[i] == '#') {
            i++;
            if (n < 5)
                idx[n++] = (int8_t)(t[i] - '1');
            t[i] = 's';
        }
    }
    char *w[5];
    for (k = 0; k < 5; k++) {
        int16_t list = *(int16_t *)(item + idx[k] * 4 + 2), entry = *(int16_t *)(item + idx[k] * 4 + 4);
        w[k] = PTR32(char, DS_ADDR(ptr32, 0x5573)[list * 0xe + entry]);
    }
    char *s = fmt(t, w[0], w[1], w[2], w[3], w[4]);
    far_free(t);
    return s;
}

/* 340d:2a3c – Senderkarte rechnen und zeichnen: Bevölkerung (DS:7c5e) und Reichweite (DS:8b8a)
 * je Bildpunkt, dann Reichweite und Zuschauerzahl in Satz 0 der Senderliste ablegen */
void station_map_view(int16_t player, int16_t mode)
{
    int16_t p = g_station_player, count = 0;
    if (mode == 1) {
        station_map_panel(0);
        screen_update();
        return;
    }
    if (mode != 0)
        return;
    clock_suspend();
    for (int16_t i = 1; i < 0x21; i++) {
        uint8_t *st = STATION(p, i);
        if (*(int32_t *)(st + 4) != 0 && *(int16_t *)(st + 0xc) != 2)
            count++;
    }
    coverage_map_build(p);
    mouse_hide();
    station_map_panel(0x21);
    int16_t x0 = *(int32_t *)(STATION(p, 1) + 4) != 0 ? 0x54 : 0; /* Satellit: nur rechte Hälfte */
    int16_t x1 = *(int32_t *)(STATION(p, 2) + 4) != 0 ? 0x54 : 0xa8; /* Kabel: nur linke Hälfte */
    for (int16_t x = x0; x < x1; x++) {
        for (int16_t y = 0; y < 0x6d; y++) {
            int16_t c = map_cell_value(x, y, DS_PTR(uint16_t, 0x7c5e)) != 0 ? 0xf : 0;
            int16_t v = map_cell_value(x, y, DS_PTR(uint16_t, 0x8b8a));
            if (v == 1)
                c = 0x10;
            else if (v == 10)
                c = 0x11;
            else if (v == 100)
                c = 0x12;
            if (c != 0)
                draw_sprite(BANK_BMP(RBANK(0x104), sprite_ofs(c)), (int16_t)(x + 0x30), (int16_t)(y + 4));
        }
        if ((x & 1) == 0)
            screen_update();
    }
    screen_update();
    draw_frame_cells(0x1b, 1, 0x26, 6, g_upper_screen, 1);
    draw_tile_box(0x1b, 0xb, 0xb, 9, 3);
    draw_tile_box(0x1b, 0x14, 0xb, 8, 2);
    draw_tile_box(0x1b, 0x1c, 0xb, 8, 1);
    draw_tile_box(0x1b, 0x24, 0xb, 8, 2);
    draw_tile_box(0x1b, 0x2c, 0xb, 9, 1);
    uint8_t *sum = STATION(p, 0);
    *(int32_t *)(sum + 4) = map_population_sum(0, 0, 0xa7, 0x6c, DS_PTR(uint16_t, 0x8b8a));
    *(int32_t *)sum = *(int32_t *)(STATION(player, 0) + 4) * 0x268 / 10;
    char *money = fmt_money(*(int32_t *)(sum + 4) * 0x9a0, 3);
    text_draw_to_free(0xdc, 4, fmt(g_text(0x61), DS_PTR(char, 0x8bc0 + p * 0x17), count, money), g_upper_screen);
    far_free(money);
    screen_update();
    mouse_show();
    clock_resume();
}

/* 340d:2da7 – Auktion: 10 Lose in zwei Spalten; mode > 100 zeichnet nur Los mode-100 neu */
void auction_panel(int16_t mode)
{
    clip_reset();
    if (mode == 1) {
        window_close(DS_I16(0x8fbe));
        return;
    }
    if (mode != 0 && mode != 2 && mode <= 100)
        return;
    if (mode == 0)
        DS_I16(0x8fbe) = window_open(2, 1, 0x25, 0xd, NULL, 3);
    if (mode < 100)
        frame_draw(0x10, 8, 0x130, 0x70, 0);
    for (int16_t u = 0; u < 2; u++) {
        for (int16_t i = 1; i < 6; i++) {
            int16_t lot = u * 5 + i, x = u * 0x8e;
            if (mode >= 100 && lot != mode - 100)
                continue;
            int16_t prog = DS_I16(0x1aed + lot * 9);
            frame_draw((int16_t)(x + 0x14), (int16_t)(i * 0x13 - 7), (int16_t)(x + 0x9c), (int16_t)(i * 0x13 + 0xb), prog == 0);
            if (prog == 0)
                continue;
            draw_sprite(BANK_BMP(DS_PTR(uint8_t, 0x7b12), 0x62), (int16_t)(x + 0x15), (int16_t)(i * 0x13 - 6));
            char *s = prog < 1000 ? fmt(DS_ADDR(char, 0x143a), film_title_get(prog))
                                  : fmt(DS_ADDR(char, 0x1440), series_title_get(prog - 1000));
            text_draw_free((int16_t)(x + 0x26), (int16_t)(i * 0x13 - 0xe), s);
            /* Release-Fassung: Gebotsschritt immer aufschlagen (Beta: nicht beim eigenen Gebot),
             * ohne Bieter ein zweites Mal */
            int16_t bidder = DS_I8(0x1af3 + lot * 9);
            int32_t price = auction_bid_increment(lot) + DS_I32(0x1aef + lot * 9);
            if (bidder < 0) {
                bidder = g_local_player;
                price += auction_bid_increment(lot);
            }
            text_draw_free((int16_t)(x + 0x26), (int16_t)(i * 0x13 - 6),
                           fmt(DS_ADDR(char, 0x1446), price, DS_PTR(char, 0x8bc0 + bidder * 0x17)));
        }
    }
}

/* 340d:3047 – Drehbuchagentur: bis zu 5 Angebote (DS:194a, 9 B), gestaffelt hintereinander */
void production_offers_panel(void)
{
    clip_reset();
    if (DS_PTR(Bitmap, 0x0110) == NULL) {
        DS_SET_PTR(0x0110, bitmap_alloc(0x40, 0x21));
        mouse_hide();
        screen_grab(DS_PTR(Bitmap, 0x0110), 0x58, 0x1f);
    } else {
        mouse_hide();
        draw_bitmap_fast(DS_PTR(Bitmap, 0x0110), 0x58, 0x1f);
    }
    mouse_show();
    for (int16_t i = 5; i > 0; i--) {
        int8_t st = DS_I8(0x194c + i * 9);
        if (st >= 0 && st != 'c' && DS_I16(0x194a + i * 9) != 0)
            blit_sprite_masked(BANK_BMP(RBANK(0x104), sprite_ofs(i)), g_upper_screen, (int16_t)(i * 7 + 0x54), 0x23, 0, 0,
                               0x18, (int16_t)(0x1e - i));
    }
}

/* 340d:313f – Belegungsplan der Studios: Raster mit Tagen 14..8 / 7..1 und Studiomarken */
void studio_booking_panel(void)
{
    clip_reset();
    bmp_fill(g_upper_screen, 0, 0, 0x28, 0x78, 0);
    room_frame_draw();
    uint8_t *bank = RBANK(0x104);
    draw_sprite(BANK_BMP(bank, 0x42), 0x19, 7);
    for (int16_t r = 0; r < 7; r++) {
        int16_t y = (int16_t)(r * 0xf + 7);
        if (r != 0) {
            draw_sprite(BANK_BMP(bank, 0x4a), 8, y);
            text_draw_free(0xb, (int16_t)(y - 5), fmt(DS_ADDR(char, 0x1450), 0xe - r));
            for (int16_t x = 0x19; x < 0x98; x += 8)
                draw_sprite(BANK_BMP(bank, 0x52), x, y);
            draw_sprite(BANK_BMP(bank, 0x52), 0x90, y);
            draw_sprite(BANK_BMP(bank, 0x3a), 0x54, (int16_t)(y + 2));
        }
        draw_sprite(BANK_BMP(bank, 0x4a), 0xa8, y);
        text_draw_free(0xab, (int16_t)(y - 5), fmt(DS_ADDR(char, 0x1455), 7 - r));
        for (int16_t x = 0xb9; x < 0x138; x += 8)
            draw_sprite(BANK_BMP(bank, 0x52), x, y);
        draw_sprite(BANK_BMP(bank, 0x52), 0x130, y);
        draw_sprite(BANK_BMP(bank, 0x3a), 0xf4, (int16_t)(y + 2));
    }
    for (int16_t f = 1; f < 0xe; f++) {
        for (int16_t d = 0; FLOOR_B(f, d + 0x14) != 0 && (uint8_t)FLOOR_B(f, d + 0x14) < 4; d++) {
            int16_t c = 1, s = (uint8_t)FLOOR_B(f, d + 0x6e), xy[2];
            if (s == 0 && FLOOR_B(f, d + 0x46) != 0)
                c = (int16_t)((uint8_t)FLOOR_B(f, d + 0x46) + 4);
            else if (DS_I8(0x6154 + s * 0x10) >= 1)
                c = (int16_t)(DS_I8(0x6154 + s * 0x10) + 0xc);
            else if (DS_I8(0x6148 + s * 0x10) >= 1)
                c = (int16_t)(DS_I8(0x6148 + s * 0x10) + 1);
            else if (s != 0)
                c = 0xc;
            building_map_door_pos(f, d, &xy[0], &xy[1]);
            draw_sprite(BANK_BMP(bank, sprite_ofs(c)), xy[0], xy[1]);
        }
    }
}

/* 340d:3434 – zweite Sprechblasenform mit zwei Verzierungen; Text wird freigegeben */
void message_box2_draw(char *text, int16_t mode)
{
    speech_bubble(mode, 8, 3, 0x1f, 0xc, 1);
    if (mode != 1) {
        draw_sprite(BANK_BMP(RBANK(0x108), 10), 0x75, 0x50);
        draw_sprite(BANK_BMP(RBANK(0x108), 0x12), 0xbb, 0x50);
        text_box_wrap_free(0x44, 0x14, text, 0x17, 1);
    }
}

/* 340d:34b0 – Einschaltquote des Spielers in der Statuszeile (vor der ersten Sendestunde leer) */
void status_ratings_update(void)
{
    char *s;
    if (DS_I16(0x012c) == 0 && hour_index() > 0)
        s = fmt_money(DS_I32(0x248f + g_local_player * 10), 3);
    else
        s = fmt(DS_ADDR(char, 0x145a));
    status_text(s, 0x93);
    far_free(s);
}

#define g_studio        DS_I16(0x79be)
#define STUDIO(s)       DS_ADDR(uint8_t, 0x6145 + (s) * 0x10)
#define g_shows         DS_PTR(uint8_t, 0x713c)
#define PLAYER_OUT(p)   DS_U8(0x8bd0 + (p) * 0x17)

/* Stabsymbol: Preisstufe 1..3 -> Bild 3, 1, 2 */
static int16_t staff_tier_icon(int16_t k)
{
    return k == 1 ? 3 : k == 2 ? 1 : k == 3 ? 2 : k;
}

/* 340d:351e – Studio: Türen und Belegt-Schild */
void studio_panel_draw(void)
{
    draw_sprite(BANK_BMP(RBANK(0x110), 10), 4, 0x4f);
    draw_sprite(BANK_BMP(RBANK(0x110), 2), 4, 0x1e);
    draw_bitmap_fast(DS_PTR(Bitmap, 0x010c), 0xf8, 0x53);
    if (*(int16_t *)STUDIO(g_studio) != 0)
        draw_sprite(BANK_BMP(RBANK(0x100), 2), 0xf8, 0x53);
}

/* 340d:359d – Studio: Besetzung der laufenden Folge als Figurenreihe, leere Plätze als Platzhalter */
void studio_cast_preview(void)
{
    uint8_t *st = STUDIO(g_studio);
    int8_t size = (int8_t)st[2];
    int16_t x = size == 2 ? 0x80 : size == 3 ? 0x70 : 0x90; /* sonst im Original uninitialisiert */
    int16_t left = size, show = *(int16_t *)st, ep = (int8_t)st[4];
    if (show == 0 || ep > (int8_t)st[5])
        return;
    uint8_t *e = g_shows + show * 0x1b3 + (ep - 1) * 0x23;
    for (int16_t r = 1; r < 10; r++) {
        int16_t type = (int8_t)e[r * 3 + 9], cast = (int8_t)e[r * 3 + 0xb];
        if (type > 0x10 && type < 0x2a && cast != 0) {
            draw_bitmap_fast(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(type - 0xf))), x, 0x32);
            draw_sprite(BANK_BMP(RBANK(0x100), sprite_ofs((int16_t)(cast + 3))), x, 0x30);
            x += 0x20;
            left--;
        }
    }
    for (; left > 0; left--) {
        draw_sprite(BANK_BMP(RBANK(0x100), 0x12), x, 0x30);
        x += 0x20;
    }
}

/* 340d:372f – Studio: Sprechblase mit Folgenmarken der Produktion */
void studio_production_dialog(int16_t mode)
{
    speech_bubble(mode, 0xb, 3, 0x1f, 9, 1);
    if (mode == 1)
        return;
    draw_frame_cells(0xb, 3, 0x1f, 9, g_upper_screen, 1);
    draw_tile_box(0xb, 0x1b, 0x14, 9, 3);
    for (int16_t i = 0; i < 5; i++)
        draw_tile_box(0xb, (int16_t)(i * 8 + 0x24), 0x14, i == 4 ? 9 : 8, (int16_t)(i % 2 + 1));
    text_box_wrap_free(0x60, 0x14, text_get_copy(0x8d), 0x13, 1);
    uint8_t *st = STUDIO(g_studio);
    for (int16_t e = (int8_t)st[4]; e <= (int8_t)st[5]; e++)
        draw_sprite(BANK_BMP(RBANK(0x100), 0x3a), (int16_t)(e * 2 + 0xd8), (int16_t)((e - (int8_t)st[4]) * 4 + 0x2c));
    screen_update();
}

/* 340d:386f – Sicherheitsabfrage mit zwei Knöpfen */
void confirm_bubble_dialog(int16_t mode)
{
    speech_bubble(mode, 7, 2, 0x1a, 10, 1);
    if (mode == 1)
        return;
    draw_frame_cells(7, 2, 0x1a, 10, g_upper_screen, 1);
    text_box_wrap_free(0x3c, 0xc, text_get_copy(0xea), 0x13, 1);
    text_box_wrap_free(0x50, 0x20, text_get_copy(0xeb), 0x10, 1);
    draw_sprite(BANK_BMP(RBANK(0x100), 2), 0x3c, 0x24);
    draw_sprite(BANK_BMP(RBANK(0x100), 10), 0x3c, 0x3c);
    screen_update();
}

/* Darstellerbild: < 0x10 aus Bank DS:0100, sonst aus DS:010c */
static Bitmap *actor_image(int16_t img)
{
    return img < 0x10 ? BANK_BMP(RBANK(0x100), sprite_ofs(img)) : BANK_BMP(RBANK(0x10c), sprite_ofs((int16_t)(img - 0xf)));
}

/* 340d:393b – Requisitenregal: oben Darsteller der Gruppe DS:79c2 (bzw. drei Stabstufen), unten die
 * Rollen der gewählten Folge ab DS:79c6 */
void production_plan_panel(void)
{
    clip_reset();
    int16_t p = g_local_player;
    uint8_t *prod = DS_ADDR(uint8_t, 0x732c + p * 0x4b + DS_I16(0x79c0) * 0xf);
    int16_t show = *(int16_t *)prod, ep = *(int16_t *)(prod + 2);
    uint8_t *sh = g_shows + show * 0x1b3;
    uint8_t *e = sh + (ep - 1) * 0x23 + 8; /* +2 Faktor, Rollen ab +r*3+1 (Typ) / +r*3+3 (Besetzung) */
    int16_t grp = DS_I16(0x79c2);
    draw_frame(0x34, 3, 0x132, 0x3e, g_upper_screen, 1);
    if (grp < 1 || grp > 7) { /* Stab in drei Preisstufen */
        for (int16_t k = 1; k < 4; k++) {
            int16_t x = k * 0x47;
            bmp_draw(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(grp - 0xf))), g_upper_screen, (int16_t)(x + 0x14), 0x19);
            draw_sprite(BANK_BMP(RBANK(0x108), sprite_ofs((int16_t)(staff_tier_icon(k) + 9))), (int16_t)(x + 0x14), 0x17);
            text_draw_centered_to_free((int16_t)(x + 0x24), 0x28, fmt(DS_ADDR(char, 0x1470), g_text(k + 0xec)), g_upper_screen);
            text_draw_centered_to_free((int16_t)(x + 0x24), 0x30,
                                       fmt(DS_ADDR(char, 0x1473), DS_I32(0x6215 + (grp - 0xf) * 0x10 + k * 4) * (int8_t)e[2]),
                                       g_upper_screen);
        }
        draw_sprite(BANK_BMP(RBANK(0x108), 0x82), 0x88, 9);
        text_draw_centered_to(0xb5, 2, g_text(grp + 0xc0), g_upper_screen);
    } else { /* fünf Darsteller ab DS:79c4 */
        uint8_t *actors = DS_ADDR(uint8_t, 0x4ff3 + (grp - 1) * 0xc0);
        if (DS_I16(0x79c4) > 0)
            draw_sprite(BANK_BMP(RBANK(0x108), 0x72), 0x38, 7);
        if (*(int16_t *)(actors + (DS_I16(0x79c4) + 6) * 0xc) != 0)
            draw_sprite(BANK_BMP(RBANK(0x108), 0x7a), 0x4c, 7);
        text_draw(0x38, 0x10, g_text(grp + 0x91));
        text_draw(0x38, 0x22, g_text(0x16b));
        for (int16_t k = 1; k < 6; k++) {
            int16_t x = k * 0x24 + 0x54;
            uint8_t *a = actors + (k + DS_I16(0x79c4)) * 0xc;
            blit_rect(actor_image(*(int16_t *)(a + 0xa)), g_upper_screen, x, 9, 0, 0, 0x20, 0x14);
            draw_sprite(BANK_BMP(RBANK(0x108), 0x4a), x, 7);
            text_draw_free(x, 0x1a,
                           fmt(DS_ADDR(char, 0x145c), *(int32_t *)(a + 2), ((int8_t)a[6] << 3) / 5, ((int8_t)a[8] << 3) / 5,
                               ((int8_t)a[9] << 3) / 5));
        }
    }
    for (int16_t x = 8; x < 0x138; x += 0x10) {
        draw_sprite(BANK_BMP(RBANK(0x108), 0x62), x, 0x45);
        for (int16_t y = 0x4d; y < 0x6d; y += 8)
            draw_sprite(BANK_BMP(RBANK(0x108), 0x6a), x, y);
        draw_sprite(BANK_BMP(RBANK(0x108), 0x6a), x, 0x69);
    }
    int16_t roles = DS_I16(0x79c6);
    if (roles > 0)
        draw_sprite(BANK_BMP(RBANK(0x108), 0x72), 0xc, 0x4d);
    if (e[(roles + 4) * 3 + 1] != 0 && roles + 4 < 0xb)
        draw_sprite(BANK_BMP(RBANK(0x108), 0x7a), 0xc, 0x5e);
    for (int16_t r = roles + 1; r <= roles + 3; r++) {
        int16_t type = (int8_t)e[r * 3 + 1], cast = (int8_t)e[r * 3 + 3];
        int16_t x = (int16_t)((r - roles) * 0x58);
        draw_sprite(BANK_BMP(RBANK(0x108), 0x82), (int16_t)(x - 0x30), 0x66);
        if (type >= 1 && type <= 0xf) { /* Rolle: Darstellergruppe */
            int16_t g = (int8_t)sh[type + 0x1a3];
            if (cast == 0)
                draw_sprite(BANK_BMP(RBANK(0x108), sprite_ofs(g)), (int16_t)(x - 0x14), 0x50);
            else
                bmp_draw(actor_image(DS_I16(0x4ffd + (g - 1) * 0xc0 + cast * 0xc)), g_upper_screen, (int16_t)(x - 0x14), 0x50);
            draw_sprite(BANK_BMP(RBANK(0x108), 0x4a), (int16_t)(x - 0x14), 0x4e);
            text_draw_centered_to((int16_t)(x - 3), 0x5f, g_text(g + 0x91), g_upper_screen);
        } else if (type > 0xf && cast < 0x2a) { /* Stab */
            if (cast == 0) {
                draw_sprite(BANK_BMP(RBANK(0x108), type == 0x10 ? 0x3a : 0x42), (int16_t)(x - 0x14), 0x50);
                draw_sprite(BANK_BMP(RBANK(0x108), 0x4a), (int16_t)(x - 0x14), 0x4e);
            } else {
                bmp_draw(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(type - 0xf))), g_upper_screen, (int16_t)(x - 0x14), 0x50);
                draw_sprite(BANK_BMP(RBANK(0x108), sprite_ofs((int16_t)(staff_tier_icon(cast) + 9))), (int16_t)(x - 0x14), 0x4e);
            }
            text_draw_centered_to((int16_t)(x - 3), 0x5f, g_text(type + 0xc0), g_upper_screen);
        }
    }
}

/* 340d:40c6 – Geschenkauswahl: 10 Geschenke (DS:722c) mit Preis */
void people_select_dialog(int16_t mode)
{
    speech_bubble(mode, 10, 0, 0x26, 0xb, 1);
    if (mode == 1)
        return;
    text_draw(0x58, -2, g_text(0x104));
    for (int16_t u = 0; u < 2; u++) {
        for (int16_t i = 1; i < 6; i++) {
            int16_t g = u * 5 + i;
            draw_bitmap_fast(BANK_BMP(RBANK(0x10c), sprite_ofs(g)), (int16_t)(i * 0x28 + 0x38), (int16_t)(u * 0x24 + 0x10));
            text_draw_centered_to_free((int16_t)(i * 0x28 + 0x48), (int16_t)(u * 0x24 + 0x22),
                                       fmt(DS_ADDR(char, 0x1476), DS_I32(0x722c + g * 8)), g_upper_screen);
        }
    }
    screen_update();
}

/* 340d:41be – Chefbüro: Figuren der Spieler auf der Zufriedenheitsleiter, Wettbewerbsstand */
void player_ranking_panel(void)
{
    if (DS_PTR(Bitmap, 0x010c) == NULL) {
        DS_SET_PTR(0x010c, bitmap_alloc(0x38, 0x4e));
        DS_SET_PTR(0x0110, bitmap_alloc(0x18, 0x4e));
        screen_grab(DS_PTR(Bitmap, 0x010c), 0xa8, 0xc);
        screen_grab(DS_PTR(Bitmap, 0x0110), 0x110, 0xc);
    } else {
        draw_bitmap_fast(DS_PTR(Bitmap, 0x010c), 0xa8, 0xc);
        draw_bitmap_fast(DS_PTR(Bitmap, 0x0110), 0x110, 0xc);
    }
    for (int16_t i = 1; i < 4; i++) {
        if (PLAYER_OUT(i) != 0)
            continue;
        int16_t meter = DS_I16(0x8bc8 + i * 0x17);
        int16_t s = meter == 0 ? 4 : i, x = i == 1 ? 0xaa : i == 2 ? 0xc2 : 0x110;
        draw_sprite(BANK_BMP(RBANK(0x104), sprite_ofs(s)), x, (int16_t)(0x3e - (meter >> 1)));
        if (PLAYER_OUT(i) == 1) /* im Original nie erreicht */
            draw_sprite(BANK_BMP(RBANK(0x104), 0x22), x, (int16_t)(0x3e - (meter >> 1)));
    }
    if (contest_tick() < 2) {
        draw_sprite(BANK_BMP(RBANK(0x104), 0x2a), 0x48, 8);
        if (contest_tick() == 1) {
            int16_t y = 0x18;
            for (int16_t i = 1; i < 4; i++) {
                if (PLAYER_OUT(i) != 0)
                    continue;
                int16_t x = 0x4a;
                for (int16_t k = 0; k < contest_share(i) >> 2; k++)
                    draw_sprite(BANK_BMP(RBANK(0x104), sprite_ofs((int16_t)(i + 7))), ++x, y);
                y += 6;
            }
        }
        draw_sprite(BANK_BMP(RBANK(0x104), 0x32), 0x48, 8);
    }
}

/* Balkenhöhe. Release-Fassung: (Wert / 1000) * 15 / (Maßstab * 1000); die Beta rechnete
 * Wert * 15 / (Maßstab * 1 000 000) und lief bei großen Werten über */
static int16_t stat_bar_height(int32_t v)
{
    int32_t div = (int32_t)DS_I16(0x79dc) * 1000;
    if (div == 0)
        return 0; /* PORT: Original teilt durch 0 */
    return (int16_t)(v / 1000 * 15 / div);
}

/* 340d:440b – Statistik: Zuschauer (Balkenpaare) gestern/heute/Nachrichten (DS:79d8) eines Senders
 * (DS:79da) bzw. Image aller Spieler (DS:7a4e) */
void statistics_panel(void)
{
    int16_t img_view = DS_I16(0x7a4e), page = DS_I16(0x79d8), ch = DS_I16(0x79da), scale = DS_I16(0x79dc);
    int32_t *bars_e = NULL, *bars_a = NULL;
    int8_t *flags = NULL;
    int16_t n = 7, title = 0x102;
    DS_SET_PTR(0x8f2e, RBANK(0x10c));
    DS_SET_PTR(0x8f2a, g_upper_screen);
    DS_I16(0x0386) = 1;
    DS_I16(0x0388) = 4;
    DS_I16(0x038c) = 0x13b;
    DS_I16(0x038a) = 4;
    DS_I16(0x038e) = 0x74;
    for (int16_t y = 0; y < 0x71; y += 8) {
        for (int16_t c = 1; c < 0x27; c++)
            bank_draw(10, (int16_t)(c << 3), y);
        bank_draw(2, 4, y);
        bank_draw(2, 0x138, y);
    }
    bank_draw(0x2a, 0xc, 0x46);
    text_draw(0xf, 0x40, g_text(img_view == 0 ? 0x150 : 0x151));
    if (img_view == 0) {
        for (int16_t i = 1; i < 4; i++) {
            if (PLAYER_OUT(i) == 0) {
                bank_draw(sprite_ofs((int16_t)(i + 2)), 0xc, (int16_t)(i * 0xe - 6));
                text_draw(0xf, (int16_t)(i * 0xe - 0xc), DS_PTR(char, 0x8bc0 + i * 0x17));
            }
        }
        if (ch != 0)
            bank_draw(0x32, 0xc, (int16_t)(ch * 0xe - 6));
    }
    if (page == 0) {
        bars_e = DS_ADDR(int32_t, 0x789e + ch * 0x20);
        bars_a = DS_ADDR(int32_t, 0x791e + ch * 0x20);
        flags = DS_ADDR(int8_t, 0x5553 + ch * 8);
    } else if (page == 1) {
        bars_e = DS_ADDR(int32_t, 0x74ca + ch * 0x20);
        bars_a = DS_ADDR(int32_t, 0x7690 + ch * 0x20);
        flags = DS_ADDR(int8_t, 0x5533 + ch * 8);
        n = hour_index();
        title = 0x103;
    } else if (page == 2) {
        bars_e = DS_ADDR(int32_t, 0x6d89 + ch * 0x20);
        title = 0x120;
    }
    if (img_view == 0) {
        bank_draw(0x2a, 0xc, 0x38);
        text_draw(0xf, 0x32, g_text(title));
        for (int16_t i = 1; i <= n; i++) {
            int16_t h;
            if (page < 2) {
                h = stat_bar_height(bars_a[i]);
                bank_draw(0x132, (int16_t)(i * 0x1e + 0x41), (int16_t)(0x5b - h));
                for (int16_t y = (int16_t)(0x5c - h); y < 0x5b; y++)
                    bank_draw(sprite_ofs((int16_t)(ch + 0x23)), (int16_t)(i * 0x1e + 0x41), y);
            }
            if (bars_e != NULL && (i != n || page != 1 || DS_I16(0x71cc) > 4)) {
                h = stat_bar_height(bars_e[i]);
                bank_draw(0x152, (int16_t)(i * 0x1e + 0x47), (int16_t)(0x5b - h));
                for (int16_t y = (int16_t)(0x5c - h); y < 0x5b; y++)
                    bank_draw(sprite_ofs((int16_t)(ch + 0x27)), (int16_t)(i * 0x1e + 0x47), y);
            }
        }
    } else {
        for (int16_t i = 1; i < 4; i++) {
            if (PLAYER_OUT(i) != 0)
                continue;
            uint16_t img = DS_U16(0x8bd1 + i * 0x17);
            int16_t h = (int16_t)(-(int16_t)((uint16_t)(img * 0x4b) / 100));
            bank_draw(0xfa, (int16_t)(i * 0x3c + 0x40), (int16_t)(h + 0x59));
            for (int16_t y = (int16_t)(h + 0x5a); y < 0x5b; y++)
                bank_draw(sprite_ofs((int16_t)(i + 0x1c)), (int16_t)(i * 0x3c + 0x40), y);
            bank_draw(sprite_ofs((int16_t)(i + 2)), (int16_t)(i * 0x3c + 0x36), 99);
            text_draw_centered_to_free((int16_t)(i * 0x3c + 0x4e), 0x5d, fmt(DS_ADDR(char, 0x147b), (int16_t)img),
                                       g_upper_screen);
        }
    }
    bank_draw(0x4a, 0x4c, 0x5b);
    bank_draw(0x52, 0x5b, 8);
    if (img_view == 0) {
        int16_t v = (int16_t)(scale * 5);
        for (int16_t y = 0x10; y < 0x4d; y += 0xf) {
            bank_draw(0x3a, 0x58, y);
            text_draw_free(0x45, (int16_t)(y - 0xb), fmt(DS_ADDR(char, 0x147f), v));
            v = (int16_t)(v - scale);
        }
    } else {
        int16_t v = 100;
        for (int16_t y = 0x10; y < 0x4d; y += 0xf) {
            bank_draw(0x3a, 0x58, y);
            text_draw_free(0x45, (int16_t)(y - 0xb), fmt(DS_ADDR(char, 0x1484), v));
            v -= 0x14;
        }
    }
    int16_t hour = 0x12;
    for (int16_t x = 0x5d; x < 0x112; x += 0x1e) {
        bank_draw(sprite_ofs(img_view == 0 ? 9 : 0x23), x, 0x5b);
        if (page != 2 && img_view == 0) {
            bank_draw(0x62, x, 0x5e);
            text_draw_free((int16_t)(x + 8), 0x58, fmt(DS_ADDR(char, 0x148a), hour));
        }
        if (++hour == 0x18)
            hour = 0;
    }
    if (img_view == 0 && flags != NULL) {
        for (int16_t i = 1; i <= n; i++) {
            if (page < 2 && (i != n || page != 1 || DS_I16(0x71cc) > 4) && flags[i] != 1)
                bank_draw(sprite_ofs(flags[i] == 0 ? 0x21 : 0x22), (int16_t)(i * 0x1e + 0x46), 0x66);
        }
    }
    DS_I16(0x0386) = 0;
    screen_update();
}
