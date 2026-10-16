/* PORT: Menüleiste "Spiel" (Neues Spiel, Pause, Spieltempo und Schalter des Control-Centers – beides
 * steht im Spielstand –, Beenden), "Einstellungen" (Sprache, Vollbild; settings.ini) und
 * "Hilfe" (Über Mad TV Recompiled), einmal für alle Plattformen beschrieben. Windows baut daraus ein
 * Win32-Menü (video.c), Linux zeichnet sie selbst (menubar.c). Texte in UTF-8. */
#ifndef MADTV_MENU_H
#define MADTV_MENU_H

enum MenuCmd {
    MENU_SEPARATOR = -1, /* Trennlinie */
    MENU_NONE,
    MENU_QUIT,
    MENU_LANG_DE,
    MENU_LANG_EN,
    MENU_FULLSCREEN,
    MENU_ABOUT,
    MENU_NEW_GAME,
    MENU_PAUSE,
    MENU_SPEED_SLOW,
    MENU_SPEED_NORMAL,
    MENU_SPEED_FAST,
    MENU_TV_FAMILY,
    MENU_TV_PICTURE,
    MENU_MUSIC,
    MENU_CMD_COUNT
};

/* Zustand, nach dem Häkchen gesetzt und Einträge gesperrt werden */
enum MenuGame { MENU_GAME_NONE, MENU_GAME_SETUP, MENU_GAME_RUNNING }; /* vor Spielbeginn, "Neues Spiel"-Bildschirm, Spiel läuft */
typedef struct {
    int lang;       /* 0 Deutsch, 1 Englisch, -1 noch keine */
    int fullscreen;
    int game;       /* enum MenuGame */
    int speed;      /* Spieltempo (20, 30, 60) */
    int switches;   /* Schalter des Control-Centers an: Bit 0 Fernsehfamilie, 1 Fernsehbild, 2 Musik */
    int paused;     /* Pause (Raum -5) */
} MenuState;

/* Markierung eines Eintrags: Häkchen (an/aus) oder Auswahlpunkt (einer aus einer Gruppe
 * aufeinanderfolgender Einträge) */
enum MenuMark { MENU_MARK_NONE, MENU_MARK_CHECK, MENU_MARK_RADIO };

typedef struct MenuItem {
    int cmd;                    /* MENU_NONE bei Untermenüs */
    const char *text[2];        /* Deutsch, Englisch; text[0] == NULL beendet die Liste */
    const char *key[2];         /* angezeigtes Tastenkürzel (Deutsch, Englisch) oder NULL */
    int mark;                   /* enum MenuMark */
    const struct MenuItem *sub; /* Untermenü oder NULL */
} MenuItem;

/* Menüs der Leiste (jedes mit Untermenü) */
extern const MenuItem menu_bar[];

/* Text in der Sprache lang (0 Deutsch, 1 Englisch; -1 = noch keine gewählt, dann Deutsch) */
const char *menu_text(const MenuItem *it, int lang);
const char *menu_key(const MenuItem *it, int lang);
/* Häkchen bzw. Auswahlpunkt gesetzt? Eintrag wählbar? */
int menu_checked(int cmd, const MenuState *s);
int menu_enabled(int cmd, const MenuState *s);
/* Spieltempo eines Befehls MENU_SPEED_* (Spielsekunden je Sekunde wie die drei Knöpfe im
 * Optionen-Monitor), sonst 0 */
int menu_speed(int cmd);
/* Schalter des Control-Centers zu einem Befehl (0 Fernsehfamilie, 1 Fernsehbild, 2 Musik), sonst -1 */
int menu_switch(int cmd);

/* Fenster "Über Mad TV Recompiled": Titel und Text (Zeilen mit "\n", Tasten und Beschreibung durch
 * "\t" getrennt). platform ist der Name der Plattform, z. B. "Windows". */
const char *menu_about_title(int lang);
const char *menu_about_text(int lang, const char *platform);
/* Rückfrage vor "Neues Spiel" und Beschriftung der Knöpfe Ja/Nein */
const char *menu_new_game_text(int lang);
const char *menu_yes(int lang);
const char *menu_no(int lang);

#endif
