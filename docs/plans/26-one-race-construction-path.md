# One Race Construction Path Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first. Every task here is **sim-sensitive**; use the most capable model for implementation and review.

**Goal:** The playable game, `race-soak` and the `oracle-sim` replay all build races through one factory, so behaviour verified against the original is the behaviour players get.

**Architecture:** Split `make_race_setup` (src/race_setup.*) into course-scoped `load_race_assets` and race-scoped `build_race`. `build_race` attaches **every** `Race` hook: personalities and EXE image, finish line, terrain, traffic, pedestrians, track ring, crash library, ride library, attack codes and nitro. `Race::wiring()` reports which hooks are attached so tests can assert completeness. The oracle harness adopts the factory first, with a byte-identical diagnostic report as proof. Then the game adopts it, which intentionally changes game behaviour to the verified path.

**Tech Stack:** C++20, CTest, `tools/verify_sim.py`, the disc under `../cdrom/ROADRASH` (golden-backed steps exit 77 without it).

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md) P0. It finishes [plan 17](17-all-courses-port.md) Task 2 ("one `make_race_setup`"), which created the factory but never moved the game or the harness onto it.

## Global Constraints

- `oracle-sim` (diagnostic mode) output must be **byte-identical** before and after Tasks 1, 2 and 4. `verify-sim` acceptance stays as it is (currently failing for known reasons); never bless a divergence.
- Game behaviour changes only in Task 3, and the handoff states which hooks the game gained.
- `race-soak` stays deterministic (run A equals run B). Its printed hashes **change** in Task 1 because the factory gains hooks; record the old and new hashes and the reason.
- No RNG consumption is added or reordered relative to the oracle harness's current order: ctor, prepare hook (demo hold), pedestrians, freeze, RNG state, ring, terrain, traffic, crash library, ride library, nitro.
- `Race` keeps raw pointers to the ride library, attack codes, traffic, pedestrians and ring. Their owners are heap objects in `RaceAssets`/`RaceInstance` and must not move after `build_race` returns.

## Review Focus

