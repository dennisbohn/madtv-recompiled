#include "rtlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgroup.h"
#include "hes.h"
#include "platform.h"
#include "unpack.h"
#include "video.h"

/* Globale Variablen der Hardware-Bibliothek im Datensegment */
#define MOUSE_DX     DS_I16(0x16a2)
#define MOUSE_DY     DS_I16(0x16a4)
#define KEYMAP_ASCII DS_ADDR(uint8_t, 0x16ac)
#define KEYMAP_SCAN  DS_ADDR(uint8_t, 0x172e)
#define TICK_COUNTER DS_U16(0x9014)

/* Reserve hinter jeder Bitmap: die Shift-Blitter schreiben bis zu 8 Byte über das
 * Zeilenende hinaus (im Original harmlos, hier soll kein Heap-Block überschrieben werden). */
enum { BITMAP_SLACK = 16 };

/* ======================================================================== Video */

int16_t vga_detect(void)
{
    return 0x100;
}

void vga_set_mode13(void)
{
    video_clear(0);
}

void vga_restore_mode(void)
{
}

void vga_init_segs(void)
{
    /* Original: DS:16a6 = DS:16a8 = A000h. Der Bildschirm ist hier g_screen. */
}

void vga_wait_vsync(void)
{
    platform_present_now();
}

void vga_set_palette(const void *pal)
{
    const uint8_t *p = pal;
    int n = p[0] | (p[1] << 8);
    uint8_t rgb[768] = {0};
    if (n > 256)
        n = 256;
    for (int i = 0; i < n * 3; i++)
        rgb[i] = (uint8_t)((p[2 + i] & 63) * 255 / 63);
    video_set_palette(rgb);
}

/* ======================================================================== Bitmaps */

Bitmap *bitmap_alloc(int16_t w_px, int16_t h)
{
    int16_t cols = (int16_t)(w_px >> 3);
    if (w_px & 7)
        cols++;
    uint16_t size = (uint16_t)(cols * h * 8 + 8); /* 16-Bit-Arithmetik wie im Original */
    Bitmap *b = tc_malloc(size);
    if (!b)
        return NULL;
    b->wcols = cols;
    b->h = h;
    b->pixels = (uint8_t *)b + 8;
    return b;
}

uint16_t spritebank_size(const SpriteBank *b)
{
    uint16_t size = (uint16_t)(b->count * 8 + 2);
    for (int i = 0; i < b->count; i++)
        size = (uint16_t)(size + b->bmp[i].wcols * b->bmp[i].h * 8);
    return size;
}

void scr_grab(Bitmap *dst, int16_t xcol, int16_t y)
{
    int w = dst->wcols * 8;
    const uint8_t *s = g_screen + (uint16_t)(y * SCREEN_W + xcol * 8);
    uint8_t *d = dst->pixels;
    for (int r = 0; r < dst->h; r++, s += SCREEN_W, d += w)
        memcpy(d, s, (size_t)w);
}

void scr_blit(const Bitmap *src, int16_t xcol, int16_t y)
{
    int w = src->wcols * 8;
    uint8_t *d = g_screen + (uint16_t)(y * SCREEN_W + xcol * 8);
    const uint8_t *s = src->pixels;
    for (int r = 0; r < src->h; r++, s += w, d += SCREEN_W)
        memcpy(d, s, (size_t)w);
}

void bmp_fill(Bitmap *dst, int16_t xcol, int16_t y, int16_t wcols, int16_t h, uint8_t color)
{
    int stride = dst->wcols * 8, w = wcols * 8;
    uint8_t *d = dst->pixels + (uint16_t)(y * stride) + xcol * 8;
    for (int r = 0; r < h; r++, d += stride)
        memset(d, color, (size_t)w);
}

void bmp_blit(const BlitArgs *a)
{
    int ds = a->dst->wcols * 8, w = a->src->wcols * 8;
    uint8_t *d = a->dst->pixels + (uint16_t)(a->y * ds) + a->xcol * 8;
    const uint8_t *s = a->src->pixels;
    for (int r = 0; r < a->src->h; r++, s += w, d += ds)
        memcpy(d, s, (size_t)w);
}

