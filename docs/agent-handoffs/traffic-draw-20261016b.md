# Traffic spawn-timer lane draw vs nulled pool (Plan 10 Task 7, 20261016b)

Date: 2026-10-16. READ-ONLY research at HEAD `5e4cd98`, no tracked-file edits, no builds, no captures, no commits.
Task: exclude the timer path (`0x0044dd90`/`0x0044df60` placement vs disabled placement) as the source of the
sched900 tick-893 +2 native RNG draws (suspected contact sound rolls).

## 1. Pinned REA spot-check (exactly one call, this task)

Function `0x0044df60` (placement) — `0x0044dd90` was already covered 20261016 (traffic-timer-trace), so the
complementary placement side is pinned here:

```
export PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"
export GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x0044df60 --format md
```

- Exit 0. EXE pre-verified `shasum -a 256` =
  `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (matches
  `analysis/inventory.json`, 541184 bytes).
- `evidence_id ev_c86a2496a56ac91d0d777dd9fa7a6490f409c6e8c18e2706bd3f3031880a04b9`,
  Ghidra 12.1.4, body `0x44df60-0x44e21c`.
- Full dossier saved as untracked scratch `analysis/traffic-draw-20261016b/rea-44df60-20261016b.md` (367974 B).
- Spot-check only, not runtime proof (repo REA policy).

### Draw sites in `0x0044df60` (all INSIDE the pool gate)

- Entry gate (first statement, pseudocode + disasm `0x44df63 MOV EAX,[0x0049e654]; 0x44df68 TEST; 0x44df77 JZ 0x0044e213`):
  `if (DAT_0049e654 != 0) { ... }` — nulled pool (`DAT_0049e654 == 0`) returns with **zero draws inside placement**.
- 1x direct `_rand()` (import thunk `0x455eb0`, call site `0x44e0b2`): `rand() % accepted` vehicle pick
  (`CDQ; IDIV` at `0x44e0b7-0x44e0bd`). NOTE: bypasses the `0x00441750` mode-3 canned-value wrapper; lane picks
  in `0x0044dd90` use the wrapper instead. Zero-draw expiry in mode 3 therefore applies to lane picks, not to
  this pick (solo-replay divergence proves real draws, mode != 3 — same parity caveat as the timer handoff).
- 4x `FUN_00441750` (call sites `0x44e10c/0x44e11f/0x44e12a/0x44e135`): phase `(rand&0x7f)+0xc0`, then three
  jitter bytes (`+0x72/+0x1c9/+0x1ca`). Serial `DAT_004b7e98` bump follows.
- Early exits consuming fewer: accepted-count 0 (`0x44e0ac JLE → 0x44e213`, only the type pick drawn) and
  `FUN_004387a0` null pool-slot (`0x44e0fc JZ → 0x44e213`, type pick only, no phase/jitter/serial).
- So per successful placement: **5 draws** (1 type + 4 phase/jitter). Per gated expiry with placement enabled:
  1 (lane, in caller) + up to 5 = up to 6.
- Caller xrefs in dossier confirm the lane-then-place split: `0x0041f040 → 0x0044df60` edge at `0x41f19d`;
  `0x0044dd90` edges at `0x44de00/0x44de7e/0x44df16` per the 20261016 timer handoff (3x `FUN_00441750` lane
  sites `0x44dde4/0x44de6c/0x44df04`, each inside a fire branch + char lane gate). Lane draws live in the
  CALLERS, never in `0x0044df60`.

## 2. Timer draw count per tick (solo nulled-pool replay)

Native `TrafficSystem::step_timers` (`src/traffic.cpp:530-577`, read via rg/sed only):

- Oncoming (`7e90`, init 3515): `oncoming -= div256(rate)+distance`; fire `state<0` → gated lane draw
  `random()%count` (lane `-1-(u%lanes)`), reset `+= oncoming_reload` (reset applies even when gate shut).
- Behind (`7e94`, init 0): `behind -= distance; behind += div256(rate)`; fire `state<0` → gated lane draw
  (lane `draw%lanes`), reset `+= other_reload`, else clamp `2*other_reload`.
- Ahead (`7e80`, init -3515): same update shape; fire `state>0` (INVERTED) → gated lane draw, reset
  `-= other_reload`, else lower clamp `-2*other_reload` with `!=` guard.
- Each branch fires at most once per tick and draws at most one lane pick, so the **timer path draws
  0-3 per tick** (one per gated expiry), typically 0. Placement draws add 0 in solo replay (below).
- Hold ticks (`countdown_step`, `src/traffic.cpp:508-513`, distance 0): bank only
  (`-9` oncoming / `+9` behind / `+9` rear per tick at rate 2500; 59 hold ticks → behind 531, rear/oncoming
  ∓2984). No fire possible before ~391 ticks, so **0 draws on every hold tick** — consistent with observed
  `rng_calls==0` + constant RNG across hold frames (countdown-driver-trace-20261016 §4).
- `0x0041f040` crossing think (`src/traffic.cpp:421-464`): `random()%range` lane/range draw precedes
  `place_crossing`; same draw-then-gate shape (native `if(spawn_enabled_)place_crossing` after the draw).

## 3. Why detached-traffic replay retains lane-pick draws

- `tests/oracle_sim_test.cpp:423-425`: solo replay calls `set_spawn_enabled(false)` — "Solo captures null the
  car pool, but `0x0044dd90` still draws a lane".
- `src/traffic.hpp:121-127,208` documents the contract: nulled-pool solo = "draw, never place"; disabled
  spawns "reproduce exactly that (draw, no placement, no serial bump)".
- Mechanism (`src/traffic.cpp:545/557/570`): `if(spawn_enabled_)place(...)` / `:451`
  `if(spawn_enabled_)place_crossing(...)` — the `random()%count` / `random()%range` lane draws execute
  unconditionally on the gated path; only `place`/`place_crossing` (type pick + 4 phase/jitter draws +
  serial bump + activation) are skipped. Car motion/horn never run with an empty pool either way.
- Net per gated expiry in solo replay: **exactly 1 draw** (the lane pick), 0 cars, 0 serial bump — identical
  stream position to the original's nulled pool (`0x0044df60` early return after the caller drew).
- Countdown hydration (`tests/oracle_sim_test.cpp:696-705`): demo+`brain_window` banks 1 timer tick with a
  throwing RNG lambda ("hydrated hold frame consumed RNG" — asserts the no-fire/0-draw hold property);
  legacy captures run that tick live via `race.countdown_step()`; remaining hold frames `1..green-1` run live
  `countdown_step()` (`src/race.cpp:2207-2232`) with real `random()`. Total banked hold history ≈ 59 ticks
  (behind 531 at tick 60, 639 at first motion tick 72) per countdown-driver-trace-20261016 §5-6.

## 4. Attributing the 893 +2 draws: contact, not timer

- Assigned gap (tick735-target-20261016, gate2-20261016): single-anchor sched900 first diverges frame 892
  (tick 893), `rng original 707629878 native 2897718112`; all compared state through 892 matches, RNG matched.
  The +2 is **native-only extra draws** (native stream advanced past the original).
- Timer path cannot produce a native-only +2 here: native disabled-placement draws (lane picks only, §2-3)
  are stream-identical to the original nulled pool by construction, and they matched through 892. A same-tick
  double timer fire would draw in BOTH binaries, not native-only.
- Contact path CAN produce exactly +2 native-only (`src/race.cpp:513-518,947-972`):
  `Race::collide` runs `separate` + `contact(ra,rb)` + `contact(rb,ra)`; only the `&self==&riders_[0]`
  (player-self) direction draws, 2 draws per sounding contact (`(random()&1)+x` + `random()&1`, quiet or loud
  branch; silent-below-threshold draws 0). `knock_off` (`:730-...`) performs no RNG (comment-verified), so one
  player-involved sounding contact = exactly +2. Reverse direction draws 0.
- Candidate pair (codex-sim-research-20261015, contact893-rea-20261016): node4 bike @30208 (`bike_c0=4`, w2c=0)
  vs player ~30093 — longitudinal gap 115 ≤ 128, lateral gap 10221 ≤ 15360, so native geometry passes and
  native `collide_riders` (`src/race.cpp:2235-2286`, no `+0xc0` role gates — `ScanObject` `src/race.hpp:366-371`
  carries no flags byte) collides and draws twice, while the original `0x004368f0` evaluates the `+0xc0 & 5==5`
  candidate gate first (REA `ev_2e5e2ab2...`, `0x436969-0x436973` — re-run 20261016, byte-identical to the 20251015
  artifact). Static-only; runtime `+0xc0` bytes at 892→893 are not in any golden dump (frame-0 `bike_c0` only).
- Secondary suspect to keep in the attribution set (tick735 handoff §2): `ai_can_overtake` draw sites
  (`0x00465a10` table × dt vs `rand&0xff`, `src/race.cpp:1471`). The per-tick capture below discriminates all
  three (timer lane picks vs contact sounds vs overtake rolls) by caller address.
- Caveat carried from contact893-rea-20261016: on current HEAD the 893 gap is shadowed by the tick-735
  `brain+0x194 ai_target_x` first divergence (commit `b4d3c0c` null-slice retention); 893 is reachable in
  single-anchor replay only after 735 is re-cleared (private gate binary already shows 893; do NOT re-derive
  here — no builds/runs in this task).

## 5. Next per-tick `rng_calls` capture spec (no capture run here)

Proxy already supports it (`oracle/aweman_proxy.c:150-200,267-270,2094-2096`, read-only):

```
RR_ORACLE_SOLO=1 RR_ORACLE_FRAMES=900 RR_ORACLE_RANDCOUNT=1 RR_ORACLE_RANDLOG=1 \
  RR_ORACLE_RANDTRACE=<tick> oracle/run.sh sched900-20261016b 900
