#include "race.hpp"
#include "track_ring.hpp"
#include "pedestrians.hpp"
#include "terrain_ground.hpp"
#include "traffic.hpp"
#include "pedestrians.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdlib>
#include <stdexcept>

namespace rr {
namespace {
int32_t div_pow2(int64_t x, int n) { return int32_t(x / (int64_t(1) << n)); }
int32_t div256(int64_t x) { return div_pow2(x, 8); }
int32_t wrap_angle(int32_t a) { return a > 0x7fffff ? a - 0x1000000 : a; }
constexpr int32_t ground_wall = 999;                                 // 0x00439b10 wall height
// Grid (0x00464218): slots 0..3 at sample 0x32, 4..7 at 0x33, four lateral offsets.
constexpr int32_t grid_lateral[4] = {16384, -3616, -23616, -43616};
int32_t grid_position(int slot) { return slot < 4 ? 0x3200 : 0x3300; }
int32_t ai_speed_units(int32_t v) { return (v >> 12) + (v >> 7) + (v >> 9); }   // 0x00409f70
}

std::vector<Opponent> original_opponents(const std::vector<uint8_t>& exe, const std::vector<BikeSpec>& specs,
                                         int level, int count);
namespace {
// Read one little-endian i32 from the reference EXE by virtual address.
int32_t exe_word(const std::vector<uint8_t>& exe, uint32_t va) {
    constexpr uint32_t text_va = 0x00401000, text_raw = 0x400, text_size = 0x5fc00;
    constexpr uint32_t data_va = 0x00463000, data_raw = 0x61600, data_size = 0x16400;
    size_t raw;
    if (va >= text_va && va + 4 <= text_va + text_size) raw = text_raw + (va - text_va);
    else if (va >= data_va && va + 4 <= data_va + data_size) raw = data_raw + (va - data_va);
    else throw std::runtime_error("EXE word outside mapped sections");
    if (raw + 4 > exe.size()) throw std::runtime_error("EXE word outside file");
    return int32_t(uint32_t(exe[raw]) | uint32_t(exe[raw + 1]) << 8 |
                   uint32_t(exe[raw + 2]) << 16 | uint32_t(exe[raw + 3]) << 24);
}
}  // namespace

std::vector<Personality> read_personalities(const std::vector<uint8_t>& exe) {
    // 0x00464558: 10 rider personalities, 0x204 bytes each (0x004078f0 copies
    // them verbatim to 0x004c6b40; the AI brain is one such copy).
    std::vector<Personality> out;
    for (int k = 0; k < 10; ++k) {
        const uint32_t base = 0x00464558 + 0x204u * uint32_t(k);
        Personality p;
        for (int i = 0; i < 5; ++i) {
            p.cur2_levels[i] = exe_word(exe, base + 0x1a8 + 4u * uint32_t(i));
            p.cur_levels[i] = exe_word(exe, base + 0x1bc + 4u * uint32_t(i));
        }
        for (int i = 0; i < 30; ++i) {
            p.rating[i] = exe_word(exe, base + 4u * uint32_t(i));
            p.rating_tgt[i] = exe_word(exe, base + 0x78 + 4u * uint32_t(i));
        }
        for (int i = 0; i < 3; ++i) p.ease[i] = exe_word(exe, base + 0x12c + 4u * uint32_t(i));
        const uint32_t b165 = base + 0x165;
        constexpr uint32_t data_va = 0x00463000, data_raw = 0x61600;
        p.overtake_index = int(exe[data_raw + (b165 - data_va)]);
        p.reach = exe_word(exe, base + 0x1d4);
        out.push_back(p);
    }
    return out;
}

// Full-field grid tables (0x00405600): level < 4 uses 0x00464088, else 0x00464108.
// 16 slots of (lateral, sub-slice selector); position equals the selector.
void read_grid_slot(const std::vector<uint8_t>& exe, int level, int slot,
                    int32_t& lateral, int32_t& pos) {
    if (slot < 0 || slot > 15) throw std::runtime_error("Grid slot out of range");
    const uint32_t base = (level < 4 ? 0x00464088u : 0x00464108u) + 8u * uint32_t(slot);
    lateral = exe_word(exe, base);
    pos = exe_word(exe, base + 4);
}
std::vector<Opponent> original_opponents(const std::vector<uint8_t>& exe, const std::vector<BikeSpec>& specs,
                                         int level, int count) {
    if (level < 0 || level > 4 || count < 0 || count > 14) throw std::runtime_error("Invalid level or opponent count");
    constexpr uint32_t data_va = 0x00463000, data_raw = 0x61600, data_size = 0x16400;
    auto at = [&](uint32_t va) {
        if (va < data_va || va + 4 > data_va + data_size || exe.size() < data_raw + (va - data_va) + 4)
            throw std::runtime_error("Opponent table outside .data");
        return data_raw + (va - data_va);
    };
    auto word = [&](uint32_t va) {
        const size_t p = at(va);
        return int32_t(uint32_t(exe[p]) | uint32_t(exe[p+1]) << 8 | uint32_t(exe[p+2]) << 16 | uint32_t(exe[p+3]) << 24);
    };
    auto byte = [&](uint32_t va) { return exe[at(va & ~3u) + (va & 3u)]; };
    std::vector<Opponent> out;
    for (int k = 1; k <= count; ++k) {
        // 0x00407990: slots 0..8 use personality copies 0x004c6b40 + slot
        // rows; 9..14 use 0x004c5f20 rows, which 0x004078f0 fills at career
        // start with row 0 and +0x1bc speeds jittered by rand()%13-6 (drawn
        // then, outside the race stream: not reproduced).
        const uint32_t personality = 0x00464558 + 0x204u * uint32_t(k < 9 ? k : 0);
        const size_t bike = byte(0x004641c8 + uint32_t(level + k * 5));
        if (bike >= specs.size()) throw std::runtime_error("Opponent bike index out of range");
        const uint32_t row = 0x00463c78 + 0x40u * uint32_t(k);
        // 0x00405600 builds slots 1..<DAT_004642d4-1 with the grid tables
        // 0x00464088 (level<4) / 0x00464108: position equals the selector,
        // lateral is the table lateral (0x00405990 adds road centre at
        // build). Slots 1..7 existed before; 8..14 complete the offline 15.
        int32_t grid_lat = 0, grid_pos = 0;
        read_grid_slot(exe, level, k, grid_lat, grid_pos);
        Opponent o{specs[bike], word(row + 0xc) / 2, word(personality + 0x1bc + 4u * uint32_t(level)),
                   word(personality + 0x1a8 + 4u * uint32_t(level)),
                   int(byte(personality + 0x18c)), k, k, grid_pos, grid_lat, false,
                   word(0x004642a0 + 4u * uint32_t(level)),
                   {word(row + 0x30), word(row + 0x34), word(row + 0x38), word(row + 0x3c)},
                   word(row + 0x18 + 4u * uint32_t(level))};
        out.push_back(o);
    }
    return out;
}
int32_t race_road_base_q8(int32_t golden_lateral, int32_t route_offset, int32_t grid_word) {
    // 0x00405990: lateral = sub-slice +0xcc (absolute Q8 road centre) +
    // grid offset. RouteSample::offset is relative (world units), so the
    // absolute anchor is golden - relative*256 - grid. No RNG/allocation.
    return golden_lateral - route_offset * 256 - grid_word;
}

Race::Race(std::vector<RouteSample> a, std::vector<RouteSample> b, const PhysicsTables& t,
           const BikeSpec& spec, int32_t rider_value, std::vector<Opponent> opponents, int level, int32_t road_base_q8,
           const std::vector<Personality>& personalities, const std::vector<uint8_t>& exe_image)
    : routes_{std::move(a), std::move(b)}, tables_(t), level_(level), road_base_(road_base_q8) {
    if (!exe_image.empty()) {
        std::array<int32_t,5> eager{};
        for (uint32_t i = 0; i < 5; ++i) eager[i] = exe_word(exe_image, 0x00465a10 + 4u * i);
        overtake_eagerness_ = eager;
    }
    if (routes_[0].size() < 64 || routes_[1].size() < 64) throw std::runtime_error("Route too short for a race");
    auto brain_from = [&](AiState& ai, const Personality& p) {
        for (int i = 0; i < 5; ++i) { ai.spd_a[i] = p.cur2_levels[i]; ai.spd_b[i] = p.cur_levels[i]; }
        for (int i = 0; i < 30; ++i) { ai.rating[i] = p.rating[i]; ai.rating_tgt[i] = p.rating_tgt[i]; }
        for (int i = 0; i < 3; ++i) ai.ease[i] = p.ease[i];
        ai.overtake_index = p.overtake_index;
        ai.reach = p.reach;
    };
    auto place_rider = [&](const BikeSpec& s, int32_t value, int slot, int32_t grid_pos, int32_t grid_lat) {
        Rider r;
        r.slot = slot;
        init_bike_object(r.bike, tables_);
        apply_spec(r.bike, s, value);
        r.bike.i32(0x18) = 1;                                   // updated every tick
        // Table grid (0x00464088/0x00464108): position equals the selector;
        // lateral = road centre (Q8) + table lateral. grid_pos 0 keeps the
        // legacy two-row synthetic grid.
        const int32_t pos = grid_pos ? grid_pos : grid_position(slot);
        r.sample = r.previous_sample = size_t(pos >> 8);
        r.bike.i32(0x1c) = pos;
        r.bike.i32(0x138) = pos;
        // 0x00405990: lateral = sub-slice +0xcc (road centre, Q8) + grid offset.
        // RouteSample::offset is world units (edges/AI scale it by 256);
        // road_base_ anchors the relative route to the original absolute
        // frame (ctor param); the default 0 keeps synthetic routes exact.
        const int32_t lat = grid_pos ? grid_lat : grid_lateral[slot % 4];
        r.bike.i32(0x28) = routes_[0][r.sample].offset * 256 + lat + road_base_;
        // 0x00405990 passes the same grid offset to 0x00407990 as the
        // brain's lane bias (+0x190), which 0x004091f0 adds to its target.
        r.brain.bias = lat;
        r.bike.i32(0x2f0) = r.bike.i32(0x2ec);   // bike condition (0x00405c80)
        return r;
    };
    Rider player = place_rider(spec, rider_value, 0, 0, 0);
    if (!personalities.empty()) brain_from(player.brain, personalities[0]);
    riders_.push_back(player);
    int colour = 1;
    for (const auto& o : opponents) {
        if (o.slot < 1 || o.slot > 15) throw std::runtime_error("Opponent grid slot must be 1..15");
        Rider r = place_rider(o.spec, o.rider_value, o.slot, o.grid_pos, o.grid_lat);
        r.ai = true;
        r.rider_idx = o.rider;
        r.frozen = o.frozen;
        r.colour = colour++ % 9;
        // 0x00405990 for AI riders: level throttle scale and softer low gears.
        r.bike.i32(0x260) = (o.throttle_percent * r.bike.i32(0x260)) / 100;
        r.bike.i32(0x1d0) -= 0x19; r.bike.i32(0x1e0) -= 0xf; r.bike.i32(0x1f0) -= 8; r.bike.i32(0x200) -= 4;
        r.brain.base_speed = r.brain.current_speed = o.base_speed;
        r.brain.current2 = o.cur2_speed;
        // Synthetic-test fallback: the traced replan resets speeds from the
        // per-level tables (spd_a/spd_b), which only read_personalities fills.
        // Without them the reset would zero the speed and stall the field.
        for (int i = 0; i < 5; ++i) { r.brain.spd_a[i] = o.cur2_speed; r.brain.spd_b[i] = o.base_speed; }
        // 0x00405990 draws the lane as rand()%3 at build; -1 requests the draw.
        r.brain.lane = o.lane >= 0 ? o.lane : int(random() % 3);
        if (!personalities.empty() && o.rider >= 0) {
            brain_from(r.brain, personalities[o.rider < 9 ? size_t(o.rider) : 0]);
            if (o.rider >= 9) r.brain.overtake_index = o.rider + 1;   // 0x00407990: +0x165 = slot + 1
        }
        r.stamina = o.stats;
        r.weapon = o.weapon;
        riders_.push_back(r);
    }
    // Nitro setup (0x0040b0e0): copy the six words from the player spec
    // at +0x14c when the spec carries them, else keep EXE defaults; the
    // accumulator starts at maximum<<8. Synthetic specs are zero there, so
    // the zero-guard keeps tests on the documented defaults.
    {
        bool spec_nitro = false;
        for (int k = 0; k < 6; ++k)
            if (spec.d[size_t(77 + k)] != 0) spec_nitro = true;
        if (spec_nitro) {
            nitro_.recharge = spec.d[77]; nitro_.boost = spec.d[78]; nitro_.max = spec.d[79];
            nitro_.charges = spec.d[80]; nitro_.minimum = spec.d[81]; nitro_.tail = spec.d[82];
        }
        nitro_charges_ = nitro_.charges;
        for (auto& rider : riders_) rider.nitro_charge = nitro_.max << 8;
    }
    update_camera();
    previous_camera_position_=camera_position();
}

void Race::set_crash_library(const ActorAnimationLibrary& library,const CrashConfig& config) {
    crash_library_=&library;crash_config_=config;
    for(auto& rider:riders_)init_crash_actor(rider.crash_actor,config);
}

RaceWiring Race::wiring() const {
    RaceWiring w;
    w.terrain = bool(terrain_source_);
    w.traffic = traffic_ != nullptr;
    w.pedestrians = pedestrians_ != nullptr;
    w.ring = ring_ != nullptr;
    w.crash_library = crash_library_ != nullptr;
    w.ride_library = ride_library_ && attack_codes_;
    w.nitro = nitro_set_;
    w.finish = finish_q8_ != 0;
    w.personalities = overtake_eagerness_.has_value();
    return w;
}

void Race::set_demo_hold(bool v, int base_speed, int lane, int32_t bias,
                       int weapon, RiderStats stats) {
    demo_hold_ = v;
    if (v && !riders_.empty()) {
        // Seed the player AI brain like an opponent build: replan starts at
        // zero (first reset 60 ticks after green) with base/current from the
        // rider's personality row. The demo rider's own value/weapon/stats
        // come from its rider-table row (0x00405990/0x00405c80).
        riders_[0].brain.base_speed = riders_[0].brain.current_speed = base_speed;
        // Scalar-only demo fixtures have no imported personality speed tables.
        // Match the opponent fallback; otherwise the first replan resets the
        // demo player's speed to zero. Imported tables remain authoritative.
        auto& brain = riders_[0].brain;
        if (std::all_of(brain.spd_b, brain.spd_b + 5, [](int32_t v) { return v == 0; })) {
            std::fill(brain.spd_b, brain.spd_b + 5, base_speed);
            std::fill(brain.spd_a, brain.spd_a + 5, base_speed);
        }
        riders_[0].brain.lane = lane;
        riders_[0].brain.bias = bias;
        riders_[0].stamina = stats;
        riders_[0].weapon = weapon;
    }
}

SliceEdges Race::edges(const Rider& r, size_t i) const {
    const auto& s = route_of(r)[i];
    // Imported terrain is attached by the game; standalone physics fixtures keep their original synthetic sides.
    return {s.offset - s.path.left, s.offset + s.path.right,
            s.offset - s.path.left - s.path.shoulder_left, s.offset + s.path.right + s.path.shoulder_right, terrain_source_?s.path.marking_flags:uint8_t(0), terrain_source_?terrain_source_(s).type:uint8_t(0)};
}

void Race::accumulate(Rider& r, uint32_t from, uint32_t to, size_t from_sample, size_t to_sample) {
    // 0x004394c0: bend and rise swept over the move, weighted by the Q8 distance in each sample.
    const auto& route = route_of(r);
    Bike& b = r.bike;
    const int32_t bend_now = route[to_sample].path.bend, rise_now = route[to_sample].path.rise;
    if (((to ^ from) & 0xffffff00u) == 0) {
        const int32_t d = int32_t(to - from);
        b.i32(0x130) = -(bend_now * d);
        b.i32(0x12c) = rise_now * d;
        return;
    }
    int32_t in_new = int32_t(to & 0xff), in_old;
    if (int32_t(to) < int32_t(from)) { in_old = -int32_t(from & 0xff); in_new -= 0x100; }
    else in_old = 0x100 - int32_t(from & 0xff);
    const int32_t bend_old = route[from_sample].path.bend, rise_old = route[from_sample].path.rise;
    b.i32(0x130) = -(bend_now * in_new + bend_old * in_old);
    b.i32(0x12c) = rise_now * in_new + rise_old * in_old;
}

void Race::cross_sample(Rider& r, int32_t delta) {
    // 0x00439390 + 0x004394c0 for one step of at most one sample.
    Bike& b = r.bike;
    const uint32_t from = uint32_t(b.i32(0x1c));
    uint32_t to = from + uint32_t(delta);
    const size_t last = route_of(r).size() - 2;
    if (int32_t(to) < 0) to = 0;
    // 0x00406890: finish when the nonzero global DAT_004b6e58 is reached
    // (with the +0x2c scheduler gate and the +0xc0 bit-0x10 latch moving to
    // 0x00437800). finish_q8_ carries DAT_004b6e58; 0 keeps route-end
    // semantics for synthetic routes. +0x2c is nonzero for every inserted
    // scheduler actor (bike.cpp scheduler insert), so the gate passes here.
    if (finish_q8_ != 0 && (b.u8(0xc0) & 0x10) == 0 && int32_t(to) >= finish_q8_) {
        const size_t start_sample = r.sample;
        to = uint32_t(finish_q8_);
        b.i32(0x134) = int32_t(from);
        b.i32(0x1c) = int32_t(to);
        b.i32(0x138) = int32_t(to);
        r.previous_sample = start_sample;
        r.sample = size_t(to >> 8);
        accumulate(r, from, to, start_sample, r.sample);
        if (!r.finished) { r.finished = true; r.finish_tick = ticks_; }
        b.u8(0xc0) = uint8_t(b.u8(0xc0) | 0x10);   // 0x00437800 rider+bike latch
        return;
    }
    if ((to >> 8) > last) {
        to = uint32_t(last << 8);
        if (!r.finished) { r.finished = true; r.finish_tick = ticks_; }
        b.u8(0xc0) = uint8_t(b.u8(0xc0) | 0x10);   // 0x00437800 latch on the legacy path too
    }
    b.i32(0x134) = int32_t(from);
    b.i32(0x1c) = int32_t(to);
    b.i32(0x138) = int32_t(to);
    const size_t from_sample = r.sample;
    if (((to ^ from) & 0xffffff00u) != 0) {
        r.previous_sample = r.sample;
        r.sample = to >> 8;
        // Entering a split: the side of half 0's right edge decides the branch (0x00439390).
        const auto& a = routes_[0][r.sample];
        if (r.route == 0 && int32_t(to) > int32_t(from) && a.event == RoadEvent::split) {
            const int32_t half0_right = a.offset + a.path.right;
            if ((b.i32(0x28) >> 8) >= half0_right) r.route = 1;
        }
    }
    accumulate(r, from, to, from_sample, r.sample);
}

void Race::move_along(Rider& r, int32_t distance, bool sweep) {
    // 0x00439270: split long moves into whole-sample steps and sum the swept bend/rise.
    // sweep=false is the third-argument-0 form used by collisions: position only.
    Bike& b = r.bike;
    if (distance == 0) { if (sweep) { b.i32(0x130) = 0; b.i32(0x12c) = 0; } return; }
    const int32_t saved_bend = b.i32(0x130), saved_rise = b.i32(0x12c);
    const int32_t pos = b.i32(0x1c);
    int32_t steps = ((pos + distance) >> 8) - (pos >> 8);
    int32_t unit = 0x100;
    if (steps < 0) { steps = -steps; unit = -0x100; }
    if (steps < 2) {
        cross_sample(r, distance);
        if (!sweep) { b.i32(0x130) = saved_bend; b.i32(0x12c) = saved_rise; }
        return;
    }
    const size_t start_sample = r.sample;
    const int32_t magnitude = std::abs(distance);
    int32_t first = magnitude & 0xff;
    if (unit < 0) first = -first;
    cross_sample(r, first);
    int32_t bend = b.i32(0x130), rise = b.i32(0x12c);
    for (int32_t n = magnitude >> 8; n > 0 && !r.finished; --n) {
        cross_sample(r, unit);
        bend += b.i32(0x130); rise += b.i32(0x12c);
    }
    b.i32(0x134) = pos;
    r.previous_sample = start_sample;
    b.i32(0x130) = sweep ? bend : saved_bend;
    b.i32(0x12c) = sweep ? rise : saved_rise;
}

void Race::separate(Rider& r1, uint32_t t1, Rider& r2, uint32_t t2) {
    // 0x00436a40, line for line. p1 is the walk's outer node, p2 its partner;
    // t1/t2 are their object types (+0xc4). 0x00439270 moves along the road.
    Bike& p1 = r1.bike; Bike& p2 = r2.bike;
    const int32_t m1 = p1.i32(0x194), m2 = p2.i32(0x194);
    p2.i32(400) = m1; p1.i32(400) = m2;
    const int32_t m = m1 + m2;
    int32_t rel = p1.i32(0x108) - p2.i32(0x108);
    rel = std::abs((rel >> 9) + (rel >> 12) + (rel >> 7));
    p1.u8(0x19c) = 0; p2.u8(0x19c) = 0;
    auto stack = [&](bool& raise1, bool& any) {   // height stacking when one object is under half the other's height
        if (p1.i32(0x17c) < p2.i32(0x17c) / 2 && p1.i32(0x20) == 0) { raise1 = false; any = true; }
        else if (p2.i32(0x17c) < p1.i32(0x17c) / 2 && p2.i32(0x20) == 0) { raise1 = true; any = true; }
        else { raise1 = false; any = false; }
    };
    if (p2.i32(0x178) + p1.i32(0x178) < std::abs(p1.i32(0x30) - p2.i32(0x30))) {
        bool raise1 = false, any = false;
        stack(raise1, any);
        int32_t x1 = p1.i32(0x28), h1 = p1.i32(0x174) / 2;
        if (p2.i32(0x28) - p2.i32(0x174) < x1 - h1 && h1 + x1 < p2.i32(0x28) + p2.i32(0x174) && 0x2000 < p2.i32(0x17c))
            p1.u8(0x19c) |= 0x40;
        const int32_t h2 = p2.i32(0x174) / 2;
        if (x1 - p1.i32(0x174) < p2.i32(0x28) - h2 && h2 + p2.i32(0x28) < x1 + p1.i32(0x174) && 0x2000 < p1.i32(0x17c))
            p2.u8(0x19c) |= 0x40;
        const int32_t depth = (p2.i32(0x178) - std::abs(p1.i32(0x1c) - p2.i32(0x1c))) + 8 + p1.i32(0x178);
        if (any) {
            const int32_t up = (p2.i32(0x17c) - std::abs(p1.i32(0x20) + ((p1.i32(0x17c) - p2.i32(0x20)) - p2.i32(0x17c)))) + 0x800 + p1.i32(0x17c);
            if (raise1) p1.i32(0x20) = up + p1.i32(0x20) + 3;
            else p2.i32(0x20) = up + p2.i32(0x20) + 3;
        }
        if (p2.i32(0x30) < p1.i32(0x30)) {
            p1.u8(0x19c) |= 1; p2.u8(0x19c) |= 2;
            if (!any) {
                move_along(r1, (depth * m2) / m + 3, false);
                if (t1 != 4 || rel < 0x14) move_along(r2, -((depth * m1) / m) - 3, false);
            }
        } else {
            p1.u8(0x19c) |= 2; p2.u8(0x19c) |= 1;
            if (!any) {
                if (t1 != 4 || rel < 0x14) move_along(r1, -((depth * m2) / m) - 3, false);
                move_along(r2, (depth * m1) / m + 3, false);
            }
        }
        const int32_t imp = t2 == 1 ? (p2.i32(0x108) - p1.i32(0x108)) * ((m2 * 2) / m)
                                    : ((p2.i32(0x108) - p1.i32(0x108)) * m2 * 2) / m;
        p1.i32(0x184) = imp; p2.i32(0x184) = -imp;
    } else {
        p1.i32(0x184) = 0; p2.i32(0x184) = 0;
    }
    if (p1.i32(0x174) + p2.i32(0x174) < std::abs(p1.i32(0x3c) - p2.i32(0x3c))) {
        const int32_t depth = (p2.i32(0x174) - std::abs(p1.i32(0x28) - p2.i32(0x28))) + 0x800 + p1.i32(0x174);
        if (p2.i32(0x3c) < p1.i32(0x3c)) {
            p1.u8(0x19c) |= 4; p2.u8(0x19c) |= 8;
            p1.i32(0x28) += (depth * m2) / m + 3;
            p2.i32(0x28) -= (depth * m1) / m + 3;
        } else {
            p1.u8(0x19c) |= 8; p2.u8(0x19c) |= 4;
            p1.i32(0x28) -= (depth * m2) / m + 3;
            p2.i32(0x28) += (depth * m1) / m + 3;
        }
        const int32_t imp = t2 == 1 ? (p2.i32(0x10c) - p1.i32(0x10c)) * ((m2 * 2) / m)
                                    : ((p2.i32(0x10c) - p1.i32(0x10c)) * m2 * 2) / m;
        p1.i32(0x188) = imp; p2.i32(0x188) = -imp;
    } else {
        p1.i32(0x188) = 0; p2.i32(0x188) = 0;
    }
    {
        const int32_t a = p1.i32(0x17c), b = p2.i32(0x17c);
        int32_t imp = 0;
        if (a + b < std::abs(p1.i32(0x34) + ((a - p2.i32(0x34)) - b))) {
            const int32_t depth = (b - std::abs(((p1.i32(0x20) - p2.i32(0x20)) - b) + a)) + 0x800 + a;
            if (p2.i32(0x34) < p1.i32(0x34)) {
                p1.u8(0x19c) |= 0x20;
                p2.u8(0x19c) = uint8_t((p2.u8(0x19c) & 0xbf) | 0x10);
                p1.i32(0x20) += (depth * m2) / m + 3;
                p2.i32(0x20) = (p2.i32(0x20) - (depth * m1) / m) - 3;
                if (p2.i32(0x20) < 0) { p1.i32(0x20) -= p2.i32(0x20); p2.i32(0x20) = 0; }
            } else {
                p1.u8(0x19c) = uint8_t((p1.u8(0x19c) & 0xbf) | 0x10);
                p2.u8(0x19c) |= 0x20;
                p1.i32(0x20) -= (depth * m2) / m + 3;
                p2.i32(0x20) = p2.i32(0x20) + 3 + (depth * m1) / m;
                if (p1.i32(0x20) < 0) { p2.i32(0x20) -= p1.i32(0x20); p1.i32(0x20) = 0; }
            }
            imp = t2 == 1 ? (p2.i32(0x110) - p1.i32(0x110)) * ((m2 * 2) / m)
                          : ((p2.i32(0x110) - p1.i32(0x110)) * m2 * 2) / m;
            p1.i32(0x18c) = imp;
            imp = -imp;
        } else {
            p1.i32(0x18c) = 0;
        }
        p2.i32(0x18c) = imp;
    }
    if (p1.u8(0x19c) == 0) {
        // Overlapping on every axis already.
        const int32_t b17c = p2.i32(0x17c);
        bool any = false, raise1 = false;
        if (p1.i32(0x17c) < b17c / 2 && p1.i32(0x20) == 0) { any = true; raise1 = false; }
        else if (b17c < p1.i32(0x17c) / 2 && p2.i32(0x20) == 0) { any = true; raise1 = true; }
        if (any) {
            const int32_t up = (b17c - std::abs(p1.i32(0x20) + ((p1.i32(0x17c) - p2.i32(0x20)) - b17c))) + 0x800 + p1.i32(0x17c);
            if (raise1) p1.i32(0x20) = up + p1.i32(0x20) + 3;
            else p2.i32(0x20) = up + p2.i32(0x20) + 3;
        } else {
            const int32_t fwd = t2 == 1 ? (p2.i32(0x108) - p1.i32(0x108)) * ((m2 * 2) / m)
                                        : ((p2.i32(0x108) - p1.i32(0x108)) * m2 * 2) / m;
            p1.i32(0x184) = fwd; p2.i32(0x184) = -fwd;
            const int32_t lat = t2 == 1 ? (p2.i32(0x10c) - p1.i32(0x10c)) * ((m2 * 2) / m)
                                        : ((p2.i32(0x10c) - p1.i32(0x10c)) * m2 * 2) / m;
            p1.i32(0x188) = lat; p2.i32(0x188) = -lat;
            if (std::abs(div_pow2(fwd, 2)) < std::abs(lat) || p1.i32(0x10c) <= p1.i32(0x108)) {
                const int32_t x1 = p1.i32(0x28);
                const int32_t depth = (p2.i32(0x174) - std::abs(x1 - p2.i32(0x28))) + 0x800 + p1.i32(0x174);
                if (t1 != 4) {   // bikes are never pushed sideways here
                    if (lat < 0) { p1.u8(0x19c) |= 8; p1.i32(0x28) = (x1 - (depth * m2) / m) - 3; }
                    else { p1.u8(0x19c) |= 4; p1.i32(0x28) = (depth * m2) / m + x1 + 3; }
                }
                if (t2 != 4) {
                    if (-lat < 1) { p2.u8(0x19c) |= 8; p2.i32(0x28) -= (depth * m1) / m + 3; }
                    else { p2.u8(0x19c) |= 4; p2.i32(0x28) += (depth * m1) / m + 3; }
                }
            } else {
                const int32_t depth = (p2.i32(0x178) - std::abs(p1.i32(0x1c) - p2.i32(0x1c))) + 8 + p1.i32(0x178);
                if (fwd < 0) {
                    p1.u8(0x19c) |= 2;
                    if (t1 != 4 || rel < 0x14) move_along(r1, -((depth * m2) / m) - 3, false);
                } else {
                    p1.u8(0x19c) |= 1;
                    move_along(r1, (depth * m2) / m + 3, false);
                }
                if (-fwd < 1) {
                    p2.u8(0x19c) |= 2;
                    if (t2 != 4 || rel < 0x14) move_along(r2, -((depth * m1) / m) - 3, false);
                } else {
                    p2.u8(0x19c) |= 1;
                    move_along(r2, (depth * m1) / m + 3, false);
                }
            }
        }
    }
    for (int k = 0; k < 5; ++k) { p1.i32(0x30 + 4 * k) = p1.i32(0x1c + 4 * k); p2.i32(0x30 + 4 * k) = p2.i32(0x1c + 4 * k); }
}

void Race::collide(Rider& ra, Rider& rb) {
    // 0x004368f0 pair of bikes (+0xc4 == 4): 0x00436a40, then each handler.
    separate(ra, 4, rb, 4);
    contact(ra, rb);
    contact(rb, ra);
}
// Rider-against-car pass: 0x004368f0 pair loop over the scheduler object list
// (cars are +0xc4 == 0x10 objects), 0x00436a40 separation, 0x004049c0 bike
// side for cars. Car dims via 0x004387a0 from template 0x00475698:
// +0x174 = 16384, +0x178 = 64, +0x17c = 8960 (see analysis/car-notes.md).
// The original runs this inside 0x004368f0 right after the rider-pair loop;
// the native call sits at the same point in Race::step (after collide pairs,
// before the camera update), for every rider, player and AI.
void Race::collide_traffic(Rider& r) {
    if (!traffic_ || r.finished) return;
    if (r.bike.i32(0x2f4) != 0) return;   // crashed riders: 0x004049c0 rejects
    constexpr int32_t car_half_length = 64;      // 0x00475698 +0x48
    constexpr int32_t car_half_height = 8960;    // 0x00475698 +0x4c
    Bike& s = r.bike;
    const int32_t position = s.i32(0x1c), lateral = s.i32(0x28), height = s.i32(0x20);
    const int32_t half_length = s.i32(0x178), half_width = s.i32(0x174), half_height = s.i32(0x17c);
    const auto& route = route_of(r);
    const auto& sample = route[r.sample];
    for (auto& car : traffic_->cars_mut()) {
        if (!car.active || !car.collides) continue;
        const int32_t car_half_width = TrafficSystem::car_half_width(car);   // 0x00475700 + kind * 0x18
        // Split-roadway exclusion (0x004368f0 +0x36 & 3 shared-half test).
        if (sample.dual) {
            // Cars run on the player's route; riders on the other branch pass by.
            if (r.route != riders_[0].route) continue;
        }
        if (std::abs(position - car.position) > half_length + car_half_length) continue;
        if (std::abs(lateral - car.lateral) > half_width + car_half_width) continue;
        if (std::abs(height) > half_height + car_half_height) continue;
        // Separate along the previously-clear axis (0x00436a40, mass-weighted;
        // car mass 1800 >> bike 256, so the rider takes nearly all of it).
        // The longitudinal share needs course-ring traversal (0x00439270);
        // lateral separation is direct. Previous-position copies stay in sync.
        const bool was_clear_forward =
            car_half_length + half_length < std::abs(s.i32(0x30) - car.position);
        const bool was_clear_lateral =
            car_half_width + half_width < std::abs(s.i32(0x3c) - car.lateral);
        constexpr int32_t bike_mass = 256, car_mass = 1800;
        constexpr int32_t total = bike_mass + car_mass;
        // The car keeps no previous-position copy (no TrafficCar member);
        // the axis-choice compares against its current position.
        if (was_clear_forward) {
            const int32_t depth =
                (car_half_length + half_length - std::abs(position - car.position)) + 8;
            const int32_t push = (depth * car_mass) / total + 3;
            if (car.position < s.i32(0x30)) move_along(r, push, false);
            else if (r.bike.i32(0x108) - car.speed < 0x14) move_along(r, -push, false);
            // Car +0x108 (speed copy) has no TrafficCar member; car.speed
            // (+0xec) is the same value on a settled car (move() writes speed
            // directly, never staging through +0x108).
            s.i32(0x184) = ((car.speed - s.i32(0x108)) * car_mass * 2) / total;
        } else {
            s.i32(0x184) = 0;
        }
        if (was_clear_lateral) {
            const int32_t depth =
                (car_half_width + half_width - std::abs(lateral - car.lateral)) + 0x800;
            const int32_t push = (depth * car_mass) / total + 3;
            if (car.lateral < s.i32(0x3c)) { s.i32(0x28) += push; car.lateral -= (depth * bike_mass) / total + 3; }
            else { s.i32(0x28) -= push; car.lateral += (depth * bike_mass) / total + 3; }
            s.i32(0x188) = ((0 - s.i32(0x10c)) * car_mass * 2) / total;
        } else {
            s.i32(0x188) = 0;
        }
        s.i32(0x18c) = 0;
        // Contact flags 0x00436a40: bits 1/2 forward, 4/8 lateral (rider is
        // param_1). The car's own flags live on the scheduler object, which
        // has no native handle — only the rider side is observable.
        s.u8(0x19c) = uint8_t((was_clear_forward ? (car.position < s.i32(0x30) ? 1 : 2) : 0) |
                              (was_clear_lateral ? (car.lateral < s.i32(0x3c) ? 4 : 8) : 0));
        // Containment bit 0x40 (0x00436a40 prologue): the car's width contains
        // the rider when the rider's half-extent sits inside the car's span.
        if (car.lateral - car_half_width < lateral - half_width / 2 &&
            lateral + half_width / 2 < car.lateral + car_half_width)
            s.u8(0x19c) = uint8_t(s.u8(0x19c) | 0x40);
        for (int k = 0; k < 5; ++k) s.i32(0x30 + 4 * k) = s.i32(0x1c + 4 * k);
        contact_car(r, car);
        if (s.i32(0x2f4) != 0) return;   // knocked off: stop scanning
    }
}
// BORR span builder (plan 16 Task 2): the 0x00451cb0 lateral-slot walk over
// a section's BORR profiles, keeping only slots strictly inside the road
// edges (0x00454240 strictly-inside test) at sample fraction 0x80. The walk
// (record/stride/spacing) mirrors the billboard builder so both consume
// random() in the same order; the hazard half-width uses the same 224-unit
// inset the walk places slots at.
std::vector<RoadHazard> road_hazard_spans(const CourseSection& section, uint32_t sample_base,
                                          std::function<uint32_t()> random) {
    std::vector<RoadHazard> result;
    if (section.roadside_profiles.empty()) return result;
    size_t record = 0; int32_t start = 0, end = 0;
    auto period = [](uint32_t word) { return (word & 0x0f000000) ? int32_t((word >> 12) & 15) : 0; };
    auto duration = [&](uint32_t word) { return (period(word) + 1) * int32_t((word >> 24) & 15); };
    end = duration(section.roadside_profiles[0]);
    for (size_t j = 0; j < section.samples.size(); ++j) {
        if (j && int64_t(j) > end) {
            if (record + 1 >= section.roadside_profiles.size()) break;
            ++record; start = int32_t(j); end = start + duration(section.roadside_profiles[record]);
        }
        const uint32_t word = section.roadside_profiles[record];
        const uint8_t selector = uint8_t(word >> 16);
        const int32_t stride = period(word) + 1;
        if ((selector & 63) == 63 || (int32_t(j) - start) % stride) continue;
        bool left = (word & 0x10000000) != 0;
        if ((word & 0x80000000) && ((int32_t(j) - start) & 1) == 0) left = !left;
        int32_t spacing;
        if (word & 0x0f000000) spacing = (word & 0x20000000) ? int32_t(((word & 15) + 4) * 256) : int32_t((word & 15) * 64);
        else { const int32_t amount = int32_t(((word & 0xf000) >> 8) + (word & 15)); spacing = (word & 0x20000000) ? (amount + 32) * 32 : amount * 8; }
        const auto& path = section.samples[j];
        int32_t x = left ? -path.left + 224 - spacing : path.right - 224 + spacing;
        const int count = int((word >> 8) & 7) + 1;
        for (int object = 0; object < count; ++object) {
            if (-path.left < x && x < path.right)
                result.push_back({sample_base + uint32_t(j), (x - 224) * 256, (x + 224) * 256});
            const int32_t step = (word & 0x800) ? spacing / 2 + int32_t(random() % uint32_t(spacing - spacing / 2 + 1)) : spacing;
            x += left ? -step : step;
        }
    }
    return result;
}
// In-road BORR hazard pass (plan 16 Task 2). 0x00454240 instantiates only
// BORR slots strictly inside the road edges (0x00438930, pool DAT_00476ce8,
// template 0x00476cb8 class 1); 0x004373f0 sweeps the rider box
// (lateral +/- +0x174 at sample fraction 0x80) against each slot and
// 0x00437660 builds the static hazard actor and runs the shared 0x00436a40
// separator plus the rider contact handler. Hazards are immovable: the rider
// takes the full lateral push. The contact branch is class 1
// (0x004049c0: other +0xc4 == 1, short: the halved template +0x17c fits in
// half the bike's; reason 1 through knock_off, small-gap speed transfer,
// big-gap crash entry reason 1). No RNG, no allocation in the tick path.
void Race::collide_hazards(Rider& r) {
    if (r.finished || r.bike.i32(0x2f4) != 0) return;   // 0x004049c0 rejects
    const auto& table = hazards_[r.route];
    if (table.empty() || r.sample >= table.size()) return;
    const auto& spans = table[r.sample];
    if (spans.empty()) return;
    Bike& s = r.bike;
    for (const auto& h : spans) {
        const int32_t half = s.i32(0x174);
        if (s.i32(0x28) + half <= h.lo_q8 || h.hi_q8 <= s.i32(0x28) - half) continue;
        contact_hazard(r, h.lo_q8, h.hi_q8);
        if (s.i32(0x2f4) != 0) return;   // knocked off: stop scanning
    }
}
void Race::contact_hazard(Rider& self, int32_t hazard_lo_q8, int32_t hazard_hi_q8) {
    Bike& s = self.bike;
    if (s.i32(0x2f4) != 0) return;
    const int32_t elasticity = s.i32(0x180);
    s.i32(0x188) = div256(int64_t(s.i32(0x188)) * elasticity);
    s.i32(0x184) = div256(int64_t(s.i32(0x184)) * elasticity);
    const int32_t hit = std::abs(s.i32(0x184));
    if (&self == &riders_[0]) {
        if (hit < 0x1771) {
            if (hit >= 0x7d1 || std::abs(s.i32(0x188)) >= 0x3e9) {
                sound(SoundEvent((random() & 1) + 3), s);
                sound(SoundEvent(random() & 1), s);
            }
        } else {
            sound(SoundEvent((random() & 1) + 4), s);
            sound(SoundEvent(random() & 1), s);
        }
    }
    const int32_t magnitude = (std::abs(s.i32(0x184)) + std::abs(s.i32(0x188)) +
                               std::abs(s.i32(0x18c))) >> 10;
    // Class 1 (hazard short): knock_off reason 1; on survival the small-gap
    // branch transfers impulse to speed, the big-gap branch crashes reason 1
    // with the default 0x004052c0 vector pair (0x004049c0 local_18 ladder).
    const bool down = knock_off(self, magnitude, magnitude, 1);
    if (down) {
        return;
    } else if (s.i32(0xec) - 0 < 0x1771) {
        s.i32(0xec) += s.i32(0x184);
    } else {
        crash_reason(self, 1, {-256, 0, 1792}, {0, 0, 1024});
        return;
    }
    // Lateral separation against the immovable slot (0x00436a40 with the
    // static actor mass >> bike mass: the rider takes it all).
    const int32_t centre = (hazard_lo_q8 + hazard_hi_q8) / 2;
    const int32_t depth = (s.i32(0x174) - std::abs(s.i32(0x28) - centre)) + 0x800;
    s.i32(0x28) += s.i32(0x28) < centre ? -(depth + 3) : (depth + 3);
    s.i32(0x184) = 0; s.i32(0x188) = 0; s.i32(0x18c) = 0; s.u8(0x19c) = 0;
    if (s.i32(0xec) > 30000) s.i32(0xec) = 30000;
    if (s.i32(0xec) < -1000) s.i32(0xec) = 0;
}

// Shared knock-off decision 0x004052c0(victim, stamina_damage, bike_damage,
// reason). The grunt/crash sound draws stay upstream in the 0x004049c0 caller
// (shared stream position); this helper performs no RNG.
// Reason table: 0 zeroes both inputs; 1/4/5 keep both; 2 scales both by 7/4
// with C++ truncation toward zero (the assembly adds a sign-bias then shifts
// right arithmetically, which rounds negative dividends up, i.e. matches
// trunc(p*7/4) for p*7 != INT32_MIN); 3 divides both by 8 with the same
// truncation caveat; all other reasons including silent-hit 6 zero both.
// Stamina damage 0x004050c0: max -= 4*p floored at base/2, current -= 64*p
// floored at zero. Bike damage 0x004051e0: +0x2f0 -= p floored at zero.
// Owner/network bypasses: flag -1 (remote) or (flag 1 without local
// ownership) skip both applications and return the untouched +0x2f4 state,
// mirroring the assembly's early-return branches (network decisions from
// 0x00405990/0x004060b0 are not ported; offline play always passes flag 0).
// When either application returns zero, crash entry 0x00403d60 runs with the
// assembly's argument order: rider impulse (local_1c+3) then bike impulse
// (local_1c). Reason 4 passes {-80,0,0}/{0,0,0}; every other reason passes
// {-256,0,1792}/{0,0,1024}. Reason 4 without a crash selects riding
// animation 9 on the victim's +0x70 block (recorded on Rider for the render
// owner; the Rider object has no animation block of its own). Reason 5
// multiplies bike+0x188 by the scaled stamina damage; when that field is
// nonzero and the victim is still riding (+0x2f4 == 0), steer +0x288 gains
// trunc(+0x2d8 * +0x188 / 256) computed in original-width 32-bit wrap
// before truncation, then clamps to +-+0x29c.
bool Race::knock_off(Rider& victim, int32_t stamina_damage, int32_t bike_damage,
                     int32_t reason, bool owner_local, int32_t owner_flag,
                     std::optional<std::array<int32_t,3>> rider_impulse,
                     std::optional<std::array<int32_t,3>> bike_impulse) {
    int32_t p = stamina_damage, q = bike_damage;
    if (reason == 0) { p = 0; q = 0; }
    else if (reason == 1 || reason == 4 || reason == 5) { /* keep */ }
    else if (reason == 2) {
        // 0x004052c0: (p*7 + ((p*7 >> 31) & 3)) >> 2, i.e. sar-rounded /4.
        const auto scale74 = [](int32_t v) {
            const uint32_t prod = uint32_t(v) * 7u;
            const int32_t p7 = prod <= uint32_t(INT32_MAX) ? int32_t(prod)
                                                          : int32_t(int64_t(prod) - 0x100000000LL);
            return (p7 + ((p7 >> 31) & 3)) >> 2;
        };
        p = scale74(p); q = scale74(q);
    } else if (reason == 3) {
        // 0x004052c0: (p + ((p >> 31) & 7)) >> 3, i.e. sar-rounded /8.
        p = (p + ((p >> 31) & 7)) >> 3; q = (q + ((q >> 31) & 7)) >> 3;
    } else { p = 0; q = 0; }
    const bool bypass = owner_flag == -1 || (owner_flag == 1 && !owner_local);
    if (!bypass) {
        // 0x004050c0 (local/offline branch): products in int64 then narrowed,
        // matching the assembly's 32-bit imul before the add.
        RiderStats& st = victim.stamina;
        Bike& b = victim.bike;
        st.max = int32_t(int64_t(st.max) + int64_t(p) * -4);
        const int32_t floor = st.base / 2;   // truncating divide, as original
        if (st.max < floor) st.max = floor;
        st.current = int32_t(int64_t(st.current) + int64_t(p) * -0x40);
        if (st.current < 1) st.current = 0;
        // 0x004051e0 (local/offline branch).
        b.i32(0x2f0) -= q;
        if (b.i32(0x2f0) < 1) b.i32(0x2f0) = 0;
    }
    if (victim.stamina.current == 0 || victim.bike.i32(0x2f0) == 0) {
        // 0x00403d60 with the reason-4 vectors or the default vectors,
        // assembly argument order preserved (rider impulse, then bike).
        if (!rider_impulse)
            rider_impulse = reason == 4 ? std::array<int32_t,3>{-80, 0, 0}
                                        : std::array<int32_t,3>{-256, 0, 1792};
        if (!bike_impulse)
            bike_impulse = reason == 4 ? std::array<int32_t,3>{0, 0, 0}
                                       : std::array<int32_t,3>{0, 0, 1024};
        crash_reason(victim, reason, *rider_impulse, *bike_impulse);
    }
    if (reason == 4 && victim.bike.i32(0x2f4) == 0) victim.contact_animation = 9;
    else if (reason == 5) {
        // 0x004052c0: original-width 32-bit wraparound product.
        const uint32_t prod = uint32_t(victim.bike.i32(0x188)) * uint32_t(p);
        victim.bike.i32(0x188) = prod <= uint32_t(INT32_MAX) ? int32_t(prod)
                                                             : int32_t(int64_t(prod) - 0x100000000LL);
    }
    if (victim.bike.i32(0x188) != 0 && victim.bike.i32(0x2f4) == 0) {
        // Steering path shared by every reason (0x004052c0 tail): original
        // widths first (+0x2d8 * +0x188 wraps 32-bit), bias +sar-round /256.
        const uint32_t raw = uint32_t(victim.bike.i32(0x2d8)) * uint32_t(victim.bike.i32(0x188));
        const int32_t wide = raw <= uint32_t(INT32_MAX) ? int32_t(raw)
                                                        : int32_t(int64_t(raw) - 0x100000000LL);
        const int32_t add = (wide + ((wide >> 31) & 0xff)) >> 8;
        const int32_t next = int32_t(uint32_t(victim.bike.i32(0x288)) + uint32_t(add));
        const int32_t lim = victim.bike.i32(0x29c);
        victim.bike.i32(0x288) = (lim < next || (-lim != -next && next <= -lim)) ? (lim < next ? lim : -lim) : next;
    }
    return victim.bike.i32(0x2f4) != 0;
}

// Bike side of the contact handler 0x004049c0 against a car (+0xc4 == 0x10).
// Cars are tall (+0x17c = 8960 >= 0x2000), so the hit is class 2 when the
// car's width contains the rider (+0x19c bit 0x40, set by collide_traffic
// from the 0x00436a40 prologue rule), otherwise class 3. Class 1 needs a
// short other object and never fires for cars. The knock-off decision below
// is the shared 0x004052c0 helper; the grunt/crash sound draws above stay
// upstream in this caller, preserving the shared-stream position.
void Race::contact_car(Rider& self, TrafficCar& car) {
    Bike& s = self.bike;
    const int32_t elasticity = s.i32(0x180);
    s.i32(0x188) = div256(int64_t(s.i32(0x188)) * elasticity);
    s.i32(0x184) = div256(int64_t(s.i32(0x184)) * elasticity);
    const int32_t hit = std::abs(s.i32(0x184));
    // Player sounds (shared with the bike-bike head): silent below both
    // thresholds (0x004049c0 jumps to LAB_00404c2d with iVar9 = 6).
    bool silent = false;
    if (&self == &riders_[0]) {
        if (hit < 0x1771) {
            if (hit >= 0x7d1 || std::abs(s.i32(0x188)) >= 0x3e9) {
                sound(SoundEvent((random() & 1) + 3), s);
                sound(SoundEvent(random() & 1), s);
            } else {
                silent = true;
            }
        } else {
            sound(SoundEvent((random() & 1) + 4), s);
            sound(SoundEvent((random() & 1) + 1), s);
        }
    } else if (hit < 0x7d1 && std::abs(s.i32(0x188)) < 0x3e9) {
        // The scratch-bike fixture path: the original's silent goto fires
        // regardless of player identity (the player gate only guards the
        // sound draws, not the branch).
        silent = true;
    }
    const int32_t old_steer_rate = s.i32(0xf0);
    const bool contained = (s.u8(0x19c) & 0x40) != 0;
    const int32_t magnitude = (std::abs(s.i32(0x184)) + std::abs(s.i32(0x188)) +
                               std::abs(s.i32(0x18c))) >> 10;
    // Silent hits (iVar9 = 6): the knock-off decision zeroes its params
    // (0x004052c0 default case: no damage, returns false) and the handler
    // runs the lateral branch below with local_20 = steer rate.
    if (silent) {
        if (car.lateral < s.i32(0x28)) {
            if (s.i32(0xf0) < 0) s.i32(0xf0) = 0;
            s.i32(0xf0) += hit / 2;
            s.i32(0xec) = std::max(0, s.i32(0xec) - div_pow2(hit, 2));
        } else {
            if (s.i32(0xf0) > 0) s.i32(0xf0) = 0;
            s.i32(0xf0) -= hit / 2;
            s.i32(0xec) = std::max(0, s.i32(0xec) - div_pow2(hit, 2));
        }
    } else if (!contained) {
        // Class 3: glance off the car's side, with the 0x004052c0 decision.
        // 0x004049c0: other lateral < self lateral shoves +f0 (away from the
        // car), else -f0; speed loses |184|/4 either way. The car caller has
        // +0xc4 & 0xc != 0, so the assembly passes stamina local_20 and bike
        // damage (local_20 + (iVar8>>31 & 15)) >> 4, where iVar8 is the
        // pre-shift magnitude sum (|184|+|188|+|18c| before >>10); both scale
        // by /8 inside the helper.
        const int32_t full = std::abs(s.i32(0x184)) + std::abs(s.i32(0x188)) + std::abs(s.i32(0x18c));
        const int32_t swing = (magnitude + ((full >> 31) & 15)) >> 4;
        const bool down = knock_off(self, magnitude, swing, 3);
        if (down) {
            s.i32(0xf4) += div_pow2(hit, 8);
        } else if (car.lateral < s.i32(0x28)) {
            if (s.i32(0xf0) < 0) s.i32(0xf0) = 0;
            s.i32(0xf0) += hit / 2;
            s.i32(0xec) = std::max(0, s.i32(0xec) - div_pow2(hit, 2));
        } else {
            if (s.i32(0xf0) > 0) s.i32(0xf0) = 0;
            s.i32(0xf0) -= hit / 2;
            s.i32(0xec) = std::max(0, s.i32(0xec) - div_pow2(hit, 2));
        }
    } else if (const bool down = knock_off(self, magnitude, magnitude, 2); down) {
        // Class 2 and the rider went down in the decision: the impulse still
        // transfers to speed (0x004049c0).
        s.i32(0xec) += s.i32(0x184);
    } else if (s.i32(0xec) - car.speed < 0x1771) {
        // Class 2, small speed gap: rear-end pushes speed over.
        s.i32(0xec) += s.i32(0x184);
    } else {
        // Class 2, big speed gap: knocked off with reason 2 (animation 29).
        crash_car(self);
        return;
    }
    if (s.i32(0x2f4) != 0) return;
    // Scenery-marking block (0x004049c0 iVar9 == 0 path): when local_18[3]
    // (speed at entry) exceeds 1000 and the rider's sample differs from the
    // car's +0x1ec sample, mark +0x298 by the car's +0x124 sub-kind, record
    // the sample/time/flags, zero the steer rate and grow +0x288 by half.
    // Cars never take this path in the golden (iVar9 is 2/3/6: the
    // else-if/else ladder above), so it is documented, not executed.
    // Nudge apart sideways (0x00404f4d).
    const int32_t push = div_pow2(s.i32(0x174) + 16384, 4) * s.i32(0x18);
    s.i32(0x28) += car.lateral < s.i32(0x28) ? push : -push;
    // Tail (0x00404feb): impulses and flags clear; speed/velocity clamps.
    s.i32(0x184) = 0; s.i32(0x188) = 0; s.i32(0x18c) = 0; s.u8(0x19c) = 0;
    if (s.i32(0xec) > 30000) s.i32(0xec) = 30000;
    if (s.i32(0xec) < -1000) s.i32(0xec) = 0;
    if (s.i32(0xf4) > 0x600) s.i32(0xf4) = 0x600;
    if (s.i32(0xf4) < -10000) s.i32(0xf4) = -10000;
    // Steering kick from the steer-rate change (0x00404f93 tail).
    if (old_steer_rate != s.i32(0xf0)) {
        const int32_t kick =
            div256(int64_t(s.i32(0x2d8)) * (s.i32(0xf0) - old_steer_rate) * 2);
        const int32_t lim = s.i32(0x29c);
        const int32_t next = s.i32(0x288) + kick;
        s.i32(0x288) = std::max(-lim, std::min(lim, next));
    }
}

// Crash entries for contact 0x00403d60. crash_car keeps the reason-2 car path
// (animation 29); crash_reason carries any reason with the 0x004052c0 vector
// pair in the assembly's argument order (rider impulse, then bike impulse).
void Race::crash_car(Rider& r) {
    crash_reason(r, 2, {-80 - 176, 0, 1792}, {0, 0, 1024});
}
void Race::crash_reason(Rider& r, int32_t reason, std::array<int32_t,3> rider_impulse,
                        std::array<int32_t,3> bike_impulse) {
    if (crash_library_) {
        if (r.crash_active) return;
        r.bike.i32(0x2f4) = 0;
        begin_crash(r.bike, r.crash_actor, crash_config_, crash_library_->playback[0],
                    crash_library_->fallback, uint32_t(ticks_), reason, rider_impulse, bike_impulse);
        r.crash_active = true; r.crash_timer = 0;
        r.crash_actor.route = r.route; r.crash_actor.sample = r.sample;
        r.crash_actor.previous_sample = r.previous_sample;
        return;
    }
    // Stand-in (no crash library): the shared helper already computed the
    // vector pair for the golden path; the reason lands in +0x2f8.
    (void)rider_impulse; (void)bike_impulse;
    r.bike.i32(0x2f4) = 1;
    r.bike.i32(0x2f8) = reason;
    r.bike.u8(0xc0) |= 0x10;
    r.crash_timer = 0;
}

void Race::contact(Rider& self, Rider& other) {
    // Bike-against-bike branch of the contact handler 0x004049c0: the
    // knock-off decision 0x004052c0 runs on contained hits (reason 2 here;
    // class 1 needs a short other object, +0x17c < 0x2800, which bikes never
    // satisfy: bike +0x17c = 0x2300 tall, so the reason-1 branch is dead for
    // bike-bike). Bikes are tall (>= 0x2000), so the hit is class 2 when the
    // other bike's width contains ours (+0x19c bit 0x40), otherwise class 3.
    Bike& s = self.bike; const Bike& o = other.bike;
    if (s.i32(0x2f4) != 0) return;
    const int32_t elasticity = s.i32(0x180);
    s.i32(0x188) = div256(int64_t(s.i32(0x188)) * elasticity);
    s.i32(0x184) = div256(int64_t(s.i32(0x184)) * elasticity);
    const int32_t hit = std::abs(s.i32(0x184));
    if (&self == &riders_[0]) {
        // 0x004049c0: the player grunts and the contact makes a crash sound by its strength.
        if (hit < 0x1771) {
            if (hit >= 0x7d1 || std::abs(s.i32(0x188)) >= 0x3e9) {
                if (s.i32(0x2f4) == 0) sound(SoundEvent((random() & 1) + 3), s);
                sound(SoundEvent(random() & 1), s);
            }
        } else {
            if (s.i32(0x2f4) == 0) sound(SoundEvent((random() & 1) + 4), s);
            sound(SoundEvent((random() & 1) + 1), s);
        }
    }
    const bool contained = (o.i32(0x28) - o.i32(0x174) < s.i32(0x28) - s.i32(0x174) / 2) &&
                           (s.i32(0x28) + s.i32(0x174) / 2 < o.i32(0x28) + o.i32(0x174));
    // 0x004049c0 class routing for two bikes: contained hits run the 0x004052c0
    // decision with reason 2 (class 1 is dead for bike-bike: it needs the
    // other object short, +0x17c < 0x2800, but bikes are 0x2300 tall).
    // Glancing (class-3) hits run the lateral branch below.
    const int32_t magnitude = (std::abs(s.i32(0x184)) + std::abs(s.i32(0x188)) +
                               std::abs(s.i32(0x18c))) >> 10;
    if (contained) {
        const bool down = knock_off(self, magnitude, magnitude, 2);
        if (down) {
            s.i32(0xec) += s.i32(0x184);
        } else if (s.i32(0xec) - o.i32(0x108) < 0x1771) {
            s.i32(0xec) += s.i32(0x184);
        } else {
            crash_reason(self, 2, {-256, 0, 1792}, {0, 0, 1024});
            return;
        }
    } else if (o.i32(0x28) < s.i32(0x28)) {
        if (s.i32(0xf0) < 0) s.i32(0xf0) = 0;
        s.i32(0xf0) += hit / 2;
        s.i32(0xec) = std::max(0, s.i32(0xec) - div_pow2(hit, 2));
    } else {
        if (s.i32(0xf0) > 0) s.i32(0xf0) = 0;
        s.i32(0xf0) -= hit / 2;
        s.i32(0xec) = std::max(0, s.i32(0xec) - div_pow2(hit, 2));
    }
    // Nudge apart sideways (0x00404f4d).
    const int32_t push = div_pow2(s.i32(0x174) + o.i32(0x174), 4) * s.i32(0x18);
    s.i32(0x28) += o.i32(0x28) < s.i32(0x28) ? push : -push;
}

void Race::move(Rider& r, bool player) {
    // 0x004395e0 (single player, no network ghost branch).
    Bike& b = r.bike;
    if (b.i32(0x160) > 0) b.i32(0x104) = b.i32(0xf0);
    b.i32(0x108) = b.i32(0xec);
    b.i32(0x10c) = b.i32(0x104);
    b.i32(0x110) = 0;
    int32_t effective = b.i32(0x108), lateral = b.i32(0x104), forward;
    if (effective < 0) { lateral = -lateral; forward = effective * -4; } else forward = effective * 4;
    int32_t yaw = wrap_angle(approx_atan2(forward, lateral) * 65536);
    const int32_t yaw_gain = (b.u8(0x278) & 4) ? b.i32(700) : b.i32(0x2b8);
    yaw = div256(yaw_gain) * yaw;
    if (b.u8(0x278) & 1) yaw = yaw < 1 ? -0x3f0000 : 0x3f0000;
    if (b.i32(0xec) < 4000) { if (yaw > 0x180000) yaw = 0x180000; if (yaw < -0x180000) yaw = -0x180000; }
    b.i32(0x5c) = yaw;
    const int32_t dt = b.i32(0x18);
    const int32_t distance = ((b.i32(0x108) * 2 - b.accel * dt) * dt) >> 1;
    for (int k = 0; k < 5; ++k) b.i32(0x30 + 4*k) = b.i32(0x1c + 4*k);   // previous position/height copy
    move_along(r, div256(distance));
    int32_t heading = 0;
    const int32_t bend = b.i32(0x130);
    if (bend != 0) {
        int32_t push;
        if (!(b.u8(0x278) & 4) || int8_t(b.u8(0x164)) < 0x33) push = bend * 400 / dt;
        else if (player) push = bend * 800 / dt;
        else push = bend * 600 / dt;
        b.i32(0x10c) += div256(push);
        heading = approx_atan2(distance, div256(int64_t(bend) * 400)) * 65536;
    }
    if (heading > 0x7fffff) heading -= 0x1000000;
    b.i32(0x128) = heading;
    if (b.i32(0xec) > 300) b.i32(0x28) += dt * b.i32(0x10c);
}

void Race::walls(Rider& r) {
    // Simplified stand-in for 0x0043a610: stop at the edge of the shoulder, crash when fast.
    Bike& b = r.bike;
    const SliceEdges e = edges(r, r.sample);
    const int32_t x = b.i32(0x28) >> 8;
    if (x > e.left_shoulder && x < e.right_shoulder) return;
    b.i32(0x28) = (x >= e.right_shoulder ? e.right_shoulder - 1 : e.left_shoulder + 1) * 256;
    b.i32(0x10c) = 0; b.i32(0x104) = 0; b.i32(0xf0) = 0; b.i32(0x288) = 0;
    if (b.i32(0xec) > 4000) {
        if (&r == &riders_[0]) sound(SoundEvent::grunt_c, b);   // 0x0043a610
        if(b.i32(0xc4)==4)crash(r);
        else b.i32(0xec)=b.i32(0xec)*3/4;
    } else b.i32(0xec) = b.i32(0xec) * 3 / 4;
}

void Race::vertical(Rider& r) {
    // 0x0043a190 with imported terrain ground height from 0x00439b10.
    Bike& b = r.bike;
    const SliceEdges e = edges(r, r.sample);
    const auto& s = route_of(r)[r.sample].path;
    const int32_t x = b.i32(0x28) >> 8;
    int32_t ground = 0;
    if (x < e.left || x > e.right) {
        if (x > e.right) ground = x >= e.right_shoulder ? ground_wall : s.height_right;
        else ground = x <= e.left_shoulder ? ground_wall : s.height_left;
    }
    if(terrain_source_) {
        const auto& route=route_of(r);const auto& current=route[r.sample];const auto& next=route[std::min(r.sample+1,route.size()-1)];
        ground=terrain_height(terrain_source_(current),current.path,terrain_source_(next),next.path,b.i32(0x28)-current.offset*256,uint8_t(b.i32(0x1c)),next.offset-current.offset);
    }
    b.i32(0x24) = ground;
    const int32_t dt = b.i32(0x18);
    int32_t ground_move = (ground - b.i32(0x38)) * 0x100 + b.i32(0x12c);
    const int32_t above = (b.i32(0xf4) * dt - ground_move) + b.i32(0x20);
    int32_t height_above = above * 100 / 99;
    const int32_t vy = b.i32(0x11c) * dt + b.i32(0xf4);
    const int32_t above_before = b.i32(0x34);
    b.i32(0xf4) = vy;
    const int32_t threshold = (dt * 4 - 3) * b.i32(0x16c);
    const int32_t rise_now = s.rise, rise_before = route_of(r)[r.previous_sample].path.rise;
    bool grounded = false;
    if (threshold < above) {
        b.u8(0x278) |= 0x20;
        if (threshold - above_before != 0 && above_before <= threshold) {
            const int32_t cap = b.i32(0x1b4) * b.i32(0x1b8);
            if (cap < vy) b.i32(0xf4) = cap;
        }
        b.i32(0x160) = 0;
        b.i32(0x11c) = -b.i32(0x1b4);
    } else if (threshold < above_before && vy < b.i32(0x168)) {
        // Landing.
        if(b.i32(0xc4)==4) {
        int32_t wobble = ((std::abs(vy) * b.i32(0x288)) >> 12) + 1;
        const int32_t lim = b.i32(0x29c) * 2;
        if (wobble > lim) wobble = lim; else if (wobble < -lim) wobble = -lim;
        b.i32(0x288) = wobble;
        }
        const int32_t rebound = -div256(int64_t(b.i32(0x170)) * vy);
        const int32_t k = b.i32(0xec) < 0 ? -5 - rise_now : rise_now + 5;
        b.i32(0xf4) = k * (b.i32(0xec) >> 7) + rebound;
        b.i32(0x11c) = b.i32(0x1b4) * -2;
        b.i32(0xfc) = -0x1000;
        b.i32(0x160) = tables_.contact_base;
        height_above = 0;
        if (&r == &riders_[0]) sound(SoundEvent::bounce, b);   // 0x0043a190
    } else {
        grounded = true;
    }
    if (grounded) {
        if (threshold < above_before) b.u8(0x164) = 1;
        if (dt != 1) ground_move = dt == 16 ? ground_move >> 4 : ground_move / dt;
        b.i32(0xf4) = ground_move;
        int32_t c = div256(int64_t(rise_now - rise_before) * b.i32(0xec));
        c = div_pow2(c, 3) + tables_.contact_base;
        b.i32(0x160) = c < 0 ? 0 : c;
        height_above = 0;
    }
    b.i32(0x20) = height_above;
    int32_t pitch = div_pow2(height_above - b.i32(0xfc), 6) + b.i32(0xfc);
    if (pitch > 0x2000) pitch = 0x2000;
    if (pitch < -0x2000) pitch = -0x2000;
    b.i32(0xfc) = pitch;
    b.i32(0x100) = pitch - height_above;
}

void Race::crash(Rider& r) {
    if(crash_library_) {
        if(r.crash_active)return;
        // traction currently raises the crash flag directly; entry applies once.
        r.bike.i32(0x2f4)=0;
        begin_crash(r.bike,r.crash_actor,crash_config_,crash_library_->playback[0],crash_library_->fallback,uint32_t(ticks_),0);
        r.crash_active=true;r.crash_timer=0;
        r.crash_actor.route=r.route;r.crash_actor.sample=r.sample;r.crash_actor.previous_sample=r.previous_sample;
        return;
    }
    // Simplified stand-in for 0x00403d60: fall, slide to a stop, then remount on the road.
    r.bike.i32(0x2f4) = 1;
    r.bike.u8(0xc0) |= 0x10;
    r.crash_timer = 0;
}

void Race::step_crash(Rider& r,bool player,uint32_t controls) {
    auto& bike=r.bike;auto& body=r.crash_actor;const auto& library=*crash_library_;
    auto motion=[&](Rider& actor) {
        auto& b=actor.bike;b.i32(0x18)=1;
        if(!b.i32(0x160)){b.i32(0x114)=b.i32(0x118)=0;b.u8(0x164)=0;}
        const int surface=surface_class(edges(actor,actor.sample),b.i32(0x28));
        integrate_speed(b,tables_,surface);
        advance_actor_motion(b,tables_,[&](int32_t distance){move_along(actor,distance);});
        walls(actor);vertical(actor);
    };
    motion(r);
    Rider detached;detached.bike=body.motion;detached.route=body.route;detached.sample=body.sample;detached.previous_sample=body.previous_sample;
    motion(detached);body.motion=detached.bike;body.route=detached.route;body.sample=detached.sample;body.previous_sample=detached.previous_sample;
    CrashRecoveryInput recovery;recovery.player=player;recovery.state_elapsed=r.crash_timer;
    if(player) {
        if(controls&control_accelerate)recovery.walk_controls|=0x800;
        if(controls&control_brake)recovery.walk_controls|=0x1000;
        if(controls&control_left)recovery.walk_controls|=0x4000;
        if(controls&control_right)recovery.walk_controls|=0x2000;
    }
    const auto action=recover_crash_actor(bike,body,library.playback[0],library.fallback,uint32_t(ticks_),recovery);
    r.crash_timer=recovery.state_elapsed;
    if(action==CrashRecoveryAction::remount) {
        remount_crash_actor(bike,r.crash_bike_animation,body,crash_config_,library.playback[0],library.fallback,uint32_t(ticks_));
        r.stamina.current=r.stamina.max;r.crash_active=false;return;
    }
    if(action==CrashRecoveryAction::wrecked)bike.i32(0x2f0)=0;
    advance_crash_bike_animation(r.crash_bike_animation,library.playback[0],library.fallback,bike.i32(0xec),uint32_t(ticks_),[&]{return random();});
    CrashRiderAnimationInput input;input.crash=bike.i32(0x2f4);input.speed=body.motion.i32(0xec);input.height_q8=body.motion.i32(0x20);
    advance_crash_rider_animation(body.animation,library.playback[0],library.fallback,input,uint32_t(ticks_),[&]{return random();});
    if(bike.i32(0x2f4)!=input.crash)r.crash_timer=0;
    bike.i32(0x2f4)=input.crash;if(input.crash==2)++r.crash_timer;
}

int32_t Race::curvature(const Rider& r, size_t sample, int samples) const {
    // 0x00449b40: average of sub-slice +0xb4 (atan(bend) << 14, 0x00454240) over the next samples.
    const auto& route = route_of(r);
    int64_t sum = 0;
    for (int k = 0; k < samples && sample + size_t(k) < route.size(); ++k)
        sum += wrap_angle(atan_ratio(tables_, route[sample + size_t(k)].path.bend) * (1 << 14));
    return int32_t(sum / samples);
}

// Ring scans for the AI replan (0x00408cd0 via 0x00449ce0). The original walks
// scheduler links in position order from the rider's own half node, skipping
// riders whose lateral sits outside their slice bounds. Returns a riders_
// index or -1. Only bike/half pairs participate (halves carry the scan state).
int Race::ai_scan_replan(const Rider& r, bool forward, int32_t cap) const {
    // 0x00408cd0: from the rider's half, walk rider halves (mask 8) with
    // 0x00449ce0 until one lies strictly inside its slice's road edges
    // (+0xe0/+0xe4); return that half's bike (+0x1c4). The police rider
    // (DAT_004642dc) skips the edge test; cops are not riders here.
    build_scan_list();
    int at = scan_find(int(&r - riders_.data()), 8);
    for (;;) {
        at = scan_next(at, 8, forward, cap);
        if (at < 0) return -1;
        const ScanObject& o = scan_[size_t(at)];
        const Rider& q = riders_[size_t(o.rider)];
        const SliceEdges e = edges(q, q.sample);
        const int32_t x = o.lateral >> 8;
        if (e.left < x && x < e.right) return o.rider;
    }
}

void Race::build_scan_list() const {
    // Rider bikes then halves (0x00405990 creates bike then half), riders in
    // build order (player first, 0x00405600). Pedestrians carry their own
    // creation order: ring-setup spawns precede the riders.
    scan_.clear();
    for (size_t i = 0; i < riders_.size(); ++i) {
        const Rider& r = riders_[i];
        ScanObject bike;
        bike.position = r.bike.i32(0x1c);
        bike.lateral = r.bike.i32(0x28);
        bike.speed = r.bike.i32(0x108);
        bike.type = 4;
        bike.order = 0x10000000u + uint32_t(i) * 2;
        bike.rider = int(i);
        bike.sample = &route_of(r)[std::min(r.sample, route_of(r).size() - 1)];
        scan_.push_back(bike);
        ScanObject half = bike;
        half.lateral = half_x(r);
        half.type = 8;
        half.order = bike.order + 1;
        scan_.push_back(half);
    }
    if (pedestrians_) {
        const auto& pool = pedestrians_->pool();
        for (size_t i = 0; i < pool.size(); ++i) {
            const auto& p = pool[i];
            if (!p.active) continue;
            ScanObject o;
            o.position = p.body.bike.i32(0x1c);
            o.lateral = p.body.bike.i32(0x28);
            o.speed = p.body.bike.i32(0x108);
            o.type = 0x20;
            o.order = p.order;
            o.pedestrian = int(i);
            const auto& road = route_of(p.body);
            o.sample = &road[std::min(p.body.sample, road.size() - 1)];
            scan_.push_back(o);
        }
    }
    if (traffic_) {
        const auto& road = route_of(riders_[0]);
        const auto& cars = traffic_->cars();
        for (size_t i = 0; i < cars.size(); ++i) {
            const auto& c = cars[i];
            if (!c.active) continue;
            ScanObject o;
            o.position = c.position;
            o.lateral = c.lateral;
            o.type = 1;
            o.order = 0x80000000u + c.serial;
            o.car = int(i);
            o.sample = &road[std::min(size_t(std::max(c.position, 0) >> 8), road.size() - 1)];
            scan_.push_back(o);
        }
    }
    std::stable_sort(scan_.begin(), scan_.end(), [](const ScanObject& a, const ScanObject& b) {
        return a.position != b.position ? a.position < b.position : a.order < b.order;
    });
}

int Race::scan_find(int rider, uint32_t type) const {
    for (size_t k = 0; k < scan_.size(); ++k)
        if (scan_[k].rider == rider && scan_[k].type == type) return int(k);
    return -1;
}

int Race::scan_next(int from, uint32_t mask, bool forward, int32_t window) const {
    // 0x00449ce0: the next object along the list in that direction whose type
    // matches the mask; the walk stops (null) at the first object window or
    // more away from the starting object, matching or not. Single-road
    // slices all carry road flags 7, so the +0x36 half test always passes.
    if (from < 0) return -1;
    if (window < 0) window = 0x20000000;
    const ScanObject& f = scan_[size_t(from)];
    if (forward) {
        for (size_t k = size_t(from) + 1; k < scan_.size(); ++k) {
            if (window <= scan_[k].position - f.position) return -1;
            if (scan_[k].type & mask) return int(k);
        }
    } else {
        for (int k = from - 1; k >= 0; --k) {
            if (window <= f.position - scan_[size_t(k)].position) return -1;
            if (scan_[size_t(k)].type & mask) return k;
        }
    }
    return -1;
}

// 0x0044a110: object size by type.
static int32_t object_size(uint32_t type) {
    switch (type) {
    case 1: case 2: return 0x2000;
    case 4: case 8: case 0x20: return 0x3000;
    case 0x10: return 0x8000;
    default: return 0x1000;
    }
}

// 0x00409020: slow for the curvature 8 samples ahead (4-sample average).
int32_t Race::ai_curve_limit(const Rider& r) const {
    const size_t at = std::min(r.sample + 8, route_of(r).size() - 1);
    const int32_t c = std::abs(curvature(r, at, 4));
    const int32_t base = r.brain.current_speed;
    const int32_t q = base - div_pow2(base, 2);
    if (c > 0x30000) {
        const int32_t half = q / 2;
        return (q - (int32_t(((int64_t(half) << 8) / 0x600) * (c >> 8)) >> 8)) + half;
    }
    return base;
}

// 0x004082f0: every 60 ticks ease the rating table, pick neighbours, rate the
// overtake steps, keep the nearer target and reset the speed from the level
// tables with the rubber band / fixed cases.
void Race::ai_replan(Rider& r) {
    AiState& ai = r.brain;
    // Ease the rating table once per 60 ticks banked (the police rider's
    // +0x1a4 decay applies to DAT_004642dc only).
    auto ease = [](int32_t& cur, int32_t target, int32_t step) {
        if (cur == target) return;
        if (target < cur) { cur -= step; if (cur < target) cur = target; }
        else { cur += step; if (target < cur) cur = target; }
    };
    while (ai.replan_ticks > 0x3b) {
        for (int k = 0; k < 10; ++k)
            for (int j = 0; j < 3; ++j)
                ease(ai.rating[size_t(k * 3 + j)], ai.rating_tgt[size_t(k * 3 + j)], ai.ease[size_t(j)]);
        ai.replan_ticks -= 0x3c;
    }
    const int32_t cap = 0xa00;   // 0x5000 only for the police rider
    const int ahead = ai_scan_replan(r, true, cap);
    ai.nbr_ahead = ahead;
    const int behind = ai_scan_replan(r, false, cap);
    ai.nbr_behind = behind;
    // Player half +0x1fc is the player's 1-based place; inside the top three
    // the overtake thresholds drop.
    const int place = this->place(0);
    const int32_t near = place < 4 ? 4 - place : 0;
    auto rate = [&](int nbr) -> int {
        if (nbr < 0) return 0;
        const int idx = riders_[size_t(nbr)].brain.overtake_index;   // neighbour brain +0x165
        int32_t v = 0;
        if (idx < 10) v = (ai.rating[size_t(idx * 3 + 1)] - ai.rating[size_t(idx * 3)]) + ai.rating[size_t(idx * 3 + 2)];
        if (v < (0x10 - near) * 1000 - level_ * 0x5dc) return 1;
        return ((0x10 - near) * 2000 - level_ * 3000 <= v) + 2;
    };
    ai.ov_ahead = rate(ahead);
    ai.ov_behind = rate(behind);
    int pick = ahead, step = ai.ov_ahead;
    if (ai.ov_ahead <= ai.ov_behind) {
        pick = behind; step = ai.ov_behind;
        if (ai.ov_behind <= ai.ov_ahead) {   // equal ratings: present one, else the nearer
            if (behind < 0) {
                pick = ahead; step = ai.ov_ahead;
                if (ahead < 0) { pick = -1; step = 0; }
            } else if (ahead >= 0) {
                const int32_t self = r.bike.i32(0x1c);
                const int32_t db = std::abs(self - riders_[size_t(behind)].bike.i32(0x1c));
                const int32_t da = std::abs(self - riders_[size_t(ahead)].bike.i32(0x1c));
                if (da <= db) { pick = ahead; step = ai.ov_ahead; }
            }
        }
    }
    ai.target_rider = pick;
    ai.target_step = step;
    if (level_ >= 0 && level_ < 5) {
        ai.current_speed = ai.spd_b[size_t(level_)];
        ai.current2 = ai.spd_a[size_t(level_)];
    }
    const Rider& player = riders_[0];
    const int32_t self = r.bike.i32(0x1c);
    if (ahead == 0 || behind == 0) {
        int32_t d = (self - player.bike.i32(0x1c)) >> 8;
        if (d < -9) d = -9; else if (d > 9) d = 10;
        ai.current_speed = ((tables_.rubber_band[size_t(d + 9)] + 100) * ai.current_speed) / 100;
    } else if (pick < 0 || step != 3) {
        const int32_t v = ai.current_speed;
        switch ((ahead < 0 ? 0 : 2) | (behind >= 0 ? 1 : 0)) {
        case 0:
            if (self <= player.bike.i32(0x1c)) ai.current_speed = div_pow2(v, 2) + v;
            else ai.current_speed = ((level_ + 0x2c) * v * 2) / 100;
            break;
        case 1: ai.current_speed = div_pow2(v, 4) + v; break;
        case 2: ai.current_speed = ((level_ + 0x5e) * v) / 100; break;
        default:
            if (self <= player.bike.i32(0x1c)) ai.current_speed = div_pow2(v, 5) + v;
            else ai.current_speed = (((level_ - 6) / 2 + 100) * v) / 100;
            break;
        }
    } else {
        ai.current_speed = div_pow2(ai.current_speed, 3) + ai.current_speed;
    }
    if (player.bike.i32(0x2f4) != 0 && std::abs(player.bike.i32(0x1c) - self) > 0xa00) {
        if (level_ == 3) ai.current_speed = div_pow2(int64_t(ai.current_speed) * 3, 2);
        else if (level_ != 4) ai.current_speed /= 2;
    }
}

// 0x00409510: nonzero while the rider plays an attack animation (weapon and
// variant tables 0x004644f8/0x00464508/0x00464518/0x00464528 against the anim
// code at rider_half+0x80). Native tracks attacks by timer: exact while no
// attack runs, which covers the verified solo path.
int Race::ai_attack_anim(const Rider& r) const {
    return r.attack.timer != 0 ? 1 : 0;
}

// 0x004092b0: true while the rider is fully behind its target.
bool Race::ai_behind_target(const Rider& r) const {
    if (r.brain.target_rider < 0) return false;
    const Rider& t = riders_[size_t(r.brain.target_rider)];
    return r.bike.i32(0x1c) + r.bike.i32(0x178) < t.bike.i32(0x1c) - t.bike.i32(0x178);
}

// 0x004092e0: overtake eligibility against the target (position and lateral
// reach scaled by brain+0x1d4, then a level-scaled random draw).
bool Race::ai_can_overtake(Rider& r) {
    if (r.brain.target_rider < 0) return false;
    const Rider& t = riders_[size_t(r.brain.target_rider)];
    if (!(uint32_t(r.hit_until) < uint32_t(ticks_) &&
          uint32_t(t.hit_until) < uint32_t(ticks_)))
        return false;
    const int32_t reach = r.brain.reach;
    const int32_t dpos = ((r.bike.i32(0xec) - t.bike.i32(0xec)) >> 4) - t.bike.i32(0x1c) + r.bike.i32(0x1c);
    if (std::abs(dpos) >= reach * 0x10) return false;
    const int32_t dlat = r.bike.i32(0x28) - t.bike.i32(0x28);
    if (std::abs(dlat) >= reach * 0x2000) return false;
    // Level-scaled draw (0x00465a10[level] * dt vs rand&0xff). The +0x7c rating
    // comparison past this needs the cops target (open); without cops the
    // draw decides.
    int32_t table = 0;
    if (level_ >= 0 && level_ < 5 && overtake_eagerness_) table = (*overtake_eagerness_)[size_t(level_)] * r.bike.i32(0x18);
    const uint32_t draw = random() & 0xff;
    if (table - int32_t(draw) == 0 || int32_t(draw) > table) return false;
    return true;
}

// 0x0040a4a0: true when the target is within 0xf0 ahead (adjusted for lengths).
bool Race::ai_fight_range(const Rider& r) const {
    if (r.brain.target_rider < 0) return false;
    const Rider& t = riders_[size_t(r.brain.target_rider)];
    const int32_t ahead_t = t.bike.i32(0x1c) + t.bike.i32(0x178);
    const int32_t self_pos = r.bike.i32(0x1c) - r.bike.i32(0x178);
    if (ahead_t < self_pos) {
        const int32_t gap = self_pos - ahead_t;
        if (gap < 0xf0) {
            const int32_t dlat = r.bike.i32(0x28) - t.bike.i32(0x28);
            return (uint32_t)(dlat < 0 ? -dlat : dlat) >> 8 < 0x80;
        }
    }
    return false;
}

// 0x00408df0: latch a new AI state, resetting the skid byte, the per-state
// timer and the target object when the state actually changes.
static void ai_set_state(AiState& ai, int state, int target) {
    if (ai.state != state) {
        ai.state = state;
        ai.state_flag = 0;
        if (state >= 0 && state < int(ai.state_ticks.size())) ai.state_ticks[size_t(state)] = 0;
        ai.target_rider = target;
    }
}

// 0x00408ad0: per-tick AI state transitions from crash state, target presence,
// proximity and overtake eligibility.
void Race::ai_transition(Rider& r) {
    AiState& ai = r.brain;
    const int32_t crash = r.bike.i32(0x2f4);
    if (crash == 0) {
        if (ai.target_rider < 0) {
            ai_set_state(ai, 1, ai.target_rider);
            return;
        }
        if (ai_attack_anim(r) != 0) return;
        if (ai.target_step == 1) {
            ai_set_state(ai, ai_behind_target(r) ? 2 : 1, ai.target_rider);
            return;
        }
        if (ai.target_step == 2) {
            // Lane index of a rider within its slice: (lateral - centre) in
            // lane units of 0xfa, bounded by the slice lane count.
            auto lane_index = [&](const Rider& q, int32_t& idx) {
                const auto& route = route_of(q);
                const size_t at = std::min(q.sample, route.size() - 1);
                const int32_t centre = route[at].offset * 256;
                const int32_t v = q.bike.i32(0x28) - centre;
                idx = int32_t((v + ((v >> 31) & 0xff)) >> 8) / 0xfa - (v < 0 ? 1 : 0);
                if (idx < 0) return false;
                const int32_t lanes = route[at].path.right_q8 >> 8;
                return idx < lanes;
            };
            // Two independent tests: own lane on the road and 0x004092e0 -> 3;
            // else the target's lane on the road and 0x0040a4a0 -> 5; else 1.
            int32_t self_lane = 0, tgt_lane = 0;
            if (lane_index(r, self_lane) && ai_can_overtake(r)) { ai_set_state(ai, 3, ai.target_rider); return; }
            if (lane_index(riders_[size_t(ai.target_rider)], tgt_lane) && ai_fight_range(r)) { ai_set_state(ai, 5, ai.target_rider); return; }
            ai_set_state(ai, 1, ai.target_rider);
            return;
        }
        if (ai.target_step == 3) {

            if (ai_can_overtake(r)) ai_set_state(ai, 3, ai.target_rider);
            else ai_set_state(ai, 4, ai.target_rider);
            return;
        }
        return;
    }
    if (crash != 2 && crash != 3) ai_set_state(ai, 9, -1);
    else ai_set_state(ai, 8, -1);
}
// 0x004091f0: straight target from the lane preference a slice ahead.
void Race::ai_straight_target(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    const auto& route = route_of(r);
    const size_t next = std::min(r.sample + 1, route.size() - 1);
    const int32_t lanes = route[next].path.right_q8 >> 8;
    int32_t lane = 0;
    if (ai.lane == 1) lane = (lanes - 1) / 2;
    else if (ai.lane == 2) lane = std::abs(lanes - 1);
    ai.target_position = b.i32(0x1c) + 0x3c0;
    ai.target_x = div_pow2(curvature(r, next, 8), 3) + (route[next].offset + lane * 0xfa) * 0x100 + ai.bias;
    ai.target_speed = ai_curve_limit(r);
}

// 0x004090a0: take the inside of the curve when committed to the current
// target (position within 0x3c0 of it); else fall back to the straight target.
void Race::ai_curve_target(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    if ((b.i32(0x1c) >> 8) - (ai.target_position >> 8) != -0x3c0) {
        const auto& route = route_of(r);
        const int32_t c = curvature(r, r.sample, 8);
        const uint32_t mag = uint32_t(c < 0 ? -c : c);
        if (mag < 0x30000) { ai_straight_target(r); return; }
        const int32_t shown = ai_speed_units(b.i32(0xec));
        const int32_t lim = ai_curve_limit(r);
        if (shown < lim / 2) { ai_straight_target(r); return; }
        const auto& s = route[r.sample];
        const int32_t left_edge = s.offset - s.path.left;
        const int32_t half = (s.path.right_q8 / 2) * 0x100;
        uint32_t m = mag > 0x60000 ? 0x60000 : mag;
        int32_t off = int32_t((int64_t((m << 8) / 0x60000) * half) >> 8);
        if (c < 0) off = -off;
        ai.target_position = b.i32(0x1c) + 0x3c0;
        ai.target_x = off + (left_edge + s.path.left_q8) * 0x100 + half;
        ai.target_speed = lim;
        const int32_t x = b.i32(0x28);
        if (c < 1) { if (x < ai.target_x) ai.target_x = x; }
        else if (ai.target_x < x) ai.target_x = x;
    }
}

// 0x00409f70: steering and speed control toward the target point.
void Race::ai_speed_steer(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    const int8_t skid = int8_t(b.u8(0x164));
    const int32_t k = skid < 41 ? 1 : 4;
    const int32_t lead = (-1 - div_pow2(b.i32(0x18), 3)) * k * b.i32(0x288) * 4;
    const int32_t error = (lead - b.i32(0x28)) + ai.target_x;
    uint32_t angle = uint32_t(approx_atan2(ai.target_position - b.i32(0x1c), div256(error)) & 0xff) * 0x10000;
    int32_t steer_angle = int32_t(angle);
    if (steer_angle > 0x7fffff) steer_angle -= 0x1000000;
    if (b.i32(0x160) != 0 || std::abs(b.i32(0x60)) > 0xd) steer(b, tables_, steer_angle * 8, 0);
    const int32_t shown = ai_speed_units(b.i32(0xec)), want = ai.target_speed;
    if (shown < want && (skid < 41 || shown < want / 2)) drive_mode(b, 1);
    else if (shown <= want) drive_mode(b, 0);
    else drive_mode(b, 2);
}

// 0x00449e90: nearest rider closing on self within the lateral window,
// scored by time-to-meet. Returns a riders_ index or -1 with the gap.
// Candidates are scanned in lateral order off self's ring successor, exactly
// as the 0x00449da0 slice-order walk yields them; the caller (0x00408e40)
// passes 0x3f/0/0x96/0x960/0xc000.
int Race::ai_scan_overtake(const Rider& r, int32_t& gap) const {
    build_scan_list();
    const int me = int(&r - riders_.data());
    const int start = scan_find(me, 4);
    if (start < 0) return -1;
    const ScanObject self = scan_[size_t(start)];
    const int32_t lo = self.lateral - 0xc000 / 2;
    int best = -1;
    int32_t best_t = 0x96 + 1;
    int at = start;
    for (;;) {
        do {
            at = scan_next(at, 0x3f, true, 0x960);
            if (at < 0) return best;
        } while (scan_[size_t(at)].rider == me && scan_[size_t(at)].type == 8);   // own half
        const ScanObject& o = scan_[size_t(at)];
        const int32_t size = object_size(o.type);
        const int32_t left = o.lateral - size / 2;
        if (size + left <= lo) return best;          // wholly left of the window: stop
        if (lo + 0xc000 <= left) return best;        // wholly right: stop
        uint32_t closing = uint32_t(self.speed);
        if (o.type & 0x3c) closing -= uint32_t(o.speed);
        const int32_t c = int32_t(closing);
        if (best < 0 || (c > 0 && self.position < o.position) || (c < 0 && o.position < self.position)) {
            if (c == 0) {
                gap = 0x7fffffff;
                best_t = 0x7fffffff;
                best = at;
            } else {
                const int32_t t = (std::abs(self.position - o.position) * 0x100) / std::abs(c);
                if (t <= 0x96 && t < best_t) { gap = t; best = at; best_t = t; }
            }
        }
    }
}
// 0x00408e40: overtake gate. Latches the scanned rider and steers off it when
// closing; returns true when a target was latched (the caller then skips its
// own target choice). The size table is 0x0044a110 on the node type.
bool Race::ai_overtake(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    int32_t gap = 0;
    const int at = ai_scan_overtake(r, gap);
    ai.overtake_latched = -1;
    ai.overtake_type = 0;
    ai.overtake_position = 0;
    if (at < 0) return false;
    const ScanObject o = scan_[size_t(at)];
    // Latching the replan target itself (+0x168 is a bike) counts as none;
    // either way the low byte returned is 0.
    if (o.type == 4 && ai.target_rider >= 0 && o.rider == ai.target_rider) return false;
    ai.overtake_latched = o.rider;
    ai.overtake_type = o.type;
    ai.overtake_position = o.position;
    // Pass on the side away from self, half the summed sizes plus 0x5000 off
    // the object; flip when that point leaves the object's slice lanes.
    int32_t side = (object_size(o.type) + object_size(4)) / 2 + 0x5000;
    if (b.i32(0x28) <= o.lateral) side = -side;
    const RouteSample& s = *o.sample;
    const int32_t v = o.lateral - s.offset * 0x100 + side;
    const int32_t lane = int8_t(int8_t(div_pow2(v, 8) / 0xfa) - (v < 0 ? 1 : 0));
    const int32_t right = div_pow2(s.path.right_q8, 8), left = div_pow2(s.path.left_q8, 8);
    if (right <= lane || (-lane != left && lane <= -left)) side = -side;
    ai.target_position = o.position + 0x3c0;
    ai.target_x = o.lateral + side;
    if (gap > 0x24 && std::abs(b.i32(0x1c) - o.position) > 799) {
        const int32_t lim = ai_curve_limit(r);
        ai.target_speed = gap < 0x4b ? lim - div_pow2(lim, 2) : lim;
        return true;
    }
    // 0x00449fe0: heading to the object, (atan & 0xff) << 16, wrapped.
    uint32_t angle = uint32_t(approx_atan2(o.position - b.i32(0x1c), div_pow2(o.lateral - b.i32(0x28), 8)) & 0xff) * 0x10000;
    if (angle > 0x7fffff) angle -= 0x1000000;
    if (std::abs(int32_t(angle)) < 0xf) ai.target_speed = ai_curve_limit(r) / 2;
    return true;
}

void Race::ai_think(Rider& r) {
    // 0x00407bd0: transitions (0x00408ad0), state dispatch through 0x00465980,
    // the state's tick counter, then the replan accumulator (0x004082f0 once
    // it passes 0x3b). The 0x0040a410 lookahead is not ported.
    ai_transition(r);
    switch (r.brain.state) {
    case 1: case 2: ai_state_racing(r); break;   // 0x00409000 (2: thunk 0x004092d0)
    // 0x00409460 starts with 0x00409ca0; its attack rolls (brain +0x1d0/
    // +0x1d1) and 0x004095d0 are not ported: fight() keeps the stand-in.
    case 3: ai_state_follow(r); break;
    case 4: ai_state_follow(r); break;           // 0x00409ca0
    case 5: ai_state_fight(r); break;            // 0x0040a4f0
    case 6: ai_state_shadow(r); break;           // 0x0040a560
    default: break;                              // 0: 0x00408e30 does nothing
    }
    AiState& ai = r.brain;
    const int32_t dt = r.bike.i32(0x18);
    if (ai.state >= 0 && ai.state < int(ai.state_ticks.size())) ai.state_ticks[size_t(ai.state)] += dt;
    ai.replan_ticks += dt;
    if (ai.replan_ticks > 0x3b) ai_replan(r);
}

// Racing states 1 and 2 (0x00409000): overtake gate, then the inside-curve or
// straight target choice, then steering and speed control (0x00409f70).
void Race::ai_state_racing(Rider& r) {
    if (!ai_overtake(r)) ai_curve_target(r);
    ai_speed_steer(r);
}

// Follow state 4 (0x00409ca0; state 3 runs it first): close on the target's
// half alongside, matching its speed through 0x00449a80, unless it is far
// ahead (then race). Police-only caps (DAT_004642dc) are not modelled.
void Race::ai_state_follow(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    if (ai.target_rider < 0) { ai_state_racing(r); return; }
    const Rider& t = riders_[size_t(ai.target_rider)];
    const int32_t tpos = t.bike.i32(0x1c), tlat = half_x(t);
    const int32_t dpos = tpos - b.i32(0x1c);
    const int32_t dist = std::abs(dpos) >> 4;
    const int32_t window = 0xa0;   // 500 for the police rider
    if (dpos > 0 && window < dist) { ai_state_racing(r); return; }
    if (ai_overtake(r)) { ai_speed_steer(r); return; }
    // Target half speed: +0x108 for the player (+0x324 == 0), else +0xec.
    const int32_t tspeed = ai.target_rider == 0 ? t.bike.i32(0x108) : t.bike.i32(0xec);
    const bool alongside = (dist << 4) < b.i32(0x178) * 4;
    if (alongside) {
        int32_t off = std::min(dist * 0x1000 + 14000, 28000);
        const auto& road = route_of(t);
        const RouteSample& ts = road[std::min(t.sample, road.size() - 1)];
        const int32_t v = off - ts.offset * 0x100 + tlat;
        if (tlat < b.i32(0x28)) {
            const int32_t lane = div_pow2(v, 8) / 0xfa - (v < 0 ? 1 : 0);
            if (lane >= 0 && lane < div_pow2(ts.path.right_q8, 8)) off = -off;
        }
        ai.target_position = tpos + 0x3c0;
        ai.target_x = tlat - off;
    } else {
        ai_curve_target(r);
    }
    const int32_t speed = b.i32(0xec);
    // 0x00449a80 mode 0: gap (Q8 >> 4) plus 1, clamped speeds, step 5.
    const int32_t shown = ai_speed_units(speed);
    int32_t d = ((tpos - b.i32(0x1c)) >> 4) + 1;
    int32_t want = ai.current_speed;
    if (d <= window) {
        want = ai.current2;
        if (-window == d || -d < window) {
            if (d < 0) d = div_pow2(d, 3);
            want = d + ai_speed_units(tspeed);
            if (ai.current_speed < want) want = ai.current_speed;
            else if (want < ai.current2) want = ai.current2;
        }
    }
    if (want - shown <= 5) { if (5 < shown - want) want = shown - 5; }
    else want = shown + 5;
    ai.target_speed = want;
    if (std::abs(tspeed - speed) < 2000 && alongside && speed > 4000) b.i32(0xec) = ai.target_speed * 100;
    ai_speed_steer(r);
}

// Shadow state 6 (0x0040a560): sit on the target at the current speed.
void Race::ai_state_shadow(Rider& r) {
    AiState& ai = r.brain;
    if (!ai_overtake(r) && ai.target_rider >= 0) {
        const Rider& t = riders_[size_t(ai.target_rider)];
        ai.target_speed = ai.current_speed;
        ai.target_position = t.bike.i32(0x1c);
        ai.target_x = t.bike.i32(0x28);
    }
    ai_speed_steer(r);
}

// Fight state 5 (0x0040a4f0): overtake gate, else straight target snapped to
// the target's lateral unless too far ahead, then steering and speed control.
void Race::ai_state_fight(Rider& r) {
    AiState& ai = r.brain;
    if (!ai_overtake(r)) {
        ai_straight_target(r);
        if (ai.target_rider >= 0) {
            const Rider& t = riders_[size_t(ai.target_rider)];
            const int32_t tlat = t.bike.i32(0x28);
            if (std::abs(ai.target_x - tlat) <= 0x8fff) ai.target_x = tlat;
            // ...unless the target is a live chaser (needs cops: open).
            else {
                ai_state_racing(r);
                return;
            }
        }
    }
    ai_speed_steer(r);
}

void Race::step_rider(Rider& r, uint32_t controls, bool player) {
    Bike& b = r.bike;
    if (r.frozen) return;   // solo oracle: the scheduler never runs this node
    if (r.finished) {   // coast to a stop past the line
        drive_mode(b, 0);
        integrate_speed(b, tables_, 0);
        return;
    }
    if(crash_library_ && b.i32(0x2f4)) {if(!r.crash_active)crash(r);step_crash(r,player,controls);return;}
    // 0x00434660 head: demo hold (or race-not-started) takes the AI branch;
    // the control word is ignored. fight() with zero controls is exactly the
    // branch's stamina regen with no attacks started.
    const bool demo_player = demo_hold_ && player;
    fight(r, demo_player ? 0 : controls, size_t(&r - riders_.data()));
    nitro_charge_tick(r);
    if (b.i32(0x2f4) == 0) {
        if (r.ai || demo_player) ai_think(r);
        else {
            // 0x00434660: nitro activate/stop, then drive (0x0040acd0) and
            // steer (0x0040ad30). Boost multiplies the throttle force
            // (0x0040adb0); nitro_throttle records the post-boost force.
            nitro_think(r, controls, player);
            if ((b.u8(0xc0) & 0x10) == 0) {
                if (controls & control_accelerate) drive_mode(b, 1);
                if (controls & control_brake) drive_mode(b, 2);
                else if (!(controls & control_accelerate)) drive_mode(b, 0);
            } else {
                drive_mode(b, 2);
            }
            if ((b.u8(0xc0) & 0x20) != 0) {
                b.i32(0x114) = nitro_throttle_force(r, b.i32(0x114));
                r.nitro_throttle = b.i32(0x114);
            } else {
                r.nitro_throttle = b.i32(0x114);
            }
            const int32_t lean = (controls & control_lean_up) ? 1 : ((controls & control_lean_down) ? -1 : 0);
            const int mode = (controls & control_left) ? 1 : ((controls & control_right) ? 2 : 0);
            steer_input(b, tables_, mode, lean);
        }
    } else {
        // Crashed: slide to a stop, wait one second, remount at the nearest lane.
        b.i32(0x114) = 0; b.i32(0xf0) = 0; b.i32(0x288) = 0;
        const int32_t v = b.i32(0xec);
        b.i32(0xec) = v > 0 ? std::max(0, v - v / 32 - 20) : 0;
        if (b.i32(0xec) == 0 && ++r.crash_timer > ticks_per_second) {
            const SliceEdges e = edges(r, r.sample);
            int32_t x = b.i32(0x28) >> 8;
            if (x < e.left + 125) x = e.left + 125;
            if (x > e.right - 125) x = e.right - 125;
            b.i32(0x28) = x * 256;
            b.i32(0x2f4) = 0; b.u8(0xc0) &= uint8_t(~0x10); b.i32(0x240) = 0; b.i32(0x23c) = 0;
            b.i32(0x104) = 0; b.i32(0x10c) = 0; b.u8(0x278) = 0;
        }
    }
    const int surface = surface_class(edges(r, r.sample), b.i32(0x28));
    traction(b, tables_, surface, player);
    if (b.i32(0x2f4) != 0 && !(b.u8(0xc0) & 0x10)) crash(r);   // skid crash raised by traction
    integrate_speed(b, tables_, surface);
    shift_gears(b);
    move(r, player);
    walls(r);
    vertical(r);
}

void Race::sound(SoundEvent e, const Bike& b) {
    if (sound_events_.size() < 16) sound_events_.push_back({e, b.i32(0x28), b.i32(0x20), b.i32(0x1c), b.i32(0xec), 0});
}

uint32_t Race::random() {
    seed_ = seed_ * 0x343fdu + 0x269ec3u;
    return (seed_ >> 16) & 0x7fff;
}

// Plan 10: field order follows tests/sim_fields.hpp rider_fields. Values are
// read from the same members the game steps, keyed by grid slot.
RaceSnapshot Race::snapshot() const {
    RaceSnapshot out{ticks_, seed_, {}};
    for (const auto& r : riders_) {
        const std::string p = "rider." + std::to_string(r.slot) + ".";
        const Bike& b = r.bike;
        out.fields.push_back({p + "position", b.i32(0x1c)});
        out.fields.push_back({p + "lateral", b.i32(0x28)});
        out.fields.push_back({p + "height", b.i32(0x20)});
        out.fields.push_back({p + "velocity", b.i32(0xf4)});
        out.fields.push_back({p + "speed", b.i32(0xec)});
        out.fields.push_back({p + "gear", b.i32(0x23c)});
        out.fields.push_back({p + "condition", b.i32(0x2f0)});
        out.fields.push_back({p + "crash", b.i32(0x2f4)});
        out.fields.push_back({p + "slice", int64_t(r.sample)});
        out.fields.push_back({p + "flags", b.u8(0xc0)});
        out.fields.push_back({p + "steer", b.i32(0x288)});
        out.fields.push_back({p + "steer_rate", b.i32(0xf0)});
        out.fields.push_back({p + "lat_vel", b.i32(0x10c)});
        out.fields.push_back({p + "ground", b.i32(0x160)});
        out.fields.push_back({p + "stamina_current", r.stamina.current});
        out.fields.push_back({p + "stamina_max", r.stamina.max});
        out.fields.push_back({p + "weapon", r.weapon});
        out.fields.push_back({p + "ai_target_x", r.brain.target_x});
        out.fields.push_back({p + "ai_target_position", r.brain.target_position});
        out.fields.push_back({p + "ai_target_speed", r.brain.target_speed});
        out.fields.push_back({p + "ai_current_speed", r.brain.current_speed});
        out.fields.push_back({p + "ai_base_speed", r.brain.base_speed});
        out.fields.push_back({p + "ai_replan", r.brain.replan_ticks});
        out.fields.push_back({p + "ai_lane", r.brain.lane});
        out.fields.push_back({p + "ai_bias", r.brain.bias});
        out.fields.push_back({p + "anim", int64_t(r.ride_animation.animation)});
        out.fields.push_back({p + "anim_frame", int64_t(r.ride_animation.frame)});
        out.fields.push_back({p + "anim_queued", int64_t(r.ride_animation.queued)});
        out.fields.push_back({p + "anim_loops", int64_t(r.ride_animation.loops)});
        out.fields.push_back({p + "anim_deadline", int64_t(r.ride_animation.deadline)});
        out.fields.push_back({p + "attack_weapon", int64_t(r.attack.weapon)});
        out.fields.push_back({p + "steal_until", int64_t(r.steal_until)});
        out.fields.push_back({p + "hit_until", int64_t(r.hit_until)});
        out.fields.push_back({p + "nitro_charge", int64_t(r.nitro_charge)});
        out.fields.push_back({p + "nitro_throttle", int64_t(r.nitro_throttle)});
        out.fields.push_back({p + "nitro_charges", int64_t(nitro_charges_)});
    }
    // Free-traffic rows (tests/sim_fields.hpp car_fields) for every pool
    // slot, active or not; the replay comparator requires the intended
    // set (missing field = FAIL). No RNG, no allocation in the tick path
    // beyond the field vector the snapshot already builds.
    if (traffic_) traffic_->append_snapshot(out.fields);
    return out;
}

void Race::fight(Rider& r, uint32_t controls, size_t index) {
    Bike& b = r.bike;
    if (b.i32(0x2f4) != 0) { r.attack.timer = 0; return; }
    if (r.stamina.current < r.stamina.max)
        r.stamina.current = std::min(r.stamina.max, r.stamina.current + b.i32(0x18));
    // 0x00434660: latch the attack weapon from the control word, then find
    // the target through 0x00409400/0x004095a0/0x00409c10 (native: nearest
    // rider on the chosen side). Descriptor-frame hits (0x004096f0) fire
    // from fight() via resolve_hit when the riding animation shows frame 2;
    // without a ride bank the legacy tick-7 stand-in preserves the old tests.
    if (r.attack.timer == 0) {
        bool start = false;
        if (!r.ai) start = (controls & (control_punch | control_kick | control_backhand)) != 0;
        else {
            // AI attack rule (stand-in for AI states 2..5): strike the player when alongside.
            const Rider& p = riders_[0];
            const int reach = weapon_reach[r.weapon] * 0x1000;
            start = p.bike.i32(0x2f4) == 0 && std::abs(p.bike.i32(0x1c) - b.i32(0x1c)) < 0x40 &&
                    std::abs(p.bike.i32(0x28) - b.i32(0x28)) < reach && (ticks_ + int32_t(index) * 7) % 45 == 0;
            controls = control_punch;
        }
        if (start) {
            // 0x00434660: kick uses weapon 3, otherwise the carried weapon; side toward the nearest rider.
            r.attack.weapon = (controls & control_kick) ? 3 : r.weapon;
            r.attack.backhand = (controls & control_backhand) != 0;
            const Rider* nearest = nullptr;
            for (const auto& o : riders_)
                if (&o != &r && std::abs(o.bike.i32(0x1c) - b.i32(0x1c)) < 0x100 &&
                    (!nearest || std::abs(o.bike.i32(0x28) - b.i32(0x28)) < std::abs(nearest->bike.i32(0x28) - b.i32(0x28))))
                    nearest = &o;
            r.attack.right = nearest ? b.i32(0x28) < nearest->bike.i32(0x28) : b.i32(0x60) >= 0;
            r.attack.timer = 1;
            r.attack.fired = false;
        }
        return;
    }
    ++r.attack.timer;
    if (fight_hit_frame(r)) resolve_hit(r, index);
    if (r.attack.timer > attack_ticks) { r.attack.timer = 0; r.attack.fired = false; }
}

// 0x004096f0 head: the selected runtime descriptor's flags (0x600000)
// must allow combat, and animation frame byte +0x81 == 2 is the hit event.
// The flags word lives on the runtime descriptor selected via rider+0x7c,
// which no native bank carries (ActorGraphicsDescriptor has no flags word),
// so the native gate is the frame event only: ride_animation.frame == 2
// fires once per swing (attack.fired). Without a ride bank (no animation
// advance) the legacy tick-7 stand-in fires once instead. Flag-gating the
// hit is open until the runtime descriptor word is traced to a native bank.
bool Race::fight_hit_frame(const Rider& r) const {
    if (r.attack.fired) return false;
    if (ride_library_) return r.ride_animation.frame == 2;
    return r.attack.timer == attack_legacy_hit_tick;
}

// 0x00409c10: side match ((attacker_lateral < target_lateral) == side), then
// |dpos| < 0x40, lateral reach by attack weapon (5/7/8 fists/club/chain,
// kick 5), and |dheight| < 0x3000.
bool Race::fight_reach(const Rider& self, const Rider& other, bool side) const {
    if ((self.bike.i32(0x28) < other.bike.i32(0x28)) != side) return false;
    if (std::abs(self.bike.i32(0x1c) - other.bike.i32(0x1c)) >= 0x40) return false;
    if (std::abs(self.bike.i32(0x28) - other.bike.i32(0x28)) >= weapon_reach[self.attack.weapon] * 0x1000) return false;
    if (std::abs(self.bike.i32(0x20) - other.bike.i32(0x20)) >= 0x3000) return false;
    return true;
}

void Race::resolve_hit(Rider& r, size_t index) {
    // 0x004096f0 after the descriptor/frame gate: nearest rider on the
    // attacked side within fight_reach (0x00409400/0x004095a0/0x00409c10).
    // Steal path first (all gates before the rand()&3 draw): target steal
    // deadline +0x1cc expired, target frame < 2, attacker +0x1d4 == 0
    // (fists), target +0x1d4 neither 0 nor kick 3, target actively
    // attacking (0x00409510). Failed steal still hits. Hit deadline +0x1d0
    // (native: victim hit_until) suppresses damage until strictly expired;
    // a landed hit sets tick+(5-level)*6, then rand()%(6-level) decides a
    // repeat attack and (non-player only) rand()&3==2 queues animation 6.
    // 0x00409ad0 scales strength by stamina (floor half), multiplies the
    // per-weapon EXE table at 0x00465a00, then knock_off: kicks add
    // condition damage trunc(damage/4)+1 with reason 5, others reason 4.
    r.attack.fired = true;
    Bike& b = r.bike;
    Rider* target = nullptr;
    for (auto& o : riders_) {
        if (&o == &r || o.bike.i32(0x2f4) != 0) continue;
        if (!fight_reach(r, o, r.attack.right)) continue;
        if (!target || std::abs(o.bike.i32(0x28) - b.i32(0x28)) < std::abs(target->bike.i32(0x28) - b.i32(0x28))) target = &o;
    }
    const int w = r.attack.weapon;
    const bool involves_player = index == 0 || target == &riders_[0];
    // 0x004096f0: weapon swish when the player swings or is swung at.
    if (involves_player) {
        if (w == 1) sound(SoundEvent::swish_club, b);
        else if (w == 2) sound(SoundEvent::swish_chain, b);
    }
    if (!target) return;
    if (index == 0) last_target_ = int(target - riders_.data());
    // Steal: every gate above, then exactly one rand() draw.
    if (uint32_t(target->steal_until) < uint32_t(ticks_) && target->ride_animation.frame < 2 &&
        r.attack.weapon == 0 && target->attack.weapon != 0 && target->attack.weapon != 3 &&
        target->attack.timer != 0 && (random() & 3) == 3) {
        r.weapon = target->weapon;
        r.attack.weapon = target->weapon;
        target->weapon = 0;
        target->attack.weapon = 0;
        r.steal_until = ticks_ + 300;
        return;
    }
    if (!(uint32_t(ticks_) > uint32_t(target->hit_until))) return;
    // 0x00409ad0: impact sound by weapon (kick alternates kick/punch on the tick parity).
    if (involves_player) {
        static constexpr SoundEvent impact[3] = {SoundEvent::impact_punch, SoundEvent::impact_club, SoundEvent::impact_chain};
        sound(w == 3 ? SoundEvent(10 + (ticks_ & 1)) : impact[w], b);
    }
    const RiderStats& s = r.stamina;
    int32_t damage = int32_t((int64_t((s.max << 8) / s.base) * s.strength) >> 8);
    if (damage < s.strength / 2) damage = s.strength / 2;
    damage = (int32_t(weapon_damage[w]) * damage + ((int32_t(weapon_damage[w]) * damage >> 31) & 0xff)) >> 8;
    const int32_t bike_damage = w == 3 ? (damage + ((damage >> 31) & 3)) / 4 + 1 : 0;
    const bool down = knock_off(*target, damage, bike_damage, bike_damage == 0 ? 4 : 5);
    (void)down;
    target->hit_until = ticks_ + (5 - level_) * 6;
    if ((random() % (6 - level_)) == 0) { /* repeat-attack decision: requeue handled by the anim tail */ }
    else if (index != 0 && (random() & 3) == 2) { /* queue animation 6 on the attacker: render-side */ }
    Bike& tb = target->bike;
    if (r.attack.backhand) { tb.i32(0xec) -= 300; b.i32(0xec) += 300; }   // 0x00409bd0 shove
    auto clamp_steer = [](Bike& k) {
        const int32_t lim = k.i32(0x29c);
        k.i32(0x288) = std::max(-lim, std::min(lim, k.i32(0x288)));
    };
    tb.i32(0x288) += r.attack.right ? 3000 : -3000;
    b.i32(0x288) += r.attack.right ? -1000 : 1000;
    if (w == 3) tb.i32(0x288) += r.attack.right ? 8000 : -8000;
    clamp_steer(tb); clamp_steer(b);
}

// Nitro (player-global in the original; per-rider copies here):
// 0x0040b110 activates when nitro bit 0x10 and accelerate bit 1 are both
// held (checked in 0x00434660): boost flag +0xc0 bit 0x20 clear, charges > 0,
// accumulator/256 >= minimum. It decrements one charge, sets the flag and
// emits event 14. 0x0040b170 stops boost on accelerate release or finish
// (event 21). 0x0040b1a0 (called from 0x00407a90 for the player only) drains
// 256*dt while boosting, else recharges until the maximum test fails.
// 0x0040adb0 multiplies throttle force by the boost multiplier with an
// arithmetic >>8 shift.
void Race::nitro_think(Rider& r, uint32_t controls, bool player) {
    if (!player) return;
    Bike& b = r.bike;
    if ((b.u8(0xc0) & 0x10) == 0) {
        if ((controls & control_nitro) && (controls & control_accelerate)) nitro_think_activate(r);
        if (!(controls & control_accelerate)) nitro_think_stop(r);
    } else {
        nitro_think_stop(r);
    }
}
void Race::nitro_think_activate(Rider& r) {
    Bike& b = r.bike;
    if ((b.u8(0xc0) & 0x20) != 0) return;
    if (nitro_charges_ <= 0) return;
    if ((r.nitro_charge + ((r.nitro_charge >> 31) & 0xff)) / 256 < nitro_.minimum) return;
    --nitro_charges_;
    b.u8(0xc0) = uint8_t(b.u8(0xc0) | 0x20);
    sound(SoundEvent::nitro_on, b);
}
void Race::nitro_think_stop(Rider& r) {
    Bike& b = r.bike;
    if ((b.u8(0xc0) & 0x20) == 0) return;
    b.u8(0xc0) = uint8_t(b.u8(0xc0) & uint8_t(~0x20));
    sound(SoundEvent::nitro_off, b);
}
void Race::nitro_charge_tick(Rider& r) {
    Bike& b = r.bike;
    if ((b.u8(0xc0) & 0x20) == 0) {
        if ((r.nitro_charge + ((r.nitro_charge >> 31) & 0xff)) / 256 < nitro_.max)
            r.nitro_charge += b.i32(0x18) * nitro_.recharge;
    } else {
        r.nitro_charge += b.i32(0x18) * -0x100;
        if (r.nitro_charge < 1) nitro_think_stop(r);
    }
}
int32_t Race::nitro_throttle_force(Rider&, int32_t force) {
    // 0x0040adb0 boost branch: force * multiplier, arithmetic >>8. int64
    // product then narrowed matches the 32-bit imul before the sar.
    const int64_t prod = int64_t(force) * int64_t(nitro_.boost);
    return int32_t(prod >> 8);
}
// Riding animation advanced in the Race tick before snapshots (0x00407a90
// think part + 0x00407f30 selection + 0x0043ae90 advance). Without a ride
// bank there is nothing to advance; the render owner (Game::tick) keeps its
// own states. Contact animation 9 (knock_off reason 4) and attack swing
// codes (fight start) select here; crash states are owned by step_crash.
void Race::advance_ride_animation(Rider& r, bool player) {
    if (r.frozen) return;
    // The pending contact selection (knock_off reason 4 -> animation 9) is
    // consumed in the tick even without a ride bank; selection itself needs
    // the bank. Crash states stay owned by step_crash.
    if (!ride_library_) { r.contact_animation = -1; return; }
    if (r.bike.i32(0x2f4) != 0) return;
    const auto& bank = ride_library_->playback[0];
    const auto& fallback = ride_library_->fallback;
    auto& st = r.ride_animation;
    if (r.contact_animation >= 0) {
        set_animation_with_fallback(st, bank, r.contact_animation, uint32_t(ticks_), fallback);
        r.contact_animation = -1;
    }
    if (r.attack.timer == 1 && attack_codes_) {
        const auto& codes = *attack_codes_;
        set_animation_with_fallback(st, bank,
            codes[r.attack.right ? 1 : 0][r.attack.backhand ? 1 : 0][r.attack.weapon & 3],
            uint32_t(ticks_), fallback);
    }
    RidingAnimationInput input;
    input.speed = r.bike.i32(0xec); input.throttle = r.bike.i32(0x114);
    input.lateral_speed = r.bike.i32(0x100); input.boost = r.bike.i32(0xf4);
    input.flags = r.bike.u8(0x278); input.player = player;
    input.wrecked = (r.bike.u8(0xc0) & 0x10) != 0;
    input.camera_distance = std::abs(r.bike.i32(0x1c) - int32_t(camera_position()));
    advance_riding_animation(st, bank, fallback, input, uint32_t(ticks_));
}

void Race::update_camera() {
    // 0x00437c30 (single road): trail the rider laterally, one sample behind.
    const Rider& p = riders_[0];
    const SliceEdges e = edges(p, p.sample);
    const int32_t centre = (e.left + e.right) / 2;
    const int32_t d = div256(p.bike.i32(0x28)) - centre;
    int32_t off = d - div_pow2(d, 2);
    if (d - off >= 0xc9) off = d - 200;
    else if (d - off < -200) off = d + 200;
    if (crashed()) {
        if (camera_lift_ < 300) ++camera_lift_;
        if (camera_back_ < 900) camera_back_ += 3;
    } else {
        camera_lift_ = camera_lift_ > 0 ? camera_lift_ - 1 : 0;
        camera_back_ = camera_back_ > 2 ? camera_back_ - 3 : 0;
    }
    camera_x_ = centre + off;
    camera_height_ = p.bike.i32(0x24) + camera_lift_;
    if (camera_height_ < 0x97) camera_height_ = 0x96;
}

uint32_t Race::camera_position() const {
    const int32_t p = bike().i32(0x138) - camera_back_ - 0x100;
    return uint32_t(p < 0 ? 0 : p);
}

Camera Race::camera() const {
    const size_t s = camera_position() >> 8;
    return {camera_x_ - route()[s].offset, camera_height_};
}

int Race::place(size_t index) const {
    const Rider& me = riders_.at(index);
    int place = 1;
    for (size_t i = 0; i < riders_.size(); ++i) {
        if (i == index) continue;
        const Rider& o = riders_[i];
        bool ahead;
        if (o.finished && me.finished) ahead = o.finish_tick < me.finish_tick;
        else if (o.finished != me.finished) ahead = o.finished;
        else ahead = o.bike.i32(0x1c) > me.bike.i32(0x1c);
        place += ahead;
    }
    return place;
}

void Race::countdown_step() {
    ++ticks_;
    // 0x00438930 stores the player bike's last run as creation tick - 1
    // (template interval 1), so its first scheduled tick sees dt 2
    // (measured: solo captures, tick 1). Opponents follow the distance-based
    // 0x00437e60 cadence, which is not modelled: they keep dt 1.
    countdown_started_ = true;
    if (!riders_.empty()) riders_[0].bike.i32(0x18) = ticks_ == 1 ? 2 : 1;
    for (size_t i = 0; i < riders_.size(); ++i) {
        Rider& r = riders_[i];
        if (r.frozen || r.finished || r.bike.i32(0x2f4) != 0) continue;
        if (r.ai || (demo_hold_ && i == 0)) ai_think(r);
    }
}

void Race::collide_riders() {
    // 0x004368f0 over the position-sorted object list. Outer nodes: those the
    // scheduler ran this tick (+0x10 == tick) with flags & 6 == 6 (frozen
    // solo nodes never qualify). Each first meets the BORR hazards when its
    // fraction sits within its half-length of mid-slice (0x004373f0), then
    // walks the objects after it in list order with flags & 5 == 5, stopping
    // at the first whose position gap exceeds the summed half-lengths
    // (+0x178), and contacts those within the summed half-widths (+0x174),
    // on a shared road half, and within the height test on +0x17c.
    // Runtime flags are the template byte | 4: bikes and pedestrians (3)
    // take both roles, rider halves (0) neither. Free cars stay in
    // collide_traffic (not in this walk: their list order is approximate);
    // pedestrian pairs (0x00440760 on both) are not ported.
    build_scan_list();
    auto bike_of = [&](const ScanObject& o) -> Bike& {
        return o.rider >= 0 ? riders_[size_t(o.rider)].bike : pedestrians_->pool()[size_t(o.pedestrian)].body.bike;
    };
    auto rider_or_ped = [](const ScanObject& o) { return (o.rider >= 0 && o.type == 4) || o.pedestrian >= 0; };
    for (size_t i = 0; i < scan_.size(); ++i) {
        const ScanObject a = scan_[i];
        if (!rider_or_ped(a)) continue;
        if (a.rider >= 0) {
            const Rider& r = riders_[size_t(a.rider)];
            if (r.frozen || r.finished) continue;
        } else {
            const auto& p = pedestrians_->pool()[size_t(a.pedestrian)];
            if (freeze_actors_ || p.last_tick != ticks_) continue;
        }
        Bike& ab = bike_of(a);
        if (a.rider >= 0 && std::abs(int32_t(uint32_t(ab.i32(0x1c)) & 0xff) - 0x80) < ab.i32(0x178))
            collide_hazards(riders_[size_t(a.rider)]);
        for (size_t j = i + 1; j < scan_.size(); ++j) {
            const ScanObject b = scan_[j];
            if (!rider_or_ped(b)) continue;
            Bike& bb = bike_of(b);
            if (ab.i32(0x178) + bb.i32(0x178) < std::abs(ab.i32(0x1c) - bb.i32(0x1c))) break;
            if (std::abs(ab.i32(0x28) - bb.i32(0x28)) > ab.i32(0x174) + bb.i32(0x174)) continue;
            const Rider& ar = a.rider >= 0 ? riders_[size_t(a.rider)] : pedestrians_->pool()[size_t(a.pedestrian)].body;
            const Rider& br = b.rider >= 0 ? riders_[size_t(b.rider)] : pedestrians_->pool()[size_t(b.pedestrian)].body;
            if (route_of(ar)[ar.sample].dual && ar.route != br.route) continue;
            const int32_t dz = (ab.i32(0x20) - bb.i32(0x20)) - bb.i32(0x17c) + ab.i32(0x17c);
            if (std::abs(dz) > ab.i32(0x17c) + bb.i32(0x17c)) continue;
            if (a.rider >= 0 && b.rider >= 0) {
                collide(riders_[size_t(a.rider)], riders_[size_t(b.rider)]);
            } else if (a.rider >= 0 || b.rider >= 0) {
                Rider& r = riders_[size_t(a.rider >= 0 ? a.rider : b.rider)];
                Pedestrian& p = pedestrians_->pool()[size_t(a.rider >= 0 ? b.pedestrian : a.pedestrian)];
                if (!r.finished && r.bike.i32(0x2f4) == 0) contact_rider_pedestrian(r, p, a.rider >= 0);   // 0x004049c0 rejects crashed riders
            }
        }
    }
}

void Race::step(uint32_t controls) {
    if (riders_[0].finished && std::all_of(riders_.begin(), riders_.end(), [](const Rider& r) { return r.finished; })) return;
    ++ticks_;
    if (countdown_started_ && !riders_.empty()) riders_[0].bike.i32(0x18) = 1;
    for (size_t i = 0; i < riders_.size(); ++i) step_rider(riders_[i], controls, i == 0);
    step_pedestrians();   // same scheduler pass (0x00438170) as the riders
    collide_riders();
    for (size_t i = 0; i < riders_.size(); ++i) collide_traffic(riders_[i]);
    collide_pedestrians();   // pedestrians against cars
    for (size_t i = 0; i < riders_.size(); ++i) advance_ride_animation(riders_[i], i == 0);
    update_camera();
    const auto& player=riders_[0];const auto& road=route_of(player);
    camera_heading_=advance_heading(camera_heading_,previous_camera_position_,camera_position(),road[player.previous_sample].path.bend,road[player.sample].path.bend,(player.bike.u8(0xc0)&16)!=0);
    // 0x00454120 ring advance (terrain/BORR/DZHR draws) before 0x0044dd90.
    if (ring_) ring_->advance(int32_t(camera_position()) - int32_t(previous_camera_position_), ring_random_, ring_sink_);
    else build_pedestrian_slices();
    if (traffic_) {
        // Lane-scan obstacles for 0x0044a070 (mask 0x3f): bikes, riders on
        // foot and pedestrians on the cars' (camera) roadway.
        auto& obstacles = traffic_->obstacles();
        obstacles.clear();
        for (const auto& r : riders_) {
            if (route_of(r)[r.sample].dual && r.route != riders_[0].route) continue;
            const Bike& b = r.bike;
            obstacles.push_back({b.i32(0x1c), b.i32(0x28), b.i32(0x108), b.i32(0x10c), 4});
            if (r.crash_active) { const Bike& f = r.crash_actor.motion; obstacles.push_back({f.i32(0x1c), f.i32(0x28), f.i32(0x108), f.i32(0x10c), 8}); }
        }
        if (pedestrians_) for (const auto& p : pedestrians_->pool()) {
            if (!p.active) continue;
            const Bike& b = p.body.bike;
            obstacles.push_back({b.i32(0x1c), b.i32(0x28), b.i32(0x108), b.i32(0x10c), 0x20});
        }

        // 0x0044b3b0: distance moved this step feeds the spawn timers; the
        // ring front rides 0x5000 ahead of the ring camera, which runs
        // 0x100 ahead of the view camera (0x0044b250: 0x3200 vs 0x3100).
        const int32_t camera=int32_t(camera_position());
        traffic_->step(road,camera+0x100,camera+0x5100,camera-int32_t(previous_camera_position_),
            player.bike.i32(0x1c),player.bike.i32(0x28),[this]{return random();},uint32_t(ticks_));
    }
    previous_camera_position_=camera_position();
}
}
