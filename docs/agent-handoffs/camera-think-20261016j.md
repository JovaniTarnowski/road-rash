# camera-think-20261016j — Plan 11 background: camera think/update order

Date: 2026-10-16. Read-only research in `/Users/archit/Dev/rr/native-port`.
Baseline: HEAD `45ae3ed` (verified via `git rev-parse`; shared tree has other
workers' untracked files, untouched). No commits.

Task: Plan 11 background — camera think/update order: `0x00437c30` relative to
the bike step, `0x0040a600` auto-walk interplay, camera height 150 + Q8
position law. Spot-check only: no builds, no captures.

## Owned / changed files (only these)

- `docs/agent-handoffs/camera-think-20261016j.md` (this file, new/untracked)
- `analysis/camera-20261016j/` (new/untracked scratch dir):
  - `rea-437d80-cameraheight.md` (+ `.log`) — REA success
  - `rea-38450-framecamera.md` (+ `.log`) — REA success

## Tool record (REA budget: 2 calls, both used)

Pinned env per `docs/TOOLING.md`:
`PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`npx --yes rea-agents@6.4.0 function … --format md`.
EXE `../cdrom/ROADRASH/ROADRASH.EXE` sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(reported inside both REA files; re-hashed on disk this run — match).

| Request | evidence_id | Result |
| --- | --- | --- |
| `0x00437d80` | `ev_f4d4b46fe2a26a7c0b52daa8318a4befe91c862abfaf6ac92e6bd3f5e0cf0d32` | REA returned the `FUN_00437c30` body (norm 1390 chars), whitespace-identical to `analysis/decompiled/00437c30.c`. `0x00437d80` is the small param-taking tail variant (bike ptr `+0x24`/`+0x138`); REA's procedure DB resolved the request to the containing `37c30` body. Third independent confirmation of `37c30` (prior: Oct-12 REA 1842 raw chars). |
| `0x00438450` | `ev_9c59d5254f8f246c340f168760216bfbbbc4495037b10042f58be0dea35fec83` | Pseudocode norm 1115 chars, whitespace-identical to `analysis/decompiled/00438450.c` (frame-end camera publish + scans). |

No further REA calls made (budget respected). Disassembly spot-checks below
are against `analysis/original-disassembly.txt`.

## Recovered law (disasm-verified this run, `437c30`–`437dd3`)

- Entry saves old position: `uVar2 = DAT_004b7ef4` (`437c33`).
- Split-sample centre blend (`437c66–437cf8`): only when bike type byte
  `== 2` and subtype in `{2,5}` (negated lane word) / `{4,6}` (`+0x14`-based);
  if `|pos| < 0x10`, blend toward the ring sample centre via the `0x0049e820`
  table (`>>4` weighted step). Single-road native port (`race.cpp:2162-2169`)
  covers only the non-split path — expected gap, already known.
- Lateral follow (`437cfb–437d3f`): `d = (bike+0x28 >> 8, rounded) − centre`;
  `off = d − (d>>2, rounded)`; asymmetric clamp: if `d−off >= 0xc9` (201)
  `off = d−200`; elif `d−off < −200` `off = d+200`. Native matches verbatim
  (`race.cpp:2166-2169`).
- Crash ramps (`437d3f–437d79`): when `bike+0xc0 & 0x10` or `DAT_004753c4`:
  `DAT_004c3e00` (lift) `+1` to 300 (`0x12c`), `DAT_004c3e04` (pull-back) `+3`
  to 900 (`0x384`). Native ramp-up matches (`race.cpp:2170-2172`).
  OPEN QUESTION for the porter: the `37c30` body has **no decay branch** —
  when neither condition holds it falls through to `437d7e` unchanged, while
  native decays (`lift −1`, `back −3`). Decay may live in another function
  (possibly the `0x00437d80` path or a per-frame caller). Flagged, not resolved.
- Height (`437d80–437da4`): `DAT_004b7eec = bike+0x24 + lift`; floor
  `0x96` = 150 (`cmpl $0x96; jg keep; else mov 0x96`). Native
  `if (height < 0x97) height = 0x96` (`race.cpp:2179`) is integer-equivalent.
  Race init height 150 (`road.hpp:19`, `0x0044b250`).
- Position Q8 (`437db5–437dc5`): `DAT_004b7ef4 = bike+0x138 − backoff − 0x100`
  (one sample behind the bike), **no lower clamp** — can go negative near race
  start. Native `camera_position()` floors at 0 (`race.cpp:2182-2185`):
  known pre-existing divergence (harmless after the first samples; also noted
  in the timer-delta handoff).
- Camera speed (`437dca–437dd3`): `DAT_004b7ee4 = (new − old) << 8`.
- `0x00437d80` (export `00437d80.c`, 19 lines): same height-floor +
  position-store tail but driven by an explicit bike pointer
  (`*(param_1+0x24)`, `*(param_1+0x138)`); no separate native port.
- `0x00438450`: per-frame (not per-tick) publish — averages `7ef8>>2`,
  copies 10 dwords `0x4b7ee0 → 0x4bc7d0` (camera to render state), then HUD /
  opponent-gap scans (`0x00438550`). Called once per frame at `44b39d`.
- `0x0040a600` auto-walk (not re-traced; REA-confirmed Oct 12, 4422 chars):
  states flying/sliding → on-foot → remount; walk speed 1200 (`0x0040a9d0`);
  no-keys-held auto-walks to the bike and zeroes the 1200-tick timer, held
  accel/brake/left/right bits (`DAT_004b7ed0 & 0x7800`) steer manually;
  proximity box `|Δlong| ≤ 9`, `|Δlat−6000| < 6001`; remount via `0x00404560`.
  Camera keeps following `bike+0x138` while the rider is on foot.

## Update order (per-step driver `0x0044b3b0`, disasm `44b3b0`–`44b5c3`)

```
frame loop 0x00442e10 → per-tick 0x0044b2e0: tick++ (44b317-44b319)
└─ 0x0044b3b0 per step:
   1. alternating timers 0x00475610/0x00475614
   2. call 0x0044b0e0 (queue drain)
   3. call 0x00438170(0) object scheduler  ← rider thinks + bike step run HERE
        (player 0x00434660 / AI 0x00407bd0 / run-back 0x0040a600 states /
         move 0x00406890, via scheduler dispatch — hence no direct call sites)
   4. GATED call 0x00437c30 at 44b417–44b419:
        only if [DAT_004642d8+0x2fc]+0x10 == DAT_004b7ecc (player slice stamp == tick)
        → stale 7ef4 on skipped ticks (countdown/pre-green suspect, per timer handoff)
   5. uVar3 = 7ef4 snapshot (44b423); delta = 7ef4 − 7f08
   6. chunked 0x00454120 ring-cursor advance per ±0x100 (builds slices)
   7. recompute ebx = uVar3 − 7f08; call 0x0044dd90(full delta) traffic timers
   8. tail: gated 7ef8 accumulation; 7f08 = uVar3 on ALL paths (resync)
└─ post-tick, once per frame: call 0x00438450 (44b39d)
```

So: **bike step (scheduler) → camera `37c30` → ring `454120` → traffic
timers `44dd90` → frame-end publish `38450`**. The camera always reads
post-step bike fields (`+0x28` lateral, `+0x24` height, `+0x138` position).

Native `Race::step` (`race.cpp:2288-2332`) matches this relative order:
`step_rider*` → `step_frozen_schedule` → pedestrians/collisions/animations →
`update_camera()` → heading/ring advance → `traffic_->step` →
`previous_camera_position_ = camera_position()`. Deltas vs original: (a) native
camera runs unconditionally (original gated — primary tick-319 suspect, owned
by the timer trace); (b) native `camera_position()` floor at 0 (original
unclamped); (c) single-road lateral only; (d) possible lift/backoff decay
placement (open question above).

## Capture spec (NOT run — for the porter)

Proxy hook per tick: `DAT_004b7ecc` (tick), scheduler stamp
`[DAT_004642d8+0x2fc]+0x10`, bike `+0x138/+0x28/+0x24/+0xc0`,
`DAT_004b7ee8/7eec/7ef4/7ee4`, `DAT_004c3e00/3e04`, `DAT_004b7f08/7ef8`.

- `napa-camera-gate`: countdown through green, solo. Verifies on which ticks
  `37c30` is skipped (stamp ≠ tick) and that `44dd90` receives 0-delta there.
- `napa-camera-follow`: weave laterally at speed; checks the `d − d/4`,
  `+200/−200` clamp and `+0x138 − back − 0x100` law plus speed `<<8`.
- `napa-camera-crash`: crash at speed; records lift/backoff ramps AND where
  decay happens (settles the open question).
- `napa-walk-auto`: crash, release all keys (auto-walk at 1200 to bike) vs
  hold each of accel/brake/left/right (steered walk); camera must track
  `bike+0x138` throughout; remount tick recorded.
- Acceptance: `oracle-sim` zero divergence on the camera words; pixel check on
  horizon/road rows for one follow + one crash frame.

## NOT-changed list (explicit)

Tracked files: none touched (verified: `git status` shows no modified
tracked files). Specifically NOT changed: `docs/PROGRESS.md`,
`docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`, `docs/plans/10*`,
`docs/plans/32*`, `src/race.cpp`, `src/traffic.*`, `src/terrain.*`,
`src/race_setup.*`, oracle proxy sources, `CMakeLists.txt`,
`oracle_sim_test` / `traffic_test` / `race_setup_test`. No build dirs
created or built; no captures run; no `.log` outside the scratch dir.

## Checks / remaining work

- Checks performed: `git rev-parse` (HEAD `45ae3ed`), EXE hash match, 2 REA
  `--format md` calls (both success, logs kept), whitespace-normalised
  pseudocode comparison vs `analysis/decompiled/` (both TRUE), disasm reads
  of `44b3b0` driver + `437c30` body, native reads (`race.cpp:2161-2190`,
  `2288-2332`; `road.hpp:19`; `view_state.*`; prior `camerathink-rea-20261012`
  + `timer-delta-trace-20261016` handoffs; RE §§105/165/646/648).
- Remaining for porter: settle lift/backoff decay location; port gated-camera
  behaviour + split-sample blend; run the four captures above; `oracle-sim`
  acceptance.
- Live processes / resources: none (no REA/Ghidra/Wine/build processes left
  running; no locks held).
