# AI replan / target-choice / scan-order research — 20261016c (read-only Plan 10 Task 6)

Baseline: `aae00a1` (HEAD, verified `git rev-parse`). `git status --short` shows no tracked
modifications; no commits, no pushes, no builds, no Wine captures, no Ghidra/prefix mutation.
New files only: this handoff + untracked scratch `analysis/ai-replan-20261016c/`
(rea-4082f0.md/.log, rea-408cd0.md/.log, NOTES.md).

Scope: AI replan `0x004082f0` + target choice `0x004090a0`/`0x004091f0` + scan order for
equal-position objects. The "bike-before-half" tie rule from the tick-735 note is
**not** runtime-proven; this handoff records what REA confirms and what capture would decide it.

## REA spot-check (2 calls max, pinned env)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`rea-agents@6.4.0`, `--format md`.
EXE hash verified: `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.

1. `function ../cdrom/ROADRASH/ROADRASH.EXE 0x004082f0 --format md` → exit 0,
   evidence_id `ev_a64f3b2b982bd88d793d6ce942c6e9e585c3b6b6eb98e4b844e23279701c505e`
   (7024 lines, saved `analysis/ai-replan-20261016c/rea-4082f0.md`).
2. `function ../cdrom/ROADRASH/ROADRASH.EXE 0x00408cd0 --format md` → exit 0,
   evidence_id `ev_2d02b1d485b48797bd9bb6083ec25a78a371a3806130e40390a16ca81a351eda`
   (994 lines, saved `analysis/ai-replan-20261016c/rea-408cd0.md`).

Not called this session (call budget): `0x004091f0` (straight target) and `0x004090a0`
(curve target). Curve-target structure is cited from the prior session's REA
(`0x004090a0` ev_7fc273f1fc62c8dfa1ab309983aef4ef2d4efeaf2f8f2e3ed374d3544e42e060,
tick735-target-20261016.md); no new claim is made about `0x004091f0` internals beyond that.
REA is a spot-check, not runtime proof (per `docs/TOOLING.md`).

## Findings

### 1. Replan `0x004082f0` — cadence and draw sites
- Window select `0x408312–21`: EBX=`0x5000` when `DAT_004642dc` node `+0x2fc` equals the
  self node `+0x2fc`, else `0xa00`. EBX is the scan window passed to both `0x00408cd0` calls.
- Cadence gate `0x408326–2f`: brain `+0x188` (native `AiState::replan_ticks`,
  `src/race.hpp:143`) `< 0x3c` (60) skips the easing block and goes straight to the scans
  at `0x408440`. Otherwise the 10-row easing loop (`0x408378–422`, CL 0..9) steps currents
  (`+0x0/+0x4/+0x8`) toward targets (`+0x78/+0x7c/+0x80`) by `+0x12c/+0x130/+0x134` with
  target clamping, then `+0x188 -= 0x3c` (`0x408428–31`) and repeats while still `>= 0x3c`.
- Scans: `0x40844a CALL 0x00408cd0` (EDX=1, forward) → brain `+0x16c` (`nbr_ahead`);
  `0x40845e CALL 0x00408cd0` (EDX=0, backward) → `+0x170` (`nbr_behind`).
- Downstream in the same function: overtake-step ratings → `+0x178`/`+0x17c`; target pick
  → `+0x168` (`target_rider`) / `+0x174` (`target_step`) with the `|gap|` tie-break at
  `0x4087dd–808`; `0x00432500` gate at `0x408849` (target kept only with `+0x324==0` and
  `DAT_004c5d7c==4`, else nulled); speed select via jump table at `0x408976`
  (`DAT_004b8a12` level + `DAT_004642d8` road-half state); tail `0x00449c20` range check at
  `0x408a65` against `0xa00`.
- Draw sites: **no RNG call observed inside `0x004082f0` itself** — callees are exactly
  `0x00408cd0` (x2), `0x00432500`, `0x00449c20`. Simulation RNG draws attributed to this
  path live downstream (`0x004092e0` ai_can_overtake reach draw; contact/grunt path), not
  in replan. A per-tick `rng_calls` + draw-site attribution capture at the real divergence
  (893 RNG) is still the way to confirm which downstream site fires.

### 2. Target choice `0x004090a0` / `0x004091f0`
- Cited (not re-probed): `0x004090a0` entry skip when
  `(pos>>8) - (brain+0x198>>8) == -0x3c0` (`0x4090bb–cb → JZ 0x4091da`); curvature via
  `CALL 0x00449b40`, `mag < 0x30000 → CALL 0x004091f0`; speed-units
  (SAR 0xc/0x7/0x9) → `CALL 0x00409020`, `shown < lim/2 → CALL 0x004091f0`; inside-curve
  math with `0x60000` clamp writing `+0x198`/`+0x194`/`+0x19c`, then `+0x194` clamped to
  lateral (`0x4091b9–d4`). Callers `0x40900e`, `0x409e29`.
- Tick-735 arithmetic (unchanged from tick735-target-20261016.md): rider 0 think-entry
  position 25255; unlatched fall-through gives straight target `tpos = 25255+0x3c0 = 26215`,
  `tx = 5632`. Latched stale-native path on the 27648 type-4 bike gives
  `tpos = 27648+0x3c0 = 28608`, `tx = -9984 + 0x8000 = 22784` with side
  `(0x3000+0x3000)/2+0x5000 = 0x8000` for type-4/8/0x20 objects.

### 3. Scan order for equal-position objects (tie rule — NOT proven)
- `0x00408cd0` pseudocode (this session):
  `bVar1 = (DAT_004642dc != 0 && [DAT_004642dc+0x2fc] == self+0x2fc)`;
  loop `node = FUN_00449ce0(self+0x2fc, 8, forward, window)` (`0x408d07`, mask 8 admits
  type-8 halves); skip while lateral (`obj+0x28>>8`) is outside the slice
  (`+0xe0`/`+0xe4` at `0x408d12–2d`) unless `bVar1`; return `node[0x71]` (`+0x1c4`).
  **Order is inherited from `0x00449ce0` iteration, not decided here.**
- Native claim (`src/race.hpp:363-365`, declarations `373-374`): position-ordered
  scheduler list (`0x00438aa0` insert sort, ties in creation order) feeding
  `scan_next` (`0x00449ce0`).
- 735 instance (tick735-target-20261016.md): at pos 27648, nodeaddr.6 type-4 bike
  (lat -9984) precedes nodeaddr.5 type-8 half (lat 8192); first candidate wins ties.
  This is **one observation consistent with creation order**, not a proven general
  "bike-before-half" rule. `0x00449ce0`'s own tie-break (insertion order vs type priority)
  was already REA'd (ev_ea2ee596…, same session notes it matches `src/race.cpp`
  NULL-slice comments) but REA cannot show which of two live equal-position nodes the
  scheduler actually returns first — that needs the runtime scan-order log below.
- `tests/frozen_state.hpp` contributes nothing to scan order (hydrates laterals/slices/
  sched metadata; no ordering logic). No other tie-break note found beyond the 735 handoff.

## Native-vs-original table

| Item | Original (REA) | Native (`src/race.hpp` decl.) | Parity status |
|---|---|---|---|
| Replan entry/window | `0x004082f0`: EBX `0x5000` vs `0xa00` by `DAT_004642dc +0x2fc` identity (`0x408312–21`) | `ai_replan` (`379`); window source not in header | Structural; window-select mapping unvalidated at runtime |
| Replan cadence | `+0x188` vs `0x3c`, `-= 0x3c` loop (`0x408326–2f`, `0x408428–3a`) | `replan_ticks +0x188` (`143`) | Field mapped; cadence behavior not per-tick proven here |
| Rating easing | 10 rows, steps `+0x12c/+0x130/+0x134`, target clamp (`0x408378–422`) | `rating/rating_tgt/ease` (`153–155`) | Structural match only |
| Replan scans | `0x00408cd0` fwd/bwd at `0x40844a/0x40845e` → `+0x16c/+0x170` | `ai_scan_replan` via `0x00449ce0` (`384`); `nbr_ahead/behind` (`149–150`) | Declared; pick parity depends on scan order (open) |
| Scan wrapper | `0x00408cd0`: mask 8, lateral-vs-slice `+0xe0/+0xe4` skip unless same-brain (`0x408d02–33`), return `+0x1c4` | `scan_next(from, mask, forward, window)` (`374`) | Structural; `+0x1c4` return mapping needs harness check |
| NULL-slice gate | `0x00449ce0` self/candidate gates (prior REA ev_ea2ee596) | NULL-slice retention guards (b4d3c0c, per 735 handoff) | Fixed-binary parity at 735; stale-binary artifact retired |
| Target curve/straight | `0x004090a0` structure (prior REA ev_7fc273f1) → `0x004091f0` fall-through | `ai_curve_target` (`388`) / `ai_straight_target` (`389`); `target_x +0x194`, `target_position +0x198` (`137–138`) | 735 arithmetic reconciled; `0x004091f0` not re-probed here |
| Target latch | `+0x168/+0x174` pick, `0x00432500 + DAT_004c5d7c==4` gate (`0x408826–6a`) | `target_rider +0x168`, `target_step +0x174` (`147–148`) | Structural only |
| Equal-pos order | `0x00438aa0` insert sort (header comment); `0x00449ce0` first-candidate-wins | `build_scan_list` / creation-order ties (`363–374`) | **Open: bike-before-half needs runtime proof** |

## Discriminating capture (proposed, NOT run — no builds/captures per task)

Goal: prove or refute the equal-position tie rule and close 735 for good, on the
fixed binary (`build-agent-gate-20261016` generation or a fresh rebuild by owner/integrator).

1. Fixture: `analysis/oracle/sched900-20261015.tsv --driver=demo` (same as 735 work).
2. Log per tick 734–736 for rider 0 (demo-hold player AI): brain `+0x168` (target_rider),
   `+0x194` (target_x), `+0x198` (target_position), plus `+0x16c/+0x170/+0x174` and
   think-entry position. Expectation at 735 on fixed native: `tx=5632, tpos=26215,
   latched=-1` (matches original; stale binary gave `22784/28608`).
3. Scan-order log at the same ticks: dump native `scan_[]` entries in iteration order
   around position 27648 — `(position, type, lateral, rider/half identity)` — and mark
   which index `ai_scan_replan`/`ai_scan_overtake` latched vs skipped (with slice
   present/NULL reason). The original-side order cannot be logged without a new probe;
   the decidable native-side questions are: (a) does the 27648 bike precede its half in
   `scan_[]`? (b) does the first equal-position candidate win? (c) does any
   equal-position pair ever surface half-first (refuting a hard bike-before-half rule)?
4. Success criterion: (a)+(b) with NULL-slice skips at 27648/30208/31488 reproduces
   `latched=-1 → 26215/5632`. A single half-first equal-position pair, or a
   bike-first order that still latches the half, refutes the 735 note's tie
   decomposition and reopens `build_scan_list` ordering (not the NULL-slice guards).

## Read-only LSP

`python3 tools/agent_lsp.py --check src/race.hpp`: clangd `--check` exit 0, 0 errors.
Preflight `healthy:false` solely because `CMakeLists.txt` is newer than
`build-clangd/compile_commands.json` (stale-config timestamp caveat, same as prior
handoffs). No source diagnostic; no build configured.

## Explicit NOT-changed list

No tracked file edited. Read but untouched: `src/race.hpp`, `tests/frozen_state.hpp`,
`docs/agent-handoffs/tick735-target-20261016.md`, `docs/agent-handoffs/replan-verify-20261012.md`.
Live-claimed paths avoided (no edits, no builds affecting them): `docs/PROGRESS.md`,
`docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`, `docs/plans/10-simulation-oracle.md`,
`docs/plans/32-completion-review-next-steps.md`, `src/race.cpp`, `src/traffic.cpp`,
`src/traffic.hpp`, `src/terrain.cpp`, `src/terrain.hpp`, `src/race_setup.cpp`,
`oracle/aweman_proxy.c`, `CMakeLists.txt`, `tests/oracle_sim_test.cpp`,
`tests/traffic_test.cpp`, `tests/race_setup_test.cpp`. No fixture/golden/allowlist touched;
no Wine capture run; no Ghidra project or prefix mutation.

## Remaining work (for owner/integrator, not this session)

- Run the 734–736 per-tick brain + scan-order capture above on a fresh fixed binary.
- Attribute the real current divergence (893 RNG, +2 native draws; prime suspect per prior
  handoff: contact path `bike_c0` gate vs downstream `0x004092e0` reach draw) with
  `rng_calls` + draw-site logging at 890–893.
- Do not change equal-position ordering or `+0x36` road-half handling on 735 evidence alone.
