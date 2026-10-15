# Completion and code review — October 14, 2026

Baseline: `6788592687607d84a4d09d31bbd9b47ea943d79d` on the shared tree.
Review scope: current plans, status, recent commits and handoffs; race construction,
race screen, view interpolation, rendering caches, lifecycle, police wiring,
audio streaming, save writing and native test/build infrastructure. This is a
targeted engineering review, not a line-by-line audit of every parser or a new
measurement of original-game fidelity. No production files or accepted goldens
were changed. The active SIM agent's claims were inspected and preserved.

## Completion estimate

**Approximately 65% complete against the documented whole-port definition of
done; plausible range 55–70%.** Feature implementation alone is around 80%,
but implementation and acceptance are different deliverables. These are
judgment estimates of remaining work, not measured code percentages or schedule
predictions. The uncertainty is dominated by reverse engineering and hardware.

The following weighting makes the estimate inspectable. Count overlapping
plans only once; do not divide completed checkboxes by all historical plans.

| Deliverable | Weight | Estimated completion | Weighted points |
| --- | ---: | ---: | ---: |
| Formats, import, foundation and tooling | 10% | 95% | 9.5 |
| Race behavior and exact simulation acceptance | 30% | 50% | 15.0 |
| All-course rendering, animation and visual acceptance | 20% | 75% | 15.0 |
| Front end, career, saves and settings | 10% | 80% | 8.0 |
| Audio, music, video and trigger acceptance | 10% | 80% | 8.0 |
| macOS/Android platform, input and device acceptance | 15% | 40% | 6.0 |
| Distribution, upgrade and final acceptance | 5% | 40% | 2.0 |
| **Total** | **100%** | | **63.5 ≈ 65%** |

Scope follows plan 00's single-player macOS ARM64/Android ARM64 definition,
including options/high-resolution/local upscaling requirements. Multiplayer
is excluded pending a decision; the additional browser port is excluded.
Owner-approved dropped work, such as extended draw distance, is not charged
as missing work. A playable macOS prototype is much farther along than a
verified, distributable whole port.

Evidence supporting substantial implementation:

- One race factory supplies game, soak and replay; all five courses use the
  scene pipeline. The previous review's three-construction-path P0 is fixed.
- C++20, defined field proxies, threaded indexed/RGB raster paths, persistent
  raster scratch, atomic saves and warning gates exist. Plans 25–30 delivered
  substantial engineering work; do not reopen their original findings blindly.
- MIDI/SF1 synthesis, native settings, enhancement controls, pedestrians,
  intersections, crashes and actor animation have implementations.
- The current SIM integration fixes billboard forwarding/overflow, validates
  fresh captures and hydrates frozen setup state. This is meaningful progress.

Evidence preventing a higher completion claim:

- Exact simulation acceptance remains open. Latest handoff identifies tick
  210 frozen-scheduler dt4 versus native dt1; this review did not run a fresh
  original capture. Real player-branch proof remains open. A passing
  expected-divergence diagnostic does not close this requirement.
- Police helpers are not live police: `load_race_assets` reads `PoliceConfig`,
  but `make_police_actor`, `police_spawn_due` and `police_arrest` have no live
  callers. `RaceScreen` produces win/loss/wreck outcomes, not a live arrest.
- `Race::walls` remains an explicit stand-in. Fight/overtake lookahead,
  pedestrian pair contact and parts of traffic behavior remain incomplete.
- `load_race_assets` still uses start `0x3200` and route-A end as bounds,
  independent of level. Five rendered courses are not 25 accepted races.
- Component pixel fixtures and original-list replay do not prove complete
  native scene construction. Natural-frame and branch-route acceptance,
  original save round trips and Android hardware acceptance remain open.

## Findings

### R1 — P1: quick-race restart remains latched (confirmed by source)

`src/race_screen.cpp:81,103`: the event sets `restart_ = true`, and every
subsequent `iterate` calls `g_.restart()` without clearing it. A single Enter,
gamepad South or touch after finishing continually reconstructs the race and
resets its countdown. `Game::restart` rebuilds actors and race state, so this
also repeatedly performs expensive setup. The action translator's tests do
not execute this stateful screen path.

Consume the request once and reset screen timing/view/end state as appropriate.
Add an integration regression that requests one restart, iterates multiple
frames and proves exactly one rebuild and an advancing countdown.

### R2 — P2: reverse interpolation jumps about half the uint32 range (reproduced)

`src/view_state.cpp:10–11`: `uint64_t(b - a)` scales an unsigned modular
difference. For backward movement 1256 → 1000 at alpha 32768, production
`blend_view` returns **2147484776**, expected **1128**. The teleport test
explicitly permits a small backward displacement, so it does not prevent this.
The same helper handles actor positions. A probe linked to the fresh production
library reproduced the result (`analysis/review-20261014/blend_probe.cpp`).

Use a deliberately defined shortest signed modular delta for permitted small
steps, then wrap the result. Preserve the forward-through-zero behavior.
Test backward movement, backward-through-zero and both endpoints.

`blend_i32` at line 8 also widens **after** `b - a`. A synthetic INT_MIN →
INT_MAX lateral transition triggers UBSan signed overflow at this exact line.
This is a confirmed arithmetic defect; such extremes were not observed in
normal gameplay. Widen each operand before subtraction and treat heading wrap
using its own angle period rather than a linear signed midpoint.

### R3 — P2: interpolation snapshots span render frames instead of ticks (source)

