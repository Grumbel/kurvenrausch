#include "road.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

void project(ScreenPoint& p, float world_x, float world_y, float world_z,
             float cam_x, float cam_y, float cam_z, float depth,
             int screen_w, int screen_h, float road_width) {
    p.cam_z = world_z - cam_z;
    p.scale = depth / p.cam_z;
    const float half_w = static_cast<float>(screen_w) / 2.f;
    const float half_h = static_cast<float>(screen_h) / 2.f;
    p.x = half_w + p.scale * (world_x - cam_x) * half_w;
    p.y = half_h - p.scale * (world_y - cam_y) * half_h;
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

    // The camera may sit partway into the base segment, so start the curve
    // accumulation with the part of the curve already passed.
    float x = 0.f;
    float dx = -track.segment(base).curve * base_percent;
    float max_y = static_cast<float>(fb.height());

    camera_depth_ = view.camera_depth;
    const int count = std::min(view.draw_distance, n_segments);
    slices_.clear();
    slices_.reserve(static_cast<size_t>(count));

    for (int n = 0; n < count; ++n) {
        const int index = (base + n) % n_segments;
        const Segment& seg = track.segment(index);
        // Segments past the end of the track are seen through the loop.
        const float loop = index < base ? track_len : 0.f;
        const float cam_z = view.position - loop;
        const float cam_x = view.player_x * track.road_width;
        const float z1 = static_cast<float>(index) * seg_len;

        Slice s;
        s.index = index;
        project(s.p1, x, seg.y1, z1, cam_x, cam_y, cam_z, view.camera_depth,
                fb.width(), fb.height(), track.road_width);
        project(s.p2, x + dx, seg.y2, z1 + seg_len, cam_x, cam_y, cam_z, view.camera_depth,
                fb.width(), fb.height(), track.road_width);
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
    const RoadTheme& theme = track.theme;
    const int band = seg.alt ? 0 : 1;
    const float fog_amount = 1.f - s.fog;
    auto fogged = [&](Color c) { return blend(c, theme.fog, fog_amount); };

    const ScreenPoint& a = s.p1; // near
    const ScreenPoint& b = s.p2; // far
    const int lanes = std::max(1, track.lanes);

    // Only draw rows above the nearer road already on screen.
    fb.set_clip(0, 0, fb.width(), clip_row(s.clip));

    // Grass spans the full width.
    fb.fill_trapezoid(b.y, 0.f, static_cast<float>(fb.width()),
                      a.y, 0.f, static_cast<float>(fb.width()), fogged(theme.grass[band]));

    // Rumble strips.
    const float ra = a.w / static_cast<float>(std::max(6, 2 * lanes));
    const float rb = b.w / static_cast<float>(std::max(6, 2 * lanes));
    const Color rumble = fogged(theme.rumble[band]);
    fb.fill_trapezoid(b.y, b.x - b.w - rb, b.x - b.w, a.y, a.x - a.w - ra, a.x - a.w, rumble);
    fb.fill_trapezoid(b.y, b.x + b.w, b.x + b.w + rb, a.y, a.x + a.w, a.x + a.w + ra, rumble);

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
        if (seg.alt && lanes > 1) {
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
    }

    fb.reset_clip();
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
        for (const RoadsideObject& obj : seg.scenery) {
            if (!projectable) break;
            const SceneryInfo& info = scenery_info(obj.kind);
            const float px_per_unit = s.p1.scale * half_w;
            const float width = info.width * px_per_unit;
            float left = s.p1.x + obj.offset * track.road_width * px_per_unit;
            if (info.centered) left -= width / 2.f;
            else if (obj.offset < 0.f) left -= width;
            const Bitmap& bmp = sprites.scenery(obj.kind);
            const float height = width * static_cast<float>(bmp.h) / static_cast<float>(bmp.w);
            const bool flip = info.mirrorable && obj.offset < 0.f;
            fb.blit_scaled(bmp, left, s.p1.y - height, width, height, flip,
                           fog_amount, track.theme.fog);
        }

        // Moving objects on this segment, far to near.
        const float z0 = static_cast<float>(s.index) * seg_len;
        auto first = std::lower_bound(objects.begin(), objects.end(), z0,
                                      [](const RoadSprite& o, float z) { return o.z < z; });
        auto last = std::lower_bound(first, objects.end(), z0 + seg_len,
                                     [](const RoadSprite& o, float z) { return o.z < z; });
        for (auto o = std::make_reverse_iterator(last); o != std::make_reverse_iterator(first); ++o) {
            const Bitmap& bmp = *o->bitmap;
            if (o->fixed) {
                fb.reset_clip();
                fb.blit_scaled(bmp, o->sx, o->sy, o->sw, o->sh);
                fb.set_clip(0, 0, fb.width(), clip);
                continue;
            }
            if (!projectable) continue;
            const float t = (o->z - z0) / seg_len;
            const float scale = s.p1.scale + (s.p2.scale - s.p1.scale) * t;
            const float x = s.p1.x + (s.p2.x - s.p1.x) * t;
            const float y = s.p1.y + (s.p2.y - s.p1.y) * t;
            const float px_per_unit = scale * half_w;
            const float width = o->world_width * px_per_unit;
            const float height = width * static_cast<float>(bmp.h) / static_cast<float>(bmp.w);
            const float cx = x + o->offset * track.road_width * px_per_unit;
            fb.blit_scaled(bmp, cx - width / 2.f, y - height, width, height, false,
                           fog_amount, track.theme.fog);
        }
    }
    fb.reset_clip();
}

} // namespace racer
