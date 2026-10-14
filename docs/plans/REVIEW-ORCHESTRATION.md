# Orchestrating plans 25–31 with subagents

Source: [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md).
This file is for the **orchestrator**, the one session that dispatches
workers, merges their branches and owns the shared docs. Workers read only
their own task, the plan's Global Constraints, and the brief the orchestrator
sends them.

## Roles

| Role | Does | Never does |
| --- | --- | --- |
| Orchestrator (integrator) | Picks tasks from the wave table and creates one worktree per worker. Sends the brief and reviews diffs. Runs the integration gate, merges, and updates `PROGRESS.md`, `VERIFICATION.md`, `00-INDEX.md` and plan checkboxes. | Edit a file a live worker owns |
| Implementer (subagent, `isolation: worktree`) | One task: writes a failing test, implements, runs focused tests, commits on its branch, and writes `docs/agent-handoffs/<task-id>.md`. | Touch shared docs, CMake outside the lines the task names, or another task's files |
| Reviewer (fresh subagent, read-only) | Checks the branch diff against the task's acceptance, the plan's Review Focus and AGENTS.md fidelity rules. Returns approve/reject with file:line reasons. | Edit code |
| Researcher (Explore subagent, read-only) | Answers one bounded question, e.g. "list every caller of X". | Edit anything |

Use the most capable model for orchestration, for reviewing plans 26 and 27,
and for any task marked **sim-sensitive**. Mid-tier models suit mechanical
tasks, which are marked **mechanical**.

## Rules every brief repeats

1. Work in your worktree (`git worktree add ../rr-wt-<task-id> -b review/<task-id> <base>`)
   with build dir `build-wt-<task-id>`. Do not use the shared `build/`.
2. The files you may edit are listed under **Owns**. Ask the orchestrator before
   touching any other file.
3. Golden-backed tests exit 77 without the disc. Run them anyway when
   `../cdrom/ROADRASH` exists, and report skip versus pass separately.
4. Sim-sensitive tasks: record `race-soak` and `oracle-sim` outputs **before**
   and after; they must be byte-identical unless the task says otherwise.
5. Stage explicit paths. One commit per task, `<area>: <what>`, ending with the
   Co-Authored-By line from 00-INDEX.
6. Finish with a handoff: baseline commit, changed files, commands and their
   results, skips, and remaining work.

## Brief template

```
Task: Plan <NN> Task <k> - <title>
Repo: /Users/archit/Dev/rr/native-port, base commit <sha>
Worktree/branch: ../rr-wt-<id> / review/<id>; build dir build-wt-<id>
Read first: docs/plans/<NN>-....md (Global Constraints, Review Focus, Task <k> only), AGENTS.md
Owns: <file list from the task>
Interfaces you must produce: <copied from the task>
Acceptance: <commands + expected output copied from the task>
Do not: edit shared docs, CMake beyond the named lines, or files outside Owns.
Report: diff summary, every command run with exit code, skips, open questions.
```

## Shared-file serialization

Two tasks that own the same file never run concurrently. The hot files:

| File | Tasks that edit it (in this order) |
| --- | --- |
| `src/game_main.cpp` | 25.1 (+ orchestrator hunks from 25.3, 25.6) → 26.3 → 28.1 → 28.2 → 28.3 → 28.4 → 30.3 G1 (then plan 23 Task 3) |
| `src/race.hpp` / `src/race.cpp` | 25.6 → 26.1 → 27.3 (wave-split by file group) |
| `src/bike.hpp` | 27.1 → 27.2 |
| `src/raster_rgb.cpp` | 25.3 → 29.2 → 29.3 |
| `src/render_threads.cpp` | 25.2 → 29.4 |
| `CMakeLists.txt` | orchestrator applies each task's CMake hunk at merge |
| `docs/*` (shared) | orchestrator only |

## Waves

Tasks in one wave are independent and can run in parallel, one worktree each.
A wave starts when the tasks it depends on are **merged**, not merely done.

| Wave | Parallel tasks | Depends on |
| --- | --- | --- |
| W1 | 25.1, 25.2, 25.3, 25.5, 25.6, 30.1, 30.2 (header), 31.1 | — |
| W2 | 26.1, 27.1, 30.3 groups G2–G5, 30.4, 31.2 | W1 merged (25.6 before 26.1; 25.2/25.3 before plan 29) |
| W3 | 26.2, 27.2 (decision-gated), 29.1, 30.2 fan-out, 30.5 | 26.1, 27.1 |
| W4 | 26.3, 27.3 fan-out (decision-gated), 29.2 → 29.3 (same file, serial) | 26.2, 27.2, 29.1 |
| W5 | 26.4, 28.1, 29.4 | 26.3 |
| W6 | 28.2 → 28.3 → 28.4 → 30.3 G1 (serial; one worker), 31.3 | 28.1 |

Conflicts inside a wave: 27.1 and 30.4 both touch `src/bike.cpp`, and 30.3 G4
and 30.4 both touch `src/import_job.cpp`. Run each such pair
in sequence (merge the first, rebase the second) even inside one wave. Plan 27
Task 3 groups C–E overlap plan 30 Task 3 groups. Schedule 27.3 after 30.3 has
merged, or drop the overlapping files from one of them.

Plan 27 Task 3 fans out widest: one implementer per file group, all editing
disjoint files and all byte-identical against the soak and oracle outputs. It
must run when no other session is editing `race.cpp`, `bike.cpp` or
`traffic.cpp`; check `git status` and the claims registry
(`python3 tools/agent_doctor.py`) first.

## Per-task loop (orchestrator)

1. Claim the task's **Owns** files (`tools/agent_workspace.py` claim, or the
   handoff convention) and create the worktree.
2. Dispatch the implementer with the brief.
3. On return, dispatch a fresh reviewer with the task text and `git diff base..branch`.
4. If rejected, send the reasons back to the same implementer (SendMessage) and
   repeat step 3.
5. Merge into the integration branch, then run the **integration gate**:
   `cmake --build build-integ --parallel && ctest --test-dir build-integ --output-on-failure`
   plus `-L sim` (plan 30 labels) and, for sim-sensitive tasks, the soak and
   oracle hash comparison.
6. Update the shared docs and the plan checkbox; release the claims.

## Integration acceptance for the whole set

- `ctest` passes in Debug and in `build-asan`. `tools/tsan.sh` is clean after
  plans 25.2 and 29.
- The `oracle-sim` diagnostic report is byte-identical across every plan here,
  including plan 26. `race-soak` hashes are unchanged by plans 25, 27, 28, 29
  and 30. Plan 26 Task 1 changes them deliberately (the factory gains hooks);
  the handoff records old and new hashes, and later tasks compare against the
  new ones.
- `tools/ci.sh` passes, including the Android debug assemble.
