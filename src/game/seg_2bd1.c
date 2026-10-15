/* Segment 2bd1 – Spielregeln: Stundenwechsel, Studios, Auktion, Kosten, Sendegebiete, Ereignisse, Preise.
 *
 * Datenbanken (siehe seg_2a62.c): Filme DS:2489 (23 B), Serien DS:2626 (57 B), Shows DS:713c (0x1b3 B).
 * Studio-Plätze DS:6145, 12 x 16 Byte: +0 Show, +3 Besitzer, +4 Folge, +6 Miete (long),
 *   +0xa Fertigstellungstag, +0xc Stunde, +0xe, +0xf Folgemieter (-1 = freigeben).
 * Auktionslose DS:1aed, 10 x 9 Byte: +0 Programm (Serie +1000), +2 Preis (long), +6 Bieter, +7, +8. */
#include "game.h"

#define g_films           DS_PTR(uint8_t, 0x2489)
#define g_series          DS_PTR(uint8_t, 0x2626)
#define g_shows           DS_PTR(uint8_t, 0x713c)
#define FILM(n)           (g_films + (n) * 0x17)
#define SERIES(n)         (g_series + (n) * 0x39)
#define SHOW(n)           (g_shows + (n) * 0x1b3)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))

#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define PLAYER_METER(p)   DS_I16(0x8bc8 + (p) * 0x17) /* Anzeige +0xc, nie über dem Image */
#define PLAYER_SUBSCR(p, c) DS_U8(0x8bcc + (p) * 0x17 + (c)) /* Nachrichten-Abos */
#define PLAYER_OUT(p)     DS_U8(0x8bd0 + (p) * 0x17)
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)  /* Programmplan, 9999 = Fortsetzung */
#define AD_SCHEDULE(p, h) DS_I16(0x4433 + (p) * 0x5a + (h) * 2)  /* Werbeplan */
#define STUDIO(i)         DS_ADDR(uint8_t, 0x6145 + (i) * 0x10)
#define LOT(i)            DS_ADDR(uint8_t, 0x1aed + (i) * 9)
#define AD_CONTRACT(p, i) DS_ADDR(uint8_t, 0x1b50 + (p) * 0x46 + (i) * 7)
#define AD_CONTRACT_COUNT(p) DS_I16(0x7688 + (p) * 2)
#define AD(n)             DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)   /* Werbekunde */
#define AI_BUDGET(p, ofs) DS_I32(0x6e09 + (p) * 0x3c + (ofs))
#define STATIONS(p)       DS_ADDR(uint8_t, 0x1ce9 + (p) * 0x1ce)

/* 2bd1:0005 – Anzahl freier, produzierbarer Shows */
int16_t shows_free_count(void)
{
    int16_t n = 0;
    for (int16_t i = 1; i < 0x33; i++)
        if (SHOW(i)[4] == 0 && (UW(SHOW(i), 5) & 2))
            n++;
    return n;
}

/* 2bd1:004b – Bit "eingeplant" für alle Programme aus den restlichen Tagesplänen neu setzen */
void programmes_mark_scheduled(void)
{
    for (int16_t i = 1; i < 0xc9; i++)
        UW(FILM(i), 7) &= ~2;
    for (int16_t i = 1; i < 0x33; i++)
        UW(SERIES(i), 0x26) &= ~2;
    for (int16_t p = 1; p < 4; p++) {
        int16_t h = hour_index_after_min55();
        while (SCHEDULE(p, h) == 9999) /* Anfang einer laufenden Sendung suchen */
            h--;
        for (; h < 0xf; h++) {
            int16_t prog = SCHEDULE(p, h);
            if (prog > 0 && prog < 1000)
                UW(FILM(prog), 7) |= 2;
            else if (prog > 1000 && prog < 2000)
                UW(SERIES(prog - 1000), 0x26) |= 2;
        }
    }
}

/* 2bd1:012f – Summe der Nachrichtenqualität aller aktiven Spieler (max 95) */
int16_t newscast_quality_sum(void)
{
    int16_t sum = 0;
    for (int16_t p = 1; p < 4; p++)
        sum += PLAYER_OUT(p) == 0 ? DS_I8(0x3f2b + p) : 0;
    return sum > 0x5f ? 0x5f : sum;
}

/* 2bd1:016c – Summe von DS:2481[p] aller aktiven Spieler (max 95) */
int16_t channel_value_sum(void)
{
    int16_t sum = 0;
    for (int16_t p = 1; p < 4; p++)
        sum += PLAYER_OUT(p) == 0 ? DS_I16(0x2481 + p * 2) : 0;
    return sum > 0x5f ? 0x5f : sum;
}

/* 2bd1:01ac – Anzahl Folgen einer Show (Serienproduktion) bzw. 1 */
int16_t show_episode_count(int16_t show)
{
    if (UW(SHOW(show), 5) & 1)
        return (int8_t)SERIES(W(SHOW(show), 6))[0x22];
    return 1;
}

/* 2bd1:01f6 – Aktualität erholt sich: Filme in Fünfteln, Serienfolgen in Zwanzigsteln */
void programmes_topicality_recover(void)
{
    for (int16_t i = 1; i < 0xc9; i++) {
        uint8_t *f = FILM(i);
        int8_t max = (int8_t)f[1], cur = (int8_t)f[3];
        if (cur < max && max > 0) {
            int16_t k = 1;
            while (cur >= max * k / 5)
                k++;
            f[3] = (uint8_t)(max * k / 5);
        }
    }
    for (int16_t i = 1; i < 0x33; i++) {
        uint8_t *s = SERIES(i);
        for (int16_t e = 1; e <= (int8_t)s[0x22]; e++) {
            int8_t *ep = (int8_t *)(s + e * 3);
            if (ep[1] < ep[0]) {
                int16_t k = 1;
                while (ep[1] >= ep[0] * k / 0x14)
                    k++;
                ep[1] = (int8_t)(ep[0] * k / 0x14);
            }
        }
    }
}

/* 2bd1:0301 – Stundenbeginn: fertige Produktionen in den Studios abliefern */
void hour_start(void)
{
    for (int16_t i = 1; i < 0xd; i++) {
        uint8_t *st = STUDIO(i);
        if (W(st, 0xa) != g_day || W(st, 0xc) > hour_index() + 0x11)
            continue;
        W(st, 0xa) = 0;
        int16_t episode = (int8_t)st[4];
        player_message_push(0x10e, (int8_t)st[3], i, W(st, 0), episode - 1, W(st, 0xc), 0);
        int16_t show = W(st, 0);
        uint8_t *sh = SHOW(show);
        int16_t is_series = UW(sh, 5) & 1;
        int16_t prog = W(sh, 6);
        if (is_series)
            prog += 1000;
        if ((is_series && st[4] == 2) || !is_series)
            programme_buy(prog, (int8_t)st[3], 0);
        if (is_series) {
            if (prog < 0x3e9) {
                UW(FILM(prog), 6) |= 0x20;
                if (UW(sh, 5) & 4)
                    UW(FILM(prog), 7) |= 4;
            } else {
                uint8_t *s = SERIES(prog % 1000);
                UW(s, 0x25) |= 0x20;
                UW(s, ((int8_t)st[4] - 1) * 3 + 2) |= 1;
                if (W(s, 0x31) == 1 && (int8_t)st[4] > 2) {
                    int16_t e = 1;
                    while (e < (int8_t)st[4] && s[e * 3] != s[e * 3 + 1])
                        e++;
                    if (e < (int8_t)st[4])
                        W(s, 0x31) = e;
                }
            }
        }
        if ((is_series && (int8_t)SERIES(W(sh, 6))[0x22] < (int8_t)st[4]) || show == 0x18) {
            sh[4] = 99; /* Produktion abgeschlossen */
            studio_slot_release(i);
        }
        if (is_series || !(UW(sh, 5) & 4)) {
            st[0xe] = 0;
            if ((int8_t)st[0xf] == -1)
                studio_release(i);
            else if ((int8_t)st[0xf] > 0)
                studio_rent(i, (int8_t)st[0xf]);
        }
        if (g_cur_room == 0x1f && (int8_t)STUDIO(i)[3] == DS_I16(0x7a16)) {
            set_cursor_shape(3);
            text_cache_titles_free(0);
            text_cache_descr_free(0);
            text_cache_titles(0x1f, DS_I16(0x7a16));
            text_cache_descr(0x1f, DS_I16(0x7a16));
            set_cursor_shape(0);
            if (g_cur_dialog == 2) {
                archive_list_dialog(0x13, 8, 1, 0);
                archive_list_dialog(0x13, 8, 0, DS_I16(0x7a16));
                screen_update();
            }
        } else if (g_cur_room == 4 && (int8_t)STUDIO(i)[3] == DS_I16(0x7a16)) {
            movie_agency_reload_texts();
        }
    }
}