void bmp_blit_rect(const BlitRectArgs *a)
{
    int ss = a->src->wcols * 8, ds = a->dst->wcols * 8, w = a->w * 8;
    const uint8_t *s = a->src->pixels + (uint16_t)(a->sy * ss) + a->sx * 8;
    uint8_t *d = a->dst->pixels + (uint16_t)(a->dy * ds) + a->dx * 8;
    for (int r = 0; r < a->h; r++, s += ss, d += ds)
        memcpy(d, s, (size_t)w);
}

static void copy_trans(uint8_t *d, const uint8_t *s, int n)
{
    for (int i = 0; i < n; i++)
        if (s[i])
            d[i] = s[i];
}

/* Gemeinsamer Kern von 43fd (opak) und 4432 (transparent): Teilrechteck mit Pixelversatz.
 * Bildet das Original exakt nach, inklusive der Breitenkorrektur, wenn das Rechteck nicht
 * bis zum rechten Rand der Quelle reicht, und der Sonderbehandlung bei sx*8 mod 256 != 0. */
static void blit_rect_shift(const BlitRectShiftArgs *a, int trans)
{
    int ds = a->dst->wcols * 8, ss = a->src->wcols * 8;
    int xpix = a->xpix;
    int bx = a->w * 8;
    int sx8 = (uint16_t)(a->sx * 8);
    uint8_t *d = a->dst->pixels + (uint16_t)(a->dy * ds) + a->dx * 8 + xpix;
    if (sx8 + bx != ss)
        bx = bx + 8 - xpix;
    int sskip = ss - bx, dskip = ds - bx;
    const uint8_t *s = a->src->pixels + sx8 + (uint16_t)(a->sy * ss);
    if (sx8 & 0xff) {
        d -= xpix;
        bx += xpix;
        s -= xpix;
        dskip -= xpix;
        sskip -= xpix;
    }
    /* opak: rep movsw mit bx/2 Wörtern; transparent: bx/8 Gruppen à 8 Pixel.
     * Ist bx kein Vielfaches davon, verschiebt sich jede Zeile wie im Original um den Rest. */
    int n = trans ? (bx >> 3) * 8 : (bx >> 1) * 2;
    for (int r = 0; r < a->h; r++) {
        if (trans)
            copy_trans(d, s, n);
        else
            memmove(d, s, (size_t)n);
        d += n + dskip;
        s += n + sskip;
    }
}

void bmp_blit_rect_shift(const BlitRectShiftArgs *a)
{
    blit_rect_shift(a, 0);
}

void bmp_blit_rect_trans_shift(const BlitRectShiftArgs *a)
{
    blit_rect_shift(a, 1);
}

void bmp_glyph_trans(const BlitShiftArgs *a)
{
    int ds = a->dst->wcols * 8;
    uint8_t *d = a->dst->pixels + (uint16_t)(a->y * ds) + a->xcol * 8 + a->xpix;
    const uint8_t *s = a->src->pixels;
    int rows = (uint8_t)a->src->h; /* Original zählt nur das Low-Byte */
    if (rows == 0)
        rows = 256;
    for (int r = 0; r < rows; r++, s += 8, d += ds)
        copy_trans(d, s, 8);
}

void bmp_blit_rect_trans(const BlitRectArgs *a)
{
    int ss = a->src->wcols * 8, ds = a->dst->wcols * 8, w = (a->w * 8) >> 3 << 3;
    const uint8_t *s = a->src->pixels + (uint16_t)(a->sy * ss) + a->sx * 8;
    uint8_t *d = a->dst->pixels + (uint16_t)(a->dy * ds) + a->dx * 8;
    for (int r = 0; r < a->h; r++, s += ss, d += ds)
        copy_trans(d, s, w);
}

