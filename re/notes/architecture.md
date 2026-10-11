# MadTV (MTV.EXE) – Architektur-Übersicht

Stand: erste Grobkartierung auf Basis von `re/out/decompiled.c` (Ghidra), `functions.tsv`,
`dgroup_strings.tsv` sowie gezielter Disassemblierung der Rohbytes von `Gamefiles/MTV.EXE`
(Capstone, nur für Switch-Tabellen und Aufrufstellen). Nicht alles ist verifiziert –
Vermutungen sind mit **(?)** markiert. Namen in `code` sind Vorschläge für die Umbenennung in Ghidra.

Adressierung: Datei-Offset = `0x4400 + (seg - 0x1000) * 16 + off` (MZ-Header 0x4400 Bytes, Ladebasis 0x1000).
DGROUP/DS = `0x45d4`.

---

## 1. Start und Hauptschleife

### 1.1 Startup (Segment 1000)
`entry @ 1000:0000` ist der normale Turbo-C-Startcode (c0l): DOS-Version (INT 21h/30h), Speicher
schrumpfen (4Ah), BSS nullen, Interruptvektoren 0/4/5/6 sichern (`FUN_1000_012f`), `_setupio`/Init-Tabellen
über Funktionszeiger `[0x1944]`/`[0x1948]`, Timer-Tick lesen (INT 1Ah), dann

```
FUN_1054_0005(argc, argv, envp)   ; == main()
FUN_4519_0010()                   ; == exit()
```

Weitere Funktionen in Seg 1000 sind Turbo-C-Runtime-Helfer:

| Adresse | Vorschlag | Bemerkung |
|---|---|---|
| 1000:0412 | `LXMUL@` | 32-bit Multiplikation (sehr häufig) |
| 1000:042b / 042f / 0434 / 0439 | `LDIV@` / `LUDIV@` / `LMOD@` / `LUMOD@` | 32-bit Division |
| 1000:0396 | `PADA@` (huge pointer add) | nur aus der Heap-Lib (4520/4551/4589) |
| 1000:03f1 | `PCMP@`/Normalisierung | nur Heap-Lib |
| 1000:04ce / 050a | `LXLSH@` / `LXRSH@` | 32-bit Shifts |
| 1000:0529 | `SCOPY@` | Struct-Kopie (`rep movsw`) |

### 1.2 `main()` = `FUN_1054_0005` → `game_main`
Ablauf:
1. `atexit`-ähnliche Registrierung (`FUN_451d_0003` = `atexit_far`) von Cleanup-Routinen:
   Tastatur (44ef:0009), Timer (42fe:0015), Videomodus (43d1:0000), Sound (42a5:02c0).
2. Hardware-Init über die Bibliotheks-Segmente: Maus (INT 33h, 43cb/43c7), Videomodus 13h
   (INT 10h AX=0013h, `FUN_43cf_0009`), Video-Segment 0xA000 (`FUN_43d5_0008`), Tastaturpuffer leeren,
   Sound/Musik (`FUN_42a5_0002` lädt HES 99/98/95, `FUN_4301_03ad` lädt HES 307–309).
3. `FUN_1c74_0008` → Laufwerk/Verzeichnis bestimmen, `findfirst("*.HES")`.
4. Laden der Grund-Ressourcen (Paletten, Fonts, Sprites, Mauszeiger) über den HES-Loader (siehe §3),
   `FUN_2a62_000c` (= `db_load_all`) lädt die Spieldatenbank.
5. Allokation der Offscreen-Bitmaps (`FUN_44bc_000e(w,h)` = `bitmap_alloc`, u. a. 320×80, 320×120, 208×150 …).
6. **Hauptschleife**:

```c
while (cur_room /*DAT_836c*/ != 1000) {
    if (game_phase /*DAT_7c58*/ == 0) { room_enter(0x41); ... game_phase = 1; }      // Titel-/Setup-Screen
    else if (game_phase == 2) { ... room_enter(...) ...; game_phase = 3; }
    int door = game_tick();                     // FUN_1dd5_000f
    if (door == 0) {
        ... Sonderfälle Raum 0x3d (Timeout 900 Ticks) ...
        if (mouse_click /*DAT_7af4*/ == 1) room_click();        // FUN_1581_0009
        if (mouse_click == 2) room_leave();                      // FUN_1054_2490 (rechte Maustaste = zurück)
    } else if (!room_entry_blocked(player, 0) /*FUN_2bd1_3271*/)
        room_enter(door);                                        // FUN_1054_0d49
}
```

### 1.3 Raum-/Screen-Zustandsmaschine
Globale Zustände:

