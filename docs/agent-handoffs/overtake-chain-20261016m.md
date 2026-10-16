# AI overtake chain — 20261016m (read-only Plan 10 Task 6 background)

- Task: Plan 10 Task 6 background — AI overtake chain (`0x00408e40` gate,
  `0x00408cd0` scan context, curve/straight targets `0x004090a0`/`0x004091f0`,
  `ai_overtake` flag).
- Baseline: `f41cc62` (HEAD, verified `git rev-parse` this session).
- Read-only: no tracked file edited, no builds, no captures, no Ghidra/prefix
  mutation, no commits. New files only: this handoff + untracked scratch
  `analysis/overtake-20261016m/` (`rea-449fe0.md`/`.log`, `rea-44a110.md`/`.log`,
  `NOTES.md`).
- Sibling in-flight edits avoided (read, never touched): `src/course_scene.cpp`,
  `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
  `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`, `tests/road_test.cpp`.
  All `rg`/`fd` in this session excluded those eight.

## REA spot-check (2 calls max, pinned env, UNCOVERED addrs only)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`rea-agents@6.4.0`, `--format md`.
EXE hash verified (`shasum -a 256`):
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.

Already covered — NOT re-probed: `0x00408e40` (ev_c74005c9…),
`0x00408cd0` (ev_2d02b1d4…), `0x004090a0` (ev_7fc273f1…), plus
`0x004091f0` / `0x00409000` / `0x00409f70` / `0x00449e90` / `0x00449ce0` /
`0x004092e0` (all have `analysis/oracle/rea-*.md` dossiers).

1. `function ../cdrom/ROADRASH/ROADRASH.EXE 0x00449fe0 --format md` → exit 0,
   evidence_id `ev_f4e1db0d64bc33b4f1459b9173fe941aa473f146ee83e66e1059d02913ecf6dc`
   (828 lines, saved `analysis/overtake-20261016m/rea-449fe0.md`).
   Pseudocode identical to `analysis/decompiled/00449fe0.c`: 
   `FUN_0044f5a0(dpos, (dlat + (dlat>>31 & 0xff)) >> 8)`, `& 0xff * 0x10000`,
   `> 0x7fffff → -= 0x1000000`. Assembly `0x449fe0–0x44a017`, one callee
   `0x0044f5a0` at `0x449ffd`. Native match: `src/race.cpp:1687-1688`
   (`approx_atan2` + `div_pow2`, same wrap).
2. `function ../cdrom/ROADRASH/ROADRASH.EXE 0x0044a110 --format md` → exit 0,
   evidence_id `ev_ac3a47ed1b1a875418258a09028c2d0248b393d659e6d466248157f69a240953`
   (858 lines, saved `analysis/overtake-20261016m/rea-44a110.md`).
   Pseudocode identical to `analysis/decompiled/0044a110.c`: switch on
   `+0xc4`: 1/2→`0x2000`, 4/8/`0x20`→`0x3000`, `0x10`→`0x8000`,
   default→`0x1000`. Native match: `object_size` (`src/race.cpp:1329-1336`)
   identical; overtake side (`:1672`) `(sz + sz4)/2 + 0x5000` gives `0x8000`
   for the 735 type-4 bike, confirming the 735 side correction.

REA is a spot-check, not runtime proof (per `docs/TOOLING.md`).

## Chain diagram (original addr → native carrier)

```
ai_think 0x00407bd0 (race.cpp:1693)
 ├─ ai_transition 0x00408ad0 (race.cpp:ai_transition)
 └─ state dispatch 0x00465980
     ├─ 1 → ai_state_racing 0x00409000 (race.cpp:1717)
     ├─ 2 → thunk 0x004092d0 → 0x00409000
     ├─ 4 → ai_state_follow 0x00409ca0 (race.cpp:1725; state 3 runs it first)
     ├─ 5 → ai_state_fight 0x0040a4f0 (race.cpp:1788)
     └─ 6 → ai_state_shadow 0x0040a560 (race.cpp:1775)
           │ each arm calls:
           ▼
     ai_overtake 0x00408e40 gate (race.cpp:1654 → bool; brain +0x180 latch)
      ├─ ai_scan_overtake 0x00449e90 (race.cpp:1616; args 0x3f/0/0x96/0x960/0xc000
      │    via scan_next 0x00449ce0; replan-side analogue 0x00408cd0 → +0x16c/+0x170)
      ├─ latch: overtake_latched/type/position (brain +0x180; -1/0 = none);
      │    replan target itself (+0x168 bike) counts as none → return false
      ├─ pass side: object_size ★0x0044a110 (NEW ev_ac3a47ed) + 0x5000,
      │    slice-lane flip; +0x198 = obj.pos + 0x3c0, +0x194 = obj.lat + side
      ├─ far: gap > 0x24 && |dpos| > 799 → ai_curve_limit 0x00409020
      │    (gap < 0x4b ? lim - lim/4 : lim)
      └─ near: heading ★0x00449fe0 (NEW ev_f4e1db0d; atan &0xff<<16, wrap)
           |angle| < 0xf → lim/2
           return true → caller skips own target choice
           return false → fall through:
             ai_curve_target 0x004090a0 / ai_straight_target 0x004091f0
             (target_x +0x194, target_position +0x198, target_speed +0x19c)
      ──► ai_speed_steer 0x00409f70 (race.cpp:1594; steer/throttle/brake closer)
