/* Segment 2a62 – Spieldatenbank laden, Geld, Nachrichten, Programmlisten.
 *
 * Datenbanken (Zeiger im Datensegment, Speicher in game_main angelegt):
 *   DS:2489 Filme   201 x 23 Byte   (201.HES)
 *   DS:2626 Serien   51 x 57 Byte   (202.HES)
 *   DS:713c Folgen   51 x 435 Byte  (203.HES; je Serie 8 Byte Kopf + 10 Folgen à 35 Byte)
 * Direkt im Datensegment: Werbekunden DS:3f2f (204.HES), DS:2711 (205.HES),
 * Nachrichtenthemen DS:5573 (206.HES), DS:3a81 (48.HES). */
#include "game.h"

#define g_text_pool      DS_PTR(char, 0x8aa8)    /* Texte aus den Sprachdateien */
#define g_text_pool_pos  DS_U16(0x11e4)
#define g_load_buf       DS_PTR(uint8_t, 0x8f90)
#define g_load_pos       DS_U16(0x11e2)
#define g_lang_files     DS_ADDR(int16_t, 0x055e) /* 8 HES-Nummern je Sprache */
#define g_episodes       DS_PTR(uint8_t, 0x713c)

/* 2a62:000c – Datenbanken laden und Texte der gewählten Sprache zuordnen */
void db_load_all(void)
{
    int16_t ad_texts = g_lang_files[g_language * 8 + 5];   /* 46/249: Werbekunden */
    int16_t news_texts = g_lang_files[g_language * 8 + 7]; /* 44/247: Nachrichten */
    uint16_t pool = (uint16_t)(hes_filesize(news_texts) + hes_filesize(ad_texts));
    g_text_pool = far_calloc(pool, 1);

    timer_pause();
    hes_load(0xc9, DS_PTR(uint8_t, 0x2489));
    /* PORT: ORIGINAL-BUG in den Spieldaten – alle produzierbaren Shows (Genre 8) haben Länge 0.
     * Im Original zeigen sie "0 blocks" und das Spiel stürzt beim Einplanen ab. Eine Show ist eine
     * Ein-Stunden-Sendung, daher hier Länge 1. Die Länge steht nicht im Spielstand. */
    for (int16_t i = 1; i < 0xc9; i++) {
        uint8_t *f = DS_PTR(uint8_t, 0x2489) + i * 0x17;
        if (f[0] == 8 && f[4] == 0)
            f[4] = 1;
    }
    hes_load(0xca, DS_PTR(uint8_t, 0x2626));
    hes_load(0xcb, g_episodes);
    hes_load(0xcc, DS_ADDR(uint8_t, 0x3f2f));
    hes_load(0xcd, DS_ADDR(uint8_t, 0x2711));
    hes_load(0xce, DS_ADDR(uint8_t, 0x5573));
    hes_load(0x30, DS_ADDR(uint8_t, 0x3a81));

    /* Werbekunden: zwei Längentabellen (je 51 Wörter), danach die Texte */
    g_load_pos = 0;
    g_load_buf = far_calloc((uint16_t)hes_filesize(ad_texts), 1);
    hes_load(ad_texts, g_load_buf);
    int16_t *lengths = (int16_t *)(g_load_buf + g_load_pos);
    g_load_pos += 0x66;
    for (int16_t i = 1; i < 0x33; i++)
        if (lengths[i] != 0)
            text_pool_take(lengths[i], (char **)DS_ADDR(uint8_t, 0x3f2f + i * 0x18));
    lengths = (int16_t *)(g_load_buf + g_load_pos);
    g_load_pos += 0x66;
    for (int16_t i = 1; i < 0x33; i++)
        if (lengths[i] != 0)
            text_pool_take(lengths[i], (char **)DS_ADDR(uint8_t, 0x3f33 + i * 0x18));
    far_free(g_load_buf);

    /* Nachrichtenthemen: 51 x 11 Längen, je Thema 10 Textvarianten */
    g_load_buf = far_calloc((uint16_t)hes_filesize(news_texts), 1);
    g_load_pos = 0;
    hes_load(news_texts, g_load_buf);
    lengths = (int16_t *)g_load_buf;
    g_load_pos += 0x462;
    for (int16_t i = 1; i < 0x33; i++)
        for (int16_t j = 1; j < 0xb; j++)
            if (lengths[i * 10 + j] != 0)
                text_pool_take(lengths[i * 10 + j], (char **)DS_ADDR(uint8_t, i * 0x38 + 0x5573 + j * 4));
    far_free(g_load_buf);
    timer_resume();
    episodes_init_from_first();
}

