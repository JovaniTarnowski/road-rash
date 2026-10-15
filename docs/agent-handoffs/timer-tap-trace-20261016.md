# Timer tap-word trace: lane-count words feeding 0x0044dd90

Date: 2026-10-16. Read-only RE trace in `/Users/archit/Dev/rr/native-port`.
Baseline: shared-tree HEAD (unmodified by this session; no commits).
No source edits, no Wine captures/runs, no build dirs touched.
One handoff file (this one); one pinned REA call (see §5).

Prior: `traffic-timer-trace-20261016.md` (0x0044dd90 byte-exact, timer init,
branch polarities). This note answers what WRITES the tap words it reads.

## 1. Writers of traversal-slice bytes +0x2c/+0x30

Object identity (element-scaling verified against disassembly throughout):
`DAT_0049e820` slots are 0x2F8-byte blocks (`004552e0`: stride `puVar3+0xbe`
ints). Two sub-slice objects are EMBEDDED at block +0x30 and +0x194
(`int*` arithmetic: `piVar2+0xc` = byte +0x30, `piVar2+0x65` = byte +0x194).
The timer (`44ddce movl 0x30(%edi)`, `44de5a/44def2 movl 0x2c(%esi)`,
where `%edi/%esi = *(slot+0xc)`) reads the **sub-slice's** +0x2c/+0x30.

- **Primary/only initializing writer: `FUN_00454240`** via
  `FUN_004528f0` — disassembly `4542e5 leal 0x30(%edi),%esi` /
  `4542f3 leal 0x2c(%edi),%ecx` / `4542f6 mov %esi,%edx` /
  `4542ff call 0x4528f0` with `%ecx` = lane iterator (`param_2+0x10`),
  `%edx` = sub+0x30, stack = sub+0x2c. `004528f0` (`452908`–`45291a`):
  null iterator → both words `0x200` (2 lanes); else
  **sub+0x30 = laneIter[5] `sarl` 8, sub+0x2c = laneIter[6] `sarl` 8**
  (Q16 accumulator floor-shifted to Q8 lane units; 256 = 1 lane).
  Side mapping: [5] = LEFT (NALR rec +8/+10), [6] = RIGHT (rec +9/+11),
  so branch A (oncoming) reads LEFT, branches B/C read RIGHT — matching
  native `left_q8` / `right_q8` use.
- **Build points** (when 00454240 runs): `00454c80` tail
  (`FUN_00454240(piVar2[sel], routepos, flag)`) ← `004552e0:62-76`
  race-init loop (128 slices from back, alternating `00454c80`/`00453030`)
  and ← `00454120:25-32` per-tick advance (one slice per 0x100 boundary
  crossing: `00453030(ea20)`, `00450f90`, `00454c80(oldfront, ee30)`,
  `00450f90`, `00453030(ee30)` — slice built at old-front base with the
  traversal iterator still at that base: no off-by-one).
- **Lane-iterator init**: `00452f50` (traversal setup) → `00452cb0(...,1)`
  → `00452820` forward init: [5],[6] = record-0 start bytes `<<16`,
  [2]=[3]=0, [4]=record0 ptr. (`00452d70` → `004528c0` → `00452820(...,1)`
  same shape.) Backward re-entry (`00453030`/`00453b60` with param_3=0)
  loads LAST-record end bytes instead.
- **Lane-iterator stepping**: `00452e10` → `00452920` (±0x100/sample),
  driven by `00453030` traversal advance. Forward semantics verified
  against `src/course.cpp walk_intervals` line-for-line: j<end add slopes,
  j==end load end bytes, enter next record only at j==start keeping values.
  `00452820` record layout (stride 0x14): +0 start, +4 end, +8/+9 start
  bytes L/R, +10/+11 end bytes L/R, +12/+16 Q16 slopes L/R — = NALR chunk.
- **Post-write adjuster: `FUN_004547e0`** (`00454c80` tail, when
  sub+0x2c != -1): ramps sub+0x30/+0x2c `+1` toward neighbor-sub-slice sums
  (`004547e0:32-40`), firing ONLY when the word's low byte is 0, and
  rewrites count bytes +0x34/+0x35 from neighbor sums (not timer inputs).
  Consequence: words may read native_value or native_value+1 on plateaus,
  but `>>8` counts are INVARIANT (increment from low-byte-0 never crosses
  a 256 boundary; ramp words with nonzero low bytes are untouched).
  Neighbor-link pointees (sub +8/+0xc) not fully resolved — partial.
- **Others ruled out**: `00450f90` (21 lines, no 0x2c/0x30 refs) never
  writes them. `00454890` writes `+0x30` only on stack/intersection
  scratch and runs only for iterator kind > 1 — not single-road taps
  (not fully traced — labelled).

