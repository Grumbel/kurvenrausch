# Kurvenrausch

Classic pseudo-3D bitmap racer in the spirit of *OutRun*, *Lotus Esprit Turbo
Challenge* and *Pole Position*: a lap through Europe and the USA with weather,
cliff roads, traffic, and a synthesised engine.

Written in **C++17** with **SDL2**. The picture is a low-resolution
framebuffer (320x240, or 640x480 in HD) scaled up with nearest-neighbour
filtering for chunky pixels, drawn by either of two renderers that look the
same: a classic software renderer, scanline by scanline, or an OpenGL ES 2
renderer that draws the road, sprites, night lighting, HUD and mirror on the
GPU (the default where GL is available; it keeps handhelds like the R36S at
60 fps). All art is generated procedurally as pixel art at startup and all
sound is synthesised on the fly: there are no asset files. Packaged with a
**Nix flake**.

![Kurvenrausch on the Cote d'Azur corniche](docs/screenshot.png)

One lap takes you around the world through sixteen zones, each with its own country, scenery,
weather and road markings, fading smoothly into one another:

![The sixteen zones: France, England, the Netherlands, Germany, Switzerland, Italy, Egypt, Kenya, India, Korea, Japan, Australia, Arizona, California, San Francisco, Brazil](docs/zones.png)

## Features

- Segment-based pseudo-3D road: perspective projection (`scale = depth / z`),
  curves by accumulated lateral offset, hills and crests with correct
  occlusion, alternating rumble strips and grass bands, lane markings, a
  chequered start line, and distance fog (aerial perspective toward blue air)
- **Sixteen zones** with blended looks: the Cote d'Azur (palms, sunny),
  England (narrow lanes between hedgerows and stone walls, drizzle), the
  Netherlands (dead flat: windmills, tulip fields, a canal), the Black Forest
  (firs in the rain), the Alps (snowfall, chalets, a pass), Tuscany (cypress
  avenues at sunset), Egypt (date palms along the Nile, pyramids), Kenya
  (acacias, giraffes and Kilimanjaro under an orange sky), India (banyans,
  temples, cows, haze), Korea (red maples and hanok gates in autumn), Japan
  (cherry blossom, shrine gates, a snow-capped volcano), the Australian
  outback (red earth, gum trees, Uluru), Arizona (desert, cacti, mesas,
  telephone poles), the California coast (sandstone cliff and sea in thick
  fog), San Francisco (steep streets between Victorian row houses) and the
  Amazon rainforest in a downpour; a banner announces each country. Roads
  differ in width as well as in markings
- **Two tracks**, picked in the pause menu's RACE SETUP: the Small World
  above, and the Grand Tour, more than twice as long, which stops in two or
  three cities of every country (Paris, London, Amsterdam, Muenchen, Roma,
  Cairo, Mumbai, Tokyo, Sydney, Las Vegas, Los Angeles, Rio de Janeiro,
  ...): town
  houses and shops in Europe, glass towers in Tokyo or Sydney, flat-roofed
  houses in Cairo or Delhi, the country's own trees between them and lit
  windows at night. Each track keeps its own lap record
- **Cliffs and guard rails** along the road: strata-textured rock walls that
  rise and fall with the terrain, rails above the sea or a valley; both are
  solid, the car scrapes along them and throws sparks
- **Weather**: rain streaks, swaying snowflakes, fog, a sun, tinted clouds and
  haze; wet and icy roads reduce grip. Driving, the rain and snow stream out
  of the vanishing point in ever longer streaks the faster you go. The
  weather changes as you drive: fronts pass over, the rainy zones dry up for
  a while, storms darken the sky, thicken the fog and make the road
  slippery, each climate its own way (monsoon in India, the odd desert
  thunderstorm, never a drop in Egypt), with lightning and rolling thunder in
  the worst of it
- **Day and night**: a day passes in eight minutes (a clock under the lap
  counter): sunrise and sunset redden the sky, at night the picture darkens
  to blue with stars overhead while tail lights, indicators, headlights, lit
  windows and neon keep shining (lights are marked as such where the pixel
  art is painted); the headlights (L) light the road ahead
- **Attract mode**: the game starts, as in an arcade, following the cars of
  the traffic around the world (a new one every 20 seconds), and goes back
  to it after two minutes without anybody at the controls; any key, button,
  stick or tap starts a race, or, after the break, goes back to the race
  where it was left. EXTRAS → WATCH DEMO starts it from the menu; how long
  it waits (or never) and whether it shows any text are set there too
- **Wet spots**: puddles on the road in the rainy and snowy zones; hit one
  fast and the car aquaplanes, throwing up spray. Rarely, an **oil slick**:
  hardly any grip at all
- Roadside scenery as scaled, fogged pixel-art sprites; objects behind a crest
  peek over it
- **Two renderers**: software (the reference) or OpenGL ES 2 / desktop GL
  (`--renderer auto|software|gles`, F8 switches while playing, the choice is
  kept). The GPU path draws the same scene: road strips, a sprite atlas, the
  night lightmap with the software light formulas evaluated per pixel, the
  HUD, menus, particles and the rear-view mirror. OPTIONS → VIDEO → RESOLUTION
  picks SD (320x240) or HD (640x480), RENDERER the scene path; DEBUG →
  PRESENT how the picture reaches the window (SDL or GL)
- Parallax backdrop: copper-banded sky, sun and moon on a day-long arc, drifting clouds, mountains and hills
  scrolling at different rates through bends
- Road markings per region: dashed white lines in Europe, double yellow centre
  line and white edge lines in the USA
- AI traffic of cars in seven colours, hatchbacks, pickups, vans, slow
  trucks and buses (city buses, coaches, school buses), and the rare rival
  sports car that
  races you when you come close; it changes lanes to pass, blinking its
  indicators, or brakes behind slower vehicles (and you) when it can't;
  rear-ending one slows you down
- **Oncoming traffic**: one lane of every road comes towards you, headlights
  and all, on the country's side of the road: the left lane where traffic
  keeps right, the right lane in England, Japan, Australia, India and Kenya
  (three-lane roads are two lanes your way and one against). Meet a car
  head-on and you crash; it swerves and brakes when it sees you coming
- Working brake lights, on your car and on the traffic; tyres that roll and
  flicker at speed
- **Horn**: cars ahead in your line signal and pull over to let you through
- **Close passes**: squeeze past a car and your driver (or passenger, on the
  right) waves, with a whoosh and a little boost beyond top speed
- **Nitro**: three canisters per lap, each a three second burn with flames
  from the exhausts, more thrust and a higher top speed
- **Rear-view mirror** at the top of the screen: the road behind, drawn by the
  same renderer looking back, with the fronts of the cars you have passed
- **Forks**: the road splits OutRun-style into two routes that join again
  later, in smooth S-bends, the other road in view alongside; signposted and
  named on screen: the Autobahn or the Landstrasse in
  Germany, Route 66 or the Canyon Road in Arizona; keep to the side of the
  road you want to take
- **Jumps**: San Francisco's streets climb and drop at up to 55% grade, flat
  at every crossing; take a crest fast and the car flies, landing with a thump
- Arcade handling with analog steering and pedals; centrifugal force, off-road
  slowdown; hit something off the road at speed and the car **tumbles** over
  in a cloud of dust and debris before it is put back on the road
- **Fuel and gas stations**: the tank drains with the engine's load (about a
  lap and a quarter at full throttle); every zone has a gas station with a
  forecourt to pull onto, where the tank fills up while you stand by the
  pumps. Down to the reserve, a message says how far it is to the next one. Run dry and the engine sputters and dies; stranded, the driver pours
  in a spare can after a few seconds (sized to reach the next gas station)
