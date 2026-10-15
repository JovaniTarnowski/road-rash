# Handoff: human-path AI target-state trace (read-only) — October 16, 2026

## Task / scope

Read-only reverse-engineering trace: how the original maintains AI target
state on the HUMAN player path (`DAT_004753c8==0`). No source edits, no Wine
captures, no build dirs touched. Baseline `1a18b39` (plus in-flight
`240e54f`/`831eecd`/`3ed82f6` observed, untouched).

## Headline finding

The original does **not** maintain AI target state per-tick on the human
path — it sets it **once at spawn** (`0x00405c80`) and leaves it **frozen**.
Golden proof (`analysis/oracle/sched-human-20261016.tsv`, read-only python):
every `rider.0.brain_*` word is bit-identical across all 1431 ticks —
`brain_194=18176`, `brain_198=13760`, `brain_19c=0`, `brain_188=0`,
`brain_18c=1`, `brain_190=16384`, `brain_1a0=70`, `brain_1a4=108`,
state ticks `138..15c` all 0, `160/164/168/16c/170/174/178/17c/180/184`
all 0. Native's human path (skip `ai_think`) already has the right per-tick
behavior; it is only missing the **spawn initialization**.

## 1. Human path of `0x00434660` — no brain writes, no actuation

Full-function read (`analysis/decompiled/00434660.c`, 133 lines) verified
against `analysis/original-disassembly.txt` (`434660`–`434940`).
Human branch = `DAT_004753c8==0` AND `DAT_004c5d7c==1` AND crash `+0x2f4==0`.
Callees on that path (asm-confirmed, no `0x00407bd0` anywhere):
`00409510`, `00409400`/`004095d0`, `0043ae60`, `00436830`, `004096f0`,
`00403d60`, `0040ac70`/`0040aca0`, `0040b110`/`0040b170`/`0040acd0`/
`0040ad10`/`0040ad30`, `00407f30`. Own-frame stores are only `+0x1d4`
(attack weapon), `+0x1c`/`+0xec`/`+0x2f4`/`+0x28` (combat/drive side
effects). **No store to brain `+0x194/+0x198/+0x19c/+0x188` exists.**
Neither `0x004090a0` nor `0x004091f0` (target selection) nor `0x00409f70`
(steer/speed actuation) nor `0x00408e40` (overtake) is called. Actuation on
the human path is direct drive/steer from the control word
(`0x0040acd0`/`0x0040ad30`), not AI control. So: not "targets without
actuation" per-tick — **no target computation at all**.

Crash sub-branch (crash `+0x2f4!=0`, `4346c6`–`4346f0`): calls `0x00407bd0`
(transition + state dispatch + `+0x188` accum + `0x004082f0` replan) then
returns. Code fact, golden-untested (golden crash=0 all 1431 ticks): a live
crash accumulates `replan_ticks` that then freezes after remount. Needs a
crashed-human golden to verify; do not guess.

Bike-object think `0x00407A90` (`rider.0.think=4225680` in this golden) is
animation selection only (`+0x7c/+0x81`, `0x0040ac70`/`0x0043ae90`) — no
brain writes. The player-half think is `0x00434660` (`rider.1.think`).

## 2. Demo path contrast + native mapping

Demo branch (`DAT_004753c8!=0`, `434941`–`434971`): `FUN_00407bd0` +
optional sound, return. `0x00407bd0` = transition `0x00408ad0` → state
dispatch `*0x465980` (states reach `0x004090a0`/`0x004091f0` etc.) →
state-tick accum `0x138[state]` → `+0x188 += dt`, `0x004082f0` if
`>= 0x3c` → `0x00407f30` move. Native `step_rider` (`src/race.cpp:1821`:
`if (r.ai || demo_player) ai_think(r)`) mirrors the split exactly.
**Minimal native change is spawn-side only; per-tick code must NOT gain an
`ai_think` call on the human path** (that would actuate AI steering/speed
and consume the `0x004092e0` RNG draw).

