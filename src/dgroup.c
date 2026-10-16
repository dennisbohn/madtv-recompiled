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

void dgroup_init(void)
{
    memset(g_ds, 0, sizeof g_ds); /* Variablen ohne Startwert (BSS) */
    ds_init_data();
}
