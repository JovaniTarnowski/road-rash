#pragma once
#include <cstdint>

namespace rr {
struct FrontendData;
struct MenuItem;
// Builds the native screens and popups from original item geometry already in data.
void add_native_screens(FrontendData& data, bool mobile);
// Native popups, keyed like FrontendData::popups (fake addresses 0xF000_0000 + n, never valid EXE addresses).
namespace native_popup { constexpr uint32_t display_mode = 0xF0000001, window_size = 0xF0000002, scaling = 0xF0000003,
                                            filtering = 0xF0000004, on_off = 0xF0000005, touch = 0xF0000006; }
// Setter ids for native popup choices (negative: never an EXE setter).
namespace native_setter { constexpr int display_mode = -1, window_size = -2, scaling = -3, filtering = -4,
                                            vsync = -5, fps = -6, touch_opacity = -7, haptics = -8; }
// Direct-action handler ids (fake addresses, never valid EXE handlers).
namespace native_action {
constexpr uint32_t toggle_mute = 0xF0000010, toggle_swap = 0xF0000011;
constexpr uint32_t toggle_pad_swap = 0xF0000012, toggle_left_handed = 0xF0000013;
}
}
