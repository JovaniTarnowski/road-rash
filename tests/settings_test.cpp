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
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
