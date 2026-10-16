/* Segment 3b0a – KI: Zustandsautomat, Programm- und Werbeplanung, Einkauf, Budget, Wege.
 *
 * Zustand der KI i (0/1): DS:24c5[i] Aktion am Ziel, DS:24c7[i]/DS:24cb[i] Zähler,
 * DS:24df[p] Mindestqualität, DS:24ea[p], DS:24ee[p] Mindestqualität für Shows.
 * DS:7744 + p*6: Zielgruppen-Maske der Werbeverträge (Wort) und mittlere Mindestreichweite (long).
 * DS:7894 + halb*4 + p: Zähler eingeplanter Sendungen ungerader Länge (Vor-/Nachmittag). */
#include <stdlib.h>
#include "game.h"

#define AI_IDX            DS_I8(0x24e7)
#define AI_PLAYER         DS_I8(0xf2 + DS_I8(0x24e7))
#define AI_ACTION         DS_I8(0x24c5 + DS_I8(0x24e7))
#define AI_COUNT1         DS_I16(0x24c7 + DS_I8(0x24e7) * 2)
#define AI_COUNT2         DS_I16(0x24cb + DS_I8(0x24e7) * 2)
#define AI_MIN_QUALITY(p) DS_I16(0x24df + (p) * 2)
#define AI_TARGET_MASK(p) DS_U16(0x7744 + (p) * 6)
#define AI_TARGET_REACH(p) DS_I32(0x7746 + (p) * 6)
#define AI_ODD(half, p)   DS_I8(0x7894 + (half) * 4 + (p))
#define BUDGET(p)         DS_ADDR(uint8_t, 0x6e09 + (p) * 0x3c)
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define SHOW(n)           (DS_PTR(uint8_t, 0x713c) + (n) * 0x1b3)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define PLAYER_OUT(p)     DS_U8(0x8bd0 + (p) * 0x17)
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define AD_SCHEDULE(p, h) DS_I16(0x4433 + (p) * 0x5a + (h) * 2)
#define ARCHIVE(p, g, k)  DS_I16(0x6ef9 + (p) * 0x6c + (g) * 8 + (k) * 2)
#define ARCHIVE_COUNT(p, g) DS_I8(0x6f59 + (p) * 0x6c + (g))
#define AD(n)             DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)
#define AD_NEED(n)        DS_I32(0xa8 + ((int8_t)AD(n)[0xa] - 1) * 4) /* Mindestzuschauer */
#define AD_CONTRACT_ID(p, i) DS_I16(0x1b50 + (p) * 0x46 + (i) * 7)
#define AD_CONTRACT_COUNT(p) DS_I16(0x7688 + (p) * 2)
#define AI_CONTRACT(p, i) DS_ADDR(int8_t, 0x4ebf + (p) * 0x4d + (i) * 7)
#define AI_C_DONE(p, i)   (UW(AI_CONTRACT(p, i), 6) & 4)
#define GENRE_CAP(g, h)   DS_U8(0x7140 + (g) * 8 + (h))
#define FIGURE(f)         DS_ADDR(uint8_t, 0x7b22 + (f) * 0x1e)

static int16_t first_free_hour(int16_t min)
{
    return g_minute > min ? hour_index() + 1 : hour_index();
}

/* 3b0a:000c – ein Schritt der aktiven KI */
void ai_tick(void)
{
    int8_t p = AI_PLAYER, act = AI_ACTION, r;
    if (PLAYER_OUT(p) != 0 || ai_check_arrival() != 1) {
        /* unterwegs: im Senderaum (5) auf der Etage des Menschen sofort weiter */
        if (PLAYER_OUT(p) == 0 && act == 5 && W(FIGURE(p - 1), 2) == W(FIGURE(p - 1), 6) &&
            W(FIGURE(g_player_idx), 2) == W(FIGURE(p - 1), 2))
            goto next;
        goto done;
    }
    switch (act) {
    case 0:
        ai_plan_schedule();
        goto done;
    case 1:
        ai_choose_ad_contracts();
        goto next;
    case 2: /* Filmhändler/Auktion */
        if (AI_COUNT1 == 0) {
            ai_choose_films();
            ai_auction_overbid();
        } else if (AI_COUNT1 == 1) {
            ai_buy_films();
            market_update_prices();
        } else if (AI_COUNT1 == 3 && archive_total_length(p) > 13) {
            ai_sell_old_programmes();
        }
        if (++AI_COUNT1 < 4)
            goto done;
        AI_COUNT1 = 0;
        goto next;
    case 3:
        ai_buy_news();
        goto next;
    case 4: /* Büro: Planung */
        r = ai_check_can_act();
        if (r == 0) {
            ai_optimize_ads();
            ai_optimize_programmes();
            ai_fill_ad_slots();
            ai_schedule_ads();
            ai_goto_next();
            DS_U8(0x24ea + p) = 0;
        } else if (r == 2) {
            AI_COUNT2 = 0;
            DS_U8(0x24ea + p) = 1;
        } else {
            DS_U8(0x24ea + p) = 1;
            if (AI_COUNT2 < 0x21) {
                ai_state_show_event();
                AI_COUNT2++;
            } else {
                ai_state_news_event();
            }
        }
        goto done;
    case 7: /* Studio */
        /* Aktion hier neu bestimmen; eine unbekannte endet ohne Aktion */
        ai_choose_studio_action(p);
        switch (DS_I8(0x262c + p * 3)) {
        case 0: goto next;
        case 1: ai_rent_studio(); goto done;
        case 2: case 3: ai_buy_production(); goto done;
        case 4: ai_prepare_episode(); goto done;
        case 5: ai_cast_episode(); goto done;
        case 6: ai_produce_episode(); goto done;
        default: goto done;
        }
    case 8:
        ai_schedule_programmes();
        if (++AI_COUNT1 < 0xf)
            goto done;
        AI_COUNT1 = 0;
        goto next;
    case 9:
        ai_choose_gift();
        goto next;
    case 10:
        ai_give_gift();
        goto next;
    case 11:
        ai_pick_destination();
        goto next;
    case -1:
    default:
        if ((uint16_t)(act + 1) > 0xd)
            goto done;
        goto next; /* Zustände 5, 6, 12 */
    }
next:
    ai_goto_next();
done:
    ai_switch_player();
}

/* 3b0a:02cf – schwache eingeplante Filme gegen bessere aus dem Archiv tauschen */
void ai_optimize_programmes(void)
{
    int16_t start = first_free_hour(5);
    int8_t p = AI_PLAYER, ok = 1;
    for (int16_t h = start; h <= 0xe; h++) {
        int16_t prog = SCHEDULE(p, h), s = h;
        /* ORIGINAL-BUG: bei Fortsetzungsstunden (9999) liest das Original den Datensatz "Film 9999"
         * weit hinter der Filmtabelle (zufälliger Speicher). PORT: leerer Datensatz statt eines
         * Zugriffs außerhalb des Speichers – Qualität/Länge 0, der Platz wird nicht getauscht. */
        static uint8_t no_film[0x17];
        uint8_t *cur = prog == 9999 ? no_film : FILM(prog);
        if (prog == 9999) {
            while (SCHEDULE(p, s) == 9999)
                s--;
            prog = SCHEDULE(p, s);
        }
        if (prog == 0 || prog >= 1000 || s <= start || FILM(prog)[0] == 7)
            continue;
        int32_t rate = ai_rate_slot(h, 0, 1);
        if ((int8_t)cur[3] >= AI_MIN_QUALITY(p) && bit_test(AI_TARGET_MASK(p), (int16_t)((int8_t)cur[0] - 1)) &&
            AI_TARGET_REACH(p) <= rate)
            continue;
        if (ok != 1)
            continue;
        for (int16_t g = 1; g <= 10; g++) {
            for (int16_t k = 0; k < ARCHIVE_COUNT(p, g); k++) {
                int16_t cand = ARCHIVE(p, g, k);
                uint8_t *f = FILM(cand);
                if (ai_rate_slot(h, cand, 0) < rate && bit_test(AI_TARGET_MASK(p), (int16_t)((int8_t)cur[0] - 1)))
                    ok = 0;
                if (cand == 0 || (int8_t)f[3] <= AI_MIN_QUALITY(p) || (UW(f, 7) & 2) || f[4] != cur[4] || ok != 1 ||
                    !bit_test(AI_TARGET_MASK(p), (int16_t)((int8_t)f[0] - 1)))
                    continue;
                for (int16_t x = s; x < (int8_t)cur[4] + s; x++) { /* Werbung in diesen Stunden lösen */
                    int16_t ad = AD_SCHEDULE(p, x);
                    if (ad == 0)
                        continue;
                    int16_t c = 0;
                    while (AD_CONTRACT_ID(p, c) != ad && c < AD_CONTRACT_COUNT(p))
                        c++;
                    if (c >= 10)
                        continue;
                    if (AI_C_DONE(p, c)) {
                        ok = 0;
                    } else {
                        AD_SCHEDULE(p, x) = 0;
                        ai_ad_spot_count(x, c, 0, p);
                    }
                }
                if (ok == 1) {
                    schedule_remove_programme(p, h);
                    schedule_place_programme(k, g, s, p);
                    goto next_hour;
                }
            }
        }
    next_hour:;
    }
}

