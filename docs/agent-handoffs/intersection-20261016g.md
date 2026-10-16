# Intersection follow-up — controller think, crossing cars, three open items (20261016g)

Task: READ-ONLY Plan 24 Task 1 follow-up — intersections. Controller think
every 30 ticks, crossing-car park/stop/follow, signal latch index-0/2;
kind-0 lane changes around non-car obstacles (open), split-roadway parks
(open), sub-slice link for signal laterals (open).
Baseline: `242f718` (HEAD verified; short-hash command needs a longer
prefix in this tree — full hash used).
No commits. No builds. No captures.

## Owned / changed files

- `docs/agent-handoffs/intersection-20261016g.md` (this file)
- `analysis/intersection-20261016g/` (scratch only): `rea-41ef60-signal.md`
  (+ `.log`), `rea-44e710-carthink.md` (+ `.log`), `open-items.md`
  (open-item semantics with export line refs)

## Evidence

Pinned REA (`rea-agents@6.4.0`, Ghidra 12.1.4, Node v24.13.0, JDK 21),
EXE sha256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
verified, `--format md`, 2/2 call budget spent on the open-item functions:

| Function | evidence_id | normalized pseudocode vs `analysis/decompiled/` |
| --- | --- | --- |
| `0x0041ef60` (sub-slice link) | `ev_ede177b17070ae79454883d1d44bdc84735aad725b5151c77685301731479824` | 837 chars, match TRUE |
| `0x0044e710` (kind-0 lane change + parks) | `ev_cc674880e06b84b7de5d4e615e7e1597cabed7e7dc1e8adc731765ebd86c9c69` | 6588 chars, match TRUE |

NOT called this turn (Oct-12 confirmations stand — see
`intersectctl-rea-20261012.md`, `carthink-rea-20261012.md`): `0x0041eca0`,
`0x0041f040`, `0x0044df60`. REA is a spot-check, not runtime proof.
Sources read (rg/fd, no edits): `src/traffic.hpp/.cpp`,
`tests/traffic_test.cpp` (lines 499–633), `analysis/intersections-notes.md`
(incl. Oct-12 second-pass corrections), `analysis/intersection-capture-notes.md`,
`docs/REVERSE_ENGINEERING.md` "Traffic zones and intersections",
`analysis/decompiled/{0041ef60,0044e710,0044dc10}.c`.

## Open-item resolution (trace closed, port gaps confirmed)

1. **kind-0 lane change (open).** `0044e710.c:63-97`: gap `< (iVar7·3)/2`
   + slower-obstacle gate + obstacle type without `0x10` → kind 0 shifts
   lane (0 → +1 iff right lanes ≥ 2, else −1; kind 2 mirrored on the
   left), `bVar2` halves the lateral target (`:292-297`). Cars never
   trigger it. Port never changes lanes (`traffic.cpp:230` comment) — the
   Oct-12 car-vs-crossing deadlock is a real divergence risk.
2. **Split-roadway parks (open).** `0044dc10.c` export (not REA'd, outside
   budget): kind 1/3 park past the other roadway's edge on type-2 slices;
   plus `0044e710.c:122-214` linked-slice lane-validity parks (own +
   linked counts, lane ± step retry, else park). Port has neither (only a
   single-slice `target = 0`, `traffic.cpp:261-264`).
3. **Sub-slice link (open).** `0x0041ef60`: `node+8+side·4` is a linked
   sub-slice pointer (disassembly `41ef6c/41ef81/41ef8f`); lateral
   interpolates own-shoulder-mid → linked same-side shoulder-mid by
   fraction/256. Port uses the next route sample (`place_signal`).
   Needs `RouteSample` to carry the link; unverified hypothesis that the
   link equals next-slice on plain road.

Verified ported + tested (no action): 30-tick think (`next_tick` pinned,
`traffic_test.cpp:530`); crossing stop/follow/ring-window park + red-hold/
green-go (`:532-585`); index-0/2 latch (`test_signal_phase`, REA e710:56
shows the `0x00468f00[kind]` lookup); signal draw still dead (no DAT
writer — unchanged).

## Capture golden plan (Step 5, still open — nothing run this turn)

1. Extend `dump_sim_tick` in `oracle/aweman_proxy.c` (SHARED — coordinate;
   never edit mid-flight): car pool `0x0049e654` 16×0x210 via
   `rr_sim_car_fields`, ring cursors `DAT_004b6e50/0049f240/004b6e5c`,
   fix node-think offset `+0xdc` → `+0xe0`. Rebuild via `oracle/build.sh`
   + `install.sh`.
2. Re-capture `city-intersection` (CITY course 1, seed 1, drive 2, SIM=1)
   to the natural `end` marker (no FRAMES). Bootstrap is flaky (menu
   re-entry poisons the race — `g_race_started` never cleared): kill
   orphan Wine first, check fds, verify no live session owns the prefix.
   Details: `analysis/intersection-capture-notes.md`.
3. Accept: car-field replay, single RNG anchor, zero divergence; one CITY
   frame pixel-matches in `oracle-scene` (signal heads invisible, so the
   frame is unaffected by them).

## NOT-changed list

Zero tracked-file modifications (`git status --short` shows no `M` lines —
only pre-existing `??` entries). Untouched: `src/traffic.*`, `src/race.cpp`,
`tests/`, `oracle/`, prefix/install, `docs/REVERSE_ENGINEERING.md`,
`docs/plans/24*`, `analysis/decompiled/`, `analysis/oracle/`,
`tools/decompile.sh` outputs. No sibling paths touched (noted live
`analysis/save-20261016g-rea-*` tmp files + `siren-engine-20261016g/` belong
to another session — left alone).

## Live processes / resources

None held. Two ephemeral REA Ghidra sessions, both exit 0, cleaned up.
No Wine, no builds, no locks. Next: port the three gaps (lane change →
split parks → sub-slice link, each with a failing test first) once the
capture pipeline yields the Step-5 golden.
