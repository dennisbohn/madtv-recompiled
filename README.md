# Mad TV Recompiled

A native Windows and Linux port of the DOS game *Mad TV* (Rainbow Arts, 1991), without DOSBox.

The original executable `MTV.EXE` (Turbo C 2.0, 16-bit real mode) was decompiled function by
function (557 functions) and translated into C11. The original memory layout of the data
segment is kept, so the translated code addresses its variables as the original did. Platform
services (graphics, input, timer, file access, AdLib sound driver) are reimplemented on top of
SDL2; music is synthesized with the Nuked-OPL3 emulator. Game logic, AI behaviour and the
savegame format are intended to be identical to the original.

The source code in this repository contains no game data. Self-contained executables with the game
data of the revised release of Mad TV embedded (`MadTV-Recompiled-<version>.exe` for Windows,
`MadTV-Recompiled-<version>-linux` for Linux) are provided separately under *Releases*.

## How this port was made

This is an AI-assisted port. The analysis of the original machine code, the translation of its
functions into C, the platform layer and most of the debugging were done by an AI model (Claude
by Anthropic). Goals, design decisions, testing and acceptance were done by a human. Deviations
from the original are marked with `PORT` comments, so the translation can be checked against
the original code.

## Running

**Release build:** `MadTV-Recompiled-<version>.exe` from *Releases*. Single file, no installation. The executable is
not code-signed; Windows SmartScreen may show a warning on first start. On Linux, make
`MadTV-Recompiled-<version>-linux` executable (`chmod +x`) and start it. It is a 64-bit program and runs
on distributions from Ubuntu 22.04 / Debian 12 on (glibc 2.34 or later) without additional packages.

**Repository build** (`bin/madtv.exe`, `bin/SDL2.dll`) reads the game data from folders next to
the executable:

| Folder | Content |
|---|---|
| `Gamefiles2/` | `SOURCE1.RSC`, `SOURCE2.RSC` of the revised release – all game data |
| `Gamefiles/` | `MTV.EXE` of the original release – only its data segment is used, since the port is translated from this build's code |

The game starts windowed on first launch and afterwards in the last used mode; `-window` forces
windowed mode. Alt+Enter, F11 or the menu toggle fullscreen. The menu bar offers *Game > New Game /
Pause / Speed / TV family / TV picture / Music / Quit*, *Settings > Language / Fullscreen* and *Help > About*; in fullscreen it is hidden
and shown by pressing Alt. As in the original, there is no quit option inside the game itself.

*New Game* (after a confirmation) returns to the original's new-game screen (player, names,
difficulty), as after losing a game. *Speed*, *TV family*, *TV picture* and *Music* run the same code
as the speed buttons and switches in the game's Control Center (programme planner); if it is open,
it shows the change. Like in the original, these are stored in the savegame, while language and
fullscreen are program settings (`settings.ini`). *Pause* (also key P) does the same as clicking the
clock field in the status bar; as in the original, a click also ends the pause.

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
- The music switch (Control Center and menu) takes effect immediately: switching off stops the current
  piece, switching on starts the piece of the current room. In the original it only took effect at the
  next room change. Rooms that keep the previous room's music (e.g. the elevator board) have no piece
  of their own; there the music starts in the next room.
- Out-of-bounds reads in the original (news variant check, AI programme swap) are replaced by
  bounded equivalents.
- Additions: menu bar with new game, pause (key P), game speed, quit, language, Control Center switches, fullscreen and about (replaces
  the language prompt at start), savegame folder, window icon and title.

## Building

Requires MSYS2 with the 32-bit toolchain (`mingw-w64-i686-gcc`, `mingw-w64-i686-SDL2`) and
Python 3:

```sh
cd port
mingw32-make build/madtv.exe   # development build, reads ../Gamefiles and ../Gamefiles2
mingw32-make test              # unit tests
mingw32-make release           # self-contained build/release/MadTV-Recompiled-<version>.exe
```