/* 3b0a:066c – Werbung in Stunden mit zu wenig Zuschauern entfernen oder gegen bessere tauschen */
void ai_optimize_ads(void)
{
    int16_t h = first_free_hour(0x37);
    int8_t p = AI_PLAYER;
    if (h == 0)
        h = 1;
    for (; h <= 0xe; h++) {
        int16_t ad = AD_SCHEDULE(p, h);
        if (ad == 0 || SCHEDULE(p, h) == 0)
            continue;
        int16_t c = 0;
        while (AD_CONTRACT_ID(p, c) != ad && c < AD_CONTRACT_COUNT(p))
            c++;
        if (c >= AD_CONTRACT_COUNT(p))
            continue;
        if (AD_NEED(AD_CONTRACT_ID(p, c)) > ai_rate_slot(h, 0, 1)) {
            AD_SCHEDULE(p, h) = 0;
            ai_ad_spot_count(h, c, 0, p);
        } else {
            int16_t better = ai_find_better_ad(c, h);
            if (better != 0) {
                ai_ad_spot_count(h, c, 0, p);
                schedule_place_ad(better, h, p);
                ai_ad_spot_count(h, better, 1, p);
            }
        }
    }
}

/* 3b0a:081c – Vertrag mit höherer Mindestreichweite, die die Stunde noch schafft (0 = keiner) */
int16_t ai_find_better_ad(int16_t contract_idx, int16_t hour)
{
    int8_t p = AI_PLAYER, prime = hour > 7;
    int32_t best = 0, rate = ai_rate_slot(hour, 0, 1);
    int32_t need = AD_NEED(AD_CONTRACT_ID(p, contract_idx));
    int16_t pick = 0;
    int8_t *ac = AI_CONTRACT(p, contract_idx);
    if (ac[prime] < ac[prime + 2] && !(UW(ac, 6) & 4)) { /* Vertrag in dieser Tageshälfte übererfüllt */
        for (int16_t c = 0; c < AD_CONTRACT_COUNT(p); c++) {
            int8_t *cc = AI_CONTRACT(p, c);
            int32_t n = AD_NEED(AD_CONTRACT_ID(p, c));
            if (n > need && n > best && n <= rate && !(UW(cc, 6) & 4) &&
                ai_ad_target_ok(hour, AD_CONTRACT_ID(p, c), 0) == 1) {
                best = n;
                pick = c;
                if (cc[prime] > cc[prime + 2])
                    break;
            }
        }
    }
    return pick == contract_idx ? 0 : pick;
}

/* 3b0a:0a41 – leere Werbeplätze mit dem anspruchsvollsten passenden Vertrag füllen */
void ai_fill_ad_slots(void)
{
    int8_t p = AI_PLAYER, h = (int8_t)first_free_hour(0x37);
    int16_t n = AD_CONTRACT_COUNT(p);
    int8_t pick = 0, stop = 0, found = 0;
    int32_t best = 0;
    if (h == 0)
        h = 1;
    for (; h <= 0xe; h++) {
        int8_t prime = h > 7;
        int32_t rate = ai_rate_slot(h, 0, 1);
        if (AD_SCHEDULE(p, h) != 0 || SCHEDULE(p, h) == 0)
            continue;
        for (int8_t c = 0; c < n && stop == 0; c++) {
            int16_t ad = AD_CONTRACT_ID(p, c);
            int8_t *ac = AI_CONTRACT(p, c);
            if (!(UW(ac, 6) & 1) && !(UW(ac, 6) & 2) && AD_NEED(ad) < best)
                continue;
            if (ai_ad_target_ok(h, ad, 0) != 1 || AD_NEED(ad) >= rate || (UW(ac, 6) & 4))
                continue;
            best = AD_NEED(AD_CONTRACT_ID(p, c));
            pick = c;
            found = 1;
            if (ac[prime] > ac[prime + 2])
                stop = 1;
        }
        if (found == 1) {
            schedule_place_ad(pick, h, p);
            ai_update_ad_contracts();
            ai_ad_spot_count(h, pick, 1, p);
        }
        stop = 0;
        found = 0;
        pick = 0;
        best = 0;
    }
}

/* 3b0a:0cf3 – eine freie Stunde (Zähler DS:24cb) mit dem besten Archivprogramm füllen */
void ai_fill_programme_slots(void)
{
    int8_t p = AI_PLAYER, start = (int8_t)first_free_hour(5);
    /* Kandidatenwerte bleiben wie im Original von Eintrag zu Eintrag stehen */
    int8_t episode = 0, len = 0, quality = 0, best_q = 0, cap_genre = 0, best_g = 0, best_len = 0;
    int8_t first = 0, gap = 0, lo = 0, hi = 0;
    int16_t best = 0, best_k = 0, skip = 0;
    if (start == 0)
        start = 1;
    if (AI_COUNT2 < start)
        AI_COUNT2 = start;
    int8_t h = (int8_t)AI_COUNT2;
    if (h <= 0xe) {
        int8_t half = h >= 8;
        int8_t hh = h >= 8 ? (int8_t)(h - 7) : h;
        if (SCHEDULE(p, h) == 0) {
            if (h == start)
                first = 1;
            int8_t stop = h <= 7 ? 8 : 7, free = 0;
            for (int8_t x = h; SCHEDULE(p, x) == 0 && x != stop && x <= 0xe; x++)
                free++;
            for (int8_t g = 1; g <= 0xb; g++) {
                for (int8_t k = 0; k < ARCHIVE_COUNT(p, g); k++) {
                    int16_t prog = ARCHIVE(p, g, k);
                    if (prog != 0) {
                        if (g == 0xb) {
                            skip = 0;
                            lo = h >= 8 ? 8 : 1;
                            hi = h >= 8 ? 0xe : 7;
                            for (int8_t x = lo; x <= hi; x++)
                                if (SCHEDULE(p, x) == prog + 1000)
                                    skip = 1;
                            episode = (int8_t)series_next_episode(p, prog, h);
                            len = (int8_t)SERIES(prog)[0x23];
                            quality = (int8_t)SERIES(prog)[episode * 3 + 1];
                            cap_genre = (int8_t)(SERIES(prog)[0x21] + 10);
                        } else {
                            uint8_t *f = FILM(prog);
                            if (first) {
                                skip = 0;
                            } else {
                                skip = (UW(f, 7) & 2) ? -1 : 0;
                                if (hh < 4 && skip == 0)
                                    skip = (UW(f, 7) & 8) ? -1 : 0; /* Skandalfilm erst ab 21 Uhr */
                            }
                            len = (int8_t)f[4];
                            quality = (int8_t)f[3];
                            cap_genre = g;
                        }
                    }
                    if (len <= free && len % 2 != 0 && skip == 0) { /* ungerade Längen sparsam einsetzen */
                        if (AI_ODD(half, p) == 0 && g != 0xb && AI_MIN_QUALITY(p) > quality)
                            skip = 1;
                        if (AI_ODD(half, p) % 2 != 0 || free == 0) {
                            if (AI_MIN_QUALITY(p) > quality)
                                skip = 1;
                            for (int8_t x = lo; x <= hi; x++)
                                if (SCHEDULE(p, x) == 0 && (x < h || h + len - 1 < x))
                                    gap = 1;
                            if (gap == 1 && ai_count_short_programmes() <= (len > 1 ? 2 : 3))
                                skip = 1;
                        }
                    }
                    if (len > free || skip != 0 || prog == 0 || (prog <= 1000 && FILM(prog)[0] == 7))
                        continue;
                    if (best == 0) {
                        best = prog;
                        best_k = k;
                        best_q = g == 0xb ? (int8_t)SERIES(best)[episode * 3 + 1] : (int8_t)FILM(best)[3];
                        best_len = g == 0xb ? (int8_t)SERIES(best)[0x23] : (int8_t)FILM(best)[4];
                        best_g = g;
                    }
                    /* ORIGINAL-BUG: bewertet mit dem Genre als Stunde */
                    if (ai_rate_slot(g, g == 0xb ? prog + 1000 : prog, 1) <= AI_TARGET_REACH(p))
                        continue;
                    if (!bit_test(AI_TARGET_MASK(p), (int16_t)(best_g - 1)))
                        continue;
                    if ((int16_t)(GENRE_CAP(cap_genre, hh) * quality) < (int16_t)(GENRE_CAP(best_g, hh) * best_q))
                        continue;
                    best = prog;
                    best_k = k;
                    best_q = g == 0xb ? (int8_t)SERIES(best)[episode * 3 + 1] : (int8_t)FILM(best)[3];
                    best_len = g == 0xb ? (int8_t)SERIES(best)[0x23] : (int8_t)FILM(best)[4];
                    best_g = g;
                    if (best_len == free)
                        goto place;
                }
            }
        place:
            if (best != 0) {
                schedule_place_programme(best_k, best_g, h, p);
                if (best_len % 2 != 0)
                    AI_ODD(half, p)++;
                if (DS_I8(0x15ea) > 0) /* Testmodus: auf Taste warten */
                    while (kbd_getkey() == -1)
                        ;
            }
        }
    }
    if (++AI_COUNT2 > 0xe) {
        AI_COUNT2 = 0;
        ai_goto_next();
    }
}

