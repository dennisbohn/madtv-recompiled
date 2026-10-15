/* Segment 1c74 – Ressourcen, Speicher, Zufall und Spieluhr.
 *
 * Basisschicht für fast alle anderen Module: HES-Dateien laden und entpacken, Speicher
 * anfordern/freigeben, Zufallszahlen, die Uhr im Spiel (Start 17:00 Uhr) und ein Cache für
 * gepackte Sprite-Bänke ("font_load"/"font_get"). */
#include <string.h>

#include "game.h"
#include "platform.h"

/* Verschachtelungszähler und Zustand */
#define g_timer_pause_depth DS_I16(0x0d02) /* timer_pause/timer_resume */
#define g_drive_io_depth    DS_I16(0x0d04) /* Laufwerkszugriffe (nur Zähler) */
#define g_clock_run_depth   DS_I16(0x0d06)
#define g_clock_stop_depth  DS_I16(0x0d08)
#define g_clock_seconds     DS_I16(0x0d0a) /* letzte berechnete Spielzeit in Sekunden */
#define g_figure_bank_slot  DS_ADDR(int16_t, 0x0d0c) /* je Figur-Slot geladene HES-Nummer */
#define g_clock_accum       DS_I32(0x6123) /* aufgelaufene Ticks * Tempo */
#define g_clock_offset      DS_I16(0x6121)
#define g_clock_speed       DS_I16(0x7282) /* Spielsekunden pro 50 Ticks */
/* PORT: Testhilfe MADTV_CLOCK beschleunigt nur die Spieluhr */
#define CLOCK_RATE          ((int32_t)g_clock_speed * platform_clock_factor())
#define g_clock_ticks       DS_U16(0x7284) /* Ticks seit letztem Zurücksetzen */
#define g_clock_stopped     DS_I16(0x7a72)
#define g_rand_salt         DS_I32(0x8e8c)
#define g_resource_cache    DS_ADDR(uint8_t, 0x7be0) /* Slots à 6 Byte: void *data; int16 unpacked_size */
#define g_figure_frames     DS_PTR(uint8_t, 0x00f8)  /* Figuren-Animationsbank */
#define g_text_table        DS_ADDR(ptr32, 0x8370)   /* Zeiger auf Spieltexte */

/* Slot des Ressourcen-Caches (6 Byte, im Original zwei Far-Pointer-Hälften + Größe) */
#define CACHE_DATA(slot) (MEM_PTR(void, g_resource_cache + (slot) * 6))
#define CACHE_SIZE(slot) (*(int16_t *)(g_resource_cache + (slot) * 6 + 4))

/* 1c74:0008 – Laufwerk merken und Liste aller HES-Dateien anlegen */
void hes_init_file_list(void)
{
    dos_get_drive(DS_ADDR(int16_t, 0x8e90));
    g_drive_io_depth++; /* Original: leerer Laufwerks-Hook 44f5:000f davor */
    debug_nop();
    dos_set_drive(DS_I16(0x8e92));
    hes_list(DS_ADDR(int16_t, 0x8c5c));
}

/* 1c74:0039 – liefert 1, wenn Datei 500 vorhanden ist (Spiel auf Festplatte), sonst 2 */
int16_t disk_detect(void)
{
    return hes_filesize(500) == -1 ? 2 : 1;
}

/* 1c74:0051 – Spieluhr während Ladevorgängen anhalten (verschachtelbar) */
void timer_pause(void)
{
    g_timer_pause_depth--;
    if (g_timer_pause_depth == 0) {
        clock_suspend();
        drive_io_begin();
    }
}

/* 1c74:0067 – Gegenstück zu timer_pause */
void timer_resume(void)
{
    g_timer_pause_depth++;
    if (g_timer_pause_depth == 1) {
        drive_io_end();
        clock_resume();
    }
}

/* 1c74:007d */
void drive_io_begin(void)
{
    g_drive_io_depth++; /* Original: leerer Laufwerks-Hook 44f5:000f davor */
    debug_nop();
}

