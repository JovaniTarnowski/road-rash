# Optional live race terrain walker trace

Plan10 Task5, isolated checkout baseline c452be2 plus already accepted seeded
terrain fixture changes. Owner terrain_review owns only oracle/aweman_proxy.c
and this handoff. Sibling native TrackRing/replay diagnostics edits preserved.

Parent reports genuine seeded capture succeeded with Course4 menu bootstrap:
all960 hidden states and18 final RNG states match. The fixture did not justify
a terrain-step behavior change. The prior unset-course attempt never reached
a frame hook and was aborted/rejected; future first-frame component recipes
must explicitly include `RR_ORACLE_COURSE=4`. This corrects the earlier
terrain-fixtures-oracle handoff recipe's `-u RR_ORACLE_COURSE` item. These
runtime findings belong to the parent's capture, not this worker's own run.

## Added diagnostic

`RR_ORACLE_TERRAIN_TRACE=1` adds read-only snapshots to the existing
post-step dump_sim_tick. Default trace and seeded mode remain unchanged.
There are no new detours, simulation writes, RNG calls or dispatch changes.
Frame0 gets `ring.terrain.schema=1`; each SIM tick gets:

```
ring.walk.<back/front>.active
ring.walk.<back/front>.position
ring.walk.<back/front>.terrain.available
ring.walk.<back/front>.terrain.position
ring.walk.<back/front>.terrain.last_sample
ring.walk.<back/front>.terrain.record
ring.walk.<back/front>.terrain.point.<0..7>.<start/end/x/height/dx/dh>
```

`active` is original-only context; native comparator does not need to compare
it. Local position = section+8. Outer back=0x0049ea20/front=0x0049ee30;
active=outer+4; section=outer+0x28+active*0x1f4; terrain=section+0xb4.
Active is range-checked0..1. Full terrain state readability checked through
0x104 bytes; `available` requires a nonnull readable definition pointer.
Unavailable cases emit active,local position (or-1),available0 only.
Available cases log all eight signed32 state fields with the same offsets
and names as accepted seeded fixture mode. Definition pointer isn't emitted.

Native worker timer_tests directly confirmed this schema. The diagnostic
covers the active section only; it doesn't claim both split-side snapshots,
node identity or full course parity. First Napa section is the current target.

## Checks / integration

Private `./oracle/build.sh > /tmp/terrain-trace-oracle-20261015-build.log 2>&1`
exit0; empty compiler log. `git diff --check` exit0. Built private DLL SHA256:
`2a5a93cfbfec04d9a6e4fd12d0db421c45f74146087a63e73f3164b1c175e5a1`.
No Wine/install/capture/main-checkout writes or commits performed.
No LSP channel for Windows-only C; target MinGW build is the syntax/type gate.

Incremental patch relative to already-copied seeded proxy:
`/tmp/terrain-trace-20261015.patch` (71 lines). It adds only trace flag,
read-only snapshot helper, frame0 marker, dump calls and option parse.
Original seeded-only snapshot saved `/tmp/terrain-trace-20261015-base.c`.
Integrator may apply narrow patch to main after its existing capture releases
prefix/install; do not copy over unrelated main proxy edits.

For the next coordinated simulation capture, use established human Course4
settings plus `RR_ORACLE_TERRAIN_TRACE=1`,
`RR_ORACLE_TERRAIN_SEEDED=0`, `RR_ORACLE_RASTER=0`. Keep the tested control
replay, seed, SOLO/HUMAN and frame count exactly as established by root.
Require complete capture and new proxy hash, then native replay compares
full state from initial frame0 and every later tick. Legacy captures without
the schema marker intentionally don't invoke this comparison.

Source edits frozen after successful build. No owned processes/resources held.
Root owns integration, runtime captures, main native build gates and commits.
