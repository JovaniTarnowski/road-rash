# Handoff: SIM fault + tick-208 contact audit (session sim-fault-2, October 14, 2026)

## Task / plan

Plan 10 Task 5 (single-anchor human-control replay, tick-208 divergence).
Baseline commit: `97a4281` (contact-tail port) + uncommitted SIM diagnostics in
`oracle/aweman_proxy.c` + `oracle/build.sh` (sibling's, PRESERVED untouched).

## Owned files (claim sim-fault-2)

`oracle/aweman_proxy.c` (untouched), `src/race.cpp` (contact/contact_car/crash_car fix),
`tests/oracle_sim_test.cpp` (untouched), `tests/race_test.cpp` (2 new tests + fixture).
Build dir: `build-agent-sim`. Also touched (mechanical, required by the fix):
`tools/sim_known_divergences.json` (human-golden rule refresh; demo entry byte-identical).

## Result

Tick-208 divergence CLOSED exactly. `sim-napa-solo-human.tsv` (`--driver=demo`,
single green anchor) now matches every compared field, the brain window and rng
through tick 209. First divergence moved frame 207 (tick 208, steer 45 off) to
frame 209 (tick 210, rng). `race-test` 0 failures (2 new tests). Demo golden
`sim-napa-solo.tsv` output byte-identical (no pre-208 drift).

## Mechanism (evidence-backed)

1. Tick 208 was a GLANCING (class-3) bike-bike hit (slots 0 vs 2). Proof: both
   sides' steer rate moved 240 -> 1383 (+1143 = |scaled 0x184|/2); class 1/2
   never touch 0xf0.
2. The 45-unit residual was the MISSING class-3 `knock_off` call, not the AI
   steer exit: the original runs `FUN_004052c0(0, swing, 3)` before the lateral
   branch (004049c0 lines 95-102, 125-131); native skipped it. The decision's
   own tail kick (`0x2d8 * scaled 0x188 >> 8`) is -45/-46; the 0x00404f93 kick
   (+1768) was already identical. Post-fix tick 208 steer matches EXACTLY (2392).
3. Class routing correction (REA second opinion on 0x004049c0 + repo decompile):
   contained bike-bike hits are CLASS 1, not 2 (`other.0x17c 0x2300 < 0x2800`
   and `c4 & 0x10 == 0`; the old comment misread 0x2300 > 0x2800). Bike callers
   (`c4 & 0xc != 0`) pass stamina param 0 and bike param `(mag+bias)>>4`.
   Car callers (`c4 & 0xc == 0`) keep `(mag, mag)`. The old car class-3
   `(mag, swing)` and bike class-2 `(mag, mag)` params were both wrong.
4. Order fix: sounds/class/magnitudes read UNSCALED impulses (elasticity
   `0x180` = 192, real); scaling (incl. previously missing 0x18c) applies after.
5. `div_pow2`/`div256` trunc == sar-round-bias PROVEN identical for all int32
   (exhaustive python check, shifts 2/4/8/10/14): the "negligible off-by-one"
   notes in plan10-contact-tail-20261014.md are wrong; there is no difference.
6. Crash vectors: the class-gap `0x00403d60` call uses the handler's local_18
   pair (rider {0,0,0x200}, bike {0,0,0x100}), not knock_off defaults
   (crash_car + class-1 gap crash updated). Gap crashes fall through to nudge
   + tail (no kick: steer rate untouched), matching LAB_00404f49 flow.
7. contact_car local_21 guard (previously flagged): implicitly covered — every
   down path returns via the 0x2f4 check before the kick (commented).

## Tick 210 (new first divergence, NOT fixed)

Native-only bike-bike contact at tick 210. Proof: native lateral differs by
exactly one 960-unit nudge push (33427 vs 32467), speed by exactly shit/4 = 424
from native S184 = -2262, native rng advances exactly 2 draws (contact sounds);
original shows no nudge/speed-loss/draws. Trigger is an uncovered frozen-rider
word (0x108/0x10c/0x3c staleness from grid setup, Task 6 fix order 1); this
golden era carries no frozen laterals. Needs a current-era SIM capture with
frozen bike-word coverage, or the grid-setup port.

## SIM fault status

- Reproduced with the installed (diagnostic, 13:07) DLL: probe capture
  `sim-fault2-probe` (course 4, seed 1, drive 2, solo, FRAMES=250) dumped
  exactly 2 frames then faulted (trace `oracle-capture-3c645e86...tsv`, 40943 B;
  tool timed out at 600 s, Wine exit 124, capture rejected into
  `failed-captures/capture-3c645e86...`, no partial in `analysis/oracle/`).
  Fault context (faulteip.log, 11:33 run): c0000005 read of 0x1FE from ntdll
  (eip 0x7bf63cc2). SIM still blocked for fresh captures. Leftover winedbg/
  winedevice/wineserver from the probe cleaned; prefix idle at handoff.
- Sibling forensics (read-only): their 13:07 menudbg.log run stepped/dumped
  ticks 1-85+ cleanly past green (tick ~61) with presents/proj healthy and all
  KillTimer calls returning not-found — timer-kill did not find armed timers,
  yet that run progressed much further than the 2-tick fault. Suggests the
  fault is run-dependent (timing/ordering), not a hard SIM-hook bug. Left for
  the SIM-fault owner; my probe added no log spam (no MENUDBG/FAULTEIP set).
- REA spot-checks (nvm node v24.13.0, Ghidra 12.1.4): 0x004052c0 and 0x004049c0
  pseudocode IDENTICAL to repo exports; class-1 routing + knock shapes
  corroborated. 0x004395e0/0x00436a40 covered by line-audit (no separate()/
  move() changes needed). AI-steer-exit theory superseded by the knock finding.

## Checks

- `cmake --build build-agent-sim --parallel`: exit 0, no warnings.
- `./build-agent-sim/race-test`: 0 failures (RACE_EXIT=0).
- `ctest --test-dir build-agent-sim --output-on-failure`: 94/94 passed, exit 0
  (555.99 s; unit 66, sim 12, slow 7, disc 18).
- `verify_sim.py --mode diagnostic` (both fixtures): known-divergence, exit 0.
- `tools/agent_gate.py native --build-dir build-agent-sim`: RUNNING (background).

## Remaining work (concrete next actions)

1. Finish gate; commit `src/race.cpp tests/race_test.cpp
   tools/sim_known_divergences.json` (+ this handoff) with `<area>: <what>` +
   Co-Authored-By footer. Do NOT stage oracle/ or settings files.
2. Tick 210: port grid setup (Task 6 fix order 1, 0x00405990) or recapture with
   frozen-word coverage once SIM works; the 0x3c-staleness semantics for frozen
   riders (separate() end-copies vs freeze) needs one instrumented run.
3. Open micro-gaps (untouched, documented): 0x2f4-entry else-branch
   (0x004049c0 line 219+: f4 += 0x150 on crashed riders; the kick reads
   uninitialized local_20 — do NOT port UB); 0x1e9 zeroing + 0x00407410 tail
   thunk (unobservable in goldens: raw_1e8 == 0 both sides).
4. SIM fault stays with its owner; uncommitted oracle diagnostics preserved.

## Live resources at handoff

- ctest gate (pid ~30337) and SIM probe capture (tool-owned wine prefix claim
  session sim-fault-2) may still be running; REA done. No stale processes of
  mine otherwise. Do NOT kill pid 17085 (owner's rr-game from build/).
