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
    check(linked, "Graphics Options has an item that enters the Display screen");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
