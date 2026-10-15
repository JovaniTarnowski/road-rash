# Timer distance-input trace: per-tick input to 0x0044dd90 (plan 10 Task 7 follow-up)

Date: 2026-10-16. Read-only RE trace in `/Users/archit/Dev/rr/native-port`.
Baseline: shared tree with untracked worker files present, untouched.
No source edits, no Wine runs, no build dirs touched, no REA call (disasm-verified instead, see §4).
No commit.

## 1. What DAT_004b7ef4 / DAT_004b7f08 are (disasm evidence)

Reader/writer census from `analysis/original-disassembly.txt`:

- **DAT_004b7ef4 = view-camera Q8 position.** Only two writers exist in the
  whole EXE: init `44b28e: movl $0x3100, 0x4b7ef4` (`0x0044b250` race setup)
  and `437dc5: movl %eax, 0x4b7ef4` inside `0x00437c30` (camera update).
  Readers all use it as the camera anchor: AI/car distance via `0x00449c20`
  (`408109`, `4407dd`, `44ef3b`), sound hearing-radius `|obj+0x1c - cam| <
  0x1e00` (`434dcb`), vehicle-sound attenuation (`435121`, `435181`),
  camera-speed derivation (`437c33`), render (`438451/484b4/48b`).
- **DAT_004b7f08 = private last-consumed-camera watermark of `0x0044b3b0`.**
  Only writers: init `44b2c4: movl $0x3100` (`0x0044b250`) and the three
  `0x0044b3b0` tail stores (`44b4f6`, `44b554`, `44b5ba`) — **every exit
  path sets `DAT_004b7f08 = uVar3`** (snapshot of `7ef4` taken at tick
  start, `44b423`). Only reader: `0x0044b3b0` itself. Nothing else in the
  EXE touches it.
- **DAT_004b7ef8** (same struct, `44b27c` zero-init) is the bend/rise sweep
  accumulator written by the `0x0044b3b0` tail block; read by camera/render
  code (`438451`). Not a timer input.

Update timing inside `0x0044b3b0` (verified instruction-by-instruction,
`44b3b0`–`44b5c3`):

