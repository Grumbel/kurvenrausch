# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# The R36S handheld (RK3326, 640x480) running ArkOS, whose userland is
# Ubuntu 19.10 (eoan): glibc 2.30, libstdc++ from GCC 9, SDL2 2.0.x. A
# binary built against nixpkgs' glibc does not run there, so this builds
# against a sysroot made of eoan's arm64 packages, from the Ubuntu archive
# with their published hashes (Pingus fetches a prebuilt sysroot instead).
# The compiler is nixpkgs' aarch64 cross GCC, but every header and library
# comes from the sysroot: libstdc++ 9's headers with its shared library (so
# no ABI shims are needed), the shared libgcc_s; GCC's own libgcc.a supplies
# only compiler builtins. On the device, SDL2 is ArkOS's (KMS/DRM, its own
# build), the stable SDL2 ABI.
#
#   nix build .#kurvenrausch-r36s                the binary
#   nix build .#kurvenrausch-r36s-portmaster     a PortMaster port: copy to /roms/ports/
#   nix build .#kurvenrausch-r36s-portmaster-zip for PortMaster's autoinstall

{ pkgs
, version
, date
}:

let
  lib = pkgs.lib;

  # eoan arm64 packages: glibc, the kernel headers, GCC 9's runtime and
  # libstdc++ headers, SDL2 2.0.10 (headers and library to link against).
  debs = [
    { file = "pool/main/g/glibc/libc6_2.30-0ubuntu2_arm64.deb"; sha256 = "4853da1e3c9eae109708b30dc5df00b7ced888c09f0e77bf6731015a97fb859a"; }
    { file = "pool/main/g/glibc/libc6-dev_2.30-0ubuntu2_arm64.deb"; sha256 = "a0b1860c16c50934a5fe3206ac386267d24b015b0ab677005d38e60ac1d27823"; }
    { file = "pool/main/l/linux/linux-libc-dev_5.3.0-18.19_arm64.deb"; sha256 = "7575a37b1abf722f950fa8202c49f24b9c887dcf8e1f97bad15b555cb8c483de"; }
    { file = "pool/main/g/gcc-9/libgcc1_9.2.1-9ubuntu2_arm64.deb"; sha256 = "81dfdee2c50f13dab98967b0062e51f1cebb967f407e3593c5a77ed2146ea008"; }
    { file = "pool/main/g/gcc-9/libstdc++6_9.2.1-9ubuntu2_arm64.deb"; sha256 = "5af692b4ecf237979411acf74b7319785e09dd3450a917a94f3ea65cd260436e"; }
    { file = "pool/main/g/gcc-9/libstdc++-9-dev_9.2.1-9ubuntu2_arm64.deb"; sha256 = "aa0efe55ba5002e2d3572271b6c0106dfb59f7a6c61be0a0c406c4836e12472b"; }
    { file = "pool/main/g/gcc-9/libgcc-9-dev_9.2.1-9ubuntu2_arm64.deb"; sha256 = "4e4a83c4d83471eff726fd819b4e5d266fd01c0fbb66f1376718b78bfd4ae116"; }
    { file = "pool/universe/libs/libsdl2/libsdl2-2.0-0_2.0.10+dfsg1-1ubuntu1_arm64.deb"; sha256 = "34f06694f13bbee317b89be868768e8fd5b030287431a630881f45ff99dc9264"; }
    { file = "pool/universe/libs/libsdl2/libsdl2-dev_2.0.10+dfsg1-1ubuntu1_arm64.deb"; sha256 = "9eb97797c392b68449045d814f8763ca984853abefff94ababdc72270b7d65ea"; }
  ];
  fetchDeb = d: pkgs.fetchurl {
    url = "http://old-releases.ubuntu.com/ubuntu/${d.file}";
    inherit (d) sha256;
  };

  sysroot = pkgs.stdenvNoCC.mkDerivation {
    pname = "arkos-sysroot";
    version = "eoan-arm64";
    dontUnpack = true;
    nativeBuildInputs = [ pkgs.dpkg ];
    installPhase = ''
      mkdir -p $out
      ${lib.concatMapStrings (d: "dpkg-deb -x ${fetchDeb d} $out\n") debs}
      # Absolute symlinks point into the device's root: into the sysroot.
      find $out -type l | while read -r link; do
        target=$(readlink "$link")
        case "$target" in
          /*) ln -sfn "$out$target" "$link" ;;
        esac
      done
      # GCC 16's drivers link -lgcc_s_asneeded, a script of its own: the
      # same here, for eoan's libgcc_s.
      echo 'INPUT ( AS_NEEDED ( -lgcc_s ) )' > $out/usr/lib/gcc/${multiarch}/9/libgcc_s_asneeded.so
    '';
    # aarch64 files: the host's fixups must keep off.
    dontFixup = true;
  };

  cross = pkgs.pkgsCross.aarch64-multiplatform.stdenv.cc;
  gcc = cross.cc;
  binutils = cross.bintools.bintools;
  tp = lib.removeSuffix "-" cross.targetPrefix; # aarch64-unknown-linux-gnu
  multiarch = "aarch64-linux-gnu";
  gccLib = "${gcc}/lib/gcc/${tp}/${gcc.version}";

  # Compiler drivers that see nothing but the sysroot (and GCC's own
  # freestanding headers and builtins).
  cflags = lib.concatStringsSep " " [
    "-nostdinc"
    "--sysroot=${sysroot}"
    "-B${binutils}/${tp}/bin"
    "-isystem ${gccLib}/include"
    "-isystem ${sysroot}/usr/include/${multiarch}"
    "-isystem ${sysroot}/usr/include"
    "-march=armv8-a -mtune=cortex-a35"
    # GCC 10+ calls helpers for atomics (__aarch64_ldadd4_rel ...) that
    # GCC 9's libgcc lacks; the Cortex-A35 (ARMv8.0, no LSE) would take the
    # plain instructions anyway.
    "-mno-outline-atomics"
  ];
  cxxflags = lib.concatStringsSep " " [
    "-nostdinc -nostdinc++"
    "--sysroot=${sysroot}"
    "-B${binutils}/${tp}/bin"
    "-isystem ${sysroot}/usr/include/c++/9"
    "-isystem ${sysroot}/usr/include/${multiarch}/c++/9"
    "-isystem ${sysroot}/usr/include/c++/9/backward"
    "-isystem ${gccLib}/include"
    "-isystem ${sysroot}/usr/include/${multiarch}"
    "-isystem ${sysroot}/usr/include"
    "-march=armv8-a -mtune=cortex-a35"
    # GCC 10+ calls helpers for atomics (__aarch64_ldadd4_rel ...) that
    # GCC 9's libgcc lacks; the Cortex-A35 (ARMv8.0, no LSE) would take the
    # plain instructions anyway.
    "-mno-outline-atomics"
  ];
  ldflags = lib.concatStringsSep " " [
    "-B${sysroot}/usr/lib/${multiarch}"
    "-B${gccLib}"
    "-L${sysroot}/usr/lib/gcc/${multiarch}/9"
    "-L${sysroot}/usr/lib/${multiarch}"
    "-L${sysroot}/lib/${multiarch}"
    "-Wl,-rpath-link,${sysroot}/usr/lib/${multiarch}:${sysroot}/lib/${multiarch}"
    "-Wl,--dynamic-linker=/lib/ld-linux-aarch64.so.1"
  ];
  # Linking C++: the sysroot's libstdc++, never GCC's own (-nostdlib++).
  driver = name: compiler: flags: linkFlags: libs: pkgs.writeShellScript name ''
    for a in "$@"; do
      case "$a" in
        -c|-S|-E|-M|-MM) exec ${gcc}/bin/${tp}-${compiler} ${flags} "$@" ;;
      esac
    done
    exec ${gcc}/bin/${tp}-${compiler} ${flags} ${linkFlags} "$@" ${libs}
  '';
  cc = driver "arkos-gcc" "gcc" cflags ldflags "-lm";
  cxx = driver "arkos-g++" "g++" cxxflags "-nostdlib++ ${ldflags}" "-lstdc++ -lm";

  # What the game links against instead of eoan's libSDL2, which needs
  # ALSA, PulseAudio, X11, Wayland ... (not in the sysroot): a library of the
  # same name exporting the same functions, empty. The device loads its own
  # SDL2 by that name at run time.
  sdlStub = pkgs.runCommand "libSDL2-link-stub" { } ''
    mkdir -p $out/lib
    ${binutils}/bin/${tp}-nm -D --defined-only ${sysroot}/usr/lib/${multiarch}/libSDL2-2.0.so.0 \
      | awk '$2 == "T" { print "void " $3 "(void) {}" }' > stub.c
    ${cc} -shared -fPIC -o $out/lib/libSDL2.so -Wl,-soname,libSDL2-2.0.so.0 stub.c
  '';

  r36s = pkgs.stdenv.mkDerivation {
    pname = "kurvenrausch-r36s";
    inherit version;
    src = lib.cleanSource ../.;
    nativeBuildInputs = [ pkgs.cmake ];
    cmakeFlags = [
      "-DCMAKE_SYSTEM_NAME=Linux"
      "-DCMAKE_SYSTEM_PROCESSOR=aarch64"
      "-DCMAKE_SYSROOT=${sysroot}"
      "-DCMAKE_C_COMPILER=${cc}"
      "-DCMAKE_CXX_COMPILER=${cxx}"
      "-DCMAKE_AR=${binutils}/bin/${tp}-ar"
      "-DCMAKE_RANLIB=${binutils}/bin/${tp}-ranlib"
      "-DCMAKE_STRIP=${binutils}/bin/${tp}-strip"
      "-DCMAKE_BUILD_TYPE=Release"
      "-DCMAKE_SKIP_RPATH=ON"
      "-DBUILD_TESTING=OFF"
      "-DPROJECT_VERSION_FULL=${version}"
      "-DPROJECT_DATE=${date}"
      # eoan's SDL2 CMake files name /usr: give the sysroot's directly.
      "-DSDL2_INCLUDE_DIRS=${sysroot}/usr/include"
      "-DSDL2_LIBRARIES=${sdlStub}/lib/libSDL2.so"
      # Mali-G31 / ArkOS: ES 2.0 context (see SuperTux Origins SUPERTUX_R36S).
      "-DENABLE_OPENGLES2=ON"
    ];
    # A device binary: no nix store RPATH, interpreter or shebangs.
    dontPatchELF = true;
    dontPatchShebangs = true;
    postInstall = ''
      ${binutils}/bin/${tp}-strip $out/bin/kurvenrausch
    '';
    dontStrip = true;
  };

  # PortMaster: the launcher script, the port folder and its metadata.
  portmaster = pkgs.stdenvNoCC.mkDerivation {
    pname = "kurvenrausch-r36s-portmaster";
    inherit version;
    dontUnpack = true;
    dontPatchShebangs = true; # the launcher runs with the device's bash
    dontFixup = true;
    installPhase = ''
      mkdir -p $out/kurvenrausch
      install -m755 ${r36s}/bin/kurvenrausch $out/kurvenrausch/kurvenrausch
      cp ${../LICENSES/GPL-3.0-or-later.txt} $out/kurvenrausch/LICENSE.txt
      cp ${../data/icons/hicolor/256x256/apps/io.github.grumbel.kurvenrausch.png} $out/kurvenrausch/cover.png
      cp ${../mk/r36s/screenshot.png} $out/screenshot.png
      cp ${../mk/r36s/Kurvenrausch.sh} $out/Kurvenrausch.sh
      chmod +x $out/Kurvenrausch.sh
      substitute ${../mk/r36s/port.json} $out/port.json --subst-var-by version ${version}
      cp ${../mk/r36s/README.md} $out/README.md
    '';
  };

  portmasterZip = pkgs.runCommand "kurvenrausch-${version}-r36s-portmaster.zip" { nativeBuildInputs = [ pkgs.zip ]; } ''
    mkdir -p $out
    cd ${portmaster}
    zip -r -9 $out/kurvenrausch.zip .
  '';

in {
  inherit sysroot r36s portmaster portmasterZip;
}
