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
// One popup choice row copied from the template choice. The nav chain is
// rewired for the new table: template rows carry the original popup's links.
MenuItem choice_item(const MenuItem& t, int y, int target, int caption, size_t index, size_t count) {
    MenuItem m = t;
    m.y = y;
    m.target = target;   // 1-based choice for the 0x00445440 confirm handler
    m.caption = caption;
    m.handler = 0x00445440;
    m.enabled = true;
    m.nav[2] = uint8_t((index + count - 1) % count);
    m.nav[3] = uint8_t((index + 1) % count);
    m.nav[4] = uint8_t((index + 1) % count);
    m.nav[5] = uint8_t((index + count - 1) % count);
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
        out.push_back(choice_item(t, t.y + int(i) * t.h, choices[i].first, choices[i].second, i + 1, choices.size() + 1));
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
    // Plan 19 Task 8: RENDER RESOLUTION row (both desktop and mobile).
    rows.push_back({ns::render_resolution, ns::d_render_resolution, native_popup::resolution});
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
    // The template has 4 rows (header + 3 choices); popups needing more
    // choices cycle the choice rows (plan 19 Task 8: resolution needs 5).
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
    // Plan 19 Task 8: match + 1x..4x. Choice index maps to render_scale - 1 (see the setter).
    data.popups[native_popup::resolution] = build_popup(rtpl, ns::render_resolution,
        {{1, ns::res_match}, {2, ns::scale_1x}, {3, ns::scale_2x}, {4, ns::scale_3x}, {5, ns::scale_4x}});
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
    // Gamepad page (plan 18 Task 7): fixed-mapping info rows (disabled), swap toggle,
    // dead-zone slider, Back to Configure Input. Touch page: opacity popup, handedness, Back.
    auto column = [&](MenuScreen& screen, int title) {
        screen.title = title;
        screen.title_format = data.screens[0x13].title_format;
        screen.items.clear();
    };
    auto chain = [&](MenuScreen& screen) {
        const size_t n = screen.items.size();
        for (size_t i = 0; i < n; ++i) {
            screen.items[i].nav[2] = uint8_t((i + n - 1) % n);
            screen.items[i].nav[3] = uint8_t((i + 1) % n);
            screen.items[i].nav[4] = uint8_t((i + 1) % n);
            screen.items[i].nav[5] = uint8_t((i + n - 1) % n);
        }
    };
    auto button = [&](int y, int caption, int desc, uint32_t handler, int target, uint32_t flags) {
        MenuItem m = tpl;
        m.y = y;
        m.caption = caption;
        m.desc = desc;
        m.handler = handler;
        m.target = target;
        m.flags = flags;
        m.enabled = true;
        return m;
    };
    {
        MenuScreen& pad = data.screens[screen_id::native_gamepad];
        column(pad, ns::gamepad_title);
        const int maps[4] = {ns::pad_map1, ns::pad_map2, ns::pad_map3, ns::pad_map4};
        for (int i = 0; i < 4; ++i) {
            MenuItem m = button(tpl.y + i * step, maps[i], 0, 0, 0,
                                item_flag::title_desc | item_flag::description);
            m.enabled = false;   // info rows: visible, never focused
            pad.items.push_back(m);
        }
        pad.items.push_back(button(tpl.y + 4 * step, ns::pad_swap, ns::d_swap,
                                   native_action::toggle_pad_swap, 0, item_flag::title_desc));
        MenuItem dz = button(tpl.y + 5 * step, ns::pad_deadzone, ns::d_deadzone, 0, 0,
                             item_flag::title_desc | item_flag::native_slider);
        dz.desc = ns::d_deadzone;
        pad.items.push_back(dz);
        pad.items.push_back(button(tpl.y + 6 * step, ns::back, 0, 0, 0x12,
                                   item_flag::title_desc | item_flag::escape));
        chain(pad);
    }
    {
        MenuScreen& touch = data.screens[screen_id::native_touch];
        column(touch, ns::touch_title);
        touch.items.push_back(button(tpl.y, ns::touch_opacity, ns::d_touch, 0,
                                     int(native_popup::touch), item_flag::title_desc | item_flag::description));
        touch.items.push_back(button(tpl.y + step, ns::touch_left, ns::d_lefthand,
                                     native_action::toggle_left_handed, 0, item_flag::title_desc));
        touch.items.push_back(button(tpl.y + 2 * step, ns::back, 0, 0, 0x12,
                                     item_flag::title_desc | item_flag::escape));
        chain(touch);
    }
    // Pause menu (plan 18 Task 8): RESUME, SOUND, DISPLAY, CONTROLS, QUIT RACE.
    // Resume carries the escape flag so Back/B/Escape resumes the race.
    {
        MenuScreen& pause = data.screens[screen_id::native_pause];
        column(pause, ns::pause_title);
        pause.items.push_back(button(tpl.y, ns::resume, 0,
                                     native_action::pause_resume, 0,
                                     item_flag::title_desc | item_flag::escape));
        pause.items.push_back(button(tpl.y + step, ns::sound, 0, 0,
                                     screen_id::native_sound, item_flag::title_desc));
        pause.items.push_back(button(tpl.y + 2 * step, ns::display, 0, 0,
                                     screen_id::native_display, item_flag::title_desc));
        pause.items.push_back(button(tpl.y + 3 * step, ns::controls, 0, 0,
                                     0x12, item_flag::title_desc));
        pause.items.push_back(button(tpl.y + 4 * step, ns::quit_race, ns::d_quit_race,
                                     native_action::pause_quit, 0,
                                     item_flag::title_desc | item_flag::description));
        chain(pause);
    }
    // Configure Input links: appended after the label rows so label indices keep
    // working; nav extended from the last label row through the new rows.
    {
        auto& items = data.screens[0x12].items;
        MenuItem gp = tpl;
        gp.y = tpl.y + 8 * step;
        gp.caption = ns::gamepad_title;
        gp.handler = 0;
        gp.target = screen_id::native_gamepad;
        gp.desc = 0;
        gp.flags = item_flag::title_desc | item_flag::description;
        gp.enabled = true;
        gp.nav[2] = 21;
        gp.nav[5] = 21;
        MenuItem th = gp;
        th.y = tpl.y + 9 * step;
        th.caption = ns::touch_title;
        th.target = screen_id::native_touch;
        th.nav[2] = 22;
        th.nav[5] = 22;
        if (mobile) {
            gp.nav[3] = 23;
            gp.nav[4] = 23;
            th.nav[3] = 0;
            th.nav[4] = 0;
            items.push_back(gp);
            items.push_back(th);
        } else {
            gp.nav[3] = 0;
            gp.nav[4] = 0;
            items.push_back(gp);
        }
        items[21].nav[3] = 22;
        items[21].nav[4] = 22;
        items[0].nav[2] = uint8_t(items.size() - 1);
        items[0].nav[5] = uint8_t(items.size() - 1);
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
