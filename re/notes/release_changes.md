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
