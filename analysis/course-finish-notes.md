# Course finish and full field (plan 17 remainder)

Research: `docs/research/post-11a-2026-10-12/simulation-and-courses.md`
("Course census and finish events", "Factory and field setup").
Baseline: prior CourseBounds slice (`e7dc0a6`: `split_nodes` /
`make_route_choices` / `finish_event_sample` in `src/course.*`).

## Finish crossing (both branches)

Chain: `0x004525c0` (first raw-type-6 CESR event with
`signed param[0] - level == 1`) → `0x00453e60` (section+0x2c) →
`0x00453f40` (graph-relative remaining distance) → `0x00454240`
(global `DAT_004b6e58 = section_base + finish*0x100`, runtime-subslice
+0x36 bit 0x20 when active event type is 7 and parameter matches
level+1) → `0x00406890` (actor +0x1c reaches the nonzero global, with
the +0x2c scheduler gate, when +0xc0 bit 0x10 is clear) →
`0x00437800` (latch +0xc0 bit 0x10 on rider and bike objects, record
finishing tick/order) → player coasts/brakes, `0x00416010` decides the
outcome. It does not select course coordinates.

Native change (`src/race.*`, finish regions only): `Race` carries
`finish_q8_` (the global, 0 = legacy route-end rule for synthetic
routes) with `set_finish_q8`/`finish_q8`. `cross_sample` checks the
`0x00406890` gate first — nonzero line, latch clear, `to >= line` —
clamps to the line, finishes with `finish_tick = ticks_`, and sets the
`0x00437800` latch. The legacy route-end fallback now sets the latch
too. Game/replay wiring: `finish_q8 = finish_event_sample(course,
route, level) << 8` immediately after construction (route choice
selects the branch; both branches use the same gate).

`0x0044b250` grid origin 0x3200 is independent of level; the
`0x00468f58` mileage table is menu text, not race bounds.

## 15-rider field

`0x00405600` sets `DAT_004642d4 = 0x10` offline and loops slots 1
through `<count-1`: player + 14 opponents, 15 total. Per-slot build is
`0x00405990` with rider rows `0x00463c78` (k = slot = param_6, 1..14),
bike bytes `0x004641c8[level + k*5]`, personalities `0x00464558`
(+0x1bc/+0x1a8 speeds, +0x18c lane init — `rand()%3` at build via
`0x00441750`), throttle scale `0x004642a0[level]`, grid tables
`0x00464088` (level < 4) / `0x00464108` (position = selector, lateral
added to road centre). Native change: `original_opponents` accepts
`count <= 14` and fills `grid_pos`/`grid_lat` from the level-correct
table for every slot; the `Race` ctor already places table-grid riders
and draws `rand()%3` lanes for lane −1 through `Race::random()` in
build order.

Roster/RNG notes (`0x004317a0`, `0x00431840`, `0x004078f0`): 10
personalities copied to `0x004c6b40`; the `local_80` roster maps slots
to rider indices with `-1` = random pick. Slots 8..14 reuse rows
8..14 (only 10 personality records exist) — same as the original's
`param_6` indexing. Network/owner bypass branches in `0x004050c0` /
`0x004051e0` and the `0x004642dc` chase-target logic are retained scope,
not flattened.

## Runtime golden status: BLOCKED (no capture run)

Needs original-runtime proof per research Remaining-work: finish
crossing on both branches + field identity/scheduler order. Entry
points: `RR_ORACLE_LEVEL`/`BIKE`/`START` selectors at race
construction, `DRIVE=human|demo` with the real `DAT_004753c8` branch,
pre-step RNG + exact tick/green boundary, `finish_event_sample << 8`
as the expected line, 14-opponent identity dump in build order.
Suggested commands (from `native-port/`, ~4–6 min each in background):

- `RR_ORACLE_LEVEL=0 RR_ORACLE_COURSE=NAPA ... oracle/run.sh` — short
  human bibliography to green + pre/post RNG (branch entry evidence).
- Full-race human captures on both Napa branch combos to the
  level-tagged line — finish tick/order/latch vs native.
- 15-rider identity capture (slot, rider row, bike byte, grid pos/lat,
  lane draw) — field-setup golden.

None run in this slice: the shared `build/` dir was lock-held by the
SimReplay slice and this assignment forbids verification runs (main
agent verifies under the lock protocol).
