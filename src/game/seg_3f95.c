/* Segment 3f95 – KI: Senderangebote, Nachrichten, Quotenschätzung, Werbe-/Programmplanung,
 * Geschenke, Wegwahl in den Aufzugsplan. */
#include "game.h"

#define AI_IDX            DS_I8(0x24e7)
#define AI_PLAYER         DS_I8(0xf2 + DS_I8(0x24e7))
#define BUDGET(p)         DS_ADDR(uint8_t, 0x6e09 + (p) * 0x3c)
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_OUT(p)     DS_U8(0x8bd0 + (p) * 0x17)
#define PLAYER_IMG(p)     DS_I16(0x8bd1 + (p) * 0x17)
#define PLAYER_METER(p)   DS_I16(0x8bc8 + (p) * 0x17)
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define AD_SCHEDULE(p, h) DS_I16(0x4433 + (p) * 0x5a + (h) * 2)
#define ARCHIVE(p, g, k)  DS_I16(0x6ef9 + (p) * 0x6c + (g) * 8 + (k) * 2)
#define ARCHIVE_COUNT(p, g) DS_I8(0x6f59 + (p) * 0x6c + (g))
#define AD(n)             DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)
#define AD_NEED(n)        DS_I32(0xa8 + ((int8_t)AD(n)[0xa] - 1) * 4)
#define AD_CONTRACT_ID(p, i) DS_I16(0x1b50 + (p) * 0x46 + (i) * 7)
#define AD_CONTRACT_COUNT(p) DS_I16(0x7688 + (p) * 2)
#define AI_CONTRACT(p, i) DS_ADDR(int8_t, 0x4ebf + (p) * 0x4d + (i) * 7)
#define STATIONS(p)       DS_ADDR(uint8_t, 0x1ce9 + (p) * 0x1ce)
#define NEWS_ARCH(p, i)   DS_ADDR(uint8_t, 0x455f + (p) * 600 + (i) * 0x1e)
#define NEWS_COUNT(p)     DS_I16(0x7286 + (p) * 2)
#define NEWSCAST(p, k)    DS_ADDR(uint8_t, 0x3d4b + (p) * 0x78 + (k) * 0x1e)
#define FLOOR_B(f, ofs)   DS_U8(0x7c62 + (f) * 0x7a + (ofs))
#define ELEV_PLAN(i, k)   DS_ADDR(int8_t, 0x8a40 + (i) * 8 + (k) * 2) /* 13 Haltestellen x 4 (Etage, Tür) */
#define GIFT(g)           DS_ADDR(uint8_t, 0x722c + (g) * 8)         /* +0 Preis, +4 Wert, +5 Zähler */

static int16_t contract_index(int8_t p, int16_t ad)
{
    int16_t c = 0;
    while (c < AD_CONTRACT_COUNT(p) && AD_CONTRACT_ID(p, c) != ad)
        c++;
    return c;
}

/* 3f95:0000 – Preisangebot der KI für einen Senderstandort (zufällig 15..45 % des Stufenpreises) */
int32_t ai_calc_bid(int16_t ai_idx, int16_t x, int16_t y)
{
    static const int8_t pct[4] = {0x0f, 0x1e, 0x28, 0x2d}; /* Kopie von DS:15f6 */
    int8_t p = DS_I8(0xf2 + ai_idx);
    coverage_map_build(p);
    int32_t price = price_step_next(station_reach_gain(x, y) * 0x268 / 10);
    int16_t i = 3;
    while (L(STATIONS(p) + i * 0xe, 4) != 0 && i < 0x21)
        i++;
    if (i <= 0x20) {
        int32_t bid = price * pct[rand_range(4, 1) - 1] / 100;
        if (bid <= L(BUDGET(p), 0x14))
            return bid;
    }
    return -1;
}

