# Bitmap Racer

Classic pseudo-3D bitmap racer in the spirit of *OutRun*, *Lotus 1/2/3*, and *Pole Position*.

Built with **raw C++17**, **SDL2** software rendering (classic pixel / scanline techniques), a lightweight **ECS**, and fully **data-driven** track geometry. Packaged with a **Nix flake**.

## Features

- Pseudo-3D road with perspective projection (`scale = depth / z`)
- Curves via cumulative lateral offset (classic “dx += curve” technique)
- Hills & valleys via per-segment world height + vertical projection
- Cliffs / roadside objects as billboard sprites with hill clipping
- Alternating road / grass / rumble colors for the authentic 80s/90s look
- Player car with acceleration, braking, steering and centrifugal force
- Looping track, lap counter, speed HUD
- Clean separation: ECS components, systems, pure data track definition

## Build (Nix)

```bash
# Enter the reproducible shell
nix develop

# Or build the package
nix build

# Run
./result/bin/bitmap_racer
# or from the shell after cmake:
mkdir build && cd build
cmake ..
make -j
./bitmap_racer
```

## Controls

| Key            | Action          |
|----------------|-----------------|
| ↑ / W          | Accelerate      |
| ↓ / S          | Brake           |
| ← → / A D      | Steer           |
| R              | Restart         |
| Esc            | Quit            |

## Architecture

```
include/
  types.hpp        – Vec2, Color, classic palette
  ecs.hpp          – minimal Entity-Component-System
  components.hpp   – Transform, Velocity, Player, Camera, Segment, Track
  renderer.hpp     – software pixel renderer (scanlines, sprites)
  road.hpp         – track builder + projection + render system
  game.hpp         – main loop, fixed timestep
  input.hpp        – keyboard polling
src/               – implementations
flake.nix          – Nix environment + package
CMakeLists.txt
```

### Classic techniques used

1. **Segmented road** – track is a circular array of short segments, each with `curve` and `y` (height).
2. **Perspective scale** – `scale = camera_depth / z`; screen X/Y/W derived from it.
3. **Curve accumulation** – while walking segments, `x += dx; dx += segment.curve`.
4. **Hill clipping** – painter’s algorithm (far → near) + running `max_y` so crests hide the road beyond.
5. **Software scanline fill** – trapezoids drawn as horizontal spans (no GPU polygons).
6. **Billboard sprites** – trees/cliffs projected with the same scale, clipped by hill height.

## Extending

- Add traffic AI as extra entities with `Transform` + `Velocity`.
- Load tracks from a simple text/JSON format (the `Track` struct is already pure data).
- Replace procedural sprites with SDL surfaces loaded from BMP/PNG.
- Add multiple cameras or split-screen by instantiating more `Camera` components.

## Credits

Inspired by the excellent write-ups of Louis Gorenfeld (“Lou’s Pseudo 3D Page”) and Jake Gordon’s JavaScript Racer tutorial. The code itself is original.

MIT License.
