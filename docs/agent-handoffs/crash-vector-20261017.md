# crash-vector-20261017 — contact crash impulse axis, full 0x00403d60 audit

Task: fix a measured fidelity error in the class-1/2 contact crash impulse
and audit every crash-entry call site (plan 10 Task 5/6 input, plan 16
Task 1). Baseline `851e795` (strict verify-sim acceptance). Owned files:
`src/crash.hpp`, `src/race.cpp`, `tests/race_test.cpp`,
`tests/crash_test.cpp`, `docs/REVERSE_ENGINEERING.md`, `docs/PROGRESS.md`,
`docs/VERIFICATION.md`. Build dir `build-agent-crashvec-20261017`; no Wine or
Ghidra this session (read-only `analysis/disassembly` + `analysis/decompiled`).

## Finding

`Race::contact_car` (via `crash_car`) and `Race::contact` (bike/bike) passed
the crash entry the rider impulse `{0,0x200,0}`. The original's
`0x004049c0` builds its own `local_18` pair and calls `0x00403d60` at
`0x404eeb`:

```
404ee7: movl %eax, 0x20(%esp)        # [S+0x20] = 0
404eeb: pushl %edi                   # esp = S-4
404eec: movl $0x200, 0x28(%esp)      # [S+0x24] = 0x200   (rider word 2)
404ef4: leal 0x20(%esp), %edx        # edx = S+0x1c -> rider struct
404ef8: movl %eax, 0x20(%esp)        # [S+0x1c] = 0
404efe: movl %eax, 0x30(%esp)        # [S+0x2c] = 0
404f02: movl $0x100, 0x34(%esp)      # [S+0x30] = 0x100   (bike word 2)
404f0a: movl %eax, 0x2c(%esp)        # [S+0x28] = 0
404f12: pushl %eax                   # param_3 = S+0x28 -> bike struct
404f13: calll 0x403d60
```

`FUN_00403d60(int param_1, int *param_2, int *param_3, undefined4 param_4)`
is `__fastcall`, so `param_2 = edx` = the rider struct at `[S+0x1c]` and
`param_3` = the bike struct at `[S+0x28]`. `param_2` is added to the half
(`param_1+0x2fc`) and `param_3` to the bike, component `[i]` into
`+0xec`/`+0xf0`/`+0xf4` (decompile lines 99-106). Therefore:

- rider `{0,0,0x200}` — the 0x200 is the half's **vertical velocity** `+0xf4`
- bike `{0,0,0x100}` — the bike's `+0xf4`

The port put 0x200 on `+0xf0` (steer rate) at the two sites that carried
`{0,0x200,0}`; `Race::contact_static` already had the correct axis. The
comment above `crash_car` and `REVERSE_ENGINEERING.md`'s roadside-object
entry both stated the correct pair, so the code contradicted its own traced
documentation.

## Change

- `src/crash.hpp`: the traced pair is now
  `rr::kContactCrashRiderImpulse{0,0,0x200}` /
  `rr::kContactCrashBikeImpulse{0,0,0x100}`, with the disassembly evidence
  in the comment.
- `src/race.cpp`: `contact_static`, `crash_car` and `contact` all use the
  constants.
- `tests/race_test.cpp`: pins the constants against the traced values
  (synthetic, always runs).
- `tests/crash_test.cpp`: `begin_crash` with the pair must land the rider
  impulse on the half's `+0xf4` and the bike impulse on the bike's `+0xf4`,
  and must not touch `+0xf0` or `+0xec` (asset-gated).

Both new checks were confirmed to fail with the pre-fix pair and pass with
the fix.

## Full 0x00403d60 call-site audit

| site | reason | rider (param_2) | bike (param_3) | native |
| --- | --- | --- | --- | --- |
| `0x404eeb` (`0x004049c0` class 1/2 gap) | 1/2 | `{0,0,0x200}` | `{0,0,0x100}` | `contact_static`/`contact_car`/`contact` — shared constants |
| `0x4053b2` (`0x004052c0`) | 4 | `{-80,0,0}` | `{0,0,0}` | `knock_off` reason 4 |
| `0x4053b2` (else) | other | `{-256,0,1792}` | `{0,0,1024}` | `knock_off` default |
| `0x434825` | 6 | none | none | **not modeled** |
| `0x438e34` | 6 | `{-0x600,0,0}` | `{0x400,0,0}` | **not modeled** |
| `0x43a50b` (`0x0043a190`) | 6 | `{0,0,-0x50}` | `{0,0,0x50}` | **not modeled** (high-jump wreck) |
| `0x43ac62` (`0x0043a610`) | 3 | none | none | `walls` reason 3 |
| `0x43ad96` (`0x0043a610`) | 6 | `{0,0,0x50}` | `{0,0,0x10}` | `walls` reason 6 |

`0x434825` is in the player-think region (attack control bit + `+0xec < 0x3e8`
+ `+0x2f4 == 0`); `0x438e34` sits near a 25-tick global timer. Both are
reason-6 crashes with no impulse or with a lateral pair. `0x43a50b` is
`0x0043a190`'s high-fall wreck: when `dt*0x4600 <= above` on the upward
threshold crossing it knocks off reason 2 (`0x2000000`/`0x2000000`) and
crashes reason 6 with rider `{0,0,-0x50}`/bike `{0,0,0x50}`; the port's
`Race::vertical` does not model that branch. These three remain open and are
listed in `REVERSE_ENGINEERING.md` rather than waved through.

## Evidence

- `race-test` exit 0, `0 failures`.
- `crash-test` exit 0, `contact crash impulse pair lands on the vertical
  velocity (+0xf4)`.
- `BUILD_DIR=build-agent-crashvec-20261017 make verify-sim` exit 0,
  `acceptance_met: true`, `1431 ticks identical` (the fix does not change the
  solo replay — it never reaches a class-1/2 gap crash). Receipt
  `analysis/agent-gates/oracle-12060ac09aff4f2093f2f84ec47d9bb5/receipt.json`.
- Full Debug build exit 0; `ctest` 101/101.

## Remaining

- End-to-end capture proof of the class-1/2 crash needs a bike/bike or
  bike/car contact scenario (plan 10 Task 6 `napa-field-l1` /
  `napa-crash-car`); the solo acceptance golden cannot exercise it.
- Unmodeled crash entries `0x434825`, `0x438e34`, `0x43a50b` (high-jump
  wreck) — plan 11 Task 4 / plan 16 Task 1.
- `docs/plans/00-INDEX.md` status table is stale (reviewed October 14, shows
  plan 10 Task 5 in progress) and should be refreshed.
- Live-AI parking/waking (plan 10 Task 6) and the 70 unmodeled-but-constant
  rider-half words (previous handoff) are still open.