/* 2bd1:0668 – höchstens 4 Programme eines Genres (Archiv + Auktion + Studio); sonst Meldung, -1 */
int16_t archive_capacity_check(int16_t player, int16_t prog, int16_t genre)
{
    if (genre == 0)
        programme_genre_get(&prog, &genre);
    int16_t n = DS_I8(0x6f59 + player * 0x6c + genre);
    for (int16_t i = 1; i < 0xb; i++) {
        if ((int8_t)LOT(i)[6] == player) {
            int16_t p = W(LOT(i), 0), g;
            programme_genre_get(&p, &g);
            if (g == genre)
                n++;
        }
    }
    for (int16_t i = 1; i < 0xd; i++) {
        uint8_t *st = STUDIO(i);
        if ((int8_t)st[3] == player && W(st, 0) != 0) {
            uint8_t *sh = SHOW(W(st, 0));
            if (((UW(sh, 5) & 1) && st[4] == 1 && genre == 0xb) ||
                (!(UW(sh, 5) & 1) && (int8_t)FILM(W(sh, 6))[0] == genre))
                n++;
        }
    }
    if (n < 4)
        return 0;
    show_message(0x58);
    return -1;
}

/* 2bd1:07c2 – Genre eines Programms; Serien: *prog -= 1000, Genre 11 */
void programme_genre_get(int16_t *prog, int16_t *genre)
{
    if (*prog < 0x3e9) {
        *genre = (int8_t)FILM(*prog)[0];
    } else {
        *prog -= 1000;
        *genre = 0xb;
    }
}

/* 2bd1:07fb – restliche Spots des zur Stunde gesendeten Werbevertrags */
int16_t ad_spots_remaining(int16_t player, int16_t hour)
{
    int16_t ad = AD_SCHEDULE(player, hour) - 3000;
    int16_t n = DS_I8(0x3f38 + ad * 0x18);
    for (int16_t i = 0; i < AD_CONTRACT_COUNT(player); i++) {
        if (W(AD_CONTRACT(player, i), 0) == ad) {
            n = (int8_t)AD_CONTRACT(player, i)[4];
            break;
        }
    }
    for (int16_t h = 7; h >= hour + 1; h--) {
        int16_t v = AD_SCHEDULE(player, h);
        if (v - 3000 == ad)
            n--;
        else if (v - 2000 == ad)
            n = DS_I8(0x3f38 + ad * 0x18) - 1;
    }
    return n;
}

/* 2bd1:08f0 – Werbeverträge mit heutigem Stichtag: Strafe zahlen, entfernen */
void ad_contracts_expire(void)
{
    for (int16_t p = 1; p < 4; p++) {
        for (int16_t i = 0; i < AD_CONTRACT_COUNT(p); i++) {
            uint8_t *c = AD_CONTRACT(p, i);
            if (W(c, 5) != g_day)
                continue;
            int16_t ad = W(c, 0);
            int32_t penalty = L(AD(ad), 0x14);
            PLAYER_MONEY(p) -= penalty;
            if (p == g_local_player) {
                money_add(5, penalty);
                if (penalty == 0)
                    player_message_push(0x110, p, ad, 0, 0, 0, 0);
                else
                    player_message_push(0x10f, p, ad, penalty, 0, 0, 0);
            }
            for (int16_t h = 1; h < 0xf; h++)
                if (AD_SCHEDULE(p, h) == W(c, 0))
                    AD_SCHEDULE(p, h) = 0;
            list_remove(p, i);
            i--;
        }
    }
}

/* 2bd1:0a80 – Sonderfilm 0x3a: zur Auktion stellen oder nach 3 Tagen ohne Ausstrahlung wegnehmen */
void special_film_check(void)
{
    int16_t i;
    for (i = 0; i < 3; i++)
        DS_I16(0x79a6 + (i + 0x3a) * 2) = 0;
    int16_t owner = (int8_t)FILM(0x3a)[5];
    if (owner == 0) {
        for (i = 1; i < 0xb; i++) {
            if (W(LOT(i), 0) == 0) {
                auction_lot_set(i, 0x3a, -1);
                break;
            }
        }
    } else {
        if (g_day <= 0)
            return;
        int16_t h = 1;
        while (h < 8 && SCHEDULE(owner, h) != 0x3a)
            h++;
        if (h < 8) {
            DS_I16(0x79a6 + (owner + 0x3a) * 2) = 0x29;
        } else {
            if (++DS_I16(0x2421) < 3)
                return;
            programme_remove_from_archives(0x3a);
            for (h = 1; h < 0xb; h++) {
                if (W(LOT(h), 0) == 0) {
                    auction_lot_set(h, 0x3a, -1);
                    break;
                }
            }
            if (owner > 0 && owner < 4) {
                DS_I16(0x79a6 + (owner + 0x3a) * 2) = 0x111;
                PLAYER_METER(owner) >>= 1;
                status_meter_draw();
            }
        }
    }
    DS_I16(0x2421) = 0;
}

/* 2bd1:0bc8 – Auktionslos mit Programm belegen (kind 0/-1 Film, sonst Serie) */
void auction_lot_set(int16_t lot, int16_t prog, int16_t kind)
{
    uint8_t *l = LOT(lot);
    W(l, 0) = prog;
    if (kind == 0 || kind == -1) {
        uint8_t *f = FILM(prog);
        L(l, 2) = film_price(prog);
        if (f[5] == 0 || kind == -1)
            l[7] = (uint8_t)(rand_range(7, prog) + 3);
        else
            l[7] = f[5];
        f[5] = l[7];
        UW(f, 6) &= ~0x20;
        if (prog == 0x1d) {
            W(f, 0x11) = 4;
            W(f, 0xf) = g_day + 4;
            DS_I16(0x7a22) = 3;
        } else if (prog == 0xbb) {
            W(f, 0x11) = 3;
            W(f, 0xf) = g_day + 4;
            DS_I16(0x7a7c) = 5;
        } else if (f[0] == 7) {
            W(f, 0x11) = rand_range((int16_t)(8 - (int8_t)f[4]), 3);
            W(f, 0xf) = (int16_t)(rand_range(3, prog) + g_day + 3);
        }
    } else {
        W(l, 0) = (int16_t)(prog + 1000);
        uint8_t *s = SERIES(prog);
        L(l, 2) = series_price(prog);
        if (s[0x24] == 0)
            l[7] = (uint8_t)rand_range(7, prog);
        else
            l[7] = s[0x24];
        s[0x24] = l[7];
        UW(s, 0x25) &= ~0x20;
    }
    l[8] = 0;
    l[6] = 0;
}

/* 2bd1:0d6e – Nachricht für den menschlichen Spieler in den Briefkasten */
void player_message_push(int16_t msg_id, int16_t player, int32_t a1, int32_t a2, int32_t a3, int32_t a4, int32_t a5)
{
    if (player != g_local_player)
        return;
    uint8_t *m = DS_ADDR(uint8_t, 0x609f + DS_I8(0x7227) * 0x1a);
    W(m, 0) = msg_id;
    L(m, 2) = a1;
    L(m, 6) = a2;
    L(m, 0xa) = a3;
    L(m, 0xe) = a4;
    L(m, 0x12) = a5;
    W(m, 0x16) = g_day;
    W(m, 0x18) = g_hour;
    message_slot_alloc(player);
    schedule_room_refresh();
}

/* 2bd1:0e2f – Kreditzinsen: 5 % pro Tag */
void loan_interest_pay(void)
{
    int32_t credit = DS_I32(0x7abe);
    if (credit != 0) {
        PLAYER_MONEY(g_local_player) -= credit / 20;
        money_add(0xf, credit / 20);
    }
}

/* 2bd1:0e8b – tägliche Kosten: Studiomieten, Sender, Nachrichten-Abos */
void daily_costs_pay(void)
{
    for (int16_t p = 1; p < 4; p++) {
        for (int16_t i = 1; i < 0xd; i++) {
            uint8_t *st = STUDIO(i);
            if ((int8_t)st[3] != p)
                continue;
            int32_t rent = L(st, 6);
            PLAYER_MONEY(p) -= rent;
            if (p == g_local_player)
                money_add(8, rent);
            else
                AI_BUDGET(p, 0x10) -= rent;
        }
        int32_t upkeep = L(STATIONS(p), 0) / 5;
        PLAYER_MONEY(p) -= upkeep;
        if (p == g_local_player) {
            money_add(9, upkeep);
        } else {
            int32_t direct, share;
            if (DS_U8(0x6e09 + p * 0x3c + 0x25) == 1) {
                direct = upkeep;
                share = 0;
            } else {
                direct = upkeep >> 1;
                share = upkeep >> 1;
            }
            AI_BUDGET(p, 0x14) -= share;
            ai_adjust((int8_t)p, direct * -1, -1);
        }
        for (int16_t c = 1; c < 4; c++) {
            int32_t cost = 0;
            if (PLAYER_SUBSCR(p, c) != 0) {
                if (c == 1)
                    cost = 1000;
                else if (c == 2 || c == 3)
                    cost = 10000;
                PLAYER_MONEY(p) -= cost;
                if (p == g_local_player)
                    money_add(10, cost);
                else
                    AI_BUDGET(p, 0xc) -= cost;
            }
        }
    }
}

