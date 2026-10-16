# Bench release-20261016f — Plan 32 slice E whole-frame Release matrix (READ-ONLY)

Task: Plan 32 slice E measurement follow-up at HEAD `0141e7f`, read-only.
Rerun the whole-frame matrix from `docs/agent-handoffs/bench-20261016d.md`
(Debug, baseline `8851ef6`) in Release. No tracked file edited, no
CMakeLists change, no target registered, no commits. All builds/runs used
the owned dir `build-agent-benchrel-20261016f` only. No Wine, no Android,
no oracle.

Baseline: `0141e7f6b5c9fbb1b74b120e7b940cd9ee28faf0` (shared tree
`/Users/archit/Dev/rr/native-port`; `git status` shows no tracked-file
modifications by this session, only pre-existing untracked analysis /
fuzz-seed / scratch-handoff noise plus the owned build dir).

NOTE — HEAD moved since the Debug matrix: `8851ef6..0141e7f` contains two
commits: `1b79351 hud: resolution pen width, needle order, solo target gap
normalization` and `0141e7f course: original start/finish bounds lookup
with 25-race coverage`. The HUD commit changes dashboard pixels, which is
load-bearing for the hash verdict below.

## Owned build (exact commands)

Hardware: Apple M3 Pro, arm64, 19327352832 B RAM, macOS 27.0.1,
12 CPUs (`hw.ncpu`). Compiler: AppleClang 21.0.0
(clang-2100.3.34.2, `Target: arm64-apple-darwin27.0.0`) via `/usr/bin/c++`.

```sh
cmake -S . -B build-agent-benchrel-20261016f -DCMAKE_BUILD_TYPE=Release \
  -DRR_BENCH_RENDER=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build-agent-benchrel-20261016f \
  --target race_cache-test rr-bench-render hires_render-test --parallel 8
```

Runs (all from `native-port/`, disc present at `../cdrom/ROADRASH`):

```sh
./build-agent-benchrel-20261016f/race_cache-test --benchmark   # 3 runs
/usr/bin/time -l ./build-agent-benchrel-20261016f/race_cache-test --benchmark
for s in 1 2 4; do for t in 1 4; do \
  ./build-agent-benchrel-20261016f/rr-bench-render --scale $s --threads $t --frames 20; \
done; done
./build-agent-benchrel-20261016f/hires_render-test            # exit 0, 0 failures
```

(`-DRR_BENCH_RENDER=ON` is the pre-existing CMake option gating the
`rr-bench-render` target; no CMakeLists edit. `dash_bench-test` was not
built — its source `tests/dash_bench.cpp` is untracked scratch and the
task scope lists only the three targets above.)

## Results — whole-frame `race_cache-test --benchmark` (Release)

`cold_ms` = first draw on a new canvas (not a controlled cold disk/cache
measurement). p50/p95/p99 over 20 warm frames. Allocations counted per
warm draw via `CountAllocs`.

IMPORTANT: every run aborts after printing row=5 — the in-harness
`rgb_baseline` assertion (`race_cache_test.cpp:64`) throws
`RGB canvas output changed from pre-optimization baseline` because the
row=5 RGB hash no longer matches the hardcoded post-fix value. Rows 6–11
(2x 16:9, 4x 4:3, 4x 16:9) therefore have NO Release numbers at this HEAD
from this harness. This is measurement-only per constraints, so the
assert was left untouched.

Run 1 (exit 1, abort at row 5):

```text
row=0 scale=1 wide=0 rgb=0 hash=16908656509277595288 allocations=378 cold_ms=2.18983 p50=1.22467 p95=1.27283 p99=1.29054
row=1 scale=1 wide=0 rgb=1 hash=2800544501961077726 allocations=378 cold_ms=1.62225 p50=1.17471 p95=1.23375 p99=1.24113
row=2 scale=1 wide=1 rgb=0 hash=10442994832978502141 allocations=378 cold_ms=2.08833 p50=2.26079 p95=2.39358 p99=2.397
row=3 scale=1 wide=1 rgb=1 hash=12480883420206857557 allocations=1530 cold_ms=2.17508 p50=2.42671 p95=2.56758 p99=2.59187
row=4 scale=2 wide=0 rgb=0 hash=6403728731288650074 allocations=378 cold_ms=7.80312 p50=7.06758 p95=7.343 p99=7.39854
row=5 scale=2 wide=0 rgb=1 hash=11345352425216510781 allocations=1674 cold_ms=7.92454 p50=7.42617 p95=7.52829 p99=7.69029
RGB canvas output changed from pre-optimization baseline
```

Run 2 (stability): identical hashes all 6 printed rows; p50
1.32475/1.24592/2.51154/2.51196/7.18829/8.09004 (within ~9% of run 1).

Run 3 (under `/usr/bin/time -l`, exit 1, same abort): p50
1.34908/1.17792/2.42692/2.54542/7.65017/18.9213 — row=5 p50/p95
(18.92/51.06) is a system outlier (cf. 7.43/7.53 run 1, 8.09/8.85 run 2),
not harness behaviour; row=4 p99 26.8 likewise vs 7.4–7.7 otherwise.

