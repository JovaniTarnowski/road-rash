# Math helpers background (fixed-point semantics for separate/integrate_speed) — 20261016i

Task: read-only background on fixed-point helpers (`high_product` / `sine` /
`div_pow2` / shift rounding, sar on negatives, `uint32_t` wrap) used by
`separate()` / `integrate_speed()`. Spot-check only; no builds, no captures.

Baseline: `a7c1ecb`. No tracked file edited, no commit. New files only:
this handoff + `analysis/math-20261016i/` (`rea-4390c0.md`, `rea-44f720.md`,
`inventory.md`).

## REA spot-checks (pinned env, at most 2 calls — both used)

Env per `docs/TOOLING.md`: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=.../openjdk@21...`, `npx --yes rea-agents@6.4.0 function
../cdrom/ROADRASH/ROADRASH.EXE <addr> --format md`, exit 0 both.
EXE sha256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`.

| Addr | Native counterpart | evidence_id | Result |
| --- | --- | --- | --- |
| `0x004390c0` (`integrate_speed` force/drag region) | `src/bike.cpp:261-286` | `ev_cafab47718a1ec44b2e434fde3e95d9d2b5db2a87aadd85f805b13b50fea36ce` | REA pseudocode matches `analysis/decompiled/004390c0.c` (whitespace-normalized); assembly confirms sar/imul idioms below |
| `0x0044f720` (`atan_ratio` table helper) | `src/bike.cpp:175-180` | `ev_c87618ad1941a252c12fd91d4330c150a982db45a019c225b9749d8c9d827a78` | REA pseudocode matches `analysis/decompiled/0044f720.c`; `NEG`/`IDIV`/`MOV AL,[ECX+0x476768]` confirm negate, trunc divide, byte-table read |

Raw outputs kept local: `analysis/math-20261016i/rea-4390c0.md` (1434 lines),
`rea-44f720.md` (864 lines), plus `.stderr` logs. REA is a spot-check, not
runtime proof. Prior `mathhelpers-rea-20261012` already covered `0x0044f5a0`
(atan2), `0x0044f4a0` (sine), `0x0044a870`; sibling sessions covered
`0x00436a40` (separate) — none re-run here.

## Exact semantics table (assembly-authoritative, `0x004390c0` bytes `4390c9..43913f`)

| Idiom | Bytes | Semantics | Native match |
| --- | --- | --- | --- |
| `abs(x)` | `CDQ,XOR,SUB` (`(x^(x>>31))-(x>>31)`, sar 31) | exact, both signs | `std::abs` on `int32_t` (`bike.cpp:262,275`) |
| airborne `speed_abs>>2` | `SAR EDI,2` | floor div 4; operand `>= 0` so == trunc | `base = speed_abs >> 2` (`bike.cpp:264`) |
| contact `(c*speed_abs)>>14`, `c` = `MOVSX` signed byte `0x0046dce4` | `IMUL,SAR 0xe` | floor div 16384; `speed_abs >= 0`, sign = sign of surface coeff | `((c * speed_abs) >> 14)` (`bike.cpp:268`) |
| aero `(coeff*(base>>8))>>8` | `SAR 8,IMUL,SAR 8` | floor shifts; inner operand `>= 0` | `(b.i32(0x150) * (base >> 8)) >> 8` (`bike.cpp:271`) |
| lateral `abs(+118)>>4` | `CDQ,AND 0xf,ADD,SAR 4` | biased shift = **trunc-toward-zero** div 16; input already `abs`'d so bias is 0, == floor | `std::abs(b.i32(0x118)) >> 4` (`bike.cpp:275`) |
| forward `accel = force-drag-lateral`, `v += dt*accel` | `SUB,SUB,IMUL(32-bit wrap),ADD` | wrap `imul`, wrap add | `b.i32(0x18) * b.accel` + `+=` via `I32Field` uint32 wrap (`bike.hpp:41`) |
| stall clamp | `TEST,SETLE,DEC,AND 0x64` | `force>=1 ? nv : (force<1 ? 0 : 100)` | `nv < 1 ? (114<1 ? 0 : 100) : nv` (`bike.cpp:280`) |
| reverse `accel = force + abs(lateral+drag)` | `ADD,CDQ,XOR,SUB` | abs of sum, then wrap imul/add | `bike.cpp:283-285` |
| `atan_ratio` neg / table / reciprocal | `NEG / MOV AL,[table] / CDQ+IDIV` | `-f(-x)`; byte table `0x00476768`; `0x100-f(0x10000/x)` trunc `IDIV`, `0x100` cap | `bike.cpp:175-180` line-for-line |
| `sine` quadrant fold (prior REA) | — | `sine[i]*2` 32-bit wrap, Q15→Q16 | `bike.cpp:182-190`; `actor_sincos` (`actor_render.cpp:49-67`, `0x0044f510`) same fold via wrapping add/sub |
| `high_product` (HUD only, not sim) | — | `(int64(a)*b)>>32` floor high-word | `src/hud.cpp:9`, used `hud.cpp:209` minimap scaling |
| rpm sar-round + wrap imul | — | `(x+((x>>31)&0xff))>>8` = trunc div 256; `uint32_t*uint32_t` wrap | `bike.cpp:394-395` (`0x004349c0`) |
| `separate` mass splits / `rel` term | — | C `/` trunc (matches `IDIV`) for `(depth*m)/m`; `rel>>9+rel>>12+rel>>7` floor shifts can see negative `rel` | `race.cpp:378,403-404` etc. |

