# rebind-dispatch-20261012

Task: Plan 18 Task 1 remainder — rebind flow/conflict rule + MiniDash/OpponentList dispatch (exports + disasm + registry strings; no REA/capture). Tree clean + index empty at start (verified); unowned slice only.

Baseline: `e0dec89` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (rerun at commit time for docs-only change)

## Findings

- `0x0040fbf0` conflict rule read in full: 11-row scan, same-code rows zeroed + relabeled; CONFIRMED PORTED (`apply_rebind`, tested). `0x0040fdf0`/`0x0040e580` ported + tested. Key-label TEXT (`0x0040e7b0`, 61 KB) uncompared — open.
- Registry strings resolved: `MiniDash`=`0x467ac4`, `OpponentList`=`0x467ab4`, block also holds `ChatState`/`Super`/`Spor`.
- Menu wndproc cycle handlers: `0x4235f8` (byte at `0x474f48`, guard `0x496008`) and `0x423625` (MiniDash). In-race F10 cycle at `0x4432b8`.
- UNRESOLVED + honestly recorded: whether `0x4235f8` targets OpponentList (binary struct at `0x474f48`, not the string — cannot claim it). Needs the `0x4235xx` message-ID→handler mapping. If it IS OpponentList with a 0-1-2-0 cycle, the native ON-only behavior is a deviation.
- Open list narrowed to: menu key→handler mapping, key-label text fidelity, Escape race-entry.

## Doc edits

- `docs/REVERSE_ENGINEERING.md`: "Open:" line narrowed + two evidence bullets.
- This handoff. No plan/source/test touched (Task 1 stays owner scope).

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my 2 pathspecs before committing.
- Suggested next trace (15 min): `0x4235xx` dispatcher head — which message IDs reach `0x4235f8` vs `0x423625` (answers the key mapping + the OpponentList question together).
- Integrator: fold into plan 18 Task 1 Steps 3–4 commit.

## Live processes / resources

No live REA/Ghidra/Wine processes. No build locks. /tmp/rsrc.py scratch outside repo (reused).
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