## 2. Values at the tick-319 / tick-376 taps (Napa, course 4)

Golden `analysis/oracle/sched900-20261015.tsv` (read-only python3):
tick 319 `ring.front=35621` (prior "~35365" was front−0x100);
tick 376 `ring.front=36670`; tick 1 `ring.front=33280` (=12800+0x5000 ✓).
Tap bases/slots: 319 → spawn 35365 = sample **138**, slot
`((35365&0x7f00)>>6)=40`; 376 → spawn 36414 = sample **142**, slot 56.
Route starts with section 0 (link 0, len 700; first split at route 700),
so both taps are section-0-local samples on a single roadway.

Section 0 NALR (`SGS+0x1fe4`, 8 records; symmetric L/R): record 1 covers
samples 115–130 (start bytes 2,2; end bytes 1,1; slopes ∓4369); record 2
starts at 250. Python re-walk of the committed `walk_intervals` logic:

| j | Q16 | word (+0x30/+0x2c) | count |
|---|---|---|---|
| 115 | 0x20000 | 512 | 2 |
| 126–129 | 0x14445…0x11112 | 324…273 | 1 |
| 130–150 (incl. **138**, **142**) | 0x10000 | **256** | **1** |

Native `PathSample` there: `left_q8=right_q8=256`
(`q16/256`; `left=right=250` world units),
`traffic_lane_count(256)=int8_t(div256(256))=1`.
Both taps: original gate sees 1, native sees 1. Ring-slot freshness:
init built bases 512–33024; sample-138/142 bases (35328/36352) were built
by per-tick advances (front 33280→35621→36670 crosses them) — fresh,
traversal iterator in lockstep, single-road so +0xc = own-half sub-slice.

## 3. Equality statement (native `left_q8/right_q8` vs +0x30/+0x2c)

**Equal sample-for-sample wherever the walks agree, and the walks agree
everywhere reached going forward on a single roadway.** Evidence:
five-course NALR census (read-only python3 over disc `DATA/COURSES/*.CRS`
with the committed walk): **zero negative Q16 accumulator values in any
section of any course**, so original `sarl` floor-shift and native
truncating `/256` coincide bit-for-bit; null-lane sections agree too
(0x200 = 2 lanes both sides). Asymmetric L/R lanes are common (all
courses) but each side reads its own word on both implementations.

Where they CAN differ (no effect at 138/142):
1. `004547e0` +1 smoothing: original plateau words (low byte 0) may be
   native_value+1. Gate-count invariant (§1). Word-level, not count-level.
2. Dual-roadway split/merge windows: original tap follows the traversal
   +0xc sub-slice link (repointed by `00453030`/`00454c80` half selection);
   native always reads own-half route sample. Diverges where halves'
   NALR differ — SPECULATION (half-selection proof needs `00453030` work).
3. Backward travel: original `00452920` backward stepping + `00452cb0(0)`
   re-init vs native forward-only parse. Demo ticks here are forward-only.
4. Stale/aliased ring slots (wrap): none possible here (slots rebuilt per
   crossing; both taps fresh).

**Tick-319 inference** (arithmetic, capture-gated for timer state):
tap words agree (=1, gate passes both sides) and `rng_calls` is 0 on every
tick of sched900 (it is not a per-tick draw counter — informational only).
A fresh slice with count 1 means the original's tick-319 silence is NOT a
lane-gate effect: branch-B state did not go < 0 that tick while native's
did — i.e. the divergence is in timer-state history (per-tick distance
deltas / rate path), not in tap words. Consistent with 376 drawing on both
sides (branch-A first expiry with gate 1 at sample 142 on both).

## 4. Limits / next

- `004547e0` neighbor links (sub +8/+0xc pointees) and `00454890`
  intersection path not fully traced; dual-half tap selection needs
  `00453030` half-link proof before touching dual-zone spawn code.
- Branch identity of the 376 draw (A vs B) and the B-state histories need
  capture data (per-tick param_1 series); not re-derived here (no Wine).

## 5. Pinned REA call (one)

`npx rea-agents@6.4.0 function "../cdrom/ROADRASH/ROADRASH.EXE" 0x00454240
--format md` from `native-port/`, env per `docs/TOOLING.md`
(node v24.13.0, ghidra 12.1.4 libexec, JDK 21). EXE hash-verified
`73d7f974…a60f18a2` = `analysis/inventory.json`. Exit 0. Output piped
through `tail`: only the normalized value-flow tail observed
(truncated=false, omitted-ops lower bound 0); pseudocode block not
re-captured, so no new structural claim is taken from REA — the writer
evidence above rests on the project's disassembly (authoritative, byte
offsets verified) and decompile. Prior `0x0044dd90` REA note stands.
