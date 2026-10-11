# MadTV recompiled

Native Windows-Portierung des DOS-Spiels **Mad TV** (Rainbow Arts, 1991) – ohne DOSBox.
Das Originalprogramm `MTV.EXE` (Turbo C 2.0) wurde Funktion für Funktion zurückübersetzt
(557 Funktionen) und als lesbarer C11-Code mit SDL2 neu aufgebaut. Ziel ist exakt das
Verhalten des Originals: Spiellogik, Grafik, KI, Spielstände (kompatibel mit dem Original)
und die AdLib-Musik (OPL3-Emulation).

Die Spieldaten sind **nicht** enthalten und müssen aus dem Originalspiel stammen.

## Spielen

1. `bin/madtv.exe` und `bin/SDL2.dll` in einen Ordner kopieren.
2. Daneben die Spieldateien ablegen:
   - `Gamefiles/` – `MTV.EXE` und die `.HES`-Dateien des Originals
   - `Gamefiles2/` – optional `SOURCE1.RSC`/`SOURCE2.RSC` der überarbeiteten Fassung; daraus
     werden korrigierte Daten übernommen (Shows, Texte, Titelbild, Intro)
3. `madtv.exe` starten.

Bedienung wie im Original. Alt+Enter oder F11 schaltet auf Vollbild; das Fenster lässt sich
normal schließen.

## Bauen

Benötigt MSYS2 mit der 32-Bit-Toolchain (`mingw-w64-i686-gcc`, `mingw-w64-i686-SDL2`):

```sh
cd port
mingw32-make build/madtv.exe   # Spiel
mingw32-make test              # Unit-Tests (erwarten ../Gamefiles)
```

## Aufbau

| Pfad | Inhalt |
|---|---|
| `port/src/game/` | übersetzte Originalmodule (`seg_XXXX.c` = Code-Segment von MTV.EXE) |
| `port/src/` | Plattformschicht (SDL2), Laufzeitbibliothek, Sound-Treiber, Entpacker |
| `port/TRANSLATION.md` | Übersetzungsrichtlinien |
| `port/third_party/nuked-opl3/` | OPL3-Emulator Nuked-OPL3 (LGPL-2.1) |
| `re/` | Analyse: Notizen, Funktionssignaturen, Ghidra-Skripte, Python-Werkzeuge |
| `src/`, `build.ps1` | älterer DOSBox-Launcher (C#) |

## Lizenzen Dritter

- Nuked-OPL3 © Nuke.YKT – LGPL-2.1 (`port/third_party/nuked-opl3/LICENSE`)
- SDL2 – zlib-Lizenz

Mad TV ist © 1991 Rainbow Arts. Dieses Projekt enthält keine Spieldaten des Originals.
