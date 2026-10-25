# Plan and police review — October 18, 2026

Baseline: `f06a0d4`. Plan 32 C/D, plan 24 Task 2. Read-only source review;
only this unique handoff was written. No shared status, source or test edits.

## Ownership and plan order

Read native AGENTS, CURRENT_STATUS, plan 00, plan 32, plan 24, plan 17,
recent commits and October 18 police/full-field/level-5 handoffs. Registry
`python3 tools/agent_workspace.py list` still assigns the proxy, CMake,
replay helpers and SIM_SCENARIOS to sim-l5-20261018. Existing dirty simulation
files were preserved. Sound source/tests belong to the sibling sound worker.

Highest independent slice was the plan-32D real arrest → RaceScreen outcome →
career fine/BUSTED movie integration regression. Parent has claimed and is
implementing `tests/race_screen_test.cpp`. Current police_test only demonstrates
Race::busted; helper career tests separately cover fines. Original cop movement,
city-cops and crashed-cop oracle comparisons remain required acceptance; no new
runtime fidelity claim follows from this review.

## Concrete arithmetic mismatch in unclaimed police.cpp

`police_arrest` uses an int64 magnitude. Original 0x00407f30 uses 32-bit
`cdq; xor; sub` absolute values followed by signed comparisons. INT32_MIN
therefore stays negative and passes these original thresholds:

- crash==2 speed: 0x00407f72–0x00407f8a, compare to 0x708 and SETL;
- position delta: 0x00407f9e–0x00407fb4, compare to 0x200 and JGE;
- speed delta: 0x00407fb6–0x00407fc8, compare to 5000 and JGE.

Examples with owner 0, found_cop true, all flags clear:
1. crash 2, bike_speed=cop_speed=INT32_MIN, equal position;
2. bike_speed=cop_speed=0, cop_position=INT32_MIN, rider_position=0;
3. bike_speed=0, cop_speed=INT32_MIN, equal position.
All return true under the original arithmetic, false under current native.
This is a synthetic semantic boundary; no ordinary gameplay reachability was
established. Safe independent follow-up: emulate wrapped int32 absolute value
using unsigned operations/explicit signed conversion in police.cpp; add the
three focused original-arithmetic boundaries to police_test. Do not claim an
oracle capture comparison from those tests.

## Other reviewed point requiring evidence before a change

Native ai_replan has a comment for the cop target draw-callback gate but no
actual branch. Original 0x00408849 calls 0x00432500; 0x00408850–0x00408868 clears
target and step when target +0xe4 is zero (or the separate two-player rule).
0x00432500 is exactly `target->draw_callback != 0`. Parked native riders already
lose their slices and are skipped by scans, so this review did not establish a
reachable single-player mismatch. Do not blindly equate this gate with `live`:
original parking swaps think/move callbacks and is distinct from draw presence.

## Tools and evidence

- Disc EXE SHA256 rechecked: 73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2.
- Used rg/fd and existing keyed exports plus original-disassembly.txt.
- Pinned `npx --yes rea-agents@6.4.0 function ... 0x004091f0 --format md
  --filter-output raw_result.value.pseudocode` completed exit 0, using the
  documented Node 24/Ghidra/JDK environment. Whitespace-normalized output is
  identical to analysis/decompiled/004091f0.c. Local output/log:
  /tmp/codex-plan-review-4091f0-20261018.md and .log. No shared Ghidra import.
- C++ LSP protocol unavailable in this client. `python3 tools/agent_lsp.py
  --check src/police.cpp` ran clangd AST checks, 0 errors. Wrapper exit 1 is
  compile-database timestamp staleness (CMakeLists newer, 181 entries,
  no missing sources); it is not a clangd diagnostic failure. No symlink or
  database changed.
- No native build/test run for this documentation-only review. No Wine process,
  capture, prefix install, build lock or resource claim held; REA exited.
