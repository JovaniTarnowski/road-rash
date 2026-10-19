#include "career.hpp"
#include "byte_reader.hpp"
#include "exe_tables.hpp"
#include <cstring>
#include <stdexcept>

namespace rr {
namespace {
void put32(uint8_t* p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = uint8_t(v >> (8 * i)); }
constexpr uint32_t save_magic = 0xcafedead;
constexpr size_t save_size = 0xa0;
}

CareerTables read_career_tables(const std::vector<uint8_t>& exe) {
    CareerTables t;
    const auto cash = read_exe_data(exe, 0x00469000, 18);
    for (size_t i = 0; i < t.start_cash.size(); ++i) t.start_cash[i] = uint16_t(cash[2 * i] | cash[2 * i + 1] << 8);
    const auto bikes = read_exe_data(exe, 0x004641c8, t.level_bike.size());
    std::copy(bikes.begin(), bikes.end(), t.level_bike.begin());
    const auto prices = read_exe_data(exe, 0x00468fc0, 4 * t.bike_price.size());
    const ByteReader price_reader(prices);
    for (size_t i = 0; i < t.bike_price.size(); ++i) t.bike_price[i] = price_reader.le<uint32_t>(4 * i);
    const auto standings = read_exe_data(exe, 0x0046ab48, 0x4c * t.prize.size());
    const ByteReader standing_reader(standings);
    for (size_t i = 0; i < t.prize.size(); ++i) t.prize[i] = standing_reader.le<uint32_t>(0x4c * i);
    for (size_t p = 0; p < 10; ++p) {
        const auto rec = read_exe_data(exe, 0x00464558 + uint32_t(p) * 0x204, 120);
        const ByteReader rel_reader(rec);
        for (size_t r = 0; r < 10; ++r)
            for (size_t k = 0; k < 3; ++k) t.default_relations[p][r][k] = int32_t(rel_reader.le<uint32_t>(12 * r + 4 * k));
    }
    const auto spare = read_exe_data(exe, 0x00464558 + 0x1bc, 4 * t.spare_levels.size());
    const ByteReader spare_reader(spare);
    for (size_t l = 0; l < t.spare_levels.size(); ++l) t.spare_levels[l] = int32_t(spare_reader.le<uint32_t>(4 * l));
    for (auto b : t.level_bike)
        if (b >= t.bike_price.size()) throw std::runtime_error("Invalid default bike table");
    return t;
}

int character_rider(int character) {
    static constexpr int map[8] = {8, 7, 2, 5, 4, 6, 3, 1};   // 0x00415fc0
    return character >= 1 && character <= 8 ? map[character - 1] : 0;
}

int showroom_bike(int slot) {
    // 0x0041ff00: Rat 0x80.., Sport 0x85.., Super 0x8a..
    static constexpr int map[15] = {0, 1, 2, 4, 3, 10, 12, 14, 11, 13, 5, 7, 9, 6, 8};
    return slot >= 0x80 && slot <= 0x8e ? map[slot - 0x80] : 0;
}

uint32_t career_random(Career& c) {
    c.rng = c.rng * 214013u + 2531011u;
    return (c.rng >> 16) & 0x7fff;
}

void reset_relations(Career& c, const CareerTables& t) {
    c.relations = t.default_relations;
    // 0x00407918..0x00407978: copy personality 0 into each spare row, then
    // +0x1bc[level] += rand()%13 - 6 for levels 0..4 (0x00441750 draws).
    for (auto& row : c.spare_levels)
        for (size_t l = 0; l < row.size(); ++l) row[l] = t.spare_levels[l] + int32_t(career_random(c) % 13) - 6;
}

void select_mode(Career& c, const CareerTables& t, GameMode mode) {
    if (c.mode != mode) {
        c.state.level = 0;
        c.state.qualified = 0;
        c.state.character = 0;
        reset_relations(c, t);
    }
    c.mode = mode;
}

void reset_for_character(Career& c, const CareerTables& t) {
    if (c.mode == GameMode::big_game) {
        c.state.level = 0;
        c.state.qualified = 0;
        c.state.cash = c.state.character < t.start_cash.size() ? t.start_cash[c.state.character] : 0;
    }
    const size_t i = size_t(c.state.level) + size_t(character_rider(c.state.character)) * 5;
    c.state.bike = t.level_bike.at(i);
    reset_relations(c, t);   // 0x004078f0 via 0x0041f630
}

void choose_character(Career& c, const CareerTables& t, int character) {
    if (character < 1 || character > 8) throw std::runtime_error("Invalid character");
    c.state.character = uint8_t(character);
    reset_for_character(c, t);
    if (c.mode == GameMode::big_game) c.unsaved = true;
}

void set_level(Career& c, const CareerTables& t, int level) {
    if (level < 1 || level > 5) throw std::runtime_error("Invalid level");
    c.state.level = uint8_t(level - 1);
    c.state.qualified = 0;
    c.state.bike = t.level_bike.at(size_t(c.state.level) + size_t(character_rider(c.state.character)) * 5);
}

void start_race(Career& c, int course) {
    if (course < 1 || course > 5) throw std::runtime_error("Invalid course");
    if (c.mode != GameMode::mano) c.state.player = 0;
    c.state.course = uint8_t(course);
}

PurchaseResult buy_bike(Career& c, const CareerTables& t, int bike) {
    if (bike < 0 || bike >= int(t.bike_price.size()) || c.state.bike >= t.bike_price.size()) throw std::runtime_error("Invalid bike");
    if (c.state.bike == bike) return PurchaseResult::already_owned;
    const uint32_t trade_in = t.bike_price[c.state.bike] >> 1;
    if (uint32_t(trade_in + c.state.cash) < t.bike_price[size_t(bike)]) return PurchaseResult::cannot_afford;
    c.state.cash = c.state.cash + (trade_in - t.bike_price[size_t(bike)]);
    c.state.bike = uint8_t(bike);
    c.unsaved = true;
    return PurchaseResult::bought;
}

RaceReport finish_race(Career& c, const CareerTables& t, RaceOutcome outcome, int place) {
    RaceReport r;
    if (place < 0) place = 0;
    if (place > 14) place = 14;
    c.state.place = uint8_t(place);
    if (c.mode == GameMode::attract) { r.screen = screen_id::main_menu; return r; }
    switch (outcome) {
    case RaceOutcome::won: r.screen = screen_id::won; break;
    case RaceOutcome::lost: r.screen = screen_id::lost; break;
    case RaceOutcome::busted: r.screen = screen_id::busted; break;
    default: r.screen = screen_id::wrecked; break;
    }
    // 0x00449150: a win qualifies the course (not in mano-a-mano).
    if (r.screen == screen_id::won && c.mode != GameMode::mano && c.state.course >= 1 && c.state.course <= 5)
        c.state.qualified |= uint8_t(1u << (c.state.course - 1));
    if (r.screen == screen_id::won || r.screen == screen_id::lost) {
        // 0x00421290: Big Game pays every finisher by place, times level+1.
        if (c.mode == GameMode::big_game) {
            r.prize = t.prize[size_t(place)] * (uint32_t(c.state.level) + 1);
            c.state.cash += r.prize;
        }
    } else if (c.mode == GameMode::big_game) {
        // 0x004214e0: fine (level+1)*400 or repairs at a tenth of the bike price.
        const bool busted = r.screen == screen_id::busted;
        r.penalty = busted ? (uint32_t(c.state.level) * 5 + 5) * 0x50 : t.bike_price.at(c.state.bike) / 10;
        if (r.penalty <= c.state.cash) {
            c.state.cash -= r.penalty;
            r.penalty_message = busted ? 197 : 194;
        } else {
            r.penalty_message = 198;
            r.game_over = true;
            reset_for_character(c, t);   // 0x0041f630
        }
    } else if (c.mode == GameMode::thrash) {
        c.state.qualified = 0;
        r.penalty_message = r.screen == screen_id::busted ? 196 : 193;
    }
    return r;
}

int after_results(Career& c) {
    if (c.state.qualified != all_qualified) {
        switch (c.mode) {
        case GameMode::attract: return screen_id::main_menu;
        case GameMode::thrash: return screen_id::thrash_courses;
        case GameMode::big_game: return c.state.character ? screen_id::street : screen_id::characters;
        default: return screen_id::mano;
        }
    }
    c.unsaved = true;
    c.state.qualified = 0;
    if (c.state.level != 4) {
        ++c.state.level;
        return screen_id::level_up;
    }
    return screen_id::finale;
}

std::string results_video(const Career& c, int screen, uint32_t random) {
    auto numbered = [&](const char* base, uint32_t n) { return std::string(base) + std::to_string(random % n + 1); };
    switch (screen) {
    case screen_id::wrecked: return numbered("WRECK", 6);
    case screen_id::busted: return numbered("BUSTED", 6);
    case screen_id::lost: return numbered("LOSE", 10);
    case screen_id::won:
        if (c.state.qualified == all_qualified) return c.state.level == 4 ? "FINALWIN" : numbered("LEVEL", 6);
        return numbered("WIN", 6);
    default: return {};
    }
}

uint32_t save_checksum(const uint8_t* p, size_t n) {
    uint32_t alternating = 0, sum = 0, weighted = 0;
    for (; n != 0; --n, ++p) {
        const uint32_t b = *p;
        sum += b;
        weighted += uint32_t(n) * b;
        alternating += (n & 1) ? b : uint32_t(0) - b;
    }
    return (alternating << 16 | (sum & 0xffff)) ^ weighted;
}

std::vector<uint8_t> write_save(const Career& c) {
    std::vector<uint8_t> b(save_size);
    put32(&b[4], save_magic);
    const GameState& s = c.state;
    const uint8_t head[8] = {s.player, s.character, s.level, s.qualified, s.bike, s.course, s.place, s.flags};
    std::memcpy(&b[8], head, 8);
    put32(&b[16], s.cash);
    std::memcpy(&b[20], s.name.data(), s.name.size());
    const size_t rider = size_t(character_rider(c.state.character));
    for (size_t p = 0; p < 10; ++p)
        for (size_t k = 0; k < 3; ++k) put32(&b[40 + 12 * p + 4 * k], uint32_t(c.relations[p][rider][k]));
    put32(&b[0], save_checksum(&b[4], save_size - 4));
    return b;
}

Career read_save(const std::vector<uint8_t>& b, const CareerTables& t, uint32_t rng) {
    if (b.size() != save_size) throw std::runtime_error("Save file has the wrong size");
    const ByteReader save(b);
    if (save.le<uint32_t>(4) != save_magic) throw std::runtime_error("Not a Road Rash save file");
    if (save.le<uint32_t>(0) != save_checksum(&b[4], save_size - 4)) throw std::runtime_error("Save file checksum mismatch");
    Career c;
    GameState& s = c.state;
    s.player = b[8]; s.character = b[9]; s.level = b[10]; s.qualified = b[11];
    s.bike = b[12]; s.course = b[13]; s.place = b[14]; s.flags = b[15];
    s.cash = save.le<uint32_t>(16);
    std::memcpy(s.name.data(), &b[20], s.name.size());
    if (s.character > 8 || s.level > 4 || s.qualified > all_qualified || s.bike > 14 || s.course > 5)
        throw std::runtime_error("Save file has out-of-range values");
    s.name.back() = '\0';
    c.rng = rng;
    reset_relations(c, t);   // 0x004078f0 before the saved triples are applied
    const size_t rider = size_t(character_rider(s.character));
    for (size_t p = 0; p < 10; ++p)
        for (size_t k = 0; k < 3; ++k) c.relations[p][rider][k] = int32_t(save.le<uint32_t>(40 + 12 * p + 4 * k));
    c.mode = GameMode::big_game;   // 0x0042f110 -> 0x00444fb0(2)
    c.unsaved = false;
    return c;
}
}
