#include "menubar.h"

#include <SDL_syswm.h>
#include <dlfcn.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"

/* stb_truetype unverändert eingebunden; Warnungen nur hier abschalten */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb/stb_truetype.h"
#pragma GCC diagnostic pop

/* Noto Sans (Teilmenge, third_party/noto-sans), per .incbin eingebunden (src/menufont.S) */
extern const unsigned char menubar_font[];

/* Maße in Fensterpunkten, nach KDE Breeze (Schrift 10 pt bei 96 dpi) */
#define FONT_PX 13.333f
#define BAR_H 26
#define BAR_PAD 2    /* Abstand der Titel vom linken Rand */
#define TITLE_PAD 9  /* Innenabstand eines Titels links/rechts */
#define ITEM_H 28
#define SEP_H 9      /* Trennlinie */
#define MENU_PAD 4   /* Innenabstand eines Menüs oben/unten */
#define MARK_W 30    /* Spalte für Häkchen/Auswahlpunkt */
#define KEY_GAP 40   /* Mindestabstand Text - Tastenkürzel */
#define ARROW_W 22   /* Spalte für den Untermenü-Pfeil */
#define RIGHT_PAD 12
#define MIN_MENU_W 140

/* Farben (Breeze Light) */
static const SDL_Color COL_BAR = {239, 240, 241, 255};
static const SDL_Color COL_MENU = {252, 252, 252, 255};
static const SDL_Color COL_TEXT = {35, 38, 41, 255};
static const SDL_Color COL_KEY = {112, 125, 138, 255};
static const SDL_Color COL_DISABLED = {160, 164, 168, 255};
static const SDL_Color COL_FRAME = {188, 190, 191, 255};
static const SDL_Color COL_HOVER = {61, 174, 233, 60};
static const SDL_Color COL_HOVER_FRAME = {61, 174, 233, 255};

static SDL_Window *g_win;
static SDL_Renderer *g_ren;

/* ---- Schrift ---- */

static const int g_extra[] = {0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x203A, 0x20AC};
enum { N_ASCII = 95, N_LATIN = 96, N_EXTRA = sizeof g_extra / sizeof *g_extra };
static stbtt_fontinfo g_font;
static int g_font_ok = -1;
static stbtt_packedchar g_pc[N_ASCII + N_LATIN + N_EXTRA];
static SDL_Texture *g_atlas, *g_check, *g_dot, *g_icon;
static int g_atlas_w, g_atlas_h, g_check_px;
static float g_baked_dpi, g_ascent, g_descent;

static int glyph_index(int cp)
{
    if (cp >= 32 && cp < 127)
        return cp - 32;
    if (cp >= 160 && cp < 256)
        return N_ASCII + cp - 160;
    for (int i = 0; i < (int)N_EXTRA; i++)
        if (g_extra[i] == cp)
            return N_ASCII + N_LATIN + i;
    return '?' - 32;
}

/* nächstes Zeichen aus UTF-8 */
static int utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int cp = *p++, n = 0;
    if (cp >= 0xF0)
        cp &= 0x07, n = 3;
    else if (cp >= 0xE0)
        cp &= 0x0F, n = 2;
    else if (cp >= 0xC0)
        cp &= 0x1F, n = 1;
    while (n-- > 0 && (*p & 0xC0) == 0x80)
        cp = (cp << 6) | (*p++ & 0x3F);
    *s = (const char *)p;
    return cp;
}

/* Häkchen (dot = 0) oder Auswahlpunkt (dot = 1) kantengeglättet in eine kleine Textur zeichnen:
 * Deckung aus dem Abstand zu zwei Strecken bzw. zum Mittelpunkt */
static SDL_Texture *make_mark(int n, float thick, int dot)
{
    static const float seg[2][4] = {{0.16f, 0.52f, 0.40f, 0.76f}, {0.40f, 0.76f, 0.86f, 0.24f}};
    uint32_t *px = malloc((size_t)n * n * 4);
    if (!px)
        return NULL;
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++) {
            float d = 1e9f;
            if (dot) {
                float dx = x + 0.5f - n / 2.0f, dy = y + 0.5f - n / 2.0f;
                d = sqrtf(dx * dx + dy * dy) - n * 0.2f; /* Punkt mit Radius 0,2 n + thick/2 */
            }
            for (int k = 0; k < 2 && !dot; k++) {
                float ax = seg[k][0] * n, ay = seg[k][1] * n, bx = seg[k][2] * n, by = seg[k][3] * n;
                float px_ = x + 0.5f - ax, py_ = y + 0.5f - ay, dx = bx - ax, dy = by - ay;
                float t = (px_ * dx + py_ * dy) / (dx * dx + dy * dy);
                t = t < 0 ? 0 : t > 1 ? 1 : t;
                float ex = px_ - t * dx, ey = py_ - t * dy, dist = sqrtf(ex * ex + ey * ey);
                if (dist < d)
                    d = dist;
            }
            float a = thick / 2 - d + 0.5f;
            a = a < 0 ? 0 : a > 1 ? 1 : a;
            px[y * n + x] = ((uint32_t)(a * 255 + 0.5f) << 24) | 0xFFFFFF;
        }
    SDL_Texture *t = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, n, n);
    if (t) {
        SDL_UpdateTexture(t, NULL, px, n * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    }
    free(px);
    return t;
}