| Global | Bedeutung |
|---|---|
| `DAT_45d4_836c` | `cur_room` – aktueller Raum/Screen (0 = Hochhaus/Flur, 1000 = Programmende, negative Werte = Spezial-Overlays −1/−3/−4/−5/−6) |
| `DAT_45d4_8a28[]` + `DAT_45d4_8e6e` | Raum-Stack (vorheriger Raum wird bei `room_enter` gepusht) |
| `DAT_45d4_8b84` | `room_substate` – Unterdialog im Raum (0 = keiner; 1..10 je Raum) |
| `DAT_45d4_7a16` | Besitzer/„Etagen-Spieler“ des aktuell betretenen Raums (aus Türtabelle `0x7ca8 + floor*0x7a + door`) |
| `DAT_45d4_7af2` | aktueller (menschlicher) Spieler, 0-basiert; `DAT_45d4_8b88` = derselbe 1-basiert |
| `DAT_45d4_7c58` | Spielphase (0 = Start, 1 = läuft, 2/3 = Ladevorgang) |
| `DAT_45d4_012c` | „Simulation pausiert“ (KI-Tick aus) |

Drei Dispatcher bilden die Zustandsmaschine:

* **`room_enter(id)` = `FUN_1054_0d49`** → Timer anhalten, Stack pushen, Musik wechseln (`FUN_42a5_04dd/02c6`),
  alte Raumressourcen freigeben (`FUN_1054_1bb9`), dann **`room_load(id)` = `FUN_1054_0e4c`**:
  `switch(id-1)` über eine 65-Einträge-Sprungtabelle bei **1054:0e75** (lädt Hintergrund + Sprites je Raum).
* **`room_click()` = `FUN_1581_0009`** → sparse switch (21 Werte) bei **1581:00cd** auf die Raum-Handler.
* **`room_leave()` = `FUN_1054_2490`** → if-Kaskade über `cur_room` × `room_substate` (schließt Unterdialoge,
  sonst Raum verlassen → `FUN_1054_0bfc`).

Raumtabelle (HES-Nummern dezimal; `bg` = Parameter von `FUN_1054_1ce0` = `room_load_background(n)`,
das HES *n* und *n−50* lädt):

| Raum-ID | Laden (1054:0e4c) | Klick-Handler | Vermutung |
|---|---|---|---|
| 0x00 | – (Hochhaus) | `FUN_1581_19f0` | Hochhaus/Flur mit Fahrstuhl, Figuren (Seg 27bb) |
| 0x01 | bg 104, 181 | `FUN_1581_2220`* | (?) Nachrichtenstudio (News-Ticker `FUN_1dd5_0dcb`, `FUN_38b6_066a`) |
| 0x02 | bg 110, 150/162/186, `FUN_1433_0002` | `FUN_1581_23ca`* | (?) Archiv/Programm-Liste |
| 0x03 | bg 105, 182 | `FUN_19e6_0004` | (?) |
| 0x04 | bg 101, 157/154/166, Filmtexte | `FUN_1581_088c` | Filmhändler (Angebotsliste `DAT_2463`, Anzahl `DAT_732a`) |
| 0x05 | bg ?, 170/168/112/62/192/154/157 … | `FUN_1581_3aaf` | (?) |
| 0x08 | bg 106 + Unter-Switch (8 Fälle, 1054:1128) | `FUN_1581_0345` | (?) |
| 0x09 | bg 103, 165/154/185 + Filmtexte | `FUN_1581_353d` | (?) |
| 0x0a | bg 108, 193 + Unter-Switch (1054:121b) | `FUN_1581_37d8` | (?) |
| 0x0b | bg 111, 156/154 | `FUN_1581_3156` | (?) |
| 0x0c | bg 109, 171/174/154/151/197 | `FUN_19e6_0c29` | (?) |
| 0x0d / 0x3d | 161, 152 | `FUN_1581_1bbd` / `FUN_1581_201c` | 0x3d: Spielende/Beenden (ruft `exit`) (?) |
| 0x0e | bg 102, 151, 159 | `FUN_19e6_1be9`* | (?) |
| 0x10–0x18, 0x40 | nur bg 117–127 | 0x40: `FUN_1581_454e` | einfache Räume (Nebenräume des Hochhauses) |
| 0x19 | 163, 153 | `FUN_1581_29fc` | (?) |
| 0x1f | 150/158/152/175 + Filmtexte (Archiv+Sendeplan) | `FUN_1581_0c78` | eigenes Büro / Programmplanung |
| 0x3a | bg 116, 182 | `FUN_1581_4135` | (?) |
| 0x3e | 160, 157 | `FUN_1581_37d8` | (?) |
| 0x3f | 167/172/168/169 + Texte | `FUN_19e6_0f78` | (?) |
| 0x41 | 150 | `FUN_19e6_09f7` | Titel/Neues-Spiel-Setup (startet KI-Init und ersten Tag) |
| 0x42 | – | (Sonderfall in 1581_0009) | (?) |