Peak RSS (run 3, whole-process regression+benchmark binary, aborted at
row 5 so a lower bound vs a full 12-row run): maximum resident set size
141672448 B; peak memory footprint 134480472 B. Not per-case. Prior
Release figures (297 MB baseline / 381 MB current, different binary
contents) and the Debug figure (183631872 B RSS) are different-binary /
different-coverage numbers, not comparable deltas.

## Results — synthetic `rr-bench-render` (Release, mean of 20 frames)

Checksums bit-identical 1 vs 4 threads at every scale
(`rgb_upscaled` == `rgb` checksums — harness renders the same path).

```text
s=1 t=1: indexed  1.454  rgb  4.102  rgb_fog 24.642  rgb_upscaled 12.659
s=1 t=4: indexed  0.933  rgb  2.880  rgb_fog  8.376  rgb_upscaled  2.393
s=2 t=1: indexed  7.082  rgb 10.736  rgb_fog 21.170  rgb_upscaled 10.779
s=2 t=4: indexed  6.041  rgb  9.536  rgb_fog 11.299  rgb_upscaled  7.527
s=4 t=1: indexed 14.308  rgb 23.703  rgb_fog 23.854  rgb_upscaled 20.841
s=4 t=4: indexed 13.863  rgb 20.612  rgb_fog 23.727  rgb_upscaled 19.404
```

Threaded speedup (4 vs 1) in Release: s=1 indexed 1.6x / rgb 1.4x /
fog 2.9x; s=2 indexed 1.2x / rgb 1.1x / fog 1.9x; s=4 ~1.0–1.15x
(memory-bound, no gain). Same shape as Debug
(2.1x/2.9x/3.3x @ s=1 → ~1.0x @ s=4). Debug÷Release ratio ~6–10x
(e.g. s=1 rgb 55.7→4.1, s=4 rgb 159.7→23.7), expected for -O2 vs
unoptimized Debug; not a regression signal.

## Results — `hires_render-test` (Release): exit 0, `0 failures`.

Correctness gate (s=1 exact, s=2..4 centre-match, s=4 sweep, thread
identity) passes in Release at this HEAD. (It had exceeded the 120 s
observation budget in Debug with no verdict.)

## Comparison to Debug medians and prior Release baselines

- Output identity — MIXED (this is the headline finding):
  - Rows 0–3 hashes EXACTLY match the Debug matrix AND the hardcoded
    post-fix baselines (RGB `2800544…`/`1248088…`; indexed rows also
    identical). No output change at 1x.
  - Row 4 (2x 4:3 Original-scene with HUD) hash CHANGED:
    `2448064240350656025` (Debug @8851ef6) → `6403728731288650074`
    (Release @0141e7f).
  - Row 5 (2x 4:3 RGB) hash CHANGED: `8749086642151112002` →
    `11345352425216510781`, tripping the `rgb_baseline` assert.
  - The no-HUD `original_scene` check (`race_cache_test.cpp:88`) PASSED
    at rows 0–4 before the abort, so the scene/terrain/actor path is
    pixel-identical; the change is HUD/dashboard pixels only —
    consistent with `1b79351` (pen width, needle order, solo-target gap).
  - No-output-change verdict by hash equality: FAILS at this HEAD for
    HUD-composited frames ≥2x (rows 4–5); HOLDS for rows 0–3 and for the
    HUD-less scene path. The hardcoded `rgb_baseline[]`/
    `original_scene[]` goldens predate `1b79351` — whether to re-bless
    is an integrator/OMP decision, explicitly NOT done here.
- Allocs/frame identical to post-fix Debug values on all measured rows:
  378 indexed + RGB-4:3 rows, 1530 (1x 16:9 RGB), 1674 (2x RGB). The −1
  retained-horizon fix is intact; horizon-only path still zero-alloc
  (asserted in-harness before the abort point each row).
- Timing vs prior Release after-fix medians
  (`codex-performance-20261015`, RGB p50: 1x 4:3 1.149, 1x 16:9 2.362,
  2x 4:3 7.747, 2x 16:9 8.569, 4x 4:3 21.555, 4x 16:9 24.621 ms):
  row 1 (1x 4:3 RGB) 1.17–1.25 vs 1.149 (+2–9%); row 3 (1x 16:9 RGB)
  2.43–2.55 vs 2.362 (+3–8%); row 5 (2x 4:3 RGB) 7.43/8.09 vs 7.747
  (within run noise). No timing regression signal at 1x–2x within the
  measured rows; rows 6–11 have no Release data at this HEAD.
- Timing Release-vs-Debug (this HEAD Release p50 ÷ Debug-matrix p50):
  ~8x at 1x (1.22/10.78, 1.17/10.75), ~6.5x at 2x (7.07/46.2,
  7.43/50.9) — same ratio shape the Debug handoff reported in reverse,
  confirming the Debug numbers were pure build-type effect.

## What remains (not done, per constraints)

- Rows 6–11 Release timings/hashes are unobtainable from
  `race_cache-test --benchmark` at this HEAD without touching the
  `rgb_baseline` assert (tracked file — left alone). Options for the
  integrator: re-bless goldens post-`1b79351`, or measure rows 6–11 via
  a scratch harness.
- Per-case memory, controlled cold-cache metrics, further alloc
  elimination (378–1674 `new`/frame outside horizon), Android
  thermal/device budgets (plan F).
- No Wine, no Android, no oracle, no commits. Held resources: none (no
  processes left running; owned build dir only).
