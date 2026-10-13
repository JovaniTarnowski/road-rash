#include "bike.hpp"
#include <iostream>

// Hand-computed expectations from the original routines named in each check.
int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };

    // Synthetic spec: gear table and rates chosen so the arithmetic is easy to follow.
    rr::BikeSpec spec{};
    spec.d[1] = -800; spec.d[2] = 1024; spec.d[3] = 160; spec.d[4] = 160; spec.d[5] = 1; spec.d[6] = 125;  // mass 125
    auto sp = [&](int offset) -> int32_t& { return spec.d[7 + (offset - 0x1c4) / 4]; };
    for (int g = 0; g < 6; ++g) { sp(0x1d0 + 16*g) = 256 - 8*g; sp(0x1d4 + 16*g) = 3000 + 2000*g; sp(0x1d8 + 16*g) = 2000*g - 999; }
    sp(0x250) = 40; sp(0x254) = 10; sp(0x258) = 20; sp(0x26c) = -60; sp(0x270) = 10; sp(0x274) = 10;
    sp(0x2a0) = 50; sp(0x2a4) = 30; sp(0x2b0) = 120; sp(0x294) = 320; sp(0x290) = 64;

    rr::Bike b;
    rr::apply_spec(b, spec, 150);   // 0x00405ea0 with rider value 150 (param_2)
    // +0x260 = 40*(125+75)*256 / (125+75+(150-75)/8) = 2048000/209 = 9799
    check(b.i32(0x260) == 2048000 / 209, "max throttle 0x260");
    check(b.i32(0x264) == (2048000 / 209) / 10, "throttle rise rate 0x264");
    check(b.i32(0x268) == (2048000 / 209) / 15, "throttle fall rate 0x268 (divisor clamped to 15)");
    check(b.i32(0x29c) == 0x2e00, "steer limit 0x29c");
    // 0x2a0 = 50*200/(125+150/4) = 10000/162 = 61; 0x2a8 = 0x2e00/61
    check(b.i32(0x2a0) == 10000 / 162 && b.i32(0x2a8) == 0x2e00 / (10000 / 162), "steer rates");

    // 0x0040adb0: throttle ramps by dt*rise per call, force = gear factor * throttle >> 16.
    b.i32(0x18) = 1;
    rr::throttle(b, b.i32(0x260));
    check(b.i32(0x240) == b.i32(0x264), "throttle one step");
    check(b.i32(0x114) == (256 * b.i32(0x264)) >> 16, "drive force from gear 0 factor");

    // 0x004391d0: upshift above +0x1d4 of the current gear, downshift below +0x1d8.
    b.i32(0x23c) = 0; b.i32(0xec) = 3001; rr::shift_gears(b);
    check(b.i32(0x23c) == 1, "upshift");
    b.i32(0xec) = 900; rr::shift_gears(b);
    check(b.i32(0x23c) == 0, "downshift");

    // 0x004390c0: speed += dt*(drive - drag - |lateral force|/16); drag = aero*(v>>8)>>8 + contact term.
    rr::Bike c; rr::apply_spec(c, spec, 150);
    c.i32(0x18) = 1; c.i32(0xec) = 5120; c.i32(0x114) = 100; c.i32(0x118) = 0; c.i32(0x160) = 0; c.i32(0x150) = 130;
    rr::integrate_speed(c, rr::PhysicsTables::synthetic(), 0);
    // contact 0 → iVar2 = |v|>>2 = 1280; drag = 130*(1280>>8)>>8 = 2; v' = 5120 + (100-2)
    check(c.i32(0xec) == 5120 + 98, "speed integration without ground contact");

    // Speed never goes below zero while driving forward with no drive.
    c.i32(0xec) = 10; c.i32(0x114) = 0; c.i32(0x160) = 74; c.i32(0x154) = 50;
    rr::integrate_speed(c, rr::PhysicsTables::synthetic(), 0);
    check(c.i32(0xec) == 0, "speed clamps at zero without drive");

    // 0x0043a0c0 surface class: 0 inside road edges, 3 beyond the shoulder.
    rr::SliceEdges e{-500, 500, -700, 700, 0, 0};
    check(rr::surface_class(e, 0) == 0, "on road");
    check(rr::surface_class(e, 600 << 8) == 1, "shoulder (type 0 terrain)");
    check(rr::surface_class(e, 800 << 8) == 3, "beyond shoulder");
    check(rr::surface_class(e, -800 * 256) == 3, "beyond left shoulder");

    // 0x0044f5a0 angle approximation: 0x20 per 45 degrees, 256 per turn.
    check(rr::approx_atan2(100, 0) == 0 && rr::approx_atan2(100, 100) == 0x20 && rr::approx_atan2(0, 100) == 0x40, "atan quadrants");
    check(rr::approx_atan2(-100, 0) == 0x80, "atan backwards");

    // Skid-crash counter (0x0046dce0) belongs to one rider: another bike's skidding must not affect it.
    auto skidding = [&]() {
        rr::Bike k; rr::apply_spec(k, spec, 150);
        k.i32(0x18) = 1; k.i32(0x160) = 1; k.i32(0x148) = 0; k.i32(0x294) = 0; k.i32(0x290) = 64;
        k.i32(0x288) = k.i32(0x29c); k.i32(0xec) = 5000;
        return k;
    };
    rr::Bike other = skidding();
    for (int i = 0; i < 20; ++i) rr::traction(other, rr::PhysicsTables::synthetic(), 0, true);
    rr::Bike fresh = skidding();
    for (int i = 0; i < 10; ++i) rr::traction(fresh, rr::PhysicsTables::synthetic(), 0, true);
    check(fresh.i32(0x2f4) == 0, "skid counter is per bike (no crash after 10 ticks)");

    // 0x004349c0 update_engine_rpm: needle +0x2dc from gear ratio and speed.
    auto rpm_bike = [&](int32_t speed, int32_t gear, int32_t ratio, int32_t rpm,
                        int32_t cached, int32_t height, int32_t crash) {
        rr::Bike b;
        b.i32(0xec) = speed; b.i32(0x23c) = gear;
        b.i32(0x1cc + gear * 0x10) = ratio;
        b.i32(0x2dc) = rpm; b.i32(0x2e8) = cached;
        b.i32(0x20) = height; b.i32(0x2f4) = crash;
        return b;
    };
    {
        auto b = rpm_bike(0, 0, 256, 0x2000, 0, 0, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x2000 && b.i32(0x2e0) == 0x9ffe && b.i32(0x2e8) == 0, "rpm stationary stays at 0x2000");
    }
    {
        auto b = rpm_bike(0x400, 0, 0x100, 0x2000, 0, 0, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x2400, "rpm accelerates toward gear ratio target");
    }
    {
        auto b = rpm_bike(0x400, 1, 0x200, 0x2400, 0x300, 0, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x2800, "rpm gear change at constant speed");
    }
    {
        auto b = rpm_bike(0x400, 0, 0x100, 0x2000, 0, 0x65, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x3000 && b.i32(0x2e8) == 0, "rpm airborne +0x1000, no cache refresh");
    }
    {
        auto b = rpm_bike(0x400, 0, 0x100, 0xffff, 0, 0x65, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0xffff, "rpm airborne capped at 0xffff");
    }
    {
        auto b = rpm_bike(0x400, 0, 0x100, 0x3000, 0, 0, 1);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x2000, "rpm crashed above 0x2001 drops -0x1000");
    }
    {
        auto b = rpm_bike(0x400, 0, 0x100, 0x2000, 0, 0, 1);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x2400, "rpm crashed at or below 0x2001 still clamps");
    }
    {
        auto b = rpm_bike(0x400, 0, 0x100, 0x2000, 0x400, 0, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x2000 && b.i32(0x2e0) == 0, "rpm speed unchanged skips update");
    }
    {
        auto b = rpm_bike(0x10000, 0, 0x100, 0x2000, 0, 0, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x4000, "rpm rate-limited to +0x2000 per update");
    }
    {
        auto b = rpm_bike(0x10000, 0, 1, 0x8000, 0, 0, 0);
        rr::update_engine_rpm(b);
        check(b.i32(0x2dc) == 0x6000, "rpm rate-limited to -0x2000 per update");
    }

    // Plan 23 Task 2: a zeroed spec divides by zero in 0x00405ea0 (steer
    // rates 0x2a8/0x2ac, gear-rate divisors). ARM64 silently yields zero;
    // wasm32 traps. apply_spec guards every divisor to zero so both compute
    // the same thing. Real specs never hit a zero divisor.
    {
        rr::BikeSpec zero{};
        rr::Bike z;
        rr::apply_spec(z, zero, 90);
        check(z.i32(0x250) == 0 && z.i32(0x26c) == 0, "zero spec throttle/brake max");
        check(z.i32(0x2a0) == 0 && z.i32(0x2a4) == 0, "zero spec steer numerators");
        check(z.i32(0x2a8) == 0 && z.i32(0x2ac) == 0, "zero spec steer rates defined");
        check(z.i32(0x260) == 0 && z.i32(0x27c) == 0, "zero spec max rates");
        check(z.i32(0x264) == 0 && z.i32(0x268) == 0 && z.i32(0x280) == 0 && z.i32(0x284) == 0,
            "zero spec rate divisors defined");
        check(z.i32(0x29c) == 0x2e00 && z.i32(0x2c4) == 0x14, "zero spec constants intact");
    }

    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