/* 1c74:008c */
void drive_io_end(void)
{
    /* Original: DOS-Puffer leeren (INT 21h/0Dh) und leerer Hook 44f5:0010 – entfällt */
    g_drive_io_depth--;
    debug_nop();
}

/* 1c74:00a0 – leerer Debug-Haken */
void debug_nop(void)
{
}

/* 1c74:00a1 – HES-Datei ungepackt in einen neuen Puffer laden */
void *hes_load_raw(int16_t hes_no)
{
    int16_t unpacked;
    timer_pause();
    int16_t size = hes_size_or_ask_disk(hes_no, &unpacked, 0);
    void *buf = far_calloc((uint16_t)size, 1);
    hes_read_into(hes_no, buf);
    timer_resume();
    return buf;
}

/* 1c74:00f4 */
void hes_read_into(int16_t hes_no, void *buf)
{
    timer_pause();
    hes_load(hes_no, buf);
    timer_resume();
}

/* 1c74:0111 – Sprite-Bank laden und entpacken (Modus 0) */
SpriteBank *hes_load_unpack0(int16_t hes_no)
{
    int16_t unpacked;
    int16_t size = hes_size_or_ask_disk(hes_no, &unpacked, 0);
    SpriteBank *bank = far_calloc((uint16_t)unpacked, 1);
    uint8_t *packed = far_calloc((uint16_t)size, 1);
    hes_read_into(hes_no, packed);
    hes_unpack_sprites(packed, bank); /* Original übergibt zusätzlich Modus 0 (ungenutzt) */
    far_free(packed);
    return bank;
}

/* 1c74:018e – Sprite-Bank laden und entpacken (Modus 1) */
SpriteBank *hes_load_unpack(int16_t hes_no)
{
    int16_t unpacked;
    int16_t size = hes_size_or_ask_disk(hes_no, &unpacked, 1);
    SpriteBank *bank = far_calloc((uint16_t)unpacked, 1);
    uint8_t *packed = far_calloc((uint16_t)size, 1);
    hes_read_into(hes_no, packed);
    hes_unpack_sprites(packed, bank); /* Original übergibt zusätzlich Modus 1 (ungenutzt) */
    far_free(packed);
    return bank;
}

/* 1c74:020d – Zufallszahl 1..n; neu gesät aus Ticks, seed und einem globalen Salz */
int16_t rand_range(int16_t n, int16_t seed)
{
    /* PORT: Das Spiel zieht oft in Wiederholschleifen ("bis ein freier Eintrag kommt"). Innerhalb
     * desselben 50-Hz-Takts liefert derselbe seed aber dieselbe Zahl; das Original drehte daher,
     * bis der nächste Takt kam (beim Start eines neuen Spiels fast 2 Sekunden). Hier wird bei
     * einer Wiederholung mit gleichem Takt und seed sofort der Takt verwendet, den das Original
     * nach dem Warten gehabt hätte; die Folge der gezogenen Werte entspricht dem Original. */
    static uint16_t last_t, last_seed, skip;
    uint16_t t = ticks_peek();
    if (t == last_t && (uint16_t)seed == last_seed)
        skip++; /* Wiederholung: so, als wäre schon ein weiterer Takt vergangen */
    else
        skip = 0;
    last_t = t;
    last_seed = (uint16_t)seed;
    tc_srand((uint16_t)(t + skip + seed + (uint16_t)g_rand_salt)); /* srand nimmt nur das Low-Word */
    return (int16_t)(tc_rand() % n + 1);
}

/* 1c74:023d – Spieluhr auf 0 setzen */
void clock_reset(void)
{
    ticks_reset();
    ticks_set(0);
    g_clock_accum = 0;
    g_clock_run_depth++;
    debug_nop();
}

/* 1c74:0260 – bisher gelaufene Zeit in den Akkumulator übernehmen */
void clock_suspend(void)
{
    if (g_clock_stopped == 0)
        g_clock_accum += (int32_t)ticks_get() * CLOCK_RATE;
    timer_shutdown();
    g_clock_run_depth--;
    debug_nop();
}

