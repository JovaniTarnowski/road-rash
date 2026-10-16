# damagesplits-20261016o — combat damage splits REA spot-check (READ-ONLY)

Task: read-only background — damage splits: hit `0x00409c10`, damage
`0x00409ad0`/`0x004052c0`, stamina `0x004050c0`, condition `0x004051e0`,
knock-offs / steering knocks. Pick UNCOVERED addrs and spot-check with
pinned REA (max 2 calls). No builds, no captures, no commits.

Baseline: `5ffafa9` (HEAD at start; tree carries sibling uncommitted edits,
see NOT-changed list). Worktree: `/Users/archit/Dev/rr/native-port`
(read-only; no tracked files edited).

Owned/created files (ONLY writes):
- `docs/agent-handoffs/damagesplits-20261016o.md` (this file, new)
- `analysis/damage-20261016o/` (new scratch dir):
  - `rea-409c10-reach.md` (+ `.log`) — REA success, exit 0
  - `rea-4052c0-knockoff.md` (+ `.log`) — REA success, exit 0

## Why these two addrs (UNCOVERED pick)

Prior handoffs already REA-confirmed (whitespace-identical, pinned REA 6.4.0):
`0x004050c0`, `0x004051e0`, `0x00449da0` (`damagesplits-rea-20261012`);
`0x00409400`, `0x004095a0`, `0x00409ad0`, `0x00409bd0`, `0x00408d50`
(`aicombat-leaves-rea-20261012`); `0x004090a0`, `0x004091f0`, `0x004095d0`,
`0x004096f0`, `0x004092b0` (REVERSE_ENGINEERING Combat section).
Of the damage-split set in this task, the UNCOVERED ones were the hit
predicate `0x00409c10` and the shared knock-off decision `0x004052c0`
(`0x00409ad0`/`0x004050c0`/`0x004051e0` already covered). Those two got the
two REA calls; everything else below is re-statement of covered exports +
native code read via `rg` (eight sibling-owned files excluded from search).

## REA evidence (pinned env, hash-verified EXE)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`rea-agents@6.4.0` (`--version` → `6.4.0`), Ghidra 12.1.4 backend.
EXE: `../cdrom/ROADRASH/ROADRASH.EXE`, sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(matches `analysis/inventory.json` + `docs/REVERSE_ENGINEERING.md`; live
`shasum` re-verified this session). Both calls `--format md`, exit 0.

| Function | evidence_id | REA norm chars | Export chars | Match |
| --- | --- | --- | --- | --- |
| `0x00409c10` (fight reach) | `ev_0b8e23f407086b1ede2455898601cf90705b710f4ccfa1ee92b56d38594e7e0d` | 663 | 663 | TRUE |
| `0x004052c0` (knock-off decision) | `ev_f2a06902e9853bddc4c2734eb01f1d7d7615a2b9c866f5ad3a98326665b3fdb0` | 1383 | 1383 | TRUE |

Char method: whitespace-stripped (`re.sub(r'\s+','',…)`) comparison of REA
`normalized_result.pseudocode` vs `analysis/decompiled/00409c10.c` /
`004052c0.c`. Both byte-identical modulo whitespace. Full REA dossiers
(incl. assembly, callers/callees, refs) kept git-ignored in
`analysis/damage-20261016o/`; never commit (disc-derived).

RNG draws: NONE in either function. `0x00409c10` has zero callees (pure
predicate). `0x004052c0` callees are exactly `0x00403d60` (crash entry),
`0x004050c0`, `0x004051e0`, `0x0040ac70` (anim set) — no `0x00441750` RNG,
no `rand` token in pseudocode (the two `rand` greps hit only the table
header `operand_input`). The `rand` substring hits were verified false
positives. All combat randomness lives in the callers (`0x004096f0` steal
roll `(rand&3)==3`, `rand%(6-level)` repeat/shove; `0x004049c0` grunt/crash
sound draws; shared-stream position preserved upstream of `knock_off`).

## Damage formulas (from covered exports + native `src/race.cpp` / `src/race.hpp`)

`0x00409c10 fight_reach(self, target, side, weapon)` — all-AND gate:
side match `((self_lat < tgt_lat) == side)`, then `|dpos| < 0x40`
(`+0x1c`), then lateral `|dlat| < reach·0x1000` (`+0x28`) with
reach ladder 5 default (fists/kick) / 7 club / 8 chain (`SHL EDX,0xc`),
then `|dheight| < 0x3000` (`+0x20`). Returns byte in AL. Native
`Race::fight_reach` (`src/race.cpp:2000`) and `weapon_reach[4]={5,7,8,5}`
(`src/race.hpp:68`) match exactly.

`0x00409ad0` (covered 20261012; re-stated from `analysis/decompiled/00409ad0.c`
+ native `resolve_hit` `src/race.cpp:2055`): attacker rider-half `+0x2fc`
→ stats `+0x1e0`: `tmp = ((max<<8)/base · strength)>>8`, floor
`strength/2`; `dmg = (table[weapon]·tmp + sign-bias(0xff))>>8` with EXE
table `0x00465a00` = native `{256,320,384,64}`; player-involved impact
sound 11/9/8, kick alternates 10/11 on tick parity (`DAT_004b7ecc & 1`);
kick-only bike part `((dmg + bias3)>>2)+1`, then
`0x004052c0(victim, dmg, bike_part, bike_part==0 ? 4 : 5)`.

