/* Segment 4301 – Intro: Titelbild und 14 animierte Szenen (bis Taste oder Mausklick).
 *
 * Jede Szene (Tabelle DS:161c, 8 Byte: Hintergrund, bis zu drei Animationsströme) zeigt ein
 * Hintergrundbild in einer 208x144-Bitmap und spielt LZW-gepackte Delta-Animationen ab.
 * Ein Strom beginnt mit der Bildanzahl (Byte), danach folgen RLE-Delta-Bilder. Die Szenen-
 * parameter übergibt intro_play über globale Variablen. */
#include "game.h"

#define g_intro_bg       DS_PTR(SpriteBank, 0x160a) /* Hintergrund der Szene */
#define g_intro_stream1  DS_PTR(uint8_t, 0x160e)
#define g_intro_stream2  DS_PTR(uint8_t, 0x1612)
#define g_intro_stream3  DS_PTR(uint8_t, 0x1616)
#define g_intro_abort    DS_I16(0x161a) /* Taste/Maus gedrückt */
#define g_intro_scenes   DS_ADDR(int16_t, 0x161c)
#define g_intro_next_str DS_I16(0x168c) /* nächster Strom 1..3 */
#define g_intro_loops    DS_I16(0x902c)
#define g_intro_y        DS_I16(0x902e)
#define g_intro_done     DS_I16(0x9030)
#define g_intro_scene    DS_I16(0x9032)
#define g_intro_frame    DS_I16(0x9034)
#define g_intro_canvas   DS_PTR(Bitmap, 0x9036)
#define g_intro_cur      DS_PTR(int8_t, 0x903a)  /* Anfang des laufenden Stroms (Bildanzahl) */
#define g_intro_hold     DS_I16(0x903e)          /* > 1000: Standbild für einige Takte */
#define g_intro_delay    DS_U16(0x9040)          /* Ticks pro Bild */
#define g_intro_pos      DS_PTR(uint8_t, 0x9042) /* Leseposition im Strom */
#define g_intro_streams  DS_I16(0x9046)          /* Anzahl Ströme der Szene */

/* 4301:0001 – LZW-Ressource laden und entpacken (neuer Puffer) */
void *intro_load_frame(int16_t hes_no)
{
    uint16_t len;
    hes_read(hes_no, &len, 0, 2);
    uint8_t *dst = far_calloc(len, 1);
    void *work = far_calloc(0x2000, 1);
    uint8_t *packed = hes_load_raw(hes_no);
    lzw_decode(dst, packed, work);
    far_free(packed);
    far_free(work);
    return dst;
}

/* 4301:0090 – Szene g_intro_scene vorbereiten: Hintergrund und Animationsströme laden */
void intro_scene_setup(void)
{
    if (g_intro_stream1)
        far_free_ptr(DS_ADDR(ptr32, 0x160e));
    if (g_intro_stream2)
        far_free_ptr(DS_ADDR(ptr32, 0x1612));
    if (g_intro_stream3)
        far_free_ptr(DS_ADDR(ptr32, 0x1616));
    if (g_intro_bg)
        far_free_ptr(DS_ADDR(ptr32, 0x160a));

    const int16_t *scene = &g_intro_scenes[(g_intro_scene - 1) * 4];
    DS_SET_PTR(0x160a, hes_load_unpack0(scene[0]));
    bmp_copy_at(&g_intro_bg->bmp[0], g_intro_canvas, 0, 0);
    if (g_intro_scene == 1)
        far_free_ptr(DS_ADDR(ptr32, 0x160a));
    if (scene[1] != 0)
        DS_SET_PTR(0x160e, intro_load_frame(scene[1]));
    if (scene[2] != 0)
        DS_SET_PTR(0x1612, intro_load_frame(scene[2]));
    if (scene[3] != 0)
        DS_SET_PTR(0x1616, intro_load_frame(scene[3]));
    scr_blit(g_intro_canvas, 4, g_intro_y);
}

/* 4301:01f7 – nächstes Delta-Bild auf die Leinwand anwenden */
void intro_frame_step(void)
{
    vga_wait_vsync();
    uint8_t *pos = g_intro_pos; /* Leseposition steht als 4-Byte-Zeiger im Datensegment */
    rle_delta_frame_bmp(&pos, g_intro_canvas);
    DS_SET_PTR(0x9042, pos);
}

/* 4301:0212 – ein Animationsschritt: Strom wechseln oder nächstes Bild, dann anzeigen */
void intro_tick(void)
{
    if (g_intro_frame == 0) {
        if (g_intro_streams < g_intro_next_str) {
            /* alle Ströme durch: Durchlauf zählen, Szene ggf. beenden */
            g_intro_next_str = 1;
            g_intro_loops++;
            if ((g_intro_scene != 3 || g_intro_loops == 3) && (g_intro_scene != 9 || g_intro_loops == 3) &&
                (g_intro_scene != 5 || g_intro_loops == 2))
                g_intro_done = 1;
        }
        if (g_intro_next_str == 1) {
            if (g_intro_scene != 1)
                bmp_copy_at(&g_intro_bg->bmp[0], g_intro_canvas, 0, 0);
            g_intro_hold = 0;
        }
        uint8_t *stream = g_intro_next_str == 1   ? g_intro_stream1
                          : g_intro_next_str == 2 ? g_intro_stream2
                          : g_intro_next_str == 3 ? g_intro_stream3
                                                  : NULL;
        if (stream) {
            DS_SET_PTR(0x903a, (int8_t *)stream);
            DS_SET_PTR(0x9042, stream + 1);
        }
        g_intro_next_str++;
        g_intro_frame++;
    } else if (g_intro_hold <= 1000) {
        intro_frame_step();
        g_intro_frame++;
        if (*g_intro_cur + 1 == g_intro_frame)
            g_intro_frame = 0;
    } else {
        g_intro_hold--;
    }

    if (g_intro_done == 0) {
        /* Szene 2: bei Bild 35 kurz anhalten */
        if (g_intro_scene == 2 && g_intro_frame == 0x23 && g_intro_hold == 0)
            g_intro_hold = 0x3ed;
        scr_blit(g_intro_canvas, 4, g_intro_y);
    } else {
        g_intro_next_str = 1;
    }
}

