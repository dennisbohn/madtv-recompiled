#!/bin/sh
# Schlankes, statisches 32-Bit-SDL2 für den Linux-Build (gcc -m32, Fedora: glibc-devel.i686, cmake,
# ninja-build sowie die i686-devel-Pakete von X11, Wayland, libxkbcommon, libdecor, Mesa (GL/EGL),
# PipeWire, PulseAudio und ALSA; siehe README).
#
# Wie tools/build_sdl.sh nur, was der Port benutzt: Video (X11, Wayland), Renderer OpenGL/GLES2 und
# Software (Rückfall), Audio (PipeWire, PulseAudio, ALSA), Timer. Die Systembibliotheken von Video
# und Audio lädt SDL zur Laufzeit (dlopen); die fertige Datei hängt dadurch nur von libc, libm und
# libdl ab und läuft unter X11 wie Wayland.
#
# Aufruf aus port/: sh tools/build_sdl_linux.sh   -> build/sdl-linux/install (lib, include, pkgconfig)
set -e
VER=2.32.10
DIR=build/sdl-linux
SRC=build/sdl/SDL2-$VER
mkdir -p "$DIR" build/sdl
if [ ! -d "$SRC" ]; then
    curl -sSL -o "build/sdl/SDL2-$VER.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$VER/SDL2-$VER.tar.gz"
    tar xzf "build/sdl/SDL2-$VER.tar.gz" -C build/sdl
fi
# dynamische API lässt sich nur im Header abschalten
sed -i 's/^#define SDL_DYNAMIC_API 1$/#define SDL_DYNAMIC_API 0/' "$SRC/src/dynapi/SDL_dynapi.h"

# 32-Bit-pkg-config-Dateien (/usr/lib statt /usr/lib64)
export PKG_CONFIG_LIBDIR=/usr/lib/pkgconfig:/usr/share/pkgconfig
rm -rf "$DIR/build"
cmake -S "$SRC" -B "$DIR/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_INSTALL_PREFIX="$PWD/$DIR/install" \
    -DCMAKE_C_FLAGS="-m32 -ffunction-sections -fdata-sections -I/usr/include/libdecor-0" \
    -DCMAKE_EXE_LINKER_FLAGS="-m32" -DCMAKE_SHARED_LINKER_FLAGS="-m32" \
    -DCMAKE_LIBRARY_ARCHITECTURE=i386-linux-gnu -DCMAKE_SYSTEM_PROCESSOR=i686 \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_TESTS=OFF \
    -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF \
    -DSDL_LOCALE=OFF -DSDL_MISC=OFF \
    -DSDL_VULKAN=OFF -DSDL_KMSDRM=OFF -DSDL_RPI=OFF -DSDL_VIVANTE=OFF -DSDL_OFFSCREEN=OFF \
    -DSDL_DISKAUDIO=OFF -DSDL_DUMMYAUDIO=OFF -DSDL_DUMMYVIDEO=OFF \
    -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_ESD=OFF -DSDL_ARTS=OFF -DSDL_NAS=OFF -DSDL_OSS=OFF \
    -DSDL_LIBSAMPLERATE=OFF -DSDL_IBUS=OFF -DSDL_FCITX=OFF -DSDL_DBUS=OFF \
    -DSDL_X11=ON -DSDL_X11_SHARED=ON -DSDL_WAYLAND=ON -DSDL_WAYLAND_SHARED=ON \
    -DSDL_WAYLAND_LIBDECOR=ON -DSDL_WAYLAND_LIBDECOR_SHARED=ON \
    -DSDL_PIPEWIRE=ON -DSDL_PIPEWIRE_SHARED=ON -DSDL_PULSEAUDIO=ON -DSDL_PULSEAUDIO_SHARED=ON \
    -DSDL_ALSA=ON -DSDL_ALSA_SHARED=ON
cmake --build "$DIR/build"
cmake --install "$DIR/build" > /dev/null
echo "SDL $VER (32 Bit, schlank) in $DIR/install"
