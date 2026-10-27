// Plan 34 Task 3 Step 3: rollback gives the same race as a straight run of
// the sealed inputs. Needs the disc (77 without it). A sealed timeline with
// late (repeat-last) substitutions and an AI hand-over is delivered to a
// rollback client with random delays up to the depth under test (and out of
// order); the client predicts, corrects and must end on the straight run's
// state hash, with the same per-60-tick hashes, for every depth 0..16 with
// two humans and several depths with eight.
#include "assets.hpp"
#include "check.hpp"
#include "golden.hpp"
#include "net/rollback.hpp"

#include <filesystem>
#include <iostream>
#include <map>

namespace {

constexpr int countdown = 60; // shortened countdown keeps the test fast; the client uses 0x168

uint32_t intent(int seat, int t) {
    uint32_t w = rr::control_accelerate;
    const int phase = (t / (31 + 9 * seat)) % 5;
    if (phase == 1) w |= rr::control_left;
    if (phase == 3) w |= rr::control_right;
    if ((t + 5 * seat) % 41 < 4) w |= rr::control_punch;
    return w;
}

struct Timeline {
    std::vector<std::vector<uint32_t>> words;
    std::vector<uint8_t> present, late;
};

// What a server would seal: each seat's intended word unless "late" (then
// the seat's previous sealed word); seat 1 is AI-driven for a stretch.
Timeline seal(int humans, int ticks, uint32_t seed) {
    Timeline tl;
    std::vector<uint32_t> last(size_t(humans), 0u);
    uint32_t x = seed;
    for (int t = 0; t < ticks; ++t) {
        std::vector<uint32_t> w(size_t(humans), 0u);
        uint8_t late = 0, present = uint8_t((1u << humans) - 1u);
        if (humans > 1 && t >= 300 && t < 420) present = uint8_t(present & ~2u);
        for (int s = 0; s < humans; ++s) {
            x = x * 1103515245u + 12345u;
            const bool is_late = (x >> 16) % 100 < 12;
            w[size_t(s)] = is_late ? last[size_t(s)] : intent(s, t);
            if (is_late) late |= uint8_t(1u << s);
            last[size_t(s)] = w[size_t(s)];
        }
        tl.words.push_back(w);
        tl.present.push_back(present);
        tl.late.push_back(late);
    }
    return tl;
}

rr::RaceInstance fresh(const rr::Assets& assets, const std::vector<uint8_t>& exe, int humans,
                       std::unique_ptr<rr::RaceSetup>& keep) {
    rr::NetRaceConfig net;
    for (int k = 0; k < humans; ++k) net.humans.push_back({k, 1 + k % 8, -1, "P"});
    net.ai_bikers = humans > 4 ? 1 : 2;
    net.seed = 1234;
    keep = std::make_unique<rr::RaceSetup>(
        rr::make_race_setup(assets, exe, rr::net_race_config("NAPA", 0, net)));
    return std::move(keep->instance);
}

struct Result {
    uint64_t final_hash = 0;
    std::map<int32_t, uint64_t> hashes;
    int max_depth = 0, stalls = 0;
};

Result straight(const rr::Assets& assets, const std::vector<uint8_t>& exe, int humans, const Timeline& tl) {
    std::unique_ptr<rr::RaceSetup> keep;
    rr::RaceInstance in = fresh(assets, exe, humans, keep);
    // A rollback client with depth 0 that holds every frame up front runs
    // the sealed inputs straight through.
    rr::net::Rollback r(in, humans, 0, 0, countdown);
    for (size_t t = 0; t < tl.words.size(); ++t) r.sealed(int32_t(t), tl.words[t], tl.late[t], tl.present[t]);
    r.advance_to(int32_t(tl.words.size()));
    Result out;
    for (const auto& h : r.take_hashes()) out.hashes[h.tick] = h.hash;
    out.final_hash = rr::state_hash(in);
    return out;
}

Result rolled(const rr::Assets& assets, const std::vector<uint8_t>& exe, int humans, const Timeline& tl,
              int depth, uint32_t seed) {
    std::unique_ptr<rr::RaceSetup> keep;
    rr::RaceInstance in = fresh(assets, exe, humans, keep);
    rr::net::Rollback r(in, humans, 0, 16, countdown);
    const int n = int(tl.words.size());
    // Arrival tick of each sealed frame: t + random 0..depth (so frames also
    // arrive out of order); the client runs depth ticks ahead.
    std::vector<int> arrival(static_cast<size_t>(n), 0);
    uint32_t x = seed;
    for (int t = 0; t < n; ++t) {
        x = x * 1664525u + 1013904223u;
        arrival[size_t(t)] = t + int((x >> 8) % uint32_t(depth + 1));
    }
    Result out;
    for (int now = 0; now < n + depth + 2; ++now) {
        for (int t = 0; t < n; ++t)
            if (arrival[size_t(t)] == now)
                r.sealed(t, tl.words[size_t(t)], tl.late[size_t(t)], tl.present[size_t(t)]);
        if (now < n) r.local_input(now, intent(0, now)); // the local seat's own (pre-lateness) input
        r.advance_to(std::min(now + 1, n));
        out.max_depth = std::max(out.max_depth, r.last_rollback_depth());
        out.stalls += r.stalled();
    }
    r.advance_to(n);
    for (const auto& h : r.take_hashes()) out.hashes[h.tick] = h.hash;
    out.final_hash = rr::state_hash(in);
    return out;
}

} // namespace

int main() {
    rr::test::Checks check;
    const std::string root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(root + "/ROADRASH.EXE")) {
        std::cout << "SKIP no disc\n";
        return rr::golden::skip;
    }
    rr::Assets assets(root);
    const std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");

    for (int humans : {2, 8}) {
        const int ticks = humans == 2 ? 600 : 480;
        const Timeline tl = seal(humans, ticks, 99u + uint32_t(humans));
        const Result base = straight(assets, exe, humans, tl);
        check(base.hashes.size() == size_t(ticks / 60), "straight run hashes every 60 ticks");
        const std::vector<int> depths =
            humans == 2 ? std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}
                        : std::vector<int>{0, 1, 4, 8, 16};
        for (int depth : depths) {
            const Result r = rolled(assets, exe, humans, tl, depth, 7u * uint32_t(depth) + 1u);
            const std::string what = std::to_string(humans) + " humans, depth " + std::to_string(depth);
            check(r.final_hash == base.final_hash, what + ": final hash equals the straight run");
            check(r.hashes == base.hashes, what + ": every confirmed 60-tick hash equals the straight run");
            check(r.max_depth <= 16, what + ": rollback stays inside the window");
            std::cout << what << ": deepest correction " << r.max_depth << " ticks, stalled ticks "
                      << r.stalls << "\n";
        }
    }
    return check.finish("rollback");
}
