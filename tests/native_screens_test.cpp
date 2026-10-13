#include "assets.hpp"
#include "frontend.hpp"
#include "native_screens.hpp"
#include "native_strings.hpp"
#include <iostream>

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    for (int s = rr::native_slot_base; s < rr::ns::count; ++s) check(!rr::native_string(s).empty(), "every native slot has text");
    const std::string root = argc > 1 ? argv[1] : RR_SOURCE_DIR "/../cdrom/ROADRASH";
    const rr::Assets assets(root);
    if (!assets.exists("ROADRASH.EXE")) { std::cout << "skip: no disc\n"; return failures ? 1 : 77; }
    rr::FrontendData data = rr::read_frontend_data(assets.read("ROADRASH.EXE"));
    rr::add_native_screens(data, false);
    const auto& display = data.screens[rr::screen_id::native_display];
    check(display.title == rr::ns::display_title, "display screen has the native title");
    check(display.items.size() == 7, "desktop display screen: mode, size, scaling, filtering, vsync, fps, back");
    const auto& gfx = data.screens[0x13].items.front();
    for (const auto& m : display.items)
        check(m.w == gfx.w && m.h == gfx.h && m.x == gfx.x, "native buttons copy the Graphics Options button geometry");
    for (size_t i = 0; i < display.items.size(); ++i) {   // vertical chain, wrapping, Escape on Back
        const auto& m = display.items[i];
        check(m.nav[3] == (i + 1) % display.items.size() && m.nav[2] == (i + display.items.size() - 1) % display.items.size(), "up/down chain");
    }
    check((display.items.back().flags & rr::item_flag::escape) && display.items.back().target == 0x13, "Back returns to Graphics Options");
    rr::FrontendData mobile = rr::read_frontend_data(assets.read("ROADRASH.EXE"));
    rr::add_native_screens(mobile, true);
    check(mobile.screens[rr::screen_id::native_display].items.size() == 6, "mobile hides window size and adds touch, haptics");
    bool linked = false;
    for (const auto& m : data.screens[0x13].items) linked |= m.target == rr::screen_id::native_display;
    const auto& pad = data.screens[rr::screen_id::native_gamepad];
    check(pad.items.size() == 7, "gamepad page: 4 mapping rows, swap, dead zone, back");
    check(pad.title == rr::ns::gamepad_title, "gamepad title");
    check(pad.items[5].caption == rr::ns::pad_deadzone, "dead zone slider row");
    check(pad.items[6].target == 0x12, "gamepad back returns to Configure Input");
    const auto& touch = data.screens[rr::screen_id::native_touch];
    check(touch.items.size() == 3 && touch.title == rr::ns::touch_title, "touch page rows");
    check(data.screens[0x12].items.size() == 23, "desktop 0x12 gains the gamepad link");
    check(mobile.screens[0x12].items.size() == 24, "mobile 0x12 gains gamepad and touch links");
    bool pad_linked = false, desk_touch = false;
    for (const auto& m : data.screens[0x12].items) {
        pad_linked |= m.target == rr::screen_id::native_gamepad;
        desk_touch |= m.target == rr::screen_id::native_touch;
    }
    check(pad_linked, "Configure Input links the gamepad page");
    check(!desk_touch, "desktop 0x12 does not link the touch page");
    bool mobile_touch = false;
    for (const auto& m : mobile.screens[0x12].items) mobile_touch |= m.target == rr::screen_id::native_touch;
    check(mobile_touch, "mobile 0x12 links the touch page");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
