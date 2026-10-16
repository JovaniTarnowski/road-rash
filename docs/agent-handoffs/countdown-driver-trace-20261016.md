# Countdown/hold driver trace: 0x0044b3b0 DOES run during the hold (plan 10 Task 7)

Date: 2026-10-16. Read-only RE trace in `/Users/archit/Dev/rr/native-port`.
Baseline: shared tree with untracked worker files present, untouched.
No source edits, no Wine runs, no build dirs touched, no REA calls
(byte-level `analysis/original-disassembly.txt` evidence throughout).
No commit. Read-only python3 used on gitignored golden only.

## 0. Verdict: STEPS-RUN-SILENTLY (binary question decided)

During every pre-green hold tick, `0x0044b3b0` executes and unconditionally
calls `0x0044dd90` (traffic timers) with `param_1 = 0`. Each such tick banks
**+9 into behind (`7e94`), +9 into rear (`7e80`), −9 into oncoming (`7e90`)**,
with no ring-cursor motion, no slice builds, no RNG draws. After the 59 hold
ticks (1–59) the behind state is **531 = 59 × 9** (init 0), vs 0 in a native
run whose `countdown_step()` never touches traffic. Direction matches the
tick-319 fork (native behind-timer fires at 319, original silent: original
starts 531 high, needs 531 more distance to drive the state `< 0`).

Zero-delta window is actually ticks **1–71** (cursors frozen through 71,
first motion at 72), so by first motion behind = 639. The "green at 60" =
think/speed launch (speed 0 → 100 at tick 60); camera delta stays 0 through
tick 71. See §5.

## 1. Countdown ownership: two layers, only one is tick-visible

**Layer A — menu/frame layer (`0x00442e10`, countdown `DAT_004753c0`).**
Init `0x00442dd0` (called once at race setup `4163e7`, inside `0x004163d0`
*after* `0x00442990` zeroes tick `DAT_004b7ecc` at `442a1e`): solo value
**180** (`442dd0`–`442e05`: `0x168 + (solo ? -0xB4 : 0)`), netplay 360.
Per-frame driver `0x00442e10` (called from the main loop at `416560` with
`%edi` = frame delta: 1 if `DAT_004c5cd1 != 0` else clamped frame-ms 1..20):

- `442f02–442f09`: if `4753c0 <= 0` at entry → fall to `442f3b` → call
  `0x0044b2e0(%edi)` (the ONLY per-frame path to the sim; see §2).
- While `4753c0 > 0`: `4753c0 -= %edi`, store; if still `> 0` return with
  **no sim at all** (`442f14 jg 442f42`); on zero-crossing play the GO
  block (`442f16–442f2c`, same `0x4b8490`+`0x100` beep call as race
  setup `416414`/`41642f`) and zero it (`442f2f`), still no sim that frame.
- `DAT_004753b4` (checked at `442ed1`/`442f45`, returned to the main loop;
  nonzero exits the race at `416565`) is the **exit-race flag**, not green:
  set at `442e49/442e7e/442ef9/416072/430e57/431535`, cleared only at
  `442ddd` init. Irrelevant during the hold (it is 0).

Visible digit logic at `4472fa–447313`: displays `4753c0 / 60 + 1`
(format string `0x466654`) — i.e. 4,3,2,1 over the 180 units. The `/60`
strongly suggests units are 1/60 s with `%edi = 1`/frame (the `4c5cd1`
path): a 180-frame ≈ 3 s 3-2-1-GO. (Inference: `%edi` value in-capture not
directly observed; timing cross-check below supports edi=1: 180 countdown
frames + 900 one-tick frames ≈ 1080 frames in the 26.5 s capture ≈ 41 fps,
plausible for a Wine capture. If `%edi` were frame-ms (~25–30), the
countdown would be ~6–7 frames — inconsistent with the /60 digit staging.
Labeled inference, not load-bearing: either way layer A produces no ticks.)

**Layer B — in-sim zero-delta hold (the 59 ticks).** Once layer A opens,
`0x0044b2e0` runs every frame and `0x0044b3b0` runs every tick, but with
camera delta 0 (§3–§4). The TSV tick stream PROVES layer A expired before
tick 3 (ticks are contiguous 1–900 = `DAT_004b7ecc`, incremented once per
`0x0044b2e0` iteration at `44b30c–44b31e` before the single hooked
`44b324` call site): ticks 1–2 are the two setup warm-up `0x0044b2e0(1)`
calls (`4163f5/416401`, after `442dd0` set 180 — hence controls=0 there,
before the first frame poll; ticks 3+ show controls=5). Layer-A frames
between tick 2 and tick 3 dumped nothing (hook only fires inside the sim).

