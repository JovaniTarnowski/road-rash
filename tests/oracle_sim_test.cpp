#include "assets.hpp"
#include "bike.hpp"
#include "career.hpp"
#include "course.hpp"
#include "exe_tables.hpp"
#include "golden.hpp"
#include "frozen_state.hpp"
#include "race.hpp"
#include "race_setup.hpp"
#include <cstdio>
#include <filesystem>
#include <functional>
#include <optional>
#include <sstream>
#include <iostream>

// Plan 10 task 5 step 2a (+ replay-harness corrections, Oct 12): DEFAULT is a
// single RNG anchor at the green light (set_rng_state once from the golden
// header, free-run after) so missing or extra rand() calls surface as rng
// divergence. --reanchor keeps the older per-tick re-anchoring as a
// diagnostic mode for localising field divergences; it MUST NOT be used for
// acceptance. --capabilities reports the replay contract the binary
// implements (see tools/verify_sim.py acceptance gate).
// Exit 0 on full match, 1 on divergence (first tick/field + last-8 RNG both
// sides), 77 when the golden is missing. Car/node comparison is task 7 work.
namespace {
// The replay builds its race through the shared rr::make_race_setup factory
// (plan 26 Task 2), the same mechanism the game uses: header-driven
// bike/rider selection (no SPEC-1 default), the course's own vehicle library
// WITH its EXE descriptors (traffic.cpp requires the EXE for views/playback),
// traffic + crash config attached to the Race, and grid-lateral placement
// from the game's own route data (no absolute course-specific anchor: Race
// lateralises from route offset + grid word, exactly as 0x00405990 reads
// sub-slice +0xcc plus the grid offset).
int32_t exe_i32(const std::vector<uint8_t>& exe, uint32_t va) {
    // .data section mapping of the reference executable (see read_physics_tables).
    constexpr uint32_t data_va = 0x00463000, data_raw = 0x61600, data_size = 0x16400;
    if (va < data_va || va + 4 > data_va + data_size || data_raw + (va - data_va) + 4 > exe.size())
        throw std::runtime_error("EXE word outside mapped sections");
    const size_t p = data_raw + (va - data_va);
    return int32_t(uint32_t(exe[p]) | uint32_t(exe[p + 1]) << 8 |
                   uint32_t(exe[p + 2]) << 16 | uint32_t(exe[p + 3]) << 24);
}
// Rider-table row (0x00463c78, 0x40 bytes): weight +0xc, weapon base +0x18,
// fight stats +0x30..+0x3c. Returns {value, weapon0, stats}.
struct RiderRow { int32_t value; int32_t weapon0; rr::RiderStats stats; };
RiderRow read_rider_row(const std::vector<uint8_t>& exe, int rider) {
    if (rider < 0 || rider > 9) throw std::runtime_error("Rider index out of range");
    const uint32_t row = 0x00463c78 + 0x40u * uint32_t(rider);
    return {exe_i32(exe, row + 0xc) / 2, exe_i32(exe, row + 0x18),
            {exe_i32(exe, row + 0x30), exe_i32(exe, row + 0x34),
             exe_i32(exe, row + 0x38), exe_i32(exe, row + 0x3c)}};
}
// The intended per-tick comparison set (plan 10 acceptance): every native
// snapshot field for every live rider plus every traffic pool slot, plus
// rng. This is a REQUIRED set: a missing golden field is a FAIL (exit 1),
// never a skip-green. Golden keys are resolved through the era map below:
// the proxy schema grew over time (10-field era -> think/raw era -> full
// brain/bike era), so a native field maps to its golden key by absolute
// original offset. Player-think evidence (rider.0.think == 0x00434660 =
// 4408928, measured) is checked once at green: the branch is read from the
// original DAT_004753c8 probe (player.demo_hold) or the explicit
// --driver=human|demo flag — never from solo, the filename, or nonzero
// recorded input. Animation fields use absolute original ticks (deadlines
// accumulate from absolute ticks; no local-zero rebase). Golden-only
// extras (raw_/bike_ windows, node.*, probes) are diagnostic context,
// counted, never skipped.
const char* golden_key_for(const std::string& slot_prefix,
                           const std::string& suffix,
                           const std::map<std::string, int64_t>& golden,
                           std::string& out) {
    // Direct bike-half fields share names across eras. REQUIRED whenever
    // the proxy dumps that era's schema: the golden MUST carry the key or
    // the run FAILS (missing field = FAIL, never skip-green). The solo
    // golden's think/raw era predates lat_vel (bike +0x10c): the proxy
    // must add it (with car/police/animation fields) before acceptance.
    // Until then the era-gap rule in compare_fields counts it uncovered.
    static const char* direct[] = {"position", "lateral", "height", "velocity",
        "speed", "gear", "condition", "crash", "flags", "lat_vel", nullptr};
    if (suffix == "slice") {
        // Era rename: the oracle dumps bike +0x140 as slice_ptr (a host
        // pointer in current captures, a debug-only value). The sample
        // index is derived from the golden's position word (grid selector
        // = sample, 0x00405990) instead of comparing against the pointer.
        out = slot_prefix + "slice_ptr";
        return out.c_str();
    }
    if (suffix == "steer" || suffix == "steer_rate" || suffix == "ground" ||
        suffix == "stamina_current" || suffix == "stamina_max" || suffix == "weapon") {
        // Offsets the proxy never dumps (no golden key in any era).
        return nullptr;
    }
    // Animation/combat/nitro fields (plan 16): direct golden keys when the
    // proxy dumps them, else uncovered (reported, never failing).
    static const char* passthrough[] = {"anim", "anim_frame", "anim_queued", "anim_loops",
        "anim_deadline", "attack_weapon", "steal_until", "hit_until",
        "nitro_charge", "nitro_throttle", "nitro_charges", nullptr};
    for (const char** d = passthrough; *d; ++d)
        if (suffix == *d) {
            out = slot_prefix + suffix;
            if (golden.find(out) == golden.end()) return nullptr;
            return out.c_str();
        }
    // AI offsets live in the brain window (rider.N.brain_<hex>, brain
    // +0x138..+0x1c4) in the full era; earlier eras lack them.
    static const struct { const char* suffix; const char* hex; } ai_map[] = {
        {"ai_target_x", "194"}, {"ai_target_position", "198"},
        {"ai_target_speed", "19c"}, {"ai_current_speed", "1a4"},
        {"ai_base_speed", "1bc"}, {"ai_replan", "188"},
        {"ai_lane", "18c"}, {"ai_bias", "190"},
    };
    for (const auto& m : ai_map)
        if (suffix == m.suffix) {
            out = slot_prefix + "brain_" + m.hex;
            if (golden.find(out) == golden.end()) return nullptr;
            return out.c_str();
        }
    return nullptr;
}
int compare_fields(const std::map<std::string, int64_t>& golden,
                   const std::vector<rr::FieldValue>& native, size_t frame, int64_t tick,
                   const std::vector<std::pair<int64_t, uint64_t>>& rng_history,
                   size_t& uncovered, std::string& missing) {
    for (const auto& f : native) {
        const auto dot = f.name.rfind('.');
        const std::string prefix = f.name.substr(0, dot + 1);
        const std::string suffix = f.name.substr(dot + 1);
        auto it = golden.find(f.name);
        if (it != golden.end()) {
            if (it->second != f.value) {
                std::cout << "first divergence at frame " << frame << " (tick " << tick
                          << "): " << f.name << " original " << it->second << " native " << f.value << '\n';
                std::cout << "rng history (last " << rng_history.size() << " ticks, golden vs native):";
                for (const auto& e : rng_history) std::cout << " (" << e.first << "," << e.second << ")";
                std::cout << '\n';
                return 1;
            }
            continue;
        }
        // Car pool slots have no stable index across implementations: match
        // a native car row to the golden ring node with equal
        // kind+position+lateral, then compare the remaining field there.
        // Unmatched cars (or ambiguous matches) stay uncovered, reported
        // per frame, never skipped-green.
        if (prefix.rfind("car.", 0) == 0 && (suffix == "vehicle" || suffix == "lane" ||
            suffix == "speed" || suffix == "phase")) {
            auto car_field = [&](const std::string& addr, const char* field) -> const int64_t* {
                auto jt = golden.find("ring." + addr + "." + field);
                return jt == golden.end() ? nullptr : &jt->second;
            };
            int64_t kind = -1, position = 0, lateral = 0;
            {
                auto jk = golden.find(prefix + "kind");
                auto jp = golden.find(prefix + "position");
                auto jl = golden.find(prefix + "lateral");
                if (jk == golden.end() || jp == golden.end() || jl == golden.end()) {
                    ++uncovered;
                    continue;
                }
                kind = jk->second; position = jp->second; lateral = jl->second;
            }
            std::string match_addr;
            for (const auto& kv : golden) {
                if (kv.first.rfind("ring.", 0) != 0) continue;
                const size_t dot = kv.first.find('.', 5);
                if (dot == std::string::npos) continue;
                const std::string addr = kv.first.substr(5, dot - 5);
                auto k = car_field(addr, "kind"), p = car_field(addr, "position"), l = car_field(addr, "lateral");
                if (k && p && l && *k == kind && *p == position && *l == lateral) {
                    if (!match_addr.empty()) { match_addr.clear(); break; }   // ambiguous
                    match_addr = addr;
                }
            }
            auto want = match_addr.empty() ? nullptr : car_field(match_addr, suffix.c_str());
            if (!want) { ++uncovered; continue; }
            if (*want != f.value) {
                std::cout << "first divergence at frame " << frame << " (tick " << tick
                          << "): " << f.name << " original " << *want << " native " << f.value
                          << " (via ring." << match_addr << "." << suffix << ")\n";
                std::cout << "rng history (last " << rng_history.size() << " ticks, golden vs native):";
                for (const auto& e : rng_history) std::cout << " (" << e.first << "," << e.second << ")";
                std::cout << '\n';
                return 1;
            }
            continue;
        }
        std::string key;
        const char* mapped = golden_key_for(prefix, suffix, golden, key);
        // Era-coverage carve-out: car.* rows only exist when the proxy
        // dumps them (traffic pool absent in solo captures), and several
        // offsets predate the dumped eras (lat_vel/steer/ground/stamina/
        // weapon via no key in the think/raw era, slice via pointer,
        // ai/anim/nitro windows undumped). Those stay counted-as-uncovered
        // (reported, never skipped-green) so old captures still parse;
        // every other intended rider field is REQUIRED (missing = FAIL).
        // The proxy MUST add lat_vel + car/police/animation fields before
        // acceptance; then this carve-out shrinks to car-rows-absent only.
        const bool car_row = prefix.rfind("car.", 0) == 0;
        const bool era_gap = mapped == nullptr || golden.find(key) == golden.end();
        if (era_gap && (car_row || suffix == "lat_vel" || suffix == "steer" || suffix == "steer_rate" || suffix == "ground" ||
                        suffix == "stamina_current" || suffix == "stamina_max" || suffix == "weapon" ||
                        suffix == "slice" || suffix.rfind("ai_", 0) == 0 ||
                        suffix.rfind("anim", 0) == 0 || suffix == "attack_weapon" ||
                        suffix == "steal_until" || suffix == "hit_until" ||
                        suffix.rfind("nitro_", 0) == 0)) {
            ++uncovered;
            continue;
        }
        if (!mapped) { missing = f.name + " (no golden key in any era)"; return 1; }
        auto jt = golden.find(key);
        if (jt == golden.end()) { missing = f.name + " (via " + key + ": absent)"; return 1; }
        int64_t want = jt->second;
        if (suffix == "slice") {
            // Pointer-valued slice_ptr: derive the sample from position.
            auto pos = golden.find(prefix + "position");
            if (pos == golden.end()) { missing = f.name + " (no position for slice derive)"; return 1; }
            want = pos->second >> 8;
        }
        if (want != f.value) {
            std::cout << "first divergence at frame " << frame << " (tick " << tick
                      << "): " << f.name << " original " << want << " native " << f.value
                      << " (via " << key << ")\n";
            std::cout << "rng history (last " << rng_history.size() << " ticks, golden vs native):";
            for (const auto& e : rng_history) std::cout << " (" << e.first << "," << e.second << ")";
            std::cout << '\n';
            return 1;
        }
    }
    return 0;
}
}  // namespace
int main(int argc, char** argv) {
    bool reanchor = false;
    std::string driver_cli;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--reanchor") reanchor = true;
        else if (a.rfind("--driver=", 0) == 0) {
            driver_cli = a.substr(9);
            if (driver_cli != "human" && driver_cli != "demo")
                throw std::runtime_error("--driver must be human|demo");
        } else if (a == "--capabilities") {
            // Replay contract (checked by tools/verify_sim.py acceptance):
            // human control words consumed every tick with a single RNG
            // anchor at green; the demo branch is an explicit --driver
            // choice, never inferred.
            std::cout << "human-controls-single-anchor\n";
            std::cout << "driver-axis\n";
            std::cout << "green-pre-step-anchor\n";
            std::cout << "strict-field-set\n";
            return 0;
        } else args.push_back(a);
    }
    const std::string file = args.size() > 0 ? args[0] : rr::golden::oracle_dir() + "/sim-napa-solo.tsv";
    if (!std::filesystem::exists(file)) { std::cout << "SKIP missing " << file << '\n'; return rr::golden::skip; }
    const auto g = rr::golden::load(file);
    if (g.exe != "73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2") throw std::runtime_error("different executable");
    if (g.frames.empty()) throw std::runtime_error("no frames");
    const auto& h = g.frames.at(0);
    auto get = [&](const char* k) { auto it = h.find(k); if (it == h.end()) throw std::runtime_error(std::string("header lacks ") + k); return it->second; };
    static const char* original_courses[] = {"CITY", "HIWAY", "MEDLY", "NAPA", "CANYN"};  // 0x0046df58
    const int course_entry = int(get("course"));
    if (course_entry < 1 || course_entry > 5) throw std::runtime_error("course out of range");
    const int level = int(get("level"));
    const std::string assets_root = args.size() > 1 ? args[1] : std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";
    if (!std::filesystem::exists(assets_root + "/ROADRASH.EXE")) { std::cout << "SKIP no disc\n"; return rr::golden::skip; }
    // driver=human|demo is an axis independent of solo: solo isolates the
    // field (scheduler freeze), driver selects the 0x00434660 branch.
    // --driver defaults to the golden's measured player.demo_hold header
    // (the oracle's DAT_004753c8 sample); a CLI value overrides the header.
    // STOP: neither the solo flag, the filename, nor nonzero recorded input
    // proves the branch. All current captures predate the demo_hold probe
    // and carry no measurement: without --driver they are unverified.
    int measured_hold = -1;  // player.demo_hold sampled across frames; -1 = unmeasured
    {
        bool any = false, all0 = true, all1 = true;
        for (const auto& f : g.frames) {
            auto it = f.find("player.demo_hold");
            if (it == f.end()) continue;
            any = true;
            if (it->second != 0) all0 = false;
            if (it->second != 1) all1 = false;
        }
        if (any) measured_hold = all0 ? 0 : all1 ? 1 : -2;  // -2 = mixed (mid-capture transition)
    }
    // 0x00434660 player-think evidence: entry address + invocation counter
    // when the probe recorded them (player.think / player.think_count).
    // Measured in current captures: rider.0.think == 4225680 (0x00407a90,
    // the AI-think callee) because the solo golden runs the demo/AI side;
    // the player-think entry 0x00434660 (= 4408928) shows as rider.1.think
    // (the frozen opponent keeps the player-think dispatch word). New
    // captures MUST record player.think/player.think_count directly.
    auto hfind = [&](const char* k) { auto it = h.find(k); return it == h.end() ? -1 : it->second; };
    const int64_t think_entry = hfind("player.think");
    const int64_t think_count = hfind("player.think_count");
    const int64_t r0_think = hfind("rider.0.think"), r1_think = hfind("rider.1.think");
    const bool demo = !driver_cli.empty() ? driver_cli == "demo"
                                          : measured_hold == 1 ? true
                                          : measured_hold == 0 ? false
                                          : throw std::runtime_error("golden has no player.demo_hold measurement: pass --driver=human|demo");
    if (measured_hold == -2 && driver_cli.empty())
        throw std::runtime_error("golden player.demo_hold changes mid-capture: pass --driver=human|demo");
    const bool solo = h.find("solo") != h.end() && h.at("solo") != 0;
    rr::Assets assets(assets_root);
    std::vector<uint8_t> exe = assets.read("ROADRASH.EXE");
    const std::string course_name = original_courses[course_entry - 1];
    // Grid lateral: the original places the player at sub-slice +0xcc (the
    // absolute road centre at the grid sample) plus the 0x00464218 grid
    // word. The native route stores centres relative to the course root,
    // so the harness passes the difference as the Race road_base_q8 anchor
    // via the shared rr::race_road_base_q8 helper (same ctor the game
    // uses; the game passes 0 = relative frame). Measured per golden (not
    // a course constant), reported every run.
    int32_t road_base = 0;
    {
        // Local course read for the sample offset (the factory reads the
        // course again for the race itself).
        const rr::Course course = rr::read_course(assets.read("DATA/COURSES/" + course_name + ".CRS"));
        const std::vector<rr::RouteSample>& ref = rr::make_route(course, false);
        auto hpos = h.find("rider.0.position"), hlad = h.find("rider.0.lateral");
        if (hpos != h.end() && hlad != h.end()) {
            const size_t sample = size_t(uint32_t(hpos->second) >> 8);
            const int32_t grid_word = exe_i32(exe, 0x00464218 + 8u * uint32_t(level));
            if (sample < ref.size())
                road_base = rr::race_road_base_q8(int32_t(hlad->second), ref[sample].offset, grid_word);
            std::cout << "road_base: " << road_base << " (golden lat " << hlad->second
                      << " - route[" << sample << "].offset*256 - grid " << grid_word << ")\n";
        }
    }
    rr::RaceConfig config;
    config.course = course_name;
    config.level = level;
    config.character = 1;
    config.career = true;
    config.bike_from_table = true;   // default-bike setup stream (finding 1, 2026-10-13)
    config.solo = solo;
    config.road_base_q8 = road_base;
    // Track ring (plan 10 Task 5): the original builds it inside the setup
    // stream, srand(seed) + ring.pre_draws (9,098 on the Napa solo
    // captures, measured by ring-probe), then 0x00405600 and the countdown
    // consume ring.post_draws (11) before green. The factory loads this
    // state just before ring setup (0x004552e0).
    {
        const int64_t pre = h.count("ring.pre_draws") ? h.at("ring.pre_draws") : 9098;
        uint32_t state = uint32_t(h.count("seed") ? h.at("seed") : 1);
        for (int64_t k = 0; k < pre; ++k) state = state * 0x343fdu + 0x269ec3u;
        config.ring_rng_state = state;
    }
    // Demo goldens (DAT_004753c8=1): the "player" is AI-driven and ignores
    // the recorded control words. Human goldens (DAT_004753c8=0) leave demo
    // hold clear so race.step consumes the control words via apply_controls.
    // The oracle demo player uses character 1's rider 8 for bike/weight/
    // weapon/stats (its 0x00463c78 row), but its AI brain is the 0x004c6b40
    // copy of personality row DAT_00467a6c — which the RR_ORACLE_COURSE
    // bootstrap leaves at its startup value 0 (menus bypassed), NOT the
    // career rider 8. Measured: golden base/current2 = pers0 L0 (87/70).
    // The harness therefore seeds the player brain from the golden's
    // measured base (demo.base_speed) or personality row 0, while
    // bike/value/weapon/stats stay rider-8 (character 1). A future oracle
    // probe should record DAT_00467a6c directly (open: analysis/replay-notes.md).
    // Runs after the ctor, before the ring (the factory's prepare hook).
    if (demo) {
        config.prepare = [&](rr::Race& rr) {
            const int rider = rr::character_rider(1);
            const RiderRow row = read_rider_row(exe, rider);
            const auto personalities = rr::read_personalities(exe);
            const int64_t dbase = hfind("demo.base_speed");
            const int64_t dlane = hfind("demo.lane"), dbias = hfind("grid.bias");
            const int64_t dweapon = hfind("demo.weapon");
            const int64_t dstr = hfind("demo.stats_strength"), dbas = hfind("demo.stats_base");
            const int64_t dmax = hfind("demo.stats_max"), dcur_st = hfind("demo.stats_current");
            // Brain row: measured base matched against the personality table
            // (proves which row the original used); default row 0 (bootstrap).
            int brain_row = 0;
            if (dbase >= 0 && !personalities.empty()) {
                for (size_t k = 0; k < personalities.size(); ++k)
                    if (personalities[k].cur_levels[0] == int(dbase)) { brain_row = int(k); break; }
            }
            const int base = dbase >= 0 ? int(dbase)
                : !personalities.empty() ? personalities[size_t(brain_row)].cur_levels[0] : 95;
            const int lane = dlane >= 0 ? int(dlane) : 1;
            // Unmeasured: the grid offset the ctor stored (0x00405990 -> +0x190;
            // the human-era golden reads brain_190 = 16384 = the level-0 word).
            const int32_t bias = dbias >= 0 ? int32_t(dbias) : rr.riders().at(0).brain.bias;
            const int weapon = dweapon >= 0 ? int(dweapon) : row.weapon0;
            const rr::RiderStats stats{dstr >= 0 ? int32_t(dstr) : row.stats.strength,
                                      dbas >= 0 ? int32_t(dbas) : row.stats.base,
                                      dmax >= 0 ? int32_t(dmax) : row.stats.max,
                                      dcur_st >= 0 ? int32_t(dcur_st) : row.stats.current};
            // Seed the player brain tables from the measured row BEFORE
            // set_demo_hold (the ctor loaded personalities[0]; correct it when
            // the measurement points elsewhere).
            if (!personalities.empty() && brain_row != 0 && !rr.riders().empty()) {
                auto& brain = rr.riders()[0].brain;
                const auto& p = personalities[size_t(brain_row)];
                for (int i = 0; i < 5; ++i) { brain.spd_a[i] = p.cur2_levels[i]; brain.spd_b[i] = p.cur_levels[i]; }
                for (int i = 0; i < 30; ++i) { brain.rating[i] = p.rating[i]; brain.rating_tgt[i] = p.rating_tgt[i]; }
                for (int i = 0; i < 3; ++i) brain.ease[i] = p.ease[i];
                brain.overtake_index = p.overtake_index;
                brain.reach = p.reach;
            }
            rr.set_demo_hold(true, base, lane, bias, weapon, stats);
        };
    }
    rr::RaceSetup setup = rr::make_race_setup(assets, exe, config);
    rr::Race& race = *setup.instance.race;
    // Solo isolation (plan 10 Task 5): the oracle nulls the traffic pool, so
    // 0x0044df60 never places cars. Detaching keeps the replay exact while
    // the spawn-timer schedule is unresolved (Task 7): attached timers fire
    // natively at tick 319 (behind, density-40 schedule, tap count 1) where
    // the original stays silent, and the extra lane draw regresses 376 to
    // 319. The spawn_enabled gate below stays for the Task-7 port (draw
    // before place, verified against 0x0044dd90/0x0044df60), when timer
    // inits/taps/density match and the tick-376 lane pick replays.
    // Field scenarios keep traffic attached.
    if (solo) race.set_traffic(nullptr);
    // The factory's last RNG draw is the ring setup's end (0x004552e0);
    // the countdown then consumes ring.post_draws (11) before green.
    const std::optional<uint32_t> ring_end_state = race.rng_state();
    const int64_t ring_post = h.count("ring.post_draws") ? h.at("ring.post_draws") : 11;
    // The golden rng is the live CRT state AFTER each tick's step (the hook
    // dumps after 0x0044b3b0). The race-setup stream (menu entry through grid
    // build, 11,461 calls) is not reproduced: a single anchor at green covers
    // it (known limitation until the setup consumers are ported).
    const int64_t tick0 = get("tick");
    std::function<std::string(const std::map<std::string, int64_t>&)> brain_line;
    // Start-hold: the golden's leading frames run during the original's
    // pre-green countdown (menu-layer, wall-clock); rider state is frozen and
    // rng unconsumed. The port holds its countdown outside Race (game_main),
    // so replay begins at the first frame where player state moves (green),
    // with RNG anchored once there. Hold frames are asserted frozen below.
    // Green-boundary rule (off-by-one fix): the dump is POST-step, so the
    // green frame's rng field is the state AFTER the green tick already
    // consumed its draws. Anchoring from it then re-executing the green
    // control word would shift the stream by one tick. The anchor is
    // therefore the PRE-step state of the green tick, i.e. the preceding
    // frame's post-step rng (proven equivalent: hold frames consume zero
    // draws — rng_calls==0 and rng constant across every hold frame, and
    // the branch proof above shows the player think took the same path).
    // Replay then executes the green frame's own control word first.
    size_t green = 1;
    {
        const auto& f0 = g.frames.at(0);
        auto pos0 = f0.find("rider.0.position"), spd0 = f0.find("rider.0.speed");
        for (size_t i = 1; i < g.frames.size(); ++i) {
            const auto& f = g.frames[i];
            auto p = f.find("rider.0.position"), s = f.find("rider.0.speed");
            if ((p != f.end() && pos0 != f0.end() && p->second != pos0->second) ||
                (s != f.end() && spd0 != f0.end() && s->second != spd0->second)) { green = i; break; }
        }
        if (green == 0 || green >= g.frames.size()) throw std::runtime_error("no green frame: player state never moves");
        const auto& fg = g.frames.at(green);
        const auto& fp = g.frames.at(green - 1);
        auto frg = fg.find("rng");
        auto frp = fp.find("rng");
        auto calls = fg.find("rng_calls");
        if (frg == fg.end() || frp == fp.end()) throw std::runtime_error("green frames lack rng");
        // Pre-step equivalence proof: every hold frame (0..green-1) must
        // show zero consumed draws and a constant rng; otherwise the
        // preceding post-step state is NOT the green pre-step state.
        int64_t anchor = frp->second;
        bool proven = true;
        for (size_t i = 0; i < green; ++i) {
            const auto& f = g.frames[i];
            auto fr = f.find("rng"), fc = f.find("rng_calls");
            if (fr == f.end() || fr->second != anchor) { proven = false; break; }
            if (fc != f.end() && fc->second != 0) { proven = false; break; }
        }
        if (!proven) {
            std::cout << "green pre-step anchor unproven at frame " << green << " (tick " << tick0 + int64_t(green)
                      << "): hold frames consume rng or vary; capture pre-step RNG to proceed\n";
            return 1;
        }
        (void)calls;
        if (ring_end_state) {
            // Closure: the ring setup must end exactly ring.post_draws draws
            // before the green pre-step state.
            uint32_t x = *ring_end_state;
            for (int64_t k = 0; k < ring_post; ++k) x = x * 0x343fdu + 0x269ec3u;
            if (x != uint32_t(anchor)) {
                std::cout << "ring setup does not close on the green anchor (" << ring_post << " post draws)\n";
                return 1;
            }
            std::cout << "ring setup closes on the green anchor (+" << ring_post << " draws)\n";
        }
        race.set_rng_state(uint32_t(anchor));
        std::cout << (reanchor ? "[reanchor] " : "[single-anchor] ") << "start-hold: " << green << " frozen frames, green at tick " << (tick0 + int64_t(green))
                  << " driver=" << (demo ? "demo" : "human") << (driver_cli.empty() ? " (measured player.demo_hold=" + std::to_string(measured_hold) + ")" : " (cli override)")
                  << " think=0x" << std::hex << think_entry << std::dec << " count=" << think_count
                  << " rider.0.think=" << r0_think << " rider.1.think=" << r1_think << " (0x00434660=4408928)\n";
    }
    // Default: free-run from the single green anchor, so any missing or
    // extra Race::random() call shows up as rng divergence. --reanchor
    // restores per-tick re-anchoring (diagnostic only): scenery (0x451xxx),
    // combat (0x404bxx) and horn/traffic (0x44dexx) consumers run in the
    // original tick but are not ported yet, so stream-position equality is
    // not expected there; field comparison is the signal. The last-8 rng
    // history is kept for diagnosis in both modes.
    // Tick values stay absolute (original ticks): animation deadlines
    // accumulate from absolute tick counts, so a rebased local zero would
    // silently shift every deadline comparison.
    // Golden object pointers (brain +0x168/+0x16c/+0x170/+0x180) resolve
    // through the frame-0 node dump to a (position, type) pair; native riders
    // are matched by bike position.
    std::map<int64_t, std::pair<int64_t, int64_t>> node_at;   // address -> (position, type)
    for (const auto& kv : h) {
        if (kv.first.rfind("nodeaddr.", 0) != 0) continue;
        const std::string n = kv.first.substr(9);
        if (n.find('.') != std::string::npos) continue;
        auto p = h.find("nodeaddr." + n + ".position"), t = h.find("nodeaddr." + n + ".type");
        if (p == h.end() || t == h.end()) continue;
        node_at[kv.second] = {p->second, t->second};
    }
    auto rider_at = [&](int64_t position) -> int {
        for (size_t k = 0; k < race.riders().size(); ++k)
            if (race.riders()[k].bike.i32(0x1c) == position) return int(k);
        return -1;
    };
    auto golden_rider = [&](const std::map<std::string, int64_t>& f, const char* key) -> int {
        auto it = f.find(key);
        if (it == f.end() || it->second == 0) return -1;
        auto n = node_at.find(it->second);
        return n == node_at.end() ? -2 : rider_at(n->second.first);
    };
    auto gpos = [&](const std::map<std::string, int64_t>& f, const char* key) -> int64_t {
        auto it = f.find(key);
        if (it == f.end()) return -2;
        if (it->second == 0) return -1;
        auto n = node_at.find(it->second);
        return n == node_at.end() ? it->second : n->second.first;
    };
    auto gval = [](const std::map<std::string, int64_t>& f, const std::string& key) -> int64_t {
        auto it = f.find(key); return it == f.end() ? -2 : it->second;
    };
    auto pos_of = [&](int idx) -> int64_t { return idx < 0 ? -1 : race.riders()[size_t(idx)].bike.i32(0x1c); };
    brain_line = [&](const std::map<std::string, int64_t>& frame) {
        const auto& br = race.riders()[0].brain;
        std::ostringstream o;
        o << "brain state=" << br.state << "/" << gval(frame, "rider.0.brain_160")
          << " target=" << pos_of(br.target_rider) << "/" << gpos(frame, "rider.0.brain_168")
          << " ahead=" << pos_of(br.nbr_ahead) << "/" << gpos(frame, "rider.0.brain_16c")
          << " behind=" << pos_of(br.nbr_behind) << "/" << gpos(frame, "rider.0.brain_170")
          << " latched=" << (br.overtake_type ? br.overtake_position : -1) << "/" << gpos(frame, "rider.0.brain_180")
          << " step=" << br.target_step << "/" << gval(frame, "rider.0.brain_174")
          << " ov=" << br.ov_ahead << "," << br.ov_behind << "/" << gval(frame, "rider.0.brain_178") << "," << gval(frame, "rider.0.brain_17c")
          << " cur2=" << br.current2 << "/" << gval(frame, "rider.0.brain_1a0")
          << " cur=" << br.current_speed << "/" << gval(frame, "rider.0.brain_1a4")
          << " tx=" << br.target_x << "/" << gval(frame, "rider.0.brain_194")
          << " tpos=" << br.target_position << "/" << gval(frame, "rider.0.brain_198")
          << " tspd=" << br.target_speed << "/" << gval(frame, "rider.0.brain_19c")
          << " replan=" << br.replan_ticks << "/" << gval(frame, "rider.0.brain_188");
        return o.str();
    };
    // First brain word that differs from the golden window (empty if the
    // golden carries no brain window or everything matches).
    auto brain_diff = [&](const std::map<std::string, int64_t>& f) -> std::string {
        if (f.find("rider.0.brain_160") == f.end()) return {};
        const auto& br = race.riders()[0].brain;
        const std::pair<const char*, int64_t> words[] = {
            {"160", br.state}, {"174", br.target_step}, {"178", br.ov_ahead}, {"17c", br.ov_behind},
            {"188", br.replan_ticks}, {"18c", br.lane}, {"190", br.bias}, {"194", br.target_x},
            {"198", br.target_position}, {"19c", br.target_speed}, {"1a0", br.current2}, {"1a4", br.current_speed}};
        for (const auto& w : words) {
            const int64_t want = gval(f, std::string("rider.0.brain_") + w.first);
            if (want != w.second) return std::string("brain +0x") + w.first + " original " + std::to_string(want) + " native " + std::to_string(w.second);
        }
        for (int st = 0; st < 10; ++st) {
            char key[32]; std::snprintf(key, sizeof key, "rider.0.brain_%x", 0x138 + 4 * st);
            const int64_t want = gval(f, key);
            if (want != -2 && want != br.state_ticks[size_t(st)])
                return std::string("brain +0x") + (key + 14) + " original " + std::to_string(want) + " native " + std::to_string(br.state_ticks[size_t(st)]);
        }
        const std::pair<const char*, int> ptrs[] = {{"rider.0.brain_168", br.target_rider}, {"rider.0.brain_16c", br.nbr_ahead}, {"rider.0.brain_170", br.nbr_behind}};
        for (const auto& p : ptrs) {
            const int want = golden_rider(f, p.first);
            if (want != p.second) return std::string(p.first + 8) + " original rider " + std::to_string(want) + " native " + std::to_string(p.second);
        }
        const int64_t latched = gpos(f, "rider.0.brain_180");
        const int64_t mine = race.riders()[0].brain.overtake_type ? race.riders()[0].brain.overtake_position : -1;
        if (latched != -2 && latched != mine) return "brain_180 original position " + std::to_string(latched) + " native " + std::to_string(mine);
        return {};
    };
    // Countdown (plan 10 Task 5): the original thinks through the hold frames.
    // Captures with a brain window seed the frozen field and the demo brain
    // from frame 0 (post tick 1: the global brain copy carries words from its
    // previous use, and frozen bikes sit off their grid words), then run the
    // native countdown ticks, comparing the brain every frame.
    // Older captures without node laterals or a brain window keep the native
    // grid placement and the ctor brain, and run the countdown from tick 1.
    const bool brain_window = h.find("rider.0.brain_160") != h.end();
    // Declared frozen words hydrate human and demo solo captures once.
    // Older captures retain their existing demo-only lateral fallback.
    if (solo) {
        try { rr::golden::apply_frozen_state(h, race.riders(), demo); }
        catch (const std::runtime_error& error) {
            std::cout << error.what() << '\n';
            return 1;
        }
    }
    // Spawn-frozen player scheduler (frame 0 is post tick 1): hydrate the
    // slotted rider.0.bike_c/10/14/18 keys when the capture dumps them, so
    // the launch ramp consumes the previous run's dt (tick-2 dt 2) while
    // contact reads the fresh one. Legacy captures without the keys keep
    // the build/countdown dt management.
    if (solo) {
        auto& p = race.riders()[0];
        auto skey = [&](const char* k) {
            auto it = h.find(std::string("rider.0.bike_") + k);
            return it == h.end() ? nullptr : &it->second;
        };
        const int64_t* s_last = skey("10");
        const int64_t* s_next = skey("c");
        const int64_t* s_interval = skey("14");
        const int64_t* s_dt = skey("18");
        if (s_last && s_next && s_interval && s_dt &&
            *s_last >= std::numeric_limits<int32_t>::min() && *s_last <= std::numeric_limits<int32_t>::max() &&
            *s_next >= std::numeric_limits<int32_t>::min() && *s_next <= std::numeric_limits<int32_t>::max() &&
            *s_interval >= 1 && *s_interval <= std::numeric_limits<int32_t>::max() &&
            *s_dt >= std::numeric_limits<int32_t>::min() && *s_dt <= std::numeric_limits<int32_t>::max()) {
            p.sched_last = int32_t(*s_last);
            p.sched_next = int32_t(*s_next);
            p.sched_interval = int32_t(*s_interval);
            p.bike.i32(0x18) = int32_t(*s_dt);
            p.sched_live = true;
        }
    }
    // Measured spawn-frozen brain snapshot (frame 0 is post tick 1): hydrate
    // once for any solo capture with a brain window. The human path never
    // touches these words (0x00405c80 spawn init stays frozen), so the same
    // snapshot covers demo and human; demo keeps its countdown below.
    if (solo && brain_window) {
        auto& br = race.riders()[0].brain;
        auto w = [&](const char* k) { return int32_t(gval(h, std::string("rider.0.brain_") + k)); };
        br.state = w("160"); br.target_step = w("174"); br.ov_ahead = w("178"); br.ov_behind = w("17c");
        br.replan_ticks = w("188"); br.lane = w("18c"); br.bias = w("190"); br.target_x = w("194");
        br.target_position = w("198"); br.target_speed = w("19c"); br.current2 = w("1a0"); br.current_speed = w("1a4");
        br.base_speed = w("1bc");
        br.state_flag = uint8_t(w("164"));
        for (int st = 0; st < 10; ++st) { char key[16]; std::snprintf(key, sizeof key, "%x", 0x138 + 4 * st); br.state_ticks[size_t(st)] = w(key); }
        br.target_rider = golden_rider(h, "rider.0.brain_168");
        br.nbr_ahead = golden_rider(h, "rider.0.brain_16c");
        br.nbr_behind = golden_rider(h, "rider.0.brain_170");
    }
    if (demo && solo) {
        // Frame 0 is post tick 1: seeded captures resume at tick 2, others
        // run tick 1 natively too.
        race.set_ticks(int32_t(brain_window ? tick0 : tick0 - 1));
        const uint32_t rng_before = race.rng_state();
        if (!brain_window) race.countdown_step();
        for (size_t i = 1; i < green; ++i) {
            race.countdown_step();
            if (std::getenv("RR_SIM_TRACE")) std::cout << "countdown tick " << tick0 + int64_t(i) << ": " << brain_line(g.frames[i]) << '\n';
            const std::string d = brain_diff(g.frames[i]);
            if (!d.empty()) {
                std::cout << "first divergence at frame " << i << " (tick " << tick0 + int64_t(i) << ", countdown): " << d << '\n';
                if (!std::getenv("RR_SIM_CONTINUE")) return 1;
            }
        }
        if (race.rng_state() != rng_before) { std::cout << "countdown consumed draws\n"; return 1; }
        if (std::getenv("RR_SIM_SIZES")) for (const auto& r : race.riders())
            std::cout << "sizes slot " << r.slot << " pos " << r.bike.i32(0x1c) << " w " << r.bike.i32(0x174) << " l " << r.bike.i32(0x178) << " h " << r.bike.i32(0x17c) << '\n';
        std::cout << "countdown: " << green - 1 << " native ticks, brain equal\n";
    } else {
        race.set_ticks(int32_t(tick0 + int64_t(green) - 1));
    }
    std::vector<std::pair<int64_t, uint64_t>> rng_history;  // (golden, native)
    size_t uncovered = 0;  // native fields no golden era dumps (acceptance gaps, counted not skipped)
    for (size_t i = green; i < g.frames.size(); ++i) {
        const auto& frame = g.frames[i];
        auto fc = frame.find("controls");
        if (fc == frame.end()) throw std::runtime_error("frame lacks controls");
        if (reanchor && i > green) { auto fp = g.frames[i - 1].find("rng"); if (fp != g.frames[i - 1].end()) race.set_rng_state(uint32_t(fp->second)); }
        // Demo goldens ignore control words (AI branch never reads them);
        // human goldens consume them through apply_controls (0x00434660).
        race.step(demo ? 0 : uint32_t(fc->second));
        auto snap = race.snapshot();
        // Frozen opponents never move and the solo dump names only the
        // player's bike and half (rider.0/rider.1): drop their rows.
        if (solo) {
            std::vector<rr::FieldValue> kept;
            for (auto& f : snap.fields) {
                const bool frozen_row = f.name.rfind("rider.", 0) == 0 && f.name.rfind("rider.0.", 0) != 0;
                if (!frozen_row) kept.push_back(std::move(f));
            }
            snap.fields = std::move(kept);
        }
        // Diagnostic: RR_SIM_TRACE=<prefix> prints every native field with
        // that prefix beside its golden value (same key) each replayed tick.
        if (const char* trace = std::getenv("RR_SIM_TRACE")) {
            std::cout << "tick " << tick0 + int64_t(i) << ':';
            for (const auto& f : snap.fields) {
                if (f.name.rfind(trace, 0) != 0) continue;
                auto it = frame.find(f.name);
                std::cout << ' ' << f.name.substr(f.name.rfind('.') + 1) << '=' << f.value;
                if (it != frame.end() && it->second != f.value) std::cout << "/orig " << it->second;
            }
            std::cout << "  " << brain_line(frame) << '\n';
        }
        const auto fr = frame.find("rng");
        const uint64_t want_rng = fr == frame.end() ? 0 : uint64_t(fr->second);
        rng_history.push_back({want_rng, snap.rng});
        if (rng_history.size() > 8) rng_history.erase(rng_history.begin());
        const auto ft = frame.find("tick");
        if (ft != frame.end() && ft->second != tick0 + int64_t(i)) {
            std::cout << "tick not contiguous at frame " << i << '\n';
            return 1;
        }
        // RNG is compared first every tick (plan 10 review focus 1): a
        // stream-position break is reported before any field, with the
        // last-8-ticks history for both sides.
        // Diagnostic: RR_SIM_CONTINUE=1 keeps replaying past divergences
        // (each tick re-anchors nothing; use with RR_SIM_TRACE).
        static const bool keep_going = std::getenv("RR_SIM_CONTINUE") != nullptr;
        if (keep_going) continue;
        if (want_rng != snap.rng) {
            std::cout << "first divergence at frame " << i << " (tick " << tick0 + int64_t(i) << "): rng original " << want_rng << " native " << snap.rng << '\n';
            std::cout << "rng history (last " << rng_history.size() << " ticks, golden vs native):";
            for (const auto& e : rng_history) std::cout << " (" << e.first << "," << e.second << ")";
            std::cout << '\n';
            return 1;
        }
        if (const std::string d = brain_diff(frame); !d.empty() && h.find("rider.0.brain_160") != h.end() && demo && solo) {
            std::cout << "first divergence at frame " << i << " (tick " << tick0 + int64_t(i) << "): " << d << '\n';
            return 1;
        }
        std::string missing;
        if (int rc = compare_fields(frame, snap.fields, i, tick0 + int64_t(i), rng_history, uncovered, missing)) {
            if (!missing.empty()) std::cout << "missing required field at frame " << i << " (tick " << tick0 + int64_t(i) << "): " << missing << '\n';
            return rc;
        }
    }
    // Coverage report: golden-only extras are diagnostic context (not
    // acceptance); uncovered native fields are acceptance gaps the proxy
    // never dumped (listed, never silently skipped).
    {
        size_t extras = 0;
        const auto snap0 = race.snapshot();
        for (const auto& kv : g.frames.back()) {
            bool native_has = false;
            for (const auto& f : snap0.fields) if (f.name == kv.first) { native_has = true; break; }
            if (!native_has && kv.first != "tick" && kv.first != "rng" && kv.first != "rng_calls" &&
                kv.first != "controls" && kv.first != "riders" && kv.first != "course" && kv.first != "level" &&
                kv.first != "bike" && kv.first != "character" && kv.first != "seed" && kv.first != "solo" &&
                kv.first != "tick1_caller" && kv.first != "probe.S" && kv.first != "probe.end" &&
                kv.first != "probe.player" && kv.first != "player.demo_hold" && kv.first != "player.think" &&
                kv.first != "player.think_count" && kv.first != "grid.bias" &&
                kv.first != "demo.base_speed" && kv.first != "demo.lane") ++extras;
        }
        // Name one uncovered native field per rider slot for the report.
        std::string uncovered_names;
        {
            std::map<std::string, bool> seen;
            // Re-scan the final snapshot against the final golden frame.
            for (const auto& f : snap0.fields) {
                const auto dot = f.name.rfind('.');
                const std::string prefix = f.name.substr(0, dot + 1);
                const std::string suffix = f.name.substr(dot + 1);
                if (g.frames.back().find(f.name) != g.frames.back().end()) continue;
                std::string key;
                const char* mapped = golden_key_for(prefix, suffix, g.frames.back(), key);
                if ((!mapped || g.frames.back().find(key) == g.frames.back().end()) && !seen[suffix]) {
                    seen[suffix] = true;
                    uncovered_names += (uncovered_names.empty() ? "" : ",") + suffix;
                }
            }
        }
        std::cout << g.frames.size() << " ticks identical (" << extras << " golden-only diagnostic fields not in acceptance set; "
                  << uncovered << " field-comparisons uncovered across eras: " << uncovered_names << ")\n";
    }
    return 0;
}