/* 3b0a:138e – Planung im Büro: offene Werbeverträge platzieren, sonst Programmlücken füllen */
void ai_plan_schedule(void)
{
    int8_t p = AI_PLAYER;
    int16_t n = AD_CONTRACT_COUNT(p);
    int16_t h = first_free_hour(5);
    programmes_mark_scheduled();
    ai_schedule_live();
    if (ai_ads_all_satisfied() == 0 && SCHEDULE(p, h) != 0 && AI_COUNT2 == 0) {
        if (AI_COUNT1 < n) {
            int16_t c = AI_COUNT1;
            int16_t ad = AD_CONTRACT_ID(p, c);
            int8_t *ac = AI_CONTRACT(p, c);
            if (ad != 0 && AD_NEED(ad) > 0 && ac[2] + ac[3] < (int8_t)(ac[1] + ac[0]))
                ai_place_ad_contract(c, (int8_t)(ac[1] + ac[0]));
        }
        if (AI_COUNT1 + 1 < n) {
            AI_COUNT1++;
        } else {
            AI_COUNT1 = 0;
            ai_goto_next();
        }
    } else {
        ai_fill_programme_slots();
    }
}

/* 3b0a:150e – Programme mit passender Zielgruppe einplanen, um Spots eines Vertrags zu senden */
void ai_place_ad_contract(int16_t contract_idx, int8_t spots)
{
    int8_t p = AI_PLAYER, scheduled = 0, len = 0, ofs = 0, n = 0, j = 0;
    int8_t slots[15];
    int16_t prog = 0;
    int16_t ad = AD_CONTRACT_ID(p, contract_idx);
    int8_t remaining = spots;
    if (ad != 0) {
        for (int8_t g = 1; g <= 0xb; g++) {
            int16_t count = ARCHIVE_COUNT(p, g);
            if (g == 0xb)
                ofs = (int8_t)0xe8;
            /* ORIGINAL: prüft nur den letzten Archiveintrag des Genres (Serien mit Versatz -24) */
            if (ai_ad_target_ok((int16_t)(ARCHIVE(p, g, count - 1) + ofs), ad, 1) != 1)
                continue;
            for (int8_t k = 0; k < count; k++) {
                prog = ARCHIVE(p, g, k);
                if (prog != 0) {
                    if (g == 0xb) {
                        scheduled = (UW(SERIES(prog), 0x26) & 2) ? -1 : 0;
                        len = (int8_t)SERIES(prog)[0x23];
                        prog += 1000;
                    } else {
                        scheduled = (UW(FILM(prog), 7) & 2) ? -1 : 0;
                        len = (int8_t)FILM(prog)[4];
                    }
                    if (!(UW(AI_CONTRACT(p, contract_idx), 6) & 4) && scheduled == 0) {
                        n = ai_find_free_slots(slots, len);
                        if (n != 0)
                            j = 0;
                        for (; n != 0 && j <= n - 1; j++) {
                            if (ai_rate_slot(slots[j], prog, 1) < AD_NEED(ad))
                                continue;
                            if (prog < 1000 && (((UW(FILM(prog), 7) & 8) && slots[j] < 4) || FILM(prog)[0] == 7))
                                continue;
                            remaining -= len;
                            schedule_place_programme(k, g, slots[j], p);
                            if (prog != 0)
                                ai_place_ad_range(contract_idx, slots[j], (int8_t)(slots[j] + len - 1));
                            if (remaining <= 0)
                                return;
                            slots[j] = 0;
                            break;
                        }
                    }
                }
                slots[j] = 0;
            }
        }
    }
    if (prog == 0)
        DS_U8(0x15eb) = 1;
}

/* 3b0a:1864 – Spots eines Vertrags in den Stunden from..to platzieren */
void ai_place_ad_range(int16_t contract_idx, int8_t from_hour, int8_t to_hour)
{
    int8_t p = AI_PLAYER;
    for (int8_t h = from_hour; h <= to_hour; h++) {
        if (SCHEDULE(p, h) != 0 && !(UW(AI_CONTRACT(p, contract_idx), 6) & 4)) {
            schedule_place_ad(contract_idx, h, p);
            ai_ad_spot_count(h, contract_idx, 1, p);
        }
    }
    ai_update_ad_contracts();
}

/* ---- Einkauf. Wunschliste DS:24f2 + p*0x4d, 7 x 11 B: +0 Angebots-/Losnummer, +2 Punkte,
 * +4 Programm, +6 Preis (long), +0xa 1 = Auktion. Letzter gültiger Index DS:7292[p].
 * Eigene Gebote DS:754a + p*0x42 (6 B: Los, Gebot long), Anzahl DS:7710[p]. */
#define WISH(p, i)        DS_ADDR(uint8_t, 0x24f2 + (p) * 0x4d + (i) * 0xb)
#define WISH_LAST(p)      DS_I16(0x7292 + (p) * 2)
#define BID(p, i)         DS_ADDR(uint8_t, 0x754a + (p) * 0x42 + (i) * 6)
#define BID_COUNT(p)      DS_I16(0x7710 + (p) * 2)
#define OFFER(i)          DS_ADDR(uint8_t, 0x19cd + (i) * 8)
#define LOT(i)            DS_ADDR(uint8_t, 0x1aed + (i) * 9)
#define PLAYER_IMG(p)     DS_I16(0x8bd1 + (p) * 0x17)
#define PLAYER_METER(p)   DS_I16(0x8bc8 + (p) * 0x17)
#define FLOOR_OWNER(f, d) DS_U8(0x7c62 + (f) * 0x7a + (d) + 0x46)