/* 2bd1:10d6 – nicht verkaufte Angebote vom Vortag aus den Marktlisten nehmen */
void market_daily_cleanup(void)
{
    for (int16_t i = 1; i < 0xb; i++) {
        uint8_t *e = DS_ADDR(uint8_t, 0x1980 + i * 7); /* Werbeangebote */
        if (e[2] == 0 && W(e, 0) != 0) {
            AD(W(e, 0))[0xf] = 0;
            W(e, 0) = 0;
        }
    }
    for (int16_t i = 1; i < 6; i++) {
        uint8_t *e = DS_ADDR(uint8_t, 0x194a + i * 9); /* Showangebote, zwei Tage */
        if (e[2] == 0 && W(e, 0) != 0 && (int8_t)++e[3] > 1) {
            SHOW(W(e, 0))[4] = 0;
            W(e, 0) = 0;
        }
    }
    for (int16_t i = 1; i < 0xb; i++) {
        uint8_t *e = DS_ADDR(uint8_t, 0x19cd + i * 8);
        if (W(e, 0) != 0) {
            if (W(e, 0) < 1000)
                UW(FILM(W(e, 0)), 6) |= 0x20;
            else
                UW(SERIES(W(e, 0) - 1000), 0x25) |= 0x20;
            W(e, 0) = 0;
        }
    }
}

/* 2bd1:11e3 – Filmlizenzen mit heutigem Ablauftag entziehen */
void licenses_expire(void)
{
    for (int16_t p = 1; p < 4; p++) {
        for (int16_t k = 0; k < DS_I8(0x6f60 + p * 0x6c); k++) {
            int16_t film = DS_I16(0x6f31 + p * 0x6c + k * 2);
            if (film != 0x1d && film != 0xbb && W(FILM(film), 0xf) == g_day) {
                /* ORIGINAL-BUG: k wird nach dem Entfernen nicht zurückgesetzt, der Nachfolger wird übersprungen */
                programme_remove_from_archives(film);
                player_message_push(0x113, p, film, 0, 0, 0, 0);
            } else if (film == 0x1d && W(FILM(0x1d), 0xf) == g_day) {
                special_film_1d_tick();
            } else if (film == 0xbb && W(FILM(0xbb), 0xf) == g_day) {
                special_film_bb_tick();
            }
        }
    }
}

/* 2bd1:12cc – 15 verschiedene Schauspieler zufällig für den Markt auswählen */
void actors_market_shuffle(void)
{
    for (int16_t i = 1; i < 0x10; i++) {
        int16_t group, ok = 0;
        uint8_t *src;
        while (!ok) {
            ok = 1;
            do {
                do {
                    group = rand_range(6, i) - 1;
                    src = DS_ADDR(uint8_t, 0x4ff3 + group * 0xc0 + i * 0xc);
                } while (W(src, 2) == 0 && W(src, 4) == 0);
            } while (group == 4);
            for (int16_t k = 1; k < i; k++)
                if (W(src, 0) == DS_I16(0x5473 + k * 0xc))
                    ok = 0;
        }
        far_memcpy(DS_ADDR(uint8_t, 0x5473 + i * 0xc), src, 0xc);
    }
}

/* 2bd1:13b0 – neues Spiel: Startprogramme verteilen und Plan des Menschen füllen */
void start_programmes_assign(void)
{
    int16_t seed = 1, n;
    for (int16_t p = 1; p <= 3; p++) {
        n = rand_range(3, seed) + 0x11; /* eine freie Serie 18..20 */
        while (SERIES(n)[0x24] != 0)
            n = rand_range(3, ++seed) + 0x11;
        programme_buy((int16_t)(n + 1000), p, 0);
        for (int32_t k = 1; k <= 3; k++) { /* drei freie Filme des Genres 6 mit Bit 5 */
            n = rand_range(200, seed);
            while (FILM(n)[5] != 0 || FILM(n)[0] != 6 || !(UW(FILM(n), 6) & 0x20))
                n = rand_range(200, ++seed);
            programme_buy(n, p, 0);
        }
    }
    int16_t h = 1;
    while (h <= 7) {
        n = rand_range(0xfa, seed++);
        int16_t len;
        if (n <= 200) {
            uint8_t *f = FILM(n);
            if ((int8_t)f[5] != g_local_player)
                continue;
            len = (int8_t)f[4];
            if (len + h - 1 - (h - 1) / 7 * 7 > 7 || (UW(f, 7) & 2))
                continue;
            SCHEDULE(g_local_player, h) = n;
            for (int16_t k = h + 1; k <= len + h - 1; k++)
                SCHEDULE(g_local_player, k) = 9999;
            h += (int8_t)f[4];
            UW(f, 7) |= 2;
        } else {
            n -= 200;
            uint8_t *s = SERIES(n);
            if ((int8_t)s[0x24] != g_local_player)
                continue;
            len = (int8_t)s[0x23];
            if (len + h - 1 - (h - 1) / 7 * 7 > 7)
                continue;
            SCHEDULE(g_local_player, h) = (int16_t)(n + 1000);
            if (W(s, 0x31) == 0)
                W(s, 0x31) = 1;
            for (int16_t k = h + 1; k <= (int8_t)s[0x23] + h - 1; k++) {
                SCHEDULE(g_local_player, k) = 9999;
                if (h <= 7)
                    SCHEDULE(g_local_player, k + 0xf) = series_next_episode(g_local_player, n, k);
            }
            h += (int8_t)s[0x23];
        }
    }
}

/* ---- Sendegebiete: Bevölkerungskarte far DS:7c5e, Abdeckungskarte far DS:8b8a.
 * 2 Bit pro Bildpunkt, 8 Punkte pro Wort, 0x15 Wörter pro Zeile, 0x6d Zeilen.
 * Sender-Kreis: Maske DS:11f2, 16 Zeilen x 2 Wörter. 16-Bit-Schiebe um 16 ergeben 0 (wie x86). */
#define POP_MAP   DS_PTR(uint16_t, 0x7c5e)
#define COV_MAP   DS_PTR(uint16_t, 0x8b8a)
#define CIRCLE    DS_ADDR(uint16_t, 0x11f2)
#define FLOOR_B(floor, ofs) DS_U8(0x7c62 + (floor) * 0x7a + (ofs))
#define g_text(id)        PTR32(char, DS_ADDR(ptr32, 0x8370)[id])

static uint16_t shr16(uint16_t v, int n) { return n >= 16 ? 0 : (uint16_t)(v >> n); }
static uint16_t shl16(uint16_t v, int n) { return n >= 16 ? 0 : (uint16_t)(v << n); }

/* 2bd1:1637 – Bevölkerung im Senderkreis an x,y */
int32_t station_reach_at(int16_t x, int16_t y)
{
    int s = (x & 7) * 2, r = 16 - s;
    uint16_t *buf = far_calloc(0x40, 1);
    uint16_t *dst = buf, *mask = CIRCLE;
    uint16_t *src = POP_MAP + y * 0x15 + (x >> 3);
    for (int16_t i = 0; i < 0x10; i++) {
        dst[0] = (shr16(src[0], s) | shl16(src[1], r)) & mask[0];
        dst[1] = (shr16(src[1], s) | shl16(src[2], r)) & mask[1];
        dst += 2;
        mask += 2;
        src += 0x15;
    }
    int32_t n = mask_population_count(buf);
    far_free(buf);
    return n;
}

/* 2bd1:1729 – Wert einer Kartenzelle über Tabelle DS:1272 */
int16_t map_cell_value(int16_t x, int16_t y, uint16_t *map)
{
    return DS_ADDR(int16_t, 0x1272)[(map[y * 0x15 + (x >> 3)] >> ((x & 7) << 1)) & 3];
}

/* 2bd1:175f – gewichtete Bevölkerung im Rechteck (Gewichte DS:127a) */
int32_t map_population_sum(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t *map)
{
    int16_t *w = DS_ADDR(int16_t, 0x127a);
    int32_t sum = 0;
    for (uint16_t y = (uint16_t)y0; y < (uint16_t)y1; y++) {
        for (uint16_t x = (uint16_t)x0; x < (uint16_t)x1; x += 8) {
            uint16_t v = map[(x >> 3) + y * 0x15];
            int16_t s = (int16_t)(w[v & 3] + w[v >> 2 & 3] + w[v >> 4 & 3] + w[v >> 6 & 3] +
                                  w[v >> 8 & 3] + w[v >> 10 & 3] + w[v >> 12 & 3] + w[v >> 14]);
            sum += s;
        }
    }
    return sum;
}

