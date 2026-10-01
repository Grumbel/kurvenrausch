# TODO

## Current tip

Base: `d66c91a Initial checkin`. Work line: kurvenrausch-001.x.

## Review of the initial checkin (2026-10-01)

The initial code compiles but cannot render a usable road:

1. Projection units are wrong: `scale = depth / (z/segment_length + depth)`
   gives scale ~1, multiplied with world units (road width 2000) -> the road
   is ~1e6 px wide.
2. The segment under the camera wraps to `total_length` and is culled.
3. Hill clipping is inverted: drawing far->near while clipping as if
   near->far hides every nearer segment.
4. `Color::to_rgba()` packs ABGR but the texture is RGBA8888 (channel swap).
5. No interpolation inside a segment (steppy camera height / curves).
6. Fixed timestep is cosmetic; physics runs on variable dt.
7. Renderer leaks the SDL_Renderer, has no software fallback, discards the
   parallax offset; const `World::get` would not compile; `World::destroy`
   never shrinks the alive list.

## Plan

- [ ] Fix pixel format, add software renderer fallback, fix leaks.
- [ ] Split software framebuffer from SDL presentation; low-res, integer
      scaled output; headless `--screenshot` mode for verification.
- [ ] Rewrite road projection / clipping / track data properly.
- [ ] Fixed-timestep physics (centrifugal force, off-road, collisions).
- [ ] Pixel-art bitmap sprites (palms, trees, rocks, billboards, start
      gantry, player car) with scaled, clipped, fogged blitting.
- [ ] Parallax background (sky bands, mountains, hills, clouds) and fog.
- [ ] Bitmap font HUD: speed, lap, lap times.
- [ ] Traffic cars as ECS entities.

## Open questions

- Project naming: repository is "kurvenrausch", code/README say
  "Bitmap Racer" / `bitmap_racer`. Rename?
- README says MIT but there is no LICENSE file and no SPDX headers.
