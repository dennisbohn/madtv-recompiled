/* Segment 1433 – Text-Cache: Nachrichten-, Film-, Serien- und Showtexte aus den sprachabhängigen
 * HES-Dateien (Nummern in DS:0562.., je Sprache 16 Byte) nachladen und wieder freigeben.
 *
 * Jede Textdatei beginnt mit einer Tabelle von 16-Bit-Endoffsets; Eintrag n reicht von
 * index[n-1] bis index[n]. Ein Eintrag besteht aus Pascal-Strings (Längenbyte + Text). */
#include "game.h"

#define g_text_index    DS_PTR(uint16_t, 0x609b) /* geladene Offsettabelle */
#define g_text_file     DS_I16(0x8ed8)           /* HES-Nummer der offenen Textdatei */
#define TEXT_FILE(k)    DS_I16(0x0562 + g_language * 0x10 + (k) * 2) /* k: 0 Filme, 1 Serien, 2 Shows, 4 Nachrichten */
#define FILM_CACHE(i)   DS_ADDR(uint8_t, 0x63c5 + (i) * 10)   /* id, Titel, Beschreibung */
#define SERIES_CACHE(i) DS_ADDR(uint8_t, 0x65af + (i) * 0x5a) /* id, 22 Texte */
#define SHOW_CACHE(i)   DS_ADDR(uint8_t, 0x6af5 + (i) * 0x42) /* id, 16 Texte */
#define NEWS_SLOT(i)    DS_ADDR(uint8_t, 0x729a + (i) * 6)    /* id, Text */
/* Größe der Offsettabelle einer Textdatei mit n Einträgen (Endoffsets für 0..n, je 2 Byte) */
#define TEXT_INDEX_BYTES(n) ((uint16_t)(((n) + 1) * 2))
#define g_shows         DS_PTR(uint8_t, 0x713c)
#define SHOW_IS_SERIES(s) (*(uint16_t *)(g_shows + (s) * 0x1b3 + 5) & 1)
#define SHOW_PROG(s)    (*(int16_t *)(g_shows + (s) * 0x1b3 + 6))

static void open_text_index(int16_t file_slot, uint16_t bytes, uint16_t alloc_n, uint16_t alloc_size)
{
    timer_pause();
    DS_SET_PTR(0x609b, far_calloc(alloc_n, alloc_size));
    g_text_file = TEXT_FILE(file_slot);
    hes_read(g_text_file, g_text_index, 0, bytes);
}

static void close_text_index(void)
{
    far_free(g_text_index);
    timer_resume();
}

/* Eintrag n der offenen Datei lesen und in count Einzelstrings (je eigener Puffer) zerlegen;
 * dst: Tabelle von 4-Byte-Zeigern im Datensegment */
static void load_entry(int16_t n, ptr32 *dst, int16_t count)
{
    uint16_t ofs = g_text_index[n - 1];
    int16_t len = (int16_t)(g_text_index[n] - ofs);
    uint8_t *buf = far_calloc((uint16_t)(len + 1), 1), *p = buf;
    hes_read(g_text_file, buf, ofs, (uint16_t)len);
    for (int16_t k = 0; k < count; k++) {
        if (*p == 0) {
            dst[k] = 0;
            p++;
        } else {
            dst[k] = P32(far_calloc((uint16_t)(*p + 1), 1));
            far_memcpy(PTR32(char, dst[k]), p + 1, *p);
            p += *p + 1;
        }
    }
    far_free(buf);
}

/* 1433:0002 – Nachrichtentexte: Archiv des Spielers, die drei gewählten, ggf. Banknachricht */
void text_cache_news(int16_t player)
{
    int16_t slot = 0, i;
    open_text_index(4, 0x322, 0x322, 1);
    for (i = 0; i < DS_I16(0x7286 + player * 2); i++)
        text_cache_load_news(slot++, DS_I16(0x455f + player * 600 + i * 0x1e));
    for (i = 1; i < 4; i++) {
        int16_t id = DS_I16(0x3d4b + player * 0x78 + i * 0x1e);
        if (id != 0)
            text_cache_load_news(slot++, id);
    }
    if (g_cur_room == 2 && g_cur_dialog == 2)
        text_cache_load_news(slot++, DS_I16(0x6127));
    for (; slot < 0x18; slot++)
        MEM_SET_PTR(NEWS_SLOT(slot) + 2, NULL);
    close_text_index();
}

