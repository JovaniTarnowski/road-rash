# REA + capture session 20261026 (sim parity support, no code)

Owner request: another agent owns the code. This session does REA and oracle
captures only — no edits to `src/`, `tests/`, `tools/`, `oracle/` code.

## Workstreams

1. Fight steal/isolated captures (plan 10 Task 6: dedicated fights, steals).
2. Contact `+0xac` REA: writer of the sub-slice `+0xac` bit `0x4000` gate in
   `0x004373f0` (blocks contact acceptance past tick 3841).
3. Solo-jump + traffic-solo/field captures.
4. Split-branch avoidance scripts (dodge the 2055 static, reach split commit).

## Environment

- Baseline `main` at session start; owned build dir unused (no builds needed).
- Captures via `tools/oracle_capture.py` (holds ResourceClaims on prefix).
- REA 6.4.0 ephemeral (never touches `analysis/ghidra-project/`).
- Published goldens stay in `analysis/oracle/` (git-ignored); companions
  (`.controls`, `.wine.log`) alongside.

## Log

- (start) mapped knobs/controls/recipes; REA setup confirmed per TOOLING.
- Controls format: one hex word/tick at `0x004b7ed0`; `1` throttle,
  `0x21` throttle+punch, `0x5`/`0x201` steer, `0x3` brake.
  Detection rules (no code changes): steal <=> any `rider.N.raw_1cc`
  nonzero (attacker `steal_until` set); weapon in `raw_1c8`.
  Fight/L5 traces show weapons change (pickups) but `raw_1cc` always 0.
- Staged `/tmp/rr-captures/`: `steal.controls` (3000w, punch every 8th),
  `avoid-left/right.controls` (6500w, steer window ticks 1900-2060),
  `traffic-field.controls` + `traffic-solo.controls` (4200w throttle).
- Other agent's capture occupied the prefix 08:11-08:20; waited.
- 08:23 launched `sim-napa-fight-steal` (course4/seed1/field/HUMAN,
  punch-every-8th, quit via 0xb4 rule => ~3181 frames) in background.
- `sim-napa-fight-steal` DONE: 3181 frames, human, complete, valid.
  Result: ZERO steals (`raw_1cc` all 0), ZERO opponent crashes despite
  94 completed player attacks with neighbors alongside (median lateral
  gap 0). Every-8th may starve hit resolution, or L1 AI attacks too
  rarely to open steal windows. Retrying L5 + every-16th.
- 08:26 launched `sim-napa-fight-steal-l5` (LEVEL=5, punch every 16th,
  5000 words => ~5181 frames) in background.
- `sim-napa-fight-steal-l5` DONE: 5181 frames, human, complete, valid.
  ZERO steals again despite 103 player attacks, frequent opponent
  crashes, opponents often armed-attacking. Overlap analysis: only 2
  of 2060 player-attack ticks had an armed-swinging opponent in reach
  (vs 1/1347 in fight, 0/1880 in steal-L1) — punching suppresses AI
  swings. By contrast throttle-only `contact` has 201 overlap ticks in
  4381 frames. Frame<2 + rand&3==3 gate needs volume: passive farm.
- 08:29 launched `sim-napa-steal-farm` (LEVEL=5, throttle-only, 11000
  words => ~11181 frames, ~45-60 min) in background.
- `sim-napa-steal-farm` DONE: 11181 frames, human, complete, valid.
  ZERO steals; only 35 overlap ticks (vs 201 in L1 contact). L5 player
  rides lonelier (p90 neighbors 1 vs 4 at L1) — L1 passive is the
  richer farm. Queued: 15000-word L1 passive farm (~700 overlaps).
- 08:34 launched `sim-napa-avoid-left` (SOLO, steer-left window ticks
  1900-2060, 6500 words) in background; avoid-right next.
- `sim-napa-avoid-left` DONE: 6681 frames, valid. Steer authority is
  huge (~3300 lateral/tick sustained; +110k -> -161k in 100 ticks) —
  the 160-tick window overshot into a DIFFERENT static (crash at 2050
  vs baseline 2055, then frozen, no remount). Lesson: dodge needs a
  small nudge, not a swerve.