/* Schrift für die aktuelle Pixeldichte in eine Textur packen (bei Wechsel des Monitors neu) */
static int bake(float dpi)
{
    if (g_atlas && fabsf(dpi - g_baked_dpi) < 0.01f)
        return 0;
    if (g_font_ok < 0)
        g_font_ok = stbtt_InitFont(&g_font, menubar_font, stbtt_GetFontOffsetForIndex(menubar_font, 0)) ? 1 : 0;
    if (!g_font_ok)
        return -1;
    float size = FONT_PX * dpi;
    int w = 512, h = 256;
    for (int tries = 0; tries < 5; tries++, (tries & 1) ? (h *= 2) : (w *= 2)) {
        unsigned char *bmp = calloc((size_t)w * h, 1);
        stbtt_pack_context pc;
        if (!bmp || !stbtt_PackBegin(&pc, bmp, w, h, 0, 1, NULL)) {
            free(bmp);
            return -1;
        }
        stbtt_PackSetOversampling(&pc, 2, 1);
        stbtt_pack_range r[3];
        memset(r, 0, sizeof r);
        r[0].font_size = STBTT_POINT_SIZE(size);
        r[0].first_unicode_codepoint_in_range = 32;
        r[0].num_chars = N_ASCII;
        r[0].chardata_for_range = g_pc;
        r[1].font_size = STBTT_POINT_SIZE(size);
        r[1].first_unicode_codepoint_in_range = 160;
        r[1].num_chars = N_LATIN;
        r[1].chardata_for_range = g_pc + N_ASCII;
        r[2].font_size = STBTT_POINT_SIZE(size);
        r[2].array_of_unicode_codepoints = (int *)g_extra;
        r[2].num_chars = N_EXTRA;
        r[2].chardata_for_range = g_pc + N_ASCII + N_LATIN;
        int ok = stbtt_PackFontRanges(&pc, menubar_font, 0, r, 3);
        stbtt_PackEnd(&pc);
        if (!ok) {
            free(bmp);
            continue;
        }
        uint32_t *argb = malloc((size_t)w * h * 4);
        SDL_Texture *t = argb ? SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h) : NULL;
        if (t) {
            for (int i = 0; i < w * h; i++)
                argb[i] = ((uint32_t)bmp[i] << 24) | 0xFFFFFF;
            SDL_UpdateTexture(t, NULL, argb, w * 4);
            SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
            SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear); /* Glyphen liegen auf Teilpixeln */
        }
        free(argb);
        free(bmp);
        if (!t)
            return -1;
        if (g_atlas)
            SDL_DestroyTexture(g_atlas);
        g_atlas = t;
        g_atlas_w = w;
        g_atlas_h = h;
        int asc, desc, gap;
        stbtt_GetFontVMetrics(&g_font, &asc, &desc, &gap);
        float scale = stbtt_ScaleForMappingEmToPixels(&g_font, size);
        g_ascent = asc * scale;
        g_descent = desc * scale;
        if (g_check)
            SDL_DestroyTexture(g_check);
        if (g_dot)
            SDL_DestroyTexture(g_dot);
        g_check_px = (int)lrintf(14 * dpi);
        g_check = make_mark(g_check_px, 1.7f * dpi, 0);
        g_dot = make_mark(g_check_px, 1.0f, 1);
        g_baked_dpi = dpi;
        return 0;
    }
    return -1;
}

static float text_width(const char *s)
{
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    while (*s)
        stbtt_GetPackedQuad(g_pc, g_atlas_w, g_atlas_h, glyph_index(utf8_next(&s)), &x, &y, &q, 0);
    return x;
}

