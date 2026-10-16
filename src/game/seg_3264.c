/* Segment 3264 – Märkte (Werbung, Shows, Filme, Auktion), Quotenberechnung, Tagesprüfung. */
#include "game.h"

#define g_shows           DS_PTR(uint8_t, 0x713c)
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define SHOW(n)           (g_shows + (n) * 0x1b3)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define PLAYER_METER(p)   DS_I16(0x8bc8 + (p) * 0x17)
#define PLAYER_OUT(p)     DS_U8(0x8bd0 + (p) * 0x17)
#define PLAYER_IMG(p)     DS_I16(0x8bd1 + (p) * 0x17)
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define AD_SCHEDULE(p, h) DS_I16(0x4433 + (p) * 0x5a + (h) * 2)
#define AD(n)             DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)
#define AD_CONTRACT(p, i) DS_ADDR(uint8_t, 0x1b50 + (p) * 0x46 + (i) * 7)
#define AD_CONTRACT_COUNT(p) DS_I16(0x7688 + (p) * 2)
#define STATION_REACH(p)  DS_I32(0x1ced + (p) * 0x1ce)
#define REACH_LEVEL(i)    DS_I32(0xa8 + (i) * 4)    /* 10 Reichweitenstufen */
#define STUDIO(i)         DS_ADDR(uint8_t, 0x6145 + (i) * 0x10)
#define LOT(i)            DS_ADDR(uint8_t, 0x1aed + (i) * 9)
#define CHAN_VALUE(p)     DS_I16(0x2481 + (p) * 2)  /* Zuschauerwert der laufenden Sendung */
#define RATING(p)         DS_I16(0x248d + (p) * 10) /* Quote, +2 Zuschauer (long), +6, +8 */
#define VIEWERS(p)        DS_I32(0x248f + (p) * 10)
#define GENRE_NOW(p)      DS_I8(0x71c6 + (p))
#define TOPIC_GENRE(i)    DS_I16(0x94 + (i) * 4)    /* Thema des Tages (3 Tage) */
#define TOPIC_BONUS(i)    DS_U16(0x96 + (i) * 4)

/* 3264:0001 – Werbeangebote (DS:1980, 10 x 7 B) mit freien Werbekunden auffüllen */
void ad_market_update(void)
{
    int16_t avail = 0, i, a;
    int32_t reach = max_i32(DS_I32(0x1ebb), max_i32(DS_I32(0x2089), DS_I32(0x2257)));
    reach = (int32_t)((uint32_t)reach * 0x9a0u) * 2 / 5;
    int16_t level = 1;
    while (REACH_LEVEL(level - 1) < reach && level <= 10)
        level++;
    level--;
    for (i = 1; i < 0xb; i++)
        if (W(DS_ADDR(uint8_t, 0x1980 + i * 7), 0) != 0)
            AD(W(DS_ADDR(uint8_t, 0x1980 + i * 7), 0))[0xf] = 0x68;
    for (int16_t p = 1; p < 4; p++)
        for (i = 0; i < AD_CONTRACT_COUNT(p); i++)
            AD(W(AD_CONTRACT(p, i), 0))[0xf] = (uint8_t)(p + 100);
    for (a = 1; a < 0x33; a++) {
        uint8_t *ad = AD(a);
        if ((int8_t)ad[0xf] < 101)
            ad[0xf] = 0;
        else
            ad[0xf] -= 100;
        if (ad[0xf] == 0 && L(ad, 0x10) != 0 && (int8_t)ad[0xa] <= level)
            avail++;
    }
    int16_t pick = 0; /* im Original uninitialisiert */
    for (i = 1; i <= 10; i++) {
        uint8_t *e = DS_ADDR(uint8_t, 0x1980 + i * 7);
        if (W(e, 0) != 0 || avail == 0)
            continue;
        uint8_t *ad = NULL;
        if (avail < 6) {
            for (a = 1; a <= 0x32; a++) {
                ad = AD(a);
                if (ad[0xf] == 0 && (int8_t)ad[0xa] <= level && L(ad, 0) != 0) {
                    pick = a;
                    break;
                }
            }
        } else {
            for (;;) {
                pick = rand_range(0x32, i);
                ad = AD(pick);
                if (ad[0xf] == 0 && L(ad, 0x10) != 0 && (int8_t)ad[0xa] <= level && L(ad, 0) != 0)
                    break;
            }
        }
        W(e, 0) = pick;
        e[2] = 0;
        e[3] = 0;
        ad[0xf] = 4;
        avail--;
    }
}