/* 1c74:0291 */
void clock_resume(void)
{
    ticks_reset();
    ticks_set(0);
    g_clock_offset = (int16_t)-(50 / (int32_t)g_game_speed_setting);
    g_clock_run_depth++;
    debug_nop();
}

/* 1c74:02b5 – zuletzt berechnete Spielzeit (Sekunden) ohne Neuberechnung */
void clock_get_cached(int16_t *out_seconds)
{
    *out_seconds = g_clock_seconds;
}

/* 1c74:02c3 – Spielzeit in Sekunden berechnen; setzt Stunde/Minute/Sekunde (Tag beginnt 17:00) */
void clock_get_seconds(int16_t *out_seconds)
{
    static int last_factor = 1; /* PORT: Testhilfe MADTV_CLOCK – Faktorwechsel ohne Zeitsprung */
    if (platform_clock_factor() != last_factor) {
        if (g_clock_stopped == 0) {
            g_clock_accum += (int32_t)ticks_get() * g_clock_speed * last_factor;
            ticks_set(0);
        }
        last_factor = platform_clock_factor();
    }
    g_clock_ticks = g_clock_stopped == 0 ? ticks_get() : 0;
    if (g_clock_ticks >= 60000u) {
        /* Tickzähler vor dem 16-Bit-Überlauf in den Akkumulator umbuchen */
        clock_suspend();
        clock_resume();
        g_clock_ticks = ticks_get();
    }
    *out_seconds = (int16_t)((uint32_t)g_clock_accum / 50u + (uint32_t)(g_clock_ticks / 50u) * (uint32_t)CLOCK_RATE);

    int16_t hours = (int16_t)(*out_seconds / 3600);
    int16_t minutes = (int16_t)(*out_seconds / 60 - hours * 60);
    int16_t seconds = (int16_t)(*out_seconds - minutes * 60 - hours * 3600);
    hours += 17;
    if (hours >= 24)
        hours -= 24;
    g_clock_seconds = *out_seconds;
    g_hour = hours;
    g_minute = minutes;
    g_second = seconds;
}

/* 1c74:0384 – Pixelzeiger einer Sprite-Bank nach dem Laden/Kopieren neu setzen */
void spritebank_fix_pointers(SpriteBank *bank)
{
    int16_t n = (int16_t)bank->count;
    uint8_t *px = (uint8_t *)&bank->bmp[n];
    for (int16_t i = 1; i <= n; i++) {
        Bitmap *b = &bank->bmp[i - 1];
        b->pixels = P32(px);
        px += b->h * b->wcols * 8;
    }
}

/* 1c74:03e4 – Speicher freigeben und Zeiger nullen (Zeiger-Slot im Datensegment bzw. in einem
 * Datenblock des Spiels, 4 Byte) */
void far_free_ptr(ptr32 *pp)
{
    far_free(PTR32(void, *pp));
    *pp = 0;
}

/* 1c74:0408 – Bytes eines Figuren-Animationsbilds der Breite w */
int32_t figure_frame_bytes(int16_t w, int16_t with_extra)
{
    int16_t cols = with_extra == 0 ? DS_I16(0x836e) : (int16_t)(DS_I16(0x836e) + DS_I16(0x836a));
    return (int32_t)cols * w / 8;
}

/* 1c74:0457 – Dateigröße und entpackte Größe ermitteln; fehlt die Datei, zur Diskette auffordern */
int16_t hes_size_or_ask_disk(int16_t hes_no, int16_t *unpacked_size, int16_t mode)
{
    int16_t size;
    for (;;) {
        hes_sprite_info(hes_no, &size, unpacked_size, mode);
        if (size != -1)
            break;
        /* Diskette 1 enthält die Dateien bis 300, Diskette 2 den Rest */
        disk_request_dialog(0, hes_no > 300 ? 0x1ad : 0x11f);
        do {
            game_tick();
        } while (DS_I16(0x7af4) == 0); /* bis Maustaste */
        disk_request_dialog(1, 0);
    }
    return size;
}