void bmp_blit_trans_shift(const BlitShiftArgs *a)
{
    int ds = a->dst->wcols * 8, w = a->src->wcols * 8;
    uint8_t *d = a->dst->pixels + (uint16_t)(a->y * ds) + a->xcol * 8 + a->xpix;
    const uint8_t *s = a->src->pixels;
    for (int r = 0; r < a->src->h; r++, s += w, d += ds)
        copy_trans(d, s, w);
}

void scr_blit_rect(const ScrRectArgs *a)
{
    int ss = a->src->wcols * 8, w = a->w * 8;
    const uint8_t *s = a->src->pixels + (uint16_t)(a->sy * ss) + a->sx * 8;
    uint8_t *d = g_screen + (uint16_t)(a->dy * SCREEN_W + a->dx * 8);
    for (int r = 0; r < a->h; r++, s += ss, d += SCREEN_W)
        memcpy(d, s, (size_t)w);
}

/* ======================================================================== Maus */

int16_t mouse_init(void)
{
    return 1;
}

void mouse_setup(void)
{
    int16_t dx, dy;
    uint16_t b;
    platform_mouse_take(&dx, &dy, &b); /* aufgelaufene Bewegung verwerfen */
    MOUSE_DX = 0;
    MOUSE_DY = 0;
}

int16_t mouse_poll(void)
{
    int16_t dx, dy;
    uint16_t buttons;
    platform_mouse_take(&dx, &dy, &buttons);
    int16_t ax, ay;
    if (platform_mouse_abs(&ax, &ay)) {
        /* PORT: Windows-Zeiger absolut – Bewegung bis dorthin melden (Zeiger DS:7bd6/7bd8 plus
         * noch nicht verarbeitete Bewegung DS:7a28/7a2a) */
        dx = (int16_t)(ax - DS_I16(0x7bd6) - DS_I16(0x7a28));
        dy = (int16_t)(ay - DS_I16(0x7bd8) - DS_I16(0x7a2a));
    }
    MOUSE_DX = dx;
    MOUSE_DY = dy;
    return (int16_t)buttons;
}

/* ======================================================================== Tastatur */

int16_t kbd_init(void)
{
    return 1;
}

void kbd_flush(void)
{
    platform_key_flush();
}

int16_t kbd_getkey(void)
{
    int32_t k = platform_key_get();
    if (k < 0)
        return -1;
    uint8_t ascii = (uint8_t)(k & 0xff), scan = (uint8_t)(k >> 8);
    uint8_t code = ascii ? KEYMAP_ASCII[ascii & 0x7f] : KEYMAP_SCAN[scan & 0x7f];
    return code == 0xff ? -1 : code;
}

void kbd_shutdown(void)
{
}

/* ======================================================================== Zeit */

/* Der AdLib-Treiber stellt den PIT auf 1193182/11930 = 100,015 Hz und zählt DS:9014 bei jedem
 * zweiten Interrupt hoch: 50,008 Hz (re/notes/sound_driver.md). Als exakter Bruch: */
enum { TICK_NUM = 1193182, TICK_DEN = 11930 * 2 * 1000 }; /* Ticks pro Millisekunde */
static uint32_t g_tick_base_ms;
static uint16_t g_tick_base;
static uint16_t g_last_tick;

int16_t timer_init(void)
{
    return -1;
}

void timer_shutdown(void)
{
}

void ticks_set(uint16_t n)
{
    g_tick_base = n;
    g_tick_base_ms = platform_ms();
    TICK_COUNTER = n;
}

void ticks_reset(void)
{
    ticks_set(0);
}

/* Zählerstand ohne Ereignisverarbeitung und ohne Schlafen (für rand_range: im Original ein
 * einfacher Speicherzugriff; mit Schlafen würde der Zufallsseed in Schleifen springen) */
