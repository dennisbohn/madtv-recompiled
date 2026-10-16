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
| programme_buy | 2bd1:20e4 | 2da8:22f6 | Film ohne Platz im Archiv: bei Genre 8 zusätzlich Bit 0x20 löschen | übernommen |
| film_price | 2bd1:3ba7 | 2da8:3f5b | Ohne Qualitätswert (f[1] == 0) Grundpreis statt 0 | übernommen |
| case_store_to_archive | 300f:0599 | 322a:061e | Besitzer von Film/Serie auf den Spieler setzen | übernommen |
| event_npc_strike | 2bd1:2a41 | 2da8:2d3b | DS:7a4a = 0x48 bzw. 0x49, wenn Raum 0x13 bzw. 0x12 zur Ruine wird (Index des Ereigniszustands) | übernommen |
| random_events | 2bd1:266b | 2da8:2905 | Bombenereignis endet immer mit Zustand 1; danach Zustand DS:79a6 + 7a4a·2 = 0 | übernommen |
| programmes_topicality_recover | 2bd1:01f6 | 2da8:0217 | Aktualität erholt sich nur an geraden Tagen | übernommen |
| image_distribute | 2bd1:3895 | 2da8:3be8 | Mit nur einem Spieler im Spiel nichts verteilen | übernommen |
| ratings_rank_update | 2bd1:363d | 2da8:395a | Mit nur einem Spieler: Rang 1 für den lokalen Spieler in der Stunde eintragen | übernommen |
| production_costs_update | 300f:03f8 | 322a:0424 | Gage nur mit Darsteller; Typ 0xf aus der ersten Tabelle, Index sh[Typ+0x1a3]−1; Gage × Byte 0xa der Folge | übernommen |
| ai_choose_studio_action | 3b0a:3d84 | 3dd9:41ff | Produktion (6) nur bei freiem Studio (+0xa, +0xe), fehlende Besetzung (5) immer | übernommen |
| market_update_prices | 3264:0438 | 34b8:04b9 | Billige Filme nur mit Preis zählen; Kandidaten (Filme mit Preis, Serien) vorher zählen und nur so viele Plätze füllen; Filme brauchen Preis | übernommen |
| ratings_calc | 3264:08ce | 34b8:0c4a | Phase 1 ohne Anhebung von DS:3f2b über rating_from_viewers; Film 0x3a immer einmalig; Serienfolge mindestens 1 und im Sendeplan (+0x1e) vermerkt; Mindestzuschauer der Werbung auf 100.000 statt 10.000 gerundet | übernommen |
| broadcast_hour_tick | 3077:0781 | 329e:0828 | Phasenzähler an jeder Stelle dupliziert | Rauschen |
| studio_produce_episode | 3077:0a98 | 329e:0b98 | Code an gemeinsamen Enden dupliziert | Rauschen |
| savegame_io | 3077:1d7c | 329e:2020 | umgebaut, gleiche Wirkung | Rauschen |
| ai_tick | 3b0a:000c | 3dd9:000c | Studio (Aktion 7): ai_choose_studio_action vorher aufrufen; unbekannte Studioaktion endet ohne Aktion (Beta: weiter in Fall 8); sonst Endstücke dupliziert | übernommen |
| ai_choose_films | 3b0a:190e | 3dd9:1a3f | gleiche Abbuchung in beiden Zweigen | Rauschen |
| ai_budget_plan | 3b0a:24cc | 3dd9:26ea | Code dupliziert | Rauschen |
| ai_can_take_contract | 3b0a:30bd | 3dd9:33cb | höchstens 8 Werbeverträge | übernommen |
| ai_ad_spots_per_day | 3b0a:3410 | 3dd9:3754 | Restlaufzeit 0 → 1 (keine Division durch 0) | übernommen |
| ai_unfinished_avg_quality | 3b0a:3f21 | 3dd9:43d6 | ohne Einträge 0, Durchschnitt über n statt n + 1 Einträge | übernommen |
| ai_archive_avg_quality | 4151:080a | 448b:0895 | Anzahl 0 → 1 (keine Division durch 0) | übernommen |
| ai_calc_bid | 3f95:0000 | 42ba:000a | Rückgabe über lokale Variable | Rauschen |
| ai_check_arrival | 3b0a:361d | 3dd9:3985 | An der Tür Zutritt prüfen (room_entry_check), verwehrt → nächstes Ziel | übernommen |
| ai_goto_next | 3b0a:3854 | 3dd9:3c0d | Endstücke dupliziert | Rauschen |
| ai_pick_destination | 3f95:174b | 42ba:1800 | Zufallssuche höchstens 20 Versuche; Felder der Hinweistafel tauschen statt Etage/Tür des Ziels einzutragen | übernommen |
| ai_rate_slot | 3f95:037d | 42ba:03bc | Kein Ausgangswert über rating_from_viewers; Genre-Anteil direkt statt Maximum (rate[] im Original sonst uninitialisiert, im Port 0) | übernommen |
| ai_produce_episode | 4151:0d1a | 448b:0df7 | Genre über Film/Serie der Show (Beta: Shownummer); ohne Ausnahme für Bit 4 | übernommen |
| ai_prepare_episode | 4151:0e28 | 448b:0f05 | anders strukturiert, gleiche Wirkung | Rauschen |
| ai_cast_episode | 4151:0f75 | 448b:1087 | beide Zweige gleich | Rauschen |
| episode_cast_complete | 2bd1:22af | 2da8:24ea | Startwert in lokaler Variable | Rauschen |
| event_npc_init | 2bd1:250c | 2da8:276d | gemeinsam genutztes Register | Rauschen |
| ad_genre_allowed | 2bd1:3e86 | 2da8:4258 | gemeinsam genutztes Register | Rauschen |
| game_tick | 1dd5:000f | 1ed7:0004 | Variablen erneut gelesen; tetris_tick am gemeinsamen Ende | Rauschen |
| room8_items_panel | 340d:0f5b | 36a7:0fe5 | mouse_hide/mouse_show je Zweig ausgeglichen | schon im Port |
| ad_offers_panel | 340d:144f | 36a7:14fb | mouse_hide/mouse_show je Zweig ausgeglichen | schon im Port |
| archive_shelf_panel | 340d:155a | 36a7:1613 | mouse_hide/mouse_show je Zweig ausgeglichen | schon im Port |
| production_offers_panel | 340d:3047 | 36a7:31c3 | mouse_hide/mouse_show je Zweig ausgeglichen | schon im Port |
| schedule_view | 340d:03a3 | 36a7:03c6 | Konstante erneut geladen | Rauschen |
| dialog_frame | 340d:1022 | 36a7:10b9 | Konstante erneut geladen | Rauschen |
| series_episode_dialog | 340d:167b | 36a7:1742 | Konstante erneut geladen | Rauschen |
| cursor_attach_item | 340d:1aa6 | 36a7:1b87 | Konstante erneut geladen | Rauschen |
| station_map_view | 340d:2a3c | 36a7:2b9d | Konstante erneut geladen | Rauschen |
| speech_bubble_text52 | 38b6:0dc5 | 3b74:0e17 | Konstante erneut geladen | Rauschen |
| player_setup_screen | 38b6:1269 | 3b74:12cc | Konstante erneut geladen | Rauschen |
| auction_panel | 340d:2da7 | 36a7:2f27 | Angezeigter Preis: Gebotsschritt immer aufschlagen, ohne Bieter doppelt | übernommen |
| statistics_panel | 340d:440b | 36a7:4611 | Balkenhöhe (Wert/1000)·15/(Maßstab·1000) statt Wert·15/(Maßstab·1.000.000) (kein Überlauf) | übernommen |
| genre_menu_dialog | 38b6:0253 | 3b74:0267 | Fachanzeige 5 nur bei vollem Fach (4) und passendem Titel im Koffer | übernommen |
| player_message_dialog | 38b6:17fd | 3b74:1875 | Text-Cache je Zweig freigegeben | schon im Port |
| channel_info_card_draw | 239a:3228 | 24e1:3502 | Text nach dem Zeichnen freigegeben | schon im Port |
| ad_requirements_text_build | 239a:3a2f | 24e1:3d66 | Text je Zweig freigegeben | schon im Port |
| info_card_film_small | 239a:3e0f | 24e1:4172 | Ohne Qualitätswert Anzeige 40 (Beta: Division durch 0) | übernommen |
| building_draw | 27bb:000b | 293d:000f | Wandzeichnung in beiden Zweigen | schon im Port |
| facade_draw | 27bb:2951 | 293d:2c8a | Konstante erneut geladen | Rauschen |
| tv_picture_bg_draw | 1f51:38b1 | 205e:3b14 | Konstante erneut geladen | Rauschen |
| figures_update | 27bb:0ce1 | 293d:0d62 | Bildfolge als switch statt Tabelle | Rauschen |
| elevator_update | 27bb:198f | 293d:1b63 | Nullsetzen am gemeinsamen Ende dupliziert | Rauschen |
| player_eliminated_sequence | 27bb:231e | 293d:257a | Computerspieler scheidet aus: Budget +0x14 auf 0, Fernseher auf den eigenen Sender, Besitzer seiner Serien freigeben | übernommen |
| room_region_restore | 1f51:2c53 | 205e:2def | Zeiger nur ausblenden, wenn er über dem Bereich liegt | schon im Port |
| room_animate | 1f51:1703 | 205e:17d1 | Code an gemeinsamen Enden dupliziert | Rauschen |
| tv_picture_compose | 1f51:3105 | 205e:32ed | Verzweigung umgestellt, gleiche Wirkung | Rauschen |
| game_end_sequence | 1f51:41a1 | 205e:447b | Gibt Show-, Serien- und Filmtabellen frei (Platz für die Schlusssequenz unter DOS); danach Programmende | entfällt |
| room_hover | 239a:000a | 24e1:000e | Archiv: Tooltip nur für belegte Plätze des Fachs; sonst Endstücke dupliziert | übernommen |
| hover_dispatch | 239a:1568 | 24e1:16bb | Endstücke dupliziert; Rollen-Tooltip wie im Port | Rauschen |
| intro_play | 4301:03ad | 465f:03f2 | Titelbild 100 Takte (2 s) fest stehen lassen | übernommen (ersetzt die bisherige Port-Haltezeit von bis zu 3 s) |
| room_click_supermarket_shelves | 19e6:0f78 | 1aa4:10cc | Code an gemeinsamen Enden dupliziert | Rauschen |
