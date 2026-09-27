#!/bin/bash
# Build Hatari with SidecarTridge Multi-device support (md-emulator).
# Copyright (C) 2026 Neil Rackett
# SPDX-License-Identifier: GPL-2.0-or-later
#
#   build-hatari.sh [hatari source folder]
#
# By default Hatari v2.6.1 is cloned into a per-user cache
# ($MDEMU_CACHE, else ~/.cache/md-emulator/hatari), shared by every
# project and kept out of md-emulator itself (often a submodule). The
# patch is re-applied from clean whenever it changes; later runs just
# rebuild. Prints the path of the hatari binary last.
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
CACHE=${MDEMU_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/md-emulator}
SRC=${1:-$CACHE/hatari}
HATARI_GIT=${HATARI_GIT:-https://framagit.org/hatari/hatari.git}
HATARI_TAG=${HATARI_TAG:-v2.6.1}
PATCH="$HERE/hatari-multidevice.patch"

sha1() { if command -v shasum >/dev/null; then shasum -a 1; else sha1sum; fi | cut -d' ' -f1; }

if [ ! -d "$SRC" ]; then
    mkdir -p "$(dirname "$SRC")"
    git clone --depth 1 --branch "$HATARI_TAG" "$HATARI_GIT" "$SRC" >&2
fi

# (Re)apply the patch to clean sources when it is new or has changed.
PATCH_SUM=$(sha1 < "$PATCH")
if [ "$(cat "$SRC/.md-emulator-patch" 2>/dev/null)" != "$PATCH_SUM" ]; then
    if grep -q multidevice.h "$SRC/src/cpu/memory.c"; then
        git -C "$SRC" checkout -- . >&2
        git -C "$SRC" clean -fdq -- src >&2
    fi
    patch -d "$SRC" -p1 < "$PATCH" >&2
    echo "$PATCH_SUM" > "$SRC/.md-emulator-patch"
fi

# Our own files: always refreshed, so changes here reach the build.
cp "$HERE/multidevice.c" "$SRC/src/multidevice.c"
cp "$HERE/multidevice.h" "$SRC/src/includes/multidevice.h"
cp "$ROOT/include/mdemu_plugin.h" "$SRC/src/includes/mdemu_plugin.h"
if ! grep -q multidevice.c "$SRC/src/CMakeLists.txt"; then
    cat >> "$SRC/src/CMakeLists.txt" <<'CMAKE'

# SidecarTridge Multi-device (md-emulator): loads .mdfw firmware plugins
target_sources(${APP_NAME} PRIVATE multidevice.c)
target_link_libraries(${APP_NAME} ${CMAKE_DL_LIBS})
CMAKE
fi

ARCH_ARGS=()
if [ "$(uname -s)" = "Darwin" ]; then
    ARCH_ARGS=(-DCMAKE_OSX_ARCHITECTURES="$(uname -m)")
fi
cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_OSX_BUNDLE=0 ${ARCH_ARGS[@]+"${ARCH_ARGS[@]}"} >&2
cmake --build "$SRC/build" -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)" >&2

# What it was built from, for mdfw run to compare (same files, same order).
cat "$PATCH" "$HERE/multidevice.c" "$HERE/multidevice.h" "$ROOT/include/mdemu_plugin.h" \
    | sha1 > "$SRC/.md-emulator-stamp"
echo "$SRC/build/src/hatari"
