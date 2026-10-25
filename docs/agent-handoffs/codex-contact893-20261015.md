# Plan10 contact role flags — October 15, 2026

Parent assigned read-only893 investigation, then private implementation.
Main-checkout source untouched. Research baseline109c640 plus parent-staged
f636319; private implementation baselinef636319 in detached worktree
`/Users/archit/Dev/rr/.codex-sim735-20261015/native-port`. Own main paths only
this handoff and `analysis/codex-contact893-20261015/`. Main Race ownership
remains parent-coordinated; worker does not clear/reap retained OMP claims.

## Proven first gap after null-slice scan correction

Same900tick single-anchor demo exits1 at893: RNG707629878 vs2897718112.
Native continuation shows player speed6866 vs8450,lateral13895 vs18451,
steering kick; brain words still match. Original893 RNG plus exactly2 MSVC
steps is1448754465 then2897718112.

Read-only native LLDB conditional stop at collide(ra,rb) with ra.pos30093:

| Word | ra/player | rb/frozen slot7 |
|---|---:|---:|
| position |30093|30208|
| lateral |18451|28672|
| +c0 flags |7|4|
| half-length178 |64|64|
| half-width174 |7680|7680|
| slice-present |true|false|

Native collides at115 longitudinal gap<=128 and10221 lateral gap<=15360.
Two subsequent random() breakpoint stacks are BOTH in contact(player,same
partner), private race.cpp971/972, proving the extra draws' consumer.
Scripts/logs `analysis/codex-contact893-20261015/contact893*.lldb/log`.
Initial original nodeaddr.4 confirms pos30208,lat28672,bike_c0=4,w2c=0;
per-tick node4 remains stationary and thinkNULL at892/893. Original+c0 is
sampled only in frame0, so do not label it an independently dumped893 flag.
Selected trace fields/hash in capture893-selected.json.

Original004368f0 requires outer flags&6==6, candidate flags&5==5.
Assembly436924..43692e confirms outer;436969..436973 confirms candidate
**before distance tests** at436979 onward. Contact pass does not use a NULL
slice guard: it dereferences slices after the role gate. Do not substitute
blanket frozen/null-slice exclusions. Frozen valid candidates are essential.

Pinned REA6.4.0 completed exit0 for004368f0 with documented Node24/JDK21/
Ghidra environment. Normalized pseudocode exactly equals address-keyed
export1225characters; original hash73d7f974…a60f18a2. Files rea-4368f0.md/log.
No original binary, shared Ghidra project, Wine/install/capture mutated.

## Necessary initialization prerequisite exposed by the exact gates

First private2guard build/focused3test gate passed but replay regressed to
204: RNG1793882572 vs2193798822 (2 extra native draws). Kept failing log
`/tmp/codex-contact893-demo900.log` and continuation. LLDB204 identified valid
frozen bike slot2,pos13824,lat19200,flags7. The earlier pedestrian at the
same longitudinal position had native flags0x13, so the new exact candidate
mask skipped it, removing the previous distance stop and exposing the bike.

Cause: native PedestrianSystem::apply_template sets template flags3; spawn
adds native guard0x10, but neither applied original scheduler insertion.
Original00438930 ends with00437bb0; assembly437bb8 OR4 /437bc4 ANDf7 proves
insertion sets live4 and clears removed8. Thus original pedestrian flags
are7 (native also retains its separate0x10 guard). Runtime flags=template|4
was already described in collide_riders's comment but missing from code.

Final private4file delta:

- src/race.cpp: exact outer&6==6 before hazards and inner&5==5 before
  distance/overlap, preserving scheduler/position order and existing gates.
- src/pedestrians.cpp: apply_template ends with original insertion flag
  tail `(flags &0xf7)|4`; existing spawn0x10 guard stays unchanged.
- tests/race_test.cpp: discriminate outer/candidate role flags0,4,5,6,7
  plus upper-bit variants against same-world nonoverlapping baseline. Valid
  frozen candidates still affect physical state and consume sound RNG;
  invalid roles preserve physics/RNG. Expected cases enumerate valid roles
  independently rather than repeating the implementation's bitmask formula.
- tests/pedestrian_test.cpp: original initialization sets live4/clears8 and
  permits both roles; actual spawned actors retain live bit. Existing staged
  hit tests now receive correct insertion flags through normal initialization.

No golden/allowlist edits, no per-tick reanchor, and no new RNG draw.

## Validation and limits

Private full Debug build exit0, no warnings/errors;
`/tmp/codex-contact893-build2.log`. Focused race/car_contact/frozen_state/
pedestrian4/4pass,11.66s; `/tmp/codex-contact893-focused2.log`.

```
build-agent-sim735/oracle-sim-test /Users/archit/Dev/rr/native-port/analysis/oracle/sched900-20261015.tsv /Users/archit/Dev/rr/cdrom/ROADRASH --driver=demo
```

Final replay exits0, **900 ticks identical**, including58 native countdown
ticks/brains with one green RNG anchor. Output explicitly reports256
golden-only diagnostic fields and119422 uncovered field-comparisons across
eras; this does not establish all-fields/full-game parity. Same fresh600
also exits0. Logs `/tmp/codex-contact893-demo900-2.log` and `-demo600.log`.

Existing1431tick human trace still first differs189 (same original/native
RNG pair409811144/1232322109), while legacy2402tick demo still first differs
222 lateral43537/43777. Logs `-human.log`, `-legacy-demo.log`. These longer
fixtures do **not** prove parity beyond900 in the current demo scenario.
No accepted same-setup demo fixture longer than900 was found; a future root-
coordinated longer original capture remains required before any claim beyond
900. No new golden/capture was generated to bridge that absence. Plan10
Task5/6/7 and strict human acceptance remain open.
