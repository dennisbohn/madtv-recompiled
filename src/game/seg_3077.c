/* Segment 3077 – Stündliche Sendelogik, Kaufregeln, Studios und Spielstände.
 *
 * Spielstände (NNN.HES, Startzustand eines neuen Spiels = 43.HES, Slots ab 210.HES) werden
 * feldweise serialisiert: Film-/Serien-/Folgendaten in gepackter Form, danach feste Bereiche
 * des Datensegments. Dasselbe Programm (savegame_serialize) liest oder schreibt, je nach
 * g_save_mode. */
#include <string.h>

#include "game.h"

#define g_save_mode  DS_I16(0x8fb0) /* 1 = laden, 2 = speichern */
#define g_save_buf   DS_PTR(uint8_t, 0x8faa)
#define g_save_pos   DS_U16(0x8fae)
#define g_save_error DS_I16(0x7a78)

/* 3077:11be */
void savegame_load(int16_t hes_no)
{
    g_save_mode = 1;
    savegame_serialize(hes_no);
}

/* Film-Datensatz (23 Byte) <-> Spielstand-Format (18 Byte) */
static void film_unpack(uint8_t *f, const uint8_t *t)
{
    f[1] = t[0];
    f[3] = t[1];
    f[5] = t[3];
    f[6] = (uint8_t)((f[6] & 0xdf) | ((t[4] & 1) << 5));
    f[6] = (uint8_t)((f[6] & 0x7f) | (((t[4] >> 1) & 1) << 7));
    f[7] = (uint8_t)((f[7] & 0xfe) | ((t[4] >> 2) & 1));
    f[7] = (uint8_t)((f[7] & 0xfd) | (((t[4] >> 3) & 1) << 1));
    memcpy(f + 8, t + 5, 2);
    memcpy(f + 0xa, t + 7, 2);
    f[0xe] = t[0xd];
    memcpy(f + 0xf, t + 0xe, 2);
    memcpy(f + 0x11, t + 0x10, 2);
}

static void film_pack(uint8_t *t, const uint8_t *f)
{
    t[0] = f[1];
    t[1] = f[3];
    t[3] = f[5];
    t[4] = (uint8_t)((t[4] & 0xf0) | ((f[6] >> 5) & 1) | (((f[6] >> 7) & 1) << 1) | ((f[7] & 1) << 2) |
                     (((f[7] >> 1) & 1) << 3));
    memcpy(t + 7, f + 0xa, 2);
    memcpy(t + 5, f + 8, 2);
    memcpy(t + 0xe, f + 0xf, 2);
    memcpy(t + 0x10, f + 0x11, 2);
    t[0xd] = f[0xe];
}

/* Serien-Datensatz (57 Byte) <-> Spielstand-Format (41 Byte: 33 Byte Kopf + Felder) */
static void series_unpack(uint8_t *s, const uint8_t *t)
{
    s[0x24] = t[0x21];
    s[0x25] = (uint8_t)((s[0x25] & 0xdf) | ((t[0x22] & 1) << 5));
    s[0x25] = (uint8_t)((s[0x25] & 0x7f) | (((t[0x22] >> 1) & 1) << 7));
    s[0x26] = (uint8_t)((s[0x26] & 0xfe) | ((t[0x22] >> 2) & 1));
    s[0x26] = (uint8_t)((s[0x26] & 0xfd) | (((t[0x22] >> 3) & 1) << 1));
    memcpy(s + 0x29, t + 0x25, 2);
    memcpy(s + 0x27, t + 0x23, 2);
    memcpy(s + 0x31, t + 0x27, 2);
    far_memcpy(s, t, 0x21);
}

static void series_pack(uint8_t *t, const uint8_t *s)
{
    t[0x21] = s[0x24];
    t[0x22] = (uint8_t)((t[0x22] & 0xf0) | ((s[0x25] >> 5) & 1) | (((s[0x25] >> 7) & 1) << 1) |
                        ((s[0x26] & 1) << 2) | (((s[0x26] >> 1) & 1) << 3));
    memcpy(t + 0x25, s + 0x29, 2);
    memcpy(t + 0x23, s + 0x27, 2);
    memcpy(t + 0x27, s + 0x31, 2);
    far_memcpy(t, s, 0x21);
}

