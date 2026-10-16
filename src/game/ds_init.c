/* Startwerte des Datensegments.
 *
 * MTV.EXE bringt neben dem Code ein vorbelegtes Datensegment mit: Tabellen (Schrift, Klickbereiche,
 * Tastatur, Animationen, Preise, ...), Formatvorlagen für Texte und einige Variablen mit Startwert.
 * Der Spielcode liest sie über ihre Adresse im Datensegment (DS_*-Makros). Hier stehen sie als
 * eigene Tabellen; ds_init_data schreibt sie beim Start an diese Adressen. Alles übrige des
 * Segments beginnt mit 0.
 *
 * Nicht übernommen ist, was nur die DOS-Laufzeitbibliothek von Turbo C brauchte: Copyright und
 * Fehlertexte (DS:0004), Suchmuster "*.HES" (DS:17b0), DOS-Fehlercodes (DS:17c7), Far-Zeiger der
 * Exit-Hooks (DS:1820), Zeichenklassen (DS:183f), Heap-Grenzen (DS:1944) und ein Stück Maschinencode
 * am Segmentende (DS:a7e0). Werte stammen aus der Release-Fassung; Adressen sind die des Ports
 * (re/notes/release_changes.md). */
#include "dgroup.h"

#include <string.h>

/* Klickbereich (12 Byte, wie im Original hintereinander) */
typedef struct {
    int16_t x0, y0, x1, y1;
    int8_t room;   /* Raum */
    int8_t dialog; /* Dialog, -1 = in jedem */
    int8_t owner;  /* 1 = nur im eigenen Raum */
    int8_t id;     /* Kennung, die hotspot_at_mouse liefert */
} Hotspot;
_Static_assert(sizeof(Hotspot) == 12, "Hotspot muss 12 Byte groß sein");

/* DS:0094 – Thema des Tages der letzten Tage: Genre und Quotenbonus in Zehnteln (TOPIC_GENRE/TOPIC_BONUS) */
static const int16_t daily_topics[5][2] = {
    {0, 13},
    {0, 15},
    {0, 13},
    {0, 10},
    {0, 10}
};

/* DS:00a8 – Zuschauerzahl je Reichweitenstufe 0..9 (REACH_LEVEL; Mindestzuschauer der Werbung) */
static const int32_t reach_levels[10] = {
    0, 250000, 500000, 1000000, 2500000,
    5000000, 10000000, 25000000, 50000000, 100000000
};

/* DS:00d0 – unbekannt, im Port nicht gelesen */
static const int32_t unused_00d0[8] = {50000, 0, 100000, 0, 250000, 0, 500000, 0};

/* DS:00f2 – Spielernummern der beiden Computergegner (room_click_new_game_setup setzt sie neu) */
static const int8_t ai_players[2] = {2, 3};

/* DS:012e – Schrift: Glyphe in der Schriftbank je Zeichen (0 = nicht darstellbar; draw_string) */
static const uint8_t glyph_index[256] = {
    [0x01] = 0x59, [0x02] = 0x5a, [0x03] = 0x5b, [0x04] = 0x5e, [0x05] = 0x5f, [0x06] = 0x60,
    [0x07] = 0x61, ['!'] = 0x48, ['"'] = 0x50, ['$'] = 0x4c, ['%'] = 0x51, ['&'] = 0x58,
    [0x27] = 0x57, ['('] = 0x4d, [')'] = 0x4e, ['*'] = 0x62, ['+'] = 0x4f, [','] = 0x46,
    ['-'] = 0x4a, ['.'] = 0x45, ['/'] = 0x52, ['0'] = 0x44, ['1'] = 0x3b, ['2'] = 0x3c,
    ['3'] = 0x3d, ['4'] = 0x3e, ['5'] = 0x3f, ['6'] = 0x40, ['7'] = 0x41, ['8'] = 0x42,
    ['9'] = 0x43, [':'] = 0x47, ['<'] = 0x56, ['='] = 0x4b, ['>'] = 0x55, ['?'] = 0x49,
    ['A'] = 0x1, ['B'] = 0x2, ['C'] = 0x3, ['D'] = 0x4, ['E'] = 0x5, ['F'] = 0x6,
    ['G'] = 0x7, ['H'] = 0x8, ['I'] = 0x9, ['J'] = 0xa, ['K'] = 0xb, ['L'] = 0xc,
    ['M'] = 0xd, ['N'] = 0xe, ['O'] = 0xf, ['P'] = 0x10, ['Q'] = 0x11, ['R'] = 0x12,
    ['S'] = 0x13, ['T'] = 0x14, ['U'] = 0x15, ['V'] = 0x16, ['W'] = 0x17, ['X'] = 0x18,
    ['Y'] = 0x19, ['Z'] = 0x1a, ['['] = 0x5c, ['a'] = 0x1e, ['b'] = 0x1f, ['c'] = 0x20,
    ['d'] = 0x21, ['e'] = 0x22, ['f'] = 0x23, ['g'] = 0x24, ['h'] = 0x25, ['i'] = 0x26,
    ['j'] = 0x27, ['k'] = 0x28, ['l'] = 0x29, ['m'] = 0x2a, ['n'] = 0x2b, ['o'] = 0x2c,
    ['p'] = 0x2d, ['q'] = 0x2e, ['r'] = 0x2f, ['s'] = 0x30, ['t'] = 0x31, ['u'] = 0x32,
    ['v'] = 0x33, ['w'] = 0x34, ['x'] = 0x35, ['y'] = 0x36, ['z'] = 0x37, ['{'] = 0x5d,
    [0x81] = 0x3a, [0x82] = 0x68, [0x83] = 0x6d, [0x84] = 0x38, [0x85] = 0x6c, [0x88] = 0x6a,
    [0x8a] = 0x69, [0x8e] = 0x1b, [0x90] = 0x74, [0x94] = 0x39, [0x97] = 0x6f, [0x99] = 0x1c,
    [0x9a] = 0x1d, [0xa0] = 0x6b, [0xa1] = 0x71, [0xa2] = 0x72, [0xa3] = 0x6e, [0xa4] = 0x73,
    [0xa8] = 0x75, [0xad] = 0x76, [0xc0] = 0x63, [0xc1] = 0x64, [0xc2] = 0x65, [0xc3] = 0x66,
    [0xc4] = 0x67, [0xc5] = 0x70, [0xe1] = 0x54
};

