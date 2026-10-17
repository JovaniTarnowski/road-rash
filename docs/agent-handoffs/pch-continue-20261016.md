# pch-continue-20261016

Task: side streets, scenery parity, live terrain RNG, wall collision

Baseline: 562a15716d2d65ca98d7035e3ce0de96fba979f3

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-pch-continue-20261016`
Owner PID: 67828
Resource token: `0a234dc6-8688-436c-850f-00b1a8008a1c`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.hpp`
- `/Users/archit/Dev/rr/native-port/src/building_panels.cpp`
- `/Users/archit/Dev/rr/native-port/src/building_panels.hpp`
- `/Users/archit/Dev/rr/native-port/tests/scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/oracle_scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pch-continue-20261016.md`

## Progress / evidence

Follow-up of pch-terrain-parity (562a157). Commits: 782165b (side streets,
end faces, hash-exact comparator) and the walls commit.

1. Peninsula buildings: `0x0040ba10`, `0x0040bde0` flag-16 curb,
   `0x0040c450`, end-face/side-facade first cell (`0x0044a9b0`), flips
   dropped by `0x004504f0`. All terrain nodes match on every capture.
2. Walls/rails: `0x0043a610` transcription, `0x0040a6d5` bounds wreck,
   shared `wall_profile()`. Unit-checked on CANYN; no original wall-hit
   capture comparison yet (replays diverge earlier).
3. Live ring terrain: K=1137 / pre 4231 for CANYN from `ring-probe`;
   native ring jitter still differs; investigation notes in PROGRESS.
4. Scenery: fractions and placement stream (see PROGRESS); not changed.

## Checks / remaining work

- ctest 101/101; oracle-terrain and oracle-terrain-medly pass.
- Next: calibrate a CANYN SIM replay (start hold) to compare wall hits;
  find the setup draw positions for walker terrain (plan 10); give
  SceneryObject its position fraction and slice-build ground.
- Captures `canyn-sim-20261016`, `pch-tunnel-20261016` are local and
  git-ignored. No live processes.
