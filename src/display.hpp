#pragma once
#include <SDL3/SDL.h>
#include "settings.hpp"

namespace rr {
struct DisplayConfig {
    bool fullscreen = false;
    int window_w = 1280, window_h = 960;          // ignored when fullscreen
    int logical_w = 640, logical_h = 480;
    SDL_RendererLogicalPresentation presentation = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    SDL_ScaleMode texture_scale = SDL_SCALEMODE_NEAREST;
    int vsync = 1;
};
// logical_h is 480 for menus/videos and 510 for the race (dashboard strip below the view).
DisplayConfig display_config(const Settings&, int logical_h, bool mobile);
// Idempotent: only calls SDL for fields that differ from the window's current state.
void apply_display(SDL_Window*, SDL_Renderer*, const DisplayConfig&);
// For every streaming texture the game creates.
void apply_texture_scale(SDL_Texture*, const DisplayConfig&);
}
