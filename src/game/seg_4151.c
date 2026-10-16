/* Segment 4151 – KI: Hilfsfunktionen, Budget, Werbeverträge, Studioproduktion.
 *
 * Die beiden KI-Spieler stehen in DS:00f2[0..1]; der gerade aktive ist DS:00f2[DS:24e7].
 * KI-Budget DS:6e09 + p*0x3c: +0..+0x18 sieben Konten (long), +0x1c sieben Prozentanteile,
 *   +0x23.., +0x2b, +0x2c vier Tagesbudgets (long, Index DS:7225[ki]).
 * KI-Werbeverträge DS:4ebf + p*0x4d (7 B je Vertrag, parallel zur Liste DS:1b50):
 *   +0 Spots Prime, +1 Spots sonst, +2/+3 gesendete Prime/sonst, +4 gesendet gesamt, +6 Erfüllt-Bits. */
#include <stdlib.h>
#include "game.h"

#define AI_PLAYER         DS_I8(0xf2 + DS_I8(0x24e7))
#define BUDGET(p)         DS_ADDR(uint8_t, 0x6e09 + (p) * 0x3c)
#define DAY_BUDGET(p)     (*(int32_t *)(BUDGET(p) + 0x2c + DS_I8(0x7225 + DS_I8(0x24e7)) * 4))
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define SHOW(n)           (DS_PTR(uint8_t, 0x713c) + (n) * 0x1b3)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define ARCHIVE(p, g, k)  DS_I16(0x6ef9 + (p) * 0x6c + (g) * 8 + (k) * 2)
#define ARCHIVE_COUNT(p, g) DS_I8(0x6f59 + (p) * 0x6c + (g))
#define AD(n)             DS_ADDR(uint8_t, 0x3f2f + (n) * 0x18)
#define AD_CONTRACT(p, i) DS_ADDR(uint8_t, 0x1b50 + (p) * 0x46 + (i) * 7)
#define AD_CONTRACT_COUNT(p) DS_I16(0x7688 + (p) * 2)
#define AI_CONTRACT(p, i) DS_ADDR(int8_t, 0x4ebf + (p) * 0x4d + (i) * 7)
#define STUDIO(i)         DS_ADDR(uint8_t, 0x6145 + (i) * 0x10)
#define SHOW_OFFER(i)     DS_ADDR(uint8_t, 0x194a + (i) * 9)
#define AI_PROD(p, i)     DS_ADDR(uint8_t, 0x1a25 + (p) * 0x32 + (i) * 5)
#define AI_PROD_COUNT(p)  DS_I16(0x7652 + (p) * 2)
#define AI_STUDIO(p)      DS_I8(0x262a + (p) * 3)

/* gekaufte/bezahlte Beträge bei der KI verbuchen */
static void ai_spend(int8_t p, int32_t amount)
{
    PLAYER_MONEY(p) -= amount;
    L(BUDGET(p), 0x10) -= amount;
    DAY_BUDGET(p) -= amount;
    ai_update_avg_income(p);
}

/* 4151:0002 – noch nicht eingeplante einstündige Filme und Serien im Archiv */
int8_t ai_count_short_programmes(void)
{
    int8_t p = AI_PLAYER, n = 0;
    for (int16_t g = 1; g < 0xb; g++) {
        for (int16_t k = 0; k < ARCHIVE_COUNT(p, g); k++) {
            uint8_t *f = FILM(ARCHIVE(p, g, k));
            if (f[4] == 1 && !(UW(f, 7) & 2))
                n++;
        }
    }
    for (int16_t k = 0; k < ARCHIVE_COUNT(p, 0xb); k++) {
        uint8_t *s = SERIES(ARCHIVE(p, 0xb, k));
        /* ORIGINAL: prüft das Wort bei Archiv+0x60+k*2 (Genre-Zähler) statt eines Eintrags */
        if (DS_I16(0x6f59 + p * 0x6c + k * 2) != 0 && !(UW(s, 0x26) & 2) && s[0x23] == 1)
            n++;
    }
    return n;
}

/* 4151:010c – freie Lücken ab der nächsten Stunde mit mindestens min_len Stunden */
int8_t ai_find_free_slots(int8_t *out_hours, int8_t min_len)
{
    int16_t from = g_minute < 6 ? hour_index() : hour_index() + 1;
    int8_t n = 0, p = AI_PLAYER;
    if (from == 0)
        from = 1;
    for (int16_t h = from; h < 0xf; h++) {
        if (SCHEDULE(p, h) != 0)
            continue;
        int16_t stop = h < 8 ? 8 : 7, len = 0;
        for (int16_t x = h; SCHEDULE(p, x) == 0 && x != stop && x < 0xf; x++)
            len++;
        if (len >= min_len)
            out_hours[n++] = (int8_t)h;
    }
    return n;
}

