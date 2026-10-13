# Mad TV Recompiled

A native Windows port of the DOS game *Mad TV* (Rainbow Arts, 1991), without DOSBox.

The original executable `MTV.EXE` (Turbo C 2.0, 16-bit real mode) was decompiled function by
function (557 functions) and translated into C11. The original memory layout of the data
segment is kept, so the translated code addresses its variables as the original did. Platform
services (graphics, input, timer, file access, AdLib sound driver) are reimplemented on top of
SDL2; music is synthesized with the Nuked-OPL3 emulator. Game logic, AI behaviour and the
savegame format are intended to be identical to the original.

The source code in this repository contains no game data. A self-contained `MadTV-Recompiled-<version>.exe` with the
game data of the revised release of Mad TV embedded is provided separately under *Releases*.

## How this port was made

This is an AI-assisted port. The analysis of the original machine code, the translation of its
functions into C, the platform layer and most of the debugging were done by an AI model (Claude
by Anthropic). Goals, design decisions, testing and acceptance were done by a human. Deviations
from the original are marked with `PORT` comments, so the translation can be checked against
the original code.

## Running

**Release build:** `MadTV-Recompiled-<version>.exe` from *Releases*. Single file, no installation. The executable is
not code-signed; Windows SmartScreen may show a warning on first start.

**Repository build** (`bin/madtv.exe`, `bin/SDL2.dll`) reads the game data from folders next to
the executable:

| Folder | Content |
|---|---|
| `Gamefiles2/` | `SOURCE1.RSC`, `SOURCE2.RSC` of the revised release – all game data |
| `Gamefiles/` | `MTV.EXE` of the original release – only its data segment is used, since the port is translated from this build's code |

The game starts windowed on first launch and afterwards in the last used mode; `-window` forces
windowed mode. Alt+Enter, F11 or the menu toggle fullscreen. The menu bar offers *Game > Quit*,
*Settings > Language / Fullscreen* and *Help > About*; in fullscreen it is hidden and shown by
pressing Alt. As in the original, there is no quit option inside the game itself.

The language (German/English) follows the system language on first start and is stored in
`settings.ini` in the savegame folder. It can be changed at any time via the menu or F10; the
current screen is redrawn in the new language.

Savegames are stored in `%USERPROFILE%\Saved Games\Mad TV Recompiled`. The file format is
unchanged from the original.

## Deviations from the original

Game logic is unchanged. Deviations concern presentation and platform integration; each is
marked with a `PORT` comment in the source.

- Frames are presented at the display refresh rate with V-Sync. The game's wait for vertical
  retrace is emulated at 70 Hz, so game timing does not depend on the monitor.
- The mouse cursor and dragged objects are drawn as an overlay at display refresh rate instead
  of into video memory. In fullscreen the mouse is relative and clamped to the game's cursor
  limits.
- Elevator cabin, building scroll and parallax background are interpolated between the game's
  12.5 Hz building steps and positioned with sub-pixel offsets. Walking figures keep their
  original step size.
- Screen transitions progress per pixel based on elapsed time; total duration is unchanged.
- Output scaling: integer nearest-neighbour upscale of the 320×200 image, then bilinear
  downscale to the 4:3 output area.
- `rand_range` reseeds from the 50 Hz tick counter; retry loops no longer wait for the next
  tick (faster new-game setup, same sequence of drawn values).
- Out-of-bounds reads in the original (news variant check, AI programme swap) are replaced by
  bounded equivalents.
- Additions: menu bar with quit, language, fullscreen and about (replaces the language prompt at start),
  savegame folder, window icon and title.

## Building

Requires MSYS2 with the 32-bit toolchain (`mingw-w64-i686-gcc`, `mingw-w64-i686-SDL2`) and
Python 3:

```sh
cd port
mingw32-make build/madtv.exe   # development build, reads ../Gamefiles and ../Gamefiles2
mingw32-make test              # unit tests
mingw32-make release           # self-contained build/release/MadTV-Recompiled-<version>.exe
```

`release` embeds the data segment (from `V1_EXE`, default `../Gamefiles/MTV.EXE`) and the
archives from `V2_DIR` (default `../Gamefiles2`) as Windows resources, removes all savegame
entries from the archives (`tools/make_bundle.py`) and links SDL statically.

## Source layout

| Path | Content |
|---|---|
| `port/src/game/` | translated original modules (`seg_XXXX.c` = code segment of MTV.EXE) |
| `port/src/` | platform layer (SDL2), runtime library, sound driver, decompressors |
| `port/res/` | program icon and version resource |
| `port/tools/` | unit tests, `make_bundle.py` |
| `port/TRANSLATION.md` | translation guidelines (German) |
| `port/third_party/nuked-opl3/` | OPL3 emulator Nuked-OPL3 (LGPL-2.1) |
| `re/` | analysis: notes, function signatures, Ghidra scripts, Python tools |
| `src/`, `build.ps1` | older DOSBox launcher (C#) |

Source comments are in German.

## Third-party licenses

- Nuked-OPL3 © Nuke.YKT – LGPL-2.1 (`port/third_party/nuked-opl3/LICENSE`)
- SDL2 – zlib license

Mad TV is © 1991 Rainbow Arts. The source code of this project contains no game data.
