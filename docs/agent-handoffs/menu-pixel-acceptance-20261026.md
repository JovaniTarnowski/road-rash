# Handoff: all-screen menu pixel acceptance (plan 13 Task 6 Step 2 slice)

Task: close the gaps from `menu-text-art-gaps-20261026.md` (plan 13 Task 6
Step 2 remainder). The mechanical harness stays; art mapping + text fidelity
are now pinned for every screen the Wine frontend-acceptance run captures.

Baseline: `56807c6` (plus uncommitted frontend-acceptance worker files —
see "File claims" below). Owned build dir: `build-agent-menu-gaps`
(Debug, fresh configure, full gate 106/106).

## Changed files (mine — safe to commit with explicit paths)

- `src/frontend.hpp`: `set_rest_picture_for_test(n)` (public, test-only),
  `rest_picture_override_` member, `desc_fill_` member.
- `src/frontend.cpp`: override applied in `background_for` after the
  `next_random` draw (RNG call count preserved); `describe()` sets
  `desc_fill_ = (m.flags & 0x20f08) != 0`; both draw paths gate the
  description dark-box fill on it; cleared on `enter`/`set_focus`.
- `tests/frontend_test.cpp`: `masked_compare` takes exact/big floors;
  `masked_compare_items` takes the items vector + per-screen pins; new
  end-of-test block replays the oracle script on a fresh `Frontend`
  (main menu, Big Game, Cydney, direct `enter` to 0x10/0x11/0x12/0x13)
  and pins fe-characters/restroom/jukebox/controls/graphics.
- Docs: `docs/PROGRESS.md` (new Timeline entry, gap entry closed out),
  `docs/VERIFICATION.md` (acceptance section rewritten with new numbers),
  `docs/REVERSE_ENGINEERING.md` (desc-fill rule, background inheritance,
  label-slot sharing), `docs/plans/13-music-and-frontend-completion.md`
  (Task 6 Step 2 slice note; step stays open).

## What was green (not regressed)

- menu-09 60%/98%/0%, fe-main 60%/98%/0%, fe-street 67%/96%/0% — identical
  numbers after the fill change (their desc boxes sit in masked bands).
- Full gate `ctest --test-dir build-agent-menu-gaps`: 106/106 pass.

## Measured results (all in-test, goldens git-ignored, skip when absent)

| Screen | Masked / exact / big | Note |
| --- | --- | --- |
| characters | 67% / 82% / 13% | pin 0.70/0.15; residual is GDI glyph fringe on unmasked bio text (overlay verified, no structure left) |
| restroom | 69% / 84% / 4% | standard pins; Rest3 pinned |
| jukebox | 69% / 85% / 4% | standard pins; inherits Rest3 like the original |
| controls | 92% / 80% / 2% | cover_max 0.95 (item rows cover the screen) |
| graphics | 68% / 75% / 7% | pin 0.65/0.10; desc text + metric-derived box edge |

## Root causes closed (corrections to the previous handoff)

1. **Characters was NOT a portrait-variant mismatch.** The portrait was
   already right (`Chars/Axle-N` via focus image). The 55%-big block was the
   description dark-box fill: the original draws characters bios (flags
   `0x5`) as text directly on the photo. Static proof: `0x00420470`
   `test edi,8` gates the `0x41fb90` invalidate; focused-item caller
   `0x4208bd` passes raw flags; dynamic-desc caller `0x4208d0` forces
   `or ebx,0xc`. Fill iff `flags & 0x20f08`. Left-side photo matched at 0%
   throughout — the decoder was never the issue.
2. **Restroom/jukebox/graphics confirmed.** Override `Restroom/Rest3`
   (trace cache 1) + inheritance (no `0x0041d970` case for 0x11/0x13, both
   sides keep previous) verified by 4%/4% big — mapping now evidence-backed.
3. **Controls labels.** Golden label column shows stale character bios
   (slots 160-168 shared) because the script's direct `enter 18` bypasses
   the restroom-row handler that refreshes them. Real play always takes the
   handler path, so the port keeps refresh-on-enter; artifact pixels sit
   inside masked item rects. No behavior change made — deliberate.

## Suggested next steps (not started)

1. GDI-hinting text research (exact glyph match) + desc-box widths derived
   from it; then tighten the characters/graphics pins to the 0.12/0.05
   standard.
2. Original captures for the unpinned screens (course lists, loading,
   Olley's/showroom, Panzer, schmooze, results/standings/penalty, dialogs,
   pause, native pages) + same-mask pins.
3. Caller-3 (`0x4208d0`) dynamic descriptions are not ported as such
   (course length is, via `describe()`); the fill gate already accounts for
   them (`0x20f00`).

## File claims / coordination

- The frontend-acceptance worker (`frontend-acceptance-all-20261026.md`,
  still Pending) claims `src/frontend.cpp`, `src/frontend.hpp`,
  `src/text.*`, `tests/frontend_test.cpp` among others but has made NO
  edits to them (tree diff touches only `CMakeLists.txt`,
  `oracle/aweman_proxy.c`, `src/rri.cpp` + untracked harness files, all
  preserved untouched). This handoff's `src`/`tests` edits are the assigned
  gap-closure work; merge order with their future edits needs the
  integrator's eye on `frontend_test.cpp` (both add blocks at the end).
- Do NOT commit their files. Commit only the paths listed above.
- No live processes held. No Wine runs needed; analysis used only the
  existing goldens + static disassembly (capstone) of `0x00420470`,
  callers `0x4208bd`/`0x420a84`, and IAT resolution (imports `MoveWindow`,
  `DrawTextExA`, `SetTextColor`, `OffsetRect`, `InvalidateRect`).

## Checks

- `cmake -S . -B build-agent-menu-gaps -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build-agent-menu-gaps --parallel` → clean.
- `ctest --test-dir build-agent-menu-gaps --output-on-failure` →
  100% tests passed, 0 failed out of 106.
- `./build-agent-menu-gaps/frontend-test ../cdrom/ROADRASH` → 0 failures;
  eight masked lines as in the table above.
- ASan/UBSan tree NOT run (shared `build-agent-open-san` left alone);
  changes are branch/member-only, safe by inspection.
