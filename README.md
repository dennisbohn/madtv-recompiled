# MadTV recompiled

Native Windows-Portierung des DOS-Spiels **Mad TV** (Rainbow Arts, 1991) – ohne DOSBox.
Das Originalprogramm `MTV.EXE` (Turbo C 2.0) wurde Funktion für Funktion zurückübersetzt
(557 Funktionen) und als lesbarer C11-Code mit SDL2 neu aufgebaut. Ziel ist exakt das
Verhalten des Originals: Spiellogik, Grafik, KI, Spielstände (kompatibel mit dem Original)
und die AdLib-Musik (OPL3-Emulation).

Der Quellcode in diesem Repository enthält **keine** Spieldaten. Die fertige Spielfassung
wird getrennt davon als einzelne `MadTV.exe` unter *Releases* angeboten; nur diese Datei
bringt die Spieldaten der überarbeiteten Fassung von Mad TV mit.

## Spielen

**Fertige Fassung:** `MadTV.exe` aus den Releases herunterladen und starten – eine einzige
Datei, portabel, ohne Installation. Da sie nicht signiert ist, zeigt Windows beim ersten Start
eventuell einen SmartScreen-Hinweis („Weitere Informationen“ → „Trotzdem ausführen“).

**Mit eigenen Spieldaten** (`bin/madtv.exe` und `bin/SDL2.dll` aus dem Repository), daneben:
- `Gamefiles2/` – `SOURCE1.RSC` und `SOURCE2.RSC` der überarbeiteten Fassung: alle Spieldaten
  (Shows, Texte, Grafik, Musik, Intro)
- `Gamefiles/` – `MTV.EXE` der Originalfassung; daraus wird nur das Datensegment gelesen, weil
  der Port aus dem Code dieser Fassung übersetzt ist

Bedienung wie im Original. Das Spiel startet im Vollbild; Alt+Enter oder F11 schaltet um
(`-window` startet im Fenster). ESC verlässt Räume wie im Original und fragt in der
Gebäudeansicht, ob das Spiel beendet werden soll.

Spielstände liegen getrennt von den Spieldaten unter `Gespeicherte Spiele\Mad TV Recompiled`
(`%USERPROFILE%\Saved Games\Mad TV Recompiled`) und sind mit dem Original kompatibel.

## Bauen

Benötigt MSYS2 mit der 32-Bit-Toolchain (`mingw-w64-i686-gcc`, `mingw-w64-i686-SDL2`) und
Python 3:

```sh
cd port
mingw32-make build/madtv.exe   # Entwicklerfassung, liest ../Gamefiles und ../Gamefiles2
mingw32-make test              # Unit-Tests
mingw32-make release           # Einzel-EXE build/release/MadTV.exe mit eingebetteten Daten
```

`release` bettet das Datensegment (aus `V1_EXE`, Standard `../Gamefiles/MTV.EXE`) und die
Archive aus `V2_DIR` (Standard `../Gamefiles2`) ein, entfernt dabei alle Spielstand-Einträge
(`tools/make_bundle.py`) und linkt SDL statisch.

## Aufbau

| Pfad | Inhalt |
|---|---|
| `port/src/game/` | übersetzte Originalmodule (`seg_XXXX.c` = Code-Segment von MTV.EXE) |
| `port/src/` | Plattformschicht (SDL2), Laufzeitbibliothek, Sound-Treiber, Entpacker |
| `port/res/` | Programmsymbol (`make_icon.py` erzeugt `madtv.ico`) |
| `port/tools/` | Unit-Tests, `make_bundle.py` für die Einzel-EXE |
| `port/TRANSLATION.md` | Übersetzungsrichtlinien |
| `port/third_party/nuked-opl3/` | OPL3-Emulator Nuked-OPL3 (LGPL-2.1) |
| `re/` | Analyse: Notizen, Funktionssignaturen, Ghidra-Skripte, Python-Werkzeuge |
| `src/`, `build.ps1` | älterer DOSBox-Launcher (C#) |

## Lizenzen Dritter

- Nuked-OPL3 © Nuke.YKT – LGPL-2.1 (`port/third_party/nuked-opl3/LICENSE`)
- SDL2 – zlib-Lizenz

Mad TV ist © 1991 Rainbow Arts. Der Quellcode dieses Projekts enthält keine Spieldaten.