/* feste Bereiche des Datensegments in Spielstand-Reihenfolge (Offset, Länge) */
static const uint16_t g_save_areas[][2] = {
    {0x7b22, 0xb4},  {0x8c4c, 0xc},   {0x8c34, 2},     {0x7c62, 0x6ac},
};
static const uint16_t g_save_areas2[][2] = {
    {0x79a6, 0xf0},  {0x7a96, 0x50},  {0x8a40, 0x68},  {0x1ce9, 0x738}, {0x6ef9, 0x1b0}, {0x43f7, 0x168},
    {0x3919, 0x168}, {0x248d, 0x28},  {0x609f, 0x82},  {0x7227, 1},     {0x1b50, 0x118}, {0x1a25, 200},
    {0x7688, 8},     {0x7652, 8},     {0x1c68, 0x78},  {0x74ca, 0x80},  {0x789e, 0x80},  {0x6d89, 0x80},
    {0x791e, 0x80},  {0x7690, 0x80},  {0x5533, 0x20},  {0x5553, 0x20},  {0x2463, 0x1e},  {0x71d1, 0x54},
    {0x7472, 0x54},  {0x732a, 2},     {0x455f, 0x960}, {0x3d4b, 0x1e0}, {0x36bb, 600},   {0x6127, 0x1e},
    {0x3a81, 0x2ca}, {0x7228, 2},     {0x3f2b, 4},     {0x6145, 0xd0},  {0x1aed, 99},    {0x19cd, 0x58},
    {0x1980, 0x4d},  {0x7286, 8},     {0x194a, 0x36},  {0x732c, 300},   {0x7460, 8},     {0x74c6, 4},
    {0x71c6, 4},     {0x2421, 2},     {0x765a, 2},     {0x71d0, 1},     {0x2710, 1},     {0x7282, 2},
    {0x2481, 8},     {0x71c4, 2},     {0x8b88, 2},     {0x6123, 4},     {0x15eb, 1},     {0x00f0, 2},
    {0x6e09, 0xf0},  {0x71c0, 4},     {0x24c5, 0x2d},  {0x00f2, 2},     {0x4ebf, 0x134}, {0x7458, 8},
    {0x7744, 0x18},  {0x24f2, 0x134}, {0x7292, 8},     {0x7710, 8},     {0x754a, 0x108}, {0x2423, 0x40},
    {0x7894, 8},     {0x7225, 4},     {0x262a, 0xc},   {0x70a9, 0x93},  {0x8ab4, 200},   {0x0094, 0x14},
    {0x7888, 0xc},   {0x7468, 10},    {0x722a, 0x58},  {0x789c, 2},     {0x547f, 0xb4},
};

/* 3077:11d2 – Spielstand hes_no lesen (g_save_mode 1) oder schreiben (2) */
void savegame_serialize(int16_t hes_no)
{
    uint8_t film[0x12] = {0}, series[0x29] = {0}, episodes[0x161] = {0}, ad[1] = {0};

    timer_pause();
    if (g_save_mode == 1)
        savegame_io(NULL, (int16_t)-hes_no); /* Datei öffnen */
    else
        savegame_io(NULL, 0); /* Puffer anlegen */

    uint8_t *films = DS_PTR(uint8_t, 0x2489), *series_db = DS_PTR(uint8_t, 0x2626), *eps = DS_PTR(uint8_t, 0x713c);
    if (g_save_mode == 1) {
        for (int16_t i = 1; i <= g_film_count; i++) {
            savegame_io(film, 0x12);
            film_unpack(films + i * 0x17, film);
        }
        for (int16_t i = 1; i <= g_series_count; i++) {
            savegame_io(series, 0x29);
            series_unpack(series_db + i * 0x39, series);
        }
        for (int16_t i = 1; i <= g_show_count; i++) {
            savegame_io(episodes, 0x161);
            eps[i * 0x1b3 + 4] = episodes[0];
            for (int16_t e = 0; e < 10; e++)
                memcpy(eps + i * 0x1b3 + e * 0x23 + 8, episodes + 1 + e * 0x23, 0x23);
        }
        for (int16_t i = 1; i < 0x33; i++) {
            savegame_io(ad, 1);
            DS_U8(0x3f3e + i * 0x18) = ad[0];
        }
    } else {
        for (int16_t i = 1; i <= g_film_count; i++) {
            film_pack(film, films + i * 0x17);
            savegame_io(film, 0x12);
        }
        for (int16_t i = 1; i <= g_series_count; i++) {
            series_pack(series, series_db + i * 0x39);
            savegame_io(series, 0x29);
        }
        for (int16_t i = 1; i <= g_show_count; i++) {
            episodes[0] = eps[i * 0x1b3 + 4];
            for (int16_t e = 0; e < 10; e++)
                memcpy(episodes + 1 + e * 0x23, eps + i * 0x1b3 + e * 0x23 + 8, 0x23);
            savegame_io(episodes, 0x161);
        }
        for (int16_t i = 1; i < 0x33; i++) {
            ad[0] = DS_U8(0x3f3e + i * 0x18);
            savegame_io(ad, 1);
        }
    }

    for (size_t i = 0; i < sizeof g_save_areas / sizeof *g_save_areas; i++)
        savegame_io(DS_ADDR(void, g_save_areas[i][0]), (int16_t)g_save_areas[i][1]);
    for (int16_t p = 1; p < 4; p++) /* Spielerdaten (15 Byte je Spieler) */
        savegame_io(DS_ADDR(void, 0x8bc4 + p * 0x17), 0xf);
    for (size_t i = 0; i < sizeof g_save_areas2 / sizeof *g_save_areas2; i++)
        savegame_io(DS_ADDR(void, g_save_areas2[i][0]), (int16_t)g_save_areas2[i][1]);
    for (int16_t p = 1; p < 4; p++) /* Spielernamen (40 Byte, über den Namenszeiger) */
        savegame_io(DS_PTR(char, 0x8bbc + p * 0x17), 0x28);

    if (g_save_mode == 1)
        savegame_io(NULL, 0); /* Puffer freigeben */
    else
        savegame_io(NULL, (int16_t)-hes_no); /* Datei schreiben */
    timer_resume();
}