/* 3f95:0104 – bessere Nachrichten aus dem Archiv in die Sendung übernehmen (max. 3 pro Besuch) */
void ai_buy_news(void)
{
    int8_t p = AI_PLAYER;
    int16_t bought = 0;
    int16_t need = DS_I16(0x15fa + (int8_t)BUDGET(p)[0x2b] * 2);
    while (L(BUDGET(p), 0xc) > 0 && !ai_news_quality_ok(p, need) && NEWS_COUNT(p) >= 1 && bought <= 2) {
        int16_t worst = 0, worst_v = 0x96, best = -1, best_v = 0;
        for (int16_t k = 1; k < 4; k++) {
            if (W(NEWSCAST(p, k), 0x16) < worst_v) {
                worst_v = W(NEWSCAST(p, k), 0x16);
                worst = k;
            }
        }
        for (int16_t i = 0; i < NEWS_COUNT(p); i++) {
            int32_t price = genre_value_get(DS_I8(0x457c + p * 600 + i * 0x1e));
            if (price < L(BUDGET(p), 0xc) && best_v < W(NEWS_ARCH(p, i), 0x16)) {
                best_v = W(NEWS_ARCH(p, i), 0x16);
                best = i;
            }
        }
        if (worst == 0 || best < 0 || best_v <= worst_v)
            break;
        L(BUDGET(p), 0xc) -= genre_value_get(DS_I8(0x457c + p * 600 + best * 0x1e));
        far_memcpy(NEWSCAST(p, worst), NEWS_ARCH(p, best), 0x1e);
        player_news_remove(p, best);
        bought++;
    }
    newscast_rate(p);
}

/* 3f95:0332 – reicht die mittlere Nachrichtenqualität? */
int16_t ai_news_quality_ok(int16_t player, int16_t threshold)
{
    int16_t sum = 0;
    for (int16_t k = 1; k < 4; k++)
        sum += W(NEWSCAST(player, k), 0x16);
    return threshold <= sum / 3;
}

/* 3f95:037d – Quote der KI zur Stunde schätzen, wenn sie prog sendet (0 = Plan).
 * mode 0: Quote, 1: Zuschauer, 2: Rang unter den Sendern (1..3). Nachbildung von ratings_calc. */
int32_t ai_rate_slot(int16_t hour, int16_t prog_id, int8_t mode)
{
    int8_t me = AI_PLAYER;
    int16_t lo = hour > 7 ? 8 : 1, hh = hour > 7 ? hour - 7 : hour;
    int16_t rate[4] = {0}, q[4] = {0}, g[4] = {0}, p, k;
    struct { int16_t rating; int32_t viewers; int16_t copy; } r[4] = {{0}};
    if (hour_index() == hour && prog_id == 0 && g_minute >= 5) /* läuft gerade: echte Werte */
        return mode == 0 ? DS_I16(0x248d + me * 10) : DS_I32(0x248f + me * 10);
    for (p = 1; p <= 3; p++) {
        int16_t prog;
        /* rate[p] wird nur unten für Stunden <= 7 gesetzt. PORT: Das Original lässt rate[] sonst
         * uninitialisiert (Stapelinhalt), hier 0. */
        if (p == me && prog_id != 0) {
            prog = prog_id;
        } else {
            int16_t s = hour;
            while (SCHEDULE(p, s) == 9999)
                s--;
            prog = SCHEDULE(p, s);
        }
        if (prog == 0) {
            q[p] = 0;
            g[p] = 0;
        } else if (prog > 0 && prog < 2000) {
            if (prog < 1000) {
                q[p] = (int8_t)FILM(prog)[3];
                g[p] = (int8_t)FILM(prog)[0];
                if (p == me && (UW(FILM(prog), 7) & 8) && hour % 7 < 4)
                    q[p] = 0; /* Skandalfilm vor 21 Uhr */
            } else {
                uint8_t *s = SERIES(prog - 1000);
                q[p] = (int8_t)s[W(s, 0x31) * 3 + 1];
                g[p] = (int8_t)s[0x21] + 10;
                int16_t runs = prog == prog_id ? 1 : 0;
                for (int16_t x = hour; x >= lo; x--)
                    if (SCHEDULE(p, x) == prog)
                        runs++;
                if (runs != 0) /* im Original Division durch 0 möglich */
                    q[p] = (int16_t)(q[p] / runs);
            }
        }
        for (k = 0; k < 3; k++)
            if (DS_I16(0x7888 + AI_IDX * 6 + k * 2) == g[p])
                q[p] = (int16_t)((uint16_t)(q[p] * DS_U16(0x96 + k * 4)) / 10u);
    }
    for (p = 1; p <= 3; p++) {
        if (g[p] == 0) {
            q[p] = 0;
            continue;
        }
        int16_t total = 0;
        for (k = 1; k <= 3; k++)
            if (g[p] == g[k])
                total += q[k];
        int16_t cap = DS_U8(0x7140 + g[p] * 8 + hh);
        if (cap < total)
            q[p] = (int16_t)((int16_t)(cap * q[p]) / total);
        q[p] = (int16_t)((int16_t)(q[p] * cap) / 100);
        if (hour <= 7 && q[p] != 0)
            rate[p] = ai_genre_share(p, hour);
        if (q[p] != 0)
            q[p] += rate[p] / 5;
    }
    int16_t capped = 0, sum = 0;
    for (p = 1; p <= 3; p++) {
        capped += q[p];
        sum += q[p];
    }
    if (capped > 100)
        capped = 100;
    for (p = 1; p <= 3; p++) {
        if (sum != 0) {
            r[p].rating = (int16_t)((int16_t)(q[p] * capped) / sum);
            int32_t v = (int32_t)((uint32_t)DS_I32(0x1ced + p * 0x1ce) * (uint32_t)(int32_t)r[p].rating) / 100;
            r[p].viewers = (int32_t)((uint32_t)v * 0x9a0u);
        } else {
            r[p].rating = 0;
            r[p].viewers = 0;
        }
    }
    for (p = 1; p <= 3; p++)
        r[p].copy = r[p].rating;
    if (mode == 1)
        return r[me].viewers;
    if (mode == 2) {
        int16_t rank = 1, out = 0;
        if (r[1].rating == 0 && r[2].rating == 0 && r[3].rating == 0)
            return 3;
        for (p = 1; p <= 3; p++) {
            if (PLAYER_OUT(p) == 0 && me != p && r[p].rating >= r[me].rating)
                rank++;
            if (PLAYER_OUT(p) != 0)
                out++;
        }
        if (rank == 2 && out == 1)
            rank = 3;
        return rank;
    }
    return r[me].copy;
}