/* 4151:01e0 – 1, wenn alle Werbeverträge erfüllt sind */
int8_t ai_ads_all_satisfied(void)
{
    int8_t p = AI_PLAYER, ok = 1;
    for (int8_t i = 0; i < AD_CONTRACT_COUNT(p); i++) {
        uint16_t bits = UW(AI_CONTRACT(p, i), 6);
        if ((!(bits & 1) || !(bits & 2)) && !(bits & 4))
            ok = 0;
    }
    return ok;
}

/* 4151:0276 – Zielgruppen-Maske (DS:7744) und mittlere Mindestreichweite (DS:7746) der Verträge */
void ai_update_ad_contracts(void)
{
    int8_t p = AI_PLAYER;
    int16_t n = AD_CONTRACT_COUNT(p);
    uint16_t mask[11] = {0}; /* im Original für leere Einträge uninitialisiert */
    uint16_t all = 0;
    int32_t sum = 0, avg;
    static int8_t neg; /* im Original eine nicht initialisierte lokale Variable, die von Vertrag zu Vertrag stehen bleibt */
    if (n == 0) {
        avg = 0;
        all = 0xffff;
    } else {
        for (int16_t i = 0; i < n; i++) {
            int16_t ad = W(AD_CONTRACT(p, i), 0);
            uint8_t *a = AD(ad);
            if (ad == 0 || (int8_t)a[9] <= AI_CONTRACT(p, i)[4]) {
                if (ad != 0)
                    mask[i] = 0;
                continue;
            }
            sum += DS_I32(0xa8 + ((int8_t)a[0xa] - 1) * 4);
            mask[i] = 0;
            for (int16_t k = 1; k < 4; k++) {
                int8_t c = (int8_t)a[0xb + k];
                if (c != 0)
                    neg = c < 0;
                bit_set(&mask[i], (int16_t)(abs(c) - 1));
            }
            if (mask[i] == 0)
                mask[i] = 0xffff;
            else if (neg)
                mask[i] = (uint16_t)~mask[i];
        }
        for (int16_t i = 0; i < n; i++)
            all |= mask[i];
        avg = sum / n;
    }
    DS_I32(0x7746 + p * 6) = avg;
    DS_U16(0x7744 + p * 6) = all;
}

/* 4151:0480 – Betrag auf die sieben Budgetkonten verteilen; bei Werbegeld (idx >= 0) nur,
 * wenn der Vertrag erfüllt ist */
void ai_adjust(int16_t player, int32_t amount, int16_t contract_idx)
{
    int8_t p = (int8_t)player;
    uint8_t *b = BUDGET(p);
    if (contract_idx >= 0) {
        uint8_t *c = AD_CONTRACT(p, contract_idx);
        if ((int8_t)c[4] < (int8_t)AD(W(c, 0))[9])
            amount = 0;
    }
    for (int8_t k = 0; k <= 6; k++)
        L(b, k * 4) += amount * (int8_t)b[0x1c + k] / 100;
}

/* 4151:0553 – neue KI: Budget zurücksetzen, 4/5 des Geldes als Konto 2, Rest verteilen */
void ai_init_new_game(void)
{
    for (int16_t i = 0; i < 2; i++) {
        int8_t p = DS_I8(0xf2 + i);
        uint8_t *b = BUDGET(p);
        for (int16_t k = 0; k < 7; k++) {
            b[0x1c + k] = 0;
            b[0x23 + k] = 1;
        }
        DS_U8(0x262a + p * 3) = 0;
        DS_U8(0x262b + p * 3) = 0;
        for (int16_t k = 0; k <= 3; k++)
            L(b, 0x2c + k * 4) = 0;
        DS_U8(0x7225 + i) = 0;
        b[0x2b] = 3;
        L(b, 8) = (PLAYER_MONEY(p) << 2) / 5;
        L(b, 0x14) = 0xc15c;
        PLAYER_MONEY(p) = PLAYER_MONEY(p) / 5 - 0xc15c;
        ai_budget_plan(p);
        ai_adjust(p, PLAYER_MONEY(p), -1);
    }
}

/* 4151:06a8 – gesendeten Spot eines Vertrags zählen (add) bzw. zurücknehmen */
void ai_ad_spot_count(int16_t hour, int16_t contract_idx, int8_t add, int8_t player)
{
    int8_t d = add ? 1 : -1;
    int8_t *c = AI_CONTRACT(player, contract_idx);
    if (hour < 8)
        c[2] += d;
    else
        c[3] += d;
    c[4] += d;
    uint16_t *bits = (uint16_t *)(c + 6);
    *bits = c[2] < c[0] ? (*bits & ~1) : (*bits | 1);
    *bits = c[3] < c[1] ? (*bits & ~2) : (*bits | 2);
    *bits = c[4] < (int8_t)AD(W(AD_CONTRACT(player, contract_idx), 0))[9] ? (*bits & ~4) : (*bits | 4);
}

