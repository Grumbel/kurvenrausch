#include "road.hpp"
#include <algorithm>
#include <cmath>

namespace racer {

void RoadSystem::add_segment(Track& t, float curve, float y) {
    Segment s;
    s.index = static_cast<int>(t.segments.size());
    s.curve = curve;
    s.y = y;
    bool alt = ((s.index / 3) % 2) == 0;
    s.color_road   = alt ? Palette::RoadLight : Palette::RoadDark;
    s.color_grass  = alt ? Palette::GrassLight : Palette::GrassDark;
    s.color_rumble = alt ? Palette::RumbleLight : Palette::RumbleDark;
    s.color_lane   = Palette::Lane;
    t.segments.push_back(s);
}

void RoadSystem::add_road(Track& t, int enter, int hold, int leave, float curve, float y) {
    float y0 = t.segments.empty() ? 0.f : t.segments.back().y;
    auto ease_in  = [](float a, float b, float p) { return a + (b - a) * p * p; };
    auto ease_out = [](float a, float b, float p) { return a + (b - a) * (1.f - (1.f - p) * (1.f - p)); };
    auto ease_inout = [&](float a, float b, float p) {
        return p < 0.5f ? ease_in(a, (a + b) * 0.5f, p * 2.f)
                        : ease_out((a + b) * 0.5f, b, p * 2.f - 1.f);
    };

    for (int n = 0; n < enter; ++n) {
        float p = static_cast<float>(n) / std::max(1, enter);
        add_segment(t, ease_in(0.f, curve, p), ease_inout(y0, y, p));
    }
    for (int n = 0; n < hold; ++n)
        add_segment(t, curve, y);
    for (int n = 0; n < leave; ++n) {
        float p = static_cast<float>(n) / std::max(1, leave);
        add_segment(t, ease_out(curve, 0.f, p), y);
    }
}

void RoadSystem::add_sprite(Track& t, int seg, float offset, int type, float scale) {
    if (seg < 0 || seg >= static_cast<int>(t.segments.size())) return;
    t.segments[seg].sprites.push_back({offset, scale, type});
}

void RoadSystem::build_demo_track(Track& track) {
    track.segments.clear();
    track.segment_length = 200.f;
    track.road_width     = 2000.f;
    track.rumble_width   = 0.12f;
    track.lanes          = 3;

    add_road(track, 0, 40, 0, 0.f, 0.f);
    add_road(track, 15, 30, 15, 3.f, 600.f);
    add_road(track, 12, 25, 12, 0.f, -300.f);
    add_road(track, 10, 35, 15, -4.5f, 200.f);
    add_road(track, 12, 18, 12, 3.5f, 900.f);
    add_road(track, 12, 18, 12, -3.5f, 1100.f);
    add_road(track, 8, 50, 8, 0.f, -100.f);
    add_road(track, 5, 12, 5, 1.5f, -700.f);
    add_road(track, 8, 30, 8, 0.f, -500.f);
    add_road(track, 15, 25, 15, -2.5f, 300.f);
    add_road(track, 12, 30, 12, 2.5f, 0.f);
    add_road(track, 0, 60, 0, 0.f, 0.f);

    for (size_t i = 0; i < track.segments.size(); ++i) {
        if (i % 6 == 0)
            add_sprite(track, static_cast<int>(i), -1.35f, 0, 1.0f + (i % 4) * 0.15f);
        if (i % 9 == 3)
            add_sprite(track, static_cast<int>(i),  1.40f, 0, 0.9f + (i % 3) * 0.1f);
        if (track.segments[i].y < -250.f && i % 4 == 0) {
            add_sprite(track, static_cast<int>(i), -1.55f, 1, 1.4f);
            add_sprite(track, static_cast<int>(i),  1.55f, 1, 1.4f);
        }
        if (i % 35 == 17)
            add_sprite(track, static_cast<int>(i), 1.7f, 2, 1.1f);
    }
    track.rebuild();
}

/*
 * Classic pseudo-3D projection (Jake Gordon javascript-racer / Lou)
 *
 *   camera_space.z = world.z - camera.z
 *   scale          = cameraDepth / camera_space.z
 *   screen.x       = width/2  + scale * camera_space.x * width/2
 *   screen.y       = height/2 - scale * camera_space.y * height/2
 *   screen.w       = scale * roadWidth * width/2
 *
 * Curves: while walking segments, x += dx; dx += segment.curve
 * Hills:  segment.y feeds into camera_space.y; far→near + maxY clip
 */
void RoadSystem::project_segments(const Track& track,
                                  float player_z, float player_x,
                                  const Camera& cam,
                                  int screen_w, int screen_h,
                                  std::vector<Projected>& projected,
                                  float& max_y) {
    const int n_seg  = static_cast<int>(track.segments.size());
    const int draw_n = std::min(n_seg - 1, static_cast<int>(cam.draw_distance));
    projected.assign(static_cast<size_t>(draw_n + 1), Projected{});

    const int   base_idx     = track.index_from_z(player_z);
    const float base_percent = player_z / track.segment_length
                               - std::floor(player_z / track.segment_length);

    const float camera_x     = player_x * track.road_width;
    const float camera_y     = cam.height;
    const float camera_z     = player_z;
    const float camera_depth = cam.depth;   // ~0.84

    float x  = 0.f;
    float dx = -(base_percent * track.get(base_idx).curve);  // start mid-segment

    max_y = static_cast<float>(screen_h);

    for (int n = 0; n <= draw_n; ++n) {
        const int idx = (base_idx + n) % n_seg;
        const Segment& seg = track.get(idx);

        // World Z of this segment's near edge
        float world_z = (static_cast<float>(base_idx + n) - base_percent)
                        * track.segment_length;
        // Actually simpler: distance in front of camera
        float cz = (n - base_percent) * track.segment_length;
        if (cz < 1.f) cz = 1.f;

        float cx = x - camera_x;
        float cy = seg.y - camera_y;

        // THE classic formula
        float scale = camera_depth / cz;

        Projected& p = projected[static_cast<size_t>(n)];
        p.scale = scale;
        p.x = (screen_w / 2.f) + (scale * cx * screen_w  / 2.f);
        p.y = (screen_h / 2.f) - (scale * cy * screen_h  / 2.f);
        p.w = (scale * track.road_width * screen_w / 2.f);
        p.clip = max_y;

        // Accumulate curve for next segment
        x  += dx;
        dx += seg.curve;
        (void)world_z;
        (void)camera_z;
    }
}

void RoadSystem::render(Renderer& r, const Track& track,
                        const std::vector<Projected>& projected,
                        float player_z, float /*player_x*/,
                        const Camera& cam) {
    const int n_seg    = static_cast<int>(track.segments.size());
    const int base_idx = track.index_from_z(player_z);
    const int draw_n   = static_cast<int>(projected.size()) - 1;
    if (draw_n < 1) return;

    float max_y = static_cast<float>(r.height());

    for (int n = draw_n - 1; n >= 0; --n) {
        const Projected& p1 = projected[static_cast<size_t>(n)];
        const Projected& p2 = projected[static_cast<size_t>(n + 1)];

        if (p1.scale <= 0.f || p2.scale <= 0.f) continue;
        if (p1.y >= max_y && p2.y >= max_y) continue;

        // Don't draw segments that are above the screen or inverted
        if (p1.y < 0.f && p2.y < 0.f) continue;

        const int idx = (base_idx + n) % n_seg;
        const Segment& seg = track.get(idx);

        r.draw_segment(p1, p2,
                       seg.color_road, seg.color_grass,
                       seg.color_rumble, seg.color_lane,
                       track.lanes, track.rumble_width);

        for (const auto& sp : seg.sprites) {
            float sx = p1.x + p1.w * sp.offset;
            float sy = p1.y;
            float sc = p1.scale * sp.scale * 300.f;  // sprite size tune
            if (sy < max_y + 50.f && sc > 0.5f)
                r.draw_sprite(sx, sy, sc, sp.type, sp.offset < 0.f);
        }

        if (p1.y < max_y)
            max_y = p1.y;
    }
    (void)cam;
}

} // namespace racer