/* 3f95:0a77 – geschätzter Zuschaueranteil (Nachrichtenqualität von p, fällt pro Stunde auf 85 %) */
int16_t ai_genre_share(int16_t genre, int16_t hour)
{
    int16_t v = DS_I8(0x3f2b + genre);
    for (int16_t h = hour_index(); h < hour; h++)
        v = (int16_t)(v * 0x55 / 100);
    return (int16_t)(DS_I8(0x1ce0 + hour) * v / 100);
}

/* 3f95:0ab8 – Werbeplan umsortieren: Lücken füllen, anspruchsvolle Spots in bessere Stunden;
 * am Stichtag fällige Verträge zuletzt noch platzieren */
void ai_schedule_ads(void)
{
    int8_t p = AI_PLAYER;
    for (int16_t h = hour_index_after_min55(); h < 0xf; h++) {
        int16_t cur = AD_SCHEDULE(p, h);
        if (cur == 0) { /* späteren Spot nach vorn holen */
            int16_t ad = 0, k = h + 1;
            for (; ad == 0 && k < 0xf; k++) {
                ad = AD_SCHEDULE(p, k);
                if (ai_ad_target_ok(h, ad, 0) == 0 || ai_rate_slot(h, 0, 1) < AD_NEED(ad))
                    ad = 0;
                if (ad != 0)
                    break;
            }
            if (ad != 0) {
                int16_t c = contract_index(p, ad);
                ai_ad_spot_count(c, k, 0, p); /* ORIGINAL-BUG: Stunde und Vertrag vertauscht */
                ai_ad_spot_count(c, h, 1, p);
                AD_SCHEDULE(p, h) = ad;
                AD_SCHEDULE(p, k) = 0;
            }
            continue;
        }
        int16_t prime = h > 7;
        int16_t c = contract_index(p, cur);
        if (c >= AD_CONTRACT_COUNT(p))
            continue;
        int16_t lim = 0xe;
        if (AI_CONTRACT(p, c)[prime] < AI_CONTRACT(p, c)[prime + 2])
            lim = (int16_t)(prime * 7 + 7);
        int16_t pick = 0, pick_h = 0;
        int32_t best = AD_NEED(cur), rate = ai_rate_slot(h, 0, 1);
        for (int16_t k = h; k <= lim; k++) {
            int16_t ad = AD_SCHEDULE(p, k);
            if (ad == 0)
                continue;
            int32_t need = AD_NEED(ad);
            if (need < rate && ai_ad_target_ok(h, ad, 0) && need > best && ai_ad_target_ok(k, cur, 0)) {
                pick = ad;
                best = need;
                pick_h = k;
            }
        }
        if (pick == 0)
            continue;
        ai_ad_spot_count(c, h, 0, p); /* ORIGINAL-BUG: vertauscht wie oben */
        ai_ad_spot_count(c, pick_h, 1, p);
        int16_t c2 = contract_index(p, pick);
        ai_ad_spot_count(c2, pick_h, 0, p);
        ai_ad_spot_count(c2, h, 1, p);
        AD_SCHEDULE(p, pick_h) = cur;
        AD_SCHEDULE(p, h) = pick;
    }
    for (int16_t c = 0; c < AD_CONTRACT_COUNT(p); c++) {
        int16_t due = DS_I16(0x1b55 + p * 0x46 + c * 7);
        if (due > g_day + 1 || (UW(AI_CONTRACT(p, c), 6) & 4))
            continue;
        for (int16_t mode = 1; mode <= 2; mode++)
            ai_place_ad_spots(p, AD_CONTRACT_ID(p, c), 1, due == g_day ? 7 : 0xe, mode, c);
    }
}