/* 2bd1:1847 – Reichweite und Gesamtwert der Sender eines Spielers neu berechnen */
void stations_reach_update(int16_t player)
{
    uint8_t *st = STATIONS(player);
    int32_t value = 0;
    coverage_map_build(player);
    L(st, 4) = map_population_sum(0, 0, 0xa7, 0x6c, COV_MAP);
    for (int16_t i = 1; i < 0x21; i++)
        if (L(st + i * 0xe, 4) != 0)
            value += L(st + i * 0xe, 0);
    L(st, 0) = value;
}

/* 2bd1:1906 – Abdeckungskarte aus allen Sendern des Spielers aufbauen
 * (Sender 1/2: linke/rechte Kartenhälfte komplett) */
void coverage_map_build(int16_t player)
{
    uint16_t *cov = COV_MAP, *pop = POP_MAP;
    far_memset(cov, 0x11e2);
    for (int16_t i = 1; i <= 0x20; i++) {
        uint8_t *rec = STATIONS(player) + i * 0xe;
        if (L(rec, 4) == 0 || W(rec, 0xc) == 2)
            continue;
        if (i == 1) {
            for (int16_t y = 0; y < 0x6c; y++) {
                int16_t row = y * 0x15;
                for (int16_t c = 0; c < 10; c++)
                    cov[row + c] = pop[row + c];
                /* ORIGINAL-BUG: AND 0 statt Maske, Wort 10 behält nur sein eigenes Low-Byte */
                cov[row + 10] = (uint16_t)(int16_t)(int8_t)(uint8_t)cov[row + 10];
            }
        } else if (i == 2) {
            for (int16_t y = 0; y < 0x6c; y++) {
                int16_t row = y * 0x15;
                for (int16_t c = 0xb; c < 0x15; c++)
                    cov[row + c] = pop[row + c];
                cov[row + 10] |= pop[row + 10] & 0xff;
            }
        } else {
            coverage_stamp(i, player, pop, cov);
        }
    }
    DS_I32(0x7ab2) = map_population_sum(0, 0, 0x54, 0x6d, cov);
    DS_I32(0x7ab6) = map_population_sum(0x54, 0, 0xa8, 0x6d, cov);
}

/* 2bd1:1af5 – Türposition in der Gebäudeübersicht */
void building_map_door_pos(int16_t floor, int16_t slot, int16_t *x, int16_t *y)
{
    *y = (int16_t)((0xe - floor) * 0xf + 9);
    switch (FLOOR_B(floor, slot)) {
    case 80:  *x = 0x26; break;
    case 116: *x = 0x3d; break;
    case 180: *x = 0x6a; break;
    case 216: *x = 0x81; break;
    }
    if (floor < 8) { /* untere Etagen in der rechten Spalte */
        *y -= 0x69;
        *x += 0xa0;
    }
}

/* 2bd1:1b88 – Raumname an einer Tür (mit Besitzer), neu angelegter String */
char *door_name_text(int16_t floor, int16_t slot)
{
    uint8_t owner = FLOOR_B(floor, 0x46 + slot);
    uint8_t alt = FLOOR_B(floor, 0x32 + slot);
    uint8_t id = alt != 0 ? alt : FLOOR_B(floor, 0x28 + slot);
    if (owner == 0)
        return fmt(g_text(id));
    char *name = FLOOR_B(floor, 0x1e + slot) == 1 ? DS_PTR(char, 0x8bbc + owner * 0x17)  /* Spielername */
                                                  : DS_PTR(char, 0x8bc0 + owner * 0x17); /* Sendername */
    return fmt(DS_ADDR(char, alt != 0 ? 0x12b6 : 0x12bc), g_text(id), name);
}

/* 2bd1:1cef – Stunde als Planindex: 17 Uhr = 0 ... 16 Uhr = 23 */
int16_t hour_index(void)
{
    return g_hour < 0x11 ? g_hour + 7 : g_hour - 0x11;
}

/* 2bd1:1d05 */
int16_t hour_index_after_min5(void)
{
    return g_minute < 5 ? hour_index() : hour_index() + 1;
}

/* 2bd1:1d18 – erste noch planbare Stunde */
int16_t hour_index_after_min55(void)
{
    return g_minute < 0x37 ? hour_index() : hour_index() + 1;
}

/* 2bd1:1d2b – Senderkreis (maskiert mit der Bevölkerung) in die Abdeckungskarte stempeln */
void coverage_stamp(int16_t station, int16_t player, uint16_t *src_map, uint16_t *dst_map)
{
    uint8_t *rec = STATIONS(player) + station * 0xe;
    int16_t x = W(rec, 8), y = W(rec, 0xa);
    int s = (x & 7) * 2, r = 16 - s;
    uint16_t *src = src_map + y * 0x15 + (x >> 3);
    uint16_t *dst = dst_map + y * 0x15 + (x >> 3);
    uint16_t *mask = CIRCLE;
    for (int16_t i = 0; i < 0x10; i++) {
        dst[0] |= shl16(mask[0], s) & src[0];
        dst[1] |= (shr16(mask[0], r) | shl16(mask[1], s)) & src[1];
        dst[2] |= src[2] & shr16(mask[1], r);
        mask += 2;
        dst += 0x15;
        src += 0x15;
    }
}

/* 2bd1:1e49 – zusätzliche Bevölkerung, die ein neuer Sender an x,y erreichen würde */
int32_t station_reach_gain(int16_t x, int16_t y)
{
    int s = (x & 7) * 2, r = 16 - s;
    uint16_t *buf = far_calloc(0x40, 1);
    uint16_t *dst = buf;
    uint16_t *src = COV_MAP + y * 0x15 + (x >> 3);
    for (int16_t i = 0; i < 0x10; i++) {
        dst[0] = shr16(src[0], s) | shl16(src[1], r);
        dst[1] = shr16(src[1], s) | shl16(src[2], r);
        dst += 2;
        src += 0x15;
    }
    int32_t before = mask_population_count(buf);
    uint16_t *mask = CIRCLE;
    dst = buf;
    src = POP_MAP + y * 0x15 + (x >> 3);
    for (int16_t i = 0; i < 0x10; i++) {
        dst[0] |= (shr16(src[0], s) | shl16(src[1], r)) & mask[0];
        dst[1] |= (shr16(src[1], s) | shl16(src[2], r)) & mask[1];
        dst += 2;
        mask += 2;
        src += 0x15;
    }
    int32_t after = mask_population_count(buf);
    far_free(buf);
    return after - before;
}

/* 2bd1:1fdf – gewichtete Bevölkerung einer 16x16-Maske (Gewichte DS:1282) */
int32_t mask_population_count(uint16_t *mask)
{
    int16_t *w = DS_ADDR(int16_t, 0x1282);
    int16_t sum = 0;
    for (int16_t i = 0; i < 0x20; i++) {
        uint16_t v = mask[i];
        sum += w[v & 3] + w[v >> 2 & 3] + w[v >> 4 & 3] + w[v >> 6 & 3] +
               w[v >> 8 & 3] + w[v >> 10 & 3] + w[v >> 12 & 3] + w[v >> 14 & 3];
    }
    return sum;
}

/* 2bd1:208f – Anzahl Schauspielerrollen einer Show */
int16_t show_role_count(int16_t show)
{
    int16_t n = 0;
    for (int16_t i = 1; i < 0xb; i++) {
        int8_t type = (int8_t)SHOW(show)[i * 3 + 9];
        if (type > 0x10 && type < 0x2a)
            n++;
    }
    return n;
}

/* ---- Archiv: DS:6ef9 + p*0x6c, je Genre 4 Einträge (Serien = Genre 11), Anzahl bei +0x60+Genre */
#define ARCHIVE(p, g, k)    DS_I16(0x6ef9 + (p) * 0x6c + (g) * 8 + (k) * 2)
#define ARCHIVE_COUNT(p, g) DS_I8(0x6f59 + (p) * 0x6c + (g))
#define SUITCASE(i)         DS_I16(0x2463 + (i) * 2)
#define SUITCASE_COUNT      DS_I16(0x732a)

/* 2bd1:20e4 – Programm kaufen: Besitzer, Geld, ins Archiv (max 4 je Genre) oder in den Koffer */
void programme_buy(int16_t prog, int16_t player, int32_t price)
{
    if (prog < 1000) {
        uint8_t *f = FILM(prog);
        f[5] = (uint8_t)player;
        PLAYER_MONEY(player) -= price;
        int16_t g = (int8_t)f[0];
        int16_t n = ARCHIVE_COUNT(player, g);
        if (n < 4) {
            ARCHIVE(player, g, n) = prog;
            ARCHIVE_COUNT(player, g)++;
        } else if (player == g_local_player && SUITCASE_COUNT < 10) {
            SUITCASE(SUITCASE_COUNT) = prog;
            SUITCASE_COUNT++;
        } else {
            f[5] = 0;
        }
        if (player == g_local_player)
            money_add(1, price);
    } else {
        int16_t sr = prog - 1000;
        SERIES(sr)[0x24] = (uint8_t)player;
        PLAYER_MONEY(player) -= price;
        if (ARCHIVE_COUNT(player, 0xb) < 4) {
            ARCHIVE(player, 0xb, ARCHIVE_COUNT(player, 0xb)) = sr;
            ARCHIVE_COUNT(player, 0xb)++;
        } else if (player == g_local_player && SUITCASE_COUNT < 10) {
            SUITCASE(SUITCASE_COUNT) = prog;
            SUITCASE_COUNT++;
        } else {
            SERIES(sr)[0x24] = 0;
        }
        if (player == g_local_player)
            money_add(2, price);
    }
}

