# Plan 10 frozen scheduler cadence — October 14, 2026

Read-only source trace after frozen-state hydration, baseline02d7e8d in
isolated worktree. No source edits, captures, builds, or commits. This
handoff is the only write. No active resources remain.

## Root cause now measured

Fresh `analysis/oracle/codex-sim-frozen900.tsv` closes frozen initialization
but demo replay still first differs at tick210: rng849046096 vs2140064586,
player lateral32467 vs33427, speed2262 vs1838. Native LLDB showed all three
pairs are player slot0 against frozen slot2 (pos13824):

| tick | native player lat before contact | native slot2 lat | slot2 previous lat |
|---|---:|---:|---:|
|208|28203|19200|16384|
|209|30092|18240|19200|
|210|32467|17280|18240|

Native opponent contact callback nudges slot2 by960 each contact, with
native +18=1. Integrator's measured `codex-sim-mask300.tsv` instead gives
slot2 +18=4 near contact: original lat19200 ->15360 at208 ->11520 at209.
Tick210 original gap32467-11520=20947 exceeds15360 total widths, so no
third pair. Native gap32467-17280=15187 still overlaps, causing the extra
player nudge, speed loss, and two sounds. Exact extra dimensions, masses,
flags, crash and slice mask agree; this is scheduling state, not geometry.

Original slot2 address20985272 (`bikestate.14035b8`): frame1 last(+10)=1,
dt(+18)=5; ticks2..4 retain these; tick5 last5/dt4; tick9 last9/dt4;
tick205 last205/dt4; ticks206..208 last205/dt4; tick209 last209/dt4.
Thus first measured dt5 must not be hydrated once and kept forever either.

## Exact scheduler trace

- `0044b3b0.c:22` calls `0x00438170(0)` (assembly44b3fe). Therefore use
  its simple scheduler branch, not the distance-zone optimized branch.
- `0x438347..43835b`: each node runs iff signed node+0x0c <= global tick;
  then calls `0x00437e60` regardless of NULL think/move.
- `0x437e69..437e6f`: `dt = tick-old(node+0x10)`; writes node+0x10=tick,
  node+0x18=dt **before** checking think/move.
- `0x437e80..437e88`: NULL move jumps to0x438108, skipping all cadence
  recomputation in the move path. Thus node+0x14 interval survives freeze.
- `0x438108..438113`: node+0x0c = node+0x14 + tick. Original x86 wraps.
  Tail still performs road-list boundary callback if its conditions hold;
  that is separate from think/move freeze.
- `0x004049c0` contact nudge at404f4d uses `(widths/16)*node+0x18`.
  Native `Race::contact` mirrors that multiplication, but frozen native
  dt remains ctor1 because step_rider returns before scheduling metadata.

REA6.4.0 for0x437e60 succeeded under documented Node24/Ghidra12/JDK21;
private output `/tmp/frozen-scheduler-rea-437e60-20261014.json`. Pseudocode
matches existing authoritative export after whitespace normalization.
Native diagnostic logs: `/tmp/frozen900-lldb-values-20261014.log` and
`/tmp/frozen900-trace-20261014.log`. No native state was patched in LLDB.

## Required next implementation, not a hardcoded timestep

Capture initial frozen `+0c` next deadline, `+10` last tick, `+14` interval,
`+18` retained dt. Hydrate once alongside other declared initial state.
For frozen callbacks-null nodes, run the original scheduler metadata step
at every native countdown and race tick: if next<=tick, dt=tick-last,
last=tick, next=tick+interval; otherwise retain dt. Keep movement/AI null.
This reproduces post-tick1 dt5 -> post-tick5 dt4 without deriving a cadence
from sample position or substituting four globally. Interval4 derives from
pre-freeze original setup; the present native constructor does not reproduce
that setup history, so capture provides diagnostic initial metadata.

Current `countdown_step` comments explicitly admit opponent cadence is
unmodelled; `step_rider`'s 'scheduler never runs this node' comment is false
for the original. A broader fidelity follow-up also needs frozen outer
collision qualification based on scheduled-last==tick and flags&6==6;
current collide_riders unconditionally skips frozen outer nodes, although
the original scheduler can mark them run. Preserve handler mutations from
contacts (frozen refers to think/move, not immutability). Do not change
moving riders' cadence without corresponding trace coverage.

## Grid formula is now numeric evidence

All12 in-range bikes (11 frozen plus player) have next-record width512 and
centre0. Using sign24 bends, averaged/8 then divided/8, formula closes bike
lateral exactly at every one. Lane1 contributes0 here because (2-1)/2=0;
lane2 contributes64000. Example slot2: average22528 ->2816, bias16384,
lane1 zero =>19200. Slot3: average26624/8=3328 +64000 +16384=83712.
Slot5: average-86016/8=-10752 +64000 +16384=69632. Player build14336/8
+16384=18176; later scratch brain target10240 need not equal bike lateral.
Remaining three out-of-ring bikes have no valid next chain, as explicitly
reported by grid availability, not forced through this formula.
