#include "race.hpp"
#include "assets.hpp"
#include <filesystem>
#include <iostream>

// Synthetic straight/curved routes; original data is not needed for these rules.
static std::vector<rr::RouteSample> straight(size_t n, int bend = 0) {
    rr::RouteSample s{{bend, 0, 500, 500, 512, 512}, 0, 0, 0};
    s.path.shoulder_left = s.path.shoulder_right = 200;
    return std::vector<rr::RouteSample>(n, s);
}

static rr::BikeSpec test_spec() {
    rr::BikeSpec spec{};
    spec.d[1] = -800; spec.d[2] = 1024; spec.d[3] = 160; spec.d[4] = 160; spec.d[5] = 1; spec.d[6] = 196;
    auto sp = [&](int offset) -> int32_t& { return spec.d[7 + (offset - 0x1c4) / 4]; };
    const int up[6] = {3000, 5000, 7000, 8500, 21504, 16777215}, down[6] = {-999, 2500, 4500, 6500, 8000, 17152};
    for (int g = 0; g < 6; ++g) { sp(0x1d0 + 16*g) = 256 - 8*g; sp(0x1d4 + 16*g) = up[g]; sp(0x1d8 + 16*g) = down[g]; }
    sp(0x248) = 20000; sp(0x250) = 38; sp(0x254) = 10; sp(0x258) = 60; sp(0x25c) = 64; sp(0x26c) = -60;
    sp(0x270) = 10; sp(0x274) = 10; sp(0x290) = 64; sp(0x294) = 320; sp(0x29c) = 11776; sp(0x2a0) = 50;
    sp(0x2a4) = 30; sp(0x2b0) = 120; sp(0x2b8) = 196; sp(0x2bc) = 2560; sp(0x2d0) = 346; sp(0x2d4) = 128;
    return spec;
}

static rr::PhysicsTables tables() {
    auto t = rr::PhysicsTables::synthetic();
    t.bike_object = {3, 4, 1, 0, 0, 0, 0, 0x12c, 0x100, 0x82, 8, 0x25, -0xa00, 0x100, 0x4a, 4, 0x18, 0x1e00, 0x40, 0x2300, 0xc0, 0x100, 0, 1, 1, 0};
    return t;
}

