// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "road.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

// `direction` is +1 looking along the track and -1 looking back; `horizon`
// is the screen row of eye level and `y_scale` the vertical pixels per unit.
void project(ScreenPoint& p, float world_x, float world_y, float world_z,
             float cam_x, float cam_y, float cam_z, float depth, int direction,
             int screen_w, float horizon, float y_scale, float road_width) {
    p.cam_z = (world_z - cam_z) * static_cast<float>(direction);
    p.scale = depth / p.cam_z;
    const float half_w = static_cast<float>(screen_w) / 2.f;
    p.x = half_w + p.scale * (world_x - cam_x) * half_w;
    p.y = horizon - p.scale * (world_y - cam_y) * y_scale;
    p.w = p.scale * road_width * half_w;
}

float exponential_fog(float distance, float density) {
    return 1.f / std::exp(distance * distance * density);
}

int clip_row(float clip_y) { return pixel_edge(clip_y); }

} // namespace

void RoadRenderer::render(Framebuffer& fb, const Track& track, const RoadView& view,
                          const SpriteSheet& sprites, std::vector<RoadSprite>& objects) {
    const int n_segments = static_cast<int>(track.segments.size());
    const float seg_len = track.segment_length;
    const float track_len = track.length();

    const int base = track.index_at(view.position);
    const float base_percent = std::fmod(track.wrap(view.position), seg_len) / seg_len;
    const float cam_y = view.player_y + view.camera_height;
    const int dir = view.direction < 0 ? -1 : 1;
    const float half_h = static_cast<float>(fb.height()) / 2.f;
    const float horizon = view.horizon > 0.f ? view.horizon : half_h;
    const float y_scale = view.y_scale > 0.f ? view.y_scale : half_h;

    // The camera may sit partway into the base segment, so start the curve
    // accumulation with the part of the curve already passed. Looking back,
    // the bend accumulates the same way: a right-hand bend curves to the
    // right behind the car as well as ahead of it, and the mirror keeps sides.
    float x = 0.f;
    float dx = -track.segment(base).curve * (dir > 0 ? base_percent : 1.f - base_percent) + view.yaw;
    float max_y = static_cast<float>(fb.height());

    camera_depth_ = view.camera_depth;
    direction_ = dir;
    const int count = std::min(view.draw_distance, n_segments);
    slices_.clear();
    slices_.reserve(static_cast<size_t>(count));

    for (int n = 0; n < count; ++n) {
        const int index = ((base + dir * n) % n_segments + n_segments) % n_segments;
        const Segment& seg = track.segment(index);
        // Segments past the end of the track (or before its start, looking
        // back) are seen through the loop.
        float loop = 0.f;
        if (dir > 0 && index < base) loop = track_len;
        if (dir < 0 && index > base) loop = -track_len;
        const float cam_z = view.position - loop;
        // The car's lateral position, in half-widths of the road where it is.
        const float cam_x = view.player_x * track.half_width_at(view.position + view.player_z) + view.shift;
        const float z1 = static_cast<float>(index) * seg_len;
        const float z2 = z1 + seg_len;

        Slice s;
        s.index = index;
        const int near = dir > 0 ? index : index + 1; // boundaries at the near and far end
        project(s.p1, x, dir > 0 ? seg.y1 : seg.y2, dir > 0 ? z1 : z2, cam_x, cam_y, cam_z,
                view.camera_depth, dir, fb.width(), horizon, y_scale, track.half_width(near));
        project(s.p2, x + dx, dir > 0 ? seg.y2 : seg.y1, dir > 0 ? z2 : z1, cam_x, cam_y, cam_z,
                view.camera_depth, dir, fb.width(), horizon, y_scale, track.half_width(near + dir));
        x += dx;
        dx += seg.curve;

        s.clip = max_y;
        s.fog = exponential_fog(static_cast<float>(n) / static_cast<float>(count), view.fog_density);
        // Skip segments behind the camera, facing away (downhill beyond a
        // crest), or fully hidden behind nearer road.
        s.road_visible = s.p1.cam_z > view.camera_depth &&
                         s.p2.y < s.p1.y &&
                         s.p2.y < max_y;
        if (s.road_visible) {
            draw_segment(fb, track, s);
            max_y = s.p2.y;
        }
        slices_.push_back(s);
    }

    for (RoadSprite& o : objects) o.z = track.wrap(o.z);
    std::sort(objects.begin(), objects.end(),
              [](const RoadSprite& a, const RoadSprite& b) { return a.z < b.z; });
    draw_sprites(fb, track, sprites, objects);
}

