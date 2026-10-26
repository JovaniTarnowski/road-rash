# Napa contact capture (plan 10 Task 6, steer-in-to-opponent)

Task: capture `napa-contact` (field, human, throttle-only with weaves if
needed) long enough to cover walk-back remount, the chase, and at least one
deliberate or natural bike-bike contact without attacks; replay to first
divergence; fix in plan order (contact → combat).

Baseline: fight slice green (2581 ticks, committed before this slice).

Owned files (this slice): same set as the fight slice (sim + harnesses +
docs). Build directory: `build-solo-split-845` (owned).

Resource claims: oracle capture `napa-contact-20261026` will hold the
prefix/install/output claims while running; no other Wine processes may run
then. Ghidra/REA: project export first, REA 6.4.0 spot-checks only.

## Recipe (plan 10 Task 6 `napa-contact`)

- Course 4 (Napa), seed 1, level default, field, HUMAN=1.
- Input `C:\napa-contact-20261026.controls` (seeded uniquely in the prefix;
  source `analysis/oracle/sim-napa-contact.controls`, git-ignored): 4,200 hex
  words, all `1` (throttle). No punches (isolate contact; fight already
  covers combat). Rationale: the fight capture's throttle-only player
  crashes on the roadside static ~2055, walks back, and (if it remounts
  in range) chases the pack -- rear-end/sideswipe contacts should occur
  naturally in the 2,100..4,200 window. If the run wrecks or coasts
  without contacts, steer-in scripts (weave/lane-hold variants) are the
  fallback in this same slice.
- `RR_ORACLE_FRAMES=4500` (4,200 script + coast + `0xb4` quit window).
- Expected validation: header, contiguous ticks, end marker, EXE hash,
  exit 0, `control_branch human`. Partial validation if the quit rule
  ends it early (like the 2,581-tick fight capture). Never overwrite
  accepted captures.

## Analysis so far (from the fight golden, same seed/field)

- 0 nonzero `rider.0.raw_184` in 2,581 ticks, RNG in sync throughout,
  only the player crashes (static ~2055): no evidenced bike-bike contact
  yet, so the response path is unverified live (synthetic + solo-frozen
  contacts only).
- L1 field (2,400 ticks) also crash-free: same gap.
- Golden rider rows are scheduler-list order, NOT stable slots: row 12
  is the player half (think `0x00434660`, mirrors player pos/lat), and
  row numbers shift as the list re-sorts. Use frame-0 `nodeaddr` grid
  positions (or the harness `rider_node_identities`) for stable slot
  identity when mining trajectories -- never raw row indices.
- Early lateral "jumps" >3000 are row-reassignment artifacts (grid
  spawn/list order), not motion.

## Remaining (explicitly not in this slice)

- `napa-solo-jump`, `napa-traffic-solo/field`.
- Split branch commits (blocked: need avoidance scripts; separate slice).
- CTest registration only on clean full pass without golden-only suffix.