/* 3264:0274 – Showangebote (DS:194a, 5 x 9 B) erneuern; Show 24 ist immer im Angebot */
void production_market_update(void)
{
    for (int16_t s = 1; s < 0x33; s++) {
        uint8_t *sh = SHOW(s);
        if (sh[4] == 99 && (UW(sh, 5) & 2) && !(UW(sh, 5) & 1) && (UW(FILM(W(sh, 6)), 7) & 4)) {
            sh[4] = 0; /* Film ist abgelaufen: Show wieder frei, Besetzung löschen */
            for (int16_t e = 0; e < 10; e++)
                for (int16_t r = 0; r < 0xb; r++)
                    sh[e * 0x23 + r * 3 + 0xb] = 0;
        }
    }
    uint8_t *s24 = SHOW(0x18);
    if (s24[4] == 0) {
        uint8_t *e = DS_ADDR(uint8_t, 0x1953);
        W(e, 0) = 0x18;
        e[3] = 0;
        L(e, 4) = L(s24, 0);
        e[8] = (uint8_t)show_role_count(0x18);
        s24[4] = 4;
    }
    int16_t n = shows_free_count();
    for (int16_t i = 1; i <= 5 && n >= 1; i++, n--) {
        uint8_t *e = DS_ADDR(uint8_t, 0x194a + i * 9);
        if (W(e, 0) != 0)
            continue;
        int16_t s;
        uint8_t *sh;
        do {
            s = rand_range(0x32, i);
            sh = SHOW(s);
        } while (sh[4] != 0 || !(UW(sh, 5) & 2));
        W(e, 0) = s;
        e[3] = 0;
        L(e, 4) = L(sh, 0);
        sh[4] = 4;
        e[8] = (uint8_t)show_role_count(s);
    }
}

