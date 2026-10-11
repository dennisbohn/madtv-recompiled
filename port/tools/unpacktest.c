/* Selbsttest: entpackt alle HES-Dateien und prüft die Ergebnisse gegen die Header.
 *   unpacktest [Gamefiles-Ordner]
 * Ausgabe je Datei: Nummer, Typ, Ergebnisgröße, FNV-1a-Prüfsumme (zum Vergleich mit re/tools). */
#include <stdio.h>
#include <stdlib.h>

#include "hes.h"
#include "unpack.h"

static uint32_t fnv1a(const uint8_t *p, size_t n)
{
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}

int main(int argc, char **argv)
{
    if (argc >= 2)
        hes_set_dir(argv[1]);
    int sprites = 0, lzw = 0, errors = 0;
    for (int no = 0; no < 400; no++) {
        size_t size;
        uint8_t *d = hes_file_load(no, &size);
        if (!d)
            continue;
        if (hes_is_sprite_bank(d, size)) {
            HesSpriteSet b;
            if (hes_decode_sprites(d, size, &b) != 0) {
                printf("%03d sprites FEHLER\n", no);
                errors++;
            } else {
                size_t total = 0;
                for (int i = 0; i < b.count; i++)
                    total += (size_t)b.sprites[i].w * b.sprites[i].h;
                printf("%03d sprites %zu %08x\n", no, total, fnv1a(b.storage, total));
                hes_free_sprites(&b);
                sprites++;
            }
        } else if (size > 8 && (size_t)(d[2] | (d[3] << 8)) + 8 >= size - 2 &&
                   (size_t)(d[2] | (d[3] << 8)) + 8 <= size + 2) {
            size_t out_size = 0;
            uint8_t *out = lzw_unpack(d, size, &out_size);
            size_t expect = (size_t)(d[0] | (d[1] << 8));
            if (!out || out_size != expect) {
                printf("%03d lzw FEHLER (%zu statt %zu)\n", no, out_size, expect);
                errors++;
            } else {
                printf("%03d lzw %zu %08x\n", no, out_size, fnv1a(out, out_size));
                lzw++;
            }
            free(out);
        }
        free(d);
    }
    printf("Sprite-Bänke: %d, LZW: %d, Fehler: %d\n", sprites, lzw, errors);
    return errors != 0;
}