- **Chemical plants**: in Germany, Korea, Arizona and Rotterdam; stand on
  the forecourt and the nitro canisters fill up again, one by one
- **Car dealers**: stop on the forecourt and steer left or right to choose
  a car, each with its own top speed, acceleration and grip. Seventeen in
  all, every vehicle on the road among them: the sports car dealers (red;
  Italy, Japan, California) sell the Spider, GT Coupe, Muscle car, Roadster,
  the rivals' Racer and a Supercar, the everyday dealers (blue; England,
  Korea) the Hot Hatch, Mini, Saloon, Taxi, Estate and a retired police
  Patrol car, the truck stops the Big Rig, Van, Box Truck, Pickup and a
  Coach. Each dealer sells the cars at home in its part of the world (the
  Italian sports cars in Italy, the Japanese ones in Japan, the muscle car
  in California, ...), and the car you came in stays on offer
- **Dirt and car washes**: puddles splash the car with mud, crashes add more,
  oil slicks leave black spots; stop at one of the car washes in England,
  Kenya and the outback to have it washed clean
- **Motels and hospitals**: stop at a motel (India, Route 66) and steer left
  or right to pick up another passenger, among them a granny and a dog, or
  nobody at all; at a
  hospital (Holland, Korea) another driver takes the wheel, or, past the
  last of them, you take an ambulance. A crash leaves the driver with a
  bandage round the head until a hospital patches them up
