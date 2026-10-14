# Docs and Status Hygiene Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) conventions and [REVIEW-ORCHESTRATION.md](REVIEW-ORCHESTRATION.md) first. Shared docs belong to the orchestrator: workers draft into their handoff files, and the orchestrator applies the edits.

**Goal:** A new agent reading `00-INDEX.md` and `PROGRESS.md` gets the current state, every handoff is either merged into the shared docs or listed as open, and a cheap check stops the most common drift from coming back.

**Architecture:** One refresh pass (Task 1), a generated handoff index with an archive folder (Task 2), and three doc-consistency checks added to the existing `agent-tooling` CTest (Task 3).

**Tech Stack:** Markdown, Python 3 stdlib, the existing `tools/agent_status.py` and `tests/agent_tooling_test.py`.

**Spec:** [../agent-review/CODE-REVIEW-2026-10-13.md](../agent-review/CODE-REVIEW-2026-10-13.md), finding 17.

## Global Constraints

- Never claim a verification that was not measured. Copy status words from receipts, plan checkboxes and `VERIFICATION.md`, not from memory (AGENTS.md).
- Handoffs are moved, never deleted. Their paths may be cited by commits and other docs, so Task 2 leaves an index entry for every moved file.
- `CURRENT_STATUS.md` stays generated from `agent-status.json` (`make agent-status`); do not hand-edit it.

## Review Focus

1. **A handoff that recorded live process IDs or held resources** must not be archived while those resources may still be held. Archive only handoffs whose resources section is empty or explicitly released.
2. **Links into moved handoffs** from `PROGRESS.md`, `VERIFICATION.md` and plans must be rewritten. Task 3's link check catches misses.
3. **Concurrent sessions editing `PROGRESS.md`.** Task 1 is a single orchestrator commit, made while no other session holds a docs claim.
4. **A plan superseded mid-flight** (as 12 was by 17) must be marked in the index table, not silently left "New".
5. **The language standard** changes with plan 22 Task 1. The check in Task 3 reads `CMAKE_CXX_STANDARD` rather than hard-coding 20.

---

### Task 1: Refresh the header of `PROGRESS.md` and the index status

**Wave:** W1 · **Owns:** drafts in `docs/agent-handoffs/review-31-1.md`; the orchestrator applies them to `docs/PROGRESS.md` (first two sections) and `docs/plans/00-INDEX.md` ("Where things stand", the plan status table) · **Profile:** research

- [x] **Step 1: Collect facts** into the handoff, each with its source:
  - `rg -n CMAKE_CXX_STANDARD CMakeLists.txt` gives the language standard.
  - `git log --oneline -60` and the plan checkboxes (`rg -n "^- \[x\]" docs/plans/1[0-9]*.md docs/plans/2[0-4]*.md`) give the done/open status per plan.
  - `docs/CURRENT_STATUS.md` gives the gate receipts.
  - `rg -n "police|cops" docs/PROGRESS.md | head` gives the police/pedestrian status as last recorded (plan 24).
- [x] **Step 2: Draft replacements** for these places:
  - the `PROGRESS.md` opening paragraph (standard, which courses use the original scene pipeline, police/pedestrians, plan range);
  - every row of `PROGRESS.md`'s "Status by area" table that the facts contradict;
  - `00-INDEX.md` "Where things stand (date)" (new date, same structure);
  - the status-table cells for plans 19–24 and new rows for 25–31.
- [x] **Step 3 (orchestrator):** apply the drafts in one commit, `docs: refresh progress header and plan index (review 2026-10-13)`.

---

### Task 2: Handoff index and archive

**Wave:** W2 · **Owns:** `tools/handoff_index.py`, `docs/agent-handoffs/INDEX.md` (generated), `docs/agent-handoffs/archive/` (moves applied by the orchestrator) · **Profile:** mechanical

- [ ] **Step 1: Test first** (a case in `tests/agent_tooling_test.py`). Build a temp `docs/agent-handoffs` with three files: one cited by a commit message (`git log --grep` fixture: initialise a temp git repo), one cited from `PROGRESS.md`, and one cited nowhere and containing `PIDs: 1234`. The tool must classify them as `merged`, `merged` and `open (resources)`.
- [ ] **Step 2: Implement** `python3 tools/handoff_index.py [--write] [--archive]`. For each `docs/agent-handoffs/*.md`, it records: title (first heading), date (filename or mtime), whether its filename appears in `docs/*.md`, `docs/plans/*.md` or `git log --all --format=%B`, and whether it mentions live resources (`PID`, `pid`, `lock`, `claim` followed by something other than "released"/"none"). `--write` regenerates `INDEX.md` as a table. `--archive` moves `merged` handoffs with no live resources into `archive/YYYY-MM/` and rewrites links in `docs/` (plain string replacement of the relative path).
- [ ] **Step 3:** Run it with `--write` only. The orchestrator reviews `INDEX.md`, then runs `--archive` in a separate commit: `docs: archive merged handoffs (index kept)`.

---

### Task 3: Doc-consistency checks

**Wave:** W6 · **Owns:** `tests/agent_tooling_test.py` (three new cases), `tools/doc_checks.py` · **Profile:** mechanical

- [ ] **Step 1:** `tools/doc_checks.py` exits nonzero with a message per problem:
```python
import pathlib, re, sys
root = pathlib.Path(__file__).resolve().parent.parent
problems = []
# 1. Every plan file has a row in 00-INDEX, and every linked plan exists.
index = (root / "docs/plans/00-INDEX.md").read_text()
for plan in sorted((root / "docs/plans").glob("[0-9][0-9]*.md")):
    if plan.name != "00-INDEX.md" and plan.name not in index:
        problems.append(f"00-INDEX.md: no row for {plan.name}")
for link in re.findall(r"\]\(([0-9][0-9][^)]*\.md)\)", index):
    if not (root / "docs/plans" / link).exists():
        problems.append(f"00-INDEX.md: broken link {link}")
# 2. Relative links in docs/*.md and docs/plans/*.md resolve.
for doc in list((root / "docs").glob("*.md")) + list((root / "docs/plans").glob("*.md")):
    # Drop fenced and inline code first: C++ lambdas like `[&](bool v)` look like links.
    prose = re.sub(r"`[^`\n]*`", "", re.sub(r"```.*?```", "", doc.read_text(), flags=re.S))
    for target in re.findall(r"\]\((?!https?:|#)([^)#\s]+)", prose):
        if not (doc.parent / target).exists():
            problems.append(f"{doc.relative_to(root)}: broken link {target}")
# 3. PROGRESS.md names the configured C++ standard.
std = re.search(r"CMAKE_CXX_STANDARD (\d+)", (root / "CMakeLists.txt").read_text()).group(1)
if f"C++{std}" not in (root / "docs/PROGRESS.md").read_text().split("## Timeline")[0]:
    problems.append(f"PROGRESS.md header does not mention C++{std}")
print("\n".join(problems) or "doc checks OK")
sys.exit(1 if problems else 0)
```
- [ ] **Step 2:** Run it on the current tree and record the problem list in the handoff. Fix the broken links it reports in the docs this plan owns; send the rest to the orchestrator as a list.
- [ ] **Step 3:** Call it from `tests/agent_tooling_test.py` so the existing `agent-tooling` CTest enforces it. Commit: `tools: doc consistency checks in agent-tooling`.
