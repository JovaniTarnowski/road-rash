# race-cache scope 20261016i — READ-ONLY failure isolation (HEAD a7c1ecb)

Task: scope open fullgate-20261016h failure #13 `RGB canvas output changed
from pre-optimization baseline` (tests/race_cache_test.cpp:64). Suspect:
HUD commit 1b79351. Constraints: no tracked edits, no golden/blessing, no
claimed paths, no commits. Owned binaries only.

Baseline: `git rev-parse HEAD` = a7c1ecb. `git log --oneline 12f45ec..a7c1ecb
--stat` = docs-only (fullgate/nitro/skid handoffs); source identical to the
fullgate build baseline 12f45ec, so `build-agent-fullgate-20261016h/`
binaries (built 21:20 Oct 15, Debug) remain valid oracles for this analysis.
`python3 tools/agent_workspace.py list` checked; none of the files read here
are in another session's claim set touched by me (I wrote nothing but this
handoff). No builds configured; only read-only runs of owned binaries.

## 1. Failing row set (confirmed, not inferred)

Test row map (tests/race_cache_test.cpp:37): scale {1,2,4} x wide {0,1} x
rgb {0,1} = 12 rows, `row++` per combo. `rgb_baseline[]` (line 63, from
fdcd4aa) holds the 6 RGB rows; `original_scene[]` (line 87) holds the 6
no-HUD scene hashes. Indexed HUD-composited hashes are printed but NOT
asserted.

Owned-binary rerun (`./build-agent-fullgate-20261016h/race_cache-test`,
read-only, exit nonzero via ctest; direct-run output below, timings vary):

```text
row=0 scale=1 wide=0 rgb=0 hash=16908656509277595288
row=1 scale=1 wide=0 rgb=1 hash=2800544501961077726   (= rgb_baseline[0], PASS)
row=2 scale=1 wide=1 rgb=0 hash=10442994832978502141
row=3 scale=1 wide=1 rgb=1 hash=12480883420206857557  (= rgb_baseline[1], PASS)
row=4 scale=2 wide=0 rgb=0 hash=6403728731288650074   (indexed HUD-composited)
row=5 scale=2 wide=0 rgb=1 hash=11345352425216510781  (!= rgb_baseline[2]=8749086642151112002 -> THROW line 64)
RGB canvas output changed from pre-optimization baseline
```

Identical hashes in fullgate ctest log (/tmp/fullgate-20261016h-ctest.log
#13), bench-release run 1 (bench-release-20261016f.md §run 1), and this
rerun — stable across Debug/Release and reruns.

Verdict table:

- PASS (bit-identical to pre-1b79351): rows 0–3 (all s=1, indexed + RGB,
  both wide modes). RGB entries match `rgb_baseline[0..1]`.
- CONFIRMED CHANGED: row 4 indexed HUD-composited
  `2448064240350656025` (Debug @8851ef6 per bench-release handoff) ->
  `6403728731288650074` (this HEAD); row 5 RGB `8749086642151112002` ->
  `11345352425216510781` (this is the thrown failure).
- UNMEASURED (harness aborts at row 5, lines 56–62 note): rows 6–11
  (s=2 16:9, s=4 4:3/16:9). No Release or Debug numbers exist at this HEAD.
  Expected: RGB rows 7/9/11 diverge, indexed HUD rows 6/8/10 diverge the
  same way (same compose path); scene-only rows would pass (see §2).

## 2. Scene-vs-HUD isolation verdict: HUD-only, scene bit-identical

- The per-row no-HUD check (lines 83–88: strip `game.assets.hud`,
  redraw indexed, compare `original_scene[(row-1)/2]`) PASSED for rows 0–4:
  the harness reached row 5, so no `Original scene output changed` throw
  fired. Scene/terrain/actor/horizon path is pixel-identical through s=2.
- Corroboration: bench-release handoff records the `original_scene` check
  passing rows 0–4; `hires_render-test` passes in Release at this HEAD and
  `ctest -R 'hud|course'` 3/3 passes in the fullgate rerun; `hud-test`
  (owned binary) exits 0.
- Therefore the divergence enters through HUD/dashboard composition only,
  consistent with 1b79351 being the sole HUD-affecting commit in
  8851ef6..0141e7f (other commit 0141e7f is course bounds lookup).

## 3. Fidelity fix or regression? Matches documented 0x00446c30 parameters

HUD diff (1b79351, src/hud.cpp) has three parts; only one explains the hash:

1. Pen width (CAUSE): old `line(...,width=scale)` stamped a
   `(2*scale+1)`-wide centered block (s=1: 3-wide, s=2: 5-wide, s=4:
   9-wide). New `band = needle_pen(!low_).width * scale` with
   `needle_pen = {high?3:2, 0x721108}` (src/hud.cpp:60-65, 200-210)
   stamps 3/6/12-wide at s=1/2/4 in high-res. At s=1 the new band (3)
   equals the old block (3) -> rows 0–3 identical, exactly as observed.
   At s>=2 widths differ (5 vs 6, 9 vs 12) -> rows 4+ diverge, exactly as
   observed. Documented source: `CreatePen(PS_SOLID, HiRes+2, 0x81172)`
   at analysis/decompiled/00446c30.c:145, cached per HiRes slot; color
   0x081172 = R72 G11 B08 = native 0x721108. Direction is TOWARD the
   original parameter. Caveat (hud-needle-20261016d.md §7, kept in
   src/hud.cpp:10-15 comment): GDI band/cap SHAPE is still open — the
   native Bresenham-plus-block stamp is an approximation, so the new
   hashes are closer in width but not proven GDI-exact.
2. Needle order (NOT the cause): speed (DAT_0049e10c, layout+0x54) then
   rpm (DAT_0049e108, layout+0x40), matching 00446c30.c:149-176. Both
   needles share color 0x721108, so reorder is pixel-invisible today
   (fidelity-only, correct to keep).
3. Solo gap normalization (NOT the cause here): `nearest_opponent`
   returns `{}` with gap 0 when no target (matches 0x00438550 leaving
   DAT_004b8a30 null + 0x00446b80 gating name/meter/gap together), plus
   `state()` gates target block on name-row presence. race_cache snapshot
   (NAPA, 7 opponents, tick 300) has a live target, so this path is cold.

Conclusion: the change walks like an intended fidelity fix (width
parameter now matches the traced CreatePen call; s=1 output provably
unchanged), NOT a scene regression. But the new s>=2 raster is still an
unvalidated approximation of GDI stroking — re-blessing now would enshrine
an approximation as golden without the parked-needle/GDI reference the
needle handoff calls for.

## 4. Integrator decision needed (THEY run; I did not)

Do NOT bless silently. OMP/integrator picks one:

Option A — re-bless (accept 1b79351 as the new fidelity baseline):
```sh
cmake -S . -B build-agent-rebless --parallel  # or reuse an owned dir
cmake --build build-agent-rebless --target race_cache-test --parallel
./build-agent-rebless/race_cache-test 2>&1 | grep '^row='   # capture rows 0-5
# rows 6-11 are unreachable until the assert is lifted: temporarily comment
# the line-64 check in a SCRATCH checkout only, capture rows 6-11 hashes,
# then in the real tree update tests/race_cache_test.cpp rgb_baseline[]
# entries 2..5 to the new values. NEVER invent values; paste captured ones.
ctest --test-dir build-agent-rebless -R '^race-cache$' --output-on-failure
ctest --test-dir build-agent-rebless --output-on-failure  # full gate
```

Option B — revert (keep fdcd4aa goldens authoritative):
```sh
git revert 1b79351   # or partial revert of the pen-band hunk only, keeping order+gap fixes
cmake --build build-agent-rebless --parallel
ctest --test-dir build-agent-rebless -R '^race-cache$,^hud$' --output-on-failure
# then re-open the 0x00446c30 width task with a GDI-reference capture first
# (needle-trace §7), so the next attempt lands with proof, not new goldens.
```

Recommendation: prefer B-partial (keep order + gap-normalization fixes,
revisit width with a parked-needle capture/GDI reference) OR A only with
explicit OMP sign-off noting the shape approximation stays open. Either
way, indexed HUD-composited hashes need no golden edit (unasserted), and
`original_scene[]` must NOT be touched (scene path is clean).

Held resources: none. Files written: this handoff only. No commits.
