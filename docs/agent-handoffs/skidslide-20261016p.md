# Handoff: skid/slide steer-authority + shape tables (Plan 10 Task 5) — 20261016p

## Task / baseline

Read-only background completing the leftover from `skid-push-20261016h`:
`0x0040ad30` steer authority (not re-run last session — 2-call budget cap) plus
the per-Bike vs global `DAT_0046dce0` shape gap. Baseline HEAD `e309e0a`.
No tracked files edited; no builds, no captures, no commits.

Prior handoff read: `docs/agent-handoffs/skid-push-20261016h.md`
(`0x00438c70`/`0x004395e0` human gates, force/thermal laws).
Sibling's eight render/road files were not touched or read
(see NOT-changed list). Native steer/skid code read via `rg` excluding those
eight: `src/bike.cpp`, `src/bike.hpp`, `tests/bike_test.cpp`.

## Evidence (2 pinned-REA calls, both exit 0, `--format md`)

Env per `docs/TOOLING.md`: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=…/openjdk@21`, `rea-agents@6.4.0`. EXE hash verified:
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.

| fn | evidence_id | result |
| --- | --- | --- |
| `0x0040ad30` steer authority | `ev_027b1a07fafeef05b3225823ed7d9aed7c7e6d4750d5b308c4a523b7cd9e6a94` | pseudocode 389/389 chars whitespace-identical to `analysis/decompiled/0040ad30.c`; disasm `0x40ad35-0x40ada7` confirms both flag loads + `SUB EDX,0x20` human path; single callee `CALL 0x0040af50` |
| `0x0040af50` steer apply | `ev_4760d6665f1a9faa67f167f48862e43b959b7874a247a3a389e30ea633f30139` | pseudocode 1635/1635 chars whitespace-identical to `analysis/decompiled/0040af50.c`; callee `0x0044f4a0` (sine) |

Raw outputs (disc-derived, local-only): `analysis/skidslide-20261016p/`
(`rea-40ad30-steer.md/.log`, `rea-40af50-steerapply.md/.log`, `notes.md`).

RNG draws: **0 in both** (`rand` hits are `operand_input` header substrings).

## Shape table (original `.data`, dumped locally from EXE)

| addr | name | values (surface class 0..3 unless noted) |
| --- | --- | --- |
| `0x0046dcd8` | contact_base | 74 |
| `0x0046dce0` | skid-tick global (init) | 0 — single dword; `+= +0x18` while skidding, `>0x19` → `0x00403d60(reason 6)`, cleared on exit |
| `0x0046dce4` | surface_drag | [0, 2, 20, 30] |
| `0x0046dce8` | grip | [256, 240, 128, 100] |
| `0x0046dcf8` | steer_gain | [230, 230, 128, 100] |
| `0x0046dd08` | steer_damp | [6, 5, 1, 1] |
| `0x0046dd18` | skid_loss | [12, 12, 0, 0] |
| `0x0046dd28` | curve [surface + 4*((pos>>8)&7)] | rows: [256,256,247,242] [256,256,264,269] [256,256,249,244] [256,256,268,273] [256,256,243,238] [256,256,268,273] [256,256,245,240] [256,256,264,269] (range 238..273) |

Native `read_physics_tables` (`src/bike.cpp:46-72`) reads every row above.
`PhysicsTables::synthetic()` matches all except `curve` (fill 256 — test-only
gap; real-EXE path is exact).

## Demo-vs-human gating (porter-ready)

| gate | original (human side) | native today | direction |
| --- | --- | --- | --- |
| steer authority `0x0040ad30` | human (`DAT_004753c8==0`) **and** `DAT_00476c98≠0` (set by `0x0044f7c0` when control config `DAT_00465fb8..fce` has any byte): `s = DAT_004b7e7c ∓ 0x20` (analog axis from `0x0044fa00`, `param_2/800`); otherwise fixed `∓0x20` | `steer_input` (`src/bike.cpp:227-233`) fixed `±0x20` always | demo-tuned |
| steer apply `0x0040af50` | no demo/human gate — pure shape: clamp ±0x200000, sine target `/+0x2b0`, limit `+0x29c` (+¼ when leaning), rates `+0x2a8`/`+0x2ac` × `+0x18`, lean shaping `+0x2d0`/`+0x2d4` | `steer` (`src/bike.cpp:192-225`) matches | ported |
| traction skid `0x00438c70:66-67` | player bike (`== DAT_004642d8`) **and** human **and** `grip < +0x290` **and** `+0x29c <= \|steer\|` → global `DAT_0046dce0 += dt`, `>0x19` → crash | `traction` (`src/bike.cpp:339-346`) gates on `player` bool only, per-`Bike.skid_ticks`, simplified `+0x2f4=1` | human-rule-always-on (fires on demo too) **+ shape gap** (per-bike vs single global) |
| move push `0x004395e0` slide branch | human player bike `iVar6*800` direct, else `(iVar6*600)/dt` | always dt-scaled | demo-tuned (per prior handoff) |

Caller note: `0x00434660:117-130` derives mode (control-word bits `4`→left,
`0x200`→right) and lean (`0x400`/`0x8`) from `DAT_004b7ed0`; native
`apply_controls` mirrors the bit mapping but feeds the demo-side fixed `±0x20`.

Per-Bike vs global detail: with one global dword, a second skid window
inherits residue from the first; natively each `Bike` accumulates independently
(`tests/bike_test.cpp:74-85` pins the isolation — that test encodes the
*divergence*, not original behavior, and must be revisited at port time).

## NOT changed

No tracked file touched — including the sibling's eight live render/road files,
which were neither read nor edited: `src/course_scene.cpp`,
`src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
`tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`, `tests/road_test.cpp`.
Also untouched: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`,
`docs/VERIFICATION.md`, all plans, `src/race.cpp`, `traffic.*`, `terrain.*`,
`race_setup.*`, proxy/oracle, `CMakeLists.txt`, build dirs, `../cdrom`.
Writes are exactly: this handoff + `analysis/skidslide-20261016p/`
(2 REA `.md`, 2 `.log`, `notes.md`). No builds, no captures, no commits.

## Remaining (porter-side)

1. Port the `0x0040ad30` analog fork (`DAT_004b7e7c ∓ 0x20` when
   `DAT_00476c98≠0` and human) behind `demo_hold_`; prove on the
   `sched-human-20261016` golden with per-tick `s`/`+0x288` during the
   hard-steer windows (words 300–359, 480–539, 840–899, 1020–1079).
2. Port the skid-gate `demo_hold` fork **and** collapse `skid_ticks` to a single
   global (or document per-bike as accepted divergence); revisit
   `tests/bike_test.cpp:74-85`, which currently pins the divergent shape.
3. `PhysicsTables::synthetic().curve` fill-256 vs original 238..273: test-only;
   no action unless a test asserts off-road curve values.