/* Punkte eines Programms: Länge * Qualität * 20 / (Preis in Tausend) */
static int16_t ai_programme_score(int16_t prog, int32_t price, int8_t *genre, int16_t *quality)
{
    int8_t len;
    if (prog < 1000) {
        *genre = (int8_t)FILM(prog)[0];
        *quality = (int8_t)FILM(prog)[3];
        len = (int8_t)FILM(prog)[4];
    } else {
        *genre = 0xb;
        *quality = series_avg_quality(prog);
        len = (int8_t)((int8_t)SERIES(prog - 1000)[0x22] * (int8_t)SERIES(prog - 1000)[0x23]);
    }
    int32_t k = price / 1000;
    if (k == 0)
        k = 1;
    return (int16_t)((int32_t)(int16_t)(len * *quality * 0x14) / k);
}

/* 3b0a:190e – Wunschliste aus Filmangeboten (und über ai_auction_bid aus der Auktion) füllen */
void ai_choose_films(void)
{
    int8_t p = AI_PLAYER, counts[12], genre_taken = 0;
    int16_t i = 0, min_q = AI_MIN_QUALITY(p), done = 0;
    for (int16_t g = 1; g < 0xc; g++)
        counts[g] = ARCHIVE_COUNT(p, g);
    while (i < (int8_t)BUDGET(p)[0x2a] && !done) {
        uint8_t *w = WISH(p, i);
        for (int16_t o = 1; o < 0xb; o++) {
            int16_t prog = W(OFFER(o), 0);
            if (prog == 0)
                continue;
            int8_t g;
            int16_t q;
            int32_t price = prog < 1000 ? film_price(prog) : series_price(prog - 1000);
            int16_t score = ai_programme_score(prog, price, &g, &q);
            int16_t ok = !(q < min_q || counts[g] > 3);
            for (int8_t k = 0; k < i; k++)
                if (W(WISH(p, k), 4) == prog)
                    ok = 0;
            if (ok && W(w, 2) < score && price <= L(BUDGET(p), 8)) {
                W(w, 4) = prog;
                W(w, 0) = o;
                L(w, 6) = price;
                W(w, 2) = score;
                w[0xa] = 0;
                genre_taken = g;
            }
        }
        ai_auction_bid(i);
        if (W(w, 4) == 0 || (i > 0 && W(WISH(p, i - 1), 4) == W(w, 4))) {
            done = 1;
            WISH_LAST(p) = (int16_t)(i - 1);
        } else {
            L(BUDGET(p), 8) -= L(w, 6);
            counts[genre_taken]++;
            WISH_LAST(p) = i;
            i++;
        }
    }
}

/* 3b0a:1c47 – Wunschliste abarbeiten: Angebote kaufen, Auktionsgebote abgeben */
void ai_buy_films(void)
{
    int8_t p = AI_PLAYER, rival = DS_I8(0xf3 - AI_IDX), bought = 0;
    for (int16_t i = 0; i <= WISH_LAST(p) && WISH_LAST(p) >= 0; i++) {
        uint8_t *w = WISH(p, i);
        if (W(w, 4) == 0)
            continue;
        int16_t slot = W(w, 0);
        if (w[0xa] == 0) {
            W(OFFER(slot), 0) = 0;
            PLAYER_MONEY(p) -= L(w, 6);
            OFFER(slot)[7] -= 100;
            int16_t g = W(w, 4) < 1000 ? (int8_t)FILM(W(w, 4))[0] : 0xb;
            if (W(w, 4) > 1000)
                W(w, 4) -= 1000;
            if (ARCHIVE_COUNT(p, g) < 4) {
                ARCHIVE(p, g, ARCHIVE_COUNT(p, g)) = W(w, 4);
                ARCHIVE_COUNT(p, g)++;
                bought = 1;
            }
        } else {
            uint8_t *b = BID(p, BID_COUNT(p));
            L(b, 2) = L(w, 6);
            W(b, 0) = slot;
            if ((int8_t)LOT(slot)[6] == rival) /* überbotene KI bekommt ihr Gebot zurück */
                L(BUDGET(rival), 8) += L(w, 6);
            LOT(slot)[6] = (uint8_t)p;
            L(LOT(slot), 2) += auction_bid_increment(slot);
            BID_COUNT(p)++;
        }
        W(w, 4) = 0;
        W(w, 0) = 0;
        L(w, 6) = 0;
        W(w, 2) = 0;
        w[0xa] = 0;
    }
    WISH_LAST(p) = 0;
    if (g_cur_room == 4 && DS_I16(0x7a16) == p && bought)
        movie_agency_reload_texts();
}

/* 3b0a:1ef1 – bestes Auktionslos in Wunschlisten-Eintrag limit eintragen */
void ai_auction_bid(int16_t limit)
{
    int8_t p = AI_PLAYER, counts[12];
    int8_t rival = AI_IDX != 0 ? DS_I8(0xf2) : DS_I8(0xf3);
    int16_t min_q = AI_MIN_QUALITY(p);
    for (int16_t g = 1; g < 0xc; g++)
        counts[g] = ARCHIVE_COUNT(p, g);
    for (int16_t l = 1; l < 0xb; l++) {
        uint8_t *lot = LOT(l);
        int16_t prog = W(lot, 0);
        if (prog == 0 || (int8_t)lot[6] == p)
            continue;
        int8_t g;
        int16_t q;
        int32_t price = auction_bid_increment(l) + L(lot, 2);
        int16_t score = ai_programme_score(prog, price, &g, &q);
        int16_t ok = !(q < min_q || counts[g] > 2);
        if ((int8_t)lot[6] == rival)
            ok = 0;
        for (int8_t k = 0; k < limit; k++)
            if (W(WISH(p, k), 4) == prog)
                ok = 0;
        if (ok) {
            uint8_t *w = WISH(p, limit);
            if (W(w, 2) < score && price <= L(BUDGET(p), 8)) {
                W(w, 4) = prog;
                W(w, 0) = l;
                L(w, 6) = price;
                W(w, 2) = score;
                w[0xa] = 1;
            }
        }
    }
}

/* 3b0a:2194 – überbotene eigene Gebote erhöhen oder aufgeben (nur ab Image 10) */
void ai_auction_overbid(void)
{
    int8_t p = AI_PLAYER;
    if (PLAYER_IMG(p) <= 9)
        return;
    for (int16_t i = 0; i < BID_COUNT(p); i++) {
        int16_t l = W(BID(p, i), 0);
        uint8_t *lot = LOT(l);
        if ((int8_t)lot[6] != g_local_player)
            continue;
        int16_t prog = W(lot, 0), q, len;
        if (prog < 1000) {
            q = (int8_t)FILM(prog)[3];
            len = (int8_t)FILM(prog)[4];
        } else {
            q = series_avg_quality(prog);
            len = (int16_t)((int8_t)SERIES(prog - 1000)[0x22] * (int8_t)SERIES(prog - 1000)[0x23] / 2);
        }
        int16_t div = (int16_t)(L(lot, 2) / 1000);
        int16_t score = div != 0 ? (int16_t)((int16_t)(q * len * 0x14) / div) : 0; /* Original: Division durch 0 möglich */
        int32_t inc = auction_bid_increment(l);
        if (inc >= L(BUDGET(p), 8) || score <= 2) { /* aufgeben: Gebot zurück ins Budget */
            L(BUDGET(p), 8) += L(BID(p, i), 2);
            for (int16_t k = i; k < BID_COUNT(p) - 1; k++)
                far_memcpy(BID(p, k), BID(p, k + 1), 6);
            BID_COUNT(p)--;
            far_memset(BID(p, BID_COUNT(p)), 6);
        } else {
            lot[6] = (uint8_t)p;
            L(lot, 2) += auction_bid_increment(l);
            L(BUDGET(p), 8) -= auction_bid_increment(l);
            L(BID(p, i), 2) += auction_bid_increment(l);
        }
    }
}

