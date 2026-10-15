# surface-fix

Task: surface_class margin fix

Baseline: 6ecbca990f33939c31e0ae55be61b46d3199c870

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-surface-fix`
Owner PID: 1
Resource token: `4a43ac1e-5e2b-43a0-ab6d-e31b6e98fe78`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/bike.cpp,tests/bike_test.cpp`

## Progress / evidence

Plan 16 Task 4 Step 1 trace input (`offroad-trace-20261015.md` §1.5):
`surface_class` (`src/bike.cpp`) returned inverted 0x30-margin classes
for non-type-0 terrain vs `0x0043a0c0` (decompile lines 33/41,
`(cond)-1 & 2`: within margin → 0, beyond → 2; all other branches
verified identical incl. strictness). Swapped both ternary arms; added
8 `bike_test` margin cases (inner/edge/outer × both sides, type 1).

- `bike-test`: 0 failures.
- Both golden replays byte-identical to `sim_known_divergences.json`
  (demo frame 221 lateral, human frame 209 rng) — on-road neutral.
- Full `ctest` in `build-agent-surface-fix`: RUNNING at handoff time;
  commit only on green.

## Checks / remaining work

- [x] Fix + synthetic boundary tests (no golden needed — static inversion).
- [ ] Full gate green → commit `src/bike.cpp tests/bike_test.cpp` + this
  handoff. Shared docs (PROGRESS/VERIFICATION/REVERSE) are claimed by
  the live SIM worker — doc updates owed to them, recorded here instead:
  retire the "Not ported yet" shading/terrain reading if desired; the
  `walls()` stand-in and finish-clamp divergences (§5 of the trace) stay
  open for the port owner.

## Live processes / resources

File claims above. Record capture/Ghidra processes here.