1. **Moving a `RaceSetup` after wiring.** Every object `Race` points to must live behind a `unique_ptr` (the attack codes in particular). Task 1's test moves the setup and steps the race under ASan.
2. **Restart in the game reuses course assets but must rebuild traffic, pedestrians and the ring.** Old cars or pedestrians must not leak into the new race (Task 3 test: cars are empty at tick 0 after a restart).
3. **Courses without a `.CAR` file, or whose scene fails to load.** The game today falls back to the flat road; the factory throws. Task 3 keeps the game's fallback explicit, with `wiring().terrain == false` logged.
4. **The demo/attract path** (`set_demo_hold`) must still run before the ring consumes RNG (Task 1's `prepare` hook).
5. **The quick-race bike choice (`--bike`, Thrash shop)** versus the career table bike: `RaceConfig::career=false` must keep honouring `config.bike` (Task 3 test).

---

### Task 1: Complete the factory and add `Race::wiring()`

**Wave:** W2 (after plan 25 Task 6 merges) · **Owns:** `src/race_setup.hpp`, `src/race_setup.cpp`, `src/race.hpp` (the `RaceWiring` struct and accessor only), `src/race.cpp` (the `wiring()` body only), `tests/race_setup_test.cpp` (exists: plan 17 Task 2 sweep, 35 s), `tests/race_soak_test.cpp` (field-path updates only) · **Profile:** sim-sensitive

**Files:**
- Modify: `src/race_setup.hpp`, `src/race_setup.cpp`, `src/race.hpp`, `src/race.cpp`, `tests/race_soak_test.cpp`, `tests/race_setup_test.cpp` (already registered in CMake; keep its 25-race sweep and update its field paths to `setup.assets.*` / `setup.instance.*`)

**Interfaces:**
- Produces (exact names; Tasks 2–4 and plan 28 use them):
```cpp
namespace rr {
using AttackCodes = std::array<std::array<std::array<int8_t,4>,2>,2>;
AttackCodes read_attack_codes(const std::vector<uint8_t>& exe);   // 0x004644f8 / 0x00464518, 8 words each
NitroConfig nitro_from_spec(const BikeSpec&);                     // words 77..82 (+0x14c); all zero -> NitroConfig{}

struct RaceConfig {             // existing fields kept; new fields:
    // ...course, level, bike, character, career, opponents, high_resolution, metric...
    bool solo = false;                         // freeze opponents + scheduler actors (oracle)
    int32_t road_base_q8 = 0;                  // Race ctor anchor (game 0; replay measured)
    bool track_ring = true;                    // ring-driven DZHR pedestrians (0x004552e0)
    std::optional<uint32_t> ring_rng_state;    // RNG state loaded just before ring setup
    std::function<void(Race&)> prepare;        // runs after ctor, before ring (demo hold)
};
struct RaceAssets {             // course-scoped: loaded once per course
    Course course;
    std::unique_ptr<CourseScene> scene;
    std::unique_ptr<Hud> hud;
    std::unique_ptr<ActorNodes> actors;
    std::unique_ptr<VehicleLibrary> vehicles;  // null when the course has no .CAR
    std::unique_ptr<AttackCodes> attack_codes;
    CrashConfig crash;
    PoliceConfig police;
    RaceBounds bounds;
    PhysicsTables physics;
    std::vector<BikeSpec> specs;
    std::vector<Personality> personalities;
    int bike_index = 0;
    int32_t rider_value = 0;
};
struct RaceInstance {           // race-scoped: rebuilt on restart
    std::unique_ptr<TrafficSystem> traffic;
    std::unique_ptr<PedestrianSystem> pedestrians;
    std::unique_ptr<TrackRing> ring;
    std::unique_ptr<Race> race;
};
RaceAssets load_race_assets(const Assets&, const std::vector<uint8_t>& exe, const RaceConfig&);
RaceInstance build_race(const RaceAssets&, const std::vector<uint8_t>& exe, const RaceConfig&);
struct RaceSetup { RaceAssets assets; RaceInstance instance; };
RaceSetup make_race_setup(const Assets&, const std::vector<uint8_t>& exe, const RaceConfig&);  // load + build
}
// race.hpp, public:
struct RaceWiring { bool terrain=false, traffic=false, pedestrians=false, ring=false, crash_library=false,
                    ride_library=false, nitro=false, finish=false, personalities=false; };
RaceWiring wiring() const;   // derived from the existing members; no new state except nitro_set_
```

- [x] **Step 1: Record the baselines.** In `analysis/review-26/before/`, save `ctest -R "race-soak|oracle-sim" -V` output and `build-wt-26-1/oracle-diagnostic.json`.

- [x] **Step 2: Write the failing test.** Append a second section to the existing `tests/race_setup_test.cpp` `main` (it already exits 77 without the disc). The block below is self-contained; merge its includes and reuse the existing `assets`/`exe` instead of re-reading them:

```cpp
#include "race_setup.hpp"
#include "golden.hpp"
#include <iostream>
#include <utility>

int main() {
    int failures = 0;
    auto check = [&](bool v, const std::string& what) { if (!v) { ++failures; std::cerr << "FAIL " << what << '\n'; } };
    const std::string root = std::string(RR_SOURCE_DIR) + "/../cdrom/ROADRASH";   // same as race_soak_test
    if (!std::filesystem::exists(root + "/ROADRASH.EXE")) { std::cout << "SKIP no disc\n"; return rr::golden::skip; }
    rr::Assets assets(root);
    const auto exe = assets.read("ROADRASH.EXE");
    for (const char* course : {"NAPA", "CITY", "HIWAY", "CANYN", "MEDLY"}) {
        rr::RaceConfig config; config.course = course; config.character = 1;
        rr::RaceSetup setup = rr::make_race_setup(assets, exe, config);
        const rr::RaceWiring w = setup.instance.race->wiring();
        const std::string c = course;
        check(w.terrain && w.pedestrians && w.ring && w.crash_library && w.ride_library &&
              w.nitro && w.finish && w.personalities, c + ": every hook attached");
        check(w.traffic == bool(setup.assets.vehicles), c + ": traffic iff vehicles");
        rr::RaceSetup moved = std::move(setup);           // pointers must survive a move
        for (int t = 0; t < 600; ++t) moved.instance.race->step(0x1);
        rr::RaceInstance again = rr::build_race(moved.assets, exe, config);   // restart path
        check(again.race->ticks() == 0, c + ": fresh race");
        check(!again.traffic || again.traffic->cars().empty() ||
              std::none_of(again.traffic->cars().begin(), again.traffic->cars().end(),
                           [](const auto& car) { return car.active; }), c + ": no cars carried over");
    }
    if (failures == 0) std::cout << "race_setup OK\n";
    return failures ? 1 : 0;
}
```
  Add `#include <algorithm>` and `#include <filesystem>`. `rr::golden::skip` is the constant `77` in `tests/golden.hpp`.

- [x] **Step 3: Run it and confirm it fails to compile** (`wiring`, `RaceInstance` and `build_race` do not exist yet).

- [x] **Step 4: Implement `wiring()`** in race.cpp from the existing members: `terrain = bool(terrain_source_)`, `traffic = traffic_ != nullptr`, `pedestrians = pedestrians_ != nullptr`, `ring = ring_ != nullptr`, `crash_library = crash_library_ != nullptr`, `ride_library = ride_library_ && attack_codes_`, `finish = finish_q8_ != 0`, `personalities = overtake_eagerness_.has_value()` (from plan 25 Task 6). Add `bool nitro_set_ = false;`, set to true in `set_nitro`, and return it as `nitro`.

- [x] **Step 5: Implement the factory split** in race_setup.cpp. Move the existing body's asset loading into `load_race_assets`, adding `attack_codes = std::make_unique<AttackCodes>(read_attack_codes(exe))` and `personalities = read_personalities(exe)`. `build_race` performs, **in this order**:
  1. `make_route(course,false/true)`; `Race` ctor with `assets.specs[bike_index]`, `rider_value`, `original_opponents(exe, specs, level, config.solo ? 14 : config.opponents)` (set `frozen` on each when solo), level, `config.road_base_q8`, `assets.personalities`, `exe`;
  2. `if (config.prepare) config.prepare(*race);`
  3. pedestrians: when `config.track_ring`, an actor source that returns a static empty vector (as the harness does); otherwise `scene->dzhr_actors`. Looks always come from `scene->pedestrian_look`. Then `race->set_pedestrians`, and `race->set_freeze_actors(true)` when solo;
  4. `if (config.ring_rng_state) race->set_rng_state(*config.ring_rng_state);` then, when `config.track_ring`, `ring = std::make_unique<TrackRing>(assets.course, config.level); race->set_track_ring(ring.get());`
  5. `set_finish_q8(bounds.finish_position_q8)` and the terrain source;
  6. traffic: when `vehicles` exists, `std::make_unique<TrafficSystem>(*vehicles, exe)`, `configure(6)`, `set_traffic`;
  7. `set_crash_library(actors->library(), crash)`, `set_ride_library(&actors->library(), attack_codes.get())`, `set_nitro(nitro_from_spec(specs[bike_index]))`.

  Copy `read_attack_codes` and `nitro_from_spec` verbatim from `tests/oracle_sim_test.cpp:477-505` (the static `attack_codes` loop and the spec-word block), as free functions. The bike/rider selection logic already in `make_race_setup` is unchanged.

- [x] **Step 6: Update `race_soak_test.cpp`** field paths (`setup.race` → `setup.instance.race`, `setup.scene` → `setup.assets.scene`). Change nothing else.

- [x] **Step 7: Run** `race_setup-test` (OK, or 77 without the disc), `ctest -R "race-soak|oracle-sim|race|traffic|pedestrian"`, and the ASan tree for `race_setup-test`. `oracle-diagnostic.json` must be identical to the baseline (the harness is untouched in this task). Record the changed soak hashes in the handoff.

- [x] **Step 8: Commit.** `race: one factory attaching every Race hook (assets/instance split, wiring report)`

---

### Task 2: The oracle harness builds through the factory

**Wave:** W3 · **Owns:** `tests/oracle_sim_test.cpp` · **Profile:** sim-sensitive

**Files:**
- Modify: `tests/oracle_sim_test.cpp` (`make_race_bundle`, `RaceBundle`, and the wiring block at lines ~370–505)

**Interfaces:**
- Consumes: `RaceConfig` (`solo`, `road_base_q8`, `ring_rng_state`, `prepare`), `make_race_setup`, `RaceSetup`.

- [x] **Step 1: Baseline.** Use the Task 1 "before" outputs. They must still match on the Task 1 merge commit; re-run to confirm.
- [x] **Step 2: Replace the bundle.** Keep the code that computes `road_base` (it reads `make_route(read_course(...))` for the sample offset; keep a local course read for that). Move the demo-hold block into a lambda assigned to `config.prepare`. Compute the ring RNG state exactly as now and pass it as `config.ring_rng_state`. Set `config.solo = solo`, `config.character = 1`, `config.career = true`. Then `rr::RaceSetup setup = rr::make_race_setup(assets, exe, config); rr::Race& race = *setup.instance.race;`. Delete the hand wiring for pedestrians, ring, terrain, traffic, crash, ride library and nitro, and delete `make_race_bundle`/`RaceBundle`. `ring_end_state` becomes `race.rng_state()` read immediately after `make_race_setup` returns. The factory's last RNG consumer is the ring (step 4), and steps 5–7 draw no RNG. Assert that with a comment and a check that compares against a state recomputed the old way, once.
- [x] **Step 3: Prove identity.** Run `ctest -R oracle-sim -V` and diff `oracle-diagnostic.json` against the baseline. They must be **byte-identical**, including `first_divergence`. If they differ, stop: the factory's order or configuration differs from the harness's, so fix the factory (via the orchestrator, since Task 1's files are merged) rather than the expectation.
- [x] **Step 4: Commit.** `test: oracle replay builds races through make_race_setup`

---

### Task 3: The game builds through the factory

**Wave:** W4 (after Task 2) · **Owns:** `src/game_main.cpp` · **Profile:** sim-sensitive (intentional game behaviour change)

**Files:**
- Modify: `src/game_main.cpp` (`Game` fields, `load_course`, `restart`, every `g.race`/`g.scene`/`g.traffic`/`g.actors`/`g.pedestrians` use)

**Interfaces:**
- Consumes: `load_race_assets`, `build_race`, `RaceConfig`, `RaceWiring`.
- Produces: `Game::assets` (`rr::RaceAssets`), `Game::instance` (`rr::RaceInstance`) and `Game::config` (`rr::RaceConfig`). The old members `course`, `scene`, `hud`, `actors`, `vehicles`, `traffic`, `pedestrians`, `race`, `crash_config`, `attack_codes`, `physics`, `specs`, `opponents` and `rider_value` are removed. Keep thin accessors `rr::Race& race()`, `rr::CourseScene* scene()` and `rr::TrafficSystem* traffic()` so the call-site churn stays mechanical.

- [ ] **Step 1: List the call sites.** `rg -n "g\.(race|scene|traffic|actors|pedestrians|hud|course|crash_config|attack_codes|specs|physics|opponents|rider_value)\b|\b(race|scene|traffic|actors|pedestrians|hud)->" src/game_main.cpp > /tmp/26-3-sites.txt`; attach the count to the handoff.
- [ ] **Step 2: `load_course`** becomes: `rr::clear_rgba_mip_cache(); assets = rr::load_race_assets(a, exe, config);`. On exception, keep today's fallback: log `"%s scene unavailable: %s"` and fall back to the flat-road path. Implement it as `config.track_ring = false` with a `RaceAssets` that has no scene, so `load_race_assets` needs a non-throwing scene step. Add `bool optional_scene = false` to `RaceConfig`; when true, a scene failure leaves `scene` null instead of throwing. That one-field `race_setup.cpp` change goes through the orchestrator.
- [ ] **Step 3: `restart`** becomes `instance = rr::build_race(assets, exe, config);`, followed by the existing actor-state initialisation and `reset_view_history()`. The quick-race bike (`--bike`, Thrash) maps to `config.career = false; config.bike = bike;`.
- [ ] **Step 4: Log the wiring once per race** (`SDL_Log("race wiring: terrain=%d traffic=%d ...")`). Add a `--print-wiring` CLI flag that prints that line to stdout and exits after the first tick.
- [ ] **Step 5: Register the headless check** (CMake hunk via the orchestrator): `add_test(NAME game-race-wiring COMMAND rr-game --assets ${RR_SOURCE_DIR}/../cdrom/ROADRASH --race --print-wiring)` with `PASS_REGULAR_EXPRESSION "terrain=1 .*ring=1 .*ride_library=1 .*nitro=1 .*finish=1 .*personalities=1"`. Without the disc, the game must exit 77 for `--print-wiring` (set `SKIP_RETURN_CODE 77`).
- [ ] **Step 6: Verify.** Full `ctest`; `rr-game --race --ticks 3600 --autopilot --screenshot` exits 0 on all five courses (`--course 0..4`); manual play of one Napa race: opponents overtake, attacks land on descriptor frames, the race ends at the original finish line, and Enter restarts with no cars carried over. Record what was observed in the handoff. This is an intentional behaviour change, so describe it in `PROGRESS.md` via the orchestrator.
- [ ] **Step 7: Commit.** `game: build races through the shared factory (personalities, ride bank, nitro, ring, finish)`

---

### Task 4: Remove leftovers and guard against a fourth path

**Wave:** W5 · **Owns:** `src/race_setup.hpp`, `tools/` (one new check), `CMakeLists.txt` hunk · **Profile:** mechanical

- [ ] **Step 1:** `rg -n "std::make_unique<rr::Race>|rr::Race race\(|new rr::Race" src tests` must list only `race_setup.cpp` and unit tests that use synthetic routes (`race_test`, `crash_test`, `bike_test`, `actor_traffic_test`, ...). Record the list.
- [ ] **Step 2:** Add the comment `// The only production/replay constructor of Race; tests with synthetic routes may build Race directly.` to `race_setup.hpp`. Add an `agent-tooling` check (`tests/agent_tooling_test.py` style) that fails if `src/` constructs `rr::Race` outside `race_setup.cpp`:
```python
import pathlib, re, sys
hits = [p for p in pathlib.Path("src").glob("*.cpp")
        if p.name != "race_setup.cpp" and re.search(r"make_unique<\s*(rr::)?Race\s*>|\bRace\s+\w+\s*\(", p.read_text())]
sys.exit(f"Race constructed outside race_setup.cpp: {hits}" if hits else 0)
```
- [ ] **Step 3:** Run the full gate, `tools/ci.sh` (Android debug assemble included), and commit: `race: single construction path guard`.