## 2. Call-path census: every hold tick executes 0x0044b3b0 → 0x0044dd90

- `0x0044b3b0` has exactly ONE call site in the EXE: `44b324`
  (`grep 44b3b0`: single `calll 0x44b3b0` at line 103665).
- `0x0044b2e0` has exactly THREE call sites: warm-ups `4163f5`, `416401`
  (`ecx = 1`, one iteration each) and the per-frame `442f3d`
  (`ecx = %edi`). Each iteration increments `DAT_004b7ecc` once, then hits
  `44b324` exactly once (loop `44b30c–44b397` contains one call).
- The oracle proxy replaces the `44b324` call with `hook_step`
  (`oracle/aweman_proxy.c:2014`), which calls `0x0044b3b0` itself then dumps
  with `tick = RD32(0x004b7ecc)`. So **each TSV tick block ⟺ one
  `0x0044b3b0` execution**, and ticks 1–59 contiguous ⟺ `0x0044b3b0` ran
  59 times pre-green. "Does not run at all" is refuted by the golden alone.
- Inside `0x0044b3b0`, the `0x0044dd90` call (`44b492`) is unconditional:
  `44b481 mov ecx,ebx → 44b483 mov ebx,esi → 44b485 call 454120 →
  44b48a sub ebx,[7f08] → 44b490 mov ecx,ebx → 44b492 call 44dd90` is
  straight-line code; the first conditional after it is `44b497 test`.
  No early `ret` exists between entry and the tail (only rets: `44b506`,
  `44b570`, `44b5c3`). Scheduler passes `44b0e0`/`438170` (`44b3f7/fc`)
  are likewise unconditional.

## 3. param_1 = 0 during the hold (proof, not assumption)

- `param_1 = DAT_004b7ef4 − DAT_004b7f08` full-tick delta (`44b423` snapshot
  `uVar3`, `44b48a` recompute; prior handoff `timer-delta-trace-20261016`
  §1 verified `7f08` resyncs to `uVar3` on ALL exit paths
  `44b4f6/44b554/44b5ba`). So delta = 0 ⟺ camera word `7ef4` unchanged
  since last tick.
- `7ef4` writers (census, unchanged from prior handoff): init `44b28e`
  (`0x3100`) and camera update `0x00437c30` (`437dc5`), itself conditional
  at `44b417–44b419` on `[*DAT_004642d8 + 0x2fc] + 0x10 == DAT_004b7ecc`.
- Independent proof delta = 0 for ticks 1–71: ring cursors frozen
  (front 33280/back 512/cam 12800). `0x00454120` advances all three cursors
  1:1 by its param (`45413f–454145`: `edx = esi+front, ecx = esi+back,
  ebx += esi`) and early-outs on param 0 (`454125 test; 454127 je 454237`,
  which just pops and returns). `0x0044b3b0` emits only nonzero chunk
  params plus one residual call; frozen cursors ⟺ all params were 0 ⟺
  delta = 0 (nonzero delta cannot net to zero short of ≥32768-unit ring
  wraparound in one tick — impossible). First cursor motion at tick 72
  (+1/+1/+1) confirms the 1:1 advance.
- Whether `0x00437c30` was *skipped* (gate closed) or *ran with static
  output* (bike static) during 1–59 is undetermined from the TSV
  (`7ef4`/`7f08` not dumped; gate object `[player+0x2fc]+0x10` is the
  rider-half word, not dumped — `rider.0.bike_10` is the bike-half node's
  own last-run, which ticks every tick, interval 1). Does not affect the
  timer verdict: delta = 0 is proven by the cursors either way.

## 4. Exact per-tick timer math at param_1 = 0 (disasm-verified)

`0x0044dd90` (`44dd93–44ddab`, `44de18–44de37`, branch C likewise):
biased-`>>8` scaling `(x + ((x>>31)&0xff)) >> 8` (`cltd; and $0xff; sar $8`).

