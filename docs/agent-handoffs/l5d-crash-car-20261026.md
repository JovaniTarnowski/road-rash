# L5d tick-2278 crashed car contact (plan 10 Task 6)

Baseline: `f8a4c59` (crash-entry finish recompute; L5d pinned at 2278
lateral).

## Symptom (all in one tick, same crashed rider, same car)

`oracle-sim-field-l5d`, tick 2278 (frame 2277), native rider 9
(crashed at 2267, riderless bike):
- Car `13e2038`, kind 0, lane 0: pos 123254->123300 (+46),
  speed 10744->1934 (hard brake for the rider ahead), lateral 32000.
  Car motion matches in both worlds (follow correct).
- Rider bike: pos 123319/123319 (no longitudinal move),
  lateral 49976->62008 (+12032, away from the car),
  steer 0->-2, velocity -514->-184 (+330), speed 11094/11094.
  Native initially missed all three (stayed 49976/0/-514):
  lateral, steer and velocity diverged in sequence at the same tick
  as the fix progressed (lateral -> velocity -184 vs -178 (off 6) ->
  speed 11094 vs 10667 (off 427, bleed) -> steer 0 vs 855 (kick)).

## Root cause (measured, REA + disassembly, no guessing)

- `0x004368f0` (`analysis/decompiled/004368f0.c`) walks pairs and calls
  `0x00436a40` separation unconditionally: it gates only on
  `+0x10` (tick), `+0xc0` flags (`&6==6` outer, `&5==5` inner),
  geometry and callbacks -- never on `+0x2f4` (crash). Separation runs
  for crashed riders too. Native `collide_traffic` returned early for
  `+0x2f4 != 0` ("0x004049c0 rejects", inaccurate), skipping the
  riderless-bike shove entirely.
- `0x004049c0` handles crashed (vertical `+0xf4 += 0x150`, line 219;
  then nudge + steer tail, no `0x004052c0` re-knock). Native
  bike-bike `contact()` already does this
  (`if (+0x2f4 != 0) +0xf4 += 0x150`, then nudge/kick with down=false);
  native car `contact_car()` did not: no bump, called `knock_off`
  (which returns true for already-crashed via `+0x2f4 != 0`), so
  down=true suppressed the steer kick and added a spurious
  `+shit/256` bump (the 6 off), plus a `|184|/4` speed bleed (the 427
  off) and a `shit/2` steer-rate shove (the 855 vs -2 kick).
- `0x0043a580` (REA 6.4.0, Ghidra 12.1.4, Node v25.8.1, JAVA_HOME
  openjdk@21; disc hash verified): riderless-bike move is prep
  `0x00438c70`, integrate `0x004390c0`, gears `0x004391d0`,
  road move `0x004395e0`, then `0x0043a610` walls gated on
  `+0x324`/`+0x328` branch bytes vs the player, then `0x0043a190`.
  The walls gate was NOT the 2278 trigger here (routes 0/0 same,
  `RR_FIELD_TRACE` probe, reverted): the trigger is purely the
  skipped car separation for the crashed bike. The `+0x324` type-word
  gap (native Bike never sets `+0x324` 0/1/2; all read as player 0)
  stays open (plan-17/plan-11 Task 4); it did not fire at 2278.

## Fix (committed in this slice)

`src/race.cpp` `collide_traffic`: removed the crashed early-return;
separation + `contact_car` now run for crashed (mirroring `0x004368f0`).

`src/race.cpp` `contact_car`: crashed (`+0x2f4 != 0`) mirrors
bike-bike `contact()`'s crashed path --
- `+0xf4 += 0x150` bump at entry (the only `+0xf4` change);
- skip `knock_off` (down stays false; no re-knock, no stamina/bike
  damage, no `+shit/256` bump) in both class-3 and class-2-contained
  branches;
- skip the `|184|/4` speed bleed and the `shit/2` steer-rate shove
  (crashed path has neither; the tail kick below uses magnitude);
- tail kick baseline is `magnitude` (`full>>10`, i.e. local_20) vs
  `+0xf0` for crashed (e.g. 2 vs 0 gives -2), `old_steer_rate` vs
  `+0xf0` otherwise. Nudge (lateral push), impulse/flag clear,
  clamps and the uncrashed-only finish recompute are unchanged.

## Verification (owned build `build-agent-crash-2278`, Debug)

- `cmake -S . -B build-agent-crash-2278 -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`, build: exit 0.
- L5d: 2278 lateral/velocity/speed/steer sequence gone;
  `2400 ticks identical` (was pinned at 2278 lateral; pin retired).
- No regression (focused, same build): L1 `2400 identical`,
  schema2 `2400 identical`, L5 `2400 identical`,
  L5e `2400 identical (+43 golden-only)`,
  strict human `sched-human-cov2-20261017` `1431 identical`.
- Full `ctest --test-dir build-agent-crash-2278 -j8`: **106/106 passed**
  (L5d/L5e verified manually outside CTest: L5d `2400 identical`
  +13 golden-only, L5e `2400 identical` +43 golden-only; CTest
  unregistered since a golden-only suffix can neither pass nor pin).
  Receipt in `VERIFICATION.md`.
  LSP: same tooling limit as prior slices (`agent_lsp.py` no owned DB;
  `clangd --check` via `build/` shows only Apple ExtractFunction tweak
  FAILs, no parse/type diagnostics). `rg`/`fd` used; REA 6.4.0 with
  `GHIDRA_INSTALL_DIR`, `JAVA_HOME`, `/opt/homebrew/bin` Node.

## Remaining

- L5d pin retired and CTest unregistered (now `2400 identical` +13
  golden-only, like L5e; a golden-only suffix can neither pass nor pin,
  so verified manually). Full 2400 green in this slice, so L5d closes.
- `+0x324` type-word never set (all bikes read as player 0):
  `0x0043a580`/`0x004060b0`/`0x00406890`/`0x00407410` branch gates
  (`+0x324`/`+0x328` vs player via `DAT_004c4fb0`) still unported.
  Did not fire at 2278 (routes 0/0), but will matter on split
  branches (the `+23/-23/+298` offsets). Plan-17/plan-11 Task 4.
- Uncrashed `0x004049c0:267-268` trailing `0x00407410` not ported
  (deliberately open). Full `0x00407410` walk not ported.
- `contact_car` class-2-contained for crashed still takes the small-gap
  `+0x184` speed-transfer path when down=false (untested: 2278 was
  class 3 glance, not contained). Flagged for the next car-contact
  divergence, if any.
