# pch-terrain-parity-20261016

Task: Tunnel physics, guard rails, native-vs-original terrain node comparison

Baseline: 15110b5101076a8041d3d200ee3a6dcfe0b626b3

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-pch-terrain-parity-20261016`
Owner PID: 67828
Resource token: `3b10d2c2-bfa7-4234-be8d-d5f792ae936e`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course.hpp`
- `/Users/archit/Dev/rr/native-port/src/course.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.hpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.cpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.hpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_ground.cpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_ground.hpp`
- `/Users/archit/Dev/rr/native-port/src/scenery.cpp`
- `/Users/archit/Dev/rr/native-port/src/scenery.hpp`
- `/Users/archit/Dev/rr/native-port/src/race.cpp`
- `/Users/archit/Dev/rr/native-port/tests/scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/course_scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/oracle_scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/terrain_ground_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pch-terrain-parity-20261016.md`

## Progress / evidence

Follow-up to commits 08f7e1f and 15110b5 (mountain sides, tunnels). The user
asked to fix: no tunnel physics, no pixel-level comparison, missing guard
rails.

1. Comparison method: scene dumps of `pch-tunnel-20261016` (and older
   captures) hold the projected strips (`0x004c0a5c` is the farthest of 32;
   strip +8 sample, +0x14 record; record +0xc sub-slice) and the display
   list tail. Loading each strip's sub-slice wall/hill values into
   `CourseScene` (new `set_terrain`) and building native nodes for the same
   camera gives exact node equality for all terrain; see VERIFICATION.
2. Findings fixed on the way: mountain occlusion row (`0x00444630`
   param_3[7]); lit tunnel set never used (dump evidence + 'CLGP' at chunk
   start in native `read_cel_group`).
3. Guard rail `0x0041c200` (category 6, CESR event 5).
4. Physics: type 3 sub-slices, `0x00439b10` cases 1 and 3.

Files: `src/course_scene.cpp/.hpp`, `src/terrain_ground.cpp/.hpp`,
`src/race.cpp` (offset argument), `src/family.cpp/.hpp` (`row_shift`),
`tests/oracle_scene_test.cpp`, `tests/terrain_ground_test.cpp`,
`CMakeLists.txt` (oracle-terrain), docs.

## Checks / remaining work

- ctest 100/100; oracle-terrain passes on the tunnel capture.
- Open: live-RNG terrain in CourseScene (plan 10), `0x0043a610` wall/rail
  collision (plan 11), scenery billboard parity, `0x0041c200`'s node-kind-4
  branch (road edge instead of shoulder; not reached on the captures),
  MEDLY building curbs/facades with off-centre camera (pre-existing).
- No live processes; claims released at commit.
