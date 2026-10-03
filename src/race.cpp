#include "race.hpp"
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
                   int(byte(personality + 0x18c)), k, word(0x004642a0 + 4u * uint32_t(level)),
                   {word(row + 0x30), word(row + 0x34), word(row + 0x38), word(row + 0x3c)},
                   word(row + 0x18 + 4u * uint32_t(level))};
        if (o.weapon < 0 || o.weapon > 2) throw std::runtime_error("Unexpected opponent weapon");
        out.push_back(o);
    }
    return out;
}

Race::Race(std::vector<RouteSample> a, std::vector<RouteSample> b, const PhysicsTables& t,
           const BikeSpec& spec, int32_t rider_value, std::vector<Opponent> opponents, int level)
    : routes_{std::move(a), std::move(b)}, tables_(t), level_(level) {
    if (routes_[0].size() < 64 || routes_[1].size() < 64) throw std::runtime_error("Route too short for a race");
    auto place_rider = [&](const BikeSpec& s, int32_t value, int slot) {
        Rider r;
        init_bike_object(r.bike, tables_);
        apply_spec(r.bike, s, value);
        r.bike.i32(0x18) = 1;                                   // updated every tick
        const int32_t pos = grid_position(slot);
        r.sample = r.previous_sample = size_t(pos >> 8);
        r.bike.i32(0x1c) = pos;
        r.bike.i32(0x138) = pos;
        // 0x00405990: lateral = sub-slice +0xcc (road offset) + grid offset.
        r.bike.i32(0x28) = routes_[0][r.sample].offset + grid_lateral[slot % 4];
        r.bike.i32(0x2f0) = r.bike.i32(0x2ec);   // bike condition (0x00405c80)
        return r;
    };
    riders_.push_back(place_rider(spec, rider_value, 0));
    int colour = 1;
    for (const auto& o : opponents) {
        if (o.slot < 1 || o.slot > 7) throw std::runtime_error("Opponent grid slot must be 1..7");
        Rider r = place_rider(o.spec, o.rider_value, o.slot);
        r.ai = true;
        r.colour = colour++ % 9;
        // 0x00405990 for AI riders: level throttle scale and softer low gears.
        r.bike.i32(0x260) = (o.throttle_percent * r.bike.i32(0x260)) / 100;
        r.bike.i32(0x1d0) -= 0x19; r.bike.i32(0x1e0) -= 0xf; r.bike.i32(0x1f0) -= 8; r.bike.i32(0x200) -= 4;
        r.brain.base_speed = r.brain.current_speed = o.base_speed;
        r.brain.lane = o.lane;
        r.stamina = o.stats;
        r.weapon = o.weapon;
        riders_.push_back(r);
    }
    update_camera();
}

SliceEdges Race::edges(const Rider& r, size_t i) const {
    const auto& s = route_of(r)[i];
    // Terrain chunks are not decoded yet: flags 0, type 0 (building-style sides).
    return {s.offset - s.path.left, s.offset + s.path.right,
            s.offset - s.path.left - s.path.shoulder_left, s.offset + s.path.right + s.path.shoulder_right, 0, 0};
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
        crash(r);
    } else b.i32(0xec) = b.i32(0xec) * 3 / 4;
}

