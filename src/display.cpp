#include "display.hpp"

namespace rr {
DisplayConfig display_config(const Settings& s, int logical_h, bool mobile) {
    DisplayConfig c;
    c.fullscreen = mobile || s.display_mode == DisplayMode::fullscreen;
    c.logical_w = 640;
    c.logical_h = logical_h;
    c.window_w = 640 * s.window_scale;
    c.window_h = logical_h * s.window_scale;
    c.presentation = s.scale_mode == ScaleMode::integer ? SDL_LOGICAL_PRESENTATION_INTEGER_SCALE
                   : s.scale_mode == ScaleMode::stretch ? SDL_LOGICAL_PRESENTATION_STRETCH
                                                        : SDL_LOGICAL_PRESENTATION_LETTERBOX;
    c.texture_scale = s.filter == FilterMode::smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST;
    c.vsync = s.vsync ? 1 : 0;
    return c;
}

void apply_display(SDL_Window* window, SDL_Renderer* renderer, const DisplayConfig& c) {
    const bool is_full = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (is_full != c.fullscreen) SDL_SetWindowFullscreen(window, c.fullscreen);
    if (!c.fullscreen) {
        int w = 0, h = 0;
        SDL_GetWindowSize(window, &w, &h);
        if (w != c.window_w || h != c.window_h) SDL_SetWindowSize(window, c.window_w, c.window_h);
    }
    int lw = 0, lh = 0;
    SDL_RendererLogicalPresentation mode{};
    SDL_GetRenderLogicalPresentation(renderer, &lw, &lh, &mode);
    if (lw != c.logical_w || lh != c.logical_h || mode != c.presentation)
        SDL_SetRenderLogicalPresentation(renderer, c.logical_w, c.logical_h, c.presentation);
    SDL_SetRenderVSync(renderer, c.vsync);
}

void apply_texture_scale(SDL_Texture* texture, const DisplayConfig& c) {
    if (texture) SDL_SetTextureScaleMode(texture, c.texture_scale);
}
}