/* 4151:0786 */
void ai_ad_contract_init(int16_t contract_idx)
{
    int8_t p = AI_PLAYER;
    int8_t *c = AI_CONTRACT(p, contract_idx);
    c[0] = ai_ad_spots_per_day(contract_idx, p);
    c[1] = ai_ad_spots_per_day(contract_idx, p);
    c[2] = 0;
    c[3] = 0;
    c[4] = 0;
    *(uint16_t *)(c + 6) &= ~7;
}

/* 4151:080a – mittlere Aktualität des Archivs (zählt je Genre einen Eintrag zu viel, wie das Original) */
int16_t ai_archive_avg_quality(int8_t player)
{
    int16_t sum = 0, n = 0;
    for (int8_t g = 0; g < 0xc; g++) {
        for (int8_t k = 0; k <= ARCHIVE_COUNT(player, g); k++) {
            if (g < 0xb)
                sum += (int8_t)FILM(ARCHIVE(player, g, k))[3];
            else
                sum += series_avg_quality(ARCHIVE(player, g, k));
            n++;
        }
    }
    if (n == 0) /* Release-Fassung: keine Division durch 0 */
        n = 1;
    return (int16_t)(sum / n);
}

/* 4151:08e8 – freies Studio mit bestem Preis/Größe-Verhältnis im Budget mieten */
void ai_rent_studio(void)
{
    int8_t p = AI_PLAYER, i;
    int32_t best = 0, budget = L(BUDGET(p), 0x2c) / 10;
    int16_t pick = 0;
    for (i = 1; i <= 0xc; i++) {
        uint8_t *st = STUDIO(i);
        if (st[3] != 0)
            continue;
        int32_t rent = L(st, 6), v = rent / (int8_t)st[2];
        if (rent <= budget && v > best) {
            best = v;
            pick = i;
        }
    }
    if (pick != 0)
        studio_rent_request(p, i); /* ORIGINAL-BUG: Schleifenzähler (13) statt des gewählten Studios */
    ai_goto_next();
}

/* 4151:09fd – Show aus dem Angebot kaufen (beste Qualität, Preis bis 30 % von Konto 4) */
void ai_buy_production(void)
{
    int8_t p = AI_PLAYER, best = 0, biggest = 0, q;
    uint8_t *dst = AI_PROD(p, AI_PROD_COUNT(p));
    int16_t pick = 0;
    int32_t budget = L(BUDGET(p), 0x10) * 3 / 10;
    for (int8_t i = 1; i <= 0xc; i++) { /* größtes belegtes eigenes Studio (wird nicht benutzt) */
        uint8_t *st = STUDIO(i);
        if ((int8_t)st[3] == p && st[0xe] != 0 && (int8_t)st[2] > biggest)
            biggest = (int8_t)st[2];
    }
    (void)biggest;
    for (int8_t i = 1; i <= 5; i++) {
        uint8_t *e = SHOW_OFFER(i);
        if (e[2] != 0)
            continue;
        int16_t show = W(e, 0);
        int8_t genre = (UW(SHOW(show), 5) & 1) ? 0xb : (int8_t)FILM(W(SHOW(show), 6))[0];
        if (L(e, 4) > budget || ARCHIVE_COUNT(p, genre) > 3)
            continue;
        q = production_avg_quality(show);
        if (q >= best && DS_I8(0x24ee + p) <= q) {
            best = q;
            pick = i;
        }
    }
    if (pick != 0) {
        uint8_t *e = SHOW_OFFER(pick);
        W(dst, 0) = W(e, 0);
        dst[2] = e[8];
        W(dst, 3) = 1;
        AI_PROD_COUNT(p)++;
        ai_spend(p, L(e, 4));
        e[2] -= 100;
        for (int8_t i = 1; i < 0xb; i++)
            if ((int8_t)SHOW_OFFER(i)[2] < 0)
                far_memset(SHOW_OFFER(i), 9);
        production_market_update();
    }
    ai_goto_next();
}

/* 4151:0d1a – Folge im gemieteten Studio produzieren */
void ai_produce_episode(void)
{
    int8_t p = AI_PLAYER;
    int16_t slot = AI_STUDIO(p);
    uint8_t *st = STUDIO(slot);
    uint8_t *sh = SHOW(W(st, 0));
    /* Release-Fassung: Genre über den Film bzw. die Serie der Show (die Beta nahm die
     * Shownummer); ohne die Ausnahme für Bit 4 */
    int16_t genre = (UW(sh, 5) & 1) ? (int8_t)SERIES(W(sh, 6))[0x21] + 10 : (int8_t)FILM(W(sh, 6))[0];
    if (st[0xe] == 0) {
        if ((UW(sh, 5) & 1) || archive_contains(p, genre, W(sh, 6)) == 0)
            studio_produce_episode(AI_STUDIO(p));
    }
    ai_goto_next();
}