/* 1c74:04d1 – gepackte Sprite-Bank in den Ressourcen-Cache laden */
void font_load(int16_t hes_no, int16_t slot, int16_t mode)
{
    hes_size_or_ask_disk(hes_no, &CACHE_SIZE(slot), mode);
    MEM_SET_PTR(g_resource_cache + slot * 6, hes_load_raw(hes_no));
}

/* 1c74:0517 – Kopie einer gecachten Sprite-Bank entpacken (Aufrufer gibt frei) */
SpriteBank *font_get(int16_t slot, int16_t mode)
{
    SpriteBank *bank = far_calloc((uint16_t)CACHE_SIZE(slot), 1);
    hes_unpack_sprites(CACHE_DATA(slot), bank); /* Original übergibt zusätzlich mode (ungenutzt) */
    (void)mode;
    return bank;
}

/* 1c74:0570 – Figuren-Sprites aller Spieler und der Nebenfiguren laden */
void figure_sprites_load_all(void)
{
    int16_t i;
    for (i = 0; i < 3; i++) {
        if (i == DS_I16(0x7af2) && DS_ADDR(int16_t, 0x7b3c)[DS_I16(0x7af2) * 15] == 2)
            figure_sprites_load(0x18, i);
        else
            figure_sprites_load((int16_t)(i + 0x11), i);
    }
    figure_sprites_load(0x14, 3);
    figure_sprites_load(0x15, 4);
    if (DS_I16(0x7a3c) >= 1 && DS_I16(0x7a3c) <= 2)
        figure_sprites_load(0x17, 5);
    else if (DS_I16(0x7a3c) == 3 || DS_I16(0x7a3c) == 4)
        figure_sprites_load(0x16, 5);
}

/* 1c74:0610 – Animationsbilder einer Figur in die Figurenbank kopieren (nur bei Wechsel) */
void figure_sprites_load(int16_t hes_no, int16_t slot)
{
    uint16_t frame = (uint16_t)figure_frame_bytes(0x130, 1);
    uint8_t *dst = g_figure_frames + (uint16_t)((uint16_t)(frame * (uint16_t)slot) * 10u) + 0x1e2;
    if (g_figure_bank_slot[slot] != hes_no) {
        SpriteBank *bank = hes_load_unpack(hes_no);
        far_memcpy(dst, (uint8_t *)bank + 0x52, (uint16_t)((uint16_t)figure_frame_bytes(0x130, 1) * 10u));
        far_free(bank);
        g_figure_bank_slot[slot] = hes_no;
    }
}

/* 1c74:06a3 – genullten Speicher anfordern */
void *far_calloc(uint16_t n, uint16_t size)
{
    return tc_farcalloc(n, size);
}

/* 1c74:06c9 */
void far_free(void *p)
{
    tc_free(p);
}

/* 1c74:06db – Spieluhr anhalten (z. B. in Menüs), verschachtelbar */
void clock_stop(void)
{
    g_clock_stop_depth++;
    if (g_clock_stop_depth == 1) {
        g_clock_accum += (int32_t)ticks_get() * CLOCK_RATE;
        ticks_set(0);
        g_clock_stopped = 1;
    }
}

/* 1c74:0712 */
void clock_start(void)
{
    g_clock_stop_depth--;
    if (g_clock_stop_depth == 0) {
        g_clock_stopped = 0;
        ticks_set(0);
        g_clock_offset = 0;
    }
}

/* 1c74:0733 – Kopie eines Spieltexts in neuem 500-Byte-Puffer */
char *text_get_copy(int16_t text_id)
{
    char *buf = far_calloc(500, 1);
    const char *src = PTR32(char, g_text_table[text_id]);
    far_memcpy(buf, src, (uint16_t)strlen(src));
    return buf;
}