/* 3f95:0f3d – restliche Spots eines Vertrags verteilen: mode 1 freie Plätze, 2 fremde Spots ersetzen */
void ai_place_ad_spots(int16_t player, int16_t advertiser_id, int16_t from_hour, int16_t to_hour, int16_t mode,
                       int16_t contract_idx)
{
    int16_t left = (int8_t)AD(advertiser_id)[9] - AI_CONTRACT(player, contract_idx)[4];
    int32_t need = AD_NEED(advertiser_id);
    for (int16_t h = from_hour; h <= to_hour && left >= 1; h++) {
        int16_t place = 0;
        if (mode == 1 && AD_SCHEDULE(player, h) == 0 && ai_rate_slot(h, 0, 1) > need &&
            ai_ad_target_ok(h, advertiser_id, 0))
            place = 1;
        if (mode == 2) {
            int16_t other = AD_SCHEDULE(player, h);
            if (other != 0 && ai_rate_slot(h, 0, 1) > need && ai_ad_target_ok(h, advertiser_id, 0)) {
                int16_t c2 = contract_index((int8_t)player, other);
                if (c2 < AD_CONTRACT_COUNT(player) && !(UW(AI_CONTRACT(player, c2), 6) & 4)) {
                    place = 1;
                    ai_ad_spot_count(h, c2, 0, (int8_t)player);
                }
            }
        }
        if (place) {
            AD_SCHEDULE(player, h) = advertiser_id;
            ai_ad_spot_count(h, contract_idx, 1, (int8_t)player);
            left--;
        }
    }
}

/* 3f95:1131 – Geschenk mit dem besten Preis/Wirkung-Verhältnis aussuchen und bezahlen */
void ai_choose_gift(void)
{
    int8_t p = AI_PLAYER;
    int32_t best = 0;
    int16_t pick = 0;
    if (L(BUDGET(p), 4) <= 30000)
        return;
    for (int16_t g = 9; g > 2; g--) {
        int32_t price = L(GIFT(g), 0);
        int8_t value = (int8_t)GIFT(g)[4];
        if (price < L(BUDGET(p), 4) && value != 0 && price / value > best) {
            best = price / value;
            pick = g;
        }
    }
    if (pick != 0) {
        L(BUDGET(p), 4) -= L(GIFT(pick), 0);
        DS_I8(0x789c + AI_IDX) = (int8_t)pick;
    }
}

/* 3f95:12a1 – Geschenk übergeben: Image-Anzeige steigt, Wirkung des Geschenks sinkt auf 70 % */
void ai_give_gift(void)
{
    int8_t p = AI_PLAYER;
    int16_t g = DS_I8(0x789c + AI_IDX);
    if (g == 0)
        return;
    PLAYER_METER(p) += (int8_t)GIFT(g)[4];
    GIFT(g)[4] = (uint8_t)((int8_t)GIFT(g)[4] * 0x46 / 100);
    GIFT(g)[5]++;
    DS_I8(0x789c + AI_IDX) = 0;
    if (PLAYER_IMG(p) < PLAYER_METER(p))
        PLAYER_METER(p) = PLAYER_IMG(p);
}

