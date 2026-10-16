/* Segment 1054 – Hauptprogramm, Raumwechsel, Tagesablauf und Bildschirmübergänge */
#include <string.h>

#include "game.h"
#include "platform.h"

static void debug_room_jump(void);
static void debug_autoplan(void);
void save_menu_load(int16_t slot); /* seg_1581.c */

/* 1054:0005 – Hauptprogramm: Initialisierung, Intro, Laden der Daten, Hauptschleife */
int16_t g_opponent_stars;

void game_main(int argc, char **argv)
{
    int16_t click_handled = 0;

    /* 1. Argument d/D bzw. e/E wählt die Sprache (dann keine Abfrage),
     * 2. Argument: Anzahl führender "*" (höchstens 10) vervielfacht das Startkapital der
     * Computergegner (room_click_new_game_setup). PORT: Das Original liest das 2. Argument auch,
     * wenn es fehlt (Nullzeiger); hier gilt dann 0. */
    g_language = -1;
    if (argc > 1) {
        if (argv[1][0] == 'd' || argv[1][0] == 'D')
            g_language = 0;
        if (argv[1][0] == 'e' || argv[1][0] == 'E')
            g_language = 1;
        int16_t n = 0;
        while (argc > 2 && argv[2][n] == '*')
            n++;
        g_opponent_stars = n > 10 ? 10 : n;
    }

    far_memset(DS_ADDR(uint8_t, 0x79a6), 200);
    DS_I16(0x7a2e) = 3;
    g_game_speed_setting = 12;
    DS_I16(0x7a08) = 1;

    tc_atexit(kbd_shutdown);
    tc_atexit(timer_shutdown);
    tc_atexit(vga_restore_mode);
    tc_atexit(snd_shutdown);
    vga_detect();
    vga_set_mode13();
    vga_init_segs();
    kbd_init();
    kbd_flush();
    snd_init();
    mouse_init();
    mouse_setup();
    timer_init();
    hes_init_file_list();
    timer_resume();

    /* Grund-Sprites: Bank 3, Bank 2 mit einzelnen UI-Elementen */
    DS_SET_PTR(0x8366, hes_load_unpack(3));
    SpriteBank *ui = hes_load_unpack(2);
    DS_SET_PTR(0x8c3c, ui);
    DS_SET_PTR(0x8e78, &ui->bmp[0]);
    DS_SET_PTR(0x8e7c, &ui->bmp[1]);
    DS_SET_PTR(0x8e80, &ui->bmp[2]);
    DS_SET_PTR(0x7aea, &ui->bmp[3]);
    DS_SET_PTR(0x7aee, &ui->bmp[12]);

    /* Palette fürs Intro */
    DS_SET_PTR(0x8ab0, hes_load_raw(299));
    vga_set_palette(DS_PTR(uint8_t, 0x8ab0));
    far_free(DS_PTR(uint8_t, 0x8ab0));

    language_select();
    init_tables();
    g_local_player = 1;
    music_on_enter(500, 0);
    intro_play();
    music_on_leave(500, 0);

    /* Datenbanken: je Eintrag 0..Anzahl: Filme 23 Byte, Serien 57, Shows mit Folgen 435 */
    DS_SET_PTR(0x2489, far_calloc((uint16_t)(g_film_count + 1), 23));
    DS_SET_PTR(0x2626, far_calloc((uint16_t)(g_series_count + 1), 57));
    DS_SET_PTR(0x713c, far_calloc((uint16_t)(g_show_count + 1), 435));
    db_load_all();

    /* Spielpalette */
    DS_SET_PTR(0x8ab0, hes_load_raw(1));
    vga_set_palette(DS_PTR(uint8_t, 0x8ab0));
    far_free(DS_PTR(uint8_t, 0x8ab0));
    timer_pause();

    DS_SET_PTR(0x8c36, hes_load_unpack(6));
    DS_SET_PTR(0x7b12, hes_load_unpack(0xb));
    DS_SET_PTR(0x79a2, hes_load_unpack(0xf));
    DS_SET_PTR(0x8b7c, hes_load_unpack0(4));
    DS_SET_PTR(0x8b80, hes_load_unpack0(7));
    DS_SET_PTR(0x8362, hes_load_unpack(0xd));
    DS_SET_PTR(0x835e, hes_load_unpack(0x10));

    /* Spaltenzahl der Figurenbilder aus der Größe von Bank 4 ableiten */
    uint16_t bank4 = spritebank_size(DS_PTR(SpriteBank, 0x8b7c));
    DS_U16(0x836e) = (uint16_t)((uint16_t)((bank4 - 0x62) * 8) / 0x1a8u);
    DS_I16(0x836a) = DS_U16(0x836e) < 8;

    font_load(0xc, 2, 1);
    font_load(5, 0, 0);
    font_load(0xe, 1, 0);
    font_load(0xee, 0xb, 0);
    font_load(0xed, 10, 0);
    font_load(0xec, 9, 0);
    font_load(0xdc, 0xe, 0);

    /* Figuren-Animationsbank: 60 Bilder à 16 x 19 Pixel */
    DS_I16(0x8aae) = (int16_t)figure_frame_bytes(0x130, 1);
    int32_t bank_bytes = (int32_t)(uint16_t)(DS_I16(0x8aae) + 8) * 0x3c;
    SpriteBank *figures = far_calloc((uint16_t)(bank_bytes + 2), 1);
    DS_SET_PTR(0x00f8, figures);
    figures->count = 0x3c;
    uint8_t *px = (uint8_t *)&figures->bmp[0x3c];
    for (int16_t i = 0; i < 0x3c; i++) {
        figures->bmp[i].wcols = 2;
        figures->bmp[i].h = 0x13;
        figures->bmp[i].pixels = P32(px);
        px += (int16_t)figure_frame_bytes(0x130, 1);
    }
    figure_sprites_load_all();

    DS_SET_PTR(0x7c5e, hes_load_raw(0xa4)); /* Bevölkerungskarte */
    {
        /* PORT: Die KI (3b0a:46aa) liest/schreibt beide Karten mit doppelter Zeilenbreite über das
         * Pufferende hinaus (im DOS-Original trifft das den folgenden Heapblock). Puffer auffüllen. */
        uint8_t *padded = far_calloc(0x2400, 1);
        far_memcpy(padded, DS_PTR(uint8_t, 0x7c5e), 0x11e2);
        far_free(DS_PTR(uint8_t, 0x7c5e));
        DS_SET_PTR(0x7c5e, padded);
    }
    timer_resume();
    savegame_load(0x2b); /* Startzustand eines neuen Spiels */

    DS_I16(0x7bd6) = 0xa0;
    DS_I16(0x7bd8) = 100;
    DS_I16(0x8c2e) = 0;
    DS_I16(0x8bba) = 1;
    g_cur_room = 0;
    DS_I16(0x8e5c) = 2;
    g_player_idx = (int16_t)(g_local_player - 1);
    DS_I16(0x8b86) = 0;
    DS_I16(0x79b2) = 1;
    cursor_reset();

    DS_SET_PTR(0x8b8a, far_calloc(0x2400, 1)); /* PORT: 0x11e2 + Reserve, siehe oben */
    g_hour = 0x11;
    DS_SET_PTR(0x8c58, hes_load_unpack0(9));
    DS_SET_PTR(0x799e, hes_load_unpack(10));
    DS_SET_PTR(0x8eb2, bitmap_alloc(0x140, 0x50));
    panel_build();
    far_free(DS_PTR(SpriteBank, 0x799e));
    far_free(DS_PTR(SpriteBank, 0x8c58));
    lower_screen_present();
    far_free(DS_PTR(Bitmap, 0x8eb2));
    status_bar_draw();

    /* Offscreen-Bitmaps */
    DS_SET_PTR(0x8c30, bitmap_alloc(0x78, 100));
    /* PORT: die Zeigerpuffer DS:8e70/8ebe/8eb6 entfallen (Zeiger-Overlay, rtlib.c) */
    DS_SET_PTR(0x8e88, bitmap_alloc(0x30, 0xb));
    DS_SET_PTR(0x8e66, bitmap_alloc(0x30, 0xb));
    DS_SET_PTR(0x7bdc, bitmap_alloc(0x68, 0x3a));
    DS_SET_PTR(0x8e62, bitmap_alloc(0x140, 0x78));
    DS_SET_PTR(0x8e6a, bitmap_alloc(0x88, 0x38));
    DS_SET_PTR(0x8eba, bitmap_alloc(0xd0, 0x96));
    scr_grab(DS_PTR(Bitmap, 0x8e66), 0x13, 0x7f);
    status_show_start();
    DS_I16(0x8aac) = 0;

    /* Hauptschleife bis Raum 1000 (Spielende) */
    while (g_cur_room != 1000) {
        game_menu_poll(); /* PORT: vor dem Aufbau des Bildschirms "Neues Spiel" (nach Neues Spiel) */
        if (DS_I16(0x7c58) == 0) {
            /* erster Durchlauf: Bildschirm "Neues Spiel" */
            DS_I16(0x012c) = 2;
            room_enter(0x41);
            mouse_show();
            DS_I16(0x7c58) = 1;
        } else if (DS_I16(0x7c58) == 2) {
            /* nach dem Einrichten: Spiel beginnt beim Chef */
            DS_I16(0x012c) = 1;
            DS_I16(0x8c4a) = 1;
            room_reload();
            status_money_update();
            status_ratings_update();
            DS_I16(0x7a0c) = 1;
            player_walk_to_boss();
            DS_I16(0x7a0c) = 0;
            clock_suspend();
            clock_reset();
            DS_I16(0x012c) = 0;
            mouse_show();
            DS_I16(0x7c58) = 3;
        }
        if (g_cur_room == 0x41 && DS_I16(0x7c58) == 1 && platform_debug_env("MADTV_LOAD") > 0) {
            /* PORT: Testhilfe – Spielstand n direkt laden (wie "Load game" + Klick auf Slot n) */
            DS_SET_PTR(0x8ef9, DS_ADDR(uint8_t, 0x8bbc + g_local_player * 0x17));
            room_enter(0x3a);
            g_cur_dialog = 4;
            save_menu_load((int16_t)platform_debug_env("MADTV_LOAD"));
        }
        if (g_cur_room == 0x3a && DS_I16(0x7c58) == 1 && platform_debug_env("MADTV_NEWGAME"))
            room_leave(); /* PORT: Testhilfe – versehentlich geöffnetes Lademenü schließen (stoppt sonst die Uhr) */
        if (g_cur_room == 0x41 && DS_I16(0x7c58) == 1 && platform_debug_env("MADTV_NEWGAME")) {
            DS_I16(0x8ee3) = 0x4a; /* PORT: Testhilfe – "New game" direkt auslösen (wie room_click) */
            DS_SET_PTR(0x8ef9, DS_ADDR(uint8_t, 0x8bbc + g_local_player * 0x17));
            room_click_new_game_setup();
        }
        if (DS_I16(0x7c58) == 3 && DS_I16(0x012c) == 0 && g_cur_room >= 0)
            debug_room_jump();

        language_poll(); /* PORT: Sprachwechsel aus Fenstermenü/F10 */
        int16_t door = game_tick();
        if (door == 0) {
            /* Figur steht vor der Aufzugtafel -> Raum 0x3d */
            if (g_cur_room == 0 && DS_ADDR(int16_t, 0x7b30)[g_player_idx * 15] == 3 &&
                DS_ADDR(int16_t, 0x7b34)[g_player_idx * 15] == -2)
                room_enter(0x3d);
            int16_t now;
            if (g_cur_room == 0x3d && (clock_get_cached(&now), DS_I16(0x7a6c) + 900 < now)) {
                /* Aufzugtafel zu lange offen: zurück ins Gebäude */
                while (g_cur_room > 0)
                    room_leave();
            }
            if (g_mouse_buttons != 0 && !click_handled) {
                click_handled = 1;
                if (g_mouse_buttons == 2) {
                    if (g_cur_room != 0)
                        room_leave();
                } else if (g_mouse_buttons == 1) {
                    room_click();
                }
            }
            if (g_mouse_buttons == 0 && click_handled)
                click_handled = 0;
        } else {
            /* Figur hat eine Tür erreicht */
            DS_I16(0x79ca) = room_entry_check(g_player_idx, 0);
            if (DS_I16(0x79ca) == 0) {
                room_enter(door);
            } else {
                DS_ADDR(int16_t, 0x7b32)[g_player_idx * 15] = 2;
                DS_ADDR(int16_t, 0x7b34)[g_player_idx * 15] = -1;
            }
        }
    }

    mouse_hide();
    bmp_fill(DS_PTR(Bitmap, 0x8e62), 0, 0, 0x28, 0x78, 0);
    screen_transition(1, 2, 4);
}

