#pragma once
#include "framebuffer.hpp"
#include "track.hpp"

// Temporary flat drawing for the HUD and sky. To be replaced by a parallax
// background and a bitmap font HUD.
namespace racer::placeholder {

void draw_background(Framebuffer& fb, const RoadTheme& theme);
void draw_hud(Framebuffer& fb, float speed_fraction, int lap);

} // namespace racer::placeholder
