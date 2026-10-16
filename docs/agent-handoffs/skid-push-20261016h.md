# Handoff: skid/slide + move-push + steer-authority background (Plan 10 Task 5) — 20261016h

## Task / scope

Read-only background for Plan 10 Task 5: skid/slide `0x00438c70` + move-push
`0x004395e0` + steer authority `0x0040ad30` (human physics gates the native
game runs demo-tuned, per `docs/SIM_SCENARIOS.md:229-231`).
Baseline `12f45ec`. No tracked files edited; no builds, no captures, no commits.

## Evidence (pinned REA spot-check)

Env per `docs/TOOLING.md`: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=…/openjdk@21`, `rea-agents@6.4.0`, `--format md`, both exit 0 on the
hash-verified EXE (`73d7f974…f18a2`). At most 2 calls used; `0x0040ad30` was
NOT re-called (budget cap — Oct-12 whitespace-identical confirmation stands).

| fn | evidence_id | result |
| --- | --- | --- |
| `0x00438c70` | `ev_f8aa9739d6e797fdb6b9674f814ac79da711ed23b9b58bd8a7b53c0d02d825c1` | human-gate lines present (pseudocode + disasm `0x438db3`/`0x438dc2` + xref table) |
| `0x004395e0` | `ev_2caed8b7522007f03fac6db513771ec432f5fac0b3714a1352ac339de465e079` | human-gate lines present (pseudocode + disasm `0x439840`/`0x439849`) |
| `0x0040ad30` | prior `driveleaves-rea-20261012` (494/495 chars, TRUE) | not re-run |

Raw outputs (disc-derived, local-only): `analysis/skid-20261016h/rea-438c70-skid.md`,
`rea-4395e0-move.md` (+ `.log`s), `notes.md` (this slice's only other write).

RNG draws: **zero in all three** — callee sets are `{0043a0c0, 0044f5a0,
00403d60}` / `{0044f5a0, 00439270}` / leaves per prior handoffs; no
`0x00441750`/`rand` reference in any export or fresh REA text (only
`operand_input` table-header substrings). Divergences here are deterministic.

## Demo-tuned vs original

| gate | original (export line) | native today | direction |
| --- | --- | --- | --- |
| steer authority `0x0040ad30:8,16` | human: `DAT_004b7e7c`-based (`-0x20` offset left / raw right); demo: fixed `∓0x20` | `steer_input` (`src/bike.cpp:227-233`) fixed `±0x20` always | demo-tuned |
| traction skid `0x00438c70:66-67` | player bike (`== DAT_004642d8`) **and** human (`DAT_004753c8==0`) **and** `grip < +0x290` **and** `+0x29c <= \|steer\|` → `+0x278=1`, `DAT_0046dce0 += dt`, `>0x19` → `0x00403d60(reason 6)` | `traction` (`src/bike.cpp:339-346`) gates on `player` (bike identity) only, no `demo_hold` gate | human-rule-always-on (fires on demo captures too); note native `skid_ticks` is per-`Bike`, original `DAT_0046dce0` is a single global |
| move push `0x004395e0:115` | slide branch (`+0x278&4`, `+0x164 > 0x32`): human player bike `iVar6*800` direct, else `(iVar6*600)/dt` | `move()` always dt-scaled form (per `REVERSE_ENGINEERING.md:1288-1289`, `offroad-trace-20261015.md` §2) | demo-tuned |

Force/thermal laws (`0x00438c70`, export + fresh REA agree): grip
`g=((((base − heat·0x1000>>8))·+0x294)>>8·grip[s])>>8·+0x160)>>8`, base
`+0x148` clean / `+0x14c` skidding, heat timer `+0x298` decays by `dt`/`tick`
floor 0; demand `|u|+|lat|`; skid byte `+0x164 = demand−g` (wrap); `skid >
0x28` bleeds `skid_loss[s]` (`0x0046dd18`) off speed; rough-surface
curve-restrict `0x0046dd28`, damp `0x0046dd08`, gain `0x0046dcf8`; yaw
halved < 4000, +1/8 > 8000; lateral applied only when speed > 300.
Out of this slice: sched `+0xc` copy `0x00438170:78`, wreck rule
`0x0040a600:78` (recorded in `REVERSE_ENGINEERING.md:1290-1293`).

## Human-scenario capture spec (for the porter)

Base: `sched-human-20261016` recipe (`docs/SIM_SCENARIOS.md:212-231`) —
course 4, seed 1, solo, `RR_ORACLE_HUMAN=1`, recorded 1250-word human stream,
1431 ticks, `player.demo_hold == 0` throughout. To traverse THESE gates the
golden must show: `+0x164 > 0x32` with `+0x278&4` set (slide branch → the
`800`-vs-`600` push fork), and grip-below-demand skid ticks with
`DAT_0046dce0` accumulation. Candidate windows are the existing script's
hard-steer phases (words 300–359 left, 480–539 right, 840–899 left,
1020–1079 right). Per-tick `+0x164`/`+0x278`/`+0x10c`/`+0x298`/`DAT_0046dce0`
proof is porter-side to verify against the proxy's dumped field set before
claiming gate coverage; pre-step force/thermal capture need per
`demo584-next-20261015.md:90`.

## NOT changed

No tracked file touched. Avoided live-claimed paths: `docs/PROGRESS.md`,
`docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`, `docs/plans/10-*`,
`docs/plans/32*`, `src/race.cpp` (not read), `traffic.*`, `terrain.*`,
`race_setup.*`, proxy/`oracle`, `CMakeLists.txt`, `oracle_sim_test`,
`traffic_test`, `race_setup_test`. `git status` otherwise unchanged from slice
start (pre-existing untracked scratch + three unrelated handoffs only).

## Remaining (porter-side)

1. Port the three `demo_hold` forks behind `demo_hold_` with per-gate replay
   evidence on the human golden (note the skid gate's inverse direction).
2. Confirm proxy dumps the §-listed per-tick skid/thermal fields.
3. Sched-copy `0x00438170` + wreck `0x0040a600` gates still open (Task 5 slice).
