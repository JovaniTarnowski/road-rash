# Human-parity terrain research — October 16, 2026 (Plan 10 Task 5)

Read-only research. No source, build, capture, Ghidra-project, or shared-doc
mutation. HEAD `b4d3c0c` (2026-10-15 19:50 -0700, null-slice scan fix).
Untracked `analysis/codex-*`session dirs observed, untouched.

## 1. Baseline replays (existing binary, read-only)

`build/oracle-sim-test` mtime Oct 15 19:26 — **stale**: predates HEAD
(19:50, which integrates the null-slice fix). Results below are what this
binary measures, not post-HEAD behavior; the integrator must rebuild before
re-baselining sched900.

- `build/oracle-sim-test analysis/oracle/terrain-human200-20261015.tsv`
  → exit 1: `road_base: 1792`, `first divergence at frame 0 (tick 1, ring
  terrain): ring.walk.front.terrain.point.0.end original 132 native 133`.
- `... sched600-20261015.tsv --driver=demo` → exit 0: 600 ticks identical
  (270 golden-only diagnostics / 76822 uncovered era comparisons reported,
  not acceptance).
- `... sched900-20261015.tsv --driver=demo` → exit 1: `first divergence at
  frame 734 (tick 735): brain +0x194 original 5632 native 22784`.
  This is the pre-fix signature (NULL-slice overtake target). The "tick893
  RNG" figure from `codex-sim-research-20261015` came from that worker's
  **private worktree build**, not from this binary — do not cite 893 as main
  status until a post-HEAD rebuild replays sched900. No Wine captures run.

## 2. Fixture hashes (read-only)

