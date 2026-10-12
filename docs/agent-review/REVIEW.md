# Cross-agent session review

Reviewed October 12, 2026, America/Los_Angeles. This is a workflow review and
proposal, not a gameplay verification run. Only new files under
`docs/agent-review/` were written; existing source, instructions, plans, Makefile
and agent configuration were left intact.

## Coverage and method

Found **14 prior top-level conversations plus four OMP child conversations**:
four Codex, seven Claude, three OMP parents and four OMP children. Parsed all
23,685 JSONL records (108,386,595 bytes; no malformed records), extracted user
requests, assistant text, tool calls/results and compaction summaries, and
reviewed milestones, failures, handoffs and recurring issues. Cross-checked
important claims against Git commits and the current source/docs. This is a
textual history review: image/audio attachments were not independently inspected,
and historical test outcomes were not rerun.

[SESSION_INVENTORY.json](SESSION_INVENTORY.json) records source paths, record
counts, timestamp ranges and SHA-256 hashes for fixed byte-prefix snapshots.
Two OMP histories continued appending during review; inventory hashes cover the
recorded prefixes, not later additions. Some sessions are unfinished; their last
reviewed messages are checkpoints rather than completed results.
The current review conversation is excluded. Deleted/cloud-only histories and
history no longer retained after compaction cannot be covered.

**OpenCode coverage gap:** its read-only SQLite database has the project
`2c13b8e087108bb56295857312d7bf381200a7e7` with worktree
`/Users/archit/Dev/rr`, but zero matching sessions by project ID or directory.
Also checked directory/title matches across its session table, legacy
`storage/session/**/*.json`, runtime logs and the secondary Muse session index.
`~/.local/share/opencode/log/opencode.log` contains project activity, including
watching `AGENTS.md` and MCP connections around 00:15 PDT on October 12. Those
are operational logs, not conversation history. OpenCode was inspected, but its
agent decisions cannot be reviewed from the available data; zero records found
does not prove it was never used.

## Session-by-session findings

IDs below identify local transcripts, not inferred authors of Git changes.
Dates describe session openings in the user's timezone; continuations can be later.

