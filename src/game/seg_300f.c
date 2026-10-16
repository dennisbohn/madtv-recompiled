/* Segment 300f – Tageswechsel der Statistiken, Studios, Sender, Produktionskosten, Koffer. */
#include "game.h"

#define g_shows           DS_PTR(uint8_t, 0x713c)
#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define STUDIO(i)         DS_ADDR(uint8_t, 0x6145 + (i) * 0x10)
#define PRODUCTION(p, i)  DS_ADDR(uint8_t, 0x732c + (p) * 0x4b + (i) * 0xf)
#define PRODUCTION_COUNT(p) DS_I16(0x7460 + (p) * 2)
#define SUITCASE(i)       DS_I16(0x2463 + (i) * 2)
#define SUITCASE_COUNT    DS_I16(0x732a)

/* 300f:000b – Finanzstatistik: heute -> gestern (DS:7472), neuer Tag beginnt mit dem Kontostand */
void finance_day_rotate(void)
{
    far_memcpy(DS_ADDR(uint8_t, 0x7472), DS_ADDR(uint8_t, 0x71d1), 0x54);
    far_memset(DS_ADDR(uint8_t, 0x71d1), 0x54);
    DS_I32(0x71d1) = PLAYER_MONEY(g_local_player);
}

/* 300f:0052 – Quotenstatistik: Tageswerte sichern, 7-Tage-Verlauf weiterschieben */
void stats_day_rotate(void)
{
    for (int16_t p = 1; p < 4; p++) {
        far_memcpy(DS_ADDR(uint8_t, 0x1c6a + p * 0x1e), DS_ADDR(uint8_t, 0x1c78 + p * 0x1e), 0xe);
        far_memset(DS_ADDR(uint8_t, 0x1c78 + p * 0x1e), 0xe);
    }
    far_memcpy(DS_ADDR(uint8_t, 0x789e), DS_ADDR(uint8_t, 0x74ca), 0x80);
    far_memcpy(DS_ADDR(uint8_t, 0x791e), DS_ADDR(uint8_t, 0x7690), 0x80);
    far_memcpy(DS_ADDR(uint8_t, 0x5553), DS_ADDR(uint8_t, 0x5533), 0x20);
    for (int16_t p = 1; p < 4; p++) {
        int32_t *hist = DS_ADDR(int32_t, 0x6d89 + p * 0x20);
        for (int16_t k = 1; k < 7; k++)
            hist[k] = hist[k + 1];
        int32_t sum = 0;
        for (int16_t h = 1; h < 8; h++)
            sum += DS_I32(0x74ca + p * 0x20 + h * 4);
        hist[7] = sum / 7;
        /* das Original summiert danach den Verlauf noch einmal, ohne das Ergebnis zu verwenden */
    }
    far_memset(DS_ADDR(uint8_t, 0x74ca), 0x80);
    far_memset(DS_ADDR(uint8_t, 0x7690), 0x80);
}

/* 300f:0204 – Studioplatz freigeben und zugehörige Produktionen aus der Liste entfernen */
void studio_slot_release(int16_t slot)
{
    uint8_t *st = STUDIO(slot);
    if (W(st, 0) == 0)
        return;
    int16_t owner = (int8_t)st[3];
    int16_t show = W(st, 0);
    W(st, 0) = 0;
    st[5] = 0;
    st[4] = 1;
    if (show > 0) {
        for (int16_t i = 0; i < PRODUCTION_COUNT(owner); i++) {
            if (W(PRODUCTION(owner, i), 0) == show) {
                production_remove(owner, i);
                i--;
            }
        }
    }
}

/* 300f:0286 – Senderkäufe/-verkäufe des Tages abrechnen, Reichweite neu berechnen */
void stations_settle(int16_t player)
{
    uint8_t *base = DS_ADDR(uint8_t, 0x1ce9 + player * 0x1ce);
    W(base, 0xc) = 0;
    for (int16_t i = 1; i < 0x21; i++) {
        uint8_t *st = base + i * 0xe;
        if (W(st, 0xc) == 2) { /* verkauft */
            L(st, 4) = 0;
        } else if (W(st, 0xc) == 1) { /* gekauft */
            PLAYER_MONEY(player) -= L(st, 0);
            money_add(3, L(st, 0));
        }
        W(st, 0xc) = 0;
    }
    stations_reach_update(player);
}