void RoadRenderer::draw_segment(Framebuffer& fb, const Track& track, const Slice& s) const {
    const Segment& seg = track.segment(s.index);
    const RoadTheme& theme = track.look(s.index);
    const int band = seg.alt ? 0 : 1;
    const float fog_amount = 1.f - s.fog;
    auto fogged = [&](Color c) { return blend(c, theme.fog, fog_amount); };

    const ScreenPoint& a = s.p1; // near
    const ScreenPoint& b = s.p2; // far
    const int lanes = std::max(1, theme.lanes);

    // Only draw rows above the nearer road already on screen.
    fb.set_clip(0, 0, fb.width(), clip_row(s.clip));

    // Grass spans the full width.
    fb.fill_trapezoid(b.y, 0.f, static_cast<float>(fb.width()),
                      a.y, 0.f, static_cast<float>(fb.width()), fogged(theme.grass[band]));

    // Ground beyond a rail or cliff: sea or valley, or rock. It is mostly hidden
    // behind the edge feature itself, which is drawn later with the sprites.
    const float wf = static_cast<float>(fb.width());
    for (int side = -1; side <= 1; side += 2) {
        const Edge kind = side < 0 ? seg.left : seg.right;
        if (kind == Edge::None) continue;
        const float off = kind == Edge::Rail ? rail_offset : cliff_offset;
        const Color c = kind == Edge::Rail ? fogged(theme.beyond[band]) : fogged(theme.rock[0]);
        const float xa = a.x + static_cast<float>(side) * off * a.w;
        const float xb = b.x + static_cast<float>(side) * off * b.w;
        if (side < 0) fb.fill_trapezoid(b.y, 0.f, xb, a.y, 0.f, xa, c);
        else fb.fill_trapezoid(b.y, xb, wf, a.y, xa, wf, c);
    }

    const int near = direction_ > 0 ? s.index : s.index + 1;

    // Where a fork's routes part or meet, the other route's road beside
    // ours. Where the two overlap they must look the same whichever is the
    // active one, so both roads' rumble strips go down first, then both
    // surfaces, then both lane markings.
    const float oa = track.branch_offset(near), ob = track.branch_offset(near + direction_);
    const bool other_road = !std::isnan(oa) && !std::isnan(ob);
    const float oca = other_road ? a.x + oa * a.w : 0.f, ocb = other_road ? b.x + ob * b.w : 0.f;

    // A gas station's forecourt on the right: paved, with a kerb at its edge.
    const float court_a = track.forecourt_at(near), court_b = track.forecourt_at(near + direction_);
    if (court_a > 1.f || court_b > 1.f) {
        const float oa = std::max(court_a, 1.f), ob = std::max(court_b, 1.f);
        const Color paving = fogged(blend(theme.road[band], Color{0xb4, 0xb0, 0xa8}, 0.3f));
        fb.fill_trapezoid(b.y, b.x + b.w, b.x + ob * b.w, a.y, a.x + a.w, a.x + oa * a.w, paving);
        const float ka = a.w / 40.f, kb = b.w / 40.f;
        fb.fill_trapezoid(b.y, b.x + ob * b.w, b.x + ob * b.w + kb, a.y, a.x + oa * a.w, a.x + oa * a.w + ka,
                          fogged(theme.rumble[0]));
    }

    // Rumble strips (the other road's too), then the other road's surface.
    const float ra = a.w / static_cast<float>(std::max(6, 2 * lanes));
    const float rb = b.w / static_cast<float>(std::max(6, 2 * lanes));
    const Color rumble = fogged(theme.rumble[band]);
    if (other_road) {
        fb.fill_trapezoid(b.y, ocb - b.w - rb, ocb + b.w + rb, a.y, oca - a.w - ra, oca + a.w + ra, rumble);
    }
    fb.fill_trapezoid(b.y, b.x - b.w - rb, b.x - b.w, a.y, a.x - a.w - ra, a.x - a.w, rumble);
    fb.fill_trapezoid(b.y, b.x + b.w, b.x + b.w + rb, a.y, a.x + a.w, a.x + a.w + ra, rumble);
    if (other_road) fb.fill_trapezoid(b.y, ocb - b.w, ocb + b.w, a.y, oca - a.w, oca + a.w, fogged(theme.road[band]));

    if (seg.checker) {
        // Chequered start/finish line, two rows of squares.
        constexpr int squares = 8;
        const int parity = s.index % 2;
        for (int i = 0; i < squares; ++i) {
            const float f0 = static_cast<float>(i) / squares;
            const float f1 = static_cast<float>(i + 1) / squares;
            const Color c = fogged(theme.checker[(i + parity) % 2]);
            fb.fill_trapezoid(b.y, b.x - b.w + 2.f * b.w * f0, b.x - b.w + 2.f * b.w * f1,
                              a.y, a.x - a.w + 2.f * a.w * f0, a.x - a.w + 2.f * a.w * f1, c);
        }
    } else {
        fb.fill_trapezoid(b.y, b.x - b.w, b.x + b.w, a.y, a.x - a.w, a.x + a.w,
                          fogged(theme.road[band]));

        // Dashed lane markers on alternating bands.
        if (seg.alt && lanes > 1 && !(theme.us_markings && lanes == 2)) {
            const float la = a.w / static_cast<float>(std::max(32, 8 * lanes));
            const float lb = b.w / static_cast<float>(std::max(32, 8 * lanes));
            const Color lane = fogged(theme.lane);
            for (int i = 1; i < lanes; ++i) {
                const float f = static_cast<float>(i) / static_cast<float>(lanes);
                const float xa = a.x - a.w + 2.f * a.w * f;
                const float xb = b.x - b.w + 2.f * b.w * f;
                fb.fill_trapezoid(b.y, xb - lb, xb + lb, a.y, xa - la, xa + la, lane);
            }
        }

        if (theme.us_markings) {
            // Solid white edge lines, and on two lane roads a double yellow line.
            const Color white = fogged(theme.lane);
            const float e = 0.93f, t = 1.f / 60.f;
            for (int side = -1; side <= 1; side += 2) {
                const float sd = static_cast<float>(side);
                fb.fill_trapezoid(b.y, b.x + sd * e * b.w - b.w * t, b.x + sd * e * b.w + b.w * t,
                                  a.y, a.x + sd * e * a.w - a.w * t, a.x + sd * e * a.w + a.w * t, white);
            }
            if (lanes == 2) {
                const Color yellow = fogged(theme.center_line);
                const float gap = 0.035f, th = 1.f / 80.f;
                for (int side = -1; side <= 1; side += 2) {
                    const float sd = static_cast<float>(side);
                    fb.fill_trapezoid(b.y, b.x + sd * gap * b.w - b.w * th, b.x + sd * gap * b.w + b.w * th,
                                      a.y, a.x + sd * gap * a.w - a.w * th, a.x + sd * gap * a.w + a.w * th, yellow);
                }
            }
        }
    }

    // The other road's lane markings, over both surfaces.
    if (other_road && seg.alt) {
        const float la = a.w / static_cast<float>(std::max(32, 8 * lanes));
        const float lb = b.w / static_cast<float>(std::max(32, 8 * lanes));
        for (int i = 1; i < lanes; ++i) {
            const float f = static_cast<float>(i) / static_cast<float>(lanes);
            const float xa = oca - a.w + 2.f * a.w * f, xb = ocb - b.w + 2.f * b.w * f;
            fb.fill_trapezoid(b.y, xb - lb, xb + lb, a.y, xa - la, xa + la,
                              fogged(theme.us_markings ? theme.center_line : theme.lane));
        }
    }

    // A patch on the road. Water: darker than the road at its edge,
    // mirroring the sky towards the middle, with thin glints of light. Oil:
    // nearly black, with an iridescent sheen of purple, teal and gold.
    const float wa = track.patch_width_at(near), wb = track.patch_width_at(near + direction_);
    if (seg.patch != Patch::None && (wa > 0.f || wb > 0.f)) {
        const float ca = track.patch_center_at(near), cb = track.patch_center_at(near + direction_);
        const float xa = a.x + ca * a.w, xb = b.x + cb * b.w;
        auto band_of = [&](float from, float to, Color c) {
            fb.fill_trapezoid(b.y, xb + from * wb * b.w, xb + to * wb * b.w, a.y, xa + from * wa * a.w, xa + to * wa * a.w,
                              fogged(c));
        };
        const int glint = s.index % 4; // glints scattered along the patch
        if (seg.patch == Patch::Oil) {
            const Color slick{0x16, 0x14, 0x1a};
            const Color sheen[3] = {{0x6c, 0x3c, 0x7c}, {0x2c, 0x74, 0x7c}, {0x8c, 0x7c, 0x34}};
            band_of(-1.f, 1.f, slick);
            band_of(-0.55f, -0.35f, blend(slick, sheen[s.index % 3], 0.8f));
            band_of(0.05f, 0.2f, blend(slick, sheen[(s.index + 1) % 3], 0.7f));
            if (glint == 2) band_of(0.4f, 0.55f, blend(slick, sheen[(s.index + 2) % 3], 0.6f));
        } else {
            const Color edge = blend(theme.road[band], Color{0x10, 0x12, 0x18}, 0.4f);
            const Color mirror = blend(edge, theme.sky_horizon, 0.35f);
            band_of(-1.f, 1.f, edge);
            band_of(-0.75f, 0.75f, mirror);
            if (glint == 1) band_of(-0.45f, -0.3f, blend(mirror, Color{0xff, 0xff, 0xff}, 0.45f));
            if (glint == 3) band_of(0.2f, 0.32f, blend(mirror, Color{0xff, 0xff, 0xff}, 0.35f));
        }
    }

    fb.reset_clip();
}

