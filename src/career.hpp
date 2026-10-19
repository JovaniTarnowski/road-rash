#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rr {
// Game mode 0x004753b0.
enum class GameMode : uint8_t { attract = 0, thrash = 1, big_game = 2, mano = 3 };

// Race outcome 0x004c5d7c, set through 0x00416010. 0x7f (finished) becomes won when the
// 0-based place is below 3 (single player) and lost otherwise.
enum class RaceOutcome : uint8_t { racing = 1, won = 2, lost = 3, busted = 4, wrecked = 5 };

// The 32-byte game-state block at 0x004b8a10, copied verbatim into save files.
struct GameState {
    uint8_t player = 0;      // +0 network player slot (0 outside mano-a-mano, 0x004160c0)
    uint8_t character = 0;   // +1 1..8 (Axle..Teflon Mike), 0 = none chosen
    uint8_t level = 0;       // +2 0..4
    uint8_t qualified = 0;   // +3 bit n-1 for course n (1 City .. 5 Pacific Highway); 0x1f = level complete
    uint8_t bike = 1;        // +4 bike id 0..14 (BIKESPEC order)
    uint8_t course = 1;      // +5 1..5, last race started (0x004160c0)
    uint8_t place = 0;       // +6 0-based place of the last finished race (0x004c5d78)
    uint8_t flags = 1;       // +7 copied from 0x004754b0; meaning not yet traced
    uint32_t cash = 0;       // +8
    std::array<char, 20> name{};   // +0xc alias (0x0041c060), NUL-terminated
};
constexpr uint8_t all_qualified = 0x1f;

// Tables read from the user's executable at runtime.
struct CareerTables {
    std::array<uint16_t, 9> start_cash{};     // 0x00469000, by character (slot 0 unused)
    std::array<uint8_t, 50> level_bike{};     // 0x004641c8[level + rider*5]
    std::array<uint32_t, 15> bike_price{};    // 0x00468fc0, by bike id
    std::array<uint32_t, 15> prize{};         // 0x0046ab48 + place*0x4c (standings +0)
    // Personality p (0x00464558 + p*0x204) starts with 10 x 3 relation dwords, one triple per rider.
    std::array<std::array<std::array<int32_t, 3>, 10>, 10> default_relations{};
    std::array<int32_t, 5> spare_levels{};    // personality 0 +0x1bc: level speeds copied to rows 9..14
};
CareerTables read_career_tables(const std::vector<uint8_t>& exe);

// 0x00415fc0: character 1..8 -> rider personality index; 0 for none.
int character_rider(int character);
// 0x0041ff00: showroom slot 0x80..0x8e -> bike id.
int showroom_bike(int slot);

struct Career {
    GameMode mode = GameMode::thrash;
    GameState state{};
    // Runtime personality copies at 0x004c6b40 (0x204 bytes each) start with 10 relation
    // triples: relations[personality][other][k]. Saves store relations[p][player's personality].
    std::array<std::array<std::array<int32_t, 3>, 10>, 10> relations{};
    // Rider rows 9..14 (0x004c5f20 + row*0x204, read by 0x00407990): personality 0 with the five
    // +0x1bc level speeds jittered by rand()%13-6 in 0x004078f0, row-major, 30 draws.
    std::array<std::array<int32_t, 5>, 6> spare_levels{};
    // The CRT rand() state (0x00455eb0, per-thread +0x14) shared by the menus and 0x004078f0.
    // 0x00448200 seeds it with srand(time(NULL)) at launch, so the jitter differs per second.
    uint32_t rng = 1;
    bool unsaved = false;   // 0x004753ac
};
// MSVC rand() on a Career's CRT state.
uint32_t career_random(Career&);

// Next front-end screen ids (0x00421df0 screen table indices).
namespace screen_id {
constexpr int race_loading = 8, main_menu = 9, thrash_courses = 10, mano = 11, mano_courses = 12, big_game_courses = 27,
              characters = 14, street = 15, restroom = 16, wrecked = 0x1c, busted = 0x1d, won = 0x1e,
              lost = 0x1f, level_up = 0x20, penalty = 0x21, standings = 0x22, finale = 0x23;
constexpr int native_display = 0x24, native_sound = 0x25, native_pause = 0x26;
constexpr int native_gamepad = 0x27, native_touch = 0x28, native_enhancements = 0x29, screen_count = 0x2a;
}

// 0x00445090: switching mode resets level, qualifications and character.
void select_mode(Career&, const CareerTables&, GameMode);
// 0x004078f0: restore the relation triples from the personality defaults and re-jitter the
// spare rows' level speeds (30 draws from Career::rng).
void reset_relations(Career&, const CareerTables&);
// 0x0041f5a0 + 0x0041f630: choose a character; Big Game restarts the career with that rider's cash.
void choose_character(Career&, const CareerTables&, int character);
// 0x0041f630: Big Game restart / default bike for the level.
void reset_for_character(Career&, const CareerTables&);
// 0x004453f0: Player Level option (1..5).
void set_level(Career&, const CareerTables&, int level);
// 0x004160c0: race start.
void start_race(Career&, int course);

enum class PurchaseResult { bought, already_owned, cannot_afford };
// 0x004215e0: trade in at half price.
PurchaseResult buy_bike(Career&, const CareerTables&, int bike);

struct RaceReport {
    int screen = 0;              // first results screen
    int video_kind = 0;          // 0 none; see results_video
    int penalty_message = 0;     // string id shown on the 0x21 screen (0 none)
    uint32_t penalty = 0;        // fine or repair cost
    bool game_over = false;
    uint32_t prize = 0;
};
// Results: 0x00449150 (qualification), 0x00416120 (screen), 0x00421290 (prize) and
// 0x004214e0 (fines and repairs). place is 0-based.
RaceReport finish_race(Career&, const CareerTables&, RaceOutcome, int place);
// 0x0041f6d0: after the results screens; returns the next screen.
int after_results(Career&);

// 0x00449150 result videos: "Wreck"/"Busted"/"Win" (1..6), "Lose" (1..10), "Level", "FinalWin".
std::string results_video(const Career&, int screen, uint32_t random);

// Save files (0x0042f000 / 0x0042f110): 0xa0 bytes =
// checksum(0x0042efc0 over the next 0x9c bytes), 0xcafedead, GameState, 10 x 3 relation dwords.
uint32_t save_checksum(const uint8_t* bytes, size_t count);
std::vector<uint8_t> write_save(const Career&);
// rng: the CRT state when the file is read; 0x0042f110 runs 0x004078f0 on a valid file.
Career read_save(const std::vector<uint8_t>&, const CareerTables&, uint32_t rng = 1);   // throws std::runtime_error
}
