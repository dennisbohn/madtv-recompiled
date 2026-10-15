/* Tests für den Textformatierer fmt() (Segment 1d41) gegen von Hand nachvollzogene Ergebnisse. */
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "platform.h"

static int fails;

static void expect(const char *got, const char *want, size_t n, const char *what)
{
    if (memcmp(got, want, n) != 0) {
        printf("FEHLER %s: \"", what);
        for (size_t i = 0; i < n; i++)
            printf(got[i] >= 32 ? "%c" : "\\%d", got[i]);
        printf("\"\n");
        fails++;
    }
}

int main(int argc, char **argv)
{
    const char *dir = argc >= 2 ? argv[1] : "build/data";
    if (dgroup_load(dir) != 0) {
        printf("DGROUP.bin nicht gefunden\n");
        return 1;
    }
    char *s;

    s = fmt("#02d:#02d", 7, 5);
    expect(s, "07:05", 6, "Uhrzeit");
    tc_free(s);

    /* ORIGINAL-BUG: bei negativen Zahlen ohne Breite wird die Breite intern 0xFFFF, die
     * Ausgabeposition rückt nicht weiter und der folgende Text überschreibt die Zahl. */
    s = fmt("Geld: #l $", (int32_t)-1234567);
    expect(s, "Geld:  $", 9, "long negativ");
    tc_free(s);

    s = fmt("[#+.6s]", "abc");
    expect(s, "[abc...]", 9, "String mit Füllzeichen");
    tc_free(s);

    s = fmt("[#3s]", "abcdef");
    expect(s, "[abc]", 6, "String gekürzt");
    tc_free(s);

    s = fmt("[#_4d]", 42);
    expect(s, "[{{42]", 7, "Füllung mit {");
    tc_free(s);

    s = fmt("[#4d]", 42);
    expect(s, "[\0\0" "42]", 7, "Füllung mit NUL (Original-Verhalten)");
    tc_free(s);

    s = fmt("#v", 'A' + 0x40);
    expect(s, "A", 2, "Zeichen");
    tc_free(s);

    s = fmt("#8f", 3);
    expect(s, "\1\2\4\2\7\3\5\3\1", 10, "Balken");
    tc_free(s);

    s = fmt("#q");
    expect(s, "Fuckq", 6, "unbekannter Typ");
    tc_free(s);

    printf("%s: %d Fehler\n", fails ? "FEHLGESCHLAGEN" : "OK", fails);
    return fails != 0;
}