```

- `RR_ORACLE_RANDCOUNT=1`: entry-patches `0x00455eb0` (expects `call 0x4592b0` prologue), counts every CRT
  `rand()` incl. the direct `0x44e0b2` type pick; dumps per-tick `rng_calls` + `rng` (MSVC seed at
  `getptd()+0x14`). NOTE: existing `sched900-20261015.tsv` was captured WITHOUT it (spare-half-trace-20261016:
  `rng_calls≡0` by construction) — must re-capture, never overwrite the accepted golden; new name + receipt.
- `RR_ORACLE_RANDTRACE=<tick>` (one tick per run): dumps up to 64 caller return addresses as `randtrace.N`
  for that tick. `RR_ORACLE_RANDLOG=1`: full-run `(tick<<32|caller)` stream as `randlog.N` (cap 32768).
- Discriminating set, ticks 890-893, both binaries (original capture + native `RR_SIM_TRACE`-style run):
  `rng`, `rng_calls`, `randtrace`/`randlog` caller sets, `rider.0.position/lateral/speed`,
  contact/collide counters if traced. Expected signatures: timer lane pick callers in
  `0x44dde4/0x44de6c/0x44df04` (+ chosen-lane taps); contact sounds via `0x004049c0` region callers;
  overtake via `0x00465a10`-region caller. A native-only +2 with contact-region callers and zero timer-region
  delta at 893 closes the attribution; any `0x44dd90`-region delta reopens the timer hypothesis instead.
- Also still missing (countdown-driver-trace §6, unchanged): `7ef4/7f08/7e94` oracle fields and the tick-60
  think-unblock trigger — propose `traffic.behind`-style proxy fields in a later capture-gated task, NOT here.

## 6. Read-only LSP

`python3 tools/agent_lsp.py --check src/traffic.hpp` against existing `build-clangd/compile_commands.json`
(181 entries, no CDB/config changes): `clangd --check` exit 0, 0 errors. (`CMakeLists.txt` flagged newer than
the CDB — pre-existing, untouched.)

## 7. NOT-changed list (explicit)

No tracked file was modified. Read but untouched: `src/traffic.hpp`, `src/traffic.cpp`, `src/race.cpp`,
`src/race.hpp`, `src/race_setup.cpp`, `oracle/aweman_proxy.c`, `CMakeLists.txt`, `tests/oracle_sim_test.cpp`,
`tests/traffic_test.cpp`, `tests/race_setup_test.cpp`, `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`,
`docs/VERIFICATION.md`, `docs/plans/10-simulation-oracle.md` (+ `10*`/`32*`), `analysis/decompiled/0044dd90.c`,
`analysis/decompiled/0044df60.c`, `analysis/decompiled/0041f040.c`, `analysis/original-disassembly.txt`,
`analysis/inventory.json`, `analysis/oracle/sched900-20261015.tsv`. No builds configured or run, no Wine/proxy
runs, no Ghidra project writes, no commits. New untracked outputs only: this handoff +
`analysis/traffic-draw-20261016b/rea-44df60-20261016b.md` (full REA dossier). Prior REA artifacts reused
read-only: `traffic-timer-trace-20261016.md` (`0x0044dd90`), `countdown-driver-trace-20261016.md` (hold banking),
`contact893-rea-20261016.md` + `analysis/codex-contact893-20261016/` (flags gate, +2 mechanism),
`tick735-target-20261016.md` (893 spec, shadowing caveat), `countdown-timers-20261015.md` (Task 7 Step 2 baseline).