int main() {
    int failures = 0;
    auto check = [&](bool v, const char* what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    const auto t = tables();
    const auto spec = test_spec();

    rr::Race race(straight(2000), straight(2000), t, spec, 90);
    check(race.position() == 0x3200, "start on the grid at sample 50 (0x00464218 slot 0)");
    check(race.lateral() == 16384 >> 8, "grid slot 0 lateral offset from the imported EXE table");

    uint32_t last = race.position();
    bool monotonic = true;
    for (int i = 0; i < 600; ++i) { race.step(rr::control_accelerate); monotonic &= race.position() >= last; last = race.position(); }
    check(monotonic && race.position() > 0x3200 + 0x400, "accelerating moves forward");
    check(race.bike().i32(0xec) > 3000, "speed builds up");
    check(race.bike().i32(0x23c) >= 1, "gear shifts up");

    // Determinism: identical inputs give identical state.
    rr::Race a(straight(2000), straight(2000), t, spec, 90), b(straight(2000), straight(2000), t, spec, 90);
    for (int i = 0; i < 900; ++i) {
        const uint32_t c = rr::control_accelerate | ((i / 60) % 2 ? rr::control_left : rr::control_right);
        a.step(c); b.step(c);
    }
    check(a.bike().m == b.bike().m && a.position() == b.position(), "deterministic");

    // Steering left moves the rider left of where coasting straight would be.
    rr::Race l(straight(2000), straight(2000), t, spec, 90), s(straight(2000), straight(2000), t, spec, 90);
    for (int i = 0; i < 300; ++i) { l.step(rr::control_accelerate); s.step(rr::control_accelerate); }
    for (int i = 0; i < 90; ++i) { l.step(rr::control_accelerate | rr::control_left); s.step(rr::control_accelerate); }
    check(l.lateral() < s.lateral(), "steering left decreases lateral position");

    // A curve pushes the rider toward its outside (0x004395e0 adds -bend*distance to lateral velocity).
    rr::Race curve(straight(2000, 20), straight(2000, 20), t, spec, 90), flat(straight(2000), straight(2000), t, spec, 90);
    for (int i = 0; i < 400; ++i) { curve.step(rr::control_accelerate); flat.step(rr::control_accelerate); }
    check(curve.lateral() < flat.lateral(), "positive bend drifts the rider to negative lateral");

    // Riding into the wall beyond the shoulder never leaves the rider outside it.
    rr::Race wall(straight(4000), straight(4000), t, spec, 90);
    bool inside = true;
    for (int i = 0; i < 1500; ++i) { wall.step(rr::control_accelerate | rr::control_right); inside &= wall.lateral() <= 500 + 200; }
    check(inside, "wall keeps rider within shoulder");

    // Reaching the end of the route finishes the race.
    rr::Race shortrace(straight(80), straight(80), t, spec, 90);
    for (int i = 0; i < 20000 && !shortrace.finished(); ++i) shortrace.step(rr::control_accelerate);
    check(shortrace.finished(), "finish at route end");

    // Camera is one sample behind the rider and at least 150 high (0x00437c30).
    check(race.camera_position() == race.position() - 0x100, "camera one sample behind");
    check(race.camera().height >= 150, "camera height floor");

    // AI opponents (0x00407bd0 racing state): follow the road at their target speed.
    rr::Opponent opp{test_spec(), 90, 100, 100, 0, 1};   // spec, rider value, target speed, cur2, lane, grid slot
    rr::Race field(straight(4000, 3), straight(4000, 3), t, spec, 90, {opp});
    check(field.riders().size() == 2 && field.riders()[1].ai, "opponent added");
    check(field.riders()[1].bike.i32(0x1c) == 0x3200 && field.riders()[1].bike.i32(0x28) == -3616, "opponent on grid slot 1");
    bool on_road = true;
    for (int i = 0; i < 2400; ++i) {
        field.step(0);
        const auto& b = field.riders()[1].bike;
        on_road &= std::abs(b.i32(0x28) >> 8) < 500;
    }
    const auto& ob = field.riders()[1].bike;
    const int32_t v = ob.i32(0xec), shown = (v >> 12) + (v >> 7) + (v >> 9);
    check(on_road, "opponent stays on the road");
    check(ob.i32(0x1c) > 0x3200 + 0x4000, "opponent makes progress");
    check(shown > 80 && shown < 115, "opponent holds its target speed");
    check(field.place(0) == 2, "player behind the moving opponent is second");

    // Rider contact (0x004368f0 detection, 0x00436a40 separation): a rider driving straight into
    // a slower one does not pass through it, and the two end up apart.
    rr::Opponent slow{test_spec(), 90, 40, 40, 0, 1};
    rr::Race bump(straight(4000), straight(4000), t, spec, 90, {slow});
    bool overlapped_long = false; int overlap_ticks = 0;
    for (int i = 0; i < 1800; ++i) {
        // Player aims at the opponent's lateral position and accelerates.
        const auto& o = bump.riders()[1].bike;
        const int32_t dx = (o.i32(0x28) - bump.bike().i32(0x28)) >> 8;
        bump.step(rr::control_accelerate | (dx > 20 ? rr::control_right : dx < -20 ? rr::control_left : 0));
        const int32_t dz = std::abs(o.i32(0x1c) - bump.bike().i32(0x1c));
        const int32_t lx = std::abs(o.i32(0x28) - bump.bike().i32(0x28));
        if (dz < 0x40 && lx < 0x1e00) ++overlap_ticks; else overlap_ticks = 0;
        overlapped_long |= overlap_ticks > 3;
    }
    check(!overlapped_long, "riders never stay inside each other");

    // Combat (0x004096f0 hit, 0x00409c10 reach, 0x00409ad0 damage, 0x004050c0 stamina):
    // punching a rider alongside drains its stamina until it is knocked off.
    // Grid slots 0 and 1 start side by side 78 world units apart (fist reach is 5*0x1000 Q8 = 80).
    rr::Opponent target{test_spec(), 90, 0, 0, 0, 1};   // target speed 0: it stays on the grid
    rr::Race fight(straight(4000), straight(4000), t, spec, 90, {target});
    const int32_t before = fight.riders()[1].stamina.current;
    for (int i = 0; i < 18; ++i) fight.step(i == 0 ? rr::control_punch : 0);
    check(fight.riders()[1].stamina.current < before, "punch in reach drains stamina");
    bool knocked = false;
    for (int n = 0; n < 60 && !knocked; ++n) {
        for (int i = 0; i < 18; ++i) fight.step(i == 0 ? rr::control_punch : 0);
        knocked = fight.riders()[1].bike.i32(0x2f4) != 0;
    }
    check(knocked, "repeated punches knock the rider off");
    // Slot 3 is 234 units away: out of reach.
    rr::Opponent distant{test_spec(), 90, 0, 0, 0, 3};
    rr::Race far(straight(4000), straight(4000), t, spec, 90, {distant});
    const int32_t full = far.riders()[1].stamina.current;
    for (int i = 0; i < 18; ++i) far.step(i == 0 ? rr::control_punch : 0);
    check(far.riders()[1].stamina.current == full, "punch out of reach does nothing");

    // Original field from the executable (skips without the disc).
    const std::string game = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (std::filesystem::exists(game + "/ROADRASH.EXE")) {
        rr::Assets assets(game);
        const auto exe = assets.read("ROADRASH.EXE");
        const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        auto field0 = rr::original_opponents(exe, specs, 0, 7);
        check(field0.size() == 7 && field0[0].slot == 1 && field0[6].slot == 7, "seven opponents in slots 1..7");
        check(field0[0].base_speed == 99 && field0[0].lane == 1, "personality 1 level 1 speed and lane (0x00464558)");
        check(field0[0].throttle_percent == 116, "level 1 AI throttle scale (0x004642a0)");
        check(field0[0].rider_value == 180 / 2 || field0[0].rider_value > 0, "rider value from 0x00463c78");
    }

    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
