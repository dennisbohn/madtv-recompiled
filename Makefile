# Build mit MSYS2, 32-Bit-Toolchain (Zeiger müssen 4 Byte groß sein, siehe src/dgroup.h):
#   PATH=/c/msys64/mingw32/bin:$PATH mingw32-make
CC      ?= gcc
CFLAGS  ?= -O2 -g -std=c11 -Wall -Wextra -Wno-unused-parameter
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null) -lole32 -ldwmapi

BUILD := build
# Ordner mit dem Originalspiel (Release-Fassung: MTV.EXE, SOURCE1.RSC, SOURCE2.RSC). Die Daten
# werden in jedes Programm eingebettet, aber nie ins Repository übernommen.
GAMEDATA ?= gamedata
DATA := $(BUILD)/data

PLATFORM := src/hes.c src/video.c src/menu.c src/unpack.c src/platform.c src/inflate.c
RUNTIME  := src/dgroup.c src/rtlib.c
OPL      := third_party/nuked-opl3/opl3.c
HEADERS  := $(wildcard src/*.h)

GAME_SRC := $(wildcard src/game/*.c)

all: $(BUILD)/madtv.exe $(BUILD)/SDL2.dll

# Spieldaten, Programmsymbol und Versionsangaben als Windows-Ressourcen (tools/make_bundle.py)
$(DATA)/bundle.rc: tools/make_bundle.py tools/unlzexe.py res/madtv.ico $(GAMEDATA)/MTV.EXE $(GAMEDATA)/SOURCE1.RSC $(GAMEDATA)/SOURCE2.RSC
	python -I tools/make_bundle.py $(GAMEDATA) $(DATA)

$(DATA)/bundle_res.o: $(DATA)/bundle.rc res/version.rc
	windres -I $(DATA) -I res $(DATA)/bundle.rc -O coff -o $@

# Das Spiel
$(BUILD)/madtv.exe: src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h $(DATA)/bundle_res.o | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -DMADTV_VERSION='"$(VERSION)"' -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(DATA)/bundle_res.o $(SDL_LIBS)

# Konsolenprogramme
$(BUILD)/unpacktest.exe: tools/unpacktest.c src/hes.c src/unpack.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -Isrc -o $@ tools/unpacktest.c src/hes.c src/unpack.c

$(BUILD)/rttest.exe: tools/rttest.c $(PLATFORM) $(RUNTIME) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Isrc -o $@ tools/rttest.c $(PLATFORM) $(RUNTIME) $(filter-out -mwindows,$(SDL_LIBS))

GAME_OBJS_SRC := src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC)

$(BUILD)/fmttest.exe: tools/fmttest.c $(GAME_OBJS_SRC) $(HEADERS) src/game/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Isrc -Isrc/game -o $@ tools/fmttest.c $(GAME_OBJS_SRC) $(filter-out -mwindows,$(SDL_LIBS))

$(BUILD)/SDL2.dll: | $(BUILD)
	cp "$(shell dirname "$$(which gcc)")/SDL2.dll" $@

$(BUILD):
	mkdir -p $(BUILD)

# Testprogramme lesen DGROUP.bin und die Archive aus $(DATA)
test: $(BUILD)/unpacktest.exe $(BUILD)/rttest.exe $(BUILD)/fmttest.exe $(BUILD)/SDL2.dll $(DATA)/bundle.rc
	$(BUILD)/unpacktest.exe $(DATA) > $(BUILD)/unpacktest.log && tail -1 $(BUILD)/unpacktest.log
	$(BUILD)/rttest.exe $(DATA) $(BUILD)/rttest.bmp
	$(BUILD)/fmttest.exe $(DATA)

clean:
	rm -rf $(BUILD)

.PHONY: all clean test release

# ---- Release-EXE (wie madtv.exe, aber SDL statisch und ohne Debug-Infos) ----
RELEASE  := $(BUILD)/release
VERSION  := $(shell python -I tools/version.py)
RELEASE_EXE := $(RELEASE)/MadTV-Recompiled-$(VERSION)-windows.exe
# Release: schlankes, statisches SDL aus tools/build_sdl.sh (nur benutzte Teile); --gc-sections
# entfernt beim Linken alle nicht aufgerufenen Funktionen
SDL_SLIM := $(BUILD)/sdl/install
SDL_SLIM_LIB := $(SDL_SLIM)/lib/libSDL2.a
SDL_SLIM_PC := PKG_CONFIG_PATH=$(CURDIR)/$(SDL_SLIM)/lib/pkgconfig pkg-config

$(SDL_SLIM_LIB): tools/build_sdl.sh
	sh tools/build_sdl.sh

$(RELEASE_EXE): src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h $(DATA)/bundle_res.o $(SDL_SLIM_LIB) | $(RELEASE)
	$(CC) $(CFLAGS) -g0 $$($(SDL_SLIM_PC) --cflags sdl2) -DMADTV_VERSION='"$(VERSION)"' -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(DATA)/bundle_res.o $$($(SDL_SLIM_PC) --static --libs sdl2) -ldwmapi -static -static-libgcc -Wl,--gc-sections
	strip $@

release: $(RELEASE_EXE)

$(RELEASE):
	mkdir -p $(RELEASE)

# ---- Linux: Einzeldatei (64 Bit, Spieldaten eingebettet, SDL statisch) ----
# Gleiche Daten wie unter Windows, eingebunden per .incbin (bundle.S aus make_bundle.py; eigener
# Ordner, weil die Pfade darin absolut sind).
# Die fertige Datei braucht zur Laufzeit glibc, X11 oder Wayland, Mesa und PipeWire/PulseAudio/ALSA
# (SDL lädt sie per dlopen, siehe tools/build_sdl_linux.sh). Für Releases im Ubuntu-22.04-Container
# bauen (tools/build_linux.sh), damit sie auch mit älterer glibc läuft.
# Ohne PIE gelinkt: Code und statische Daten liegen unter 4 GB, damit ihre Adressen in die 4 Byte
# großen Far-Pointer des Datensegments passen (siehe src/dgroup.h).
LINUX     := $(BUILD)/linux
LINUX_BIN := $(LINUX)/MadTV-Recompiled-$(VERSION)-linux
SDL_LINUX := $(BUILD)/sdl-linux/install
# selbst gezeichnete Menüleiste (Schrift per .incbin, Pfad relativ zur Projektwurzel)
MENUBAR   := src/menubar.c src/menufont.S
MENUBAR_DEPS := $(MENUBAR) third_party/stb/stb_truetype.h third_party/noto-sans/NotoSans-Regular-subset.ttf

$(SDL_LINUX)/lib/libSDL2.a: tools/build_sdl_linux.sh
	SDL_LINUX_DIR=$(BUILD)/sdl-linux sh tools/build_sdl_linux.sh

$(LINUX)/bundle.S: tools/make_bundle.py tools/unlzexe.py res/madtv.ico $(GAMEDATA)/MTV.EXE $(GAMEDATA)/SOURCE1.RSC $(GAMEDATA)/SOURCE2.RSC
	python3 -I tools/make_bundle.py $(GAMEDATA) $(LINUX)

$(LINUX_BIN): src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h $(MENUBAR_DEPS) $(LINUX)/bundle.S $(SDL_LINUX)/lib/libSDL2.a
	$(CC) $(CFLAGS) -g0 -no-pie -ffunction-sections -fdata-sections -I$(SDL_LINUX)/include/SDL2 -D_REENTRANT -DMADTV_VERSION='"$(VERSION)"' -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(MENUBAR) $(LINUX)/bundle.S $(SDL_LINUX)/lib/libSDL2.a -pthread -lm -ldl -no-pie -Wl,--gc-sections
	strip $@

linux: $(LINUX_BIN)

.PHONY: linux