| Client / session | Work and useful practice | Lesson for tooling |
| --- | --- | --- |
| Codex `01a0e85a…` (Sep 28) | Initial disc analysis and native resource/course work; user explicitly redirected to native macOS/Android. | Make target platform and the distinction between game, viewer and Wine reference visible at entry. |
| Codex `01a0fb50…` (Oct 1) | Lifecycle/input, codecs, packaging, original raster/road-panel comparisons. Reported hardware gaps and incomplete port clearly. | Preserve scoped evidence; component pixel equality is valuable but does not establish a complete native race. |
| Codex `01a0fb51…` (Oct 1) | Run instructions and game launch. | One reliable game launcher and a separately named viewer prevent user confusion. |
| Codex `01a129ac…` (Oct 10) | Owner clarified first level means opening Napa; traffic, actor animation, crash/police helpers and combat captures. | Carry milestone and acceptance across clients; arithmetic/field-width pitfalls deserve durable guidance. |
| Claude `0e73bf9b…` (Sep 28) | Wine/Winlator stopgap, handheld controls and shortcut debugging; validated path escaping against the crash. | Archive legacy setup context separately; its patched EXE must never become the native oracle reference. |
| Claude `b928808a…` (Sep 28) | Native road/bike/rider/AI/contact/combat work, plans and playable Napa branch. Explicitly documented approximations and absent original comparisons. | Plans need actual interface checks and task-level fidelity evidence, not just passing synthetic tests. |
| Claude `f2bc4a56…` (Oct 2) | Oracle DLL/proxy setup; ordinal-forwarder limitation handled with a trampoline. Interrupted before completion. | Handoffs need installed DLL state, capture process ownership and next action. |
| Claude `e3f45668…` (Oct 2) | Text/fonts, original menus, career/save format, audio and original display-list raster replay. Usage-limit stop explicitly distinguished incomplete port; capture finished later. | Persist capture receipts and completion validation so later sessions can resume safely. |
| Claude `bbf53391…` (Oct 11) | Audited stale docs/architecture; plans 10–15; later diagnosed owner playtest problems and wrote plan 11a. Identified weaker RNG/demo replay acceptance. | Central status needs freshness and evidence; shared doc edits also need ownership. |
| Claude `c09df177…` (Oct 11) | Plans 16–22 for gameplay, all courses, settings, rendering/upscaling and hardening. Owner clarified higher resolution means internal rendering. | Keep fidelity baseline separate from optional enhancements; validate plan assumptions and dependencies before execution. |
| Claude `fd2d5677…` (Oct 12) | Added Makefile; caught that default course/bike flags bypass menus and removed defaults. Disclosed dry-run-only verification. | Keep command wrappers thin and test command semantics; preserve this existing Makefile rather than replace it. |
| OMP `01a12a3e…` (Oct 11) | Continued plan-10 hooks and first simulation capture. | Long oracle runs need reliable preflight, timeout, completion checks and resumable metadata. |
| OMP `01a12a57…` (Oct 11) | Extensive simulation/ring/AI tracing and demo-hold work; many hypotheses refined against probes. | Turn hypotheses into a small evidence ledger and first-divergence experiments; avoid broad shared-file rewrites during concurrent work. |
| OMP `01a12ed2…` (Oct 11) | Four parallel slices, tree stabilization and build-lock coordination; verified single-anchor failure. | File lists were insufficient where a separate session still edited the same tree; worktrees and one integrator are needed. |
| OMP child `SingleAnchorReplay` | Default single RNG anchor, diagnostic reanchor flag, first-divergence report. Found "human" capture still uses demo AI. | Assert the executed control branch in capture metadata; naming a scenario is not proof. |
| OMP child `OptionsTrace` | Docs-only options/pause research with bounded file ownership; avoided builds. | Small research slices work well; publish their conclusions under `docs/`, not only pending `analysis/` notes. |
| OMP child `BikeShadowNodes` | Trace and reproduction notes; identified missing state in dump-v3 blocking native actor comparison; request-budget stop. | Define required capture schema before assigning parity work, and leave an explicit incomplete handoff. |
| OMP child `CarCollision` | Contact tracing; scratch capture returned header only after a crash; restored shared DLL. | Isolate oracle state and reject incomplete captures atomically; prevent process/DLL interference. |

## Priority findings

### 1. Shared workspace interference is the largest avoidable cost

This is demonstrated, not hypothetical. `docs/TOOLING.md` records two broad
staging incidents: `37c4597` includes traffic sources in a simulation-snapshot
commit, and `51cb898` includes another session's plans 10–15 in a traffic commit.
Both are confirmed by `git show --stat`. Claude's review reports overwritten
uncommitted index edits. The latest OMP parent and children record a broken
`race.hpp` scope, dropped declarations, duplicate helpers and build-lock races
while another session was editing the shared simulation files. History alone
does not prove which individual edit caused every break.

**Change:** prefer a worktree and build directory per implementation worker.
Where shared-tree work is necessary, one session owns each shared file and one
integrator owns final builds, staging and summaries. Documentation is a shared
resource too. A `/tmp/rr-build.lock` cannot protect source edits; a lock's age is
not evidence its owner is dead. Use owner PID/session metadata and a resource key.

### 2. A green test suite can conceal missing fidelity

Current `CMakeLists.txt:71` marks `oracle-sim` with `WILL_FAIL TRUE`, and missing
fixtures can skip with code 77. CTest success therefore cannot establish parity;
an unrelated ordinary failure may satisfy the expected-failure property.
`tests/oracle_sim_test.cpp` now defaults to one RNG anchor, but solo construction
still explicitly invokes `set_demo_hold`.

The latest OMP `analysis/sim-notes.md` reports a tick-120 speed mismatch, original
1479 versus native 1474, with RNG equal. Its new `sim-napa-solo-human` capture
still exercises demo AI and ignores the recorded human control words. These are
reported historical results, not independently reproduced in this review.

