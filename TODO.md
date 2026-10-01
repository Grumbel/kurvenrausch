# TODO

## Current tip

Base: `fc65858 Fix rendering: correct projection, pixel format, colors &
sprites` (upstream master). Work line: kurvenrausch-002.x, bundles are
cumulative from `fc65858`.

Latest bundle: `kurvenrausch-002.1-classic-racer-fc65858.bundle`, tip is
the "Update README and TODO" commit on `master`.

History: 001.1 was built on `d66c91a Initial checkin`. Upstream then added
`cc083f5 Rename project to Kurvenrausch` and `fc65858` (its own rendering
fix), so the work was rebased onto upstream master and the 001.1 bundle was
superseded by 002.1.

Rebase notes: upstream's rename (CMake target, flake, window title) is
kept. Its rendering fix (components/types/renderer/road/game rewrites)
overlaps with, and is superseded by, the road/framebuffer rewrite here;
conflicts were resolved in favour of the rewrite. `Color::to_u32()`
became `Color::argb()`. `flake.nix` builds with CMake and installs the
`kurvenrausch` target, which matches; the Nix build itself still has not
been run (no Nix in the sandbox).

## Round 2 plan (003.x): licence, gamepad, biomes, weather, audio

Requested: README screenshot; SPDX headers (`reuse lint` clean, GPL-3.0-or-later,
"2026 Ingo Ruhnke <grumbel@gmail.com>"); more biomes / weather / countries incl.
a cliff beside the road; synthesised engine and tyre sound; SDL GameController.

Design decisions (so work can be continued from here):

- Licence: the README said MIT, the owner now asks for GPL-3.0-or-later.
  Headers in source files, `REUSE.toml` for docs/other files, text in `LICENSES/`.
- Input: `InputState` becomes analog (throttle, brake, steer in -1..1); keyboard
  is digital 0/1, pads add analog sticks/triggers, merged by max. The pad mapping
  is a pure function so it is unit-tested; SDL only feeds it. Hot-plug + rumble.
- Zones: the track is a list of `Zone`s (country, region, `RoadTheme`, decor rule).
  `RoadTheme` is a blendable "look" (colours, fog density, rain, snow, grip, ...);
  per-segment looks are precomputed with smooth transitions between zones, also
  across the lap seam. Discrete fields (lane count, markings) come from the
  nearer zone. Background, fog, weather and handling read the look at the player.
- Roadside edges: per segment `Edge {None, Rail, Cliff}` per side, with a fade at
  the ends of a run. Drawn as column-filled quads (not sprites) in the far->near
  pass; the ground beyond is rock (cliff) or sea/valley (rail). Both are solid.
- Weather: screen-space rain streaks / snowflakes driven by look intensities;
  fog is the look's density; wet/snow reduce grip (steering authority, more
  centrifugal push, earlier skidding).
- Audio: `Synth` is pure DSP (no SDL) so it can be rendered offline and analysed:
  additive engine fed by rpm/throttle, noise-based road/wind/rain, band-passed
  skid squeal, gravel crackle, barrier scrape, crash burst. `Audio` wraps the SDL
  device; parameters cross threads via atomics. `--wav FILE` dumps a headless run.
- Tools: `tools/make_screenshots.py` renders the README images via headless mode.

## Done (002.1)

- AGENTS.md / TODO.md / .gitignore.
- ECS: const access fixed, destroy() removes entities and components.
- SDL presentation split from software drawing (Display / Framebuffer);
  ARGB8888 channel order fixed; software renderer fallback; leaks fixed;
  F11 / Alt+Enter fullscreen; headless `--screenshot/--frames/--position`.
- Road rewritten the classic way: correct projection units, smooth curve
  accumulation, near->far road with occlusion line, far->near sprites with
  per-segment clip, fog, rumble/grass bands, lane dashes, start line.
  Track data (Segment with y1/y2, RoadTheme) and TrackBuilder with eased
  curve/hill sections. 320x240 framebuffer shown at 3x.
- Fixed 60 Hz physics: speed-scaled steering, centrifugal push, off-road
  slowdown, crashes into solid scenery, start-line lap timing.
- Procedural pixel-art sprites (palm, tree, bush, boulder, billboard,
  gantry, player car x3 frames, 4 traffic liveries) with scaled, clipped,
  fogged blitting; 5x7 bitmap font.
- Parallax backdrop: banded sky, clouds, mountains, hills with haze.
- HUD: lap time, lap, best/last, km/h, rev bar, GO!/LAP/NEW RECORD.
- Traffic as ECS entities with lane changes and rear-end collisions.

Verified: builds warning-free (-Wall -Wextra -Wpedantic, GCC 13); ASan +
UBSan clean over full headless laps; screenshots checked at start line,
curves, crests, downhill, countryside, traffic. Not verified here: the Nix
build (no Nix in the sandbox) and interactive play/feel with a real
display and keyboard.

## Open work / ideas

- Play-test handling and tune: lap is ~2 min at top speed (6855 segments);
  centrifugal force, traffic density and speeds may want tuning.
- Sound (engine pitch by rpm, skid, crash) via SDL audio, synthesised.
- Uphill / downhill car sprite frames; skid/smoke when off-road.
- Camera pitch / horizon shift with slope (OutRun style).
- Multiple themes per track section (beach, desert, night) using
  RoadTheme blending; branching stages.
- Load tracks from a data file.
- Gear shifting (manual/auto) instead of virtual gears in the rev bar.
- Traffic avoids the player more cleverly; cars on curves could show
  turning frames.

## Open questions

- README says MIT but there is no LICENSE file and no SPDX/REUSE headers.
  Add them with the copyright holder's confirmation.
