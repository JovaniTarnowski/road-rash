# Scheduler walk + track-ring verify — Oct 16, 2026q (read-only)

Read-only background at HEAD `940f4d7`. No tracked files edited; no builds,
captures, or commits. New files: this handoff +
`analysis/schedwalk-20261016q/` (notes + 2 REA mds + 2 empty logs).

## Fresh REA (pinned toolchain, 2/2 call budget spent)

`PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=.../openjdk@21/...`, EXE
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(matches `analysis/inventory.json`), `--format md`, both exit 0:

- 0x004552e0 (ring fill) →
  `ev_fe955139caab0a759e0a4c6d18aace8a80773cc4ecbd7786a1b36114c4ade21b`
- 0x00449c80 (scheduler nearer-of) →
  `ev_37ffeb105fcdb677b9a478bd72dee8000cbb491b42a467249f2da0857634bbdd`

Whitespace-normalized pseudocode (`## raw_result.value.pseudocode`) vs
`analysis/decompiled/`: `004552e0` 2131==2131 MATCH; `00449c80`
406==406 MATCH. REA is a spot-check per `docs/TOOLING.md`, not runtime proof.
Both addrs were UNCOVERED by prior REA (schedwalk-rea-20261012 covered
`00449ce0/00449c20/00449e60`; trackring-verify-20261012 covered
`00454c80/00453030`; terrain-interleave-20261016b covered
`00454240/00451380`). Still UNCOVERED (committed-text only):
`00449c30`, `00449e90`, `00455260`, `004551c0`, `004515a0`, `00451cb0`,
`004520d0`.

## Walk table (fact unless marked)

| # | Addr | Role | Export shape | Native counterpart |
|---|---|---|---|---|
| S1 | 0x00449ce0 | scheduler directional walk (mask + road-half filters) | prior REA-identical (schedwalk-rea-20261012) | `Race::scan_next` (`src/race.hpp:374`, `src/race.cpp:1304-1305`) |
| S2 | 0x00449c20 | absolute-distance helper (logical-shift quirk) | prior REA-identical | comment only (`src/crash.cpp:67`) |
| S3 | 0x00449e60 | windowed scan (lane / 0x100-page) | prior REA-identical | comment only (`src/traffic.cpp:113`) |
| S4 | 0x00449c80 | nearer-of backward/forward `00449ce0`, tie → backward via `00449c20<=` | **this session REA-identical** (`00449c80.c:10-22`); no RNG | NO native wrapper (porter gap; `scan_next` covers only S1) |
| S5 | 0x00449c30 | comparator (`>>4` / `>>0xc` by param_3) | committed text only, NOT re-probed | none found (open) |
| S6 | 0x00449e90 | overlap search over `00449e60` with `0044a110` widths | committed text only, NOT re-probed | none found (open) |
| R1 | 0x004552e0 | ring fill | **this session REA-identical**; phases: init `:13-29` (jitter/sort `00453e60`, walker roots `00452f50`, walk-to-camera `00453ae0` ×2), table `:30-36` (128 slots), links `:37-60`, reset loop `:61-69`, build loop `:70-76`, tail `:77-91` | `TrackRing::setup` (`src/track_ring.cpp:175-196`) models init + 128× build/step |
| R2 | 0x00455260 | per-slice reset (`004551c0` ×2 on `+0x30/+0x194`) | committed text only (`00455260.c:7-21`), called by R1 `:64` | no direct counterpart (folded into setup) |
| R3 | 0x00454c80 | build_slice: slot select `(pos&0x7f00)>>6`, rewiring, 1× `00454240` always + 2nd iff slice byte `[7]>1` (`:268-273`), tails `004547e0/00454890` | prior REA-identical (trackring-verify-20261012) | `TrackRing::build_slice` (`src/track_ring.cpp:153-172`, `track_ring.hpp:88`); 2nd-call arm NOT modelled (known, cf. interleave suspect 3) |
| R4 | 0x00453030 | walker step (delta 0x100; section-change via `00452e10/00452cb0`) | prior REA-identical | `TrackRing::step` (`src/track_ring.cpp:124-151`) forward single-road only |
| R5 | 0x00454120 | ring advance (0x100 slice rebuild via `00453030/00450f90/00454c80`) | prior REA-identical (ringplumb-rea-20261012) | `TrackRing::advance` (`src/track_ring.cpp:198-214`) |
| R6 | 0x00454240 | slice body (2× `00451cb0`, `004520d0`, billboard loop) | fresh REA by terrain-interleave-20261016b | `build_slice` body, see suspect note below |

Step-bound note: both R1 loops are `iVar7=0; iVar7+=0x100 while (iVar7<0x8000)`
→ 128 iters at `base+0x0..base+0x7f00`. The task string "to 0x8200" matches
no constant in `004552e0.c`; bound is `<0x8000`, and native
`track_ring.cpp:191` (`k<0x8000, k+=0x100`) agrees. The R1 build loop calls
`00454c80(base+k)` then `00453030(front,0x100)` per iteration.

## Terrain-interleave relevance note

This session confirms only scaffolding around the open interleave suspects
in `terrain-interleave-20261016b.md`, and settles none of them:

- Suspect 1 (per-step vs per-transition `00451380` in `004515a0`) lives
  inside the walker-step path (R4); R1/R4 order confirmed but per-step
  invocation frequency NOT re-probed (budget; `004515a0` still committed-text).
- Suspect 2 (Borr `random`-gate in `00451cb0`) and suspect 3 (conditional 2nd
  `00454240` in R3 `:270-273`) live inside the slice-build path; native
  `build_slice` still models exactly one `00454240`-body and a `random`-gated
  `slots` early-out (`src/track_ring.cpp:104-110` vs `00451cb0.c` loop).
  Unchanged.
- Suspect 4 (billboard-tail in `004520d0`) unchanged: native 1-jitter-draw +
  mark per qualifying billboard (`:163-172`) still matches the fresh-`00454240`
  inventory; RNG-freedom of `00438930/00451c20/0041ec70` still unverified.
- S4 (`00449c80`) is scheduler-side (AI/combat target pick) and draws no RNG,
  so it cannot shift the CRT stream behind the NAPA LIHR record-7 divergence;
  relevant only as the verified tie-break rule for future AI ports.
- The decisive capture spec in terrain-interleave-20261016b (per-slice draw
  counters, 2nd-call flag, per-step `00451380` invoked/draws) still stands;
  nothing here substitutes for it.

## NOT changed

No edits to any tracked file. In particular untouched (sibling live work):

- `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp` (the eight in-flight
  files; also excluded from this session's `rg` reads)
- `docs/PROGRESS.md`, `REVERSE_ENGINEERING.md`, `VERIFICATION.md`,
  `plans/10`, `plans/32`, `src/track_ring.*`, `src/terrain.*`,
  `src/race.*`, `src/traffic.*`, `oracle/*`, `CMakeLists.txt`,
  `tests/oracle_sim_test.cpp`, `analysis/decompiled/*`, Ghidra project.
- No builds, no captures, no prefix/DLL writes, no commits. `rg`/`read`
  only; REA ephemeral-source-immutable. Remaining work: REA on
  `004515a0`/`00454c80` to promote interleave suspects 1-3, REA or
  disassembly on S5/S6/R2 for the scheduler/ring backlog, then the
  capture above (capture owner owns oracle + goldens).

## Live resources

No live processes, no build locks, no captures held. REA evidence local:
`analysis/schedwalk-20261016q/rea-004552e0.md`,
`analysis/schedwalk-20261016q/rea-00449c80.md` (+ empty `.log`s, `notes.md`).
