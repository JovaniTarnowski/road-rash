# codex-sim-integrate-20261014

Task: Integrate validated SIM oracle repair and restore accidentally orphaned MiniDash commit

Baseline: d77f19017d86d548633097aca0155cd37fd06dc2

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-codex-sim-integrate-20261014`
Owner PID: 62801
Resource token: `9e5f3376-5e89-43ed-8d6e-d40f76730e65`

Owned files:

- `/Users/archit/Dev/rr/native-port/oracle/aweman_proxy.c`
- `/Users/archit/Dev/rr/native-port/oracle/actor_record_hooks.h`
- `/Users/archit/Dev/rr/native-port/oracle/actor_record_buffer_test.c`
- `/Users/archit/Dev/rr/native-port/CMakeLists.txt`
- `/Users/archit/Dev/rr/native-port/tests/oracle_sim_test.cpp`
- `/Users/archit/Dev/rr/native-port/tests/frozen_state.hpp`
- `/Users/archit/Dev/rr/native-port/tests/frozen_state_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md`
- `/Users/archit/Dev/rr/native-port/docs/SIM_SCENARIOS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/docs/plans/10-simulation-oracle.md`
- `/Users/archit/Dev/rr/native-port/src/hud.cpp`
- `/Users/archit/Dev/rr/native-port/src/hud.hpp`
- `/Users/archit/Dev/rr/native-port/tests/hud_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/NAPA_IMPLEMENTATION.md`
- `/Users/archit/Dev/rr/native-port/docs/plans/11-napa-race-completion.md`

## Progress / evidence

Restored the unchanged, validated MiniDash contribution as6ecbca9 after
reading the accidental-reset incident. No shared history was rewritten.

Integrated tested production oracle patch from isolated02d7e8d checkout:
- forward original billboard calls, cap512 records, reset per present;
- count dumped ticks for SIM termination;
- log first-frame frozen copies/velocities/flags/crash and grid formula data;
- strictly hydrate measured frozen state once in solo replay, human or demo.

Clean production oracle is staged via its exact patch rather than sweeping
the older OMP diagnostics into the commit. Shared diagnostics are retained;
only their g_frame declaration scope was repaired so that dirty build also
compiles. oracle/build.sh's pre-existing -lwinmm edit remains unstaged.

Isolated full native gate97/97, zero skips, viewer/course sweep pass.
Native proxy cross-build passes with no warnings; strict synthetic wrapper
and frozen-state tests plus ASan/UBSan pass.900-tick capture and300-tick
forensic follow-up validate cleanly; previous250-present/70-tick mismatch
was correctly rejected before repairing SIM termination counting.
Shared integrated full native gate is running; record final receipt before
commit. Installed DLL has been changed back from the temporary mask probe
to the clean fixed production DLL, hash
4ed76fc2be4014edc2d1f57c1bed8623b2dd9fe49abc79fe44ad07711b5feedd.
Original DLL and logs are preserved in analysis/sim-forensics.

New measured gap: candidate dt4 vs native1 explains tick210 extra contact.
Read frozen-scheduler-trace-20261014.md. Capture +0c/+10/+14/+18 and model
metadata scheduling even with NULL callbacks; don't hardcode timestep or
seed every tick. No golden/known-divergence expectation was changed.

## Checks / remaining work

Next independent requirement: genuine human branch proof.0x434660 gates
on demo byte0x4753c8==0 AND outcome/mode0x4c5d7c==1. Clear demo only once
after setup, never force mode continuously. Entry trampoline and human-only
0x434931->0x40ad30 callsite can prove actual invocation; filter by rider
half+0x1c4 owner equal global player. Sampling the demo flag alone is weak.
Full human acceptance, native scheduler parity and original actor pixels
remain open. See forthcoming human-branch-probe handoff.

## Live processes / resources

No Wine capture processes remain. The temporary mask-probe DLL is no
longer installed. Main/isolated claims will be released at handoff. Native
gate may still be running until final receipt is recorded. Stable isolated
checkout is retained for evidence; local commercial traces are not committed.
