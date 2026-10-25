# Level-5 continuation audit — October 18, 2026

Read-only source/evidence audit requested by the main integrator. Baseline
`f06a0d4a5daf6649ab678a9a545e1c4884d3fffb`; only this handoff was written.
No capture, build, claim reaping, source edit, commit, or push was performed.
The previous `sim-l5-20261018` handoff is incomplete and does not describe the
substantial subsequent working-tree implementation. PID 60062 was already
confirmed dead by the integrator; this audit did not change its resources.

## Current measured prefix and first failure

No process using `build-agent-sim-l5-20261018` was present before execution.
The existing binary (mtime October 18 19:07:22) is newer than the audited
`race.cpp` and `race_schedule.cpp`. Commands run from the repository:

```sh
build-agent-sim-l5-20261018/oracle-sim-test analysis/oracle/napa-field-l5-20261018.tsv
env RR_FIELD_ALL=1 build-agent-sim-l5-20261018/oracle-sim-test analysis/oracle/napa-field-l5-20261018.tsv
```

Both exit 1. Both report five equal native countdown ticks, green at tick 6,
`driver=demo`, single-anchor replay, and countdown closure on the green anchor.
Frames 0–711 (ticks 1–712) compare equal. First failure is frame 712/tick 713:

```text
first divergence at frame 712 (tick 713): rng original 463413313 native 3370523862
```

The original RNG state is one MSVC draw after the native state. `RR_FIELD_ALL`
identifies the affected original slot as rider 6, native rider 3:

```text
rng mismatch original 463413313 native 3370523862
  also rider.6.raw_1a0 original 4996752 native 0
  also rider.6.raw_1f8 original 1536396 native 1536389
  also rider.6.anim original 28 native 36
  also rider.6.anim_queued original 28 native 31
  also rider.6.anim_deadline original -1 native 743
first divergence at frame 712 (tick 713): rider.6.raw_190 original 16777216 native 0 (native rider 3)
```

This localizes continuation to the crashed rider half's animation/body state
at tick 713. It does **not** establish the root cause. Inspect its pre-think
body speed, half cadence, crash phase and animation at `0x00407f30`, plus
`0x0043ae90`, using the existing address-keyed exports before REA spot checks.
`advance_crash_rider_animation` selects animation 36 below speed 2500 and draws
in its 2500–7000 branch; the native/original transition evidence warrants
checking this input and scheduler ordering, without changing RNG artificially.
Existing `/tmp/l5-all.log` matches this fresh result. Older `/tmp/l5-split*.log`
record superseded failures at tick 629; `/tmp/l5_full.log` stops at tick 561.

## Capture identity and stale reports