/* 3b0a:24cc – Tagesbudget: Ziele nach Image, Quote, Reichweite; Prozentanteile der Konten */
void ai_budget_plan(int8_t player)
{
    int8_t human = (int8_t)g_local_player, rival = DS_I8(0xf2 + (AI_IDX == 0)), pct[7];
    int8_t aq_me = (int8_t)ai_archive_avg_quality(player);
    int8_t aq_rival = (int8_t)ai_archive_avg_quality(rival);
    int8_t aq_human = (int8_t)ai_archive_avg_quality(human);
    int8_t r_me = ai_eval_ratings(0, player), r_rival = ai_eval_ratings(0, rival), r_human = ai_eval_ratings(0, human);
    int8_t weakest_archive = 0, rank;
    int16_t sum = 0;
    uint8_t *b = BUDGET(player);
    int16_t img = PLAYER_IMG(player);
    if (PLAYER_IMG(rival) < img && PLAYER_IMG(human) < img)
        b[0x2b] = 1;
    else if (PLAYER_IMG(rival) < img || PLAYER_IMG(human) < img)
        b[0x2b] = 2;
    else
        b[0x2b] = 3;
    b[0x1c] = (uint8_t)((3 - (int8_t)b[0x2b]) * 10);
    if (DS_I16(0x7a74) == 3)
        b[0x1c] = 0;
    if (r_rival < r_me && r_human < r_me)
        rank = 1;
    else if (r_rival < r_me || r_human < r_me)
        rank = 2;
    else
        rank = 3;
    int16_t m = PLAYER_METER(player);
    if (m < PLAYER_METER(rival) || m < PLAYER_METER(human))
        b[0x24] = (m < PLAYER_METER(rival) && m < PLAYER_METER(human)) ? 3 : 2;
    else
        b[0x24] = 1;
    if (aq_me <= aq_rival && aq_me <= aq_human)
        weakest_archive = 1;
    b[0x25] = (uint8_t)(weakest_archive + (archive_total_length(player) < 0x15) + 1);
    int32_t total = DS_I32(0x1ced + player * 0x1ce) + DS_I32(0x1ced + rival * 0x1ce) + DS_I32(0x1ced + human * 0x1ce);
    int16_t share = total != 0 ? (int16_t)((int32_t)((uint32_t)DS_I32(0x1ced + player * 0x1ce) * 100u) / total) : 0;
    b[0x28] = share > 0x1c ? 1 : share < 0x14 ? 3 : 2;
    if (DS_I16(0x7468) == 3 && contest_tick() < 2)
        b[0x27] = b[0x2b];
    else
        b[0x27] = (uint8_t)(rank > 1 ? rank - 1 : rank);
    if (DS_I16(0x7468) == 1 && contest_tick() < 2)
        b[0x26] = b[0x2b];
    else
        b[0x26] = (uint8_t)(rank > 1 ? rank - 1 : rank);
    b[0x29] = 1;
    int16_t d = (int8_t)b[0x1c] + 100;
    for (int16_t k = 1; k <= 6; k++) {
        pct[k] = (int8_t)(DS_I8(0x15ec + k) * 100 / d);
        b[0x1c + k] = (uint8_t)((int8_t)b[0x23 + k] * pct[k]);
        sum += (int8_t)b[0x1c + k];
    }
    for (int16_t k = 1; k <= 6; k++)
        b[0x1c + k] = (uint8_t)((int8_t)b[0x1c + k] * 100 / sum);
    b[0x2a] = (uint8_t)(b[0x2b] << 1);
}

/* 3b0a:2900 – Mittel über die 7 Abendstunden von gestern: 0 = Quote in Prozent, 1 = Programmqualität */
int8_t ai_eval_ratings(int8_t mode, int8_t player)
{
    int16_t sum = 0;
    if (g_day == 1)
        return (int8_t)(mode == 0 ? 10 : 15);
    if (mode == 0) {
        int32_t reach = (int32_t)((uint32_t)DS_I32(0x1ced + player * 0x1ce) * 0x9a0u);
        for (int16_t h = 1; h <= 7; h++)
            if (reach != 0) /* im Original Division durch 0 möglich */
                sum += (int16_t)((int32_t)((uint32_t)DS_I32(0x789e + player * 0x20 + h * 4) * 100u) / reach);
    } else if (mode == 1) {
        for (int16_t h = 1; h < 8; h++) {
            int16_t s = h;
            if (DS_I16(0x3919 + player * 0x5a + h * 2) == 0)
                continue;
            while (DS_I16(0x3919 + player * 0x5a + s * 2) == 9999 && s > 0)
                s--;
            int16_t prog = DS_I16(0x3919 + player * 0x5a + s * 2);
            if (prog < 1000)
                sum += (int8_t)FILM(prog)[1];
            else
                sum += (int8_t)SERIES(prog - 1000)[series_next_episode(player, prog - 1000, h) * 3];
        }
    }
    return (int8_t)(sum / 7);
}

/* 3b0a:2a7f – Programm aus dem Archiv zum aktuellen Preis verkaufen */
void ai_sell_programme(int16_t genre, int16_t idx)
{
    int8_t p = AI_PLAYER;
    if (genre == 0xb) {
        int16_t s = ARCHIVE(p, 0xb, idx);
        PLAYER_MONEY(p) += series_price(s);
        L(BUDGET(p), 8) += series_price(s);
        programme_remove_from_archives(s + 1000);
        SERIES(s)[0x24] = 0;
    } else {
        int16_t f = ARCHIVE(p, genre, idx);
        PLAYER_MONEY(p) += film_price(f);
        L(BUDGET(p), 8) += film_price(f);
        programme_remove_from_archives(f);
        FILM(f)[5] = 0;
    }
}

/* 3b0a:2bb9 – schwache Filme und die schwächste Serie verkaufen */
void ai_sell_old_programmes(void)
{
    int8_t p = AI_PLAYER, ok = 1;
    for (int16_t g = 1; g < 0xb; g++) {
        if (g == 8 || g == 7)
            continue;
        for (int16_t k = 0; k < ARCHIVE_COUNT(p, g); k++) {
            int16_t f = ARCHIVE(p, g, k);
            if (f == 0)
                continue;
            uint8_t *fp = FILM(f);
            if (fp[4] == 1 && (DS_I8(0x7894 + p) > 1 || DS_I8(0x7898 + p) > 1))
                ok = 0;
            if (!(UW(fp, 7) & 2) && ok && L(fp, 8) != 0 && (int8_t)fp[1] + 3 < AI_MIN_QUALITY(p))
                ai_sell_programme(g, k);
        }
    }
    if (ARCHIVE_COUNT(p, 0xb) > 3) {
        int16_t worst = 100, pick = -1;
        for (int16_t k = 0; k < ARCHIVE_COUNT(p, 0xb); k++) {
            uint8_t *s = SERIES(ARCHIVE(p, 0xb, k));
            if (UW(s, 0x26) & 2)
                continue;
            int16_t q = 0;
            for (int16_t e = 1; e <= (int8_t)s[0x22]; e++)
                q += (int8_t)s[e * 3 + 1];
            q /= (int8_t)s[0x22];
            if (q < worst) {
                worst = q;
                pick = k;
            }
        }
        if (pick >= 0)
            ai_sell_programme(0xb, pick);
    }
}

/* 3b0a:2dbc – mittlere Aktualität einer Serie (ORIGINAL: zählt Folgen 2..n+1) */
int16_t series_avg_quality(int16_t series_id)
{
    int16_t sum = 0;
    if (series_id > 1000)
        series_id -= 1000;
    uint8_t *s = SERIES(series_id);
    if (s[0x22] == 0)
        return 0;
    for (int16_t e = 1; e <= (int8_t)s[0x22];) {
        e++;
        sum += (int8_t)s[e * 3 + 1];
    }
    return (int16_t)(sum / (int8_t)s[0x22]);
}