/* 1433:0120 */
void text_cache_load_news(int16_t slot, int16_t text_id)
{
    uint8_t *e = NEWS_SLOT(slot);
    int16_t len = (int16_t)(g_text_index[text_id] - g_text_index[text_id - 1]);
    *(int16_t *)e = text_id;
    if (len == 0) {
        MEM_SET_PTR(e + 2, NULL);
        return;
    }
    char *t = far_calloc((uint16_t)(len + 1), 1);
    MEM_SET_PTR(e + 2, t);
    hes_read(g_text_file, t, g_text_index[text_id - 1], (uint16_t)len);
}

/* 1433:01de */
void text_cache_news_free(void)
{
    for (int16_t i = 0; i < 0x18; i++)
        if (MEM_PTR(char, NEWS_SLOT(i) + 2) != NULL)
            far_free_ptr((ptr32 *)(NEWS_SLOT(i) + 2));
}

static void film_if_new(int16_t film, int16_t *slot)
{
    if (film != 0 && film < 1000 && text_cache_has_film(film) == 0)
        text_cache_load_film(film, (*slot)++);
}

static void film_archive(int16_t player, int16_t *slot)
{
    for (int16_t g = 1; g < 0xb; g++)
        for (int16_t k = 0; k < DS_I8(0x6f59 + player * 0x6c + g); k++)
            text_cache_load_film(DS_I16(0x6ef9 + player * 0x6c + g * 8 + k * 2), (*slot)++);
}

static void film_suitcase(int16_t *slot)
{
    for (int16_t i = 0; i < DS_I16(0x732a); i++)
        if (DS_I16(0x2463 + i * 2) < 1000)
            text_cache_load_film(DS_I16(0x2463 + i * 2), (*slot)++);
}

/* 1433:021b – Film-Titel/-Beschreibungen für einen Raum laden (Plätze 0..2 fest, ab 3 raumweise).
 * arg: Film (room 0), Spieler (0x1f, 4) bzw. Studio (5) */
void text_cache_titles(int16_t room, int16_t arg)
{
    int16_t slot = 3, i, k;
    open_text_index(0, TEXT_INDEX_BYTES(g_film_count), TEXT_INDEX_BYTES(g_film_count), 1);
    if (room == 0) {
        text_cache_load_film(arg, 0);
    } else if (room == 0x1f) { /* Programmplaner */
        film_archive(arg, &slot);
        for (i = 1; i <= hour_index(); i++)
            film_if_new(DS_I16(0x43f7 + arg * 0x5a + i * 2), &slot);
    } else if (room == 99 || room == 100) { /* Statistik / Tagesabschluss */
        if (room == 100)
            while (*(int16_t *)FILM_CACHE(slot) != 0)
                slot++;
        if (g_day > 1)
            for (k = 1; k < 4; k++)
                for (i = 1; i < 8; i++)
                    film_if_new(DS_I16(0x3919 + k * 0x5a + i * 2), &slot);
        for (k = 1; k < 4; k++)
            for (i = 1; i < 8; i++)
                film_if_new(DS_I16(0x43f7 + k * 0x5a + i * 2), &slot);
    } else if (room == 4) { /* Archiv */
        if (arg == g_local_player)
            film_suitcase(&slot);
        film_archive(arg, &slot);
    } else if (room == 8) { /* Filmhändler und Auktion */
        film_suitcase(&slot);
        for (i = 1; i < 0xb; i++) {
            int16_t f = DS_I16(0x19cd + i * 8);
            if (f != 0 && f < 1000)
                text_cache_load_film(f, slot++);
        }
        for (i = 1; i < 0xb; i++) {
            int16_t f = DS_I16(0x1aed + i * 9);
            if (f != 0 && f < 1000)
                text_cache_load_film(f, slot++);
        }
    } else if (room == 9) { /* Drehbuchagentur: Filme der Drehbücher */
        for (i = 0; i < DS_I16(0x7652 + g_local_player * 2); i++) {
            int16_t s = DS_I16(0x1a25 + g_local_player * 0x32 + i * 5);
            if (!SHOW_IS_SERIES(s))
                text_cache_load_film(SHOW_PROG(s), slot++);
        }
        for (i = 1; i < 6; i++) {
            int16_t s = DS_I16(0x194a + i * 9);
            if (s != 0 && !SHOW_IS_SERIES(s))
                text_cache_load_film(SHOW_PROG(s), slot++);
        }
    } else if (room == 5) { /* Studio */
        if (DS_I8(0x6148 + arg * 0x10) == g_local_player) {
            for (i = 0; i < DS_I16(0x7652 + g_local_player * 2); i++) {
                int16_t s = DS_I16(0x1a25 + g_local_player * 0x32 + i * 5);
                if (!SHOW_IS_SERIES(s))
                    text_cache_load_film(SHOW_PROG(s), slot++);
            }
        }
        int16_t s = DS_I16(0x6145 + arg * 0x10);
        if (s != 0 && !SHOW_IS_SERIES(s))
            text_cache_load_film(SHOW_PROG(s), slot);
    } else if (room == 0xc) { /* Produktionen */
        for (i = 0; i < DS_I16(0x7460 + g_local_player * 2); i++) {
            int16_t s = DS_I16(0x732c + g_local_player * 0x4b + i * 0xf);
            if (!SHOW_IS_SERIES(s))
                text_cache_load_film(SHOW_PROG(s), slot++);
        }
    }
    close_text_index();
}