/* 3f95:1390 – eingeplante Sendungen gegen Archivprogramme gleicher Länge tauschen, die einen
 * besseren Rang bringen (Durchläufe je nach Schwierigkeit DS:7a74) */
void ai_schedule_programmes(void)
{
    int8_t p = AI_PLAYER;
    int16_t h = DS_I16(0x24c7 + AI_IDX * 2);
    if (hour_index_after_min5() > h)
        return;
    int16_t thr = 2;
    for (int16_t pass = 2; pass <= DS_I16(0x7a74); pass++, thr--) {
        int16_t lim = h <= 7 ? 7 : 0xe;
        int16_t prog = SCHEDULE(p, h);
        if (prog == 9999)
            continue;
        int16_t run = 1;
        while (SCHEDULE(p, h + run) == (prog != 0 ? 9999 : 0) && h + run <= lim)
            run++;
        int16_t good = 0;
        for (int16_t x = h; x < h + run; x++)
            if (ai_rate_slot(x, 0, 2) <= thr)
                good++;
        if (good >= run)
            continue;
        int16_t best = 0, best_g = 0, best_k = 0, empty = 0, slot_len;
        if (prog == 0) {
            int16_t x = h;
            while (SCHEDULE(p, h) == 0 && x <= lim) /* ORIGINAL-BUG: prüft immer Stunde h */
                x++;
            slot_len = x - h + 1;
            empty = 1;
        } else if (prog < 1000) {
            slot_len = (int8_t)FILM(prog)[4];
        } else {
            slot_len = (int8_t)SERIES(prog - 1000)[0x23];
        }
        for (int16_t gi = 1; gi <= 0xb; gi++) {
            for (int16_t k = 0; k < ARCHIVE_COUNT(p, gi); k++) {
                int16_t cand = ARCHIVE(p, gi, k);
                if (gi <= 10 ? (UW(FILM(cand), 7) & 2) != 0 : (UW(SERIES(cand), 0x26) & 2) != 0)
                    continue;
                if (gi == 7)
                    continue;
                if (gi != 0xb && (UW(FILM(cand), 7) & 8) && h % 7 <= 3)
                    continue;
                int16_t genre = gi <= 10 ? (int8_t)FILM(cand)[0] : (int8_t)SERIES(cand)[0x21] + 10;
                int16_t len = gi <= 10 ? (int8_t)FILM(cand)[4] : (int8_t)SERIES(cand)[0x23];
                /* ORIGINAL: leere Lücken werden nie gefüllt, nur gleich lange Sendungen getauscht */
                if (empty || len != slot_len)
                    continue;
                int16_t ok = 1;
                for (int16_t x = h; x < h + len; x++) {
                    int16_t ad = AD_SCHEDULE(p, x);
                    if (ad != 0) {
                        if (ad_genre_allowed(ad, genre) == 0)
                            ok = 0;
                        break;
                    }
                }
                if (!ok)
                    continue;
                if (gi == 0xb)
                    cand += 1000;
                int16_t cnt = 0;
                for (int16_t x = h; x < h + run; x++)
                    if (ai_rate_slot(x, cand, 2) <= thr)
                        cnt++;
                if (cnt > good) {
                    best = cand;
                    good = cnt;
                    best_g = gi;
                    best_k = k;
                }
            }
        }
        if (best != 0) {
            schedule_place_programme(best_k, best_g, h, p);
            programmes_mark_scheduled();
        }
    }
}

/* 3f95:174b – zufälliges Ziel (Raum des Menschen o. ä.) mit der Haltestelle der Ereignisfigur
 * im Aufzugsplan DS:8a40 vertauschen */