/* 1054:076c (Release 1054:085e) – Tastenkürzel: Steuerung des Büro-Tetris, Figur umschalten (J),
 * Leertaste Pause */
void hotkey_handle(int16_t key)
{
    int16_t tetris = g_cur_room == 1 && g_cur_dialog == 2;
    SpriteBank *buttons = DS_PTR(SpriteBank, 0x0100);
    if (key == 0x22 && tetris) { /* ',' rechts drehen */
        DS_I16(0x7a62) = 2;
        draw_sprite((Bitmap *)((uint8_t *)buttons + 0x8a), 0xbd, 0x59);
    } else if (key == 0x23 && tetris) { /* '.' links drehen */
        DS_I16(0x7a62) = 1;
        draw_sprite((Bitmap *)((uint8_t *)buttons + 0x92), 0xca, 0x59);
    } else if (key == 0x3d && tetris) { /* 'X' nach links */
        DS_I16(0x7a62) = 3;
        draw_sprite((Bitmap *)((uint8_t *)buttons + 0x9a), 0xbd, 0x65);
    } else if (key == 0x28 && tetris) { /* 'C' nach rechts */
        DS_I16(0x7a62) = 4;
        draw_sprite((Bitmap *)((uint8_t *)buttons + 0xa2), 0xca, 0x65);
    } else {
        if (key == 0 && tetris) { /* F1: schneller */
            if (DS_I16(0x7a5e) - 1 > 0)
                DS_I16(0x7a5e)--;
            else
                DS_I16(0x7a5e) = 1;
            return;
        }
        if (key == 0x1f && tetris) { /* Enter: fallen lassen */
            DS_I16(0x7a62) = 5;
            return;
        }
        if (key == 0x2f) { /* 'J': Spielfigur umschalten */
            DS_ADDR(int16_t, 0x7b3c)[g_player_idx * 15] = DS_I16(0x7a14) != 0 ? 4 : 2;
            DS_I16(0x7a14) = DS_I16(0x7a14) == 0;
            figure_sprites_load_all();
            return;
        }
        /* Leertaste wie ein Klick auf das Uhrzeitfeld (Pause bzw. Pause beenden; room_click geht
         * mit gesetztem DS:7a7a direkt dorthin) */
        if (key == 0x20 && DS_I16(0x012c) == 0) {
            DS_I16(0x7a7a) = 1;
            room_click();
        }
        return;
    }
    screen_update();
}

/* Oberflächentexte der gewählten Sprache: 430 nullterminierte Strings hintereinander
 * (Teil von init_tables; PORT: eigene Funktion für den Sprachwechsel) */
static void ui_texts_load(void)
{
    int16_t text_hes = DS_ADDR(int16_t, 0x055e)[g_language * 8];
    DS_SET_PTR(0x8b8e, far_calloc((uint16_t)hes_filesize(text_hes), 1));
    hes_load(text_hes, DS_PTR(char, 0x8b8e));
    int16_t ofs = 0;
    for (int16_t i = 0; i < 0x1ae; i++) {
        char *t = DS_PTR(char, 0x8b8e) + ofs;
        DS_ADDR(ptr32, 0x8370)[i] = P32(t);
        ofs = (int16_t)(ofs + strlen(t) + 1);
    }
}

/* ---- PORT: Sprache während des Spiels wechseln (Fenstermenü oder F10) ----
 * Lädt alle sprachabhängigen Texte neu: Oberflächentexte, Werbekunden und Nachrichtenthemen,
 * die Text-Caches der Filme/Serien/Shows/Nachrichten und den Text-Datensatz des Raums
 * "Produktionen". Die alten Texte bleiben absichtlich im Speicher (wenige KB je Wechsel): gerade
 * angezeigte Kästen und Listen können noch Zeiger darauf halten und zeigen bis zum nächsten
 * Neuzeichnen die alte Sprache. Zeiger auf Oberflächentexte im Datensegment (z. B. in den
 * Spielerdaten) werden auf die neuen Texte umgestellt. Spielstände enthalten die Sprache nicht. */
static void language_switch(int16_t lang)
{
    static ptr32 old[0x1ae]; /* bisherige Tabelle (4-Byte-Zeiger) */
    memcpy(old, DS_ADDR(ptr32, 0x8370), sizeof old);
    g_language = lang;
    timer_pause();
    ui_texts_load();
    timer_resume();
    /* Zeiger auf alte Oberflächentexte im Datensegment umstellen (außer der Tabelle selbst) */
    for (uint32_t o = 0; o + sizeof(ptr32) <= DGROUP_SIZE; o++) {
        if (o >= 0x8370 && o < 0x8370 + sizeof old)
            continue;
        ptr32 p;
        memcpy(&p, g_ds + o, sizeof p);
        if (p == 0 || p < old[0] || p > old[0x1ad])
            continue;
        for (int16_t i = 0; i < 0x1ae; i++)
            if (p == old[i]) {
                memcpy(g_ds + o, &DS_ADDR(ptr32, 0x8370)[i], sizeof p);
                break;
            }
    }
    if (DS_PTR(char, 0x8aa8) != NULL) { /* Datenbanken geladen (nach dem Intro) */
        timer_pause();
        db_texts_load();
        timer_resume();
        text_cache_reload();
    }
    if (g_cur_room == 0xc && DS_PTR(uint8_t, 0x110) != NULL)
        DS_SET_PTR(0x110, hes_load_raw(g_language == 0 ? 0x2a : 0xf6));
    DS_I16(0x79ae) = 1; /* offenen Hinweiskasten bzw. Infokarte neu aufbauen (wie room_enter) */
    /* Raum ohne offenen Unterdialog neu aufbauen wie beim Betreten (ohne Raumwechsel, Musik und
     * Überblendung), damit fest gezeichnete Texte sofort in der neuen Sprache erscheinen.
     * Die Gebäudeansicht zeichnet sich ohnehin laufend neu. */
    if (g_cur_room > 0 && g_cur_dialog == 0) {
        mouse_hide();
        room_free_resources();
        room_load(g_cur_room);
        screen_update();
        mouse_show();
    }
}

/* Wunsch aus Fenstermenü/F10 übernehmen und als Einstellung speichern */
void language_poll(void)
{
    int lang = platform_language_request();
    if (lang < 0 || lang == g_language)
        return;
    platform_setting_set("language", lang);
    platform_language_show(lang);
    if (DS_PTR(char, 0x8b8e) == NULL) /* Texte noch nicht geladen (Spielstart) */
        g_language = (int16_t)lang;
    else
        language_switch((int16_t)lang);
}

/* PORT: "Neues Spiel" aus der Menüleiste: zurück in den Bildschirm "Neues Spiel" (Spielerwahl, Namen,
 * Schwierigkeit) wie nach dem Ausscheiden (player_eliminated_sequence). Offene Räume und Dialoge
 * werden nicht einzeln geschlossen: der Startzustand (43.HES) ersetzt die Spieldaten, room_enter
 * gibt die Raumressourcen frei, die Hauptschleife baut den Bildschirm auf. */
static void new_game_restart(void)
{
    if (DS_I16(0x79d6) != 0) { /* laufende Fernsehanimation beenden (wie beim Laden) */
        DS_I16(0x79d6) = 1;
        while (DS_I16(0x79d6) != 0)
            game_tick();
    }
    clock_stop();
    music_on_leave(1000, 0);
    text_cache_titles_free(0); /* Textpuffer der Räume (sonst beim Verlassen freigegeben) */
    text_cache_descr_free(0);
    text_cache_news_free();
    savegame_load(0x2b);
    /* wie vor der Hauptschleife in game_main */
    g_cur_room = 0;
    g_cur_dialog = 0;
    DS_I16(0x8e6e) = 0; /* Raumstapel */
    DS_I16(0x8c2e) = 0;
    DS_I16(0x8bba) = 1;
    DS_I16(0x8e5c) = 2;
    DS_I16(0x8b86) = 0;
    DS_I16(0x79b2) = 1; /* Zeigerform: Pfeil (set_cursor_shape(0) hieße "vorige Form vom Stapel") */
    DS_I16(0x8aac) = 0; /* Zeiger-Zähler */
    g_player_idx = (int16_t)(g_local_player - 1);
    cursor_reset();
    DS_I16(0x7c58) = 0;
    /* untere Leiste neu aufbauen wie beim Programmstart (Geld- und Quotenanzeige leer) */
    DS_SET_PTR(0x8c58, hes_load_unpack0(9));
    DS_SET_PTR(0x799e, hes_load_unpack(10));
    DS_SET_PTR(0x8eb2, bitmap_alloc(0x140, 0x50));
    panel_build();
    far_free(DS_PTR(SpriteBank, 0x799e));
    far_free(DS_PTR(SpriteBank, 0x8c58));
    lower_screen_present();
    far_free(DS_PTR(Bitmap, 0x8eb2));
    status_bar_draw();
    status_show_start();
}

/* PORT: Menüleiste: Zustand für Häkchen und gesperrte Einträge melden, Wünsche ausführen
 * ("Neues Spiel" nur bei laufendem Spiel, Spieltempo und Schalter wie im Control-Center) */
void game_menu_poll(void)
{
    int running = DS_I16(0x7c58) == 3 && DS_I16(0x012c) == 0;
    int paused = g_cur_room == -5;
    int switches = 0;
    for (int16_t i = 0; i < 3; i++)
        switches |= options_switch_on(i) << i;
    platform_game_show(running ? 2 : 1, DS_I16(0x7282), switches, paused);
    int pause = platform_pause_request(); /* Pause umschalten wie Klick auf Start */
    if (pause && paused)
        pause_end();
    else if (pause && running && pause_allowed())
        pause_begin();
    int speed = platform_speed_request();
    if (speed > 0)
        options_speed_set((int16_t)speed);
    int sw = platform_switch_request();
    if (sw >= 0 && (running || sw >> 1 == 2)) /* Fernseher erst im laufenden Spiel */
        options_switch_set((int16_t)(sw >> 1), (int16_t)(sw & 1));
    if (platform_new_game_request() && running)
        new_game_restart();
}

/* 1054:0927 – Grunddaten aus 208.HES auf die Tabellen verteilen, Oberflächentexte der
 * gewählten Sprache laden und Textzeiger aufbauen, Zähler nullen */
