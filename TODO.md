# TODO

## Current tip

Base: `fc65858 Fix rendering: correct projection, pixel format, colors &
sprites` (upstream master). Work line: kurvenrausch-003.x, bundles are
cumulative from `fc65858`.

Latest bundle: `kurvenrausch-004.1-rear-view-mirror-fc65858.bundle`, tip is
the "Show the mirror in the README screenshots, update TODO" commit on `master`.
004.1 builds on 003.1 (`fe8e0df`) and adds the rear-view mirror.

History: 001.1 was built on `d66c91a`; upstream then added a rename and its own
rendering fix, so the work was rebased onto `fc65858` (002.1). 003.1 adds the
licence change, gamepad, zones/biomes, weather, cliffs, audio and screenshots.
Upstream master has since been fast-forwarded to `64fda32`, the tip of 002.1, so
003.1 applies on top of it as a plain fast-forward; the bundles stay cumulative
from `fc65858` regardless, as the rules require.
The rebase notes: upstream's rename is kept, its rendering fix is superseded by
the road/framebuffer rewrite, `Color::to_u32()` became `Color::argb()`.

## Round 3 (004.1): rear-view mirror, done

Requested: a mirror at the top of the screen showing what goes on behind the
car. Delivered as a 112x30 mirror centred at the top, in a housing on a stem.

Design decisions:

- No separate renderer: `RoadView::direction = -1` makes `RoadRenderer` walk
  the segments backwards from the camera (`cam_z = (z - camera) * direction`,
  near/far edges and heights swapped, loop seam handled both ways). Lateral
  positions keep their sign, which is exactly the mirror image (left stays
  left), so nothing is flipped; curves accumulate with the same sign (a
  right-hand bend curves right behind the car as well). Unit-tested in
  `test_road_mirror`.
- `RoadView::horizon` / `y_scale` let a view put eye level anywhere and keep
  the main view's proportions in a framebuffer of another shape; 0 keeps the
  old H/2 behaviour, the main view renders bit-identically to before.
- The mirror camera sits at the car (not the chase camera), 700 units up,
  depth 1.2 (narrower than the main view); constants at the top of game.cpp.
  It renders into its own `Framebuffer` and `RoadRenderer` (the slices are
  per renderer), then `Framebuffer::blit` copies it into the housing.
- Backdrop: `BackdropView {horizon, zoom, mirror}`. Mirrored, the layers show
  the opposite half of their panorama, flipped and scrolling the other way;
  no sun (it is ahead). Cloud positions are now altitudes above the horizon.
- Sprites: traffic shows its front in the mirror (`make_car_front`, driver on
  the left as a mirror shows it), billboards their back
  (`SpriteSheet::scenery_back`); other scenery is the same from both sides.
- HUD: the lap counter moved from the top centre to under the lap time; the
  country banner and the lap messages moved down a little to clear the mirror.

Verified: builds warning-free, unit tests, ASan+UBSan on a full headless lap,
on positions around the track including both sides of the lap seam, and on
the live loop with the dummy drivers; `nix build` (now available, works);
screenshots of every zone, the start gantry and billboard backs behind,
traffic behind, uphill behind, bends.
Not verified: how it looks/feels in interactive play on a real display.

Ideas: a key to toggle the mirror; weather drops on the mirror; headlights of
cars behind in a night stage.

## Round 2 (003.1): done

Requested and delivered: README screenshots; SPDX headers with
`reuse lint` clean (GPL-3.0-or-later, "2026 Ingo Ruhnke <grumbel@gmail.com>");
biomes, weather, countries and a cliff road; synthesised engine and tyre sound;
SDL GameController support.

Design decisions (so work can be continued from here):

- Licence: the README used to say MIT; the owner asked for GPL-3.0-or-later.
  Headers in source files, `REUSE.toml` for docs/other files, text in `LICENSES/`.
- Input: `InputState` is analog (throttle, brake, steer in -1..1); keyboard is
  digital 0/1, pads add analog sticks/triggers, merged by max. The pad mapping
  is a pure function (`merge_pad`) and unit-tested; SDL only feeds it. Hot-plug
  and rumble are handled in `Input`.
- Zones: the track is a list of `Zone`s (country, region, `RoadTheme`, decor
  rule). `RoadTheme` is a blendable look (colours, fog density, rain, snow,
  grip, sun, haze, ...); `Track::finish()` precomputes per-segment looks with
  smooth transitions, also across the lap seam. Discrete fields (lane count,
  markings) come from the nearer zone. `mix_themes()` must list every field;
  a `static_assert` on `sizeof(RoadTheme)` reminds you when you add one.
  Background, fog, weather, handling and sound read the look at the player.
- Roadside edges: per segment `Edge {None, Rail, Cliff}` per side, with a fade
  at the ends of a run (`Track::edge_height`). Drawn as column-filled quads in
  the far->near pass, not as sprites; ground beyond is rock (cliff) or
  sea/valley (rail). Solid via `barrier_limit()`. Decor never goes on an edge
  side (checked by a test).
- Weather: screen-space rain streaks / snowflakes driven by look intensities;
  fog is the look's density; wet/snow reduce grip (steering authority, more
  centrifugal push, earlier skidding).
- Audio: `Synth` is pure DSP (no SDL) so it is rendered offline in the tests and
  analysed by spectrum; `Audio` wraps the SDL device; parameters cross threads
  as atomics. `--wav FILE` dumps the sound of a headless run.
- Lap detection counts only forward crossings of the start line
  (`crossed_line_forward`); the old check counted any backward step as a lap.

Verified: every commit builds warning-free (-Wall -Wextra -Wpedantic, GCC 13)
and passes the unit tests and `reuse lint`; ASan/UBSan clean on full headless
laps (also with `--wav`) and on the live loop with SDL's dummy drivers;
ThreadSanitizer clean on the live loop (audio thread vs game thread); gamepad
path checked with SDL's virtual joystick; screenshots checked for every zone,
the zone transitions and the cliff/rail stretches; sound checked by spectrum
(engine pitch tracks the revs, effect bands, levels, no clipping).

Not verified: how the sound actually sounds to a human or on real audio
hardware; a physical gamepad; the Nix build (no Nix in the sandbox); how the
game feels to drive interactively. The autopilot laps in about 2'12" (traffic
included) against about 1'47" at top speed, because it does not dodge cars.

## Done earlier (002.1)

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

- Play-test handling and tune: grip values per zone (rain 0.8, snow 0.7),
  traffic density and speeds, scrape drag, centrifugal force.
- Listen to the sound on real hardware and tune the mix (gains are in
  `Synth::render`; the tests pin the relations, not absolute levels). Ideas:
  traffic whoosh / Doppler, thunder with lightning in the rain, tunnels with
  reverb, a radio / music.
- Real gamepad test; rumble strength tuning; remappable buttons.
- Night stage with headlights; lightning flashes; tunnels.
- Uphill / downhill car sprite frames; tyre smoke when skidding.
- Camera pitch / horizon shift with slope (OutRun style).
- Branching stages, as in OutRun; load tracks from a data file.
- Manual gearbox instead of the virtual automatic gears.
- Smarter autopilot (dodge traffic, brake for grip); smarter traffic; turning
  frames for cars in bends.
- Country entry signs at the zone borders.

## Open questions

None at the moment.