static void text_draw(const char *s, float x, float baseline, SDL_Color c)
{
    float y = baseline;
    stbtt_aligned_quad q;
    SDL_SetTextureColorMod(g_atlas, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(g_atlas, c.a);
    while (*s) {
        stbtt_GetPackedQuad(g_pc, g_atlas_w, g_atlas_h, glyph_index(utf8_next(&s)), &x, &y, &q, 0);
        SDL_Rect src = {(int)lrintf(q.s0 * g_atlas_w), (int)lrintf(q.t0 * g_atlas_h),
                        (int)lrintf((q.s1 - q.s0) * g_atlas_w), (int)lrintf((q.t1 - q.t0) * g_atlas_h)};
        SDL_FRect dst = {q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0};
        if (src.w > 0 && src.h > 0)
            SDL_RenderCopyF(g_ren, g_atlas, &src, &dst);
    }
}

/* ---- Zustand und Aufbau ---- */

static MenuState g_state = {-1, 0, MENU_GAME_NONE, 0, 0, 0}; /* zuletzt gezeichneter Zustand */
static int g_open = -1;     /* aufgeklapptes Menü der Leiste */
static int g_item = -1;     /* markierter Eintrag im Menü */
static int g_sub = -1;      /* Eintrag, dessen Untermenü offen ist */
static int g_sub_item = -1; /* markierter Eintrag im Untermenü */
static int g_hot = -1;      /* Titel unter dem Zeiger (ohne offenes Menü) */
static int g_pointer_in;    /* Zeiger über Leiste oder Menü */
static int g_alt_alone;     /* Alt gedrückt, ohne weitere Taste */
static int g_cmd;           /* gewählter Befehl */
enum { DLG_MAX_BUTTONS = 3 };
static char g_dlg_title[128], g_dlg_text[1200]; /* Dialog ("Über", Rückfragen) */
static char g_dlg_button[DLG_MAX_BUTTONS][32];
static int g_dlg, g_dlg_n, g_dlg_focus, g_dlg_result;
static int g_dlg_hot = -1, g_dlg_pressed = -1; /* Knopf unter dem Zeiger bzw. gedrückt */

enum { MAX_TITLES = 8, MAX_ITEMS = 16 };
typedef struct {
    float px;  /* Ausgabepixel je Fensterpunkt (Mauskoordinaten) */
    float dpi; /* Ausgabepixel je Maßeinheit (px mal Oberflächenskalierung) */
    SDL_FRect bar, title[MAX_TITLES];
    int n_titles;
    SDL_FRect menu, item[MAX_ITEMS]; /* aufgeklapptes Menü */
    int n_items;
    SDL_FRect sub, sub_item[MAX_ITEMS]; /* offenes Untermenü */
    int n_sub;
} Layout;

static int count(const MenuItem *m)
{
    int n = 0;
    while (m[n].text[0])
        n++;
    return n;
}

/* Oberflächenskalierung des Desktops. Unter X11 (auch XWayland) erwarten KDE und GNOME, dass
 * Programme sie selbst anwenden; sie steht als Xft.dpi in der X-Ressourcendatenbank (96 = 100 %).
 * libX11 ist dann schon geladen (SDL lädt sie per dlopen). MADTV_UI_SCALE ersetzt den Wert. */
float menubar_ui_scale(void)
{
    static float scale;
    if (scale > 0)
        return scale;
    if (!g_win)
        return 1; /* noch kein Fenster: erst danach feststellen */
    scale = 1;
    const char *env = getenv("MADTV_UI_SCALE");
    if (env && *env) {
        float v = strtof(env, NULL);
        if (v >= 0.5f && v <= 4)
            scale = v;
        return scale;
    }
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (!g_win || !SDL_GetWindowWMInfo(g_win, &info) || info.subsystem != SDL_SYSWM_X11)
        return scale; /* Wayland: SDL meldet die Skalierung über die Ausgabegröße */
    void *x11 = dlopen("libX11.so.6", RTLD_LAZY | RTLD_NOLOAD);
    char *(*resources)(void *) = x11 ? (char *(*)(void *))dlsym(x11, "XResourceManagerString") : NULL;
    const char *db = resources ? resources(info.info.x11.display) : NULL;
    const char *p = db ? strstr(db, "Xft.dpi:") : NULL;
    if (p) {
        float dpi = strtof(p + 8, NULL);
        if (dpi >= 48 && dpi <= 480)
            scale = dpi / 96;
    }
    if (x11)
        dlclose(x11);
    return scale;
}

/* Ausgabepixel je Fensterpunkt */
static float px_now(void)
{
    int ww = 0, wh = 0, ow = 0, oh = 0;
    SDL_GetWindowSize(g_win, &ww, &wh);
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    return ww > 0 && ow > 0 ? (float)ow / (float)ww : 1.0f;
}

static float dpi_now(void)
{
    return px_now() * menubar_ui_scale();
}

static int visible(void)
{
    return !g_state.fullscreen || g_open >= 0;
}

/* Menü items als Spalte ab (x, y) anlegen; rechts vom Fensterrand weg nach links ausweichen (alt_x) */
static void layout_menu(const MenuItem *items, float x, float y, float alt_x, float min_w, float dpi, float win_w,
                        SDL_FRect *menu, SDL_FRect *rects, int *n)
{
    float w = min_w > MIN_MENU_W * dpi ? min_w : MIN_MENU_W * dpi;
    *n = count(items);
    if (*n > MAX_ITEMS)
        *n = MAX_ITEMS;
    for (int i = 0; i < *n; i++) {
        float iw = (MARK_W + RIGHT_PAD) * dpi + text_width(menu_text(&items[i], g_state.lang));
        if (items[i].key)
            iw += KEY_GAP * dpi + text_width(items[i].key);
        if (items[i].sub)
            iw += ARROW_W * dpi;
        if (iw > w)
            w = iw;
    }
    w = ceilf(w);
    if (x + w > win_w)
        x = alt_x - w >= 0 ? alt_x - w : (win_w - w > 0 ? win_w - w : 0);
    float pad = roundf(MENU_PAD * dpi), cy = roundf(y) + pad;
    for (int i = 0; i < *n; i++) {
        float ih = roundf((items[i].cmd == MENU_SEPARATOR ? SEP_H : ITEM_H) * dpi);
        rects[i] = (SDL_FRect){roundf(x) + 1, cy, w - 2, ih};
        cy += ih;
    }
    *menu = (SDL_FRect){roundf(x), roundf(y), w, cy + pad - roundf(y)};
}

/* Eintrag kann markiert bzw. gewählt werden (keine Trennlinie, nicht gesperrt) */
static int selectable(const MenuItem *it)
{
    return it->cmd != MENU_SEPARATOR && (it->sub || menu_enabled(it->cmd, &g_state));
}

/* nächster wählbarer Eintrag ab from in Richtung step (Tastatur), -1 wenn keiner */
static int next_selectable(const MenuItem *items, int n, int from, int step)
{
    for (int k = 0; k < n; k++) {
        int i = ((from + step * k) % n + n) % n;
        if (selectable(&items[i]))
            return i;
    }
    return -1;
}

static void layout(Layout *L)
{
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    memset(L, 0, sizeof *L);
    L->px = px_now();
    L->dpi = L->px * menubar_ui_scale();
    bake(L->dpi);
    float d = L->dpi;
    L->bar = (SDL_FRect){0, 0, (float)ow, roundf(BAR_H * d)};
    float x = roundf(BAR_PAD * d);
    for (int i = 0; menu_bar[i].text[0] && i < MAX_TITLES; i++, L->n_titles++) {
        float w = roundf(2 * TITLE_PAD * d + text_width(menu_text(&menu_bar[i], g_state.lang)));
        L->title[i] = (SDL_FRect){x, 0, w, L->bar.h};
        x += w;
    }
    if (g_open < 0 || g_open >= L->n_titles)
        return;
    SDL_FRect *t = &L->title[g_open];
    layout_menu(menu_bar[g_open].sub, t->x, L->bar.h, t->x + t->w, 0, d, (float)ow, &L->menu, L->item, &L->n_items);
    if (g_sub >= 0 && g_sub < L->n_items && menu_bar[g_open].sub[g_sub].sub) {
        SDL_FRect *it = &L->item[g_sub];
        layout_menu(menu_bar[g_open].sub[g_sub].sub, L->menu.x + L->menu.w - 2, it->y - roundf(MENU_PAD * d),
                    L->menu.x + 2, 0, d, (float)ow, &L->sub, L->sub_item, &L->n_sub);
    }
}

static int inside(const SDL_FRect *r, float x, float y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static void close_menu(void)
{
    g_open = g_item = g_sub = g_sub_item = -1;
}

static void open_menu(int t, int keyboard)
{
    g_open = t;
    g_item = keyboard ? next_selectable(menu_bar[t].sub, count(menu_bar[t].sub), 0, 1) : -1;
    g_sub = g_sub_item = -1;
}

static void activate(const MenuItem *it, int in_sub, int idx)
{
    if (!selectable(it))
        return;
    if (it->sub) {
        g_sub = idx;
        g_sub_item = next_selectable(it->sub, count(it->sub), 0, 1);
    } else if (it->cmd != MENU_NONE) {
        g_cmd = it->cmd;
        close_menu();
    }
    (void)in_sub;
}

/* ---- öffentliche Funktionen ---- */

void menubar_init(SDL_Window *win, SDL_Renderer *ren)
{
    g_win = win;
    g_ren = ren;
}

void menubar_shutdown(void)
{
    if (g_atlas)
        SDL_DestroyTexture(g_atlas);
    if (g_check)
        SDL_DestroyTexture(g_check);
    if (g_dot)
        SDL_DestroyTexture(g_dot);
    if (g_icon)
        SDL_DestroyTexture(g_icon);
    g_atlas = g_check = g_dot = g_icon = NULL;
}

int menubar_height_points(void)
{
    return (int)lrintf(BAR_H * menubar_ui_scale());
}

int menubar_height_px(void)
{
    return visible() ? (int)lrintf(BAR_H * dpi_now()) : 0;
}

void menubar_set_fullscreen(int fullscreen)
{
    g_state.fullscreen = fullscreen;
    close_menu();
    g_pointer_in = 0;
}

int menubar_active(void)
{
    return g_open >= 0;
}

int menubar_wants_cursor(void)
{
    return g_dlg || (visible() && (g_pointer_in || g_open >= 0));
}

int menubar_take_command(void)
{
    int c = g_cmd;
    g_cmd = MENU_NONE;
    return c;
}

/* ---- Dialog "Über Mad TV Recompiled" (modal, im Fenster gezeichnet) ---- */

typedef struct {
    float dpi;
    SDL_FRect box, header, button[DLG_MAX_BUTTONS], icon;
    float text_x, text_y, text_w;
} DialogLayout;

#define DLG_W 470
#define DLG_PAD 20
#define DLG_ICON 48
#define DLG_KEYCOL 56

static float dialog_line_h(float dpi)
{
    return roundf(FONT_PX * dpi * 1.45f);
}

/* Text umbrechen und (draw = 1) zeichnen; Rückgabe: Höhe. Zeilen mit "\t" sind Taste und
 * Beschreibung in zwei Spalten, Leerzeilen trennen Absätze. */
static float dialog_text(float x, float y, float w, float dpi, int draw)
{
    float lh = dialog_line_h(dpi), y0 = y, space = text_width(" ");
    const char *p = g_dlg_text;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t n = eol ? (size_t)(eol - p) : strlen(p);
        char line[400];
        if (n >= sizeof line)
            n = sizeof line - 1;
        memcpy(line, p, n);
        line[n] = 0;
        p += n + (eol ? 1 : 0);
        if (!line[0]) { /* Absatz */
            y += roundf(lh * 0.6f);
            continue;
        }
        float base = y + roundf((lh - (g_ascent - g_descent)) / 2 + g_ascent);
        char *tab = strchr(line, '\t');
        if (tab) {
            *tab = 0;
            if (draw) {
                text_draw(line, x, base, COL_TEXT);
                text_draw(tab + 1, x + roundf(DLG_KEYCOL * dpi), base, COL_TEXT);
            }
            y += lh;
            continue;
        }
        /* Wörter umbrechen */
        float cx = 0;
        char *word = line;
        while (*word) {
            char *sp = strchr(word, ' ');
            if (sp)
                *sp = 0;
            float ww = text_width(word);
            if (cx > 0 && cx + ww > w) {
                y += lh;
                base += lh;
                cx = 0;
            }
            if (draw)
                text_draw(word, x + cx, base, COL_TEXT);
            cx += ww + space;
            if (!sp)
                break;
            word = sp + 1;
        }
        y += lh;
    }
    return y - y0;
}

static void dialog_layout(DialogLayout *A)
{
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    memset(A, 0, sizeof *A);
    float d = A->dpi = dpi_now();
    bake(d);
    float pad = roundf(DLG_PAD * d), w = roundf(DLG_W * d);
    if (w > ow - 2 * pad)
        w = ow - 2 * pad;
    float header_h = roundf(BAR_H * d * 1.15f), icon = roundf(DLG_ICON * d);
    A->text_x = pad + icon + roundf(16 * d);
    A->text_w = w - A->text_x - pad;
    float th = dialog_text(0, 0, A->text_w, d, 0);
    float body = th > icon ? th : icon;
    float bh = roundf(30 * d);
    float h = header_h + pad + body + pad + bh + pad;
    A->box = (SDL_FRect){roundf((ow - w) / 2), roundf((oh - h) / 2), w, h};
    if (A->box.y < 0)
        A->box.y = 0;
    A->header = (SDL_FRect){A->box.x, A->box.y, w, header_h};
    A->icon = (SDL_FRect){A->box.x + pad, A->box.y + header_h + pad, icon, icon};
    A->text_x += A->box.x;
    A->text_y = A->box.y + header_h + pad;
    float right = A->box.x + w - pad; /* Knöpfe rechtsbündig, in der angegebenen Reihenfolge */
    for (int i = g_dlg_n - 1; i >= 0; i--) {
        float bw = roundf(2 * 16 * d + text_width(g_dlg_button[i]));
        if (bw < roundf(88 * d))
            bw = roundf(88 * d);
        A->button[i] = (SDL_FRect){right - bw, A->box.y + h - pad - bh, bw, bh};
        right -= bw + roundf(8 * d);
    }
}

void menubar_set_icon(const uint8_t *rgba, int w, int h)
{
    if (!g_ren || g_icon)
        return;
    g_icon = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h); /* Bytes R,G,B,A */
    if (g_icon) {
        SDL_UpdateTexture(g_icon, NULL, rgba, w * 4);
        SDL_SetTextureBlendMode(g_icon, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(g_icon, SDL_ScaleModeLinear);
    }
}

void menubar_dialog_open(const char *title, const char *text, const char *const *buttons, int n)
{
    SDL_strlcpy(g_dlg_title, title, sizeof g_dlg_title);
    SDL_strlcpy(g_dlg_text, text, sizeof g_dlg_text);
    g_dlg_n = n < 1 ? 1 : n > DLG_MAX_BUTTONS ? DLG_MAX_BUTTONS : n;
    for (int i = 0; i < g_dlg_n; i++)
        SDL_strlcpy(g_dlg_button[i], n > 0 ? buttons[i] : "OK", sizeof g_dlg_button[i]);
    g_dlg = 1;
    g_dlg_focus = 0;
    g_dlg_result = -1;
    g_dlg_hot = g_dlg_pressed = -1;
    close_menu();
}

int menubar_dialog_active(void)
{
    return g_dlg;
}

void menubar_dialog_close(void)
{
    g_dlg = 0;
}

int menubar_dialog_result(void)
{
    return g_dlg_result;
}

static void dialog_finish(int button)
{
    g_dlg_result = button;
    g_dlg = 0;
}

static int dialog_event(const SDL_Event *e)
{
    DialogLayout A;
    switch (e->type) {
    case SDL_KEYDOWN:
        switch (e->key.keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_SPACE: dialog_finish(g_dlg_focus); break;
        case SDLK_ESCAPE: dialog_finish(-1); break;
        case SDLK_LEFT: g_dlg_focus = (g_dlg_focus + g_dlg_n - 1) % g_dlg_n; break;
        case SDLK_RIGHT:
        case SDLK_TAB: g_dlg_focus = (g_dlg_focus + 1) % g_dlg_n; break;
        default: break;
        }
        return 1;
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        dialog_layout(&A);
        float px = px_now();
        int mx = e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x;
        int my = e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y;
        g_dlg_hot = -1;
        for (int i = 0; i < g_dlg_n; i++)
            if (inside(&A.button[i], mx * px, my * px))
                g_dlg_hot = i;
        if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
            g_dlg_pressed = g_dlg_hot;
            if (g_dlg_hot >= 0)
                g_dlg_focus = g_dlg_hot;
        } else if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
            if (g_dlg_pressed >= 0 && g_dlg_pressed == g_dlg_hot)
                dialog_finish(g_dlg_hot);
            g_dlg_pressed = -1;
        }
        return 1;
    }
    case SDL_KEYUP:
    case SDL_MOUSEWHEEL:
    case SDL_TEXTINPUT: return 1;
    default: return 0;
    }
}

