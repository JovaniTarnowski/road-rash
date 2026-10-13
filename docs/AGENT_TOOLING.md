# Agent tooling

Implemented October 12, 2026 from the P0/P1 items in
[the session review](agent-review/REVIEW.md). The canonical instructions are
`native-port/AGENTS.md`. `CLAUDE.md` imports them; the parent workspace files route
to this repository. File presence and imports are checked locally; actual
instruction loading in each external client must be checked in that client's
session. No client preferences/plugins were installed or modified.

## Start a session and claim ownership

Check Git status and the current [evidence matrix](CURRENT_STATUS.md), then use
a unique session name and a **long-lived owner PID** (your terminal/agent process,
not a one-shot subprocess):

```sh
python3 tools/agent_workspace.py list
python3 tools/agent_workspace.py start renderer-42 --owner-pid 12345 \
  --task 'Fix actor projection' --files src/actor_render.cpp tests/actor_render_test.cpp
```

The command claims files and writes `docs/agent-handoffs/renderer-42.md` with
baseline, build directory, token, checks and handoff sections. Update that file
during work. It rejects overlapping file/directory ownership. To create an
isolated detached worktree at an explicit ref, add:

```sh
--worktree /absolute/path/to/rr-renderer-42 --ref HEAD
```

Dirty edits are **not copied** into a new worktree. Build there only after its
dependencies/assets are available; commercial data stays outside source.
No checkout is automatically removed and no commit is automatically created.

Release with `agent_workspace.py release TOKEN --session renderer-42`.
`reap TOKEN` requires the recorded PID to be gone; there is no age-based
expiry. A reused live PID prevents reaping. The registry is under the user's
temporary directory (`RR_AGENT_STATE` overrides it). If a registry mutation
crashes leaving its mutex, inspect its `owner.json` and remove that mutex only
after verifying the owner is gone.

These claims coordinate participating tools. Unmanaged editors, old sessions
and direct `cmake`/Wine commands can bypass them; do not assume they acquire
claims automatically. The registry is shared across local worktrees, uses
canonical absolute paths and treats parent/child directories as overlapping.
Do not run tools as different users against the same shared resources.

## Doctor and evidence lookup

`make doctor` is read-only: Git state, resource owners, tool paths/versions,
JDK/SDK/NDK, SDL, Ghidra, Wine, original EXE hash and instruction files.
It never installs tools, invokes REA or starts adb. If adb is already running,
it lists devices; `--adb-serial SERIAL` records the intended device. Listing a
device is not hardware verification.

Use existing exports before a new decompile:

```sh
python3 tools/analysis_index.py 0x004349c0
make analysis-index
```

The local address index maps pseudocode and documented address references to
evidence paths/lines. It does not infer signatures or claim runtime parity.
For REA spot-checks use `npx --yes rea-agents@6.4.0` with the environment in
[TOOLING.md](TOOLING.md); do not use `@latest` in reproducible evidence recipes.
The Ghidra wrappers now claim the project/export paths and propagate failures;
`decompile_at.sh` retains full output in `analysis/decompile-at.log` instead of
masking errors through `grep ... || true`. Full imports still overwrite the
analysis project: preserve annotations first.

## Verification profiles

Choose your session's build directory. Existing Makefile commands remain;
new profiles retain complete logs and JSON receipts:

```sh
make gate-native BUILD_DIR=build-agent-my-session
make gate-sanitizers BUILD_DIR=build-agent-my-session
make verify-sim BUILD_DIR=build-agent-my-session
make gate-android-debug
make gate-android-release
make gate-package BUILD_DIR=build-agent-my-session
```

The direct interface also supports focused CTest runs:

```sh
python3 tools/agent_gate.py native --build-dir build-agent-my-session --tests 'race|bike'
```

| Profile | Coverage |
| --- | --- |
| native | Configure/build + CTest; separate pass/skip/fail and simulation-diagnostic counts |
| sanitizers | ASan + UBSan configure/build + CTest in its own directory |
| oracle / verify-sim | Required player-control evidence, receipt/EXE/trace validation and zero-divergence replay |
| android-debug | AssembleDebug and APK inspection for original assets |
| android-release | AssembleRelease using explicitly selected local debug signing, asset inspection; not an owner-signed release |
| package | Static-SDL Release build, tests, source/config/binary identity, licences, ARM64/system dependencies and ad-hoc signature |

Receipts under ignored `analysis/agent-gates/<run>/` contain source commit and
fingerprint, build path/config, exact commands, exit codes, durations, logs,
coverage and artifact paths. A source change during a gate invalidates its pass.
The gate owns its build directory; Android gates serialize the Gradle project.
No tests matching a focused regex is an error. A command failure is not hidden
by output filtering. Gate receipts are local; do not commit disc-derived output.

`tools/ci.sh` runs native and Android debug profiles in `build-agent-ci` by
default. Set `RR_BUILD_DIR`, `RR_ANDROID_RELEASE=1`, `RR_SIM_ACCEPTANCE=1`,
`RR_CODEC_ORACLE=1` or `RR_STANDALONE_APP=1` for the corresponding extra gates.
Existing `build-release` apps must pass the stronger source-identity verifier;
old HEAD-only stamped apps need rebuilding.

### Diagnostic versus acceptance

CTest's `oracle-sim` no longer uses `WILL_FAIL`. Diagnostic success requires
either a full matching replay or **exact** recorded stdout/stderr and trace
hash from `tools/sim_known_divergences.json`, with exit code 1. Missing fixtures
return 77 and remain visible as skips. A changed mismatch, crash or extra error
fails the gate. A known divergence is never simulation acceptance.