- Solo inits via `0x0044db80` (verified `44dbc3–44dbe3`): `44dd00(-25)` →
  `7e84 = 7ea4 = 2500`; `44dd00(6)` → `7e88 = 7e90 = 7e9c = 3515`;
  then `7e94 = 0` (`44dbd2`), `7e80 = −7e9c` (`44dbe1–44dbe3`).
  Writer census over all `4b7e8x/4b7e9x` stores: only `44db80`, `44dd00`,
  `44dd90` write them (other hits are byte reads of `7e8c` at
  `441563/44e782` and unrelated word `7e98` at `44e151/44f47f`).
- Per zero-delta tick: `scaled(2500) = 2500>>8 = 9` (positive, no bias).
  - A oncoming: `7e90 −= 9` → 3515 → 2984 after 59 ticks.
  - B behind: `7e94 −= 0; 7e94 += 9` → **0 → 531 after 59 ticks**
    (no-fire clamp at `2*7e9c = 7030` never engaged).
  - C rear: `7e80 −= 0; 7e80 += 9` → −3515 → −2984 after 59 ticks.
- No fires possible during the hold (thresholds need 391+ ticks at ±9),
  and all three `441750` RNG calls sit strictly inside fire branches
  (`44dde4/44de6c/44df04`, each guarded by `jns`/`jle`-not-taken +
  char lane gate). Observed `rng_calls = 0` and constant RNG
  (`1133891140`) for all frozen ticks — consistent, and independently
  proves no fire/spawn occurred.
- The `0x0044b3b0` tail takes the `44b497 je → 44b5b9` direct-resync path
  when `ebx = 0` (skips the `7ef8` slope accumulation) — timers still
  updated before the branch. Zero side effects besides §4.

## 5. Launch staging observed (golden `sched900-20261015.tsv`, seed 1, course 4)

- Ticks 1–59: rider state frozen (pos 12800, speed 0), cursors frozen, RNG
  constant. Player node (think `0x407a90` = 4225680) is nonetheless
  *scheduled every tick* (`rider.0.bike_10` last = tick−…​= tick, interval 1,
  next = tick+1; drone ring nodes run every 4 ticks: lasts …53,57,61).
  So the hold is enforced by pre-green gating *inside* the think/camera
  path with zero camera delta — not by skipping the sim.
- Tick 60: speed 0 → 100 (think unblocks; "green" in tick space), but
  position/cursors/RNG still frozen → delta still 0 → tick 60's `44dd90`
  also banks +9 (behind = 540 after tick 60).
- Ticks 60–71: speed ramps (98…262), velocity still 0, no motion.
- Tick 72: velocity 27, position/cursors advance (+1) — first nonzero delta;
  `44dd90` resumes distance-driven updates from behind = 639.

## 6. What this decides for Task 7, and what is still missing

- Native `Race::countdown_step()` (`src/race.cpp:2197–2216`) only bumps
  `ticks_` and steps frozen schedules/think — never camera, rings, or
  traffic. To match the original, native must apply **59 (hold) + 12
  (60–71) zero-delta `0x0044dd90` timer-only steps**: behind +9/tick from 0
  (531 at tick 60, 639 at first motion), rear/oncoming ∓ from ∓3515 —
  with no cursor motion, no slice builds, no RNG. (Whether to model
  ticks 60–71 identically is a replay-alignment detail; the 59-tick 531
  offset is the task's stated quantity.)
- Do NOT change the delta formula (prior handoff §3 stands): with
  `7f08` resynced every tick, `param_1 = 7ef4 − 7f08_prev` is exact.
- Missing evidence (labeled, needs future work, no action taken):
  1. Identity of the tick-60 think-unblock trigger (which green flag
     `0x00407a90`/`0x00434660` gates on). Player think runs every tick, so
     the gate is inside the think, not the scheduler. Candidate hunt in
     think-side disasm not done.
  2. `%edi` (layer-A decrement/frame) in-capture: inferred 1 from the
     `/60` digit staging + total-time fit (§1), not directly observed.
  3. `7ef4/7f08/7e94` are not oracle-dumped; the 531 is derived
     (init + scale + zero-delta count + writer census + no-fire proof),
     not read out. A future proxy field (`traffic.behind` etc.) could
     confirm directly; expressly not added here (read-only task).

## 7. Tool record

`rg`-family greps + `sed` reads of `analysis/original-disassembly.txt`
only; `oracle/aweman_proxy.c` (hook/step/dump logic) and
`analysis/oracle/sched900-20261015.tsv` (+ receipt) via read-only python3.
EXE hash in golden header (`73d7f974…a60f18a2`) matches
`analysis/inventory.json`; not re-hashed. No builds, no Wine, no source
touches, no commits.