/* 4151:0e28 – nächste gekaufte Show in ein freies eigenes Studio legen */
void ai_prepare_episode(void)
{
    int8_t p = AI_PLAYER;
    int16_t show = 0, slot = 0, found = 0;
    if (AI_PROD_COUNT(p) != 0)
        show = W(AI_PROD(p, AI_PROD_COUNT(p) - 1), 0);
    for (int16_t i = 1; i <= 0xc && !found; i++) {
        uint8_t *st = STUDIO(i);
        if ((int8_t)st[3] != p)
            continue;
        if (W(st, 0xa) == 0 && W(st, 0) != 0) {
            show = W(st, 0);
        } else {
            if (W(st, 0) != 0 || show == 0 || (int8_t)st[2] < show_role_count(show))
                continue;
            W(st, 0) = show;
            st[4] = AI_PROD(p, AI_PROD_COUNT(p) - 1)[3];
            st[5] = (uint8_t)(st[4] - 1);
            W(st, 0xa) = 0;
            list5_remove(p, AI_PROD_COUNT(p) - 1);
        }
        found = 1;
        slot = i;
    }
    if (found)
        studio_prepare_episode(slot);
    ai_goto_next();
}

/* 4151:0f75 – Rollen der aktuellen Folge besetzen (Budget: 60 % von Konto 4, nach Rollengewicht) */
void ai_cast_episode(void)
{
    int8_t p = AI_PLAYER;
    int16_t slot = AI_STUDIO(p);
    int16_t show = W(STUDIO(slot), 0);
    uint8_t *sh = SHOW(show);
    int16_t ep = (int8_t)STUDIO(slot)[4] - 1;
    uint8_t *e = sh + ep * 0x23 + 8;           /* Rolle r: e + r*3 + 1 Typ, +2 Gewicht, +3 Darsteller */
    int16_t weights = 0, best = 0, pick = 0;
    int32_t fee = 0;                            /* bleibt von Rolle zu Rolle stehen wie im Original */
    int32_t budget = L(BUDGET(p), 0x10) * 0x3c / 100;
    int8_t r;
    for (r = 1; r <= 10; r++)
        if (e[r * 3 + 1] != 0)
            weights += (int8_t)e[r * 3 + 2];
    if (weights == 0)
        weights = 1;
    for (r = 1; r <= 10; r++) {
        int8_t type = (int8_t)e[r * 3 + 1];
        if (type == 0)
            continue;
        int8_t group = (int8_t)sh[type + 0x1a4];
        int16_t share = (int16_t)((int8_t)e[r * 3 + 2] * 100 / weights);
        int32_t limit = (int32_t)share * budget / 100;
        if (type <= 0xf && group != 0) {
            if (group < 8) {
                for (int8_t a = 0; a <= 0xf; a++) {
                    int32_t base = DS_I32(0x4ff3 + (group - 1) * 0xc0 + a * 0xc + 2);
                    if (base <= 0)
                        continue;
                    fee = base * (int8_t)e[2];
                    if (fee > limit)
                        continue;
                    /* ORIGINAL-BUG: übergibt den Rollentyp statt des Darstellers */
                    int16_t s = actor_suitability(show, ep + 1, r, type);
                    if (s > best) {
                        best = s;
                        pick = a;
                    }
                }
            } else {
                for (int8_t a = 0; a <= 0xf; a++) {
                    for (int8_t g = 0; g <= 6; g++) {
                        int32_t base = DS_I32(0x4ff3 + g * 0xc0 + a * 0xc + 2);
                        if (base == 0 || g == 4)
                            continue;
                        fee = base * (int8_t)e[2];
                        if (fee > limit)
                            continue;
                        int16_t s = actor_suitability(show, ep + 1, r, type);
                        if (s > best) {
                            best = s;
                            pick = a;
                        }
                    }
                }
            }
            if (best != 0) {
                e[r * 3 + 3] = (uint8_t)pick;
                ai_spend(p, fee); /* ORIGINAL-BUG: Gage des zuletzt geprüften Darstellers */
            }
            best = 0;
        } else {
            for (int8_t k = 1; k <= 3; k++) { /* Stab: drei Preisstufen */
                fee = DS_I32(0x6215 + (type - 0xf) * 0x10 + k * 4) * (int8_t)e[2];
                if (fee <= limit)
                    pick = k;
            }
            if (pick != 0) {
                e[r * 3 + 3] = (uint8_t)pick;
                ai_spend(p, fee); /* ORIGINAL-BUG: Preis der Stufe 3 */
            }
        }
        pick = 0;
    }
    ai_goto_next();
}