/* 2a62:029e – len Bytes aus dem Ladepuffer in den Textpool übernehmen (NUL-terminiert) */
void text_pool_take(int16_t len, char **out)
{
    far_memcpy(g_text_pool + g_text_pool_pos, g_load_buf + g_load_pos, (uint16_t)len);
    g_load_pos = (uint16_t)(g_load_pos + len);
    *out = g_text_pool + g_text_pool_pos;
    g_text_pool_pos = (uint16_t)(g_text_pool_pos + len + 1);
}

/* 2a62:02eb – bei Serien mit Flag "gleiche Folgen" (Bit 2 gesetzt, Bit 0 frei) Folge 0 auf
 * Folgen 1..9 kopieren */
void episodes_init_from_first(void)
{
    for (int16_t i = 1; i < 0x33; i++) {
        uint8_t *series = g_episodes + i * 0x1b3;
        uint16_t flags = *(uint16_t *)(series + 5);
        if (!(flags & 1) && (flags & 4))
            for (int16_t e = 1; e < 10; e++)
                far_memcpy(series + e * 0x23 + 8, series + 8, 0x23);
    }
}

#define FILM(n)           (DS_PTR(uint8_t, 0x2489) + (n) * 0x17)
#define SERIES(n)         (DS_PTR(uint8_t, 0x2626) + (n) * 0x39)
#define W(p, ofs)         (*(int16_t *)((uint8_t *)(p) + (ofs)))
#define UW(p, ofs)        (*(uint16_t *)((uint8_t *)(p) + (ofs)))
#define L(p, ofs)         (*(int32_t *)((uint8_t *)(p) + (ofs)))
#define PLAYER_MONEY(p)   DS_I32(0x8bc4 + (p) * 0x17)
#define PLAYER_SUBSCR(p, c) DS_U8(0x8bcc + (p) * 0x17 + (c))
#define SCHEDULE(p, h)    DS_I16(0x43f7 + (p) * 0x5a + (h) * 2)
#define LOT(i)            DS_ADDR(uint8_t, 0x1aed + (i) * 9)

/* ---- Nachrichten
 * Themen DS:3a81 (48.HES, 50 x 14 B): +0..+8 fünf Textbausteine (Tausender = Variantenmodus),
 *   +0xa nächste Meldung (+1000 = verbraucht), +0xd Kategorie.
 * Meldungen DS:2711 (205.HES, 10 B): +0..+6 vier Folgemeldungen (9999 = Ende), +8 Wert, +9 Abstand.
 * Bausteinvarianten DS:5573 (206.HES, 0x38 B je Baustein), Variante v vorhanden bei +0x2c+v.
 * Laufende Ketten DS:70a9, 7 x 0x15 B: +0 Thema, +2 letztes Thema, +4 Meldung, +6..+0xe Varianten,
 *   +0x12 Countdown, +0x14 Kategorie.
 * Neue Meldungen DS:36bb, 20 x 0x1e B (Anzahl DS:7228), Spielerarchiv DS:455f + p*600 (Anzahl DS:7286). */
#define CHAIN(c)          DS_ADDR(uint8_t, 0x70a9 + (c) * 0x15)
#define TOPIC(t)          DS_ADDR(uint8_t, 0x3a81 + (t) * 0xe)
#define NEWSITEM(n)       DS_ADDR(uint8_t, 0x2711 + (n) * 10)
#define FRESH(i)          DS_ADDR(uint8_t, 0x36bb + (i) * 0x1e)
#define FRESH_COUNT       DS_I16(0x7228)
#define NEWS_ARCH(p, i)   DS_ADDR(uint8_t, 0x455f + (p) * 600 + (i) * 0x1e)
#define NEWS_COUNT(p)     DS_I16(0x7286 + (p) * 2)

