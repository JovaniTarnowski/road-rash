# Plan 32 — completion review follow-through

Written October 14, 2026 against `6788592`. Companion:
[review and completion estimate](../agent-review/REVIEW-2026-10-14-COMPLETION.md).
This consolidates execution order; it does not replace plans 10–24 or reopen
completed review plans. No implementation is claimed complete here.

## Ownership and order

The active SIM integrator owns oracle files, replay helpers, CMake and shared
status documents. Continue that slice without interruption. Claims at review
time are listed by `python3 tools/agent_workspace.py list`; recheck before edits.
Use an owned build directory, keep new handoffs unique and serialize shared
registration/status edits. Read current `AGENTS.md` and plan 00 first.

### A. Fix screen regressions (R1, R4)

Files: `src/race_screen.*`, focused new screen integration test; coordinate
CMake registration with the integrator.

- [x] Consume restart once. Reset clock/history and any screen result/countdown
  bookkeeping required by restart; avoid holding stale state across rebuilds.
- [x] Handle renderer resets before pause-menu routing, clear pause texture and
  all affected cached textures, and check upload/render failures.
- [x] Add regressions exercising the screen across multiple iterations, not
  only the pure `race_action` translator: finish → restart → advance; reset
  while active; reset while pause menu is open; resume without elapsed-time jump.
- [x] Run focused screen/session/action tests, native integration gate and
  a bounded SDL smoke flow. Run ASan/UBSan on the new lifecycle tests.

Acceptance: one restart request produces one rebuilt race, countdown advances,
and renderer recovery displays a usable pause menu. Device acceptance remains
separate until actually observed.

### B. Correct presentation interpolation (R2, R3)

Files: `src/view_state.*`, `src/race_screen.cpp`,
`tests/view_state_test.cpp`, new screen harness. Serialize with A.

- [x] Use explicit shortest signed modular deltas for wrapped positions;
  widen signed operands before subtracting. Define heading period separately.
- [x] Capture adjacent simulation-tick snapshots even during catch-up;
  initialize both after discontinuities. Use the supplied frame timestamp
  consistently so synthetic timing tests can control screen progression.
- [x] Cover backward movement and wrap in both directions, alpha endpoints,
  signed extremes, heading wrap, 0/1/2/4-tick frames and post-reset first frames.
- [x] Verify Original mode output and simulation hashes remain unchanged;
  run focused UBSan and frame pacing checks at 30/60/120 rendered FPS.

Acceptance: the reproduced 1256 → 1000 midpoint is 1128, no UBSan arithmetic
failure, adjacent snapshots are exactly one simulation tick apart, and the
same controls yield identical race state with interpolation on or off.

### C. Finish the current simulation critical path (plans 10, 16)

This is the active agent's work. Do not prescribe a competing guessed fix.

- [ ] Capture scheduler metadata `+0c/+10/+14/+18`, including frozen nodes
  with NULL callbacks; port exact scheduling/cadence and compare the first
  divergence using immutable fresh capture receipts.
- [ ] Prove actual player-control branch invocation and owner identity, beyond
  a sampled demo flag. Use the existing human-branch handoff design.
- [ ] Reach strict single-anchor human replay without reanchoring, frozen dt
  hardcoding, altered goldens or an expanded known-divergence allowlist.
- [ ] Add braking/steering/crash/contact scenarios, then field replay and the
  remaining AI fight/overtake/contact work in plans 11/16. Track setup RNG
  separately from the allowed once-at-green anchor.

Acceptance: `make verify-sim` passes required original scenarios with validated
human proof, no skips and zero unexplained differences. Moving the first
divergence later is progress, not final acceptance.

### D. Complete live race systems (plans 11, 16, 17, 24)

- [ ] Wire police spawn/pursuit/actors/contact/arrest into the live factory and
  race result path. Test that a real arrest reaches career penalties and the
  busted cutscene. Helper-only `police_test` is insufficient.
- [ ] Replace `Race::walls` with the recovered behavior; finish crash/on-foot
  traffic and police-owner recovery semantics. Preserve original integer/RNG
  behavior with scenario captures first.
- [ ] Recover course/level start and finish bounds; remove route-A-end fallback
  from accepted configurations. Enumerate splits/branches explicitly.
- [ ] Accept five courses × five levels with bounded drives, both-route sweeps,
  required level-1/5 original field traces and natural-frame comparisons.