namespace {

// Colour of the rock face at height h above the ground. `top` is the wall's
// height at this column and u its position along the track, so the strata
// and cracks stay attached to the rock as the camera moves.
// Smooth noise along the track (u in segments), 0 .. 1, varying over about
// 1 / freq segments.
float smooth_noise(float u, float freq, int seed) {
    const float p = u * freq;
    const float i = std::floor(p), t = p - i;
    const float a = hash01(static_cast<int>(i), seed), b = hash01(static_cast<int>(i) + 1, seed);
    return a + (b - a) * t * t * (3.f - 2.f * t);
}

// How far the cliff face juts out at u, 0 (a gully) .. 1 (a buttress):
// ridges a couple of segments apart, with smaller ribs on them.
float cliff_relief(float u) { return 0.7f * smooth_noise(u, 0.5f, 21) + 0.3f * smooth_noise(u, 1.7f, 22); }

// `light` is how the face is turned at this column: below 1 away from the
// light (the shaded flank of a ridge, the back of a gully), above 1 towards it.
Color cliff_color(const RoadTheme& th, float h, float top, float u, float light, int x, int y) {
    const float dither = bayer4(x, y) - 0.5f;
    const float strata = h / 260.f + 0.5f * hash01(static_cast<int>(std::floor(u * 1.5f)), 3) + 0.3f * dither;
    const int band = static_cast<int>(std::floor(strata));
    Color c = th.rock[((band % 3) + 3) % 3];
    const float frac = strata - std::floor(strata);
    if (frac < 0.14f) c = blend(c, th.rock[0], 0.65f);       // crevice between layers
    else if (frac < 0.24f) c = blend(c, th.rock[2], 0.4f);   // the lit lip of the ledge above it
    if (hash01(static_cast<int>(std::floor(u * 4.f)), 9) > 0.86f) c = blend(c, th.rock[0], 0.5f); // crack
    // Darker where the wall meets the ground, catching more light higher up.
    c = blend(th.rock[0], c, 0.55f + 0.45f * std::clamp(h / 1100.f, 0.f, 1.f));
    // The relief: flanks turned away fall into shadow, those facing the light
    // brighten; dithered so the bands stay pixel art.
    const float l = light + 0.12f * dither;
    if (l < 1.f) c = blend(c, blend(th.rock[0], Color{0x10, 0x0c, 0x10}, 0.45f), std::min(1.f, 1.f - l));
    else c = blend(c, th.rock[2], std::min(1.f, l - 1.f));
    if (th.cap_amount > 0.01f && h > top - 700.f * th.cap_amount + 220.f * dither) c = th.cap;
    return c;
}

} // namespace