/* 3b0a:2e4d – Werbeverträge mit dem höchsten Honorar annehmen, die zur Reichweite passen */
void ai_choose_ad_contracts(void)
{
    int8_t p = AI_PLAYER, refresh = 0;
    ad_market_update();
    for (int8_t i = 0; i < (int8_t)BUDGET(p)[0x2a]; i++) {
        int32_t best = 0;
        int16_t pick = 0, slot = 0;
        int16_t n = AD_CONTRACT_COUNT(p);
        for (int16_t o = 1; o < 0xb; o++) {
            int16_t ad = DS_I16(0x1980 + o * 7);
            if (ad == 0) {
                refresh = 1;
                continue;
            }
            int32_t need = AD_NEED(ad);
            int32_t v = (int32_t)((uint32_t)DS_I32(0x1ced + p * 0x1ce) * (uint32_t)(int32_t)DS_I16(0x24d7 + i * 2)) / 100;
            int32_t can = (int32_t)((uint32_t)v * 0x9a0u);
            if (ai_can_take_contract(ad) == 1 && need <= can && L(AD(ad), 0x10) > best) {
                slot = o;
                best = L(AD(ad), 0x10);
                pick = ad;
            }
        }
        if (pick != 0) {
            AD(pick)[0xf] = (uint8_t)p;
            AD_CONTRACT_ID(p, n) = pick;
            DS_I16(0x1b55 + p * 0x46 + n * 7) = (int16_t)((int8_t)AD(pick)[8] + g_day);
            ai_ad_contract_init(n);
            AD_CONTRACT_COUNT(p)++;
            DS_I16(0x1980 + slot * 7) = 0;
            DS_U8(0x1982 + slot * 7) -= 100;
            refresh = 1;
        }
    }
    if (refresh)
        ad_market_update();
}

/* 3b0a:30bd – kann die KI einen weiteren Vertrag schaffen? (Spots/Tag, Restspots, Zielgruppe, Image) */
int8_t ai_can_take_contract(int16_t advertiser_id)
{
    int8_t p = AI_PLAYER, ok, open_targets = 0;
    int16_t per_day = 0, left = hour_index_after_min5();
    uint8_t *a = AD(advertiser_id);
    for (int16_t c = 0; c < AD_CONTRACT_COUNT(p); c++) {
        per_day += AI_CONTRACT(p, c)[0];
        left += (int8_t)AD(AD_CONTRACT_ID(p, c))[9] - DS_I8(0x1b54 + p * 0x46 + c * 7);
    }
    int16_t rate = a[8] == 0 ? 1 : (int8_t)a[9] / (int8_t)a[8];
    ok = per_day + rate < 6 && left < 0xf && AD_CONTRACT_COUNT(p) < 8; /* höchstens 8 */
    if (ok == 1) {
        ok = 0;
        for (int16_t k = 1; k < 4; k++)
            if ((int8_t)a[0xb + k] < 1)
                open_targets++;
        if (open_targets == 3)
            ok = 1;
    }
    if (PLAYER_IMG(p) < (int16_t)rating_from_viewers(p, AD_NEED(advertiser_id)))
        ok = 0;
    return ok;
}

/* 3b0a:323f – Gesamtlänge aller Programme im Archiv */
int8_t archive_total_length(int8_t player)
{
    int8_t sum = 0;
    for (int8_t g = 1; g < 0xc; g++)
        for (int8_t k = 0; k < ARCHIVE_COUNT(player, g); k++)
            sum += g < 0xb ? (int8_t)FILM(ARCHIVE(player, g, k))[4] : (int8_t)SERIES(ARCHIVE(player, g, k))[0x23];
    return sum;
}

/* 3b0a:32f0 – Gesamtlänge der Programme eines Genres im Archiv (Serien: Genre 10 + Seriengenre) */
int16_t ai_genre_total_length(int8_t genre)
{
    int8_t p = AI_PLAYER, sum = 0;
    if (genre < 0xb) {
        int8_t last = ARCHIVE_COUNT(p, genre) == 0 ? 0 : (int8_t)(ARCHIVE_COUNT(p, genre) - 1);
        if (ARCHIVE(p, genre, last) != 0)
            for (int16_t k = 0; k < ARCHIVE_COUNT(p, genre); k++)
                sum += (int8_t)FILM(ARCHIVE(p, genre, k))[4];
    } else {
        for (int16_t k = 0; k < ARCHIVE_COUNT(p, 0xb); k++)
            if ((int8_t)SERIES(ARCHIVE(p, 0xb, k))[0x21] == genre - 10)
                sum += (int8_t)SERIES(ARCHIVE(p, 0xb, k))[0x23];
    }
    return sum;
}

/* 3b0a:3410 – benötigte Spots pro Tag bis zum Stichtag */
int8_t ai_ad_spots_per_day(int16_t contract_idx, int8_t player)
{
    int8_t *c = AI_CONTRACT(player, contract_idx);
    if (UW(c, 6) & 4)
        return 0;
    int16_t left = (int8_t)AD(AD_CONTRACT_ID(player, contract_idx))[9] - c[4];
    int16_t days = DS_I16(0x1b55 + player * 0x46 + contract_idx * 7) - g_day + 1;
    if (days == 0) /* keine Division durch 0 */
        days = 1;
    return (int8_t)(left / days);
}

/* 3b0a:34c6 – passt die Zielgruppe des Werbekunden zum Programm (bzw. zur Sendung der Stunde)? */
int8_t ai_ad_target_ok(int16_t hour_or_prog, int16_t advertiser_id, int8_t is_prog)
{
    int8_t r = 1;
    int16_t genre;
    int8_t *c = DS_ADDR(int8_t, 0x3f2f + advertiser_id * 0x18 + 0xb);
    if (is_prog == 0) {
        int8_t p = AI_PLAYER;
        while (SCHEDULE(p, hour_or_prog) == 9999 && hour_or_prog > 0)
            hour_or_prog--;
        hour_or_prog = SCHEDULE(p, hour_or_prog);
    }
    if (advertiser_id == 0)
        return 0;
    if (hour_or_prog == 0)
        genre = 0;
    else if (hour_or_prog < 1000)
        genre = (int8_t)FILM(hour_or_prog)[0];
    else
        genre = (int8_t)SERIES(hour_or_prog - 1000)[0x21] + 10;
    for (int16_t k = 1; k < 4; k++)
        if (c[k] < 0 && abs(c[k]) == genre)
            r = 0;
    if (r != 0) {
        r = 2;
        for (int16_t k = 1; k < 4; k++)
            if (c[k] > 0 && r == 2)
                r = 0;
        for (int16_t k = 1; k < 4; k++)
            if (c[k] == genre)
                r = 1;
        if (r == 2)
            r = 1;
    }
    return r;
}

/* 3b0a:361d – ist die KI-Figur angekommen? Blockiert sie den Menschen in einem KI-Raum, zahlt der Mensch */
int8_t ai_check_arrival(void)
{
    int8_t p = AI_PLAYER, arrived = 0;
    uint8_t *f = FIGURE(p - 1);
    int16_t now;
    clock_get_cached(&now);
    if (!(DS_I16(0x15f3) == 0 && W(f, 0x10) == 0 && W(f, 6) == W(f, 2) && W(f, 4) == W(f, 0))) {
        if (DS_I16(0x24cf + AI_IDX * 2) == 0) {
            DS_I16(0x24cf + AI_IDX * 2) = 0;
            return 0;
        }
    }
    int16_t free = 1;
    for (int16_t i = 0; i <= 2; i++) {
        uint8_t *o = FIGURE(i);
        if (p - 1 == i || W(o, 0x10) != 0 || W(o, 0x12) != W(f, 0x12) || W(o, 6) != W(f, 6))
            continue;
        if (g_local_player - 1 == i && FLOOR_OWNER(W(o, 6), W(o, 0x12)) == (uint8_t)p) {
            /* Mensch steht im Raum der KI: Strafe */
            DS_I16(0x79f6) = p;
            DS_I16(0x79ca) = 0x12a;
            DS_I32(0x7ad2) = DS_I32(0x7ac2);
            PLAYER_MONEY(g_local_player) -= DS_I32(0x7ac2);
            money_add(0x10, DS_I32(0x7ac2));
            status_money_update();
            DS_I32(0x7ac2) <<= 1;
            if (DS_I32(0x7ac2) > 200000)
                DS_I32(0x7ac2) = 200000;
            DS_I16(0x15f3) = 1;
            while (g_cur_room != 0)
                room_leave();
            DS_I16(0x15f3) = 0;
        } else {
            free = 0;
            ai_goto_next();
        }
    }
    /* Zutritt prüfen wie beim Menschen (z. B. Chef eines anderen Spielers);
     * verwehrt, dann nächstes Ziel */
    if (free == 1 && room_entry_check((int16_t)(p - 1), 0) != 0) {
        ai_goto_next();
        free = 0;
    }
    if (free == 1) {
        if (DS_I8(0x24e8 + AI_IDX) == 0) { /* einmal 300 Sekunden im Raum verweilen */
            if (DS_I16(0x24cf + AI_IDX * 2) == 0)
                DS_I16(0x24cf + AI_IDX * 2) = now;
            if (abs(now - DS_I16(0x24cf + AI_IDX * 2)) >= 300) {
                arrived = 1;
                DS_I16(0x24cf + AI_IDX * 2) = 0;
                DS_I8(0x24e8 + AI_IDX) = 1;
            }
        } else {
            arrived = 1;
        }
    }
    return arrived;
}

