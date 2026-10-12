# city-intersection capture log (plan 24 Task 1 Step 2)

Scenario: `city-intersection` — CITY (original course 1), level 1 (default
bike/character), seed 1, drive=2 attract-demo autopilot, full field
(`RR_ORACLE_SOLO` unset → opponents + traffic live).

Golden: `analysis/oracle/city-intersection.tsv` (70 MB, 2,822 frames, ticks
1–2821 contiguous per `golden_diff.py` self-diff "2822 frames identical",
`end` marker present) + `analysis/oracle/city-intersection.controls`
(8,466 control words).

## Commands

```sh
# 1. Rebuild install (a sibling's uncommitted oracle/aweman_proxy.c edit had
#    rebuilt the DLL mid-flight twice → "oracle DLL changed during capture").
#    NEVER edit oracle/ files; coordinate + rebuild via:
oracle/build.sh && oracle/install.sh
# 2. Capture to the race's natural end. RR_ORACLE_FRAMES is a hard frame
#    deadline, NOT a race-end detector: the game keeps simulating (menus/
#    results) after the finish, so any FRAMES value truncates or overshoots.
#    First run used FRAMES=3000 and the gate rejected it (2,822 real ticks).
#    Accepted run: no FRAMES → process killed at timeout, but the trace had
#    already written `end` at the race end (tick 2821) — the run is complete
#    because the trace, not the process exit, carries the end marker.
RR_ORACLE_COURSE=1 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_SIM=1 \
  RR_ORACLE_RECORD='C:\city-intersection.controls' \
  oracle/run.sh city-intersection 1500
# 3. Self-check + replay:
python3 tools/golden_diff.py analysis/oracle/city-intersection.tsv analysis/oracle/city-intersection.tsv
./build/oracle-sim-test --driver=demo analysis/oracle/city-intersection.tsv
```

Results: self-diff exit 0 ("2822 frames identical"). Replay DIVERGES as
required (native has no intersection/traffic-density port yet):
`first divergence at frame 5 (tick 6): rider.0.ai_target_x original 24832
native 8455 (via rider.0.brain_194)`, rng identical to that point
(742760408 = 742760408). Note: bare run without `--driver` aborts with
`golden has no player.demo_hold measurement: pass --driver=human|demo` —
this golden predates the demo_hold probe, so the driver axis must be forced
(drive=2 ⇒ demo is correct).

## What the golden covers (evidence for Steps 3–5)

- Full CITY field: 13 riders at green (`riders 13`), 12 at race end;
  40–61 non-rider scheduler nodes per tick (`node.<i>.position/.kind`).
- Two live `0x0041f040` intersection controllers at race start: header
  `nodeaddr.0.think = 4321344` (0x41f040, position 33024, kind 0) and
  `nodeaddr.18.think = 4321344` (position 22784, kind 0). Controllers are
  scheduler nodes but the per-tick `node.<i>.think` key reads +0xdc, NOT the
  +0xe0 think pointer (proxy line 391) — per-tick controller phase is NOT
  observable; only header-frame `nodeaddr.<i>.think` (+0xe0, correct) is.
- First type-1 CESR zone (raw type 0 → event_type 1) in section 0 at samples
  85–89; player (pos 12800 = 0x3200 ring-camera start, 256 units/sample)
  reaches sample 85 at tick ≈921. Later type-1 zones at 125–129, 165–169,
  201–205 (section 0) etc. — zone_end density timing is observable via
  rng_calls/positions across these ticks.
- `rng_calls` is 0 at all sampled ticks (10/100/500/921/1000/1500/2000/2500):
  with the demo autopilot on CITY, no `rand()` fires per tick outside setup —
  traffic spawns, if any, are not per-tick rand consumers here.

## GAPS vs the plan's capture spec (fault report)