**Change:** split diagnostic replay from strict acceptance. A diagnostic expected
failure must match an exact known signature; acceptance must demand the required
fixtures and return failure on any divergence. Emit pass/skip/known-divergence/
unexpected-failure counts. Require capture metadata to demonstrate the actual
player-control branch. Preserve component-original comparisons without inflating
them into end-to-end fidelity claims.

### 3. Knowledge is substantial, but entry points and status drift

Root `AGENTS.md` is one sentence; root `CLAUDE.md` is `@AGENTS.md`. Neither lives
in the native Git repository. Starting Codex inside `native-port/` can omit the
parent instructions under documented Git-root discovery rules. Keep a concise
canonical file at `native-port/AGENTS.md`, with routing guidance at workspace root
and a native Claude wrapper. Test actual instruction loading for all four clients;
do not assume identical discovery/import behavior. This layout recommendation is
based on [official OpenAI instruction-discovery documentation](https://learn.chatgpt.com/docs/agent-configuration/agents-md).

`00-INDEX.md` still has historical overview sections saying 53 tests/no police/no
original simulation checks, while later progress entries report 62 tests and
component checks. README also retains earlier placeholder/rendering and packaging
statements alongside newer sections. Some are intentionally dated snapshots, but
they are easy to mistake for current status. Detailed findings should remain
available without forcing every task to reread every document; this matches
[official guidance on concise, task-relevant instructions](https://developers.openai.com/blog/rethinking-skills-and-prompts-for-gpt-6-astra).

**Change:** one small current status/acceptance matrix with evidence links and
last-verified commit. Keep historical snapshots in the timeline. Worker-specific
handoffs under `docs/agent-handoffs/` prevent contention on PROGRESS/index edits.
The unmerged `analysis/{sim,nodes,options}-notes.md` are useful but do not satisfy
the existing instruction to document all progress under `docs/` on their own.

### 4. Tool success and capture success need explicit contracts

Session commands repeatedly pipe builds into `tail`; under a shell without
`pipefail`, the displayed exit code can belong to `tail`. Current
`tools/decompile_at.sh` ends its headless pipeline with `|| true`, suppressing
failure. `oracle/run.sh` ignores the Wine exit status and copies capture outputs;
it does not validate an end marker before reporting success. The CarCollision
child's three-line capture demonstrates why file existence is insufficient.

**Change:** preserve the subprocess exit code, retain full logs, and use a capture
validator before moving a temporary capture to its accepted path. Include
scenario, EXE hash, proxy hash, seed, controls hash, actual mode/branch, dump
schema, tick count and completion status in a receipt. Run captures in isolated
prefix/install/output state, or serialize all mutations. Stop only owned processes;
the child transcript includes broad `pkill -f ROADRASH.EXE` against shared state.

### 5. Packaging identity and device evidence need stronger provenance

The old viewer launcher and stale `.app` caused the owner's "two versions"
confusion. Commit `633522f` fixes launcher identity and adds `RRBuildCommit`
checks, so retain that solution. A HEAD-only stamp cannot distinguish two dirty
builds at the same commit. Add dirty-tree/source/config identity and show the
running binary path/build ID in diagnostic output.

APK assembly and the earlier working Winlator setup are different evidence from
native handheld gameplay. Most native sessions correctly disclose absent/locked
hardware. Keep that distinction in every acceptance report. Also correct plan
22's assertion that `tools/ci.sh` assembles both debug and release: the current
script assembles debug only. The separate release command must remain explicit
until the gate is changed.

## Proposed instructions and placement

[AGENTS.proposed.md](AGENTS.proposed.md) is a complete proposed canonical file,
ready to place at `native-port/AGENTS.md`. It is not active and changes no runtime
behavior. It retains the owner's documentation requirement and adds only rules
grounded in this review: scope, file ownership, evidence boundaries, verification,
safe captures and durable handoffs.

Recommended rollout:

1. Put the proposal in `native-port/AGENTS.md`; keep root `AGENTS.md` as a short
   workspace router instructing agents to read it for port work and identifying
   the legacy directories. Keep root `CLAUDE.md` and add native `CLAUDE.md` as
   thin imports of the corresponding instruction file.
2. Start a harmless instruction-summary task in each client from both workspace
   root and native Git root. Record which files were actually loaded. The
   OpenCode watcher log is evidence of discovery activity, not proof all rules
   were applied. OMP behavior remains to be checked explicitly.
3. Replace client-specific mandatory skill banners in plans with portable action
   and acceptance requirements plus optional skill suggestions. The current
   superpowers banners assume a Claude plugin unavailable in every client.
   This review did not invoke those historical plan instructions.
4. Keep detailed reverse-engineering gotchas in TOOLING and evidence docs. Add
   small scoped instructions under `oracle/` or parser areas only when a stable
   rule applies there; avoid duplicating the full project policy everywhere.

## Tooling backlog, in recommended order

All entries below are suggestions, not implemented tooling. Extend the existing
Makefile, CMake and scripts rather than introducing another competing command set.

| Priority / proposed addition | Behavior and acceptance |
| --- | --- |
| P0: instruction placement and startup check | Canonical instructions load in all four clients from both entry directories; wrappers stay short. |
| P0: workspace ownership and worktrees | Worker handoff records task, owned files, baseline and resources; overlapping writers cannot claim the same file. Independent build trees; integrator alone stages shared changes. |
| P0: strict `verify-sim` and diagnostic replay | Strict mode rejects missing fixtures, demo-only "human" captures and any mismatch. Diagnostic mode accepts only the recorded expected signature, not arbitrary nonzero exit. Report coverage independently of CTest color. |
| P0: oracle capture wrapper/validator | Configurable root/prefix, owned process timeout, serialized or isolated DLL install, unique output, complete/contiguous trace validation and atomic publish. A crash/header-only result preserves the last accepted capture and exits nonzero. |
| P1: `agent-doctor` / `make doctor` | Read-only preflight: Git root/dirty files, active resource owners, CMake/compiler/SDL, JDK/SDK/NDK, Ghidra, Wine/reference hash, pinned REA runtime and selected adb serial. Reports unavailable capabilities without installing or starting anything. |
| P1: gate profiles | Existing Makefile gains native, sanitizers, oracle acceptance, Android debug/release and packaging profiles. Each emits command, build config, commit/source identity, outcome and artifact path. Full cross-platform gate at integration, focused gates during iteration. |
| P1: status/evidence manifest | One machine-readable matrix: subsystem, implemented state, fidelity level, fixture/command, last verified source identity, remaining acceptance and device coverage. Generate current summary from it; keep timeline append-only. |
| P1: build provenance | Retain RRBuildCommit; add source fingerprint/dirty state/config/compiler. Running game and app expose this identity; packaging checks reject mismatched source/config. |
| P1: analysis index and exit-code cleanup | Address-to-symbol/field/evidence index, reuse existing exports, serialize Ghidra writes. Pin REA instead of `@latest` for reproduction. Headless/build failures propagate while full logs stay available. |
| P2: reusable session inventory | Read-only, project-filtered adapters for all four stores; snapshot counts/hashes/parse gaps; no credentials or raw transcripts committed. Missing/deleted OpenCode history is explicitly reported. |
| P2: planned C++ hardening | Implement existing plan 22 after its assumptions are corrected: synthetic parser fuzzing, sanitizer/TSan coverage, warnings/tidy. Scope regression checks to real failures and keep Android compatibility. |

Start with the four P0 items. A new agent orchestration framework or broad plugin
installation would not address the demonstrated failures as directly. Existing
bounded research assignments, original-fixture comparisons, explicit approximation
notes and honest hardware limitations are good practices worth retaining.

## Review validation and progress record

- Read-only session-store discovery and JSONL parsing completed; source snapshots
  have hashes and zero parse errors. OpenCode gap documented.
- Cross-checked instruction files, current Makefile, CMake test registration,
  CI/oracle/Ghidra/packaging scripts, evidence notes and the two interleaved commits.
- Added this report, the proposed instructions and the inventory under `docs/`.
- No source changes, commits, configuration installs, captures, builds or gameplay
  tests were performed. Historical green results are attributed to their sessions.