/* 3b0a:3854 – nächstes Ziel der KI-Figur wählen und losschicken */
void ai_goto_next(void)
{
    int8_t *act = DS_ADDR(int8_t, 0x24c5 + AI_IDX);
    int8_t p = AI_PLAYER;
    if (*act < 0xc) {
        (*act)++;
        ai_choose_studio_action(p);
        if (*act == 7 && DS_I8(0x262c + p * 3) < 1)
            *act = 8;
        if (*act == 8 && DS_I16(0x7a74) == 1)
            *act = 9;
        if (*act == 9 && L(BUDGET(p), 4) < 30000)
            *act = 10;
        if (*act == 10 && DS_I8(0x789c + AI_IDX) == 0)
            *act = 11;
        if (*act == 11 && (DS_I16(0x7a3c) == 0 || DS_I16(0x7a3c) == 4 || DS_I16(0x7a46) != 0))
            *act = 12;
        if (*act == 12 && rand_range(10, 1) != 1)
            *act = 0;
    } else {
        *act = 0;
    }
    DS_I8(0x24e8 + AI_IDX) = 0;
    int8_t *visit;
    int8_t c;
    switch (*act) {
    case 0: case 4: case 8:
        ai_figure_goto(p, 1, p, 0); /* eigenes Büro */
        break;
    case 1: ai_figure_goto(p, 0xb, 0, 0); break;
    case 2: ai_figure_goto(p, 8, 0, 0); break;
    case 3: ai_figure_goto(p, 2, p, 0); break;
    case 5: /* Räume von Spieler 1 besuchen */
    case 6: /* Räume des anderen KI-Spielers besuchen */
        visit = DS_ADDR(int8_t, (*act == 5 ? 0x24d3 : 0x24d4) + AI_IDX * 2);
        if (*visit % 3 == 0 && PLAYER_METER(p) > 0x13) {
            int16_t owner = *act == 5 ? 1 : DS_I8(0xf2 + (AI_IDX == 0));
            c = (int8_t)(*visit / 3);
            if (c == 0)
                ai_figure_goto(p, 2, owner, 0);
            else if (c == 1)
                ai_figure_goto(p, 1, owner, 0);
            else if (c == 2)
                ai_figure_goto(p, 4, owner, 0);
        }
        c = (*visit)++;
        if (c > 9)
            *visit = 0;
        break;
    case 7:
        ai_choose_studio_action(p);
        switch (DS_I8(0x262c + p * 3)) {
        case 1: ai_figure_goto(p, 10, 0, 0); break;
        case 2: case 3: ai_figure_goto(p, 9, 0, 0); break;
        case 4: ai_figure_goto(p, 1, p, 0); break;
        case 5: ai_figure_goto(p, 0xc, 0, 0); break;
        case 6: ai_figure_goto(p, 5, p, DS_I8(0x262a + p * 3)); break;
        }
        break;
    case 9: ai_figure_goto(p, 0xc, 0, 0); break;
    case 10: ai_figure_goto(p, 0xe, 0, 0); break;
    case 11: ai_figure_goto(p, 0xd, 0, 0); break;
    case 12: ai_figure_goto(p, 0x40, 0, 0); break;
    }
    AI_COUNT1 = 0;
    AI_COUNT2 = 0;
}

/* 3b0a:3cb4 – zur anderen KI wechseln */
void ai_switch_player(void)
{
    DS_I8(0x24e7) = DS_I8(0x24e7) == 0;
}

/* 3b0a:3cc5 – KI-Figur zu einer Tür schicken */
void ai_figure_goto(int16_t figure, int16_t room_type, int16_t owner, int16_t sub)
{
    uint8_t *f = FIGURE(figure - 1);
    int16_t floor, door;
    building_find_door((uint16_t)room_type, (uint16_t)owner, (uint16_t)sub, &floor, &door);
    if (W(f, 6) == floor && W(f, 0x12) == door) {
        if (DS_I8(0xf2) == figure)
            DS_I8(0x24e8 + AI_IDX) = 1; /* ORIGINAL: nur für die erste KI */
    } else {
        W(f, 0x14) = W(f, 0x12);
        W(f, 6) = floor;
        W(f, 0x12) = door;
        W(f, 4) = W(f, 6) == W(f, 2) ? DS_U8(0x7c62 + floor * 0x7a + door) + 8 : 0x99;
    }
}

/* 3b0a:3d84 – nächste Studioaktion bestimmen (DS:262a Studio, 262b Anzahl, 262c Aktion) */
void ai_choose_studio_action(int8_t player)
{
    int16_t free = 0, shows = 0;
    uint8_t *s = DS_ADDR(uint8_t, 0x262a + player * 3);
    s[2] = 0xff;
    for (int16_t i = 1; i < 0xd; i++) {
        uint8_t *st = DS_ADDR(uint8_t, 0x6145 + i * 0x10);
        if ((int8_t)st[3] != player)
            continue;
        if (W(st, 0) == 0) {
            free++;
        } else if (episode_cast_complete(W(st, 0), (int8_t)st[4]) == 1) {
            /* produzieren nur, wenn das Studio frei ist (+0xa, +0xe); sonst fehlende Besetzung (5) */
            if (W(st, 0xa) == 0 && st[0xe] == 0) {
                s[2] = 6;
                s[0] = (uint8_t)i;
            }
        } else {
            s[2] = 5;
            s[0] = (uint8_t)i;
        }
    }
    if ((int8_t)s[2] == -1) {
        if (DS_I16(0x7652 + player * 2) != 0)
            shows = DS_I16(0x7652 + player * 2);
        if (shows != 0 && free != 0) {
            s[2] = 4;
        } else if (shows == 0 && free == 0) {
            s[2] = 2;
        } else if (shows == 0) {
            s[2] = 3;
            s[1] = (uint8_t)free;
        } else {
            s[2] = 1;
            s[1] = (uint8_t)shows;
        }
    }
}

/* 3b0a:3ea3 – mittlere Qualität einer Show (Folgen: Byte +8 je Folge) */
int8_t production_avg_quality(int16_t prod_id)
{
    uint8_t *sh = SHOW(prod_id);
    if (UW(sh, 5) & 1) {
        int16_t n = show_episode_count(prod_id);
        int8_t sum = 0;
        for (int8_t e = 0; e < n; e++)
            sum += (int8_t)sh[e * 0x23 + 8];
        return (int8_t)(sum / n);
    }
    return (int8_t)sh[8];
}

/* 3b0a:3f21 – mittlere Qualität der gekauften, noch nicht produzierten Shows (ein Eintrag zu viel) */
int16_t ai_unfinished_avg_quality(int8_t player)
{
    /* ohne Einträge 0, sonst Durchschnitt über die n Einträge */
    int16_t sum = 0, n = DS_I16(0x7652 + player * 2);
    if (n == 0)
        return 0;
    for (int8_t i = 0; i < n; i++)
        sum += production_avg_quality(DS_I16(0x1a25 + player * 0x32 + i * 5));
    return (int8_t)(sum / n);
}

