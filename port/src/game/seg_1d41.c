/* Segment 1d41 – Textformatierung und kleine Speicher-/String-Helfer.
 *
 * fmt() ist das sprintf des Spiels. Platzhalter beginnen mit '#' statt '%':
 *   #[Füllung][Breite]Typ
 *   Füllung: '_' = mit '{' auffüllen, '+c' = mit Zeichen c, '0' = mit '0'
 *            (ohne Angabe wird mit NUL-Bytes aufgefüllt – wie im Original)
 *   Breite:  Zeichenzahl, oder '*n' = so viele Zeichen, wie in n 8-Pixel-Spalten passen
 *   Typ:     d int, l long, s String, v Zeichen (Wert - 64), f Balkengrafik, z Byte
 * Ein unbekannter Typ erzeugt den Text "Fuck" (Entwickler-Platzhalter des Originals). */
#include <stdarg.h>
#include <string.h>

#include "game.h"

#define g_glyph_width DS_ADDR(uint8_t, 0x022e) /* Pixelbreite je Zeichen (0 = 1 Pixel) */

/* 1d41:0003 – formatiert in einen neuen 1000-Byte-Puffer (Aufrufer gibt ihn frei) */
char *fmt(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char *result = far_calloc(1000, 1);
    const char *p = format;
    char *dst = result;
    const char *end = format + strlen(format);

    do {
        int16_t width = 0;
        char *pad = far_calloc(2, 1);
        int16_t pad_set = 0;

        if (*p != '#') {
            *dst++ = *p++;
            goto next;
        }
        p++;
        if (*p == '_') {
            pad_set = 1;
            p++;
            pad[0] = '{';
        } else if (*p == '+') {
            pad_set = 1;
            p++;
            pad[0] = *p;
            p++;
        } else if (*p == '0') {
            pad[0] = '0';
            p++;
        }

        int16_t digits = fmt_count_digits(p);
        if (digits > 0) {
            width = tc_atoi(p);
            p += digits;
        } else if (*p == '*') {
            p++;
            digits = fmt_count_digits(p);
            width = tc_atoi(p);
            p += digits;
            width = (int16_t)(width + 1000); /* Breite in Spalten, siehe text_fit_width */
        }

        switch (*p) {
        case 'd': {
            int16_t v = (int16_t)va_arg(ap, int);
            fmt_int(dst, &v, (uint16_t *)&width, pad);
            p++;
            dst += width;
            break;
        }
        case 'z': {
            /* UNKLAR: Das Original liest hier nur 1 Byte von der Argumentliste (Stack-Zeiger +1).
             * Der Typ wird von keinem Text des Spiels benutzt; hier als int-Argument behandelt. */
            int16_t v = (int8_t)va_arg(ap, int);
            fmt_int(dst, &v, (uint16_t *)&width, pad);
            p++;
            dst += width;
            break;
        }
        case 'l': {
            int32_t v = va_arg(ap, int32_t);
            fmt_long(dst, &v, (uint16_t *)&width, pad);
            p++;
            dst += width;
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == NULL) {
                p++;
                if (pad_set == 1) {
                    str_fill(dst, pad, width);
                    dst += width;
                }
                break;
            }
            if (width > 1000)
                width = text_fit_width(s, (int16_t)(width - 1000));
            uint16_t len = (uint16_t)strlen(s);
            if ((width != 0 && len > (uint16_t)width) || pad_set == 1) {
                if (len < (uint16_t)width) {
                    memcpy(dst, s, len);
                    str_fill(dst + len, pad, (int16_t)(width - len));
                } else {
                    memcpy(dst, s, (uint16_t)width);
                }
                dst += width;
            } else {
                memcpy(dst, s, len);
                dst += len;
            }
            p++;
            break;
        }
        case 'v':
            p++;
            *dst++ = (char)(va_arg(ap, int) - 0x40);
            break;
        case 'f': {
            /* Balkengrafik: Rahmenzeichen 1, dazwischen width-1 Zeichen mit Skalenstrichen
             * bei 1/4 und 1/2; die ersten v Felder gefüllt (2/4/6), der Rest leer (3/5/7). */
            p++;
            int16_t v = (int16_t)va_arg(ap, int);
            *dst++ = 1;
            for (int16_t i = 1; i < width; i++) {
                if (v == 0) {
                    if (i % (width >> 2) != 0)
                        *dst = 3;
                    else
                        *dst = (char)(i % (width >> 1) != 0 ? 5 : 7);
                } else {
                    if (i % (width >> 2) != 0)
                        *dst = 2;
                    else
                        *dst = (char)(i % (width >> 1) != 0 ? 4 : 6);
                    v--;
                }
                dst++;
            }
            *dst++ = 1;
            break;
        }
        default:
            /* Unbekannter Typ: Original schreibt "Fuck" und lässt das Zeichen stehen */
            memcpy(dst, DS_ADDR(char, 0x0d1a), 4);
            dst += 4;
            break;
        }
    next:
        far_free(pad);
    } while (p <= end); /* verarbeitet auch das abschließende NUL */

    /* ORIGINAL-BUG: kopiert 1 Byte von Adresse 0000:0000; das NUL steht bereits davor. */
    *dst = 0;
    va_end(ap);
    return result;
}