/* 3077:1d69 */
void savegame_save(int16_t hes_no)
{
    g_save_mode = 2;
    savegame_serialize(hes_no);
}

/* 3077:1d7c – Spielstand-Puffer: len > 0 kopieren (je nach Modus hinein/heraus),
 * len < 0 Datei -len öffnen (laden) bzw. schreiben (speichern), len == 0 Puffer anlegen
 * (speichern) bzw. freigeben (laden). Fehler beim Schreiben setzt g_save_error. */
void savegame_io(void *data, int16_t len)
{
    if (g_save_mode == 2) {
        if (len == 0) {
            DS_SET_PTR(0x8faa, far_calloc(40000u, 1));
            g_save_pos = 0;
            return;
        }
        if (len < 0) {
            /* Probeschreiben (prüft den Platz auf dem Datenträger), dann die echten Daten */
            if (hes_write_dgroup((int16_t)-len, g_save_pos) == (int32_t)g_save_pos) {
                hes_write((int16_t)-len, g_save_buf, g_save_pos);
                g_save_error = 0;
            } else {
                g_save_error = 1;
            }
            far_free(g_save_buf);
            return;
        }
        far_memcpy(g_save_buf + g_save_pos, data, (uint16_t)len);
        g_save_pos = (uint16_t)(g_save_pos + len);
    } else if (g_save_mode == 1) {
        if (len < 0) {
            DS_SET_PTR(0x8faa, hes_load_raw((int16_t)-len));
            g_save_pos = 0;
            return;
        }
        if (len == 0) {
            far_free(g_save_buf);
            return;
        }
        far_memcpy(data, g_save_buf + g_save_pos, (uint16_t)len);
        g_save_pos = (uint16_t)(g_save_pos + len);
    }
}

/* ---------------------------------------------------------------- Planung, Kauf, Studios */
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define SHOW(n)           (DS_PTR(uint8_t, 0x713c) + (n) * 0x1b3)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define BUDGET(p)         DS_ADDR(uint8_t, 0x6e09 + (p) * 0x3c)
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define ARCHIVE(p, g, k)  DS_I16(0x6ef9 + (p) * 0x6c + (g) * 8 + (k) * 2)
#define ARCHIVE_COUNT(p, g) DS_I8(0x6f59 + (p) * 0x6c + (g))
#define STUDIO(i)         DS_ADDR(uint8_t, 0x6145 + (i) * 0x10)
#define LOT(i)            DS_ADDR(uint8_t, 0x1aed + (i) * 9)
#define PRODUCTION(p, i)  DS_ADDR(uint8_t, 0x732c + (p) * 0x4b + (i) * 0xf)
#define PRODUCTION_COUNT(p) DS_I16(0x7460 + (p) * 2)
#define AI_PROD(p, i)     DS_ADDR(uint8_t, 0x1a25 + (p) * 0x32 + (i) * 5)
#define AI_PROD_COUNT(p)  DS_I16(0x7652 + (p) * 2)
#define FLOOR_B(f, ofs)   DS_U8(0x7c62 + (f) * 0x7a + (ofs))
#define g_hour_phase      DS_I8(0x2710) /* 0 Nachrichten, 1 Programm, 2 Werbung abgerechnet */