static int on_key(const SDL_KeyboardEvent *k)
{
    SDL_Keycode sym = k->keysym.sym;
    if (sym == SDLK_LALT || sym == SDLK_RALT) {
        if (k->type == SDL_KEYDOWN && !k->repeat)
            g_alt_alone = 1;
        else if (k->type == SDL_KEYUP && g_alt_alone) { /* Alt allein: Menü öffnen bzw. schließen */
            g_alt_alone = 0;
            if (g_open >= 0)
                close_menu();
            else
                open_menu(0, 1);
            return 1;
        }
        return g_open >= 0;
    }
    if (k->type == SDL_KEYDOWN)
        g_alt_alone = 0;
    if (g_open < 0)
        return 0;
    if (k->type != SDL_KEYDOWN)
        return 1;
    const MenuItem *menu = menu_bar[g_open].sub;
    int n = count(menu), n_titles = count(menu_bar);
    int in_sub = g_sub >= 0 && g_sub_item >= 0;
    const MenuItem *sub = g_sub >= 0 ? menu[g_sub].sub : NULL;
    int n_sub = sub ? count(sub) : 0;
    switch (sym) {
    case SDLK_DOWN:
    case SDLK_UP: {
        int step = sym == SDLK_DOWN ? 1 : -1;
        if (in_sub) {
            int i = next_selectable(sub, n_sub, g_sub_item + step, step);
            if (i >= 0)
                g_sub_item = i;
        } else {
            g_item = next_selectable(menu, n, g_item < 0 ? (step > 0 ? 0 : n - 1) : g_item + step, step);
            g_sub = -1;
        }
        break;
    }
    case SDLK_RIGHT:
        if (!in_sub && g_item >= 0 && menu[g_item].sub) {
            g_sub = g_item;
            g_sub_item = next_selectable(menu[g_item].sub, count(menu[g_item].sub), 0, 1);
        } else {
            open_menu((g_open + 1) % n_titles, 1);
        }
        break;
    case SDLK_LEFT:
        if (in_sub) {
            g_sub = g_sub_item = -1;
        } else {
            open_menu((g_open + n_titles - 1) % n_titles, 1);
        }
        break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:
        if (in_sub)
            activate(&sub[g_sub_item], 1, g_sub_item);
        else if (g_item >= 0)
            activate(&menu[g_item], 0, g_item);
        break;
    case SDLK_ESCAPE:
        if (g_sub >= 0)
            g_sub = g_sub_item = -1;
        else
            close_menu();
        break;
    default: break;
    }
    return 1;
}