uint16_t ticks_peek(void)
{
    static int speed = -1; /* Testhilfe MADTV_SPEED=n: alle Spieltakte n-mal schneller */
    if (speed < 0) {
        speed = platform_debug_env("MADTV_SPEED");
        if (speed < 1)
            speed = 1;
    }
    uint32_t elapsed = platform_ms() - g_tick_base_ms;
    uint16_t t = (uint16_t)(g_tick_base + (uint32_t)((uint64_t)elapsed * (uint32_t)speed * TICK_NUM / TICK_DEN));
    TICK_COUNTER = t;
    return t;
}

uint16_t ticks_get(void)
{
    platform_pump();
    uint16_t t = ticks_peek();
    if (t == g_last_tick)
        platform_sleep_ms(1); /* Warteschleifen des Spiels nicht mit 100 % CPU drehen lassen */
    g_last_tick = t;
    TICK_COUNTER = t;
    return t;
}

/* ======================================================================== HES-Dateien */

static FILE *hes_open(int16_t no, const char *mode)
{
    char path[600];
    snprintf(path, sizeof path, "%s/%03d.HES", platform_game_dir(), no);
    return fopen(path, mode);
}

/* ---- Korrigierte Spieldaten aus der zweiten Programmversion ----
 * Die Spieldaten in "Gamefiles" sind an einigen Stellen unfertig (Shows ohne Länge und
 * Zuschauer-Bits, Darstellernamen "<Not set>", ältere Texte, Titelbild ohne Schriftzug und
 * Copyright, unfertige Abspannszene im Intro). Die zweite Version (Ordner "Gamefiles2" neben
 * "Gamefiles") enthält dieselben Dateien korrigiert in SOURCE1.RSC und SOURCE2.RSC:
 * Verzeichnis aus 6-Byte-Einträgen (u32 Offset, u16 Länge), Eintrag n = Datei n.HES.
 * Übernommen werden nur diese Daten-, Text- und Introdateien, keine Spielstände. */
static const int16_t g_overlay_files[][2] = { /* HES-Nummer, Archiv */
    {42, 1},  {43, 1},  {45, 1},  {46, 1},  {47, 1},  {199, 1}, {201, 1}, {203, 1}, {204, 1}, {206, 1},
    {207, 1}, {246, 1}, {251, 1}, {253, 1}, {300, 2}, {307, 2}, {308, 2}, {309, 2}, {327, 2}, {328, 2},
};
static uint8_t *g_rsc[3];
static long g_rsc_size[3];
static int g_rsc_tried[3];

static const uint8_t *hes_overlay(int16_t no, long *len)
{
    int which = 0;
    for (size_t i = 0; i < sizeof g_overlay_files / sizeof *g_overlay_files; i++)
        if (g_overlay_files[i][0] == no)
            which = g_overlay_files[i][1];
    if (!which)
        return NULL;
    if (!g_rsc_tried[which]) {
        g_rsc_tried[which] = 1;
        char path[600];
        snprintf(path, sizeof path, "%s2/SOURCE%d.RSC", platform_game_dir(), which);
        FILE *f = fopen(path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            g_rsc_size[which] = ftell(f);
            fseek(f, 0, SEEK_SET);
            g_rsc[which] = malloc((size_t)g_rsc_size[which]);
            if (g_rsc[which] && fread(g_rsc[which], 1, (size_t)g_rsc_size[which], f) != (size_t)g_rsc_size[which]) {
                free(g_rsc[which]);
                g_rsc[which] = NULL;
            }
            fclose(f);
            if (g_rsc[which])
                fprintf(stderr, "Spieldaten-Korrekturen aus %s\n", path);
        }
    }
    const uint8_t *rsc = g_rsc[which];
    long size = g_rsc_size[which];
    if (!rsc || size < 600 * 6)
        return NULL;
    const uint8_t *e = rsc + no * 6;
    uint32_t off = (uint32_t)(e[0] | e[1] << 8 | e[2] << 16 | (uint32_t)e[3] << 24);
    uint16_t l = (uint16_t)(e[4] | e[5] << 8);
    if (l == 0 || (long)off + l > size)
        return NULL;
    *len = l;
    return rsc + off;
}