/* 3077:0049 – Nachrichten-Abo umschalten */
void player_toggle_flag(int16_t player, int16_t idx)
{
    DS_U8(0x8bcc + player * 0x17 + idx) = DS_U8(0x8bcc + player * 0x17 + idx) == 0;
}

/* 3077:0087 – Gebot des Menschen auf ein Auktionslos setzen/zurücknehmen (Bieter < 0 = vorgemerkt) */
int16_t trade_item_toggle(uint16_t idx)
{
    uint8_t *l = LOT(idx);
    int8_t *bidder = (int8_t *)&l[6];
    if (W(l, 0) <= 0)
        return 0;
    if (*bidder == g_local_player) {
        show_message(0x2a);
        return 0;
    }
    if (*bidder >= 0 && money_check(g_local_player, auction_bid_increment((int16_t)idx) + L(l, 2)) != 0)
        return 0;
    if (*bidder < 0) { /* Vormerkung zurücknehmen: alter Bieter wieder vorn */
        *bidder += 100;
        if (*bidder > 0 && *bidder < 4)
            L(BUDGET(*bidder), 8) -= L(l, 2);
    } else if (*bidder != g_local_player) {
        if (*bidder > 0 && *bidder < 4) /* überbotene KI bekommt ihr Geld zurück */
            L(BUDGET(*bidder), 8) += L(l, 2);
        *bidder -= 100;
    }
    return 1;
}

/* 3077:0195 – Werbevertrag in die Stunde legen */
int16_t schedule_place_ad(int16_t contract_idx, int16_t hour, int16_t player)
{
    int16_t ad = DS_I16(0x1b50 + player * 0x46 + contract_idx * 7);
    if (ad != 0)
        DS_I16(0x4433 + player * 0x5a + hour * 2) = ad;
    return ad != 0;
}

/* 3077:01fd – Programm aus dem Archiv in den Plan legen (mit Prüfungen und Meldungen) */
int16_t schedule_place_programme(int16_t archive_idx, int16_t genre, int16_t hour, int16_t player)
{
    int16_t placed = 0;
    if (SCHEDULE(player, hour) != 9999) {
        if (hour < hour_index() || (hour_index() == hour && g_minute > 0x36)) {
            show_message(0x2d); /* Stunde schon vorbei */
        } else if (hour_index() == hour && g_minute > 5) {
            show_message(0x2e); /* läuft schon */
        } else {
            int16_t ok = 1, len;
            int16_t prog = ARCHIVE(player, genre, archive_idx);
            if (genre == 0xb) {
                len = (int8_t)SERIES(prog)[0x23];
            } else {
                uint8_t *f = FILM(prog);
                len = (int8_t)f[4];
                if (f[0] == 7) { /* Lizenzfilm nur zum festen Termin */
                    int16_t day = hour > 7 ? g_day + 1 : g_day;
                    if (W(f, 0x11) != (hour - 1) % 7 + 1 || W(f, 0xf) != day)
                        ok = 0;
                }
            }
            int16_t base = hour < 8 ? 0 : 7;
            if (base + 7 < hour + len - 1) {
                show_message(0x31); /* passt nicht mehr in den Tag */
            } else if (ok) {
                for (int16_t x = hour; x <= hour + len - 1; x++) {
                    int16_t v = SCHEDULE(player, x);
                    if (v > 0 && v < 0x7d1) {
                        schedule_clear_slot(player, x);
                        for (int16_t y = x + 1; SCHEDULE(player, y) == 9999; y++)
                            schedule_clear_slot(player, y);
                    }
                }
                SCHEDULE(player, hour) = prog;
                SCHEDULE(player, hour + 0xf) = 0;
                if (genre == 0xb)
                    SCHEDULE(player, hour) += 1000;
                for (int16_t x = hour + 1; x <= hour + len - 1; x++)
                    SCHEDULE(player, x) = 9999;
                placed = 1;
            }
        }
    }
    programmes_mark_scheduled();
    return placed;
}

