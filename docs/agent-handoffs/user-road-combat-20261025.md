# user-road-combat-20261025

Task: Investigate intersection holes and player combat with real assets

Baseline: a4572e0453896b22924740568643a34c4d1586e1

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-user-road-combat-20261025`
Owner PID: 95735
Resource token: `e304e32d-d8cb-41e9-a407-452cf6845466`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/src/building_panels.cpp`
- `/Users/archit/Dev/rr/native-port/tests/building_panels_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/race_test.cpp`

## Findings and changes

User report: missing intersection road and inability to hit rivals, on native
and web. Reviewed the owner's 40.41-second City recording from October 24,
including 1 Hz overview, 6 Hz intersection samples at seconds 20–24 and full
frames at 15/21/25/30/38. The recording shows punch/kick animations. No exact
City missing-road frame has yet been isolated; requested a timestamp. Do not
claim this report fully resolved by the rural renderer fix below.

1. Confirmed combat failure with original descriptor assets: held normal club
   attacks on either side did not damage an in-range target over 90 live
   scheduler ticks (2 failures in the initial 18-case matrix). `fight` reset
   at the legacy 18-tick cap, restarting before the club hit frame. Original
   `0x00434660` calls `0x00409510` to gate attack animation completion, not an
   18-tick timer. Native descriptor-backed player attacks now use that existing
   predicate; the legacy cap remains for callers without codes/bank. AI attack
   selection is unchanged. Real-asset regression expands to 36 combinations:
   tap/hold × fists/club/chain × left/right × punch/kick/backhand. These tests
   cover damage against a controlled in-range opponent under live scheduling,
   not full original combat/RNG parity or physical device input.
2. Confirmed missing type-4 intersection branch in `CourseScene`: original
   `0x0043bef0` calls shoulder `0x0041d260`, then cross-road `0x0041d110` for
   marking flags 8/16. Native incorrectly emitted ordinary hill bands. Added
   the sixteen outward 256-world-unit tiles from both shoulders, category-5
   frame-2 pavement selected once, original horizontal visibility tests
   (`0x00443990`), signed 32-bit wrapping and Hor+ view edges. Retained depth
   metadata for enhanced rendering. Original `0x0043b940` always returns 16
   for terrain type 4. Checked field loads, scale shifts, side negation and
   material selection against `analysis/original-disassembly.txt` at
   0x0041d110–0x0041d25e. This restores a missing rural intersection renderer;
   it is not a measured City pixel-parity fix.

Reference disc EXE SHA-256 matched `analysis/inventory.json`:
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
No patched Android binary, new Wine capture, Ghidra mutation or RNG used for
rendering. Local visual evidence/probes: `analysis/user-road-combat/` (ignored,
disc-derived, do not commit). Imported NAPA/CITY/MEDLY scene previews were
inspected; synthetic tile tests check both sides at widths 320/426, seamless
adjacency, viewport coverage, material, depths and 16-tile cap.

## Files / concurrency

Changed `src/race.cpp` (fight hunks only), `src/course_scene.cpp`,
`src/terrain_panels.{cpp,hpp}`, `tests/race_test.cpp`, `tests/scene_test.cpp`.
Other OMP crash-physics edits appeared in `race.cpp` during this session;
leave them untouched and stage only this session's two fight hunks.
`building_panels.cpp` and its tests were investigated, not changed.
Additional claim receipts: `user-combat-fix-20261025.md` and
`user-road-panels-20261025.md`. All work uses the primary handoff's build dir;
the other claim-generated build dirs were not used.

## Verification

- Configure and full Debug build: passed in `build-agent-user-road-combat-20261025`.
- Focused CTest `^(race|scene|building_panels|course_scene)$`: 4/4 pass,
  including the all-course/all-sample scene-construction sweep.
- Full 106-test CTest: running, receipt `/tmp/rr-user-ctest.log`.
- Emscripten Release browser build: passed; validated nine-file, asset-free
  bundle in `build-agent-user-road-combat-web/dist`; `/tmp/rr-user-web.log`.
- Browser smoke: running, `/tmp/rr-user-browser.log`.
- C++ LSP channel unavailable in this client. Owned compile database generated.
  Homebrew clangd check of the added terrain helper: zero errors. Apple clangd
  tooling helper used the shared database and hit ExtractFunction tweak failures;
  this was a tooling feature-check failure, not a compiler diagnostic. Actual
  C++ compilation succeeds.
- No new original combat/intersection capture, sanitizers, Android/device test,
  or public deployment observed. Existing original-list renderer and simulation
  replays are part of full CTest; they do not establish parity of these newly
  exercised scenarios. City gap remains open pending reproduction/timestamp.

## Live processes / resources

Private builds above. Temporary localhost browser server at port 8879; stop
before handoff. No held Wine prefix, capture output or Ghidra resources.
