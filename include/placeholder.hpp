#pragma once
#include "framebuffer.hpp"
#include "track.hpp"

// Temporary flat-shaded drawing for scenery, car, HUD and sky. To be replaced
// by bitmap sprites, a parallax background and a bitmap font HUD.
namespace racer::placeholder {

void draw_background(Framebuffer& fb, const RoadTheme& theme);
void draw_scenery(Framebuffer& fb, Scenery kind, float left, float bottom, float width, float fog);
void draw_car(Framebuffer& fb, float center_x, float bottom, int steer);
void draw_hud(Framebuffer& fb, float speed_fraction, int lap);

} // namespace racer::placeholder