- 08:36 launched `sim-napa-avoid-l8` (SOLO, steer-left only ticks
  2040-2047, 3000 words => ~3181 frames) in background; right-8 next
  if it clips the other side.
- `sim-napa-avoid-l8` DONE: 3181 frames, valid. Only -4.3k lateral at
  2055 (same crash, anim 29). Short-window authority ~600/tick
  (velocity builds up; 3300/tick was sustained). Need ~10x more.
- Launched `sim-napa-avoid-l26` (steer-left ticks 2024-2049, 3000w)
  in background; expect -25..-40k at 2055, clear of both statics
  (baseline +219k, left-run crash at -124k).
- `sim-napa-avoid-l26` DONE: 3181 frames, valid. DODGED the 2055
  static (anim 3 riding through 2055, -29k lateral). Second crash at
  tick 2206 (pos 81480, lat ~245k, anim 29). Run dies there.
- Launched `sim-napa-avoid-l26l46` (window1 + steer-left 2160-2205,
  6500w => ~6681 frames) in background to dodge the 2206 static and
  reach the split.
- `sim-napa-avoid-l26l46` DONE: 6681 frames, valid. BOTH statics
  dodged — continuous riding ticks 2056-4910 (2854 ticks, pos
  76k->180k), then third static at 4910 (pos 180315, lat ~89k,
  drifting right +800/tick). Fast riding over whatever crests lie in
  76k-180k (jump verification needs +0x20 height — not in solo dumps;
  owner's probe call).
- Launched `sim-napa-avoid-3w` (windows 1+2 + steer-left 4860-4905,
  6500w) in background to pass the 4910 static.
- `sim-napa-avoid-3w` DONE: 6681 frames, valid. NO crashes after the
  windows — full-length solo ride past 4910 to pos 240986+ (still
  riding at input exhaustion). This is the canonical past-crash trace:
  2854+ continuous riding ticks through the split region. Split-commit
  and jump verification are owner's (route tables; +0x20 height not
  in solo dumps).
- Launched `sim-napa-traffic-solo` (SOLO=1, throttle 4200w) in
  background; traffic-field (L5) next, then the long L1 steal farm.
- `sim-napa-traffic-solo` DONE: 4381 frames, valid — but ZERO vehicle
  keys. Root cause found in proxy: `sim_freeze_others()` nulls the
  traffic pool so `0x0044df60` never places cars (by design, for
  deterministic solo replays). "Traffic-solo" is impossible under
  SOLO=1. Bonus finding: all 4381 positions IDENTICAL to split-left
  (determinism cross-check across runs).
- Launched `sim-napa-traffic-field` (LEVEL=5 field, throttle 4200w)
  in background — the real traffic scenario (contact run already
  shows field spawns cars throughout).
- `sim-napa-traffic-field` DONE: 4381 frames, human, complete, valid.
  Rich traffic: vehicle types 1/2/3/4/6/8 (6/8 new vs L1).
- Launched `sim-napa-steal-farm-l1` (L1 passive, 15000 words =>
  ~15181 frames, ~70-90 min) in background. Last queued capture.