/* 2bd1:22af – 1, wenn alle Rollen der Folge besetzt sind */
int16_t episode_cast_complete(int16_t show, int16_t episode)
{
    uint8_t *ep = SHOW(show) + (episode - 1) * 0x23;
    int16_t ok = 1;
    for (int16_t r = 1; ep[r * 3 + 9] != 0; r++)
        if (ep[r * 3 + 0xb] == 0)
            ok = 0;
    return ok;
}

/* 2bd1:2316 – Eignung eines Schauspielers für eine Rolle in Prozent */
int16_t actor_suitability(int16_t show, int16_t episode, int16_t role, int16_t actor)
{
    uint8_t *sh = SHOW(show);
    int16_t got = 0, need = 0;
    int16_t t = (int8_t)sh[(episode - 1) * 0x23 + role * 3 + 9] - 1;
    int8_t group = (int8_t)sh[t + 0x1a4];
    for (int16_t i = 0; i < 4; i++) {
        if (i == 1)
            continue;
        int8_t req = (int8_t)sh[t * 4 + i + 0x168];
        got += min_i16(req, DS_I8(0x4ff9 + (group - 1) * 0xc0 + actor * 0xc + i));
        need += req;
    }
    return need == 0 ? 0 : (int16_t)(got * 100 / need);
}

/* 2bd1:2404 – auf Vielfache von step runden */
int32_t round_to_step(int32_t value, int32_t step)
{
    return (value + (step >> 1)) / step * step;
}

/* 2bd1:2431 – Zuschauer in Quote relativ zur Reichweite umrechnen (gerundet) */
int32_t rating_from_viewers(int16_t player, int32_t viewers)
{
    int32_t base = (int32_t)((uint32_t)L(STATIONS(player), 4) * 0x9a0u);
    int32_t div = base / 100;
    if (div == 0)
        return 0; /* im Original Division durch 0 */
    return (base / 200 + viewers) / div;
}

/* 2bd1:24a5 – Sonderfilm 0x1d: Lizenz verlängern oder nach Ablauf wegnehmen */
void special_film_1d_tick(void)
{
    if (DS_I16(0x7a22) < 1) {
        DS_I16(0x7a22)--;
        programme_remove_from_archives(0x1d);
        DS_I16(0x7a22) = 3;
    } else {
        DS_I16(0x7a22)--;
        W(FILM(0x1d), 0xf) = g_day + 3;
        W(FILM(0x1d), 0x11) = 4;
    }
}

/* 2bd1:24d9 – dasselbe für Sonderfilm 0xbb */
void special_film_bb_tick(void)
{
    if (DS_I16(0x7a7c) < 1) {
        DS_I16(0x7a7c)--;
        programme_remove_from_archives(0xbb);
        DS_I16(0x7a7c) = 5;
    } else {
        DS_I16(0x7a7c)--;
        W(FILM(0xbb), 0xf) = g_day + 2;
        W(FILM(0xbb), 0x11) = 3;
    }
}

/* ---- Ereignisfigur (Figur 5, DS:7bb8): Typ DS:7a3c 1/2 Terrorist (Ziel Raum 0x13/0x12),
 * 3 Gerichtsvollzieher (Ziel 7a3e), 4 Hausmeister. Zustand: Typ<4 -> DS:79a6[0x47+Typ], sonst DS:7a68. */
#define EVT_FIG           DS_ADDR(uint8_t, 0x7bb8)
#define g_event_type      DS_I16(0x7a3c)
#define g_event_target    DS_I16(0x7a3e)
#define EVT_STATE(t)      DS_I16(0x79a6 + ((t) + 0x47) * 2)

static void event_fig_reset(void)
{
    uint8_t *f = EVT_FIG;
    W(f, 0x18) = 2;
    W(f, 0x10) = 0;
    W(f, 4) = 0x48;
    W(f, 0) = 0x48;
    W(f, 6) = 1;
    W(f, 2) = 1;
    W(f, 8) = 0;
    W(f, 0x12) = -1;
    W(f, 0xe) = 0;
}

/* 2bd1:250c – Ereignis des Tages festlegen (Gerichtsvollzieher bei mehr als 100000 Schulden) */
void event_npc_init(void)
{
    int16_t type = 0;
    g_event_target = 8;
    if (PLAYER_MONEY(g_local_player) < -100000) {
        g_event_target = g_local_player;
        type = 3;
    }
    if (type == 0 && g_day > 2) {
        type = rand_range(9, 1) - 6;
        if (type > 0 && type < 3 && EVT_STATE(type) == 0)
            type = 0;
    }
    if (type < 1) {
        event_fig_reset();
        DS_I16(0x7a68) = 1;
        g_event_type = 4;
    } else {
        if (EVT_STATE(type) != 0) {
            EVT_STATE(type) = 2;
            if (type < 3)
                news_add_special(0xa6, 0x10, type);
            event_fig_reset();
            g_event_type = type;
        }
        DS_I16(0x7a68) = 0;
    }
    DS_I16(0x7a44) = 0;
    DS_I16(0x7a46) = 0;
}

/* 2bd1:266b – Ereignisfigur steuern: Ziel suchen, hingehen, zuschlagen, zurück */
void random_events(void)
{
    int16_t *state = g_event_type < 4 ? &EVT_STATE(g_event_type) : &DS_I16(0x7a68);
    uint8_t *f = EVT_FIG;
    if (*state == 0)
        return;
    if (*state == 2 && hour_index() >= 1 && !(g_cur_room == 0 && DS_I16(0x8c2e) <= 0x1e)) {
        /* Start: vor 23 Uhr nur zufällig (abhängig vom Spieltempo), Hausmeister immer */
        int16_t go;
        if (hour_index() > 5)
            go = 1;
        else if (rand_range((int16_t)(162000L / DS_I16(0x7282)), 1) == 1)
            go = 1;
        else
            go = g_event_type == 4;
        if (go) {
            uint16_t room = 0, owner = 0; /* im Original bei unbekanntem Typ uninitialisiert */
            *state = 3;
            switch (g_event_type) {
            case 1: room = 0x13; owner = 0; break;
            case 2: room = 0x12; owner = 0; break;
            case 3:
                if (g_event_target < 4) {
                    room = 1;
                    owner = (uint16_t)g_event_target;
                } else {
                    room = (uint16_t)g_event_target;
                    owner = 0;
                }
                break;
            case 4: room = 4; owner = (uint16_t)g_event_target; break;
            }
            for (int16_t i = 0; i < 0xd; i++) {
                for (int16_t k = 0; k < 4; k++) {
                    int16_t fl = DS_I8(0x8a40 + i * 8 + k * 2);
                    int16_t sl = DS_I8(0x8a41 + i * 8 + k * 2);
                    if (FLOOR_B(fl, sl + 0x1e) == room && (FLOOR_B(fl, sl + 0x46) == owner || owner == 0)) {
                        W(f, 0x10) = 1;
                        W(f, 6) = i + 1;
                        int16_t door = -1;
                        for (int16_t j = 0; j <= k; j++)
                            if (DS_I8(0x8a40 + i * 8 + j * 2) != 0)
                                door++;
                        W(f, 0x12) = door;
                        W(f, 4) = 0x99;
                        W(f, 0x16) = 0;
                    }
                }
            }
        }
    } else if (*state == 3 && W(f, 0x10) == 0) {
        switch (room_entry_check(5, 1)) {
        case 0: *state = 4; break;
        case 1:
            *state = W(f, 0x12) + 1000;
            W(f, 0x10) = 2;
            W(f, 0x12) = -1;
            break;
        case 2: *state = 0x22; break;
        }
        if (*state < 1000)
            *state = 4;
    } else if (*state >= 4 && *state <= 0x20) {
        (*state)++; /* vor der Tür warten */
    } else if (*state == 0x21) {
        DS_I16(0x7a44) = W(f, 0x12);
        DS_I16(0x7a46) = W(f, 2);
        DS_I16(0x7a42) = 0;
        if (g_event_type <= 2) {
            DS_I16(0x7a42) = 1; /* Bombe tickt */
            DS_I16(0x7a48) = 1;
        } else if (g_event_type == 3 || g_event_type == 4) {
            event_npc_strike();
        }
        W(f, 0x12) = -1;
        W(f, 8) = 0x48;
        W(f, 6) = 1;
        W(f, 4) = 0x99;
        *state = 0x22;
    } else if (*state == 0x22 && W(f, 4) == W(f, 0) && W(f, 2) == W(f, 6) && W(f, 0x16) == 0 && W(f, 0xe) == 0) {
        W(f, 0x10) = 0;
        if (g_event_type <= 2)
            EVT_STATE(g_event_type) = DS_I16(0x7a4a) != 0 ? 0 : 1;
        else
            DS_I16(0x7a3a) = 1;
    } else if (*state >= 1000) {
        W(f, 0x12) = *state % 1000;
        if (room_entry_check(4, 1) == 0)
            *state = 3;
        else
            W(f, 0x12) = -1;
    }
    if (DS_I16(0x7a42) != 0) {
        DS_I16(0x7a42)++;
        if (DS_I16(0x7a42) == 100) { /* Explosion */
            event_npc_strike();
            DS_I16(0x7a42) = 1000;
            DS_I16(0x7a48) = 2;
        }
        if (DS_I16(0x7a42) == 0x41a) {
            DS_I16(0x7a42) = 0;
            DS_I16(0x7a48) = 3;
        }
    }
}