## 3. Replan on the human path: does NOT run

Golden `brain_188=0` all 1431 ticks; no `+0x188` store in the human branch.
`ai_replan` runs for the player only (a) once at spawn (see §4) and
(b) inside the crashed sub-branch. `0x004082f0` threshold verified in asm:
`addl 0x188(%edi); cmpl $0x3c; jl skip` ≡ native `> 0x3b` (`race.cpp:1700`).

## 4. Where the frozen values come from: `0x00405c80` at spawn

`0x00405990` builds every rider (player included, `param_5==0` also reaches
line 74) and calls `FUN_00405c80`, whose decompile matches asm
(`405c80`–`405e28`) line-for-line:
- zeroes brain `+0x160/+0x164/+0x168/+0x16c/+0x170/+0x174/+0x178/+0x17c/+0x180`
  (hence golden `state=0` — note native default `state=1`, uncompared for
  human, see §6);
- calls `0x004091f0` → `+0x198 = pos+0x3c0` (=13760 ✓),
  `+0x194` = straight target (=18176 ✓), `+0x19c` = curve limit with
  `current_speed=0`, which is **identically 0 for any curvature** (base 0 →
  `(0-X)+0` with `half=0`; verified in `ai_curve_limit`, `race.cpp:1327`);
- snaps `bike+0x28 = brain+0x194` (spawn lateral IS the target);
- calls `0x004082f0` once → in solo (scan NULL both ways, see below):
  `+0x16c/+0x170/+0x178/+0x17c/+0x168/+0x174 = 0`,
  `+0x1a4 = 87 + 87/4 = 108` (level-0 table → rubber-band case 0),
  `+0x1a0 = spd_a[0] = 70`.
- `+0x1bc` (base 87) arrives via the personality-record copy (`0x00407990`
  selects `0x004c6b40 + rider*0x81` words = `0x204` bytes); native
  `brain_from` (`race.cpp:134`) copies the tables into `spd_a/spd_b` but
  never sets `base_speed/current_speed/current2` for the player.

Solo-scan evidence: golden `178/17c/16c/170/168/174` all 0 means
`0x00408cd0` returned NULL (the `==0` checks in `0x004082f0` take the
zero-rating path). Native `ai_scan_replan` would return **self (index 0)**
at spawn (own half inside edges), taking the rubber-band branch instead of
case 0 → wrong `current_speed` (87, not 108, if `rubber_band[9]=0`).
Implementer must exclude self from the scan (matches original) — verify
empirically in the replay run.

Lane: golden word `brain_18c=1` with high bytes 0 ⇒ lane **byte** = 1
(`0x00407990` byte-store; sole writer of brain `+0x18c`). Only-player
career-vs-arcade difference in `0x00405990` is this lane
(`rand()%3` vs 0), so the capture ran non-career and drew 1. The draw is
pre-anchor (setup stream, 11,461 calls, `oracle_sim_test.cpp:418-421`), so
it is RNG-invisible; but the **value** feeds the spawn formula. Follow the
`demo.lane` precedent (`oracle_sim_test.cpp:369,382,385`): seed the human
player lane from the golden's measured `brain_18c` in a harness prepare
hook — no sim hardcoding. Native `bias` already equals golden 16384
(`grid_lateral[0]`, `race.cpp:21`).

## 5. Minimal native change (for the implementer, NOT implemented)

1. At player spawn (ctor, `0x00405c80` equivalent): `base_speed =
   current_speed` from `spd_b[level]`, `current2` from `spd_a[level]`;
   run straight-target (`ai_straight_target` with seeded lane) then snap
   lateral to `target_x` (no-op by road-base construction, keep for
   fidelity); run `ai_replan` once (after self-scan exclusion).