/* DS:022e – Schrift: Breite in Pixeln je Zeichen (0 = 1 Pixel; draw_string, text_width) */
static const uint8_t glyph_width[256] = {
    ['!'] = 1, ['"'] = 3, ['$'] = 5, ['%'] = 4, ['&'] = 5, [0x27] = 2,
    ['('] = 2, [')'] = 2, ['*'] = 4, ['+'] = 3, [','] = 1, ['-'] = 3,
    ['.'] = 1, ['/'] = 3, ['0'] = 4, ['1'] = 4, ['2'] = 4, ['3'] = 4,
    ['4'] = 4, ['5'] = 4, ['6'] = 4, ['7'] = 4, ['8'] = 4, ['9'] = 4,
    [':'] = 1, ['<'] = 3, ['='] = 3, ['>'] = 3, ['?'] = 3, ['A'] = 4,
    ['B'] = 4, ['C'] = 4, ['D'] = 4, ['E'] = 4, ['F'] = 4, ['G'] = 4,
    ['H'] = 4, ['I'] = 1, ['J'] = 3, ['K'] = 4, ['L'] = 4, ['M'] = 5,
    ['N'] = 4, ['O'] = 4, ['P'] = 4, ['Q'] = 4, ['R'] = 4, ['S'] = 4,
    ['T'] = 5, ['U'] = 4, ['V'] = 5, ['W'] = 5, ['X'] = 5, ['Y'] = 5,
    ['Z'] = 4, ['['] = 6, ['a'] = 4, ['b'] = 4, ['c'] = 4, ['d'] = 4,
    ['e'] = 4, ['f'] = 3, ['g'] = 4, ['h'] = 4, ['i'] = 1, ['j'] = 3,
    ['k'] = 3, ['l'] = 1, ['m'] = 5, ['n'] = 4, ['o'] = 4, ['p'] = 4,
    ['q'] = 4, ['r'] = 3, ['s'] = 4, ['t'] = 3, ['u'] = 4, ['v'] = 3,
    ['w'] = 5, ['x'] = 3, ['y'] = 3, ['z'] = 3, ['{'] = 4, [0x81] = 4,
    [0x82] = 4, [0x83] = 4, [0x84] = 4, [0x85] = 4, [0x88] = 4, [0x8a] = 4,
    [0x8e] = 4, [0x90] = 4, [0x94] = 4, [0x97] = 4, [0x99] = 4, [0x9a] = 4,
    [0xa0] = 4, [0xa1] = 2, [0xa2] = 4, [0xa3] = 4, [0xa4] = 4, [0xa8] = 3,
    [0xad] = 1, [0xc0] = 8, [0xc1] = 8, [0xc2] = 8, [0xc3] = 8, [0xc4] = 8,
    [0xc5] = 8, [0xe1] = 4
};

/* DS:032e – Filmhändler: Rechteck x0, y0, x1, y1 der Kassetten 1..10 im Regal (Eintrag 0 ungenutzt) */
static const int16_t dealer_slots[11][4] = {
    {0, 0, 0, 0},
    {81, 66, 97, 101},
    {92, 65, 107, 99},
    {99, 65, 117, 97},
    {109, 64, 125, 96},
    {119, 63, 133, 94},
    {126, 62, 141, 92},
    {136, 61, 151, 90},
    {142, 60, 159, 88},
    {151, 60, 166, 86},
    {159, 60, 172, 85}
};