void RoadRenderer::draw_edge(Framebuffer& fb, const Track& track, const Slice& s, int side) const {
    const Segment& seg = track.segment(s.index);
    const Edge kind = side < 0 ? seg.left : seg.right;
    if (kind == Edge::None) return;

    const RoadTheme& th = track.look(s.index);
    const float half_w = static_cast<float>(fb.width()) / 2.f;
    const float off = (kind == Edge::Rail ? rail_offset : cliff_offset) * static_cast<float>(side);
    const float fog_amount = 1.f - s.fog;
    const ScreenPoint& a = s.p1;
    const ScreenPoint& b = s.p2;

    // Screen position of the base line and the top line at both ends.
    const float xa = a.x + off * a.w, xb = b.x + off * b.w;
    // Boundary index (segment start) and track position at the near end.
    const int near = direction_ > 0 ? s.index : s.index + 1;
    const float h1 = track.edge_height(near, side);
    const float h2 = track.edge_height(near + direction_, side);
    const float ppu_a = a.scale * half_w, ppu_b = b.scale * half_w; // pixels per world unit
    const float ta = a.y - h1 * ppu_a, tb = b.y - h2 * ppu_b;
    if (std::abs(xb - xa) < 0.01f) return; // seen edge-on

    const int x0 = std::max(0, pixel_edge(std::min(xa, xb)));
    const int x1 = std::min(fb.width(), pixel_edge(std::max(xa, xb)));
    const float u0 = static_cast<float>(near);
    const float du = static_cast<float>(direction_);

    for (int x = x0; x < x1; ++x) {
        const float t = std::clamp((static_cast<float>(x) + 0.5f - xa) / (xb - xa), 0.f, 1.f);
        const float u = u0 + du * t; // position along the track in segments
        const float base = a.y + (b.y - a.y) * t;
        const float top = ta + (tb - ta) * t;
        const float ppu = ppu_a + (ppu_b - ppu_a) * t;
        float height = (base - top) / ppu; // world units at this column
        float light = 1.f;
        float top_y = top;
        if (kind == Edge::Cliff) {
            // Ridges and gullies along the face: lit on the flank facing the
            // light, shaded on the other, gullies darker; buttresses stand
            // taller and the top edge is ragged.
            const float relief = cliff_relief(u);
            const float slope = (cliff_relief(u + 0.02f) - cliff_relief(u - 0.02f)) / 0.04f;
            light = 0.9f + 0.4f * std::clamp(slope * static_cast<float>(side) * 0.8f, -1.f, 1.f) +
                    0.45f * (relief - 0.5f);
            height *= 0.9f + 0.12f * relief + 0.06f * (smooth_noise(u, 7.f, 23) - 0.5f);
            top_y = base - height * ppu;
        }
        const int y0 = pixel_edge(top_y), y1 = pixel_edge(base);
        for (int y = y0; y < y1; ++y) {
            const float h = (base - (static_cast<float>(y) + 0.5f)) / ppu;
            Color c;
            if (kind == Edge::Cliff) {
                c = cliff_color(th, h, height, u, light, x, y);
            } else {
                // Two horizontal bars on posts; the gaps show the ground behind.
                const bool post = (direction_ > 0 ? t : 1.f - t) < 0.1f; // at the segment start
                const float r = h / rail_height;
                const bool upper = r > 0.6f && r <= 0.95f;
                const bool lower = r > 0.2f && r <= 0.42f;
                if (!post && !upper && !lower) continue;
                c = post ? th.rail[1] : th.rail[0];
                if (upper && r > 0.88f) c = blend(c, Color{255, 255, 255}, 0.4f); // highlight
            }
            fb.put_pixel(x, y, blend(c, th.fog, fog_amount));
        }
    }
}

