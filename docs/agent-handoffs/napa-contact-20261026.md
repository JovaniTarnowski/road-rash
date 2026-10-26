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

## Contact replay: green through 3840, pinned at 3841 (single static)

- Capture validated as a partial (`validate --simulation --frames 4381`):
  header, 4,381 contiguous ticks 1-4381, EXE hash, exit 0,
  `control_branch human`, `complete true`. Promoted to
  `analysis/oracle/sim-napa-contact.tsv` (git-ignored). Trace
  `7ea88dc3…`. No accepted capture overwritten.
- Native replay (`oracle-sim-test`, auto human): `1469` ring-front
  identical after the camera-follow fix (half image, not bike words;
  contact shoves no longer leak into the ring); `2700` half `+0x1a0`
  link word excluded as a host address (non-state, like `bike_44`);
  `2724` remount clears the half crash bits; reaches tick 3840 with 0
  sim-field divergences.
- First divergence at tick 3841: RNG draw (2 draws native, 0 golden)
  plus the full glance response (lateral +5376, speed -1148, steer and
  steer-rate kicks): a single-box BORR static at lateral units 1372
  (sample 372, box `[-44544,-2048]` pre-triple) overlaps the player
  only under the 3x wide rule (`0x004373f0:47-50`: `|units| > 0x3ff`,
  one box, sub-slice `+0xac` bit `0x4000` clear).
- Root-cause analysis (no porting, per time-box): native always
  triples (no writer for the bit found in the export set; `rg 0xac`
  finds only readers in `004373f0`/`00446250`/`004463e0`). The golden
  demonstrably does NOT triple here (else it would contact
  identically), yet the triple IS required elsewhere (removing it
  breaks the 2055 crash replay, the fight RNG and the solo splits).
  So the bit varies per slice, but its writer is not in the available
  exports -- the rule cannot be faithfully ported without REA on the
  ring build/draw paths. No golden touched.
- The `+0x1a0` contact-link words are host addresses (20986896 = bike
  node at tick 2700): excluded from comparison as non-state (like
  `bike_44`/`slice_ptr`); any behavioral effect would surface as a sim
  divergence, which remains fatal.

## Remaining (explicitly not in this slice)

- `napa-contact` acceptance (blocked on the `+0xac` triple rule above).
- `napa-solo-jump`, `napa-traffic-solo/field`.
- Split branch commits (blocked: need avoidance scripts; separate slice).
- CTest registration only on clean full pass without golden-only suffix.

## Follow-up 20261026: sound-test regression from the camera hunk

- The camera hunk broke CTest `sound` ("negative split separation
  truncates toward zero"). Bisected in `/tmp/rr-bisect` (parent
  `44679e4`): ctor-init hunk alone passes, +camera hunk reproduces,
  remount hunk inert for sound (recovery path never runs in the
  fixture). Root cause: the fixture's `position()` poked only player
  bike `+0x138`, which the old camera read directly; the new camera
  follows `half_mirror.position` (bike `+0x1c` image). In the sim
  `+0x1c`/`+0x138`/mirror always agree outside a contact tick, so the
  fixture now pokes all three together (plus body `+0x1c` and the
  mirror in the crashed-player block). Fixture-only change in
  `tests/sound_test.cpp`; no sim, golden, or expectation touched.
