# Plan 10 Task 6 research: 0x00436a40 early-return guard (+0x144) — READ-ONLY

- Task: Plan 10 Task 6 research on the newly noted `0x00436a40` early-return guard.
- Baseline: HEAD `7992dfe` (`git rev-parse HEAD`), tree untouched except this file + scratch.
- Mode: read-only. No tracked file edited, no builds, no Wine captures, no Ghidra writes.
- Date: 2026-10-16.

## 1. Guard semantics (pinned REA)

Pinned call (one call, exit 0; byte-identical to the existing capture):

- `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`
  `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`
  `JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`
- `npx --yes rea-agents@6.4.0 function ../cdrom/ROADRASH/ROADRASH.EXE 0x00436a40 --format md`
- EXE sha256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (verified locally).
- Fresh output `analysis/separate144-20261016/rea-436a40-fresh.md` is byte-identical
  (`cmp` clean) to `analysis/codex-contact893-20261016/rea-436a40-20261016.md`
  (same `evidence_id ev_227a4647…`). All addresses below confirmed in both.

Guard, both directions, at function head (`__fastcall`, `param_1`=ECX=outer, `param_2`=EDX=partner):

| Address | Instruction | Meaning |
|---|---|---|
| 0x436a45 | `MOV EAX,[ECX+0x144]` | load outer+0x144 |
| 0x436a4c | `CMP EAX,EDX` | outer+0x144 == partner? |
| 0x436a53 | `JZ 0x004373e4` | early return, direction 1 |
| 0x436a59 | `MOV EAX,[EBX+0x144]` | load partner+0x144 (EBX=EDX saved at 0x436a4f) |
| 0x436a5f | `CMP EAX,ESI` | partner+0x144 == outer? (ESI=ECX saved at 0x436a51) |
| 0x436a61 | `JZ 0x004373e4` | early return, direction 2 |

Early-return target `0x4373e4` (`LAB_004373e4`) is the epilogue
(`POP EBP/EDI/ESI/EBX; ADD ESP,0x20; RET`). Firing the guard skips the entire
body AND the tail: the 5-word previous-position copy (`0x4373c6..0x4373db`,
`[0x1c..0x2c]→[0x30..0x40]`) and the `DAT_004977b4 = 1` write at `0x4373dd`.
So it is a pure no-op return — no mass exchange, no `+0x19c` flag clears, no
"separation ran" flag for the caller.

+0x1a0 partner-link store sites (caller `0x004368f0`, NOT in `0x00436a40`):

| Address | Instruction |
|---|---|
| 0x4369ea | `MOV [ESI+0x1a0],EDI` (outer+0x1a0 = partner) |
| 0x4369f0 | `MOV [EDI+0x1a0],ESI` (partner+0x1a0 = outer) |
| 0x4369f6 | `CALL 0x00436a40` (ECX=ESI outer, EDX=EDI partner) |

Post-call handler dispatch reads `+0x1a4`, not `+0x1a0`:
`0x4369fb MOV EAX,[ESI+0x1a4]` / `0x436a09 MOV EAX,[EDI+0x1a4]`; `CALL EAX`
(`piVar2[0x69]`/`piVar4[0x69]` in pseudocode). Decompiler dword index `[0x68]`
= byte `0x1a0`; `[0x69]` = byte `0x1a4` — verified against assembly, no
element-scaling error.

Critical asymmetry: the guard reads `+0x144`, but the caller links `+0x1a0`.
They are different fields (dword indices `0x51` vs `0x68`). Inside `0x00436a40`
itself `+0x1a0` is never read — the partner links are write-only from the
separator's perspective. No writer of `+0x144` is visible in either REA capture
(`0x004368f0` writes only `+0x1a0`; `0x00436a40` writes `+0x190` mass words,
impulses, `+0x19c`, position words). Candidate `+0x144` writers (contact
handlers via `+0x1a4`, or the second caller `0x00437660`) are an open REA
question — needs an xrefs-on-write query for `+0x144`, not done here.

## 2. Native side (read-only)

`Race::separate` = `src/race.cpp:370-511`, `Race::collide` = `src/race.cpp:513-518`
(`separate(ra, 4, rb, 4)` then `contact` both ways). Findings:

- No `+0x144` guard: `separate()` starts directly with the `+0x194` mass
  exchange (`src/race.cpp:374-375`, `p2.i32(400) = m1; p1.i32(400) = m2`).
- Native DOES write the `+0x190` mass words (decimal 400 = 0x190), matching the
  original (`0x436a6d MOV [EBX+0x190],EAX` / `0x436a7d MOV [ESI+0x190],EAX`).
  The "never writes" gap is specifically the caller-side `+0x1a0` partner links:
  the native pair loop (`src/race.cpp:2242-2296`) and `collide()` write nothing
  at bike `+0x1a0`.