void init_tables(void)
{
    timer_pause();
    DS_SET_PTR(0x8f90, far_calloc(0x1a90, 1));
    DS_U16(0x11e2) = 0; /* Leseposition für config_read */
    hes_load(0xd0, DS_PTR(uint8_t, 0x8f90));
    config_read(DS_ADDR(void, 0x1ce0), 8);
    config_read(DS_ADDR(void, 0x6145), 0xd0);   /* Studios */
    config_read(DS_ADDR(void, 0x8bbc), 0x5c);   /* Spielerdaten */
    config_read(DS_ADDR(void, 0x4ff3), 0x540);
    config_read(DS_ADDR(void, 0x6215), 0x180);
    config_read(DS_ADDR(void, 0x266e), 0x60);
    config_read(DS_ADDR(void, 0x1ce9), 0x738);  /* Senderstandorte */
    config_read(DS_ADDR(void, 0x722a), 0x58);
    config_read(DS_ADDR(void, 0x7b22), 0xb4);   /* Figuren (6 x 30 Byte) */
    config_read(DS_ADDR(void, 0x7c62), 0x6ac);  /* Türen je Etage */
    config_read(DS_ADDR(void, 0x7140), 0x80);
    far_free(DS_PTR(uint8_t, 0x8f90));

    ui_texts_load();
    timer_resume();

    /* Namenspuffer mit Vorgaben aus den Texten */
    DS_SET_PTR(0x8bd3, far_calloc(0x29, 1));
    far_memcpy(DS_PTR(char, 0x8bd3), PTR32(char, DS_ADDR(ptr32, 0x8370)[0x3f]), 7);
    DS_U32(0x8bd7) = DS_ADDR(ptr32, 0x8370)[0x47];
    DS_SET_PTR(0x8bea, far_calloc(0x29, 1));
    far_memcpy(DS_PTR(char, 0x8bea), PTR32(char, DS_ADDR(ptr32, 0x8370)[0x42]), 6);
    DS_U32(0x8bee) = DS_ADDR(ptr32, 0x8370)[0x4c];
    DS_SET_PTR(0x8c01, far_calloc(0x29, 1));
    far_memcpy(DS_PTR(char, 0x8c01), PTR32(char, DS_ADDR(ptr32, 0x8370)[0x45]), 10);
    DS_U32(0x8c05) = DS_ADDR(ptr32, 0x8370)[0x91];

    far_memset(DS_ADDR(void, 0x1980), 0x4d);
    far_memset(DS_ADDR(void, 0x1b50), 0x118);
    far_memset(DS_ADDR(void, 0x7688), 8);
    far_memset(DS_ADDR(void, 0x729a), 0x90);
    far_memset(DS_ADDR(void, 0x63c5), 0x1ea);
    far_memset(DS_ADDR(void, 0x65af), 0x546);
    far_memset(DS_ADDR(void, 0x6af5), 0x294);
    DS_I16(0x7282) = 0x1e; /* Spielsekunden je Tick-Einheit */
}

/* 1054:0bd3 – nächste n Bytes aus dem Grunddaten-Puffer (208.HES) nach dst kopieren */
void config_read(void *dst, uint16_t n)
{
    far_memcpy(dst, DS_PTR(uint8_t, 0x8f90) + DS_U16(0x11e2), n);
    DS_U16(0x11e2) = (uint16_t)(DS_U16(0x11e2) + n);
}

/* Ressourcen-Slots des aktuellen Raums (Sprite-Bänke bzw. Bitmaps, Far-Pointer im Datensegment) */
#define ROOM_RES(ofs) DS_PTR(SpriteBank, ofs)
#define ROOM_BMP(ofs) DS_PTR(Bitmap, ofs)
#define g_room_stack  DS_ADDR(int16_t, 0x8a28)
#define g_room_depth  DS_I16(0x8e6e)
#define g_room_owner  DS_I16(0x7a16) /* Besitzer des Raums (Spieler 1..3, 0 = neutral) */

/* 1054:0bfc – zurück in den vorigen Raum (Raumstapel); beim Gebäude dieses neu aufbauen */
void room_reload(void)
{
    clock_stop();
    mouse_hide();
    int16_t old_room = g_cur_room;
    DS_I16(0x79f8) = 0;
    int16_t room = g_room_stack[g_room_depth];
    g_room_depth--;
    if (g_cur_room < 1) {
        int16_t was_overlay = g_cur_room < 0;
        g_cur_room = room;
        if (was_overlay)
            screen_update();
    } else {
        g_cur_room = room;
        room_animate(2);
        music_on_leave(g_cur_room, g_room_owner);
        room_free_resources();
        music_on_enter(g_cur_room, g_room_owner);
        room_load(g_cur_room);
        if (g_cur_room != 0)
            screen_transition(1, 1, 4);
    }
    if (g_cur_room == 0 && old_room > 0) {
        /* zurück im Gebäude: Gebäudegrafik neu aufbauen */
        DS_I16(0x7a6a) = 1;
        clock_suspend();
        DS_SET_PTR(0x8e6a, bitmap_alloc(0x88, 0x38));
        DS_SET_PTR(0x8eba, bitmap_alloc(0xd0, 0x96));
        DS_SET_PTR(0x00fc, font_get(2, 1));
        DS_SET_PTR(0x00f4, font_get(hour_index() >= 4, 0)); /* Tag/Nacht */
        clock_resume();
        DS_I16(0x8c4a) = 1;
        DS_I16(0x79ae) = 1;
        building_draw(DS_I16(0x8c2e));
        building_animate();
        screen_transition(1, 1, 4);
        game_tick();
        DS_I16(0x7a6a) = 0;
    }
    mouse_show();
    clock_start();
}

/* PORT: Testhilfe MADTV_AUTOPLAN – leere Stunden 1..7 des eigenen Planers reihum mit
 * Archivprogrammen füllen (Filme mit Fortsetzungsstunden, Serien mit der nächsten Folge) */
static void debug_autoplan(void)
{
    static int16_t next;
    if (!platform_debug_env("MADTV_AUTOPLAN"))
        return;
    int16_t p = g_local_player, list[64], n = 0;
    for (int16_t g = 1; g < 0xc; g++)
        for (int16_t k = 0; k < DS_I8(0x6f59 + p * 0x6c + g); k++) {
            int16_t id = DS_I16(0x6ef9 + p * 0x6c + g * 8 + k * 2);
            if (id != 0 && n < 64)
                list[n++] = g == 0xb ? (int16_t)(id + 1000) : id;
        }
    if (n == 0)
        return;
    for (int16_t h = 1; h < 8;) {
        int16_t *slot = DS_ADDR(int16_t, 0x43f7 + p * 0x5a + h * 2);
        if (*slot != 0) {
            h++;
            continue;
        }
        int16_t prog = list[next++ % n];
        int16_t len = prog < 1000 ? DS_PTR(int8_t, 0x2489)[prog * 0x17 + 4]
                                  : DS_PTR(int8_t, 0x2626)[(prog - 1000) * 0x39 + 0x23];
        if (len < 1)
            len = 1;
        if (h + len - 1 > 7) {
            h++;
            continue;
        }
        int16_t free_run = 1;
        for (int16_t k = 1; k < len; k++)
            if (slot[k] != 0)
                free_run = 0;
        if (!free_run) {
            h++;
            continue;
        }
        slot[0] = prog;
        DS_I16(0x4415 + p * 0x5a + h * 2) = 0;
        for (int16_t k = 1; k < len; k++)
            slot[k] = 9999;
        h = (int16_t)(h + len);
    }
}

/* PORT: Testhilfe – mit MADTV_ROOM=n nach Spielstart einmal in Raum n springen. Die Figur wird
 * dazu an eine passende Tür gesetzt (bevorzugt eine eigene), damit der Raumbesitzer stimmt. */
static void debug_room_jump(void)
{
    static int next;
    int room = platform_debug_room(next);
    if (room < 0)
        return;
    if (next++ == 0 && platform_debug_prog() > 0) /* Testhilfe: Programm für 18:00 */
        DS_I16(0x43f7 + g_local_player * 0x5a + 2) = (int16_t)platform_debug_prog();
    while (g_cur_room > 0) /* z.B. Chefgespräch am Spielanfang */
        room_leave();
    int16_t bf = -1, bd = -1, want = room, door_room = 0;
    for (int16_t f = 0; f < 14; f++)
        for (int16_t d = 0; d < 10; d++)
            if (DS_U8(0x7c80 + f * 0x7a + d) == room)
                door_room = 1;
    if (!door_room) /* Raum ohne eigene Tür (Planer, Senderkarte ...): über das eigene Büro */
        want = 1;
    for (int16_t f = 0; f < 14 && bd < 0; f++)
        for (int16_t d = 0; d < 10; d++)
            if (DS_U8(0x7c80 + f * 0x7a + d) == want) {
                if (bd < 0 || DS_U8(0x7ca8 + f * 0x7a + d) == g_local_player) {
                    bf = f;
                    bd = d;
                }
            }
    if (bd >= 0) {
        DS_ADDR(int16_t, 0x7b24)[g_player_idx * 15] = bf;
        DS_ADDR(int16_t, 0x7b34)[g_player_idx * 15] = bd;
    }
    room_enter((int16_t)room);
    platform_debug_log("MADTV_ROOM: Raum %d betreten bei %u ms", room, (unsigned)platform_ms());
}

/* 1054:0d49 – Raum betreten (aktuellen Raum auf den Stapel legen). Negative Räume sind
 * Einblendungen ohne Raumwechsel (-1 Meldung, -3 Info, -5 Sprachauswahl). */
void room_enter(int16_t room)
{
    clock_stop();
    mouse_hide();
    DS_I16(0x79f8) = 0;
    g_room_depth++;
    g_room_stack[g_room_depth] = g_cur_room;
    g_cur_room = room;
    if (room > 0) {
        g_room_owner = DS_U8(0x7ca8 + DS_ADDR(int16_t, 0x7b24)[g_player_idx * 15] * 0x7a +
                             DS_ADDR(int16_t, 0x7b34)[g_player_idx * 15]);
        music_on_leave(room, g_room_owner);
        room_free_resources();
        if (g_cur_room != 8)
            music_on_enter(g_cur_room, g_room_owner);
        room_load(g_cur_room);
        if (g_cur_room == 8)
            music_on_enter(8, g_room_owner);
        DS_I16(0x79ae) = 1;
        if (g_cur_room != 0x3f)
            screen_transition(2, 1, 4);
        if (g_cur_room == 0x3d)
            clock_get_cached(DS_ADDR(int16_t, 0x7a6c));
        DS_I16(0x7a18) = 0;
    }
    mouse_show();
    clock_start();
}

/* 1054:0e4c – Ressourcen eines Raums laden und den Raum aufbauen (Hintergrund, Sprite-Bänke
 * in den Slots DS:0100..0124, Texte), danach raumspezifisch zeichnen */
