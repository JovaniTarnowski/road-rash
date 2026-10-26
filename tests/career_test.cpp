#include "assets.hpp"
#include "career.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <string>

using namespace rr;

static CareerTables synthetic() {
    CareerTables t;
    for (size_t i = 0; i < t.start_cash.size(); ++i) t.start_cash[i] = uint16_t(100 * i);
    for (size_t i = 0; i < t.level_bike.size(); ++i) t.level_bike[i] = uint8_t(i % 15);
    for (size_t i = 0; i < t.bike_price.size(); ++i) t.bike_price[i] = uint32_t(1000 * (i + 1));
    for (size_t i = 0; i < t.prize.size(); ++i) t.prize[i] = uint32_t(1000 - 60 * i);
    for (size_t p = 0; p < 10; ++p)
        for (size_t r = 0; r < 10; ++r)
            t.default_relations[p][r] = {int32_t(p), int32_t(r), int32_t(p * 10 + r)};
    return t;
}

int main(int argc, char** argv) {
    int failures = 0;
    auto check = [&](bool v, const char* what) {
        if (!v) {
            ++failures;
            std::cerr << "FAIL " << what << "\n";
        }
    };
    const CareerTables t = synthetic();

    // Character selection: Axle (1) is personality 8; Big Game starts at level 0 with the table cash.
    Career c;
    select_mode(c, t, GameMode::big_game);
    choose_character(c, t, 1);
    check(character_rider(1) == 8 && character_rider(8) == 1 && character_rider(0) == 0,
          "character to rider");
    check(c.state.cash == 100 && c.state.level == 0 && c.state.bike == t.level_bike[0 + 8 * 5] && c.unsaved,
          "big game start");
    check(c.relations[3][8] == t.default_relations[3][8], "relations restored from defaults");

    // Prize for 1st at level 1 (index 0) and level 3 (index 2).
    start_race(c, 2);
    auto r = finish_race(c, t, RaceOutcome::won, 0);
    check(r.screen == screen_id::won && r.prize == 1000 && c.state.cash == 1100 && c.state.qualified == 2,
          "first place prize level 1");
    check(after_results(c) == screen_id::street, "back to the street");
    c.state.level = 2;
    r = finish_race(c, t, RaceOutcome::lost, 5);
    check(r.screen == screen_id::lost && r.prize == (1000 - 300) * 3 && c.state.place == 5,
          "loser still paid by place");

    // Fine greater than cash: game over restarts the career for the same character.
    c.state.cash = 100;
    c.state.level = 1;
    r = finish_race(c, t, RaceOutcome::busted, 9);
    check(r.screen == screen_id::busted && r.penalty == 800 && r.game_over && r.penalty_message == 198,
          "fine exceeds cash");
    check(c.state.level == 0 && c.state.cash == 100 && c.state.character == 1, "game over restarts career");
    // Fine exactly equal to cash is paid.
    c.state.cash = 400;
    r = finish_race(c, t, RaceOutcome::busted, 9);
    check(!r.game_over && c.state.cash == 0 && r.penalty_message == 197, "exact fine");
    // Repairs: a tenth of the bike price.
    c.state.bike = 4;
    c.state.cash = 600;
    r = finish_race(c, t, RaceOutcome::wrecked, 9);
    check(r.screen == screen_id::wrecked && r.penalty == 500 && c.state.cash == 100 &&
              r.penalty_message == 194,
          "repair cost");

    // Bike shop: trade in at half price; exactly enough cash buys.
    c.state.bike = 1;
    c.state.cash = 2000; // trade-in 1000, bike 2 costs 3000
    check(buy_bike(c, t, 2) == PurchaseResult::bought && c.state.cash == 0 && c.state.bike == 2,
          "buy with exact cash");
    check(buy_bike(c, t, 2) == PurchaseResult::already_owned, "already owned");
    check(buy_bike(c, t, 14) == PurchaseResult::cannot_afford && c.state.bike == 2, "cannot afford");
    check(showroom_bike(0x83) == 4 && showroom_bike(0x8e) == 8, "showroom mapping");

    // Qualifying the fifth course levels up; at level 5 it is the finale.
    c.state.level = 1;
    c.state.qualified = 0x0f;
    start_race(c, 5);
    r = finish_race(c, t, RaceOutcome::won, 1);
    check(c.state.qualified == all_qualified && results_video(c, r.screen, 0).rfind("LEVEL", 0) == 0,
          "level video");
    check(after_results(c) == screen_id::level_up && c.state.level == 2 && c.state.qualified == 0,
          "level up");
    c.state.level = 4;
    c.state.qualified = 0x1e;
    start_race(c, 1);
    r = finish_race(c, t, RaceOutcome::won, 0);
    check(results_video(c, r.screen, 7) == "FINALWIN" && after_results(c) == screen_id::finale, "finale");

    // Thrash: wreck loses qualifications; no money.
    Career th;
    select_mode(th, t, GameMode::thrash);
    th.state.qualified = 3;
    th.state.cash = 50;
    r = finish_race(th, t, RaceOutcome::wrecked, 3);
    check(th.state.qualified == 0 && th.state.cash == 50 && r.penalty_message == 193, "thrash wreck");
    check(after_results(th) == screen_id::thrash_courses, "thrash returns to courses");
    check(results_video(th, screen_id::lost, 19) == "LOSE10", "lose video index");

    // Saves: round trip, checksum, and rejection of every truncation and of garbage.
    Career s = c;
    s.state.cash = 123456;
    s.state.name = {'R', 'a', 's', 'h'};
    s.relations[9][character_rider(s.state.character)] = {-1, 2, 0x7fffffff};
    const auto bytes = write_save(s);
    check(bytes.size() == 0xa0 && bytes[4] == 0xad && bytes[7] == 0xca, "save layout");
    const Career l = read_save(bytes, t);
    check(l.state.cash == 123456 && l.state.character == s.state.character &&
              l.relations[9] == s.relations[9] && l.mode == GameMode::big_game && !l.unsaved,
          "save round trip");
    check(write_save(l) == bytes, "save byte identity");
    for (size_t n = 0; n < bytes.size(); ++n) {
        try {
            read_save({bytes.begin(), bytes.begin() + long(n)}, t);
            ++failures;
        } catch (const std::runtime_error&) {
        }
    }
    std::mt19937 rng(7);
    for (int i = 0; i < 2000; ++i) {
        std::vector<uint8_t> g(0xa0);
        for (auto& x : g) x = uint8_t(rng());
        if (i & 1) {
            g[4] = 0xad;
            g[5] = 0xde;
            g[6] = 0xfe;
            g[7] = 0xca;
        }
        try {
            read_save(g, t);
            ++failures;
        } catch (const std::runtime_error&) {
        }
    }
    auto flipped = bytes;
    flipped[50] ^= 1;
    try {
        read_save(flipped, t);
        ++failures;
    } catch (const std::runtime_error&) {
    }
    // Checksum definition from 0x0042efc0 on a tiny input: bytes {1,2}: n=2 gives sum 1, weighted 2, alt -1; n=1 gives sum 3, weighted 4, alt 1.
    const uint8_t tiny[2] = {1, 2};
    check(save_checksum(tiny, 2) == ((uint32_t(1) << 16 | 3) ^ 4), "checksum definition");

    if (argc > 1 && std::string(argv[1]) == "--corpus") {
        Assets a(argc > 2 ? argv[2] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
        if (!a.exists("ROADRASH.EXE")) {
            std::cout << "skip: no disc\n";
            return 77;
        }
        const auto real = read_career_tables(a.read("ROADRASH.EXE"));
        check(real.start_cash[1] == 200 && real.start_cash[3] == 1000, "starting cash table");
        check(real.prize[0] == 1000 && real.prize[1] == 750 && real.prize[14] == 10, "prize table");
        check(real.bike_price[7] == 40000 && real.level_bike[0] == 1, "price and bike tables");
        check(real.default_relations[1][0][2] == 28672 && real.default_relations[0][1][0] == 8192,
              "relation defaults");
        Career b;
        select_mode(b, real, GameMode::big_game);
        choose_character(b, real, 3); // Cydney -> personality 2
        check(b.state.cash == 1000 && b.state.bike == real.level_bike[2 * 5], "Cydney starts with $1000");
        // Driven-career save round trip (plan 13 Task 6 Step 1 remainder):
        // drive a career through a win, a purchase and a save, then reload.
        // The bytes must round-trip identically and reload the driven state.
        start_race(b, 1);
        auto rep = finish_race(b, real, RaceOutcome::won, 0);
        check(rep.screen == screen_id::won && b.state.qualified == 1, "driven win qualifies City");
        const uint32_t after_prize = b.state.cash;
        // Buy the cheapest affordable upgrade if possible; otherwise keep the bike.
        int upgrade = -1;
        for (int bike = 0; bike < 15; ++bike) {
            Career probe = b;
            if (buy_bike(probe, real, bike) == PurchaseResult::bought) {
                upgrade = bike;
                break;
            }
        }
        if (upgrade >= 0) check(buy_bike(b, real, upgrade) == PurchaseResult::bought, "driven purchase");
        b.state.name = {'D', 'R', 'I', 'V', 'E', 'N'};
        const auto driven_bytes = write_save(b);
        check(driven_bytes.size() == 0xa0, "driven save size");
        const Career reloaded = read_save(driven_bytes, real);
        check(reloaded.state.cash == b.state.cash && reloaded.state.bike == b.state.bike &&
                  reloaded.state.qualified == b.state.qualified &&
                  reloaded.state.character == b.state.character &&
                  std::string(reloaded.state.name.data()) == "DRIVEN",
              "driven save reloads state");
        check(write_save(reloaded) == driven_bytes, "driven save byte identity");
        (void)after_prize;
    }
    if (argc > 1 && std::string(argv[1]) == "--save-corpus") {
        // Original-writer file from the RR_ORACLE_SAVE capture (plan 13 Task 6
        // Step 1): seeded state + personality triples, written by 0x0042f000.
        const std::string path =
            argc > 2 ? argv[2] : std::string(RR_SOURCE_DIR "/analysis/oracle/save-seed-20261026.rrs");
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            std::cout << "skip: no original save\n";
            return 77;
        }
        const std::vector<uint8_t> golden((std::istreambuf_iterator<char>(f)),
                                          std::istreambuf_iterator<char>());
        check(golden.size() == 0xa0, "original save size");
        if (golden.size() == 0xa0) {
            const Career l = read_save(golden, t);
            check(l.state.character == 3 && l.state.level == 2 && l.state.qualified == 5,
                  "original save header");
            check(l.state.bike == 7 && l.state.course == 4 && l.state.place == 1 && l.state.flags == 1,
                  "original save race fields");
            check(l.state.cash == 0x12345678 && std::string(l.state.name.data()) == "SAVETEST",
                  "original save cash and name");
            check(l.mode == GameMode::big_game && !l.unsaved, "original save forces big game");
            const size_t rider = size_t(character_rider(3));
            check(rider == 2, "seeded rider index");
            for (size_t p = 0; p < 10; ++p)
                for (size_t k = 0; k < 3; ++k) {
                    int32_t want;
                    if (p == 9 && k == 2)
                        want = 0x7fffffff;
                    else {
                        want = int32_t(0x01010101u * uint32_t(p) + 0x101u * uint32_t(k) + 7u);
                        if (((p + k) & 1)) want = -want;
                    }
                    check(l.relations[p][rider][k] == want, "original save relations");
                }
            check(write_save(l) == golden, "original save byte identity");
        }
        // Original-driven save from the Wine frontend acceptance run
        // (analysis/frontend/acceptance-20261026, plan 13 Task 6 Step 1):
        // the original played Big Game Cydney through a City race and saved
        // via the restroom UI. Skip 77 only when no save evidence exists.
        {
            const std::string dir = std::string(
                RR_SOURCE_DIR "/analysis/frontend/acceptance-20261026/34069bf3262241b3bdf4efa59c500751/");
            std::ifstream df(dir + "fe-career.RRS", std::ios::binary);
            if (!df) {
                std::cout << "skip: no driven original save\n";
            } else {
                const std::vector<uint8_t> driven((std::istreambuf_iterator<char>(df)),
                                                  std::istreambuf_iterator<char>());
                Assets da(argc > 2 ? argv[2] : RR_SOURCE_DIR "/../cdrom/ROADRASH");
                if (!da.exists("ROADRASH.EXE")) {
                    std::cout << "skip: no disc for driven save tables\n";
                } else {
                    const auto real = read_career_tables(da.read("ROADRASH.EXE"));
                    check(driven.size() == 0xa0, "driven original save size");
                    if (driven.size() == 0xa0) {
                        const Career d = read_save(driven, real);
                        check(d.state.character == 3 && d.state.level == 0 && d.state.qualified == 0,
                              "driven save header");
                        check(d.state.bike == 2 && d.state.course == 1 && d.state.place == 0,
                              "driven save race fields");
                        check(d.state.cash == 651, "driven save cash after City race");
                        check(d.mode == GameMode::big_game && !d.unsaved, "driven save forces big game");
                        check(write_save(d) == driven, "driven original save byte identity");
                        std::ifstream sf(dir + "fe-career-saved.state", std::ios::binary);
                        if (sf) {
                            const std::vector<uint8_t> state((std::istreambuf_iterator<char>(sf)),
                                                             std::istreambuf_iterator<char>());
                            check(state.size() == 0xa0 - 8, "driven state dump size");
                            if (state.size() == 0xa0 - 8)
                                check(std::vector<uint8_t>(driven.begin() + 8, driven.end()) == state,
                                      "driven save payload equals dumped game state");
                        }
                    }
                }
            }
        }
    }
    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
