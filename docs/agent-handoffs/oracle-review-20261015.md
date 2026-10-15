# Integrator review: SIM worker oracle tree (accept/reject per hunk) — October 15, 2026

Read-only review of the uncommitted SIM-worker tree for the plan-10
integrator (claims `codex-sim-integrate-20261014`, stale PID 62801 verified
gone; tree edits preserved untouched). Baseline `8ba6e4a`. No source edits,
no builds in shared dirs, no captures by this review.

## Accept (production — merge as-is)

1. Frozen-field capture (`oracle/aweman_proxy.c` hunks `+295`, `+315`,
   `+391`): `probe.frozen_words=1` marker; type-4 bike words
   `0x30..0x40/0x108/0x10c/0x110/0x2f4` + `bike_c0`; grid `next/centre/
   bends`; brain `p.18c/190-198`. Matches the replay side already in
   `tests/oracle_sim_test.cpp` + `tests/frozen_state.hpp`.
2. SIM tick-count limit (`+495` hunk): quit on dumped-tick count
   (`g_sim_frames`) instead of presented frames. Fixes the buffer250
   miscount (70 ticks for 250 presents).
3. Actor-record extraction (`1406` hunk + new
   `oracle/actor_record_hooks.h` + `oracle/actor_record_buffer_test.c`):
   removes the non-forwarding 512-entry `hook_select`/`hook_billboard`
   (whose overflow corrupted `g_frame`/`g_pixels` in SIM mode) and replaces
   with forwarding + capped + per-present `reset_actor_records()` (present
   macro, `2000` hunk). Synthetic regression (4099 calls/wrapper) passes;
   `oracle-actor-record` CTest green in the owned build.
4. `g_frame`/`g_quit_after` declaration move (`+48` add / `596` remove):
   benign, required by (5)'s removal.
5. Test side: `tests/frozen_state.hpp`, `tests/frozen_state_test.cpp`,
   `tests/oracle_sim_test.cpp` hydration, `CMakeLists.txt` registrations,
   `docs/{SIM_SCENARIOS,VERIFICATION,REVERSE_ENGINEERING,PROGRESS}`,
   `docs/plans/10-simulation-oracle.md` footer. CMake + untracked sources
   must land atomically (else pristine HEAD breaks).

## Reject (do NOT merge)

6. Unconditional per-step timer killing (`+539` hunk): `KillTimer`/
   `timeKillEvent` run on EVERY step whenever `g_sim && g_race_started`
   (only the logging is env-gated). Kills the game's own menu/splash
   timers mid-race and can mask real faults; the SIM fault is already
   explained by instrumentation corruption (item 3). The GREENSTART
   countdown skip in the same block (`RD32(0x004753c0) = 0`) alters race
   timing. Reject the whole block.
7. Fault/PAGE_GUARD handler (`+23`, `+48`, `+2112` hunks,
   present-macro guard line): `AddVectoredExceptionHandler` installs
   unconditionally; `ExitProcess(11)` on non-self guard trips can kill
   good captures; the frame-60 `VirtualProtect PAGE_GUARD` self-test
   mutates protection at runtime. Experimental — reject.
8. All `RR_ORACLE_MENUDBG` logging (`+133`, `+529`, `+567` ×2, `2000`
   present-exit/frameX, `2096` dllattach, `1406` near_wedge secwalk):
   diagnostic volume + capture-time overhead in the prefix. Reject.
9. `RR_ORACLE_FAULTEIP` log path (inside the rejected handler): reject.
10. `oracle/build.sh -lwinmm`: only needed by the rejected `timeKillEvent`
    (item 6). `KillTimer` is user32 (default-linked). Leave `build.sh`
    unchanged.
11. `tests/dash_bench.cpp`: untracked scratch benchmark; leave untracked
    (CMake already gates it behind `RR_BENCH_DASH`, off by default).

## Suggested merge order (integrator)

a. Items 1–2 (capture) + item 5 docs, rebuild + reinstall DLL, recapture
   frozen900-equivalent; confirm header fields parse.
b. Item 3 (forwarding fix) + item 4 registration; run
   `oracle-actor-record`, `frozen_state`, full native gate.
c. Rerun the three solo replays; tick-210 signature must be unchanged
   (owned-build evidence: frame 209, rng 849046096 vs 2140064586).
d. Then the `+0x0c/+0x10/+0x14` oracle extension per
   `sched-cadence-20261015.md` §6 (still open; writer traced to
   `0x00438930`, see `sched-interval-writer-20261015.md`).

## Live resources

None held. Owned builds `build-goal-20261015` (native) and
`build-goal-asan` (sanitizer) only; `build-goal-ci` belongs to the
background `tools/ci.sh` run (native + android-debug, oracle opt-out).
