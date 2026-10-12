#include "dgroup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t g_ds[DGROUP_SIZE];

/* Lage von DGROUP im Ladeabbild von MTV.EXE (Ladesegment 0, DGROUP-Segment 35d4h). */
enum { DGROUP_PARA = 0x35d4 };

int32_t ds_offset_of(const void *p)
{
    const uint8_t *b = p;
    if (b < g_ds || b >= g_ds + DGROUP_SIZE)
        return -1;
    return (int32_t)(b - g_ds);
}

int dgroup_load(const char *game_dir)
{
    char path[600];
    snprintf(path, sizeof path, "%s/MTV.EXE", game_dir);
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;

    uint8_t hdr[28];
    if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr || hdr[0] != 'M' || hdr[1] != 'Z') {
        fclose(f);
        return -1;
    }
    long header_bytes = (long)(hdr[8] | (hdr[9] << 8)) * 16;
    long last_page = hdr[2] | (hdr[3] << 8), pages = hdr[4] | (hdr[5] << 8);
    long image_end = (last_page ? (pages - 1) * 512 + last_page : pages * 512);
    long start = header_bytes + (long)DGROUP_PARA * 16;
    long len = image_end - start;
    if (len <= 0 || len > DGROUP_SIZE) {
        fclose(f);
        return -1;
    }

    static uint8_t img[DGROUP_SIZE];
    fseek(f, start, SEEK_SET);
    size_t got = fread(img, 1, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len)
        return -1;
    return dgroup_load_mem(img, (size_t)len);
}

int dgroup_load_mem(const uint8_t *data, size_t len)
{
    if (len < 0x2000 || len > DGROUP_SIZE)
        return -1;
    memset(g_ds, 0, sizeof g_ds); /* BSS */
    memcpy(g_ds, data, len);

    /* Prüfung: Turbo-C-Copyright am Anfang von DGROUP */
    if (memcmp(g_ds + 4, "Turbo-C", 7) != 0)
        return -1;

    /* Die einzigen relozierten Far-Pointer in DGROUP (Exit-Hooks der Laufzeitbibliothek,
     * DS:1820..182b) zeigen in Originalcode und werden nicht gebraucht. */
    memset(g_ds + 0x1820, 0, 12);
    return 0;
}
