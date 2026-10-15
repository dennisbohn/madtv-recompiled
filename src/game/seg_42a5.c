/* Segment 42a5 – Anbindung des Soundtreibers und Musikauswahl je Raum.
 *
 * Musik gibt es nur mit AdLib (g_sound_is_adlib). Jeder Raum hat ein Stück (HES 78-97,
 * 147-149, 254/255); beim Betreten wird es geladen und gestartet, beim Verlassen gestoppt.
 * Bestimmte Räume (Karten, Laden/Speichern, Aufzugtafel ...) lassen die laufende Musik weiterspielen. */
#include "game.h"

#define g_song_no        DS_I16(0x1604) /* aktuell geladenes Stück (HES-Nummer), 0 = keins */
#define g_music_room     DS_I16(0x1606) /* Raum, dessen Musik läuft */
#define g_music_leaving  DS_I16(0x1608) /* zuletzt verlassener Raum */
#define g_song_buffer    DS_PTR(uint8_t, 0x9000)
#define g_song_data      DS_PTR(uint8_t, 0x8ff8) /* paragraph-ausgerichtet für den Treiber */
#define g_music_off      DS_I16(0x79f2) /* Option: Musik nicht wechseln */

/* PORT: für music_switch_apply */
static int16_t g_music_owner;   /* Besitzer des Raums g_music_room */
static int16_t g_music_pending; /* Musik eingeschaltet, im Raum aber kein eigenes Stück */

/* 42a5:0002 – Soundtreiber initialisieren.
 * Original: lädt 099.HES (Hardware-Erkennung) und führt den Code aus, lädt dann 098.HES
 * (AdLib) bzw. 095.HES (PC-Lautsprecher) und installiert ihn mit Parametern aus dem
 * Erkennungsdatensatz. Da die Erkennung auch ohne Karte AdLib meldet (re/notes/sound_driver.md),
 * wird hier direkt der native AdLib-Ersatz (sound.c) mit Port 388h initialisiert. */
void snd_init(void)
{
    g_sound_is_adlib = 1;
    psm_slot_0(0x388, 0, 0, 0);
    psm_slot_5(0);
}

/* 42a5:01a8 – Stück laden (LZW-entpacken) und dem Treiber übergeben (noch nicht starten) */
void music_load_play(int16_t hes_no)
{
    uint16_t unpacked_len;
    hes_read(hes_no, &unpacked_len, 0, 2);
    uint16_t alloc = (uint16_t)(unpacked_len + 0xf);
    DS_SET_PTR(0x9000, far_calloc(alloc, 1));
    DS_SET_PTR(0x8ff8, para_align(g_song_buffer));
    uint8_t *packed = far_calloc((uint16_t)hes_filesize(hes_no), 1);
    void *work = far_calloc(0x2000, 1);
    hes_read_into(hes_no, packed);
    lzw_decode(g_song_data, packed, work);
    far_free(work);
    far_free(packed);
    psm_slot_6(g_song_data, unpacked_len);
}

/* 42a5:027f */
void music_free(void)
{
    far_free(g_song_buffer);
}

/* 42a5:028f – Wiedergabe einschalten und sofort (ohne Einblenden) starten */
void music_start(void)
{
    psm_slot_5(1);
    psm_slot_9(4, 0);
}

/* 42a5:02a8 – sofort stoppen und Wiedergabe ausschalten */
void music_stop(void)
{
    psm_slot_9(2, 0);
    psm_slot_5(0);
}

/* 42a5:02c0 – per atexit registriert */
void snd_shutdown(void)
{
    psm_slot_1();
}

/* Musikstück für einen Raum (0 = keine Änderung) */
static int16_t song_for_room(int16_t room, int16_t owner_player)
{
    if (room == 0)
        return 0xff; /* Gebäude/Flur */
    if (owner_player == g_local_player) {
        switch (room) {
        case 1: return 0x61; /* Büro */
        case 2: return 0x57; /* Nachrichtenstudio */
        case 4: return 0x52; /* Filmagentur */
        case 5: return 0x4e; /* Studio */
        case 3: return 0x60; /* Chef */
        default: return 0;
        }
    }
    if (owner_player != 0)
        return 0x5d; /* Raum eines anderen Spielers */
    switch (room) {
    case 0x0a: return 0x5e;
    case 0x0e: return 0x5c;
    case 0x18: return 0x5b;
    case 0x13: return 0x5a;
    case 0x12: return 0x59;
    case 0x0b: return 0x58;
    case 0x14: return 0x55;
    case 0x15: return 0x54;
    case 0x17: return 0x53;
    case 0x0c: return 0x51;
    case 0x10: return 0x50;
    case 0x11: return 0xfe;
    case 0x08: return 0x4f;
    case 0x09: return 0x93;
    case 0x16: return 0x94;
    case 0x42: return 0x56;
    case 500: return 0x95; /* Intro */
    default: return 0;
    }
}