/* 1433:0855 – -1, wenn der Film schon im Cache ist */
int16_t text_cache_has_film(int16_t film_id)
{
    int16_t i;
    for (i = 0; i < 0x31 && *(int16_t *)FILM_CACHE(i) != film_id; i++)
        ;
    return i < 0x31 ? -1 : 0;
}

/* 1433:0889 – Titel und Beschreibung eines Films in Platz slot */
void text_cache_load_film(int16_t film_id, int16_t slot)
{
    if (slot >= 0x31) /* PORT: das Original schriebe hinter den Cache */
        return;
    *(int16_t *)FILM_CACHE(slot) = film_id;
    load_entry(film_id, (ptr32 *)(FILM_CACHE(slot) + 2), 2);
}

/* 1433:09a2 – fixed 0: Raumplätze 3..48 freigeben, sonst die festen Plätze 0..2 */
void text_cache_titles_free(int16_t fixed)
{
    int16_t from = fixed == 0 ? 3 : 0, to = fixed == 0 ? 0x30 : 2;
    for (int16_t i = from; i <= to; i++)
        text_cache_film_free(i);
}

/* 1433:09df */
void text_cache_film_free(int16_t slot)
{
    uint8_t *c = FILM_CACHE(slot);
    *(int16_t *)c = 0;
    if (MEM_PTR(char, c + 2) != NULL)
        far_free_ptr((ptr32 *)(c + 2));
    if (MEM_PTR(char, c + 6) != NULL)
        far_free_ptr((ptr32 *)(c + 6));
}

static void series_if_new(int16_t prog, int16_t *slot, int16_t limit)
{
    if (prog > 1000 && prog < 2000 && text_cache_has_series(prog - 1000) == 0 && (!limit || *slot < 0xf))
        text_cache_load_series(prog - 1000, (*slot)++);
}

static void series_archive(int16_t player, int16_t *slot)
{
    for (int16_t k = 0; k < DS_I8(0x6f64 + player * 0x6c); k++)
        text_cache_load_series(DS_I16(0x6f51 + player * 0x6c + k * 2), (*slot)++);
}

static void series_suitcase(int16_t *slot)
{
    for (int16_t i = 0; i < DS_I16(0x732a); i++)
        if (DS_I16(0x2463 + i * 2) > 1000)
            text_cache_load_series(DS_I16(0x2463 + i * 2) - 1000, (*slot)++);
}