`0x004050c0` stamina split (covered; native `src/race.cpp:776`):
`max += p·(−4)` floored at `base/2` (trunc divide); `current += p·(−0x40)`,
floor 0 (`<1 → 0`); remote/owner bypass skips both and returns untouched
`+0x2f4` state. `0x004051e0` condition twin: `bike+0x2f0 −= q`, floor 0,
same gates; HUD hook `0x0043c9f0` on player-char hits.

`0x004052c0(victim, p, q, reason)` reason ladder (REA-confirmed this session):
0 → `p=q=0`; 1/4/5 keep; 2 → both `trunc(v·7/4)` via
`(v·7 + ((v·7>>31)&3))>>2`; 3 → both `trunc(v/8)` via `(v+((v>>31)&7))>>3`;
default (incl. silent-hit 6) → zero. Then `0x004050c0` + `0x004051e0`;
if either returns 0 → `0x00403d60` crash entry with reason-4 vectors
`{-80,0,0}/{0,0,0}` else `{-256,0,1792}/{0,0,1024}`. Reason 4 + still
riding (`+0x2f4==0`) → anim 9 via `0x0040ac70(half+0x70, 9)`. Reason 5 →
`bike+0x188 *= p` (32-bit wrap `IMUL`). Steering-knock tail (every reason,
when `+0x188 != 0` and still riding): `+0x288 += (+0x2d8·+0x188 + bias)>>8`
sar-rounded `/256`, clamped to `±+0x29c` (native `src/race.cpp:807`;
note the clamp idiom `(-lim != -next && next <= -lim)`). Returns
`+0x2f4 != 0` (down). Performs no RNG itself.

Knock-offs / steering knocks split: `0x00409bd0` shove (covered 20261012;
native `src/race.cpp:2066`): backhand `∓300` speed transfer + side kicks
`±(1000/2000/3000)` to `+0x288`, kick adds `±8000`, each clamped to
`±+0x29c`. `0x004052c0` reason-2 path (contact-car class 2, ×7/4) and
reason-3 (×1/8) feed the same steering tail above; `0x004049c0`/`0x00404f93`
steer kicks are caller-side (`+0x2d8·Δrate·2>>8`), not inside the splits.

## Capture spec (for the integrator / porter; NOT run)

No captures run (read-only task). To prove runtime behavior of these two:
oracle hook on rider/bike words + scheduler tick; scenario A (fight):
two AI riders side-by-side within `|dpos|<0x40`, lateral inside/outside
`reach·0x1000` per weapon, heights in/out of `0x3000`, both `side` values
→ expect `0x00409c10` gate outcomes; scenario B (knock-off): drive each
reason 0–6 with fixed `(p,q)` and fixed `+0x188/+0x2d8/+0x288/+0x29c`
(pre-seed RNG, offline owner flag 0) → expect scaled `(p,q)`, stamina
`max/current`, `+0x2f0`, crash-entry reason-4-vs-default vectors, anim-9
vs `+0x188` multiply, and `+0x288` tail clamp. Require valid header,
contiguous ticks, completion marker; coordinate prefix/DLL/output ownership
per `AGENTS.md`; never overwrite an accepted capture with a partial run.
Callers to hook per REA dossiers: `0x00409c10` ← `0x004096f0`
(incoming refs `0x4097b3`, `0x409aa7`); `0x004052c0` ← `0x004049c0`,
`0x00409ad0`, `0x0043a190`.

## NOT-changed list (explicitly untouched)

- Tracked: `src/course_scene.cpp`, `src/race_caches.hpp`,
  `src/race_screen.cpp`, `src/road.cpp`, `src/road.hpp`,
  `tests/course_scene_test.cpp`, `tests/race_cache_test.cpp`,
  `tests/road_test.cpp` (sibling live-claimed; excluded from `rg`, never
  read/edited), plus everything else tracked (`src/race.cpp`,
  `src/race.hpp` read-only via `rg`/`read`; `docs/REVERSE_ENGINEERING.md`,
  plans, all other `src/`/`tests/`/`docs/`/`tools/`/`cmake/` untouched).
- No shared-summary edits, no source/test/plan changes, no builds, no
  `ctest`, no oracle/Wine captures, no Ghidra project writes
  (`analysis/ghidra-project/` untouched; `tools/decompile.sh` not run),
  no commits, no live processes held, no build locks, no prefix/DLL work.
- Prior covered splits NOT re-REA'd (quota): `0x00409ad0`, `0x004050c0`,
  `0x004051e0`, `0x00409bd0`, `0x00409400`, `0x004095a0`, `0x00408d50`,
  `0x00449da0` — conclusions carried from the 20261012 handoffs.

## Remaining work (for integrator)

Link this handoff from plan 05/10/16 combat-trace indexes as the
`0x00409c10` + `0x004052c0` spot-check; keep
`analysis/damage-20261016o/rea-*.md/.log` local (git-ignored,
disc-derived). Open: runtime capture proof per spec above; actual port
edits (sibling scope); `0x00409bd0` steering-clamp idiom already ported
but still awaiting oracle parity; no new divergences found.