`src/race_screen.cpp:164–185` executes all catch-up ticks before rotating and
capturing views once. At 30 rendered FPS, two 60 Hz ticks can separate the
snapshots; `alpha_q16()` still measures a fraction of one tick. This yields an
incorrect presentation trajectory under load. `reset_view_history` invalidates
the ready flag but does not seed both snapshots, so the next capture can also
reuse a pre-reset `view_cur`.

Rotate/capture around each actual simulation tick; seed both snapshots after
restart/resume. Keep all presentation work independent of simulation RNG.
Test 0/1/2/4 ticks per rendered frame and a reset followed by the first tick.

### R4 — P2: pause texture survives renderer resets (source; device symptom untested)

`src/race_screen.cpp:69,93,198–205`: reset handling clears sprites/canvas but
not `pause_tex_`. More critically, an open pause menu returns before reaching
that handling. Target contents can be lost, and device resets require texture
recreation; the retained texture is reused unless image width changes. Several
pause draw/upload return values are ignored. Foreground/device recovery can
therefore leave a blank pause screen without a reported upload failure.

Process renderer events before menu input routing, invalidate every screen-owned
GPU cache and recreate/reupload pause contents. Verify reset while racing and
while paused. Hardware behavior remains a device acceptance item.

### R5 — P2: widened dashboard panels only fill half their width (source)

`src/race_caches.hpp:114–126`: `side_w` already equals the width of **each**
side margin. The single loop partitions `side_w` into two halves, filling only
`side_w/2` columns on each side. Each panel should cover `side_w` columns.
The RGB branch also never composites the generated side panels from `frame`.
This disagrees with the stated stretched/darkened side-panel behavior.

Cover full left/right extents in both output paths, with sentinel/background
checks at every margin column at 16:9 and ultrawide sizes. Confirm expected art
with the owner during visual acceptance; no artwork change is proposed here.

## Performance and maintainability

These are measured code-path observations, not FPS improvement promises:

1. `CourseCanvas::draw` creates a full-size `Framebuffer hz` every RGB frame
   (`race_caches.hpp:154`). At scale 4 and 4:3 this alone allocates roughly
   4.9 MB/frame; widescreen increases it. Retain a horizon buffer by size and
   reuse it, or convert only the horizon region into persistent RGB storage.
2. Dashboard composition and indexed palette mapping run before deciding
   `use_rgb`; RGB subsequently composites the RGB dashboard again. Avoid the
   unused indexed pass, preserve side panels and cache static dashboard layers.
   Benchmark complete `CourseCanvas` frames, not just raster kernels.
3. `Game::rider_nodes` builds/sorts temporary vectors for repeated slice calls,
   while `blend_view` matches actor IDs with a nested scan. Profile allocation
   counts and frame-time percentiles before changing the small actor lists.
4. Optional upscaling is attached synchronously on the draw path. Plan 20
   already records an approximately 15-second first-enable stall. Move cache
   discovery/decoding into a cancellable loading step; publish immutable texture
   data at a frame boundary. Do not perform worker mutations during rendering.
5. Keep `Race` decomposition incremental: separate scheduler metadata and
   presentation snapshots first. A broad physics rewrite before goldens pass
   would make the current divergence harder to isolate.

## Plan quality

The plans have good source addresses, explicit acceptance and a useful
diagnostic/acceptance distinction. However, status is fragmented:

- `00-INDEX.md` still says no original scene for other courses below a header
  saying all five scenes are implemented, and says simulation has no oracle
  in an old review block. Historical dates exist but these blocks are too easy
  to read as current facts.
- `PROGRESS.md`'s table says MIDI is not implemented and pedestrians are not
  started, despite implementation and later timeline evidence. Current status
  receipts reference older source commits; they cannot certify this HEAD.
- Checkbox counts mix translated helpers, fixture verification and end-to-end
  acceptance. Plan 17's all-course renderer progress does not close per-level
  bounds or original branch-route comparisons.

The integrator should publish one current matrix with separate implementation,
synthetic tests, original comparison, platform coverage and residual gaps.
Preserve historical evidence under clearly marked checkpoints.

## Validation

Fresh owned build: `build-review-20261014`, Debug, C++20,
`RR_STRICT_WARNINGS=ON`, compilation database enabled. Configure and full build
passed with no warning/error diagnostics in the build log. Full CTest:
**97/97 passed, zero skips, exit 0, 206.84 seconds** (`-j 4`). This includes
the expected-divergence oracle diagnostic, not strict simulation acceptance.
Build source fingerprint: `f0d2b594ac772454f61d8583777657f3ed016716c8f26db580ba280a9c87cd08`.
During the review, the active agent committed `95af953` (oracle metadata dump
and shared documentation). The native build remains identified with the stated
baseline; it does not validate that new proxy instrumentation.

The reverse probe called the actual compiled `blend_view`. The overflow probe
compiled the actual `src/view_state.cpp` with UBSan and linked the fresh library;
it aborted with signed-overflow diagnostics (expected reproduction, not a suite
pass). Probes and logs are local under `analysis/review-20261014` and `/tmp`.

C++ LSP client integration was unavailable; `agent_lsp.py` found a healthy
181-entry database and ran clangd on `view_state.cpp`. clangd returned 3 for
four ExtractFunction feature-check errors, not compiler parse diagnostics.
The strict compiler build is the compile evidence.

No fresh Wine/original capture, Android hardware run, Android assembly,
full sanitizer suite, package/signing or browser validation was performed by
this review. Existing receipts are historical evidence only.

Next steps: [completion execution plan](../plans/32-completion-review-next-steps.md).