/* 2a62:036d – Betrag in der Finanzstatistik (DS:71d5, long je Kategorie) verbuchen */
void money_add(int16_t category, int32_t amount)
{
    DS_I32(0x71d5 + category * 4) += amount;
}

/* 2a62:0387 – Nachrichtenticker: Ketten weiterlaufen lassen, neue Meldungen verteilen */
void news_update(int16_t elapsed, int16_t min_new)
{
    int16_t r;
    for (int16_t c = 0; c < 7; c++)
        if (W(CHAIN(c), 0) != 0)
            W(CHAIN(c), 0x12) -= elapsed;
    do
        r = news_chain_step(0);
    while (r != -1);
    news_chains_refill();
    r = 0;
    while (news_count_type1() < min_new && r != -1)
        r = news_chain_step(1000);
    news_chains_refill();
    news_distribute();
}

/* 2a62:0405 – neue Meldungen der Kategorie 1 */
int16_t news_count_type1(void)
{
    int16_t n = 0;
    for (int16_t i = 0; i < FRESH_COUNT; i++)
        if (FRESH(i)[0x1c] == 1)
            n++;
    return n;
}

static void fresh_make_room(void)
{
    if (FRESH_COUNT > 0x13) {
        for (int16_t i = 1; i < 0x14; i++)
            far_memcpy(FRESH(i - 1), FRESH(i), 0x1e);
        FRESH_COUNT--;
    }
}

/* 2a62:0430 – aktuelle Meldung der Kette als neue Nachricht anlegen (Wert nach Bausteinqualität) */
void news_generate(int16_t chain)
{
    fresh_make_room();
    uint8_t *n = FRESH(FRESH_COUNT), *c = CHAIN(chain);
    W(n, 0) = W(c, 4);
    int16_t item = W(c, 4);
    uint8_t *t = TOPIC(W(c, 0));
    int16_t q = 0, cnt = 0;
    for (int16_t k = 0; k < 5; k++) {
        W(n, 2 + k * 4) = W(t, k * 2) % 1000;
        W(n, 4 + k * 4) = W(c, 6 + k * 2);
        if (W(t, k * 2) != 0 && W(t, k * 2) < 1000) {
            q += DS_I8(0x559f + W(t, k * 2) * 0x38 + W(c, 6 + k * 2));
            cnt += 10;
        }
    }
    int16_t base = (int8_t)NEWSITEM(item)[8];
    W(n, 0x16) = cnt == 0 ? base : (int16_t)(base * (q * 0x46 / cnt + 0x1e) / 100);
    int16_t lvl = 0;
    while (W(n, 0x16) > DS_I8(0x11e6 + lvl))
        lvl++;
    n[0x1d] = (uint8_t)lvl;
    n[0x1c] = c[0x14];
    W(n, 0x1a) = g_day;
    W(n, 0x18) = g_hour;
    FRESH_COUNT++;
}

/* 2a62:0627 – Sondermeldung (Ereignisse) direkt anlegen */
void news_add_special(int16_t topic, int16_t param1, int16_t param2)
{
    fresh_make_room();
    uint8_t *n = FRESH(FRESH_COUNT);
    W(n, 0) = topic;
    W(n, 0x16) = (int8_t)NEWSITEM(topic)[8];
    W(n, 2) = param1;
    W(n, 4) = param2;
    n[0x1c] = 1;
    W(n, 0x1a) = g_day;
    W(n, 0x18) = g_hour;
    FRESH_COUNT++;
}

