# L5d tick-2267 crash-entry finish recompute (plan 10 Task 6)

Baseline: `14df8b2` (slot-rule lane-validity; L5/L5e green, L5d pinned at 2267).

## Symptom

`oracle-sim-field-l5d`: `first divergence at frame 2266 (tick 2267):
rider.9.raw_1f8 original 1455026 native 1455003` (23, finish-distance only;
all cars/riders/RNG otherwise identical, `RR_FIELD_ALL` clean).

Golden slot 9 at 2266/2267 is two physical riders swapping in
position-sorted order (overtake):
- Tick 2266: slot 9 pos 122947 raw 1455060 (sum 1578007 = finish+23);
  slot 10 pos 122584 raw 1455377 (sum 1577961 = finish-23, native rider 9).
- Tick 2267: slot 9 pos 122958 raw 1455026 (sum 1577984 = finish exact,
  native rider 9, +374 move, crash 0->1);
  slot 10 pos 122947 raw 1455060 (the other rider, unmoved, +23 kept).

Native rider 9 went -23 -> -23 (incremental preserved branch offset);
original went -23 -> 0 (recompute reset to trunk). Diff 23.

## Root cause (measured, REA + disassembly, no guessing)

- `0x00403d60` (crash entry) ends with `thunk_FUN_00407410(iVar2)` on the
  crash body (`analysis/decompiled/00403d60.c:135`). Native `crash_entry`
  and `Race::crash` copied the body and synced the mirror but never
  recomputed `finish_distance`.
- `0x00407410` (REA 6.4.0 `function 0x00407410`, Ghidra 12.1.4, Node
  v25.8.1 via `/opt/homebrew/bin`, `JAVA_HOME` openjdk@21; whitespace
  match to `analysis/decompiled/00407410.c` for the head/direct path;
  full pseudocode in `/tmp` REA dump, disc hash verified
  `73d7f974b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`):
  when `DAT_004b6e58 != 0` and body `+0x2c != 0`, direct
  `finish - body_pos`; else the node-length walk. For this live crash
  (finish known, slice present) direct applies.
- `0x004060b0` disassembly (`analysis/original-disassembly.txt`
  `4060e4-4060f2`, `406165-40616e`): the same-node incremental vs
  recompute gate compares `*(slice_ptr)` (first DWORD = next pointer)
  before/after the `0x0043a580` move, not node IDs. Native compared
  `road[sample].node`, so it missed the crash-entry reset (and would
  miss any slice-relink handover). The finish walk itself (branch-aware
  `+23/-23/+298` offsets seen from tick 2240 onward) stays unported;
  this slice ports only the crash-entry direct reset.
- `0x00406890` (`analysis/decompiled/00406890.c:15-16`): crashed-half
  callback does `+0x1f8 += +0x30 - +0x1c` (incremental body delta).
  Native `crash_body_motion` already does this
  (`body_before - body_after`). With the entry reset, both sides then
  integrate the same body delta (half_mirror/body positions already
  match, verified by `RR_FIELD_ALL` and `RR_FIELD_TRACE=9`).

## Fix (committed in this slice)

`src/race.cpp` `crash_entry` and `Race::crash`: after `sync_half_mirror`,
when `finish_q8_ != 0 && bike_slice_present` (proxy for body `+0x2c != 0`
on a live rider), set
`r.finish_distance = finish_q8_ - body_pos`
(`r.crash_actor.motion.i32(0x1c)`; equals bike pos at entry).
Mirrors the `0x00403d60` tail direct path; no walk, no change when
finish unknown or slice absent (synthetic/parked keep prior behavior).

## Verification (owned build `build-agent-crash-2267`, Debug)

- `cmake -S . -B build-agent-crash-2267 -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, build `oracle-sim-test`: exit 0.
- L5d: 2267 gone, advances to tick 2278
  (`rider.10.lateral original 62008 native 49976`, riderless-bike
  lateral for the same crashed rider; finish now matches at 2278).
  Re-pinned as known-divergence in the same commit
  (`tools/sim_known_divergences.json`: divergence 2277/2278, output
  `17af24b1…cb3f22`, reason documents the crash-entry fix + the open
  riderless-bike lateral next slice; diagnostic only).
- No regression: L1 `2400 identical`, schema2 `2400 identical`,
  L5 `2400 identical`, L5e `2400 identical (+43 golden-only)`,
  strict human `sched-human-cov2-20261017` `1431 identical`.
- Full `ctest --test-dir build-agent-crash-2267 -j8`: **107/107 passed**
  (receipt in `VERIFICATION.md`).
  LSP: `tools/agent_lsp.py --check src/race.cpp` runs; only the known
  Apple-clangd ExtractFunction tweak FAILs (no parse/type diagnostics),
  same tooling limit as prior slices. `rg`/`fd` used for search.

## Remaining

- L5d now diverges at tick 2278 on riderless-bike lateral (crashed
  rider 9, now slot 10: bike lateral 49976 vs 62008, steer 0 vs -2;
  pos/speed/crash/finish match). Pre-existing (hidden behind 2267),
  not caused by this fix (fix touches only finish at entry).
  Next: trace `0x0043a580` riderless-bike move / `0x0043a610` walls
  lateral for the crash bike (steer source, wall clamp, surface),
  with REA for `0x0043a580` + disassembly for the lateral path.
- Uncrashed `0x004049c0` trailing `0x00407410` (line 267-268) not
  ported (would reset branch offsets on every non-crash contact).
  Left open deliberately: L1 passes without it; port only with a
  contact-driven replay pin.
- Full `0x00407410` walk (branch-aware remaining distance) not
  ported; native still uses direct/incremental. Needed for
  split-branch riders generally (the `+23/-23/+298` offsets), plan-17
  ring/slice work. This slice only resets at crash entry.
- `tools/sim_known_divergences.json` L5d pin re-pinned 2267 -> 2278 in
  the same commit (divergence + output `17af24b1…cb3f22` + reason
  documenting this fix and the open lateral next slice). No golden changed.
