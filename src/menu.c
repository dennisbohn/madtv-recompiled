#include "menu.h"

#include <stdio.h>

#ifndef MADTV_VERSION
#define MADTV_VERSION "?" /* setzt das Makefile aus res/version.rc */
#endif

static const MenuItem menu_speed_items[] = {
    {MENU_SPEED_SLOW, {"Langsam", "Slow"}, {NULL, NULL}, MENU_MARK_RADIO, NULL},
    {MENU_SPEED_NORMAL, {"Normal", "Normal"}, {NULL, NULL}, MENU_MARK_RADIO, NULL},
    {MENU_SPEED_FAST, {"Schnell", "Fast"}, {NULL, NULL}, MENU_MARK_RADIO, NULL},
    {0, {NULL, NULL}, {NULL, NULL}, 0, NULL},
};

static const MenuItem menu_game[] = {
    {MENU_NEW_GAME, {"Neues Spiel", "New Game"}, {NULL, NULL}, MENU_MARK_NONE, NULL},
    {MENU_PAUSE, {"Pause", "Pause"}, {"Leertaste", "Space"}, MENU_MARK_CHECK, NULL},
    {MENU_NONE, {"Geschwindigkeit", "Speed"}, {NULL, NULL}, MENU_MARK_NONE, menu_speed_items},
    {MENU_SEPARATOR, {"", ""}, {NULL, NULL}, MENU_MARK_NONE, NULL},
    {MENU_TV_FAMILY, {"Fernsehfamilie", "TV family"}, {NULL, NULL}, MENU_MARK_CHECK, NULL},
    {MENU_TV_PICTURE, {"Fernsehbild", "TV picture"}, {NULL, NULL}, MENU_MARK_CHECK, NULL},
    {MENU_MUSIC, {"Musik", "Music"}, {NULL, NULL}, MENU_MARK_CHECK, NULL},
    {MENU_SEPARATOR, {"", ""}, {NULL, NULL}, MENU_MARK_NONE, NULL},
    {MENU_QUIT, {"Beenden", "Quit"}, {"Alt+F4", "Alt+F4"}, MENU_MARK_NONE, NULL},
    {0, {NULL, NULL}, {NULL, NULL}, 0, NULL},
};

static const MenuItem menu_lang[] = {
    {MENU_LANG_DE, {"Deutsch", "Deutsch"}, {"F10", "F10"}, MENU_MARK_RADIO, NULL},
    {MENU_LANG_EN, {"English", "English"}, {"F10", "F10"}, MENU_MARK_RADIO, NULL},
    {0, {NULL, NULL}, {NULL, NULL}, 0, NULL},
};

static const MenuItem menu_settings[] = {
    {MENU_NONE, {"Sprache", "Language"}, {NULL, NULL}, MENU_MARK_NONE, menu_lang},
    {MENU_FULLSCREEN, {"Vollbild", "Fullscreen"}, {"F11", "F11"}, MENU_MARK_CHECK, NULL},
    {0, {NULL, NULL}, {NULL, NULL}, 0, NULL},
};

static const MenuItem menu_help[] = {
    {MENU_ABOUT, {"Über Mad TV Recompiled", "About Mad TV Recompiled"}, {NULL, NULL}, MENU_MARK_NONE, NULL},
    {0, {NULL, NULL}, {NULL, NULL}, 0, NULL},
};

const MenuItem menu_bar[] = {
    {MENU_NONE, {"Spiel", "Game"}, {NULL, NULL}, MENU_MARK_NONE, menu_game},
    {MENU_NONE, {"Einstellungen", "Settings"}, {NULL, NULL}, MENU_MARK_NONE, menu_settings},
    {MENU_NONE, {"Hilfe", "Help"}, {NULL, NULL}, MENU_MARK_NONE, menu_help},
    {0, {NULL, NULL}, {NULL, NULL}, 0, NULL},
};

const char *menu_text(const MenuItem *it, int lang)
{
    return it->text[lang == 1 ? 1 : 0];
}

