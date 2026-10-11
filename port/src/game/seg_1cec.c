/* Segment 1cec – Textausgabe mit Zeilenumbruch.
 *
 * text_box() ist der eigentliche Renderer; die übrigen Funktionen sind Hüllen dafür mit
 * Standardziel (Spielszene DS:8e62), Ausrichtung oder anschließendem Freigeben des Texts
 * (*_free, typisch für Ergebnisse von fmt()). */
#include <string.h>

#include "game.h"

#define g_upper_screen DS_PTR(Bitmap, 0x8e62)
#define g_font_select  DS_I16(0x8b86) /* 0 = Standardschrift (8 px Zeilen), 1 = kleine (7 px) */
#define UNLIMITED      ((int16_t)-1)

/* 1cec:000b */
void text_draw(int16_t x, int16_t y, const char *str)
{
    text_box(x, y, str, 0, 0, g_upper_screen, UNLIMITED);
}

/* 1cec:0033 – rechtsbündig an x */
void text_draw_right_to_free(int16_t x, int16_t y, char *str, Bitmap *dst)
{
    text_box((int16_t)(x - text_width(str)), y, str, 0, 0, dst, UNLIMITED);
    far_free(str);
}

/* 1cec:0076 */
void text_draw_free(int16_t x, int16_t y, char *str)
{
    text_box(x, y, str, 0, 0, g_upper_screen, UNLIMITED);
    far_free(str);
}

/* 1cec:00ab */
void text_draw_to(int16_t x, int16_t y, const char *str, Bitmap *dst)
{
    text_box(x, y, str, 0, 0, dst, UNLIMITED);
}

/* 1cec:00d1 */
void text_box_wrap(int16_t x, int16_t y, const char *str, int16_t width, uint16_t flags)
{
    text_box(x, y, str, width, flags, g_upper_screen, UNLIMITED);
}

/* 1cec:00fb */
void text_draw_to_free(int16_t x, int16_t y, char *str, Bitmap *dst)
{
    text_box(x, y, str, 0, 0, dst, UNLIMITED);
    far_free(str);
}

/* 1cec:012e – zentriert um x */
void text_draw_centered_to_free(int16_t x, int16_t y, char *str, Bitmap *dst)
{
    text_box((int16_t)(x - text_width(str) / 2), y, str, 0, 0, dst, UNLIMITED);
    far_free(str);
}

/* 1cec:0177 */
void text_draw_centered(int16_t x, int16_t y, const char *str)
{
    text_box((int16_t)(x - text_width(str) / 2), y, str, 0, 0, g_upper_screen, UNLIMITED);
}

/* 1cec:01b5 */
void text_draw_centered_to(int16_t x, int16_t y, const char *str, Bitmap *dst)
{
    text_box((int16_t)(x - text_width(str) / 2), y, str, 0, 0, dst, UNLIMITED);
}

/* 1cec:01f1 */
void text_box_centered(int16_t x, int16_t y, const char *str, int16_t width, uint16_t flags, Bitmap *dst,
                       int16_t max_lines)
{
    text_box((int16_t)(x - text_width(str) / 2), y, str, width, flags, dst, max_lines);
}

/* 1cec:022e */
void text_box_to_free(int16_t x, int16_t y, char *str, int16_t width, uint16_t flags, Bitmap *dst)
{
    text_box(x, y, str, width, flags, dst, UNLIMITED);
    far_free(str);
}

/* 1cec:0263 */
void text_box_free(int16_t x, int16_t y, char *str, int16_t width, uint16_t flags, Bitmap *dst, int16_t max_lines)
{
    text_box(x, y, str, width, flags, dst, max_lines);
    far_free(str);
}

/* 1cec:0297 */
void text_box_wrap_free(int16_t x, int16_t y, char *str, int16_t width, uint16_t flags)
{
    text_box(x, y, str, width, flags, g_upper_screen, UNLIMITED);
    far_free(str);
}

/* Länge der nächsten Zeile ab p bestimmen (gemeinsame Regel von text_box und text_line_count).
 * fit = Zeichen, die in die Breite passen; line = Kopie davon. *skip = 1, wenn das Trennzeichen
 * (Leerzeichen oder '~') übersprungen wird. */
static int16_t line_length(const char *p, const char *line, int16_t fit, uint16_t flags, int16_t *skip)
{
    int16_t i;
    *skip = 0;
    for (i = 0; i < fit; i++)
        if (line[i] == '~') {
            *skip = 1;
            return i;
        }
    if (fit < (int16_t)strlen(p)) {
        /* rückwärts nach Leerzeichen/Bindestrich (nur mit Flag 1) oder Textende suchen */
        int16_t j = fit, k;
        do {
            k = j;
            j = (int16_t)(k - 1);
            if (j < 1)
                return fit; /* kein Trennpunkt: hart umbrechen */
        } while (!(((line[j] == ' ' || line[j] == '-') && (flags & 1)) || line[j] == '\0'));
        if (line[j] != '-') {
            *skip = 1;
            return j; /* vor dem Leerzeichen trennen */
        }
        return k; /* Bindestrich bleibt in der Zeile */
    }
    return fit;
}

/* 1cec:02ce – Text umbrechen und zeichnen. width in 8-Pixel-Spalten (0 = 0x50); '~' erzwingt
 * einen Umbruch; flags Bit0 = an Leerzeichen/Bindestrich umbrechen, Bit1 = feste Ziffernbreite.
 * Die erste Zeile steht eine Zeilenhöhe unter y. */
void text_box(int16_t x, int16_t y, const char *str, int16_t width, uint16_t flags, Bitmap *dst, int16_t max_lines)
{
    if (str == NULL) /* PORT: das Original liest dann ab DS:0000 (leer) */
        return;
    const char *p = str;
    const char *end = str + strlen(str);
    if (width == 0)
        width = 0x50;
    for (;;) {
        if (end < p || p == NULL)
            return;
        int16_t fit = text_fit_width(p, width);
        char *line = far_calloc((uint16_t)(fit + 1), 1);
        strncpy(line, p, (size_t)fit);
        int16_t skip;
        int16_t len = line_length(p, line, fit, flags, &skip);
        line[len] = '\0';
        if (g_font_select == 0)
            y += 8;
        else if (g_font_select == 1)
            y += 7;
        draw_string(x, y, line, dst, (flags & 2) ? 1 : 0);
        far_free(line);
        if (p[len] == '\0')
            return;
        max_lines--;
        if (max_lines == 0)
            return;
        p += len + skip;
    }
}

/* 1cec:046d – Anzahl Zeilen, die text_box für str ergeben würde */
int16_t text_line_count(const char *str, int16_t width, uint16_t flags)
{
    if (str == NULL) /* PORT: wie text_box */
        return 0;
    int16_t lines = 0;
    int16_t total = (int16_t)strlen(str);
    int16_t done = 0;
    const char *p = str;
    for (;;) {
        if (*p == '\0')
            return lines;
        int16_t fit = text_fit_width(p, width);
        int16_t skip;
        /* Original prüft direkt im Quelltext statt in einer Kopie */
        int16_t len = line_length(p, p, fit, flags, &skip);
        lines++;
        if (p[len] == '\0')
            return lines;
        p += len + skip;
        done = (int16_t)(done + len + skip);
        if (total <= done)
            return lines;
    }
}
