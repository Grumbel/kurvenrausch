# Kurvenrausch

Classic pseudo-3D bitmap racer in the spirit of *OutRun*, *Lotus Esprit Turbo
Challenge* and *Pole Position*: a lap through Europe and the USA with weather,
cliff roads, traffic, and a synthesised engine.

Written in **C++17** with **SDL2**. Everything is drawn in software, scanline by
scanline, into a 320x240 framebuffer that is scaled up with nearest-neighbour
filtering for chunky pixels. All art is generated procedurally as pixel art at
startup and all sound is synthesised on the fly: there are no asset files.
Packaged with a **Nix flake**.

![Kurvenrausch on the Cote d'Azur corniche](docs/screenshot.png)

One lap takes you around the world through sixteen zones, each with its own country, scenery,
weather and road markings, fading smoothly into one another:

![The sixteen zones: France, England, the Netherlands, Germany, Switzerland, Italy, Egypt, Kenya, India, Korea, Japan, Australia, Arizona, California, San Francisco, Brazil](docs/zones.png)

## Features

- Segment-based pseudo-3D road: perspective projection (`scale = depth / z`),
  curves by accumulated lateral offset, hills and crests with correct
  occlusion, alternating rumble strips and grass bands, lane markings, a
  chequered start line, and distance fog
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
- **Two tracks**, picked on the pause menu's TRACK line: the Small World
  above, and the Grand Tour, more than twice as long, which stops in two or
  three cities of every country (Paris, London, Amsterdam, Muenchen, Roma,
  Cairo, Mumbai, Tokyo, Sydney, Los Angeles, Rio de Janeiro, ...): town
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
  to blue with stars overhead while tail lights, indicators and headlights
  keep shining; the headlights (L) light the road ahead
- **Attract mode**: the game starts, as in an arcade, following the cars of
  the traffic around the world (a new one every 20 seconds), and goes back
  to it after two minutes without anybody at the controls; any key, button,
  stick or tap starts a race
- **Wet spots**: puddles on the road in the rainy and snowy zones; hit one
  fast and the car aquaplanes, throwing up spray. Rarely, an **oil slick**:
  hardly any grip at all
- Roadside scenery as scaled, fogged pixel-art sprites; objects behind a crest
  peek over it
- Parallax backdrop: copper-banded sky, drifting clouds, mountains and hills
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
  pumps. Run dry and the engine sputters and dies; stranded, the driver pours
  in a spare can after a few seconds
- **Car dealers**: stop on the forecourt and steer left or right to choose
  a car, each with its own top speed, acceleration and grip. Seventeen in
  all, every vehicle on the road among them: the sports car dealers (red;
  Italy, Japan, California) sell the Spider, GT Coupe, Muscle car, Roadster,
  the rivals' Racer and a Supercar, the everyday dealers (blue; England,
  Korea) the Hot Hatch, Mini, Saloon, Taxi, Estate and a retired police
  Patrol car, the truck stops the Big Rig, Van, Box Truck, Pickup and a
  Coach
- **Dirt and car washes**: puddles splash the car with mud, crashes add more,
  oil slicks leave black spots; stop at one of the car washes in England,
  Kenya and the outback to have it washed clean
- **Motels and hospitals**: stop at a motel (India, Route 66) and steer left
  or right to pick up another passenger, among them a granny and a dog; at a
  hospital (Holland, Korea) another driver takes the wheel. A crash leaves
  the driver with a bandage round the head until a hospital patches them up
- **Camera views**: the classic chase view, a far one from higher up, the
  bumper, and the cockpit with the dashboard and the wheel turning in the
  driver's hands (higher up in the truck)
- **Truck stops** (before the Autobahn, in the outback): the Big Rig, an
  orange cab-over tractor, the Van and the Box Truck, slow but planted
- **Mini map** of the lap with your position, the start line, the gas
  stations, the car dealers, car washes, motels, hospitals and truck stops
- **Police chases**: now and then, driving fast, a police car turns up in
  the mirror, lights flashing and siren wailing. Close behind it is a little
  slower than the standard car flat out, so it gains only when you slow down
  (traffic, the verge, braking); once it catches up it overtakes, cuts in
  ahead and slows down to stop you. Brought to a halt behind it, you are
  pulled over; swerve past and it starts over; get far enough ahead (nitro, a
  faster car, clean lines) or hold out until it gives up and you escape. A
  meter under the mirror shows how near you are to getting away
- **Radio**: three synthesised songs (Sunset Cruise, Turbo Breeze, Night
  Drive), each a loop of pads, bass, lead and drums from a step sequencer;
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
# Nix: reproducible shell, or build the package
nix develop
nix build
./result/bin/kurvenrausch

# or with CMake and SDL2 installed
cmake -B build
cmake --build build
./build/kurvenrausch
```

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
320x240 picture fills the 640x480 screen at exactly twice the size; the
controls are in `mk/r36s/README.md`.

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
browser Esc pauses, and the pause menu has no Quit.

## Controls

| Keyboard         | Gamepad                  | Action             |
|------------------|--------------------------|--------------------|
| ↑ / W            | Right trigger, A         | Accelerate         |
| ↓ / S            | Left trigger, B          | Brake; standing, let go and press again: reverse |
| ← → / A D        | Left stick, D-pad        | Steer              |
| Ctrl             | Left shoulder            | Handbrake          |
| H                | X                        | Horn               |
| Space            | Y, right shoulder        | Nitro              |
| P                | Start                    | Pause menu: resume, restart, start in a chosen country, screen shape, options, quit |
| R                |                          | Restart            |
| C                | Back                     | Camera view: chase, far, bumper, cockpit |
| N                | Right stick, flicked up / down | Radio: next (or previous) track, or off |
| L                | D-pad up                 | Headlights         |
| Q / E            | Right stick, flicked left / right | Indicator left / right (again: off; it also goes off after a lane change) |
| Z                | D-pad down               | Hazard lights      |
| Tab              | Right stick, clicked     | Mini map: zoomed in around the car, or the whole lap |
| M                |                          | Mute sound         |
| F11 / Alt+Enter  |                          | Toggle fullscreen  |
| Esc              |                          | Quit (in the pause menu: back to the race) |

On a touch screen (phones, tablets, the web page on them) controls appear
once you touch it, at the screen's own resolution and across the whole
screen: drag sideways on the left to steer, GAS and BRK (brake, and
reverse) on the right with NOS, HB (handbrake) and H (horn), the pause
button right of the mirror; in the pause menu tap a line, on the country
line its left or right end to pick the country.

OPTIONS in the pause menu switches gameplay features, kept for the next
run: the time of day (the day passing, or held at day, dusk or night),
fuel, how many nitro canisters each lap brings (0 to 9), police chases,
the weather (changing, always clear, always stormy) and how much traffic
there is (none to heavy).

The picture is 4:3 with black bars on wider screens; SCREEN in the pause
menu switches it to WIDE, as wide as the screen (up to 2:1), showing more
to the sides.

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

## Architecture

```
include/
  types.hpp        Color (ARGB8888), blending, dithering, hash noise
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
  hud.hpp          HUD drawing
  framebuffer.hpp  software framebuffer: clipping, trapezoids, scaled blits
  display.hpp      SDL window presentation, BMP export
  input.hpp        keyboard and gamepad input (analog), rumble
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
