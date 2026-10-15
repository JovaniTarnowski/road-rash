# INCIDENT: interleaved commits + orphaned tip on shared main (October 14, 2026)

## What happened (reflog evidence)

Two agents committed to shared `main` within ~4 minutes without
coordinating staging state; a third command (`reset`) then orphaned a tip:

1. `95b8bc8` (opencode session): message "docs: Debug vs Release
   determinism receipt" but content = own 5-line VERIFICATION hunk PLUS
   codex-contact-guard's PRE-STAGED frozen files (`src/race.cpp` 19-line
   contact-guard delta, `tests/car_contact_test.cpp` new 100 lines,
   `CMakeLists.txt` car_contact registration + dash_bench guard,
   `docs/agent-handoffs/codex-contact-guard-20261014.md`,
   ~46-line VERIFICATION section). The `git add <explicit paths>` +
   `git status` check passed, but `git diff --cached --stat` was NOT
   inspected — the staged set contained another worker's `git add`.
2. `29fd605` (gemini-fuzz-fixes): `src/explode.cpp` + `tests/resource_test.cpp`. Clean.
3. `02d7e8d` (codex): docs-only record commit (NAPA/PROGRESS/REVERSE/
   VERIFICATION/codex-goal handoff/codex-sim-compile handoff/plan 10).
   Clean. NOTE: it does NOT re-commit the contact-guard code — the only
   committed copy of that code is inside `95b8bc8` (still in history).
4. `2b08e6f` (codex): "hud: show original numeric speed in MiniDash"
   (`src/hud.cpp/hpp`, `tests/hud_test.cpp`, docs, mini-speed handoff).
5. Opencode session then ran `git reset HEAD~1` to repair (1) — but HEAD
   had moved to `2b08e6f`, so the reset orphaned `2b08e6f` instead of
   `95b8bc8`. Reset immediately recognized as wrong; NO further resets,
   rebases, checkouts, or `git add`/`commit` performed after that point.
6. `c2ab4d9` (gemini-fuzz-fixes): `src/rri.cpp` + `tests/resource_test.cpp`.
   Clean (disjoint files; unaffected by the above).

## Content preservation proof (all read-only, this session)

- `git cat-file -t 95b8bc8` = commit (contact-guard code safe in history).
- Worktree `src/race.cpp`, `tests/car_contact_test.cpp`,
  `CMakeLists.txt` show NO unstaged modification vs HEAD → byte-identical
  to the frozen contact-guard content. Their full gate evidence stands.
- Orphan `2b08e6f` fully intact: `git diff 2b08e6f -- <its 8 tracked
  paths>` is EMPTY (worktree == orphan tree), and the untracked
  `docs/agent-handoffs/codex-mini-speed-20261014.md` is byte-IDENTICAL
  to `git show 2b08e6f:<that path>`. Nothing to recover — only the tip
  pointer is gone (object retained via reflog).
- All other in-flight work untouched: SIM-owner `oracle/*` diagnostics,
  codex-goal doc edits (NAPA/PROGRESS/REVERSE/plan-11), `tests/dash_bench.cpp`
  scratch, `analysis/*` dirs, gemini fuzz files.

## Attribution corrections (for blame readers)

- `95b8bc8` message covers only the 5-line determinism hunk; its
  `race.cpp`/`car_contact_test.cpp`/`CMakeLists.txt` hunks +
  `codex-contact-guard-20261014.md` + the "car knock-off tail" VERIFICATION
  section are codex-contact-guard's frozen contribution (see that handoff:
  "Source/test edits are frozen for the integrator's full native gate").
- `2b08e6f` (reflog only): codex MiniDash work; content lives on in the
  worktree for its owner to re-commit with correct attribution.

## Recommended path (owner/integrator decision — NOT executed here)

- Do NOT reset/rebase/checkout shared `main`: three live workers hold
  claims and unstaged work on this tree (codex-goal docs, SIM oracle,
  gemini fuzz). Any history rewrite now risks real loss.
- Codex operator: re-commit MiniDash hud work from the worktree
  (`git add src/hud.cpp src/hud.hpp tests/hud_test.cpp` + docs) at your
  convenience; your frozen contact-guard code needs no action (already
  committed, gate evidence in your handoff).
- Integrator: the contact-guard shared-tree full gate (build-integrator
  rebuild + 95-test ctest) is still owed per the codex handoff; run it
  before any refactor touches `contact_car`.
- Process: `git diff --cached --stat` inspection before EVERY commit
  (the missed step that caused this); file claims via
  `tools/agent_workspace.py` before staging shared docs; prefer
  per-worker worktrees for implementation slices (AGENTS.md already says
  this — this incident is the reason why).

## Checks

- Zero mutations after the erroneous reset: this handoff file is the only
  write of the session past that point. No builds, no captures, no
  processes touched (stray `race_soak-test` from an earlier `--help`
  probe was already reaped; verified GONE).
- Live workers at write time: PID 48854 (codex claims), PID 58522
  (isolated validation build), PID 1 fuzz-fix claim set. None contacted
  or disturbed.
