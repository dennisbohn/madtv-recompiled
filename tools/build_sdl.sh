#!/bin/sh
# Schlankes, statisches SDL2 für den Release-Build bauen (MSYS2, 32-Bit-Toolchain mit cmake und
# ninja: pacman -S mingw-w64-i686-cmake mingw-w64-i686-ninja).
#
# Gebaut wird nur, was der Port benutzt: Video (Fenster, Ereignisse), Renderer Direct3D 11 und
# Software (Rückfall), Audio (WASAPI/DirectSound), Timer. Abgeschaltet sind Joystick, Gamecontroller,
# HIDAPI, Haptik, Sensoren, Power, OpenGL/GLES/Vulkan, Direct3D 9/12 und die dynamische API
# (Austausch der SDL-Version zur Laufzeit, bei statischem Linken ohne Nutzen).
#
# Aufruf aus der Projektwurzel: sh tools/build_sdl.sh   -> build/sdl/install (lib, include, pkgconfig)
set -e
VER=2.32.10
DIR=build/sdl
SRC=$DIR/SDL2-$VER
mkdir -p "$DIR"
if [ ! -d "$SRC" ]; then
    curl -sSL -o "$DIR/SDL2-$VER.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$VER/SDL2-$VER.tar.gz"
    tar xzf "$DIR/SDL2-$VER.tar.gz" -C "$DIR"
fi
# dynamische API lässt sich nur im Header abschalten
sed -i 's/^#define SDL_DYNAMIC_API 1$/#define SDL_DYNAMIC_API 0/' "$SRC/src/dynapi/SDL_dynapi.h"

rm -rf "$DIR/build"
cmake -S "$SRC" -B "$DIR/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DCMAKE_INSTALL_PREFIX="$PWD/$DIR/install" \
    -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL_TESTS=OFF \
    -DSDL_JOYSTICK=OFF -DSDL_HAPTIC=OFF -DSDL_HIDAPI=OFF -DSDL_SENSOR=OFF -DSDL_POWER=OFF \
    -DSDL_LOCALE=OFF -DSDL_MISC=OFF \
    -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF \
    -DSDL_DISKAUDIO=OFF -DSDL_DUMMYAUDIO=OFF -DSDL_DUMMYVIDEO=OFF -DSDL_OFFSCREEN=OFF \
    -DHAVE_D3D_H=0 -DHAVE_D3D12_H=0
cmake --build "$DIR/build"
cmake --install "$DIR/build" > /dev/null
echo "SDL $VER (schlank) in $DIR/install"
