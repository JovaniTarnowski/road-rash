#include "race.hpp"
#include <cstdlib>
#include <stdexcept>

namespace rr {
namespace {
int32_t div_pow2(int64_t x, int n) { return int32_t(x / (int64_t(1) << n)); }
int32_t div256(int64_t x) { return div_pow2(x, 8); }
int32_t wrap_angle(int32_t a) { return a > 0x7fffff ? a - 0x1000000 : a; }
constexpr int32_t grid_position = 0x3200, grid_lateral = 16384;   // 0x00464218 slot 0
constexpr int32_t ground_wall = 999;                                // 0x00439b10 wall height
}

Race::Race(std::vector<RouteSample> a, std::vector<RouteSample> b, const PhysicsTables& t,
           const BikeSpec& spec, int32_t rider_value)
    : routes_{std::move(a), std::move(b)}, tables_(t) {
    if (routes_[0].size() < 64 || routes_[1].size() < 64) throw std::runtime_error("Route too short for a race");
    init_bike_object(bike_, tables_);
    apply_spec(bike_, spec, rider_value);
    bike_.i32(0x18) = 1;                                          // updated every tick
    sample_ = previous_sample_ = size_t(grid_position >> 8);
    bike_.i32(0x1c) = grid_position;
    bike_.i32(0x138) = grid_position;
    // 0x00405990: lateral = sub-slice +0xcc (road offset) + grid offset.
    bike_.i32(0x28) = routes_[0][sample_].offset + grid_lateral;
    update_camera();
}

SliceEdges Race::edges(size_t i) const {
    const auto& s = route()[i];
    // Terrain chunks are not decoded yet: flags 0, type 0 (building-style sides).
    return {s.offset - s.path.left, s.offset + s.path.right,
            s.offset - s.path.left - s.path.shoulder_left, s.offset + s.path.right + s.path.shoulder_right, 0, 0};
}

void Race::accumulate(uint32_t from, uint32_t to, size_t from_sample, size_t to_sample) {
    // 0x004394c0: bend and rise swept over the move, weighted by the Q8 distance in each sample.
    const auto& r = route();
    const int32_t bend_now = r[to_sample].path.bend, rise_now = r[to_sample].path.rise;
    if (((to ^ from) & 0xffffff00u) == 0) {
        const int32_t d = int32_t(to - from);
        bike_.i32(0x130) = -(bend_now * d);
        bike_.i32(0x12c) = rise_now * d;
        return;
    }
    int32_t in_new = int32_t(to & 0xff), in_old;
    if (int32_t(to) < int32_t(from)) { in_old = -int32_t(from & 0xff); in_new -= 0x100; }
    else in_old = 0x100 - int32_t(from & 0xff);
    const int32_t bend_old = r[from_sample].path.bend, rise_old = r[from_sample].path.rise;
    bike_.i32(0x130) = -(bend_now * in_new + bend_old * in_old);
    bike_.i32(0x12c) = rise_now * in_new + rise_old * in_old;
}

void Race::cross_sample(int32_t delta) {
    // 0x00439390 + 0x004394c0 for one step of at most one sample.
    const uint32_t from = uint32_t(bike_.i32(0x1c));
    uint32_t to = from + uint32_t(delta);
    const size_t last = route().size() - 2;
    if (int32_t(to) < 0) to = 0;
    if ((to >> 8) > last) { to = uint32_t(last << 8); finished_ = true; }
    bike_.i32(0x134) = int32_t(from);
    bike_.i32(0x1c) = int32_t(to);
    bike_.i32(0x138) = int32_t(to);
    const size_t from_sample = sample_;
    if (((to ^ from) & 0xffffff00u) != 0) {
        previous_sample_ = sample_;
        sample_ = to >> 8;
        // Entering a split: the side of half 0's right edge decides the branch (0x00439390).
        const auto& a = routes_[0][sample_];
        if (current_ == 0 && int32_t(to) > int32_t(from) && a.event == RoadEvent::split) {
            const int32_t half0_right = a.offset + a.path.right;
            if ((bike_.i32(0x28) >> 8) >= half0_right) current_ = 1;
        }
    }
    accumulate(from, to, from_sample, sample_);
}

void Race::move_along(int32_t distance) {
    // 0x00439270: split long moves into whole-sample steps and sum the swept bend/rise.
    if (distance == 0) { bike_.i32(0x130) = 0; bike_.i32(0x12c) = 0; return; }
    const int32_t pos = bike_.i32(0x1c);
    int32_t steps = ((pos + distance) >> 8) - (pos >> 8);
    int32_t unit = 0x100;
    if (steps < 0) { steps = -steps; unit = -0x100; }
    if (steps < 2) { cross_sample(distance); return; }
    const size_t start_sample = sample_;
    int32_t magnitude = std::abs(distance);
    int32_t first = magnitude & 0xff;
    if (unit < 0) first = -first;
    cross_sample(first);
    int32_t bend = bike_.i32(0x130), rise = bike_.i32(0x12c);
    for (int32_t n = magnitude >> 8; n > 0 && !finished_; --n) {
        cross_sample(unit);
        bend += bike_.i32(0x130); rise += bike_.i32(0x12c);
    }
    bike_.i32(0x134) = pos;
    previous_sample_ = start_sample;
    bike_.i32(0x130) = bend;
    bike_.i32(0x12c) = rise;
}

void Race::move() {
    // 0x004395e0 (single player, no network ghost branch).
    Bike& b = bike_;
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
    move_along(div256(distance));
    int32_t heading = 0;
    const int32_t bend = b.i32(0x130);
    if (bend != 0) {
        int32_t push;
        if (!(b.u8(0x278) & 4) || int8_t(b.u8(0x164)) < 0x33) push = bend * 400 / dt;
        else push = bend * 800 / dt;   // player skidding (0x004642d8 branch)
        b.i32(0x10c) += div256(push);
        heading = approx_atan2(distance, div256(int64_t(bend) * 400)) * 65536;
    }
    if (heading > 0x7fffff) heading -= 0x1000000;
    b.i32(0x128) = heading;
    if (b.i32(0xec) > 300) b.i32(0x28) += dt * b.i32(0x10c);
}

void Race::walls() {
    // Simplified stand-in for 0x0043a610: stop at the edge of the shoulder, crash when fast.
    const SliceEdges e = edges(sample_);
    const int32_t x = bike_.i32(0x28) >> 8;
    if (x > e.left_shoulder && x < e.right_shoulder) return;
    bike_.i32(0x28) = (x >= e.right_shoulder ? e.right_shoulder - 1 : e.left_shoulder + 1) * 256;
    bike_.i32(0x10c) = 0; bike_.i32(0x104) = 0; bike_.i32(0xf0) = 0; bike_.i32(0x288) = 0;
    if (bike_.i32(0xec) > 4000) crash();
    else bike_.i32(0xec) = bike_.i32(0xec) * 3 / 4;
}

void Race::vertical() {
    // 0x0043a190 with ground height from 0x00439b10 case 0 (no terrain selectors decoded).
    Bike& b = bike_;
    const SliceEdges e = edges(sample_);
    const auto& s = route()[sample_].path;
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
    const int32_t rise_now = s.rise, rise_before = route()[previous_sample_].path.rise;
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

void Race::crash() {
    // Simplified stand-in for 0x00403d60: fall, slide to a stop, then remount on the road.
    bike_.i32(0x2f4) = 1;
    bike_.u8(0xc0) |= 0x10;
    crash_timer_ = 0;
}

void Race::update_camera() {
    // 0x00437c30 (single road): trail the rider laterally, one sample behind.
    const SliceEdges e = edges(sample_);
    const int32_t centre = (e.left + e.right) / 2;
    const int32_t d = div256(bike_.i32(0x28)) - centre;
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
    camera_height_ = bike_.i32(0x24) + camera_lift_;
    if (camera_height_ < 0x97) camera_height_ = 0x96;
}

uint32_t Race::camera_position() const {
    const int32_t p = bike_.i32(0x138) - camera_back_ - 0x100;
    return uint32_t(p < 0 ? 0 : p);
}

Camera Race::camera() const {
    const size_t s = camera_position() >> 8;
    return {camera_x_ - route()[s].offset, camera_height_};
}

void Race::step(uint32_t controls) {
    if (finished_) return;
    ++ticks_;
    Bike& b = bike_;
    if (b.i32(0x2f4) == 0) apply_controls(b, tables_, controls);
    else {
        // Crashed: slide to a stop, wait one second, remount at the nearest lane.
        b.i32(0x114) = 0; b.i32(0xf0) = 0; b.i32(0x288) = 0;
        const int32_t v = b.i32(0xec);
        b.i32(0xec) = v > 0 ? std::max(0, v - v / 32 - 20) : 0;
        if (b.i32(0xec) == 0 && ++crash_timer_ > ticks_per_second) {
            const SliceEdges e = edges(sample_);
            int32_t x = b.i32(0x28) >> 8;
            if (x < e.left + 125) x = e.left + 125;
            if (x > e.right - 125) x = e.right - 125;
            b.i32(0x28) = x * 256;
            b.i32(0x2f4) = 0; b.u8(0xc0) &= uint8_t(~0x10); b.i32(0x240) = 0; b.i32(0x23c) = 0;
            b.i32(0x104) = 0; b.i32(0x10c) = 0; b.u8(0x278) = 0;
        }
    }
    const int surface = surface_class(edges(sample_), b.i32(0x28));
    traction(b, tables_, surface, true);
    if (b.i32(0x2f4) != 0 && !(b.u8(0xc0) & 0x10)) crash();   // skid crash raised by traction
    integrate_speed(b, tables_, surface);
    shift_gears(b);
    move();
    walls();
    vertical();
    update_camera();
}
}