/* 3077:0415 */
void schedule_clear_slot(int16_t player, int16_t hour)
{
    SCHEDULE(player, hour) = 0;
    SCHEDULE(player, hour + 0xf) = 0;
}

/* 3077:0453 – ganze Sendung (Start + Fortsetzungen) aus dem Plan nehmen */
void schedule_remove_programme(int16_t player, int16_t hour)
{
    while (SCHEDULE(player, hour) == 9999)
        hour--;
    int16_t end = hour;
    do
        end++;
    while (SCHEDULE(player, end) == 9999);
    for (int16_t x = hour; x < end; x++)
        schedule_clear_slot(player, x);
}

/* 3077:04c2 – darf der Mensch das Programm verkaufen? (Meldung bei Nein) */
int16_t programme_check_sellable(int16_t archive_idx, int16_t genre)
{
    int16_t p = g_local_player, ok = 1, i;
    int16_t prog = ARCHIVE(p, genre, archive_idx);
    if (genre == 0xb) {
        if (UW(SERIES(prog), 0x26) & 2) {
            show_message(0x33); /* eingeplant */
            return 0;
        }
        for (i = 1; i <= 0xc; i++) { /* läuft noch eine Produktion dazu? */
            uint8_t *sh = SHOW(W(STUDIO(i), 0));
            if ((UW(sh, 5) & 1) && W(sh, 6) == prog)
                ok = 0;
        }
        for (i = 0; i < AI_PROD_COUNT(p); i++) {
            uint8_t *sh = SHOW(W(AI_PROD(p, i), 0));
            if ((UW(sh, 5) & 1) && W(sh, 6) == prog)
                ok = 0;
        }
        if (!ok)
            show_message(0x35);
        return ok;
    }
    uint8_t *f = FILM(prog);
    if (L(f, 8) == 0 || prog == FILM_BETTY_CULTURE) {
        show_message(0x36);
        ok = 0;
    } else if (UW(f, 7) & 2) {
        show_message(0x37);
        ok = 0;
    } else if (f[0] == 7) {
        show_message(0x39);
        ok = 0;
    }
    if (ok == 1) {
        for (i = 1; i <= 0xc; i++) {
            uint8_t *sh = SHOW(W(STUDIO(i), 0));
            if (!(UW(sh, 5) & 1) && W(sh, 6) == prog)
                ok = 0;
        }
        for (i = 0; i < AI_PROD_COUNT(p); i++) {
            uint8_t *sh = SHOW(W(AI_PROD(p, i), 0));
            if (!(UW(sh, 5) & 1) && W(sh, 6) == prog)
                ok = 0;
        }
        if (!ok)
            show_message(0x35);
    }
    return ok;
}

/* 3077:0781 – Stundenablauf: zur vollen Stunde Nachrichten/Studios, dann die drei Abrechnungen
 * (Minute 0 Nachrichten, 5 Programm, 55 Werbung). 1 = es ist etwas passiert */
int16_t broadcast_hour_tick(void)
{
    int16_t r = 0;
    if (DS_I16(0x765a) < hour_index()) {
        news_values_decay(0x55, 0x55);
        news_update(1, 0);
        if (g_cur_room == 2) {
            set_cursor_shape(3);
            text_cache_news_free();
            text_cache_news(DS_I16(0x7a16));
            set_cursor_shape(0);
        }
        hour_start();
        DS_I16(0x765a)++;
        r = 1;
    }
    if (hour_index() <= 0)
        return r;
    if ((g_minute < 0x37 && g_hour_phase > 2) || (g_minute < 5 && g_hour_phase > 1))
        g_hour_phase = 0;
    if (g_minute >= 0 && g_hour_phase <= 0) {
        ratings_calc(1);
        status_ratings_update();
    } else if (g_minute >= 5 && g_hour_phase <= 1) {
        ratings_calc(2);
        status_ratings_update();
        programmes_on_air_update();
    } else if (g_minute >= 0x37 && g_hour_phase <= 2) {
        ratings_calc(3);
        programmes_mark_scheduled();
    } else {
        return r;
    }
    g_hour_phase++;
    return 1;
}