- **Picks up where you left off**: quit and start again and the race goes
  on from the same place, at the same time of day and with the same fuel
- **Night lights**: cities glow at night, the sky above them orange, their
  windows lit and their street lamps throwing pools of warm light on the
  road; out in the desert and the outback the night is as dark as it gets.
  The traffic's headlights light the road ahead of them, their tail lights
  glow red behind
- **Secrets**: a few cars from the movies are hidden in the game, each
  found by doing the right thing in the right place, each with a trick of
  its own. No dealer sells them
- **Tunnels and bridges**: through the Alps, Korea and Japan in tunnels,
  their walls and lamps around you and the way out framed ahead; over the
  Rhine and the Amazon on steel truss bridges; across the Golden Gate into
  San Francisco; under road bridges on the Autobahn and in the cities
- **Trains**: railways cross the road in the Netherlands, Germany, India,
  the outback and Arizona. Now and then a long train comes as you near a
  crossing, the lamps flashing, timed to clear the road just before you get
  there at the speed you go: keep it up and it is a near miss, go faster and
  you may meet it. The traffic waits for it
- **Animals crossing**: now and then an animal of the countryside walks
  across the road ahead: cows in India, giraffes in Kenya, deer in the
  forests, the Alps and Korea, a flock of sheep in England, kangaroos hopping
  across the outback. The traffic stops for them, the horn hurries them,
  and hitting one is a bump or, at speed, a crash
- **Taxi fares**: drive the Taxi and people wait at the kerb, waving for
  you. Stop beside one with the passenger seat empty (pick NOBODY at a
  motel) and they get in and name a place a little further on; stop
  anywhere there and they pay. The fares paid show under the clock
- **Emergency vehicles**: ambulances in the traffic, every other one on a
  call with its lights flashing; drive the Patrol car or the ambulance and
  the hazard switch works the lightbar and siren, and the traffic pulls over
- **Camera views**: the classic chase view, a far one from higher up, the
  bumper, and the cockpit with the dashboard and the wheel turning in the
  driver's hands (higher up in the truck)
- **Truck stops** (before the Autobahn, in the outback): the Big Rig, an
  orange cab-over tractor, the Van and the Box Truck, slow but planted
- **Your garage**, just before the start line: every car you have driven
  off a lot (or found) waits there; stop and steer to take another one
- **Advance signs** announce every stop 500 and 200 m ahead (nearer where
  there is no room), on its side of the road; they read at night too
- **Mini map** of the lap with your position, the start line, the gas
  stations, the car dealers, car washes, motels, hospitals, truck stops and
  your garage
- **Police chases**: now and then, driving fast, a police car turns up in
  the mirror, lights flashing and siren wailing. Close behind it is a little
  slower than the standard car flat out, so it gains only when you slow down
  (traffic, the verge, braking); once it catches up it overtakes, cuts in
  ahead and slows down to stop you. Brought to a halt behind it, you are
  pulled over; swerve past and it starts over; get far enough ahead (nitro, a
  faster car, clean lines) or hold out until it gives up and you escape. A
  meter under the mirror shows how near you are to getting away
- **Radio**: six synthesised songs (Sunset Cruise, Turbo Breeze, Night Drive,
  Coast Run, Alpine Pass, Neon Strip), each a loop of pads, bass, lead and
  drums from a step sequencer;
  flick the right stick up or down, or press N, to change the track or switch it off
- **Synthesised sound**: a six-cylinder engine with exhaust pulses ringing a
  pipe and a body resonance for the bass, building with the revs, surging as
  the throttle opens and crackling on the overrun; gear changes, tyre squeal,
  gravel, wind, rain, splashes, barrier scraping and crashes; `M` mutes