int32_t hes_filesize(int16_t no)
{
    long ol;
    if (hes_overlay(no, &ol))
        return (int32_t)ol;
    FILE *f = hes_open(no, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fclose(f);
    return (int32_t)n;
}

/* Liest höchstens bis Dateiende. Windows prüft bei ReadFile den ganzen Zielpuffer, daher
 * darf nicht wie im Original pauschal 0xFFFF Byte angefordert werden. */
static int32_t hes_read_clamped(int16_t no, void *buf, long ofs, long len)
{
    long olen;
    const uint8_t *od = hes_overlay(no, &olen);
    if (od) {
        if (ofs > olen)
            ofs = olen;
        if (len > olen - ofs)
            len = olen - ofs;
        if (len > 0)
            memcpy(buf, od + ofs, (size_t)len);
        return (int32_t)(len > 0 ? len : 0);
    }
    FILE *f = hes_open(no, "rb");
    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (ofs > size)
        ofs = size;
    if (len > size - ofs)
        len = size - ofs;
    fseek(f, ofs, SEEK_SET);
    size_t n = len > 0 ? fread(buf, 1, (size_t)len, f) : 0;
    fclose(f);
    return (int32_t)n;
}

int32_t hes_load(int16_t no, void *buf)
{
    return hes_read_clamped(no, buf, 0, 0xFFFF);
}

int32_t hes_read(int16_t no, void *buf, uint16_t ofs, uint16_t len)
{
    return hes_read_clamped(no, buf, ofs, len);
}

int32_t hes_write(int16_t no, const void *buf, uint16_t len)
{
    FILE *f = hes_open(no, "wb");
    if (!f)
        return -1;
    size_t n = fwrite(buf, 1, len, f);
    fclose(f);
    return (int32_t)n;
}

int32_t hes_write_dgroup(int16_t no, uint16_t len)
{
    return hes_write(no, g_ds, len);
}

void hes_sprite_info(int16_t no, int16_t *filesize, int16_t *unpacked_size, int16_t mode)
{
    (void)mode; /* wird im Original nicht ausgewertet */
    uint8_t hdr[1 + 2 * 255];
    int16_t n = 0;
    if (hes_read(no, hdr, 0, 1) == 1) {
        n = hdr[0];
        hes_read(no, hdr + 1, 1, (uint16_t)(n * 2));
    }
    uint16_t size = (uint16_t)(n * 8 + 2);
    for (int i = 0; i < n; i++) {
        int cols = hdr[1 + 2 * i] >> 3;
        if (hdr[1 + 2 * i] & 7)
            cols++;
        size = (uint16_t)(size + cols * hdr[2 + 2 * i] * 8);
    }
    *unpacked_size = (int16_t)size;
    *filesize = (int16_t)hes_filesize(no); /* Original speichert nur das Low-Word */
}

int16_t hes_list(int16_t *out)
{
    int16_t count = 0;
    for (int no = 0; no < 1000; no++) {
        FILE *f = hes_open((int16_t)no, "rb");
        if (f) {
            fclose(f);
            out[1 + count++] = (int16_t)no;
        }
    }
    out[0] = count;
    return count;
}

void hes_unpack_sprites(const uint8_t *src, SpriteBank *dst)
{
    int n = src[0];
    size_t hdr = 1 + 2 * (size_t)n;
    size_t size = hdr + 6 + 0x10000; /* genaue Größe unbekannt; Decoder prüft selbst auf Ende */
    HesSpriteSet set;
    if (hes_decode_sprites(src, size, &set) != 0) {
        dst->count = 0;
        return;
    }
    dst->count = (uint16_t)n;
    uint8_t *px = (uint8_t *)&dst->bmp[n];
    for (int i = 0; i < n; i++) {
        const HesSprite *s = &set.sprites[i];
        Bitmap *b = &dst->bmp[i];
        b->wcols = (int16_t)((s->w + 7) >> 3);
        b->h = (int16_t)s->h;
        b->pixels = px;
        for (int y = 0; y < s->h; y++)
            memcpy(px + y * b->wcols * 8, s->pixels + y * s->w, (size_t)s->w);
        px += b->wcols * 8 * b->h;
    }
    hes_free_sprites(&set);
}

void lzw_decode(uint8_t *dst, const uint8_t *src, void *work)
{
    (void)work;
    size_t packed = (size_t)(src[2] | (src[3] << 8)) + 8;
    size_t n = 0;
    uint8_t *out = lzw_unpack(src, packed, &n);
    if (out) {
        memcpy(dst, out, n);
        free(out);
    }
}

void rle_delta_frame_bmp(uint8_t **stream, Bitmap *dst)
{
    const uint8_t *s = *stream;
    /* Länge des Datenstroms ist dem Original unbekannt; begrenzt wird durch das Frame-Ende 0x62. */
    rle_delta_frame(&s, s + 0x10000, (uint8_t *)dst + 8, (size_t)dst->wcols * 8 * (size_t)dst->h);
    *stream = (uint8_t *)s;
}

int16_t dos_get_drive(int16_t *out)
{
    out[0] = 1;
    out[1] = 2; /* C: */
    return 1;
}

int16_t dos_set_drive(int16_t drive)
{
    (void)drive;
    return 26;
}

/* ======================================================================== Turbo-C-Laufzeit */

/* Zustand des Zufallsgenerators liegt wie im Original im Datensegment (DS:1940) */
#define RAND_SEED DS_U32(0x1940)

void tc_srand(uint16_t seed)
{
    RAND_SEED = seed;
}

int16_t tc_rand(void)
{
    RAND_SEED = RAND_SEED * 0x015A4E35u + 1;
    return (int16_t)((RAND_SEED >> 16) & 0x7FFF);
}

void *tc_malloc(uint16_t n)
{
    return malloc((size_t)n + BITMAP_SLACK);
}

void *tc_farmalloc(uint32_t n)
{
    return malloc((size_t)n + BITMAP_SLACK);
}

void *tc_calloc(uint16_t n, uint16_t size)
{
    return calloc(1, (size_t)n * size + BITMAP_SLACK);
}

void *tc_farcalloc(uint32_t n, uint32_t size)
{
    return calloc(1, (size_t)n * size + BITMAP_SLACK);
}

void tc_free(void *p)
{
    free(p);
}

static char *tc_numtoa(uint32_t v, int neg, char *buf, int radix)
{
    char tmp[34];
    int i = 0;
    do {
        int dgt = (int)(v % (uint32_t)radix);
        tmp[i++] = (char)(dgt < 10 ? '0' + dgt : 'a' + dgt - 10);
        v /= (uint32_t)radix;
    } while (v);
    char *p = buf;
    if (neg)
        *p++ = '-';
    while (i)
        *p++ = tmp[--i];
    *p = 0;
    return buf;
}

char *tc_itoa(int16_t v, char *buf, int16_t radix)
{
    if (radix == 10 && v < 0)
        return tc_numtoa((uint32_t)(-(int32_t)v), 1, buf, 10);
    return tc_numtoa(radix == 10 ? (uint32_t)v : (uint16_t)v, 0, buf, radix);
}

char *tc_ltoa(int32_t v, char *buf, int16_t radix)
{
    if (radix == 10 && v < 0)
        return tc_numtoa((uint32_t)(-(int64_t)v), 1, buf, 10);
    return tc_numtoa((uint32_t)v, 0, buf, radix);
}

char *tc_ultoa(uint32_t v, char *buf, int16_t radix)
{
    return tc_numtoa(v, 0, buf, radix);
}

int16_t tc_atoi(const char *s)
{
    return (int16_t)atoi(s);
}

int16_t tc_atexit(void (*fn)(void))
{
    return (int16_t)atexit(fn);
}

void tc_exit(int16_t code)
{
    exit(code);
}
