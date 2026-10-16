# Terrain ring-fill interleave narrowing — Oct 16, 2026b (Plan 10 Task 5)

Read-only research at HEAD `5e4cd98`. No tracked files edited; no builds,
captures, or commits. New files: this handoff + scratch
`analysis/terrain-interleave-20261016b/` (notes + 2 REA logs).

Ground truth (re-cited from `human-terrain-20261016.md`, not re-derived):
NAPA LIHR record 7 = [126, 150]; point.0 min_length 1, max_length 3.
Original end 132 → length 2 → draw%3 = 1; native end 133 → length 3 →
draw%3 = 2. Record end (150) far → clamp hypothesis dead: pure CRT
draw-stream shift at the same sample with the same start.

## Fresh REA (pinned toolchain, 2/2 call budget spent)

`PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=.../openjdk@21/...`, EXE
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`:

- 0x00454240 → `ev_b2b5f25809cbd534eaff3d68293a6663db752ba2f4e628c138b09a5f4948e491`
- 0x00451380 → `ev_685799344e651d31b1dd7bbdee8e6bbc0e37f84eb681ec8ea145fbec88af2607`

0x00454c80 / 0x004515a0 NOT re-probed (budget); cited from committed
`analysis/decompiled/*.c` text and marked as such below.

## Ranked suspects (with exact draw deltas)

1. **Per-step vs per-transition `hill_slopes` invocation — NEW top suspect.**
   Committed `004515a0.c:40-45`: while `hill_record < count-1`, `FUN_00451380`
   is called on EVERY forward step, not only on record transitions (44 = maybe
   advance, 45 = unconditional call). Native `TerrainIterator::step`
   (`src/terrain.cpp:121-138`) calls `hill_slopes` only when `advance`
   (record index changed). Delta: up to 8 points × 3 draws per non-transition
   step in any multi-record section (≈129 setup steps, records 0→7 of 46).
   Single-record sections are no-ops on both sides (matches). Why seeded
   fixtures (960 states, ±256 steps, synthetic 2-record) still pass: long
   synthetic lengths likely never expire, making per-step calls draw-neutral
   there; NAPA lengths 1-3 expire almost every 0x100 step. Seeded coverage is
   blind to this by construction. Needs one fresh-REA confirmation of
   `004515a0.c:40-45` (stale-export risk).
2. **Borr `random`-gate mismatch — ±(count+1) draws per `00451cb0` call.**
   Committed `00451cb0.c`: loop runs `byte(0x1d)+1` iterations with 1 RNG draw
   each IFF `(char)param_1[5] != 0`, else deterministic 0-draw stepping; there
   is NO early-out on byte +0x15. Native `Borr::slots`
   (`src/track_ring.cpp:104-110`) returns with 0 draws when word-bit-11
   `random` is clear. Whenever bit11==0 but byte(+5)!=0: native 0 vs original
   (count+1), ×2 `00451cb0` calls per slice. Period/selector gates match.
3. **Missing conditional 2nd `00454240` per slice — whole-block delta.**
   Committed `00454c80.c:268-272`: always 1× `00454240`, plus a 2nd iff slice
   byte `(char)piVar2[7] > 1`. Ring fill `004552e0.c:70-76` = 128 ×
   { `00454c80`, step }. Native `setup` (`src/track_ring.cpp:191-194`) models
   exactly one `00454240`-body per slice, never the second. Cost on qualifying
   slices: a full extra { 2×00451cb0 + 004520d0 + billboard loop } draw block.
4. **Billboard-tail residual — likely 0, unverified.**
   Fresh `00454240` body contains ZERO direct `FUN_00441750` calls; the
   1-draw-per-qualifying-0x80000000-billboard + `|0x3f00` mark lives in
   `004520d0.c:54-69`, matching native's 1 jitter draw + mark per billboard
   (`src/track_ring.cpp:163-172`). Closes only if `00438930`/`00451c20`/
   `0041ec70` are RNG-free (not yet shown).
5. **DOWNGRADED (confirmed matches): `hill_slopes` skip edge + per-point
   draws + record-transition edges.** Fresh `00451380` REA + assembly:
   skip iff `end > sample && start < sample` (both strict; `JLE`→redraw,
   `JL`→skip), exactly 3 draws per uncovered point, zero-length early return —
   all identical to `src/terrain.cpp:80-105`. Transition edge
   `next.start <= sample` matches (`004515a0.c:41` vs `terrain.cpp:127`;
   committed text only).

## Decisive capture spec (for the capture owner)

Extend `RR_ORACLE_TERRAIN_TRACE=1` on the established human course-4 recipe
(same controls/seed/SOLO/HUMAN/frames); do not overwrite accepted goldens.
Dump every frame 0 → ≥ tick 189:

- Keep: `ring.walk.{front,back}.{position,terrain.position,last_sample,record,
  point.i.start/end/x/height/dx/dh}`.
- NEW per ring-fill slice k (0..127): walker **section id** (active LIHR
  payload); active section's **record-bounds table**
  (`ring.terrain.record.<k>.start/end` for all k); CRT **draw counter pre/post
  slice**, and pre/post each phase (00451cb0-a/b, 004520d0) if breakpointable;
  `00454c80` **second-call-taken flag**; per-step `00451380`
  **invoked flag + draws consumed** (settles suspect 1 in one run).
- Header: `ring.pre_draws`/`ring.post_draws` + `seed` (absent today; current
  `rng_calls` reads 0 on sampled frames).
- Native side (implementer): report-all-terrain-diffs mode for the setup frame
  (first-diff stop hides whether native x/height/dx/dh also differ).

First discriminator: original per-slice draw totals vs native replay —
suspect 1 predicts divergence on the first non-transition step inside a
multi-record section with expired points; suspects 2/3 predict slice-granular
deltas keyed to roadside-profile bits / slice byte > 1.

## LSP (read-only)

`python3 tools/agent_lsp.py --check src/track_ring.cpp`: 181 entries, no
missing sources, `healthy: false` (CMakeLists.txt newer than DB); Apple clangd
`--check` exit 3 on the known preexisting `ExtractFunction` break/continue
tweak failures; no parse diagnostic for the file.

## NOT changed

No edits to any tracked file, claimed or otherwise: `docs/PROGRESS.md`,
`REVERSE_ENGINEERING.md`, `VERIFICATION.md`, `plans/10`, `plans/32`,
`src/race.cpp`, `src/traffic.*`, `src/terrain.*`, `src/race_setup.cpp`,
`oracle/aweman_proxy.c`, `CMakeLists.txt`, `tests/oracle_sim_test.cpp`,
`tests/traffic_test.cpp`, `tests/race_setup_test.cpp`, `src/track_ring.cpp`.
No builds, captures, Ghidra project writes (REA ephemeral-source-immutable),
no commits. Only this handoff + `analysis/terrain-interleave-20261016b/`
(notes + 2 REA logs) are new. Remaining work: fresh REA on 0x004515a0 +
0x00454c80 to promote suspects 1-3 from committed-text to confirmed, then the
capture above.
