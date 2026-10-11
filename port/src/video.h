/* Ersatz für VGA-Modus 13h: 320x200-Framebuffer mit 256-Farben-Palette,
 * dargestellt über SDL2 im Seitenverhältnis 4:3 (wie auf einem CRT). */
#ifndef MADTV_VIDEO_H
#define MADTV_VIDEO_H

#include <stdint.h>

enum { SCREEN_W = 320, SCREEN_H = 200 };

/* Entspricht dem Bildschirmspeicher bei A000:0000. */
extern uint8_t g_screen[SCREEN_W * SCREEN_H];

int video_init(const char *title, int scale);
void video_shutdown(void);
void video_set_palette(const uint8_t rgb[768]);
void video_set_title(const char *title);
void video_toggle_fullscreen(void);
/* Logische Ausgabegröße, in der SDL Mausbewegungen meldet (320x200 werden darauf gestreckt). */
void video_logical_size(int *w, int *h);
/* Framebuffer anzeigen (ersetzt das Warten auf den vertikalen Strahlrücklauf). */
void video_present(void);
/* aktuelles Bild mit Palette als 8-Bit-BMP speichern (Testhilfe) */
int video_save_bmp(const char *path);

/* Sprite mit Farbschlüssel 0 an Pixelposition zeichnen, abgeschnitten am Rand. */
void video_blit_trans(const uint8_t *px, int w, int h, int x, int y);
void video_clear(uint8_t color);

#endif