- Keyboard and **gamepad** (any controller SDL knows, hot-pluggable, with
  rumble)
- HUD in a bitmap font: lap time, lap counter, best and last lap, speedometer,
  rev counter, GO! / LAP / NEW RECORD banners
- Fixed 60 Hz simulation, independent of the frame rate

## Build

```bash
# Nix develop: out-of-tree Debug build, edit → run in one command
nix develop
kurvenrausch-configure          # once (or when the tree moved)
kurvenrausch-run                # build if needed, then run
kurvenrausch-run-gdb --help     # same under gdb (auto-run; quit on exit 0)

# Package (RelWithDebInfo, no ccache)
nix build
./result/bin/kurvenrausch

# Plain CMake + system SDL2
cmake -B build && cmake --build build
./build/kurvenrausch
```

`nix develop` puts `kurvenrausch-configure` / `-build` / `-run` / `-run-gdb`
on PATH (Pingus/biltoo pattern). The build directory defaults to
`/tmp/kurvenrausch-build`; override with `KURVENRAUSCH_BUILD_DIR`. CMake
exports `compile_commands.json`; configure symlinks `build/` there for
clangd (see `.clangd`).

### Install

```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

This installs the game with its desktop integration: the program, a desktop
entry and AppStream metadata (`io.github.grumbel.kurvenrausch`), icons in the
hicolor theme (PNGs from 16 to 256 pixels and an SVG) and the man page,
`man 6 kurvenrausch`. The Nix package (`nix build`) contains the same. The
icon is the game's own pixel art, also its window icon;
`tools/make_icons.py` regenerates the files from it.

### Windows

```bash
nix build .#kurvenrausch-win64-zip   # kurvenrausch-<version>-win64.zip
nix build .#kurvenrausch-win32-zip   # 32-bit Windows
```

Cross-compiled with MinGW on Linux, with SDL2's official prebuilt MinGW
package (`nix/windows.nix`); `.#kurvenrausch-win64` is the unzipped folder.
The zip holds `kurvenrausch.exe` (with the game's icon), `SDL2.dll`, MinGW's
thread library, the licence and a readme; lap times and choices go to
`%APPDATA%\grumbel\kurvenrausch`.

### Android

```bash
nix build .#kurvenrausch-android         # kurvenrausch-<version>.apk
nix run .#install-android-kurvenrausch   # install it on a device with adb
```

Built with the NDK and SDL2 2.30.3's Java side (`nix/android.nix`,
`mk/android/`, after Pingus), for 32- and 64-bit ARM and x86_64, Android 5.1
(API 22) and later. Landscape; touch controls, gamepads and keyboards all
work, the back button pauses. Signed with the debug key in
`mk/android/keystore/`.

### R36S and other PortMaster handhelds (ArkOS)

```bash
nix build .#kurvenrausch-r36s-portmaster-zip   # for PortMaster's autoinstall
nix build .#kurvenrausch-r36s-portmaster       # or copy its contents to /roms/ports/
```

A native aarch64 build for ArkOS (Ubuntu 19.10 underneath), made with
nixpkgs' cross compiler against a sysroot of Ubuntu 19.10's own packages
(`nix/r36s.nix`), so it runs on the device's glibc, libstdc++ and SDL2. The
controls are in `mk/r36s/README.md`. The GLES2 renderer runs the game at
60 fps on the device, in SD (the 320x240 picture at exactly twice the size)
or HD (640x480, the screen's own resolution).

### Web (WebAssembly)

```bash
nix build .#kurvenrausch-wasm   # the site: index.html, kurvenrausch.{html,js,wasm}
nix run .#kurvenrausch-wasm     # serve it on 127.0.0.1 and open a browser
```

Emscripten compiles the game into a web page; the Nix build is offline,
with SDL2 built from its release tarball (`nix/wasm.nix`,
`mk/wasm/scripts/`), and `result/` can be put on any static web server.
Without Nix, with Emscripten installed (it then fetches its SDL2 port):
`emcmake cmake -B build-wasm && cmake --build build-wasm`. The page
(`mk/wasm/shell.html`) scales the picture to the window, keeps the lap times
and choices in the browser's storage (with buttons to download them and to
load them elsewhere) and pauses the game when the tab is hidden. In the
browser Esc pauses (same as native), and the pause menu has no Quit.