/* 4301:0361 – Szene abspielen, bis sie endet oder Taste/Maus gedrückt wird */
void intro_run_scene(void)
{
    g_intro_frame = 0;
    g_intro_done = 0;
    for (;;) {
        ticks_set(0);
        intro_tick();
        while (ticks_get() < g_intro_delay)
            ;
        if (kbd_getkey() != -1 || mouse_poll() > 0)
            g_intro_abort = 1;
        if (g_intro_done != 0 || g_intro_abort != 0)
            return;
    }
}

static void scene(int16_t no, int16_t y, int16_t streams, uint16_t delay, int16_t reset_loops)
{
    if (g_intro_abort != 0)
        return;
    g_intro_scene = no;
    if (y >= 0)
        g_intro_y = y;
    intro_scene_setup();
    g_intro_streams = streams;
    g_intro_delay = delay;
    if (reset_loops)
        g_intro_loops = 0;
    intro_run_scene();
}

static void scene_still(int16_t no, int16_t y)
{
    if (g_intro_abort != 0)
        return;
    g_intro_scene = no;
    if (y >= 0)
        g_intro_y = y;
    intro_scene_setup();
    ticks_set(0);
    while (ticks_get() < 0x96) /* 3 Sekunden Standbild */
        ;
}

/* 4301:03ad – Titelbild und Intro in Endlosschleife bis Taste/Maus */
void intro_play(void)
{
    /* Titelbild aus drei Teilen (HES 307-309) */
    Bitmap *title = bitmap_alloc(0x140, 200);
    DS_SET_PTR(0x160e, (uint8_t *)title);
    bmp_fill(title, 0, 0, 0x28, 200, 0);
    static const int16_t parts[3][2] = {{0x133, 0}, {0x134, 0x70}, {0x135, 0xd0}};
    for (int16_t i = 0; i < 3; i++) {
        SpriteBank *part = hes_load_unpack0(parts[i][0]);
        bmp_copy_at(&part->bmp[0], title, parts[i][1], 1);
        far_free(part);
    }
    scr_blit(title, 0, 0);
    far_free(title);
    DS_SET_PTR(0x9036, bitmap_alloc(0xd0, 0x90));
    bmp_fill(g_intro_canvas, 0, 0, 0x1a, 0x90, 0);
    DS_SET_PTR(0x160e, NULL);
    DS_SET_PTR(0x1612, NULL);
    /* Titelbild 100 Takte (2 Sekunden) stehen lassen, nicht abbrechbar.
     * PORT: eine vom Anklicken der Sprache noch gedrückte Maustaste soll das Intro nicht
     * überspringen – erst loslassen. */
    ticks_set(0);
    while (ticks_get() < 100)
        ;
    while (mouse_poll() != 0)
        ;
    while (kbd_getkey() != -1)
        ;
    g_intro_abort = 0;

    while (g_intro_abort == 0) {
        scene(7, 0x10, 1, 0xf, 0);
        scene(2, 0x11, 1, 10, 0);
        scene(9, 0x10, 1, 10, 1);
        scene(8, 0x10, 1, 7, 0);
        scene(0xd, 0x10, 2, 7, 0);
        scene(1, 0x10, 3, 2, 0);
        scene_still(10, 0x10);
        scene(5, 0x10, 1, 10, 1);
        if (g_intro_abort == 0) {
            scene(10, 0x10, 1, 7, 0);
            bmp_copy_at(&g_intro_bg->bmp[0], g_intro_canvas, 0, 0);
            scr_blit(g_intro_canvas, 4, g_intro_y);
        }
        scene(3, 0x10, 2, 2, 1);
        scene(4, 0x11, 1, 6, 0);
        scene(3, 0x10, 2, 2, 1);
        scene(6, -1, 1, 9, 0);
        scene(3, -1, 2, 2, 1);
        scene(0xc, -1, 3, 5, 0);
        if (g_intro_abort == 0) {
            scene_still(0xe, -1);
            g_intro_streams = 1;
            g_intro_delay = 4;
            intro_run_scene();
        }
        scene(0xb, -1, 1, 0x96, 0);
    }

    if (g_intro_stream1)
        far_free_ptr(DS_ADDR(ptr32, 0x160e));
    if (g_intro_stream2)
        far_free_ptr(DS_ADDR(ptr32, 0x1612));
    if (g_intro_stream3)
        far_free_ptr(DS_ADDR(ptr32, 0x1616));
    if (g_intro_bg)
        far_free_ptr(DS_ADDR(ptr32, 0x160a));
    far_free_ptr(DS_ADDR(ptr32, 0x9036));

    /* schwarz ausblenden */
    DS_SET_PTR(0x9036, bitmap_alloc(0x140, 200));
    bmp_fill(g_intro_canvas, 0, 0, 0x28, 200, 0);
    screen_wipe_in(g_intro_canvas, 0, 0, 0x140, 200, 1, 1);
    far_free_ptr(DS_ADDR(ptr32, 0x9036));
}
