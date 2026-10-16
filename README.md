# Mad TV Recompiled

A native Windows and Linux port of the DOS game *Mad TV* (Rainbow Arts, 1991), without DOSBox.
All 557 functions of the original `MTV.EXE` were decompiled and translated into C; graphics, input,
timer and the AdLib sound driver run on SDL2 and the Nuked-OPL3 emulator. Game logic, AI and the
savegame format follow the release version of Mad TV (its changes over the first version are
listed in `re/notes/release_changes.md`).

This is an AI-assisted port: analysis, translation and most of the debugging were done by Claude
(Anthropic), goals, decisions and testing by a human. Every deviation from the original is marked
with a `PORT` comment in the source.

## Playing

Download from *Releases*: `MadTV-Recompiled-<version>-windows.exe` or
`MadTV-Recompiled-<version>-linux` (64 bit, Ubuntu 22.04 / Debian 12 or newer; `chmod +x` first).
Single file, game data included, no installation.

- Menu bar: *Game* (new game, pause, speed, Control Center switches, quit), *Settings* (language,
  fullscreen), *Help*. In fullscreen, Alt shows it.
- Keys: Space or P pause, F10 language, F11 or Alt+Enter fullscreen.
- Command line as in the original: `D`/`E` picks the language for this start, stars in the second
  argument multiply the computer players' starting capital (e.g. `MadTV-Recompiled… E ***`).
- Savegames (original format) and `settings.ini`: `%USERPROFILE%\Saved Games\Mad TV Recompiled`
  on Windows, `~/.local/share/Mad TV Recompiled` on Linux.

Deviations from the original concern presentation only: V-Sync with smooth cursor, elevator and
scrolling, time-based screen transitions, scaling to 4:3, the music switch acting immediately, and
fixes for a few out-of-bounds reads of the original.

## Building

The repository contains no game data. Put these files from the release version of Mad TV into
`gamedata/`: `MTV.EXE`, `SOURCE1.RSC`, `SOURCE2.RSC`. Every build embeds them
(`tools/make_bundle.py`).

**Windows** – MSYS2 with `mingw-w64-i686-gcc`, `-SDL2`, `-cmake`, `-ninja` and Python 3:

```sh
mingw32-make            # build/madtv.exe (development build)
mingw32-make test       # tests
mingw32-make release    # build/release/MadTV-Recompiled-<version>-windows.exe (static, reduced SDL)
```

**Linux** – `make linux` on the host (cmake, ninja, X11/Wayland/GL/audio development packages), or
for releases in an Ubuntu 22.04 container (podman/docker) so that it runs with older glibc:

```sh
sh tools/build_linux.sh  # build/ubuntu2204/linux/MadTV-Recompiled-<version>-linux
```

## Layout

| Path | Content |
|---|---|
| `src/game/` | translated original code (`seg_XXXX.c` = code segment of `MTV.EXE`) |
| `src/` | platform layer, runtime library, sound driver, menu bar |
| `tools/` | data bundling, SDL build scripts, tests |
| `res/` | icon and version resource |
| `re/` | reverse engineering notes and tools (refer to the first release of `MTV.EXE`) |
| `third_party/` | Nuked-OPL3, tinf, stb_truetype, Noto Sans |

Source comments are in German; `TRANSLATION.md` describes the translation rules.

## Licenses

Nuked-OPL3 © Nuke.YKT (LGPL-2.1), SDL2 (zlib), tinf © Joergen Ibsen (zlib), stb_truetype ©
Sean Barrett (public domain / MIT), Noto Sans © The Noto Project Authors (OFL 1.1); see
`third_party/`. Mad TV is © 1991 Rainbow Arts.
