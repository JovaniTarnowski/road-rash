# codex-performance-20261015

Task: plan 32 E (full-frame performance), plans 19–21/29 follow-through.
Baseline: `fdcd4aae2e399f38d0d089ac7fb91fefc041cad4`.
Shared checkout: `/Users/archit/Dev/rr/native-port`.
Owned build: `build-agent-performance-20261015` (Release, AppleClang 21, C++20).
Owned source: `src/race_caches.hpp`, `tests/race_cache_test.cpp`.
Parent integrator owns CMake registration/status/commit/push. No commits made here.

## Change and rationale

- RGB horizon now uses the existing retained indexed framebuffer. Conversion still
  covers every pixel, including zero-filled lower rows; resize remains at a frame
  boundary. Removed the redundant RGB clear immediately before full conversion.
- Indexed HUD/panels previously ran before `CourseScene::draw_horizon`, whose
  `fb.clear(0)` erased them. Compose them after horizon and scene rendering so
  dashboard pixels remain visible. Full-width panel arithmetic was already fixed
  in baseline; the remaining defect was call ordering. RGB no longer executes the
  unused indexed HUD conversion/LUT pass.
- Added a headless SDL software-renderer corpus regression. It compares all six
  pre-change RGB frame hashes exactly (4:3/16:9 × 1/2/4 scale), one/four render
  threads, every opaque HUD/panel pixel, and six pre-change Original no-HUD scene
  hashes. Original **with HUD** intentionally changes because the HUD is restored;
  this is not an Original full-frame bit-identity claim. All simulation snapshot
  fields, tick and RNG remain unchanged after rendering.
- Horizon-only test checks zero global `operator new` calls after warm-up and
  pixelwise equality to fresh-buffer conversion at scroll 0/-721/719/INT_MIN/INT_MAX.
  Full canvas remains allocation-heavy (378–1674 `new` calls/frame); only the
  horizon allocation is eliminated, reducing RGB complete draws by exactly one.
- `--benchmark` records complete canvas draw (scene/actors/HUD/horizon/upload;
  no SDL_RenderPresent) with first-draw time and nearest-rank p50/p95/p99 of 20
  warm frames. CTest default uses one timed frame and has no timing assertion.

## Checks and evidence

- `cmake -S . -B build-agent-performance-20261015 -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` passed.
- `cmake --build build-agent-performance-20261015 --target race_cache-test --parallel 6` passed.
- `ctest --test-dir build-agent-performance-20261015 -R '^race-cache$' --output-on-failure` passed (1/1, 1.51 sec).
- `git diff --check` passed.
- `python3 tools/agent_lsp.py` reported healthy existing compile DB; no interactive
  client LSP tool exists here. Fallback `clangd --check=tests/race_cache_test.cpp
  --compile-commands-dir=build-agent-performance-20261015` parsed/compiled without
  source diagnostics but exited 3 because Apple clangd's feature testing reports
  three internal `ExpandDeducedType` tweak failures. Do not report a clean LSP gate.
  Log: `/tmp/rr-performance-clangd.log`.
- No new reverse engineering was needed: the ordering/allocation defect and
  fixes are in native presentation code. Parent's other slices perform REA.
- Parent integrator runs whole native/sanitizer gates separately.

## Measured performance

Apple M3 Pro, 19,327,352,832 bytes physical memory; macOS, Release, one raster
thread for timed draws, SDL_CreateSoftwareRenderer into a 640×480 output surface
(upload/scaling included), Napa career with seven opponents at native tick 300,
full_colour only for RGB (no fog/upscaling/post effects), texture detail off.
20 warm frames per case. First-draw `cold_ms` is a new canvas, **not** a controlled
cold disk/OS/RGBA-cache measurement. No device budget is inferred.

Baseline native header was taken from `git show fdcd4aa:src/race_caches.hpp` into
`/tmp/rr-performance-baseline.hpp`; a temporary copy of the same benchmark harness
omitted post-change correctness checks and linked the same current rr-resource
library. Source in the shared checkout was never replaced/stashed. This isolates
canvas changes; it is not a full clean-checkout historical binary comparison.

RGB p50 before→after (ms): 1× 4:3 1.522→1.149; 1× 16:9 2.765→2.362;
2× 4:3 8.944→7.747; 2× 16:9 9.805→8.569; 4× 4:3 26.007→21.555;
4× 16:9 29.348→24.621 (roughly 13–24% improvement). Indexed timings broadly
unchanged while restoring HUD output. These are bounded measurements, not a
cross-device performance acceptance result.

Whole-process peak RSS via `/usr/bin/time -l`: baseline benchmark 296,992,768 B;
current regression+benchmark 380,911,616 B. **Not comparable for memory savings**:
current process additionally runs fresh full-frame reference allocations and
pixelwise tests, while baseline omits them. No per-case memory budget claimed.

Baseline output (`/tmp/rr-performance-before-benchmark.log`):

