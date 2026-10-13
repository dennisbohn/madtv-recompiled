/* Deflate-Entpacker für die eingebetteten Spieldaten (third_party/tinf, unverändert eingebunden).
 * tinf vergleicht an zwei Stellen int mit unsigned; die Warnung wird nur hier abgeschaltet. */
#pragma GCC diagnostic ignored "-Wsign-compare"
#include "../third_party/tinf/tinflate.c"
