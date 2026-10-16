# Änderungen der Release-Fassung gegenüber der Beta

Der Port ist aus dem Code der Beta (re/v1/MTV.EXE) übersetzt. Diese Liste hält fest, was die
Release-Fassung (gamedata/MTV.EXE) inhaltlich anders macht und wie der Port damit umgeht.
Grundlage: re/notes/v1v2_map.tsv (re/tools/v1v2_map.py), Einzelheiten mit
`python re/tools/v1v2_func.py NAME --ctx`.

Status: **offen** = noch nicht im Port, **übernommen**, **schon im Port**, **entfällt** (für den
Port ohne Bedeutung), **Rauschen** (nur Compiler-Unterschiede).

## Datensegment

- Neue Variable `DS:0580` (Release) zwischen Beta-Adressen 057e und 0580; dahinter liegt alles um
  2 Byte verschoben (siehe VAR_SHIFT in v1v2_map.py).

- Tastentabelle DS:16ac (ASCII → Spieltaste): Tabulator ergibt in der Release-Fassung 0x40, in
  der Beta nichts. Kein Programmteil wertet 0x40 aus; übernommen (tools/make_bundle.py).

## Funktionen

| Funktion | Beta | Release | Änderung | Status |
|---|---|---|---|---|
| game_main | 1054:0005 | 1054:0005 | Kommandozeile: 1. Argument `d`/`D` bzw. `e`/`E` wählt die Sprache (sonst Abfrage wie bisher); 2. Argument: Anzahl `*` (höchstens 10) nach `DS:0580` | übernommen (Sprache gilt nur für diesen Start, settings.ini bleibt) |
| room_click_new_game_setup | 19e6:09f7 | 1aa4:0ac5 | Startkapital der Computergegner (300.000 bzw. 750.000) × (`DS:0580` + 1) | übernommen (`g_opponent_stars`) |
| room_load | 1054:0e4c | 1054:0f79 | Bettys Raum (0xe) lädt zusätzlich 0x9a nach DS:0108; Supermarkt-Regal immer Datei 0x2a statt 0x2a/0xf6 je Sprache (in den Release-Daten sind beide identisch); sonst Rauschen (switch als Vergleichskette) | schon im Port |
| auction_day_end | 3264:0717 | 34b8:0905 | (1) Unverkaufte Lose (3 Runden ohne Gebot): Film/Serie zurück auf den Markt (Besitzer 0, Bit 0x20); die Beta verliert sie für immer. (2) Kandidaten (Filme 1–200 mit Bit 0x40, Besitzer 0 oder > 3, Bit 0x20, Preis ≠ 0; Serien 1–50 ebenso ohne Preis) vorher zählen und höchstens so viele Lose neu füllen; die Beta hängt, wenn es keinen Kandidaten mehr gibt. (3) Filme brauchen Preis ≠ 0 | übernommen |
| room_entry_check | 2bd1:3271 | 2da8:3643 | Neu: Tür zum Chef (Raum 3) eines anderen Spielers → Rückgabe 1 (Meldung g_text(1), kein Eintritt; bei Abfrage 0). Ein neuer Fall „Abfrage 1 → 2“ bei Bombe/Ruine ist toter Code | übernommen |
| npc_update | 27bb:11fb | 293d:12fa | „Fremdes Büro: Zutritt verweigert“ gilt für die Figur des lokalen Spielers (DS:8b88 − 1) und dessen Spielernummer (Besitzer, player_flag) statt fest Figur 0 / Spieler 1; sonst Rauschen | übernommen |
| snd_init | 42a5:0002 | 45f8:0003 | Liest die Musiktabelle anders (Einträge mit Langwert bei +4c, Suche nach +2 == 0) – passt zum Datenformat der Release-Fassung; der Port hat einen eigenen Treiber | zu prüfen |
| building_hover_draw | 27bb:1be9 | 293d:1df9 | Meldung DS:79ca erst ab 2 anzeigen (1 = still verweigert, room_entry_check) | übernommen |
| ticks_set | 42fe:001a | 465d:0014 | – | Rauschen |
| building_map_door_pos | 2bd1:1af5 | 2da8:1cc2 | switch als Vergleichskette, gleiche Fälle | Rauschen |
| info_board_build | 1054:2386 | 1054:2632 | switch als Vergleichskette, gleiche Fälle | Rauschen |
| schedule_clear_programme | 1054:2298 | 1054:2505 | while statt do-while, gleiche Wirkung | Rauschen |
| init_tables | 1054:0927 | 1054:0a0b | Variable erneut gelesen statt im Register gehalten | Rauschen |
| room_enter | 1054:0d49 | 1054:0e5f | Variable erneut gelesen statt im Register gehalten | Rauschen |
| room_load_background | 1054:1ce0 | 1054:1eec | Variable erneut gelesen statt im Register gehalten | Rauschen |
| text_cache_news | 1433:0002 | 148c:0007 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| text_cache_titles | 1433:021b | 148c:025b | Variable erneut gelesen statt im Register gehalten | Rauschen |
| text_cache_descr | 1433:0a45 | 148c:0b69 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| text_cache_series | 1433:11cf | 148c:13c6 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| room_click_building | 1581:19f0 | 15ff:1b1d | Variable erneut gelesen statt im Register gehalten | Rauschen |
| room_click_info_board | 1581:1bbd | 15ff:1d23 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| room_click_ad_agency | 1581:3156 | 15ff:33f6 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| room_click_betty | 19e6:1be9 | 1aa4:1dc0 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| adcontract_sign | 19e6:246d | 1aa4:26cd | Variable erneut gelesen statt im Register gehalten | Rauschen |
| studio_screenplay_remove | 19e6:26b9 | 1aa4:2950 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| tetris_tick | 1dd5:0dcb | 1ed7:0e2e | Variable erneut gelesen statt im Register gehalten | Rauschen |
| popup_box_create | 1f51:2e46 | 205e:3002 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| channel_programme_title_load | 239a:3849 | 24e1:3b5b | Variable erneut gelesen statt im Register gehalten | Rauschen |
| db_load_all | 2a62:000c | 2c18:000c | Variable erneut gelesen statt im Register gehalten | Rauschen |
| special_film_check | 2bd1:0a80 | 2da8:0b53 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| special_film_1d_tick | 2bd1:24a5 | 2da8:26fc | Variable erneut gelesen statt im Register gehalten | Rauschen |
| special_film_bb_tick | 2bd1:24d9 | 2da8:2735 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| contest_tick | 2bd1:408e | 2da8:4481 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| day_check_players | 3264:185a | 34b8:1c8c | Variable erneut gelesen statt im Register gehalten | Rauschen |
| find_event_door | 3f95:19f7 | 42ba:1b17 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| music_load_play | 42a5:01a8 | 45f8:024c | Variable erneut gelesen statt im Register gehalten | Rauschen |
| intro_scene_setup | 4301:0090 | 465f:00a5 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| intro_tick | 4301:0212 | 465f:0244 | Variable erneut gelesen statt im Register gehalten | Rauschen |
| hotkey_handle | 1054:076c | 1054:085e | Leertaste (0x20) = Klick aufs Uhrzeitfeld (Pause/weiter) außerhalb des Tageswechsels; Cheat M-A-D-R-I-S-Enter (DS:7a7e) entfernt | übernommen (P bleibt als Port-Taste) |
| room_click | 1581:0009 | 15ff:000a | switch als Vergleichskette; Raum 0x42 ruft room_leave direkt | Rauschen |
| day_begin | 1054:1d57 | 1054:1f6f | news_values_decay(0, 0) statt (0x23, 0x19): Nachrichten vom Vortag verlieren ihren ganzen Wert | übernommen |
| clock_update | 1054:1ea5 | 1054:20e0 | statistics_panel/screen_update an den Stellen, an die die Beta springt | Rauschen (Port entspricht schon der Release-Fassung) |
| archive_find_selected | 1054:21c4 | 1054:242a | Filmsuche nur Genres 1–10; Genre 11 wäre schon das Serienregal | übernommen |
| tv_show_current_programme | 1054:3240 | 1054:3738 | Nachrichtenstunde ohne Nachricht: Bild 0xee | schon im Port |
| tv_show_picture | 1054:3357 | 1054:3855 | – | Rauschen |
| tv_anim_setup | 1054:34c2 | 1054:39cf | DS:79d4/79d6 nur setzen, wenn das Argument nicht 2 ist | übernommen (Aufrufer mit 2 noch zu suchen) |
| schedule_room_refresh | 1054:3bb6 | 1054:4135 | screen_update dupliziert statt gemeinsamem Ende | Rauschen |
| schedule_hover_editable | 1054:22de | 1054:256e | return 1 dupliziert | Rauschen |
| room_reload | 1054:0bfc | 1054:0d03 | Wert doppelt gestapelt | Rauschen |
| room_leave | 1054:2490 | 1054:2741 | Neu: Hinweistafel (Raum 0xd) mit gehaltenem Schild verlassen setzt markierte Plätze zurück (Etage aus DS:79ac, wie im Original); sonst Code an gemeinsamen Enden dupliziert | übernommen |
| room_click_auction | 1581:0345 | 15ff:0359 | Eigenes Programm versteigern: zusätzlich Bit 0x20 löschen | übernommen |
| room_click_movie_agency | 1581:088c | 15ff:090a | Code an gemeinsamen Enden dupliziert | Rauschen |
| room_click_schedule | 1581:0c78 | 15ff:0d0e | Code an gemeinsamen Enden dupliziert | Rauschen |
| room_click_news_studio | 1581:23ca | 15ff:25a4 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| offer_list_add_selected | 19e6:22f7 | 1aa4:2546 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| clock_get_seconds | 1c74:02c3 | 1d5e:02e4 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| door_draw | 1f51:12ca | 205e:1366 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| daily_costs_pay | 2bd1:0e8b | 2da8:0fb9 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| start_programmes_assign | 2bd1:13b0 | 2da8:1538 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| schedule_panel_open | 340d:0287 | 36a7:02a2 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| confirm_bubble_dialog | 340d:386f | 36a7:3a31 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| disk_request_dialog | 38b6:0a88 | 3b74:0ac8 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| player_info_popup | 38b6:1d91 | 3b74:1e56 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| ai_fill_programme_slots | 3b0a:0cf3 | 3dd9:0dd9 | Wert doppelt gestapelt bzw. erneut geladen | Rauschen |
| room_click_office | 1581:2220 | 15ff:23d8 | switch als Vergleichskette, gleiche Fälle; screen_update am gemeinsamen Ende | Rauschen |
| room_click_station_map | 1581:29fc | 15ff:2c02 | Code an gemeinsamen Enden dupliziert | Rauschen |
| room_click_real_estate | 1581:37d8 | 15ff:3ab7 | Abbruch über Klickbereich 0x25 in Dialog 1 und 2 | schon im Port |
| room_click_studio | 1581:3aaf | 15ff:3ddf | Klickbereich 0x24 im Produktionsdialog: nur bei vorbereiteter Folge; Eintrag der Show aus der Produktionsliste nehmen (production_remove, Suche ab Index = Anzahl) und Vorbereitungsstand − 1; die Beta zählte nur die Liste herunter | übernommen |
| room_click_save_menu | 1581:4135 | 15ff:44f4 | Nach erfolgreichem Speichern Dialog 0 und room_leave (zurück ins Spiel); nach dem Laden tv_anim_setup(2) außerhalb der Sendeminuten | übernommen |
| room_click_boss | 19e6:0004 | 1aa4:000e | Code an gemeinsamen Enden dupliziert (Geld, Anzeige) | Rauschen |
| room_click_supermarket | 19e6:0c29 | 1aa4:0d65 | screen_update am gemeinsamen Ende | Rauschen |
| station_next_valid | 19e6:2022 | 1aa4:2243 | Schleife anders aufgebaut, gleicher Umlauf | Rauschen |
| station_prev_valid | 19e6:207c | 1aa4:22ab | Schleife anders aufgebaut, gleicher Umlauf | Rauschen |
| fmt | 1d41:0003 | 1e35:0000 | Lage der Sprungtabelle | Rauschen |
| screen_update | 1dd5:0884 | 1ed7:08b8 | zusätzlicher Aufruf der Laufzeitbibliothek beim Bildaufbau | entfällt (Plattformschicht) |
| draw_tile_box_to | 1dd5:16bd | 1ed7:1775 | Schleife anders aufgebaut, gleiche Wirkung | Rauschen |
| window_open | 1f51:057c | 205e:058c | Vergleich mit Konstante statt Register | Rauschen |