`*` = Funktion liegt mitten in einer von Ghidra nicht erkannten Funktion (fehlt in `functions.tsv`).
Die Raumnamen stehen in HES 253 (z. B. *Information Board, Laundry room, Supermarket, Betty's office,
Office of, News studio of, Boss of, Library of, Movie agency, Advertising agency …*); die genaue Zuordnung
ID → Name erfordert noch die Türtabelle bei `DS:0x7c80/0x7ca8` (Etage*0x7a + Tür).

### 1.4 Spiel-Tick `FUN_1dd5_000f` → `game_tick()`
Wird in der Hauptschleife und in allen modalen Warteschleifen aufgerufen:
1. `FUN_1f51_1628` (`input_poll`) → Mausklick/Tasten in `DAT_7af4`.
2. Software-Mauszeiger zeichnen (Hintergrund sichern/zurückschreiben, `FUN_43d7/43de/43f2`).
3. `FUN_3b0a_000c` = `ai_tick()` (falls Simulation läuft).
4. Alle `50/speed` Timer-Ticks (`DAT_7a30` = Spielgeschwindigkeit):
   in Raum 0 → `FUN_27bb_0b9e` (`building_animate`), sonst `FUN_1f51_1703` (`room_animate`);
   `FUN_2bd1_266b` (Zufallsereignisse (?)), `FUN_1dd5_0884` (`screen_flip`/Statuszeile).
5. `FUN_239a_000a` (`room_hover`: Tooltips/Infokarten unter dem Mauszeiger).
6. `FUN_1054_1ea5` = `clock_update()` – Uhr, Stunden-/Tageswechsel.

### 1.5 Zeitmodell
* Timer-ISR (Lib 42fe) zählt `DAT_45d4_9014` hoch; `FUN_42fe_0016` = `timer_get`, `FUN_42fe_001a` = `timer_set`.
* `FUN_1c74_02c3` = `clock_get_seconds()`: Spielzeit in Sekunden seit 17:00; `DAT_71ca` = Stunde (17..1),
  `DAT_71cc` = Minute. `FUN_2bd1_1cef` = `hour_index()` (17:00 → 0 … 01:00 → 8).
* Tagesende bei > 0x707f (≈ 8 h) → `FUN_1054_1d57` = `day_begin()` (ruft die Tagessimulation in 2bd1/3264/300f/2a62 und die KI-Tagesplanung `FUN_3b0a_3f93`).
* `FUN_3077_0781` = `broadcast_hour_tick()`: Phasen pro Stunde – Minute 0 → `FUN_3264_08ce(1)` (Zuschauerquote),
  Minute 5 → `(2)` + `FUN_2a62_0b96`, Minute 55 → `(3)` (Werbung/Abrechnung (?)).
* `FUN_1c74_020d(n, seed)` = `rand_range(n)` (1..n) mit `srand/rand` aus Lib 45cc.

---

## 2. Segment-Übersicht (Spielmodule)

Größe = Segmentgröße aus `segments.txt`; Fkt = Anzahl Ghidra-Funktionen.

