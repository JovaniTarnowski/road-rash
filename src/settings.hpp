#pragma once
#include "pe_strings.hpp"
#include <array>
#include <cstdint>
#include <string>

namespace rr {
// Values the original keeps under HKLM\SOFTWARE\Electronic Arts\RoadRash 95 (string 0x004633e8),
// stored natively as key=value text in the SDL preference directory.
struct Settings {
    Language language = Language::english;
    bool metric = false;             // original: LOCALE_IMEASURE != "1" (0x0042f490)
    bool high_resolution = false;    // "HiRes"  0x00475380: 640x480 race view
    bool inset = false;              // "Inset"  0x00475390: 320x240 inset in a 640x480 display
    bool high_res_bikers = false;    // "HiResBikers" 0x00475384
    int video_mode = 2;              // "VideoMode" 0x0047538c: 0 none, 1 small, 2 large
    bool alternate_artwork = false;  // "AlternateArtwork" 0x004753a4 (Sierra postcard variant, key A)
    bool mini_dash = false;          // "MiniDash" 0x00467a98
    bool opponent_list = false;      // "OpponentList" 0x004753a8
    int music_group = 0, music_track = 0;   // "Group"/"Track" (jukebox)
    int master_volume = 100, music_volume = 80, effects_volume = 100;   // "Mixer" block (native percent)
    std::string alias;               // player name (0x004b8a1c)
};
Settings default_settings();                         // language and units from the system locale
Settings parse_settings(const std::string& text);   // unknown keys and malformed values are ignored
std::string format_settings(const Settings&);
Settings load_settings(const std::string& path);     // defaults when missing or unreadable
bool save_settings(const std::string& path, const Settings&);
}