/* DS:039c – Mini-Spiel im Büro: Steine 1..7, je 4 Drehungen aus 4 x 4 Feldern (Wert = Farbe) */
static const int8_t tetris_pieces[7][4][4][4] = {
    {
        {
            {0, 0, 0, 0},
            {0, 1, 1, 0},
            {0, 1, 1, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 1, 1, 0},
            {0, 1, 1, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 1, 1, 0},
            {0, 1, 1, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 1, 1, 0},
            {0, 1, 1, 0},
            {0, 0, 0, 0}}},
    {
        {
            {0, 0, 0, 0},
            {0, 2, 2, 0},
            {2, 2, 0, 0},
            {0, 0, 0, 0}},
        {
            {0, 2, 0, 0},
            {0, 2, 2, 0},
            {0, 0, 2, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 2, 2, 0},
            {2, 2, 0, 0},
            {0, 0, 0, 0}},
        {
            {0, 2, 0, 0},
            {0, 2, 2, 0},
            {0, 0, 2, 0},
            {0, 0, 0, 0}}},
    {
        {
            {0, 0, 0, 0},
            {2, 2, 0, 0},
            {0, 2, 2, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 2, 0},
            {0, 2, 2, 0},
            {0, 2, 0, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {2, 2, 0, 0},
            {0, 2, 2, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 2, 0},
            {0, 2, 2, 0},
            {0, 2, 0, 0},
            {0, 0, 0, 0}}},
    {
        {
            {0, 0, 0, 0},
            {0, 3, 0, 0},
            {3, 3, 3, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 3, 0, 0},
            {0, 3, 3, 0},
            {0, 3, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 0, 0, 0},
            {3, 3, 3, 0},
            {0, 3, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 3, 0, 0},
            {3, 3, 0, 0},
            {0, 3, 0, 0}}},
    {
        {
            {0, 0, 0, 0},
            {4, 0, 0, 0},
            {4, 4, 4, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 4, 4, 0},
            {0, 4, 0, 0},
            {0, 4, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 0, 0, 0},
            {4, 4, 4, 0},
            {0, 0, 4, 0}},
        {
            {0, 0, 0, 0},
            {0, 4, 0, 0},
            {0, 4, 0, 0},
            {4, 4, 0, 0}}},
    {
        {
            {0, 0, 0, 0},
            {0, 0, 4, 0},
            {4, 4, 4, 0},
            {0, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {0, 4, 0, 0},
            {0, 4, 0, 0},
            {0, 4, 4, 0}},
        {
            {0, 0, 0, 0},
            {0, 0, 0, 0},
            {4, 4, 4, 0},
            {4, 0, 0, 0}},
        {
            {0, 0, 0, 0},
            {4, 4, 0, 0},
            {0, 4, 0, 0},
            {0, 4, 0, 0}}},
    {
        {
            {0, 0, 0, 0},
            {5, 5, 5, 5},
            {0, 0, 0, 0},
            {0, 0, 0, 0}},
        {
            {0, 5, 0, 0},
            {0, 5, 0, 0},
            {0, 5, 0, 0},
            {0, 5, 0, 0}},
        {
            {0, 0, 0, 0},
            {5, 5, 5, 5},
            {0, 0, 0, 0},
            {0, 0, 0, 0}},
        {
            {0, 5, 0, 0},
            {0, 5, 0, 0},
            {0, 5, 0, 0},
            {0, 5, 0, 0}}}
};

/* DS:055e – HES-Dateien je Sprache (Deutsch, Englisch): Oberflächentexte, Filme, Serien, Shows, Nachrichten, ... */
static const int16_t language_files[2][8] = {
    {0xc7, 0x2a, 0xcf, 0x2f, 0x2d, 0x2e, 0x31, 0x2c},
    {0xfd, 0xf6, 0xfc, 0xfb, 0xfa, 0xf9, 0xf8, 0xf7}
};

/* DS:0580 – unbekannt, im Port nicht gelesen */
static const int16_t unused_0580[13] = {0x51, 0x10, 0x2b, 0x48, 0x69, 0x41, 0x32, 0x26, 0x29, 0x37, 0x2e, 0x38, 0x1f};

/* DS:059a – zuletzt gezeichnete Minute der Uhr und des Fernsehbilds (-1 = noch keine; clock_update) */
static const int16_t clock_last_minutes[2] = {-1, -1};

/* DS:059e – Fernsehbilder: Bildnummer und Schriftbank (tv_show_picture) */
static const int16_t tv_pictures[4][2] = {
    {0xdc, 0xe},
    {0xec, 0x9},
    {0xed, 0xa},
    {0xee, 0xb}
};

/* DS:05ae – unbekannt, im Port nicht gelesen */
static const int16_t unused_05ae[1] = {0xdc};

/* DS:05e8 – Chef: Anzahl Einträge je Menü 1..5 (room_click_boss) */
static const int16_t boss_menu_sizes[5] = {3, 2, 3, 3, 3};

/* DS:0602 – Nachrichten: Rahmenstil je Kategorie 1..3 beim Ziehen (news_item_draw) */
static const int8_t news_styles[3] = {0x16, 0x15, 0x18};

/* DS:0636 – Klickbereiche: Rechteck, Raum, Dialog (-1 = alle), nur Besitzer (1), Kennung (hotspot_at_mouse) */
static const Hotspot hotspots[145] = {
    {5, 70, 44, 106, 0x19, 0, 1, 0x01},
    {220, 40, 258, 76, 0x19, 0, 1, 0x02},
    {48, 4, 200, 97, 0x19, 1, 0, 0x03},
    {256, 68, 293, 78, 0x19, 0, 0, 0x04},
    {256, 81, 293, 91, 0x19, 0, 0, 0x05},
    {256, 94, 293, 104, 0x19, 0, 0, 0x06},
    {294, 68, 311, 78, 0x19, -1, 0, 0x07},
    {294, 81, 311, 91, 0x19, -1, 0, 0x08},
    {294, 94, 311, 104, 0x19, -1, 0, 0x09},
    {264, 36, 311, 46, 0x19, 0, 1, 0x11},
    {265, 49, 276, 59, 0x19, 3, 0, 0x12},
    {277, 49, 290, 59, 0x19, 3, 0, 0x13},
    {291, 49, 311, 59, 0x19, 3, 0, 0x14},
    {264, 10, 311, 20, 0x19, 0, 1, 0x15},
    {264, 23, 311, 33, 0x19, 0, 1, 0x16},
    {88, 51, 124, 60, 0x19, 4, 1, 0x04},
    {88, 62, 124, 71, 0x19, 4, 1, 0x05},
    {88, 84, 143, 93, 0x19, 4, 1, 0x15},
    {12, 93, 27, 108, 0x1f, 0, 1, 0x0a},
    {296, 8, 311, 23, 0x1f, 0, 1, 0x45},
    {296, 25, 311, 40, 0x1f, 0, 0, 0x0d},
    {296, 42, 311, 57, 0x1f, 0, 1, 0x0e},
    {296, 59, 311, 74, 0x1f, 0, 1, 0x33},
    {296, 76, 311, 91, 0x1f, 0, 1, 0x38},
    {296, 93, 311, 108, 0x1f, 0, 1, 0x37},
    {200, 12, 288, 99, 0x1f, 1, 0, 0x17},
    {156, 72, 315, 111, 0x1f, 2, 0, 0x18},
    {176, 18, 315, 117, 0x1f, 5, 0, 0x19},
    {136, 104, 183, 114, 0x1f, 9, 0, 0x34},
    {12, 8, 59, 18, 0x1f, 7, 0, 0x04},
    {12, 22, 59, 32, 0x1f, 7, 0, 0x05},
    {12, 36, 59, 46, 0x1f, 7, 0, 0x06},
    {12, 56, 59, 66, 0x1f, 7, 0, 0x34},
    {12, 70, 59, 80, 0x1f, 7, 0, 0x4c},
    {128, 36, 160, 46, 0x1f, 10, 0, 0x46},
    {128, 60, 160, 70, 0x1f, 10, 0, 0x47},
    {128, 84, 160, 94, 0x1f, 10, 0, 0x48},
    {96, 98, 128, 110, 0x1f, 10, 0, 0x3f},
    {128, 98, 160, 110, 0x1f, 10, 0, 0x40},
    {160, 98, 192, 110, 0x1f, 10, 0, 0x41},
    {12, 12, 27, 27, 0x1f, -1, 0, 0x0b},
    {12, 30, 27, 45, 0x1f, -1, 0, 0x0c},
    {72, 4, 215, 115, 0x1f, -1, 1, 0x0f},
    {216, 4, 279, 115, 0x1f, -1, 1, 0x10},
    {149, 4, 170, 112, 0x00, 0, 0, 0x1a},
    {72, 4, 248, 112, 0x00, 0, 0, 0x1b},
    {298, 4, 303, 75, 0x01, 0, 0, 0x1c},
    {235, 45, 294, 90, 0x01, 0, 0, 0x1d},
    {62, 8, 90, 62, 0x01, 0, 1, 0x1e},
    {6, 4, 57, 106, 0x01, -1, 0, 0x44},
    {176, 77, 220, 93, 0x01, 0, 0, 0x36},
    {121, 68, 144, 84, 0x01, 0, 1, 0x49},
    {189, 89, 201, 100, 0x01, 2, 0, 0x45},
    {202, 89, 214, 100, 0x01, 2, 0, 0x46},
    {189, 101, 201, 110, 0x01, 2, 0, 0x47},
    {202, 101, 214, 110, 0x01, 2, 0, 0x48},
    {106, 36, 131, 43, 0x01, 2, 0, 0x3e},
    {136, 8, 185, 109, 0x01, 2, 0, 0x4a},
    {144, 12, 232, 100, 0x04, 1, 0, 0x17},
    {235, 13, 277, 56, 0x04, 0, 0, 0x22},
    {220, 4, 240, 35, 0x04, 0, 0, 0x1f},
    {160, 48, 195, 77, 0x04, -1, 1, 0x20},
    {16, 40, 127, 87, 0x04, -1, 1, 0x21},
    {90, 80, 105, 95, 0x08, 2, 0, 0x24},
    {142, 80, 157, 95, 0x08, 2, 0, 0x25},
    {106, 80, 121, 95, 0x08, 3, 0, 0x24},
    {190, 80, 205, 95, 0x08, 3, 0, 0x25},
    {216, 24, 279, 103, 0x08, -1, 0, 0x21},
    {81, 61, 173, 97, 0x08, -1, 0, 0x18},
    {17, 17, 58, 55, 0x08, -1, 0, 0x23},
    {103, 15, 130, 59, 0x0b, 0, 0, 0x23},
    {111, 76, 134, 106, 0x0b, 0, 0, 0x3b},
    {131, 61, 162, 91, 0x0b, 0, 0, 0x3c},
    {157, 43, 180, 73, 0x0b, 0, 0, 0x3d},
    {105, 62, 163, 106, 0x0b, -1, 0, 0x26},
    {151, 43, 181, 74, 0x0b, -1, 0, 0x26},
    {216, 24, 287, 103, 0x0b, -1, 0, 0x21},
    {4, 4, 68, 56, 0x09, -1, 0, 0x44},
    {91, 35, 140, 63, 0x09, -1, 0, 0x26},
    {16, 56, 79, 103, 0x09, -1, 0, 0x21},
    {193, 35, 247, 86, 0x09, 0, 0, 0x23},
    {4, 4, 97, 110, 0x0a, 0, 0, 0x27},
    {117, 80, 132, 95, 0x3e, -1, 0, 0x24},
    {187, 80, 202, 95, 0x3e, -1, 0, 0x25},
    {14, 28, 39, 57, 0x05, 0, 1, 0x28},
    {238, 70, 289, 113, 0x05, -1, 1, 0x29},
    {240, 16, 303, 55, 0x05, -1, 1, 0x21},
    {50, 85, 77, 120, 0x05, -1, 1, 0x0a},
    {90, 44, 215, 51, 0x05, 3, 0, 0x2a},
    {90, 52, 215, 59, 0x05, 3, 0, 0x4b},
    {90, 60, 215, 67, 0x05, 3, 0, 0x2b},
    {216, 44, 252, 67, 0x05, 3, 0, 0x24},
    {106, 80, 121, 95, 0x05, 4, 0, 0x24},
    {190, 80, 205, 95, 0x05, 4, 0, 0x25},
    {106, 80, 121, 95, 0x05, 5, 0, 0x24},
    {190, 80, 205, 95, 0x05, 5, 0, 0x25},
    {8, 22, 41, 113, 0x0c, 0, 0, 0x23},
    {60, 36, 200, 51, 0x0c, 1, 0, 0x2c},
    {60, 60, 200, 75, 0x0c, 1, 0, 0x2d},
    {16, 64, 63, 103, 0x0c, 3, 0, 0x21},
    {16, 64, 63, 103, 0x0c, 4, 0, 0x21},
    {16, 64, 63, 103, 0x0c, 5, 0, 0x21},
    {84, 4, 308, 92, 0x0c, -1, 0, 0x35},
    {53, 6, 60, 65, 0x3f, 2, 0, 0x24},
    {56, 7, 72, 22, 0x3f, -1, 0, 0x2f},
    {76, 7, 91, 22, 0x3f, -1, 0, 0x31},
    {12, 77, 27, 92, 0x3f, -1, 0, 0x30},
    {12, 94, 27, 109, 0x3f, -1, 0, 0x32},
    {8, 22, 41, 50, 0x3f, -1, 0, 0x23},
    {69, 73, 146, 112, 0x02, 0, 1, 0x3e},
    {4, 75, 19, 94, 0x02, 0, 1, 0x3f},
    {21, 75, 41, 94, 0x02, 0, 1, 0x40},
    {42, 75, 62, 94, 0x02, 0, 1, 0x41},
    {68, 8, 145, 68, 0x02, 0, 0, 0x42},
    {166, 7, 307, 108, 0x02, -1, 1, 0x43},
    {12, 41, 156, 108, 0x02, 1, 1, 0x44},
    {12, 8, 32, 28, 0x02, 1, 1, 0x3f},
    {36, 8, 56, 28, 0x02, 1, 1, 0x40},
    {60, 8, 80, 28, 0x02, 1, 1, 0x41},
    {100, 8, 115, 23, 0x02, 1, 1, 0x2f},
    {120, 8, 135, 23, 0x02, 1, 1, 0x31},
    {120, 37, 160, 80, 0x0e, -1, 0, 0x36},
    {16, 64, 63, 103, 0x0e, -1, 0, 0x21},
    {72, 8, 104, 48, 0x0e, -1, 0, 0x4a},
    {128, 72, 143, 87, 0x3a, 1, 0, 0x24},
    {184, 72, 199, 87, 0x3a, 1, 0, 0x25},
    {123, 64, 146, 87, 0x3a, 2, 0, 0x39},
    {181, 64, 204, 87, 0x3a, 2, 0, 0x3a},
    {172, 76, 299, 107, 0x03, 2, 1, 0x49},
    {156, 76, 171, 91, 0x03, 2, 1, 0x24},
    {156, 92, 171, 107, 0x03, 2, 1, 0x25},
    {28, 44, 100, 56, 0x41, 0, 0, 0x07},
    {28, 58, 100, 70, 0x41, 0, 0, 0x04},
    {124, 44, 196, 56, 0x41, 0, 0, 0x08},
    {124, 58, 196, 70, 0x41, 0, 0, 0x05},
    {220, 44, 292, 56, 0x41, 0, 0, 0x09},
    {220, 58, 292, 70, 0x41, 0, 0, 0x06},
    {28, 76, 100, 88, 0x41, 0, 0, 0x3f},
    {124, 76, 196, 88, 0x41, 0, 0, 0x40},
    {220, 76, 292, 88, 0x41, 0, 0, 0x41},
    {24, 94, 104, 108, 0x41, 0, 0, 0x4a},
    {120, 94, 200, 108, 0x41, 0, 0, 0x4b},
    {216, 94, 296, 108, 0x41, 0, 0, 0x24},
    {0, 0, 0, 0, 0x00, 0, 0, 0x00},
    {0, 0, 0, 0, 0x00, 0, 0, 0x00}
};

/* DS:0d2a – Mini-Spiel: Punkte je Stein 1..7 */
static const int8_t tetris_points[7] = {10, 8, 8, 6, 6, 6, 4};

/* DS:0d31 – Mini-Spiel: zuletzt gezeichnete Spalte (999 = keine) */
static const int16_t tetris_old_col[1] = {999};

/* DS:0d38 – Zeigerschritt der Pfeiltasten (input_poll; wächst bis 16) */
static const int16_t arrow_key_step[1] = {2};

/* DS:0d40 – Raumanimation: letzter Schritt (-1 = keiner) */
static const int16_t anim_last[1] = {-1};

/* DS:0d5a – Raumanimation: Sprite, x, y fester Figuren (room_animate) */
static const int8_t room_anim_sprites[4][3] = {
    {2, 4, 49},
    {3, 4, 49},
    {4, 7, 0},
    {5, 7, 1}
};

/* DS:0d66 – Raumanimation: Positionen x, y (Sprites 10..12) */
static const int8_t room_anim_pos_a[3][2] = {
    {29, 11},
    {30, 6},
    {32, 4}
};

/* DS:0d6c – Raumanimation: Positionen x, y (Sprites 3..9) */
static const int8_t room_anim_pos_b[7][2] = {
    {6, 0},
    {5, 0},
    {5, 0},
    {5, 0},
    {5, 0},
    {6, 0},
    {6, 1}
};

/* DS:0d7a – Raumanimation: Position je Zustand DS:7a32 */
static const int8_t room_anim_pos_c[4][2] = {
    {3, 0},
    {2, 0},
    {2, 1},
    {2, 1}
};

/* DS:0d82 – Raumanimation: Sprite, x, y (Bank 0x10c) */
static const int8_t room_anim_sprites_b[8][3] = {
    {4, 24, 18},
    {7, 24, 18},
    {8, 24, 17},
    {9, 24, 12},
    {26, 24, 17},
    {27, 24, 21},
    {9, 24, 12},
    {9, 24, 12}
};

/* DS:0d9c – Fernsehbild: Bildfolge ab Index 1 (Basis DS:0d9b, tv_picture_compose) */
static const int8_t tv_picture_steps[17] = {3, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 3};

/* DS:0dad – Stapelanzeige: Positionen x, y (room_stack_overlay_draw) */
static const int16_t stack_overlay_pos[31][2] = {
    {155, 72},
    {164, 78},
    {147, 75},
    {186, 80},
    {180, 73},
    {168, 68},
    {205, 84},
    {202, 78},
    {195, 71},
    {179, 64},
    {219, 83},
    {208, 73},
    {197, 65},
    {234, 92},
    {232, 83},
    {214, 90},
    {222, 75},
    {192, 58},
    {192, 87},
    {245, 81},
    {238, 71},
    {217, 66},
    {253, 93},
    {211, 56},
    {224, 62},
    {206, 51},
    {223, 56},
    {217, 45},
    {236, 59},
    {172, 60},
    {176, 54}
};

/* DS:0e29 – Stapelanzeige: Versatz x, y und Sprite */
static const int16_t stack_overlay_items[5][3] = {
    {6, 5, 3},
    {14, 2, 4},
    {0, 1, 5},
    {14, 4, 6},
    {7, 0, 3}
};

/* DS:0e47 – Publikum: Positionen x, y (room_crowd_draw) */
static const uint8_t crowd_pos[12][2] = {
    {197, 115},
    {213, 109},
    {185, 104},
    {198, 99},
    {173, 91},
    {166, 95},
    {177, 95},
    {171, 99},
    {161, 101},
    {168, 106},
    {178, 103},
    {156, 107}
};

/* DS:0e5f – Publikum: Markierung je Zustand DS:7a32 */
static const uint8_t crowd_marker_pos[4][2] = {
    {221, 28},
    {221, 25},
    {221, 29},
    {216, 26}
};

/* DS:0e67 – Spielende: Bild, Sprite, x, y (game_end_sequence) */
static const uint8_t game_end_sprites[6][4] = {
    {90, 2, 127, 69},
    {92, 3, 142, 87},
    {94, 4, 152, 97},
    {96, 5, 156, 104},
    {98, 6, 158, 104},
    {100, 0, 0, 0}
};

/* DS:117c – Gebäude: gezeichnete Tageszeit (1 Tag, 2 Nacht) */
static const int16_t building_daynight[1] = {1};

/* DS:117e – Gebäude: Dekoration Sprite, Breite, Höhe je Typ */
static const int8_t building_decor[12][3] = {
    {0x9, 0x18, 0x18},
    {0xb, 0x18, 0x18},
    {0xd, 0x18, 0x18},
    {0x14, 0x20, 0x11},
    {0x15, 0x10, 0xd},
    {0x16, 0x10, 0xa},
    {0x17, 0x18, 0xb},
    {0x18, 0x10, 0x15},
    {0x19, 0x18, 0xf},
    {0x1a, 0x20, 0x1c},
    {0x1c, 0x18, 0x1c},
    {0x7, 0x8, 0xa}
};

/* DS:11a2 – Gebäude: oberste gezeichnete Etage */
static const int16_t building_top_floor[1] = {5};

/* DS:11a6 – Ausscheiden eines Spielers: Laufweg je Spieler 1..3, je Abschnitt Grenze x und Schrittweite */
static const uint8_t eliminated_steps[3][5][2] = {
    {
        {105, 5},
        {70, 4},
        {35, 3},
        {20, 2},
        {0, 0}},
    {
        {130, 5},
        {110, 4},
        {85, 3},
        {55, 2},
        {20, 1}},
    {
        {130, 4},
        {100, 3},
        {60, 2},
        {20, 1},
        {0, 0}}
};

/* DS:11e6 – Nachrichten: Wertstufen beim Erzeugen (news_generate) */
static const int8_t news_value_levels[6] = {15, 30, 40, 50, 60, 100};

/* DS:11ec – Nachrichten: Wertstufen beim Altern (news_values_decay) */
static const int8_t news_decay_levels[6] = {15, 30, 40, 50, 60, 100};

/* DS:11f2 – Senderkarte: Kreismaske der Reichweite, 16 Zeilen zu 32 Bit (2 Bit je Feld) */
static const uint16_t map_circle[16][2] = {
    {0xfc00, 0x3f},
    {0xffc0, 0x3ff},
    {0xfff0, 0xfff},
    {0xfffc, 0x3fff},
    {0xfffc, 0x3fff},
    {0xffff, 0xffff},
    {0xffff, 0xffff},
    {0xffff, 0xffff},
    {0xffff, 0xffff},
    {0xffff, 0xffff},
    {0xffff, 0xffff},
    {0xfffc, 0x3fff},
    {0xfffc, 0x3fff},
    {0xfff0, 0xfff},
    {0xffc0, 0x3ff},
    {0xfc00, 0x3f}
};

/* DS:1272 – Senderkarte: Gewicht je Feldwert 0..3 (map_cell_value, map_population_sum, mask_population_count) */
static const int16_t map_cell_weights[3][4] = {
    {0, 1, 10, 100},
    {0, 1, 10, 100},
    {0, 1, 10, 100}
};

/* DS:128a – Preisstufen (price_step_next) */
static const int32_t price_steps[4] = {50000, 100000, 250000, 500000};

/* DS:129a – Wert je Nachrichtengenre (genre_value_get) */
static const int16_t genre_values[6] = {1000, 2000, 3000, 4000, 6000, 8000};

/* DS:12a6 – Nachrichtenabos: nötiges Image je Kategorie */
static const int16_t subscription_image[4] = {0, 5, 10, 15};

/* DS:12ae – Wettbewerbe: Gewinn je Art 1..4 (contest_tick) */
static const int16_t contest_prizes[4] = {8, 4, 6, 6};

/* DS:12e6 – Dialograhmen 1..7: Spalte/Zeile links oben und rechts unten in 8er-Kacheln (dialog_frame) */
static const int8_t dialog_frames[7][4] = {
    {2, 5, 15, 10},
    {27, 3, 34, 12},
    {27, 3, 35, 12},
    {2, 7, 9, 12},
    {30, 2, 37, 7},
    {2, 8, 7, 12},
    {2, 8, 7, 12}
};

/* DS:132e – Rahmen: Kachelbasis je Stil (ab 0), Spielerfahnen (ab +7), Nachrichtenrahmen je Kategorie (ab +10, +13) */
static const int8_t frame_styles[17] = {12, 21, 34, 43, 52, 61, 70, 79, 6, 27, 49, 5, 4, 7, 5, 4, 7};

/* DS:1490 – Chef: Menü, Eintrag, Text (boss_dialog) */
static const int16_t boss_texts[15][3] = {
    {0x1, 0x1, 0xaf},
    {0x1, 0x2, 0xb0},
    {0x1, 0x3, 0xb1},
    {0x2, 0x1, 0xb4},
    {0x2, 0x2, 0xb5},
    {0x3, 0x1, 0xbb},
    {0x3, 0x2, 0xbc},
    {0x3, 0x3, 0xbd},
    {0x4, 0x1, 0xc4},
    {0x4, 0x2, 0xc5},
    {0x4, 0x3, 0xc6},
    {0x5, 0x1, 0x34},
    {0x5, 0x2, 0x38},
    {0x5, 0x3, 0x3a},
    {0x0, 0x0, 0x0}
};

/* DS:14f0 – Farbe je Spieler 1..3 (Basis DS:14ef) */
static const int8_t player_colors[3] = {4, 7, 5};

/* DS:15ec – KI: Gewichte der Budgetanteile (ai_budget_plan) */
static const int8_t ai_budget_weights[7] = {0, 5, 28, 14, 18, 35, 0};

/* DS:15f6 – unbekannt, im Port nicht gelesen */
static const uint8_t unused_15f6[4] = {15, 30, 40, 45};

/* DS:15fa – KI: Nachrichtenbedarf je Stufe (ai_buy_news) */
static const int16_t ai_news_need[4] = {30, 50, 70, 90};

/* DS:1606 – Musik: Raum, dessen Stück läuft, und zuletzt verlassener Raum (-1 = keiner) */
static const int16_t music_rooms[2] = {-1, -1};

/* DS:161c – Intro: je Szene Hintergrund und drei Bildströme (HES-Nummern) */
static const int16_t intro_scenes[14][4] = {
    {0x131, 0x12e, 0x12f, 0x130},
    {0x12d, 0x12c, 0x0, 0x0},
    {0x136, 0x137, 0x138, 0x0},
    {0x13a, 0x13b, 0x0, 0x0},
    {0x13c, 0x13d, 0x0, 0x0},
    {0x13e, 0x13f, 0x0, 0x0},
    {0x140, 0x141, 0x0, 0x0},
    {0x140, 0x142, 0x0, 0x0},
    {0x143, 0x144, 0x0, 0x0},
    {0x145, 0x146, 0x0, 0x0},
    {0x147, 0x148, 0x0, 0x0},
    {0x149, 0x14a, 0x14b, 0x14c},
    {0x14d, 0x14e, 0x14f, 0x0},
    {0x150, 0x151, 0x0, 0x0}
};

/* DS:168c – Intro: nächster Bildstrom */
static const int16_t intro_next_stream[1] = {1};

/* DS:168e – Bitmasken des LZW-Entpackers im Original (der Port entpackt in src/unpack.c) */
static const int16_t lzw_masks[9] = {0x102, 0xff, 0x7f, 0x3f, 0x1f, 0xf, 0x7, 0x3, 0x1};

/* DS:16ac – Tastatur: Spieltaste je ASCII-Zeichen (0xff = keine; kbd_getkey) */
static const uint8_t keymap_ascii[130] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x21, 0x40, 0xff, 0xff, 0xff, 0x1f, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x14, 0xff, 0xff, 0xff, 0xff,
    0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1e, 0x22, 0x1d, 0x23, 0xff,
    0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x13, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34,
    0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34,
    0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff
};

/* DS:172e – Tastatur: Spieltaste je Scancode für Tasten ohne Zeichen */
static const uint8_t keymap_scan[130] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0, 0x1, 0x2, 0x3, 0x4,
    0x5, 0x6, 0x7, 0x8, 0x9, 0xff, 0xff, 0xff, 0x15, 0xff, 0xff, 0x17, 0xff, 0x18, 0xff, 0xff,
    0x16, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff
};