void room_load(int16_t room)
{
    DS_I32(0x8e5e) = 0;
    timer_pause();
    switch (room) {
    case 0x41: /* Neues Spiel */
        DS_SET_PTR((0x100), hes_load_unpack(0x96));
        break;
    case 1: /* Büro */
        room_load_background(0x68);
        DS_SET_PTR((0x100), hes_load_unpack(0xb5));
        break;
    case 0x3a: /* Laden/Speichern */
        room_load_background(0x74);
        DS_SET_PTR((0x104), hes_load_unpack(0xb6));
        break;
    case 0x1f: /* Programmplaner */
        DS_SET_PTR((0x100), hes_load_unpack(0x96));
        DS_SET_PTR((0x104), hes_load_unpack(0x9e));
        DS_SET_PTR((0x108), hes_load_unpack(0x98));
        DS_SET_PTR((0x10c), hes_load_unpack(0xaf));
        text_cache_titles(0x1f, g_room_owner);
        text_cache_descr(0x1f, g_room_owner);
        break;
    case 2: /* Nachrichtenstudio */
        room_load_background(0x6e);
        DS_SET_PTR((0x100), hes_load_unpack(0x96));
        DS_SET_PTR((0x104), hes_load_unpack(0xa2));
        DS_SET_PTR((0x108), hes_load_unpack(0xba));
        text_cache_news(g_room_owner);
        break;
    case 3: /* Chef */
        room_load_background(0x69);
        DS_SET_PTR((0x100), hes_load_unpack(0xb6));
        break;
    case 4: /* Filmagentur */
        room_load_background(0x65);
        DS_SET_PTR((0x104), hes_load_unpack(0x9d));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        DS_SET_PTR((0x10c), hes_load_unpack(0xa6));
        text_cache_titles(4, g_room_owner);
        text_cache_descr(4, g_room_owner);
        break;
    case 0xe: /* Betty */
        room_load_background(0x66);
        DS_SET_PTR((0x100), hes_load_unpack0(0x97));
        DS_SET_PTR((0x104), hes_load_unpack(0x9f));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        break;
    case 9: /* Drehbuchagentur */
        room_load_background(0x67);
        DS_SET_PTR((0x104), hes_load_unpack(0xa5));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        DS_SET_PTR((0x10c), hes_load_unpack(0xb9));
        text_cache_titles(9, 0);
        text_cache_descr(9, 0);
        text_cache_series(9, 0);
        break;
    case 8: { /* Auktion: Raumbild je nach Tageszeit */
        room_load_background(0x6a);
        static const int16_t by_hour[8] = {0xbc, 0xbc, 0xbd, 0xbd, 0xbe, 0xbe, 0xbf, 0xbf};
        uint16_t h = (uint16_t)hour_index();
        if (h < 8)
            DS_I16(0x7a06) = by_hour[h];
        DS_SET_PTR((0x110), hes_load_unpack(DS_I16(0x7a06)));
        room_variant_overlay_draw();
        far_free(ROOM_RES(0x110));
        DS_SET_PTR((0x110), hes_load_unpack(0xbb));
        DS_SET_PTR((0x100), hes_load_unpack(0x96));
        DS_SET_PTR((0x10c), hes_load_unpack(0x9b));
        DS_SET_PTR((0x104), hes_load_unpack(0x9d));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        text_cache_titles(8, 0);
        text_cache_descr(8, 0);
        break;
    }
    case 0xa: { /* Immobilien: Bild je nach Belegung DS:7a0e */
        room_load_background(0x6c);
        DS_SET_PTR((0x100), hes_load_unpack(0xc1));
        static const int16_t by_state[11] = {0xc2, 0xc2, 0xc2, 0xc3, 0xc3, 0xc4, 0xc4, 0xc4, 0xc6, 0xc6, 0xc6};
        if ((uint16_t)DS_I16(0x7a0e) < 0xb)
            DS_I16(0x7a32) = by_state[DS_I16(0x7a0e)];
        DS_SET_PTR((0x104), hes_load_unpack(DS_I16(0x7a32)));
        break;
    }
    case 0x3e:
        DS_SET_PTR((0x104), hes_load_unpack(0xa0));
        DS_SET_PTR((0x108), hes_load_unpack(0x9d));
        break;
    case 5: { /* Studio */
        DS_I16(0x79be) = DS_U8(0x7cd0 + DS_ADDR(int16_t, 0x7b24)[g_player_idx * 15] * 0x7a +
                               DS_ADDR(int16_t, 0x7b34)[g_player_idx * 15]);
        clip_reset();
        room_load_background((int16_t)(DS_I8(0x6147 + DS_I16(0x79be) * 0x10) + 0x70));
        DS_SET_PTR((0x100), hes_load_unpack(0xaa));
        DS_SET_PTR((0x104), hes_load_unpack0(0xa8));
        studio_cast_preview();
        far_free(ROOM_RES(0x104));
        DS_SET_PTR((0x104), hes_load_unpack(0x70));
        draw_sprite(&ROOM_RES(0x104)->bmp[0], 4, 4);
        far_free(ROOM_RES(0x104));
        DS_SET_PTR((0x104), hes_load_unpack(0x3e));
        draw_sprite(&ROOM_RES(0x104)->bmp[0], 0xa0, 4);
        far_free_ptr(DS_ADDR(ptr32, 0x104));
        DS_SET_PTR((0x110), hes_load_unpack(0xc0));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        DS_SET_PTR((0x10c), bitmap_alloc(0x10, 0x1d));
        DS_SET_PTR((0x11c), hes_load_unpack(0x9d));
        text_cache_titles(5, DS_I16(0x79be));
        text_cache_descr(5, DS_I16(0x79be));
        text_cache_series(5, DS_I16(0x79be));
        screen_grab(ROOM_BMP(0x10c), 0xf8, 0x53);
        break;
    }
    case 0xc: /* Supermarkt */
        room_load_background(0x6d);
        DS_SET_PTR((0x100), hes_load_unpack0(0xab));
        DS_SET_PTR((0x104), hes_load_unpack(0xae));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        DS_SET_PTR((0x10c), hes_load_unpack0(0x97));
        DS_SET_PTR((0x110), hes_load_unpack(0xc5));
        break;
    case 0x3f: /* Supermarkt-Regal */
        DS_SET_PTR((0x100), hes_load_unpack0(0xa7));
        DS_SET_PTR((0x10c), hes_load_unpack0(0xac));
        DS_SET_PTR((0x104), hes_load_unpack0(0xa8));
        DS_SET_PTR((0x108), hes_load_unpack(0xa9));
        text_cache_titles(0xc, 0);
        text_cache_descr(0xc, 0);
        text_cache_series(0xc, 0);
        DS_SET_PTR(0x110, hes_load_raw(g_language == 0 ? 0x2a : 0xf6));
        break;
    case 0xb: /* Werbeagentur */
        room_load_background(0x6f);
        DS_SET_PTR((0x104), hes_load_unpack(0x9c));
        DS_SET_PTR((0x108), hes_load_unpack(0x9a));
        break;
    case 0x19: /* Senderkarte */
        DS_SET_PTR((0x100), hes_load_unpack0(0xa3));
        DS_SET_PTR((0x104), hes_load_unpack(0x99));
        break;
    case 0xd:
    case 0x3d: /* Infotafel / Aufzugtafel */
        DS_SET_PTR((0x100), hes_load_unpack(0xa1));
        DS_SET_PTR((0x108), hes_load_unpack(0x98));
        break;
    case 0x16: room_load_background(0x75); break;
    case 0x17: room_load_background(0x76); break;
    case 0x11: room_load_background(0x77); break;
    case 0x10: room_load_background(0x78); break;
    case 0x18: room_load_background(0x79); break;
    case 0x15: room_load_background(0x7a); break;
    case 0x14: room_load_background(0x7b); break;
    case 0x40: room_load_background(0x7c); break;
    case 0x13: room_load_background(0x7f); break;
    case 0x12: room_load_background(0x7e); break;
    default: break;
    }
    timer_resume();

    /* raumspezifischer Aufbau */
    switch (g_cur_room) {
    case 0x41:
        bmp_fill(DS_PTR(Bitmap, 0x8e62), 0, 0, 0x28, 0x78, 0);
        room_frame_draw();
        player_setup_screen(0);
        break;
    case 0x1f:
        DS_SET_PTR((0x110), bitmap_alloc(0x60, 10));
        DS_SET_PTR(0x124, far_calloc(0x3a88, 1)); /* fester Puffer für Tooltips */
        g_cur_dialog = 0;
        DS_I16(0x79d8) = 0;
        DS_I16(0x79da) = g_local_player;
        DS_I16(0x7a4e) = 0;
        DS_I16(0x79a6) = 0;
        schedule_panel_open(g_room_owner);
        break;
    case 1:
        if (g_room_owner == g_local_player)
            room_stack_overlay_draw();
        room_owner_marker_draw();
        if (g_room_owner == g_local_player && DS_I16(0x7a04) >= 1) {
            DS_SET_PTR((0x114), bitmap_alloc(0x28, 0x10));
            DS_SET_PTR((0x118), bitmap_alloc(0x28, 0x10));
            screen_grab(ROOM_BMP(0x114), 0x78, 0x45);
        }
        break;
    case 0x19:
        DS_I16(0x79ac) = g_local_player;
        coverage_map_build(g_local_player);
        station_map_panel(0);
        break;
    case 8:
        DS_SET_PTR((0x114), bitmap_alloc(0x48, 0x35));
        DS_SET_PTR((0x118), bitmap_alloc(0x48, 0x35));
        screen_grab(ROOM_BMP(0x114), 0x10, 0x12);
        room8_items_panel(0);
        dialog_frame(0, 2);
        break;
    case 0xb:
        ad_offers_panel();
        dialog_frame(0, 3);
        break;
    case 4:
        DS_I16(0x79aa) = 0;
        g_cur_dialog = 0;
        programmes_mark_scheduled();
        if (g_room_owner == g_local_player)
            dialog_frame(0, 1);
        DS_SET_PTR((0x114), bitmap_alloc(0x38, 0x2d));
        screen_grab(ROOM_BMP(0x114), 0xe8, 0xc);
        break;
    case 0xd:
    case 0x3d:
        building_directory_panel();
        break;
    case 2:
        DS_SET_PTR((0x10c), bitmap_alloc(0x18, 10));
        DS_SET_PTR((0x110), bitmap_alloc(0x18, 10));
        screen_grab(ROOM_BMP(0x10c), 0xe8, 0x31);
        player_flags_draw(g_room_owner);
        DS_I16(0x7a76) = 0;
        break;
    case 9:
        DS_SET_PTR((0x114), bitmap_alloc(0x30, 0x28));
        DS_SET_PTR((0x118), bitmap_alloc(0x30, 0x28));
        DS_SET_PTR((0x11c), bitmap_alloc(0x18, 0xc));
        DS_SET_PTR((0x120), bitmap_alloc(0x20, 0x14));
        screen_grab(ROOM_BMP(0x114), 0xd0, 0x20);
        draw_sprite((Bitmap *)((uint8_t *)ROOM_RES(0x10c) + 0x12), 0xe8, 0x3c);
        draw_sprite((Bitmap *)((uint8_t *)ROOM_RES(0x10c) + 0x1a), 0xe8, 0x32);
        room_items_draw();
        production_offers_panel();
        dialog_frame(0, 4);
        break;
    case 0xe:
        player_ranking_panel();
        if (DS_U8(0x74c6 + g_local_player) != 0)
            dialog_frame(0, 7);
        if (DS_I16(0x8bc8 + g_local_player * 0x17) < 0x14 && player_flag(g_local_player, 0) != 0) {
            bubble_text_overlay(0, text_get_copy(0x15e));
            player_flag(g_local_player, 2);
        }
        break;
    case 3:
        DS_I16(0x7a6e) = 0;
        g_cur_dialog = DS_I16(0x7a12);
        if (DS_I16(0x7a12) == 1)
            room_click_boss();
        else
            boss_dialog(0, 0);
        break;
    case 0xa:
        room_crowd_draw();
        DS_SET_PTR((0x108), bitmap_alloc(0x20, 10));
        screen_grab(ROOM_BMP(0x108), 0xf8, 0x28);
        DS_SET_PTR((0x10c), bitmap_alloc(0x20, 10));
        break;
    case 0x3e:
        studio_booking_panel();
        break;
    case 5:
        DS_SET_PTR((0x114), bitmap_alloc(0x50, 0x53));
        DS_SET_PTR((0x118), bitmap_alloc(0x50, 0x53));
        screen_grab(ROOM_BMP(0x114), 0, 0x1e);
        studio_panel_draw();
        if (g_room_owner == g_local_player)
            dialog_frame(0, 5);
        break;
    case 0xc:
        DS_SET_PTR((0x114), bitmap_alloc(0x30, 0x5e));
        DS_SET_PTR((0x118), bitmap_alloc(0x30, 0x5e));
        screen_grab(ROOM_BMP(0x114), 0, 0x14);
        draw_sprite((Bitmap *)((uint8_t *)ROOM_RES(0x110) + 2), 0xc, 0x4a);
        draw_sprite((Bitmap *)((uint8_t *)ROOM_RES(0x110) + 10), 6, 0x15);
        break;
    case 0x3f:
        production_plan_panel();
        production_episode_panel();
        screen_update();
        g_cur_dialog = 2;
        break;
    case 0x3a:
        savegame_names_alloc();
        timer_pause();
        if (hes_filesize(0xd1) == -1) {
            timer_resume();
            if (disk_detect() == 1) {
                info_bubble_dialog(0);
                g_cur_dialog = 5;
            } else {
                yes_no_dialog(0);
                g_cur_dialog = 1;
            }
        } else {
            timer_resume();
            savegame_names_load();
            savegame_slots_draw();
            if (DS_I16(0x7c58) == 1) {
                g_cur_dialog = 4;
            } else {
                yes_no_dialog2(0);
                g_cur_dialog = 2;
            }
        }
        clock_stop();
        break;
    default:
        break;
    }
}

