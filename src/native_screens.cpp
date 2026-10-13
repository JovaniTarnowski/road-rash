#include "native_screens.hpp"
#include "career.hpp"
#include "frontend.hpp"
#include "native_strings.hpp"

namespace rr {
namespace {
// A native value row: caption + description slots, opens a native popup.
MenuItem value_item(const MenuItem& t, int y, int caption, int desc, uint32_t popup) {
    MenuItem m = t;
    m.y = y;
    m.target = int(popup);   // >= 0xF0000000: Frontend::activate opens the popup
    m.caption = caption;
    m.handler = 0;
    m.desc = desc;
    m.format = t.format;
    m.flags = item_flag::title_desc | item_flag::description;
    m.enabled = true;
    return m;
}
// One popup choice row copied from the template choice.
MenuItem choice_item(const MenuItem& t, int y, int target, int caption) {
    MenuItem m = t;
    m.y = y;
    m.target = target;   // 1-based choice for the 0x00445440 confirm handler
    m.caption = caption;
    m.handler = 0x00445440;
    m.enabled = true;
    return m;
}
std::vector<MenuItem> build_popup(const std::vector<MenuItem>& tpl, int header_caption,
                                  const std::vector<std::pair<int, int>>& choices) {
    std::vector<MenuItem> out;
    MenuItem header = tpl[0];
    header.caption = header_caption;
    out.push_back(header);
    for (size_t i = 0; i < choices.size(); ++i) {
        const MenuItem& t = tpl[1 + (i % (tpl.size() - 1))];
        out.push_back(choice_item(t, t.y + int(i) * t.h, choices[i].first, choices[i].second));
    }
    return out;
}
}

void add_native_screens(FrontendData& data, bool mobile) {
    const MenuItem& tpl = data.screens[0x13].items.front();
    const int step = tpl.h + 4;
    // Display screen rows (desktop): mode, size, scaling, filtering, vsync, fps, back.
    // Mobile: scaling, filtering, fps, touch, haptics, back (no mode/size; compositor owns vsync).
    struct Row { int caption, desc; uint32_t popup; };
    std::vector<Row> rows;
    if (!mobile) {
        rows.push_back({ns::display_mode, ns::d_display_mode, native_popup::display_mode});
        rows.push_back({ns::window_size, ns::d_window_size, native_popup::window_size});
    }
    rows.push_back({ns::scaling, ns::d_scaling, native_popup::scaling});
    rows.push_back({ns::filtering, ns::d_filtering, native_popup::filtering});
    if (!mobile) {
        rows.push_back({ns::vsync, ns::d_vsync, native_popup::on_off});
        rows.push_back({ns::fps, ns::d_fps, native_popup::on_off});
    } else {
        rows.push_back({ns::fps, ns::d_fps, native_popup::on_off});
        rows.push_back({ns::touch_opacity, ns::d_touch, native_popup::touch});
        rows.push_back({ns::haptics, ns::d_haptics, native_popup::on_off});
    }
    MenuScreen& screen = data.screens[screen_id::native_display];
    screen.title = ns::display_title;
    screen.title_format = data.screens[0x13].title_format;
    screen.items.clear();
    for (size_t i = 0; i < rows.size(); ++i)
        screen.items.push_back(value_item(tpl, tpl.y + int(i) * step, rows[i].caption, rows[i].desc, rows[i].popup));
    MenuItem back = tpl;
    back.y = tpl.y + int(rows.size()) * step;
    back.target = 0x13;
    back.caption = ns::back;
    back.handler = 0;
    back.desc = 0;
    back.flags = item_flag::title_desc | item_flag::escape;
    back.enabled = true;
    screen.items.push_back(back);
    const size_t n = screen.items.size();
    for (size_t i = 0; i < n; ++i) {   // vertical chain, wrapping
        screen.items[i].nav[2] = uint8_t((i + n - 1) % n);
        screen.items[i].nav[3] = uint8_t((i + 1) % n);
        screen.items[i].nav[4] = uint8_t((i + 1) % n);
        screen.items[i].nav[5] = uint8_t((i + n - 1) % n);
    }
    // Native popups from the resolution popup template (header + choice rows).
    const auto& rtpl = data.popups.at(0x00474240);
    data.popups[native_popup::display_mode] = build_popup(rtpl, ns::display_mode, {{1, ns::windowed}, {2, ns::fullscreen}});
    data.popups[native_popup::window_size] = build_popup(rtpl, ns::window_size,
        {{1, ns::scale_1x}, {2, ns::scale_2x}, {3, ns::scale_3x}, {4, ns::scale_4x}});
    data.popups[native_popup::scaling] = build_popup(rtpl, ns::scaling,
        {{1, ns::fit}, {2, ns::integer}, {3, ns::stretch}});
    data.popups[native_popup::filtering] = build_popup(rtpl, ns::filtering, {{1, ns::sharp}, {2, ns::smooth}});
    data.popups[native_popup::on_off] = build_popup(rtpl, ns::fps, {{1, ns::off}, {2, ns::on}});
    data.popups[native_popup::touch] = build_popup(rtpl, ns::touch_opacity,
        {{1, ns::touch_0}, {2, ns::touch_25}, {3, ns::touch_50}, {4, ns::touch_75}, {5, ns::touch_100}});
    // Sound screen (plan 18 Task 6): dialog-300 order — master + six channels as sliders,
    // mute + swap as direct toggles, Back to the restroom. Same button template/column.
    {
        const int caps[7] = {ns::master, ns::music, ns::effects, ns::engines, ns::others, ns::sirens, ns::interface};
        MenuScreen& sound = data.screens[screen_id::native_sound];
        sound.title = ns::sound_title;
        sound.title_format = data.screens[0x13].title_format;
        sound.items.clear();
        for (int i = 0; i < 7; ++i) {
            MenuItem m = tpl;
            m.y = tpl.y + i * step;
            m.caption = caps[i];
            m.handler = 0;
            m.target = 0;
            m.desc = 0;
            m.flags = item_flag::title_desc | item_flag::native_slider;
            m.enabled = true;
            sound.items.push_back(m);
        }
        const std::pair<int, uint32_t> toggles[2] = {{ns::mute, native_action::toggle_mute},
                                                     {ns::swap, native_action::toggle_swap}};
        for (int i = 0; i < 2; ++i) {
            MenuItem m = tpl;
            m.y = tpl.y + (7 + i) * step;
            m.caption = toggles[i].first;
            m.handler = toggles[i].second;
            m.target = 0;
            m.desc = 0;
            m.flags = item_flag::title_desc;
            m.enabled = true;
            sound.items.push_back(m);
        }
        MenuItem back = tpl;
        back.y = tpl.y + 9 * step;
        back.target = 0x10;
        back.caption = ns::back;
        back.handler = 0;
        back.desc = 0;
        back.flags = item_flag::title_desc | item_flag::escape;
        back.enabled = true;
        sound.items.push_back(back);
        const size_t n = sound.items.size();
        for (size_t i = 0; i < n; ++i) {
            sound.items[i].nav[2] = uint8_t((i + n - 1) % n);
            sound.items[i].nav[3] = uint8_t((i + 1) % n);
            sound.items[i].nav[4] = uint8_t((i + 1) % n);
            sound.items[i].nav[5] = uint8_t((i + n - 1) % n);
        }
    }
    // nav chain already routes 1->2->3).
    MenuItem& link = data.screens[0x13].items[2];
    link.target = screen_id::native_display;
    link.caption = ns::display;
    link.handler = 0;
    link.desc = ns::d_display_mode;
    link.flags = item_flag::title_desc | item_flag::description;
    link.enabled = true;
}
}
