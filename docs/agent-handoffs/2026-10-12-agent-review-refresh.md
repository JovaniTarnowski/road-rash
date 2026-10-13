# Agent review refresh

Task: review retained Codex, Claude, OpenCode and especially OMP sessions, then
update instructions and tooling. Baseline: d5ffeac. The initial dirty gameplay,
web, tests, CI, plans and shared progress files belong to other sessions.

Owned/changed: native and workspace AGENTS.md, native Makefile,
docs/TOOLING.md, docs/AGENT_TOOLING.md, docs/agent-review/REFRESH-2026-10-12.md,
docs/agent-review/SESSION_INVENTORY-2026-10-12-refresh.json,
tools/agent_sessions.py, tools/agent_lsp.py, tools/agent_doctor.py,
tests/agent_sessions_test.py and this handoff. CLAUDE pointers already import
canonical AGENTS; no duplicate policy/configuration was added.

Evidence: review report and fixed-prefix inventory; raw review text stays under
/tmp/rr-agent-review-refresh and is not a repository artifact. All 53 discovered
histories parsed; OpenCode modern/legacy stores have no rr conversation records.

Checks: `make agent-tools-test` passes 21 synthetic regressions. Make dry-run and
diff whitespace checks pass. Read-only doctor and LSP preflight run; LSP correctly
reports the known missing `car_contact_test.cpp` and newer CMake files. No native
build is needed for these Python/documentation/Make changes. No CMake or gameplay
source edited. No commits made or other agents' files staged.

Remaining operational work: OMP/integrator can refresh its compilation database
in an owned build and reload the client after source/config changes settle.
Cooperative ownership tools still need adoption by participating sessions;
this review did not invoke/reconfigure external clients or prove their loading.
Strict simulation/device acceptance remains under the relevant plan owners.

No task-owned live processes, locks or resources remain.