/* DS:1940 – Startwert des Zufallsgenerators (wie rand() in C) */
static const uint16_t rand_seed[2] = {1, 0};

/* Formatvorlagen (fmt, siehe seg_1d41.c), Layouts der Info-Tafeln und feste Texte */
static const struct {
    uint16_t ofs;
    const char *text;
} k_strings[] = {
    /* Uhrzeit, Spielstände, Sendeplan */
    {0x05b5, "#02d:#02d"},
    {0x05c0, "#*13s"},
    {0x05c6, "#*13s"},
    {0x05cc, "#*13s"},
    {0x05d2, "#s~#s"},
    {0x05d8, "#02d:#02d"},
    /* Chef-Sprechblasen und Nachrichten */
    {0x0605, "#s#s#s"},
    {0x060c, "#s#s"},
    {0x0611, "#*51s"},
    {0x0617, "*** #02d:00 #_5s ** $ #_5d ***"},
    /* Zahlenformat (fmt) */
    {0x0d1a, "Fuck"},
    {0x0d1f, "-"},
    {0x0d21, "-"},
    /* Eingabemarke (text_input) */
    {0x0d33, "["},
    /* Info-Tafeln (room_hover): Layouts "Zeilen Spalten ..." und Formatvorlagen */
    {0x0e8e, "-01"},
    {0x0e92, "107 --- 1-- --- 1-- --- 1--"},
    {0x0eae, "303 1-- 1--"},
    {0x0eba, "302 2--"},
    {0x0ec2, "#s~$ #d"},
    {0x0eca, " "},
    {0x0ecc, "301 214 2-- 2-- 2-- 121 232 2-- 143 2-- 2-- 151"},
    {0x0efc, "302 3-- 213 2-- 2-- 121 232 2-- 143 2-- 2-- 151"},
    {0x0f2c, "$ #l"},
    {0x0f31, "204 1-- 2-- 1--"},
    {0x0f41, "204 1-- 2-- 1--"},
    {0x0f51, "106 --- 1-- --- 1-- ---"},
    {0x0f69, "104 --- 1-- ---"},
    {0x0f79, "311 223 2-- 2-- 133 1-- 2-- 141 203 2-- 2--"},
    {0x0fa5, " "},
    {0x0fa7, "301 234 2-- 2-- 2-- 111 221 241"},
    {0x0fc7, "$ #l"},
    {0x0fcc, "(---)"},
    {0x0fd2, "#s #01d #s~"},
    {0x0fdf, "(---)"},
    {0x0fe5, "(---)"},
    {0x0feb, "#d. #*13s #s"},
    {0x0ff8, "301 112 1-- 221 231 241 251 261 301"},
    {0x101c, "302 3-- 214 2-- 2-- 2--"},
    {0x1034, "#s~(#s):~#s"},
    {0x1040, "#s:~#s"},
    {0x1047, "#s"},
    {0x104a, "#32f~#32f~#32f"},
    {0x1059, "#s (#s)"},
    {0x1061, "107 1-- 1-- 1-- 1-- 1-- 1--"},
    {0x107d, "301 213 2-- 1--"},
    {0x108d, "#d. #s"},
    {0x1094, "301 112 1-- 123 2-- 1--"},
    {0x10ac, " "},
    {0x10ae, "#s~#s#s"},
    {0x10b6, " "},
    {0x10b8, "#s~#s~#d.#s#s"},
    {0x10c6, "???"},
    {0x10ca, "**** #5s ***** (#_2d%) ****"},
    {0x10e6, "**** #6s **** (#_2d%) ****"},
    {0x1101, " "},
    {0x1103, "#s, "},
    {0x1109, "#s#s#s"},
    {0x1110, " "},
    {0x1112, " "},
    {0x1114, " "},
    {0x1116, "301 214 2-- 2-- 2-- 121 233 2-- 2-- 161 251 241"},
    {0x1146, "302 3-- 213 2-- 2-- 121 233 2-- 2-- 161 251 241"},
    {0x1176, "$ #l"},
    /* Gebäude: Etagennummer, Türschilder, Status */
    {0x11c4, "#d"},
    {0x11c7, "#s #s"},
    {0x11cd, "#s #s "},
    {0x11d4, "#s #s "},
    {0x11db, "  "},
    {0x11de, "  "},
    /* Geldbeträge (fmt_money) */
    {0x12b6, "#s #s"},
    {0x12bc, "#s #s"},
    {0x12c2, "#d,#d"},
    {0x12c8, "#d,#+02d"},
    /* Name ohne Eintrag (name_table_get) */
    {0x12dc, "<not set>"},
    /* Statusleiste, Sendeplan, Archiv, Nachrichten, Auktion, Studio, Statistik */
    {0x1344, "#_6l"},
    {0x1349, "#02d:00"},
    {0x1351, "#*7s~#d/#d"},
    {0x135c, "#*7s~---"},
    {0x1365, "#*7s~#d/#d"},
    {0x1371, "#*34s"},
    {0x1377, "#*17s:~#d:#*15s"},
    {0x1387, "#*17s:~#d:#*15s"},
    {0x1398, "Fehler aufgetreten!"},
    {0x13ac, "#s"},
    {0x13af, "#s:"},
    {0x13b3, "#*18s"},
    {0x13b9, "#_2d. #*15s"},
    {0x13c5, "#*16s:~#s~#s~#s~#s~#s~#s~#s~#s~#s~#s"},
    {0x13ea, "#*15s"},
    {0x13f0, "#*54s"},
    {0x13f6, "*** #02d:00 #_5s ** $ #_5d ***"},
    {0x1415, "#*54s"},
    {0x141b, "*** #02d:00 #_5s ** $ #_5d ***"},
    {0x143a, "#*14s"},
    {0x1440, "#*14s"},
    {0x1446, "#_7l #*8s"},
    {0x1450, "#_2d"},
    {0x1455, "#_2d"},
    {0x145a, " "},
    {0x145c, "$ #l~#32f~#32f~#32f"},
    {0x1470, "#s"},
    {0x1473, "#l"},
    {0x1476, "#_6l"},
    {0x147b, "#d%"},
    {0x147f, "#_3d"},
    {0x1484, "#_3d%"},
    {0x148a, "#02d"},
    /* Produktion, Genre-Menü, Mini-Spiel, Meldungen, Bilanz */
    {0x14f3, "#d"},
    {0x14f6, "#d"},
    {0x14f9, "#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s~#v #s"},
    {0x153b, "#04d"},
    {0x1540, "#_4d"},
    {0x1545, "#04d"},
    {0x154a, "#04d"},
    {0x154f, "#s #s, #s"},
    {0x155b, "#s #s #s"},
    {0x1564, "#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $~#_8l $"},
    {0x15a3, "#_8l $"},
    {0x15aa, "#_8l $~#_8l $~#_8l $"},
    {0x15bf, "#_8l $"},
    {0x15c6, "#_8l $"},
    {0x15cd, "#_8l $~#_8l $~#_8l $"},
    {0x15e2, "#_8l $"},
};

