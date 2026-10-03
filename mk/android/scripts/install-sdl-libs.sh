#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Collects build-sdl-libs.sh's results into $out: lib/<abi>/libSDL2.so,
# include/ (SDL's headers) and dex/classes.dex.
# Env: TARGET_ABIS (e.g. "armeabi-v7a arm64-v8a"), out
set -euo pipefail
mkdir -p "$out/lib" "$out/dex" "$out/include"
for abi in $TARGET_ABIS; do
  mkdir -p "$out/lib/$abi"
  cp sdl-jni/libs/"$abi"/*.so "$out/lib/$abi/"
done
cp -r sdl-jni/SDL/include/. "$out/include/"
cp classes/classes.dex "$out/dex/classes.dex"