1. `44b0e0`, `438170` scheduler passes run first.
2. `0x00437c30` (writes `7ef4`) runs **conditionally** at `44b417–44b419`:
   only if `*(*DAT_004642d8 + 0x2fc) + 0x10 == DAT_004b7ecc` (that
   scheduler object's last-run tick == current tick). If false, `7ef4` is
   stale that tick.
3. `44b423`: `uVar3 = 7ef4` snapshot; `44b437`: delta = `7ef4 - 7f08`.
4. `44b439–44b47f`: the ±0x100-split loop calls `FUN_00454120` per chunk
   (ring-cursor advance + per-slice builds). This loop consumes a **copy**
   in `%ebx`.
5. `44b483`: `%ebx` is **restored from `%esi` (= uVar3)**; `44b48a`:
   `ebx = uVar3 - 7f08` recomputed; `44b492`: `FUN_0044dd90(full delta)`.
6. Tail (`44b497–44b5c3`): gated `7ef8` accumulation, then `7f08 = uVar3`
   on all paths.

**Correction to prior handoff** (`traffic-timer-trace-20261016.md` §2):
`0x0044dd90` does **not** receive the ±0x100 residual — it receives the
**full-tick delta** `7ef4 - 7f08`. The split loop feeds only `0x00454120`.
There is **no sub-sample residual carryover into the timers**: `7f08`
resyncs to `7ef4` every tick on every path, so each tick's `param_1` is
exact and self-contained. (`0x00454120` with param 0 early-outs at
`454127`, so a zero-delta tick builds no slices but still runs the timers.)

`0x00437c30` formula (`437db5–437dc5`): `7ef4 = bike+0x138 -
DAT_004c3e04 - 0x100`, **no lower clamp** (straight-line store; the nearby
`0x96` clamp at `437d98–437da4` is camera height `7eec`, not position).
`DAT_004c3e04` is the crash back-off (+3/tick to `0x384`, `437d6a–437d79`).
`7ee4` (camera speed) = `(7ef4_new - 7ef4_old) << 8` (`437dca–437dd3`).

## 2. Native comparison (`src/race.cpp:2286-2316`, `src/traffic.cpp:494-556`)

Relative order matches the original: `update_camera()` → ring advance →
`traffic_->step` (slice builds before timer updates, `traffic.cpp:505`
then `514-551`) → `previous_camera_position_ = camera_position()`. The
native per-slice `build_slice` loop is equivalent to the original's
chunked `454120` calls (a single big `454120` call would build at most one
slice; chunking builds each crossed sample — native loops all of them).

Systematic differences found (all in the delta source, none in timer arithmetic):

1. **Conditional camera update (primary suspect).** Native `update_camera()`
   runs unconditionally every `Race::step`; the original's `437c30` is
   skipped whenever the `DAT_004642d8+0x2fc` object's `+0x10 !=
   DAT_004b7ecc`. On a skipped tick the original feeds `param_1 = 0`
   (`7ef4` stale, `7f08` already equal), so the behind timer does
   `-0 + 9 = +9` (holds/grows) while native with a nonzero camera advance
   decays it. Direction matches the observed fork (native behind-timer
   fires at demo tick 319, original silent). **Speculation:** ties to
   countdown/pre-green ticks — if `0x0044b3b0` runs before the player
   scheduler node launches, each such tick banks +9 into the behind state,
   phase-shifting every later expiry vs a native run that starts at green
   with `behind = 0`. Needs capture tick-alignment (`+0x10` identity and
   first-tick values), explicitly not derived here.
2. **Negative-camera clamp.** Native `camera_position()` floors at 0
   (`race.cpp:2172-2175`); the original stores the raw
   `bike+0x138 - back - 0x100` (can go negative near race start). Diverges
   only while `bike+0x138 - back < 0x100`. Not a tick-319 factor (both near
   `0x3100+` by then), noted for completeness.
3. **Back-off dynamics match:** native `camera_back_ += 3` to 900
   (`race.cpp:2160-2166`) == original `DAT_004c3e04` +3 to `0x384`;
   lift to 300 == `0x12c`. No delta there while uncrashed.
4. **Countdown parity:** native `countdown_step()` never touches camera or
   traffic. If the original's pre-green `0x0044b3b0` iterations call
   `0x0044dd90` with zero deltas, they still advance timer state (+9/tick
   on behind/ahead, -9/tick on oncoming) — same mechanism as (1).

Ring-front offsets used natively (`camera+0x100` ring camera,
`camera+0x5100` front, `traffic.hpp:130-134`) are consistent with setup
`0x3200` vs `0x3100` (`44b250`/`44b28e`) and `454120` advancing
`4b6e50/49f240/4b6e5c` together, but front = ring + `0x5000` was not
re-verified in disasm this run (prior static trace only).

## 3. Verdict: no expression change needed

The native expression `distance_moved = camera_position() -
previous_camera_position_`, with `previous_` synced **after** the traffic
step (`race.cpp:2313,2316`), already reproduces `DAT_004b7ef4 -
DAT_004b7f08` exactly — **given** `7f08_old == 7ef4_old` (guaranteed: all
`0x0044b3b0` paths resync) and **given** `0x00437c30` ran (the conditional
in §2.1). Do not change the delta formula, do not pass the residual, do
not carry anything across ticks. If the tick-319 fork is pursued, the
place to look is tick alignment / the `44b419` skip condition (and the
`DAT_004642d8+0x2fc` object identity), plus zone-density timing — not the
timer arithmetic, which checks out.

## 4. Tool record

No REA call made: the load-bearing facts (full-vs-residual at
`44b483/44b48a`, all-paths resync at `44b4f6/44b554/44b5ba`, conditional
camera at `44b417-44b419`, writer census) were verified directly against
`analysis/original-disassembly.txt` byte-level disassembly, which is
stronger than decompiler output for these claims. Limitation: Ghidra
pseudocode for `0x00437c30`'s lateral/height logic was not used; only the
`7ef4` store path (`437db5–437dc5`) was traced. EXE hash re-verified by
prior handoff (`73d7f974…a60f18a2`); not re-hashed here (read-only, no
reason to doubt).
