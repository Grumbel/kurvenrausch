# Kurvenrausch

Classic pseudo-3D bitmap racer in the spirit of *OutRun*, *Lotus Esprit
Turbo Challenge* and *Pole Position*.

Written in **C++17** with **SDL2**. Everything is drawn in software, scanline
by scanline, into a 320x240 framebuffer that is scaled up with
nearest-neighbour filtering for chunky pixels. All art is generated
procedurally as pixel art at startup; there are no asset files. Packaged with
a **Nix flake**.

## Features

- Segment-based pseudo-3D road: perspective projection (`scale = depth / z`),
  curves by accumulated lateral offset, hills and crests with correct
  occlusion, alternating rumble strips and grass bands, lane dashes, a
  chequered start line, and distance fog
- Roadside scenery as scaled, fogged pixel-art sprites: palm avenues,
  countryside trees, bushes, boulders, billboards and a start gantry; objects
  behind a crest peek over it
- Parallax backdrop: copper-banded sky, drifting clouds, snow-capped
  mountains and rolling hills scrolling at different rates through bends
- Convertible with steering frames, speed-dependent shake
- Arcade handling: centrifugal force in curves, off-road slowdown, crashes
  into roadside objects
- AI traffic that changes lanes to pass; rear-ending a car slows you down
- HUD with bitmap font: lap time, lap counter, best and last lap, speedometer
  and rev counter, GO! / LAP / NEW RECORD banners
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

## Controls

| Keyboard         | Gamepad                  | Action             |
|------------------|--------------------------|--------------------|
| ↑ / W            | Right trigger, A         | Accelerate         |
| ↓ / S            | Left trigger, B          | Brake              |
| ← → / A D        | Left stick, D-pad        | Steer              |
| R                | Start                    | Restart            |
| F11 / Alt+Enter  |                          | Toggle fullscreen  |
| Esc              |                          | Quit               |

Any controller SDL knows (Xbox, PlayStation, Switch Pro, most generic pads) works
and can be plugged in at any time; analog sticks and triggers steer and
accelerate proportionally, and the pad rumbles on crashes and when you leave the
road. Additional mappings can be supplied through SDL's `SDL_GAMECONTROLLERCONFIG`
environment variable. Keyboard and gamepad can be used together.

## Tests

```bash
cmake -B build && cmake --build build
ctest --test-dir build --output-on-failure
```

## Headless screenshots

```bash
./build/kurvenrausch --screenshot shot.bmp --frames 600 --position 240000
```

Simulates the given number of 60 Hz steps with a simple autopilot, starting
at the given distance along the track, renders one frame and writes it as a
BMP without opening a window. Useful for checking rendering changes in CI or
without a display.

## Architecture

```
include/
  types.hpp        Color (ARGB8888), blending, ordered dithering, palette
  ecs.hpp          minimal Entity-Component-System
  components.hpp   Transform, Velocity, Player, Traffic, Camera
  track.hpp        Track / Segment / RoadTheme data, scenery kinds, builder
  road.hpp         road projection and rendering, road sprites
  background.hpp   parallax sky, clouds, mountains, hills
  sprites.hpp      procedural pixel-art sprite sheet
  bitmap.hpp       Bitmap and paint helpers for generating sprites
  font.hpp         5x7 bitmap font
  hud.hpp          HUD drawing
  framebuffer.hpp  software framebuffer: clipping, trapezoids, scaled blits
  display.hpp      SDL window presentation, BMP export
  input.hpp        keyboard and gamepad input (analog), rumble
  game.hpp         game loop, physics, traffic, lap timing
src/               implementations
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
   clipped. Sprites are drawn far to near, clipped against the road in front
   of them.
5. **Scanline fill**: road, rumble strips and lanes are trapezoids filled as
   horizontal spans with sub-pixel edges.
6. **Billboard sprites**: scenery and cars scaled with the same projection,
   blended into the fog with distance.

## Credits

Inspired by Louis Gorenfeld's "Lou's Pseudo 3D Page" and Jake Gordon's
JavaScript Racer tutorial. The code itself is original.

## License

Copyright 2026 Ingo Ruhnke <grumbel@gmail.com>

Kurvenrausch is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [`LICENSES/GPL-3.0-or-later.txt`](LICENSES/GPL-3.0-or-later.txt).

The repository follows the [REUSE](https://reuse.software/) specification:
`reuse lint` passes.