The `.wasm` is an Emscripten module: it imports the generated JavaScript
runtime (minified names such as `a::a`), not WASI. Standalone runtimes
like `wasmtime`, Wasmer or WasmEdge therefore cannot instantiate it:

```text
Error: failed to instantiate "kurvenrausch.wasm"
Caused by: unknown import: `a::a` has not been defined
```

There is no practical lightweight WASM runtime today that runs an
Emscripten+SDL2 game with a real window outside the browser. Experimental
WASI graphics proposals (`wasi-gfx`, `wasi:surface` / `wasi:frame-buffer`)
exist, but they are not an SDL2 ABI and would need a separate graphics
backend. Use `nix run .#kurvenrausch-wasm` (or any static file server on
`result/`) instead.

## Controls

| Keyboard         | Gamepad                  | Action             |
|------------------|--------------------------|--------------------|
| ↑ / W            | Right trigger, A         | Accelerate         |
| ↓ / S            | Left trigger, B          | Brake; standing, let go and press again: reverse |
| ← → / A D        | Left stick, D-pad        | Steer              |
| Ctrl             | Left shoulder            | Handbrake          |
| H                | X                        | Horn               |
| Space            | Y, right shoulder        | Nitro              |
| P                | Start                    | Pause menu (see below) |
| R                |                          | Restart            |
| C                | Back                     | Camera view: chase, far, bumper, cockpit |
| N                | Right stick, flicked up  | Radio: next track, or off |
| L                | D-pad up                 | Headlights         |
| Q / E            | Right stick, flicked left / right | Indicator left / right (again: off; it also goes off after a lane change) |
| Z                | D-pad down               | Hazard lights (police car: lightbar and siren) |
| Tab              | Right stick, clicked     | Mini map: zoomed in around the car, or the whole lap |
| B                | Right stick, flicked down | Radio: previous track |
| M                |                          | Mute sound         |
| F11 / Alt+Enter  |                          | Toggle fullscreen  |
| F8               |                          | Switch renderer: GLES ↔ software |
| Esc              |                          | Pause (in the pause menu: back a page; Quit is a menu item) |

All of these but Esc, F8, F11 and Start can be rebound in OPTIONS →
CONTROLS, two keys and two pad inputs (buttons, triggers, stick directions)
per action.

On a touch screen (phones, tablets, the web page on them) controls appear
once you touch it, at the screen's own resolution and across the whole
screen: drag sideways on the left to steer, GAS and BRK (brake, and
reverse) on the right with NOS, HB (handbrake) and H (horn), the pause
button right of the mirror; in the pause menu tap a line, on a setting
its left or right half to step it back or on.

### Pause menu

Everything set here is kept for the next run. Arrow keys / D-pad move,
Enter / A picks, left / right change a setting, Esc / B go back a page,
Page Up / Down and the shoulder buttons page through long lists; the mouse
works too (point, click, wheel, right click to go back).

- **RESUME**, **RESTART**
- **RACE SETUP**: the track, the country (or city) to start in, the time of
  day (the day passing, or held at day, dusk or night), the weather
  (changing, always clear, always stormy, heavy fog), the traffic (none to
  heavy); START RACE drives off from there
- **OPTIONS**
  - GAMEPLAY: fuel, nitro canisters each lap (0 to 9), police chases, the
    camera view
  - VIDEO: fullscreen, screen shape (4:3 with black bars, or WIDE: as wide
    as the screen up to 2:1, showing more to the sides), SD / HD, renderer;
    HUD, mini map (zoomed, whole lap, off), mirror, rain and snow, FPS
  - AUDIO: sound on or off, effects and music volume, the radio
  - CONTROLS: KEYBOARD and GAMEPAD bindings (pick a slot, press the new key;
    Del / X clears it), rumble
  - DEBUG: car, driver and passenger, the hour; headlight beam, present
    path, the sprite viewer
- **EXTRAS**: watch the demo (the attract mode), its text, when it starts by
  itself; LAP RECORDS, the ten fastest laps of each track
- **QUIT** (not in a web page)