/* 3264:0438 – Filmangebote (DS:19cd, 10 x 8 B) auffüllen; bei kleinen Sendern bis zu 3 billige Filme */
void market_update_prices(void)
{
    int16_t seed = 0, cheap = 0, p, i;
    for (p = 1; p <= 3; p++)
        if ((int32_t)((uint32_t)STATION_REACH(p) * 0x9a0u) > 12000000)
            break;
    if (p > 3) {
        for (i = 1; i <= 200; i++) {
            uint8_t *f = FILM(i);
            if (!(UW(f, 6) & 0x40) && (f[5] == 0 || (int8_t)f[5] > 3) && (UW(f, 6) & 0x20) &&
                film_price(i) <= 60000 && L(f, 8) != 0) /* Release-Fassung: mit Preis */
                cheap++;
        }
        if (cheap > 3)
            cheap = 3;
    }
    /* Release-Fassung: Kandidaten vorher zählen (Filme mit Preis, Serien) und nur so viele Plätze
     * füllen; die Beta suchte endlos weiter, wenn keiner mehr übrig war */
    int16_t avail = 0;
    for (i = 1; i <= 200; i++) {
        uint8_t *f = FILM(i);
        if (!(UW(f, 6) & 0x40) && (f[5] == 0 || (int8_t)f[5] > 3) && (UW(f, 6) & 0x20) && L(f, 8) != 0)
            avail++;
    }
    for (i = 1; i <= 50; i++) {
        uint8_t *s = SERIES(i);
        if (!(UW(s, 0x25) & 0x40) && (s[0x24] == 0 || (int8_t)s[0x24] > 3) && (UW(s, 0x25) & 0x20))
            avail++;
    }
    for (i = 1; i <= 10; i++)
        if (DS_I8(0x19cd + i * 8 + 7) < 0)
            DS_I16(0x19cd + i * 8) = 0;
    for (i = 1; i <= 10; i++) {
        uint8_t *e = DS_ADDR(uint8_t, 0x19cd + i * 8);
        if (W(e, 0) != 0 || avail <= 0)
            continue;
        avail--;
        int16_t n = 0;
        while (n == 0) {
            n = rand_range(0xfa, seed++);
            if (n > 200) {
                n -= 200;
                uint8_t *s = SERIES(n);
                if ((UW(s, 0x25) & 0x40) || (s[0x24] != 0 && (int8_t)s[0x24] <= 3) || !(UW(s, 0x25) & 0x20)) {
                    n = 0;
                    continue;
                }
                W(e, 0) = (int16_t)(n + 1000);
                L(e, 2) = series_price(n);
                e[6] = 0;
                e[7] = (uint8_t)(rand_range(7, seed++) + 3);
                UW(s, 0x25) &= ~0x20;
                W(s, 0x31) = 1;
            } else {
                uint8_t *f = FILM(n);
                if ((UW(f, 6) & 0x40) || (f[5] != 0 && (int8_t)f[5] <= 3) || !(UW(f, 6) & 0x20) ||
                    L(f, 8) == 0 || (cheap > 0 && film_price(n) > 60000)) {
                    n = 0;
                    continue;
                }
                cheap--;
                W(e, 0) = n;
                L(e, 2) = film_price(n);
                e[6] = 0;
                e[7] = (uint8_t)(rand_range(7, seed++) + 3);
                UW(f, 6) &= ~0x20;
                if (f[0] == 7) { /* Lizenzfilm */
                    W(f, 0x11) = rand_range((int16_t)(8 - (int8_t)f[4]), seed++);
                    W(f, 0xf) = (int16_t)(rand_range(3, 3) + g_day + 1);
                }
            }
        }
    }
}

/* Release-Fassung: kann Film bzw. Serie n versteigert werden? (Bit 0x40, nicht verkauft oder in
 * keiner Auktion, Bit 0x20; Filme zusätzlich mit Preis) */
static int auction_film_ok(int16_t n)
{
    uint8_t *f = FILM(n);
    return (UW(f, 6) & 0x40) && (f[5] == 0 || (int8_t)f[5] > 3) && (UW(f, 6) & 0x20) && L(f, 8) != 0;
}

static int auction_series_ok(int16_t n)
{
    uint8_t *s = SERIES(n);
    return (UW(s, 0x25) & 0x40) && (s[0x24] == 0 || (int8_t)s[0x24] > 3) && (UW(s, 0x25) & 0x20);
}

/* 3264:0717 (Release 34b8:0905) – Auktion zum Tagesende: Zuschläge, 3 Tage ohne Gebot = Rückzug,
 * neue Lose */
void auction_day_end(void)
{
    /* Release-Fassung: Kandidaten vorher zählen und höchstens so viele Lose neu füllen. Die Beta
     * suchte endlos weiter, wenn kein Film und keine Serie mehr zu versteigern war. */
    int16_t avail = 0;
    for (int16_t n = 1; n <= 200; n++)
        if (auction_film_ok(n))
            avail++;
    for (int16_t n = 1; n <= 50; n++)
        if (auction_series_ok(n))
            avail++;
    for (int16_t i = 1; i <= 10; i++) {
        uint8_t *l = LOT(i);
        if (W(l, 0) != 0) {
            if (l[6] == 0) {
                if ((int8_t)++l[8] < 3)
                    goto next;
                /* Release-Fassung: unverkauft zurück auf den Markt (auction_lot_set hatte einen
                 * Schein-Besitzer eingetragen und Bit 0x20 gelöscht; die Beta ließ das so stehen,
                 * der Titel war damit für den Rest des Spiels verloren) */
                int16_t id = W(l, 0);
                if (id != 0 && id < 1000) {
                    FILM(id)[5] = 0;
                    UW(FILM(id), 6) |= 0x20;
                } else {
                    SERIES(id - 1000)[0x24] = 0;
                    UW(SERIES(id - 1000), 0x25) |= 0x20;
                }
            } else {
                if (W(l, 0) < 0x3e9)
                    UW(FILM(W(l, 0)), 6) |= 0x20;
                else
                    UW(SERIES(W(l, 0) - 1000), 0x25) |= 0x20;
                programme_buy(W(l, 0), (int8_t)l[6], L(l, 2));
            }
            W(l, 0) = 0;
        }
    next:
        if (i < 9 && W(l, 0) == 0 && avail > 0) {
            avail--;
            int16_t n = 0;
            while (n == 0) {
                n = rand_range(0xfa, i);
                if (n > 200) {
                    n -= 200;
                    if (!auction_series_ok(n)) {
                        n = 0;
                        continue;
                    }
                    W(SERIES(n), 0x31) = 1;
                    auction_lot_set(i, n, 1);
                } else {
                    if (!auction_film_ok(n)) {
                        n = 0;
                        continue;
                    }
                    auction_lot_set(i, n, 0);
                }
            }
        }
    }
}

