# escape-entry-20261012

Task: Time-boxed trace of the Escape race-entry point (last open in plan 18 Task 1 options section). Disassembly only; stopped per the plan's trace rule rather than guessing. Tree clean + index empty at start (verified); unowned slice only.

Baseline: `85b09d1` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (gate `bg_34` running at trace time; rerun build check at commit time)

## Eliminated (with evidence)

- `0x40a835+` control dispatch: bits `0x78/0x10/0x08/0x40` (ah) + `0x2000` (eax) drive on-foot steering/pace (`ebp` −1/0/+1, throttle byte) — not quit.
- `0x436352` table: ascending key codes 4..0x39 — rebind display codes, not a quit path.
- `0x0044fa80`: keyboard→control-word builder via the `0x465fb8` rebind table (matches native `apply_rebind` source); no Escape special-case inside.

## Still open

The race-side call site for quit-confirm pump variant A (param_1 = 1 → `PostQuitMessage` + outcome 5). Native behavior already matches the confirmed outcome-5 rule, so this is trace completeness only, no behavior risk.

## Doc edits

- `docs/REVERSE_ENGINEERING.md`: time-box bullet.
- This handoff. No plan/source/test touched.

## Checks / remaining work

- Full gate `bg_34` on `85b09d1` (toggle fix): **71/71 pass, 191 s**. This turn is docs-only on top.
- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my 2 pathspecs before committing.
- Integrator: plan 18 Task 1 now complete except key-label text fidelity + this entry point.

## Live processes / resources

No live REA/Ghidra/Wine processes (gate job `bg_34` owned by runtime delivery, not held). No build locks.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
