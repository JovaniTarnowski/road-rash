#include "bike.hpp"
#include "resource.hpp"
#include <cstdlib>
#include <stdexcept>

namespace rr {
namespace {
// (x + (x>>31 & (2^n-1))) >> n : signed division by 2^n truncating toward zero.
int32_t div_pow2(int64_t x, int n) { return int32_t(x / (int64_t(1) << n)); }
int32_t div256(int64_t x) { return div_pow2(x, 8); }
int32_t wrap_angle(int32_t a) { return a > 0x800000 ? a - 0x1000000 : a; }

uint32_t le32(const std::vector<uint8_t>& b, size_t p) {
    if (p > b.size() || b.size() - p < 4) throw std::runtime_error("Read outside buffer");
    return uint32_t(b[p]) | uint32_t(b[p+1]) << 8 | uint32_t(b[p+2]) << 16 | uint32_t(b[p+3]) << 24;
}
}

std::vector<BikeSpec> read_bike_specs(const std::vector<uint8_t>& rsc) {
    std::vector<BikeSpec> specs;
    for (const auto& r : read_resources(rsc)) {
        if (r.tag != "SPEC") continue;
        if (r.size < 89 * 4) throw std::runtime_error("SPEC resource too small");
        BikeSpec s;
        for (size_t k = 0; k < 89; ++k) s.d[k] = int32_t(le32(rsc, r.offset + 4*k));
        if (r.id < 1 || r.id > 64) throw std::runtime_error("SPEC id out of range");
        if (specs.size() < r.id) specs.resize(r.id);
        specs[r.id - 1] = s;
    }
    if (specs.empty()) throw std::runtime_error("No SPEC resources");
    return specs;
}

PhysicsTables PhysicsTables::synthetic() {
    PhysicsTables t;
    t.surface_drag = {0, 2, 20, 30};
    t.grip = {256, 240, 128, 100};
    t.steer_gain = {230, 230, 128, 100};
    t.steer_damp = {6, 5, 1, 1};
    t.skid_loss = {12, 12, 0, 0};
    t.curve.fill(256);
    for (int i = 0; i <= 256; ++i) t.atan[size_t(i)] = uint8_t(i / 2);   // linear stand-in
    t.rubber_band = {15, 15, 15, 15, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 4, 2, 0, 0, -1, -1};
    for (int i = 0; i < 1024; ++i) t.sine[size_t(i)] = i * 32;   // linear stand-in
    return t;
}

PhysicsTables read_physics_tables(const std::vector<uint8_t>& exe) {
    // .data section mapping of the reference executable (checked by read_depth_table's caller).
    constexpr uint32_t data_va = 0x00463000, data_raw = 0x61600, data_size = 0x16400;
    auto word = [&](uint32_t va) {
        if (va < data_va || va + 4 > data_va + data_size) throw std::runtime_error("Table outside .data");
        return int32_t(le32(exe, data_raw + (va - data_va)));
    };
    auto byte = [&](uint32_t va) { return int8_t(uint8_t(word(va & ~3u) >> (8 * (va & 3)))); };
    PhysicsTables t;
    t.contact_base = word(0x0046dcd8);
    for (int i = 0; i < 4; ++i) {
        t.surface_drag[size_t(i)] = byte(0x0046dce4 + uint32_t(i));
        t.grip[size_t(i)] = word(0x0046dce8 + 4u*i);
        t.steer_gain[size_t(i)] = word(0x0046dcf8 + 4u*i);
        t.steer_damp[size_t(i)] = word(0x0046dd08 + 4u*i);
        t.skid_loss[size_t(i)] = word(0x0046dd18 + 4u*i);
    }
    for (int i = 0; i < 32; ++i) t.curve[size_t(i)] = word(0x0046dd28 + 4u*i);
    for (int i = 0; i < 1024; ++i) t.sine[size_t(i)] = word(0x00475768 + 4u*i);
    for (int i = 0; i < 26; ++i) t.bike_object[size_t(i)] = word(0x00463ba8 + 4u*i);
    for (int i = 0; i < 257; ++i) t.atan[size_t(i)] = uint8_t(byte(0x00476768 + uint32_t(i)));
    for (int i = 0; i < 20; ++i) t.rubber_band[size_t(i)] = word(0x004659d4 + 4u*uint32_t(i - 9));
    // Corroborate: grip table and quarter-wave sine of the reference build.
    if (t.contact_base != 74 || t.grip[0] != 256 || t.sine[0] != 0 || t.sine[512] != 23170 || t.bike_object[1] != 4)
        throw std::runtime_error("Unrecognised physics tables");
    return t;
}

void init_bike_object(Bike& b, const PhysicsTables& t) {
    const auto& w = t.bike_object;
    b.u8(0xc0) = uint8_t(w[0]);
    b.i32(0xc4) = w[1];
    // 0x004387a0: object constants from the behaviour table words 7..
    b.i32(0x148) = w[7];  b.i32(0x14c) = w[8];  b.i32(0x150) = w[9];  b.i32(0x154) = w[10];
    b.i32(0x160) = w[11]; b.i32(0x168) = w[12]; b.i32(0x16c) = w[13]; b.i32(0x1b4) = w[14];
    b.i32(0x1b8) = w[15]; b.i32(0x170) = w[16]; b.i32(0x174) = w[17]; b.i32(0x178) = w[18];
    b.i32(0x17c) = w[19]; b.i32(0x180) = w[20]; b.i32(0x194) = w[21];
    b.i32(0x11c) = -0x25;
}

void apply_spec(Bike& b, const BikeSpec& s, int32_t rider_value) {
    b.u8(0x1bc) = uint8_t(s.d[0]);
    b.i32(0x168) = s.d[1]; b.i32(0x16c) = s.d[2]; b.i32(0x170) = s.d[3];
    b.i32(0x1b4) = s.d[4]; b.i32(0x1b8) = s.d[5];
    const int32_t mass = s.d[6];
    for (int k = 0; k < 0x4c; ++k) b.i32(0x1c4 + 4*k) = s.d[size_t(7 + k)];
    const int32_t throttle_max = b.i32(0x250) * (mass + 0x4b) * 0x100;
    b.i32(0x250) = throttle_max;
    const int32_t brake_max = b.i32(0x26c) * (mass + 0x4b) * 0x100;
    b.i32(0x26c) = brake_max;
    const int32_t steer_div = mass + div_pow2(rider_value, 2);
    const int32_t r2a0 = (b.i32(0x2a0) * (mass + 0x4b)) / steer_div;
    b.i32(0x2a0) = r2a0;
    b.i32(0x29c) = 0x2e00;
    const int32_t r2a4 = (b.i32(0x2a4) * (mass + 0x4b)) / steer_div;
    b.i32(0x2a4) = r2a4;
    b.i32(0x2a8) = 0x2e00 / r2a0;
    b.i32(0x2ac) = 0x2e00 / r2a4;
    const int32_t weight = mass + 0x4b + div_pow2(rider_value - 0x4b, 3);
    const int32_t t = throttle_max / weight, br = brake_max / weight;
    b.i32(0x260) = t;
    b.i32(0x27c) = br;
    auto clamp15 = [](int32_t v) { return v > 0xe ? 0xf : v; };
    b.i32(0x264) = t / clamp15(b.i32(0x254));
    b.i32(0x268) = t / clamp15(b.i32(0x258));
    b.i32(0x280) = br / clamp15(b.i32(0x270));
    b.i32(0x2c4) = 0x14;
    b.i32(0x284) = br / clamp15(b.i32(0x274));
    b.i32(0x248) = 0x2000;
    b.i32(0x2dc) = 0x2000;
}

void brake(Bike& b, int32_t target) {
    // Follows the instructions at 0x0040aea0 (the rates are negative for real specs).
    int32_t v;
    if (b.i32(0xec) == 0) v = 0;
    else {
        v = b.i32(0x240);
        const int32_t limit = b.i32(0x27c);
        if (b.u8(0xc0) & 0x10) target = div256(int64_t(limit) * 0x180);
        else if (target < limit) target = limit;
        if (v < target) {
            const int32_t rate = b.i32(0x18) * b.i32(0x284);
            v = (v - target > rate) ? v - rate : target;
        } else if (v > target) {
            const int32_t rate = b.i32(0x18) * b.i32(0x280);
            v = (v > rate) ? v + rate : target;
        }
    }
    b.i32(0x240) = v;
    b.i32(0x114) = b.i32(0xec) >= 0 ? v >> 8 : (-v) >> 8;
}

void throttle(Bike& b, int32_t target) {
    if (b.u8(0xc0) & 0x10) { brake(b, b.i32(0x27c)); return; }
    const int32_t current = b.i32(0x240);
    if (b.i32(0x260) < target) target = b.i32(0x260);
    int32_t v;
    if (current < target) {
        const int32_t rate = b.i32(0x18) * b.i32(0x264);
        v = (rate < target - current) ? current + rate : target;
    } else {
        v = current;
        if (target < current) {
            const int32_t rate = b.i32(0x18) * b.i32(0x268);
            v = (rate < current - target) ? current - rate : target;
        }
    }
    if (b.u8(0x244) == 0) {
        if (v < 1) { b.i32(0x240) = v; b.i32(0x114) = v >> 8; return; }
        b.i32(0x114) = int32_t((int64_t(b.i32(0x1d0 + b.i32(0x23c) * 0x10)) * v) >> 16);
    } else {
        b.i32(0x114) = 0;
    }
    b.i32(0x240) = v;
}

void drive_mode(Bike& b, int mode) {
    if (mode == 1) { throttle(b, b.i32(0x260)); return; }
    if (mode == 2) { brake(b, b.i32(0x27c)); return; }
    if (b.i32(0x240) >= 0) throttle(b, 0); else brake(b, 0);
}

int32_t atan_ratio(const PhysicsTables& t, int32_t x) {
    if (x < 0) return -atan_ratio(t, -x);
    if (x < 0x101) return t.atan[size_t(x)];
    if (x < 0x10000) return 0x100 - atan_ratio(t, 0x10000 / x);
    return 0x100;
}

int32_t sine(const PhysicsTables& t, uint32_t a) {
    const uint32_t i = a & 0x3ff;
    switch ((a & 0xfff) >> 10) {
    case 0: return t.sine[i] * 2;
    case 1: return t.sine[1023 - i] * 2;
    case 2: return t.sine[i] * -2;
    default: return t.sine[1023 - i] * -2;
    }
}

void steer(Bike& b, const PhysicsTables& t, int32_t s, int32_t lean) {
    const int32_t speed = b.i32(0xec);
    if (speed <= 0) { b.i32(0x118) = 0; b.i32(0xf0) = 0; return; }
    if (s > 0x200000) s = 0x200000;
    if (s < -0x200000) s = -0x200000;
    const int32_t sn = sine(t, uint32_t((s * 16) >> 16));
    int32_t target = int32_t(int64_t(div256(sn)) * speed / b.i32(0x2b0));
    int32_t limit = (lean != 0 ? div_pow2(b.i32(0x29c), 2) : 0) + b.i32(0x29c);
    if (limit < target) target = limit;
    else if (target < -limit) target = -limit;
    int32_t current = b.i32(0x288);
    b.i32(0x28c) = current;
    int32_t rate = (target == 0 || std::abs(target) <= std::abs(current)) ? b.i32(0x2ac) : b.i32(0x2a8);
    rate *= b.i32(0x18);
    int32_t next;
    if (target < current) {
        next = target;
        if (rate < current - target) {
            if (lean < 0) current -= div256(int64_t(b.i32(0x2d0)) * rate);
            next = lean < 1 ? current - rate : current - div256(int64_t(b.i32(0x2d4)) * rate);
        }
    } else {
        next = current;
        if (current < target) {
            next = target;
            if (rate < target - current) {
                if (lean > 0) current += div256(int64_t(b.i32(0x2d0)) * rate);
                next = lean < 0 ? current + div256(int64_t(b.i32(0x2d4)) * rate) : current + rate;
            }
        }
    }
    b.i32(0x288) = next;
    b.i32(0xf0) = div256(next) * b.i32(0x2b0);
}

void steer_input(Bike& b, const PhysicsTables& t, int mode, int32_t lean) {
    int32_t s;
    if (mode == 1) s = -0x20;
    else if (mode == 2) s = 0x20;
    else { s = 0; if (lean != 0) { s = lean * 32; lean = -lean; } }
    steer(b, t, s * 65536, lean);
}

void apply_controls(Bike& b, const PhysicsTables& t, uint32_t c) {
    // Player riding branch of 0x00434660 (nitro and attacks not ported yet).
    if ((b.u8(0xc0) & 0x10) == 0) {
        if (c & control_accelerate) drive_mode(b, 1);
        if (c & control_brake) drive_mode(b, 2);
        else if (!(c & control_accelerate)) drive_mode(b, 0);
    } else {
        drive_mode(b, 2);
    }
    const int32_t lean = (c & control_lean_up) ? 1 : ((c & control_lean_down) ? -1 : 0);
    const int mode = (c & control_left) ? 1 : ((c & control_right) ? 2 : 0);
    steer_input(b, t, mode, lean);
}

void shift_gears(Bike& b) {
    int32_t& gear = b.i32(0x23c);
    if (gear > 6) gear = 6;
    if (gear < 0) gear = 0;
    const int32_t g = gear;
    if (b.i32(0x1d4 + g * 0x10) < b.i32(0xec)) { gear = g + 1 > 6 ? 6 : g + 1; }
    else if (b.i32(0xec) < b.i32(0x1d8 + g * 0x10)) { gear = g - 1 < 0 ? 0 : g - 1; }
}

void integrate_speed(Bike& b, const PhysicsTables& t, int surface) {
    const int32_t speed_abs = std::abs(b.i32(0xec));
    int32_t base, contact_drag;
    if (b.i32(0x160) == 0) { base = speed_abs >> 2; contact_drag = 0; }
    else {
        const int32_t c = t.surface_drag[size_t(surface & 3)];
        b.i32(0x158) = c;
        contact_drag = ((c * speed_abs) >> 14) + b.i32(0x154);
        base = speed_abs;
    }
    int32_t drag = (b.i32(0x150) * (base >> 8)) >> 8;
    b.i32(0x15c) = drag;
    drag += contact_drag;
    b.i32(0x15c) = drag;
    const int32_t lateral = std::abs(b.i32(0x118)) >> 4;
    const int32_t v = b.i32(0xec);
    if (v >= 0) {
        b.accel = (b.i32(0x114) - drag) - lateral;
        const int32_t nv = v + b.i32(0x18) * b.accel;
        b.i32(0xec) = nv < 1 ? (b.i32(0x114) < 1 ? 0 : 100) : nv;
        return;
    }
    b.accel = b.i32(0x114) + std::abs(lateral + drag);
    const int32_t nv = v + b.i32(0x18) * b.accel;
    b.i32(0xec) = nv > 0 ? 0 : nv;
}

int surface_class(const SliceEdges& e, int32_t x_q8) {
    const int32_t x = x_q8 >> 8;
    if (e.left < x && x < e.right) return 0;
    if (e.flags) {
        if (e.flags & 0x20) return 0;
        if (e.flags & 0x18) return e.type == 0 ? 1 : 2;
    }
    if (e.type == 3) return 1;
    if (e.right < x) {
        if (e.right_shoulder <= x) return 3;
        if (e.type == 0) return 1;
        return x <= e.right + 0x30 ? 2 : 0;
    }
    if (x <= e.left_shoulder) return 3;
    if (e.type == 0) return 1;
    return e.left - 0x30 <= x ? 2 : 0;
}

int32_t approx_atan2(int32_t a, int32_t b) {
    if (a == 0 && b == 0) return 0;
    const int32_t x = std::abs(a), y = std::abs(b);
    int q = (a < 0 ? 1 : 0) | (b < 0 ? 2 : 0) | (x < y ? 4 : 0);
    switch (q) {
    case 0: return (y * 0x20) / x;
    case 1: return 0x80 - (y * 0x20) / x;
    case 2: return -((y * 0x20) / x);
    case 3: return (y * 0x20) / x - 0x80;
    case 4: return 0x40 - (x * 0x20) / y;
    case 5: return (x * 0x20) / y + 0x40;
    case 6: return (x * 0x20) / y - 0x40;
    default: return -0x40 - (x * 0x20) / y;
    }
}

void traction(Bike& b, const PhysicsTables& t, int surface, bool player) {
    const size_t s = size_t(surface & 3);
    int32_t grip = t.grip[s];
    if (b.i32(0x160) == 0 || b.i32(0x2f4) != 0) {
        b.i32(0x60) = wrap_angle(approx_atan2(t.contact_base, div256(b.i32(0x288))) * 0x10000);
        b.i32(0x114) = 0; b.i32(0x118) = 0; b.i32(0xf0) = 0; b.u8(0x164) = 0;
        return;
    }
    b.i32(0x118) = 0;
    if (b.i32(0xec) < 1) b.i32(0x288) = 0;
    int32_t timer = b.i32(0x298) - b.i32(0x18);
    b.i32(0x298) = timer < 0 ? 0 : timer;
    const int32_t base = b.u8(0x164) == 0 ? b.i32(0x148) : b.i32(0x14c);
    int32_t g = div256(int64_t(base - div256(int64_t(b.i32(0x298)) * 0x1000)) * b.i32(0x294));
    g = div256(int64_t(g) * grip);
    g = div256(int64_t(g) * b.i32(0x160));
    if (g < 0) g = 0;
    if (player) {
        const int32_t steer_abs = std::abs(b.i32(0x288));
        if (g < b.i32(0x290) && b.i32(0x29c) <= steer_abs) {
            b.u8(0x278) = 1;
            b.skid_ticks += b.i32(0x18);
            if (b.skid_ticks > 0x19) b.i32(0x2f4) = 1;   // 0x00403d60 crash (simplified trigger)
        } else { b.u8(0x278) = 0; b.skid_ticks = 0; }
    } else b.u8(0x278) = 0;
    int32_t u = b.i32(0x114);
    if (b.i32(0x15c) < u) u -= b.i32(0x15c);
    else if (u < 1) u = b.i32(0xec) < 1 ? 0 : u;
    else u = 0;
    int32_t steer_v = b.i32(0x288);
    const int32_t u_abs = std::abs(u);
    const int32_t lat = div_pow2(b.i32(0xec), 11) * (steer_v >> 8);
    const int32_t demand = u_abs + std::abs(lat);
    int32_t term;
    if (g < demand && b.i32(0xec) >= 0) {
        if (surface > 1) {
            const int32_t c = t.curve[s + 4 * ((uint32_t(b.i32(0x1c)) >> 8) & 7)];
            int32_t ns = div256(int64_t(c) * steer_v);
            const int32_t lim = b.i32(0x29c);
            if (ns > lim) ns = lim; else if (ns < -lim) ns = -lim;
            b.i32(0x288) = ns;
        }
        const int32_t damp = div256(int64_t(b.i32(0x288) - b.i32(0x28c)) * t.steer_damp[s]);
        term = div256(int64_t(t.steer_gain[s]) * b.i32(0x288)) + damp;
        const int8_t skid = int8_t(uint8_t(demand) - uint8_t(g));
        b.u8(0x164) = uint8_t(skid);
        if (skid > 40) { int32_t v = b.i32(0xec) - t.skid_loss[s]; b.i32(0xec) = v < 0 ? 0 : v; }
        const uint8_t flags = b.u8(0x278);
        b.u8(0x278) = flags | 4;
        if (g < u_abs) b.u8(0x278) = u < 1 ? flags | 0x14 : flags | 0xc;
    } else {
        b.u8(0x164) = 0;
        term = steer_v;
    }
    if (u < 0) term = div256(int64_t(b.i32(0x25c)) * term);
    b.i32(0x118) += term >> 8;
    int32_t hv = b.i32(0x288);
    if (b.i32(0xec) < 4000) hv /= 2;
    else if (b.i32(0xec) > 8000) hv += div_pow2(hv, 3);
    b.i32(0x60) = wrap_angle(approx_atan2(t.contact_base, div256(hv)) * 0x10000);
}
}