/* zufälliges Programm aus dem Archiv wählen (Genre gewichtet mit Füllstand) */
static int16_t archive_random_pick(int16_t player, int16_t seed)
{
    int16_t g, k;
    do {
        g = rand_range(10, seed);
        int16_t n = ARCHIVE_COUNT(player, g);
        k = rand_range(4, seed);
        if (n >= k)
            break;
    } while (1);
    return ARCHIVE(player, g, k - 1);
}

static int16_t archive_total_bugged(int16_t player)
{
    /* ORIGINAL-BUG: zählt Genre 2..12 statt 1..11 */
    int16_t sum = 0;
    for (int16_t g = 1; g <= 0xb;) {
        g++;
        sum += ARCHIVE_COUNT(player, g);
    }
    return sum;
}

/* 2bd1:2a41 – Ereignisfigur schlägt an der Tür DS:7a46/7a44 zu */
void event_npc_strike(void)
{
    int16_t fl = DS_I16(0x7a46), sl = DS_I16(0x7a44);
    uint8_t room = FLOOR_B(fl, sl + 0x1e);
    uint16_t studio = FLOOR_B(fl, sl + 0x6e);
    uint16_t owner = FLOOR_B(fl, sl + 0x46);
    if (room < 0x10 || room > 0x18) {
        if (g_event_type < 3) {
            news_add_special(0xa8, 0x10, g_event_type);
            DS_I16(0x7a4a) = 0;
        }
    } else {
        if (g_event_type < 3) {
            if ((g_event_type == 1 && room == 0x13) || (g_event_type == 2 && room == 0x12)) {
                news_add_special(0xa7, 0x10, g_event_type);
                DS_I16(0x7a4a) = 1;
            } else {
                news_add_special(0xa8, 0x10, g_event_type);
                DS_I16(0x7a4a) = 0;
            }
        }
        FLOOR_B(fl, sl + 0x1e) = 0x36; /* Ruine */
        FLOOR_B(fl, sl + 0x28) = 0x89;
        FLOOR_B(fl, sl + 0x32) = 0x8a;
        FLOOR_B(fl, sl + 0x46) = 0;
        STUDIO(studio)[3] = 0xff;
    }
    if (room == 5) {
        studio_slot_release(studio);
        player_message_push(g_event_type < 3 ? 0x133 : 0x136, owner, fl, 0, 0, 0, 0);
        FLOOR_B(fl, sl + 0x1e) = 0x36;
        FLOOR_B(fl, sl + 0x28) = 0x89;
        FLOOR_B(fl, sl + 0x32) = 0x8a;
        FLOOR_B(fl, sl + 0x46) = 0;
        STUDIO(studio)[3] = 0xff;
        W(STUDIO(studio), 0) = 0;
        STUDIO(studio)[4] = 0;
    } else if (room == 8 && g_event_type == 3) {
        DS_I16(0x7a48) = 4;
    } else if (room == 2 && g_event_type == 3) { /* Nachrichtenstudio gepfändet */
        DS_I16(0x7286 + owner * 2) = 0;
        far_memset(DS_ADDR(uint8_t, 0x3d69 + owner * 0x78), 0x5a);
        DS_I16(0x3d69 + owner * 0x78) = 0xa9;
        DS_U8(0x3f2b + owner) = 0;
    } else if (room == 4) {
        if (g_event_type == 4) {
            programme_lose(schedule_get_programme(owner, hour_index()), 0x15b, owner);
        } else if (archive_total_bugged(owner) != 0) {
            programme_lose(archive_random_pick(owner, room), g_event_type < 3 ? 0x134 : 0x138, owner);
        }
    } else if (room == 1) {
        if (owner == (uint16_t)g_local_player)
            DS_I16(0x7a04) = 0;
        if (g_event_type == 3 && archive_total_bugged(owner) != 0)
            programme_lose(archive_random_pick(owner, room), 0x138, owner);
    }
    if (owner != 0 && g_event_type < 3) { /* Versicherung */
        PLAYER_MONEY(owner) += 50000;
        if (owner == (uint16_t)g_local_player) {
            money_add(0x10, 50000);
            status_money_update();
        }
        player_message_push(0x135, owner, 0, 0, 0, 0, 0);
    }
}

/* 2bd1:2e9e – Programm verlieren: aus Archiv und restlichem Tagesplan entfernen */
void programme_lose(int16_t prog, int16_t msg_id, int16_t player)
{
    programme_remove_from_archives(prog);
    for (int16_t h = hour_index(); h < 0xf; h++) {
        if (SCHEDULE(player, h) != prog)
            continue;
        schedule_clear_programme(player, h);
        if (hour_index() == h) { /* läuft gerade */
            DS_I16(0x248d + player * 10) = 0;
            DS_I16(0x2491 + player * 10) = 0;
            DS_I16(0x248f + player * 10) = 0;
            DS_I32(0x74ca + player * 0x20 + h * 4) = 0;
            DS_I16(0x1c68 + player * 0x1e + (h + 7) * 2) = 0;
            if (player == DS_I16(0x79de)) {
                DS_I16(0x7a40) = 1;
                DS_I16(0x79ae) = 1;
                tv_show_current_programme();
            }
            if (player == g_local_player)
                DS_I16(0x79d6) = 1;
        }
    }
    if (g_event_type < 4) {
        player_message_push(msg_id, player, 0, 0, 0, 0, 0);
        if (g_event_type == 3) { /* gepfändet: halber Filmpreis wird gutgeschrieben */
            PLAYER_MONEY(player) += film_price(prog) >> 1;
            if (player == g_local_player)
                money_add(0xb, film_price(prog) >> 1);
        }
    } else {
        player_message_push(msg_id, player, prog, 0, 0, 0, 0);
    }
    if (g_cur_room == 0x1f && player == DS_I16(0x7a16))
        schedule_room_refresh();
}

/* ---- Text-Caches (gefüllt von Segment 1433) */
/* 2bd1:3024 – Filmtitel aus dem Cache DS:63c5 (49 x 10 B: id, Titel, Beschreibung) */
char *film_title_get(int16_t film)
{
    int16_t i;
    for (i = 0; DS_I16(0x63c5 + i * 10) != film && i < 0x31; i++)
        ;
    return i < 0x31 ? DS_PTR(char, 0x63c7 + i * 10) : NULL;
}

/* 2bd1:306e */
char *film_descr_get(int16_t film)
{
    int16_t i;
    for (i = 0; DS_I16(0x63c5 + i * 10) != film && i < 0x31; i++)
        ;
    return i < 0x31 ? DS_PTR(char, 0x63cb + i * 10) : NULL;
}

static int16_t series_cache_find(int16_t series)
{
    int16_t i;
    for (i = 0; DS_I16(0x65af + i * 0x5a) != series && i < 0xf; i++)
        ;
    return i;
}

/* 2bd1:30b8 – Serientitel aus dem Cache DS:65af (15 x 0x5a B) */
char *series_title_get(int16_t series)
{
    int16_t i = series_cache_find(series);
    return i < 0xf ? DS_PTR(char, 0x65b1 + i * 0x5a) : NULL;
}

/* 2bd1:3102 */
char *series_descr_get(int16_t series)
{
    int16_t i = series_cache_find(series);
    return i < 0xf ? DS_PTR(char, 0x65b5 + i * 0x5a) : NULL;
}

/* 2bd1:314c */
char *episode_title_get(int16_t series, int16_t episode)
{
    int16_t i = series_cache_find(series);
    return i < 0xf ? DS_PTR(char, 0x65b9 + i * 0x5a + (episode - 1) * 4) : NULL;
}

/* 2bd1:31a0 */
char *episode_descr_get(int16_t series, int16_t episode)
{
    int16_t i = series_cache_find(series);
    return i < 0xf ? DS_PTR(char, 0x65e1 + i * 0x5a + (episode - 1) * 4) : NULL;
}

