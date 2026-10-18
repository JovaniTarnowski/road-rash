# Full Napa live-field parity — October 18, 2026

Objective complete: both the unchanged `napa-field-l1-20261017` and the
expanded corrected-schema companion match all 2400 ticks with shared RNG.
Baseline `992106d`. Shared tree, owner PID 89267. No subagents used.

## Changed files and behavior

- `src/race.{cpp,hpp}`: original state3 attack selection/predicate, signed
  personality kick/backhand divisors, all relationship event triples and
  saturated writes, descriptor OR frame hit gate, nearest-half target/reach,
  actual repeat/taunt/steal queues and immediate live victim animation9.
  Original AI regeneration follows dispatch; the demo wrapper also regenerates.
  Bank-less synthetic component fixtures retain their legacy fight fallback.
- `src/actor_animation.hpp`, `actor_descriptors.cpp`: retain original graphics
  flags on playback descriptors, including combat bits 0x600000.
- `src/race_schedule.cpp`, `race.cpp`: retain car scan keys from the start of
  the scheduler pass; scan signed road velocity108. Clear a parked target
  on the original state change. AI animation LOD reads pre-driver camera.
- `race.cpp/hpp`: marking billboard callback (selector/frame/sample/tick,
  steering/traction response), bike mark-frame lifetime; fixed 33-slice first-
  draw collision-box preparation independently of renderer frequency.
- `src/traffic.{cpp,hpp}`, `race.cpp`: original car contact callback in pair
  order (horn roll, response flag, crossing pause), dt countdown/stop target;
  due cars move even outside the window, mark retirement now and unlink in
  the next prepass. Duplicate same-tick scheduling cannot unlink early.
- Tests: combat flags/frame 2/no-event and relationship write, parked target,
  marking type/speed/repeat gates, descriptor flags, car response/pause and
  deferred lifetime. `tests/field_replay.hpp` optionally compares every AI
  animation and all 30 relationships; normalizes signed unset deadlines.
- `oracle/aweman_proxy.c`, `oracle/sim_fields.h`, `tests/sim_fields.hpp`:
  corrected car metadata byte offsets, schema2 marker, optional ratings/AI
  animation probe and reproducible pre-race extra-personality speed inputs.
- `CMakeLists.txt`: new oracle-sim-field-schema2 regression.
  `tools/sim_known_divergences.json`: removed obsolete field failure rule;
  the two older solo diagnostic rules remain unchanged.
- Progress, verification, reverse-engineering, scenarios, plan10 and generated
  current-status docs updated. Original untracked research/handoffs/fuzz seeds
  preserved. No commercial assets, traces or dumps staged.

## Measured progression / source evidence

788:409460 attack selection and second steering call.812's hit lacked its
relationship reaction, causing836's rating error while RNG matched (this
corrects the earlier interim attribution to a missing hit). Descriptor-only
hits are independently tested.953: moved car used a new sort key and landed
behind an unrelated early-stop scan node.1001:oncoming car used positive
speed rather than signed108.1231:missing car callback draw.1232:demo-wrapper
plus AI regeneration.1293:marking billboard; its lazy first-draw boxes also
reject a premature1129 native mark.1402:pre-driver camera animation LOD.
1553:parked target slice.1803:deferred car retirement. All resolved.

Original exports used, no Ghidra import. Original EXE hash rechecked:
73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2.
At1231 diagnostic RANDCOUNT/RANDTRACE identifies45 calls:404b8c,404bb1,
44efc6, then14 triples4513f3/451479/4514e7. Thus the missing draw was horn
response, not terrain. Local diagnostic capture:
`napa-field-startup-diagnostic-20261018` (1400 complete, replay 1400 equal).

Legacy2400 trace SHA99e2034496086990d67d130defd3fbea5af6708498d791feb524f5cafab68fe5
unchanged. Four legacy aliases per car were malformed: kind73->1cc,
vehicle7f->1fc,lane7a->1e8,phase71->1c4 (decompiler element indices vs bytes).
The replay explicitly reports this schema defect and compares motion triples.
Schema2 additionally compares all proper car fields, every AI animation and
30 relationship words, with zero golden-only state keys:

- Capture: napa-field-l1-schema2-20261018,2400 contiguous ticks, displayL1,
  course4,seed1,demo,solo0,14 opponents plus cars and pedestrians.
- Trace SHA294343d533ae73fa1fea78678bb57fd0ea3901e23f2a09f248ff9bec66d275de.
- Proxy SHAb6288b48bec46f7307207b606d6895362665273fb803170227c22dad667ea0bc.
- RR_ORACLE_RATINGS1; RR_ORACLE_EXTRA_SPEEDS selects the six extra riders'
  recorded three-level startup speeds before original race construction.
  The 18 values and all environment inputs are in the capture receipt and
  SIM_SCENARIOS. Extra speed jitter precedes race seeding; this supersedes
  the old animation-frame-pacing explanation for differing field captures.
- Neither native replay uses live hydration or per-tick reanchoring. Native
  countdown, ring/build closure, one green anchor and free-run to tick2400.

## Final verification

Frozen source SHA8718e6d4f13bd5dcfb35df9bd5a9009657dc25e8950e519d35ee53bc5e5d4117.
Reused owned builds build-agent-ped308-20261017 and
build-agent-car-cadence-asan-20261017; no other worker builds modified.

- Native gate:104/104, 0 skips, five steps exit 0; viewer smoke and all-course
  sweep pass. `analysis/agent-gates/field-parity-20261018-native/receipt.json`.
- ASan/UBSan:10/10, 0 skips, three steps exit 0, including both complete field
  replays. `analysis/agent-gates/field-parity-20261018-sanitizers/receipt.json`.
- Strict human:1431 identical, acceptance_met true.
  `analysis/agent-gates/field-parity-20261018-human-final.json`.
- Field reports: build-agent-ped308-20261017/oracle-field-diagnostic.json and
  oracle-field-schema2-diagnostic.json, both pass 2400.
- C++ LSP unavailable. Compile DB preflight healthy 187 entries; LLVM clangd
  full-AST checks 0 errors for race.cpp,race_schedule.cpp,traffic.cpp,
  actor_descriptors.cpp,oracle_sim_test.cpp with --check-lines1-1. Logs under
  analysis/agent-gates/field-parity-20261018-evidence.
- Native gate separately retains the older solo tick210 known diagnostic;
  it is not a field failure or strict-human acceptance result.

## Scope and resources

This completes the requested full captured Napa L1 field, not an entire race
or all-game parity. Plan10 Tasks 6/7 keep level 5, dedicated fight/steal and
isolated traffic scenarios, police, full split/merge ring and mixed pedestrian/
controller ordering open. Human input outside the1431-tick capture is unmeasured.

No build, Wine capture or Ghidra process remains running at wrap-up. Oracle
prefix contains the new schema2 proxy; installed original EXE remains untouched.
All session claims (field-parity,ratings-probe,combat-descriptors,sort,ring-probe,
car-response,car-retire,car-schema,docs,gate,status) are released after push.
The autogenerated supplementary handoff stubs are consolidated into this file.
