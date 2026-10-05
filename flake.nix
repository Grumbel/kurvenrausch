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
            "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
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

        # ── nix develop helpers (biltoo / Pingus pattern) ───────────────
        # Real PATH scripts so `nix develop -c kurvenrausch-run` works.
        # Out-of-tree cmake/ninja, Debug by default, build-before-run, gdb.
        kurvenrauschDevPreamble = ''
          set -euo pipefail
          if [ -z "''${KURVENRAUSCH_SOURCE:-}" ]; then
            echo "$0: KURVENRAUSCH_SOURCE is not set (enter the shell with: nix develop)" >&2
            exit 1
          fi
          if [ ! -f "$KURVENRAUSCH_SOURCE/CMakeLists.txt" ]; then
            echo "$0: KURVENRAUSCH_SOURCE does not look like a Kurvenrausch tree: $KURVENRAUSCH_SOURCE" >&2
            exit 1
          fi
          KURVENRAUSCH_BUILD_DIR="''${KURVENRAUSCH_BUILD_DIR:-/tmp/kurvenrausch-build}"

          _kurvenrausch_canon_path() {
            local p="$1"
            p="''${p%/}"
            if [ -d "$p" ]; then
              ( cd "$p" && pwd )
            else
              printf '%s\n' "$p"
            fi
          }
        '';

        kurvenrauschConfigure = pkgs.writeShellScriptBin "kurvenrausch-configure" (
          kurvenrauschDevPreamble
          + ''
            _ccache_args=()
            if command -v ccache >/dev/null 2>&1; then
              _ccache_args+=(
                -DCMAKE_C_COMPILER_LAUNCHER=ccache
                -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
              )
            fi
            cmake -S "$KURVENRAUSCH_SOURCE" -B "$KURVENRAUSCH_BUILD_DIR" -G Ninja \
              -DCMAKE_BUILD_TYPE="''${CMAKE_BUILD_TYPE:-Debug}" \
              "''${_ccache_args[@]}"
            # clangd: .clangd uses CompilationDatabase: build — point it at
            # the out-of-tree dir without forcing an in-tree build/.
            ln -sfn "$KURVENRAUSCH_BUILD_DIR" "$KURVENRAUSCH_SOURCE/build"
          ''
        );

        kurvenrauschBuild = pkgs.writeShellScriptBin "kurvenrausch-build" (
          kurvenrauschDevPreamble
          + ''
            if [ ! -f "$KURVENRAUSCH_BUILD_DIR/build.ninja" ] && [ ! -f "$KURVENRAUSCH_BUILD_DIR/Makefile" ]; then
              kurvenrausch-configure || exit 1
            fi

            cache="$KURVENRAUSCH_BUILD_DIR/CMakeCache.txt"
            if [ -f "$cache" ]; then
              cached="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache" | head -n1 || true)"
              cached="$(_kurvenrausch_canon_path "$cached")"
              want="$(_kurvenrausch_canon_path "$KURVENRAUSCH_SOURCE")"
              if [ -n "$cached" ] && [ "$cached" != "$want" ]; then
                echo "kurvenrausch-build: source path changed since configure:" >&2
                echo "  cmake cache: $cached" >&2
                echo "  current:     $want" >&2
                echo "  → re-running kurvenrausch-configure" >&2
                kurvenrausch-configure || exit 1
              elif [ -n "$cached" ] && [ ! -f "$cached/CMakeLists.txt" ]; then
                echo "kurvenrausch-build: cached source tree is gone: $cached" >&2
                echo "  → re-running kurvenrausch-configure with $want" >&2
                kurvenrausch-configure || exit 1
              fi
            fi

            cmake --build "$KURVENRAUSCH_BUILD_DIR" "$@"
          ''
        );

        kurvenrauschRun = pkgs.writeShellScriptBin "kurvenrausch-run" (
          kurvenrauschDevPreamble
          + ''
            kurvenrausch-build || exit 1
            if [ ! -x "$KURVENRAUSCH_BUILD_DIR/kurvenrausch" ]; then
              echo "kurvenrausch-run: $KURVENRAUSCH_BUILD_DIR/kurvenrausch missing after build" >&2
              exit 1
            fi
            # No --datadir: the game has no runtime data tree (desktop files
            # are install-time only). SDL finds itself via the develop shell.
            exec "$KURVENRAUSCH_BUILD_DIR/kurvenrausch" "$@"
          ''
        );

        kurvenrauschRunGdb = pkgs.writeShellScriptBin "kurvenrausch-run-gdb" (
          kurvenrauschDevPreamble
          + ''
            kurvenrausch-build || exit 1
            if [ ! -x "$KURVENRAUSCH_BUILD_DIR/kurvenrausch" ]; then
              echo "kurvenrausch-run-gdb: $KURVENRAUSCH_BUILD_DIR/kurvenrausch missing after build" >&2
              exit 1
            fi
            if ! command -v gdb >/dev/null 2>&1; then
              echo "kurvenrausch-run-gdb: gdb not found (should be in the nix develop shell)" >&2
              exit 1
            fi
            # -ex run: start immediately. Quit gdb only on normal exit (0).
            gdb -q \
              -ex "set pagination off" \
              -ex "set confirm off" \
              -ex "set debuginfod enabled off" \
              -ex run \
              -ex 'python
try:
  ec = gdb.parse_and_eval("$_exitcode")
  if int(ec) == 0:
    gdb.execute("quit")
except Exception:
  pass
' \
              --args "$KURVENRAUSCH_BUILD_DIR/kurvenrausch" "$@"
          ''
        );

        # ccacheStdenv + inputsFrom: SDL2 from the package; scripts on PATH.
        kurvenrauschDevShell =
          pkgs.mkShell.override { stdenv = pkgs.ccacheStdenv; } {
            inputsFrom = [ kurvenrausch ];
            packages = (with pkgs; [
              cmake
              ninja
              gdb
              ccache
              pkg-config
              # clangd + clang-tidy + clang-format (LSP).
              clang-tools
              # Validators for desktop entry / AppStream / man (ctest).
              desktop-file-utils
              appstream
              mandoc
            ]) ++ [
              kurvenrauschConfigure
              kurvenrauschBuild
              kurvenrauschRun
              kurvenrauschRunGdb
            ];
            CMAKE_BUILD_TYPE = "Debug";
            shellHook = ''
              export KURVENRAUSCH_SOURCE="$PWD"
              export KURVENRAUSCH_BUILD_DIR="''${KURVENRAUSCH_BUILD_DIR:-/tmp/kurvenrausch-build}"

              export CCACHE_DIR="''${CCACHE_DIR:-$HOME/.cache/ccache-kurvenrausch}"
              mkdir -p "$CCACHE_DIR" 2>/dev/null || true
              export CMAKE_C_COMPILER_LAUNCHER=ccache
              export CMAKE_CXX_COMPILER_LAUNCHER=ccache

              # Host LD_LIBRARY_PATH with /usr/lib can break nix linking.
              if [ -n "''${LD_LIBRARY_PATH:-}" ]; then
                case ":$LD_LIBRARY_PATH:" in
                  *:/usr/lib*|*:/lib*)
                    echo "note: clearing LD_LIBRARY_PATH (contained system lib dirs that break nix linking)"
                    unset LD_LIBRARY_PATH
                    ;;
                esac
              fi

              echo "kurvenrausch dev shell (CMAKE_BUILD_TYPE=''${CMAKE_BUILD_TYPE:-Debug}, ccacheStdenv)"
              echo "  source:    $KURVENRAUSCH_SOURCE"
              echo "  build dir: $KURVENRAUSCH_BUILD_DIR"
              echo "  kurvenrausch-configure       # cmake once; then kurvenrausch-build is incremental"
              echo "  kurvenrausch-build [args]    # incremental cmake --build"
              echo "  kurvenrausch-run [args]      # build + run"
              echo "  kurvenrausch-run-gdb [args]  # build + gdb -q -ex run; quit on normal exit"
              echo "  also: nix develop -c kurvenrausch-run"
              echo "  nix build                    # RelWithDebInfo package (no ccache)"
            '';
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

        devShells.default = kurvenrauschDevShell;
      });
}
