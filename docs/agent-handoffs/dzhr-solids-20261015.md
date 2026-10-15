# Handoff: DZHR roadside solidity slice (plan 16 Task 2 / plan 24) — October 15, 2026

## Task / plan

Plan 16 Task 2, following the read-only `roadside-trace-20261015-scenery.md`
(static-only items): DZHR non-actor billboards with low-nibble type 0/1/2
collide through the class-1 hazard path; types 3+ are marking-only (no
crash). Tall-wall reason-3 (`0x0043a610`) and capture goldens stay open.

Baseline: `71dc6d6` (branch `main`) plus the SIM worker's uncommitted
frozen-state integration (preserved untouched).

## Mode

Implementation slice: `src/race.hpp` (struct + API), `src/race.cpp`
(builder + tick rule), `src/race_setup.cpp` (factory wiring),
`tests/race_test.cpp` (synthetic coverage), this handoff. Owned build dir
`build-goal-20261015`; no shared dirs, no captures, no prefix writes.

## Changes

- `src/race.hpp`: `DzhrSolid{lo_q8, hi_q8, type}` (type = record low
  nibble), pure `dzhr_solids_for_sample(centre_q8, objects)` (forward
  `SceneryObject`, no include cycle), `Race::set_roadside()` +
  `dzhr_[2]` tables beside `hazards_`.
- `src/race.cpp`: builder keeps static-family billboards (BORR
  `roadside_objects` skipped as draw-only), absolute Q8 lateral =
  route-frame centre + placed lateral (the rider-lateral frame), ±0x800
  size-8 half extent (approximation, documented: per-type template widths
  untraced). Tick: `collide_hazards` runs the DZHR table after the BORR
  spans (same sweep point); types ≥ 3 skip (the `0x004049c0 == 2` marking
  branch only records on the unported static actor — nothing to write
  natively). Also restructured the BORR early-returns so an empty BORR
  table no longer skips the DZHR loop (behavior identical when spans
  exist).
- `src/race_setup.cpp` (`build_race`, after terrain source): wires both
  routes' tables from `scene->objects(section, local)` via the pure
  helper, with `config.road_base_q8`. Consumes no randomness (placement
  ran once at load), so the shared race stream is untouched and builds
  are deterministic. BORR spans deliberately NOT wired: they sit strictly
  inside the road edges on the racing line and the autopilot/AI have no
  avoidance (0x00408e40 unported) — wiring them would crash every
  headless soak. Noted as follow-up with that dependency.
- `RouteSample.section/local` (`src/course.hpp:60`) is the existing
  route→scene map; no new mapping code.

## Tests (`tests/race_test.cpp`, synthetic, no disc)

- Packing: billboards kept with nibble types, BORR skipped, lateral math
  `(centre + lat*256) ± 0x800`.
- Tick: types 0/1/2 crash reason 1 (`+0x2f4 != 0 && +0x2f8 == 1`) through
  `contact_hazard`; type 3 changes nothing (no crash, stamina and lateral
  untouched); needs healthy `+0x2f0` like all contact fixtures (default
  zero condition crashes on the tick-0 overlap — same gotcha as the
  weapon pins).

## Gotchas

1. The `spans.empty() → return` early exit (pre-existing) skipped the new
   loop whenever BORR was empty — i.e. in every synthetic test and, had it
   shipped first, in every game without BORR tables. Restructured to
   per-table guards.
2. `set_hazards` has no game caller: BORR collision is currently test-only
   and the live game has zero roadside collision. This slice wires DZHR
   only (roadside, autopilot-safe); BORR wiring waits for AI avoidance.
3. `SceneryObject.type` already holds `word0 & 0xf` for DZHR billboards
   (`scenery.cpp:60`) and 0 for BORR; `static_family` (true only for
   DZHR, `:60` vs `:27`) is the discriminator, not the type value.

## Checks

- `cmake --build build-goal-20261015 --target race-test race_setup-test`:
  exit 0, no warnings. `race-test`: 0 failures (2 reds diagnosed and
  fixed: the early-return above; the zero-condition overlap).
- `ctest -R '^race$|^car_contact$|^frozen_state$'`: 3/3 pass.
- `oracle-sim-test --driver=demo sim-napa-solo-human.tsv`: signature
  UNCHANGED (frame 209 / tick 210, rng 849046096 vs 2140064586) with DZHR
  wired live — the centred demo-hold line never meets a roadside object.
- LSP: `agent_lsp.py --check` on all three files: available, no missing
  sources (shared `build-clangd` staleness is the SIM worker's uncommitted
  CMake edit, pre-existing).
- Full fast gate + `race-soak` (autopilot-vs-objects on all courses):
  running in background at handoff time; commit waits for it.

## Live resources

Background gate shell owned by this session (releases on completion).
No locks, no captures, no shared builds.

## Next (for the integrator / combat owner)

- `napa-crash-scenery` capture golden (steer into a tall wall + a DZHR
  type-0/1/2 lateral from a `NAPA.CRS` dump): reason-3 vs reason-1 vs
  clamp; type names; `+0x38 == 5` course identity (trace §6).
- BORR live wiring + AI avoidance (0x00408e40) — until then the game has
  DZHR-only roadside collision by design.
- `docs/PROGRESS.md` assumptions/test tables (shared docs, untouched).
