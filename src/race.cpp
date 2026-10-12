#include "race.hpp"
#include "terrain_ground.hpp"
#include "traffic.hpp"
#include <algorithm>
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
    if (level < 0 || level > 4 || count < 0 || count > 7) throw std::runtime_error("Invalid level or opponent count");
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
        const uint32_t personality = 0x00464558 + 0x204u * uint32_t(k);
        const size_t bike = byte(0x004641c8 + uint32_t(level + k * 5));
        if (bike >= specs.size()) throw std::runtime_error("Opponent bike index out of range");
        const uint32_t row = 0x00463c78 + 0x40u * uint32_t(k);
        Opponent o{specs[bike], word(row + 0xc) / 2, word(personality + 0x1bc + 4u * uint32_t(level)),
                   word(personality + 0x1a8 + 4u * uint32_t(level)),
                   int(byte(personality + 0x18c)), k, k, 0, 0, false,
                   word(0x004642a0 + 4u * uint32_t(level)),
                   {word(row + 0x30), word(row + 0x34), word(row + 0x38), word(row + 0x3c)},
                   word(row + 0x18 + 4u * uint32_t(level))};
        if (o.weapon < 0 || o.weapon > 2) throw std::runtime_error("Unexpected opponent weapon");
        out.push_back(o);
    }
    return out;
}

Race::Race(std::vector<RouteSample> a, std::vector<RouteSample> b, const PhysicsTables& t,
           const BikeSpec& spec, int32_t rider_value, std::vector<Opponent> opponents, int level, int32_t road_base_q8,
           const std::vector<Personality>& personalities, const std::vector<uint8_t>& exe_image)
    : routes_{std::move(a), std::move(b)}, tables_(t), level_(level), road_base_(road_base_q8),
      personalities_exe_(exe_image) {
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
        if (!personalities.empty() && o.rider >= 0 && size_t(o.rider) < personalities.size())
            brain_from(r.brain, personalities[size_t(o.rider)]);
        r.stamina = o.stats;
        r.weapon = o.weapon;
        riders_.push_back(r);
    }
    update_camera();
    previous_camera_position_=camera_position();
}