/* 1054:1bb9 – alle Raum-Ressourcen freigeben */
void room_free_resources(void)
{
    static const uint16_t slots[] = {0x8eba, 0x8e6a, 0x00fc, 0x00f4, 0x0100, 0x0104, 0x0108,
                                     0x010c, 0x0110, 0x0114, 0x0118, 0x011c, 0x0120, 0x0124};
    for (size_t i = 0; i < sizeof slots / sizeof *slots; i++)
        if (DS_PTR(void, slots[i]) != NULL)
            far_free_ptr(DS_ADDR(ptr32, slots[i]));
}

/* 1054:1ce0 – Raumhintergrund aus zwei Hälften: HES hes_no-50 rechts, hes_no links */
void room_load_background(int16_t hes_no)
{
    clip_reset();
    DS_SET_PTR(0x8e5e, hes_load_unpack0((int16_t)(hes_no - 0x32)));
    draw_bitmap_fast(&DS_PTR(SpriteBank, 0x8e5e)->bmp[0], 0xa0, 4);
    far_free(DS_PTR(SpriteBank, 0x8e5e));
    DS_SET_PTR(0x8e5e, hes_load_unpack0(hes_no));
    draw_bitmap_fast(&DS_PTR(SpriteBank, 0x8e5e)->bmp[0], 0, 4);
    far_free(DS_PTR(SpriteBank, 0x8e5e));
}

#define g_text(id)    PTR32(char, DS_ADDR(ptr32, 0x8370)[id])
#define FIGW(i, ofs)  DS_I16(0x7b22 + (i) * 0x1e + (ofs))

/* 1054:1d57 – neuer Tag: Abrechnungen, Märkte, Nachrichten, KI-Planung, Figuren an den Start */
void day_begin(void)
{
    special_film_check();
    auction_day_end();
    DS_I16(0x7470)++;
    DS_I16(0x79e6) = 0;
    contest_tick();
    if (g_day > 0) {
        news_subscriptions_check();
        ad_contracts_expire();
        market_daily_cleanup();
        licenses_expire();
        loan_interest_pay();
        stats_day_rotate();
        day_check_players();
        schedule_day_rotate();
    }
    finance_day_rotate();
    g_day++;
    debug_autoplan();
    for (int16_t dp = 1; dp < 4 && platform_debug_env("MADTV_DAYLOG"); dp++) { /* PORT: Testhilfe Tagesprotokoll */
        int16_t ns = 0;
        for (int16_t i = 1; i < 0x21; i++)
            if (DS_I32(0x1ce9 + dp * 0x1ce + i * 0xe + 4) != 0)
                ns++;
        int16_t na = 0;
        for (int16_t g = 1; g < 0xc; g++)
            na = (int16_t)(na + DS_I8(0x6f59 + dp * 0x6c + g));
        platform_debug_log("DAY %d p%d money=%ld img=%d stations=%d archive=%d contracts=%d", g_day, dp,
                           (long)DS_I32(0x8bc4 + dp * 0x17), DS_I16(0x8bd1 + dp * 0x17), ns, na, DS_I16(0x7688 + dp * 2));
    }
    market_update_prices();
    ad_market_update();
    production_market_update();
    actors_market_shuffle();
    event_npc_init();
    news_values_decay(0, 0); /* Nachrichten vom Vortag wertlos */
    news_update(5, 3);
    programmes_topicality_recover();
    if (g_day > 1)
        daily_costs_pay();
    info_board_build();
    ai_day_plan();
    daily_topic_pick();
    DS_I16(0x7a12) = 1;
    DS_I16(0x7a04)++;
    /* Spielerfiguren im Erdgeschoss, unterwegs zur eigenen Etage */
    for (int16_t i = 0; i < 3; i++) {
        if (DS_U8(0x8bd0 + (i + 1) * 0x17) == 0) {
            FIGW(i, 0x00) = 0x48;
            FIGW(i, 0x02) = 1;
            FIGW(i, 0x10) = 1;
            FIGW(i, 0x06) = (int16_t)(i * 3 + 5);
            FIGW(i, 0x12) = 2;
            FIGW(i, 0x04) = 0x99;
            FIGW(i, 0x0e) = 1;
            FIGW(i, 0x16) = 0;
            FIGW(i, 0x0c) = 1;
        }
    }
    figure_sprites_load_all();
    DS_I16(0x012a) = 0;
    DS_I16(0x0128) = 0;
    DS_I16(0x8bba) = 1;
    DS_I16(0x8c34) = 0;
}

/* 1054:1ea5 – Spieluhr fortschreiben: Tageswechsel (nach 0x7080 Sekunden), Stundenlogik,
 * Fernsehbild und Uhrzeitanzeige */
void clock_update(int16_t force_redraw)
{
    int16_t secs;
    if (g_cur_room == -5 || DS_I16(0x7a72) != 0)
        return;
    clock_get_seconds(&secs);
    if (secs > 0x707f) {
        /* Tag zu Ende: alle Räume verlassen, nach Hause, neuer Tag */
        DS_I16(0x79ca) = 0;
        tv_show_picture(0xdc);
        DS_I16(0x012c) = 1;
        status_text(g_text(0xf5), 0xb4);
        status_ratings_update();
        mouse_hide();
        while (g_cur_room != 0)
            room_leave();
        day_end_walk_home();
        g_hour = 0x11;
        day_begin();
        hour_start();
        building_animate();
        screen_transition(2, 2, 4);
        status_show_start();
        status_ratings_update();
        status_money_update();
        player_walk_to_boss();
        clock_suspend();
        clock_reset();
        clock_get_cached(&secs);
        DS_I16(0x012c) = 0;
        if (DS_I16(0x8aac) != 0 || DS_I16(0x79b2) != 1) {
            DS_I16(0x8aac) = 0;
            DS_I16(0x79b2) = 1;
        }
        mouse_show();
        force_redraw = 1;
    }
    if (broadcast_hour_tick() != 0) {
        clock_stop();
        if (g_minute > 0x36 && hour_index() > 0 && DS_I16(0x7a10) == 0)
            tv_anim_stop();
        DS_I16(0x79ae) = 1;
        DS_I16(0x7a40) = 1;
        if (DS_I16(0x79f0) == 0)
            channel_programme_title_load();
        tv_show_current_programme();
        if (g_minute > 4 && g_minute < 0x37 && DS_I16(0x7a10) == 0)
            tv_anim_setup(0);
        status_money_update();
        clock_start();
    }
    if (g_minute != DS_I16(0x059a) || force_redraw == 1) {
        char *s = fmt(DS_ADDR(char, 0x05b5), g_hour, g_minute); /* "#02d:#02d" */
        status_text(s, 0xb4);
        far_free(s);
    }
    DS_I16(0x059a) = g_minute;
    if (g_minute == DS_I16(0x059c))
        return;

    /* minutengenaue Aktualisierungen in einzelnen Räumen */
    if (g_cur_room == 0x1f) {
        int16_t d = g_cur_dialog;
        if ((d == 0 || d == 3 || d == 4 || d == 6) && (g_minute == 0 || g_minute == 5 || g_minute == 0x37))
            schedule_room_refresh();
        if ((d == 7 && g_minute == 5 && DS_I16(0x79d8) == 1)) {
            set_cursor_shape(3);
            text_cache_titles_free(0);
            text_cache_descr_free(0);
            text_cache_titles(100, DS_I16(0x79da));
            text_cache_descr(100, DS_I16(0x79da));
            set_cursor_shape(0);
            statistics_panel();
            DS_I16(0x79ae) = 1;
        } else if (d == 7 && g_minute == 0 && DS_I16(0x79d8) == 1) {
            statistics_panel();
            DS_I16(0x79ae) = 1;
        }
        if (d == 5 && g_minute > 0x36)
            ad_contract_list_dialog(0x16, 1, 2, DS_I16(0x7a16));
    }
    if (g_cur_room == 0xb && g_minute > 0x36) {
        dialog_frame(2, 3);
        DS_I16(0x79ae) = 1;
    }
    if (g_cur_room == 2 && g_minute == 0) {
        if ((g_cur_dialog == 1 || g_cur_dialog == 2) && DS_I16(0x7a16) == g_local_player) {
            player_tabs_panel(2);
        } else {
            if (g_cur_dialog != 0) {
                DS_I16(0x059c) = g_minute;
                return;
            }
            player_flags_draw(DS_I16(0x7a16));
        }
        screen_update();
    }
    DS_I16(0x059c) = g_minute;
}

/* 1054:21bf – leer */
void stub_21bf(void)
{
}

/* 1054:21c4 – Fach und Platz des ausgewählten Programms (DS:79b6) im Archiv suchen */
void archive_find_selected(void)
{
    int16_t sel = DS_I16(0x79b6), owner = DS_I16(0x7a16);
    if (sel < 1 || sel > 1999) {
        DS_I16(0x79aa) = 0;
        DS_I16(0x79a8) = 0;
    } else if (sel <= 1000) {
        for (int16_t g = 1; g < 0xb; g++) /* ohne Genre 11 (Serienregal) */
            for (int16_t k = 0; k < 4; k++)
                if (DS_I16(0x6ef9 + owner * 0x6c + g * 8 + k * 2) == sel) {
                    DS_I16(0x79a8) = k;
                    DS_I16(0x79aa) = g;
                }
    } else {
        for (int16_t k = 0; k < 4; k++)
            if (DS_I16(0x6f51 + owner * 0x6c + k * 2) == sel - 1000) {
                DS_I16(0x79aa) = 0xb;
                DS_I16(0x79a8) = k;
            }
    }
}

