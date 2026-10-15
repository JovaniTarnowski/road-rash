# Handoff: genuine human-branch proof + strict acceptance report — October 16, 2026

## Task / plan

Plan 10 Task 5 Step 2a (human-control proof) + plan 32 integration (read plan,
refresh status matrix). Baseline `59c8bf9`. No questions asked per session
instructions; shared tree preserved (no resets, explicit-path staging).

## Commits (branch `main`, pushed)

- `240e54f` — sim: human-branch probe and genuine player-control golden.
  Proxy dumps per-tick `player.demo_hold`; `RR_ORACLE_HUMAN=1` clears
  `DAT_004753c8` at race start (seed-hook pattern; bootstrap stays on
  attract otherwise; nothing re-arms mid-race). New `sched-human-20261016`:
  1431 contiguous ticks, hold 0 throughout, recorded 1250-word human stream.
- `1a18b39` — docs: file the completion review as plan 32, link it from
  `00-INDEX.md` (fixes the `agent-tooling` doc check), refresh
  `agent-status.json` + regenerate `CURRENT_STATUS.md` (frozen cadence,
  human proof with exact gaps, helpers-vs-live police split).

## Exact results (no weakening)

- Human replay, harness-inferred `driver=human (measured player.demo_hold=0)`,
  no CLI override: green at tick 2 (1 hold frame), first divergence frame 1
  (tick 2) `rider.0.ai_target_x` original 18176 native 0 (via
  `rider.0.brain_194`); RNG in sync. Position responds to human throttle
  identically (control-path evidence beyond the flag).
- Strict `verify_sim --mode acceptance --driver human` on the human golden
  (receipt + `control_branch: human` + capability pass): `unexpected-failure`,
  exit 1, `acceptance_met: false` at the tick-2 gap. Report `/tmp/human-accept.json`.
- Demo diagnostic (sched900): tick 376 (spawn-timer lane draw, Task 7).
  Old goldens byte-identical. `agent_gate.py oracle` on the default fixture
  still fails on the old human golden's missing receipt (predates slice).
- Full native gate at baseline: 96/97 (only `agent-tooling` over the then-
  missing plan-32 row); row added, `agent-tooling` now passes focused 6/6
  with the slice. Native code untouched by proxy-only edits since.

## Key recovered facts (REVERSE_ENGINEERING.md)

- Eight `DAT_004753c8` readers; six in-race human gates (think 0x00434660,
  steer 0x0040ad30, skid 0x00438c70, move-push 0x004395e0, sched +0xc copy
  0x00438170, wreck rule 0x0040a600). Sole writer in exports: 0x00421430=0.
  Native models only the think gate: live game runs demo-tuned physics in
  the other five.
- Spare player half (solo rider.1, interval 1, mirrors bike) provably cannot
  draw with no attacks/contact/crash (call-graph fact); no native counterpart
  needed for RNG. Traffic timers fully traced (`traffic-timer-trace-20261016.md`):
  solo inits, three branches with exact scalings/gates/resets, mode-3 bypass,
  REA structural confirmation; branch-at-376 left capture-gated, no hardcoding.

## Concurrent work observed

- Another worker landed plan-32 R1/R2/E fixes (`831eecd`, `3ed82f6`: view,
  race_screen, side panels) during this slice; integrated cleanly (push, no
  conflicts). Claims checked: only stale PIDs on SIM files. A/B/C racing
  screens and traffic/game files were not touched by this session.
- Reviewer artifacts filed as delivered (plan 32, review report,
  completion-review handoff). `tests/dash_bench.cpp`, `analysis/*`,
  `fuzz/seeds/*` left untracked per standing rules.

## Remaining work (ordered)

1. Human tick-2 gap: maintain AI target state on the human path without AI
   actuation (trace subagent dispatched: `human-target-trace`, read-only).
2. Five human physics gates behind `demo_hold_` (steer/skid/move/sched-copy/
   wreck) with per-gate replay evidence.
3. Task 7: traffic-timer state/rates/taps with draws-but-no-spawn under the
   nulled pool (trace complete, implementation open).
4. Re-run `agent_gate.py oracle` + full `ctest` after 1-3; register the human
   scenario in acceptance fixture selection (integrator decision).

## Held resources

- Owned `build-verify-20261015`; no locks, no live captures. New goldens
  (`sched900-20261015`, `sched-human-20261016` + receipts) are gitignored
  local evidence. Proxy installed in the shared prefix is `3645b00b`
  (demo_hold probe + RR_ORACLE_HUMAN).

## Follow-up (same day): launch parity to tick 149

- Human-target trace consequence implemented (harness-side): frame-0 brain
  hydration extended to all solo captures + `base_speed` from `brain_1bc`
  + player scheduler hydration from slotted keys + metadata stepping moved
  post-drive/pre-collide (drive reads pre-update dt, contact fresh dt --
  proven by the tick-2 `2*918 = 1836` ramp; no tick-2 contact existed to
  contest the contact side).
- Oracle `extra[]` gains `bike_240/114/260/23c` (dump-only ground truth).
  Native spec words verified equal (table 248; cap 9160 vs 9188 and rate
  916 vs 918 truncation-absorbed through saturation -- benign, recorded).
- Results: human tick 2 -> tick 149 (wall scrape, plan 11 Task 4);
  demo sched900 still 376; all legacy goldens byte-identical.
