# codex-contact-guard-20261014

Task: Plan 10 car contact tail after knock-off

Baseline: 4c14085f67b15fdf63fdadd7df97708ab78695b7

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-contact-guard`
Owner PID: 48854
Resource token: `fd06fcae-9ed2-4fbd-8d61-ca2397663957`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/race.cpp`
- `/Users/archit/Dev/rr/native-port/tests/car_contact_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/codex-contact-guard-20261014.md`

## Progress / evidence

Plan 10 follow-up to the October 14 contact-tail audit. Changed only
`src/race.cpp`, `tests/car_contact_test.cpp`, and this handoff. The integrator
owns CMake registration and shared status documentation. Source/test edits are
frozen for the integrator's full native gate; no commit made by this worker.

The previous handoff's guard concern is superseded: `crash_reason` initially
zeros +0x2f4 before calling `begin_crash`, but `begin_crash` sets it to 1 before
returning (`src/crash.cpp:118`), matching original `00403d60.c`. Therefore
`knock_off` correctly reports the decision, and a missing guard alone did not
produce a steering kick on an already-down rider. The real discrepancy was
`contact_car` returning immediately after the crash decision, skipping the
unconditional lateral nudge and impulse/flag clear. Original `0x004049c0`
continues through `0x00404f4d` and `0x00404feb` regardless of local_21; local_21
suppresses only the `0x00404f93` steering kick.

Removed that early return, carried the actual knock-off return into `down`,
and guarded only the rate-change kick. Also corrected the newly reachable
negative-speed tail bound to -1000 (original `0x00405042` region), replacing 0.
No RNG or crash-helper behavior changed.

Evidence: existing `analysis/decompiled/004049c0.c`, `004052c0.c`, `00403d60.c`;
integrator's independent `npx rea-agents@6.4.0` output at
`analysis/codex-contact-20261014/rea-004049c0.md` (evidence
`ev_63dfb661a5d0d4f1ef8d123732d93565766f0fb931069d941025f5eefec8f6ac`).
This worker did not run REA or mutate the oracle/capture environment.

New fully synthetic fixture exercises public `Race::step` car contact in both
class 2 and class 3 with a deterministic exhausted rider, real separation,
knock-off decision, and contact tail. Synthetic PE/resource headers contain no
disc bytes. Frozen-rider setup isolates contact behavior. Baseline standalone
execution failed `knock-off must clear contact impulses and flags`; fixed
execution passes impulse clearing, nudge, crash reason/state, no steering kick,
and the class-2 speed-floor assertion (-6007 clamps to -1000). This is component
coverage, not original gameplay replay or full simulation acceptance.

## Checks / remaining work

- `cmake -S . -B build-agent-contact-guard -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`: passed.
- `cmake --build build-agent-contact-guard --target rr-resource --parallel 6`: passed before/after the fix.
- Standalone synthetic test compile/run: baseline failed as above; fixed passed.
- Focused CMake build of `race-test`, `traffic-test`, `crash-test`, `car_contact-test`: passed.
- Initial focused CTest ran race/crash/traffic successfully but car_contact was Not Run: the first build used `car-contact-test` (a manually compiled output), while CMake's registered target is `car_contact-test`. Corrected build and isolated car_contact CTest passed; clean four-test rerun passed 4/4 in 9.92 seconds (`/tmp/rr-contact-focused-final.log`).
- No interactive C++ LSP tool is exposed in this client. Initial `tools/agent_lsp.py --check src/race.cpp` found the shared build-clangd database stale (C++17, missing new sources); its errors are unrelated baseline/tooling state. Private C++20 compilation database configured; `clangd --check=src/race.cpp --compile-commands-dir=build-agent-contact-guard --tweaks=''` exited 0, all checks completed with 0 errors (`/tmp/rr-contact-clangd-final.log`).
- Full native gate belongs to the integrator. No full simulation parity, sanitizer, Android, or device claims.

## Live processes / resources

File claims released at worker completion. No capture/Ghidra resources held. Owner PID 48854 belongs to the integrator; do not stop it. All private build/test and clangd commands complete.

## Final review: guard observability

The synthetic fixture pins the removed early-return defect (impulses and
flags remain uncleared before the fix) and the nudge/speed floor. Its steering
assertion confirms a crash preserves steering in the stand-in fixture; it
does **not** independently distinguish presence of the `!down` guard, because
`+0xf0` is unchanged on the current down paths.

Re-reading `begin_crash` confirms it clears `+0x288`, not `+0xf0`. Its only
write to bike `+0xf0` adds the second bike-impulse component. Car knock-off
uses `{0,0,1024}` and `crash_car` uses `{0,0,0x100}`, both with a zero second
component. The down branch also bypasses the lateral `+0xf0` adjustment.
Thus adding the animation library would not make the guard observable under
current car-contact vectors. No artificial callback, production change, or
misleading precondition assertion was introduced to manufacture that case.
The guard remains directly supported by original `0x00404f93` (`TEST AL, AL`;
`JNZ 0x00404feb`) and is retained for fidelity. No guard-removal mutation test
was claimed. Source and test files remain frozen and unchanged after final
review; this documentation clarification is the only follow-up edit.
