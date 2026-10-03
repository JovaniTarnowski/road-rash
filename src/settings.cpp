#include "settings.hpp"
#include <SDL3/SDL.h>
#include <fstream>
#include <sstream>

namespace rr {
namespace {
bool imperial_country(const char* c) {
    // Countries whose LOCALE_IMEASURE is "1" (U.S. system).
    return c && (std::string(c) == "US" || std::string(c) == "LR" || std::string(c) == "MM");
}
bool parse_int(const std::string& v, int lo, int hi, int& out) {
    try {
        size_t used = 0;
        const int x = std::stoi(v, &used);
        if (used != v.size() || x < lo || x > hi) return false;
        out = x;
        return true;
    } catch (...) { return false; }
}
}

Settings default_settings() {
    Settings s;
    int count = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&count);
    if (locales && count > 0 && locales[0]) {
        if (locales[0]->language) s.language = language_from_code(locales[0]->language);
        s.metric = locales[0]->country ? !imperial_country(locales[0]->country) : s.language != Language::english;
    }
    SDL_free(locales);
    return s;
}

Settings parse_settings(const std::string& text) {
    Settings s = default_settings();
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        int x = 0;
        auto flag = [&](bool& f) { if (parse_int(v, 0, 1, x)) f = x != 0; };
        if (k == "language") { if (parse_int(v, 0, 4, x)) s.language = Language(x); }
        else if (k == "metric") flag(s.metric);
        else if (k == "hires") flag(s.high_resolution);
        else if (k == "inset") flag(s.inset);
        else if (k == "hires_bikers") flag(s.high_res_bikers);
        else if (k == "video_mode") { if (parse_int(v, 0, 2, x)) s.video_mode = x; }
        else if (k == "alternate_artwork") flag(s.alternate_artwork);
        else if (k == "mini_dash") flag(s.mini_dash);
        else if (k == "opponent_list") flag(s.opponent_list);
        else if (k == "music_group") { if (parse_int(v, 0, 31, x)) s.music_group = x; }
        else if (k == "music_track") { if (parse_int(v, 0, 31, x)) s.music_track = x; }
        else if (k == "master_volume") { if (parse_int(v, 0, 100, x)) s.master_volume = x; }
        else if (k == "music_volume") { if (parse_int(v, 0, 100, x)) s.music_volume = x; }
        else if (k == "effects_volume") { if (parse_int(v, 0, 100, x)) s.effects_volume = x; }
        else if (k == "alias") { if (v.size() < 20) s.alias = v; }
    }
    return s;
}

std::string format_settings(const Settings& s) {
    std::ostringstream o;
    o << "language=" << int(s.language) << "\nmetric=" << s.metric << "\nhires=" << s.high_resolution << "\ninset=" << s.inset
      << "\nhires_bikers=" << s.high_res_bikers << "\nvideo_mode=" << s.video_mode << "\nalternate_artwork=" << s.alternate_artwork
      << "\nmini_dash=" << s.mini_dash << "\nopponent_list=" << s.opponent_list << "\nmusic_group=" << s.music_group
      << "\nmusic_track=" << s.music_track << "\nmaster_volume=" << s.master_volume << "\nmusic_volume=" << s.music_volume
      << "\neffects_volume=" << s.effects_volume << "\nalias=" << s.alias << "\n";
    return o.str();
}

Settings load_settings(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return default_settings();
    std::ostringstream text;
    text << in.rdbuf();
    return parse_settings(text.str());
}

bool save_settings(const std::string& path, const Settings& s) {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << format_settings(s);
        if (!out) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}
}