/* 2bd1:31f4 – Show-Texte aus dem Cache DS:6af5 (0x42 B); sucht ohne Grenze wie das Original */
char *show_text_title_get(int16_t id)
{
    int16_t i = 0;
    while (DS_I16(0x6af5 + i * 0x42) != id)
        i++;
    return DS_PTR(char, 0x6af7 + i * 0x42);
}

/* 2bd1:322e */
char *show_text_get(int16_t id, int16_t idx)
{
    int16_t i = 0;
    while (DS_I16(0x6af5 + i * 0x42) != id)
        i++;
    return DS_PTR(char, 0x6afb + i * 0x42 + idx * 4);
}

/* 2bd1:3271 – Grund, warum Figur fig die Tür, vor der sie steht, nicht betreten kann (0 = frei).
 * query != 0: nur Codes 1 (belegt) / 2 (gesperrt) statt Meldungstexten. */
int16_t room_entry_check(int16_t fig, int16_t query)
{
    uint8_t *f = DS_ADDR(uint8_t, 0x7b22 + fig * 0x1e);
    int16_t fl = W(f, 2), door = W(f, 0x12);
    int16_t r = 0;
    if (query == 0 && DS_I16(0x7a48) != 0 && fl == DS_I16(0x7a46) && door == DS_I16(0x7a44)) {
        switch (DS_I16(0x7a48)) { /* Bombe/Ruine an dieser Tür */
        case 1: r = 0x130; break;
        case 2: r = 0x131; break;
        case 3: r = 0x132; break;
        case 4: r = 0x137; break;
        }
    }
    if (r == 0 && FLOOR_B(fl, door + 0x6e) != 0 && STUDIO(FLOOR_B(fl, door + 0x6e))[0xe] != 0)
        r = query == 0 ? 0xf4 : 2; /* Studio in Betrieb */
    if (r == 0) {
        for (int16_t i = 0; i < 6; i++) {
            uint8_t *o = DS_ADDR(uint8_t, 0x7b22 + i * 0x1e);
            if (i == fig || PLAYER_OUT(i + 1) != 0 || W(o, 0x18) != 2 || W(o, 0x10) != 0 ||
                W(o, 0x12) != W(f, 0x12) || W(o, 6) != W(f, 6))
                continue;
            if (query != 0) {
                r = 1;
            } else if (FLOOR_B(fl, door + 0x46) == (uint16_t)(fig + 1)) {
                r = 0x12e; /* jemand ist im eigenen Raum */
                if (fig == 0) {
                    int32_t *fine = &DS_I32(0x7ac6 + i * 4);
                    DS_I16(0x79f6) = i + 1;
                    DS_I32(0x7ad2) = *fine;
                    PLAYER_MONEY(g_local_player) += *fine;
                    money_add(0xd, DS_I32(0x7ad2));
                    status_money_update();
                    *fine <<= 1;
                    if (*fine > 200000)
                        *fine = 200000;
                }
            } else {
                r = 299;
            }
        }
    }
    if (r == 0 && FLOOR_B(fl, door + 0x1e) == 0x36)
        r = query == 0 ? 0x12f : 2; /* Ruine */
    if (r == 0) {
        uint8_t owner = FLOOR_B(fl, door + 0x46);
        if (owner != 0 && PLAYER_OUT(owner) != 0)
            r = query == 0 ? 0x49 : 2;
    }
    return r;
}

#define RANK_SCORE(p)  DS_I16(0x8f9c + (p) * 2)
#define RANK(i)        DS_I16(0x8fa4 + (i) * 2)
#define IMAGE_DELTA(p) DS_I16(0x8f94 + (p) * 2)
#define PLAYER_IMG(p)  DS_I16(0x8bd1 + (p) * 0x17) /* Image 0..100 */

/* 2bd1:363d – Spieler nach aktueller Quote ordnen und Image anpassen (+1 / 0 / -1) */
void ratings_rank_update(void)
{
    int16_t active = 0, p, q;
    for (p = 1; p <= 3; p++)
        if (PLAYER_OUT(p) == 0)
            active++;
    if (active == 1)
        return;
    for (p = 1; p < 4; p++)
        RANK_SCORE(p) = DS_I16(0x2495 + p * 10) == 0 ? DS_I16(0x248d + p * 10) : 0;
    int16_t tie = 0;
    for (p = 1; p <= 3; p++) {
        for (q = 1; q < 4; q++)
            if (p != q && PLAYER_OUT(p) == 0 && PLAYER_OUT(q) == 0 && RANK_SCORE(p) == RANK_SCORE(q))
                tie = 1;
        if (PLAYER_OUT(p) != 0)
            RANK_SCORE(p) = -1;
    }
    if (tie) {
        for (int16_t i = 0; i < 3; i++) {
            RANK(i) = 0;
            DS_U8(0x5533 + (i + 1) * 8 + hour_index()) = 1;
            IMAGE_DELTA(i + 1) = 0;
        }
    } else {
        for (int16_t i = 0; i < 3; i++) {
            if (active == 2 && i == 1) { /* ausgeschiedener Spieler auf den mittleren Platz */
                for (p = 1; PLAYER_OUT(p) == 0; p++)
                    ;
                RANK(i) = p;
            } else {
                int16_t best = -1, who = 0;
                for (p = 1; p <= 3; p++) {
                    if (RANK_SCORE(p) > best) {
                        best = RANK_SCORE(p);
                        who = p;
                    }
                }
                RANK(i) = who;
                RANK_SCORE(who) = -2;
            }
        }
        for (int16_t i = 0; i < 3; i++)
            DS_U8(0x5533 + RANK(i) * 8 + hour_index()) = (uint8_t)i;
        for (int16_t i = 0; i < 3; i++)
            IMAGE_DELTA(RANK(i)) = (int16_t)(1 - i);
    }
    image_distribute(DS_ADDR(int16_t, 0x8f94));
}

/* 2bd1:3895 – Image-Änderungen deltas[1..3] anwenden; Gewinne und Verluste gleichen sich aus,
 * die Summe bleibt 100 */
void image_distribute(int16_t *deltas)
{
    int16_t neg_sum = 0, neg_n = 0, pos_sum = 0, pos_n = 0, p;
    for (p = 1; p <= 3; p++) {
        if (deltas[p] < 0) {
            if (PLAYER_OUT(p) != 0)
                continue;
            neg_sum += PLAYER_IMG(p) + deltas[p] < 0 ? -PLAYER_IMG(p) : deltas[p];
            neg_n++;
        } else if (deltas[p] > 0 && PLAYER_OUT(p) == 0) {
            pos_sum += PLAYER_IMG(p) + deltas[p] > 100 ? 100 - PLAYER_IMG(p) : deltas[p];
            pos_n++;
        }
    }
    if (-neg_sum != pos_sum) {
        pos_sum = min_i16(pos_sum, (int16_t)-neg_sum);
        neg_sum = (int16_t)-pos_sum;
        for (p = 1; p <= 3; p++) {
            /* im Original Division durch 0 möglich (Delta nur bei ausgeschiedenen Spielern) */
            if (deltas[p] < 0)
                deltas[p] = neg_n ? (int16_t)(neg_sum / neg_n) : deltas[p];
            else if (deltas[p] > 0)
                deltas[p] = pos_n ? (int16_t)(pos_sum / pos_n) : deltas[p];
        }
    }
    for (p = 1; p <= 3; p++) {
        PLAYER_IMG(p) += deltas[p];
        if (PLAYER_IMG(p) < 0)
            PLAYER_IMG(p) = 0;
        if (PLAYER_IMG(p) > 100)
            PLAYER_IMG(p) = 100;
        if (PLAYER_METER(p) > PLAYER_IMG(p))
            PLAYER_METER(p) = PLAYER_IMG(p);
    }
    int16_t total = PLAYER_IMG(1) + PLAYER_IMG(2) + PLAYER_IMG(3);
    if (total != 100) {
        int16_t best = 0, who = 0;
        for (p = 1; p <= 3; p++) {
            if (PLAYER_IMG(p) > best) {
                best = PLAYER_IMG(p);
                who = p;
            }
        }
        PLAYER_IMG(who) += (int16_t)(100 - total);
    }
}

/* 2bd1:3b5d – nächste Preisstufe aus der Tabelle DS:128a (4 Werte) */
int32_t price_step_next(int32_t value)
{
    int16_t i = 0;
    while (!(DS_I32(0x128a + i * 4) > value) && i < 3)
        i++;
    return DS_I32(0x128a + i * 4);
}

/* 2bd1:3b96 */
int16_t genre_value_get(int16_t genre)
{
    return DS_I16(0x129a + (int8_t)genre * 2);
}

