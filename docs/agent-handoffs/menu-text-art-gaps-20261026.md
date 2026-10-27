# Handoff: menu text + background/portrait art gaps (pixel acceptance remainder)

Task: close plan 13 Task 6 Step 2 (all-screen pixel acceptance). The
mechanical harness is done and green; what remains is art mapping and text
fidelity owned by the frontend/text modules.

Baseline: `f4f39fd` (main `0873fce`..`f4f39fd` pushed; tree otherwise clean
except the frontend-acceptance worker's uncommitted files — coordinate, see
"File claims" below).

## What is green (do not regress)

- `tests/frontend_test.cpp`: `masked_compare` + `masked_compare_items`
  (item-rect-driven mask: title banner, bio box, y>=370 button band, every
  item rect dilated 3px) with per-check `cover_max`.
- `menu-09` (60% masked, 98% exact, 0% large-diff), `fe-main` (60%/98%/0%),
  `fe-street` vs `fe-career-start.bmp` (67%/96%/0%). All skip 77 when the
  git-ignored golden is absent.

## Goldens (other worker's Wine runs, git-ignored, untracked)

- `analysis/frontend/acceptance-20261026/1f1e79684e344a44af2e1e99dc1b9509/`:
  `fe-{main,characters,street,restroom,jukebox,controls,graphics}.bmp` +
  `.state` (152-byte game-state dumps). All 640x480.
- `analysis/frontend/acceptance-20261026/34069bf3262241b3bdf4efa59c500751/`:
  `fe-career-{start,outcome,after-race,before-save,saved}.bmp`, `.state`,
  `fe-career.RRS` (already wired into `career-test --save-corpus`).
- Trace/script/receipt in each run dir give the exact navigation + focus.

## Measured gaps (all reproduced in-test, then reverted to keep the gate green)

| Screen | Unmasked exact / big | Analysis |
| --- | --- | --- |
| characters | 31% / 55% | Diff block x~280-640, y~140-360: portrait/bio picture area. Layout fits; the artwork itself differs. |
| restroom | ~1% / ~82% | Background photo vs native fill. Structural, not chrome. |
| jukebox | ~1% / ~80% | Same: nearly everything differs. |
| graphics | ~1% / ~77% | Same. |
| controls | — | No native snap exists at all (`fe-controls.bmp` has no counterpart). |

## Root causes found (static read, not yet fixed)

1. **Random restroom backgrounds.** `Frontend::background_for`
   (`src/frontend.cpp:419-426`): `rest_picture_ = rand()%6+1` per entry and
   `case 0x10: return previous in 0x11..0x13 ? "" :
   numbered("Restroom/Rest", rest_picture_)`. The golden captured one unknown
   variant, so no fixed golden can match without variant control. Same
   inheritance problem for jukebox (0x11) and graphics (0x13): neither
   switch in `background_for` has a case for them, so they keep the previous
   screen's (random) background — mapping vs the original unverified.
2. **GDI text shapes.** `src/text.cpp` renders the disc Badloc/Futura files
   through stb_truetype with a coverage>=50%-is-ink rule
   (`src/text.hpp:37-39`); exact GDI hinting is documented as not
   reproduced. This is why all text/chrome is masked. Exact glyph match is
   a separate research job (GDI rasterizer or hinting tables), not a mask
   tweak.
3. **Characters portraits.** Which `Chars/<file>-F` variant the entry shows
   vs what the test navigates to was not reconciled; the divergent block is
   the right-side picture, not the item list.

## Suggested next steps (in order)

1. Add a test-only deterministic background override (or read back
   `rest_picture_`/`background_name_`) so restroom/jukebox/graphics goldens
   compare the same variant; re-measure with `masked_compare_items`.
2. Reconcile the characters portrait variant, then pin with the item mask.
3. Snap the controls screen in the scripted flow and pin it the same way.
4. GDI-exact text stays open; do not weaken masks to force green.

## File claims / coordination

- `tests/frontend_test.cpp` edited by this session (pinned checks + helper);
  the frontend-acceptance worker also claims it
  (`docs/agent-handoffs/frontend-acceptance-all-20261026.md`, still Pending)
  plus `src/frontend.cpp`, `src/frontend.hpp`, `src/rri.cpp`, `src/text.*`,
  `oracle/aweman_proxy.c`, `tools/frontend_acceptance.py` (all uncommitted
  in their worktree). Do not reformat or move their goldens; new checks
  skip cleanly when goldens are absent.
- No live processes held. No Wine runs needed for the next step (analysis
  only); new captures only if a background variant is unrecoverable.

## Checks

- `./build-agent-open/frontend-test ../cdrom/ROADRASH` → 0 failures
  (menu-09, fe-main, fe-street lines print masked/exact/big).
- ASan/UBSan tree `build-agent-open-san`: same binary green.
