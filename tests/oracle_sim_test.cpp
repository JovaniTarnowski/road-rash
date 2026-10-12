#include "actor_nodes.hpp"
#include "assets.hpp"
#include "bike.hpp"
#include "career.hpp"
#include "course.hpp"
#include "course_scene.hpp"
#include "crash.hpp"
#include "exe_tables.hpp"
#include "golden.hpp"
#include "race.hpp"
#include "traffic.hpp"
#include <cstring>
#include <filesystem>
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
// Shared race-construction factory (game + replay, one mechanism). The game
// builds its race in Game::load_course/restart (src/game_main.cpp); the
// replay below builds the same objects from the golden header the same way:
// header-driven bike/rider selection (no SPEC-1 default), the course's own
// vehicle library WITH its EXE descriptors (traffic.cpp requires the EXE
// for views/playback), traffic + crash config attached to the Race, and
// grid-lateral placement from the game's own route data (no absolute
// course-specific anchor: Race lateralises from route offset + grid word,
// exactly as 0x00405990 reads sub-slice +0xcc plus the grid offset).
struct RaceBundle {
    rr::Course course;
    std::vector<rr::RouteSample> route_a, route_b;
    rr::PhysicsTables physics;
    rr::BikeSpec spec;
    int32_t rider_value = 90;
    std::vector<rr::Opponent> opponents;
    std::vector<rr::Personality> personalities;
    std::unique_ptr<rr::CourseScene> scene;
    std::unique_ptr<rr::VehicleLibrary> vehicles;
    std::unique_ptr<rr::TrafficSystem> traffic;
    std::unique_ptr<rr::ActorNodes> actors;
    rr::CrashConfig crash_config;
};
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
RaceBundle make_race_bundle(rr::Assets& assets, const std::vector<uint8_t>& exe,
                            const std::string& course_name, int level, bool solo) {
    // Mirrors Game::load_course + the career race-setup path (game_main.cpp):
    // default character 1 / level-0 bike from the career tables, opponents
    // from original_opponents, scene + vehicle library + traffic + crash
    // config owned by the bundle. Throws std::runtime_error on bad data.
    RaceBundle b;
    b.course = rr::read_course(assets.read("DATA/COURSES/" + course_name + ".CRS"));
    b.route_a = rr::make_route(b.course, false);
    b.route_b = rr::make_route(b.course, true);
    b.physics = rr::read_physics_tables(exe);
    const auto specs = rr::read_bike_specs(assets.read("DATA/BIKESPEC.RSC"));
    const auto tables = rr::read_career_tables(exe);
    // Header-driven selection: default character 1 (-> rider 8, 0x00415fc0)
    // at level 0, exactly as reset_for_character picks the career default.
    // No SPEC-1 / weight / stats hard-codes: everything comes from the EXE.
    const int rider = rr::character_rider(1);
    const size_t bike = tables.level_bike.at(size_t(level) + size_t(rider) * 5);
    if (bike >= specs.size()) throw std::runtime_error("Default bike index out of range");
    b.spec = specs[bike];
    const RiderRow row = read_rider_row(exe, rider);
    b.rider_value = row.value;
    b.opponents = solo ? std::vector<rr::Opponent>{} : rr::original_opponents(exe, specs, level, 7);
    if (course_name == "NAPA") {
        b.scene = std::make_unique<rr::CourseScene>(assets, exe, b.course, course_name, 1, level);
        b.actors = std::make_unique<rr::ActorNodes>(assets, exe, true);
        b.crash_config = rr::read_crash_config(exe);
    }
    if (assets.exists("DATA/CARS/" + course_name + ".CAR")) {
        // WITH EXE descriptors (views + playback banks): without them the
        // library carries animation frames only, unlike the game's.
        b.vehicles = std::make_unique<rr::VehicleLibrary>(
            assets.read("DATA/CARS/" + course_name + ".CAR"), exe);
        b.traffic = std::make_unique<rr::TrafficSystem>(*b.vehicles, exe);
        b.traffic->configure(6);
    }
    b.personalities = rr::read_personalities(exe);
    return b;
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
    // Measured in current captures: rider.0.think == 4225680 (0x00407990,
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
    RaceBundle bundle = make_race_bundle(assets, exe, course_name, level, solo);
    // Grid lateral: the original places the player at sub-slice +0xcc (the
    // absolute road centre at the grid sample) plus the 0x00464218 grid
    // word. The native route stores centres relative to the course root,
    // so the harness passes the difference as the Race road_base_q8 anchor
    // via the shared rr::race_road_base_q8 helper (same ctor the game
    // uses; the game passes 0 = relative frame). Measured per golden (not
    // a course constant), reported every run.
    auto hpos = h.find("rider.0.position"), hlad = h.find("rider.0.lateral");
    int32_t road_base = 0;
    if (hpos != h.end() && hlad != h.end()) {
        const size_t sample = size_t(uint32_t(hpos->second) >> 8);
        const int32_t grid_word = exe_i32(exe, 0x00464218 + 8u * uint32_t(level));
        const std::vector<rr::RouteSample>& ref = bundle.route_a;
        if (sample < ref.size())
            road_base = rr::race_road_base_q8(int32_t(hlad->second), ref[sample].offset, grid_word);
        std::cout << "road_base: " << road_base << " (golden lat " << hlad->second
                  << " - route[" << sample << "].offset*256 - grid " << grid_word << ")\n";
    }
    rr::Race race(std::move(bundle.route_a), std::move(bundle.route_b),
                  bundle.physics, bundle.spec, bundle.rider_value, std::move(bundle.opponents), level,
                  road_base, bundle.personalities, exe);
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
    if (demo) {
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
        const int32_t bias = dbias >= 0 ? int32_t(dbias) : race.riders().at(0).brain.bias;
        const int weapon = dweapon >= 0 ? int(dweapon) : row.weapon0;
        const rr::RiderStats stats{dstr >= 0 ? int32_t(dstr) : row.stats.strength,
                                  dbas >= 0 ? int32_t(dbas) : row.stats.base,
                                  dmax >= 0 ? int32_t(dmax) : row.stats.max,
                                  dcur_st >= 0 ? int32_t(dcur_st) : row.stats.current};
        // Seed the player brain tables from the measured row BEFORE
        // set_demo_hold (the ctor loaded personalities[0]; correct it when
        // the measurement points elsewhere).
        if (!personalities.empty() && brain_row != 0 && !race.riders().empty()) {
            auto& brain = race.riders()[0].brain;
            const auto& p = personalities[size_t(brain_row)];
            for (int i = 0; i < 5; ++i) { brain.spd_a[i] = p.cur2_levels[i]; brain.spd_b[i] = p.cur_levels[i]; }
            for (int i = 0; i < 30; ++i) { brain.rating[i] = p.rating[i]; brain.rating_tgt[i] = p.rating_tgt[i]; }
            for (int i = 0; i < 3; ++i) brain.ease[i] = p.ease[i];
            brain.overtake_index = p.overtake_index;
            brain.reach = p.reach;
        }
        race.set_demo_hold(true, base, lane, bias, weapon, stats);
    }
    if (bundle.scene) race.set_terrain_source([&](const rr::RouteSample& s) -> const rr::TerrainSlice& { return bundle.scene->terrain(s.section, s.local); });
    if (bundle.traffic) race.set_traffic(bundle.traffic.get());
    if (bundle.actors) race.set_crash_library(bundle.actors->library(), bundle.crash_config);
    // Plan 16: descriptor-frame combat timing needs the ride bank + attack
    // codes (same library the game uses); nitro setup copies the player
    // spec +0x14c words (0x0040b0e0, from 0x00405600 after the player build).
    if (bundle.actors) {
        static std::array<std::array<std::array<int8_t,4>,2>,2> attack_codes{};
        static bool attacks_loaded = false;
        if (!attacks_loaded) {
            for (int side = 0; side < 2; ++side) {
                const auto bytes = rr::read_exe_data(exe, side ? 0x464518 : 0x4644f8, 32);
                for (int kind = 0; kind < 2; ++kind)
                    for (int weapon = 0; weapon < 4; ++weapon)
                        attack_codes[side][kind][weapon] = int8_t(bytes[(kind * 4 + weapon) * 4]);
            }
            attacks_loaded = true;
        }
        race.set_ride_library(&bundle.actors->library(), &attack_codes);
    }
    {
        rr::NitroConfig nitro;
        bool spec_nitro = false;
        for (int k = 0; k < 6; ++k)
            if (bundle.spec.d[size_t(77 + k)] != 0) spec_nitro = true;
        if (spec_nitro) {
            nitro.recharge = bundle.spec.d[77]; nitro.boost = bundle.spec.d[78];
            nitro.max = bundle.spec.d[79]; nitro.charges = bundle.spec.d[80];
            nitro.minimum = bundle.spec.d[81]; nitro.tail = bundle.spec.d[82];
        }
        race.set_nitro(nitro);
    }
    // The golden rng is the live CRT state AFTER each tick's step (the hook
    // dumps after 0x0044b3b0). The race-setup stream (menu entry through grid
    // build, 11,461 calls) is not reproduced: a single anchor at green covers
    // it (known limitation until the setup consumers are ported).
    const int64_t tick0 = get("tick");
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
        const auto snap = race.snapshot();
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
            std::cout << '\n';
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
        if (want_rng != snap.rng) {
            std::cout << "first divergence at frame " << i << " (tick " << tick0 + int64_t(i) << "): rng original " << want_rng << " native " << snap.rng << '\n';
            std::cout << "rng history (last " << rng_history.size() << " ticks, golden vs native):";
            for (const auto& e : rng_history) std::cout << " (" << e.first << "," << e.second << ")";
            std::cout << '\n';
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