/* 3077:087b – reicht das (verfügbare) Geld? sonst Meldung, -1 */
int16_t money_check(int16_t player, int32_t amount)
{
    int32_t have = player == g_local_player ? money_available(player) : PLAYER_MONEY(player);
    if (have - amount < 0) {
        show_message(0x3b);
        return -1;
    }
    return 0;
}

/* 3077:08d7 – nächste Folge im Studio vorbereiten (Eintrag in die Produktionsliste) */
int16_t studio_prepare_episode(int16_t studio)
{
    uint8_t *st = STUDIO(studio);
    int16_t show = W(st, 0), owner = (int8_t)st[3];
    uint8_t *sh = SHOW(show);
    uint8_t *pr = PRODUCTION(owner, PRODUCTION_COUNT(owner));
    if (!((int8_t)st[5] < show_episode_count(show) || !(UW(sh, 5) & 1)))
        return 0;
    if (show == SHOW_VIEWER_POLL && st[5] == 1) {
        show_message(0x40);
    } else if (PRODUCTION_COUNT(owner) >= 5) {
        show_message(0x41);
    } else if (!(UW(sh, 5) & 1) && (UW(sh, 5) & 4) && (int8_t)st[5] >= 10) {
        show_message(0x14f);
    } else {
        if (!(UW(sh, 5) & 1) && (UW(sh, 5) & 4))
            UW(FILM(W(sh, 6)), 7) |= 4;
        int8_t ep = (int8_t)st[5];
        W(pr, 0) = show;
        W(pr, 2) = ep + 1;
        PRODUCTION_COUNT(owner)++;
        sh[ep * 0x23 + 10] = (UW(sh, 5) & 1) ? SERIES(W(sh, 6))[0x23] : FILM(W(sh, 6))[4];
        st[5] = (uint8_t)(ep + 1);
        return 1;
    }
    return 0;
}

/* 3077:0a98 – Folge produzieren: Qualität aus Besetzung, Aufnahme beginnt (fertig nach Dauer +0x166) */
int16_t studio_produce_episode(int16_t studio)
{
    uint8_t *st = STUDIO(studio);
    int16_t owner = (int8_t)st[3], show = W(st, 0), ep = (int8_t)st[4];
    uint8_t *sh = SHOW(show);
    int16_t genre = (UW(sh, 5) & 1) ? 0xb : (int8_t)FILM(W(sh, 6))[0];
    int8_t *e = (int8_t *)(sh + (ep - 1) * 0x23 + 8); /* +0 Grundqualität, Rolle r: +r*3+1 Typ, +2 Gewicht, +3 Besetzung */
    if (!(UW(sh, 5) & 1) && (UW(sh, 5) & 4) && archive_contains(owner, genre, W(sh, 6)) != 0) {
        show_message(0x44);
        return 0;
    }
    int16_t sum = 0, weights = 0;
    for (int16_t r = 1; r < 0xb; r++) {
        int8_t type = e[r * 3 + 1];
        int16_t v;
        if (type >= 1 && type <= 0xf) {
            v = (int16_t)(actor_suitability(show, ep, r, e[r * 3 + 3]) * e[r * 3 + 2] / 100);
        } else if (type > 0xf) {
            v = min_i16(e[r * 3 + 2], DS_I8(0x266e + (type - 0xf) * 4 + e[r * 3 + 3]));
        } else {
            continue;
        }
        sum += v;
        weights += e[r * 3 + 2];
    }
    int16_t q = weights == 0 ? 0 : (int16_t)(e[0] * sum / weights);
    q = (int16_t)(q * 0x7d / 100);
    if (UW(sh, 5) & 1) {
        uint8_t *s = SERIES(W(sh, 6));
        s[(int8_t)st[4] * 3] = (uint8_t)q;
        s[(int8_t)st[4] * 3 + 1] = (uint8_t)q;
        if (DS_I16(0x7468) == 4 && contest_tick() == 1)
            DS_I16(0x7468 + owner * 2) += q;
    } else {
        uint8_t *f = FILM(W(sh, 6));
        if (UW(f, 7) & 4)
            L(f, 8) = 0;
        else
            L(f, 8) = e[0] == 0 ? 0 : (int32_t)((uint32_t)L(f, 8) * (uint32_t)(int32_t)q) / 100;
        f[1] = (uint8_t)q;
        f[3] = (uint8_t)q;
        if (DS_I16(0x7468) == 3 && contest_tick() == 1)
            DS_I16(0x7468 + owner * 2) += q;
    }
    st[4]++;
    st[0xe] = 1;
    W(st, 0xa) = g_day;
    int16_t dur = W(sh, 0x166) == 0 ? 10 : W(sh, 0x166);
    W(st, 0xc) = hour_index() + dur + 0x10;
    while (W(st, 0xc) > 0x18) {
        W(st, 0xc) -= 0xc;
        W(st, 0xa)++;
    }
    for (int16_t i = 0; i < PRODUCTION_COUNT(owner); i++) {
        if (W(PRODUCTION(owner, i), 0) == show && W(PRODUCTION(owner, i), 2) == ep) {
            production_remove(owner, i);
            break;
        }
    }
    return 1;
}

