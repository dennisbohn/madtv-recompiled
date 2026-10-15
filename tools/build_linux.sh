#!/bin/sh
# Linux-Release (64 Bit) im Ubuntu-22.04-Container bauen, mit podman oder docker. Ergebnis wie bei
# "make linux", aber gegen glibc 2.35 gelinkt: build/ubuntu2204/linux/MadTV-Recompiled-<Version>-linux.
# Die Make-Variablen werden durchgereicht; Pfade müssen im Repository liegen, z. B. (aus der
# Projektwurzel): sh tools/build_linux.sh GAMEDATA=gamedata
set -e
cd "$(dirname "$0")/.."
ENGINE=${CONTAINER_ENGINE:-$(command -v podman || command -v docker)}
IMAGE=madtv-build-ubuntu2204
if ! "$ENGINE" image exists "$IMAGE" 2>/dev/null && ! "$ENGINE" image inspect "$IMAGE" >/dev/null 2>&1; then
    "$ENGINE" build -t "$IMAGE" -f tools/Containerfile.linux tools
fi
"$ENGINE" run --rm -v "$(pwd):/work:Z" -w /work "$IMAGE" make linux BUILD=build/ubuntu2204 "$@"
