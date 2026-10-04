# TODO

## Current tip

Tip is Round 17 (touch controls at screen resolution, the wide screen,
escapable police).
Work is committed directly to `master`; Grok still delivers cumulative git
bundles (see AGENTS.md).

Earlier history: the round numbers below
(003.1 ... 008.1) are those of the bundles that carried the earlier rounds,
cumulative from `fc65858 Fix rendering: correct projection, pixel format,
colors & sprites`; the last one, `kurvenrausch-008.1-san-francisco-fc65858`,
ended at the "Document San Francisco and the jumps" commit. 004.1 added the
rear-view mirror on top of 003.1 (`fe8e0df`); 005.1 horn, close-pass wave and
boost, and nitro; 006.1 brake lights, the crash animation, the mini map, fuel
and gas stations; 007.1 the VERSION file, new vehicles, the engine sound,
weather vection and wet spots; 008.1 San Francisco and jumps.

History: 001.1 was built on `d66c91a`; upstream then added a rename and its own
rendering fix, so the work was rebased onto `fc65858` (002.1). 003.1 adds the
licence change, gamepad, zones/biomes, weather, cliffs, audio and screenshots.
Upstream master has since been fast-forwarded to `64fda32`, the tip of 002.1, so
003.1 applied on top of it as a plain fast-forward.
The rebase notes: upstream's rename is kept, its rendering fix is superseded by
the road/framebuffer rewrite, `Color::to_u32()` became `Color::argb()`.

## Round 17: touch controls at screen resolution, the wide screen

The touch controls stopped at the 4:3 picture's edge on phones: with
`SDL_RenderSetLogicalSize` SDL maps finger events onto the picture and
clamps the ones on the black bars to its edge. Now:
- `Display` sets no logical size; it scales the framebuffer into
  `picture()` itself, so fingers stay relative to the whole window
  (`touch_to_screen()`, `screen_to_framebuffer()` for the menu taps)
- `Overlay` (`overlay.hpp`): discs, text and rectangles as alpha images,
  made once per look and cached as textures by `Display::present()`, drawn
  over the picture at screen resolution; `Overlay::draw()` paints them into
  the framebuffer for headless screenshots (screen = 320x240 there)
- `TouchControls::set_layout(TouchLayout)`: the pedals keep to the screen's
  right edge, the steering pad to its left, sizes scale with the screen's
  height (`unit()`); the pause button stays on the picture
- The wide screen: SCREEN in the pause menu (`PauseMenu::Screen`,
  `MenuAction::ToggleWide`, saved as `wide` in the choices). `Game::width_`
  follows the screen's shape each frame (`screen_width()`, 240 high, 320 ..
  `max_width` 640, even) via `set_width()`, which resizes the framebuffer,
  the display's texture and the weather. The horizontal scale stays
  `x_unit` (160 pixels per world unit, `RoadView::x_scale`), so a wider
  picture shows more to the sides; the HUD keeps to the edges, the mirror
  centres, the cockpit's dashboard is centred with its edges carried on.
  `--width W` for headless screenshots
- Not tested on a real phone yet; the live switch only on a 4:3 dummy
  window (the headless runs cover the wide rendering)
- Police: escapable by driving well. Closing in near, the police car runs
  at `chase_pace` (0.97) of the standard car's top speed, up to
  `chase_catch_up` more when far behind, so a car kept flat out holds it
  off until it gives up (`chase_give_up`); falling below that pace lets it
  catch up. Escape (lead or time) is reported once, the police car then
  drops back (`Leaving`, `giving_up`). HUD: POLICE and an escape meter
  under the mirror (`escape_progress()`). Pulled over, the forced brake no
  longer engages reverse (`reverse_armed_` held off)

## Round 16: day and night, drivable cars, attract mode

The owner's list: day/night cycle (`4e64766`), every car drivable with
sports and everyday dealers (`693e2bc`), touch fix (`5d9543f`), controller
remap without chords (`79c5e8f`), and the attract mode:
- `Game::start_attract()` at start-up and after `attract_idle_seconds` (120)
  without `InputState::any_input` (a key, button, tap, a held finger, a
  stick or trigger pushed well over); any input leaves it with `reset()`