/* 2a62:06c8 – die als nächste fällige Kette eine Meldung bringen lassen; -1 wenn keine fällig */
int16_t news_chain_step(int16_t max_time)
{
    int16_t best = -1, lim = max_time + 1, c;
    for (c = 0; c < 7; c++) {
        uint8_t *ch = CHAIN(c);
        if (W(ch, 0) != 0 && W(ch, 0x12) <= max_time && W(ch, 0x12) < lim &&
            (max_time == 0 || ch[0x14] == 1)) {
            lim = W(ch, 0x12);
            best = c;
        }
    }
    if (best != -1) {
        news_generate(best);
        uint8_t *ch = CHAIN(best);
        int16_t next = 0;
        while (next == 0) /* c ist hier 7: Seed wie im Original */
            next = W(NEWSITEM(W(ch, 4)), (rand_range(4, c) - 1) * 2);
        if (next == 9999) {
            W(ch, 2) = W(ch, 0);
            W(ch, 0) = 0;
        } else {
            W(ch, 4) = next;
            if (max_time == 0)
                W(ch, 0x12) += (int8_t)NEWSITEM(next)[9];
            else
                W(ch, 0x12) = (int8_t)NEWSITEM(next)[9];
        }
    }
    return best;
}

/* 2a62:07ed – leere Ketten mit einem neuen Thema derselben Kategorie belegen */
void news_chains_refill(void)
{
    for (int16_t c = 0; c < 7; c++) {
        uint8_t *ch = CHAIN(c);
        if (W(ch, 0) != 0)
            continue;
        int16_t t;
        for (t = 1; t <= 0x32; t++)
            if (W(TOPIC(t), 0xa) > 0 && TOPIC(t)[0xd] == ch[0x14])
                break;
        if (t > 0x32) /* alle Themen der Kategorie verbraucht: wieder freigeben */
            for (t = 1; t <= 0x32; t++)
                if (TOPIC(t)[0xd] == ch[0x14])
                    W(TOPIC(t), 0xa) += 1000;
        t = 0;
        while (t == 0) {
            t = rand_range(0x32, c);
            if (W(TOPIC(t), 0xa) < 1)
                t = 0;
            if (t != 0 && TOPIC(t)[0xd] != ch[0x14])
                t = 0;
        }
        uint8_t *tp = TOPIC(t);
        W(ch, 0) = t;
        W(ch, 4) = W(tp, 0xa);
        W(tp, 0xa) -= 1000;
        W(ch, 0x12) = 3;
        for (int16_t k = 0; k < 5; k++) {
            if (W(tp, k * 2) == 0)
                continue;
            W(ch, 6 + k * 2) = 0;
            int16_t mode = W(tp, k * 2) / 1000;
            if (mode == 0 || mode == 2) { /* freie Variante wählen */
                int16_t v = 0;
                while (v == 0) {
                    v = rand_range(10, k);
                    int16_t part = W(tp, k * 2) % 1000;
                    if (DS_U8(0x559f + part * 0x38 + v) == 0 || news_variant_in_use(part, v))
                        v = 0;
                    else
                        W(ch, 6 + k * 2) = v;
                }
            } else if (mode == 1) { /* wie der vorige Baustein */
                W(ch, 6 + k * 2) = W(ch, 6 + (k - 1) * 2);
            }
        }
    }
}

/* 2a62:0a1c – wird Baustein-Variante schon in einer Kette benutzt? (Kette 0 wird nicht geprüft) */
int16_t news_variant_in_use(int16_t topic, int16_t variant)
{
    int16_t used = 0;
    for (int16_t c = 1; c < 8; c++) /* ORIGINAL-BUG: 1..7 statt 0..6 */
        for (int16_t k = 0; k < 5; k++)
            if (W(TOPIC(W(CHAIN(c), 0)), k * 2) % 1000 == topic && W(CHAIN(c), 6 + k * 2) == variant)
                used = 1;
    return used;
}

/* 2a62:0ab6 – Qualität der Nachrichtensendung: Mittel der drei Meldungswerte */
void newscast_rate(int16_t player)
{
    int16_t sum = 0;
    for (int16_t k = 1; k < 4; k++)
        sum += DS_I16(0x3d61 + player * 0x78 + k * 0x1e);
    DS_I8(0x3f2b + player) = (int8_t)(sum / 3);
}