```text
row=0 scale=1 wide=0 rgb=0 hash=13444732443979624547 allocations=378 cold_ms=1.87158 p50=1.22367 p95=1.24133 p99=1.26267
row=1 scale=1 wide=0 rgb=1 hash=2800544501961077726 allocations=379 cold_ms=2.06771 p50=1.52179 p95=1.561 p99=1.56958
row=2 scale=1 wide=1 rgb=0 hash=18290316952315886966 allocations=378 cold_ms=2.02513 p50=2.28967 p95=2.35404 p99=2.36971
row=3 scale=1 wide=1 rgb=1 hash=12480883420206857557 allocations=1531 cold_ms=2.65525 p50=2.76467 p95=2.78733 p99=2.79142
row=4 scale=2 wide=0 rgb=0 hash=6395269177200922959 allocations=378 cold_ms=7.53846 p50=7.08621 p95=7.44721 p99=7.57137
row=5 scale=2 wide=0 rgb=1 hash=8749086642151112002 allocations=1675 cold_ms=8.92538 p50=8.94438 p95=9.13408 p99=9.34767
row=6 scale=2 wide=1 rgb=0 hash=4509599773851679003 allocations=378 cold_ms=7.89533 p50=7.87488 p95=8.32563 p99=8.34788
row=7 scale=2 wide=1 rgb=1 hash=1477636935681336033 allocations=1675 cold_ms=10.1832 p50=9.80462 p95=9.95333 p99=10.0051
row=8 scale=4 wide=0 rgb=0 hash=9343361947229583644 allocations=378 cold_ms=22.2344 p50=19.0838 p95=19.7584 p99=19.9491
row=9 scale=4 wide=0 rgb=1 hash=637450489077128212 allocations=1675 cold_ms=27.4612 p50=26.0072 p95=26.6371 p99=26.7018
row=10 scale=4 wide=1 rgb=0 hash=2853708629718616441 allocations=378 cold_ms=26.0367 p50=21.8921 p95=22.3788 p99=23.144
row=11 scale=4 wide=1 rgb=1 hash=13490494933351736534 allocations=1675 cold_ms=31.5242 p50=29.348 p95=29.9783 p99=30.9412
```

After output (`/tmp/rr-performance-after.log`):

```text
row=0 scale=1 wide=0 rgb=0 hash=16908656509277595288 allocations=378 cold_ms=1.92413 p50=1.22388 p95=1.25092 p99=1.26042
row=1 scale=1 wide=0 rgb=1 hash=2800544501961077726 allocations=378 cold_ms=1.61875 p50=1.14942 p95=1.16525 p99=1.16846
row=2 scale=1 wide=1 rgb=0 hash=10442994832978502141 allocations=378 cold_ms=2.02429 p50=2.26817 p95=2.37071 p99=2.37983
row=3 scale=1 wide=1 rgb=1 hash=12480883420206857557 allocations=1530 cold_ms=2.15296 p50=2.36229 p95=2.387 p99=2.40025
row=4 scale=2 wide=0 rgb=0 hash=2448064240350656025 allocations=378 cold_ms=7.80867 p50=7.04758 p95=7.54587 p99=8.10883
row=5 scale=2 wide=0 rgb=1 hash=8749086642151112002 allocations=1674 cold_ms=7.85375 p50=7.74737 p95=7.92387 p99=8.57358
row=6 scale=2 wide=1 rgb=0 hash=5388972872623600265 allocations=378 cold_ms=8.09808 p50=8.17329 p95=11.5418 p99=11.9288
row=7 scale=2 wide=1 rgb=1 hash=1477636935681336033 allocations=1674 cold_ms=8.69767 p50=8.56871 p95=8.70112 p99=8.72521
row=8 scale=4 wide=0 rgb=0 hash=2092615998343516040 allocations=378 cold_ms=23.9612 p50=19.1651 p95=19.3068 p99=19.3267
row=9 scale=4 wide=0 rgb=1 hash=637450489077128212 allocations=1674 cold_ms=24.688 p50=21.5549 p95=21.8558 p99=21.8937
row=10 scale=4 wide=1 rgb=0 hash=6098208975936478377 allocations=378 cold_ms=28.5331 p50=21.8886 p95=22.0922 p99=22.1165
row=11 scale=4 wide=1 rgb=1 hash=13490494933351736534 allocations=1674 cold_ms=27.7772 p50=24.6211 p95=25.0037 p99=25.1591
```

## Remaining scope and held resources

Plan 32 E remains partial: cancellable upscale attachment/loading, broader
Enhanced presets/cache states, controlled cold-cache metrics, per-case memory,
additional allocation elimination, Android thermal/device budgets remain open.
No Wine/Ghidra/oracle resources used or held. File claim token
`e945cc12-4224-4bbe-94c8-bfd8ab7baa9a` released after handoff. Source/tests frozen.