| Seg | Größe | Fkt | Verantwortung | Wichtige Einstiegspunkte (Vorschlag) |
|---|---|---|---|---|
| **1054** | 15.9 KB | 37 | Hauptprogramm, Raum-Lade-/Verlassen-Dispatcher, Tagesablauf, Uhr | `0005 game_main`, `0d49 room_enter`, `0e4c room_load` (Switch!), `2490 room_leave`, `0bfc room_reload`, `1ce0 room_load_background`, `1bb9 room_free_resources`, `1d57 day_begin`, `1ea5 clock_update`, `0927 load_config/init_tables` (HES 208/0xd0), `3ab1`/`3a53` Konfig speichern/laden (HES 209 = 400 Bytes), `2fa3`/`2fd9` Bildschirm-Überblendung, `3357 show_fullscreen_pic`, `34c2` (?) TV-Bild/Nachrichten |
| **1433** | 5.4 KB | 17 | Lazy-Loader für Texte aus den Sprachdateien (Filmtitel/-beschreibungen, Serienfolgen) in einen Cache | `021b text_cache_titles(room, player)`, `0a45 text_cache_descr`, `11cf text_cache_series`, `09a2/1134/148c` Cache freigeben, `0889/101f/1373` einzelnen Eintrag laden |
| **1581** | 18.0 KB | 15 | Klick-Dispatcher + Raum-Handler (je Raum eine große Funktion) | `0009 room_click` (sparse switch @1581:00cd), Handler: `19f0, 1bbd, 201c, 088c, 0345, 0c78, 29fc, 3156, 353d, 37d8, 3aaf, 4135, 454e`, `4112` |
| **19e6** | 10.5 KB | 32 | Weitere Raum-Handler + Unterdialog-Schließer | `0004` (Raum 3), `09f7` (Raum 0x41 Setup), `0c29` (Raum 0xc), `0f78` (Raum 0x3f), `17b4`, kleine `close_*`-Funktionen `1b54…2667` (von `room_leave` benutzt) |
| **1c74** | 1.9 KB | 29 | Ressourcen-/Speicher-/Zeit-Basisschicht | `00a1 hes_load_raw(n)`, `018e hes_load_unpack(n)` (mode 1), `0111 hes_load_unpack0(n)`, `00f4 hes_read_into`, `0457 hes_size_or_ask_disk` (Diskettenwechsel-Dialog!), `04d1 font_load(n,slot)`, `0517 font_get`, `06a3 far_calloc`, `06c9 far_free`, `03e4 far_free_ptr`, `020d rand_range`, `02c3 clock_get_seconds`, `0051/0067 timer_pause/resume`, `06db/0712 clock_stop/start`, `0570/0610` Spielerfarben/-Sprites |
| **1cec** | 1.4 KB | 15 | Text-Layout: Textbox mit Wortumbruch (`~` = Zeilenumbruch) | `02ce text_box(x,y,str,width,flags,font…)`, `046d text_line_count`, Wrapper `000b…0297` (versch. Fonts/Freigabe des Strings) |
| **1d41** | 2.4 KB | 13 | Eigene `sprintf`-Variante mit `#`-Formaten + Speicher-/String-Helfer | `0003 fmt(fmt, …)` (gibt neu allokierten String zurück; `#s #d #l #f #v`, Flags `_` `0` `+` `*`, Breite; unbekannter Specifier → „Fuck“ @0x0d1a), `07ff far_memset`, `081a far_memcpy`, `0874 text_fit_width`, `08d7 text_width` |
| **1dd5** | 6.1 KB | 9 | Spiel-Tick, Mauszeiger, Statusleiste | `000f game_tick`, `0884 screen_update/flip`, `0a44`, `0b74 cursor_reset`, `0b8d text_input` (Namenseingabe (?)), `0d83 set_cursor_shape`, `0dcb news_ticker_anim` (?), `169a/16bd` |
| **1f51** | 17.6 KB | 69 | Grafik-/UI-Kern: Sprites/Blits, Maus, Text zeichnen, Raumanimationen | `0003 mouse_show`, `019e mouse_hide`, `0250 draw_string`, `0d4f blit_sprite_masked`, `0ec4 blit_rect`, `0ce1 draw_sprite`, `0a15 sprite_width`, `1158 mouse_x`, `1186 mouse_y`, `416f mouse_in_rect`, `1628 input_poll`, `1703 room_animate` (3.8k Zeilen, Switch), `3105` (?), `3e66/3eb9` Raum-Overlay-Zeichnen, `41a1` Intro/Abspann (?) |
| **239a** | 16.9 KB | 14 | Hover/Tooltip- und Infokarten (Film, Serie, Werbung) mit Box-Layout-Sprache („301 214 2-- …“) | `000a room_hover`, `1568 hover_dispatch` (19-fach-Switch), `3228 info_card_film`, `3e0f info_card_film_small`, `3cff layout_box(spec)`, `3849 tv_screen_update` (?) |
| **27bb** | 10.9 KB | 12 | Hochhaus-Ansicht: Fahrstuhl, laufende Figuren (Spieler, KI, NPCs), Türen | `0b9e building_animate`, `0ce1` Figuren-Logik (Switch), `11fb` NPC-Bewegung, `231e` Fahrstuhlfahrt (?), `000b` Etage zeichnen |
| **2a62** | 5.9 KB | 29 | Datenbank laden + Geld/Listen-Verwaltung | `000c db_load_all` (HES 201–206, 48 + Sprachdateien), `036d money_add(player, long)` → `DAT_71d5[p]`, `111a programme_remove_from_archives`, `1437 list_remove(p, idx)`, `0f7c series_next_episode`, `1041`, `0387/0ea8` Markt auffüllen (?) |
| **2bd1** | 17.4 KB | 80 | Spielregeln/Simulation: Preise, Nachrichten an Spieler, Wettbewerb, Lookup-Tabellen | `1cef hour_index`, `0d6e player_message_push`, `3ba7 film_price`, `3c6e series_price`, `3d63 fmt_money`, `30b8/314c/3102/3024` Tabellen-Lookups (Werbeverträge (?)), `408e contest_tick` (alle 3 Tage Preisvergabe), `3271 room_entry_check`, `266b random_events`, `0301 hour_start`, `2a41/2e9e` Ereignisse mit Geldfluss, `427c` (Switch) |
| **300f** | 1.7 KB | 8 | Tageswechsel-Helfer: Statistik-Arrays rotieren (heute→gestern, 7-Tage-Historie) | `0052 stats_day_rotate`, `000b`, `0204`, `03c2` |
| **3077** | 7.9 KB | 20 | Sendebetrieb/Transaktionen + Savegame | `0781 broadcast_hour_tick`, `04c2/0087/08d7/0a98` Kauf-/Plan-Regeln (Fehlermeldungen über `340d_0050`), `0f08 pay`, `11be/1d69 savegame_save/load(slot)`, `11d2 savegame_serialize` (95× `1d7c`), `1d7c save_io(ptr,len,slot)` |
| **3264** | 6.8 KB | 9 | Simulation: Einschaltquoten pro Stunde, Markt-/Preisänderungen am Tagesbeginn | `08ce ratings_calc(phase)`, `0001`, `0274`, `0438 market_update_prices`, `0717`, `17e2` |
| **340d** | 19.1 KB | 43 | Raum-UI-Panels: Sendeplan-Ansicht, Filmlisten, Detail-Dialoge, Meldungen | `0050 show_message(text_id)`, `00d4 status_money_update`, `0128 status_text`, `03a3 schedule_view`, `085f film_detail`, `0a56 film_list`, `1022 list_dialog` (Switch), `167b`, `1aa6 scroll_list`, `1d02 dialog_close`, `2246/24a1` Programmzeilen „*** 20:00 … $ … ***“, `393b` Bilanzgrafik, `440b` Statistik |
| **38b6** | 9.5 KB | 26 | Sprechblasen, Info-/Ja-Nein-Dialoge, Bilanz | `0d45 speech_bubble`, `0a88 disk_request_dialog`, `066a news_dialog` (?), `1179 status_bar_draw`, `1e63 balance_sheet` („#_8l $“-Zeilen), `17fd`, `23e5/2473` |
| **3b06** | 0.1 KB | 3 | Mini-Mathe (min/max/Skalierung) | `0000`, `0015`, `0027` |
| **3b0a** | 18.6 KB | 36 | KI-Gegner: Zustandsmaschine je Computerspieler | `000c ai_tick` (14-Fälle-Switch auf `DAT_24c5[p]`), `138e ai_buy_films`, `2e4d`, `3f93 ai_day_plan`, `361d ai_walk/arrive`, `24cc`, `2900`, `4344`, `46aa`, `4839` |
| **3f95** | 7.1 KB | 14 | KI: Programmplanung/Bewertung (Teil 2) | `037d ai_rate_slot`, `040e`, `0ab8`, `1390`, `174b ai_schedule` (Switch, Dekompilat kaputt), `0104`, `1131` |
| **4151** | 5.4 KB | 14 | KI: Hilfsfunktionen (Budget, Ziele, Werbung) | `0553 ai_init_new_game`, `06a8`, `0276`, `0480 ai_adjust`, `08e8/09fd/0d1a/0e28/0f75` KI-Aktionen |

Bibliotheks-Segmente (zur Einordnung, nicht Teil dieser Analyse): 42a5/4301 Sound/Musik, 42fe Timer-ISR,
4444 HES-Dekompressor (`4444_04ac unpack(src,dst,mode)`), 43c6–43d5 Maus/Video-Init/Palette/Retrace,
43d7–4432 Blitter, 44bc Bitmap-Alloc, 44ce–4510 HES-Datei-I/O, 4520/4551/4580/4589 Far-Heap,
4578 `strlen`, 45bd `atoi`, 45cc `srand/rand`, 45d0 `memcpy`.

### Aufrufgraph grob (Schichten)

```
game_main (1054) ─┬─ room_enter/room_load/room_leave (1054) ── Raum-Handler (1581, 19e6)
                  ├─ game_tick (1dd5) ─┬─ ai_tick (3b0a → 3f95, 4151, 3077)
                  │                    ├─ building_animate (27bb) / room_animate (1f51)
                  │                    ├─ room_hover (239a)
                  │                    └─ clock_update (1054) → day_begin / broadcast_hour_tick
                  │                                              (2bd1, 3264, 300f, 2a62, 3077)
UI-Schicht: 340d (Panels) / 38b6 (Dialoge) → 1cec (Textbox) → 1d41 (fmt) → 1f51 (Zeichnen)
Basis: 1c74 (HES/Heap/Zeit) → Libs 44ce/4500/4510 (Datei), 4444 (Unpack), 43xx/44xx (Grafik/Maus)
Daten: 2a62 (DB + Geld), 1433 (Text-Cache)
```

---

## 3. HES-Dateien

### 3.1 Loader-API
* `FUN_44ce_0105` setzt die Dateinummer (in **AX**, Registerübergabe!) in den String `"000.HES"` @DS:17b6 ein.
* `FUN_4500_0006(n, far dst)` = ganze Datei lesen; `FUN_4510_0003(n, far dst, offset, len)` = Ausschnitt lesen;
  `FUN_4505_000a`/`FUN_450b_0005(n, src, len)` = Datei schreiben (Konfiguration/Savegame);
  `FUN_44ce_012c(n)` = Dateigröße via `findfirst`; `FUN_44ce_0007(n,&size)` = entpackte Größe aus Bildlisten-Header.
* Spielseitig: `FUN_1c74_00a1` (roh), `FUN_1c74_018e` (entpackt, Modus 1), `FUN_1c74_0111` (entpackt, Modus 0).
  Fehlt die Datei, zeigt `FUN_1c74_0457` einen Diskettenwechsel-Dialog (Text 0x11f für Datei < 301, sonst 0x1ad).
* Bildformat (aus `44ce_0007`/`1c74_0384`): `[n] [n × (breite, höhe)] [Pixeldaten]`, Breite auf 8 Pixel gerundet,
  zur Laufzeit Liste von `{w, h, far ptr}`-Einträgen (8 Byte).

### 3.2 Bekannte Dateinummern (dezimal)

| HES | Ziel | Bedeutung |
|---|---|---|
| 1–7, 9–16 | main | Grund-Grafiken (Fonts, Paletten, UI-Sprites); 1 und 299 = Paletten (`FUN_43d3_0001` setzt Palette) |
| 17–24 | `FUN_1c74_0610` | Spielfiguren-Sprites je Spieler/Farbe |
| 28–30 | 27bb | Hochhaus-/Fahrstuhl-Grafik (?) |
| 41 | `FUN_1054_3ca4` | (?) Intro |
| 48 | DS:3a81 | Tabelle (?) |
| 95, 98, 99 | Sound-Lib | Musik/Instrumente |
| 101–127 (+ n−50: 51–77) | Räume | Raumhintergründe (siehe Raumtabelle) |
| 150–197 | Räume | Raum-Sprites/Overlays |
| 164 | main | (?) |
| 201 (0xc9) | `DAT_2489` (far, 201×23 B) | **Filmdatenbank** |
| 202 (0xca) | `DAT_2626` (far, 51×57 B) | **Seriendatenbank** |
| 203 (0xcb) | `DAT_713c` (far, 51×435 B) | Serienfolgen (10 Folgen × 35-Byte-Titel) |
| 204 (0xcc) | DS:3f2f (51×24 B) | Werbekunden (Name + Slogan aus Sprachdatei) |
| 205 (0xcd) | DS:2711 | (?) |
| 206 (0xce) | DS:5573 (51×56 B) | Nachrichten-Themen (10 Textvarianten je Thema) |
| 208 (0xd0) | `FUN_1054_0927` | Konfiguration/Tabellen (?) |
| 209 (0xd1) | `FUN_1054_3ab1/3a53`, `1581_4135` | Optionen/Highscore (400 Bytes, wird **geschrieben**) |
| 220, 236–238 | main/Fonts | Zeichensätze |
| 307–309 | Sound-Lib | Musik |
| Sprachdateien | Tabelle DS:0560 + `DAT_055c`×16 | 8 Dateien je Sprache: Satz 0 = 42, 207, 47, 45, 46, 49, 44, 253; Satz 1 = 246–252. Inhalt u. a.: 42 Personennamen, 44 Nachrichten-Bausteine, 45 Show-Beschreibungen, 46 Werbekunden, 47 Film-/Serientexte, 253 UI-Texte/Raumnamen |
| Savegames | `FUN_3077_1d7c` | Dateinummer = −Slot (negativer Index → eigener Dateiname (?)) |

---

## 4. Globale Datenstrukturen (DGROUP)

Alle Strukturen sind als flache Arrays mit manuellem Stride codiert (Turbo C hat die Struct-Zugriffe
zu `basis + i*stride + feld` aufgelöst). Index 0 ist meist unbenutzt (Spieler 1..3).

| Adresse | Stride × Anzahl | Bedeutung |
|---|---|---|
| `0x8bbc` | 23 B × 4 | Spieler-Stammdaten; `+0x14` = ausgeschieden-Flag (0 = aktiv) |
| `0x7b22` | 30 B (0xf Worte) × 3+ | Figur im Hochhaus: `+2` Etage, `+4` x, `+6` Ziel, `+0xc` Zustand, `+0x10` Raum/Tür, `+0x12` Richtung … |
| `0x71d5` | 4 B (long) × 4 | **Kontostand** je Spieler (`money_add`) |
| `0x43f7` | 0x5a B × 4 | **Sendeplan** heute: 15 Worte Programm-ID je Stunde (`<1000` Film, `≥1000` Serie+1000), danach Status-/Werbe-Arrays (`+0x1e`) |
| `0x3919` | 0x5a B × 4 | zweiter Plan (morgen/gestern (?)) |
| `0x6ef9` | 0x6c B × 4 | **Archiv** je Spieler nach Genre (10 Genres × 4 Plätze), Zähler `0x6f59` |
| `0x1b50` | 0x46 B × 4 | Liste à 7-Byte-Einträge (Zähler `0x7688`) – Werbeverträge (?) |
| `0x65af` | 0x5a B × 15 | Tabelle mit ID-Suche (`2bd1_30b8/314c`) – Werbung/Verträge (?) |
| `0x63c5` | 10 B × 49 | Tabelle mit ID-Suche (`2bd1_3024`) (?) |
| `0x609f` | 26 B × n | Nachrichten an den Spieler (Typ-ID, bis 10 Parameter, Tag, Stunde) |
| `0x74ca`, `0x7690`, `0x6d89` | 0x20 B × 4 | Zuschauerstatistik pro Stunde / Tag, 7-Tage-Historie |
| `0x248d` | 10 B × 4 | Quote der laufenden Stunde je Spieler |
| `0x1ce0`, `0x1ce9` | 0x1ce B × 4 | Zuschauerpotential je Stunde / Werbeslots (?) |
| `0x2463` / `0x732a` | Wort-Liste | Angebot beim Filmhändler |
| `0x7468` | 4 Worte | Wettbewerb (alle 3 Tage, `2bd1_408e`) |
| `0x24c5[p]`, `0x24e7` | Byte | KI-Zustand je Spieler, aktuell bearbeiteter KI-Spieler |
| `0x71c4` | int | Spieltag; `0x71ca`/`0x71cc` Stunde/Minute |
| `0x7bd6/7bd8` | int | Mausposition; `0x7af4` Klick |
| `0x8e62…0x8ec0` | far ptr | Offscreen-Bitmaps (Status, Cursor-Hintergrund, Dialoge) |

Filmdatensatz (23 Byte, far-Array `DAT_2489`): `+0` Genre (char), `+1` Qualität/Preisfaktor, `+3` Jahr/Faktor,
`+5` Zustand, `+6/+7` **Bitfelder** (u. a. Bit 5 „im Besitz/verwendet“, Bit 7, Bits in `+7`), `+8` long Preis,
`+0xe`, `+0xf`, `+0x11`. Seriendatensatz (57 Byte, `DAT_2626`): Titel ab `+0`, `+0x24` Zustand, `+0x25/+0x26` Bitfelder,
`+0x27` long, `+0x31` aktuelle Folge.

---

## 5. Portierungsrisiken / Auffälligkeiten

1. **Ghidra-Switch-Tabellen kaputt (größtes Analyseproblem).** Turbo C erzeugt `jmp cs:[bx+tab]` mit der
   Tabelle direkt im Code (28 Stellen: 1054×3, 1581×4, 1f51×6, 239a×2, 27bb×4, 2bd1×3, 340d×1, 3b0a×3, 3f95×2)
   sowie 6 „sparse switches“ (`mov cx,n; mov bx,tab; cmp ax,cs:[bx]; loop`, Werte- + Zieltabelle, z. B. 1581:00cd).
   Ghidra dekodiert die Tabellen als Code → Pseudo-`swi()`, `in()/out()`, „overlapping instructions“,
   Funktionen mit 4 000–29 000 Zeilen (`1054_0e4c`, `1f51_1703`, `3f95_174b`, `3f95_19f7`, `3b0a_3854`, …)
   und fehlende Funktionen (`1581_2220`, `1581_23ca`, `19e6_1be9`). Die `swi/in/out`-Treffer in 1054/1581/1f51/
   2bd1/340d/3f95 sind **Artefakte**, keine echten Hardwarezugriffe. → Vor der Detailanalyse ein Ghidra-Skript,
   das die Tabellen als Daten markiert und die Sprungziele als Referenzen setzt, danach neu dekompilieren.
2. **Registerparameter**: Die HES-I/O-Lib (44ce/4500/4505/450b/4510) bekommt die Dateinummer in AX; Ghidra
   zeigt `void`-Signaturen. Die Aufrufer übergeben die Werte aber korrekt auf dem Stack (Turbo-C-Wrapper).
3. **Direkter Videozugriff nur in den Bibliotheken**: Mode 13h (320×200×256), Segment 0xA000 (`43d5_0008`),
   Blits in 8-Pixel-Einheiten (`x>>3`), Palette über INT 10h/1012h, Retrace-Warten über Port 0x3DA (`43d2`).
   Die Spielmodule arbeiten ausschließlich über `1f51`-Wrapper → gut durch SDL-Surface ersetzbar.
4. **Software-Mauszeiger** (Hintergrund sichern/restaurieren in `game_tick`), Maus via INT 33h, Tastatur via INT 16h.
5. **Timer-ISR** (Lib 42fe/43ae) zählt `DAT_9014`; die gesamte Spielzeit hängt daran (`50/speed`-Ticks).
   Portierung: durch `SDL_GetTicks()`-basierten Zähler (18,2 Hz oder der programmierte PIT-Takt – noch prüfen) ersetzen.
6. **Viele modale Warteschleifen**, die selbst `game_tick()` aufrufen (z. B. Diskettenwechsel `1c74_0457`,
   Dialoge, Fahrstuhl) → für SDL muss die Event-Pumpe in `game_tick`/`input_poll` sitzen; ein Umbau auf
   eine echte Hauptschleife ist nicht nötig, wenn man diese Struktur beibehält.
7. **Far-Pointer überall** (large model): Datenbanken (`DAT_2489`, `DAT_2626`, `DAT_713c`) und alle Bitmaps sind
   far; Ghidra zeigt `CONCAT22(seg,off)`/`_2_2_`-Zerlegung. Huge-Pointer-Arithmetik (`PADA@`) nur im Heap.
   Port: einfache Zeiger, `farcalloc/farfree` → `calloc/free`.
8. **Bitfelder und gepackte Structs** (Film-/Serien-Records mit 23/57 Byte, Bitfelder in Wort `+6/+7`, die sich
   überlappen) – im Port `#pragma pack(1)` bzw. explizites (De-)Serialisieren; Savegame-Format (`3077_11d2`)
   packt die Bitfelder zusätzlich um.
9. **16-Bit-Arithmetik**: `int` = 16 Bit, Geld als 32-Bit-`long` (LXMUL/LDIV). Überläufe/Vorzeichen
   (z. B. `hour_index`, Preise) beim Port mit `int16_t`/`int32_t` exakt nachbilden.
10. **Globals mit festen DGROUP-Offsets** und Arrays mit Index 0 unbenutzt; Ghidra meldet „Globals overlap
    smaller symbols“ – mehrere Strukturen werden über dieselben Adressen mit unterschiedlicher Breite gelesen.
11. **Kein selbstmodifizierender Code** gefunden (lineare Capstone-Durchsicht aller Spielsegmente: keine Stores
    nach `cs:`, keine echten `in/out/int`; die einzigen Treffer sind dekodierte Sprungtabellen). Einziger
    CS-Store ist der Turbo-C-Standard `DGROUP@` im Startup (1000:0003). Kein Inline-Assembler in den Spielmodulen erkennbar; Assembler steckt nur in den Libs.
12. **Sprach-/Diskettenlogik**: Dateinummer < 301 → Diskette 1, sonst Diskette 2 (Dialog); im Port entfernen.
13. Debug-Rest: unbekannter `#`-Specifier im Formatter erzeugt den String „Fuck“ (DS:0d1a).

---

## 6. Empfohlene nächste Schritte
1. Ghidra-Skript `FixSwitchTables.java`: alle `2E FF A7 xx xx` und `2E 3B 07`-Muster suchen, Tabellenlänge aus
   dem vorangehenden `cmp ax,imm` bzw. `mov cx,imm` lesen, Tabelle als `word[]` definieren, Ziele als
   Code/Funktion anlegen, Jump-References setzen → erneut exportieren.
2. Türtabelle (`DS:0x7c80/0x7ca8`) dekodieren und Raum-IDs mit den Raumnamen aus HES 253 verknüpfen.
3. Structs in Ghidra anlegen: `Film` (23 B), `Series` (57 B), `Figure` (30 B), `PlayerInfo` (23 B), `Schedule` (0x5a B).
4. Formatter `1d41_0003` vollständig dokumentieren (Specifier-Tabelle bei DS:0153, Handler-Tabelle +12).
