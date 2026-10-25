#include "race.hpp"
#include "traffic.hpp"
#include <iostream>
#include <stdexcept>

// Entirely synthetic PE and resource headers: no disc data or render assets.
static void word(std::vector<uint8_t>& b, size_t p, uint32_t value) {
    for (int i = 0; i < 4; ++i) b[p + i] = uint8_t(value >> (i * 8));
}
static std::vector<uint8_t> executable() {
    std::vector<uint8_t> b(0x78200);
    word(b, 0, 0x5a4d);
    word(b, 60, 0x80);
    word(b, 0x80, 0x4550);
    word(b, 0x84, 0x0001014c);
    word(b, 0x94, 0xe0);
    word(b, 0x98, 0x10b);
    word(b, 0x98 + 28, 0x400000);
    const size_t s = 0x178;
    word(b, s + 8, 0x78000);
    word(b, s + 12, 0x1000);
    word(b, s + 16, 0x78000);
    word(b, s + 20, 0x200);
    return b;
}
static std::vector<uint8_t> synthetic_container(uint32_t cel_flags) {
    std::vector<uint8_t> anim;
    auto aw = [&](uint32_t v) {
        for (int b = 0; b < 4; ++b) anim.push_back(uint8_t(v >> (b * 8)));
    };
    const uint32_t info_off = 28 + 3 * 28, ccb_off = info_off + 20;
    aw(0x5252414e);
    aw(20);
    aw(info_off);
    aw(ccb_off);
    aw(0);
    aw(0x52524644);
    aw(28 * 3 + 8);
    for (int f = 0; f < 3; ++f)
        for (int i = 0; i < 7; ++i) aw(0); // three empty frames
    aw(0x414e494d);
    aw(20);
    aw(0);
    aw(0);
    aw(3); // 'MINA' info, count at +16
    aw(0x43434220);
    for (int i = 0; i < 19; ++i) aw(0); // 'CCB ' template
    std::vector<uint8_t> cel;
    auto cw = [&](uint32_t v) {
        for (int b = 0; b < 4; ++b) cel.push_back(uint8_t(v >> (b * 8)));
    };
    // 'SNAC' wrapper: size word, type 4, then the embedded sub-container at
    // +size whose own size word leads to the cel block (0x0040b5f0 chain).
    cw(0x43414e53);
    cw(12);
    cw(4);
    cw(0x5352434e);
    cw(0x18);
    cw(1);
    cw(0);
    cw(0); // sub-container header+name
    cel.resize(48, 0);
    cw(9);
    cw(cel_flags); // cel block: count, CCB flags
    std::vector<uint8_t> container;
    auto put = [&](const std::vector<uint8_t>& v) { container.insert(container.end(), v.begin(), v.end()); };
    auto kw = [&](uint32_t v) {
        for (int b = 0; b < 4; ++b) container.push_back(uint8_t(v >> (b * 8)));
    };
    auto tag = [&](const char* t) {
        for (int i = 0; i < 4; ++i) container.push_back(uint8_t(t[i]));
    };
    tag("CRSR");
    kw(0);
    kw(0);
    tag("LBTR");
    kw(24);
    kw(0); // 24-byte header, table at 24
    tag("LBTR");
    kw(0);
    kw(3);
    kw(0); // table: three entries
    // Two identical vehicles: the original's type loop excludes the last one,
    // so a single-vehicle library can never accept a spawn.
    const uint32_t anim_off = 24 + 16 + 3 * 32, cel_off = anim_off + uint32_t(anim.size()) * 2;
    tag("MINA");
    kw(1);
    kw(anim_off);
    kw(uint32_t(anim.size())); // displays as ANIM 1
    for (int i = 0; i < 16; ++i) container.push_back(0);
    tag("MINA");
    kw(2);
    kw(anim_off + uint32_t(anim.size()));
    kw(uint32_t(anim.size()));
    for (int i = 0; i < 16; ++i) container.push_back(0);
    tag("SNAC");
    kw(1);
    kw(cel_off);
    kw(uint32_t(cel.size())); // displays as CANS 1
    for (int i = 0; i < 16; ++i) container.push_back(0);
    put(anim);
    put(anim);
    put(cel);
    return container;
}