void ai_pick_destination(void)
{
    int16_t floor = 0, door = 0, efloor = 0, edoor = 0;
    int16_t r = rand_range(10, 1);
    if (r >= 1 && r <= 5) {
        int16_t tries = 0; /* höchstens 20 Versuche */
        do {
            floor = 0;
            building_find_door((uint16_t)(rand_range(9, 1) + 0xf), 0, 0, &floor, &door);
            tries++;
        } while (floor == 0 && tries < 20);
    } else if (r == 6 || r == 7) {
        building_find_door(4, (uint16_t)g_local_player, 0, &floor, &door);
    } else if (r == 8 || r == 9) {
        floor = 0;
        for (int16_t i = 1; i < 0xd; i++)
            if (DS_I8(0x6148 + i * 0x10) == g_local_player && DS_I16(0x6145 + i * 0x10) != 0)
                building_find_door(5, (uint16_t)g_local_player, (uint16_t)i, &floor, &door);
        if (floor == 0)
            for (int16_t i = 1; i < 0xd; i++)
                if (DS_I8(0x6148 + i * 0x10) == g_local_player)
                    building_find_door(5, (uint16_t)g_local_player, (uint16_t)i, &floor, &door);
    } else if (r == 10) {
        building_find_door(2, (uint16_t)g_local_player, 0, &floor, &door);
    }
    if (floor == 0)
        return;
    int16_t n = -1, k = 0;
    while (n < door) {
        if (ELEV_PLAN(floor - 1, k)[0] != 0)
            n++;
        k++;
    }
    k--;
    find_event_door(&efloor, &edoor);
    int16_t n2 = -1, k2 = 0;
    while (n2 < edoor) {
        if (ELEV_PLAN(efloor - 1, k2)[0] != 0)
            n2++;
        k2++;
    }
    k2--;
    if (efloor != 0 && (efloor != floor || edoor != door)) {
        /* die beiden Felder tauschen */
        int8_t f0 = ELEV_PLAN(floor - 1, k)[0], d0 = ELEV_PLAN(floor - 1, k)[1];
        ELEV_PLAN(floor - 1, k)[0] = ELEV_PLAN(efloor - 1, k2)[0];
        ELEV_PLAN(floor - 1, k)[1] = ELEV_PLAN(efloor - 1, k2)[1];
        ELEV_PLAN(efloor - 1, k2)[0] = f0;
        ELEV_PLAN(efloor - 1, k2)[1] = d0;
    }
}

/* 3f95:19f7 – Haltestelle (Etage, Türindex) des Ziels der Ereignisfigur */
void find_event_door(int16_t *floor, int16_t *door)
{
    uint16_t room = 0, owner = 0; /* im Original bei unbekanntem Typ uninitialisiert */
    switch (DS_I16(0x7a3c)) {
    case 1: room = 0x13; break;
    case 2: room = 0x12; break;
    case 3:
        if (DS_I16(0x7a3e) < 4) {
            room = 1;
            owner = (uint16_t)DS_I16(0x7a3e);
        } else {
            room = (uint16_t)DS_I16(0x7a3e);
        }
        break;
    case 4: room = 4; owner = (uint16_t)DS_I16(0x7a3e); break;
    }
    for (int16_t i = 0; i < 0xd; i++) {
        for (int16_t k = 0; k < 4; k++) {
            int16_t f = ELEV_PLAN(i, k)[0], d = ELEV_PLAN(i, k)[1];
            if (FLOOR_B(f, d + 0x1e) == room && (FLOOR_B(f, d + 0x46) == owner || owner == 0)) {
                int16_t n = -1;
                for (int16_t j = 0; j <= k; j++)
                    if (ELEV_PLAN(i, j)[0] != 0)
                        n++;
                *floor = i + 1;
                *door = n;
            }
        }
    }
}

/* 3f95:1b31 – Tür eines Raumtyps (optional Besitzer/Zusatz) suchen; letzte Fundstelle gewinnt */
void building_find_door(uint16_t room_type, uint16_t owner, uint16_t sub, int16_t *floor, int16_t *door)
{
    for (int16_t f = 1; f < 0xe; f++) {
        for (int16_t d = 0; d < 4; d++) {
            if (FLOOR_B(f, d + 0x1e) == room_type && (owner == 0 || FLOOR_B(f, d + 0x46) == owner) &&
                (sub == 0 || FLOOR_B(f, d + 0x6e) == sub)) {
                *floor = f;
                *door = d;
            }
        }
    }
}