int menubar_event(const SDL_Event *e)
{
    if (g_dlg)
        return dialog_event(e);
    switch (e->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: return on_key(&e->key);
    case SDL_WINDOWEVENT:
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST || e->window.event == SDL_WINDOWEVENT_LEAVE) {
            g_pointer_in = 0;
            g_hot = -1;
        }
        if (e->window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            g_alt_alone = 0;
            close_menu();
        }
        return 0;
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: break;
    case SDL_MOUSEWHEEL: return g_open >= 0;
    default: return 0;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN)
        g_alt_alone = 0;
    if (!visible()) {
        g_pointer_in = 0;
        return 0;
    }
    Layout L;
    layout(&L);
    int mx = e->type == SDL_MOUSEMOTION ? e->motion.x : e->button.x;
    int my = e->type == SDL_MOUSEMOTION ? e->motion.y : e->button.y;
    float x = (float)mx * L.px, y = (float)my * L.px;
    int title = -1, item = -1, sub_item = -1;
    for (int i = 0; i < L.n_titles; i++)
        if (inside(&L.title[i], x, y))
            title = i;
    if (g_open >= 0 && inside(&L.menu, x, y))
        for (int i = 0; i < L.n_items; i++)
            if (inside(&L.item[i], x, y))
                item = i;
    if (L.n_sub > 0 && inside(&L.sub, x, y))
        for (int i = 0; i < L.n_sub; i++)
            if (inside(&L.sub_item[i], x, y))
                sub_item = i;
    int over_menus = (g_open >= 0 && inside(&L.menu, x, y)) || (L.n_sub > 0 && inside(&L.sub, x, y));
    g_pointer_in = inside(&L.bar, x, y) || over_menus;
    const MenuItem *menu = g_open >= 0 ? menu_bar[g_open].sub : NULL;

    if (e->type == SDL_MOUSEMOTION) {
        g_hot = title;
        if (g_open >= 0) {
            if (title >= 0 && title != g_open)
                open_menu(title, 0); /* bei offenem Menü wechselt das Überfahren eines Titels das Menü */
            else if (sub_item >= 0)
                g_sub_item = selectable(&menu[g_sub].sub[sub_item]) ? sub_item : -1;
            else if (item >= 0) {
                g_item = selectable(&menu[item]) ? item : -1;
                g_sub = menu[item].sub ? item : -1;
                g_sub_item = -1;
            } /* außerhalb bleibt die Markierung (z. B. nach Öffnen mit Alt) */
            return 1;
        }
        return g_pointer_in;
    }
    if (e->button.button != SDL_BUTTON_LEFT)
        return g_open >= 0 || g_pointer_in;
    if (e->type == SDL_MOUSEBUTTONDOWN) {
        if (title >= 0) {
            if (g_open == title)
                close_menu();
            else
                open_menu(title, 0);
            return 1;
        }
        if (over_menus)
            return 1; /* ausgelöst wird beim Loslassen */
        if (g_open >= 0) { /* Klick daneben schließt nur das Menü */
            close_menu();
            g_pointer_in = inside(&L.bar, x, y);
            return 1;
        }
        return g_pointer_in;
    }
    /* Loslassen: auch nach Drücken auf einen Titel und Ziehen auf einen Eintrag */
    if (g_open >= 0) {
        if (sub_item >= 0)
            activate(&menu[g_sub].sub[sub_item], 1, sub_item);
        else if (item >= 0 && !menu[item].sub)
            activate(&menu[item], 0, item);
        return 1;
    }
    return g_pointer_in;
}

