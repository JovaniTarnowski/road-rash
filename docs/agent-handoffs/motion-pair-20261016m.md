# motion-pair-20261016m — move_along vs vertical ordering + crash-actor 0x1ff clamp

## Task / baseline

- Read-only background at HEAD `f41cc62`: motion-pair ordering (`0x004395e0` move_along vs vertical) + crash-actor motion `0x00440990` (held within `0x1ff` of spawn) on `0x00437e60` cadence.
- Baseline commit: `f41cc62d6f92815298b8e1e954818ada9894b354` (verified via `git rev-parse HEAD`).
- No builds, no captures, no commits. REA spot-check only (1 of max 2 calls spent).

## Address-label correction (load-bearing)

- The task string "vertical `0x00438c70`" is mislabelled. `0x00438c70` is **traction** (surface-table grip `0x0046dce8`, skid/damping), confirmed by `analysis/decompiled/00438c70.c` head + `docs/REVERSE_ENGINEERING.md:153` + `src/bike.hpp:66` / `src/bike.cpp:322`.
- **Vertical is `0x0043a190`**, confirmed by `analysis/decompiled/0043adc0.c:13-15` (called in the `0x0043adc0` tail, skipped when `+0xc4 == 0x10`) and `Race::vertical` (`src/race.cpp:1079` comments `0x0043a190`).
- Everything below uses the corrected labels.

## Order law (evidence-backed)

Rider tick (`Race::step`, `src/race.cpp:1871-1878`):

```
traction (0x00438c70) → integrate_speed (0x004390c0) → move (0x004395e0)
  → walls (0x0043a610) → vertical (0x0043a190)
```

- `Race::move` (`src/race.cpp:1030-1062`) = `0x004395e0`: trapezoidal `distance = ((speed*2 - accel*dt)*dt)>>1`, `move_along(div256(distance))` **before** the bend push (`bend*400/600/800/dt` into `+0x10c`) and `lateral += dt·+0x10c` (only when speed > 300). `0x004395e0:103` calls `0x00439270(...,1)` internally.
- `Race::move_along` (`src/race.cpp:339`) = `0x00439270`: sample-stepped bend/rise sweep; `sweep=false` = position-only (collisions/separation).
- `Race::vertical` (`src/race.cpp:1079`) = `0x0043a190`: runs **after** move+walls, resolves ground/height from the post-move sample.

Crash-actor motion `0x0043adc0` (`analysis/decompiled/0043adc0.c`, 18 lines):

```
0x004390c0 (speed) → 0x00439920 (independent move) → 0x0043a610 (walls)
  → [if +0xc4 != 0x10] 0x0043a190 (vertical)
```

- Native mirror: `advance_actor_motion` (`src/crash.cpp:81-101`) = `0x00439920`-class step (impulse fold-in, heading→`+0x108/+0x10c`, `advance_route(mul(term,dt)>>9)`, `+0x28 += dt·+0x10c`); `Race::step_crash` motion lambda (`src/race.cpp:1167-1174`) runs `integrate_speed → advance_actor_motion → walls → vertical`, same order.
- Pedestrian `0x00440990` = `0x0043adc0` + clamp (see below). `Race::move_pedestrian` (`src/race_pedestrians.cpp:328-341`) mirrors it: `integrate_speed → advance_actor_motion(move_along) → vertical → 0x1ff clamp (move_along + speed=0)`.

Cadence `0x00437e60` (`analysis/decompiled/00437e60.c`, 174 lines):

```
dt = tick - last (+0x18) → think (+0xe0) → move (+0xe4) → window/park logic
  → next = interval (+0x14) + tick → lateral park at ±0xc8000
```

- Pedestrian instance: `Race::step_pedestrians` (`src/race_pedestrians.cpp:130-152`) — `dt`, `think_pedestrian` (`0x00440dd0`), `move_pedestrian` (`0x00440990`), ring-window `[back, front)` + lateral park, interval `1 / 1-d / (d+1)/2`.

## 0x1ff clamp (REA-confirmed)

- Project export `analysis/decompiled/00440990.c` (20 lines): `FUN_0043adc0(p)` then `gap = *(+0x1c8 anchor) - *(+0x1c pos)`; if `|gap| > 0x1ff`, `FUN_00439270(p, gap ∓ 0x1ff, 1)` and `*(+0xec) = 0`. Anchor `+0x1c8` = spawn position (`src/pedestrians.hpp:31`, `docs/REVERSE_ENGINEERING.md:859`).
- New pinned REA (1 call, `--format md`): `function ../cdrom/ROADRASH/ROADRASH.EXE 0x00440990` → `analysis/motion-20261016m/rea-440990-pedmove.md`, exit 0.
  - evidence_id: `ev_81428552c2a61ec7bf9901c76ee9c0830b12ef4e5506a6e3058abeec062cada5`
  - subject sha256 `73d7f974…f18a2` matches reference EXE; provider Ghidra 12.1.4.
  - `raw_result.value.pseudocode` (426 chars) whitespace-identical to `analysis/decompiled/00440990.c` (426 chars): **TRUE**.
  - Assembly in same md: `CMP EAX,0x200` (strict), `LEA EDX,[ECX+0x1ff]/[ECX-0x1ff]`, `CALL 0x00439270`, `MOV [+0xec],0`. Callees `0x43adc0`@`0x440993`, `0x439270`@`0x4409c5`. No RNG.
- Prior coverage (not re-spent): `0x004395e0` (4255 chars), `0x00438c70` (5976), `0x00437e60` (4565) per Oct-12 REA notes in `docs/REVERSE_ENGINEERING.md:156,195`; local `analysis/oracle/rea-43{95e0,8c70,7e60}-*.md` present. `0x00440990` prior tiny confirm (384 chars, `docs/agent-handoffs/pedchain-rea-20261012.md`) now superseded by this full-md evidence.

## Native files read (not edited)

- `src/race.cpp:339-368` (`move_along`), `:1030-1062` (`move`), `:1079-1147` (`vertical`), `:1165-1197` (`step_crash`), `:1871-1878` (tick order); `src/race_pedestrians.cpp:130-152,328-341`; `src/crash.cpp:81-101`; `src/pedestrians.hpp:13,31`; `src/bike.hpp:66`; `analysis/decompiled/{00440990,0043adc0,004395e0,00437e60}.c` + `00438c70.c` head; `docs/REVERSE_ENGINEERING.md` (grep), `docs/agent-handoffs/{motionpair-verify,pedchain-rea}-20261012.md`, `analysis/police-spawn-notes.md`.

## NOT-changed list (sibling in-flight + tracked)

- Did NOT touch or read the eight sibling-modified render/road files:
  `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`, `tests/road_test.cpp`.
- Did NOT edit any tracked file. `git status` before/after shows only those same eight `M` entries (theirs, preserved).
- Wrote ONLY: `docs/agent-handoffs/motion-pair-20261016m.md` (this file) + scratch `analysis/motion-20261016m/{rea-440990-pedmove.md,.log,notes.md}` (untracked).

## Checks / remaining

- Checks run: `git rev-parse HEAD`, `git status --short`, `sha256sum` EXE match, targeted `rg` reads, 1 pinned REA call (exit 0), whitespace comparison TRUE. No builds/tests/captures per scope.
- Remaining for integrator/owner: none from this slice — order law + clamp are confirmed against current native code. Open pixel/capture work (e.g. `city-pedestrians` goldens) belongs to plan-24 owners, not this note.
- REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay/pixel claim made from it.

## Live processes / resources

- No live REA/Ghidra/Wine processes held; REA call completed (exit 0). No build dirs, locks, or oracle/prefix state touched.
