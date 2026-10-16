/* Prüft src/game/ds_init.c gegen das Datensegment aus MTV.EXE: dscheck DGROUP.bin
 * (DGROUP.bin erzeugt re/tools/dgroup_extract.py aus gamedata/MTV.EXE). Bis auf die Bereiche der
 * DOS-Laufzeitbibliothek, die der Port nicht übernimmt, muss alles übereinstimmen. */
#include "dgroup.h"

#include <stdio.h>
#include <string.h>

/* nicht übernommen (siehe ds_init.c) */
static const struct {
    int from, to;
} k_skip[] = {
    {0x0004, 0x005c}, {0x17b0, 0x17c7}, {0x17c7, 0x1820}, {0x1820, 0x183f},
    {0x183f, 0x18c0}, {0x1944, 0x194a}, {0xa7e0, 0xa842},
};

static int skipped(int o)
{
    for (size_t i = 0; i < sizeof k_skip / sizeof *k_skip; i++)
        if (o >= k_skip[i].from && o < k_skip[i].to)
            return 1;
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Aufruf: dscheck DGROUP.bin\n");
        return 2;
    }
    static uint8_t ref[DGROUP_SIZE];
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    size_t n = fread(ref, 1, sizeof ref, f);
    fclose(f);
    dgroup_init();
    int bad = 0;
    for (size_t o = 0; o < DGROUP_SIZE; o++) {
        uint8_t want = o < n ? ref[o] : 0;
        if (g_ds[o] != want && !skipped((int)o)) {
            if (bad < 40)
                printf("DS:%04zx Port %02x, MTV.EXE %02x\n", o, g_ds[o], want);
            bad++;
        }
    }
    printf("%s: %d abweichende Byte\n", bad ? "FEHLER" : "OK", bad);
    return bad != 0;
}