```

`ai_overtake` flag: native `AiState::overtake_latched/type/position`
(`src/race.hpp:159-161`, brain `+0x180`); cleared to `-1`/`0` on every gate
entry (`race.cpp:1659-1661`), set only on a non-target latch (`:1667-1669`).
Original returns AL (`0` = none via `0x00408fe7`); native returns `bool`.
No RNG draws in the gate or its two newly-checked callees (prior REA already
showed none in `0x00408e40`; the draws live downstream at `0x004092e0` and the
contact path).

## 735-tie context (unchanged, carried forward)

- Stale-`build/` binary latched the 27648 type-4 bike (lat `-9984`) with side
  `0x8000` → `tx = -9984 + 32768 = 22784`, `tpos = 27648 + 0x3c0 = 28608`.
  Original and fixed native skip the NULL-slice pair via `0x00449ce0`,
  `ai_scan_overtake` returns `-1`, gate returns false, straight target gives
  `tpos = 25255 + 0x3c0 = 26215`, `tx = 5632` (tick735-target-20261016.md).
- This session's size-table REA independently confirms the `0x8000` side
  (`(0x3000+0x3000)/2 + 0x5000`), closing the old `0x3900` decomposition.
- Equal-position order (27648 bike precedes its half, first-candidate-wins) is
  one observation consistent with creation order, NOT a proven general
  bike-before-half rule (ai-replan-20261016c §3). The two callees REA'd here
  are order-independent (heading/size of the already-picked node) and add no
  tie-break evidence either way.

## Discriminating capture spec (proposed, NOT run — no builds/captures per task)

Goal: runtime-proof the full gate on the fixed binary, and separately close
the tie rule, reusing the `sched900-20261015.tsv --driver=demo` fixture:

1. Per tick 734–736 for rider 0: brain `+0x180` (latched), `+0x194/+0x198/+0x19c`
   (targets), `gap`, think-entry position, plus the picked node's
   `(position, type, lateral)`.
2. Scan-order log at the same ticks: native `scan_[]` iteration order around
   27648 with `(position, type, lateral, rider/half identity)` and latch-vs-skip
   reason (slice present/NULL). Decidable native questions: (a) 27648 bike
   precedes its half? (b) first equal-position candidate wins? (c) any
   half-first equal-position pair (refutes hard bike-before-half)?
3. Gate-arithmetic checks: latched type-4 → side must be `0x8000`; far branch
   (`gap > 0x24`, `|dpos| > 799`) → `lim` vs `lim-lim/4` at `gap < 0x4b`;
   near branch → heading angle and `lim/2` only when `|angle| < 0xf`.
4. Success: `latched=-1 → 26215/5632` with NULL-slice skips at
   27648/30208/31488, and any latched case reproduces side/speed per above.
   A half-first pair, or a bike-first order that still latches the half,
   reopens `build_scan_list` ordering — not the NULL-slice guards, not the
   heading/size callees confirmed here.

## Explicit NOT-changed list

No tracked file edited. Read but untouched: `src/race.cpp`, `src/race.hpp`,
`src/bike.cpp`, `src/bike.hpp`, `analysis/decompiled/00449fe0.c`,
`analysis/decompiled/0044a110.c`, `docs/TOOLING.md`,
`docs/REVERSE_ENGINEERING.md`, prior handoffs (`overtake-chain-rea-20261012`,
`ai-replan-20261016c`, `ai-fight-20261016e`, `tick735-target-20261016`,
`replan-20261016k`). Sibling in-flight eight excluded from all searches and
untouched (see header). Live-claimed paths avoided (no edits, no builds):
`docs/PROGRESS.md`, `docs/VERIFICATION.md`, `docs/plans/10-simulation-oracle.md`,
`src/traffic.*`, `src/terrain.*`, `src/race_setup.*`, `oracle/aweman_proxy.c`,
`CMakeLists.txt`, all `tests/`. No fixture/golden/allowlist touched; no Wine
capture run; no Ghidra project or prefix mutation.

## Remaining work (for owner/integrator, not this session)

- Run the 734–736 brain + scan-order + gate-arithmetic capture above on a fresh
  fixed binary (retires 735 hygiene + proves the tie rule).
- Per-tick `rng_calls` + draw-site attribution at the real divergence (893 RNG;
  prime suspect contact path vs `0x004092e0`) — belongs to the contact-path
  investigation, not this handoff.
- Do not change scan ordering, `+0x36` road-half handling, or `approx_atan2`
  tables on this evidence; both callees match at trace level.
