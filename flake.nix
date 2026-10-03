# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "Classic Kurvenrausch - Pseudo-3D OutRun-style racer in C++/SDL2 with ECS";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    # SDL2's official prebuilt MinGW package for the Windows builds
    # (nix/windows.nix), as Pingus uses it.
    SDL2-win32.url = "github:grumnix/SDL2-win32";
    SDL2-win32.inputs.nixpkgs.follows = "nixpkgs";
    SDL2-win32.inputs.flake-utils.follows = "flake-utils";

    # SDL2 sources for the WebAssembly build (nix/wasm.nix).
    sdl2-src = {
      url = "https://github.com/libsdl-org/SDL/releases/download/release-2.30.3/SDL2-2.30.3.tar.gz";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, flake-utils, SDL2-win32, sdl2-src }:
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

        # The date of the last change, for the man page (YYYY-MM-DD).
        lastModified = self.lastModifiedDate or "19700101000000";
        date = "${builtins.substring 0 4 lastModified}-${builtins.substring 4 2 lastModified}-${builtins.substring 6 2 lastModified}";

        kurvenrausch = pkgs.stdenv.mkDerivation {
          pname = "kurvenrausch";
          inherit version;
          src = ./.;

          nativeBuildInputs = with pkgs; [ cmake pkg-config ];
          buildInputs = with pkgs; [ SDL2 SDL2.dev ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DPROJECT_VERSION_FULL=${version}"
            "-DPROJECT_DATE=${date}"
          ];
        };

        # The ports, built on Linux hosts: the web page (Emscripten), Windows
        # (MinGW), the R36S handheld (ArkOS) and Android (the SDK and NDK,
        # which are unfree).
        r36s = import ./nix/r36s.nix { inherit pkgs version date; };
        androidPkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
          config.android_sdk.accept_license = true;
        };
        android = import ./nix/android.nix {
          pkgs = androidPkgs;
          sdlSrc = sdl2-src;
          sdlVersion = "2.30.3";
          buildToolsVersion = "30.0.3";
          packagePlatform = "22";
          compilePlatform = "33";
          targetAbis = [ "armeabi-v7a" "arm64-v8a" "x86_64" ];
          androidSdk = (androidPkgs.androidenv.composeAndroidPackages {
            platformVersions = [ "22" "33" ];
            buildToolsVersions = [ "30.0.3" ];
            includeNDK = true;
            ndkVersion = "29.0.14206865";
            includeEmulator = false;
            includeSources = false;
          }).androidsdk;
          inherit version;
          versionCode = self.revCount or 1;
        };
        windows = import ./nix/windows.nix {
          inherit pkgs version date;
          sdl2Win64 = SDL2-win32.packages.${system}.SDL2-win64;
          sdl2Win32 = SDL2-win32.packages.${system}.SDL2-win32;
        };

        # Decided by the name, so listing the outputs for other systems does
        # not need their nixpkgs.
        isLinux = nixpkgs.lib.hasSuffix "-linux" system;
        wasm = import ./nix/wasm.nix {
          inherit pkgs version gitRev date;
          sdlSrc = sdl2-src;
          sdlVersion = "2.30.3";
        };
      in {
        packages = {
          default = kurvenrausch;
          inherit kurvenrausch;
        } // nixpkgs.lib.optionalAttrs isLinux {
          sdl2-wasm = wasm.sdl2Wasm;
          kurvenrausch-wasm = wasm.kurvenrauschWasm;
          kurvenrausch-win64 = windows.win64;
          kurvenrausch-win64-zip = windows.win64Zip;
          kurvenrausch-win32 = windows.win32;
          kurvenrausch-win32-zip = windows.win32Zip;
          kurvenrausch-android = android.apk;
          sdl2-android = android.sdlAndroidLibs;
          arkos-sysroot = r36s.sysroot;
          kurvenrausch-r36s = r36s.r36s;
          kurvenrausch-r36s-portmaster = r36s.portmaster;
          kurvenrausch-r36s-portmaster-zip = r36s.portmasterZip;
        };

        apps = nixpkgs.lib.optionalAttrs isLinux {
          kurvenrausch-wasm = wasm.serveApp;
          install-android-kurvenrausch = android.installApp;
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
            # Validators for the desktop entry, AppStream data and man page,
            # run by ctest when present.
            desktop-file-utils
            appstream
            mandoc
          ];
        };
      });
}
