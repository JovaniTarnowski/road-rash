# Tick-189 terrain next probe — October 15, 2026

Plan: 10 Task 5; research supporting `c38bcfb`'s next experiments.
Baseline: `5524bf6284a5fc8f3a169ccb26bac6948a78a4dd`.
Owner: Codex terrain_review subagent. Read-only shared-source review.
Owned/changed: this handoff and `analysis/terrain-next-20261015/` only.
Preexisting `src/traffic.*` edits and other workers' files preserved.

## Finding

No justified hill-step behavior change found. `src/terrain.cpp` matches
`0x00451380` and `0x004515a0` on inspection, including strict interior-window
skip, early return for zero clamped length, single record advance, signed
length division, and all-eight-point integration. Pinned REA confirms
`0x004515a0` whitespace-identical to existing export (2,098 vs 2,101 chars).
The next useful implementation is expanded isolated terrain-fixture coverage,
then read-only race walker snapshots if that comparison passes.

**Additional coverage hole:** current `capture_terrain()` and
`tests/terrain_test.cpp` compare only 6 exported points, but all 8 points own
windows and consume RNG. Points 3 and 7 can diverge and change call counts
without any difference in the exported six-point geometry. The fixture also
never compares RNG state/count, current hill-record index, or point windows.
Merely changing fixed values to seeded draws while retaining exported-only
comparisons leaves a material blind spot.

**Inference limit:** equal per-tick call counts through tick 188 prove the
stream cursor agrees at tick boundaries, not that individual walkers/points
received the same draws. Earlier within-tick reassignment/order differences
can leave latent windows that first change the count at tick 189. Setup
closure similarly proves total draws, not internal state. Existing wording
that excludes all earlier hidden drift on count parity alone is too strong.
This is a logical limit, not a measured earlier divergence.

## Experiment 1: expand isolated capture without new detours

Existing entrypoints in `oracle/aweman_proxy.c:1570`:
`0x004510c0` init, `0x00451910` export, `0x004516e0` step. Preserve current
constant-random goldens; add a separately keyed seeded suite and output file.

The existing live CRT helpers eliminate a need to patch the rand wrapper:

1. Obtain `sim_rand_state()` (`_getptd` at `0x004592b0`, state `ptd+0x14`).
   Save state, mode `0x004753b0`, and fixed value `0x0049bce8`.
2. Set mode to 0 (anything except 3 calls CRT in `0x00441750`) and load the
   chosen seed through the state pointer. Native uses the same recurrence
   `state = state*214013 + 2531011`, result `(state>>16)&32767`.
3. For each case, use the existing synthetic terrain payloads; record seed,
   full iterator state after init and each step, plus RNG state before/after
   init and every step. Comparing RNG state avoids requiring RANDCOUNT hooks.
   A wrapper counter may additionally record draws, but restore its globals
   if the isolated probe shares a later gameplay capture.
4. Compare record indices, 8 windows, 8 x/height pairs, 8 derivative pairs,
   exported geometry, and RNG states. Use at least seeds 0/1/123/32767 and
   several mixed 32-bit seeds. Full forward/backward traversals plus a
   forward-then-backward walk expose window preservation. Existing 48-step
   cases provide the first bounded pass; record-end crossings, fractional
   steps and repeated exact-endpoint steps extend it only if needed.
5. Restore all saved globals on every exit. Require complete header/end and
   unchanged reference hash before treating a capture as evidence.

No actual seeded oracle capture was run by this worker. No pseudocode
reimplementation has been presented as an independent oracle.

## Experiment 2: exact race walker snapshot locations

The names `0x0049ea20` (back) and `0x0049ee30` (front) refer to OUTER 0x410
walkers, not section walkers. This distinction matters for probe offsets.
`0x00452f50` initializes first section state at outer+0x28 and second at
outer+0x21c. `0x00453030` chooses the active section as:

```
section = outer + 0x28 + RD32(outer+4)*0x1f4
terrain = section + 0xb4
```

In ordinary single-road Napa active=0, terrain bases are back `0x0049eafc`,
front `0x0049ef0c`. Verify the active selector and outer+0x1c mode before
assuming this; record both halves for split-mode debugging.

All offsets below are bytes, signed i32 unless marked:

| Base | Offset | Meaning |
| --- | --- | --- |
| outer | +0 | absolute walker Q8 position |
| outer | +4 | active section selector |
| outer | +0x1c | single/split mode |
| section | +4 | graph-node pointer (normalize against course root) |
| section | +8 | section-local Q8 position |
| terrain | +0 | terrain definition pointer |
| terrain | +4 | kind/tag |
| terrain | +0xc | terrain local Q8 position |
| terrain | +0x10 | previous sample |
| terrain | +0x14 | hill record index |
| terrain | +0x18/+0x1c | wall record indices |
| terrain | +0x40 | current hill-record pointer |
| terrain | +0x44+4*i | point i start, i=0..7 |
| terrain | +0x64+4*i | point i end |
| terrain | +0x84+8*i / +0x88+8*i | x / height |
| terrain | +0xc4+8*i / +0xc8+8*i | dx / dh |

Evidence: `00452f50.c` init; `00453030.c` active selector; `00452cb0.c`
terrain member; `00450fc0.c`, `00451380.c`, `004515a0.c` field layout.
Byte assembly confirms start/end stores at `451043/451046`, xy writes at
`45106b/451095`, hill-step sample/record at `4515b1-4515e7`.

Add snapshots directly beside `ring.front/back/camera` in `dump_sim_tick()`.
This reads the fixed race walkers and avoids scene iterator contamination
from globally tagging every terrain callsite. Dump every tick through 190
(or every crossing plus first snapshot); compare native snapshots from setup
onward, not only tick 189. A previous-tick snapshot supplies the pre-crossing
windows because the race ring steps only at sample crossings. At each point,
regeneration condition is `end <= sample || sample <= start`. A regenerating
point consumes 3 draws except clamped length zero, which consumes only its
length draw and aborts the WHOLE slope helper. Compare all eight points in
order, back walker before front walker (`0x00454120`).

## Checks and limits

- Read AGENTS, plan index/status, current RE notes and recent timer handoffs;
  inspected source, address-keyed exports and original x86 assembly using
  rg/fd. Shared source and existing fixtures untouched.
- Reference SHA-256 rechecked: `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
- `npx --yes rea-agents@6.4.0 function ... 0x004515a0 --format md`: exit 0,
  independent ephemeral Ghidra import; normalized pseudocode equals export.
  Evidence: `analysis/terrain-next-20261015/rea-4515a0.md`, `.log`, `checks.json`.
- No native C++ LSP tool exposed. `python3 tools/agent_lsp.py --check
  src/terrain.cpp` found healthy existing 181-entry database, built/indexed
  AST, and exited 3 solely for ExtractFunction tweak diagnostic about
  break/continue. No syntax diagnostic; do not claim a clean clangd check.
- No build/test, Wine, prefix, oracle-install or shared Ghidra mutation; no
  runtime parity claim. Documentation-only worker; integrator owns source
  changes, build gates, shared docs and commits. No commits made.
- All owned REA/clangd processes finished; no shared resources held.
