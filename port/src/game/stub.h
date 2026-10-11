/* Meldung für noch nicht übersetzte Funktionen (siehe stubs.c). Standard: Abbruch mit
 * Hinweisfenster. Mit Umgebungsvariable MADTV_STUB_CONTINUE=1 wird nur protokolliert,
 * mit MADTV_STUB_QUIET=1 ohne Fenster beendet (für automatische Testläufe). */
#ifndef MADTV_STUB_H
#define MADTV_STUB_H

void stub_hit(const char *name, const char *addr);

#define STUB(name, addr) stub_hit(name, addr)

#endif