const char *menu_key(const MenuItem *it, int lang)
{
    return it->key[lang == 1 ? 1 : 0];
}

int menu_speed(int cmd)
{
    switch (cmd) {
    case MENU_SPEED_SLOW: return 20;
    case MENU_SPEED_NORMAL: return 30;
    case MENU_SPEED_FAST: return 60;
    default: return 0;
    }
}

int menu_switch(int cmd)
{
    return cmd >= MENU_TV_FAMILY && cmd <= MENU_MUSIC ? cmd - MENU_TV_FAMILY : -1;
}

int menu_checked(int cmd, const MenuState *s)
{
    switch (cmd) {
    case MENU_LANG_DE: return s->lang == 0;
    case MENU_LANG_EN: return s->lang == 1;
    case MENU_FULLSCREEN: return s->fullscreen;
    case MENU_PAUSE: return s->paused;
    default:
        if (menu_switch(cmd) >= 0)
            return s->game != MENU_GAME_NONE && (s->switches >> menu_switch(cmd) & 1);
        return s->game != MENU_GAME_NONE && menu_speed(cmd) != 0 && menu_speed(cmd) == s->speed;
    }
}

int menu_enabled(int cmd, const MenuState *s)
{
    if (cmd == MENU_PAUSE)
        return s->game == MENU_GAME_RUNNING;
    if (cmd == MENU_NEW_GAME)
        return s->game == MENU_GAME_RUNNING; /* im "Neues Spiel"-Bildschirm ist man schon */
    if (cmd == MENU_TV_FAMILY || cmd == MENU_TV_PICTURE)
        return s->game == MENU_GAME_RUNNING; /* der Fernseher läuft erst im Spiel */
    if (menu_speed(cmd) || cmd == MENU_MUSIC)
        return s->game != MENU_GAME_NONE;
    return cmd != MENU_SEPARATOR;
}

const char *menu_about_title(int lang)
{
    return lang == 1 ? "About Mad TV Recompiled" : "Über Mad TV Recompiled";
}

const char *menu_new_game_text(int lang)
{
    return lang == 1 ? "Start a new game? Progress that has not been saved will be lost."
                     : "Ein neues Spiel beginnen? Nicht gespeicherter Spielfortschritt geht verloren.";
}

const char *menu_yes(int lang)
{
    return lang == 1 ? "Yes" : "Ja";
}

const char *menu_no(int lang)
{
    return lang == 1 ? "No" : "Nein";
}

const char *menu_about_text(int lang, const char *platform)
{
    static char text[1024];
    snprintf(text, sizeof text,
             lang != 1 ? "Mad TV Recompiled %s\n\n"
                         "Native %s-Portierung von Mad TV (1991, Rainbow Arts). Der Code des "
                         "Originalspiels wurde mit KI-Unterstützung (Claude von Anthropic) analysiert und "
                         "nach C übersetzt.\n\n"
                         "Entwicklung: Dennis Bohn\n"
                         "https://github.com/dennisbohn/madtv-recompiled\n\n"
                         "Tasten:\n"
                         "F10\tSprache wechseln\n"
                         "Leertaste\tPause\n"
                         "F11\tVollbild ein/aus (auch Alt+Enter)\n"
                         "Alt\tMenüleiste im Vollbild\n\n"
                         "Mad TV © 1991 Rainbow Arts."
                       : "Mad TV Recompiled %s\n\n"
                         "Native %s port of Mad TV (1991, Rainbow Arts). The code of the original "
                         "game was analysed and translated to C with AI assistance (Claude by "
                         "Anthropic).\n\n"
                         "Development: Dennis Bohn\n"
                         "https://github.com/dennisbohn/madtv-recompiled\n\n"
                         "Keys:\n"
                         "F10\tSwitch language\n"
                         "Space\tPause\n"
                         "F11\tToggle fullscreen (also Alt+Enter)\n"
                         "Alt\tMenu bar in fullscreen\n\n"
                         "Mad TV © 1991 Rainbow Arts.",
             MADTV_VERSION, platform);
    return text;
}
