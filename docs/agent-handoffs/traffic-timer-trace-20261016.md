# Traffic timer trace (plan 10 Task 7): why tick 376 draws one rand() with no car

Date: 2026-10-16. Read-only RE trace in `/Users/archit/Dev/rr/native-port`.
Baseline: HEAD `51cb898`-era tree (shared-tree work present, untouched).
No source edits, no Wine runs, no build dirs touched. One pinned REA call only.

## 1. `0x0044dd90` — full three-branch accounting

Source: `analysis/decompiled/0044dd90.c` (56 lines, read fully).
Verified byte-exact against `analysis/original-disassembly.txt` lines 107306–107480
(`44dd90`–`44df53`). Decompile is faithful; no element-scaling trap here
(all lane/tap accesses are byte-displacement `movl 0x30(%edi)` / `0x2c(%esi)` /
`0xc(%ecx)` on byte-addressed registers).

All state/rate words are **signed int**. All `>>8` scalings are signed
divide-by-256 with round-toward-zero bias `(x + ((x>>31)&0xff)) >> 8`
(disasm: `cltd; andl $0xff,%edx; addl %edx,%eax; sarl $8,%eax`).
Lane counts are truncated to **signed char** (`movb %ah,...; testb; jle skip`),
gate is `> 0`, i.e. effective lanes 1..127; gate-fail = no draw **but the
reset still applies** (reset is outside the gate `if`).

- **Branch A — oncoming timer** (`44dd90`–`44de12`):
  state `DAT_004b7e90`, decrement `state -= scaled(DAT_004b7e84) + param_1`
  (`subl %eax, 0x4b7e90` where `%eax = scaled + %ecx=param_1`).
  Fire: `state < 0` (`jns` over). Tap slice: index from
  `(DAT_004b6e50 - 0x100) & 0x7f00) >> 6` into `DAT_0049e820`, `+0xc` deref,
  lane word at **byte `+0x30`**, scaled `>>8` to char.
  Draw (gate>0): `u = FUN_00441750(); FUN_0044df60(2, -1 - (char)(u % lanes), 0,0, tap)`.
  Disasm uses unsigned `divl` (`movsbl` divisor, `xor %edx`, `subb %dl,%bl`
  from `bl=-1`). Reset: `state += DAT_004b7e88` (unconditional on fire path).
- **Branch B — same-direction/front timer** (`44de18`–`44de96`):
  state `DAT_004b7e94`, update `state -= param_1; state += scaled(DAT_004b7ea4)`
  (two separate adds, same net). Fire: `state < 0`. Same front-tap slice as A,
  lane word at **byte `+0x2c`**. Draw: `FUN_0044df60(0, (char)(draw % lanes), 0,0, tap)`
  (disasm: signed `idivl`; identical result to A since operands non-negative).
  Reset: `state += DAT_004b7e9c`. Else-branch upper clamp:
  `if (2*DAT_004b7e9c < state) state = 2*DAT_004b7e9c`.
- **Branch C — rear timer, INVERTED polarity** (`44deae`–`44df53`):
  state `DAT_004b7e80`, same update shape as B (`-param_1 + scaled(7ea4)`).
  Fire: **`state > 0`** (`testl %ecx,%ecx; jle no-fire`) — opposite of A/B.
  Tap slice from **`(DAT_0049f240 + 0x100) & 0x7f00) >> 6`** (rear tap,
  note `+0x100` vs A/B `-0x100`), lane word `+0x2c`, same draw shape as B.
  Fire-path update: `state -= DAT_004b7e9c` then return. No-fire path lower
  clamp: `t = -2*rate; if (t - state != 0 && state <= t) state = t`.

Call shape note: B/C pass `param_1=0` (via `xorl %ecx,%ecx`) and A passes 2;
that selects the `0044df60` lane/pool path, consistent with the existing
pool-null analysis (first-statement return on `DAT_0049e654 == 0`).

## 2. Writers and init values (all values verified by hand arithmetic)

- `FUN_0044dd00` (`analysis/decompiled/0044dd00.c`): only writer of
  `DAT_004b7e84/7ea4/7e88/7e9c/7e90`.
  Negative `param_1`: `DAT_004b7e8c = -p; DAT_004b7e84 = DAT_004b7ea4 = -p*100`.
  Otherwise: `i = 7e84*p*0x3c; DAT_004b7e88 = i>>8 (biased); DAT_004b7e90 = 7e88;`
  same from `7ea4` for `DAT_004b7e9c`. (`DAT_004b7ea0 = p`; netplay
  `DAT_004c5b00/4c5af9` shifts `p <<= 2`. `0041ec70` gates the positive path
  to solo mode but passes negatives through in netplay.)
- `FUN_0044db80` (`analysis/decompiled/0044db80.c`): race-init sequence
  `FUN_0044dd00(-0x19)` → `7e8c=25, 7e84=7ea4=2500`;
  then `FUN_0044dd00(6)` → `i = 2500*6*60 = 900000; 900000>>8 = 3515`,
  so **`DAT_004b7e88 = DAT_004b7e90 = DAT_004b7e9c = 3515`**,
  then `DAT_004b7e94 = 0; DAT_004b7e80 = -DAT_004b7e9c (= -3515)`.
  (Solo values; netplay `p<<2` path differs — capture-gated, not needed here.)
