// Plan 28 Task 4: shared race helpers (moved from game_main.cpp).
#pragma once
#include <SDL3/SDL.h>
#include <cstdint>
#include <vector>
#include "frontend.hpp"
#include "game_types.hpp"
#include "race_caches.hpp"
#include "sdl_handles.hpp"
#include "text.hpp"

namespace rr {
GamepadPtr open_first_gamepad();
uint32_t autopilot(const Race&, const TrafficSystem*);
int attack_frame(const Rider&);
int riding_frame(const Bike&);
void draw_bike(SDL_Renderer*, const Game&, SpriteCache&, const std::vector<RoadSlice>&);
void draw_opponents(SDL_Renderer*, const Game&, SpriteCache&, const std::vector<RoadSlice>&);
void draw_hud(SDL_Renderer*, const Game&);
RgbImage capture_race_frame(SDL_Renderer*);
void pause_event(Frontend*, const SDL_Event&, GamepadPtr&, SDL_Renderer*);
} // namespace rr