/* 1054:2276 */
void schedule_remove_hour(int16_t hour)
{
    schedule_clear_programme(DS_I16(0x7a16), hour);
    schedule_view(DS_I16(0x7a16));
    screen_update();
}

/* 1054:2298 – Programm ab Stunde hour aus dem Sendeplan nehmen (inkl. Folgestunden, 9999) */
void schedule_clear_programme(int16_t player, int16_t hour)
{
    do {
        DS_I16(0x43f7 + player * 0x5a + hour * 2) = 0;
        hour++;
    } while (DS_I16(0x43f7 + player * 0x5a + hour * 2) == 9999);
}

/* 1054:22de – 1, wenn der Sendeplatz unter dem Mauszeiger noch geändert werden darf */
int16_t schedule_hover_editable(void)
{
    int16_t row = (int16_t)((mouse_y() - 4) / 0x10 + 1);
    if (DS_I16(0x79a6) == 7)
        return 1;
    if (mouse_x() >= 0xd9 && mouse_x() <= 0x117) {
        if (row <= hour_index() && (hour_index() != row || g_minute > 0x36))
            return 0;
    } else if (mouse_x() > 0x37 && mouse_x() < 0xd8) {
        while (DS_I16(0x43f7 + DS_I16(0x7a16) * 0x5a + row * 2) == 9999)
            row--;
        if (row <= hour_index()) {
            if (hour_index() != row)
                return 0;
            if (g_minute > 4)
                return 0;
        }
    }
    return 1;
}

/* 1054:2386 – Raumübersicht (Infotafel) aufbauen: je Etage bis zu 4 Türen nach x-Spalte */
void info_board_build(void)
{
    static int16_t column; /* Original: Register SI bleibt bei unbekannter x-Position erhalten */
    far_memset(DS_ADDR(void, 0x8a40), 0x68);
    for (int16_t floor = 1; floor <= 0xd; floor++) {
        for (int16_t d = 0; d < 4 && DS_U8(0x7c76 + floor * 0x7a + d) != 0 && DS_U8(0x7c76 + floor * 0x7a + d) < 4; d++) {
            switch (DS_U8(0x7c62 + floor * 0x7a + d)) {
            case 80: column = 0; break;
            case 116: column = 1; break;
            case 180: column = 2; break;
            case 216: column = 3; break;
            default: break;
            }
            DS_U8(0x8a40 + column * 2 + (floor - 1) * 8) = (uint8_t)floor;
            DS_U8(0x8a41 + (floor - 1) * 8 + column * 2) = (uint8_t)d;
        }
    }
}

/* 1054:2490 – Raum bzw. Unterdialog verlassen (rechte Maustaste) */
void room_leave(void)
{
    int16_t me = g_player_idx;
    int16_t room = g_cur_room, d = g_cur_dialog;
    if (room == 0 || room == 0x41)
        return;
    if (room == -5) {
        /* Pause beenden */
        clock_suspend();
        DS_I32(0x6123) = DS_I32(0x728e);
        clock_resume();
        room_reload();
        clock_update(1);
        return;
    }
    if (room == 0x3a) {
        if (d == 1)
            yes_no_dialog(1);
        else if (d == 2)
            yes_no_dialog2(1);
        else if (d == 5)
            info_bubble_dialog(1);
        else if (DS_I16(0x7c58) != 1 || d != 4)
            goto subdialog;
        g_cur_dialog = 0;
    }
subdialog:
    d = g_cur_dialog;
    if (d != 0 && room >= 0) {
        /* offenen Unterdialog des Raums schließen */
        if (room == 0x3a) {
            if (d != 3 && d != 4)
                return;
            g_cur_dialog = 2;
            yes_no_dialog2(0);
            return;
        } else if (room == 1) {
            if (d != 1) {
                if (d == 2) {
                    minigame_panel(1);
                    g_cur_dialog = 0;
                }
                return;
            }
        } else if (room == 0x1f) {
            if (d != 1) {
                if (d == 2) {
                    archive_list_dialog(0, 0, 1, 0);
                    g_cur_dialog = 1;
                    return;
                }
                if (d != 3) {
                    if (d == 4) {
                        clip_reset();
                        draw_sprite((Bitmap *)((uint8_t *)DS_PTR(SpriteBank, 0x0100) + 0x102), 0xc, 0x5d);
                        g_cur_dialog = 0;
                        mouse_hide();
                        DS_I16(0x79b2) = 1;
                        mouse_show();
                        screen_update();
                        return;
                    }
                    if (d == 5) {
                        ad_contract_list_dialog(0, 0, 1, 0);
                        schedule_view(DS_I16(0x7a16));
                        g_cur_dialog = 0;
                        return;
                    }
                    if (d != 6) {
                        if (d == 7 || d == 9 || d == 10) {
                            if (d == 7) {
                                set_cursor_shape(3);
                                text_cache_titles_free(0);
                                text_cache_descr_free(0);
                                text_cache_titles(0x1f, DS_I16(0x7a16));
                                text_cache_descr(0x1f, DS_I16(0x7a16));
                                set_cursor_shape(0);
                            }
                        } else if (d != 8) {
                            return;
                        }
                        schedule_panel_open(DS_I16(0x7a16));
                        screen_update();
                        g_cur_dialog = 0;
                        return;
                    }
                }
                cursor_drop_item();
                g_cur_dialog = 0;
                return;
            }
            genre_menu_dialog(0, 0, 1);
            schedule_view(DS_I16(0x7a16));
        } else if (room == 3) {
            if (d == 1) {
                DS_I16(0x7a6e) = 0;
            } else {
                if (d != 2)
                    return;
                if (DS_I16(0x7a6e) > 1) {
                    DS_I16(0x7a6e) = 1;
                    DS_I16(0x7a70) = 1;
                    boss_dialog(2, -1);
                    screen_update();
                    return;
                }
                if (DS_I16(0x7a6e) != -1) {
                    boss_dialog(1, 0);
                    g_cur_dialog = 0;
                    room_reload();
                    if (g_cur_room == 0) {
                        FIGW(me, 0x12) = -1;
                        FIGW(me, 0x08) = FIGW(me, 0x04);
                        FIGW(me, 0x0c) = 0;
                    }
                    return;
                }
            }
            room_click_boss();
            return;
        } else if (room == 4) {
            if (d == 1) {
                genre_menu_dialog(0, 0, 1);
                g_cur_dialog = 0;
            } else if (d == 2) {
                if (DS_I16(0x79b2) == 2) {
                    mouse_hide();
                    DS_I16(0x79b2) = 1;
                    mouse_show();
                    g_cur_dialog = 0;
                } else {
                    series_episode_dialog(1);
                    DS_I16(0x79b4) = 0;
                    g_cur_dialog = 0;
                }
            } else if (d == 3) {
                archive_add_selected();
            } else if (d == 4) {
                dealer_offer_return();
            }
            return;
        } else if (room == 2) {
            if (d != 1) {
                if (d == 2) {
                    cursor_drop_item();
                    DS_I16(0x79ae) = 1;
                    g_cur_dialog = 1;
                }
                return;
            }
            news_slots_panel(DS_I16(0x7a16), 1);
            player_tabs_panel(1);
            player_flags_draw(DS_I16(0x7a16));
        } else if (room == 0xd) {
            /* Hinweistafel mit gehaltenem Schild verlassen: als leer markierte Plätze (Tür >= 100)
             * mit dem gehaltenen Schild füllen. PORT: Das Original nimmt als Etage DS:79ac (Etage des zuletzt getauschten
             * Schilds) statt DS:79a8 (Etage des gehaltenen) und konnte so ein falsches Schild
             * erzeugen. */
            if (d == 1) {
                for (int16_t r = 0; r <= 0xc; r++)
                    for (int16_t c = 0; c <= 4; c++) {
                        int8_t *cell = DS_ADDR(int8_t, 0x8a40 + r * 8 + c * 2);
                        if (cell[1] >= 100) {
                            cell[1] = (int8_t)DS_I16(0x79aa);
                            cell[0] = (int8_t)DS_I16(0x79a8);
                        }
                    }
                cursor_drop_item();
                building_directory_panel();
                screen_update();
                g_cur_dialog = 0;
            }
            return;
        } else if (room == 0x40) {
            if (d == 1) {
                bubble_text_dialog_small(1, NULL);
                g_cur_dialog = 0;
            }
            return;
        } else if (room == 0x19) {
            if (d == 1) {
                cursor_drop_item();
            } else if (d == 2) {
                station_map_view(DS_I16(0x79ac), 1);
                g_cur_dialog = 0;
                return;
            } else if (d == 3) {
                g_cur_dialog = 0;
                station_map_panel(0);
                screen_update();
                return;
            } else {
                if (d == 4) {
                    uint16_t st = (uint16_t)(g_local_player * 0x1ce + 0x1ce9 + DS_I16(0x79a8) * 0xe);
                    DS_I16(st + 6) = 0;
                    DS_I16(st + 4) = 0;
                    DS_I16(g_local_player * 0x1ce + DS_I16(0x79a8) * 0xe + 0x1cf5) = 2;
                    station_select_next();
                }
                return;
            }
        } else {
            switch (room) {
            case 8:
                if (d == 1) {
                    auction_withdraw_item();
                } else if (d == 4) {
                    auction_panel(1);
                    dialog_frame(0, 2);
                    screen_update();
                    g_cur_dialog = 0;
                    set_cursor_shape(3);
                    music_on_enter(g_cur_room, 0);
                    set_cursor_shape(0);
                } else if (d == 5) {
                    offer_list_add_selected();
                }
                return;
            case 0xb:
                if (d == 1)
                    adcontract_offer_reserve();
                else if (d == 2)
                    adcontract_sign();
                return;
            case 9:
                if (d == 1) {
                    screenplay_buy_selected();
                    return;
                }
                if (d == 2) {
                    screenplay_add_to_bag();
                    return;
                }
                if (d != 4)
                    return;
                speech_bubble_text52(1);
                break;
            case 0x3e:
                if (d != 1 && d != 2)
                    return;
                message_box2_draw(NULL, 1);
                g_cur_dialog = 0;
                return;
            case 5:
                if (d == 1) {
                    studio_screenplay_to_bag();
                    g_cur_dialog = 0;
                } else if (d == 2) {
                    studio_screenplay_insert();
                    g_cur_dialog = 0;
                } else if (d == 3) {
                    studio_production_dialog(1);
                    g_cur_dialog = 0;
                } else if (d == 4) {
                    speech_bubble(1, 0, 0, 0, 0, 0);
                    g_cur_dialog = 3;
                }
                return;
            case 0xc:
                if (d == 1) {
                    confirm_bubble_dialog(1);
                    g_cur_dialog = 0;
                } else if (d == 3) {
                    people_select_dialog(1);
                    dialog_frame(1, 6);
                    g_cur_dialog = 0;
                } else if (d == 4) {
                    supermarket_checkout();
                } else if (d == 5) {
                    supermarket_present_take();
                }
                return;
            case 0x3f:
                if (d != 1) {
                    if (d == 2) {
                        g_cur_dialog = 0;
                        shoplist_redraw();
                    }
                    return;
                }
                cursor_drop_item();
                break;
            case 0xe:
                if (d == 1) {
                    player_status_bubble(1);
                    g_cur_dialog = 0;
                } else if (d == 2) {
                    betty_present_give_close();
                    g_cur_dialog = 0;
                }
                return;
            default:
                return;
            }
            g_cur_dialog = 0;
            screen_update();
            return;
        }
        screen_update();
        g_cur_dialog = 0;
        return;
    }

    /* Raum verlassen */
    if (g_cur_room >= 0)
        room_animate(2);
    if (g_cur_room == -4) {
        message_box_draw(1, NULL);
        room_reload();
    }
    if (g_cur_room == -3) {
        message_box_draw(1, NULL);
        room_reload();
        goto done;
    }
    if (g_cur_room == -6) {
        bubble_text_overlay(1, NULL);
        goto done;
    }
    if (g_cur_room == 0x1f) {
        clip_reset();
        bmp_fill(DS_PTR(Bitmap, 0x8e62), 0, 0, 0x28, 0x78, 0);
        room_frame_draw();
        text_cache_titles_free(0);
        text_cache_descr_free(0);
    } else if (g_cur_room == 0x3d) {
        FIGW(me, 0x12) = -1;
        FIGW(me, 0x06) = FIGW(me, 0x02);
        goto done;
    } else if (g_cur_room == 8) {
        dialog_frame(1, 2);
        auction_assign_unbid();
        market_update_prices();
        text_cache_titles_free(0);
        text_cache_descr_free(0);
        if (DS_I16(0x732a) > 0)
            case_store_to_archive();
        goto done;
    } else if (g_cur_room == 0x19) {
        stations_settle(g_local_player);
        goto done;
    } else if (g_cur_room == 0xb) {
        adcontract_offers_commit();
        goto done;
    } else if (g_cur_room == 9 || g_cur_room == 5 || g_cur_room == 0x3f) {
        if (g_cur_room == 9)
            screenplay_offers_commit();
        else if (g_cur_room == 5 && DS_I16(0x7a16) == g_local_player)
            dialog_frame(1, 5);
        text_cache_titles_free(0);
        text_cache_descr_free(0);
        text_cache_series_free();
    } else if (g_cur_room == 4) {
        if (DS_I16(0x7a16) == g_local_player)
            dialog_frame(1, 1);
        text_cache_titles_free(0);
        text_cache_descr_free(0);
    } else if (g_cur_room == 0xc) {
        production_costs_update(g_local_player);
        goto done;
    } else {
        if (g_cur_room == 0xe && DS_U8(0x74c6 + g_local_player) != 0) {
            dialog_frame(1, 7);
        } else if (g_cur_room == 2) {
            text_cache_news_free();
            newscast_rate(DS_I16(0x7a16));
            if (DS_I16(0x7a16) == g_local_player && DS_I16(0x7a76) != 0) {
                /* Hinweis nach der Nachrichtensendung */
                speech_bubble(0, 2, 2, 0x10, 7, 1);
                text_box(0x14, 0x14, g_text(0xf7), 0xd, 1, DS_PTR(Bitmap, 0x8e62), -1);
                screen_update();
                room_enter(-3);
                DS_I16(0x057e) = 1;
            }
        } else if (g_cur_room == 0x3a) {
            for (int16_t i = 0; i < 10; i++)
                far_free(PTR32(char, DS_ADDR(ptr32, 0x8b92)[i]));
            clock_start();
        } else if (g_cur_room == -1) {
            message_box_draw(1, NULL);
        }
        goto done;
    }
done:
    if (DS_I16(0x057e) == 0) {
        room_reload();
        if (g_cur_room == 0) {
            FIGW(me, 0x12) = -1;
            FIGW(me, 0x08) = FIGW(me, 0x04);
            FIGW(me, 0x0c) = 0;
        }
    } else {
        DS_I16(0x057e) = 0;
    }
    if (DS_I16(0x7c58) != 1)
        status_money_update();
}

