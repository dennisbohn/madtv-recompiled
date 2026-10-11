# Build mit MSYS2, 32-Bit-Toolchain (Zeiger müssen 4 Byte groß sein, siehe src/dgroup.h):
#   PATH=/c/msys64/mingw32/bin:$PATH mingw32-make
CC      ?= gcc
CFLAGS  ?= -O2 -g -std=c11 -Wall -Wextra -Wno-unused-parameter
SDL_CFLAGS := $(shell pkg-config --cflags sdl2)
SDL_LIBS   := $(shell pkg-config --libs sdl2)

BUILD := build
GAME_DIR := ../Gamefiles

PLATFORM := src/hes.c src/video.c src/unpack.c src/platform.c
RUNTIME  := src/dgroup.c src/rtlib.c
OPL      := third_party/nuked-opl3/opl3.c
HEADERS  := $(wildcard src/*.h)

GAME_SRC := $(wildcard src/game/*.c)

all: $(BUILD)/madtv.exe $(BUILD)/hesview.exe $(BUILD)/unpacktest.exe $(BUILD)/rttest.exe $(BUILD)/SDL2.dll

# Das Spiel. game.h und stubs.c werden aus re/notes/sigs erzeugt (re/tools/gen_game_h.py).
$(BUILD)/madtv.exe: src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(HEADERS) src/game/*.h | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Isrc -Isrc/game -o $@ src/main.c src/sound.c $(OPL) $(PLATFORM) $(RUNTIME) $(GAME_SRC) $(SDL_LIBS)

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

.PHONY: all clean test
