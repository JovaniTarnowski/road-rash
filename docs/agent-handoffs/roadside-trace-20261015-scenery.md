# Roadside-object collision trace (plan 16 Task 2 Step 1)

- Task/plan: plan 16 Task 2 Step 1 (trace ONLY), time-boxed one session.
- Baseline commit: `b00791c` (tree also has pre-existing unrelated modifications:
  `M oracle/aweman_proxy.c`, `M oracle/build.sh`, untracked `analysis/cpp20-*/`,
  `analysis/review-27-1/`, `tests/dash_bench.cpp` — untouched).
- Owned/changed files: only this handoff (read-only trace; no source edits).
- Method: existing `analysis/decompiled/*.c` first, disassembly cross-checks
  against `analysis/original-disassembly.txt`. REA **not** invoked: pseudocode
  not suspect — `0x004049c0` (9952 chars), `0x00403d60` (4716),
  `0x0043a610` (9756), `0x004368f0` (1769), `0x004373f0` (3196),
  `0x004520d0` (2219) already whitespace-confirmed against pinned
  `rea-agents@6.4.0` per `docs/REVERSE_ENGINEERING.md` + `docs/TOOLING.md`
  §REA (toolchain: `npx rea-agents@6.4.0`, Node 22.19+/24.11+/26+).
  No oracle captures run (SIM/pixel pipeline owner-debugging; shared prefix).

## Verdict: SOME objects are solid — Task 2 is applicable

The original crashes riders on roadside objects through **three** channels.
Drawn MSRR/CESR billboards (signs/trees/buildings outside the road) are
never tested; only actor-ized objects and the wall test crash:

1. **BORR in-road slots** → real scheduler actors (pool `0x00476ce8`,
   20×0xec, template `0x00476cb8`, `+0xc4` = 1). Instantiated **only** when
   strictly inside the road edges (`analysis/decompiled/00454240.c:129-141`:
   `if (iVar6 < slot && slot < iVar8)` → `0x00438930(DAT_00476ce8,…)`).
   Outside-edge slots are draw-only. → `0x004049c0` class-1 → reason 1.
   (Already ported natively as `hazards_`; see §5.)
2. **DZHR non-actor records with low-nibble type 0/1/2** (max 2 per slice) →
   static type-1 actor → reason-1 crash. Types ≥ 3 are marking-only
   (no crash). See solid-flag predicate below.
3. **Tall wall/facade slices** (`slice byte +0x38 == 5`) → `0x0043a610`
   reason-3 crash (bike, live, rider-height-relative gate); fast scrapes →
   reason 6. Course identity of `+0x38 == 5` left open (writer unrecovered
   in time box; §6).

## 1. `0x004049c0` dispatch — all type branches (verified vs assembly)

Function is the bike's `+0x1a4` contact handler (`param_1` = self bike,
`iVar4 = *(param_1+0x1a0)` = hit object). Assembly offsets confirmed:
`0xc4(%esi)` vs 2/1/0x20 (`404a65/404a6b`, `404b33/404b39`,
`404cdd` byte/`404cf8`, `404cfe`), other `+0x17c` vs `0x2000`
(`404c2d/404c33/404c39`), `+0x19c&0x40` (`404c4b`), other `+0x17c` vs
`0x2800` (`404c55`), other `+0xc4&0x10` (`404c62`), caller/other
`+0xc4&0xc` (`404d8b/404d92`), gap `> 0x1770` ⟺ `≥ 0x1771`
(`404edf` matches decompile `0x1771`). All widths byte-exact
(`movb/cmpb` for `+0x19c`, `+0x38`, `+0x3c`).