/* 2bd1:3ba7 – Filmpreis: halber Grundpreis + Anteil nach Aktualität, auf 1000 gerundet */
int32_t film_price(int16_t film)
{
    uint8_t *f = FILM(film);
    int32_t price = L(f, 8), v;
    if (price == 0 || f[1] == 0)
        v = 0;
    else
        v = (int32_t)((uint32_t)price * (uint32_t)(int32_t)(int8_t)f[3]) / ((int8_t)f[1] * 2) + price / 2;
    return round_to_step(v, 1000);
}

/* 2bd1:3c6e – Serienpreis aus den fertigen Folgen */
int32_t series_price(int16_t series)
{
    uint8_t *s = SERIES(series);
    int16_t cur = 0, max = 0, e;
    for (e = 1; e <= 10; e++) {
        uint8_t *ep = s + e * 3;
        if (!(UW(ep, 2) & 1))
            break;
        cur += (int8_t)ep[1];
        max += (int8_t)ep[0];
    }
    int32_t price = (int32_t)((uint32_t)L(s, 0x2b) * (uint32_t)(int32_t)(e - 1)), v;
    if (price == 0 || max == 0)
        v = 0;
    else
        v = (int32_t)((uint32_t)price * (uint32_t)(int32_t)cur) / ((int32_t)max * 2) + price / 2;
    return round_to_step(v, 1000);
}

/* 2bd1:3d63 – Geldbetrag in Millionen formatieren (1: "1,2", 2: "1,23", 3: automatisch) */
char *fmt_money(int32_t value, int16_t mode)
{
    if (mode == 3)
        mode = value < 1000000 ? 2 : 1;
    if (mode == 1) {
        value = round_to_step(value, 100000);
        int16_t m = (int16_t)(value / 1000000);
        return fmt(DS_ADDR(char, 0x12c2), m, (int16_t)((value - (int32_t)m * 1000000) / 100000));
    }
    if (mode == 2) {
        value = round_to_step(value, 10000);
        int16_t m = (int16_t)(value / 1000000);
        return fmt(DS_ADDR(char, 0x12c8), m, (int16_t)((value - (int32_t)m * 1000000) / 10000));
    }
    return NULL; /* im Original uninitialisiert */
}

/* 2bd1:3e86 – darf die Werbung im Umfeld des Genres laufen? (Bedingungen DS:3f3b..3f3d) */
int16_t ad_genre_allowed(int16_t ad, int16_t genre)
{
    int8_t *c = DS_ADDR(int8_t, 0x3f3a + ad * 0x18);
    int16_t ok = 1, i, r = 0;
    for (i = 1; i < 4; i++)
        if (c[i] < 0 && -genre == c[i])
            ok = 0;
    if (ok) {
        r = 2;
        for (i = 1; i < 4; i++) {
            if (c[i] > 0 && r == 2)
                r = 0;
            if (c[i] == genre)
                r = 1;
        }
        if (r == 2)
            r = 1;
    }
    return r;
}

#define NEWS_COUNT(p)     DS_I16(0x7286 + (p) * 2)
#define NEWS_CAT(p, i)    DS_I8(0x457b + (p) * 600 + (i) * 0x1e)

/* 2bd1:3f0b – Anzahl eigener Nachrichten einer Kategorie */
int16_t news_count_by_category(int16_t category)
{
    int16_t n = 0;
    for (int16_t i = 0; i < NEWS_COUNT(g_local_player); i++)
        if (NEWS_CAT(g_local_player, i) == category)
            n++;
    return n;
}

/* 2bd1:3f4f – nächste eigene Nachricht der Kategorie nach from, -1 wenn keine */
int16_t news_find_next(int16_t category, int16_t from)
{
    do {
        from++;
        if (from >= NEWS_COUNT(g_local_player))
            break;
    } while (NEWS_CAT(g_local_player, from) != (int8_t)category);
    return from >= NEWS_COUNT(g_local_player) ? -1 : from;
}

/* 2bd1:3fa2 */
int16_t image_at_least(int16_t player, int16_t min)
{
    return PLAYER_IMG(player) < min ? 0 : -1;
}

/* 2bd1:3fc7 – Nachrichten-Abos kündigen, deren Image-Schwelle DS:12a6 nicht mehr erreicht ist */
void news_subscriptions_check(void)
{
    for (int16_t p = 1; p < 4; p++) {
        for (int16_t c = 1; c < 4; c++) {
            if (PLAYER_SUBSCR(p, c) != 0 && PLAYER_IMG(p) < DS_I16(0x12a6 + c * 2)) {
                PLAYER_SUBSCR(p, c) = 0;
                if (p == g_local_player)
                    player_message_push(0x152, p, c, 0, 0, 0, 0);
            }
        }
    }
}

/* 2bd1:4048 – Bit im Wort DS:7a66 setzen (1) / löschen (2) / abfragen */
int16_t player_flag(int16_t bit, int16_t op)
{
    if (op == 1)
        bit_set(&DS_U16(0x7a66), (int8_t)bit);
    else if (op == 2)
        bit_clear(&DS_U16(0x7a66), (int8_t)bit);
    return (int8_t)bit_test(DS_U16(0x7a66), (int8_t)bit);
}

#define CONTEST_TYPE    DS_I16(0x7468)
#define CONTEST_PTS(p)  DS_I16(0x7468 + (p) * 2)
#define CONTEST_DAY     DS_I16(0x7470)

/* 2bd1:408e – Wettbewerb alle 3 Tage: Tag 0 Typ wählen, Tag 2 auswerten */
int16_t contest_tick(void)
{
    int16_t active = 0, p;
    for (p = 1; p < 4; p++)
        if (PLAYER_OUT(p) == 0)
            active++;
    if (CONTEST_DAY % 3 == 0 && CONTEST_TYPE == 0) {
        CONTEST_TYPE = rand_range(g_day < 5 ? 2 : 4, g_day);
        for (p = 1; p < 4; p++)
            CONTEST_PTS(p) = 0;
    } else if (CONTEST_DAY % 3 == 2 && CONTEST_TYPE != 0) {
        if (contest_winner() == 0) {
            if (contest_score(1) != 0 || contest_score(2) != 0) { /* Gleichstand: einen Tag verlängern */
                CONTEST_DAY--;
                player_message_push(0x15a, g_local_player, 0, 0, 0, 0, 0);
                return CONTEST_DAY % 3;
            }
            player_message_push(0x2b, g_local_player, CONTEST_TYPE, 0, 0, 0, 0);
        } else {
            int16_t delta[4] = {0, 0, 0, 0}; /* im Original uninitialisiert, wenn nur ein Spieler übrig ist */
            int16_t win = contest_winner();
            int16_t prize = DS_I16(0x12ae + (CONTEST_TYPE - 1) * 2);
            DS_I16(0x79e6) = win;
            for (p = 1; p < 4; p++) {
                if (p == win) {
                    if (active > 1)
                        delta[p] = prize;
                    CONTEST_PTS(p) = 1;
                } else {
                    CONTEST_PTS(p) = 0;
                    if (active > 1)
                        delta[p] = (int16_t)-(active == 2 ? prize : prize >> 1);
                }
            }
            image_distribute(delta);
            if (CONTEST_PTS(g_local_player) == 1)
                player_message_push(0x159, g_local_player, CONTEST_TYPE, 0, 0, 0, 0);
            else
                player_message_push(0x158, g_local_player, CONTEST_TYPE, win, 0, 0, 0);
        }
        CONTEST_TYPE = 0;
    }
    return CONTEST_DAY % 3;
}

/* 2bd1:427c – Punktzahl eines Spielers im laufenden Wettbewerb */
int16_t contest_score(int16_t player)
{
    if (PLAYER_OUT(player) != 0 || CONTEST_PTS(player) == 0)
        return 0;
    int16_t meter = PLAYER_METER(player);
    switch (CONTEST_TYPE) {
    case 1: case 4: return (int16_t)(CONTEST_PTS(player) + (meter >> 1));
    case 2:         return (int16_t)(CONTEST_PTS(player) + meter);
    case 3:         return (int16_t)(CONTEST_PTS(player) + meter / 3);
    }
    return 0;
}

/* 2bd1:432d – Spieler mit der höchsten Punktzahl, 0 bei Gleichstand */
int16_t contest_winner(void)
{
    int16_t best = 0, who = 0, n = 0, p;
    for (p = 1; p < 4; p++) {
        if (PLAYER_OUT(p) == 0 && contest_score(p) > best) {
            best = contest_score(p);
            who = p;
        }
    }
    for (p = 1; p < 4; p++)
        if (PLAYER_OUT(p) == 0 && contest_score(p) == best)
            n++;
    return n > 1 ? 0 : who;
}

/* 2bd1:43b5 – Anteil des Spielers an allen Punkten in Prozent */
int16_t contest_share(int16_t player)
{
    int16_t sum = 0;
    for (int16_t p = 1; p < 4; p++)
        sum += contest_score(p);
    return sum == 0 ? 0 : (int16_t)(contest_score(player) * 100 / sum);
}
