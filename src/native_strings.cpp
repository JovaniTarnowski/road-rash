#include "native_strings.hpp"
#include <array>

namespace rr {
namespace {
// Captions upper case like the original buttons, descriptions in sentence case.
const std::array<std::string, ns::count - native_slot_base> kNativeStrings = {
    "DISPLAY", "Display Mode", "Window Size", "Scaling", "Filtering", "VSync", "FPS",
    "Touch Opacity", "Haptics", "Back", "Windowed", "Fullscreen", "640x480", "1280x960", "1920x1440", "2560x1920",
    "Fit", "Integer", "Stretch", "Sharp", "Smooth", "On", "Off",
    "Show the game in a window or fill the whole screen.",
    "Window size in windowed mode.",
    "How the 640x480 picture fills the window.",
    "Smooth scales the picture; sharp keeps texels crisp.",
    "Synchronize presentation with the display refresh.",
    "Show the frame counter.",
    "Transparency of the touch controls overlay.",
    "Rumble on crash and hit where the device supports it.",
    "SOUND", "Master", "Music", "Effects", "Engines", "Others", "Sirens", "Interface", "Mute", "Swap Speakers",
    "PAUSE", "Resume", "Sound", "Display", "Controls", "Quit Race", "Leave the race and return to the menu.",
    "0%", "25%", "50%", "75%", "100%",
    "GAMEPAD", "South/RT2: gas  East/LT2: brake  Pad: steer", "West: punch  North: kick  RShoulder: backhand",
    "Start: select  Back: back  Guide: pause", "Menus: D-pad + A/B",
    "Swap Punch+Kick", "Stick Dead Zone", "South and West trade punch and kick.",
    "Stick deflection needed before steering, in percent.",
    "TOUCH", "Left-Handed Layout", "Mirror the touch zones for left thumbs.",
    "Render Resolution", "Match display",
    "Race and menu pixel resolution. 640x480 is the original-exact mode; higher costs GPU time.",
    "Locked to 640x480 while Graphics Options Resolution is Low or Inset.",
    "Widescreen", "Off (4:3)", "16:10", "16:9", "Match display",
    "Hor+ wide race view. 4:3 is the original-exact mode; the centre is unchanged.",
};
const std::string kEmpty;
}
const std::string& native_string(int slot) {
    const int i = slot - native_slot_base;
    if (i < 0 || i >= int(kNativeStrings.size())) return kEmpty;
    return kNativeStrings[size_t(i)];
}
}