/* 1433:0a45 – Serientexte (Titel, Beschreibung, Folgen) für einen Raum laden */
void text_cache_descr(int16_t room, int16_t arg)
{
    int16_t slot = 3, i, k;
    open_text_index(1, TEXT_INDEX_BYTES(g_series_count), TEXT_INDEX_BYTES(g_series_count), 1);
    if (room == 0) {
        text_cache_load_series(arg, 0);
    } else if (room == 0x1f) {
        series_archive(arg, &slot);
        for (i = 1; i <= hour_index(); i++) {
            int16_t prog = DS_I16(0x43f7 + arg * 0x5a + i * 2);
            if (prog > 1000 && prog < 2000 && text_cache_has_series(prog - 1000) == 0)
                text_cache_load_series(prog - 1000, slot++);
        }
    } else if (room == 99 || room == 100) {
        if (room == 100)
            while (*(int16_t *)SERIES_CACHE(slot) != 0)
                slot++;
        if (g_day > 1)
            for (k = 1; k < 4; k++)
                for (i = 1; i < 8; i++)
                    series_if_new(DS_I16(0x3919 + k * 0x5a + i * 2), &slot, 1);
        for (k = 1; k < 4; k++)
            for (i = 1; i < 8; i++)
                series_if_new(DS_I16(0x43f7 + k * 0x5a + i * 2), &slot, 1);
    } else if (room == 4) {
        if (arg == g_local_player)
            series_suitcase(&slot);
        series_archive(arg, &slot);
    } else if (room == 8) {
        series_suitcase(&slot);
        for (i = 1; i < 0xb; i++) {
            int16_t prog = DS_I16(0x19cd + i * 8);
            if (prog > 1000 && prog < 2000)
                text_cache_load_series(prog - 1000, slot++);
        }
        for (i = 1; i < 0xb; i++) {
            int16_t prog = DS_I16(0x1aed + i * 9);
            if (prog > 1000 && prog < 2000)
                text_cache_load_series(prog - 1000, slot++);
        }
    } else if (room == 9) {
        for (i = 0; i < DS_I16(0x7652 + g_local_player * 2); i++) {
            int16_t s = DS_I16(0x1a25 + g_local_player * 0x32 + i * 5);
            if (SHOW_IS_SERIES(s))
                text_cache_load_series(SHOW_PROG(s), slot++);
        }
        for (i = 1; i < 6; i++) {
            int16_t s = DS_I16(0x194a + i * 9);
            if (s != 0 && SHOW_IS_SERIES(s))
                text_cache_load_series(SHOW_PROG(s), slot++);
        }
    } else if (room == 5) {
        if (DS_I8(0x6148 + arg * 0x10) == g_local_player) {
            for (i = 0; i < DS_I16(0x7652 + g_local_player * 2); i++) {
                int16_t s = DS_I16(0x1a25 + g_local_player * 0x32 + i * 5);
                if (SHOW_IS_SERIES(s))
                    text_cache_load_series(SHOW_PROG(s), slot++);
            }
        }
        int16_t s = DS_I16(0x6145 + arg * 0x10);
        if (s != 0 && SHOW_IS_SERIES(s))
            text_cache_load_series(SHOW_PROG(s), slot);
    } else if (room == 0xc) {
        for (i = 0; i < DS_I16(0x7460 + g_local_player * 2); i++) {
            int16_t s = DS_I16(0x732c + g_local_player * 0x4b + i * 0xf);
            if (SHOW_IS_SERIES(s))
                text_cache_load_series(SHOW_PROG(s), slot++);
        }
    }
    close_text_index();
}

/* 1433:0feb */
int16_t text_cache_has_series(int16_t series_id)
{
    int16_t i;
    for (i = 0; i < 0xf && *(int16_t *)SERIES_CACHE(i) != series_id; i++)
        ;
    return i < 0xf ? -1 : 0;
}

/* 1433:101f – 22 Texte einer Serie (Titel, Beschreibung, Folgentitel/-texte) in Platz slot */
void text_cache_load_series(int16_t series_id, int16_t slot)
{
    if (slot >= 0xf) /* PORT: das Original schriebe hinter den Cache */
        return;
    *(int16_t *)SERIES_CACHE(slot) = series_id;
    load_entry(series_id, (ptr32 *)(SERIES_CACHE(slot) + 2), 0x16);
}

/* 1433:1134 */
void text_cache_descr_free(int16_t fixed)
{
    int16_t from = fixed == 0 ? 3 : 0, to = fixed == 0 ? 0xe : 2;
    for (int16_t i = from; i <= to; i++)
        text_cache_series_free_slot(i);
}

/* 1433:1171 */
void text_cache_series_free_slot(int16_t slot)
{
    uint8_t *c = SERIES_CACHE(slot);
    *(int16_t *)c = 0;
    for (int16_t k = 0; k < 0x16; k++)
        if (((ptr32 *)(c + 2))[k] != 0)
            far_free_ptr((ptr32 *)(c + 2) + k);
}