/* 300f:03a5 */
void production_clear_first(int16_t player)
{
    far_memset(PRODUCTION(player, 0), 0xf);
}

/* 300f:03c2 – Eintrag aus dem Koffer nehmen */
void case_remove(int16_t idx)
{
    for (; idx < SUITCASE_COUNT; idx++)
        far_memcpy(&SUITCASE(idx), &SUITCASE(idx + 1), 2);
    SUITCASE_COUNT--;
}

/* 300f:03f8 – Gagen für neu besetzte Rollen verbuchen, Studiokosten (DS:74c6) abrechnen */
void production_costs_update(int16_t player)
{
    for (int16_t i = 0; i < PRODUCTION_COUNT(player); i++) {
        uint8_t *pr = PRODUCTION(player, i);
        int16_t ep = W(pr, 2);
        uint8_t *sh = g_shows + W(pr, 0) * 0x1b3;
        for (int16_t r = 1; r < 0xb; r++) {
            uint8_t *role = sh + (ep - 1) * 0x23 + r * 3;
            int16_t actor = (int8_t)role[0xb];
            /* Release-Fassung (322a:0424): ohne Darsteller keine Gage, Typ 0xf aus der ersten
             * Tabelle mit Index sh[Typ + 0x1a3] - 1 (Beta: Typ < 0xf, sh[Typ + 0x1a4]), Gage mal
             * Byte 0xa der Folge */
            if (player == g_local_player && actor != (int8_t)pr[r + 4] && actor != 0) {
                int16_t type = (int8_t)role[9];
                int32_t fee;
                if (type <= 0xf)
                    fee = DS_I32(0x4ff3 + ((int8_t)sh[type + 0x1a3] - 1) * 0xc0 + actor * 0xc + 2);
                else
                    fee = DS_I32(0x6215 + (type - 0xf) * 0x10 + actor * 4);
                fee *= (int8_t)sh[(ep - 1) * 0x23 + 0xa];
                money_add(7, fee);
            }
            pr[r + 4] = role[0xb];
        }
    }
    if (DS_I8(0x74c6 + player) > 100) {
        DS_I8(0x74c6 + player) -= 100;
        if (player == g_local_player)
            money_add(6, DS_I32(0x722c + DS_I8(0x74c6 + player) * 8));
    }
}

/* 300f:0599 – Programme aus dem Koffer ins Archiv legen, soweit Platz ist */
void case_store_to_archive(void)
{
    int16_t p = g_local_player;
    for (int16_t i = 0; i < SUITCASE_COUNT; i++) {
        int16_t prog = SUITCASE(i), genre;
        /* Release-Fassung: Besitzer auf den Spieler setzen (auch wenn das Fach voll ist) */
        if (prog < 1000) {
            FILM(prog)[5] = (uint8_t)p;
            genre = (int8_t)FILM(prog)[0];
            if (prog == 0x1d) {
                DS_I16(0x7a22) = 3;
                special_film_1d_tick();
            } else if (prog == 0xbb) {
                DS_I16(0x7a7c) = 5;
                special_film_bb_tick();
            }
        } else {
            (DS_PTR(uint8_t, 0x2626) + (prog - 1000) * 0x39)[0x24] = (uint8_t)p;
            genre = 0xb;
        }
        int8_t *count = DS_ADDR(int8_t, 0x6f59 + p * 0x6c + genre);
        if (*count < 4) {
            DS_I16(0x6ef9 + p * 0x6c + genre * 8 + *count * 2) = prog;
            if (genre == 0xb)
                DS_I16(0x6f51 + p * 0x6c + *count * 2) -= 1000;
            (*count)++;
            case_remove(i);
            i--;
        }
    }
}
