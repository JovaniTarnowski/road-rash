# RRAN bind + anim advance + shadow dispatch background — Plan 11 Task 3 (20261016l)

- Task: read-only background for Plan 11 Task 3 (rider anim/shadows not ported).
  Covers RRAN bind `0x00445a60` + frame advance `0x0043ae90` + shadow rendering.
  No implementation, no captures, no commits.
- Baseline: HEAD `4a9b29b` (verified `git rev-parse`). Shared tree carries another
  worker's in-flight edits; all preserved, none touched.
- EXE: `../cdrom/ROADRASH/ROADRASH.EXE`, sha256
  `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
  (verified via `shasum -a 256`).
- REA budget: **2/2 calls** (the allowed max), pinned `rea-agents@6.4.0`,
  Ghidra 12.1.4 libexec, JDK 21, `--format md`, env per `docs/TOOLING.md`.
  `0x0043ae90` reused from the existing pinned dossier (no new call).
- LSP/builds/captures: none (spot-check only, per `docs/TOOLING.md` REA is not
  runtime proof).

## 1. Label correction (read the table, not the brief)

The brief's "advance `0x00445a60` + frame pick `0x0043ae90`" is inverted.
Decompile + fresh REA show:

- `0x00445a60` = **bind/init**: fetch CANS 1–2 / ANIM 1–4, relocate, then 59×
  `0x0040b6a0` runtime-descriptor builds. No per-tick stepping.
- `0x0043ae90` = **per-tick advance + frame pick**: table lookup, `+0x0c`
  pre-advance frame retained for drawing, deadline/loop/queued transitions.

## 2. Addresses + evidence_ids

| Address | Role | evidence_id | Output | Match vs `analysis/decompiled/` |
|---|---|---|---|---|
| `0x00445a60` | RRAN/CANS bind, 59 bindings | `ev_53a324d24c03cce2aedbd98a74c3b54631ed0d4a57269f2fa187cd6f1d9118ce` | `analysis/anim-20261016l/rea-445a60-bind.md` (318,810 B, this session) | TRUE (4,922/4,922 norm chars) |
| `0x0043ae90` | anim advance (1 transition/call max) | `ev_e4a797a234749b0f8ddc75f8da8f333455f93c3e11bc0147fbcc53bb1167320f` | `analysis/oracle/rea-43ae90-animadv.md` (prior, reused) | TRUE (1,355 REA chars per `animadv-verify-20261012.md`) |
| `0x00446250` | bike+shadow dispatch (shadow first) | `ev_97158595018f03c482da9de984a717c136a0b4c90632fac2821b76d02ca6341a` | `analysis/anim-20261016l/rea-446250-shadow.md` (110,138 B, this session) | TRUE (967/967 norm chars) |
| `0x0043ae10` | anim set (code → slot) | `ev_abeb85b9e993c02d0f7c10d5605a7335577e8fca5b84af75c9d94d3836691e41` | `analysis/oracle/rea-43ae10-animset.md` (prior, reused) | TRUE (535 chars per `animadv-verify-20261012.md`) |

Cited without new calls (call budget): `0x004463e0` armed dispatch (adds 3rd
weapon-overlay `0x0044ba40` via `0x0040b750`), `0x0043af60` view select,
`0x00446350` shadow colour (`0xdf` iff body cop `0xde`, else `0xe0`),
`0x004463c0` body setup, `0x0044ba40` billboard (prior dossier
`rea-44ba40-billboard.md`), `0x0044d300` (no shadow darkening: same textured
path, index-0 transparency), `0x0040b6a0` descriptor build, `0x004153a0`
resource fetch, `0x0044a200`/`0x0044a1d0` RRAN/OFSS relocation.
Callers of `0x00446250` per REA: `0x00463ae4`, `0x00463bb4`, `0x0046431c`.
Caller of `0x00445a60` per REA: `0x004417a8`.

## 3. Laws (porter input)

**Advance (`0x0043ae90`):** resolve code byte `+0x10` (`0xff` masked),
slot `*(+0x08) + code*4`, frame record `slot + (+0x11)*0x14` (20-byte records:
duration signed byte `+0xc`, control `+0xd` = low-nibble action / high-nibble
loop frame). Retain record pointer at `+0x0c` (native `selected`, drawn this
tick). If `+0x14 == -1`, init `+0x14 = duration + DAT_004b7ecc` (tick base).
If `+0x14 <= tick`: action 0 → `frame++`; action 1 with `queued == -1` →
`frame = control>>4`, `loops++`; else `frame = 0`, `loops = 0`,
`anim = queued`, `queued = -1/0xff`. Then `deadline += next.duration`
(accumulate, never restart at tick). Native `advance_animation`
(`src/actor_animation.cpp:106`) ports this exactly, including the
`UINT32_MAX` first-deadline init and the `-1 → tick+3600` idle rule in
`set_animation`.

**Bind (`0x00445a60`):** for `i` in 0..1, if `DAT_004b9220[i*2]==0`: fetch
`CANS(i+1)`/`ANIM(i+1)`/`ANIM(i+3)` via `0x004153a0`; mirror CANS to
`0x004b94e0`; relocate each ANIM (`RRAN` → `0x0044a200`, `OFSS` →
`0x0044a1d0`). Then 59 `0x0040b6a0(base, src, dst, count)` calls — 48 on bank
0 (`0x004b9220`), 11 on bank 1 (`0x004b9228`) — with the exact
source/destination/count triples reproduced in native `bindings[]`
(`src/actor_descriptors.cpp`, "Address/count schema lifted from 0x00445a60").
Two destinations (`0x4b9df0`, `0x4b9280`) are never filled (BSS); native
stores `nullopt` and direct selection rejects them (`actor_descriptors.cpp:236`).

**Shadow (`0x00446250`):** stash `+0x90`; select bank (`0x4b94e4`, or
`0x4b94ec` when `DAT_004642dc` chaser matches `+0x2fc`); `0x0043af60` view
select; `+0xbc = 0`; if `+0xa8`: colour via `0x00446350`, emit
`0x0044ba40(..., clip 0, ground WITHOUT vertical)`; restore `+0x90`,
re-select, `+0xbc = 0`; if still `+0xa8` (apply `+0xac` override to
`*+0xa8+0xc`), `0x004463c0`, emit
`0x0044ba40(..., clip 1, ground + vertical>>8)`. Native `ActorNodes::rider`
(`src/actor_nodes.cpp:27-53`) matches the 2-node order, clip flags, and
forced-zero shadow roll, sampling owned HI Bob pages at RRAN logical dims.

**Hit-frame gating (combat timing):** `0x004096f0` fires only when descriptor
flags `& 0x600000` (via `rider+0x7c → +0x10 → +4`) **OR** frame byte
`+0x81 == 2`. Native enforces frame-2 only (`attack.fired` once per swing);
the flags word has no native carrier (`ActorGraphicsDescriptor` lacks it) —
open item per `analysis/plan16-combat-notes.md` and `ai-fight-20261016e.md`.
Attack-anim predicate `0x00409510` (weapon row `+0x1d4` vs anim code `+0x80`
in `0x004644f8/08/18/28`) is a `attack.timer != 0` stand-in natively.
18-tick cap / tick-7 hit are no-bank fallbacks; the banked path must be
driven by `ActorAnimationState` frame-2 events + descriptor completion.

## 4. Native gap table (what Task 3 must still port)

| Native today | Original | Delta |
|---|---|---|
| 59 `0x00445a60` bindings ported; 2 BSS slots `nullopt` | Same, + `0x600000` flags word on runtime descriptors | Trace which bank carries the flags word; add a native carrier |
| `advance_animation` / `advance_riding_animation` (codes 8/4/3/2, wreck 5, AI distance 2/0/1) | `0x0043ae90` + `0x00407f30` selection + `0x00407a90` think (REA gap #8, port from export) | Port crash/attack dispatch out of the riding branch; wire live triggers |
| `ActorNodes::rider` shadow+body, roll-zero shadow, HI sampling | `0x00446250` 2-node order (confirmed) | Keep; add `0x004463e0` weapon-overlay 3rd node via `0x0040b750` |
| Traffic `tick+phase` frame cycling (`traffic.cpp:641`, `traffic.hpp:240`) | `0x0043ae10`/`0x0043ae90` advancement | Replace approx with descriptor stepping |
| `select_actor_view` yaw/elevation (`0x0043af60` portion) | Same + orientation `0x00413ae0`, shadow remaps, depth dispatch | Port remaining `0x0043af60` tail + `0x00413ae0` |
| text/billboard "shadow" (2-px font offset) | Unrelated to rider shadows | No action; do not conflate |

## 5. Capture spec (none run — proposal for the Task 3 owner)

- `napa-anim-ride`: solo cruise at steady speed/throttle; assert native
  `advance_riding_animation` code/frame/deadline sequence matches
  `0x00407f30`+`0x0043ae90` over the existing 1,024-step corpus pattern.
- `napa-shadow-order`: start-grid frame 0 + drive frame; assert node order
  road → shadow (clip 0) → body (clip 1), tyre-below-seam geometry, and
  empty-shadow omission (5 in frame 750 precedent).
- `napa-weapon-overlay`: armed rider attack; assert 3rd `0x004463e0` node
  appears with `0x0040b750` frame and disappears after the swing.
- `napa-hit-frame`: `0x004096f0` swing; assert damage lands on frame byte 2
  (and record whether the `0x600000` flags word would also gate it).
- Register in `oracle-sim-test` + synthetic `race_test` bounds per
  `SIM_SCENARIOS.md` proxy conventions; coordinate prefix/DLL/output
  ownership per `AGENTS.md` before capturing (proxy is shared/outaged).

## 6. NOT-changed list

- No tracked file edited. In particular untouched + unread: `src/course_scene.cpp`,
  `src/race_caches.hpp`, `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp` (plus the also-modified
  `tests/course_scene_test.cpp`, likewise unread).
- Live-claimed paths avoided (neither read nor written): `src/race.cpp`,
  `src/race.hpp`, `src/traffic.*` (beyond 2 cited comment lines),
  `src/terrain.*`, `src/race_setup.*`, `oracle/aweman_proxy.c`,
  `CMakeLists.txt`, `tests/oracle_sim_test.cpp`, `tests/traffic_test.cpp`,
  `tests/race_setup_test.cpp`, all `??` sibling scratch dirs
  (incl. `bike-build-20261016l`, `car-think-20261016l`), shared `docs/`
  status files, `analysis/ghidra-project/`, `oracle/`, prefix/install.
- Written (new/untracked only): this handoff +
  `analysis/anim-20261016l/` (`rea-445a60-bind.md/.log`,
  `rea-446250-shadow.md/.log`, `notes.md`; disc-derived REA output,
  git-ignored pattern per prior sessions).
- No builds, no `ctest`, no captures, no commits, no locks held.

## 7. Next action

Hand to the Plan 11 Task 3 owner with this background + scratch dir:
port the flags-word carrier, the `0x004463e0` weapon overlay, live animation
triggers (`0x00407a90`/`0x00407f30`), and traffic descriptor stepping; then
capture §5 and accept against zero-divergence `oracle-sim`.

## Evidence paths

- Native: `src/actor_animation.cpp/.hpp`, `src/actor_descriptors.cpp:140+`
  (bindings), `src/actor_nodes.cpp:27-53`, `src/actor_render.cpp:20-46`,
  `src/rider_sprites.hpp/.cpp`, `src/traffic.hpp:240`, `src/traffic.cpp:641`.
- Original: `analysis/decompiled/00445a60.c`, `0043ae90.c`, `00446250.c`,
  `004463e0.c` + scratch REA outputs above + prior
  `analysis/oracle/rea-43ae90-animadv.md`, `rea-43ae10-animset.md`,
  `rea-44ba40-billboard.md`.
- Prior handoffs: `animadv-verify-20261012.md`, `crashanim-verify-20261012.md`,
  `actor-dispatch-20261016i.md`, `ai-fight-20261016e.md`,
  `wall-crash-20261016c.md`; `analysis/plan16-combat-notes.md:61-63`,
  `analysis/combat-anim-notes.md`.