- Per-tick decrement input: caller `FUN_0044b3b0` (`0044b3b0.c:26-47`)
  passes `param_1 = iVar4 = DAT_004b7ef4 - DAT_004b7f08` — the **race-distance
  delta since last call**, not a constant. `0044b3b0` also pre-splits deltas
  larger than `0x100` into repeated `FUN_00454120(±0x100)` position steps,
  but calls `FUN_0044dd90` **once** with the residual `iVar4` (|residual| ≤ 0x100
  after the loop... actually residual is brought within ±0x100; exact bound
  per the `uVar5`-loop arithmetic).
- Tap inputs: `DAT_004b6e50` (front, init `param_2+0x5000` in `004552e0:25`)
  and `DAT_0049f240` (rear, init `param_2-0x3000` in `004552e0:26`); both
  advanced by `FUN_00454120` each tick. The `& 0x7f00)>>6` indexes a
  `DAT_0049e820`-rooted table; `+0xc` selects the slice object.
- Rand wrapper `FUN_00441750` (`00441750.c` + disasm `441750`–`441760`):
  `if (DAT_004753b0 == 3) return DAT_0049bce8; else return _rand()`
  (else-leg jumps to `0x455eb0`, the import thunk). Mode word `DAT_004753b0`
  is the game-mode selector (0/1/2/3 across menu/race/demo paths);
  setter for the canned value is `FUN_00441740`. **Parity caveat:** in mode 3
  an "expiry" consumes zero stream draws. The solo-replay divergence proves
  the oracle ran with real draws (mode != 3) at tick 376.

## 3. Pinned REA spot-check (exactly one call)

Command (from `native-port/`, env per `docs/TOOLING.md`):
`npx rea-agents@6.4.0 function "../cdrom/ROADRASH/ROADRASH.EXE" 0x0044dd90 --format md`
Env: `PATH=$HOME/.nvm/versions/node/v24.13.0/bin:$PATH` (node v24.13.0),
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`.
EXE hash-verified: `shasum -a 256` =
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`,
matches `analysis/inventory.json` (`ROADRASH.EXE`, 541184 bytes).
Exit code: 0 (pipeline through `tail`; REA returned a full md dossier).
Pseudocode: the captured tail showed the normalized value-flow section, not
the pseudocode block (output truncation), so strict pseudocode-identity was
**not** directly re-confirmed this run. Structural agreement is exact and
recorded instead: 3 `FUN_00441750` call sites (`0x44dde4/0x44de6c/0x44df04`),
3 `FUN_0044df60` call sites (`0x44de00/0x44de7e/0x44df16`), timer-expiry
branches at `0x44ddb1/0x44de3d/0x44ded5`, lane gates at `0x44dde0/0x44de6a/0x44df02`,
clamps at `0x44dea7/0x44df46` — matching `0044dd90.c` line-for-line in
control flow. Prior TOOLING.md evidence (REA pseudocode identical to
`tools/decompile.sh` export for this same function) stands.

## 4. What native must reproduce for parity (no hardcoding)

1. Solo race inits: `7e8c=25, 7e84=7ea4=2500, 7e88=7e90=7e9c=3515,
   7e94=0, 7e80=-3515` via the `0044db80 → 0044dd00` path (including the
   netplay `<<2` fork for later modes).
2. Per-tick call with `param_1` = race-distance delta (`7ef4-7f08` residual
   from `0044b3b0`), decrement terms `scaled(7e84)=9` for A and
   `scaled(7ea4)=9` for B/C (recompute, don't constant-fold: they differ if
   difficulty/netplay paths change the bases).
3. Exact signed/bias `>>8` scalings, char-truncated lane counts with `>0`
   gate, `u % lanes` draw order (lane pick drawn **before** the `0044df60`
   call; pool-null swallows placement but the draw already happened),
   A-lane `-1 - (u % lanes)` vs B/C-lane `(draw % lanes)`, and the
   unsigned-`divl` (A) vs signed-`idivl` (B/C) distinction (no numeric
   effect while lanes > 0 — noted for completeness).
4. Resets/clamps exactly: A `+= 7e88` on fire; B `+= 7e9c` on fire else clamp
   to `2*7e9c`; C `-= 7e9c` on fire (state>0) else clamp to `-2*7e9c` with
   the `!=` guard. Resets apply even when the lane gate is 0 (0-draw expiry).
5. Front-tap (`6e50-0x100`) vs rear-tap (`9f240+0x100`) slice select, lane
   offsets `+0x30` (A) vs `+0x2c` (B/C), all advanced through `00454120`.
6. The `00441750` mode-3 canned-value bypass (zero-draw expiry in mode 3).

## 5. Capture-gated (explicitly not derived here)

- Which branch (A or B — analytic prior below) fires at tick 376 on sched900,
  and the lane-count-at-tap value that opens its gate. Needs the per-tick
  `param_1` series and slice lane words from the accepted capture; not
  re-derived (Wine runs out of scope for this task).
- Analytic prior (not a substitute): with init 3515 / step 9+d: A first-fires
  at first n with Σ(9+d)>3515; B first-fires at first n with Σ(d-9)>0 from 0;
  C needs Σ(9-d)>3515 (i.e. sustained near-stop) so it cannot be the tick-376
  event on a moving schedule. **Speculation label:** the tick-376 single draw
  is therefore an A- or B-branch first expiry with gate > 0 — branch identity
  to be settled by capture data, and native must NOT hardcode tick 376.