/* 3077:0e92 – Studio mieten (läuft gerade eine Aufnahme: vormerken) */
void studio_rent_request(int16_t player, int16_t studio)
{
    if (STUDIO(studio)[0xe] == 0)
        studio_rent(studio, player);
    else
        STUDIO(studio)[0xf] = (uint8_t)player;
}

/* 3077:0ed1 – Studio zurückgeben (läuft gerade eine Aufnahme: vormerken) */
void studio_release_request(int16_t studio)
{
    if (STUDIO(studio)[0xe] == 0)
        studio_release(studio);
    else
        STUDIO(studio)[0xf] = 0xff;
}

/* 3077:0f08 – Studio mieten: Miete steigt um die Hälfte, drei Tagesmieten werden fällig */
void studio_rent(int16_t studio, int16_t player)
{
    uint8_t *st = STUDIO(studio);
    L(st, 6) += L(st, 6) / 2;
    int32_t cost = L(st, 6) * 3;
    PLAYER_MONEY(player) -= cost;
    if (player == g_local_player) {
        money_add(8, cost);
    } else {
        L(BUDGET(player), 0x10) -= cost;
        int16_t i = 0;
        while (DS_I8(0xf2 + i) != player)
            i++;
        L(BUDGET(player), 0x2c + DS_I8(0x7225 + i) * 4) -= cost;
        ai_update_avg_income((int8_t)player);
    }
    st[3] = (uint8_t)player;
    W(st, 0) = 0;
    st[4] = 0;
    st[5] = 0;
    for (int16_t f = 1; f < 0xe; f++) {
        for (int16_t d = 0; d < 5; d++) {
            if (FLOOR_B(f, d + 0x6e) == (uint16_t)studio) {
                FLOOR_B(f, d + 0x46) = (uint8_t)player;
                FLOOR_B(f, d + 0x1e) = 5;
                FLOOR_B(f, d + 0x28) = 0x8b;
                FLOOR_B(f, d + 0x32) = 0x8c;
                FLOOR_B(f, d + 0x50) = 0xc;
                FLOOR_B(f, d + 0x5a) = 2;
            }
        }
    }
}

/* 3077:10b8 – Studio freigeben; laufende Show geht zurück in die Liste gekaufter Shows */
void studio_release(int16_t studio)
{
    uint8_t *st = STUDIO(studio);
    if (W(st, 0) != 0) {
        int16_t owner = (int8_t)st[3];
        int16_t n = AI_PROD_COUNT(owner);
        if (n < 5) {
            uint8_t *e = AI_PROD(owner, n);
            W(e, 0) = W(st, 0);
            W(e, 3) = (int8_t)st[4];
            e[2] = (uint8_t)show_role_count(W(st, 0));
        }
    }
    st[3] = 0xff;
    W(st, 0) = 0;
    st[4] = 0;
    st[5] = 0;
    for (int16_t f = 1; f < 0xe; f++) {
        for (int16_t d = 0; d < 4; d++) {
            if (FLOOR_B(f, d + 0x6e) == (uint16_t)studio) {
                FLOOR_B(f, d + 0x46) = 0;
                FLOOR_B(f, d + 0x1e) = 0x36;
                FLOOR_B(f, d + 0x28) = 0x89;
                FLOOR_B(f, d + 0x32) = 0x8a;
            }
        }
    }
}

/* 3077:1e75 – -1, wenn das Programm im Archiv des Genres liegt */
int16_t archive_contains(int16_t player, int16_t genre, int16_t prog_id)
{
    for (int16_t i = 0; i < ARCHIVE_COUNT(player, genre); i++)
        if (ARCHIVE(player, genre, i) == prog_id)
            return -1;
    return 0;
}
