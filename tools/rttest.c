/* Integrationstest der Laufzeitbibliothek (Original-API) ohne Fenster:
 *   rttest [Datenordner, Standard build/data] [ausgabe.bmp]
 * Füllt das Datensegment (src/game/ds_init.c), lädt die Archive aus dem Datenordner
 * (tools/make_bundle.py), prüft
 * Tabellen, entpackt eine Sprite-Bank im Originallayout,
 * blittet sie auf den Bildschirm und speichert das Ergebnis als BMP. */
#include <SDL.h>
#include <stdio.h>
#include <string.h>

#include "dgroup.h"
#include "hes.h"
#include "platform.h"
#include "rtlib.h"
#include "video.h"

static int fails;
#define CHECK(c)                                                    \
    do {                                                            \
        if (!(c)) {                                                 \
            printf("FEHLER %s:%d: %s\n", __FILE__, __LINE__, #c);   \
            fails++;                                                \
        }                                                           \
    } while (0)

int main(int argc, char **argv)
{
    const char *dir = argc >= 2 ? argv[1] : "build/data";
    const char *out = argc >= 3 ? argv[2] : "rttest.bmp";
    hes_set_dir(dir);
    platform_set_game_dir(dir);

    dgroup_init();
    CHECK(strcmp((char *)DS_ADDR(char, 0x1398), "Fehler aufgetreten!") == 0);
    CHECK(DS_U16(0x1690) == 0xff); /* LZW-Maskentabelle */

    /* Turbo-C-rand: bekannte Folge für Startwert 1 */
    tc_srand(1);
    CHECK(tc_rand() == 346);
    CHECK(tc_rand() == 130);

    /* Sprite-Bank im Originallayout */
    int16_t fsize;
    int16_t usize;
    hes_sprite_info(172, &fsize, &usize, 0);
    CHECK(fsize == hes_filesize(172) && fsize > 0);
    uint8_t *raw = tc_malloc((uint16_t)fsize);
    int32_t loaded = hes_load(172, raw);
    if (loaded != fsize)
        printf("hes_load=%ld fsize=%ld\n", (long)loaded, (long)fsize);
    CHECK(loaded == fsize);
    SpriteBank *bank = tc_calloc((uint16_t)usize, 1);
    hes_unpack_sprites(raw, bank);
    CHECK(bank->count == 25);
    CHECK(spritebank_size(bank) == (uint16_t)usize);

    /* Palette über die Original-API setzen (Datei = u16 Anzahl + RGB) */
    uint8_t *pal = tc_malloc(800);
    CHECK(hes_load(1, pal) == 770);
    vga_set_palette(pal);

    video_clear(0);
    for (int i = 0; i < bank->count; i++)
        scr_blit(&bank->bmp[i], (int16_t)((i % 9) * 4), (int16_t)((i / 9) * 36));

    /* Füllen + Rechteck-Blit in eine Offscreen-Bitmap und zurück */
    Bitmap *off = bitmap_alloc(320, 40);
    bmp_fill(off, 0, 0, 40, 40, 15);
    BlitRectArgs ra = {P32(&bank->bmp[0]), 0, 0, bank->bmp[0].wcols, bank->bmp[0].h, P32(off), 10, 4};
    bmp_blit_rect(&ra);
    scr_blit(off, 0, 150);

    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(g_screen, SCREEN_W, SCREEN_H, 8, SCREEN_W,
                                                        SDL_PIXELFORMAT_INDEX8);
    SDL_Color colors[256];
    for (int i = 0; i < 256; i++)
        colors[i] = (SDL_Color){(uint8_t)(pal[2 + i * 3] * 255 / 63), (uint8_t)(pal[3 + i * 3] * 255 / 63),
                                (uint8_t)(pal[4 + i * 3] * 255 / 63), 255};
    SDL_SetPaletteColors(s->format->palette, colors, 0, 256);
    SDL_SaveBMP(s, out);
    SDL_FreeSurface(s);

    printf("%s: %d Fehler\n", fails ? "FEHLGESCHLAGEN" : "OK", fails);
    return fails != 0;
}
