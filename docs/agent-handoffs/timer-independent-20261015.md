# Independent countdown timer review — plan 10 Task 7

Date: 2026-10-15 (client time). Baseline `5524bf6`. Read-only source review;
owned outputs only: this handoff and `analysis/timer-independent-20261015/`.
Other workers owned modified `src/traffic.cpp`, `src/traffic.hpp`; preserved.
No source edits, builds, Wine, commits, shared Ghidra writes, or held resources.

## Evidence and verdict

Hash checked directly before REA: original `../cdrom/ROADRASH/ROADRASH.EXE`
SHA256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`,
541184 bytes, matches `analysis/inventory.json`.

Pinned `npx --yes rea-agents@6.4.0 function
../cdrom/ROADRASH/ROADRASH.EXE 0x0044b3b0 --format md` exited 0, using
Node24.13.0/Ghidra12.1.4/JDK21 environment from `docs/TOOLING.md`.
Full output and stderr saved to `analysis/timer-independent-20261015/`.
REA pseudocode is whitespace-normalized identical to
`analysis/decompiled/0044b3b0.c` (independent import, not runtime proof).

Original assembly `44b481–44b492` restores the full camera snapshot before
subtracting `7f08` and calling `44dd90`. The call is unconditional, including
delta zero; the zero-delta branch occurs only afterward at `44b497–44b499`.
Do not pass the ring residual or accumulate residual distance across ticks.

Countdown-driver handoff's 59 hold ticks require 59 timer updates; the
subsequent 12 zero-motion race ticks already go through normal Race::step.
Do not manually bank those 12 again. Expected behind is 531 before tick60,
639 before tick72 first motion. These are derived states, not captured timer
words; current golden omits timer words.

## Recommended integration and hazards

1. Add timer-only hold entrypoint and call it once per countdown_step.
   Initialize traffic ring slice events before its first timer call, without
   moving cars or thinking intersection controllers. Original `44b250` calls
   `44db80` then `4552e0` initial ring fill before either warm-up tick.
   Native deferred fill must preserve this ordering. Fill span is
   `[front-0x8000,front)` with front=view-camera+0x5100.
2. Preserve initialization versus zone-update semantics: init6 gives
   oncoming/reload3515, behind0, ahead=-3515. Initial density40 zone calls
   `traffic_set_density(40)`, resetting oncoming and both reloads to23437,
   preserving behind/ahead. `configure(40)` would wrongly restart ahead at
   -23437. At59 hold ticks density40 state is oncoming22906/behind531/
   ahead=-2984 (assuming no later density event).
3. Keep attached solo traffic with spawn disabled, matching original null
   car pool: expiry lane draw survives, placement/type draws do not.
4. Oracle hydrated frame0 is post tick1. Existing brain-window replay skips
   countdown_step for tick1, so seed exactly one timer-only tick without
   rethinking the hydrated brain or scheduler; otherwise 58 timer ticks
   produce behind522. Non-hydrated replay already executes tick1.
5. Human solo replay currently bypasses the demo countdown loop altogether.
   It also needs hold timer history before the green anchor, independently
   of whether demo AI countdown is replayed.
6. Legacy captures lack the newer scheduler coverage. Retaining their
   documented diagnostic signature via explicit modern-coverage gating is
   reasonable, but document missing legacy timer fidelity; green suites do
   not prove human simulation acceptance. Modern hydrated tick coverage
   should remain the primary Task7 diagnostic.

No evidence justifies changing camera delta arithmetic. Original camera
update is scheduler-gated; remaining later delta parity requires captured
camera words or rider-half schedule. Density events and lane taps remain
separate acceptance concerns; no golden should be changed to bless them.

## Tool and validation record

Used rg/fd and byte disassembly. No client LSP tool exposed in this worker.
Fallback `python3 tools/agent_lsp.py --check src/race.cpp` used the existing
read-only compile database: healthy181 entries, no missing sources/stale
config. Exit3: clangd reports38 ExtractFunction tweak failures
(`Cannot extract break/continue without corresponding loop/switch statement`).
This is not a successful clean LSP gate. Full JSON saved as
`analysis/timer-independent-20261015/clangd-race.json`; no compilation or
database refresh attempted because build directories are not owned here.

Parent received recommended patch and off-by-one/human-path findings during
review and owns implementation, tests, shared status and commits.

## Follow-up: tick575 brain target mismatch is an identity-comparison defect

Parent's integrated timer replay moved first mismatch from376 to575; log
`/tmp/rr-countdown-demo.log` says `brain_168 original rider -1 native13`.
Read-only golden parsing shows the original target is **not null**:

- Tick570,574,575,576 all carry `rider.0.brain_168 = 20976340`.
- Frame0 `nodeaddr.14 = 20976340`, `.type = 4`, `.position = 21248`.
- Tick575 player position21140; tick57421111; RNG matches per parent's
  diagnostic report through574 and the575 error is pointer comparison.

`tests/oracle_sim_test.cpp` golden_rider currently translates the immutable
frame0 address to frame0 position, then searches **current** native rider
positions on every comparison. Once target13 moves away from21248 the
same valid original pointer maps to-1, falsely printed as a missing rider.
Current node identity and current position must not be conflated. This
does not establish whether native target motion itself matches the original;
the golden does not supply every frozen actor's current position each tick.

Recommended precise fix: build address->native rider index once, after
`apply_frozen_state` and before hydrated brain pointers are assigned. Map
type4 bikes by initial position; linked type8 halves may map to the same
owner if supported by the pointer field's semantics. Keep zero->-1,
unknown address->-2, and reject/flag ambiguous initial matches rather than
guess. golden_rider thereafter uses this stable map. Preserve original
positions separately for gpos/latched-position diagnostics, where position
semantics are intentional. A focused synthetic test should move a matched
native rider and confirm the address still resolves to its original index.

No new source edits, REA, builds or Wine for this follow-up. Parent received
the pointer and exact frame0 mapping during review.

Additional identity audit: sched900 frame0 has15 type4 bike nodes with15
unique positions, including exactly one21248 target and player
`probe.player=20986896` at12800. `apply_frozen_state` validates unique native
frozen positions and one-to-one linked type8 halves; hydration changes no
`+0x1c` positions. Initial21248 therefore resolves reliably to the same
native13 it resolved to through574. Build mapping once and enforce these
uniqueness checks, rather than relying on late position uniqueness.

Correction/qualification to earlier gpos advice: keeping frame0 positions
is only valid as explicitly labeled initial-position metadata. The trace's
target/ahead/behind pairs show native CURRENT position versus golden INITIAL
position, misleading after movement. The actual brain180 acceptance check
also risks a false mismatch: original field is an actor pointer while native
`ai_overtake` stores the object's scan-time position. If the object moved
before latching, its scan-time position differs from frame0. Native already
stores `overtake_latched` rider identity and `overtake_type`, so rider latches
can compare stable owner+type identity. Pedestrian/car nodes require their
own stable identity mapping; retain explicit coverage gaps for unsupported
types rather than silently blessing position mismatches. Position comparison
requires contemporaneous original position evidence that this golden lacks
for most frozen nodes.
