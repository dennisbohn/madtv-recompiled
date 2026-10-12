/* Programmstart des MadTV-Ports.
 *
 * Ersetzt den Turbo-C-Startcode (MTV.EXE 1000:0000): Spielordner suchen, Datensegment aus
 * MTV.EXE laden, Fenster öffnen und die übersetzte Original-main (1054:0005) aufrufen.
 *
 *   madtv.exe [-window] [Gamefiles-Ordner]
 * Startet im Vollbild; -window (oder MADTV_WINDOWED=1) im Fenster. Alt+Enter/F11 schaltet um.
 * Ohne Angabe wird "Gamefiles" neben der EXE bzw. im aktuellen Verzeichnis gesucht. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgroup.h"
#include "game/game.h"
#include "hes.h"
#include "platform.h"

#ifdef _WIN32
#include <windows.h>

/* Bei einem Absturz Fehleradresse und Rücksprungkette (über EBP) nach stderr schreiben; mit
 * "addr2line -e madtv.exe ADRESSE" lassen sie sich Quelltextzeilen zuordnen. */
static LONG WINAPI crash_handler(EXCEPTION_POINTERS *ep)
{
    fprintf(stderr, "CRASH code=%08lx at %p\n", (unsigned long)ep->ExceptionRecord->ExceptionCode,
            ep->ExceptionRecord->ExceptionAddress);
    fprintf(stderr, "  base %p\n", (void *)GetModuleHandleA(NULL));
    void **fp = (void **)ep->ContextRecord->Ebp;
    for (int i = 0; i < 24 && fp && !IsBadReadPtr(fp, 8); i++) {
        fprintf(stderr, "  frame %p\n", fp[1]);
        fp = (void **)fp[0];
    }
    fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}

/* Testhilfe MADTV_STACKDUMP=ms,...: zu diesen Zeitpunkten (ab Programmstart) den Hauptthread kurz
 * anhalten und seine Rücksprungkette ausgeben – findet Endlosschleifen ohne Debugger. */
static HANDLE g_main_thread;

static DWORD WINAPI stackdump_thread(LPVOID arg)
{
    const char *p = arg;
    DWORD start = GetTickCount(), at;
    while (*p) {
        at = strtoul(p, (char **)&p, 10);
        while (*p == ',')
            p++;
        while (GetTickCount() - start < at)
            Sleep(20);
        CONTEXT ctx = {.ContextFlags = CONTEXT_CONTROL};
        SuspendThread(g_main_thread);
        if (GetThreadContext(g_main_thread, &ctx)) {
            fprintf(stderr, "STACK at %lu ms: eip %p base %p\n", (unsigned long)at, (void *)ctx.Eip,
                    (void *)GetModuleHandleA(NULL));
            void **fp = (void **)ctx.Ebp;
            for (int i = 0; i < 24 && fp && !IsBadReadPtr(fp, 8); i++) {
                fprintf(stderr, "  frame %p\n", fp[1]);
                fp = (void **)fp[0];
            }
            fflush(stderr);
        }
        ResumeThread(g_main_thread);
    }
    return 0;
}
#endif

static int has_game(const char *dir)
{
    char path[600];
    snprintf(path, sizeof path, "%s/MTV.EXE", dir);
    FILE *f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

static const char *find_game_dir(int argc, char **argv)
{
    static char buf[600];
    for (int i = 1; i < argc; i++)
        if (argv[i][0] != '-' && has_game(argv[i]))
            return argv[i];
    char *base = SDL_GetBasePath();
    if (base) {
        const char *cands[] = {"Gamefiles", "../Gamefiles", "../../Gamefiles", "."};
        for (size_t i = 0; i < sizeof cands / sizeof *cands; i++) {
            snprintf(buf, sizeof buf, "%s%s", base, cands[i]);
            if (has_game(buf)) {
                SDL_free(base);
                return buf;
            }
        }
        SDL_free(base);
    }
    if (has_game("Gamefiles"))
        return "Gamefiles";
    return NULL;
}

static void fail(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MadTV", msg, NULL);
    exit(1);
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    SetUnhandledExceptionFilter(crash_handler);
    if (getenv("MADTV_STACKDUMP")) {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0,
                        FALSE, DUPLICATE_SAME_ACCESS);
        CloseHandle(CreateThread(NULL, 0, stackdump_thread, getenv("MADTV_STACKDUMP"), 0, NULL));
    }
#endif
    /* Gebündelte Fassung: Datensegment und Spieldaten-Archive stecken im Programm (Ressourcen
     * DGROUP, SOURCE1, SOURCE2); ein Spieldaten-Ordner ist dann nicht nötig. */
    size_t dg_len = 0;
    const uint8_t *dg = platform_resource("DGROUP", &dg_len);
    const char *dir = find_game_dir(argc, argv);
    if (dg) {
        if (dgroup_load_mem(dg, dg_len) != 0)
            fail("Die eingebetteten Spieldaten sind beschädigt.");
        if (!dir)
            dir = ".";
    }
    if (!dir)
        fail("Die Spieldateien wurden nicht gefunden.\n\n"
             "Lege den Ordner \"Gamefiles\" (mit MTV.EXE und den .HES-Dateien) neben madtv.exe.");
    if (!dg && dgroup_load(dir) != 0)
        fail("MTV.EXE konnte nicht gelesen werden oder ist eine unbekannte Version.");
    hes_set_dir(dir);
    int windowed = getenv("MADTV_WINDOWED") != NULL;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "-window") == 0)
            windowed = 1;
    if (platform_init(dir, windowed) != 0)
        fail(SDL_GetError());

    /* Original: main(argc, argv, envp); Kommandozeilenparameter wertet MadTV nicht aus. */
    game_main();

    platform_shutdown();
    return 0;
}