/* ---- Zeichnen ---- */

static void fill(const SDL_FRect *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(g_ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRectF(g_ren, r);
}

static void frame(const SDL_FRect *r, SDL_Color c, float dpi)
{
    float t = roundf(dpi) > 1 ? roundf(dpi) : 1;
    SDL_FRect e[4] = {{r->x, r->y, r->w, t}, {r->x, r->y + r->h - t, r->w, t}, {r->x, r->y, t, r->h},
                      {r->x + r->w - t, r->y, t, r->h}};
    SDL_SetRenderDrawColor(g_ren, c.r, c.g, c.b, c.a);
    SDL_RenderFillRectsF(g_ren, e, 4);
}

static void highlight(const SDL_FRect *r, float dpi)
{
    SDL_FRect h = {r->x + roundf(2 * dpi), r->y + roundf(1 * dpi), r->w - roundf(4 * dpi), r->h - roundf(2 * dpi)};
    fill(&h, COL_HOVER);
    frame(&h, COL_HOVER_FRAME, dpi);
}

static float baseline(const SDL_FRect *r)
{
    return roundf(r->y + (r->h - (g_ascent - g_descent)) / 2 + g_ascent);
}

static void draw_menu(const MenuItem *items, const SDL_FRect *menu, const SDL_FRect *rects, int n, int hot,
                      int open_sub, float dpi)
{
    for (int i = 3; i >= 1; i--) { /* weicher Schatten */
        float o = roundf(i * dpi);
        SDL_FRect s = {menu->x + o, menu->y + o, menu->w, menu->h};
        fill(&s, (SDL_Color){0, 0, 0, (Uint8)(14 * (4 - i))});
    }
    fill(menu, COL_MENU);
    frame(menu, COL_FRAME, dpi);
    for (int i = 0; i < n; i++) {
        const MenuItem *it = &items[i];
        const SDL_FRect *r = &rects[i];
        if (it->cmd == MENU_SEPARATOR) {
            float t = roundf(dpi) > 1 ? roundf(dpi) : 1;
            SDL_FRect line = {r->x + roundf(4 * dpi), roundf(r->y + (r->h - t) / 2), r->w - roundf(8 * dpi), t};
            fill(&line, COL_FRAME);
            continue;
        }
        int on = selectable(it);
        SDL_Color col = on ? COL_TEXT : COL_DISABLED;
        if ((i == hot || i == open_sub) && on)
            highlight(r, dpi);
        float base = baseline(r);
        if (it->mark != MENU_MARK_NONE && menu_checked(it->cmd, &g_state)) {
            float mx = r->x + roundf(MARK_W * dpi / 2);
            SDL_Texture *mark = it->mark == MENU_MARK_CHECK ? g_check : g_dot;
            if (mark) {
                SDL_SetTextureColorMod(mark, col.r, col.g, col.b);
                SDL_FRect c = {roundf(mx - g_check_px / 2.0f), roundf(r->y + (r->h - g_check_px) / 2), (float)g_check_px,
                               (float)g_check_px};
                SDL_RenderCopyF(g_ren, mark, NULL, &c);
            }
        }
        text_draw(menu_text(it, g_state.lang), r->x + roundf(MARK_W * dpi), base, col);
        float right = r->x + r->w - roundf(RIGHT_PAD * dpi);
        if (it->sub) {
            const char *arrow = "\xE2\x80\xBA"; /* › */
            text_draw(arrow, right - text_width(arrow), base, col);
        } else if (it->key) {
            text_draw(it->key, roundf(right - text_width(it->key)), base, COL_KEY);
        }
    }
}

static void dialog_draw(void)
{
    DialogLayout A;
    dialog_layout(&A);
    float d = A.dpi;
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(g_ren, &ow, &oh);
    SDL_FRect all = {0, 0, (float)ow, (float)oh};
    fill(&all, (SDL_Color){0, 0, 0, 96}); /* Spielbild abdunkeln */
    for (int i = 4; i >= 1; i--) {         /* weicher Schatten */
        float o = roundf(i * d);
        SDL_FRect sh = {A.box.x - o / 2, A.box.y, A.box.w + o, A.box.h + o * 1.5f};
        fill(&sh, (SDL_Color){0, 0, 0, (Uint8)(16 * (5 - i))});
    }
    fill(&A.box, COL_BAR);
    fill(&A.header, COL_MENU);
    SDL_FRect line = {A.header.x, A.header.y + A.header.h - roundf(d), A.header.w, roundf(d) > 1 ? roundf(d) : 1};
    fill(&line, COL_FRAME);
    frame(&A.box, COL_FRAME, d);
    text_draw(g_dlg_title, roundf(A.header.x + (A.header.w - text_width(g_dlg_title)) / 2), baseline(&A.header),
              COL_TEXT);
    if (g_icon)
        SDL_RenderCopyF(g_ren, g_icon, NULL, &A.icon);
    dialog_text(A.text_x, A.text_y, A.text_w, d, 1);
    /* Knöpfe; der mit dem Fokus (Enter) hat einen Rahmen in Akzentfarbe */
    for (int i = 0; i < g_dlg_n; i++) {
        const SDL_FRect *b = &A.button[i];
        fill(b, COL_MENU);
        if (i == g_dlg_hot)
            fill(b, COL_HOVER);
        frame(b, i == g_dlg_focus ? COL_HOVER_FRAME : COL_FRAME, d);
        text_draw(g_dlg_button[i], roundf(b->x + (b->w - text_width(g_dlg_button[i])) / 2), baseline(b), COL_TEXT);
    }
}

void menubar_draw(const MenuState *state)
{
    if (state->fullscreen != g_state.fullscreen)
        menubar_set_fullscreen(state->fullscreen);
    g_state = *state;
    if (!g_ren || (!visible() && !g_dlg))
        return;
    Layout L;
    layout(&L);
    if (!g_atlas)
        return;
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_BLEND);
    if (visible())
        fill(&L.bar, COL_BAR);
    for (int i = 0; i < L.n_titles && visible(); i++) {
        if (i == g_open || (g_open < 0 && i == g_hot && g_pointer_in))
            highlight(&L.title[i], L.dpi);
        text_draw(menu_text(&menu_bar[i], g_state.lang), L.title[i].x + roundf(TITLE_PAD * L.dpi), baseline(&L.title[i]),
                  COL_TEXT);
    }
    if (g_open >= 0) {
        draw_menu(menu_bar[g_open].sub, &L.menu, L.item, L.n_items, g_item, g_sub, L.dpi);
        if (L.n_sub > 0)
            draw_menu(menu_bar[g_open].sub[g_sub].sub, &L.sub, L.sub_item, L.n_sub, g_sub_item, -1, L.dpi);
    }
    if (g_dlg)
        dialog_draw(); /* über allem, Leiste eingeschlossen */
    SDL_SetRenderDrawBlendMode(g_ren, SDL_BLENDMODE_NONE);
}
