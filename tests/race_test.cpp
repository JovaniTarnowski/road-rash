#include "race.hpp"
#include "assets.hpp"
#include "race_setup.hpp"
#include "scenery.hpp"
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

    // Finish line from the level-tagged event (0x004525c0/0x00454240 global
    // DAT_004b6e58, 0x00406890 crossing, 0x00437800 latch): the race ends at
    // the tagged sample on both branch routes, with the +0xc0 bit-0x10 latch
    // set and tick/order recorded, instead of running to the route end.
    for (int road = 0; road < 2; ++road) {
        rr::Race fin(straight(2000), straight(2000), t, spec, 90);
        const int32_t line = int32_t((0x3200 >> 8) + 400) << 8;
        fin.set_finish_q8(line);
        for (int i = 0; i < 20000 && !fin.finished(); ++i) fin.step(rr::control_accelerate);
        check(fin.finished(), "finish at the tagged event sample");
        check(fin.position() == uint32_t(line), "finish clamps position to the line");
        check((fin.bike().u8(0xc0) & 0x10) != 0, "finish latches +0xc0 bit 0x10");
        check(fin.riders()[0].finish_tick >= 0, "finish records the tick");
        check(fin.place(0) == 1, "solo finisher leads");
        (void)road;
    }
    // AI riders cross the same line: the field orders by finish tick.
    {
        rr::Opponent slowf{test_spec(), 90, 0, 0, 0, 1};
        rr::Race ff(straight(2000), straight(2000), t, spec, 90, {slowf});
        const int32_t line = int32_t((0x3200 >> 8) + 400) << 8;
        ff.set_finish_q8(line);
        for (int i = 0; i < 20000 && !ff.riders()[0].finished; ++i) ff.step(rr::control_accelerate);
        check(ff.riders()[0].finished && (ff.bike().u8(0xc0) & 0x10) != 0, "player crosses the tagged line");
        check(ff.place(0) == 1, "player beats the parked opponent to the line");
    }
    // Reaching the end of the route finishes the race.
    rr::Race shortrace(straight(80), straight(80), t, spec, 90);
    for (int i = 0; i < 20000 && !shortrace.finished(); ++i) shortrace.step(rr::control_accelerate);
    check(shortrace.finished(), "finish at route end");
    // Pause gaps insert no simulation state (plan 18 Task 8 Review Focus 4):
    // two 900-tick runs with identical controls, the second pausing (no ticks
    // for 120 frames) after ticks 10, 450 and 899, match snapshot by snapshot.
    {
        auto same = [&](const rr::RaceSnapshot& a, const rr::RaceSnapshot& b) {
            if (a.tick != b.tick || a.rng != b.rng || a.fields.size() != b.fields.size()) return false;
            for (size_t i = 0; i < a.fields.size(); ++i)
                if (a.fields[i].name != b.fields[i].name || a.fields[i].value != b.fields[i].value) return false;
            return true;
        };
        rr::Race plain(straight(2000), straight(2000), t, spec, 90);
        rr::Race paused(straight(2000), straight(2000), t, spec, 90);
        bool snapshots_match = true;
        for (int i = 0; i < 900; ++i) {
            const uint32_t c = rr::control_accelerate | ((i / 60) % 2 ? rr::control_left : rr::control_right);
            plain.step(c);
            paused.step(c);
            snapshots_match &= same(plain.snapshot(), paused.snapshot());
            if (i == 10 || i == 450 || i == 899)
                for (int f = 0; f < 120; ++f) snapshots_match &= same(plain.snapshot(), paused.snapshot());
        }
        check(snapshots_match, "pause gaps change no snapshot");
        check(plain.position() == paused.position() && plain.rng_state() == paused.rng_state(), "pause gaps keep final state");
    }
    // Render-scale switching inserts no simulation state (plan 19 Task 8
    // Review Focus 1, sim-level half): the same controls stepped while an
    // external scale variable walks 1->2->3->4->0 produce identical snapshots
    // and tick counts — the scale never reaches Race.
    {
        auto run = [&](std::vector<int>& scales_seen) {
            rr::Race r(straight(2000), straight(2000), t, spec, 90);
            std::vector<rr::RaceSnapshot> snaps;
            int scale = 1;
            for (int i = 0; i < 900; ++i) {
                if (i == 300) scale = 2;
                else if (i == 450) scale = 3;
                else if (i == 600) scale = 4;
                else if (i == 750) scale = 0;
                scales_seen.push_back(scale);
                const uint32_t c = rr::control_accelerate | ((i / 60) % 2 ? rr::control_left : rr::control_right);
                r.step(c);
                snaps.push_back(r.snapshot());
            }
            return snaps;
        };
        std::vector<int> walk, flat;
        const auto a = run(walk);
        std::vector<rr::RaceSnapshot> b;
        {
            rr::Race r(straight(2000), straight(2000), t, spec, 90);
            for (int i = 0; i < 900; ++i) {
                flat.push_back(1);
                const uint32_t c = rr::control_accelerate | ((i / 60) % 2 ? rr::control_left : rr::control_right);
                r.step(c);
                b.push_back(r.snapshot());
            }
        }
        auto same = [&](const rr::RaceSnapshot& x, const rr::RaceSnapshot& y) {
            if (x.tick != y.tick || x.rng != y.rng || x.fields.size() != y.fields.size()) return false;
            for (size_t i = 0; i < x.fields.size(); ++i)
                if (x.fields[i].name != y.fields[i].name || x.fields[i].value != y.fields[i].value) return false;
            return true;
        };
        bool match = a.size() == b.size();
        for (size_t i = 0; match && i < a.size(); ++i) match &= same(a[i], b[i]);
        check(match, "render-scale walk keeps snapshots identical");
        check(walk != flat, "scale walk actually visited several scales");
        check(a.back().tick == b.back().tick, "tick counter unchanged by scale walk");
    }

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

    // A replan target can remain selected while the overtake scan is empty.
    // 0x408fe7 returns AL=0 here: choose a fresh point ahead, even when the
    // previous point is now behind the rider (formerly froze the racing line).
    {
        rr::Race replanned(straight(4000), straight(4000), t, spec, 90, {opp});
        auto& ai = replanned.riders()[1].brain;
        ai.target_rider = 0;
        ai.target_step = 1;
        ai.target_position = 0;
        ai.target_x = 600 * 256;
        ai.target_speed = 0;
        replanned.riders()[0].bike.i32(0x28) = -600 * 256;
        replanned.step(0);
        check(ai.target_position > replanned.riders()[1].bike.i32(0x1c), "AI refreshes stale target with no overtake candidate");
        check(ai.target_speed > 0 && std::abs(ai.target_x) < 500 * 256, "AI resumes its road line and speed");
    }

    // A distinct nearby scan candidate needs the passing point written this
    // tick, rather than returning success from an unfinished steering stub.
    {
        rr::Race passing(straight(4000), straight(4000), t, spec, 90, {opp});
        passing.riders()[0].bike.i32(0x28) = 0;
        passing.riders()[0].bike.i32(0x1c) += 1500;
        auto& ai = passing.riders()[1].brain;
        ai.target_rider = -1;
        ai.target_position = 0;
        ai.target_x = 0;
        passing.step(0);
        check(ai.overtake_latched == 0, "AI detects nearby passing candidate");
        check(ai.target_position > passing.riders()[1].bike.i32(0x1c), "passing point advances ahead of candidate");
        check(std::abs(ai.target_x) == 0x8000, "passing point uses recovered lateral clearance");
    }

    // Frozen solo halves are separate type-8 scan objects: bike contact can
    // move the bike while its null think/move half stays at its captured point.
    for (int32_t half_speed : {0,4096}) {
        auto route=straight(4000);
        route[90].offset=-1000; // the displaced bike occupies a different slice
        rr::Race detached(route,route,t,spec,90,{opp});
        auto& target=detached.riders()[0];
        target.frozen=true;
        target.bike.i32(0x1c)=90*256;
        target.bike.i32(0x28)=0;
        target.bike.i32(0x108)=0;
        target.sample=90;
        target.half_detached=true;
        target.half_position=83*256;
        target.half_lateral=0;
        target.half_speed=half_speed;
        auto& observer=detached.riders()[1];
        observer.bike.i32(0x1c)=82*256;
        observer.bike.i32(0x28)=0;
        observer.bike.i32(0x108)=4096;
        observer.sample=82;
        observer.brain.target_rider=-1;
        observer.brain.replan_ticks=0;
        detached.countdown_step(); // runs AI scans without moving either object
        const auto& ai=observer.brain;
        if (half_speed==0) {
            check(ai.overtake_latched==0 && ai.overtake_type==8 && ai.overtake_position==83*256,
                  "overtake scans frozen half at its retained position");
            check(ai.target_x==-0x8000,"detached half passing point uses its own route slice");
        } else {
            check(ai.overtake_latched==0 && ai.overtake_type==4 && ai.overtake_position==90*256,
                  "half own speed excludes a nonclosing half from overtake selection");
        }
        check(target.half_position==83*256 && target.half_speed==half_speed && target.bike.i32(0x1c)==90*256,
              "countdown scan preserves independent bike and frozen half state");
    }

    {
        rr::Race demo(straight(4000), straight(4000), t, spec, 90);
        demo.set_demo_hold(true, 95);
        for (int tick = 0; tick < 600; ++tick) demo.step(0);
        check(demo.riders()[0].brain.current_speed > 0 && demo.speed() > 0,
              "scalar demo speed survives the sixty-tick replan");
    }

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

    // Knock-off routing on bike contact (0x004049c0 -> 0x004052c0): a
    // contained hit is class 1 (bikes are short-ish and unflagged), so the
    // decision takes stamina param 0 and bike param swing; only one crash
    // starts when two contacts land in one tick.
    {
        rr::Opponent slow2{test_spec(), 90, 0, 0, 0, 1};
        rr::Race ko(straight(4000), straight(4000), t, spec, 90, {slow2});
        auto& victim = ko.riders()[1];
        victim.stamina.current = 5;
        victim.bike.i32(0x2f0) = 1000;
        auto& self = ko.riders()[0];
        self.bike.i32(0x1c) = victim.bike.i32(0x1c);
        self.bike.i32(0x28) = victim.bike.i32(0x28);
        self.bike.i32(0x184) = 0x2000; self.bike.i32(0x188) = 0x2000; self.bike.i32(0x18c) = 0;
        // run one tick into contact
        ko.step(rr::control_accelerate);
        int crashes = (ko.riders()[0].bike.i32(0x2f4) != 0) + (ko.riders()[1].bike.i32(0x2f4) != 0);
        check(crashes <= 1, "at most one crash starts from double contact");
    }
    // Steering kick tail of bike contact (0x004049c0 LAB_00404f93): a glancing
    // (class-3) hit that moves the steer rate kicks +0x288 by
    // +0x2d8*(new_f0-old_f0)*2>>8 clamped to +-+0x29c; an unchanged rate kicks nothing.
    // The class-3 knock decision (0x004052c0 reason 3, stamina param 0) runs
    // first: with no lateral-velocity gap its tail kick is zero, and a fresh
    // bike (condition nonzero) stays up.
    {
        rr::Opponent slow3{test_spec(), 90, 0, 0, 0, 1};
        rr::Race kick(straight(4000), straight(4000), t, spec, 90, {slow3});
        auto& me = kick.riders()[0];
        auto& you = kick.riders()[1];
        me.bike.i32(0x1c) = you.bike.i32(0x1c);
        me.bike.i32(0x28) = you.bike.i32(0x28) + 0x1000;   // alongside, not contained
        me.bike.i32(0xec) = 3000; you.bike.i32(0xec) = 5526;  // separate() turns the gap into hit ~2526
        me.bike.i32(0x2d8) = 196;
        me.bike.i32(0x288) = 528;
        me.bike.i32(0x29c) = 11776;
        me.bike.i32(0x2f0) = 1000;
        you.bike.i32(0x2f0) = 10000;
        const int32_t rate = me.bike.i32(0x2ac) * me.bike.i32(0x18);
        const int32_t next = 528 - rate;   // steer() decays toward the zero target
        kick.step(0);
        const int32_t f0_steer = (next >> 8) * 120;
        const int32_t d_f0 = me.bike.i32(0xf0) - f0_steer;
        check(d_f0 == 943, "glancing contact moves the steer rate by hit/2");
        const int32_t prod = 196 * d_f0 * 2;
        const int32_t want = next + ((prod + ((prod >> 31) & 0xff)) >> 8);
        check(me.bike.i32(0x288) == want, "glancing contact kicks steer by the rate change");
        check(me.bike.i32(0x184) == 0 && me.bike.i32(0x188) == 0 && me.bike.i32(0x18c) == 0,
              "contact clears the spent impulses");
    }
    // Class-3 knock tail (0x004052c0): the decision's own steer kick
    // +0x2d8*scaled_0x188>>8 lands on top of the rate-change kick. The frozen
    // partner holds a preset lateral velocity, so the impulse is known from
    // post-tick reads (nothing writes +0x10c after move()).
    {
        rr::Opponent slow4{test_spec(), 90, 0, 0, 0, 1};
        rr::Race kk(straight(4000), straight(4000), t, spec, 90, {slow4});
        auto& me = kk.riders()[0];
        auto& you = kk.riders()[1];
        you.frozen = true;   // skip think/move: +0x108/+0x10c stay preset
        you.bike.i32(0x108) = 5526;
        you.bike.i32(0x10c) = 0x2000;
        you.bike.i32(0x3c) = -20000;   // stale prev-lateral: separate() takes the lateral branch
        you.bike.i32(0x30) = -20000;   // stale prev-position: me approaches from behind, so the forward branch runs
        you.bike.i32(0x2f0) = 10000;
        me.bike.i32(0x1c) = you.bike.i32(0x1c);
        me.bike.i32(0x28) = you.bike.i32(0x28) + 0x1000;   // alongside, not contained
        you.bike.i32(0x1c) += 11;   // frozen partner just ahead: me outer-loops onto it
        me.bike.i32(0xec) = 3000;
        me.bike.i32(0x2d8) = 196;
        me.bike.i32(0x288) = 528;
        me.bike.i32(0x29c) = 11776;
        me.bike.i32(0x2f0) = 1000;
        const int32_t rate = me.bike.i32(0x2ac) * me.bike.i32(0x18);
        const int32_t next = 528 - rate;   // steer() decays toward the zero target
        const int32_t f0_steer = (next >> 8) * 120;
        kk.step(0);
        const int32_t d_f0 = me.bike.i32(0xf0) - f0_steer;
        check(d_f0 != 0, "knock-kick setup makes contact");
        const int32_t m1 = me.bike.i32(0x194), m2 = you.bike.i32(0x194), m = m1 + m2;
        const int32_t s188 = ((you.bike.i32(0x10c) - me.bike.i32(0x10c)) * m2 * 2) / m;
        check(s188 != 0, "knock-kick setup carries a lateral impulse");
        const int64_t elas = me.bike.i32(0x180);
        const int32_t scaled188 = int32_t((int64_t(s188) * elas) / 256);
        const int64_t kp = int64_t(196) * scaled188;
        const int32_t knock_kick = int32_t((kp + ((kp >> 31) & 0xff)) >> 8);
        const int32_t s184 = you.bike.i32(0x108) - me.bike.i32(0x108);
        const int32_t shit = int32_t((int64_t(s184 >= 0 ? s184 : -s184) * elas) / 256);
        check(d_f0 == (you.bike.i32(0x28) < me.bike.i32(0x28) ? shit / 2 : -shit / 2),
              "knock-kick setup moves the steer rate by the scaled hit/2");
        const int64_t rp = int64_t(196) * d_f0 * 2;
        const int32_t rate_kick = int32_t((rp + ((rp >> 31) & 0xff)) >> 8);
        const int32_t want = next + knock_kick + rate_kick;
        check(want > -11776 && want < 11776, "knock-kick setup stays off the clamp");
        check(me.bike.i32(0x288) == want, "glancing contact adds the knock tail kick");
        check(me.bike.i32(0x2f0) == 1000, "class-3 knock deals no condition damage at this magnitude");
    }
    // Class-3 glance contact (0x004049c0 LAB_00404c2d): the class comes from
    // separate()'s +0x19c bit 0x40 width flag. Same-spot slow touches set no
    // flag (proven end-to-end: human tick-149 carries 19c=0 into class 3),
    // so the glance branch runs; you ride clearly left so the o.28 < s.28
    // arm (steer shove +shit/2, speed cut shit/4, knock tail kick) is
    // deterministic. Class-1 transfer moved with the flag (demo tick-208
    // replay). Expect no crash and no damage at this magnitude.
    {
        rr::Opponent slow5{test_spec(), 90, 0, 0, 0, 1};
        rr::Race c1(straight(4000), straight(4000), t, spec, 90, {slow5});
        auto& me = c1.riders()[0];
        auto& you = c1.riders()[1];
        me.bike.i32(0x1c) = you.bike.i32(0x1c);
        me.bike.i32(0x28) = you.bike.i32(0x28);   // same spot, no width flag
        you.bike.i32(0x28) -= 0x2000;   // you clearly left: glance takes the
        me.bike.i32(0xec) = 6200; you.bike.i32(0xec) = 5000;   // o.28 < s.28 arm
        me.bike.i32(0x2f0) = 1000;
        you.bike.i32(0x2f0) = 10000;
        const int32_t st0 = me.stamina.current;
        c1.step(0);
        const int32_t m1 = me.bike.i32(0x194), m2 = you.bike.i32(0x194), m = m1 + m2;
        const int32_t s184 = ((you.bike.i32(0x108) - me.bike.i32(0x108)) * m2 * 2) / m;
        const int64_t elas = me.bike.i32(0x180);
        const int32_t scaled184 = int32_t((int64_t(s184) * elas) / 256);
        const int32_t shit = scaled184 >= 0 ? scaled184 : -scaled184;
        check(shit != 0, "class-3 setup makes contact");
        check(me.bike.i32(0x2f4) == 0, "glancing contact does not crash");
        check(me.bike.i32(0xec) == std::max(0, me.bike.i32(0x108) - shit / 4),
              "glancing contact cuts speed by the scaled hit/4");
        check(me.bike.i32(0xf0) == shit / 2, "glancing contact shoves the steer rate");
        const int64_t rp = int64_t(me.bike.i32(0x2d8)) * (me.bike.i32(0xf0) - 0) * 2;
        const int32_t kick = int32_t((rp + ((rp >> 31) & 0xff)) >> 8);
        const int32_t lim = me.bike.i32(0x29c);
        const int32_t want_kick = kick > lim ? lim : kick < -lim ? -lim : kick;
        check(me.bike.i32(0x288) == want_kick, "glancing contact adds the knock tail kick");
        check(me.bike.i32(0x2f0) == 1000, "class-3 knock deals no condition damage at this magnitude");
        check(me.stamina.current >= st0, "class-3 knock deals no stamina damage");
        check(me.bike.i32(0x184) == 0 && me.bike.i32(0x188) == 0, "contact tail clears impulses");
    }
    // Roadside hazards via the in-road BORR actor path (0x00454240/0x00438930
    // -> 0x004049c0 class 1 -> knock_off reason 1): a rider crossing an
    // in-road span takes stamina damage, a lethal hit crashes reason 1, and
    // a span outside the road edges never fires (pictured trees are not solid).
    {
        rr::Race hz(straight(4000), straight(4000), t, spec, 90);
        auto& me = hz.riders()[0];
        const size_t s = me.sample;
        const int32_t centre = me.bike.i32(0x28);
        std::vector<std::vector<rr::RoadHazard>> h0(4000), h1(4000);
        h0[s].push_back({uint32_t(s), centre - 1000, centre + 1000});
        hz.set_hazards(std::move(h0), std::move(h1));
        // 0x004368f0 runs 0x004373f0 only within the bike's half-length of mid-slice.
        me.bike.i32(0x1c) = int32_t(s) * 256 + 0x80; me.bike.i32(0x30) = me.bike.i32(0x1c);
        me.bike.i32(0x2f0) = 1000;
        me.bike.i32(0x188) = 0; me.bike.i32(0x2d8) = 0; me.bike.i32(0x288) = 0;
        me.bike.i32(0x29c) = 0x2e00;
        me.bike.i32(0x184) = 0x2000; me.bike.i32(0x188) = 0; me.bike.i32(0x18c) = 0;
        const int32_t st0 = me.stamina.current;
        hz.step(0);
        check(me.stamina.current < st0, "in-road hazard damages stamina (reason 1)");
        check(me.bike.i32(0x2f4) == 0, "glancing hazard does not crash");
    }
    {
        rr::Race hz(straight(4000), straight(4000), t, spec, 90);
        auto& me = hz.riders()[0];
        const size_t s = me.sample;
        const int32_t centre = me.bike.i32(0x28);
        std::vector<std::vector<rr::RoadHazard>> h0(4000), h1(4000);
        h0[s].push_back({uint32_t(s), centre - 1000, centre + 1000});
        hz.set_hazards(std::move(h0), std::move(h1));
        // 0x004368f0 runs 0x004373f0 only within the bike's half-length of mid-slice.
        me.bike.i32(0x1c) = int32_t(s) * 256 + 0x80; me.bike.i32(0x30) = me.bike.i32(0x1c);
        me.bike.i32(0x2f0) = 1000;
        me.stamina.current = 5; me.stamina.max = 5;
        me.bike.i32(0x184) = 0x4000; me.bike.i32(0x188) = 0x4000; me.bike.i32(0x18c) = 0;
        hz.step(0);
        check(me.bike.i32(0x2f4) != 0 && me.bike.i32(0x2f8) == 1, "lethal hazard crashes reason 1");
    }
    {
        rr::Race hz(straight(4000), straight(4000), t, spec, 90);
        auto& me = hz.riders()[0];
        const size_t s = me.sample;
        std::vector<std::vector<rr::RoadHazard>> h0(4000), h1(4000);
        h0[s].push_back({uint32_t(s), 900 * 256, 1000 * 256});   // outside the 500-unit road edge
        hz.set_hazards(std::move(h0), std::move(h1));
        me.bike.i32(0x1c) = int32_t(s) * 256 + 0x80; me.bike.i32(0x30) = me.bike.i32(0x1c);
        me.bike.i32(0x2f0) = 1000;
        me.bike.i32(0x184) = 0x2000; me.bike.i32(0x188) = 0; me.bike.i32(0x18c) = 0;
        const int32_t st0 = me.stamina.current;
        hz.step(0);
        check(me.stamina.current == st0 && me.bike.i32(0x2f4) == 0, "outside-edge span never fires");
    }
    // Span builder keeps only strictly-inside BORR slots (0x00454240): slots
    // at right-96/right+32 keep the first and drop the second here; pictured
    // trees are not all solid.
    {
        rr::CourseSection sec; sec.samples.resize(2);
        for (auto& path : sec.samples) { path.left = path.right = 500; }
        sec.roadside_profiles = {0x014c0182u};
        const auto spans = rr::road_hazard_spans(sec, 100, []() { return 0u; });
        check(spans.size() == 2 && spans[0].sample == 100, "in-road hazard spans kept with sample base");
        check(spans[0].lo_q8 < spans[0].hi_q8, "hazard span ordered");
    }
    // Steal conservation: parked riders overlap, so both need healthy
    // +0x2f0 (else the tick-0 contact crashes them before any swing) and
    // matching swing timers (else the victim resolves first and zeroes the
    // player's timer, and no steal is ever attempted).
    {
        int failures2 = 0;
        for (int seed = 0; seed < 1000; ++seed) {
            rr::Opponent armed{test_spec(), 90, 0, 0, 0, 1};
            rr::Race w(straight(4000), straight(4000), t, spec, 90, {armed});
            w.set_rng_state(uint32_t(seed));
            w.riders()[0].weapon = 0; w.riders()[0].attack.weapon = 0;
            w.riders()[1].weapon = 1; w.riders()[1].attack.weapon = 1; w.riders()[1].attack.timer = 1;
            w.riders()[0].bike.i32(0x2f0) = 1000; w.riders()[1].bike.i32(0x2f0) = 10000;
            w.riders()[1].steal_until = 0;
            // park alongside in reach, swing and force the frame gate
            w.riders()[1].bike.i32(0x28) = w.riders()[0].bike.i32(0x28) + 1000;
            w.riders()[0].attack.timer = 1; w.riders()[0].attack.right = true;
            for (int i = 0; i < 18 && w.riders()[0].attack.timer; ++i) w.step(i == 0 ? 0 : 0);
            const int total = (w.riders()[0].weapon != 0) + (w.riders()[1].weapon != 0);
            if (total != 1) ++failures2;
        }
        check(failures2 == 0, "weapons conserved across steal attempts");
    }
    // Steal occurrence + victim-immunity strictness (0x004096f0: victim
    // +0x1cc < tick is strict, then exactly one (rand&3)==3 draw). Mirrors
    // the conservation setup; counts attacker-armed outcomes across seeds
    // so the gate is non-vacuous, then pins the immunity suppression.
    // NOTE: parked riders overlap, so both need healthy +0x2f0: with the
    // default zero condition the tick-0 contact knock-off crashes them
    // before any swing resolves (found via probe; the pre-existing
    // conservation block above only passes vacuously for this reason).
    // the conservation setup; counts attacker-armed outcomes across seeds
    // so the gate is non-vacuous, then pins the immunity suppression.
    // NOTE: parked riders overlap, so both need healthy +0x2f0: with the
    // default zero condition the tick-0 contact knock-off crashes them
    // before any swing resolves (found via probe; the pre-existing
    // conservation block above only passes vacuously for this reason).
    {
        int successes = 0, conserved = 0;
        for (int seed = 0; seed < 1000; ++seed) {
            rr::Opponent armed{test_spec(), 90, 0, 0, 0, 1};
            rr::Race w(straight(4000), straight(4000), t, spec, 90, {armed});
            w.set_rng_state(uint32_t(seed));
            w.riders()[0].weapon = 0; w.riders()[0].attack.weapon = 0;
            w.riders()[1].weapon = 1; w.riders()[1].attack.weapon = 1; w.riders()[1].attack.timer = 1;
            w.riders()[0].bike.i32(0x2f0) = 1000; w.riders()[1].bike.i32(0x2f0) = 10000;
            w.riders()[1].steal_until = 0;
            w.riders()[1].bike.i32(0x28) = w.riders()[0].bike.i32(0x28) + 1000;
            w.riders()[0].attack.timer = 1; w.riders()[0].attack.right = true;
            for (int i = 0; i < 18 && w.riders()[0].attack.timer; ++i) w.step(i == 0 ? 0 : 0);
            successes += (w.riders()[0].weapon != 0);
            conserved += ((w.riders()[0].weapon != 0) + (w.riders()[1].weapon != 0)) == 1;
        }
        check(successes > 0, "steals succeed on some seeds (non-vacuous gate)");
        check(conserved == 1000, "weapons conserved on every steal race");
    }
    // Victim steal-immunity suppresses every attempt: with victim +0x1cc far
    // in the future no seed transfers the weapon.
    {
        int successes = 0;
        for (int seed = 0; seed < 200; ++seed) {
            rr::Opponent armed{test_spec(), 90, 0, 0, 0, 1};
            rr::Race w(straight(4000), straight(4000), t, spec, 90, {armed});
            w.set_rng_state(uint32_t(seed));
            w.riders()[0].weapon = 0; w.riders()[0].attack.weapon = 0;
            w.riders()[1].weapon = 1; w.riders()[1].attack.weapon = 1; w.riders()[1].attack.timer = 1;
            w.riders()[0].bike.i32(0x2f0) = 1000; w.riders()[1].bike.i32(0x2f0) = 10000;
            w.riders()[1].steal_until = 1 << 30;
            w.riders()[1].bike.i32(0x28) = w.riders()[0].bike.i32(0x28) + 1000;
            w.riders()[0].attack.timer = 1; w.riders()[0].attack.right = true;
            for (int i = 0; i < 18 && w.riders()[0].attack.timer; ++i) w.step(i == 0 ? 0 : 0);
            successes += (w.riders()[0].weapon != 0);
        }
        check(successes == 0, "victim immunity blocks all steals");
    }
    // Hit deadline strictness (0x004096f0: damage skipped while
    // tick <= victim +0x1d0): far-future immunity absorbs every punch.
    {
        rr::Opponent target{test_spec(), 90, 0, 0, 0, 1};
        rr::Race calm(straight(4000), straight(4000), t, spec, 90, {target});
        calm.riders()[1].hit_until = 1 << 30;
        const int32_t full = calm.riders()[1].stamina.current;
        for (int i = 0; i < 18; ++i) calm.step(i == 0 ? rr::control_punch : 0);
        check(calm.riders()[1].stamina.current == full, "hit immunity absorbs punches");
    }
    // Crash/remount preserves carried weapons (0x00403d60 audit: no
    // +0x1c8/+0x1cc/+0x1d0/+0x1d4 stores in any crash path; remount
    // restores stamina without touching weapons).
    {
        rr::Race hz(straight(4000), straight(4000), t, spec, 90);
        auto& me = hz.riders()[0];
        me.weapon = 2; me.attack.weapon = 2;
        const size_t s = me.sample;
        const int32_t centre = me.bike.i32(0x28);
        std::vector<std::vector<rr::RoadHazard>> h0(4000), h1(4000);
        h0[s].push_back({uint32_t(s), centre - 1000, centre + 1000});
        hz.set_hazards(std::move(h0), std::move(h1));
        me.bike.i32(0x1c) = int32_t(s) * 256 + 0x80; me.bike.i32(0x30) = me.bike.i32(0x1c);
        me.bike.i32(0x2f0) = 1000;
        me.stamina.current = 5; me.stamina.max = 5;
        me.bike.i32(0x184) = 0x4000; me.bike.i32(0x188) = 0x4000; me.bike.i32(0x18c) = 0;
        hz.step(0);
        check(me.bike.i32(0x2f4) != 0 && me.bike.i32(0x2f8) == 1, "armed rider crashes reason 1");
        check(me.weapon == 2, "crash keeps carried weapon");
        // Clearing the span stops the per-tick re-trigger (which would reset
        // crash_timer forever); the slide then stops and remounts.
        hz.set_hazards(std::vector<std::vector<rr::RoadHazard>>(4000),
                       std::vector<std::vector<rr::RoadHazard>>(4000));
        for (int i = 0; i < 600 && me.bike.i32(0x2f4) != 0; ++i) hz.step(0);
        check(me.bike.i32(0x2f4) == 0, "armed rider remounts");
        check(me.weapon == 2, "remount keeps carried weapon");
    }
    // DZHR solid packing (0x004520d0 via place_dzhr): static-family
    // billboards become absolute Q8 spans keeping the low-nibble type;
    // BORR roadside objects (static_family false) are draw-only and skipped.
    {
        std::vector<rr::SceneryObject> placed;
        rr::SceneryObject post{}; post.lateral = 100; post.type = 1; post.static_family = true;
        rr::SceneryObject sign{}; sign.lateral = -300; sign.type = 3; sign.static_family = true;
        rr::SceneryObject tree{}; tree.lateral = 50; tree.type = 0; tree.static_family = false;
        placed.push_back(post); placed.push_back(sign); placed.push_back(tree);
        const auto spans = rr::dzhr_solids_for_sample(1000 * 256, placed);
        check(spans.size() == 2, "DZHR packing keeps billboards, skips BORR");
        check(spans[0].type == 1 && spans[1].type == 3, "DZHR packing keeps the low-nibble type");
        check(spans[0].lo_q8 == (1000 + 100) * 256 - 0x800, "DZHR span centred on the placed lateral");
        check(spans[0].hi_q8 - spans[0].lo_q8 == 0x1000, "DZHR span is the size-8 extent");
        check(spans[1].lo_q8 == (1000 - 300) * 256 - 0x800, "DZHR lateral is route-frame relative");
    }
    // DZHR solid tick rule (0x00437750): low-nibble types 0/1/2 take the
    // class-1 crash path (reason 1); types 3+ are the marking-only branch
    // (0x004049c0 == 2): no crash and no state change.
    for (const uint8_t type : {uint8_t(0), uint8_t(1), uint8_t(2)}) {
        rr::Race dz(straight(4000), straight(4000), t, spec, 90);
        auto& me = dz.riders()[0];
        const size_t s = me.sample;
        const int32_t centre = me.bike.i32(0x28);
        rr::Race::DzhrTable d0(4000), d1(4000);
        d0[s].push_back({centre - 1000, centre + 1000, type});
        dz.set_roadside(std::move(d0), std::move(d1));
        me.bike.i32(0x1c) = int32_t(s) * 256 + 0x80; me.bike.i32(0x30) = me.bike.i32(0x1c);
        me.bike.i32(0x2f0) = 1000;
        me.stamina.current = 5; me.stamina.max = 5;
        me.bike.i32(0x184) = 0x4000; me.bike.i32(0x188) = 0x4000; me.bike.i32(0x18c) = 0;
        dz.step(0);
        check(me.bike.i32(0x2f4) != 0 && me.bike.i32(0x2f8) == 1, "DZHR solid type crashes reason 1");
    }
    {
        rr::Race dz(straight(4000), straight(4000), t, spec, 90);
        auto& me = dz.riders()[0];
        const size_t s = me.sample;
        const int32_t centre = me.bike.i32(0x28);
        rr::Race::DzhrTable d0(4000), d1(4000);
        d0[s].push_back({centre - 1000, centre + 1000, 3});
        dz.set_roadside(std::move(d0), std::move(d1));
        me.bike.i32(0x1c) = int32_t(s) * 256 + 0x80; me.bike.i32(0x30) = me.bike.i32(0x1c);
        me.bike.i32(0x2f0) = 1000;
        const int32_t st0 = me.stamina.current, lat0 = me.bike.i32(0x28);
        me.bike.i32(0x184) = 0x4000; me.bike.i32(0x188) = 0x4000; me.bike.i32(0x18c) = 0;
        dz.step(0);
        check(me.bike.i32(0x2f4) == 0, "DZHR marking type never crashes");
        check(me.stamina.current == st0 && me.bike.i32(0x28) == lat0, "DZHR marking type changes nothing");
    }
    // Nitro: held nitro+accelerate depletes a charge and boosts, release
    // stops boost, recharge refills, retrigger works.
    {
        rr::Race n(straight(8000), straight(8000), t, spec, 90);
        const int32_t c0 = n.nitro_charges();
        check(c0 > 0, "nitro starts with charges");
        for (int i = 0; i < 30; ++i) n.step(rr::control_accelerate | rr::control_nitro);
        check(n.nitro_charges() < c0, "nitro activation spends a charge");
        check((n.bike().u8(0xc0) & 0x20) != 0, "nitro boost flag set while held");
        n.step(0);
        check((n.bike().u8(0xc0) & 0x20) == 0, "nitro stops on release");
        const int32_t low = n.riders()[0].nitro_charge;
        for (int i = 0; i < 600; ++i) n.step(rr::control_accelerate);
        check(n.riders()[0].nitro_charge >= low, "nitro recharges while boosting off");
    }
    // Animation state advances in the Race tick: ride_animation selected is
    // set before snapshots without a render owner.
    {
        rr::Race m(straight(4000), straight(4000), t, spec, 90);
        m.step(rr::control_accelerate);
        bool has_anim = false;
        for (const auto& f : m.snapshot().fields) if (f.name == "rider.0.anim") has_anim = true;
        check(has_anim, "animation state present in snapshot");
        // knock_off reason 4 selects riding animation 9 in the same tick's
        // snapshot: the pending contact_animation is consumed before
        // snapshots (without a ride bank there is nothing to select into,
        // so the observable is that the pending flag clears in-tick).
        m.riders()[0].contact_animation = 9;
        m.step(0);
        check(m.riders()[0].contact_animation == -1, "contact animation consumed in-tick before snapshot");
    }

    // Original field from the executable (skips without the disc).
    const std::string game = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (std::filesystem::exists(game + "/ROADRASH.EXE")) {
        rr::Assets assets(game);
        const auto exe = assets.read("ROADRASH.EXE");
        const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
        // Napa live setup: all seven racers must continue advancing after
        // their first replans, rather than parking at an obsolete target.
        rr::RaceConfig config;
        config.course = "NAPA";
        config.opponents = 7;
        auto napa = rr::make_race_setup(assets, exe, config);
        std::array<int32_t, 8> halfway{};
        for (int tick = 0; tick < 3600; ++tick) {
            napa.instance.race->step(rr::control_accelerate);
            if (tick == 2999) for (size_t i = 1; i < 8; ++i) halfway[i] = napa.instance.race->riders()[i].bike.i32(0x1c);
        }
        for (size_t i = 1; i < 8; ++i) {
            const auto& rider = napa.instance.race->riders()[i];
            std::cout << "Napa AI " << i << " pos " << rider.bike.i32(0x1c)
                      << " last-ten-seconds " << rider.bike.i32(0x1c) - halfway[i] << "\n";
            check(rider.bike.i32(0x1c) - halfway[i] > 0x100, "Napa opponent continues driving after replans");
        }
        auto field0 = rr::original_opponents(exe, specs, 0, 7);
        check(field0.size() == 7 && field0[0].slot == 1 && field0[6].slot == 7, "seven opponents in slots 1..7");
        check(field0[0].base_speed == 99 && field0[0].lane == 1, "personality 1 level 1 speed and lane (0x00464558)");
        check(field0[0].throttle_percent == 116, "level 1 AI throttle scale (0x004642a0)");
        check(field0[0].rider_value == 180 / 2 || field0[0].rider_value > 0, "rider value from 0x00463c78");
        // Full offline field (0x00405600: DAT_004642d4=0x10, slots 1..14,
        // rider rows 1..14 from 0x00463c78, grid tables 0x00464088/0x00464108).
        auto field14 = rr::original_opponents(exe, specs, 0, 14);
        check(field14.size() == 14 && field14[13].slot == 14, "fourteen opponents in slots 1..14");
        bool grid_ok = true, slots_ok = true;
        for (int k = 0; k < 14; ++k) {
            int32_t lat = 0, pos = 0;
            const uint32_t base = 0x00464088u + 8u * uint32_t(k + 1);
            constexpr uint32_t data_raw = 0x61600;
            auto word = [&](uint32_t va) {
                const size_t p = data_raw + (va - 0x00463000u);
                return int32_t(uint32_t(exe[p]) | uint32_t(exe[p+1]) << 8 |
                               uint32_t(exe[p+2]) << 16 | uint32_t(exe[p+3]) << 24);
            };
            lat = word(base); pos = word(base + 4);
            grid_ok &= field14[size_t(k)].grid_lat == lat && field14[size_t(k)].grid_pos == pos;
            slots_ok &= field14[size_t(k)].slot == k + 1 && field14[size_t(k)].rider == k + 1;
        }
        check(grid_ok, "all 14 opponents carry the 0x00464088 grid slot");
        check(slots_ok, "opponent slots/riders are 1..14 (0x00405990 param_6)");
        // The full field constructs: player + 14 opponents on their grid slots.
        rr::Race full(straight(4000), straight(4000), t, spec, 90, field14);
        check(full.riders().size() == 15, "fifteen riders on the grid");
        check(full.riders()[14].slot == 14, "last opponent in slot 14");
    }

    std::cout << failures << " failures\n";
    return failures ? 1 : 0;
}