#define BLOCK(ofs, table) {ofs, table, sizeof table}
static const struct {
    uint16_t ofs;
    const void *data;
    uint16_t len;
} k_blocks[] = {
    BLOCK(0x0094, daily_topics),
    BLOCK(0x00a8, reach_levels),
    BLOCK(0x00d0, unused_00d0),
    BLOCK(0x00f2, ai_players),
    BLOCK(0x012e, glyph_index),
    BLOCK(0x022e, glyph_width),
    BLOCK(0x032e, dealer_slots),
    BLOCK(0x039c, tetris_pieces),
    BLOCK(0x055e, language_files),
    BLOCK(0x0580, unused_0580),
    BLOCK(0x059a, clock_last_minutes),
    BLOCK(0x059e, tv_pictures),
    BLOCK(0x05ae, unused_05ae),
    BLOCK(0x05e8, boss_menu_sizes),
    BLOCK(0x0602, news_styles),
    BLOCK(0x0636, hotspots),
    BLOCK(0x0d2a, tetris_points),
    BLOCK(0x0d31, tetris_old_col),
    BLOCK(0x0d38, arrow_key_step),
    BLOCK(0x0d40, anim_last),
    BLOCK(0x0d5a, room_anim_sprites),
    BLOCK(0x0d66, room_anim_pos_a),
    BLOCK(0x0d6c, room_anim_pos_b),
    BLOCK(0x0d7a, room_anim_pos_c),
    BLOCK(0x0d82, room_anim_sprites_b),
    BLOCK(0x0d9c, tv_picture_steps),
    BLOCK(0x0dad, stack_overlay_pos),
    BLOCK(0x0e29, stack_overlay_items),
    BLOCK(0x0e47, crowd_pos),
    BLOCK(0x0e5f, crowd_marker_pos),
    BLOCK(0x0e67, game_end_sprites),
    BLOCK(0x117c, building_daynight),
    BLOCK(0x117e, building_decor),
    BLOCK(0x11a2, building_top_floor),
    BLOCK(0x11a6, eliminated_steps),
    BLOCK(0x11e6, news_value_levels),
    BLOCK(0x11ec, news_decay_levels),
    BLOCK(0x11f2, map_circle),
    BLOCK(0x1272, map_cell_weights),
    BLOCK(0x128a, price_steps),
    BLOCK(0x129a, genre_values),
    BLOCK(0x12a6, subscription_image),
    BLOCK(0x12ae, contest_prizes),
    BLOCK(0x12e6, dialog_frames),
    BLOCK(0x132e, frame_styles),
    BLOCK(0x1490, boss_texts),
    BLOCK(0x14f0, player_colors),
    BLOCK(0x15ec, ai_budget_weights),
    BLOCK(0x15f6, unused_15f6),
    BLOCK(0x15fa, ai_news_need),
    BLOCK(0x1606, music_rooms),
    BLOCK(0x161c, intro_scenes),
    BLOCK(0x168c, intro_next_stream),
    BLOCK(0x168e, lzw_masks),
    BLOCK(0x16ac, keymap_ascii),
    BLOCK(0x172e, keymap_scan),
    BLOCK(0x1940, rand_seed),
};

void ds_init_data(void)
{
    for (size_t i = 0; i < sizeof k_blocks / sizeof *k_blocks; i++)
        memcpy(g_ds + k_blocks[i].ofs, k_blocks[i].data, k_blocks[i].len);
    for (size_t i = 0; i < sizeof k_strings / sizeof *k_strings; i++)
        memcpy(g_ds + k_strings[i].ofs, k_strings[i].text, strlen(k_strings[i].text) + 1);
}
