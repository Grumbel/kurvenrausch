#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds a static SDL2 for wasm32 with Emscripten, offline (no port
# download), from the same recipe as Pingus and SuperTux Origins.
#
# Env:
#   SDL_SRC  - SDL2 source tree (required)
#   PREFIX   - install prefix (default: $PWD/prefix)
#   NIX_BUILD_CORES / JOBS - parallel jobs
set -euo pipefail

export EM_CACHE="${EM_CACHE:-${TMPDIR:-/tmp}/emcache}"
mkdir -p "$EM_CACHE"

PREFIX="${PREFIX:-$PWD/prefix}"
mkdir -p "$PREFIX/lib" "$PREFIX/include"

if [ -z "${SDL_SRC:-}" ]; then
  echo "error: SDL_SRC required" >&2
  exit 1
fi

echo "==> SDL2 static (wasm32) -> $PREFIX"
cp -a "$SDL_SRC" SDL2-src
chmod -R u+w SDL2-src
mkdir -p build-sdl2
cd build-sdl2
emcmake cmake ../SDL2-src \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DSDL_SHARED=OFF \
  -DSDL_STATIC=ON \
  -DSDL_TEST=OFF \
  -DSDL_STATIC_PIC=ON
emmake make -j"${NIX_BUILD_CORES:-${JOBS:-$(nproc)}}"
emmake make install
cd ..

if [ ! -f "$PREFIX/lib/libSDL2.a" ]; then
  echo "error: no libSDL2.a under $PREFIX/lib" >&2
  exit 1
fi
echo "==> SDL2 ready under $PREFIX"