/* 3b0a:3f93 – Tagesbeginn der KI: Budgets, Vertragsziele, Abos */
void ai_day_plan(void)
{
    DS_I8(0x24c5) = -1;
    DS_I8(0x24c6) = -1;
    for (int8_t i = 0; i < 2; i++) {
        int8_t p = DS_I8(0xf2 + i);
        uint8_t *b = BUDGET(p);
        int8_t slot = DS_I8(0x7225 + i)++;
        L(b, 0x2c + slot * 4) = L(b, 0x10);
        if (DS_I8(0x7225 + i) > 3)
            DS_I8(0x7225 + i) = 1;
        ai_update_avg_income(p);
        DS_I16(0x24d7 + p * 2) = ai_eval_ratings(0, p);
        AI_MIN_QUALITY(p) = ai_eval_ratings(1, p);
        DS_U8(0x24ea + p) = 0;
        DS_I8(0x7894 + p) = DS_I8(0x7898 + p);
        DS_I8(0x7898 + p) = 0;
        far_memset(DS_ADDR(uint8_t, 0x2423 + p * 0x10), 0x10);
        for (int8_t c = 0; c < AD_CONTRACT_COUNT(p); c++) {
            int8_t *ac = AI_CONTRACT(p, c);
            ac[2] = ac[3];
            ac[3] = 0;
            UW(ac, 6) &= ~3;
            ac[0] = ai_ad_spots_per_day(c, p);
            ac[1] = ai_ad_spots_per_day(c, p);
        }
        ai_budget_plan(p);
        DS_I8(0x24ee + p) = (int8_t)ai_unfinished_avg_quality(p);
        for (int16_t c = 1; c < 4; c++)
            DS_U8(0x8bcc + p * 0x17 + c) = 0;
        int32_t news = L(b, 0xc);
        if (PLAYER_IMG(p) > 4)
            DS_U8(0x8bcd + p * 0x17) = 1;
        if (PLAYER_IMG(p) > 9 && news > 20000)
            DS_U8(0x8bce + p * 0x17) = 1;
        if (PLAYER_IMG(p) > 0xe && news > 40000)
            DS_U8(0x8bcf + p * 0x17) = 1;
    }
    ai_update_ad_contracts();
}

/* 3b0a:4219 – Lizenzfilme (Live-Übertragungen) auf ihren festen Sendetermin legen */
uint16_t ai_schedule_live(void)
{
    int8_t p = AI_PLAYER;
    int16_t n = ARCHIVE_COUNT(p, 7);
    for (int16_t k = 0; k < n; k++) {
        uint8_t *f = FILM(ARCHIVE(p, 7, k));
        if (UW(f, 7) & 2)
            continue;
        int16_t h = 0;
        if (W(f, 0xf) == g_day)
            h = W(f, 0x11);
        else if (W(f, 0xf) == g_day + 1)
            h = W(f, 0x11) + 7;
        if (h == 0)
            continue;
        if (SCHEDULE(p, h) == 9999) {
            int16_t s = h;
            while (SCHEDULE(p, s) == 9999)
                s--;
            schedule_clear_programme(p, s);
        }
        schedule_place_programme(k, 7, h, p);
        programmes_mark_scheduled();
    }
    return 0;
}

/* 3b0a:4344 – Senderstandort suchen (3 Rasterpunkte pro Aufruf); am Ende den besten kaufen */
void ai_state_news_event(void)
{
    int8_t p = AI_PLAYER, done = 0;
    uint8_t *s = DS_ADDR(uint8_t, 0x2423 + p * 0x10); /* +0 x, +2 y, +4/+6 bester Punkt, +8 Preis, +0xc Gewinn */
    for (int16_t i = 0; i < 3 && !done; i++) {
        int32_t gain = station_reach_gain(W(s, 0), W(s, 2));
        if (gain > L(s, 0xc)) {
            int32_t price = price_step_next(station_reach_at(W(s, 0), W(s, 2)) * 0x268 / 10);
            if (price >= 50000 && price <= L(BUDGET(p), 0x14)) {
                L(s, 8) = price;
                L(s, 0xc) = gain;
                W(s, 4) = W(s, 0);
                W(s, 6) = W(s, 2);
            }
        }
        W(s, 0) += 3;
        if (W(s, 0) >= 0x96) {
            W(s, 0) = 0;
            W(s, 2) += 3;
            if (W(s, 2) >= 0x5a)
                done = 1;
        }
    }
    if (done != 1)
        return;
    DS_U8(0x24ea + p) = 0;
    int16_t i;
    for (i = 3; i <= 0x20; i++)
        if (DS_I32(0x1ced + p * 0x1ce + i * 0xe) <= 0)
            break;
    uint8_t *st = DS_ADDR(uint8_t, 0x1ce9 + p * 0x1ce + i * 0xe);
    L(st, 4) = L(s, 0xc);
    L(st, 0) = round_to_step(L(s, 8), 1000);
    W(st, 8) = W(s, 4);
    W(st, 0xa) = W(s, 6);
    W(st, 0xc) = 1;
    PLAYER_MONEY(p) -= L(st, 0);
    coverage_stamp(i, p, DS_PTR(uint16_t, 0x7c5e), DS_PTR(uint16_t, 0x8b8a));
    stations_reach_update(p);
    far_memset(s, 0x10);
    L(BUDGET(p), 0x14) -= L(st, 0);
    ai_goto_next();
}

/* 3b0a:462c – darf die KI heute einen Sender kaufen? 0 nein, 1 ja, 2 Mensch ist im Sendernetz-Raum */
int8_t ai_check_can_act(void)
{
    int8_t r = 1;
    int8_t p = AI_PLAYER;
    if (L(BUDGET(p), 0x14) <= 50000 || g_hour < 0x12)
        r = 0;
    if (DS_U8(0x24ea + DS_I8(0xf2 + (AI_IDX == 0))) != 0)
        r = 0;
    if (g_cur_room == 0x19)
        r = 2;
    return r;
}

/* 3b0a:46aa – Abdeckungskarte schrittweise aufbauen (ein Sender pro Aufruf, Zähler DS:24cb) */
void ai_state_show_event(void)
{
    int8_t p = AI_PLAYER;
    uint16_t i = (uint16_t)AI_COUNT2;
    uint16_t *cov = DS_PTR(uint16_t, 0x8b8a), *pop = DS_PTR(uint16_t, 0x7c5e);
    if (i == 0)
        far_memset(cov, 0x11e2);
    uint8_t *st = DS_ADDR(uint8_t, 0x1ce9 + p * 0x1ce + i * 0xe);
    if (L(st, 4) != 0 && W(st, 0xc) != 2) {
        /* ORIGINAL-BUG: Zeilenbreite 0x2a statt 0x15 Wörter (Puffer im Port vergrößert) */
        if (i == 1) {
            for (int16_t y = 0; y < 0x6c; y++)
                for (int16_t x = 0; x < 0x15; x++)
                    cov[y * 0x2a + x] = pop[y * 0x2a + x];
        } else if (i == 2) {
            for (int16_t y = 0; y < 0x6c; y++)
                for (int16_t x = 0x15; x < 0x2a; x++)
                    cov[y * 0x2a + x] = pop[y * 0x2a + x];
        } else {
            coverage_stamp((int16_t)i, p, pop, cov);
        }
    }
    if (i == 0x20) {
        DS_I32(0x7ab2) = map_population_sum(0, 0, 0x54, 0x6d, cov);
        DS_I32(0x7ab6) = map_population_sum(0x54, 0, 0xa8, 0x6d, cov);
    }
}

/* 3b0a:4839 – mittleres Tagesbudget der letzten drei Tage */
void ai_update_avg_income(int8_t player)
{
    int32_t sum = 0;
    for (int16_t k = 1; k < 4; k++)
        sum += L(BUDGET(player), 0x2c + k * 4);
    L(BUDGET(player), 0x2c) = sum / 3;
}