/* 1d41:049b – Anzahl führender Ziffern (mehr als 10 → -1) */
int16_t fmt_count_digits(const char *s)
{
    int16_t n = 0;
    while (s[n] >= '0' && s[n] <= '9' && n <= 10)
        n++;
    return n > 10 ? -1 : n;
}

/* 1d41:04c7 – Zahl rechtsbündig mit Füllzeichen; ohne Breite wird die Länge zurückgeschrieben */
void fmt_int(char *dst, int16_t *value, uint16_t *width, const char *pad)
{
    char *tmp = far_calloc(6, 1);
    uint16_t w = *width;
    if (*value < 0) {
        memcpy(dst, DS_ADDR(char, 0x0d1f), 1); /* "-" */
        *value = (int16_t)-*value;
        dst++;
        w--;
    }
    tc_itoa(*value, tmp, 10);
    if ((uint16_t)strlen(tmp) < w && w != 0) {
        for (int16_t i = (int16_t)(strlen(tmp) + 1); i <= (int16_t)w; i++)
            memcpy(dst++, pad, 1);
    } else {
        *width = (uint16_t)strlen(tmp);
    }
    memcpy(dst, tmp, strlen(tmp));
    far_free(tmp);
}

/* 1d41:05c2 – wie fmt_int für long */
void fmt_long(char *dst, int32_t *value, uint16_t *width, const char *pad)
{
    char *tmp = far_calloc(12, 1);
    uint16_t w = *width;
    if (*value < 0) {
        memcpy(dst, DS_ADDR(char, 0x0d21), 1); /* "-" */
        *value = -*value;
        dst++;
        w--;
    }
    tc_ltoa(*value, tmp, 10);
    if ((uint16_t)strlen(tmp) < w && w != 0) {
        for (int16_t i = (int16_t)(strlen(tmp) + 1); i <= (int16_t)w; i++)
            memcpy(dst++, pad, 1);
    } else {
        *width = (uint16_t)strlen(tmp);
    }
    memcpy(dst, tmp, strlen(tmp));
    far_free(tmp);
}

/* 1d41:06d7 – Ziffernfolge in long umwandeln (ungenutzt) */
int32_t str_to_long(const char *s)
{
    int32_t v = 0;
    for (int16_t i = 0; s[i] >= '0' && s[i] <= '9'; i++)
        v = v * 10 + s[i] - '0';
    return v;
}

/* 1d41:072c – die ersten n Zeichen von dst durch src ersetzen und suffix anhängen (ungenutzt) */
void str_overwrite_prefix(char *dst, const char *src, int16_t n, const char *suffix)
{
    /* UNKLAR: far_calloc wird im Original mit nur einem Argument aufgerufen (Elementgröße undefiniert) */
    char *tmp = far_calloc((uint16_t)(strlen(suffix) + n + 1), 1);
    strcpy(tmp, dst);
    strncpy(tmp, src, (uint16_t)n); /* echtes strncpy: füllt bei kurzem src mit NUL auf */
    if (*suffix == '0' && strlen(suffix) == 1)
        memcpy(tmp + n, DS_ADDR(char, 0x0d23), 1);
    else
        memcpy(tmp + n, suffix, strlen(suffix));
    strcpy(dst, tmp);
    far_free(tmp);
}

/* 1d41:07ff – n Bytes nullen */
void far_memset(void *dst, uint16_t n)
{
    memset(dst, 0, n);
}

/* 1d41:081a – n Bytes kopieren (vorwärts, byteweise) */
void far_memcpy(void *dst, const void *src, uint16_t n)
{
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
}

/* 1d41:083d */
int16_t str_length(const char *s)
{
    return (int16_t)strlen(s);
}

/* 1d41:0857 – n-mal das Zeichen *ch schreiben */
void str_fill(char *dst, const char *ch, int16_t n)
{
    for (int16_t i = 0; i < n; i++)
        dst[i] = *ch;
}

static int16_t glyph_advance(uint8_t c)
{
    if (c == ' ')
        return 3 + 1;
    return (int16_t)((g_glyph_width[c] == 0 ? 1 : g_glyph_width[c]) + 1);
}

/* 1d41:0874 – wie viele Zeichen passen in width_cols 8-Pixel-Spalten */
int16_t text_fit_width(const char *s, int16_t width_cols)
{
    int16_t px = 0, n = 0;
    while ((px >> 3) < width_cols) {
        if (s[n] == '\0')
            break;
        px = (int16_t)(px + glyph_advance((uint8_t)s[n]));
        n++;
    }
    return n;
}

/* 1d41:08d7 – Breite eines Textes in Pixeln */
int16_t text_width(const char *s)
{
    int16_t px = 0;
    for (int16_t i = 0; s[i] != '\0'; i++)
        px = (int16_t)(px + glyph_advance((uint8_t)s[i]));
    return px;
}

/* 1d41:0927 – -1, wenn prefix ein Präfix von s ist (ungenutzt) */
int16_t str_is_prefix(const char *prefix, const char *s)
{
    /* ORIGINAL-BUG: ohne Endprüfung läuft der Vergleich bei gleichen Strings über das NUL hinaus */
    while (*s == *prefix) {
        s++;
        prefix++;
    }
    return *prefix == '\0' ? -1 : 0;
}
