# drive-leaves-20261016m — plan 10 background: throttle/brake REA re-confirmation (read-only)

## Task / plan

Plan 10 background slice: re-confirm drive leaves `0x0040adb0` (throttle) and
`0x0040aea0` (brake) — ramp branches + denominator clamps — against the
584 terrain proof (original row0 cap 9188 vs native row8 cap 9160).
Read-only: no source/test/doc edits beyond this handoff + scratch.
No builds, no captures, no commits.

Baseline HEAD: `f41cc62` (docs: anim advance, bike build, car think REA
handoffs 20261016l). Prior context read:
`docs/agent-handoffs/driveleaves-rea-20261012.md`,
`docs/agent-handoffs/demo584-next-20261015.md`,
`docs/agent-handoffs/nitro-20261016h.md`,
`docs/REVERSE_ENGINEERING.md` (drive-leaves bullet),
`docs/TOOLING.md` (REA pinned env), `src/bike.cpp`/`src/bike.hpp` (ramp port),
`analysis/decompiled/0040adb0.c` / `0040aea0.c`,
`analysis/demo584/` logs.

## Owned / changed files

- `docs/agent-handoffs/drive-leaves-20261016m.md` (this file)
- `analysis/drive-20261016m/` (untracked scratch):
  `rea-40adb0-throttle.md` + `.log`, `rea-40aea0-brake.md` + `.log`,
  `notes.md` (branch/clamp/threshold detail), `thresholds.py` (table script)

## Evidence (2 REA calls, both exit 0, `--format md`)

Pinned env per `docs/TOOLING.md`: `rea-agents@6.4.0`, Node v24.13.0,
Ghidra 12.1.4 libexec, JDK 21. EXE hash verified by `shasum -a 256`:
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(matches `analysis/inventory.json`).

| Function | evidence_id | REA raw pseudo | Export pseudo | Match |
|---|---|---|---|---|
| `0x0040adb0` throttle | `ev_d8d8d73b…9394946c` | 1392 chars | `analysis/decompiled/0040adb0.c` 1393 | TRUE |
| `0x0040aea0` brake | `ev_3a199063…66444c78` | 1112 chars | `analysis/decompiled/0040aea0.c` 1113 | TRUE |

(Char delta in both = one leading blank line, same as the Oct-12
1336/1337 and 1056/1057 confirmations; bodies identical.)

## Ramp branches (REA-confirmed)

`0x0040adb0`: nitro `+0xc0&0x10` or player wreck/cop-hold
(`DAT_004642d8` + `DAT_004753c4 > 0`) → brake fallback `+0x27c`;
clamp target to `+0x260`; rise `+0x18*+0x264` / fall `+0x18*+0x268`;
clutch `+0x244` → force 0; sub-1 pass-through `+0x114 = v>>8`;
gear force `+0x114 = (+0x1d0[gear]*v)>>16`;
nitro `+0xc0&0x20` scales `+0x114 *= DAT_00465a2c>>8`, `+0x240` unboosted.
`0x0040aea0`: stopped (`+0xec==0`) → 0; nitro → `(cap*0x180)>>8`;
else clamp target to negative cap `+0x27c`; ramps `+0x284`/`+0x280`
(negative rates; assembly SUB/ADD confirms);
`+0xec>=0 → +0x114 = v>>8`, reverse → `+0x114 = -v>>8`.
No RNG in either leaf. Native `src/bike.cpp:145-173`
(`throttle`/`brake`/`drive_mode`) reproduces every branch — no delta
found in this spot-check.

## Clamp law

`cap +0x260 = d[0x250]*(mass+75)*256 / (mass+75+trunc((rider-75)/8))`;
`rise +0x264 = cap/clamp15(d[0x254])`, `fall +0x268 = cap/clamp15(d[0x258])`.
Numerator `3050496`; row0 (rider 90) denom 332 → cap **9188**/rise 918/
fall 612; row8 (rider 91) denom 333 → cap **9160**/rise 916/fall 610.
Same SPEC/gear factor 232 both rows.

## 584-threshold table (`force = (232*throttle)>>16`, gear 2)

| tick | throttle+0x240 | force+0x114 |
|---|---|---:|
| orig 581 | 9188 (= cap row0) | 32 |
| orig 582 | 8576 | 30 |
| orig 583 | 7964 | 28 |
| orig 584 | 7352 | 26 |
| orig 585 | 6740 | 23 |
| native 584 | 7330 | 25 |

Original falls 612/tick (row0 `9188/15`); native falls 610/tick
(`9160/15`). Force edge `7344 → 25`, **`7345 → 26`**; native 7330 misses
by 15. Native `integrate_speed` at 584 is exact (`6182+25-20=6187`); the
1-unit gap is the row0/row8 hidden-input divergence (per
`demo584-next-20261015.md` fix recommendation: explicit player-rider index
/ derive from recorded `raw_1e0`), not integrator rounding. No constant,
rate, or rounding tweak justified.

## NOT-changed list

Tracked files: ALL untouched — specifically sibling in-flight
`src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
`src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
`tests/race_cache_test.cpp`, `tests/road_test.cpp` were neither read nor
touched; no other `src/`/`tests/`/`docs/`/`cmake/`/`tools/` changes.
`analysis/decompiled/`, `analysis/oracle/`, `analysis/demo584/`, build
dirs, `../cdrom`, prefix/DLL installs untouched. No builds, no `ctest`,
no captures, no commits. `git status` delta vs baseline: only the two new
paths above (plus siblings' pre-existing untracked/modified files).

## Checks / remaining work

- Checks run: EXE `shasum` match; 2 REA calls exit 0; char-count comparison
  via `thresholds.py`. No build/test (docs + scratch only).
- Remaining for plan-10 owner: rider-row-identity fix (explicit index /
  `raw_1e0` derivation), row0-vs-row8 factory check (caps 9188/9160),
  fresh-600 single-anchor diagnostic; `0x0040ad30` steer-target unchanged
  since Oct-12 confirmation (out of this slice's 2-call budget).
- REA is a spot-check per `docs/TOOLING.md`, not runtime proof.

## Live processes / resources

None. Two synchronous REA calls (ephemeral Ghidra, no shared project
writes); no background jobs, no build locks, no Wine/oracle processes held.