EXE (verified twice, receipt + REA digest):
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`

| Fixture | SHA256 (tsv) | frames | branch | complete |
| --- | --- | --- | --- | --- |
| terrain-human200-20261015 | `94bb443a…87c96e4ae79d1` | 200 (ticks 1–200) | human | true |
| terrain-seeded-20261015 | `34a09c66…93163fe96a63b70` | 1 (seeded) | n/a | (receipt present) |
| sched-human-20261016 | `c8e5105f…2531f40d` | 1431 | human | true |
| sched600-20261015 | `58460337…f16256fcf45` | 600 | demo | true |
| sched900-20261015 | `ce533a4c…571fcb731c037` | 900 | unverified | true |

Seeded component parity (960 states + 18 terminal RNG, course-4 menu
bootstrap) still holds per the terrain fixture handoffs; it proves hill
draw *formulas*, not race-setup draw *interleaving* (see §4).

## 3. Exact gap

Frame 0 (post-tick-1 dump) golden values, front walker (record 7,
position 33280 = 0x8200, last_sample 130):

- point.0: start 130, **end 132**, x 2079, height 208, dx 0, dh 0.
- Native: end **133** (comparator stops at first key; native x/height/dx/dh
  unobserved — report-all mode does not exist, see §5).
- Back walker (position 512 = 0x200, record 0): point.0 start 1, end 3 —
  reached by the comparator only after front passes, so back parity is
  unproved, not proved.

Course-ground truth (read-only parse of `NAPA.CRS`, first `LIHR` payload at
0x1180, 46 records): record **7 = [126, 150]** — the only record-7 in any
NAPA LIHR section containing sample 130. Point.0 bounds bytes
`[40,65,75,2,13,75,1,3]` = min_x 40, max_x 65, x_var 75, min_h 2, max_h 13,
h_var 75, **min_length 1, max_length 3**.

Arithmetic: length = 1 + rand()%3. Original end 132 → length 2 → draw%3 = 1.
Native end 133 → length 3 → draw%3 = 2. Record end (150) is far away, so the
**clamp hypothesis is dead** — this is a genuine draw-value difference at
the same sample with the same start.

Consistency notes: x 2079 sits 1 below max_x*32 = 2080 and height 208
equals max_h*16 = 208, so dx = dh = 0 is the ordinary ceiling-clamp path
(`(2080-2079)/2 = 0`, `(208-208)/2 = 0`), not an anomaly. Front record stays
7 with p0.end evolving 132 → 137 by tick 189; rng moves 1133891140 →
409811144 over the same span. The tick-189 RNG gap (seen on
sched-human-20261016) is unreachable on this fixture until frame-0 setup
parity is fixed — it is a downstream consequence, not an independent bug.

## 4. Setup-ordering hypothesis

Enter semantics match and are eliminated: original `0x00452cb0` with
param_3 = 1 (forward enter, `analysis/decompiled/00452cb0.c:9-14,19`) sets
walker position 0 and passes 0 as the `0x004510c0` init position — exactly
what native `TrackRing::enter` does (`track_ring.cpp:117-120`: position 0,
`emplace(section.terrain, 0, …)` → hill_record_ 0, start = end = 0, 16 init
draws). REA re-confirmation of `0x00450fc0` today (see §6) locks the init
draw order (2 sides × 4 points × 2 draws via `0x00441750`).

Remaining suspect: draw **interleaving during the `0x004552e0` ring fill**
(one +0x200 walk to back_ = 0x200, then 128 × {build_slice, step 0x100} to
0x8200). Per-step terrain slope draws (length + 2 slopes per uncovered
point) share the CRT stream with non-terrain draws. Candidate count
mismatches, in likelihood order:

1. `build_slice` non-terrain draws: native calls `borr.slots` **twice** per
   slice (`track_ring.cpp:160-161`) plus one jitter draw per billboard
   (`:168`); the original `0x00454c80 → 0x00454240` per-slice draw count is
   unproved. Any per-slice ±draw accumulates over 128 slices and shifts
   every later slope draw — including record-7's.
2. `hill_slopes` skip-condition edge (`terrain.cpp:84`: `v.end > sample &&
   sample > v.start → continue`, no draws) vs original `0x00451380`:
   a one-sided `>=`/`>` difference at any earlier step shifts the stream
   by 3 draws per point with no state-visible trace except downstream ends.
3. Record-transition edge in `TerrainIterator::step` (`terrain.cpp:126`:
   `hills[record+1].start <= sample`) vs original `0x004515a0` across the
   ~129 setup steps (records 0 → 7): entering a record one sample off
   re-times that record's whole slope block. (Start = 130 matches on both
   sides now, so a transition shift would have to be exactly compensated —
   less likely than (1)/(2), but unproved.)

Seeded fixtures cannot adjudicate any of these: they drive a synthetic
2-record fixture with ±256 steps and no slice builds, so per-slice
interleaving and multi-record NAPA transitions are both uncovered.

## 5. Discriminating capture spec (for the capture owner)

Extend `RR_ORACLE_TERRAIN_TRACE=1` on the established human course-4
recipe (same controls/seed/SOLO/HUMAN/frames); do not overwrite accepted
goldens. Dump, every frame from frame 0 through ≥ tick 189:

- Already present (keep): `ring.walk.{front,back}.{position,
  terrain.position,last_sample,record,point.i.start/end/x/height/dx/dh}`.
- NEW (hidden fields that decide §4): walker **section id** (which LIHR
  payload is active); the active section's **record-bounds table**
  (`ring.terrain.record.<k>.start/end` for all k — pins transition edges);
  a per-tick **CRT draw counter** for the ring-fill + tick (current
  `rng_calls` reads 0 on every sampled frame including tick 189, so it
  does not count setup/walker draws); `ring.pre_draws`/`ring.post_draws`
  + `seed` in the header (absent today — the harness hardcodes 9098/11).
- Native-side diagnostic (implementer note, not done here): a
  report-all-terrain-diffs mode for the setup frame, since the first-diff
  stop hides whether native x/height/dx/dh also differ (a full-row match
  except `end` would isolate the length draw; wider mismatch would point
  at an earlier stream shift).

First discriminator to check on the new trace: the tick/sample at which
the front walker enters record 7 and the draw-counter delta between
record-7 entry and point.0's length assignment, vs the native walk.

## 6. REA spot-check (1 call, pinned toolchain)

`npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE
0x00450fc0 --format md` (Node v24.13.0, Ghidra 12.1.4 libexec, JDK 21),
exit 0, log `/tmp/rea-450fc0-human-terrain.log`. EXE digest
`73d7f974…a60f18a2` verified in-report. Finding: recovered pseudocode is
**whitespace-identical to the existing export
`analysis/decompiled/00450fc0.c` (1039 non-whitespace chars)** — init
draw order and `param_3 ? last-record : 0` selection re-confirmed, no new
behavior. (All other setup-path functions — `0x004552e0` ring fill,
`0x00452cb0` enter, `0x00451380`/`0x004515a0` steps, `0x00437e60` cadence —
already carry REA confirmations per `docs/REVERSE_ENGINEERING.md`, so no
second call was spent.)

## 7. LSP (read-only)

`python3 tools/agent_lsp.py --check src/terrain.cpp`: preflight finds 181
entries, no missing sources, but `healthy: false` (`CMakeLists.txt` newer
than the compilation DB) and Apple clangd `--check` exits 3 on the known
preexisting `ExtractFunction` break/continue tweak failure; **no source
parse diagnostic** for `terrain.cpp`. Same limitation class as prior
handoffs — recorded, not a gate.

## 8. What was NOT changed

No edits to any file (claimed or otherwise): `src/race.cpp`,
`traffic.*`, `terrain.*`, oracle proxy, `race_setup.*`, `docs/PROGRESS.md`,
`docs/VERIFICATION.md`, `docs/REVERSE_ENGINEERING.md`, `docs/plans/10`,
`CMakeLists.txt` untouched. No build directories created or rebuilt, no
Wine captures, no Ghidra project writes (REA ran ephemeral
`ephemeral-source-immutable`), no commits. Only this handoff is new.