- `update_attract()`: only the traffic, background, weather, clock and
  audio run; the hidden player car sits `player_z` behind the followed car
  (`attract_car_`, a Car/Van/Truck, a new one every 20 s, alternating the
  Chase and Far views; the player's view comes back afterwards). No laps,
  police or collisions; `update_traffic` leaves the player out of the movers
- HUD: zone banner, title, blinking prompt (`HudState::attract`)
- `--attract` for headless screenshots

## Round 15: Android min SDK 22 (Android 5.1)

Lowered the Android package / min SDK from API 24 (Android 7) to API 22
(Android 5.1), matching Pingus. Changes:
- `flake.nix`: `packagePlatform = "22"`, `platformVersions = [ "22" "33" ]`
- `mk/android/app/AndroidManifest.xml`: `minSdkVersion="22"` (target stays 33)
- README Android section updated

No code changes; the build pipeline already took `PACKAGE_PLATFORM` from
the flake. Not re-tested in an API 22 emulator here (no display / long
build); the same NDK 29 + SDL 2.30.3 path works for Pingus on 22.

Rebased onto master after upstream added: mini-map zoom (`720c2cf`), police
stop from in front (`2b74d7d`), and light switches / headlights / indicators /
hazards (`4cab353`).

## Round 14: ports (touch, Windows, Android, R36S), done

All built on Linux from the flake; `nix flake show` lists them.
- Touch controls (`touch.hpp`): drag-steering pad, GAS/BRK/NOS/HB/H, pause
  button right of the mirror, taps on the pause menu (`menu_tap`,
  `PauseMenu::choose`). Shown once the screen is touched; `--touch X,Y`
  holds fingers headless. Tested in Chromium (touch events) and Android.
- Windows (`nix/windows.nix`): MinGW cross build with the prebuilt
  SDL2-win32 flake, GUI subsystem, icon resource (`data/kurvenrausch.rc.in`,
  `data/icons/kurvenrausch.ico` from `tools/make_icons.py`), zips for win64
  and win32. Tested under Wine (headless and live with Xvfb + xdotool).
  State in `%APPDATA%\grumbel\kurvenrausch` (SDL_GetPrefPath).
- Android (`nix/android.nix`, `mk/android/`): ndk-build after Pingus, SDL2
  2.30.3, NDK 29, API 22+ (Android 5.1; was 24), three ABIs, landscape via
  SDL_HINT_ORIENTATIONS, back button pauses. Tested in an API 24 x86_64
  emulator without KVM (slow but works). Note: on the rotated emulator
  `adb input` x coordinates are rescaled; SDL's normalised coordinates were
  right. Min SDK lowered to 22 to match Pingus.
- R36S / ArkOS (`nix/r36s.nix`, `mk/r36s/`): sysroot from Ubuntu eoan
  arm64 .debs (hashes from the archive's Packages index), libstdc++ 9
  headers + nixpkgs GCC 16, SDL2 link stub, `-mno-outline-atomics`,
  `libgcc_s_asneeded` script. PortMaster zip. Tested under qemu-aarch64 on
  an eoan userland (`-L` root of libsdl2's 43-package closure). Not tested
  on real hardware: the pad mapping comes from PortMaster's control.txt.
- Fixed on the way: `Store::make_dir()` on Windows (bare drive `C:`).

Open: real-device tests (R36S, a phone, Windows); the libc++_shared.so in
the APK is large (unstripped by the NDK?); an Android release key.

## Round 13: cliff relief, WebAssembly build, done

- Cliffs: ridges and gullies with lit and shaded flanks, ledges, a ragged
  top (`cliff_relief` in `road.cpp`).
- WebAssembly, after Pingus and SuperTux Origins: `nix build
  .#kurvenrausch-wasm` makes the site, `nix run .#kurvenrausch-wasm` serves
  it. SDL2 2.30.3 is a flake input built offline (`sdl2-wasm`); the game
  builds with `emcmake`, the CMake `EMSCRIPTEN` branch adds the link flags
  and the shell and installs the site. `Game::frame()` is one frame, driven
  by `emscripten_set_main_loop` in the browser; the `Game` lives on the heap.
  The state directory (`/home/web_user/.local/state`) is an IDBFS mount,
  synced after every write. Verified in headless Chromium over the DevTools
  protocol: driving, pause from the page, Esc, records surviving a reload.
- Open: touch controls for phones; the gamepad in the browser is untested;
  `ScriptProcessorNode` audio (SDL2's) is deprecated in browsers.

## Round 12: the owner's list, done

Requested (the owner's untracked `TODO` file in the repo root, which stays
untracked and uncommitted, plus requests made along the way), each its own
commit:
- Handbrake (Ctrl, left shoulder): the rear slides, sharper turns, smoke.
- Reverse: brake to a stop, let go, press again (`reverse_armed`).
- Faster steering at low speed (`steer_rate`); smoother fork entry
  (`fork_bend_push`, fixed draw order of the two roads).
- Driver and passenger as a layer (`make_occupants`), seen through the rear
  window of closed cars, the player's and the traffic's.
- Pause menu on Start / P: resume, restart, start in a chosen country, quit.
- Dirt (puddles, crashes, oil; rain rinses it slowly) and car washes.
- Lots generalised (`Lot`, `Track::lots`, one offer panel, `--visit KIND`):
  car washes, motels (passenger), hospitals (driver; a crash leaves a
  bandage until a hospital), truck stops (the Big Rig, `truck_model`).
  Changing car gives a clean car with a full tank.
- State in `$XDG_STATE_HOME/kurvenrausch/` (`state.hpp`): `choices` (car,
  driver, passenger, view, radio) written atomically, `laps.tsv` appended;
  the fastest lap there is BEST. Headless runs never touch it.
- Camera views (C / Back): chase, far, bumper, cockpit (`views.hpp`; only
  the picture changes, the physics keep the chase camera as reference).
- Dynamic weather (`climate.hpp`): a drifting front, per-zone `showers`,
  storms darken and thicken the fog and cut grip, lightning and thunder.
- Radio (`music.hpp`): three sequenced songs, N / right stick flick.
- Police chases (`police.hpp`): the police car is a `Traffic` entity of
  kind `Police` moved by `Game::update_police`, never closer than
  `police_min_gap` (7 segments: further would hide it below the mirror's
  edge, closer it would be drawn over the car).

Handoff notes:
- Headless options added: `--handbrake`, `--brake`, `--pause`, `--dirt`,
  `--wash`, `--visit`, `--view`, `--storm`, `--music`, `--police`, `--car 4`
  (the truck). The autopilot visits lots slowly; start it well before one
  (`--zone 5 --frames 1600 --dealer` stands at the Italian dealer).
- Sanitizer builds need SDL3's library on the path (sdl2-compat dlopens it):
  `LD_LIBRARY_PATH=$(nix-store -qR $(ldd build/kurvenrausch | grep -i libSDL2
  | awk '{print $3}' | cut -d/ -f1-4) | grep sdl3 | head -1)/lib`.
- `RoadTheme` has a size tripwire in `mix_themes()`; new fields need blending.
- Not verified by ear: the thunder, siren and music levels were checked by
  measurement only (RMS, peaks, band power).

## Round 11: smooth forks, six more countries, done

Requested: the transitions where the road splits feel rough; even more
countries (Korea, India, Africa and so on).

Forks, what was rough and what changed:
- The S-bends switched a constant curvature on, flipped it halfway and
  switched it off: now a full sine over 70 segments (`TrackBuilder::fork`),
  same 3.75 half-widths apart.
- The other road was cut off where the routes had parted and popped up where
  they meet: now it stays alongside the whole way at that distance
  (`update_branch_offsets`), with its own scenery (`other_route_segment()`,
  drawn shifted by the offset); scenery facing it stays between the roads
  (offset plus width at most 2.6, `facing_branch`); none at all in the bends
  (`branch_bend`).
- Changing road snapped the view to the new road's heading and position:
  now only while the roads overlap (centres under 1.5 apart), with a margin
  (`nearer_other_road`, no flipping while they coincide), and the camera keeps
  where it stood and looked (`RoadView::yaw` and `shift`, measured at the
  camera, which is a few segments behind the car) and moves over in 0.35 s.
- Measured on renders of every step through both forks: the biggest jump
  between frames fell from 1.8 to 1.2 times the usual going straight through,
  from 2.3 to 1.4 when changing road late. Traffic on a fork still drives on
  whichever route is active.

Countries (16 zones now): Egypt (Nile), Kenya (Masai Mara), India
(Rajasthan), Korea (Seoraksan), Australia (Outback) and Brazil (Amazonas),
with `make_theme()` for the common part of a theme, a decor rule each and
fifteen sprites. Order: Europe, Egypt, Kenya, India, Korea, Japan,
Australia, the USA, Brazil (ends the lap). Each has a gas station.

Verified: builds warning-free; tests (branches: smooth curvature, offsets and
headings, scenery between the roads; 16 zones; the rest as before);
ASan+UBSan through both forks changing road, crashes in each new zone, a
12000-step lap; screenshots of every new zone and sprite.

## Round 10: more countries, narrow roads, oil, car dealers, done

Requested: more countries, some with different streets (England with narrow
roads); rare oil slicks; puddles mostly in the rain regions; a car
dealership to change cars.

Design decisions:

- Puddles: the fair-weather chance dropped from 0.3% to 0.03% per segment;
  the rest follows rain and snow. Tested per zone (dry zones at most 3,
  properly wet ones at least 3, over ten times the density).
- Road width (`RoadTheme::road_scale`, blended like the rest of a look):
  `Track::half_width()` per boundary and `half_width_at()` along the track
  replace the constant wherever half-widths meet world units (projection,
  camera, scenery and traffic placement, fork offsets, the car against the
  road). At the standard width renders are pixel-identical to before.
- Zones (10 now): England (Cotswolds, road 0.62 wide, white edge lines, hedges
  and dry-stone walls at 1.12, oaks, phone boxes, drizzle) and the Netherlands
  (Holland: flat, tulips, windmills, a canal behind a rail) between France
  and Germany; Japan (Fuji: cherry trees, torii, stone lanterns, a big
  snow-capped mountain, road 0.75 wide) after San Francisco, ending the lap.
  Each has a gas station. Traffic still all drives one way (no left-hand
  traffic shown in England or Japan).
- Patches: wet spots became `Segment::patch` (Water, Oil) with
  `patch_under()`; rounder (root of a sine). Oil: about four a lap, any zone;
  above 15% of top speed 8% grip, a twitch, the sprite flicking, squeal.
- Cars: `CarModel` (Spider, GT Coupe, Hot Hatch, Muscle; top speed,
  acceleration, grip factors; none dominates). `apply_car()` sets the Player;
  grip multiplies the road's. Traffic and the speedometer use the standard top
  speed (`base_max_speed_`), so km/h differ by car; the revs are the car's own.
- Dealers: `TrackBuilder::car_dealer()` (a forecourt lot like a gas station's,
  `Segment::dealer`, showroom and sign; refuelling ignores them), in Tuscany
  and California. Stopped on the forecourt the HUD shows the offer; each push
  of the steering changes car (with a chime); you leave in the one showing.
  Headless `--car N` and `--dealer` (the autopilot pulls in and stays).

Verified: builds warning-free; tests (puddles per zone, oil, road width
blending and drawing, car models, sprites per model, zones by name);
ASan+UBSan (a full lap in the Muscle car with sound, a dealer visit with a
switch, an oil slick, steering crashes in England, Holland and Japan, the
live loop); `reuse lint`; screenshots of the three countries, oil, puddles,
the dealer and three other cars.
Not verified: how the cars and the narrow lanes feel in play.

## Round 9: Linux desktop integration, done

Requested: a man page, a .desktop file, an icon and the rest of the Linux
desktop integration. (Also: links in the README credits.)

- App ID `io.github.grumbel.kurvenrausch` (the GitHub home, reverse DNS):
  `data/<id>.desktop` (Game;ArcadeGame;, StartupWMClass = the ID),
  `data/<id>.metainfo.xml` (AppStream: description, URLs, screenshot from the
  README image on GitHub, controls, OARS, provides the binary; CC0-1.0 as
  AppStream demands; no <releases> yet, add one per release),
  `data/kurvenrausch.6.in` (man page, section 6, version and the last commit's
  date filled in by CMake; Nix passes the date from `self.lastModifiedDate`).
- Icon: `make_app_icon()`, 32x32 pixel art drawn by the game (a road into a
  sunset with the red car); the window icon (`Display::set_icon`) and
  `--icon FILE`; `tools/make_icons.py` writes hicolor PNGs (16 averaged; 32,
  64, 128, 256 whole multiples) and a scalable SVG of pixel rectangles. The
  window's X11 class / Wayland app ID is the app ID so desktops match it.
- CMake install rules (GNUInstallDirs) for all of it; the flake's package now
  uses them. Tests: the man page documents every `--help` option; the
  validators (`desktop-file-validate`, `appstreamcli validate`, `mandoc -T
  lint`) run when found, the dev shell has them.

Verified: all five tests pass with the validators; `cmake --install` and
`nix build` produce bin/, applications/, metainfo/, icons/hicolor/ (5 sizes
and scalable) and man6/; `reuse lint`; ASan on the live loop and `--icon`.
Not verified: how a real desktop shows it (the icon in a launcher, the window
matched to the entry under X11 and Wayland).

## Round 8: rolling tyres, forks, done

Requested: a spinning effect on the tyres (flicker, fitting the style);
branches in the road to different subsections of the track.

Design decisions:

- Tyres: every vehicle sprite (rear, front, the player's car) in three tread
  frames, the tread rows shifted a pixel each; `SpriteSheet::tyre_frame()`
  picks one per 25 units driven, so slow they roll, fast they flicker. Traffic
  uses its position, the player the distance its wheels rolled.
- Forks (`Branch`, `TrackBuilder::fork()`): two routes of exactly the same
  length that join again, so every position after the join is the same either
  way (lap length, zones, start line, traffic need no special handling). Only
  the active route's segments are in `Track::segments`; `choose_branch()`
  swaps them. Each route starts with a flat S-bend away from the other (50
  segments, ending 3.75 half-widths apart either side) and ends with one
  back; the shorter middle is padded back to the starting height. The other
  route's road is drawn beside ours (road, rumble strips, lane dashes) from
  per-boundary offsets (`branch_offset()`, the renderer's curve recurrence on
  the difference of the routes' curves), only where they part and meet; in
  between it is out of sight. Scenery keeps off the side facing the other
  route there (`Segment::facing_branch`). Both routes are decorated and get
  wet spots. Signs with arrows stand 10 segments before the fork; the HUD
  names both routes for 200 segments before it.
- Choosing: while the routes part, the car is on whichever road it is
  nearer to (`nearer_other_road()`); crossing over switches the active route
  and re-measures the car's lateral position from it. The choice is announced
  as the roads separate; the mini map is recomputed for the chosen route.
  Traffic keeps its positions and so is on whichever route is active.
- Forks: Germany AUTOBAHN (fast, gentle) / LANDSTRASSE (twisty, hilly), after
  the gas station; Arizona ROUTE 66 (open desert) / CANYON ROAD (between rock
  walls). The default (and the autopilot's) is the right route.

Verified: builds warning-free; tests (tread frames, both forks: equal
length, heights, decor, the offsets at fork/apart/join, swapping, the choice
rule); ASan+UBSan through both forks taking the left route, a full lap and
the live loop; screenshots of the approach, both routes parting, both forks.
Not verified: how choosing feels in play. Known simplification: traffic on
a fork drives on whichever route is active, so cars near the fork can jump
across when the player switches while the roads still overlap.

## Round 7 (008.1): San Francisco, hills, valleys and jumps, done

Requested: a San Francisco biome with huge hills and valleys and jumps.

Design decisions:

- Zone 7, "USA / SAN FRANCISCO", after the California coast; the lap's final
  descent (`downhill_to_end`) now ends it. City look: concrete sidewalks for
  grass, grey kerbs, pale sky with haze, two lanes with US markings.
  `Decor::City`: Victorian row houses (three colours) every 4 segments on
  both sides at 1.35, the odd tree, street lamps at the kerb.
- Streets: `TrackBuilder::slope(len, grade, curve)` adds straight constant
  grades, so the grade changes abruptly at each crossing (6 flat segments),
  as on the real hills. Grades 35-55%, a valley floor in the middle, a gas
  station (every zone has one). The existing hills are eased and never jump.
- Jumps (`step_vertical`, tested): on the road the car keeps the rate it
  actually rose at over the last step; if carrying on with that under gravity
  (40000 units/s^2, arcade-strong so flights last ~0.5 s) would clear the road
  by more than 15 units, it takes off. A 50% crest launches it above about a
  third of top speed. In the air: no steering, traction or brakes (nitro
  still pushes), no collisions with scenery or traffic (you can fly over
  cars); the camera follows 70% of the height and the car sprite lifts by the
  rest (capped at 40 px); the revs flare. Landing: thump, dust, a rumble and a
  short squash, a little speed lost, by impact. Any teleport of the car
  (restart, crash recovery, knock-backs) calls `place_on_road()` first.
- The zone sheet in the README is now 4 columns wide for 7 zones.

Verified: builds warning-free, tests (jump physics, the zone's grades,
heights, jumps at top speed and none in the Black Forest, decor; 7 zones);
ASan+UBSan through San Francisco (with nitro and a crash), a full lap and the
live loop; `reuse lint`; screenshots of the streets and a jump from take-off
to landing. Not verified: how the jumps feel to play; the car's own shadow
lifts with it in the air (no separate ground shadow, the road under the car
is below the bottom of the screen).

## Round 6 (007.1): version, vehicles, engine, vection, wet spots, done

Requested: a VERSION file (owner's rules: single source of truth, `-dev` in
git, Nix dev builds append `.revCount+g<rev>`, `--version`); vans, trucks
and rare rival sports cars; a more dynamic engine with more bass; rain and
snow with vection according to the speed; wet spots on the road.

Design decisions:

- VERSION: `0.1.0-dev`; CMake reads it into `PROJECT_VERSION_FULL` (or takes
  `-DPROJECT_VERSION_FULL`), gives `project()` the numeric part and the game
  `KURVENRAUSCH_VERSION`; the flake builds the dev string with
  `self.revCount or 0`. No other docs for it, as asked. Release: set VERSION
  without `-dev`, commit, tag `v` + version, bump to the next `-dev`.
- Vehicles (`vehicles.hpp`, tested): kind, width, speed range, colour count
  and share (cars 55%, vans 22%, trucks 17%, rivals 6%). Pixel art at the
  cars' 6.25 units per pixel, rear views with indicator and brake variants,
  front views for the mirror. Trucks are slow, so traffic queues behind them.
  A rival cruises at 78-90% and races at 97% of the player's top speed while
  the player is within 30 segments ahead of it or 7.5 behind.
- Engine: measured before changing (the level only followed the throttle,
  bass under 200 Hz fell from ~90% to ~8% of the energy above mid revs, no
  transients). Now every firing kicks a pipe resonance at twice the firing
  frequency and a 68 Hz body resonance; the three firings of a revolution
  differ, keeping bass at the revolution rate; level builds with the revs;
  a surge on opening (slow throttle follower), exhaust pops on the overrun
  (also on upshifts). Four tests pin this and fail on the old engine. Not
  heard on real speakers.
- Vection: particles also flow out from the vanishing point at
  `outflow_rate * speed * distance` (more for near ones); streaks follow the
  motion and grow with the outflow only, so standing looks as before; at
  speed they are fed back in around the middle. Tested.
- Wet spots: `Segment::wet_x/wet_w`, tapering like forecourts; chance per
  segment 0.003 + 0.03 * max(rain, snowfall), never in the desert, clear of
  the start and forecourts. Above 35% of top speed the car aquaplanes (grip
  x0.35, twitch, drag, pad buzz); spray and mist particles, splash sound.

Verified: every commit builds warning-free and passes the tests; ASan+UBSan
(full lap with horn and nitro, refuelling, crashes in three zones, live
loop); TSan (live loop, harness incl. splash); `reuse lint`; screenshots of
rain and snow standing and at speed, wet spots and aquaplaning spray, the new
sprites, a truck and a van in traffic and a truck's cab in the mirror.
Not verified: the sound on speakers; a rival up close in a run (its sprites
were checked); how aquaplaning feels.

## Round 5 (006.1): brake lights, crash animation, mini map, gas stations, done

Requested: working brake lights; a crash animation when going off course at
high speed; a mini map; a gas station.

Design decisions:

- Brake lights: lit variants of every rear car sprite (white-hot lamps, light
  spilling onto the bodywork, a third brake light; unlit lamps darker so the
  difference shows even on the red car). The player's light up with the brake
  pedal. Traffic now really brakes: behind something slower in its lane that
  it can't pass (another car, or the player) it follows (`follow_speed`,
  tested) instead of driving through it, and its brake lights show when it
  slows by more than 2% of top speed (no flicker while following steadily).
- Crash: hitting solid scenery off the road at 40% of top speed or more
  starts it (slower is the old knock). `crash_pose(t, side)` (pure, tested)
  drives it: two rolls through three decaying hops with a slide outwards
  (1.5 s), at rest (0.5 s), then put back in the outermost lane on that side
  while blinking (0.8 s). Dust puffs and debris are screen-space particles; a
  thump on each touchdown. Input is ignored meanwhile, nitro is cut. The car
  sprite is rotated with the new `Framebuffer::blit_rotated` via
  `RoadSprite::angle`. The trigger is the collision, as in OutRun: running
  onto open ground still just slows the car. Ask if leaving the road at speed
  should crash on its own too.
- Mini map: `track_map()` (tested). A pseudo-3D track isn't a geometric loop
  (the bends add up to 0.58 turns here) and taken literally its hard bends
  turn 170 degrees and cross over. So bends are damped (x0.6), the missing
  turning is spread evenly so the lap makes one full turn, and the remaining
  gap is spread along the path. Top right, under the lap times.
- Fuel: `Fuel` (tested) burns with the engine load, a tank lasts 100 s at
  full load (about 1.25 laps in practice). Below 3% the engine sputters, empty
  it dies (no drive, engine sound off, no nitro); standing empty for 3 s the
  driver pours in a 15% spare can. HUD gauge bottom left, blinking red when
  low. Not refilled on laps.
- Gas stations: `TrackBuilder::gas_station()`: a flat straight with a paved
  forecourt on the right (`Segment::forecourt`, out to 2.3 half-widths,
  tapering in and out), a sign before it, two pumps and the shop. One per zone
  (tested), placed off the cliff/rail runs; decor keeps clear. No off-road
  slowdown or gravel on the forecourt. Below 8% of top speed on the full-width
  forecourt the tank fills in 4 s, with a pump hum and gurgle and a chime when
  full. The route is 336 segments longer for it, so zone positions moved.
- Headless: `--fuel L` starts with L of a tank, `--steer-from N` starts the
  held steering at step N (for crashes). The autopilot pulls in when below
  45% with a station within 120 segments.

Verified: builds warning-free; unit tests (follow speed, fuel, crash pose and
touchdowns, rotated blits, the map's closure and orientation, stations, the
engine-off, pump and chime sounds); ASan+UBSan on full laps (with refuelling,
horn, nitro, running dry, crashes in several zones) and the live loop; TSan on
the live loop and on a harness switching engine/pump and triggering chimes
from another thread; screenshots of brake lights, a full crash sequence, the
map, stations in every climate, refuelling and running dry.
Not verified: interactive feel (crash threshold, fuel range, how easy pulling
in is: steering is speed-scaled, so it is easier to move over before slowing
right down), sounds on real speakers.

## Round 4 (005.1): horn, close-pass wave and boost, nitro, done

Requested: a horn on a button that the other drivers react to; a hand
gesture and a little boost whenever passing near another car; nitro on a
button.

Design decisions:

- Buttons: horn H / pad X or left shoulder (held), nitro Space / pad Y or
  right shoulder (a burn starts on the press). `InputState::horn/nitro`,
  mapped in the pure `merge_pad` (tested).
- Rules live in `driving.hpp` (pure, tested): `Nitro` (3 canisters, refilled
  on every lap, 3 s burns: +150% thrust, top speed x1.3, fading over the last
  0.4 s), `limit_speed` (throttle can't pass the top speed; above it the car
  slows by 15% of top speed per second), `boosted_speed` (+8% of top speed,
  not beyond x1.1), `close_pass` (gap to the car goes from ahead to behind,
  centres within two car widths), `yield_lane`.
- Horn reaction: cars up to 60 segments ahead within 0.45 half-widths of the
  player's line pull over to the nearest free lane out of the line, faster
  than a normal lane change ("startled" for 1.5 s). They never cut across the
  player's line unless dead ahead in it, and then go to the side they lean to.
  The honk is checked every step while held, so cars entering the range react.
- Indicators: every lane change (passing or yielding) blinks the indicator of
  that side; derived from `target_x` vs `x`, no extra state. Rear sprites and
  the mirror's front sprites have signal variants.
- Close pass: per-car `Traffic::gap` remembers the signed distance to the
  player's car; the step it flips from ahead to behind with the cars close
  sideways is a pass: whoosh (`Synth::trigger_whoosh`), boost, a pad pulse and
  a 1.2 s wave. Passed on the left the driver waves out of the left side, on
  the right the passenger out of the right. The player sprites have 12 rows
  of headroom for the arm (`SpriteSheet::player_headroom`).
- Nitro visuals: a white-hot burst with flickering tongues flaring outwards
  and up from each exhaust (the pipes are too close to the bottom of the
  screen for a flame streaming at the camera); HUD canisters bottom right,
  the burning one draining. Sound: a deep roar plus hiss, engine at full load.
- Horn sound: 415 + 523 Hz (a major third), soft-clipped and filtered.
- Headless: `--horn` holds the horn, `--nitro N` presses nitro at step N.

Verified: builds warning-free; unit tests (input mapping, nitro, speed rules,
pass detection, yielding, horn/nitro/whoosh spectra); ASan+UBSan on full laps
with horn and nitro and in several zones, and on the live loop; TSan on the
live loop and on a harness setting horn/nitro and triggering whooshes and
crashes from one thread while another renders; `reuse lint`; screenshots of
the flames, the HUD, the wave on both sides, and a honked car blinking and
pulling over before being passed with a wave.
Not verified: how it plays and sounds interactively (horn and whoosh levels,
whether the boost and nitro feel right); a real pad's button layout.

Ideas: traffic honking back or flashing; a nitro FOV kick or speed lines;
refill nitro by close passes as well; a rude gesture variant for being cut
off.

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
  `Synth::render`; the tests pin the relations, not absolute levels), the
  radio's songs, thunder and siren included. Ideas: traffic whoosh / Doppler,
  tunnels with reverb, more songs.
- Real gamepad test; rumble strength tuning; remappable buttons.
- Night stage with headlights; tunnels.
- Police: traffic pulling over for the siren; fines on the lap time instead
  of only the stop.
- Lap records per car or per route in `laps.tsv` (all the data is there).
- Uphill / downhill car sprite frames; tyre smoke when skidding.
- Camera pitch / horizon shift with slope (OutRun style).
- Branching stages, as in OutRun; load tracks from a data file.
- Manual gearbox instead of the virtual automatic gears.
- Smarter autopilot (dodge traffic, brake for grip); smarter traffic; turning
  frames for cars in bends.
- Country entry signs at the zone borders.

## Open questions

None at the moment.