- `sim-napa-steal-farm-l1` DONE: 15181 frames, human, complete,
  valid. ZERO steals; only 213 overlap ticks, clustered in ticks
  0-2000 (148) then near-zero (pack spreads as the player pulls
  away). Detection verified sound (`raw_*` dump from half object
  `rh +0x180..`; `+0x1cc` steal word persists 300 ticks — unmissable;
  mapping matches the port's weapon/standings comment). Remaining
  suspects for the owner: side-match gate, `frame<2` mapping
  (unverified — no golden ever steals), victim-anim vs
  `attack.weapon` timing.
- Launched `sim-napa-steal-bunch` (L1, brake-tap 30/300 ticks to hold
  the pack + punch every 30th, 6000w) in background: re-bunches the
  pack for sustained overlaps with fresh (non-repeated) alignments.
- `sim-napa-steal-bunch` DONE: 6181 frames, valid. ZERO steals; only
  44 overlaps (braking scattered the pack — WORSE than passive).
  Total: ~40k ticks, 0 steals.
- Launched `sim-napa-steal-vary` (L1, punch pressure phases
  30th/12th/none/20th per 1000-tick block, 4000w) in background:
  varied pressure desyncs AI while bunched. Final steal attempt.
- `sim-napa-steal-vary` DONE: 4181 frames, valid. ZERO steals despite
  207 overlaps (best rate yet). STEAL FARM CLOSED: ~45k ticks over 7
  captures, 0 steals.

## Close-out (all 4 streams)

All captures valid (`validate --simulation`: header, contiguous
ticks, EXE hash, human branch, `complete true`), receipts + input
scripts archived in `analysis/oracle/` (controls hash-matched to
receipts, e.g. steal-vary MATCH).

| Capture | Frames | Result |
|---|---|---|
| sim-napa-fight-steal (L1, punch/8th) | 3181 | 0 steals; punching suppresses AI swings (0 overlaps/1880 atk ticks) |
| sim-napa-fight-steal-l5 (L5, punch/16th) | 5181 | 0 steals; 2 overlaps/2060 atk ticks |
| sim-napa-steal-farm (L5 passive) | 11181 | 0 steals; 35 overlaps (L5 rides lonely) |
| sim-napa-steal-farm-l1 (L1 passive) | 15181 | 0 steals; 213 overlaps, bunched ticks 0-2000 |
| sim-napa-steal-bunch (L1 brake+punch) | 6181 | 0 steals; braking scatters pack (44 overlaps) |
| sim-napa-steal-vary (L1 phased punch) | 4181 | 0 steals; 207 overlaps (best rate) |
| sim-napa-avoid-left (-l8/-l26/-l26l46/-3w, SOLO) | — | steer authority mapped; 3w rides crash-free 2056->6681 (pos 76k->241k) through split region |
| sim-napa-traffic-solo (SOLO) | 4381 | 0 vehicle keys: SOLO=1 nulls traffic pool (proxy design); = split-left bit-identical (determinism x-check) |
| sim-napa-traffic-field (L5) | 4381 | vehicle types 1/2/3/4/6/8 — the traffic scenario |

Steal findings for the owner: detection rule `raw_1cc != 0`
(verified sound: half-object dump, 300-tick persistence); L1 passive
short runs are the richest farm (overlaps cluster ticks 0-2000);
punching suppresses AI swings; remaining suspects are the side-match
gate and the unverified `frame<2` mapping (no golden has ever stolen).
Jump: solo dumps lack bike `+0x20` height (airborne = `+0x20>=0x65`)
— verification needs a proxy/harness addition; riding coverage
(3w: 2854+ fast ticks over 76k-180k) delivered.
Contact `+0xac`: no writer in `.text`/sweep-hit-contact graph (REA
xrefs + 1739-node trace); two hypotheses + decisive probe spec
(slice `+0x60/+0x61`/`+0xac` dump) in the REA section above.

## Continuation: push the dodge chain to course end
- `sim-napa-avoid-ext` (windows 1-3, 9000w): DONE, 9181 frames valid.
  Rode to tick 7835 (pos 288597) then crashed (fourth static at
  ~142k lateral, drifting right +600/tick). Continuous riding
  2056->7835 = 5779 ticks. Input script archived hash-MATCH.
- Launched `sim-napa-avoid-4w` (windows 1-3 + steer-left 7780-7825,
  11000w => ~11181 frames) in background: dodge the fourth static;
  run to course end or the next obstacle.
- `sim-napa-avoid-4w` DONE: 11181 frames, valid, input archived
  hash-MATCH. Fourth static dodged; rode to tick 8667 (pos 317506)
  then crashed (fifth static, lateral ~-207k). Continuous riding
  2056->8667 = 6611 ticks, pos 76k->317k. No lateral discontinuities
  >20k anywhere (no detectable route rebase; branch resolution is the
  owner's route-table call — the split region is covered many times
  over).
- DODGE CHAIN STOPPED (diminishing returns: one static per ~30-100k,
  ~2h per window). Method + steer-authority calibration documented
  above for the owner to extend. Prefix cleaned of my temp traces
  and input scripts (published copies in analysis/oracle/).

## REA: slice `+0xac` writer hunt (stream 2)

- Triple gate at `4374a6: movl 0xac(%esi),%eax` + `testb $0x40,%ah`
  (bit `0x4000`); esi = `*(bike+0x140)` sub-slice.
- Exhaustive `.text` scan (`original-disassembly.txt`) for `0xac(%reg)`:
  stores exist ONLY at `405bff/405c07/405c3f/405c47/405c59/405c5f`
  (inside `00405990` grid placement, zeroing) and `4389af`
  (`mov %ebp,0xac(%ebx)`, ebp=0 — also zeroing at placement). Both
  target the BIKE struct (`+0xc0/+0x2fc` context), NOT the slice.
  Other `testb $0x40,%ah` sites (`40a88e/40ab80/420819`) test the
  control word / unrelated structs.
- No `orb`/`orl`/`bts` setter for the bit anywhere; no `+0xac` store in
  any exported builder (`00454240` slice build, `00451cb0` BORR spans,
  `00451910/00451c20/004520d0/00452710/004527d0/004528f0/00452b00/00452570`,
  draw-fill `0044b5f0/0044b630`).
- Readers of slice `+0xac`: ONLY `004373f0`. (Draw functions
  `00446250/004463e0` read a DIFFERENT `+0xac` — the bike's.)
- Ring slots are static-arena pointers (`0x49e820` -> `0x49f250`,
  stride `0x2f8`); build never writes `+0xac`.
- Status: writer still unidentified; two live hypotheses for the code
  owner: (a) stale/arena value never initialized in build path
  (deterministic per slot, varies per slice — fits 2055-vs-3841);
  (b) the 3841 divergence is actually renderer-filled BORR box counts
  (`+0x60/+0x61/+0x68`, filled on first draw by `0044b5f0->0044b630`
  with visibility early-outs) differing, with `+0xac==0` in both.
  Decisive probe needs a slice-word dump (proxy change — code owner's
  call): `rider.0.slice_ptr` + `nodeaddr.*.slice36` pattern exists.
- REA `xrefs 0x4373f0` DONE (REA 6.4.0/Ghidra 12.1.4, ev_cb2817a7):
  single caller `0x436952` (inside the `004368f0` contact gate).
- REA `trace-native-values 0x4373f0` DONE (depth 3, 1739 nodes;
  ephemeral, not committed): graph spans
  `004373f0 -> {00437660, 00437750} -> 00436a40 -> 00439270`. The ONLY
  `+0xac` address computation in the graph is the LOAD at `0x4374a6`;
  all 28 STOREs target globals/params (address defs are CASTs, no
  `+0xac` INT_ADD). Inter-procedural corroboration of the negative
  result: no writer in the sweep/hit/contact path.
- Fill-gating chain (for hypothesis (b)): `00438610` fires
  `0044b5f0(param_2,param_3)` once per slice (bVar3 latch + end
  fallback) iff draw context `+0x6 > 0x92`; callers pass bucket
  windows `(-10000,10000)` (`00443b40`) or `(speed,10000)`
  (`00443c00` case 4). `0044b5f0` fills only slots whose lateral is in
  the window; `00451cb0` zeroes `+0x61/+0x68` at build. So an unfilled
  slot keeps `+0x60>0, +0x61==0` -> sweep skips BORR entirely.
- Deliverable for code owner: hypotheses (a)/(b) + decisive probe
  (dump slice `+0x60/+0x61`/`+0xac` for the 3841 slice, e.g. new
  `slice_*` keys next to `slice36`; or a native `boxes_ready` audit
  at 3841). No proxy/src change made (owner's files).