/* 1054:2e1c – Tagesende: zum Erdgeschoss, Bildschirm leeren */
void day_end_walk_home(void)
{
    player_walk_to_exit();
    bmp_fill(DS_PTR(Bitmap, 0x8e62), 0, 0, 0x28, 0x78, 0);
    screen_transition(1, 2, 4);
    room_frame_draw();
    g_hour = 0x11;
}

/* 1054:2e60 – eigene Figur ins Erdgeschoss und zum Ausgang laufen lassen */
void player_walk_to_exit(void)
{
    int16_t me = g_player_idx;
    while (g_cur_room != 0)
        room_leave();
    FIGW(me, 0x06) = 1;
    FIGW(me, 0x12) = -1;
    if (FIGW(me, 0x0e) != 3 && FIGW(me, 0x16) == 0) {
        FIGW(me, 0x04) = 0x99;
        FIGW(me, 0x0e) = 1;
    }
    while (FIGW(me, 0x02) != 1 || FIGW(me, 0x0e) != 0)
        game_tick();
    FIGW(me, 0x04) = 0x48;
    FIGW(me, 0x0e) = 1;
    while (FIGW(me, 0x0e) != 0)
        game_tick();
}

/* 1054:2efb – eigene Figur zum Chef laufen lassen und eintreten; die anderen an ihr Ziel setzen */
void player_walk_to_boss(void)
{
    int16_t me = g_player_idx;
    FIGW(me, 0x12) = 2;
    FIGW(me, 0x06) = (int16_t)(g_local_player * 3 + 2);
    FIGW(me, 0x04) = 0x99;
    FIGW(me, 0x0e) = 1;
    while (FIGW(me, 0x10) != 0)
        game_tick();
    room_enter(3);
    for (int16_t i = 0; i < 3; i++) {
        if (i != g_player_idx) {
            FIGW(i, 0x00) = FIGW(i, 0x04);
            FIGW(i, 0x02) = FIGW(i, 0x06);
            FIGW(i, 0x0e) = 0;
            FIGW(i, 0x16) = 0;
            FIGW(i, 0x10) = 0;
        }
    }
    DS_I16(0x8c34) = 0;
}

/* 1054:2fa3 – Übergangseffekt für die Spielszene (nur Effekt 4 = Ringwischen) */
void screen_transition(int16_t dir, int16_t delay, int16_t effect)
{
    clip_reset();
    if (effect == 4) {
        clip_reset();
        screen_wipe_in(DS_PTR(Bitmap, 0x8e62), 0, 0, 0x140, 0x78, dir, delay);
    }
}

/* 1054:2fd9 – src ringweise auf den Bildschirm bringen: dir 1 von außen nach innen, sonst
 * von innen nach außen. Das Original zeichnet w/16 Ringe (je 8 Pixel breit und h/(w/8) Zeilen
 * hoch) und wartet vor jedem Ring delay Takte.
 * PORT: Statt in 8-Pixel-Ringen im 50-Hz-Takt wächst die aufgedeckte Fläche bei jedem
 * angezeigten Bild pixelgenau mit der vergangenen Zeit. Zu den Zeitpunkten, an denen das
 * Original einen Ring fertig hatte, ist exakt dieselbe Fläche sichtbar; Gesamtdauer
 * (w/16 * delay Takte) und Form bleiben gleich. */
void screen_wipe_in(Bitmap *src, int16_t x, int16_t y, int16_t w, int16_t h, int16_t dir, int16_t delay)
{
    uint32_t total = (uint32_t)(w >> 4) * (uint32_t)(delay > 0 ? delay : 0) * 256; /* in 1/256 Takt */
    uint32_t start = ticks_fine();
    for (;;) {
        uint32_t elapsed = ticks_fine() - start;
        int done = elapsed >= total;
        /* aufgedeckte Breite je Seite (0..w/2) und Höhe je Seite im Verhältnis h/w wie im Original */
        int px = done ? w / 2 : (int)((uint64_t)elapsed * (uint32_t)(w / 2) / total);
        int py = done ? h / 2 : px * h / w;
        if (done) {
            scr_blit_px(src, 0, 0, w, h, x, y);
            break;
        }
        if (dir == 1) { /* Rand wächst nach innen */
            for (int r = 0; r < h; r++) {
                if (r < py || r >= h - py) {
                    scr_blit_px(src, 0, r, w, 1, x, y + r);
                } else if (px > 0) {
                    scr_blit_px(src, 0, r, px, 1, x, y + r);
                    scr_blit_px(src, w - px, r, px, 1, x + w - px, y + r);
                }
            }
        } else if (px > 0) { /* Mitte wächst nach außen */
            scr_blit_px(src, w / 2 - px, h / 2 - py, 2 * px, 2 * py, x + w / 2 - px, y + h / 2 - py);
        }
        platform_wait_frame();
    }
}

/* 1054:320b – "Start" in der Statuszeile anzeigen */
void status_show_start(void)
{
    char *s = text_get_copy(0xf6);
    status_text(s, 0xb4);
    far_free(s);
}

/* 1054:3240 – Fernsehbild für das laufende Programm des gewählten Senders */
void tv_show_current_programme(void)
{
    int16_t pic;
    if (hour_index() < 1) {
        pic = 0xdc; /* Sendeschluss */
    } else if (g_minute >= 0 && g_minute <= 4) {
        pic = 0xec; /* Werbung */
    } else if (g_minute < 0x37) {
        int16_t prog = schedule_get_programme(DS_I16(0x79de), hour_index());
        if (prog == 0) {
            pic = 0xee;
        } else if (prog < 1000) {
            uint8_t *film = DS_PTR(uint8_t, 0x2489) + prog * 0x17;
            int16_t own = *(int16_t *)(film + 0xc);
            pic = own == 0 ? (int16_t)((int8_t)film[0] + 0xdc) : own;
        } else {
            uint8_t *series = DS_PTR(uint8_t, 0x2626) + (prog - 1000) * 0x39;
            int16_t own = *(int16_t *)(series + 0x2f);
            pic = own == 0 ? (int16_t)((int8_t)series[0x21] + 0xe6) : own;
        }
    } else {
        pic = DS_I16(0x4433 + DS_I16(0x79de) * 0x5a + hour_index() * 2) != 0 ? 0xed : 0xee; /* Nachrichten */
    }
    if (pic != 0)
        tv_show_picture(pic);
}

/* 1054:3357 – Bild pic_id im kleinen Fernseher der unteren Leiste zeigen (einige aus dem Cache) */
void tv_show_picture(int16_t pic_id)
{
    if ((DS_I16(0x79f0) == 0 || pic_id == 0xdc) && (pic_id != DS_I16(0x05ae) || pic_id == 0xee)) {
        int16_t i;
        for (i = 0; i < 4 && DS_I16(0x59e + i * 4) != pic_id; i++)
            ;
        SpriteBank *pic;
        if (i < 4) {
            set_cursor_shape(3);
            timer_pause();
            pic = font_get(DS_I16(0x5a0 + i * 4), 0);
            timer_resume();
            set_cursor_shape(0);
        } else {
            timer_pause();
            set_cursor_shape(3);
            pic = hes_load_unpack0(pic_id);
            set_cursor_shape(0);
            timer_resume();
        }
        clip_set_upper();
        bmp_draw_masked(&DS_PTR(SpriteBank, 0x79a2)->bmp[0], &pic->bmp[0], 5, 5); /* Rahmen */
        if (pic_id == 0xee && DS_I16(0x79de) > 1) /* Testbild mit Senderlogo */
            bmp_copy_at((Bitmap *)((uint8_t *)pic + sprite_ofs(DS_I16(0x79de))), &pic->bmp[0], 0x10, 0x1b);
        mouse_hide();
        scr_blit(&pic->bmp[0], 1, 0x7c);
        far_free(pic);
        mouse_show();
        clip_reset();
    }
    DS_I16(0x05ae) = pic_id;
}