static int32_t viewers_from_rating(int16_t p)
{
    int32_t v = (int32_t)((uint32_t)STATION_REACH(p) * (uint32_t)(int32_t)RATING(p)) / 100;
    return (int32_t)((uint32_t)v * 0x9a0u);
}

/* 3264:08ce – Quoten der laufenden Stunde: 1 = Nachrichten (Minute 0), 2 = Programm (Minute 5),
 * 3 = Werbung (Minute 55) */
void ratings_calc(int16_t phase)
{
    int16_t once[4] = {0, 0, 0, 0}; /* Kopie von DS:12d4: einmalig gesendete Filme */
    int16_t h = hour_index(), p, q, i, k, sum, total;
    if (phase == 1) {
        for (p = 1; p < 4; p++)
            newscast_rate(p);
        sum = newscast_quality_sum();
        total = 0;
        for (p = 1; p < 4; p++) {
            if (PLAYER_OUT(p) != 0)
                continue;
            /* Release-Fassung: ohne die Anhebung über rating_from_viewers(p, DS:6da5) */
            total += DS_I8(0x3f2b + p);
        }
        for (p = 1; p < 4; p++) {
            if (PLAYER_OUT(p) == 0 && total != 0) {
                DS_I8(0x3f2b + p) = (int8_t)((int16_t)(DS_I8(0x3f2b + p) * DS_I8(0x1ce0 + h)) / 100);
                RATING(p) = (int16_t)((int16_t)(DS_I8(0x3f2b + p) * sum) / total);
                VIEWERS(p) = viewers_from_rating(p);
            } else {
                RATING(p) = 0;
                VIEWERS(p) = 0;
            }
        }
        for (p = 1; p < 4; p++) {
            if (PLAYER_OUT(p) != 0)
                continue;
            DS_I16(0x2493 + p * 10) = DS_I16(0x248f + p * 10);
            DS_I32(0x7690 + p * 0x20 + h * 4) = VIEWERS(p);
            if (DS_I16(0x7468) == 1 && contest_tick() == 1)
                DS_I16(0x7468 + p * 2) += RATING(p);
        }
    } else if (phase == 2) {
        for (p = 1; p < 4; p++) {
            int16_t *val = &CHAN_VALUE(p);
            if (PLAYER_OUT(p) == 0) {
                once[p] = 0;
                if (SCHEDULE(p, h) == 0) {
                    *val = 0;
                } else {
                    int16_t hours = 0, hh = h;
                    while (hours++, SCHEDULE(p, hh) == 9999)
                        hh--;
                    int16_t prog = SCHEDULE(p, hh);
                    if (prog < 1000) {
                        uint8_t *f = FILM(prog);
                        *val = (int8_t)f[3];
                        if (p == g_local_player)
                            DS_I16(0x79f4) = *val;
                        GENRE_NOW(p) = (int8_t)f[0];
                        if (((UW(f, 7) & 4) || f[0] == 7) && (int8_t)f[4] == hours) {
                            if (prog == 0x1d)
                                special_film_1d_tick();
                            else if (prog == 0xbb)
                                special_film_bb_tick();
                            else
                                once[p] = prog;
                        }
                        if (prog == 0x3a) /* Release-Fassung: Film 0x3a immer einmalig */
                            once[p] = 0x3a;
                        if ((UW(f, 7) & 8) && h < 4) { /* Skandalfilm vor 21 Uhr */
                            DS_I16(0x2495 + p * 10) = 1;
                            if (DS_I16(0x7a68) == 1) {
                                DS_I16(0x7a68) = 2;
                                DS_I16(0x7a3e) = p;
                            }
                        } else {
                            DS_I16(0x2495 + p * 10) = 0;
                        }
                    } else {
                        uint8_t *s = SERIES(prog - 1000);
                        GENRE_NOW(p) = (int8_t)(s[0x21] + 10);
                        /* Release-Fassung: Folge mindestens 1, im Sendeplan hinter den Stunden
                         * vermerken */
                        if (W(s, 0x31) == 0)
                            W(s, 0x31) = 1;
                        DS_I16(0x43f7 + p * 0x5a + h * 2 + 0x1e) = W(s, 0x31);
                        *val = (int8_t)s[W(s, 0x31) * 3 + 1];
                        if (p == g_local_player)
                            DS_I16(0x79f4) = *val;
                        int16_t runs = 0;
                        for (int16_t x = h; x > 0; x--)
                            if (SCHEDULE(p, x) == prog)
                                runs++;
                        *val = (int16_t)(*val / runs);
                    }
                }
            }
            int16_t hit = 0;
            for (i = 0; i < 3; i++) {
                if (TOPIC_GENRE(i) == GENRE_NOW(p)) { /* Thema des Tages */
                    hit = -1;
                    *val = (int16_t)((uint16_t)(TOPIC_BONUS(i) * (uint16_t)*val) / 10u);
                    for (k = 0; k < 2; k++)
                        DS_I16(0x7888 + k * 6 + i * 2) = GENRE_NOW(p);
                }
            }
            if (hit == 0)
                for (k = 0; k < 2; k++)
                    for (i = 0; i < 3; i++)
                        if (DS_I16(0x7888 + k * 6 + i * 2) == GENRE_NOW(p))
                            DS_I16(0x7888 + k * 6 + i * 2) = 0;
        }
        for (p = 1; p < 4; p++) { /* Zuschauer eines Genres zur Stunde begrenzt (DS:7140) */
            if (PLAYER_OUT(p) != 0 || GENRE_NOW(p) == 0)
                continue;
            int16_t *val = &CHAN_VALUE(p);
            total = 0;
            for (q = 1; q < 4; q++)
                if (PLAYER_OUT(q) == 0 && GENRE_NOW(p) == GENRE_NOW(q))
                    total += CHAN_VALUE(q);
            int16_t cap = DS_U8(0x7140 + GENRE_NOW(p) * 8 + h);
            if (cap < total)
                *val = (int16_t)((int16_t)(*val * cap) / total);
            *val = (int16_t)((int16_t)(*val * cap) / 100);
            if (*val != 0)
                *val += DS_I8(0x3f2b + p) / 5;
        }
        sum = channel_value_sum();
        total = 0;
        for (p = 1; p < 4; p++)
            total += PLAYER_OUT(p) == 0 ? CHAN_VALUE(p) : 0;
        for (p = 1; p < 4; p++) {
            if (total == 0) {
                RATING(p) = 0;
                VIEWERS(p) = 0;
            } else {
                RATING(p) = (int16_t)(CHAN_VALUE(p) * sum / total);
                VIEWERS(p) = viewers_from_rating(p);
            }
        }
        for (p = 1; p < 4; p++) {
            DS_I16(0x2493 + p * 10) = RATING(p);
            if (DS_I16(0x7468) == 2 && GENRE_NOW(p) == 6 && contest_tick() == 1)
                DS_I16(0x7468 + p * 2) += 10;
        }
        for (p = 1; p < 4; p++) {
            if (SCHEDULE(p, h) == 0x3a) {
                PLAYER_METER(p) += 5;
                if (PLAYER_IMG(p) < PLAYER_METER(p))
                    PLAYER_METER(p) = PLAYER_IMG(p);
                status_meter_draw();
            }
            DS_I32(0x74ca + p * 0x20 + h * 4) = VIEWERS(p);
            DS_I16(0x1c68 + p * 0x1e + (h + 7) * 2) = RATING(p);
        }
        for (p = 1; p < 4; p++) {
            if (once[p] == 0)
                continue;
            programme_remove_from_archives(once[p]);
            for (int16_t s = 1; s < 0xd; s++) {
                uint8_t *st = STUDIO(s);
                if (st[0xe] != 0 && (once[p] == W(SHOW(W(st, 0)), 6) || once[p] == 0x8a)) {
                    st[0xe] = 0;
                    if ((int8_t)st[0xf] == -1)
                        studio_release(s);
                    else if ((int8_t)st[0xf] > 0)
                        studio_rent(p, (int8_t)st[0xf]); /* ORIGINAL-BUG: Spieler statt Studioplatz */
                }
            }
            if (once[p] == 0x8a)
                DS_I16(0x12d2) = p;
            if (g_cur_room == 0x1f && DS_I16(0x7a16) == p && g_cur_dialog == 2) {
                archive_list_dialog(0x13, 8, 1, 0);
                archive_list_dialog(0x13, 8, 0, DS_I16(0x7a16));
                screen_update();
            }
        }
        ratings_rank_update();
    } else if (phase == 3) {
        if (DS_I16(0x12d2) != 0) {
            if (DS_I16(0x12d2) == g_local_player) {
                player_message_push(0x15f, DS_I16(0x12d2), DS_I16(0x98), DS_I16(0x9c), DS_I16(0x94), 0, 0);
            } else {
                for (i = 0; i < 2 && DS_I8(0xf2 + i) != DS_I16(0x12d2); i++)
                    ;
                DS_I16(0x12d2) = DS_I8(0xf2 + i);
                for (i = 0; i < 3; i++)
                    DS_I16(0x7888 + DS_I16(0x12d2) * 6 + i * 2) = TOPIC_GENRE(i);
            }
            DS_I16(0x12d2) = 0;
        }
        for (p = 1; p < 4; p++) {
            int16_t ad = AD_SCHEDULE(p, h);
            if (PLAYER_OUT(p) != 0 || ad == 0)
                continue;
            uint8_t *a = AD(ad);
            int16_t c = 0, ok = 0;
            while (c < AD_CONTRACT_COUNT(p) && W(AD_CONTRACT(p, c), 0) != ad)
                c++;
            if (a[0xa] == 1) {
                ok = SCHEDULE(p, h) != 0;
            } else {
                int32_t v = round_to_step(VIEWERS(p), 100000); /* Release-Fassung (Beta: 10000) */
                ok = !(v < REACH_LEVEL((int8_t)a[0xa] - 1)); /* Mindestzuschauer erreicht? */
            }
            if (ok)
                ok = ad_genre_allowed(ad, GENRE_NOW(p));
            if (ok == 1) {
                uint8_t *ct = AD_CONTRACT(p, c);
                if ((int8_t)++ct[4] < (int8_t)a[9]) {
                    AD_SCHEDULE(p, h) += 3000; /* Spot gesendet */
                } else { /* Vertrag erfüllt */
                    PLAYER_MONEY(p) += L(a, 0x10);
                    if (p == g_local_player)
                        money_add(0xe, L(a, 0x10));
                    else
                        ai_adjust((int8_t)p, L(a, 0x10), c);
                    status_money_update();
                    AD_SCHEDULE(p, h) += 2000;
                    for (k = h + 1; k < 0xf; k++)
                        if (AD_SCHEDULE(p, k) == W(ct, 0))
                            AD_SCHEDULE(p, k) = 0;
                    a[0xf] = 0;
                    list_remove(p, c);
                    if (p == g_local_player && g_cur_room == 0x1f && g_cur_dialog == 6 && ad == DS_I16(0x79b6)) {
                        cursor_drop_item();
                        g_cur_dialog = 0;
                    }
                    if (p == DS_I16(0x7a16) && g_cur_room == 0x1f && g_cur_dialog == 5) {
                        ad_contract_list_dialog(0x16, 1, 1, DS_I16(0x7a16));
                        ad_contract_list_dialog(0x16, 1, 0, DS_I16(0x7a16));
                        screen_update();
                    }
                }
            } else {
                AD_SCHEDULE(p, h) += 1000; /* Spot nicht gesendet */
                if (DS_I8(0xf2) == p || DS_I8(0xf3) == p)
                    ai_ad_spot_count(h, c, 0, (int8_t)p);
            }
        }
    }
}

