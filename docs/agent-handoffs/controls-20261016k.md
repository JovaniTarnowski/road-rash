# Controls spot-check handoff — controls-20261016k

Read-only background check: player control word `0x004b7ed0` via `0x0044f800`
(keyboard+joystick), control bits, and the `DAT_004753c8` demo-hold branch in
`0x00434660`. No tracked files edited; no builds, no captures, no commits.

- Baseline: `d6ebde0` (HEAD matches; `git rev-parse` confirms).
- Reference EXE: `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
  (`../cdrom/ROADRASH/ROADRASH.EXE`, matches `analysis/inventory.json` and both
  REA subject digests below).
- New files only: `docs/agent-handoffs/controls-20261016k.md` (this file) +
  scratch `analysis/controls-20261016k/` (2 REA `--format md` outputs + logs).
- Live claims observed (untouched): 7 workspace sessions, incl.
  `countdown-timers-20261015` (owns `src/race.cpp`, shared docs) and
  `course-bounds-20261016e`. Pre-existing tracked modifications
  (`src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp/hpp`, `tests/race_cache_test.cpp`, `tests/road_test.cpp`)
  belong to another worker and were preserved.

## REA spot-checks (pinned, budget 2/2 used)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`npx --yes rea-agents@6.4.0 function <EXE> <addr> --format md`.

1. `0x0044f800` (control-word poll) → `analysis/controls-20261016k/rea-0044f800.md`
   - `evidence_id: ev_4792a6b4795c1f88394ddaac49168536ea77f25f1d14d8fb21614cfa23b9ae69`
   - Semantics (matches `docs/REVERSE_ENGINEERING.md:151`): `uVar3 = 0`;
     keyboard `FUN_0044fa80()` unless `DAT_004c5b10`; joystick OR loop over
     `DAT_0049e65c` rows via `FUN_0044f880` dead-zone test, setting
     `uVar3 |= (&DAT_00476c00)[i*2]` (bit at row stride 8, i.e. `+0x476c00`).
     Caller: `0x00442e10` (frame driver polls once per frame into
     `DAT_004b7ed0`; multi-step frames reuse the word — record at the step).
2. `0x0040a600` (run-back/recovery) → `analysis/controls-20261016k/rea-0040a600.md`
   - `evidence_id: ev_92615e29ad835b7b8c1287640cd9314db301dd34b146c38ff0c7519428cd6179`
   - Walk-steer gate re-confirmed verbatim (matches RE:871):
     `(DAT_004b7ed0 & 0x7800) != 0 && DAT_004642d8 == param_1 &&
     DAT_004753c8 == 0 && DAT_004c5d7c == 1`, then per-bit steer
     (`0x1000`/`0x800`/`0x4000`/`0x2000`); refs at `0x40a835` (word),
     `0x40a84c` (demo hold), racing-state compare adjacent.
   - `0x00434660` deliberately NOT fetched: known REA procedure-DB gap
     (RE:165 — valid code per `original-disassembly.txt`, port from project
     exports), and the budget allowed only one of `0x00434660`/`0x0040a600`.

## Bit map (native vs original)

Native `src/bike.hpp:76-79` (tables `0x00465fb9`/`0x00476c00` order):

| Native bit | Meaning | Original `0x00476c00` row (RE:973) |
| --- | --- | --- |
| `0x1` | accelerate | row 0 accel `0x1` dig — same |
| `0x2` | brake | row 1 brake `0x2` ANALOG — same bit |
| `0x4` | left | row 2 left `0x4` dig — same |
| `0x200` | right | row 3 right is `0x8` ANALOG — REMAPPED |
| `0x8` / `0x400` | lean down/up | rows hold look-back `0x80` / camera `0x400` — native reuses bits for lean |
| `0x10` | nitro | row 9 nitro is `0x200` dig — REMAPPED |
| `0x20` / `0x40` / `0x80` | punch/kick/backhand | rows 4/5/6 punch `0x10`/kick `0x20`/backhand `0x40` — SHIFTED by one row |
| (none) | dismount `0x100`, look-back, camera | row 7 `0x80`, row 8 `0x100` (dead, no binding), row 10 `0x400` — no native bit |

Key→action (`0x00465fb9` codes → `0x00476c00` bit via `0x0044fa80`): walk-gate
docs (RE:867) list accel/brake/left/right codes `0x94/0x96/0x93/0x95`;
EXE default tables (`src/settings.cpp:191-199`: `0x00466618`/`0x00466630`)
row order 0–10 accel..camera. Row-3/right default `0x8400` vs the `0x95`
reading at RE:867 left unverified — porter should re-check against the
`0x0044fa80` export before relying on it.
On-foot word bits are distinct high bits: `0x800/0x1000/0x2000/0x4000`
(`0x7800` mask, `0x0040a600:300-314`).
Native application (`src/race.cpp:1826-1855`, `src/bike.cpp:235-247`):
demo-hold (or not-started) takes the AI branch and ignores the word; else
`nitro_think`, drive dispatch (`0x1`→throttle, `0x2`→brake, else coast),
`steer_input` mode left/right + lean. Nitro/attacks in `apply_controls`
noted not-yet-ported at `bike.cpp:236`.

## Branch semantics (`DAT_004753c8` demo hold)

- `0x00434660:25` — player think: control-word branch vs demo-AI branch;
  attract path sets hold `= 1` (`0x00421430` menu path writes `= 0`; the
  `= 1` comes from bootstrap code, hence `RR_ORACLE_HUMAN=1` clears it at
  race start, nothing re-arms mid-race). Confirmed branch site `0x4346a1`.
- Six in-race readers, all `== 0` = human (RE:1276-1294): `0x00434660`
  think, `0x0040ad30` steer authority (human `DAT_004b7e7c`-based vs demo
  fixed `-/+0x20` — native `steer()` ungated), `0x00438c70` skid flag,
  `0x004395e0` move push (`iVar6*800` vs dt-scaled), `0x00438170:78`
  scheduler `+0xc` copy, `0x0040a600:78` wreck rule. Native human-path gaps
  enumerated at `docs/SIM_SCENARIOS.md:227-231`.
- Think words: AI-think `0x00407a90` = 4225680; player-think entry
  `0x00434660` = 4408928 (seen as frozen `rider.1.think` in old captures).

## Human-proof note (for porter/acceptance)

A scenario called "human" must prove it reaches the player-control branch;
attract-demo replay does not (`SIM_SCENARIOS.md:109-117,172-178,212-231`).
Recipe: `player.demo_hold == 0` on EVERY tick (`control_branch: human` by
instrumented evidence, never by name) + `player.think`/`player.think_count`
from `0x00434660` recorded every frame; `oracle-sim-test` infers the branch
from measured `demo_hold`, never from `solo`/scenario name/input nonzero.
Reference: `sched-human-20261016` (course 4, seed 1, solo, 1431 contiguous
ticks, hold 0 throughout, `RR_ORACLE_HUMAN=1` seed-hook clear) with the
1250-word human stream (`sim-napa-solo-human.controls`: throttle/brake/left/
right phases per the words→ticks table at SIM_SCENARIOS:74-85, word N drives
tick 60+N, 59-frame start-hold, `0xb4` exhaustion coast). Old
`sim-napa-solo-human*` files (no `demo_hold` probe) are demo-branch replays.

## NOT-changed list

- No tracked file modified, created, or deleted (this session wrote zero
  tracked paths; pre-existing dirty files left intact).
- No builds configured or run; no `ctest`/gate; no LSP/database refresh.
- No Wine/oracle captures; no Ghidra import/decompile; no proxy edits.
- No REA calls beyond the 2 budgeted; `0x00434660` not re-fetched (known gap).
- No commits, no staging, no workspace claims taken, no processes started.

## Remaining / next

- Porter work (human-path gates in SIM_SCENARIOS:227-231) is owned elsewhere;
  this check adds no new divergence claim.
- Suggested follow-up (not done here): verify `0x00465fb9` row-3/right code
  reading against the `0x0044fa80` export; keep native-vs-original bit
  remap table above in mind when wiring `bound_control_bits`
  (`src/input.cpp:50-53`: row 3 → `control_right`, row 9 → nitro — matches
  native bits, not original word bits).
