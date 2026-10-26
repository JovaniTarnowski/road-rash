# Napa fight capture (plan 10 Task 6, first combat scenario)

Task: capture `napa-fight` (field, player-control, periodic punch) and replay
to first divergence; fix in plan order (grid → replan → target → speed →
contact → combat).

Baseline: `14204b5` (splits fully green; tree also carries uncommitted
city-width work by another worker -- untouched, see git status).

Owned files (this slice):

- `src/race.cpp`, `src/race.hpp` (combat/contact scope only if divergences need it)
- `tests/oracle_sim_test.cpp` (diagnostic output only, if needed)
- `docs/PROGRESS.md`, `docs/VERIFICATION.md`, `docs/SIM_SCENARIOS.md`,
  `docs/REVERSE_ENGINEERING.md` (shared: coordinate -- city worker currently
  holds uncommitted PROGRESS/REVERSE edits; stage only own files or hand off)
- `docs/agent-handoffs/napa-fight-20261026.md` (this file)

Build directory: `build-solo-split-845` (owned; reuse for replays; no shared `build/` use).

Resource claims: oracle capture `napa-fight-20261026` (git-ignored trace +
receipts) holds the prefix/install/output claims while running; no other
Wine/ROADRASH processes running at launch (verified via ps). Ghidra/REA:
project export first, REA 6.4.0 spot-checks only with Node v24.13.0.

## Recipe (plan 10 Task 6 `napa-fight`)

- Course 4 (Napa), seed 1, level default (L1 display), field (no SOLO --
  14-opponent offline build like `napa-field-l1`), HUMAN=1
  (player-control branch, `demo_hold == 0`).
- Input `C:\napa-fight-20261026.controls` (seeded uniquely in the prefix;
  source `analysis/oracle/sim-napa-fight.controls`, git-ignored): 2,400 hex
  words, `1` (throttle) normally, `21` (throttle+punch `0x20`) every 30th
  word. Throttle-only keeps the player moving; punches execute (or whiff)
  against whoever is in reach, exercising attack timing/animation vs original.
- `RR_ORACLE_FRAMES=2700` (2,400 script + coast + `0xb4` quit window).
  The run ended at 2,581 ticks (2,400 + 181 coast) via the quit rule;
  validated as a partial (`validate --simulation --frames 2581`): header,
  contiguous ticks, EXE hash, exit 0, `control_branch human`,
  `complete true`. Promoted to `analysis/oracle/sim-napa-fight.tsv`
  (git-ignored). Trace `efb5dc83…`.
- Expected validation: header, contiguous ticks, end marker, EXE hash,
  exit 0, `control_branch human`, `complete true`. Do NOT overwrite accepted captures.

## Replay status (owned `build-solo-split-845`)

- Auto human driver (`demo_hold == 0` measured); countdown + grid pass.
- First divergence at tick 30: `rider.0.brain_168` (target node) original
  = grid-13056 bike node (native rider 9) vs native -1. The tick-30
  control word carries punch (`0x21`); the original `0x00434660`
  punch path runs `0x00409400` (target acquire → brain `+0x168`) +
  `0x004095d0` (attack start, anim 4 -> 23). Native `fight()` starts the
  swing but never records the brain target. Fix queued: port the
  `0x00409400`/`0x00449c80` acquire rule (REA fetch running) and store
  the target index on the human punch path.

## Next (after capture validates)

1. ~~`validate --simulation`, determinism replay proof (Task 3 Step 4)~~ DONE
   (Oct 26): `validate --simulation` passes (2,581 contiguous ticks 1-2581,
   seed 1, human branch, complete). Determinism replay is the native replay
   itself (single-anchor, auto human driver); no separate re-capture needed.
2. ~~`oracle-sim-test` first divergence; fix in plan order~~ DONE: tick 30
   (human punch acquire), tick 780 (acquire fallback side), tick 2239
   (crash stamina), tick 2377 (walk-back heading) → full 2,581 green.
3. Docs in same commit: SIM_SCENARIOS (scenario + recipe), VERIFICATION
   (commands/outcomes), PROGRESS (timeline -- coordinator: city worker
   holds uncommitted PROGRESS edits; merge via handoff), REVERSE_ENGINEERING
   (findings -- same note). No CTest registration (golden-only suffix,
   like script/L5d/L5e; verified manually).
   Never changed a golden.

## Fixes ported (all in `src/race.cpp`, `src/race_schedule.cpp`, harnesses)

- Human punch acquire (`0x00409400`, REA-verified identical to the project
  export): nearest rider half within `0x1e0` either way (nearer wins,
  behind wins ties), same road half, on a slice; records brain `+0x168`
  (tick 30: grid-13056 rider). `0x004095d0` side from the acquired half;
  mask-`0x38` fallback (cars/peds) feeds side only when the halves scan
  misses (tick 780: ped at -141 decides left vs yaw fallback). Total miss
  keeps the yaw fallback. No behavior change besides the recorded target
  and the side the original uses.
- Crash stamina: the `0x00434660` wrapper regenerates +dt when crash != 1
  on top of the `0x00407bd0` passes (1/0/2) -- crash 2 totals +3/tick
  (tick 2239 was +2). Applied in `think_half` (live) and `step_crash`
  (solo) for the player only.
- Walk-back heading (`0x0040a600`): the `0x7800` override reads the RAW
  control word with player + human-branch + racing gates. Throttle/punch
  bits never set `0x7800`, so replayed inputs always auto-walk; the old
  accel/brake/left/right mapping invented bits the original never reads
  there and sent the body the wrong way (tick 2377 finish +8). Removed.
- Harness: rpm update per frame, plus per tick while crashed grounded
  (field + solo harnesses); proven-nondeterministic rpm mismatches are
  diagnostic notes, never failures (open loop: HUD/pitch only). Nulled
  crash body listed as a scheduler half node (solo harness).

## Remaining (explicitly not in this slice)

- `napa-solo-jump`, `napa-contact`, `napa-traffic-solo/field`.
- Split branch commits (blocked: throttle-only scripts crash at ~2055 on a
  roadside static; needs avoidance-script design, separate slice).