/* 3264:1759 – Thema des Tages wählen (nicht Genre 10/15, nicht in den letzten 3 Tagen) */
void daily_topic_pick(void)
{
    int16_t again = -1, t = 0, i;
    for (i = 0; i < 3; i++)
        TOPIC_GENRE(i) = TOPIC_GENRE(i + 1);
    while (again != 0) {
        again = 0;
        t = rand_range(0xf, 0);
        for (i = 0; i < 3; i++)
            if (TOPIC_GENRE(i) == t)
                again = -1;
        if (t == 10 || t == 15)
            again = -1;
    }
    DS_I16(0xa0) = t;
}

/* 3264:17e2 – Programm, das zur Stunde läuft (Fortsetzungen 9999 zurückverfolgen) */
int16_t schedule_get_programme(int16_t player, int16_t hour)
{
    while (SCHEDULE(player, hour) == 9999)
        hour--;
    return SCHEDULE(player, hour);
}

/* 3264:1825 – Name aus der Namenstabelle far DS:0110 (Offsets relativ zum Tabellenanfang) */
char *name_table_get(int16_t id, int16_t part)
{
    uint8_t *base = DS_PTR(uint8_t, 0x110);
    int16_t ofs = *(int16_t *)(base + (id - 0x98) * 4 - part * 2);
    return ofs == 0 ? DS_ADDR(char, 0x12dc) : (char *)(base + ofs);
}

