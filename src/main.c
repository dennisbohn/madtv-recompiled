/* Programmstart des MadTV-Ports.
 *
 * Ersetzt den Turbo-C-Startcode (MTV.EXE 1000:0000): eingebettetes Datensegment laden, Fenster
 * öffnen und die übersetzte Original-main (1054:0005) aufrufen. Alle Spieldaten stecken im
 * Programm (tools/make_bundle.py).
 *
 *   madtv.exe [-window]
 * Startet beim ersten Mal im Fenster, danach im zuletzt gewählten Modus; -window (oder
 * MADTV_WINDOWED=1) erzwingt das Fenster. Alt+Enter/F11 oder das Menü schaltet um. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dgroup.h"
#include "game/game.h"
#include "platform.h"
#if defined(__linux__) && UINTPTR_MAX > 0xffffffffu
#include <malloc.h>
#endif

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

static void fail(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MadTV", msg, NULL);
    exit(1);
}

#if defined(__linux__) && UINTPTR_MAX > 0xffffffffu
/* PORT: Unter 64 Bit müssen alle Adressen, die das Spiel als 4-Byte-Far-Pointer ablegt, unter 4 GB
 * liegen (src/dgroup.h). Code und statische Daten liegen dort, weil das Programm ohne PIE gelinkt
 * ist. Den Heap legt glibc direkt hinter das Programm (brk), große Blöcke und die Heaps weiterer
 * Threads aber per mmap weit oben ab; beides wird hier abgeschaltet, bevor etwas reserviert wird. */
static void memory_init(void)
{
    mallopt(M_MMAP_MAX, 0);
    mallopt(M_ARENA_MAX, 1);
}
#endif

int main(int argc, char **argv)
{
#if defined(__linux__) && UINTPTR_MAX > 0xffffffffu
    memory_init();
#endif
#ifdef _WIN32
    SetUnhandledExceptionFilter(crash_handler);
    if (getenv("MADTV_STACKDUMP")) {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_main_thread, 0,
                        FALSE, DUPLICATE_SAME_ACCESS);
        CloseHandle(CreateThread(NULL, 0, stackdump_thread, getenv("MADTV_STACKDUMP"), 0, NULL));
    }
#endif
    size_t dg_len = 0;
    const uint8_t *dg = platform_resource("DGROUP", &dg_len);
    if (!dg || dgroup_load_mem(dg, dg_len) != 0)
        fail("Die eingebetteten Spieldaten fehlen oder sind beschädigt.");
    int windowed = getenv("MADTV_WINDOWED") != NULL;
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], "-window") == 0)
            windowed = 1;
    if (platform_init(windowed) != 0)
        fail(SDL_GetError());

    /* Original: main(argc, argv, envp); Kommandozeilenparameter wertet MadTV nicht aus. */
    game_main();

    platform_shutdown();
    return 0;
}
