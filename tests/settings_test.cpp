#include "settings.hpp"
#include <cstdio>
#include <filesystem>
#include <iostream>

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    rr::Settings s;
    s.language = rr::Language::italian; s.metric = true; s.high_resolution = true; s.video_mode = 1;
    s.master_volume = 37; s.alias = "Rasher"; s.alternate_artwork = true;
    const rr::Settings r = rr::parse_settings(rr::format_settings(s));
    check(r.language == s.language && r.metric && r.high_resolution && r.video_mode == 1 && r.master_volume == 37 &&
          r.alias == "Rasher" && r.alternate_artwork, "round trip");
    const rr::Settings g = rr::parse_settings("video_mode=9\nmaster_volume=-1\nhires=yes\nnonsense\n=\nalias=" + std::string(40, 'x') + "\n");
    const rr::Settings d = rr::default_settings();
    check(g.video_mode == d.video_mode && g.master_volume == d.master_volume && g.high_resolution == d.high_resolution && g.alias.empty(), "invalid values ignored");
    const auto path = (std::filesystem::temp_directory_path() / "rr-settings-test.txt").string();
    check(rr::save_settings(path, s) && rr::load_settings(path).alias == "Rasher", "file round trip");
    std::remove(path.c_str());
    check(rr::load_settings(path).video_mode == d.video_mode, "missing file gives defaults");
    {
        rr::Settings n;
        n.display_mode = rr::DisplayMode::fullscreen; n.window_scale = 3; n.scale_mode = rr::ScaleMode::integer;
        n.filter = rr::FilterMode::smooth; n.vsync = false; n.show_fps = true; n.touch_opacity = 20; n.haptics = false;
        const rr::Settings m = rr::parse_settings(rr::format_settings(n));
        check(m.display_mode == rr::DisplayMode::fullscreen && m.window_scale == 3 && m.scale_mode == rr::ScaleMode::integer &&
              m.filter == rr::FilterMode::smooth && !m.vsync && m.show_fps && m.touch_opacity == 20 && !m.haptics,
              "native fields round trip");
        const rr::Settings bad = rr::parse_settings("window_scale=99\nscale_mode=7\nfilter=-1\ndisplay_mode=2\ntouch_opacity=101\nvsync=");
        check(bad.window_scale == d.window_scale && bad.scale_mode == d.scale_mode && bad.filter == d.filter &&
              bad.display_mode == d.display_mode && bad.touch_opacity == d.touch_opacity && bad.vsync == d.vsync,
              "native fields reject out-of-range values");
        const rr::Settings old = rr::parse_settings("hires=1\nmaster_volume=40\n");   // a file written before this task
        check(old.high_resolution && old.master_volume == 40 && old.window_scale == d.window_scale, "old files still load");
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