Key subtlety recorded: the original mixes **floor sar** (aero/contact shifts)
with **biased-shift trunc** (lateral `+15>>4`, rpm `+255>>8`). Inside
`integrate_speed` every shifted operand is non-negative in practice
(`abs`'d speeds, positive coeffs), so floor == trunc there and the choice
cannot produce a gap. Negative dividends only occur in `separate()`-style
terms (`rel>>k` floor; `(depth*m)/m` trunc) — native uses the same idiom in
each place, so rounding is not a standing suspect.

## One-unit-gap relevance (584 / 735 class)

- Tick-584 single-unit speed gap was **proven a hidden input, not rounding**:
  original rider row 0 vs native row 8 (throttle cap/ramp 9188/918/612 vs
  9160/916/610), exact arithmetic at `analysis/demo584` thresholds
  (`force=(232*7330)>>16=25`, `aero=(130*(6182>>8))>>8=12`, `speed=6187`
  vs original 6188). See `demo584-next-20261015.md`; fix moved first
  divergence 584 → 735 (rider-row override + replay `raw_1e0` derivation).
- Consequence for future one-unit hunts: compare hidden inputs first
  (throttle `+240`, force `+114`, caps `+260/264/268`, gear factor `+1f0`,
  surface `+158`), then suspect arithmetic. Do not add rounding bias to
  close a gap — the table above shows native already matches idiom-for-idiom.
- `high_product` is HUD-only and cannot affect sim gaps. `sine`/`atan2` are
  pure tables (no RNG) per prior REA handoff.

## NOT changed (explicit)

Tracked files only **read**, never edited: `src/bike.cpp`, `src/bike.hpp`,
`src/hud.cpp`, `src/actor_render.cpp`, `src/race.cpp` (`separate`), and
expressly avoided live-claimed paths (`docs/PROGRESS.md`,
`docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`, `docs/plans-10/*`,
`docs/plans-32/*`, `src/race.cpp`, `traffic.*`, `terrain.*`,
`race_setup.*`, proxy, `CMakeLists.txt`, `oracle_sim_test`,
`traffic_test`, `race_setup_test`). No builds, no captures, no Ghidra
project writes, no commits. No live processes held (both REA calls
completed, exit 0). `git status` baseline had only pre-existing untracked
`analysis/*` session dirs; added only `docs/agent-handoffs/math-helpers-20261016i.md`
and `analysis/math-20261016i/`.

## Remaining / next

- No source change justified by this slice (semantics already match).
- Suggested next probe for the 735 gap stays input-side (brain `+0x194`
  region per VERIFICATION.md), reusing the evidence_id/rounding table here
  to rule out arithmetic early.
