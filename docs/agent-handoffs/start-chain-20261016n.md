# start-chain-20261016n — race start chain read-only background

Task: read-only background on the race start chain (countdown, green-light
anchor, start-hold 59 frozen frames, 0xb4 quit window, seed hook) at HEAD
`d82c1b1`. No source edits, no builds, no captures, no commits.

Baseline: `d82c1b1` (shared tree; 8 sibling in-flight files present, untouched).
Owned/changed files: only this handoff + scratch `analysis/start-20261016n/`
(2 REA `.md` + 2 `.log`). No tracked file edited.

## 1. Chain diagram (addresses → native mapping)

```
setup (once)                frame layer A (menu/wall-clock)      in-sim layer B (tick)
─────────────────           ─────────────────────────────      ──────────────────────
0x00442dd0 init:             0x00442e10(param_1) per frame:       0x0044b2e0(n) batch stepper:
 753b4=0, 753c4=0,            753c4 countdown → 753b4=1 on          tick++ (7ecc/c3990),
 753c0 = 0x168                 expiry; msg-pump QUIT(0x12) →        0x0044b3b0() at 0x0044b324
          (−0xB4 if solo)       753b4; controls poll 44f800;         (= oracle hook_step site),
 = 180 solo / 360 netplay      if 753b4==0:                         then periodic 44b5d0 /
                             ├─ 753c0>0: 753c0−=p1, NO sim          4163b0/43cdb0/44f040,
                             │   (zero-cross: GO beep, =0)          tail 438450
0x00405600 rider build ─┐    └─ else: 0x0044b2e0(p1) ──────────────┐
 (RNG stream: 50×       │                                          │
 personality + lane     │     netplay branch: handshake≠1          ▼
 %3/AI + %100 tail;     │     before tick 299 → 753b4=1        0x0044b3b0 per step:
 11,461-call pre-green  │                                      timers, rider pass 44b0e0,
 stream)                │                                      scheduler 438170(0),
0x0044db80 density 6 ───┤                                      camera 437c30 (stamp-gated),
0x004552e0 ring fill ───┘                                      ring advance 454120,
 (cursors 3200/3100,                                       traffic 44dd90(moved delta)
 0x8000 back-to-front)
                        HOLD (ticks 1–59, zero-delta): 44dd90 banks +9 behind/rear,
                        −9 oncoming per tick, no cursor/RNG motion → behind 531.
                        Tick 60 = think/speed launch ("green" in tick space);
                        ticks 60–71 still zero-delta (→639); first motion tick 72.
```

Native mapping (files read, not touched): Game-level wall countdown
`countdown_ticks = 3*60` (`src/lifecycle.hpp:20`), set in
`src/game_types.cpp:46`, decremented at `:128` ("race clock starts after the
countdown") — this is layer A. In-sim frozen hold is
`Race::countdown_step()` (`src/race.cpp:2207–2233`): dt 2-then-1
(`0x00438930` `+0x18` trace), frozen-schedule stepping, AI think for AI/demo
rows, and `traffic_->countdown_step` (+9/tick banking). Seed/ring factory is
`src/race_setup.cpp:141–204` (prepare hook → RNG state → `TrackRing` =
`0x004552e0` slot); anchor rule lives in `tests/oracle_sim_test.cpp:479–542`.

Prior handoffs consumed (no re-verification): `startchain-rea-20261012`
(44db80/44dd00/4552e0, no RNG in start setup),
`countdown-driver-trace-20261016` (hold timer math, 59×9=531, 1–71 window),
`countdown-verify-20261012` (dt 2-at-tick-1 closed), `countdown-timers-20261015`
(Task 7 port), `start-grid-rea-20261012` (draw order, unrelated slice).

## 2. REA spot-checks (2 calls, both UNCOVERED before, both MATCH)

Pinned env per `docs/TOOLING.md`: `rea-agents@6.4.0`, Node v24.13.0 (nvm),
Ghidra 12.1.4 libexec, JDK 21. EXE sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (matches
`analysis/inventory.json`; digest confirmed inside both REA outputs).

| Addr | Role | evidence_id | Result |
| --- | --- | --- | --- |
| `0x00442e10` | frame driver: 753c4→753b4 quit flag, 753c0 countdown, `44b2e0` gate | `ev_d95c71abccb0de53585996e03fdbef338490a5cbd8c6a1f5d6ceec042ec4acaa` | pseudocode 1057 chars, MATCH vs `analysis/decompiled/00442e10.c` |
| `0x0044b2e0` | batch stepper: tick++, `44b3b0` at `44b324`, periodic/tail calls | `ev_ee9250409c01e6a4ba37ac3b775b26d969e7f2ce0ae5d891e21f6c974d66d77e` | pseudocode 746 chars, MATCH vs `analysis/decompiled/0044b2e0.c` |

Scratch: `analysis/start-20261016n/rea-442e10-framedriver.md/.log`,
`rea-44b2e0-stepper.md/.log`. REA is a spot-check, not runtime proof.

## 3. Anchor rule (single green-light anchor, from `tests/oracle_sim_test.cpp`)

- Green = first frame where `rider.0.position`/`speed` moves vs frame 0.
- Anchor = **pre-step state of the green tick** = preceding frame's post-step
  `rng`, valid only if every hold frame shows `rng_calls==0` and constant
  `rng` (else "green pre-step anchor unproven", harness exits 1).
- Ring closure: `ring_end_state` + `ring.post_draws` (11) draws must equal the
  anchor ("ring setup closes on the green anchor").
- `set_rng_state(anchor)` once, then free-run; `--reanchor` (per-tick
  re-anchor) is diagnostic only. Setup stream (`srand(seed)` +
  `ring.pre_draws` 9098 → `0x00405600` → +11 post) is NOT reproduced (known
  limitation until setup consumers are ported).

## 4. 0xb4 quit window (status: documented, NOT re-proven here)

- `docs/REVERSE_ENGINEERING.md:565`: after outcome is set (`0x00416010`
  first-write-wins: `0x7f`→place-gated 2/3, 4 busted, 5 wrecked) the race keeps
  running for **0xb4 ticks** (`0x00442e10`); in-race Escape confirm (string
  316) sets outcome 5 = wrecked (`0x00423408`); `0x00416120` maps to screens.
- Numeric note (observation, not claim): solo countdown init is also
  `0x168−0xB4 = 180 = 0xB4` (`00442dd0.c:8`). Whether the two 0xB4 share a
  constant is unpinned.
- Native gap (from this read): no 0xB4 post-outcome run-on found in
  `src/race.cpp`/`src/game_main.cpp` (`Race::step` early-returns when all
  finished; `run_race` ends at `finished()`); `0x00423408` was NOT REA'd
  (outside the 2-call budget — suggested next spot-check).

## 5. NOT-changed / NOT-read list

- Sibling in-flight files — neither read nor touched (per task): 
...[truncated 911 chars]