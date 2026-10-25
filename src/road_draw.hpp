#pragma once
#include "road.hpp"
#include <SDL3/SDL.h>

namespace rr {
// Flat-shaded preview of original road geometry into a 640x480 area of the renderer.
// Colours and stripe period are placeholders; the original road raster is plan 02.
void draw_road(SDL_Renderer* renderer, const std::vector<RoadSlice>& slices);
} // namespace rr