Any controller SDL knows (Xbox, PlayStation, Switch Pro, most generic pads) works
and can be plugged in at any time; analog sticks and triggers steer and
accelerate proportionally, and the pad rumbles on crashes, scraping, close
passes, nitro and when you leave the road. Additional mappings can be supplied through SDL's
`SDL_GAMECONTROLLERCONFIG` environment variable. Keyboard and gamepad can be
used together.

The game remembers the car, driver and passenger you chose last, and keeps
every lap you complete, in `$XDG_STATE_HOME/kurvenrausch/` (usually
`~/.local/state/kurvenrausch/`): `choices`, and `laps.tsv` with one lap per
line (time, seconds, car, driver, passenger). The fastest lap there is the
record shown as BEST. Headless runs leave it alone.

## Tests

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

The unit tests cover the input mapping, zone blending, the route's invariants,
cliffs and barriers, weather, lane logic, lap detection and the sound synthesis
(which is rendered offline and checked by its spectrum). Further tests check
that the man page documents every option, and run `desktop-file-validate`,
`appstreamcli validate` and `mandoc -T lint` where they are installed (the
Nix dev shell has them).

## Headless mode

The game can render without a window, which is how the screenshots above are
made and how rendering changes are checked without a display:

```bash
./build/kurvenrausch --print-zones                       # list the zones
./build/kurvenrausch --track 1 --print-zones             # ... of the Grand Tour
./build/kurvenrausch --screenshot shot.bmp --zone 3 --frames 150
./build/kurvenrausch --screenshot shot.bmp --position 240000 --frames 600
./build/kurvenrausch --screenshot shot.bmp --frames 9000 --wav lap.wav
./build/kurvenrausch --screenshot shot.bmp --steer 1     # hold the steering
./build/kurvenrausch --screenshot shot.bmp --frames 600 --nitro 540   # nitro at step 540
./build/kurvenrausch --screenshot shot.bmp --frames 3600 --horn       # honk all the way
./build/kurvenrausch --screenshot shot.bmp --frames 2400 --fuel 0.3   # low fuel: pulls in
./build/kurvenrausch --screenshot shot.bmp --frames 640 --steer -1 --steer-from 600  # crash
./build/kurvenrausch --screenshot shot.bmp --zone 1 --frames 300 --car 2   # another car
./build/kurvenrausch --screenshot shot.bmp --zone 5 --frames 1400 --dealer # visit a dealer
./build/kurvenrausch --screenshot shot.bmp --frames 1500 --attract   # the attract mode
./build/kurvenrausch --screenshot shot.bmp --frames 900 --width 534  # wide screen (20:9)
```

It simulates the given number of 60 Hz steps with a simple autopilot that also
pulls in at gas stations when low on fuel (or a held steering angle), renders one frame and writes it as a BMP; `--wav` also writes
the sound of the run. `tools/make_screenshots.py` (needs Pillow) regenerates
the README images.

With `--renderer gles` the same frame is drawn by the GLES renderer in a
hidden window (this needs a display, or `xvfb-run`), for comparing the two
renderers pixel by pixel; it also prints the frame's statistics:

```bash
./build/kurvenrausch --screenshot sw.bmp --zone 14 --frames 400 --hour 23 --headlights
./build/kurvenrausch --screenshot gl.bmp --zone 14 --frames 400 --hour 23 --headlights --renderer gles
```

### Profiling

