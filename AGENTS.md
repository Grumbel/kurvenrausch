# AGENTS.md

Standing rules and project overview for anyone (human or agent) working on
this repository.

## Project

**Kurvenrausch** (binary: `kurvenrausch`) is a classic pseudo-3D bitmap racer
in the spirit of *OutRun* and *Lotus Esprit Turbo Challenge*, written in C++17
with SDL2. All drawing is done in software into a low-resolution framebuffer
that is scaled up with nearest-neighbour filtering for chunky pixels.

- Build: `cmake -B build && cmake --build build` (or `nix build`).
- LSP (clangd): `CMAKE_EXPORT_COMPILE_COMMANDS` is on; after `cmake -B build`
  the database is `build/compile_commands.json`. `.clangd` points at `build/`.
  `nix develop` provides `clang-tools` (clangd). Open the repo root in the
  editor so clangd finds `.clangd`.
- Tests: `ctest --test-dir build --output-on-failure` (`tests/tests.cpp`, no
  framework). Add a test for new pure logic.
- Screenshots for the README: `tools/make_screenshots.py` (needs Pillow). Look
  at renders after any rendering change; `--zone N`, `--print-zones`, `--steer`
  and `--wav` exist for exactly this.
- Desktop integration lives in `data/` (desktop entry, AppStream metainfo, man
  page template, icons) and is installed by CMake. The icons are generated from
  the game's `make_app_icon()` by `tools/make_icons.py`; rerun it after
  changing the icon. Keep the man page's options in step with `--help` (a
  test checks it).
- Headless check: `./build/kurvenrausch --screenshot out.bmp --frames 600`
  renders a frame without opening a window. Use this to verify rendering
  changes in environments without a display.

## Rules

- Keep `TODO.md` current: tip, open work, handoff notes. Someone must be able
  to continue from `TODO.md` plus the repository alone.
- Sanitizers: check rendering/physics/audio changes with ASan+UBSan on a full
  headless lap, and anything touching the audio thread with ThreadSanitizer on
  the live loop (`SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`).
- Commit directly to `master`; no bundles or patches as deliverables.
  Exception: work done by Grok is still delivered as cumulative `git bundle`
  files named `kurvenrausch-NNN.M-short-slug-<shortRevOfBase>.bundle`.
- Commit author: `Ingo Ruhnke <grumbel@gmail.com>`; every commit carries the
  trailer naming the model that actually wrote it, e.g.
  `Co-authored-by: Claude Opus 5.5 <noreply@anthropic.com>`.
- Small, task-focused commits; every commit must build warning-free with
  `-Wall -Wextra -Wpedantic`.
<!-- REUSE-IgnoreStart -->
- Licence: GPL-3.0-or-later (changed from the MIT claimed in the old README, at
  the owner's request). Every new file needs the SPDX header, exactly:
  `// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>` and
  `// SPDX-License-Identifier: GPL-3.0-or-later` (`#` comments in CMake/Nix/Python).
  Files that cannot carry one are listed in `REUSE.toml`. `reuse lint` must pass.
  The one exception: `data/*.metainfo.xml` is CC0-1.0, because AppStream
  requires a permissive metadata licence.
<!-- REUSE-IgnoreEnd -->
- Fix root causes; no workarounds. Don't remove features without discussion.

## Architecture notes

- The road is rendered with the classic segment technique (Lou's Pseudo 3D
  page, Jake Gordon's JavaScript racer): a looping array of segments with a
  curve value and per-edge heights, projected with `scale = depth / z`.
  Segments are walked near to far, accumulating curve offset (`x += dx;
  dx += curve`) and tracking `maxy` for hill occlusion; sprites are then drawn
  far to near, clipped against the `maxy` recorded for their segment.
- Vertical projection uses `H/2` and horizontal `W/2` (as in Gordon's racer),
  so the player car's road contact point lands exactly on the bottom screen
  row with `player_z = camera_height * camera_depth`.