Current canonical trace SHA-256, independently checked with `shasum -a 256`:
`882d66b003428a6ca5834b3627e4cfbd0641d6b2e0c751ea3da23829967e69aa`.
The receipt records 2400 contiguous ticks, complete=true, process exit 0,
course 4, display level 5 (trace level 4), seed 1, demo controls, and original
EXE SHA `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
Immutable artifact:
`analysis/oracle/napa-field-l5-20261018.captures/d303295ddc7047479fff365dc47d1f72/napa-field-l5-20261018.tsv`.

`build-agent-sim-l5-20261018/oracle-field-l5-diagnostic.json` describes the old
SHA `ebc0482e...` and pre-green one-draw failure. It is stale for the current
trace and binary. The current `docs/VERIFICATION.md` opening L5 failure note
is also historical. Neither disproves the measured prefix above. The separate
60-frame `l5-randlog-20261018` capture cannot diagnose tick 713 by itself.

## Working implementation to preserve and review

- Proxy display-level setter `0x004453f0`, extended brain schema through
  `+0x1cc`, optional old-schema handling, and CMake L5 diagnostic registration.
- Deferred field countdown RNG closure, including the pedestrian tick-1
  re-trigger and lazy pedestrian resource load after the simulation tick.
- Pedestrian node interleaving by position in the live scheduler.
- Separate crash half think/body motion and riderless bike animation/motion;
  half mirror and standings refresh; crashed-bike speed convergence when its
  half separates by more than `0x500`.
- Bike contact class routing for grounded crashed bikes, crash animation
  mirroring, and route curvature accumulation for generic bodies.

These are substantial behavior changes; they need focused L1/human and crash
regressions, a fresh owned build and the integration gate before committing.
No acceptance completion should be claimed for L5 while tick 713 fails.

## Temporary diagnostics and cleanup before integration

- `src/track_ring.cpp`: global `tmp_trace_on`, `tmp_trace`, and setup toggles.
  `src/terrain.cpp` and `src/race_pedestrians.cpp` reference this global only
  for `RR_SETUP_TRACE`. Remove the whole temporary dependency together.
- `src/race.cpp`: `RR_CRASH_TRACE` blocks in `collide`, `crash_entry`,
  `crash_bike_motion`, `crash_body_motion`; unused `crash_trace_head` and its
  `race.hpp` declaration; `RR_SETUP_TRACE` blocks in `build_draws`.
- `src/race_schedule.cpp`: `RR_CRASH_TRACE` nudge output.
- `src/course_scene.cpp`: `RR_PED_LOOK` resource mask logging.
- `tests/field_replay.hpp`: explicit TEMP-DIAG slot logging (frames 655–685),
  `RR_FIELD_BRAINS` prints and hard-coded native rider 10 countdown tracing.
  `race_pedestrians.cpp` and `race.cpp` also contain `RR_FIELD_BRAINS` logs;
  compare against HEAD before removing because at least the overtake log is
  pre-existing rather than this continuation's change.
- Some temporary printf calls pass `Bike` field proxy objects into varargs
  without `int32_t` conversions. `/tmp/l5-split3.log` shows addresses printed
  as state values; `/tmp/l5-split4.log` fixed that particular block. Audit any
  retained diagnostics rather than treating every old trace value as evidence.
- Update stale file-header limitations in `race_schedule.cpp` and
  `race_pedestrians.cpp` after the corresponding implementation is verified.
  `crash_recovery`'s comment says after body move, while `think_half` calls
  it before `run_half_node` performs the body move; reconcile from original.
- `closure_post` is assigned but not read by `FieldReplay`. Review the
  nonzero `ring.post_draws` case before generalizing the deferred closure.
- The new crash bike branch explicitly leaves `0x0043c7e0` unported; preserve
  that gap in documentation rather than claiming the entire branch complete.

Safe next action: integrator explicitly adopts the dead owner's source files,
uses a private build, confirms baseline L1/human regressions, then investigates
the tick-713 half animation transition with original evidence. Do not retune
goldens, add an RNG compensation draw, or conflate this demo capture with
human-control acceptance. Shared oracle/Ghidra mutations still need serialized
ownership; this audit holds no live processes or resources.

## Follow-up: tick-713 missing static collision identified

Further read-only analysis resolves the missing draw. The original's
`rider.6.raw_1a0 = 4996752` is exactly `0x004c3e90`, the shared static collision
object. `raw_190 = 0x01000000` is its mass copied by `0x00436a40` (both the
address-keyed export and native `Race::separate` write partner mass at +400).
This is evidence of a static collision on the **crashed rider half**, rather
than an animation threshold error in `0x00407f30`.

The original chain, from existing address-keyed exports and assembly:

1. `0x004368f0` visits every object whose last tick is current and whose
   flags satisfy `flags & 6 == 6`; it is not restricted to type-4 bikes.
   It calls `0x004373f0` if `abs((position & 255)-128) < half_length`.
2. `0x004373f0` checks BORR then DZHR static boxes; first overlap invokes
   `0x00437660` (via `0x00437750` for DZHR).
3. `0x00437660` fills the static object at `0x004c3e90`, stores that address
   to half `+0x1a0`, separates with `0x00436a40`, then invokes the half's
   callback at `+0x1a4`, namely `0x004046b0`.
4. `0x004046b0` excludes type-2 marking objects. For a crash-state-1 half
   against a static type-1 object it applies vertical/longitudinal response;
   at `0x4047c7` it sets animation 28 and `0x4047d3` queues 28 unless current
   animation is 33 or 29. This explains the original's immediate animation
   28 / queued 28 / deadline -1 despite the native's animation 36.
5. If resulting vertical velocity is below 3000, it nudges laterally using
   the **owner bike's dt** (not half dt), then at `0x404905` draws rand,
   `0x404911` performs unsigned division by 33, `0x404913` subtracts 16,
   and `0x404918` calls `0x00439270` with sweep=1.
6. Tail clears impulses/flags, clamps speed to [-2000,30000] and vertical
   velocity to [-10000,10000], then, while crashed, calls `thunk_00407410`
   to recompute finish distance from the half.

Exact numeric closure: previous state `3370523862` advances to `463413313`;
its CRT return is `(463413313 >> 16) & 32767 = 7071`. Therefore the displacement
is `7071 % 33 - 16 = -7`. The original's finish distance `1536396` is exactly
native `1536389 + 7`. The draw, displacement, state fields, and animation
differences all agree on this one missed callback.

Native `Race::collide_riders` explicitly filters the outer nodes with
`rider_or_ped`, allowing only rider type 4 and pedestrians. Its `bike_of`
always returns the owner's bike for rider-backed objects. The crash body
already sets `u8(0xc0) |= 3` in crash entry and is therefore eligible once
runtime scheduler flag 4 is included; the uncrashed half has neither role.
Native `collide_hazards` also returns immediately for crashed bikes. Do not
fix this by changing animation thresholds or adding a compensating RNG draw.

### Proposed narrow implementation and checks (not applied)

- Add crashed halves to collision-walk object resolution, using
  `crash_actor.motion`, its route/sample, flags, and `half_sched_last`.
  Preserve the position-sorted walk and same-frame static sweep order.
- Reuse the box overlap and static construction logic with actual object
  type 8, then invoke a faithful `0x004046b0` response with access to the
  owning rider, rather than `contact_static`'s bike `0x004049c0` response.
  `Race::separate` already accepts explicit types; `hit_static` currently
  hardcodes type 4 and bike callback and must not be reused unchanged.
- Keep the original half-state storage observable: synchronize collision
  words +0x190/+0x1a0 (and other callback words) into `half_object`, animation
  into `ride_animation`, body position into `half_mirror`, and finish distance
  after the callback. A split representation must not discard original data.
- Synthetic callback tests should cover state-1/type-1 static response,
  animation 36 -> 28, one deterministic RNG draw and -7 move, owner-bike dt
  for lateral nudge, type-2 early-out, phase-2 slow-object branch, and the
  velocity guard suppressing jitter. Check signed divisions and clamping.
- Integration: fresh L5 replay must pass tick 713 and report its next actual
  divergence; run original L1/schema2/human captures plus crash/race/schedule
  focused tests, then full build/test gate. This fix alone does not establish
  full 2400-tick L5 acceptance or all half-versus-bike/car contacts.

REA spot-check attempted with the documented Node 24/Ghidra/JDK environment:

```sh
env PATH=/Users/archit/.nvm/versions/node/v24.13.0/bin:/opt/homebrew/bin:/usr/bin:/bin GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00407f30 --format md --filter-output raw_result.value.pseudocode
```

It exited 1 and returned only an empty markdown table header; no usable
pseudocode was produced. No shared Ghidra or Wine state was touched. The
integrator reported host load 83 and requested no additional REA runs until
worker builds finish, so no retry was made. The diagnosis above relies on
the existing exports, authoritative `analysis/original-disassembly.txt`, and
the measured trace arithmetic. Temporary FieldRef varargs traces remain
unreliable and were not used for the decisive evidence.