/* 2a62:0afa – Lose mit offenem Gebot des Menschen (Bieter < 0) ihm zuschlagen */
void auction_assign_unbid(void)
{
    for (int16_t i = 1; i < 0xb; i++) {
        if ((int8_t)LOT(i)[6] < 0) {
            LOT(i)[6] = (uint8_t)g_local_player;
            L(LOT(i), 2) += auction_bid_increment(i);
        }
    }
}

/* 2a62:0b46 – Briefkasten des Menschen: Platz für eine Nachricht (max 5, älteste fällt raus) */
void message_slot_alloc(int16_t player)
{
    if (player == g_local_player && ++DS_I8(0x7227) > 4) {
        for (int16_t i = 1; i < 5; i++)
            far_memcpy(DS_ADDR(uint8_t, 0x609f + (i - 1) * 0x1a), DS_ADDR(uint8_t, 0x609f + i * 0x1a), 0x1a);
        DS_I8(0x7227) = 4;
    }
}

/* 2a62:0b96 – am Ende einer Sendung: Abnutzung, nächste Serienfolge */
void programmes_on_air_update(void)
{
    for (int16_t p = 1; p <= 3; p++) {
        int16_t h = hour_index(), prog;
        while ((prog = SCHEDULE(p, h)) == 9999)
            h--;
        if (prog > 1000 && prog < 2000 && hour_index() == (int8_t)SERIES(prog - 1000)[0x23] + h - 1) {
            uint8_t *s = SERIES(prog - 1000);
            if (W(s, 0x31) == 0)
                W(s, 0x31) = 1;
            SCHEDULE(p, h + 0xf) = W(s, 0x31);
            series_episode_wear(prog - 1000, W(s, 0x31));
            W(s, 0x31)++;
            int16_t n = 1;
            while (UW(s, (n + 1) * 3 + 2) & 1)
                n++;
            if (n > 10)
                n = 10;
            if (W(s, 0x31) > n)
                W(s, 0x31) = 1;
        } else if (prog > 0 && prog < 1000 && hour_index() == (int8_t)FILM(prog)[4] + h - 1) {
            film_wear(prog);
        }
    }
}

/* 2a62:0cfa – neuer Tag: gestrige Pläne sichern, morgige Stunden nach vorn schieben */
void schedule_day_rotate(void)
{
    DS_U8(0x71d0) = 0; /* Byte-Variable: 16 Bit würden den Kontostand DS:71d1 anfressen */
    DS_I16(0x765a) = 0;
    far_memcpy(DS_ADDR(uint8_t, 0x3919), DS_ADDR(uint8_t, 0x43f7), 0x168);
    for (int16_t p = 1; p < 4; p++) {
        int16_t *plan = DS_ADDR(int16_t, 0x43f7 + p * 0x5a);
        for (int16_t h = 1; h < 8; h++) {
            for (int16_t k = 0; k < 3; k++) { /* Programm, Folge, Werbung */
                plan[h + k * 15] = plan[h + 7 + k * 15];
                plan[h + 7 + k * 15] = 0;
            }
        }
    }
    DS_I16(0x79ee) = 0;
    DS_I16(0x7a48) = 0;
}

/* 2a62:0df2 – neue Meldungen in die Archive der Abonnenten der Kategorie kopieren */
void news_distribute(void)
{
    if (FRESH_COUNT <= 0)
        return;
    for (int16_t p = 1; p < 4; p++) {
        for (int16_t i = 0; i < FRESH_COUNT; i++) {
            if (PLAYER_SUBSCR(p, (int8_t)FRESH(i)[0x1c]) == 0)
                continue;
            if (NEWS_COUNT(p) > 0x13)
                player_news_drop_oldest(p);
            far_memcpy(NEWS_ARCH(p, NEWS_COUNT(p)), FRESH(i), 0x1e);
            NEWS_COUNT(p)++;
        }
    }
    FRESH_COUNT = 0;
}