2. Harness: human-side prepare seeding `lane` from golden `brain_18c`
   (demo `prepare` pattern, `oracle_sim_test.cpp:363-405`).
3. Per-tick human path: unchanged (no `ai_think`, no `+0x188` accumulation).
4. Empirical checks in the replay run: `194/198` (lane=1 adjudicates the
   `div_pow2`-truncation vs sarl-floor caution for negative curvature —
   demo passes today, spawn sample must be checked), `1a4=108/1a0=70`
   (adjudicates scan exclusion), `1bc=87`.

## 6. Acceptance

Single-anchor replay of `sched-human-20261016.tsv` must advance past frame 1
with all compared `rider.0` AI fields equal, in `sim_fields.hpp:39-46`
order: `ai_target_x` 18176, `ai_target_position` 13760, `ai_target_speed` 0,
`ai_current_speed` 108, `ai_base_speed` 87, `ai_replan` 0 (already),
`ai_lane` 1, `ai_bias` 16384 (already). `brain_diff` state/`168`/`16c`/
`170`/`180`/state-ticks checks are demo-gated (`oracle_sim_test.cpp:687`,
`597-609` demo-only seeding) and out of acceptance scope; physics already
matches per the prior handoff.

## 7. Offset / signedness verification (vs `original-disassembly.txt`)

All brain displacements are raw byte displacements (no element-scale
hazard): `0x194/0x198/0x19c/0x188/0x18c/0x190/0x160` direct; scaled forms
checked: `0x138(%edi,%eax,4)` ≡ `state_ticks[state]`; `0x004c6b40 +
slot*0x81` on `uint32*` ≡ `0x204`-byte records; `00405c80.c:39-40`
`+ 500` decimal ≡ `+0x1f4`. Signedness: `4090a0` guard `cmpl $0xFFFFFC40`
≡ `!= -0x3c0` ✓; `4091f0` lane via `movsbl`, lane-1 div sequence is
trunc-toward-zero ≡ native `(lanes-1)/2` ✓, lane-2 abs via xor/sub ✓;
`407bd0` replan gate `cmpl $0x3c; jl` (signed) ≡ `> 0x3b` ✓;
`ai_speed_units` `(v>>12)+(v>>7)+(v>>9)` arithmetic-shift ✓ (C++20
defined). Caution (speculation): native `div_pow2` truncates while asm
`sarl` floors — identical for non-negative dividends, differs for negative
`curvature >> 3` in `ai_straight_target`; replay run adjudicates.

## 8. REA spot-check

Skipped by judgment (task allowed one optional call): Node v24.13.0 and
Ghidra 12.1.4 are present per `docs/TOOLING.md`, but every relevant
decompile (`00434660/00407bd0/004090a0/004091f0/004082f0/00407990/00405c80/
00405990/00407a90`) was verified against the authoritative
`original-disassembly.txt`, which is strictly stronger than a second
Ghidra rendering; no suspicious decompile remained to adjudicate.

## 9. Corrections to existing comments (not edited; flagging only)

`analysis/replay-notes.md:16` and `tests/oracle_sim_test.cpp:289-291`
label `rider.0.think == 4225680` as `0x00407990`. Actually
`4225680 = 0x00407A90` (`0x00407990 = 4225424`); `0x00407A90` is the bike
animation think (verified by decompile), while `0x00407990` is the
4-argument brain-pointer init, not a think function. `rider.1.think ==
4408928 = 0x00434660` is correct as stated.

## Labels

Facts: frozen-brain golden data (§head), human-path callee/gate list (§1),
demo-path `434941` sequence (§2), `+0x188` frozen (§3), `0x00405c80`
store sequence (§4), comparator gating/scope (§6), asm offset checks (§7).
Inference: arcade-capture attribution from lane byte (§4), scan self-exclusion
mechanism (§4), `0x00441750` = rand (§4). Open: crashed-human brain behavior
(§1), negative-curvature div semantics at spawn (§5/§7).