Acceptance: each matrix row names implementation, exact original comparison,
fixture tests and playtest evidence. All 25 races must finish at their original
bounds; live police and on-foot behavior cannot be represented by offline tests.

### E. Measure and improve complete-frame performance (R5; plans 19–21, 29)

- [x] Repair full-width dashboard side panels in both indexed/RGB paths.
- [ ] Benchmark complete scene + actors + dashboard + horizon + upload at
  640×480, 2× and 4×, 4:3/16:9, Original/Enhanced. Record hardware, build,
  warm/cold cache, p50/p95/p99, peak memory and allocations/frame.
- [x] Reuse horizon storage; avoid unused indexed dashboard work on RGB;
  cache static layers only after measuring. Keep output comparisons exact
  where Original mode requires it and compare threaded/single-threaded output.
- [ ] Move first-enable upscale attachment into cancellable loading with safe
  frame-boundary publication; retain a predictable Original fallback.

Acceptance: no stale/black margin columns, no per-frame horizon allocation,
no first-enable multi-second blocked event loop, and documented measured
improvement without changed simulation hashes. Device budgets come from F.

October 15 continuation: the screen harness now covers actual finish/restart,
pause/resume, injected renderer resets, adjacent catch-up snapshots and
30/60/120-FPS simulation/pixel invariance. It exposed and fixed a lost menu
Resume event as well as mixed SDL/supplied timestamp resets. Focused native
and ASan/UBSan checks passed; full integration evidence is recorded in
[the integrator handoff](../agent-handoffs/codex-plan32-20261015.md) and
[screen handoff](../agent-handoffs/codex-screen-20261015.md).
The canvas restores the indexed dashboard after the horizon/scene, skips
indexed dashboard mapping in RGB, and retains horizon scratch storage.
Original no-HUD scene and RGB frame hashes remain exact; indexed HUD output
intentionally repairs the erased dashboard. Whole-frame benchmark details
and remaining E scope are in the
[performance handoff](../agent-handoffs/codex-performance-20261015.md). C/D simulation and
device acceptance remain open; software reset events do not prove GPU recovery.

### F. Start device acceptance as soon as the handheld is available (plan 14)

This can proceed independently of full original parity.

- [ ] Select and record adb serial. Test clean asset-free install/import,
  cold/warm launch, gamepad/touch, controller loss, background/foreground,
  renderer recovery and original/enhanced frame-time/memory/thermal behavior.
- [ ] Run a sustained race, record performance percentiles and thermal drift,
  then choose default resolution/enhancement settings from evidence.
- [ ] Exercise low storage, interrupted import, save persistence and upgrade
  using the same signing key. Record failures as bugs, not device skips.

Acceptance: observed evidence for plan 14's entire checklist. APK assembly
cannot substitute for a hardware run. Physical device availability is required.

### G. Close release evidence and status (plans 13, 15, 18, 20)

- [ ] Round-trip original saves; exercise career/shop/level-up, jukebox,
  settings and each outcome's movie/music transition. Close remaining upscale
  attachment/UI items or obtain an explicit scope decision.
- [ ] Integrator refreshes one canonical status matrix from current evidence,
  labels historical plan blocks and links this plan. Preserve receipt source
  identity and keep diagnostic success separate from strict acceptance.
- [ ] Run clean-checkout native/strict oracle/sanitizer/Android/package gates;
  verify data exclusion, standalone launch, upgrade persistence and licenses.
- [ ] Resolve distribution signing/multiplayer decisions only when needed;
  obtain the owner's explicit confirmation before a release tag per plan 15.

Acceptance: all plan-00 definition-of-done items have fresh, source-identified
evidence or an explicitly approved scope change. Browser work remains a separate
optional milestone and must not inflate native-port completion.

## Suggested message for the active agent

Continue the scheduler-metadata and genuine human-branch proof work on plan 10.
Please report exact before/after first divergence, immutable capture receipts,
strict acceptance outcome and remaining scope; keep one RNG anchor and do not
weaken diagnostic expectations. The separate October 14 review found screen
restart/interpolation/renderer-reset defects and wrote plan 32; these can be
scheduled after your current slice. Please refresh the canonical status matrix
at integration, distinguishing ported helpers from live systems (especially
police) and original acceptance. No need to interrupt the current capture work.
