#include "dgroup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t g_ds[DGROUP_SIZE];

void ptr32_fail(uintptr_t addr)
{
    fprintf(stderr, "Interner Fehler: Adresse %p passt nicht in einen 4-Byte-Zeiger\n", (void *)addr);
    abort();
}

int32_t ds_offset_of(const void *p)
{
    const uint8_t *b = p;
    if (b < g_ds || b >= g_ds + DGROUP_SIZE)
        return -1;
    return (int32_t)(b - g_ds);
}

int dgroup_load(const char *dir)
{
    char path[600];
    snprintf(path, sizeof path, "%s/DGROUP.bin", dir);
    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    static uint8_t img[DGROUP_SIZE];
    size_t got = fread(img, 1, sizeof img, f);
    fclose(f);
    return dgroup_load_mem(img, got);
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