| Branch (decompile lines) | Predicate | Effect |
| --- | --- | --- |
| self crashed (`35` else → `219`) | `self+0x2f4 != 0` | `+0xf4 += 0x150`, tail only. No crash. |
| car-marking (`36` false → `190-217`) | `other+0xc4 == 2` | Kind-lateral `+0x298` = 0x3c/0x78/0xb4 by `other+0x124[2]` (`404aa2/ae/ba` ✓); sets other's `+0x1e8..1f0` if `local_18[3] > 1000`; zeroes self `+0xf0`. **No crash.** (So static type-2 actors from DZHR ≥3 never crash.) |
| ped/slow (`166-188`, via class 0) | class 0 (see ladder) | speed `< 300/500/2000` gates → `0x00439270` road-advance, or `iVar9 = 6` lateral nudge (`LAB_00404f4d`), or steer kick. **No `0x00403d60`.** `other+0xc4 == 0x20` tested at `404cfe`/`404d09` (0x20, then `+0x1e8 != 2`). |
| class 1/2 (`95-124`) | ladder picks 1 or 2 | `knock_off(reason 1/2)`; no-knock + speed gap `≥ 0x1771` → `0x00403d60` reason 1/2 with handler `local_18` (`404eeb-404f12` immediates `0x200/0x100` ✓); no-knock + small gap → speed transfer; knock-off → return (crash already entered inside `knock_off`). |
| class 3 (`125-163`) | ladder picks 3 | `knock_off(reason 3)`; survival → lateral shove + quarter speed cut. No direct `0x00403d60`. |

Class ladder (`77-88`, immediates `404c39/404c55` ✓):
`other+0x17c < 0x2000` → 0; else `self+0x19c&0x40 == 0` → 3;
else `other+0x17c < 0x2800 && other+0xc4&0x10 == 0` → 1; else → 2.
Short-object halve (`38-41`): `other+0xc4 == 1` → `other+0x17c /= 2`.
Stamina/bike split (`97-102`): `(other+0xc4 & 0xc) != 0` (bikes = 4) →
`knock_off(0, mag>>4)`; else (type-1 hazards/statics) →
`knock_off(mag, mag)`. Native `contact_hazard` (`src/race.cpp:681`)
passes `(magnitude, magnitude, 1)` — **confirmed correct** for type 1.

## 2. Every caller of `0x00403d60` × reason table (7 sites, exhaustive)

Disassembly has exactly 7 `calll 0x403d60` sites; each maps 1:1 to a
decompiled call line (verified). Prior-work mapping
(`src/race.cpp:882-910`, "reason-2 car path, local_18 vectors") is
**confirmed** with one clarification: the `local_18` pair is applied
**rider-impulse first (`param_2`), bike-impulse second (`param_3`)** —
`param_2` is added to the crash actor (`iVar2` = `+0x2fc` link),
`param_3` to self (`00403d60.c:98-107`).

| Site | Caller (decompile line) | Reason | Vector pair (rider → crash actor / bike → self) | What was hit |
| --- | --- | --- | --- | --- |
| `404f13` | `004049c0.c:121` | `iVar9` ∈ {1,2} | rider `{0,0x200,0}`, bike `{0,0,0x100}` | bike-bike / bike-car **gap crash**: knock-off survived but closing-speed gap ≥ 0x1771 |
| `4053b2` | `004052c0.c:49` | `param_4` passthrough (0–6) | reason 4: rider `{-80,0,0}`, bike `{0,0,0}`; else rider `{-256,0,1792}`, bike `{0,0,1024}` | knock-off decision: any contact class (incl. hazard class 1 → reason 1 defaults), jump slam (reason 2, `0x2000000` scaled ×7/4) |
| `434825` | `00434660.c:79` | 6 | NULL / NULL | **player stunt dismount**: input bit `0x100`, live, speed `< 1000` (sets `+0x2f4 = 2`, lateral −9000). Not scenery. |
| `438e34` | `00438c70.c:79` | 6 | rider `{-0x600,0,0}`, bike `{0x400,0,0}` | **player wobble fall**: `DAT_0046dce0 > 0x19`, player only. Not scenery. |
| `43a50b` | `0043a190.c:64` | 6 | rider `{0,0,0}`, bike `{0,-0x50,0x50}`-layout (`local_18`/`+3`) | **jump-landing slam** after `knock_off(reason 2)` on slice type 1. Not scenery. |
| `43ac62` | `0043a610.c:241` | **3** | NULL / NULL (`6a 03 / 6a 00` pushes ✓) | **tall wall/facade**: slice `+0x38 == 5` (`movb 0x38(%esi); cmpb $0x5` at `43a64f/43a656` ✓), `+0xc4 == 4` bike (`43ac4f-43ac58` ✓), live. See §4. |
| `43ad96` | `0043a610.c:285` | 6 | rider `{0,0x10,0x50}`, bike `{0,0,0}` | **fast wall scrape**: `(bVar1 \|\| (|+0xf0| > 0x9c3 && uVar9 != 3)) && +0xc4 == 4`. |