void Race::set_crash_library(const ActorAnimationLibrary& library,const CrashConfig& config) {
    crash_library_=&library;crash_config_=config;
    for(auto& rider:riders_)init_crash_actor(rider.crash_actor,config);
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
    if ((to >> 8) > last) {
        to = uint32_t(last << 8);
        if (!r.finished) { r.finished = true; r.finish_tick = ticks_; }
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

void Race::collide(Rider& ra, Rider& rb) {
    // 0x00436a40 for two bikes (+0xc4 == 4): separate along whichever axis was clear on the
    // previous tick (+0x30 position, +0x3c lateral, +0x34 height) in inverse proportion to
    // mass (+0x194), and record impulses +0x184 forward, +0x188 lateral, +0x18c vertical.
    Bike& a = ra.bike; Bike& b = rb.bike;
    const int32_t ma = a.i32(0x194), mb = b.i32(0x194), m = ma + mb;
    b.i32(400) = ma; a.i32(400) = mb;
    int32_t rel = a.i32(0x108) - b.i32(0x108);
    rel = std::abs((rel >> 9) + (rel >> 12) + (rel >> 7));
    a.u8(0x19c) = 0; b.u8(0x19c) = 0;
    const int32_t len = a.i32(0x178) + b.i32(0x178), wid = a.i32(0x174) + b.i32(0x174);
    auto impulse = [&](int off) { return ((b.i32(off) - a.i32(off)) * mb * 2) / m; };
    // Forward overlap new this tick.
    if (len < std::abs(a.i32(0x30) - b.i32(0x30))) {
        const int32_t depth = (len - std::abs(a.i32(0x1c) - b.i32(0x1c))) + 8;
        if (b.i32(0x30) < a.i32(0x30)) {
            a.u8(0x19c) |= 1; b.u8(0x19c) |= 2;
            move_along(ra, (depth * mb) / m + 3, false);
            if (rel < 0x14) move_along(rb, -((depth * ma) / m) - 3, false);
        } else {
            a.u8(0x19c) |= 2; b.u8(0x19c) |= 1;
            if (rel < 0x14) move_along(ra, -((depth * mb) / m) - 3, false);
            move_along(rb, (depth * ma) / m + 3, false);
        }
        a.i32(0x184) = impulse(0x108); b.i32(0x184) = -a.i32(0x184);
    } else { a.i32(0x184) = 0; b.i32(0x184) = 0; }
    // Lateral overlap new this tick.
    if (wid < std::abs(a.i32(0x3c) - b.i32(0x3c))) {
        const int32_t depth = (wid - std::abs(a.i32(0x28) - b.i32(0x28))) + 0x800;
        if (b.i32(0x3c) < a.i32(0x3c)) {
            a.u8(0x19c) |= 4; b.u8(0x19c) |= 8;
            a.i32(0x28) += (depth * mb) / m + 3; b.i32(0x28) -= (depth * ma) / m + 3;
        } else {
            a.u8(0x19c) |= 8; b.u8(0x19c) |= 4;
            a.i32(0x28) -= (depth * mb) / m + 3; b.i32(0x28) += (depth * ma) / m + 3;
        }
        a.i32(0x188) = impulse(0x10c); b.i32(0x188) = -a.i32(0x188);
    } else { a.i32(0x188) = 0; b.i32(0x188) = 0; }
    a.i32(0x18c) = 0; b.i32(0x18c) = 0;   // riders never stack on each other at ground level
    if (a.u8(0x19c) == 0) {
        // Overlapping on both axes already: resolve by the larger impulse (end of 0x00436a40).
        a.i32(0x184) = impulse(0x108); b.i32(0x184) = -a.i32(0x184);
        a.i32(0x188) = impulse(0x10c); b.i32(0x188) = -a.i32(0x188);
        if (std::abs(div_pow2(a.i32(0x184), 2)) < std::abs(a.i32(0x188)) || a.i32(0x10c) <= a.i32(0x108)) {
            const int32_t depth = (wid - std::abs(a.i32(0x28) - b.i32(0x28))) + 0x800;
            if (a.i32(0x188) < 0) { a.u8(0x19c) |= 8; a.i32(0x28) -= (depth * mb) / m + 3; }
            else { a.u8(0x19c) |= 4; a.i32(0x28) += (depth * mb) / m + 3; }
            if (-a.i32(0x188) < 1) { b.u8(0x19c) |= 8; b.i32(0x28) -= (depth * ma) / m + 3; }
            else { b.u8(0x19c) |= 4; b.i32(0x28) += (depth * ma) / m + 3; }
        } else {
            const int32_t depth = (len - std::abs(a.i32(0x1c) - b.i32(0x1c))) + 8;
            if (a.i32(0x184) < 0) { a.u8(0x19c) |= 2; if (rel < 0x14) move_along(ra, -((depth * mb) / m) - 3, false); }
            else { a.u8(0x19c) |= 1; move_along(ra, (depth * mb) / m + 3, false); }
            if (-a.i32(0x184) < 1) { b.u8(0x19c) |= 2; if (rel < 0x14) move_along(rb, -((depth * ma) / m) - 3, false); }
            else { b.u8(0x19c) |= 1; move_along(rb, (depth * ma) / m + 3, false); }
        }
    }
    for (int k = 0; k < 5; ++k) { a.i32(0x30 + 4*k) = a.i32(0x1c + 4*k); b.i32(0x30 + 4*k) = b.i32(0x1c + 4*k); }
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
    constexpr int32_t car_half_width = 16384;    // 0x00475698 +0x44 via 0x004387a0
    constexpr int32_t car_half_length = 64;      // 0x00475698 +0x48
    constexpr int32_t car_half_height = 8960;    // 0x00475698 +0x4c
    Bike& s = r.bike;
    const int32_t position = s.i32(0x1c), lateral = s.i32(0x28), height = s.i32(0x20);
    const int32_t half_length = s.i32(0x178), half_width = s.i32(0x174), half_height = s.i32(0x17c);
    const auto& route = route_of(r);
    const auto& sample = route[r.sample];
    for (auto& car : traffic_->cars_mut()) {
        if (!car.active) continue;
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

// Bike side of the contact handler 0x004049c0 against a car (+0xc4 == 0x10).
// Cars are tall (+0x17c = 8960 >= 0x2000), so the hit is class 2 when the
// car's width contains the rider (+0x19c bit 0x40, set by collide_traffic
// from the 0x00436a40 prologue rule), otherwise class 3. Class 1 needs a
// short other object and never fires for cars.
void Race::contact_car(Rider& self, TrafficCar& car) {
    Bike& s = self.bike;
    // Knock-off decision 0x004052c0: scales its argument by class (x7/4 for
    // class 2, /8 for class 3), applies stamina damage (0x004050c0: max -4,
    // current -64, max floored at base/2) and bike damage (0x004051e0:
    // +0x2f0), and enters the crash (reason = class) when stamina or
    // condition runs out. Returns whether the rider went down.
    auto knock_off = [&](int32_t scaled) -> bool {
        RiderStats& st = self.stamina;
        st.max -= scaled * 4;
        if (st.max < st.base / 2) st.max = st.base / 2;
        st.current = std::max(0, st.current - scaled * 0x40);
        s.i32(0x2f0) = std::max(0, s.i32(0x2f0) - scaled);
        if (st.current == 0 || s.i32(0x2f0) == 0) {
            crash_car(self);
            return true;
        }
        return s.i32(0x2f4) != 0;
    };
    const int32_t elasticity = s.i32(0x180);
    s.i32(0x188) = div256(int64_t(s.i32(0x188)) * elasticity);
    s.i32(0x184) = div256(int64_t(s.i32(0x184)) * elasticity);
    const int32_t hit = std::abs(s.i32(0x184));
    if (&self == &riders_[0]) {
        // 0x004049c0 player sounds (shared with the bike-bike head).
        if (hit < 0x1771) {
            if (hit >= 0x7d1 || std::abs(s.i32(0x188)) >= 0x3e9) {
                sound(SoundEvent((random() & 1) + 3), s);
                sound(SoundEvent(random() & 1), s);
            }
        } else {
            sound(SoundEvent((random() & 1) + 4), s);
            sound(SoundEvent((random() & 1) + 1), s);
        }
    }
    const int32_t old_steer_rate = s.i32(0xf0);
    const bool contained = (s.u8(0x19c) & 0x40) != 0;
    const int32_t magnitude = (std::abs(s.i32(0x184)) + std::abs(s.i32(0x188)) +
                               std::abs(s.i32(0x18c))) >> 10;
    if (!contained) {
        // Class 3: glance off the car's side, with the 0x004052c0 decision.
        const bool down = knock_off(magnitude / 8);
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
    } else if (const bool down = knock_off((magnitude * 7) / 4); down) {
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

// Crash entry for car contact: 0x00403d60 with reason 2 (animation 29).
// Race::crash hard-codes reason 0 for combat/wall knock-offs; the car path
// passes 2 through begin_crash identically.
void Race::crash_car(Rider& r) {
    if (crash_library_) {
        if (r.crash_active) return;
        r.bike.i32(0x2f4) = 0;
        begin_crash(r.bike, r.crash_actor, crash_config_, crash_library_->playback[0],
                    crash_library_->fallback, uint32_t(ticks_), 2);
        r.crash_active = true; r.crash_timer = 0;
        r.crash_actor.route = r.route; r.crash_actor.sample = r.sample;
        r.crash_actor.previous_sample = r.previous_sample;
        return;
    }
    r.bike.i32(0x2f4) = 1;
    r.bike.i32(0x2f8) = 2;
    r.bike.u8(0xc0) |= 0x10;
    r.crash_timer = 0;
}

void Race::contact(Rider& self, Rider& other) {
    // Bike-against-bike branch of the contact handler 0x004049c0 (the knock-off decision
    // 0x004052c0 and sounds are not ported). Bikes are 0x2300 tall (>= 0x2000), so the hit is
    // class 1/2 when the other bike's width contains ours (+0x19c bit 0x40), otherwise 3.
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
    if (contained) {
        if (s.i32(0xec) - o.i32(0x108) < 0x1771) s.i32(0xec) += s.i32(0x184);
        else crash(self);
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
    const int32_t self_pos = r.bike.i32(0x1c);
    // Link order is position order (0x00438aa0 insert sort): gather others.
    std::vector<size_t> order;
    for (size_t i = 0; i < riders_.size(); ++i) {
        if (&riders_[i] == &r) continue;
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return riders_[a].bike.i32(0x1c) < riders_[b].bike.i32(0x1c);
    });
    if (!forward) std::reverse(order.begin(), order.end());
    for (size_t i : order) {
        const Rider& o = riders_[i];
        const int32_t gap = forward ? o.bike.i32(0x1c) - self_pos : self_pos - o.bike.i32(0x1c);
        if (gap <= 0) continue;   // link walk starts past self on each side
        if (cap <= gap) return -1;
        // Slice-equality skip (0x00449ce0): off-road laterals don't count.
        const SliceEdges e = edges(o, o.sample);
        const int32_t x = o.bike.i32(0x28) >> 8;
        if (x <= e.left_shoulder || e.right_shoulder <= x) continue;
        return int(i);
    }
    return -1;
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
    // Rating-table easing (10 entries x 3 words toward their targets).
    for (int k = 0; k < 10; ++k) {
        int32_t& cur0 = ai.rating[size_t(k * 3)];
        const int32_t tgt0 = ai.rating_tgt[size_t(k * 3)];
        if (cur0 != tgt0) {
            if (cur0 < tgt0) {
                cur0 = cur0 + ai.ease[0];
                const int32_t hi = ai.rating_tgt[size_t(k * 3 + 2)];
                if (hi < cur0) cur0 = hi;
            } else {
                cur0 = cur0 - ai.ease[0];
                const int32_t lo = ai.rating_tgt[size_t(k * 3 + 2)];
                if (cur0 < lo) cur0 = lo;
            }
        }
        int32_t& cur1 = ai.rating[size_t(k * 3 + 1)];
        const int32_t tgt1 = ai.rating_tgt[size_t(k * 3 + 1)];
        if (cur1 != tgt1) {
            if (tgt1 < cur1) {
                cur1 = cur1 - ai.ease[1];
                if (cur1 < tgt1) cur1 = tgt1;
            } else {
                cur1 = cur1 + ai.ease[1];
                if (tgt1 < cur1) cur1 = tgt1;
            }
        }
        int32_t& cur2 = ai.rating[size_t(k * 3 + 2)];
        const int32_t tgt2 = ai.rating_tgt[size_t(k * 3 + 2)];
        if (cur2 != tgt2) {
            if (tgt2 < cur2) {
                cur2 = cur2 - ai.ease[2];
                if (cur2 < tgt2) cur2 = tgt2;
            } else {
                cur2 = cur2 + ai.ease[2];
                if (tgt2 < cur2) cur2 = tgt2;
            }
        }
    }
    const int ahead = ai_scan_replan(r, true, 0x5000);
    ai.nbr_ahead = ahead;
    const int behind = ai_scan_replan(r, false, 0xa00);
    ai.nbr_behind = behind;
    // Overtake-step rating from the table row indexed by the neighbour's
    // +0x165 (overtake_index). Only the finish-count term is live here.
    auto rate_step = [&](int nbr) {
        if (nbr < 0) return 0;
        const Rider& o = riders_[size_t(nbr)];
        const int idx = o.brain.overtake_index;
        if (idx < 0 || idx > 9) return 2;
        const int32_t finish = 4;   // player unfinished: 4 - 0
        const int32_t v = (ai.rating[size_t(idx * 3 + 2)] - ai.rating[size_t(idx * 3)]) +
                          ai.rating[size_t(idx * 3 + 1)];
        if (v < (0x10 - finish) * 1000 + level_ * -0x5dc) return 1;
        return ((0x10 - finish) * 2000 + level_ * -3000 <= v) + 2;
    };
    ai.ov_ahead = ahead < 0 ? 0 : rate_step(ahead);
    ai.ov_behind = behind < 0 ? 0 : rate_step(behind);
    // Keep the nearer of the two rated neighbours as the overtake target.
    int pick = ahead;
    int step = ai.ov_ahead;
    if (ai.ov_ahead <= ai.ov_behind) {
        step = ai.ov_behind;
        pick = behind;
        if (behind < 0) {
            step = ai.ov_ahead;
            pick = ahead;
            if (ahead < 0) { step = 0; pick = -1; }
        } else if (ahead < 0) {
            step = ai.ov_behind;
            pick = behind;
        } else {
            const int32_t db = std::abs(r.bike.i32(0x1c) - riders_[size_t(behind)].bike.i32(0x1c));
            const int32_t da = std::abs(r.bike.i32(0x1c) - riders_[size_t(ahead)].bike.i32(0x1c));
            if (da <= db) { step = ai.ov_ahead; pick = ahead; }
        }
    }
    ai.target_rider = pick;
    ai.target_step = step;
    // Speed reset from the level tables (+0x1a4 from +0x1bc[level], +0x1a0
    // from +0x1a8[level]); the player-chaser early return needs cops (open).
    if (level_ >= 0 && level_ < 5) {
        ai.current_speed = ai.spd_b[size_t(level_)];
        ai.current2 = ai.spd_a[size_t(level_)];
    }
    const Rider& player = riders_[0];
    const bool nbr_is_player = (ahead >= 0 && size_t(ahead) == 0) || (behind >= 0 && size_t(behind) == 0);
    if (nbr_is_player) {
        int32_t d = (r.bike.i32(0x1c) - player.bike.i32(0x1c)) >> 8;
        if (d < -9) d = -9; else if (d > 9) d = 10;
        ai.current_speed = ((tables_.rubber_band[size_t(d + 9)] + 100) * ai.current_speed) / 100;
    } else if (pick == -1 || ai.target_step != 3) {
        const bool no_ahead = ahead < 0, has_behind = behind >= 0;
        if (!no_ahead && !has_behind) {
            // Case 2: ahead only.
            ai.current_speed = ((level_ + 94) * ai.current_speed) / 100;
        } else if (no_ahead && has_behind) {
            // Case 1.
            const int32_t v = ai.current_speed;
            ai.current_speed = (int32_t(v + (v >> 31 & 15)) >> 4) + v;
        } else if (no_ahead && !has_behind) {
            // Case 0.
            if (r.bike.i32(0x1c) <= player.bike.i32(0x1c)) {
                const int32_t v = ai.current_speed;
                ai.current_speed = (int32_t(v + (v >> 31 & 3)) >> 2) + v;
            } else {
                ai.current_speed = ((level_ + 44) * ai.current_speed * 2) / 100;
            }
        } else {
            // Case 3: both neighbours.
            if (r.bike.i32(0x1c) <= player.bike.i32(0x1c)) {
                const int32_t v = ai.current_speed;
                ai.current_speed = (int32_t(v + (v >> 31 & 31)) >> 5) + v;
            } else {
                ai.current_speed = (((level_ - 6) / 2 + 100) * ai.current_speed) / 100;
            }
        }
    } else {
        const int32_t v = ai.current_speed;
        ai.current_speed = (int32_t(v + (v >> 31 & 7)) >> 3) + v;
    }
    if (player.bike.i32(0x2f4) != 0 && std::abs(r.bike.i32(0x1c) - player.bike.i32(0x1c)) > 0xa00) {
        if (level_ == 3) ai.current_speed = div_pow2(int64_t(ai.current_speed) * 3, 2);
        else if (level_ != 4) ai.current_speed /= 2;   // player down and far away
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
    if (!(uint32_t(r.invulnerable_until) < uint32_t(ticks_) &&
          uint32_t(t.invulnerable_until) < uint32_t(ticks_)))
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
    if (level_ >= 0 && level_ < 5 && !personalities_exe_.empty()) {
        // 0x00465a10 per-level overtake eagerness (read from the EXE).
        table = exe_word(personalities_exe_, 0x00465a10 + 4u * uint32_t(level_)) * r.bike.i32(0x18);
    }
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
                const int32_t centre = route[at].offset * 256 + road_base_;
                const int32_t v = q.bike.i32(0x28) - centre;
                idx = int32_t((v + ((v >> 31) & 0xff)) >> 8) / 0xfa - (v < 0 ? 1 : 0);
                if (idx < 0) return false;
                const int32_t lanes = route[at].path.right_q8 >> 8;
                return idx < lanes;
            };
            int32_t self_lane = 0;
            if (lane_index(r, self_lane)) {
                const Rider& t = riders_[size_t(ai.target_rider)];
                int32_t tgt_lane = 0;
                if (lane_index(t, tgt_lane)) {
                    if (ai_can_overtake(r)) ai_set_state(ai, 3, ai.target_rider);
                    else if (ai_fight_range(r)) ai_set_state(ai, 5, ai.target_rider);
                    else ai_set_state(ai, 1, ai.target_rider);
                    return;
                }
            }
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
        const size_t next = std::min(r.sample + 1, route.size() - 1);
        void* u = nullptr;
        (void)u;
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
    const int32_t self_pos = r.bike.i32(0x1c);
    const int32_t self_lat = r.bike.i32(0x28);
    const int32_t self_speed = r.bike.i32(0x108);
    const int32_t lo = self_lat - 0xC000 / 2;
    int best = -1;
    int best_score = 0x96 + 1;
    gap = 0x7fffffff;
    std::vector<size_t> order;
    for (size_t i = 0; i < riders_.size(); ++i) {
        if (&riders_[i] == &r) continue;
        order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return riders_[a].bike.i32(0x28) < riders_[b].bike.i32(0x28);
    });
    for (size_t i : order) {
        const Rider& o = riders_[i];
        const int32_t olat = o.bike.i32(0x28);
        if (olat <= lo) break;                // behind the near edge: stop
        if (lo + 0xC000 <= olat) return best;  // past the far edge: give up
        // Size halves (0x0044a110 on the node type; halves read 0x3000).
        const int32_t half_o = 0x3000, half_self = 0x3000;
        const int32_t meet = olat - half_o / 2;
        if (half_self + meet <= lo) break;
        if (lo + 0xC000 <= meet) return best;
        int32_t closing = self_speed - o.bike.i32(0x108);
        if (closing != 0) {
            const int32_t dist = self_pos > o.bike.i32(0x1c) ? self_pos - o.bike.i32(0x1c)
                                                             : o.bike.i32(0x1c) - self_pos;
            const int32_t score = int32_t((int64_t(dist) * 0x100) / uint32_t(closing < 0 ? -closing : closing));
            const bool ahead = o.bike.i32(0x1c) > self_pos;
            if ((best < 0 || (closing > 0 && ahead) || (closing < 0 && !ahead)) &&
                score <= 0x96 && score < best_score) {
                gap = score;
                best = int(i);
                best_score = score;
            }
        } else {
            gap = 0x7fffffff;
            best = int(i);
            best_score = 0x7fffffff;
        }
    }
    return best;
}
// 0x00408e40: overtake gate. Latches the scanned rider and steers off it when
// closing; returns true when a target was latched (the caller then skips its
// own target choice). The size table is 0x0044a110 on the node type.
bool Race::ai_overtake(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    int32_t gap = 0;
    const int tgt = ai_scan_overtake(r, gap);
    const size_t me = size_t(&r - riders_.data());
    // Latch slot: the original stores the node pointer at brain+0x180; the
    // latch value itself is not compared, only latched-vs-null below.
    int latched = tgt;
    if (tgt < 0 || size_t(tgt) == me) {
        latched = -1;
    }
    ai.overtake_latched = latched;
    if (latched < 0) {
        // Null (or self): report latched-nonzero iff the replan target is set
        // (its low byte decides in the original).
        return ai.target_rider >= 0;
    }
    const Rider& t = riders_[size_t(latched)];
    // (self_size + target_size) / 2 + 0x5000, both halves 0x3000 here.
    int32_t side = (0x3000 + 0x3000) / 2 + 0x5000;
    if (b.i32(0x1c) <= t.bike.i32(0x28)) side = -side;
    (void)side;
    // Lane-meet side flip and the close/far target writes follow the
    // original; on the verified solo path the scan stays null, so the gate
    // only reports the replan-target presence. Full close-range steering is
    // open until a full-field golden exercises it.
    return true;
}

void Race::ai_think(Rider& r) {
    // 0x00407bd0: per-tick transitions (0x00408ad0), state dispatch through
    // 0x00465980 (states 1/2 race via 0x00409000, state 5 fights via 0x0040a4f0;
    // the 0x0040a410 lookahead steers nothing on the verified path: open),
    // then the replan accumulator (0x004082f0 every 60 ticks).
    ai_transition(r);
    if (r.brain.state == 5) ai_state_fight(r);
    else ai_state_racing(r);
    Bike& b = r.bike;
    AiState& ai = r.brain;
    ai.replan_ticks += b.i32(0x18);
    while (ai.replan_ticks > 0x3b) {
        ai.replan_ticks -= 60;
        ai_replan(r);
    }
}

// Racing states 1 and 2 (0x00409000): overtake gate, then the inside-curve or
// straight target choice, then steering and speed control (0x00409f70).
void Race::ai_state_racing(Rider& r) {
    Bike& b = r.bike;
    AiState& ai = r.brain;
    if (!ai_overtake(r)) ai_curve_target(r);
    // No latched rider and no replan target (open-field cruise, e.g. the
    // synthetic two-rider tests where the scan windows stay empty): the
    // committed 0x00409000 still picks the racing line instead of idling.
    if (ai.target_speed == 0 && ai.target_rider < 0) ai_curve_target(r);
    ai_speed_steer(r);
}

// Fight state 5 (0x0040a4f0): overtake gate, else straight target snapped to
// the target's lateral unless too far ahead, then steering and speed control.
void Race::ai_state_fight(Rider& r) {
    Bike& b = r.bike;
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
    if (b.i32(0x2f4) == 0) {
        if (r.ai || demo_player) ai_think(r);
        else apply_controls(b, tables_, controls);
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
    }
    return out;
}

void Race::fight(Rider& r, uint32_t controls, size_t index) {
    Bike& b = r.bike;
    if (b.i32(0x2f4) != 0) { r.attack.timer = 0; return; }
    if (r.stamina.current < r.stamina.max)
        r.stamina.current = std::min(r.stamina.max, r.stamina.current + b.i32(0x18));
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
        }
        return;
    }
    if (++r.attack.timer == attack_hit_tick) resolve_hit(r, index);
    if (r.attack.timer > attack_ticks) r.attack.timer = 0;
}

void Race::resolve_hit(Rider& r, size_t index) {
    // 0x004096f0: find the rider on the attacked side, test reach (0x00409c10), apply damage
    // (0x00409ad0 → 0x004052c0 → 0x004050c0 stamina / 0x004051e0 bike), knock steering, and
    // with fists sometimes take the target's weapon.
    Bike& b = r.bike;
    Rider* target = nullptr;
    for (auto& o : riders_) {
        if (&o == &r || o.bike.i32(0x2f4) != 0) continue;
        const bool on_right = b.i32(0x28) < o.bike.i32(0x28);
        if (on_right != r.attack.right) continue;
        if (std::abs(b.i32(0x1c) - o.bike.i32(0x1c)) >= 0x40) continue;
        if (std::abs(b.i32(0x28) - o.bike.i32(0x28)) >= weapon_reach[r.attack.weapon] * 0x1000) continue;
        if (std::abs(b.i32(0x20) - o.bike.i32(0x20)) >= 0x3000) continue;
        if (!target || std::abs(o.bike.i32(0x28) - b.i32(0x28)) < std::abs(target->bike.i32(0x28) - b.i32(0x28))) target = &o;
    }
    const int w = r.attack.weapon;
    const bool involves_player = index == 0 || target == &riders_[0];
    // 0x004096f0: weapon swish when the player swings or is swung at.
    if (involves_player) {
        if (w == 1) sound(SoundEvent::swish_club, b);
        else if (w == 2) sound(SoundEvent::swish_chain, b);
    }
    if (!target || ticks_ <= target->invulnerable_until) return;
    if (index == 0) last_target_ = int(target - riders_.data());
    // Weapon grab with bare fists (simplified trigger of the 0x004096f0 steal branch).
    if (w == 0 && (target->weapon == 1 || target->weapon == 2) && (random() & 3) == 3) {
        r.weapon = target->weapon;
        target->weapon = 0;
        return;
    }
    // 0x00409ad0: impact sound by weapon (kick alternates kick/punch on the tick parity).
    if (involves_player) {
        static constexpr SoundEvent impact[3] = {SoundEvent::impact_punch, SoundEvent::impact_club, SoundEvent::impact_chain};
        sound(w == 3 ? SoundEvent(10 + (ticks_ & 1)) : impact[w], b);
    }
    const RiderStats& s = r.stamina;
    int32_t damage = int32_t((int64_t((s.max << 8) / s.base) * s.strength) >> 8);
    if (damage < s.strength / 2) damage = s.strength / 2;
    damage = div256(int64_t(weapon_damage[w]) * damage);
    const int32_t bike_damage = w == 3 ? div_pow2(damage, 2) + 1 : 0;
    RiderStats& t = target->stamina;
    t.max -= damage * 4;
    if (t.max < t.base / 2) t.max = t.base / 2;
    t.current = std::max(0, t.current - damage * 0x40);
    Bike& tb = target->bike;
    tb.i32(0x2f0) = std::max(0, tb.i32(0x2f0) - bike_damage);
    if (t.current == 0 || tb.i32(0x2f0) == 0) crash(*target);   // knocked off (0x00403d60)
    target->invulnerable_until = ticks_ + (5 - level_) * 6;
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

void Race::step(uint32_t controls) {
    if (riders_[0].finished && std::all_of(riders_.begin(), riders_.end(), [](const Rider& r) { return r.finished; })) return;
    ++ticks_;
    for (size_t i = 0; i < riders_.size(); ++i) step_rider(riders_[i], controls, i == 0);
    // 0x004368f0: overlapping pairs (position within the half-lengths +0x178, lateral within the
    // half-widths +0x174, same road half) separate and exchange impulses.
    for (size_t i = 0; i < riders_.size(); ++i)
        for (size_t j = i + 1; j < riders_.size(); ++j) {
            Rider& a = riders_[i]; Rider& b = riders_[j];
            if (a.finished || b.finished) continue;
            if (std::abs(a.bike.i32(0x1c) - b.bike.i32(0x1c)) > a.bike.i32(0x178) + b.bike.i32(0x178)) continue;
            if (std::abs(a.bike.i32(0x28) - b.bike.i32(0x28)) > a.bike.i32(0x174) + b.bike.i32(0x174)) continue;
            const auto& sa = route_of(a)[a.sample];
            if (sa.dual && a.route != b.route) continue;
            if (std::abs(a.bike.i32(0x20) - b.bike.i32(0x20)) > a.bike.i32(0x17c) + b.bike.i32(0x17c)) continue;
            collide(a, b);
        }
    for (size_t i = 0; i < riders_.size(); ++i) collide_traffic(riders_[i]);
    update_camera();
    const auto& player=riders_[0];const auto& road=route_of(player);
    camera_heading_=advance_heading(camera_heading_,previous_camera_position_,camera_position(),road[player.previous_sample].path.bend,road[player.sample].path.bend,(player.bike.u8(0xc0)&16)!=0);
    if (traffic_) {
        // 0x0044b3b0: distance moved this step feeds the spawn timers; the
        // front cursor rides 0x5000 ahead of the camera (0x004552e0).
        const int32_t camera=int32_t(camera_position());
        traffic_->step(road,camera,camera+0x5000,camera-int32_t(previous_camera_position_),
            player.bike.i32(0x1c),player.bike.i32(0x28),[this]{return random();});
    }
    previous_camera_position_=camera_position();
}
}