/* 3264:185a – Tagesprüfung: leere Sendeplätze zählen, Schwächsten stützen, Ausscheiden bei Image < 5 */
void day_check_players(void)
{
    int16_t p, low = 100, who = 0;
    DS_I16(0x79e2) = 0;
    DS_I16(0x79e4) = 0;
    for (int16_t h = 1; h < 8; h++) {
        if (SCHEDULE(g_local_player, h) == 0)
            DS_I16(0x79e2)++;
        if (AD_SCHEDULE(g_local_player, h) == 0)
            DS_I16(0x79e4)++;
    }
    for (p = 1; p < 4; p++) {
        if (PLAYER_IMG(p) <= low) {
            low = PLAYER_IMG(p);
            who = p;
        }
    }
    if (who != 0 && who != g_local_player && g_day > 1 && g_day < 0x28)
        ai_adjust(who, 100000, -1); /* schwächste KI bekommt Geld */
    DS_I16(0x7a34) = 0;
    for (p = 1; p < 4; p++)
        if (PLAYER_IMG(p) < 5 && PLAYER_OUT(p) == 0)
            DS_I16(0x7a34) = p;
    if (PLAYER_IMG(g_local_player) < 5)
        DS_I16(0x7a34) = g_local_player;
    int16_t out = DS_I16(0x7a34);
    if (out != 0 && out != g_local_player && PLAYER_IMG(out) > 0) {
        int16_t active = 0;
        for (p = 1; p < 4; p++)
            if (PLAYER_OUT(p) == 0)
                active++;
        if (active == 3) {
            int16_t img = PLAYER_IMG(out), half = img >> 1;
            PLAYER_IMG(g_local_player) += half;
            for (p = 1; p < 4; p++) {
                if (p != out && p != g_local_player) {
                    PLAYER_IMG(p) += img - half;
                    break;
                }
            }
        } else {
            PLAYER_IMG(g_local_player) += PLAYER_IMG(out);
        }
        PLAYER_IMG(out) = 0;
    }
}
