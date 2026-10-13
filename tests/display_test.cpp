#include "display.hpp"
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    rr::Settings s = rr::default_settings();
    s.display_mode = rr::DisplayMode::windowed; s.window_scale = 2;
    auto c = rr::display_config(s, 480, false);
    check(!c.fullscreen && c.window_w == 1280 && c.window_h == 960 && c.logical_h == 480, "2x window for menus");
    c = rr::display_config(s, 510, false);
    check(c.window_h == 1020 && c.logical_h == 510, "race window keeps the dashboard strip");
    s.scale_mode = rr::ScaleMode::integer;
    check(rr::display_config(s, 480, false).presentation == SDL_LOGICAL_PRESENTATION_INTEGER_SCALE, "integer scaling");
    s.scale_mode = rr::ScaleMode::stretch;
    check(rr::display_config(s, 480, false).presentation == SDL_LOGICAL_PRESENTATION_STRETCH, "stretch");
    s.filter = rr::FilterMode::smooth;
    check(rr::display_config(s, 480, false).texture_scale == SDL_SCALEMODE_LINEAR, "smooth filter");
    s.vsync = false;
    check(rr::display_config(s, 480, false).vsync == 0, "vsync off");
    s.display_mode = rr::DisplayMode::windowed;
    check(rr::display_config(s, 480, true).fullscreen, "mobile is always fullscreen");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