## 3. `0x0043a610` wall test — scenery/building records or edges only?

Both, at different gates. Inputs are the **ring slice** (`iVar4 =
*(actor+0x2c)`; edges `+0xe0/+0xe4`, bytes `+0x38/+0x3b/+0x3c/+0x37`,
wall arrays `+0x14c/+0x158/+0x150/+0x15c/+0x160` written by the
`0x00451710` wall iterator for `+0x3c == 1` slices):

- `+0x38 == 5` branch (`25-108`): interpolated facade edges
  (`+0xe8/+0xec` with `+0x3a` inset, `+0x3b`-bit side select). Tall-gate
  (`89-107`): `+0x160 == 0` and `wallH(+0x3b: 0x40 if bits clear else
  `(bits & 0xfc) << 8) < self+0x17c + self+0x20` and bike-live →
  `uVar9 = 3` → **reason-3 crash** (`240-241`). Crash halves (`+0xc4 == 8`)
  force `uVar9 = 0` (`103-105`); cars (`0x10`) skip the branch
  entirely (`26-27`) — clamp only, never crash (both crash calls gated
  `+0xc4 == 4`: `43ac55`, disasm `275`-region).
- `+0x3c` switch (`112-221`, `movb 0x3c; cmpl $0x5; ja` at
  `43a8e1-43a8e9` ✓, cases 0/1/3/5): shoulder/terrain edges and the
  `+0x14c/+0x158` wall arrays. Only lateral clamp + `+0xf0 *= -100/256`;
  crash solely via the shared fast-scrape tail (reason 6).
- So: **building facades alongside the road crash as walls (reason 3)
  when the `+0x38 == 5` slice marks them and the height gate passes**;
  free-standing MSRR/CESR billboards do not participate.

## 4. Solid-flag predicates (exact)

- **BORR**: per-record flag word unpacked by `0x00451a90`
  (`00451a90.c:10-49`, reused by the native `road_hazard_spans`
  period/duration/spacing math — matches `src/race.cpp:605-621`).
  Solidity ≠ a flag bit: **positional** — slot lateral strictly inside
  the road edges at sample fraction `0x80` (`00454240.c:129-141`).
  Actor class: type 1 (`+0xc4 = 1`), short (`+0x17c` halved) →
  `0x004049c0` class 1 → `knock_off(mag, mag, 1)` → `0x00403d60`
  reason 1 with `{-256,0,1792}/{0,0,1024}`. Tick path: scheduler
  `0x004368f0` pair loop (`004368f0.c:27-53`) → `0x00436a40` separation
  → bike `+0x1a4 = 0x004049c0`.
- **DZHR non-actor records**: `0x004520d0` (`004520d0.c:21-52`) packs
  records with `word0 & 0x80000000 == 0` (bit 31 clear) and
  `(word0 >> 8) & 0x3f != 0x3f` into slice `+0x3d/+0x43` (max 2;
  a third **truncates the list** `+0x3d = 2`, line 51).
  `record[2] (puVar1[2]) = word0 & 0xf`. Sweep `0x004373f0` second pass
  (`004373f0.c:69-98`, rider lateral box `±0x174` + vertical overlap)
  → `0x00437750` (`00437750.c:9-30`):
  - `type & 0xf ∈ {0,1,2}` → `0x00437660` static actor with
    `+0xc4 = 1` (`_DAT_004c3f54 = 1`, line 26; height adjust
    `−0x2000`/`−0x3fff` for types 1/2) → `0x00436a40` → rider
    `0x004049c0` class-1 → **reason-1 crash path** (same as BORR).
    Bikes only in effect (`+0xc4&0x10 == 0` gate at `00437750.c:9`;
    cars `0x10` skip unless `slice+2 == 0`).
  - `type ≥ 3` → static `+0xc4 = 2` + direct `+0x1a4` call →
    `0x004049c0` **== 2 marking branch, no crash**.
- **MSRR/CESR**: zero collision reads. Iterators `0x004522a0`/`0x00452610`/
  `0x00452710` are called only from section-enter `0x00452cb0:18,20`,
  slice-build `0x00454240:57` (flag derive) and draw-side helpers
  (`004522d0`, `00452580`, `00452660`, `00452690`); never from
  `004368f0/004373f0/00437660/00437750/00436a40/004049c0/0043a610`.
  The collision sweep reads only slice `+0x60/+0x61/+0x68` (BORR slots,
  written by `0x00451cb0`) and `+0x3d/+0x43` (DZHR, written by
  `0x004520d0`). Native `tests/scenery_test.cpp` (46 lines) covers
  placement/projection only — no solid-flag fixtures exist yet.

## 5. Minimal native edit sketch (for the port owner; no code changed here)

- BORR channel: already ported (`road_hazard_spans` + `collide_hazards`
  + `contact_hazard`, `src/race.cpp:600-698`, called at `race.cpp:2203`).
  Known fidelity gap (already recorded at `REVERSE_ENGINEERING.md:267`):
  native tests sample-granularity spans; original `0x004373f0` sweeps at
  `0x80` sample-fraction precision with vertical overlap.
- DZHR channel: **not ported**. Course load: decode DZHR non-actor records
  per sample (`bit31 clear`, `type = word0 & 0xf`, side bit `0x80`,
  lateral `slice+0xcc + sign12(word1)`, keep ≤ 2 per slice-sample) into a
  per-route-sample span table beside `hazards_` (`src/race.hpp:421`).
  Tick: extend the `race.cpp:2203` hazard pass (same point: per-rider,
  after BORR hazards, before/after pair loop per `0x004368f0:25`
  sweep-before-pairs order) with the `0x00437750` rule —
  `type < 3` → existing class-1 `contact_hazard` path (reason 1);
  `type ≥ 3` → `0x004049c0` == 2 marking branch (native Rider has no
  `+0x1e8/+0x298` fields; likely record-only/skip — owner decision).
  No allocation in tick (prebuilt spans), no RNG (DZHR packing consumes
  one `rand()` per *actor* record only at slice build; non-actor packing
  is RNG-free — `004520d0.c:25-52`).
- Wall reason-3: owned by plan 11 Task 4 (`0x0043a610` port). Predicate for
  that owner: `slice+0x38 == 5`, wallH `< +0x17c + +0x20`, `+0x160 == 0`,
  `+0xc4 == 4`, `+0x2f4 == 0` → `crash_reason(r, 3, {0,0,0}, {0,0,0})`
  (NULL/NULL vectors). Native stand-in `Race::walls` (`src/race.cpp`
  ~1037-1050) is clamp-only — read-only map, untouched.

## 6. Inconclusive within time box (do not guess)

1. **Course identity of slice `+0x38 == 5`** (which DLSR/wall records
   produce it; i.e. which visible walls/facades on Napa are reason-3
   solid). No byte-`+0x38` slice writer found in the decompiled corpus
   this session. Settles with: `napa-crash-scenery` steering into the
   first tall roadside wall (controls from `build/rr-course` geometry),
   checking for reason-3 (`+0x2f8 == 3`) vs reason-6 vs clamp.
2. **DZHR non-actor type names** (what course objects types 0/1/2 vs ≥3
   are — posts? signs? mile markers?). Needs `NAPA.CRS` DZHR record dump
   joined against placed positions; same `napa-crash-scenery` capture
   (aim at a DZHR type-0/1/2 lateral from the dump) settles crash-vs-mark.
3. `0x004049c0` `+0x19c & 0x40` ("other width contains ours") provenance
   for static actors (set in `0x00436a40` forward-overlap prologue per
   RE:894-896; class 2 vs 3 routing for hazards depends on it) — replay
   detail for the port owner, covered by the `napa-crash-scenery` golden.