/* 2a62:0ea8 – Nachrichten verlieren an Wert (Archiv und laufende Sendung, in Prozent) */
void news_values_decay(int16_t pct_archive, int16_t pct_newscast)
{
    for (int16_t p = 1; p < 4; p++) {
        for (int16_t i = 0; i <= NEWS_COUNT(p); i++) { /* ORIGINAL: <= (ein Eintrag zu viel) */
            uint8_t *n = NEWS_ARCH(p, i);
            if (W(n, 0x16) != 0) {
                W(n, 0x16) = (int16_t)(W(n, 0x16) * pct_archive / 100);
                int16_t lvl = 0;
                while (W(n, 0x16) > DS_I8(0x11ec + lvl))
                    lvl++;
                n[0x1d] = (uint8_t)lvl;
            }
        }
        for (int16_t k = 1; k < 4; k++) {
            uint8_t *n = DS_ADDR(uint8_t, 0x3d4b + p * 0x78 + k * 0x1e);
            if (W(n, 0x16) != 0)
                W(n, 0x16) = (int16_t)(W(n, 0x16) * pct_newscast / 100);
        }
    }
}

/* 2a62:0f7c – welche Folge einer Serie in Stunde hour laufen wird */
int16_t series_next_episode(int16_t player, int16_t series, int16_t hour)
{
    uint8_t *s = SERIES(series);
    int16_t n = 1;
    while (n < 0xb && (UW(s, n * 3 + 2) & 1))
        n++;
    if (W(s, 0x31) == 0)
        W(s, 0x31) = 1;
    int16_t e = W(s, 0x31) - 1;
    for (int16_t h = 1; h <= hour; h++)
        if (SCHEDULE(player, h) == series + 1000 && SCHEDULE(player, h + 0xf) == 0)
            e++;
    if (n - 1 > 0) /* im Original Endlosschleife ohne fertige Folge */
        while (e > n - 1)
            e -= n - 1;
    return e;
}

/* 2a62:1041 – verfügbares Geld: Kontostand minus geplante Senderkäufe und eigene Gebote */
int32_t money_available(int16_t player)
{
    int32_t m = PLAYER_MONEY(player);
    for (int16_t i = 1; i < 0x20; i++) {
        uint8_t *st = DS_ADDR(uint8_t, 0x1ce9 + player * 0x1ce + i * 0xe);
        if (W(st, 0xc) == 1)
            m -= L(st, 0);
    }
    for (int16_t i = 1; i < 0xb; i++) {
        uint8_t *l = LOT(i);
        if (W(l, 0) == 0)
            continue;
        if ((int8_t)l[6] == player)
            m -= L(l, 2);
        else if ((int8_t)l[6] < 0 && player == g_local_player)
            m -= auction_bid_increment(i) + L(l, 2);
    }
    return m;
}

/* 2a62:111a – Programm aus allen Archiven entfernen und freigeben (Lizenzfilme behalten Besitzer) */
void programme_remove_from_archives(int16_t prog)
{
    int16_t genre;
    if (prog < 1000) {
        uint8_t *f = FILM(prog);
        if (f[0] != 7) {
            f[5] = 0;
            UW(f, 6) |= 0x20;
        }
        genre = (int8_t)f[0];
    } else {
        prog -= 1000;
        SERIES(prog)[0x24] = 0;
        UW(SERIES(prog), 0x25) |= 0x20;
        genre = 0xb;
    }
    for (int16_t p = 1; p < 4; p++) {
        int16_t *arch = DS_ADDR(int16_t, 0x6ef9 + p * 0x6c + genre * 8);
        int8_t *count = DS_ADDR(int8_t, 0x6f59 + p * 0x6c + genre);
        for (int16_t k = 0; k < *count; k++) {
            if (arch[k] == prog) {
                for (int16_t j = k; j < *count; j++)
                    arch[j] = arch[j + 1];
                (*count)--;
            }
        }
    }
}

