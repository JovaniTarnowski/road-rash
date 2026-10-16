# Handoff: plan-16 Task 6 HUD sub-slice — needle pen + opponent naming/gap (20261016d)

- Task: docs/plans/16-gameplay-systems.md Task 6 (dashboard needle pen + opponent list),
  sub-slice limited to free files per parent brief.
- Baseline: `8851ef6` (HEAD at session start; `git rev-parse HEAD` verified).
- Build dir (owned): `build-agent-hud-20261016d` (Debug). No commits/pushes; parent integrates.
- Prior trace input: `docs/agent-handoffs/needle-trace-20261015.md` (§1 pen, §5 endpoints,
  §8 opponent list) plus fresh reads of `analysis/decompiled/00438550.c`,
  `00446b00.c`, `00446b40.c`, `00446b80.c`, `00446c30.c` (stride select, meter
  offsets, needle order, pen args). No source edits outside the owned slice,
  no oracle runs, no captures.

## File claims respected

- `python3 tools/agent_workspace.py list` before editing: claimed paths were
  docs/PROGRESS.md, REVERSE_ENGINEERING.md, VERIFICATION.md, plans/10,
  plans/32, src/race.cpp, traffic.*, terrain.*, race_setup.*, proxy,
  CMakeLists.txt, oracle_sim_test, traffic_test, race_setup_test.
  None of `src/hud.cpp`, `src/hud.hpp`, `tests/hud_test.cpp` was claimed.
- Touched ONLY those three files. No new CMake targets (only extended existing
  `hud_test.cpp` cases). No edits to shared docs, plans, or claimed sources/tests.

## What was wrong (all verified against the decompile)

1. Pen width ignored resolution (`src/hud.cpp` needle lambda passed `scale`,
   i.e. always the 3-wide block). Original: `CreatePen(PS_SOLID, HiRes+2,
   0x81172)` in `0x00446c30` (`00446c30.c:143-148`) — width 3 high, 2 low,
   cached per HiRes slot.
2. Needle draw order reversed: native drew rpm then speed (speed on top).
   Original draws speed (`DAT_0049e10c`, layout `+0x54`) first, rpm
   (`DAT_0049e108`, layout `+0x40`) second, so rpm is on top
   (`00446c30.c:149-176`). Pixel-identical today (same color/width), fixed for fidelity.
3. Solo-race gap garbage: `nearest_opponent` returned `{-1, -0x20000000}` with
   no target. Original leaves stale `-0x20000000` in `DAT_004b8a34` with a null
   target (`00438550.c:34-39`); normalized to `{rider -1, gap 0}` since no draw
   path reads a gap without a target (`0x00446b80` clears name+meter; entry-10
   gap text is gated on the name).
4. `Hud::state` set `target_stamina`/`target_gap` even when no name row existed
   (short `rider_names`), leaving `target.empty()` with nonzero gap/stamina.
   `0x00446b80` always writes name+meter together; the gap now rides with the
   name, restoring the invariant `target.empty() ⟺ gap==0 && stamina==0` that
   `compose` already assumes.

Checked and deliberately NOT changed (traced, already correct):
- Needle layout offsets (tach 16-19, speedo 21-24, all strides — dumped from EXE
  strides 0-3), endpoint math (`high_product` == `imul`/`mov %edx,%eax` floor),
  `sine()` quadrant logic, angle formulas incl. `+4096`-if-negative wrap.
- Meter slots: stamina→40 (`0x00446b00`→`DAT_0049e117`), condition→45
  (`0x00446b40`→`DAT_0049e118`), target→50 (`0x00446b80`→`DAT_0049e119`);
  `meter_level` matches `((c*2-2)*15)/(b-1)+1` (guards kept: original never sees
  base ≤ 1 live, and base==1 would divide by zero there).
- `behind*2 < ahead` strict rule, tie (d==0) → ahead, first-minimal-slot wins —
  all match `00438550.c:20-39`.

## Changes

- `src/hud.hpp`: new `NeedlePen{width, color}` + `Hud::needle_pen(bool high_res)`
  (static, `{3|2, 0x721108}`) and `Hud::pen()` (selects by `low_`); updated
  pen/nearest-opponent doc comments (GDI band/cap SHAPE still open per
  needle-trace §7 — no raster guessing).
- `src/hud.cpp`: `line()` takes full band width (`3`→centered 3-wide = exact old
  behavior; `2`→2-wide anchored block); `needle_pen`/`pen` impls; compose draws
  speed-then-rpm with `pen().width*scale` band and raw `0x721108`; solo early
  `return {}`; state() target block gated on name-row presence.
- `tests/hud_test.cpp` (appended cases only): pen ids + `meter_level` edge pins
  (disc-free section); solo gap 0, tie→first-slot-ahead, `behind*2==ahead`→ahead,
  short-names invariant, synthetic parked-left (angle 3072) high 3-wide tach +
  speedo pins and low 2-wide pins (disc-gated section).

## Test evidence (owned dir `build-agent-hud-20261016d`, Debug)

- Baseline before edits: `ctest -R "^hud$"` → 1/1 Passed.
- Fail-before (new tests vs old behavior): pen-id case does not compile against
  old header (no `needle_pen` API); runtime probes each exit 1 with the old code
  path restored one at a time: `Solo race gap is not normalized`,
  `Target naming without a name row`, `Low-resolution needle is not 2-wide`.
- After: `ctest -R "^hud$"` → 1/1 Passed; neighbors
  `ctest -R "^(hud|frontend|text|framebuffer)$"` → 4/4 Passed.
- Full `cmake --build build-agent-hud-20261016d --parallel` (all targets incl.
  `rr-game`) → exit 0.
- `python3 tools/agent_lsp.py --check` on each edited file (hud.cpp/hpp,
  hud_test.cpp) → `clangd --check` exit 0, 0 errors.
- (One mid-session mishap: a `git checkout src/hud.cpp` during probing wiped the
  .cpp edits; re-applied all five edits, diff-verified against a pre-probe copy,
  forced rebuild — final binary is from the final source. No other worker files
  touched; `git status` shows only the three owned files modified.)

## Remaining work (not mine / out of slice)

- GDI band/cap SHAPE (§7 of needle-trace): needs parked-needle capture or Windows
  GDI reference raster — do not guess; spine math frozen.
- Full multi-row opponent-list overlay (§8: rect 320×60, ≤4 rows, 11/15 px rows,
  `settings.opponent_list || inset` gate) needs `game_main.cpp`/settings wiring —
  outside this slice's free files.
- Low-mode layout uses halved-high stride (low strides 0/1 differ, e.g. speedo
  ry 18 vs 14, origin x 124 vs 125) and low blit placement was not audited here;
  needs the OpponentList-on / low-res capture from Task 6 Step 2.
- `0x46afdc` nonzero writer and mini-mode readout decision still open (recorded
  in needle-trace §8/§9).
