# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "Classic Kurvenrausch - Pseudo-3D OutRun-style racer in C++/SDL2 with ECS";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # VERSION is the only source of truth. Development versions (-dev)
        # get the revision count and short hash appended; releases are used
        # as they are. revCount is missing in shallow clones and some dirty
        # evaluations, hence the fallback.
        versionBase = nixpkgs.lib.strings.fileContents ./VERSION;
        gitRev = "${self.shortRev or self.dirtyShortRev or "dirty"}";
        isDev = nixpkgs.lib.strings.hasInfix "-dev" versionBase;
        version =
          if isDev then
            "${versionBase}.${toString (self.revCount or 0)}+g${gitRev}"
          else
            versionBase;
      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "kurvenrausch";
          inherit version;
          src = ./.;

          nativeBuildInputs = with pkgs; [ cmake pkg-config ];
          buildInputs = with pkgs; [ SDL2 SDL2.dev ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DPROJECT_VERSION_FULL=${version}"
          ];

          installPhase = ''
            mkdir -p $out/bin
            cp kurvenrausch $out/bin/
          '';
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            pkg-config
            gcc
            SDL2
            SDL2.dev
            gdb
            clang-tools
          ];
        };
      });
}
