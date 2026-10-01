#include "road.hpp"
#include <algorithm>
#include <cmath>

namespace racer {

void RoadSystem::add_segment(Track& t, float curve, float y) {
    Segment s;
    s.index = static_cast<int>(t.segments.size());
    s.curve = curve;
    s.y = y;
    // Alternate colors for classic look
    bool alt = (s.index / 3) % 2 == 0;
    s.color_road   = alt ? Palette::RoadLight : Palette::RoadDark;
    s.color_grass  = alt ? Palette::GrassLight : Palette::GrassDark;
    s.color_rumble = alt ? Palette::RumbleLight : Palette::RumbleDark;
    s.color_lane   = Palette::Lane;
    t.segments.push_back(s);
}

void RoadSystem::add_road(Track& t, int enter, int hold, int leave, float curve, float y) {
    // Smooth enter / hold / leave for curves and hills
    float y0 = t.segments.empty() ? 0.f : t.segments.back().y;
    for (int n = 0; n < enter; ++n) {
        float t_enter = static_cast<float>(n) / enter;
        add_segment(t, curve * t_enter, y0 + (y - y0) * t_enter);
    }
    for (int n = 0; n < hold; ++n) {
        add_segment(t, curve, y);
    }
    for (int n = 0; n < leave; ++n) {
        float t_leave = 1.f - static_cast<float>(n) / leave;
        add_segment(t, curve * t_leave, y);
    }
}

void RoadSystem::add_sprite(Track& t, int seg, float offset, int type, float scale) {
    if (seg < 0 || seg >= static_cast<int>(t.segments.size())) return;
    Segment::Sprite sp;
    sp.offset = offset;
    sp.type = type;
    sp.scale = scale;
    t.segments[seg].sprites.push_back(sp);
}

void RoadSystem::build_demo_track(Track& track) {
    track.segments.clear();
    track.segment_length = 200.f;
    track.road_width = 2000.f;
    track.rumble_width = 0.08f;
    track.lanes = 3;

    // Build a varied track with hills, valleys, cliffs, S-curves
    // Straight start
    add_road(track, 0, 50, 0, 0.f, 0.f);

    // Gentle right curve over a hill
    add_road(track, 20, 40, 20, 2.5f, 800.f);

    // Valley dip
    add_road(track, 15, 30, 15, 0.f, -400.f);

    // Sharp left with cliffside
    add_road(track, 10, 50, 20, -4.f, 200.f);

    // Climbing S-curve
    add_road(track, 15, 20, 15, 3.f, 1200.f);
    add_road(track, 15, 20, 15, -3.f, 1400.f);

    // Long downhill straight with valleys
    add_road(track, 10, 60, 10, 0.f, -200.f);

    // Cliff drop section (steep negative y change)
    add_road(track, 5, 10, 5, 1.f, -800.f);
    add_road(track, 10, 40, 10, 0.f, -600.f);

    // Recovery climb + final curves
    add_road(track, 20, 30, 20, -2.f, 400.f);
    add_road(track, 15, 40, 15, 2.5f, 0.f);
    add_road(track, 0, 80, 0, 0.f, 0.f); // finish straight

    // Sprinkle roadside sprites: trees and cliffs
    for (size_t i = 0; i < track.segments.size(); ++i) {
        if (i % 7 == 0) {
            add_sprite(track, static_cast<int>(i), -1.3f, 0, 1.0f + (i % 3) * 0.2f);
        }
        if (i % 11 == 0) {
            add_sprite(track, static_cast<int>(i), 1.4f, 0, 0.9f);
        }
        // Cliffs on steep sections
        if (track.segments[i].y < -300.f && i % 5 == 0) {
            add_sprite(track, static_cast<int>(i), -1.6f, 1, 1.5f);
            add_sprite(track, static_cast<int>(i), 1.6f, 1, 1.5f);
        }
        // Occasional billboards
        if (i % 40 == 20) {
            add_sprite(track, static_cast<int>(i), 1.8f, 2, 1.2f);
        }
    }

    track.rebuild_lengths();
}

void RoadSystem::project_segments(const Track& track, float player_z, float player_x,
                                  const Camera& cam, int screen_w, int screen_h,
                                  std::vector<Projected>& projected,
                                  float& max_y) {
    // Classic technique from Jake Gordon / Lou:
    // Walk segments from near to far, accumulate curve (dx) and height (dy),
    // project each with perspective scale = depth / z
    projected.resize(track.segments.size());

    float base_z = player_z;
    int base_idx = track.find_segment_index(base_z);
    float x = 0.f;
    float dx = 0.f;
    max_y = static_cast<float>(screen_h);

    // Camera is slightly behind the player for classic feel
    float camera_height = cam.height;
    float camera_depth = cam.depth;

    for (int n = 0; n < static_cast<int>(track.segments.size()); ++n) {
        int idx = (base_idx + n) % static_cast<int>(track.segments.size());
        const Segment& seg = track.get(idx);

        // World Z of this segment relative to player
        float seg_z = (idx * track.segment_length) - base_z;
        if (seg_z < 0) seg_z += track.total_length; // wrap for looping track feel

        // Only project within draw distance
        if (seg_z > cam.draw_distance * track.segment_length) {
            projected[n].scale = 0.f;
            continue;
        }

        // Accumulate curve
        // The classic "add dx, then add curve to dx"
        Projected& p = projected[n];
        float world_x = x - player_x * track.road_width;
        float world_y = seg.y - camera_height;
        float world_z = seg_z;

        // Perspective projection
        float scale = camera_depth / std::max(1.f, world_z / track.segment_length + camera_depth);
        p.scale = scale;
        p.x = screen_w / 2.f + scale * world_x * screen_w / 2.f;
        p.y = screen_h / 2.f - scale * world_y * screen_h / 2.f;
        p.w = scale * track.road_width * screen_w / 2.f;
        p.clip = max_y;

        // Update max_y for hill clipping (painter's algorithm)
        if (p.y < max_y) max_y = p.y;

        // Advance curve accumulator for next segment
        x += dx;
        dx += seg.curve;
    }
}

void RoadSystem::render(Renderer& r, const Track& track,
                        const std::vector<Projected>& projected,
                        float player_z, float player_x, const Camera& cam) {
    // Draw from far to near (painter's algorithm) for correct overdraw on hills
    int base_idx = track.find_segment_index(player_z);
    float max_y = static_cast<float>(r.height());

    // First pass: find far segments still visible
    int draw_count = 0;
    for (size_t n = 0; n < projected.size(); ++n) {
        if (projected[n].scale > 0.001f) draw_count = static_cast<int>(n) + 1;
    }

    for (int n = draw_count - 1; n >= 0; --n) {
        int idx = (base_idx + n) % static_cast<int>(track.segments.size());
        const Segment& seg = track.get(idx);
        const Projected& p1 = projected[n];
        // Next segment for trapezoid (or same if last)
        int n2 = std::min(n + 1, static_cast<int>(projected.size()) - 1);
        const Projected& p2 = projected[n2];

        if (p1.scale < 0.001f || p2.scale < 0.001f) continue;
        // Clip against previous max_y (hills hide far road)
        if (p1.y >= max_y && p2.y >= max_y) continue;

        r.draw_segment(p1, p2, seg.color_road, seg.color_grass,
                       seg.color_rumble, seg.color_lane,
                       track.lanes, track.rumble_width);

        // Sprites on this segment (billboards / trees / cliffs)
        for (const auto& sp : seg.sprites) {
            float sprite_x = p1.x + p1.w * sp.offset;
            float sprite_y = p1.y;
            float sprite_scale = p1.scale * sp.scale * 0.3f;
            // Only draw if not clipped by hill
            if (sprite_y < max_y + 20.f) {
                r.draw_sprite(sprite_x, sprite_y, sprite_scale, sp.type, sp.offset < 0);
            }
        }

        if (p1.y < max_y) max_y = p1.y;
    }

    (void)player_x;
    (void)cam;
}

} // namespace racer