The plan asks for "car fields (`car.<i>.*`) + ring cursors
`DAT_004b6e50/DAT_0049f240/DAT_004b6e5c` dumped every tick". NEITHER exists
in the oracle, and this capture does NOT add them (oracle/*.c is shared
with active sessions and I MUST NOT touch it):

1. `rr_sim_car_fields` (oracle/sim_fields.h) is DEAD — no proxy code
   references it; zero `car.*` keys in any golden. Traffic-pool cars are
   only visible, if at all, as opaque `node.<i>.position/.kind` rows.
2. No `ring.*`/cursor keys exist anywhere in the proxy. Ring offset (+0x5100
   check) and zone-density timing must be inferred from player position vs
   CESR sample maps (above), not measured directly.
3. Per-tick `node.<i>.think` reads the wrong offset (+0xdc instead of +0xe0),
   so controller/signal liveness per tick cannot be read from this golden —
   only existence at tick 1 (header `nodeaddr` block).

To get the spec'd capture, a follow-up must extend `dump_sim_tick` in
`oracle/aweman_proxy.c` (car pool `0x0049e654` 16×0x210 via
`rr_sim_car_fields`; ring cursors; fix `node think` offset to +0xe0) and
re-capture. This golden remains valid for rider/field replay and the
zone-crossing tick map (type-1 zone ends at samples 89/129/169/205…,
player ticks ≈921+).

## Open questions

- Why does the race end at tick 2821 with rider.0.position 90610 — is that
  the CITY finish line, and does the `end` marker coincide with it or with
  the results screen? (Timeout run reached 87k frames without process exit,
  so `end` ≠ process exit.)
- `rng_calls` = 0 everywhere sampled: does CITY traffic spawn without rand,
  or did this demo run see no spawns? A car-pool dump would answer.
- Frame-count gate: `validate_trace(..., expected_frames=FRAMES)` counts
  `frame` lines; the natural-end procedure (no FRAMES + timeout kill) leaves
  the accepted .tsv staged only via manual copy (used here). A cleaner path:
  `RR_ORACLE_FRAMES` slightly above the expected finish + accept the
  shortfall, or gate on `end` presence.

## city-intersection-v2 attempt (October 12, 2026): bootstrap fault

Same recipe as v1 plus the rebuilt proxy (car `ring.*` + cursor keys):
`RR_ORACLE_COURSE=1 RR_ORACLE_SEED=1 RR_ORACLE_DRIVE=2 RR_ORACLE_SIM=1`,
1500 s timeout. Result: Wine exit 124 (timeout), capture rejected —
same signature as the `city-cops` attempt (menus cycle, races enter/exit,
no green light, 0 sim dumps). The v1 recipe (07:17) and the sibling's
Napa v6 (07:52) both succeeded, so the pipeline works; the CITY menu
bootstrap is flaky (prefix state or menu timing). Step 5 replay validation
waits on a successful recapture; no tree changes for this attempt.

## Environment note (October 12, 2026): orphaned Wine processes

After the failed captures, stale Wine processes linger (wineserver +
winedevices ~30 min old, a winedbg, and a 2-min-old ROADRASH.EXE with no
registered claim found). A stuck wineserver can break subsequent runs'
menu timing, and a system-wide fd spike ("Too many open files") killed
one capture instantly. Before the next Step-5 attempt: verify no live
session owns the prefix processes, kill orphans, and re-check fd counts.
Left untouched this turn for lack of ownership proof.

## Pipeline outage (October 12, 2026): no scenario reaches green light

`napa-cars-smoke` (default course, drive 2, SIM=1, FRAMES=300, 600 s)
also faulted with Wine exit 124 / rejected — the same signature as the
two CITY attempts. A live game process has sat with a 133-byte trace
(no sim dumps) for 40+ minutes. Napa captures worked at 07:52; nothing
has succeeded since ~08:00. The oracle pipeline is unusable right now
(menu bootstrap or prefix/wineserver state). Step-5 and Task 2/3
captures wait on pipeline recovery, not on new proxy code (car/ring
dumps are committed and the DLL is installed).

## napa-cars-smoke2 (October 12, 2026): menus never enter a race

No `RR_ORACLE_COURSE` (default path that produced Napa v6 at 07:52).
600 s timeout, exit 124. Trace has 2 frames with course 0 / level 0 —
the game never entered a race. Menu-to-race bootstrap is down even on
the default path; not scenario-specific. Standing down on captures
until the bootstrap is diagnosed (hook_menu_enter / menu video path).
