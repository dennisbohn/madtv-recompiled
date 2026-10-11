#include "stub.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>

void stub_hit(const char *name, const char *addr)
{
    static int logged[4096];
    unsigned h = 0;
    for (const char *p = name; *p; p++)
        h = h * 31 + (unsigned char)*p;
    if (!logged[h % 4096]) {
        logged[h % 4096] = 1;
        fprintf(stderr, "Noch nicht übersetzt: %s (%s)\n", name, addr);
        fflush(stderr);
    }
    const char *cont = getenv("MADTV_STUB_CONTINUE");
    if (cont && *cont == '1')
        return;
    const char *quiet = getenv("MADTV_STUB_QUIET");
    if (quiet && *quiet == '1')
        exit(2);
    char msg[256];
    snprintf(msg, sizeof msg, "Diese Funktion ist noch nicht übersetzt:\n\n%s (MTV.EXE %s)", name, addr);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "MadTV-Port", msg, NULL);
    exit(2);
}
