# Handoff: bike+rider build + BIKESPEC apply REA spot-check (20261016l)

Task: READ-ONLY background on bike+rider build `0x00405990` table words
(`0x00463ba8`/`0x00463c10` -> `+0xdc`/`+0xe0` think/`+0xe4` move/`+0xe8` slice)
+ BIKESPEC apply `0x00405ea0`. Spot-check only. No builds, no captures.

Baseline: `4a9b29b` (HEAD at session start; tree carries sibling in-flight
edits — none touched).

Owned writes (only these):
- `docs/agent-handoffs/bike-build-20261016l.md` (this file)
- `analysis/bike-build-20261016l/` — scratch, disc-derived, LOCAL ONLY,
  never commit (not in `.gitignore`; integrator must not stage):
  - `rea-405990-bikebuild.md` + `.log` sidecar
  - `rea-405ea0-applyspec.md` + `.log` sidecar

## 1. REA spot-checks (pinned toolchain, 2/2 allowed calls used)

Env per `docs/TOOLING.md`: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH`
(node v24.13.0), `GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=openjdk@21`. Both exit 0 on the hash-verified EXE
(`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`,
matches `analysis/inventory.json` and live `sha256sum`). Provider Ghidra
12.1.4, `rea-agents@6.4.0`, `import_mode: ephemeral-source-immutable`.
Whitespace-normalized `raw_result.value.pseudocode` vs `analysis/decompiled/`:

| Function | evidence_id | REA chars | Export chars | Match |
| --- | --- | --- | --- | --- |
| `0x00405990` | `ev_265985695c474505e7ac80b000e8957563d2f9955c4d1a4b6d9b881321e0fb1f` | 3089 | 3089 | TRUE |
| `0x00405ea0` | `ev_34870f3387e4c0ac0b240a56bc6fce85bd0e55f7052369131af0c62489a77507` | 2432 | 2432 | TRUE |

Body ranges: `0x405990-0x405c7c` (749 B), `0x405ea0-0x406072` (467 B).
Assembly spot cites (from dossiers):
- `0x00405990`: lateral anchor `0x4059a9-0x4059b8`
  (`[EBP+0xcc]+ESI`, `ECX=[0x00463ad0]`, `CALL 0x004387a0`); lane draw
  `0x405a69-0x405a78` (`CALL 0x00441750; CDQ; MOV ECX,3; IDIV; CALL 0x00407990`);
  spec call `0x405b61-0x405b79` (`[EAX+0xc]` SAR 1 = `/2`, variant byte
  `[EAX+0x5]`, `CALL 0x00405ea0`); param_5 1/2 throttle/gear scale
  `0x405b98-0x405bc4` (`0x4642a0` percent, `-0x19/-0xf/-8/-4`).
- `0x00405ea0`: SPEC fetch via `0x004153a0(0x53504543, param_3+1, 8)`
  with `0x0047ea40` cache; mass formula block `0x405f02-0x405f16` region.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof.

## 2. Table-word map (`0x00438930`, `00438930.c:17-20`)

`0x00405990` never touches the tables directly: it passes object images
(`0x00463ad0/ad4` offline) into `0x004387a0` -> `0x00438930`, which copies
template words 3..6 to the node (`param_2` = template base):

| Template word | Byte | Node offset | Role |
| --- | --- | --- | --- |
| `+0xc` (word 3) | `0x446250` bike / `0x4463e0` rider | `+0xdc` (`piVar1[0x37]`) | dispatch/draw word |
| `+0x10` (word 4) | `0x407a90` bike / `0x434660` rider | `+0xe0` (`[0x38]`) | think |
| `+0x14` (word 5) | `0x4060b0` bike / `0x406890` rider | `+0xe4` (`[0x39]`) | move |
| `+0x18` (word 6) | `0x406940` bike / `0x406b40` rider | `+0xe8` (`[0x3a]`) | slice change |

Word values carried from grid-setup-20261014/spawn-chain-20261016k EXE reads
(`0x00463ba8` words 0-9 = `3,4,1,0x446250,0x407a90,0x4060b0,0x406940,...`;
`0x00463c10` = `0,8,1,0x4463e0,0x434660,0x406890,0x406b40,...`); not
re-measured this session. Think/move roles per `0x00437e60` dispatch
(`REVERSE_ENGINEERING.md` scheduler/object-lists notes; plan 05
`+0xe0/+0xe4/+0xe8` architecture).

## 3. Spec formula (`0x00405ea0`, `00405ea0.c:21-74`)

Call site (`00405990.c:75`): `FUN_00405ea0(bike, *(rider_row+0xc)/2,
variant_byte)`. `param_2` halving is SAR-1 with sign-bias (`0x405b6f-0x405b75`:
`SUB EAX,EDX; SAR EAX,1` after `CDQ` = trunc-toward-zero `/2`); all later
`>>2`/`>>3` blocks use the same sar-with-bias idiom (trunc toward zero).
With `S = SPEC[param_3]` record, `mass = S[6]`, `rv = param_2`:

- `bike+0x1bc = byte(S[0])`; `+0x168/+0x16c/+0x170/+0x1b4/+0x1b8 = S[1..5]`
- `bike+0x1c4..+0x2f3 = S[7..82]` (0x4c dwords)
- `throttle_max = *(+0x250)*(mass+0x4b)*0x100 -> +0x250`;
  `brake_max = *(+0x26c)*(mass+0x4b)*0x100 -> +0x26c`
- `steer_div = mass + trunc(rv/4)`; `+0x2a0 = *(+0x2a0)*(mass+0x4b)/steer_div`;
  `+0x29c = 0x2e00`; `+0x2a4 = *(+0x2a4)*(mass+0x4b)/steer_div`
- `+0x2a8 = 0x2e00/+0x2a0`; `+0x2ac = 0x2e00/+0x2a4`
- `weight = mass+0x4b + trunc((rv-0x4b)/8)`;
  `+0x260 = throttle_max/weight`; `+0x27c = brake_max/weight`
- `clamp15(v) = v > 0xe ? 0xf : v`;
  `+0x264 = +0x260/clamp15(+0x254)`; `+0x268 = +0x260/clamp15(+0x258)`;
  `+0x280 = +0x27c/clamp15(+0x270)`; `+0x284 = +0x27c/clamp15(+0x274)`
- `+0x2c4 = 0x14`; `+0x248 = 0x2000`; `+0x2dc = 0x2000`

## 4. Native-vs-original table (read-only `rg`; this session)

| # | Item | Original | Native | Verdict |
| --- | --- | --- | --- | --- |
| 1 | Table copy words 7.. | `0x004387a0` slot words to `+0x148..+0x194/+0x11c=-0x25` | `init_bike_object` (`src/bike.cpp:74-84`) same words | MATCH |
| 2 | Table words 3..6 (`+0xdc..+0xe8`) | `0x00438930.c:17-20` writes think/move/slice | `init_bike_object` does NOT write them; cop path does (`src/police.cpp:47`: `0xdc+i*4=behavior[i+3]`, `0x1a4+i*4=behavior[i+22]`) | DELIBERATE DIVERGENCE (native dispatch is not function-pointer based) |
| 3 | Spec apply | `0x00405ea0` formula above | `apply_spec` (`src/bike.cpp:86-122`) same formula + `div0` guards (plan 23 wasm/ARM zero-divisor note) | MATCH (modulo guards) |
| 4 | Build order | `0x4387a0`x2 -> lane `rand%3` -> `0x00407990` -> row `0x63c78+0x40*rider` -> `0x00405c80` + `0x00405ea0` -> scale -> `0x446060`x2 + `0x43ae90` | `Race::Race place_rider` (`src/race.cpp:141-170`): `init_bike_object` + `apply_spec` only; scale at `race.cpp:183-184`; no `0x00405c80`/`0x00446060`/`0x0043ae90` | GAP (known; fix owner scope) |
| 5 | Rider value | `*(row+0xc)/2` (SAR) | `original_opponents` (`src/race.cpp:107`): `word(row+0xc)/2` (C++ trunc) | MATCH |

## 5. NOT-changed list

Tracked files: every file under `src/`, `tests/`, `docs/` (other than this
handoff), `tools/`, `oracle/`, `CMakeLists.txt` — none read-for-edit, none
modified. In particular the seven in-flight paths were not touched:
`src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
`src/road.cpp`, `src/road.hpp`, `tests/race_cache_test.cpp`,
`tests/road_test.cpp`. No builds run, no captures run, no commits.
`git status` after this write shows only this handoff + the pre-existing
sibling dirt + `analysis/bike-build-20261016l/`.

## 6. Remaining work (fix owner)

- `0x00405c80` body + `0x00446060`/`0x0043ae90` tail still uncovered by REA
  (this session spent its 2-call budget on `0x00405990`/`0x00405ea0`).
- Native `place_rider` still lacks the `0x00405c80` step and the anim-init
  tail; `+0xdc..+0xe8` deliberately unported (dispatch differs).
- `apply_spec` `div0` guards vs x86 fault on zero divisor: real specs never
  hit it (code comment); oracle behaviour unchanged — noted, not verified here.

## Live resources

No live REA/Ghidra/Wine processes. No builds, no locks, no captures. REA
dossiers are local under `analysis/bike-build-20261016l/` (disc-derived —
do not commit).
