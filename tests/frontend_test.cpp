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
        else if (s == "tab") in.tab = true; else if (s == "slash") in.opponent_key = true;
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
    // Attract demo (plan 13 Task 5 Step 2): 60 s idle on the main menu starts an AI demo race.
    {   // Fresh menu idle: no input for 60 s emits a demo start_race event, course 1 first.
        fe.enter(screen_id::main_menu);
        now += 59999; fe.update({}, now);
        FrontendEvent ae;
        bool demo = false;
        while (fe.poll(ae)) if (ae.kind == FrontendEvent::Kind::start_race) demo = true;
        check(!demo, "no demo race before 60 s");
        now += 1; fe.update({}, now);
        int course = 0;
        while (fe.poll(ae)) if (ae.kind == FrontendEvent::Kind::start_race && ae.demo) course = ae.course;
        check(course == 1, "60 s idle starts demo race on course 1");
    }
    {   // Menu input does not disarm the timer (no KillTimer(9) on input paths); it fires anyway.
        fe.enter(screen_id::main_menu);
        now += 30000; fe.update({}, now);
        key("down");
        now += 30000; fe.update({}, now);
        int course = 0;
        FrontendEvent ae;
        while (fe.poll(ae)) if (ae.kind == FrontendEvent::Kind::start_race && ae.demo) course = ae.course;
        check(course == 2, "timer fires despite menu input; second window demos course 2");
    }
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
    // Jukebox (plan 13 Task 4 Step 1): 0x00445600 track list, 0x00445730 video popup, 0x004456d0 video play.
    // Note: open_popup appends to items_, so a 3-row popup on screen 0x11 (9 items) gives 12.
    auto focus_item = [&](int want) {
        for (int guard = 0; guard < 16 && fe.focus() != want; ++guard) key("down");
        return fe.focus() == want;
    };
    fe.enter(0x11);
    check(fe.screen() == 0x11, "jukebox screen");
    snap("jukebox");
    {   // Videos item (caption 431) opens the 3-item video popup (0x473fa0); focus lands on JESSIE.
        int videos = -1;
        for (size_t i = 0; i < fe.items().size(); ++i) if (fe.items()[i].caption == 431) videos = int(i);
        check(videos >= 0, "jukebox videos item present");
        if (focus_item(videos)) key("select");
        check(fe.items().size() == 12, "jukebox video popup opens");
        key("select");   // JESSIE row already focused
        check(event_video("VIDEO/JESSIE.AVI"), "jukebox plays JESSIE");
    }
    {   // VideoMode force: with videos off, the jukebox still plays and restores the setting.
        settings.video_mode = 0;
        fe.enter(0x11);
        int videos = -1;
        for (size_t i = 0; i < fe.items().size(); ++i) if (fe.items()[i].caption == 431) videos = int(i);
        if (focus_item(videos)) key("select");
        key("down");     // DUEL row
        key("select");
        check(event_video("VIDEO/DUEL.AVI") && settings.video_mode == 0, "jukebox forces VideoMode2 then restores");
        settings.video_mode = 2;
    }
    {   // Track list: first band item opens its track popup; choosing a track selects the soundtrack.
        fe.enter(0x11);
        if (focus_item(0)) key("select");
        check(fe.items().size() == 12, "jukebox track popup opens (Hammerbox, 3 rows)");
        key("select");   // first track row already focused
        FrontendEvent se;
        bool soundtrack = false;
        while (fe.poll(se)) if (se.kind == FrontendEvent::Kind::select_soundtrack && se.group == 0 && se.track == 0) soundtrack = true;
        check(soundtrack && settings.music_group == 0 && settings.music_track == 0, "jukebox track select emits soundtrack event");
    }
    // Outcome videos (plan 13 Task 4 Step 3): every outcome maps to its video family for fixed random values.
    {   // Pure rule first: exact names, no RNG state involved.
        Career c;
        check(results_video(c, screen_id::wrecked, 0) == "WRECK1", "wrecked random 0 -> WRECK1");
        check(results_video(c, screen_id::busted, 5) == "BUSTED6", "busted random 5 -> BUSTED6");
        check(results_video(c, screen_id::lost, 9) == "LOSE10", "lost random 9 -> LOSE10");
        check(results_video(c, screen_id::won, 1) == "WIN2", "won random 1 -> WIN2");
        c.state.qualified = all_qualified;
        check(results_video(c, screen_id::won, 3) == "LEVEL4", "all-qualified level<4 -> LEVEL4");
        c.state.level = 4;
        check(results_video(c, screen_id::won, 3) == "FINALWIN", "all-qualified level 4 -> FINALWIN");
        check(results_video(c, screen_id::main_menu, 0).empty(), "no video for menu screen");
    }
    {   // End to end through race_finished: each outcome queues its family (prefix match; number is RNG).
        career.mode = GameMode::big_game;
        career.state.qualified = 0;
        career.state.level = 1;
        fe.race_finished(RaceOutcome::lost, 5, {});
        check(fe.screen() == screen_id::lost && event_video("VIDEO/LOSE"), "lost queues LOSE video");
        fe.race_finished(RaceOutcome::busted, 9, {});
        check(fe.screen() == screen_id::busted && event_video("VIDEO/BUSTED"), "busted queues BUSTED video");
        career.state.qualified = all_qualified;
        career.state.level = 2;
        fe.race_finished(RaceOutcome::won, 0, {});
        check(fe.screen() == screen_id::won && event_video("VIDEO/LEVEL"), "all-qualified win queues LEVEL video");
        career.state.level = 4;
        fe.race_finished(RaceOutcome::won, 0, {});
        check(fe.screen() == screen_id::won && event_video("VIDEO/FINALWIN"), "level-4 win queues FINALWIN video");
    }
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
    // In-race toggles: F10 cycles MiniDash 0->1->2->0; menu `/` toggles the opponent list (0x423777 sete).
    settings.mini_dash = 0;
    check(fe.cycle_mini_dash() == 1 && fe.cycle_mini_dash() == 2 && fe.cycle_mini_dash() == 0, "minidash F10 cycle");
    settings.opponent_list = false;
    check(fe.toggle_opponent_list() && settings.opponent_list, "opponent list toggles on");
    check(!fe.toggle_opponent_list() && !settings.opponent_list, "opponent list toggles off");
    settings.opponent_list = false;
    key("slash");
    check(settings.opponent_list, "menu `/` key toggles the opponent list on");
    key("slash");
    check(!settings.opponent_list, "menu `/` key toggles the opponent list off");
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
