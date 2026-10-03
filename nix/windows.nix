# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Windows builds, cross-compiled with MinGW (pkgsCross), as in Pingus: SDL2
# is the official prebuilt MinGW package (the SDL2-win32 flake). Each build
# is a folder with kurvenrausch.exe and the DLLs it needs, and a zip of it.
#
#   nix build .#kurvenrausch-win64       the folder
#   nix build .#kurvenrausch-win64-zip   kurvenrausch-<version>-win64.zip
#   (and -win32 for 32-bit Windows)

{ pkgs
, sdl2Win64
, sdl2Win32
, version
, date
}:

let
  lib = pkgs.lib;

  mkGame = { crossPkgs, sdl2, arch }:
    crossPkgs.stdenv.mkDerivation {
      pname = "kurvenrausch-${arch}-build";
      inherit version;
      src = lib.cleanSource ../.;
      nativeBuildInputs = [ pkgs.cmake ];
      buildInputs = [ sdl2 crossPkgs.windows.mcfgthreads ];
      cmakeFlags = [
        "-DCMAKE_BUILD_TYPE=Release"
        "-DPROJECT_VERSION_FULL=${version}"
        "-DPROJECT_DATE=${date}"
        "-DBUILD_TESTING=OFF"
        "-DSDL2_DIR=${sdl2}/lib/cmake/SDL2"
      ];
    };

  # The folder to ship: the program, its DLLs, the licence and a readme.
  mkFolder = { crossPkgs, sdl2, arch }:
    let game = mkGame { inherit crossPkgs sdl2 arch; };
    in pkgs.runCommand "kurvenrausch-${arch}-${version}" { } ''
      mkdir -p $out
      cp ${game}/bin/kurvenrausch.exe $out/
      cp ${sdl2}/bin/SDL2.dll $out/
      cp ${crossPkgs.windows.mcfgthreads}/bin/libmcfgthread-[0-9]*.dll $out/ # the one the program imports
      cp ${../LICENSES/GPL-3.0-or-later.txt} $out/COPYING.txt
      cat > $out/README.txt <<'EOF'
      Kurvenrausch ${version}
      A classic pseudo-3D racer: one lap around the world.

      Start kurvenrausch.exe. Arrow keys or WASD drive (up accelerates, down
      brakes and, once stopped, reverses), Space is nitro, Ctrl the
      handbrake, H the horn, C changes the view, N the radio, P or Esc
      pauses, F11 switches to fullscreen. Gamepads work too.

      Lap times and choices are kept in %APPDATA%\grumbel\kurvenrausch.

      Licence: GPL-3.0-or-later (COPYING.txt).
      Source: https://github.com/Grumbel/kurvenrausch
      EOF
      sed -i 's/^      //; s/$/\r/' $out/README.txt
    '';

  mkZip = { folder, arch }:
    pkgs.runCommand "kurvenrausch-${version}-${arch}.zip" { nativeBuildInputs = [ pkgs.zip ]; } ''
      mkdir -p $out
      work=$(mktemp -d)
      cp -r --no-preserve=mode ${folder} "$work/kurvenrausch-${version}-${arch}"
      cd "$work"
      zip -r -9 $out/kurvenrausch-${version}-${arch}.zip kurvenrausch-${version}-${arch}
    '';

  win64 = mkFolder { crossPkgs = pkgs.pkgsCross.mingwW64; sdl2 = sdl2Win64; arch = "win64"; };
  win32 = mkFolder { crossPkgs = pkgs.pkgsCross.mingw32; sdl2 = sdl2Win32; arch = "win32"; };

in {
  inherit win64 win32;
  win64Zip = mkZip { folder = win64; arch = "win64"; };
  win32Zip = mkZip { folder = win32; arch = "win32"; };
}