/* 42a5:02c6 – beim Betreten eines Raums die passende Musik starten */
void music_on_enter(int16_t room, int16_t owner_player)
{
    int16_t keep =
        g_music_off != 0 ||
        (owner_player == g_local_player &&
         (room == 0x19 || room == 0x3a || (room == 1 && g_music_room != 0 && g_music_room != 0x1f))) ||
        (room == 10 && g_music_room != 0) ||
        (owner_player != 0 && owner_player != g_local_player && room != 0 && g_music_room != 0) ||
        room == 0x3e || room == 0x3d || room == 0xd || room == 0x40 ||
        (room == 0 && (g_music_room == 0x3d || g_music_room == 0xd || g_music_room == 0x40));
    /* PORT: Musik wurde eingeschaltet, als im Raum kein eigenes Stück lief (z. B. auf der
     * Aufzugtafel): dann hier das Stück dieses Raums starten statt die Stille weiterzuführen */
    if (keep && g_music_pending && g_music_off == 0 && g_song_no == 0)
        keep = 0;
    g_music_owner = owner_player;
    if (keep) {
        g_music_room = room;
        return;
    }
    g_music_room = room;
    g_song_no = 0;
    if (g_sound_is_adlib == 1) {
        g_song_no = song_for_room(room, owner_player);
        if (g_song_no != 0) {
            music_load_play(g_song_no);
            music_start();
            g_music_pending = 0;
        }
    }
}

/* PORT: Schalter Musik (DS:79f2) sofort anwenden statt erst beim nächsten Raumwechsel: aus = das
 * laufende Stück stoppen (wie music_on_leave), an = das Stück des Raums starten (wie
 * music_on_enter). Räume, die sonst die Musik des vorigen Raums übernehmen (Aufzugtafel, Karten,
 * Laden/Speichern ...), haben kein eigenes Stück; dort setzt die Musik im nächsten Raum ein. */
void music_switch_apply(void)
{
    if (g_music_off != 0) {
        g_music_pending = 0;
        if (g_song_no != 0) {
            music_stop();
            music_free();
            g_song_no = 0;
        }
        return;
    }
    if (g_song_no != 0 || g_sound_is_adlib != 1)
        return;
    g_song_no = song_for_room(g_music_room, g_music_owner);
    if (g_song_no != 0) {
        music_load_play(g_song_no);
        music_start();
    } else {
        g_music_pending = 1;
    }
}

/* 42a5:04dd – beim Verlassen eines Raums die Musik stoppen, außer sie läuft weiter */
void music_on_leave(int16_t room, int16_t owner_player)
{
    int16_t keep =
        (owner_player == g_local_player &&
         (room == 0x19 || room == 0x3a || (room == 1 && g_music_leaving != 0 && g_music_leaving != 0x1f))) ||
        room == 0x3e ||
        (room == 10 && g_music_leaving != 0) ||
        (owner_player != 0 && owner_player != g_local_player && room != 0 && g_music_leaving != 0) ||
        room == 0x3d || room == 0xd || room == 0x40 ||
        (room == 0 && (g_music_leaving == 0x3d || g_music_leaving == 0xd || g_music_leaving == 0x40));
    g_music_leaving = room;
    if (!keep && g_song_no != 0) {
        music_stop();
        music_free();
        g_song_no = 0;
    }
}

/* 42a5:0582 – Zeiger auf Paragraphengrenze (Original: (seg+1):(off & 0xfff0)) */
void *para_align(void *p)
{
    uintptr_t a = (uintptr_t)p;
    return (void *)((a + 15) & ~(uintptr_t)15);
}
