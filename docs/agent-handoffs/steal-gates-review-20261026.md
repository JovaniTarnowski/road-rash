# steal-gates-review-20261026

Task: Review plan backlog and recover original steal gates; audit existing captures without changing simulation

Baseline: 901a02117f6449d04c96e01ecaec6a5c4ec835bc

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-steal-gates-review-20261026`
Owner PID: 95735
Resource token: `0b4430ef-7c4c-4df6-b45b-a4192760df60`

Owned files:

- `/Users/archit/Dev/rr/native-port/tools/steal_capture_audit.py`
- `/Users/archit/Dev/rr/native-port/tests/steal_capture_audit_test.py`
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/steal-gates-review-20261026.md`

## Progress / evidence

Completed the plan backlog review and steal-gate research/audit below.

## Checks / remaining work

Five synthetic checks pass; runtime steal acceptance remains open (details below).

## Live processes / resources

No live capture, Wine, Ghidra, build or server processes. File claims released at close-out.

## Plan review (October 26, 2026)

Reviewed the plan index, every numbered plan's task/status/exit-criteria
inventory, current status matrix, Git changes, resource registry and recent
OMP gameplay/contact/REA/frontend/terrain/multiplayer handoffs. This is a
backlog review, not a fresh acceptance run for every plan.

| Plans | Current interpretation / next work |
| --- | --- |
| 01–08 | Historical implementation recipes; remaining acceptance belongs to 10/11/13–18/24. Do not restart their old World/Object architecture. |
| 09, 12 | Superseded by 34 and 17 respectively. |
| 10, 11, 11a, 16 | Focus unfinished capture coverage: steals, airborne shadow, isolated/field traffic, contact after 3840, full human/all-course acceptance. 11a's checked boxes contain explicit partial evidence; they are not full acceptance. |
| 13, 18 | Frontend/save/settings implementation and round trips exist. Current frontend pixel work has an active claim; preserved. Exact MIDI/audio and full pixel acceptance remain separate. |
| 14, 15 | Device observations, owner release/signing and complete release acceptance remain outstanding; APK assembly alone is insufficient. |
| 17, 24 | All-course/level/branch and mixed pedestrian/controller parity remains open. Latest police captures supersede older “no live cops” text. Napa merge fix covers approaching type-4 hill merges, not every split case. |
| 19–21 | High-resolution/enhancement implementation substantially exists; asset attach gaps and enhancement acceptance remain. Not the next original-behavior RE priority. |
| 22, 27, 30 | Tooling/hardening and field-access work; distinguish remaining gates/optional naming work from gameplay RE. |
| 23 | Web build/deployment exists in recent handoffs; old “not published”/plan-09 scope language is historical. Browser and performance acceptance still need their own evidence. |
| 25, 26, 28, 29, 31 | Landed infrastructure/review work; do not reopen solely because older summaries lag. |
| 32 | Consolidated continuation checklist; use current captures/handoffs for actual blockers. |
| 33 | Linux/Windows platform acceptance not started. |
| 34 | In scope, C++ server selected, modern internet design. Original network background is recovered; multi-human rules/determinism and implementation remain. |

Selected plan 10 Task 6 / plan 16 Task 3: settle the steal-gate field and
measurement questions before repeating the unproductive long capture farm.

## Recovered original behavior

Reference EXE SHA-256 rechecked:
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.
Used existing `analysis/decompiled/004096f0.c`, `00409400.c`, `00409510.c`,
`00409560.c`, `004095a0.c`, `00409c10.c`, `00405c80.c`, `0043c980.c` and
`analysis/original-disassembly.txt`; no Ghidra writes or Wine capture.

1. Resolver entry `0x409712–0x409723` accepts selected graphics descriptor
   flags `&0x600000` **OR** attacker half frame byte `+0x81 == 2`.
   Merely being alongside an armed rider is not a resolver invocation.
2. `0x409400` selects the nearest type-8 half within `0x1e0` along the
   scheduler scan and writes its associated bike into brain `+0x168`.
   It does not choose whichever opponent passes the lateral reach test.
3. `0x409793` rejects victim animation 9. `0x4097a5/0x4097b3` applies
   attacker side from `0x4095a0` and `0x409c10`: strict longitudinal gap
   `<0x40`, strict lateral reach 5/7/8 units of `0x1000` for fist/club/chain
   (kick 5), strict height gap `<0x3000`; side is
   `(attacker_lateral < victim_lateral) == side`.
4. `0x4097cb–0x4097d1` compares the **victim's** half `+0x1cc` as an
   unsigned deadline; it must be strictly less than global tick.
