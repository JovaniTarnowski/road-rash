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
    // Plan 19 Task 8: when the render surface is larger than the drawable,
    // the race texture downsamples (supersampling): force linear filtering
    // whatever the Sharp/Smooth setting says.
    bool supersampling = false;
};
// logical_h is 480 for menus/videos and 510 for the race (dashboard strip below the view).
DisplayConfig display_config(const Settings&, int logical_h, bool mobile);
// Plan 19 Task 8: render scale actually used. 0 = match display (largest
// s <= 4 fitting the drawable); 1..4 fixed. The original Low/Inset
// resolution choice (high_resolution false or inset true) forces 1.
int effective_render_scale(const Settings&, int drawable_w, int drawable_h, bool mobile);
// Idempotent: only calls SDL for fields that differ from the window's current state.
void apply_display(SDL_Window*, SDL_Renderer*, const DisplayConfig&);
// For every streaming texture the game creates.
void apply_texture_scale(SDL_Texture*, const DisplayConfig&);
}
