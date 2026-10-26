# Napa solo braking/steering script (plan 10 Task 5)

Baseline: `b619cb4` (bike +0x324 type; L1/schema2/L5/L5d/L5e/human green,
gate 106/106).

## Plan

Plan 10 Task 5 solo scenarios (SIM_SCENARIOS.md "Planned"): `napa-solo-script`
(full throttle 300, hard left 60, brake 120, right 60, repeated to the first
split at sample 700) to isolate player physics (throttle/brake/steer/gears,
curve drift, jumps) with opponents/traffic off (`RR_ORACLE_SOLO=1`).

## Control design (new, git-ignored)

- `analysis/oracle/sim-napa-solo-script.controls`: 1620 hex words, 3 cycles of
  `[1*300, 5*60, 2*120, 201*60]` (`1` throttle, `5` throttle+left, `2` brake,
  `201` throttle+right; gears auto). Mirrors the human-script style
  (`sim-napa-solo-human.controls` uses `1/5/2/201/0`).
- Seeded as `prefix/drive_c/napa-script-20261026.controls` (unique, no clobber).

## Capture (running, background)

```sh
RR_ORACLE_COURSE=4 RR_ORACLE_SEED=1 RR_ORACLE_SIM=1 RR_ORACLE_SOLO=1 \
RR_ORACLE_INPUT='C:\napa-script-20261026.controls' RR_ORACLE_FRAMES=2000 \
oracle/run.sh sim-napa-solo-script
```

- Session/output unique (`sim-napa-solo-script`, not overwriting accepted
  `sim-napa-solo.tsv`/`-human.tsv`). `oracle_capture.py` holds
  prefix/install/output.parent claims for the session; claims were empty
  and no Wine/ROADRASH processes were running at launch (`ps` clean,
  `make doctor` reference hash matches).
- Expected validation (per AGENTS): valid header, contiguous ticks,
  completion marker (0xb4 rule quits ~180 ticks after input exhaustion:
  ~59 frozen + 1620 script + 181 coast ≈ 1860 frames, FRAMES=2000 covers
  it), original EXE hash, exit 0. Do NOT overwrite accepted captures on
  partial runs; receipt in `.captures/`.
- REA not needed for capture (existing proxy, no `aweman_proxy.c` changes;
  installed proxy unchanged from L5d/L5e receipts).

## Next (after capture validates)

1. ~~`python3 tools/golden_diff.py ...` + determinism replay~~ DONE (Oct 26):
   `validate --simulation --frames 1801` passes (schema, exe hash,
   1801 frames, ticks 1-1801, seed 1, `control_branch human`,
   `complete true`). First attempt without `HUMAN=1` gave
   `control_branch demo` (INPUT ignored) -- correctly rejected for
   player physics; re-captured with `HUMAN=1`.
2. ~~`oracle-sim-test ...` first divergence~~ DONE (Oct 26, owned
   `build-agent-crash-2278`): `1801 ticks identical` (+5 golden-only,
   0 uncovered), auto human driver and explicit `--driver=human` agree.
   No divergence, no porting needed.
3. Docs in same commit: SIM_SCENARIOS (scenario + capture command),
   VERIFICATION (commands/outcomes), PROGRESS (timeline). No CTest
   registration (golden-only suffix, like L5d/L5e); no golden change
   (new file, git-ignored).

## Remaining (explicitly not in this slice)

- `napa-solo-split-left/right`, `napa-solo-jump`, `napa-fight`, `napa-contact`,
  `napa-traffic-solo/field` (same Task 5 procedure, separate captures).
- Full `0x00407410` walk, `+0x328` mapping, class-2-contained crashed speed
  path (flagged in `l5d-crash-car-20261026.md`).
