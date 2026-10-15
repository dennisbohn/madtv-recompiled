#!/bin/sh
# Schlankes, statisches 64-Bit-SDL2 für den Linux-Build. Für Releases läuft das im Ubuntu-22.04-Container
# (tools/build_linux.sh); direkt aufgerufen braucht es cmake, ninja und die Entwicklerpakete von X11,
# Wayland, libxkbcommon, Mesa (GL/EGL), PipeWire, PulseAudio und ALSA (libdecor optional).
#
# Wie tools/build_sdl.sh nur, was der Port benutzt: Video (X11, Wayland), Renderer OpenGL/GLES2 und
# Software (Rückfall), Audio (PipeWire, PulseAudio, ALSA), Timer. Die Systembibliotheken von Video
# und Audio lädt SDL zur Laufzeit (dlopen); die fertige Datei hängt dadurch nur von libc, libm und
# libdl ab und läuft unter X11 wie Wayland.
#
# Aufruf aus der Projektwurzel: sh tools/build_sdl_linux.sh   -> $SDL_LINUX_DIR (Standard build/sdl-linux)/install
set -e
VER=2.32.10
DIR=${SDL_LINUX_DIR:-build/sdl-linux}
SRC=build/sdl/SDL2-$VER
mkdir -p "$DIR" build/sdl
if [ ! -d "$SRC" ]; then
    curl -sSL -o "build/sdl/SDL2-$VER.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$VER/SDL2-$VER.tar.gz"
    tar xzf "build/sdl/SDL2-$VER.tar.gz" -C build/sdl
fi
# dynamische API lässt sich nur im Header abschalten
sed -i 's/^#define SDL_DYNAMIC_API 1$/#define SDL_DYNAMIC_API 0/' "$SRC/src/dynapi/SDL_dynapi.h"

# libdecor (Fensterrahmen unter reinem Wayland) nur, wenn vorhanden (fehlt manchen Distributionen)
LIBDECOR=OFF
CFLAGS="-ffunction-sections -fdata-sections"
if pkg-config --exists libdecor-0; then
    LIBDECOR=ON
    CFLAGS="$CFLAGS $(pkg-config --cflags libdecor-0)"
fi
rm -rf "$DIR/build"
cmake -S "$SRC" -B "$DIR/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_INSTALL_PREFIX="$PWD/$DIR/install" -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_C_FLAGS="$CFLAGS" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_TESTS=OFF \
    -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF \
    -DSDL_LOCALE=OFF -DSDL_MISC=OFF \
    -DSDL_VULKAN=OFF -DSDL_KMSDRM=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF -DSDL_OFFSCREEN=OFF \
    -DSDL_DISKAUDIO=OFF -DSDL_DUMMYAUDIO=OFF -DSDL_DUMMYVIDEO=OFF \
    -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF -DSDL_OSS=OFF \
    -DSDL_LIBSAMPLERATE=OFF -DSDL_IBUS=OFF -DSDL_FCITX=OFF -DSDL_DBUS=OFF \
    -DSDL_X11=ON -DSDL_X11_SHARED=ON -DSDL_WAYLAND=ON -DSDL_WAYLAND_SHARED=ON \
    -DSDL_WAYLAND_LIBDECOR=$LIBDECOR -DSDL_WAYLAND_LIBDECOR_SHARED=ON \
    -DSDL_PIPEWIRE=ON -DSDL_PIPEWIRE_SHARED=ON -DSDL_PULSEAUDIO=ON -DSDL_PULSEAUDIO_SHARED=ON \
    -DSDL_ALSA=ON -DSDL_ALSA_SHARED=ON
cmake --build "$DIR/build"
cmake --install "$DIR/build" > /dev/null
echo "SDL $VER (64 Bit, schlank) in $DIR/install"
