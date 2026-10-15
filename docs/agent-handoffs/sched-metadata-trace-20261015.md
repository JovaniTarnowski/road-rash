# Follow-up: frozen scheduler metadata trace — driver REA + writer audit (plan 10 Task 6 fix order #1c) — October 15, 2026

Read-only follow-up to `sched-cadence-20261015.md` (§5–6) and
`sched-interval-writer-20261015.md`. No source edits, no captures, no shared builds.
Baseline: `6bbde89` + SIM-worker uncommitted tree (preserved untouched).

## 1. REA spot-check (pinned 6.4.0, this session)

Env per `docs/TOOLING.md`: node v24.13.0 (`$HOME/.nvm/.../v24.13.0`),
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=openjdk@21`. EXE sha256 `73d7f974…9a60f18a2` (matches
`analysis/inventory.json`), provider Ghidra 12.1.4.

- Call 1: `function ... 0x00438170 --format md` → exit 0 (metadata only).
- Call 2 (same target, `--filter-output raw_result.value.pseudocode`,
  saved `/tmp/rea-438170-pseudo.md`, local only): exit 0.
- Result: REA pseudocode for `0x00438170` is IDENTICAL to
  `analysis/decompiled/00438170.c` (all 89 lines: param_1==0 loop, four
  spatial-band loops, `+0x14 > 6` tail scan, player-node tail, list
  rebuild/sort calls). Combined with the prior session's `0x00437e60`
  confirmation, both scheduler functions are now REA-verified; no third
  call needed.

## 2. Writer/reader audit (`rg`, element-scale corrected)

Scheduler-node (`+0x0c/+0x10/+0x14/+0x18`) writers are confined to three
sites; all other `+ 0xc) =`-style hits in `analysis/decompiled/` are
different structs (actor templates via `*param_1`, container links):

- `0x00437e60` head (`00437e60.c:11-13`): `+0x10 = tick`,
  `+0x18 = tick − old_last`. Runs on EVERY scheduled invocation, including
  nulled (frozen) nodes — the `+0xe4 == 0` jump (`:17`) is AFTER the head.
- `0x00437e60` tail (`:153`, `LAB_00438108`): `+0x0c = +0x14 + tick`.
  Also unconditional on scheduled runs. Disassembly `438108-438113`
  (`movl 0x14; addl %edi; movl → 0xc`) confirms.
- `0x00438930` init (`00438930.c:22-24`, int-indexed: `piVar1[5]/[4]/[3]` =
  bytes `0x14/0x10/0x0c`; `piVar1[6]` = byte `0x18 = 0` at `:26`).
  Disassembly `43897f-4389a2` confirms byte-for-byte:
  `0x8(%esi)→0x14(%ebx)`, `+0x10 = DAT_004b7ecc − interval`,
  `+0x0c = DAT_004b7ecc`, `+0x18 = 0`.
- `0x00438170` tail (`00438170.c:79-80`, NEW vs prior handoffs): every
  frame rewrites `+0x0c` of the player bike node
  (`*(DAT_004642d8+0x2fc)+0xc = tick+1`) and mirrors it to
  `DAT_004642d8+0xc`. These two words churn independently of the cadence
  loop — the oracle dump should capture or explicitly exclude them.

Readers: `00438170.c:21,31-32,43-45,54-55,64-65` (gates, all signed
`int` compares) and the `+0x14 > 6` tail scan (`:72`).

CORRECTION to `sched-interval-writer-20261015.md`: it labels
`00438930.c:18-19` (`piVar1[0x38]/[0x39]`) as "bytes `0xe0/0xe4`,
think/move pointers". Correct mapping is `piVar1[0x37..0x3a]` = bytes
`0xdc/0xe0/0xe4/0xe8` copied from template `+0x0c/+0x10/+0x14/+0x18`
(disassembly `43895b-438979` confirms `0xc→0xdc, 0x10→0xe0, 0x14→0xe4,
0x18→0xe8`). The conclusion (sole interval writer = template dword +8)
is unaffected.

## 3. Signedness / wraparound (disassembly-authoritative)

- Head: `437e69 subl 0x10(%esi),%eax` — signed 32-bit, wraps mod 2³².
  `+0x18` is a signed tick delta; negative only if ticks regress (not
  observed in the solo recipe).
- Tail: `43810f addl %edi,%eax` — unsigned/signed-identical 32-bit wrap;
  no clamp. Overflow irrelevant at tick ~900.
- Gates: `cmpl`+`jle/jge/jl` throughout (`4381bf-4381c4`,
  `43821c-438221`, `438223-438228`, `438238-43823a`) — all SIGNED
  (`<=`, `<`, `< tick−2`). Native port must use signed int compares.
- `+0xc4` masks use `testb $0xc/$0x3c, %al` — unsigned bit tests on the
  low byte; widths: all four scheduler words are 32-bit `int`.
- Range-gate slice callback (`param_3`, `438116-438163`): signed distance
  compares vs `0x1e00/0x800` thresholds; frozen nodes still take this path
  (it lives in `LAB_00438108`, past the null-move jump).

## 4. Driver gate semantics (confirmed, REA-identical)

- `param_1 == 0` path: run node iff `+0x0c <= tick` (signed).
- Spatial-band paths add `+0x10 < tick` alternates for flagged nodes:
  bands 1/2/4 require `+0xc4 & 0xc` (bands 1,2,4-first-half) or
  `& 0x3c` (band 3) AND `+0x2c != 0` (bands 1,2,4-second-half); band 2 has
  the extra `+0x10 < tick−2 && +0xc4 & 0x3c` OR-leg (`:45`).
- SPECULATION (labelled): the solo/`codex-sim-frozen900` recipe is
  believed to take the `param_1 == 0` path (single green anchor, no
  spatial bands), so frozen grid bikes gate purely on `+0x0c <= tick`
  with interval 4 → runs at ticks 1,5,…,205,209. UNPROVEN until the
  oracle record lands (open unknown #3 stands).

## 5. Oracle dump extension needed (unchanged + one addition)

Per `sched-cadence` §6: extend per-node dump with `+0x0c/+0x10/+0x14`
(keep `+0x18`), wire through `oracle/sim_fields.h` + `tests/sim_fields.hpp`,
capture one `codex-sim-sched900` on the frozen900 recipe. ADDITION: also
dump (or document-as-volatile) the two driver-tail words
(`playerbike+0x0c`, `DAT_004642d8+0x0c`), which rewrite every frame
outside the cadence loop. Expect frozen-slot intervals 4, lasts
1,5,…,205,209. Do NOT hardcode interval 4 natively.

## Checks

- `git status --short`: only pre-existing SIM-worker modifications + this
  file; no source touched; no builds run; no captures; no commits.
- REA: 2/2 exit 0 (one target, metadata + pseudocode filter).
- Reads: `00438930.c`, disassembly ranges `437e60-437ebf`,
  `438108-43816b`, `438170-4382ae`, `438930-4389e2`; `rg` writer/reader
  scans over `analysis/decompiled/`.
- NOT done: oracle extension, native `sched_*` edit (fix owner + SIM
  integrator coordinate; `oracle/*`, `tests/oracle_sim_test.cpp` have live
  claims).