/* 1433:11cf – Showtexte (Rollen- und Titeltexte der Drehbücher) für einen Raum laden */
void text_cache_series(int16_t room, int16_t arg)
{
    int16_t slot, i;
    open_text_index(2, TEXT_INDEX_BYTES(g_show_count), (uint16_t)(g_show_count + 1), 2);
    if (room == 9) {
        text_cache_load_show(0x22, 0);
        slot = 1;
        for (i = 0; i < DS_I16(0x7652 + g_local_player * 2); i++)
            text_cache_load_show(DS_I16(0x1a25 + g_local_player * 0x32 + i * 5), slot++);
        for (i = 1; i < 6; i++) {
            int16_t s = DS_I16(0x194a + i * 9);
            if (s != 0)
                text_cache_load_show(s, slot++);
        }
    } else if (room == 5) {
        slot = 0;
        if (DS_I16(0x7a16) == g_local_player)
            for (i = 0; i < DS_I16(0x7652 + g_local_player * 2); i++)
                text_cache_load_show(DS_I16(0x1a25 + g_local_player * 0x32 + i * 5), slot++);
        int16_t s = DS_I16(0x6145 + arg * 0x10);
        if (s != 0)
            text_cache_load_show(s, slot);
    } else if (room == 0xc) {
        slot = 0;
        for (i = 0; i < DS_I16(0x7460 + g_local_player * 2); i++)
            text_cache_load_show(DS_I16(0x732c + g_local_player * 0x4b + i * 0xf), slot++);
    }
    close_text_index();
}

/* 1433:1373 – 16 Texte einer Show in Platz slot */
void text_cache_load_show(int16_t text_id, int16_t slot)
{
    if (slot >= 10) /* PORT: das Original schriebe hinter den Cache */
        return;
    *(int16_t *)SHOW_CACHE(slot) = text_id;
    load_entry(text_id, (ptr32 *)(SHOW_CACHE(slot) + 2), 0x10);
}

/* 1433:148c */
void text_cache_series_free(void)
{
    for (int16_t i = 0; i < 10; i++)
        text_cache_show_free_slot(i);
}

/* 1433:14a0 */
void text_cache_show_free_slot(int16_t slot)
{
    ptr32 *t = (ptr32 *)(SHOW_CACHE(slot) + 2);
    for (int16_t k = 0; k < 0x10; k++)
        if (t[k] != 0)
            far_free_ptr(&t[k]);
}

/* PORT: nach einem Sprachwechsel alle Einträge der Text-Caches aus den Dateien der neuen Sprache
 * neu laden. Die bisherigen Texte werden nicht freigegeben, weil gerade angezeigte Listen noch
 * Zeiger darauf halten können (siehe language_switch). */
void text_cache_reload(void)
{
    int16_t i;
    open_text_index(0, TEXT_INDEX_BYTES(g_film_count), TEXT_INDEX_BYTES(g_film_count), 1);
    for (i = 0; i < 0x31; i++)
        if (*(int16_t *)FILM_CACHE(i) > 0)
            load_entry(*(int16_t *)FILM_CACHE(i), (ptr32 *)(FILM_CACHE(i) + 2), 2);
    close_text_index();
    open_text_index(1, TEXT_INDEX_BYTES(g_series_count), TEXT_INDEX_BYTES(g_series_count), 1);
    for (i = 0; i < 0xf; i++)
        if (*(int16_t *)SERIES_CACHE(i) > 0)
            load_entry(*(int16_t *)SERIES_CACHE(i), (ptr32 *)(SERIES_CACHE(i) + 2), 0x16);
    close_text_index();
    open_text_index(2, TEXT_INDEX_BYTES(g_show_count), (uint16_t)(g_show_count + 1), 2);
    for (i = 0; i < 10; i++)
        if (*(int16_t *)SHOW_CACHE(i) > 0)
            load_entry(*(int16_t *)SHOW_CACHE(i), (ptr32 *)(SHOW_CACHE(i) + 2), 0x10);
    close_text_index();
    open_text_index(4, 0x322, 0x322, 1);
    for (i = 0; i < 0x18; i++)
        if (MEM_PTR(char, NEWS_SLOT(i) + 2) != NULL && *(int16_t *)NEWS_SLOT(i) > 0)
            text_cache_load_news(i, *(int16_t *)NEWS_SLOT(i));
    close_text_index();
}