5. `0x4097d7–0x4097dc` loads **victim half +0x81**, compares byte 1 and
   uses signed `jg`. Thus signed `int8_t(frame) <= 1` (including negative
   sentinel values) is exact. The native frame is already `int8_t` in
   `ActorAnimationState`; its `<2` test agrees. No frame-offset fix is needed.
6. `0x4097e8–0x409807`: attacker active attack weapon `+0x1d4 == 0`;
   victim active weapon `+0x1d4` neither 0 nor 3. Inventory `+0x1c8` alone
   is not this gate. `0x409510` then matches victim current animation
   against all four variants for that active weapon.
7. Only then does `0x40981c` consume one MSVC RNG draw and test `&3 == 3`.
   A failed roll falls through to normal damage; there is no guaranteed
   roll on every visually overlapping tick.
8. Success copies victim inventory `+0x1c8` to attacker inventory AND
   active weapon, zeros both victim weapon words, then writes
   **attacker** `+0x1cc = tick+300` (`0x409837–0x40988d`). This asymmetry
   is real: the gate reads the victim deadline and success writes the
   attacker deadline. `0x405c80` initializes the deadline to zero.
9. `0x40989c` calls `0x409560` on the victim AFTER clearing its active
   weapon. Therefore the side helper now tests fist-table entries, even
   though the previous attack used an armed animation. Native does the
   same; do not “fix” it to use the previous weapon. Attacker selects
   `0x464538[new_weapon + side*4]`, queues animation 2; victim selects
   and queues 2. `0x43c980` is a network-event encoder, gated by two
   network flags, not an extra local steal RNG/physics operation.

These findings close the proposed frame-offset/unsigned-frame and weapon
mapping questions. They do not prove runtime steal parity.

## Capture evidence correction and audit tool

The current proxy emits opponent `anim`/`anim_frame` only when
`RR_ORACLE_RATINGS` is enabled (player animation is unconditional). The
three audited farm traces omit every armed rider's animation/frame.
Earlier handoff overlap counts therefore cannot establish simultaneous
armed attack + early victim frame + actual resolver reach. Their zero
`raw_1cc` observations remain useful negative evidence, not a successful
steal acceptance test or a demonstrated RNG failure.

Added `tools/steal_capture_audit.py`: streams TSVs, checks the completion
marker and contiguous ticks, hashes input, reports missing weapon/animation
fields, and counts nonzero cooldown snapshots. It does not infer target
identity from shifting row numbers. Rows without a weapon raw window are
counted separately (the extra half row is not another armed rider).

Local output: `analysis/oracle/steal-audit-20261026.json` (uncommitted).

| Trace | Frames | Armed active-weapon snapshots | Armed snapshots with frame | Nonzero steal deadline snapshots |
| --- | ---: | ---: | ---: | ---: |
| sim-napa-steal-vary | 4181 | 16787 | 0 | 0 |
| sim-napa-steal-farm-l1 | 15181 | 44718 | 0 | 0 |
| sim-napa-fight-steal-l5 | 5181 | 12044 | 0 | 0 |

Across 24,543 frames: 98,835 rider snapshots with a weapon raw window lack animation
and frame. These are post-tick snapshots; even complete animation coverage
would not prove which gates held at the exact scheduler invocation.

Reproduction:

```sh
python3 tests/steal_capture_audit_test.py
python3 tools/steal_capture_audit.py \
  analysis/oracle/sim-napa-steal-vary.tsv \
  analysis/oracle/sim-napa-steal-farm-l1.tsv \
  analysis/oracle/sim-napa-fight-steal-l5.tsv
```

Five synthetic checks pass: absent animation cannot become early-frame
coverage; signed byte 255 maps to -1; row state does not leak across ticks;
truncated capture and tick gaps fail. No native source changed; CMake/native,
Android, sanitizer and device gates were not run for this research/tool slice.

## Concrete next action / remaining acceptance

When the frontend worker releases the proxy/prefix, capture a short L1 field
run with `RR_ORACLE_RATINGS=1`; reuse existing phased/passive controls rather
than another long blind farm. To establish steal eligibility, add a separate
resolver-entry probe recording attacker/victim node addresses, half frame,
selected descriptor flags, both weapon words, victim deadline, reach result,
and RNG before/after specifically around `0x40981c`. Keep normal RNG flow.
A successful transfer must be original-observed, then replayed from one RNG
anchor before completing plan 10/16 steal acceptance.

Shared PROGRESS/VERIFICATION/REVERSE_ENGINEERING files are actively claimed
by frontend-acceptance-all-20261026; this uniquely named handoff contains the
progress, verification and recovered facts for integration per AGENTS.md.
No accepted golden, simulation expectation, source file or shared doc changed.
