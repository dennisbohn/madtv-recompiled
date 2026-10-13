# Build mit MSYS2, 32-Bit-Toolchain (Zeiger müssen 4 Byte groß sein, siehe src/dgroup.h):
#   PATH=/c/msys64/mingw32/bin:$PATH mingw32-make
CC      ?= gcc
CFLAGS  ?= -O2 -g -std=c11 -Wall -Wextra -Wno-unused-parameter
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null) -lole32 -ldwmapi

BUILD := build
GAME_DIR := ../Gamefiles

PLATFORM := src/hes.c src/video.c src/menu.c src/unpack.c src/platform.c src/inflate.c
RUNTIME  := src/dgroup.c src/rtlib.c
OPL      := third_party/nuked-opl3/opl3.c
HEADERS  := $(wildcard src/*.h)

GAME_SRC := $(wildcard src/game/*.c)

all: $(BUILD)/madtv.exe $(BUILD)/hesview.exe $(BUILD)/unpacktest.exe $(BUILD)/rttest.exe $(BUILD)/SDL2.dll

# Programmsymbol als Windows-Ressource
$(BUILD)/madtv_res.o: res/madtv.rc res/version.rc res/madtv.ico | $(BUILD)
	windres -I res res/madtv.rc -O coff -o $@

# Das Spiel. game.h und stubs.c werden aus re/notes/sigs erzeugt (re/tools/gen_game_h.py).
$(BUILD)/madtv.exe: src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h $(BUILD)/madtv_res.o | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -DMADTV_VERSION='"$(VERSION)"' -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(BUILD)/madtv_res.o $(SDL_LIBS)

$(BUILD)/hesview.exe: tools/hesview.c $(PLATFORM) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Isrc -o $@ tools/hesview.c $(PLATFORM) $(SDL_LIBS)

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

test: $(BUILD)/unpacktest.exe $(BUILD)/rttest.exe $(BUILD)/fmttest.exe $(BUILD)/SDL2.dll
	$(BUILD)/unpacktest.exe $(GAME_DIR) > $(BUILD)/unpacktest.log && tail -1 $(BUILD)/unpacktest.log
	$(BUILD)/rttest.exe $(GAME_DIR) $(BUILD)/rttest.bmp
	$(BUILD)/fmttest.exe $(GAME_DIR)

clean:
	rm -rf $(BUILD)

.PHONY: all clean test release

# ---- Gebündelte Einzel-EXE (Spieldaten eingebettet, SDL statisch) ----
# Spieldaten werden nur lokal eingebettet und nie ins Repository übernommen.
V1_EXE   ?= ../Gamefiles/MTV.EXE
V2_DIR   ?= ../Gamefiles2
RELEASE  := $(BUILD)/release
VERSION  := $(shell python -I tools/version.py)
RELEASE_EXE := $(RELEASE)/MadTV-Recompiled-$(VERSION).exe
# Release: schlankes, statisches SDL aus tools/build_sdl.sh (nur benutzte Teile); --gc-sections
# entfernt beim Linken alle nicht aufgerufenen Funktionen
SDL_SLIM := $(BUILD)/sdl/install
SDL_SLIM_LIB := $(SDL_SLIM)/lib/libSDL2.a
SDL_SLIM_PC := PKG_CONFIG_PATH=$(CURDIR)/$(SDL_SLIM)/lib/pkgconfig pkg-config

$(SDL_SLIM_LIB): tools/build_sdl.sh
	sh tools/build_sdl.sh

$(RELEASE)/bundle.rc: tools/make_bundle.py res/madtv.ico $(V1_EXE) $(V2_DIR)/SOURCE1.RSC $(V2_DIR)/SOURCE2.RSC
	python -I tools/make_bundle.py $(V1_EXE) $(V2_DIR) $(RELEASE)

$(RELEASE)/bundle_res.o: $(RELEASE)/bundle.rc res/version.rc
	windres -I $(RELEASE) -I res $(RELEASE)/bundle.rc -O coff -o $@

$(RELEASE_EXE): src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h $(RELEASE)/bundle_res.o $(SDL_SLIM_LIB)
	$(CC) $(CFLAGS) -g0 $$($(SDL_SLIM_PC) --cflags sdl2) -DMADTV_VERSION='"$(VERSION)"' -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(RELEASE)/bundle_res.o $$($(SDL_SLIM_PC) --static --libs sdl2) -ldwmapi -static -static-libgcc -Wl,--gc-sections
	strip $@

release: $(RELEASE_EXE)

# ---- Linux: Einzeldatei (32 Bit, Spieldaten eingebettet, SDL statisch) ----
# Gleiche Daten wie die Windows-Release-EXE, eingebunden per .incbin (bundle.S aus make_bundle.py).
# V1_EXE darf statt der MTV.EXE der Originalfassung auch eine Release-EXE dieses Ports sein (das
# Datensegment wird dann aus deren Ressourcen gelesen), z. B.:
#   make linux V1_EXE=MadTV-Recompiled-0.2.0.exe V2_DIR=../madtv
# Die fertige Datei braucht zur Laufzeit 32-Bit-Bibliotheken (glibc, X11 oder Wayland, Mesa,
# PipeWire/PulseAudio/ALSA); SDL lädt sie per dlopen, siehe tools/build_sdl_linux.sh.
LINUX     := $(BUILD)/linux
LINUX_BIN := $(LINUX)/MadTV-Recompiled-$(VERSION)-linux
SDL_LINUX := $(BUILD)/sdl-linux/install
# selbst gezeichnete Menüleiste (Schrift per .incbin, Pfad relativ zu port/)
MENUBAR   := src/menubar.c src/menufont.S
MENUBAR_DEPS := $(MENUBAR) third_party/stb/stb_truetype.h third_party/noto-sans/NotoSans-Regular-subset.ttf

$(SDL_LINUX)/lib/libSDL2.a: tools/build_sdl_linux.sh
	sh tools/build_sdl_linux.sh

$(LINUX)/bundle.S: tools/make_bundle.py res/madtv.ico $(V1_EXE) $(V2_DIR)/SOURCE1.RSC $(V2_DIR)/SOURCE2.RSC
	python3 -I tools/make_bundle.py $(V1_EXE) $(V2_DIR) $(LINUX)

$(LINUX_BIN): src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h $(MENUBAR_DEPS) $(LINUX)/bundle.S $(SDL_LINUX)/lib/libSDL2.a
	$(CC) -m32 $(CFLAGS) -g0 -ffunction-sections -fdata-sections -I$(SDL_LINUX)/include/SDL2 -D_REENTRANT -DMADTV_VERSION='"$(VERSION)"' -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(MENUBAR) $(LINUX)/bundle.S $(SDL_LINUX)/lib/libSDL2.a -pthread -lm -ldl -Wl,--gc-sections
	strip $@

linux: $(LINUX_BIN)

.PHONY: linux