/* 1054:34c2 – animierte Fernsehszene (bis zu drei von fünf Ebenen) für die laufende Sendung */
void tv_anim_setup(int16_t force)
{
    int16_t layers = 0, genre = 0, x;
    if (DS_I16(0x7a10) != 0)
        return;
    set_cursor_shape(3);
    int16_t reload = !(DS_I16(0x43f7 + g_local_player * 0x5a + hour_index() * 2) == 9999 && force == 0);
    uint8_t *flags = DS_ADDR(uint8_t, 0x05b0); /* welche Ebenen die Sendung zeigt */
    if (DS_I16(0x43f7 + g_local_player * 0x5a + hour_index() * 2) == 0) {
        reload = 0;
        far_memset(flags, 5);
    } else {
        int16_t prog = schedule_get_programme(g_local_player, hour_index());
        uint16_t bits;
        if (prog < 1000) {
            uint8_t *film = DS_PTR(uint8_t, 0x2489) + prog * 0x17;
            bits = *(uint16_t *)(film + 6);
            genre = (int8_t)film[0];
        } else {
            uint8_t *series = DS_PTR(uint8_t, 0x2626) + (prog - 1000) * 0x39;
            bits = *(uint16_t *)(series + 0x25);
            genre = (int16_t)((int8_t)series[0x21] + 10);
        }
        flags[0] = (bits >> 1) & 1 ? 0xff : 0;
        flags[1] = bits & 1 ? 0xff : 0;
        flags[2] = (bits >> 2) & 1 ? 0xff : 0;
        flags[3] = (bits >> 4) & 1 ? 0xff : 0;
        flags[4] = (bits >> 3) & 1 ? 0xff : 0;
        if (hour_index() > 5)
            flags[4] = 0;
    }
    if (flags[1] == 0) {
        x = 0xd;
    } else {
        DS_I16(0x8322) = 6;
        x = 0x22;
    }
    if (reload)
        DS_I16(0x8c3a) = 0;
    tv_picture_bg_draw();
    int16_t audience = DS_I16(0x79f4);
    for (int16_t i = 0; i < 5; i++) {
        uint16_t layer = (uint16_t)(0x830e + i * 0x10);
        if (flags[i] == 0 || layers > 2) {
            DS_I16(layer + 8) = 0; /* Ebene aus */
            continue;
        }
        if (i != 1)
            DS_I16(layer + 4) = x;
        switch (i) {
        case 0: /* Moderator */
            x += 0x1a;
            DS_I16(0x8316) = 1;
            if (rand_range(5, 0) == 1)
                DS_I16(0x8318)++;
            if (DS_I16(0x8318) > 7)
                DS_I16(0x8318) = 1;
            DS_I16(0x831a) = 1;
            if (audience < 0xb)
                DS_I16(0x8314) = 4;
            else if (audience < 0x15)
                DS_I16(0x8314) = 5;
            else if (audience < 0x1f)
                DS_I16(0x8314) = 0;
            else if (genre == 4)
                DS_I16(0x8314) = 6;
            else if (genre == 5)
                DS_I16(0x8314) = 2;
            else
                DS_I16(0x8314) = 3;
            if (reload)
                DS_SET_PTR(0x830e, sprite_pool_load(0xb0));
            tv_layer0_draw();
            break;
        case 1:
            if (hour_index() < 5 || audience > 0x1e) {
                DS_I16(0x8326) = 1;
                DS_I16(0x8328) = 1;
                DS_I16(0x8324) = audience < 0xb ? 4 : audience < 0x15 ? 2 : audience < 0x1f ? 1 : 5;
            } else if (DS_I16(0x8326) != 2) {
                DS_I16(0x8326) = 2;
                DS_I16(0x8328) = 1;
            }
            if (reload)
                DS_SET_PTR(0x831e, sprite_pool_load(0xb4));
            tv_layer1_draw();
            break;
        case 2:
            x += 0x1a;
            if (hour_index() < 4 || audience > 5) {
                if (audience < 8) {
                    DS_I16(0x8336) = 2;
                } else {
                    DS_I16(0x8336) = 1;
                    DS_I16(0x8334) = audience < 0xb ? 0xc : audience < 0x15 ? 0xd : audience < 0x1f ? 9 : genre == 4 ? 0xb : 0xe;
                }
            } else {
                DS_I16(0x8336) = 3;
            }
            DS_I16(0x8338) = 0;
            if (reload)
                DS_SET_PTR(0x832e, sprite_pool_load(0xb1));
            tv_layer2_draw();
            break;
        case 3:
            x += 0x19;
            DS_I16(0x8346) = 1;
            DS_I16(0x8344) = audience < 6 ? 7 : audience < 0xb ? 6 : audience < 0x15 ? 5 : audience < 0x1f ? 8 : 9;
            if (reload)
                DS_SET_PTR(0x833e, sprite_pool_load(0xb2));
            tv_layer3_draw();
            break;
        case 4:
            x += 0x19;
            DS_I16(0x8358) = 0;
            if (audience < 0x15) {
                DS_I16(0x8356) = 1;
            } else {
                DS_I16(0x8356) = 2;
                DS_I16(0x835a) = 0;
            }
            DS_I16(0x8354) = audience < 0xb ? 0xc : audience < 0x15 ? 0 : 0xd;
            if (reload)
                DS_SET_PTR(0x834e, sprite_pool_load(0xb3));
            tv_layer4_draw();
            break;
        }
        layers++;
    }
    if (force != 2) { /* mit 2 ohne diese Zuweisung */
        DS_I16(0x79d4) = 7;
        DS_I16(0x79d6) = 2;
    }
    set_cursor_shape(0);
}

/* 1054:3a1c – Fernsehszene abschalten (Rollladen) */
void tv_anim_stop(void)
{
    DS_I16(0x79d4) = 0;
    DS_I16(0x79d6) = 1;
}

/* 1054:3a29 – Puffer für die 10 Spielstandnamen */
void savegame_names_alloc(void)
{
    for (int16_t i = 0; i < 10; i++)
        DS_ADDR(ptr32, 0x8b92)[i] = P32(far_calloc(0x29, 1));
}

/* 1054:3a53 – Spielstandnamen aus 209.HES (10 x 40 Byte) */
void savegame_names_load(void)
{
    char *raw = hes_load_raw(0xd1);
    for (int16_t i = 0; i < 10; i++)
        far_memcpy(PTR32(char, DS_ADDR(ptr32, 0x8b92)[i]), raw + i * 0x28, 0x28);
    far_free(raw);
}

/* 1054:3ab1 – Spielstandnamen nach 209.HES schreiben */
void savegame_names_save(void)
{
    char *buf = far_calloc(400, 1);
    for (int16_t i = 0; i < 10; i++)
        far_memcpy(buf + i * 0x28, PTR32(char, DS_ADDR(ptr32, 0x8b92)[i]), 0x28);
    timer_pause();
    hes_write(0xd1, buf, 400);
    timer_resume();
    far_free(buf);
}

/* 1054:3b34 – Sprite-Bank in den Pool DS:8c30 (Fernsehszene) kopieren */
SpriteBank *sprite_pool_load(int16_t hes_no)
{
    SpriteBank *bank = hes_load_unpack(hes_no);
    uint16_t size = spritebank_size(bank);
    SpriteBank *dst = (SpriteBank *)((uint8_t *)DS_PTR(Bitmap, 0x8c30) + DS_U16(0x8c3a));
    far_memcpy(dst, bank, size);
    spritebank_fix_pointers(dst);
    far_free(bank);
    DS_U16(0x8c3a) = (uint16_t)(DS_U16(0x8c3a) + spritebank_size(dst));
    return dst;
}

/* 1054:3bb6 – Programmplaner nach Zeitwechsel neu zeichnen */
void schedule_room_refresh(void)
{
    if (g_cur_room != 0x1f)
        return;
    int16_t d = g_cur_dialog;
    if (d == 0 || d == 3 || d == 4 || d == 6) {
        schedule_panel_open(DS_I16(0x7a16));
        DS_I16(0x79ae) = 1;
    } else if (d == 2) {
        archive_list_dialog(0, 0, 1, 0);
        archive_list_dialog(0x13, 8, 0, DS_I16(0x7a16));
        return;
    } else if (d == 5) {
        ad_contract_list_dialog(0, 0, 1, DS_I16(0x7a16));
        ad_contract_list_dialog(0x16, 1, 0, DS_I16(0x7a16));
    } else {
        return;
    }
    screen_update();
}

/* 1054:3c59 – Texte der Filmagentur neu laden */
void movie_agency_reload_texts(void)
{
    if (g_cur_room == 4) {
        set_cursor_shape(3);
        text_cache_titles_free(0);
        text_cache_descr_free(0);
        text_cache_titles(g_cur_room, DS_I16(0x7a16));
        text_cache_descr(g_cur_room, DS_I16(0x7a16));
        set_cursor_shape(0);
    }
}

/* PORT: Statt bei jedem Start zu fragen, gilt die gespeicherte Sprache (settings.ini im
 * Spielstandordner); beim ersten Start die des Systems (Deutsch, sonst Englisch). Gewechselt wird
 * im Fenstermenü oder mit F10 (language_poll). MADTV_LANGSELECT=1 zeigt die Originalauswahl. */
static void language_select_menu(void);

void language_select(void)
{
    int lang = platform_setting_get("language", -1);
    /* Sprache aus der Kommandozeile (game_main), keine Abfrage. Sie gilt nur für
     * diesen Start, die gespeicherte Einstellung bleibt. */
    int from_args = g_language == 0 || g_language == 1;
    if (from_args) {
        lang = g_language;
    } else if (platform_debug_env("MADTV_LANGSELECT")) {
        language_select_menu();
        lang = g_language;
    } else if (lang != 0 && lang != 1) {
        lang = platform_system_language_german() ? 0 : 1;
    }
    if (!from_args)
        platform_setting_set("language", lang);
    g_language = (int16_t)lang;
    platform_language_show(lang);
    g_cur_room = -5; /* Zustand wie nach der Originalauswahl */
    DS_I16(0x8aac) = 0;
    DS_I16(0x79b2) = 1;
    g_cursor_x = 0xa0;
    g_cursor_y = 100;
    cursor_reset();
    g_mouse_buttons = 0;
}

/* 1054:3ca4 – Sprachauswahl "(D)eutsch / (E)nglish" aus 041.HES, per Taste oder Mausklick */
static void language_select_menu(void)
{
    /* zwei Einträge à 20 Byte: Spiel-Tastencode, danach der anzuzeigende Text */
    char choices[40];
    hes_read_into(0x29, choices);
    banner_two_lines(0, choices);
    g_cur_room = -5;
    DS_I16(0x8aac) = 0;
    DS_I16(0x79b2) = 1;
    g_cursor_x = 0xa0;
    g_cursor_y = 100;
    cursor_reset();
    mouse_show();
    g_mouse_buttons = 0;

    int16_t key;
    while ((key = kbd_getkey()) != choices[0] && key != choices[20] &&
           (g_mouse_buttons == 0 || g_cursor_y < 0x5c || g_cursor_y > 0x6c))
        game_tick();

    if (g_mouse_buttons == 0 || g_cursor_y < 0x5c || g_cursor_y > 0x6c)
        g_language = choices[0] != key;
    else
        g_language = (int16_t)((g_cursor_y - 0x5c) / 8);

    mouse_hide();
    banner_two_lines(1, NULL);
}
