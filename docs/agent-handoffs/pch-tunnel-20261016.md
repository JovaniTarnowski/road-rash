# pch-tunnel-20261016

Task: Tunnel (type 3) sides and building-to-mountain transition

Baseline: 08f7e1fc3ec9a054eba618dfdf110fc50a4f3ee3

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-pch-tunnel-20261016`
Owner PID: 89660
Resource token: `256d6fb0-1298-4e6d-94f4-8554beff1208`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/course.hpp`
- `/Users/archit/Dev/rr/native-port/src/course.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.cpp`
- `/Users/archit/Dev/rr/native-port/src/course_scene.hpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.cpp`
- `/Users/archit/Dev/rr/native-port/src/terrain_panels.hpp`
- `/Users/archit/Dev/rr/native-port/tests/scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/course_scene_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/pch-tunnel-20261016.md`

## Progress / evidence

Follow-up to pch-terrain-20261016 (commit 08f7e1f), which listed "type 3
split sides" and the building neighbour as remaining.

Type 3 is a tunnel: `0x00454240` sets it for marking flag 0x40. Probe over
all courses: only on mountain sections (MEDLY s13 at 385/475/630; CANYN s0
385/475/628, s7 100, s14 385/560). The race reads `CourseScene::terrain()`
for simulation, so the tunnel flag is a separate render-only vector.

Original functions ported (decompiled exports plus disassembly where noted):

- `0x00454240` type 3: mountain side = right if its height >= 1 (left
  height then forced <= 0), mountain height >= 800, the open side's wall
  edge = 2·outer − road edge, shoulder widths halved, empty pair-B heights
  become 16.
- `0x00444630` type 3: +0x54/+0x5c curb rows, arch +0xb4..+0xe0 from
  `0x00465a54` = {256, 64, 512, 192, 512, 256, 256, 256}, +0xf4/+0xf8.
- `0x0043bef0`: shoulder skipped between two tunnel samples; arch when near
  depth > 0x3c and both ends are tunnels; portal when the near sub-slice's
  +0x18 neighbour (nearer, per `0x00454c80` links) is not a tunnel.
- `0x0040dac0` (six-panel state machine, values carried between panels),
  `0x0044fcd0` backdrop on the farthest strip (`0x004c0a5c` is the last strip
  in memory and `0x00443af0` draws it first), `0x0040ccf0`/`0x0044ada0`
  materials, `0x0040b9b0`, `0x0040bde0` tunnel sidewalks.
- `0x0040d420` portal; `0x0044f6b0`/`0x0044f6e0` fixed multiply,
  `0x0044f5a0` octant angle, `0x0044f770` sine (disassembly confirms the
  byte reads at `0x00476870` inside the word table).
- `0x00443d70`: camera sample type 3 skips both-tunnel wall/cliff sides.

Building neighbour: `0x00444630` case 0 leaves facade half-widths/heights at
+0x70..+0x8c; `0x0040dfd0`/`0x0040d030` read them as far base/top. Ported;
no route adjacency exists on the disc.

Evidence: original capture `pch-tunnel-20261016` (3600 frames, accepted,
git-ignored) vs native views and rr-game screenshots; tests in
`scene_test.cpp` and `course_scene_test.cpp`. See `docs/VERIFICATION.md`.

## Checks / remaining work

- Full build exit 0, ctest 99/99.
- Not done: simulation use of type 3 (`0x00439b10`), pixel/display-list
  comparison against the capture's scene dumps, guard-rail scenery at the
  CANYN portal (scenery, not terrain).
- No live processes; file claim released at commit.