VIDEO → SHOW FPS shows the frame rate and, on the GLES path, the draw calls,
vertices, and the time and fill (screens' worth of pixels drawn) of each
phase of the frame: `g-sky`, `g-road`, `g-spr` (sprites, particles,
cockpit), `g-light` (night lightmap), `g-comp` (night compose), `g-mirror`,
`present`. About once a second the same goes to stdout, attract mode
included. GPU drivers queue work, so CPU timers mostly show it landing in
`present`; run with `KURVENRAUSCH_GPU_SYNC=1` and every phase waits for the
GPU, so its time includes its own GPU work (the frame gets slower, the
split gets honest).

## Architecture

```
include/
  types.hpp        Color (ARGB8888), blending, dithering, hash noise, the
                   glowing() mark for lights
  ecs.hpp          minimal Entity-Component-System
  components.hpp   Transform, Velocity, Player, Traffic, Camera
  track.hpp        Track, Segment, Zone, RoadTheme (the blendable look),
                   edges (rails, cliffs), scenery kinds, the route builder
  road.hpp         road projection and rendering (ahead or, for the mirror,
                   behind), cliffs, road sprites
  background.hpp   parallax sky, sun, clouds, mountains, hills
  weather.hpp      rain and snow particles
  sprites.hpp      procedural pixel-art sprite sheet
  bitmap.hpp       Bitmap and paint helpers for generating sprites
  font.hpp         5x7 bitmap font
  hud.hpp          HUD and menu drawing
  menu.hpp         menu pages, navigation, scrolling and layout (the
                   pages themselves are built in game_menu.cpp)
  bindings.hpp     rebindable keys and pad inputs
  canvas.hpp       2D drawing for everything over the scene: into the
                   framebuffer (software) or as a GPU draw list
  framebuffer.hpp  software framebuffer: clipping, trapezoids, scaled blits
  daylight.hpp     time of day, nightfall, street lamps and headlight pools
  gles_render.hpp  the OpenGL ES 2 renderer: road, sprite atlas, night
                   lightmap and compose, draw lists, the mirror's view
  frame_stats.hpp  per-phase timings, draw counts and fill for the FPS overlay
  display.hpp      SDL window presentation (SDL or GL), BMP export
  input.hpp        keyboard, gamepad (analog), mouse and touch input, rumble
  drivetrain.hpp   gears and revs, shared by the HUD and the sound
  vehicles.hpp     the kinds of traffic: sizes, speeds, shares, the rival
  driving.hpp      nitro, fuel, overspeed, the pass boost, yielding to the
                   horn, following traffic, the crash animation
  synth.hpp        sound synthesis (pure DSP, no SDL), WAV export
  audio.hpp        SDL audio device playing the synth
  game.hpp         game loop, physics, traffic, lap timing
src/               implementations
tests/             unit tests (CTest)
data/              desktop entry, AppStream metadata, man page, icons
tools/             screenshot and icon generators
docs/              README images
LICENSES/          licence text (REUSE)
```

### Classic techniques used

1. **Segmented road**: the track is a looping array of short segments, each
   with a curve value and heights at both edges, built from sections that
   ease into and out of bends and hills.
2. **Perspective scale**: `scale = camera_depth / z`; screen x, y and road
   width follow from it.
3. **Curve accumulation**: walking the segments near to far,
   `x += dx; dx += curve`, starting with the part of the current segment
   already passed so bends move smoothly.
4. **Hill occlusion**: road is drawn near to far while tracking the highest
   row drawn so far (`max_y`); segments behind a crest are skipped or
   clipped. Sprites, cliffs and rails are drawn far to near, clipped against
   the road in front of them.
5. **Scanline fill**: road, rumble strips and lanes are trapezoids filled as
   horizontal spans with sub-pixel edges; cliffs are filled column by column.
6. **Billboard sprites**: scenery and cars scaled with the same projection,
   blended into the fog with distance.
7. **Looks**: every segment carries a precomputed blend of its zone's theme
   and its neighbours', so sky, fog, weather, markings and handling change
   smoothly along the track.
8. **Night**: the picture is drawn at full daylight, then darkened, and lit
   again by street lamps, the traffic's lights and the headlights from what
   the ground showed by day. Lights are pixels marked in their alpha channel
   where the art is painted, so they keep shining. The GLES renderer
   evaluates the same formulas per pixel into a lightmap (per-row road depth
   and centre come from a small texture; the depth buffer tells the ground
   from what stands on it) and composes it with the daylight picture.

## License

Copyright 2026 Ingo Ruhnke <grumbel@gmail.com>

Kurvenrausch is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [`LICENSES/GPL-3.0-or-later.txt`](LICENSES/GPL-3.0-or-later.txt).
The AppStream metadata file alone is under CC0-1.0, as AppStream requires a
permissive licence for metadata.

The repository follows the [REUSE](https://reuse.software/) specification:
`reuse lint` passes.

## Credits

Inspired by Louis Gorenfeld's ["Lou's Pseudo 3D Page"](http://www.extentofthejam.com/pseudo/)
and Jake Gordon's [JavaScript Racer tutorial](https://codeincomplete.com/articles/javascript-racer/). The code itself is original.