static rr::VehicleLibrary vehicles() {
    return rr::VehicleLibrary(synthetic_container(0x100000));
}
static std::vector<rr::RouteSample> route() {
    rr::RouteSample s{{0, 0, 500, 500, 512, 512}, 0, 0, 0};
    s.path.shoulder_left = s.path.shoulder_right = 200;
    return std::vector<rr::RouteSample>(1000, s);
}
static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
int main() {
    try {
        const auto library = vehicles();
        // Both class 2 (contained) and class 3 (glancing) must finish the
        // handler after 0x004052c0 knocks the rider off: 00404f4d/00404feb.
        for (bool contained : {false, true}) {
            rr::PhysicsTables tables = rr::PhysicsTables::synthetic();
            rr::BikeSpec spec{};
            rr::Race race(route(), route(), tables, spec, 90);
            rr::TrafficSystem traffic(library, executable());
            race.set_traffic(&traffic);
            auto& rider = race.riders()[0];
            auto& b = rider.bike;
            rider.frozen = true;       // Keep known pre-contact motion/previous copies.
            rider.stamina.current = 0; // Deterministic knock-off, regardless of impulse.
            b.i32(0x2f0) = 1000;
            b.i32(0x18) = 1;
            b.i32(0x174) = 7680;
            b.i32(0x178) = 64;
            b.i32(0x17c) = 8960;
            b.i32(0x20) = 0;
            b.i32(0x28) = 0;
            b.i32(0x30) = b.i32(0x1c) - 1000;
            b.i32(0x3c) = 0;
            b.i32(0x108) = 8000;
            b.i32(0xec) = 8000;
            b.i32(0xf0) = 200;
            b.i32(0x288) = 123;
            b.i32(0x29c) = 11776;
            b.i32(0x2d8) = 196;
            b.i32(0x180) = 256;
            auto& car = traffic.cars_mut()[0];
            car.active = true;
            car.position = b.i32(0x1c);
            car.speed = 0;
            car.lateral = contained ? 0 : 18000;
            race.step(0);
            check(b.i32(0x2f4) == 1, "contact must knock rider off");
            check(b.i32(0x2f8) == (contained ? 2 : 3), "contact class must select crash reason");
            check(b.i32(0x184) == 0 && b.i32(0x188) == 0 && b.i32(0x18c) == 0 && b.u8(0x19c) == 0,
                  "knock-off must clear contact impulses and flags");
            check(b.i32(0x28) == -1504, "knock-off must apply the contact tail lateral nudge");
            if (contained) check(b.i32(0xec) == -1000, "negative contact speed must clamp to -1000");
            check(b.i32(0x288) == 123, "knock-off must skip the rate-change steering kick");
        }
        // Ascending contact walk: a car behind the bike must itself be
        // due; a due bike behind a sleeping car remains eligible.
        for (int scenario = 0; scenario < 3; ++scenario) {
            auto tables = rr::PhysicsTables::synthetic();
            rr::BikeSpec spec{};
            rr::Race race(route(), route(), tables, spec, 90);
            auto& b = race.riders()[0].bike;
            b.u8(0xc0) = 7;
            b.i32(0xc4) = 4;
            b.i32(0x174) = 7680;
            b.i32(0x178) = 64;
            b.i32(0x17c) = 8960;
            b.i32(0x194) = 256;
            b.i32(0x180) = 256;
            b.i32(0x2f0) = 1000;
            race.set_live_schedule(true);
            rr::TrafficSystem traffic(library, executable());
            traffic.set_spawn_enabled(false);
            race.set_traffic(&traffic);
            auto& car = traffic.cars_mut()[0];
            car.active = true;
            car.lateral = 0;
            car.position = b.i32(0x1c) + (scenario == 1 ? 64 : -64);
            car.last_tick = 0;
            car.next_tick = scenario == 2 ? 1 : 100;
            if (scenario == 1) car.kind = 1; // sleeping crossing car, callback still runs
            car.motion.u8(0xc0) = 7;
            car.motion.i32(0xc4) = 16;
            const int32_t before = b.i32(0x28);
            race.step(0);
            check((b.i32(0x28) != before) == (scenario != 0),
                  "car contact must gate the ascending outer node's due tick");
            check((car.motion.u8(0x1f4) != 0) == (scenario != 0),
                  "car response marks only eligible contacts");
            if (scenario == 1)
                check(car.motion.i32(0x1f8) == 240, "crossing car contact with bike starts240 tick pause");
        }
        std::cout << "car contact crash tails pass\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