Strict `make verify-sim` rejects missing captures/disc data, stale/missing
capture receipts, absent human-control proof, missing replay capability, skips
and mismatches. At implementation time the solo golden still uses demo AI,
and current replay forces demo-hold for solo scenarios. Strict acceptance is
therefore **expected to fail**, and the tooling does not pretend to fix physics.

Future human acceptance requires the proxy to emit `player.demo_hold=0` in every
captured frame, and a corrected replay binary whose `--capabilities` reports
`human-controls-single-anchor`. The current proxy and replay were being edited
by another session and were preserved. Declaring support is a contract: the
binary must consume the actual control stream and use a single RNG anchor.
Do not add that declaration without implementing and validating the path.

## Captures and shared analysis state

```sh
RR_ORACLE_FRAMES=2400 RR_ORACLE_SIM=1 RR_ORACLE_SEED=1 \
  oracle/run.sh sim-napa-session
```

The positional seconds argument is now a **deadline** (default 600s), not a
timer whose expiry publishes a killed capture. Require a frame-bounded capture
that exits cleanly. The wrapper claims the Wine prefix/install/output directory,
checks the reference EXE, uses unique temporary output names and stops only its
own process group on timeout. It does not mount discs automatically or change
an existing D: mapping to another directory.

For other environments use the direct CLI:

```sh
python3 tools/oracle_capture.py capture scenario --workspace /path/to/rr \
  --prefix /path/to/prefix --wine /path/to/wine --timeout 600
python3 tools/oracle_capture.py validate analysis/oracle/scenario.tsv --simulation
python3 tools/oracle_install.py --workspace /path/to/rr --prefix /path/to/prefix
```

Installation and DLL building participate in the same resource contract.
Capture validation demands magic, reference hash, complete frame sequence,
integer fields, end marker, expected frame count and contiguous simulation
ticks. It rejects instrumentation failures and changed DLL/control inputs.
Receipts contain proxy/EXE/trace/control hashes, seed, actual instrumented branch,
schema, frame/tick bounds and exit status. Environment variables and a scenario
name alone are not proof of the branch.

Validated runs are retained immutably in `<scenario>.captures/<run>/`; the
accepted receipt points there. Legacy flat TSV/companion files remain for existing
tests. Invalid or crashed runs leave accepted output alone and retain failure
logs under `failed-captures/`. For multi-file consumers, use the immutable run
named in the receipt; only the receipt pointer is atomically published as a set.

## Build identity and current status

Every CMake build regenerates `build-info.json` and the embedded header only
when needed. `build/rr-game --build-info` prints commit, dirty state, source
fingerprint, configuration and compiler **before SDL startup**. Source identity
includes tracked/untracked source, tests, tooling and build config, excludes
documentation/generated outputs, and detects deletions. Optional packaging
checks compare the embedded binary, manifest and current source; HEAD alone
cannot establish freshness. `RR_PACKAGE_BUILD_DIR` selects an owned package tree.

`docs/agent-status.json` is the current implementation/evidence matrix. Generate
[CURRENT_STATUS.md](CURRENT_STATUS.md) with `make agent-status`, or explicitly
record a gate:

```sh
python3 tools/agent_status.py --record-gate analysis/agent-gates/RUN/receipt.json
```

Recording a green gate does not upgrade fidelity claims or hardware coverage.
Each subsystem retains its remaining acceptance and evidence paths; historical
snapshots in PROGRESS/plans stay historical. Update one subsystem's evidence
when it is actually proved rather than repeatedly rewriting all status prose.

## Validation of this tooling change

The regression suite uses synthetic fixtures to check truncated/malformed traces,
branch proof, expected-failure signatures, strict acceptance, overlapping resource
ownership/reaping, capture failure preservation/publication, unmasked command
failure, source fingerprints and coverage accounting. Run it with
`python3 tests/agent_tooling_test.py` or CTest's `agent-tooling` test.

Current integration outcomes and limitations are recorded in the implementation
[handoff](agent-handoffs/2026-10-12-agent-tooling.md) and gate receipts. No live
Wine capture, Ghidra import, handheld test or owner-signed release is implied by
these synthetic checks. P2 session exporters and C++20/fuzzing/TSan remain separate
work from this P0/P1 implementation.

## Session inventories and LSP checks

The [follow-up review](agent-review/REFRESH-2026-10-12.md) includes the later OMP
threads and applies the shared-tree, research-supersession and budget lessons to
AGENTS. OMP remains the primary development client; read its current handoff and
coordinate source ownership before implementation.

```sh
make agent-sessions
make lsp-check
make agent-tools-test
```

Inventory output defaults to ignored `analysis/agent-sessions.json`. To preserve
a review snapshot and exclude the reviewing conversation, use:

```sh
python3 tools/agent_sessions.py --exclude-id REVIEW_SESSION_ID --out analysis/review-inventory.json
```

`--project` and `--home` can select another project/local store root. Filtering
uses project cwd metadata or OpenCode project IDs. Histories with unrelated cwd,
deleted/cloud-only data and attachments remain outside coverage. Appending
JSONL is read to a fixed observed byte count; incomplete final records are
reported separately. OpenCode reads a WAL-aware read-only transaction and checks
legacy session JSON. Inventory is metadata, not an automatic assessment of the
agent's decisions. Review the relevant histories before writing findings.

`--local-text-dir /private/path` explicitly exports user/assistant text for local
inspection, excluding tool arguments and binary attachments. The directory must
be outside the Git repository; raw histories can contain private information and
must not be committed. Defaults never export conversation text.

LSP preflight checks the ignored compile database for missing sources and newer
CMake files; nonzero status means refresh/check it, not that clangd is impossible
to use. `python3 tools/agent_lsp.py --check src/bike.cpp` adds an optional parse
with a 60-second bound. See [TOOLING](TOOLING.md) for the owned-build and client
reload recipe. Neither command configures builds or changes client preferences.
