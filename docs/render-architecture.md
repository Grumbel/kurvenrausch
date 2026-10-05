# Render architecture notes

Notes on overdraw, occlusion, multithreading, and scanline-style approaches
for Kurvenrausch’s software pseudo-3D path. Not a roadmap commitment — a
record of what was considered and why.

## What the current pass does

```
near → far segments:  accumulate curve, update max_y / ceiling / mouth
                      draw road trapezoids (full width)
far  → near sprites:  crest clip, nearer overdraws farther
```

Important existing occlusion:

| Mechanism | Effect |
|-----------|--------|
| Near→far + `max_y` | Road/grass hidden behind hills |
| Per-slice `clip` | Scenery/traffic behind a crest |
| Tunnel ceiling / mouth | Sky and world outside the bore |
| Clip rects | Mouth and vertical hill line |

Road overdraw (grass → road → rumble → lanes) is intentional layering, not
the same problem as stacking large opaque sprites.

Framebuffer sizes: SD 320×240, HD 640×480 (`pixel_scale` 1 or 2).

## Cliff and building overdraw

Cliffs were one large billboard **per segment** (~1.5+ segment widths). On a
long run with `draw_distance ≈ 300` that is hundreds of overlapping blits —
the main reason long cliff stretches fell to ~30 fps.

Mitigation in tree: **subsample** (every 2nd segment near the camera, every
3rd farther; always draw run ends; widen sprites to cover the stride).

Buildings (`Townhouse`, `Shop`, …) are solid roadside sprites. Dense towns
stack in depth; size-cull / LOD is the cheap fix before a general mask.

## y-buffer (column occlusion)

A **y-buffer** stores, per screen column \(x\), the highest row already
drawn. Draw near→far; only paint pixels still open; then raise `ybuf[x]`.
That is the classic heightmap / “voxel” column trick (e.g. Comanche).

Cliffs are *almost* voxels: height from `edge_height`, lateral offset,
continuous along \(z\). A y-buffer pays off if cliffs are drawn as **column
fills** near→far. It helps less if they stay α-edged billboards (you still
pay sprite setup; the mask only saves some pixels).

The road’s `max_y` is a **single horizontal** occlusion line, not per-column.
Rails already walk columns in a related way.

**Hybrid:** keep road `max_y`; optional y-buffer only for `Edge::Cliff`
column fills; buildings/trees stay sprites.

Prefer after measuring: only if subsampled cliffs are still the fps limit
and a height-column look (or the work to keep painted silhouettes) is
acceptable.

## Framebuffer 1-bit occlusion mask

Full-screen 1-bit (or byte depth) mask + near→far **opaque** sprites can
cut overdraw for solid façades. Cost: extra buffer traffic on the hot path;
α sprites still need far→near (or a split pass).

Reasonable **later** step for dense opaque buildings at HD — not a first
fix over cliff subsampling / size culls. 8×8 tiles alone are a weak fit;
`max_y` already rejects whole hills. Tiles only as a coarse early-out on
top of a real mask, if profiles show many full rejects.

## Multithreading (screen split)

Naive “each core owns a screen rectangle and renders everything” fights
this engine:

| Split | Issue |
|-------|--------|
| Horizontal bands | `max_y` and segment walks are full-width; load imbalance |
| Vertical strips | Trapezoids/sprites span strips; curve/`max_y` still sequential in \(z\) |
| Depth ranges | Far occlusion must complete before near is correct |
| Road vs sprites | Sprites need per-slice `clip` from the road pass |

A structure that *can* work:

1. **Serial:** project slices, compute `max_y` / clip / mouth (little or no fill).
2. **Parallel:** fill trapezoids / blit into tiles or non-overlapping row bands.
3. **Serial:** composite if needed; HUD / present.

Easier MT targets: main vs mirror camera; render vs present/upscale; keep
audio off the render thread.

**Do not** start with independent full views per rectangle without
duplicating projection or racing occlusion.

## Scanline assembly (“like a console”)

Two different ideas:

| Idea | Verdict |
|------|---------|
| **Console-style scanline renderer** — per row \(y\), emit colour from road spans + sprite spans (no full-rect blit first) | Coherent architecture; large rewrite |
| **One OS thread per scanline** | Poor: tiny work per line, overhead and cache thrash |

A span-oriented pass is closer to how pseudo-3D already thinks (horizontal
road spans, sprite footprints on rows). Build span lists (mostly serial,
shared `max_y`), then **fill row bands in parallel**. Full “everything is
scanlines, no framebuffer blits” is a new renderer, not a tweak to
`blit_scaled`.

Old consoles limited sprites per scanline in hardware. Software simulation
is flexible but not free; bookkeeping can exceed blit cost at SD once
overdraw is under control.

## Practical priority (as discussed)

1. Measure SD vs HD (`draw_segment` / `blit_scaled` / present).
2. Algorithmic cuts: cliff subsample (done), building size-cull / LOD.
3. Optional opaque cliff columns + y-buffer if still cliff-bound.
4. Optional solid-sprite 1-bit mask at HD if façades still dominate.
5. Serial project + parallel row-band fill only if fill remains the bottleneck.
6. Avoid thread-per-scanline and naive screen-split of the ordered pass.

Present/upscale cost can rival CPU fill at HD; check before a large MT
investment.
