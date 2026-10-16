# race-cache 4:3 RGB baseline mismatch — root cause (2026-10-15)

- Task: find why `tests/race_cache_test.cpp` fails "RGB canvas output changed
  from pre-optimization baseline" on the 4:3 RGB rows at scale 2 and 4, and
  decide fix vs re-record.
- Baseline commit: `bb41f2b` (main HEAD at session start). The main checkout
  also has the uncommitted widescreen-centre work
  (`widescreen-centre-20261015.md`); I did not touch it.
- Owned/changed files: this handoff; `tests/race_cache_test.cpp` (golden
  re-record after owner approval, see Outcome).

## Result

Root cause: commit `1b79351` ("hud: resolution pen width, needle order, solo
target gap normalization"), `src/hud.cpp:202`:

```cpp
const int pen_band = selected_pen.width*scale;   // 3*scale at high res
```

which feeds the rewritten `line()` at `src/hud.cpp:16-18` (band x band block,
`lo=-(band/2)`, `hi=(band-1)/2`). Before it, the needle lambda passed `scale`
as a half-width, so the block was `2*scale+1` wide.

| compose scale | old width (`2s+1`) | new width (`3s`) | race-cache 4:3 RGB |
|---|---|---|---|
| 1 | 3 | 3 | unchanged (entry [0] still matches) |
| 2 | 5 | 6 | entry [2] changes |
| 4 | 9 | 12 | entry [4] changes |

The test composes the full dashboard over the scene, so the needle width is in
the RGB hash. The indexed no-HUD `original_scene` hashes are unaffected, which
matches.

## Evidence (isolated worktrees, own build dirs, no stash)

Worktrees under `/Users/archit/Dev/rr/.race-cache-rgb-20261015/` (`a/` and
`b/`, each with a `cdrom` symlink so `RR_SOURCE_DIR/../cdrom` resolves), build
dir `build-rcrgb`, target `race_cache-test`, Debug. Logs: `a-test.log`,
`b-test.log`, `b2-test.log` there.

1. `a` = `8851ef6` (parent of 1b79351): race_cache-test exit 0; row 5 hash
   `8749086642151112002`, row 9 hash `637450489077128212` (the baselines).
2. `b` = `1b79351`: exit 1; row 5 hash `11345352425216510781`, identical to
   the failure reported in gate 20261016h and in the current tree.
3. `b` with only line 202 changed to `const int pen_band = 2*scale+1;`: exit 0;
   all six RGB rows match the fdcd4aa values again (rows 5/9 as in step 1). So
   the band width alone explains the whole change; the needle order swap and
   gap normalisation in that commit are pixel-neutral here. The probe edit was
   reverted (`git checkout src/hud.cpp` in worktree `b` only).

The only other src commits since `fdcd4aa` (`60bae61`, `109c640`, `b4d3c0c`,
`0141e7f`) lie outside this pair: steps 1 and 3 show the full scene matches
without the band change.

## Classification: intended change, not a regression

- `1b79351` is a deliberate fidelity change, documented in
  `docs/agent-handoffs/hud-needle-20261016d.md` §1: the original selects
  `CreatePen(PS_SOLID, HiRes+2, 0x81172)` in `0x00446c30` (3 px at 640x480).
  Scaling that 3 px pen by the compose scale gives `3*scale`, which is what an
  integer upscale of the 640x480 dashboard shows; the old `2*scale+1` was a
  native artefact that only matched at scale 1.
- That session ran only `ctest -R "^(hud|frontend|text|framebuffer)$"`, not
  `race-cache`, so the golden drift went unnoticed.
- Still open (unchanged): the exact GDI band/cap shape (needle-trace §7).

So I am **not** fixing anything in code. Reverting line 202 would bring back
the scale-dependent width error.

## Proposal (owner approved 2026-10-15; applied, see Outcome)

Re-record the two 4:3 entries in `rgb_baseline` in
`tests/race_cache_test.cpp`:

- `[2]` (scale 2, 4:3): `8749086642151112002` -> `11345352425216510781`
- `[4]` (scale 4, 4:3): `637450489077128212` -> `9967728013922361189`

Then update the comment above the array to say that the 4:3 scale 2/4 entries
were re-recorded after `1b79351` (needle pen `3*scale`). Note also: the
owner-approved widescreen re-record (entries 1/3/5) was taken in a tree that
already contained `1b79351`, so those values already include the new needle
width (at `8851ef6` the wide scale-2/4 hashes were `1477636935681336033` /
`13490494933351736534`, before the centring fix too). The same comment should
mention that.

## Outcome

The owner approved the re-record ("re-record rgb_baseline[2]=11345352425216510781
and [4]=9967728013922361189 after 1b79351, and update the comment"). By then the
widescreen work had been committed (`cb44f8b`), so the file was clean; I edited
only the array and its comment.

Checks (own build dir `build-agent-race-cache-rgb-20261015`, Debug, logs inside it):

- `race_cache-test` exit 0; all six RGB rows match the array.
- Full `cmake --build` exit 0; `ctest --test-dir build-agent-race-cache-rgb-20261015
  --output-on-failure` exit 0: 100% passed, 0 failed out of 99.
- Caveat: the shared tree had another session's uncommitted edits during the
  gate (`src/hud.cpp`, `src/hud.hpp`, `src/race_caches.hpp`,
  `src/race_helpers.cpp`, `tests/hud_test.cpp`). The 99/99 result covers the
  tree with those edits included; they are not mine and are not in my commit.
  The race-cache hashes equal the values measured at `1b79351` in isolation.

## Held resources

- Worktrees `/Users/archit/Dev/rr/.race-cache-rgb-20261015/{a,b}/native-port`
  (detached at `8851ef6` / `1b79351`, clean). No processes running. Remove with
  `git worktree remove` once the evidence is no longer needed.
