#include "assets.hpp"
#include "frontend.hpp"
#include "pe_strings.hpp"
#include "rri.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace rr;

static void write_bmp(const std::string& path, const RgbImage& img) {
    std::ofstream f(path, std::ios::binary);
    const uint32_t row = uint32_t(img.width) * 3, pad = (4 - row % 4) % 4, size = 54 + (row + pad) * uint32_t(img.height);
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) f.put(char(v >> (8 * i))); };
    auto u16 = [&](uint16_t v) { f.put(char(v)); f.put(char(v >> 8)); };
    f.put('B'); f.put('M'); u32(size); u32(0); u32(54); u32(40); u32(uint32_t(img.width)); u32(uint32_t(img.height));
    u16(1); u16(24); u32(0); u32(size - 54); u32(2835); u32(2835); u32(0); u32(0);
    for (int y = img.height - 1; y >= 0; --y) {
        for (int x = 0; x < img.width; ++x) { const uint32_t p = img.at(x, y); f.put(char(p)); f.put(char(p >> 8)); f.put(char(p >> 16)); }
        for (uint32_t i = 0; i < pad; ++i) f.put(0);
    }
}

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const std::string& what) { if (!v) { ++failures; std::cerr << "FAIL " << what << "\n"; } };
    // Synthetic checks that need no disc: 15-bit quantisation and the darkened surface.
    check(quantise555(0xffffff) == 0xffffff && quantise555(0x070707) == 0, "quantise555");
    check(darken555(0xffffff) == 0x7b7b7b && darken555(0x080808) == 0, "darken555");

    const std::string root = argc > 1 && std::string(argv[1]) != "--dump" ? argv[1] : RR_SOURCE_DIR "/../cdrom/ROADRASH";
    std::string dump;
    for (int i = 1; i + 1 < argc; ++i) if (std::string(argv[i]) == "--dump") dump = argv[i + 1];
    Assets assets(root);
    if (!assets.exists("ROADRASH.EXE")) { std::cout << "skip: no disc\n"; return failures ? 1 : 77; }
    const auto exe = assets.read("ROADRASH.EXE");
    const TextRenderer text(assets);
    const auto tables = read_career_tables(exe);
    const auto data = read_frontend_data(exe);
    check(data.screens[9].items.size() == 4 && data.screens[0x10].items.size() == 13 && data.screens[0x22].items.size() == 15, "screen tables");
    check(data.restroom_patterns[1] == "YNNYYYYN", "restroom pattern");
    const auto strings = frontend_strings(read_string_resources(assets.read("TEXT/ENU.DLL")));
    const std::string saves = (std::filesystem::temp_directory_path() / "rr-frontend-test-saves").string();
    std::filesystem::remove_all(saves);
    Career career;
    Settings settings;
    settings.video_mode = 2;
    const auto res = read_string_resources(assets.read("TEXT/ENU.DLL"));
    const auto credits = credit_lines(res);
    check(credits[0].size() > 300 && credits[1].size() > 10, "credit tables");
    Frontend fe(assets, text, strings, credits, data, tables, career, settings, saves);
    uint64_t now = 0;
    int shot = 0;
    RgbImage img;
    auto snap = [&](const char* name) {
        fe.draw(img);
        check(img.width == 640 && img.height == 480, std::string("frame size ") + name);
        size_t lit = 0;
        for (auto p : img.pixels) lit += p != 0;
        check(lit > 10000, std::string("frame has content ") + name);
        if (!dump.empty()) write_bmp(dump + "/" + std::to_string(shot++) + "-" + name + ".bmp", img);
    };
    auto press = [&](MenuInput in) { now += 50; fe.update(in, now); };
    auto key = [&](const char* k) {
        MenuInput in;
        std::string s = k;
        if (s == "down") in.down = true; else if (s == "up") in.up = true; else if (s == "left") in.left = true;
        else if (s == "right") in.right = true; else if (s == "select") in.select = true; else if (s == "back") in.back = true;
        else if (s == "tab") in.tab = true;
        press(in);
    };
    auto event = [&](FrontendEvent::Kind kind) {
        FrontendEvent e;
        bool found = false;
        while (fe.poll(e)) if (e.kind == kind) found = true;
        return found;
    };
    auto event_video = [&](const std::string& prefix) {
        FrontendEvent e;
        bool found = false;
        while (fe.poll(e)) if (e.kind == FrontendEvent::Kind::play_video && e.video.rfind(prefix, 0) == 0) found = true;
        return found;
    };

    fe.start();
    check(event_video("VIDEO/INTRO.AVI") && fe.screen() == 7, "intro then credits");
    snap("credits");
    now += 3000; fe.update({}, now);
    check(fe.screen() == 7, "credits still scrolling");
    snap("credits-scroll");
    key("back");
    check(fe.screen() == screen_id::main_menu, "Escape skips the credits to the main menu");
    snap("main");
    // Thrash: City at level 1, a win qualifies it.
    key("select");
    check(fe.screen() == screen_id::thrash_courses && career.mode == GameMode::thrash, "thrash course select");
    snap("thrash-courses");
    key("select");
    check(fe.screen() == screen_id::race_loading && event_video("VIDEO/START"), "start video and loading screen");
    snap("loading");
    {   // 0x0041e240: the 397-wide course postcard is composited right of the "Postcard" strip.
        size_t black = 0;
        for (int y = 0; y < 480; ++y) for (int x = 400; x < 640; ++x) black += img.at(x, y) == 0;
        check(black < 240 * 480 / 10, "loading postcard fills the right side");
    }
    key("select");
    FrontendEvent e;
    bool started = false;
    while (fe.poll(e)) if (e.kind == FrontendEvent::Kind::start_race && e.course == 1) started = true;
    check(started && career.state.course == 1, "race starts on City");
    std::vector<Standing> st = {{"Player 1", 4123, true}, {"Mike", 4200, false}, {"Cydney", 4300, false}};
    fe.race_finished(RaceOutcome::won, 0, st);
    check(fe.screen() == screen_id::won && event_video("VIDEO/WIN") && career.state.qualified == 1, "win screen");
    snap("won");
    key("select");
    check(fe.screen() == screen_id::standings && fe.caption_text(fe.items()[0]).find("6:52.3") != std::string::npos, "standings rows");
    snap("standings");
    key("select");
    check(fe.screen() == screen_id::thrash_courses && fe.focus() == 1, "back to courses; focus skips qualified City");
    snap("thrash-qualified");
    // Thrash wreck clears qualifications.
    key("select"); key("select");
    while (fe.poll(e)) {}
    fe.race_finished(RaceOutcome::wrecked, 7, {});
    check(fe.screen() == screen_id::wrecked, "wreck screen");
    snap("wrecked");
    key("select");
    check(fe.screen() == screen_id::penalty && career.state.qualified == 0, "thrash penalty clears qualifications");
    snap("penalty");
    key("select");
    check(fe.screen() == screen_id::thrash_courses, "back to Thrash courses");

    // Big Game from the main menu: restroom of the Thrash course screen, Game Mode popup -> Big Game.
    fe.enter(screen_id::main_menu);
    key("right");
    check(fe.focus() == 1, "main menu right moves to Big Game");
    key("select");
    check(fe.screen() == screen_id::characters && career.mode == GameMode::big_game, "character select");
    snap("characters");
    key("down"); key("down");   // Cydney
    snap("character-focus");
    key("select");
    check(fe.screen() == screen_id::street && career.state.character == 3 && career.state.cash == 1000, "Cydney starts on the street with $1000");
    snap("street");
    // Olley's -> Rat bikes -> first bike -> Buy.
    key("right");
    key("select");
    check(fe.screen() == 0x14, "Olley's");
    snap("olleys");
    key("select");
    check(fe.screen() == 0x15, "Rat bikes");
    snap("rat-bikes");
    key("select");
    check(fe.screen() == 0x18 && event_video("VIDEO/RAT/BANZAI.AVI"), "showroom with bike video");
    snap("showroom");
    const uint32_t cash = career.state.cash;
    key("select");
    snap("bought");
    check(career.state.bike == 0 || career.state.cash == cash, "purchase applied or refused by the rules");
    key("select");
    // Back to the street and into the Panzer Klub.
    fe.enter(screen_id::street);
    key("left");
    key("select");
    check(fe.screen() == 0x19, "Panzer Klub");
    snap("panzer");
    key("select");
    check(fe.screen() == 0x1a, "schmooze");
    snap("schmooze");
    key("back");
    check(fe.screen() == 0x19, "schmooze escape");
    // Restroom: save, change, load back.
    key("right");
    key("select");
    check(fe.screen() == screen_id::restroom, "restroom");
    snap("restroom");
    for (const auto& m : fe.items()) (void)m;
    // Save Game As... is the third item.
    fe.enter(screen_id::restroom);
    while (fe.focus() != 0) key("up");
    key("down");   // Save Game is disabled before a file exists: down skips to Save Game As...
    check(fe.focus() == 2, "disabled Save Game skipped");
    key("select");
    check(!fe.current_save().empty() && std::filesystem::exists(fe.current_save()), "save written");
    const uint32_t saved_cash = career.state.cash;
    career.state.cash = 1;
    key("up"); key("up");
    key("select");   // Load Game: list of saves
    snap("load-list");
    key("select");
    check(career.state.cash == saved_cash && career.mode == GameMode::big_game, "save loaded back");
    // Bulletin board -> City -> race -> busted with no money: game over.
    fe.enter(0x1b);
    snap("bulletin");
    key("select");
    key("select");
    while (fe.poll(e)) {}
    career.state.cash = 10;
    fe.race_finished(RaceOutcome::busted, 9, {});
    key("select");
    check(fe.screen() == screen_id::penalty && fe.caption_text(fe.items()[0]).find("Game over") != std::string::npos, "game over message");
    snap("game-over");
    // Go Home asks to save first.
    fe.enter(screen_id::street);
    career.unsaved = true;
    for (int i = 0; i < 3 && fe.focus() != 2; ++i) key("right");
    key("select");
    check(fe.dialog_open(), "unsaved prompt");
    snap("unsaved-dialog");
    key("right");
    key("select");   // No
    check(event(FrontendEvent::Kind::quit), "go home quits");
    // Key rebinding storage + conflict rule (0x0040fbf0: same-code rows unbound).
    check(default_keyboard_controls()[0] == 0x9400 && default_joystick_controls()[0] == 0x0025, "control defaults match EXE");
    { std::array<uint16_t, 11> kb, joy; read_control_tables(exe, kb, joy); check(kb == default_keyboard_controls() && joy == default_joystick_controls(), "control tables read from EXE"); }
    { std::array<uint16_t, 11> t{}; bool threw = false; try { apply_rebind(t, 11, 1); } catch (const std::runtime_error&) { threw = true; } check(threw, "rebind range checked"); }
    fe.reset_keys(false);
    check(settings.controls == default_keyboard_controls() && settings.custom_controls, "keyboard defaults restore");
    fe.rebind_key(0, 0x001e);
    check(settings.controls[0] == 0x001e, "rebind stores code");
    fe.rebind_key(1, 0x001e);   // conflict: action 0 unbound, not swapped
    check(settings.controls[1] == 0x001e && settings.controls[0] == 0, "rebind conflict unbinds");
    fe.reset_keys(true);
    check(settings.controls == default_joystick_controls(), "joystick defaults restore");
    // In-race toggles: F10 cycles MiniDash 0->1->2->0; opponent key is ON-only.
    settings.mini_dash = 0;
    check(fe.cycle_mini_dash() == 1 && fe.cycle_mini_dash() == 2 && fe.cycle_mini_dash() == 0, "minidash F10 cycle");
    settings.opponent_list = false;
    check(fe.enable_opponent_list() && settings.opponent_list && fe.enable_opponent_list(), "opponent list ON-only");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