`release` embeds the data segment (from `V1_EXE`, default `../Gamefiles/MTV.EXE`) and the archives
from `V2_DIR` (default `../Gamefiles2`) as Windows resources, removes all savegame entries from
the archives (`tools/make_bundle.py`), compresses them with Deflate (unpacked at startup with
tinf) and links a reduced, static SDL. That SDL is built once by `tools/build_sdl.sh` (downloads
the SDL 2.32.10 source; needs `mingw-w64-i686-cmake` and `mingw-w64-i686-ninja`) with only the
parts the port uses: video, Direct3D 11 and software renderer, audio and timers. Joystick, game
controller, haptics, sensors, OpenGL/Vulkan and Direct3D 9/12 are left out, which roughly halves
the size of the executable.

### Linux

The Linux version is a 64-bit program. Releases are built in an Ubuntu 22.04 container (podman or
docker), so that the result runs with glibc 2.34 or later:

```sh
cd port
sh tools/build_linux.sh V1_EXE=<MTV.EXE of the original release or a MadTV-Recompiled-<version>.exe> V2_DIR=<folder with SOURCE1.RSC, SOURCE2.RSC>
```

The result `build/ubuntu2204/linux/MadTV-Recompiled-<version>-linux` is a single file with the game
data embedded (via `.incbin`, `tools/make_bundle.py`) and a static SDL built by
`tools/build_sdl_linux.sh`. Paths must lie inside the repository (it is mounted into the container).
`V1_EXE` may also be a Windows release executable: the data segment is then taken from its embedded
resources. SDL loads X11 or Wayland, OpenGL/EGL and PipeWire/PulseAudio/ALSA at runtime. Without a
container, `make linux` builds the same on the host, given cmake, ninja and the development packages
of these libraries (the result then requires the host's glibc version).

Like the original, the game stores far pointers as 4-byte values in its data segment. On 64-bit
Linux the program is linked without PIE and `malloc` is restricted to the heap behind the program,
so that all addresses stored there lie below 4 GB; `P32`/`PTR32` in `port/src/dgroup.h` convert
between pointers and these values (on 32-bit Windows they are plain casts).

The menu bar has the same entries as on Windows (both are built from `port/src/menu.c`). Linux has
no window menu of its own, so the port draws the bar, its menus and the About dialog itself, in the
style of KDE Breeze with an embedded Noto Sans (`port/src/menubar.c`, rendered with stb_truetype);
no toolkit is needed. Under X11/XWayland the desktop scaling (`Xft.dpi`) is applied to the menu
bar and the initial window size; `MADTV_UI_SCALE` overrides it. Keyboard: Alt opens the menu
(arrow keys, Enter, Esc), P pauses, F10 switches the language, F11 or Alt+Enter toggles fullscreen.
Savegames and `settings.ini` are stored in `~/.local/share/Mad TV Recompiled`.

## Source layout

| Path | Content |
|---|---|
| `port/src/game/` | translated original modules (`seg_XXXX.c` = code segment of MTV.EXE) |
| `port/src/` | platform layer (SDL2), runtime library, sound driver, decompressors, menu bar (`menu.c`; `menubar.c` drawn on Linux) |
| `port/res/` | program icon and version resource |
| `port/tools/` | unit tests, `make_bundle.py` |
| `port/TRANSLATION.md` | translation guidelines (German) |
| `port/third_party/nuked-opl3/` | OPL3 emulator Nuked-OPL3 (LGPL-2.1) |
| `port/third_party/stb/`, `port/third_party/noto-sans/` | font rendering and font of the Linux menu bar |
| `re/` | analysis: notes, function signatures, Ghidra scripts, Python tools |
| `src/`, `build.ps1` | older DOSBox launcher (C#) |

Source comments are in German.

## Third-party licenses

- Nuked-OPL3 © Nuke.YKT – LGPL-2.1 (`port/third_party/nuked-opl3/LICENSE`)
- SDL2 – zlib license
- tinf © Joergen Ibsen – zlib license (`port/third_party/tinf/LICENSE`)
- stb_truetype © Sean Barrett – public domain / MIT (`port/third_party/stb/stb_truetype.h`)
- Noto Sans © The Noto Project Authors – SIL Open Font License 1.1 (`port/third_party/noto-sans/OFL.txt`)

Mad TV is © 1991 Rainbow Arts. The source code of this project contains no game data.