/* 2a62:1299 */
void player_news_drop_oldest(int16_t player)
{
    for (int16_t i = 1; i < NEWS_COUNT(player); i++)
        far_memcpy(NEWS_ARCH(player, i - 1), NEWS_ARCH(player, i), 0x1e);
    NEWS_COUNT(player)--;
}

/* 2a62:1302 */
void player_news_remove(int16_t player, int16_t idx)
{
    far_memcpy(NEWS_ARCH(player, idx), NEWS_ARCH(player, idx + 1), (uint16_t)((NEWS_COUNT(player) - idx) * 0x1e));
    NEWS_COUNT(player)--;
}

/* 2a62:1368 – Eintrag aus Liste DS:1a25 (5 B je Eintrag) entfernen */
void list5_remove(int16_t player, int16_t idx)
{
    for (; idx <= DS_I16(0x7652 + player * 2); idx++)
        far_memcpy(DS_ADDR(uint8_t, 0x1a25 + player * 0x32 + idx * 5),
                   DS_ADDR(uint8_t, 0x1a25 + player * 0x32 + (idx + 1) * 5), 5);
    DS_I16(0x7652 + player * 2)--;
}

/* 2a62:13d1 – Produktion aus der Liste DS:732c (15 B) entfernen */
void production_remove(int16_t player, int16_t idx)
{
    far_memcpy(DS_ADDR(uint8_t, 0x732c + player * 0x4b + idx * 0xf),
            DS_ADDR(uint8_t, 0x732c + player * 0x4b + (idx + 1) * 0xf),
            (uint16_t)((DS_I16(0x7460 + player * 2) - idx) * 0xf));
    DS_I16(0x7460 + player * 2)--;
}

/* 2a62:1437 – Werbevertrag aus der Liste entfernen (bei KI auch deren Zusatzliste DS:4ebf) */
void list_remove(int16_t player, int16_t idx)
{
    if (DS_I16(0x7688 + player * 2) == 0)
        return;
    for (; idx < DS_I16(0x7688 + player * 2); idx++) {
        far_memcpy(DS_ADDR(uint8_t, 0x1b50 + player * 0x46 + idx * 7),
                   DS_ADDR(uint8_t, 0x1b50 + player * 0x46 + (idx + 1) * 7), 7);
        if (DS_U8(0xf2) == player || DS_U8(0xf3) == player)
            far_memcpy(DS_ADDR(uint8_t, 0x4ebf + player * 0x4d + idx * 7),
                       DS_ADDR(uint8_t, 0x4ebf + player * 0x4d + (idx + 1) * 7), 7);
    }
    DS_I16(0x1b50 + player * 0x46 + DS_I16(0x7688 + player * 2) * 7) = 0;
    DS_I16(0x7688 + player * 2)--;
}

/* 2a62:152c – Serienfolge nutzt sich bei Ausstrahlung ab */
void series_episode_wear(int16_t series, int16_t episode)
{
    uint8_t *s = SERIES(series);
    int8_t *ep = (int8_t *)(s + episode * 3);
    ep[1] = (int8_t)(ep[1] - (int8_t)s[0x33] * ep[0] / 0x14);
    if (ep[1] < 1)
        ep[1] = 1;
}

/* 2a62:158e – Film nutzt sich bei Ausstrahlung ab */
void film_wear(int16_t film)
{
    int8_t *f = (int8_t *)FILM(film);
    f[3] = (int8_t)(f[3] - f[1] * f[2] / 5);
    if (f[3] < 1)
        f[3] = 1;
}

/* 2a62:15db – Mindesterhöhung für ein Gebot: 10 % des Preises, abgerundet auf eine Stelle */
int32_t auction_bid_increment(int16_t lot)
{
    int32_t v = L(LOT(lot), 2) / 10;
    if (v > 100000)
        return v / 100000 * 100000;
    if (v > 10000)
        return v / 10000 * 10000;
    if (v > 1000)
        return v / 1000 * 1000;
    if (v > 100)
        return v / 100 * 100;
    if (v > 10)
        return v / 10 * 10;
    return v;
}