void RoadRenderer::draw_sprites(Framebuffer& fb, const Track& track, const SpriteSheet& sprites,
                                const std::vector<RoadSprite>& objects) const {
    const float half_w = static_cast<float>(fb.width()) / 2.f;
    const float seg_len = track.segment_length;

    // Far to near so nearer objects overdraw farther ones. Each object is
    // clipped against the road that was in front of its segment, so objects
    // behind a crest peek over it.
    for (auto it = slices_.rbegin(); it != slices_.rend(); ++it) {
        const Slice& s = *it;
        // Projected objects on segments reaching behind the camera would blow
        // up to absurd sizes; fixed screen objects are still drawn.
        const bool projectable = s.p1.cam_z > camera_depth_;
        const Segment& seg = track.segment(s.index);
        const int clip = clip_row(s.clip);
        const float fog_amount = 1.f - s.fog;

        fb.set_clip(0, 0, fb.width(), clip);
        if (projectable) {
            draw_edge(fb, track, s, -1);
            draw_edge(fb, track, s, +1);
        }
        const ScreenPoint& p0 = start(s);
        // Scenery at `shift` road half-widths from where it belongs.
        auto plant = [&](const RoadsideObject& obj, float shift) {
            const SceneryInfo& info = scenery_info(obj.kind);
            const float px_per_unit = p0.scale * half_w;
            const float width = info.width * px_per_unit;
            float left = p0.x + (obj.offset + shift) * track.half_width(s.index) * px_per_unit;
            if (info.centered) left -= width / 2.f;
            else if (obj.offset < 0.f) left -= width;
            const Bitmap& bmp = direction_ > 0 ? sprites.scenery(obj.kind) : sprites.scenery_back(obj.kind);
            const float height = width * static_cast<float>(bmp.h) / static_cast<float>(bmp.w);
            const bool flip = info.mirrorable && obj.offset < 0.f;
            fb.blit_scaled(bmp, left, p0.y - height, width, height, flip,
                           fog_amount, track.look(s.index).fog);
        };
        if (projectable) {
            // At a fork, the other route's scenery beside its road too, so
            // that nothing appears or vanishes when the player changes road.
            const float shift = track.branch_offset(s.index);
            if (const Segment* other = track.other_route_segment(s.index); other && !std::isnan(shift)) {
                for (const RoadsideObject& obj : other->scenery) plant(obj, shift);
            }
            for (const RoadsideObject& obj : seg.scenery) plant(obj, 0.f);
        }

        // Moving objects on this segment, far to near.
        const float z0 = static_cast<float>(s.index) * seg_len;
        auto first = std::lower_bound(objects.begin(), objects.end(), z0,
                                      [](const RoadSprite& o, float z) { return o.z < z; });
        auto last = std::lower_bound(first, objects.end(), z0 + seg_len,
                                     [](const RoadSprite& o, float z) { return o.z < z; });
        auto draw_object = [&](const RoadSprite& o) {
            const Bitmap& bmp = *o.bitmap;
            if (o.fixed) {
                fb.reset_clip();
                if (o.angle != 0.f) fb.blit_rotated(bmp, o.sx + o.sw / 2.f, o.sy + o.sh / 2.f, o.sw, o.sh, o.angle);
                else fb.blit_scaled(bmp, o.sx, o.sy, o.sw, o.sh);
                fb.set_clip(0, 0, fb.width(), clip);
                return;
            }
            if (!projectable) return;
            const ScreenPoint& p1 = end(s);
            const float t = (o.z - z0) / seg_len;
            const float scale = p0.scale + (p1.scale - p0.scale) * t;
            const float x = p0.x + (p1.x - p0.x) * t;
            const float y = p0.y + (p1.y - p0.y) * t;
            const float px_per_unit = scale * half_w;
            const float width = o.world_width * px_per_unit;
            const float height = width * static_cast<float>(bmp.h) / static_cast<float>(bmp.w);
            const float cx = x + o.offset * track.half_width_at(o.z) * px_per_unit;
            fb.blit_scaled(bmp, cx - width / 2.f, y - height, width, height, false,
                           fog_amount, track.look(s.index).fog);
        };
        if (direction_ > 0) {
            for (auto o = std::make_reverse_iterator(last); o != std::make_reverse_iterator(first); ++o) draw_object(*o);
        } else {
            for (auto o = first; o != last; ++o) draw_object(*o);
        }
    }
    fb.reset_clip();
}

} // namespace racer
