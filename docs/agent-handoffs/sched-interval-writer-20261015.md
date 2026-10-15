# Follow-up: build-time scheduler interval writer (plan 10 Task 6) — October 15, 2026

Closes open unknown #1 of `sched-cadence-20261015.md` (§5): which build-path
function writes bike word `+0x14` (scheduler interval).

## Finding (subagent trace, read-only; element-scale corrected)

- `analysis/decompiled/00438930.c:21-22`: `piVar1[5] = *(param_2 + 8)` —
  byte `+0x14` from template-image word at byte +8. Disassembly
  `analysis/original-disassembly.txt:43897f-438984`
  (`movl 0x8(%esi),%eax; movl %eax,0x14(%ebx)`). Sole writer in the build
  chain: `0x004387a0` calls it (`4387b5`) but writes only `>= 0xec` words
  itself; `0x00405990`/`0x00405c80`/`0x00405ea0` write no scheduler-header
  words (range-scanned `405990-405c7f`; `00405ea0.c:71` `+0x2c4` is hex
  0x2c4, not decimal confusion).
- Same function inits byte `+0x10` (`00438930.c:23`: `DAT_004b7ecc - iVar2`)
  and byte `+0x0c` (`:24`: `DAT_004b7ecc`), plus byte `+0x18 = 0` (`:26`).
  The `0x00437e60` head rewrites all three every scheduled run.
- Value still open: depends on the template image
  (`DAT_00463ad0/d4` via `00405990.c:15-16`, other `438930` callers at
  `41ed95/41efd2/454649`) and on whether a live tail recompute overwrote
  it before the freeze (unknown #2 stands). Do NOT hardcode interval 4.

## Next (unchanged)

Extend the oracle dump with `+0x0c/+0x10/+0x14`, capture on the frozen900
recipe, and cross-check the measured interval 4 against template dword +8
before touching `src/race.*`. No REA call was needed (pseudocode
non-suspect, disassembly authoritative throughout).
