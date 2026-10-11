/* HES-Viewer: zeigt die Sprite-Bänke von MadTV nativ mit SDL2 an.
 *
 *   hesview [Gamefiles-Ordner] [Startnummer]
 *   hesview --shot <nr> <datei.bmp> [Gamefiles-Ordner]   (ohne Fenster, für Tests)
 *
 * Tasten: Links/Rechts = vorherige/nächste Sprite-Bank, Hoch/Runter = Palette 1/299,
 *         Bild auf/ab = blättern, F11 = Vollbild, Esc = Ende. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hes.h"
#include "video.h"

static const int PALETTES[] = {1, 299};

/* Sprites der Bank zeilenweise auf den Bildschirm legen; ab HesSprite 'first'.
 * Rückgabe: Index des ersten Sprites, das nicht mehr passte. */
static int layout(const HesSpriteSet *b, int first)
{
    video_clear(0);
    int x = 0, y = 0, row_h = 0, i;
    for (i = first; i < b->count; i++) {
        const HesSprite *s = &b->sprites[i];
        if (x > 0 && x + s->w > SCREEN_W) {
            x = 0;
            y += row_h + 2;
            row_h = 0;
        }
        if (y > 0 && y + s->h > SCREEN_H)
            break;
        video_blit_trans(s->pixels, s->w, s->h, x, y);
        x += s->w + 2;
        if (s->h > row_h)
            row_h = s->h;
    }
    return i;
}

static int load_bank(int no, HesSpriteSet *bank)
{
    size_t size;
    uint8_t *d = hes_file_load(no, &size);
    if (!d)
        return -1;
    int rc = hes_decode_sprites(d, size, bank);
    free(d);
    return rc;
}

static int next_bank(int no, int dir)
{
    for (int i = 0; i < 400; i++) {
        no = (no + dir + 400) % 400;
        size_t size;
        uint8_t *d = hes_file_load(no, &size);
        if (!d)
            continue;
        int ok = hes_is_sprite_bank(d, size);
        free(d);
        if (ok)
            return no;
    }
    return no;
}

static int shot(int no, const char *out)
{
    uint8_t pal[768];
    HesSpriteSet bank;
    if (hes_load_palette(1, pal) != 0 || load_bank(no, &bank) != 0) {
        fprintf(stderr, "Datei %03d.HES nicht lesbar oder keine Sprite-Bank\n", no);
        return 1;
    }
    layout(&bank, 0);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom(g_screen, SCREEN_W, SCREEN_H, 8, SCREEN_W,
                                                        SDL_PIXELFORMAT_INDEX8);
    SDL_Color colors[256];
    for (int i = 0; i < 256; i++)
        colors[i] = (SDL_Color){pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255};
    SDL_SetPaletteColors(s->format->palette, colors, 0, 256);
    int rc = SDL_SaveBMP(s, out);
    SDL_FreeSurface(s);
    printf("%03d.HES: %d Sprites -> %s\n", no, bank.count, out);
    hes_free_sprites(&bank);
    return rc != 0;
}

int main(int argc, char **argv)
{
    if (argc >= 4 && strcmp(argv[1], "--shot") == 0) {
        if (argc >= 5)
            hes_set_dir(argv[4]);
        return shot(atoi(argv[2]), argv[3]);
    }
    if (argc >= 2)
        hes_set_dir(argv[1]);
    int no = next_bank(argc >= 3 ? atoi(argv[2]) - 1 : 0, +1);

    if (SDL_Init(0) != 0 || video_init("MadTV HES-Viewer", 3) != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MadTV", SDL_GetError(), NULL);
        return 1;
    }

    int pal_idx = 0, first = 0, next_first = 0, dirty = 1, running = 1;
    HesSpriteSet bank = {0};
    uint8_t pal[768];
    load_bank(no, &bank);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = 0;
            if (e.type != SDL_KEYDOWN)
                continue;
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE: running = 0; break;
            case SDLK_RIGHT:
            case SDLK_LEFT:
                hes_free_sprites(&bank);
                no = next_bank(no, e.key.keysym.sym == SDLK_RIGHT ? +1 : -1);
                load_bank(no, &bank);
                first = 0;
                dirty = 1;
                break;
            case SDLK_UP:
            case SDLK_DOWN: pal_idx ^= 1; dirty = 1; break;
            case SDLK_PAGEDOWN:
                if (next_first < bank.count) first = next_first;
                dirty = 1;
                break;
            case SDLK_PAGEUP: first = 0; dirty = 1; break;
            case SDLK_F11: video_toggle_fullscreen(); break;
            default: break;
            }
        }
        if (dirty) {
            if (hes_load_palette(PALETTES[pal_idx], pal) == 0)
                video_set_palette(pal);
            next_first = layout(&bank, first);
            char title[160];
            snprintf(title, sizeof title, "MadTV HES-Viewer - %03d.HES - Sprites %d-%d von %d - Palette %03d",
                     no, first + 1, next_first, bank.count, PALETTES[pal_idx]);
            video_set_title(title);
            dirty = 0;
        }
        video_present();
    }

    hes_free_sprites(&bank);
    video_shutdown();
    SDL_Quit();
    return 0;
}