void Race::vertical(Rider& r) {
    // 0x0043a190 with ground height from 0x00439b10 case 0 (no terrain selectors decoded).
    Bike& b = r.bike;
    const SliceEdges e = edges(r, r.sample);
    const auto& s = route_of(r)[r.sample].path;
    const int32_t x = b.i32(0x28) >> 8;
    int32_t ground = 0;
    if (x < e.left || x > e.right) {
        if (x > e.right) ground = x >= e.right_shoulder ? ground_wall : s.height_right;
        else ground = x <= e.left_shoulder ? ground_wall : s.height_left;
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
        int32_t wobble = ((std::abs(vy) * b.i32(0x288)) >> 12) + 1;
        const int32_t lim = b.i32(0x29c) * 2;
        if (wobble > lim) wobble = lim; else if (wobble < -lim) wobble = -lim;
        b.i32(0x288) = wobble;
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
    // Simplified stand-in for 0x00403d60: fall, slide to a stop, then remount on the road.
    r.bike.i32(0x2f4) = 1;
    r.bike.u8(0xc0) |= 0x10;
    r.crash_timer = 0;
}

int32_t Race::curvature(const Rider& r, size_t sample, int samples) const {
    // 0x00449b40: average of sub-slice +0xb4 (atan(bend) << 14, 0x00454240) over the next samples.
    const auto& route = route_of(r);
    int64_t sum = 0;
    for (int k = 0; k < samples && sample + size_t(k) < route.size(); ++k)
        sum += wrap_angle(atan_ratio(tables_, route[sample + size_t(k)].path.bend) * (1 << 14));
    return int32_t(sum / samples);
}

int32_t Race::ai_speed_limit(const Rider& r) const {
    // 0x00409020: slow for the curvature 8 samples ahead.
    const int32_t c = std::abs(curvature(r, std::min(r.sample + 8, route_of(r).size() - 1), 4));
    const int32_t base = r.brain.current_speed;
    const int32_t q = base - div_pow2(base, 2);
    if (c > 0x30000) {
        const int32_t half = q / 2;
        return (q - (int32_t(((int64_t(half) << 8) / 0x600) * (c >> 8)) >> 8)) + half;
    }
    return base;
}

void Race::ai_think(Rider& r) {
    // 0x00407bd0 → racing state 0x00409000 (overtaking 0x00408e40 not ported) → 0x004090a0 /
    // 0x004091f0 target choice, then 0x00409f70 steering and speed control.
    Bike& b = r.bike;
    AiState& ai = r.brain;
    const auto& route = route_of(r);
    const size_t next = std::min(r.sample + 1, route.size() - 1);
    auto straight_target = [&]() {   // 0x004091f0
        const int32_t lanes = route[next].path.right_q8 >> 8;
        int32_t lane = 0;
        if (ai.lane == 1) lane = (lanes - 1) / 2;
        else if (ai.lane == 2) lane = std::abs(lanes - 1);
        ai.target_position = b.i32(0x1c) + 0x3c0;
        ai.target_x = div_pow2(curvature(r, next, 8), 3) + (route[next].offset + lane * 0xfa) * 0x100 + ai.bias;
        ai.target_speed = ai_speed_limit(r);
    };
    const int32_t c8 = curvature(r, r.sample, 8);
    int32_t magnitude = std::abs(c8);
    const int32_t limit = ai_speed_limit(r);
    if (magnitude < 0x30000 || ai_speed_units(b.i32(0xec)) < limit / 2) straight_target();
    else {   // 0x004090a0: take the inside of the curve
        const auto& s = route[r.sample];
        const int32_t left_edge = s.offset - s.path.left;
        const int32_t half = (s.path.right_q8 / 2) * 0x100;
        if (magnitude > 0x60000) magnitude = 0x60000;
        int32_t off = int32_t((int64_t((magnitude << 8) / 0x60000) * half) >> 8);
        if (c8 < 0) off = -off;
        ai.target_position = b.i32(0x1c) + 0x3c0;
        ai.target_x = off + (left_edge + s.path.left_q8) * 0x100 + half;
        ai.target_speed = limit;
        const int32_t x = b.i32(0x28);
        if (c8 < 1) { if (x < ai.target_x) ai.target_x = x; }
        else if (ai.target_x < x) ai.target_x = x;
    }
    // 0x00409f70
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
    // 0x004082f0 every 60 ticks: reset to the level speed, rubber band against the player.
    ai.replan_ticks += b.i32(0x18);
    while (ai.replan_ticks > 0x3b) {
        ai.replan_ticks -= 60;
        ai.current_speed = ai.base_speed;
        const Rider& player = riders_[0];
        const size_t me = size_t(&r - riders_.data());
        const bool player_adjacent = std::abs(place(me) - place(0)) == 1;   // neighbours +0x16c/+0x170
        if (player_adjacent) {
            int32_t d = (b.i32(0x1c) - player.bike.i32(0x1c)) >> 8;
            if (d < -9) d = -9; else if (d > 9) d = 10;
            ai.current_speed = ((tables_.rubber_band[size_t(d + 9)] + 100) * ai.current_speed) / 100;
        } else if (b.i32(0x1c) > player.bike.i32(0x1c)) {
            ai.current_speed = (((level_ - 6) / 2 + 100) * ai.current_speed) / 100;   // case 3, ahead
        } else {
            ai.current_speed += div_pow2(ai.current_speed, 5);                       // case 3, behind
        }
        if (player.bike.i32(0x2f4) != 0 && std::abs(b.i32(0x1c) - player.bike.i32(0x1c)) > 0xa00) {
            if (level_ == 3) ai.current_speed = div_pow2(int64_t(ai.current_speed) * 3, 2);
            else if (level_ != 4) ai.current_speed /= 2;   // player down and far away
        }
    }
}

void Race::step_rider(Rider& r, uint32_t controls, bool player) {
    Bike& b = r.bike;
    if (r.finished) {   // coast to a stop past the line
        drive_mode(b, 0);
        integrate_speed(b, tables_, 0);
        return;
    }
    fight(r, controls, size_t(&r - riders_.data()));
    if (b.i32(0x2f4) == 0) {
        if (r.ai) ai_think(r);
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
    update_camera();
}
}