- Struct mapping for `+0x144` / `+0x1a0` on the bike object: NONE. `Bike.m` is
  `0x340` bytes (`src/bike.hpp:47`), so both offsets are addressable, but:
  - `0x144`: zero touches in `src/` (rg for `0x144` hits only the UI string
    `"1920x1440"` in `native_strings.cpp:9`). No native field corresponds.
  - `0x1a0`: only `src/crash.cpp:110`, which zeroes it on `rider.motion`
    (the CrashActor motion copy at crash entry — a different object, not the
    live bike). `src/race.hpp:141` `AiState::current2 // +0x1a0` is BRAIN-space
    (`+0x1a0` relative to the AI brain struct), unrelated to bike-object
    `+0x1a0`. Do not conflate.
- Frozen-state hydration (`tests/frozen_state.hpp:125-131,186-198`): `w2c`
  (bike `+0x2c`) → `Rider::bike_slice_present`; half `w2c` or `half2c`
  (half `+0x2c`) → `half_slice_present`; half lateral/position/speed →
  `half_lateral/half_position/half_speed` (`src/race.hpp:190-192`). No
  `+0x144`/`+0x1a0` keys are consumed anywhere in hydration; `step_frozen_schedule`
  (`src/race.cpp:1806-1816`) touches only `+0x18/+0x0c/+0x10/+0x14`.
- Oracle capture today: per-step rider fields (`oracle/aweman_proxy.c:420-479`)
  cover `0x1c/0x28/0x20/0xf4/0xec/0x23c/0x2f0/0x2f4/0xc0/0x140/0x288/0xf0/0x10c/0x160`
  plus extras (`0x18/0x0c/0x10/0x14/0x240/0x114/0x260/0x23c/…`) — NOT `0x144`,
  `0x190`, or `0x1a0`. Frame-1 `nodeaddr.*` has `w2c/half2c/link0/link1` but no
  `+0x144/+0x1a0`.

## 3. Can the guard fire for the sched900 892-893 pair?

Unknown from all existing runtime evidence — and that is the answer. Deciding it
requires, at ticks 892/893, for both objects of the colliding pair:

1. bike `+0x144` of each object (the guard operands), and
2. bike `+0x1a0` of each object (whether the caller even linked this pair — if
   `0x004368f0` never paired them, `+0x144` can only match via some other writer).

Neither word is captured by any current proxy key (see §2), and
`analysis/codex-contact893-20261015/capture893-selected.json` frames 892/893
carry only `rider.0.*`/`node.4.*` physics fields. Concretely: `rider.0.position`
30061→30093 vs frozen `node.4.position` 30208 tells us the pair is longitudinally
close, but says nothing about `+0x144/+0x1a0`. Note also the guard compares
`+0x144` against the partner OBJECT POINTER, while the caller stores partner
pointers at `+0x1a0` — so even a `+0x1a0`-matches-partner observation would not
imply the guard fires; only a direct `+0x144` read decides it.

Next evidence (out of scope for this read-only task, needs proxy/test owners):
extend the oracle per-step rider dump with `bike_144`/`bike_19c`(opt)/`bike_1a0`,
or take a debugger snapshot of both rider bike objects at tick 892/893 in the
sched900 run; plus an REA xrefs query for all writers of bike `+0x144` (to learn
whether any live path can ever arm the guard for bike-bike pairs).

## 4. Why no native change is justified yet

- Firing condition is currently unsatisfiable-by-evidence: no captured
  `+0x144` value exists for any tick, and no known writer arms it on the
  bike-bike path (`0x004368f0` writes `+0x1a0`, a different field).
- Adding a `+0x144` early-return natively would require inventing a writer and a
  stored value — pure speculation, and it would change `separate()` outcomes
  (skipping position/impulse writes AND the `DAT_004977b4` equivalent) on the
  exact ticks under divergence investigation. That risks masking the real
  sched900/893 gap.
- Same for `+0x1a0` partner links: write-only from the separator's view; nothing
  in `0x00436a40` reads them, so emulating the stores cannot change any compared
  field today. If a future reader of `+0x1a0` is found (handlers, `0x00437660`
  path), revisit.

## 5. Checks run

- Pinned REA `0x00436a40 --format md`: exit 0, output identical to prior capture.
- `python3 tools/agent_lsp.py --check src/race.cpp` (read-only, no CDB/build
  changes): clangd parses `race.cpp` clean — 38 reported errors are all
  `tweak: ExtractFunction` self-test noise, zero real diagnostics
  (full log: `analysis/separate144-20261016/lsp-check-race.json`).

## 6. NOT-changed list (explicit)

No edits to any tracked file. In particular, untouched per instructions:
`docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`,
`docs/plans/10-simulation-oracle.md`,
`docs/plans/32-completion-review-next-steps.md`, `src/race.cpp`, `src/traffic.*`,
`src/terrain.*`, `src/race_setup.cpp`, `oracle/aweman_proxy.c`, `CMakeLists.txt`,
`tests/oracle_sim_test.cpp`, `tests/traffic_test.cpp`, `tests/race_setup_test.cpp`.
No commits, no builds, no Wine captures, no Ghidra writes.

New files (only these):
- `docs/agent-handoffs/separate144-guard-20261016.md` (this file).
- `analysis/separate144-20261016/` (untracked scratch):
  `rea-436a40-fresh.md`, `rea-436a40-fresh.stderr`, `lsp-check-race.json`.
