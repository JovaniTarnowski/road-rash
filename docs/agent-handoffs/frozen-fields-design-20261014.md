# Plan 10 frozen-field capture design (October 14, 2026)

Read-only source review for integrator; baseline c2ab4d9. Only this handoff
written. Existing shared modifications preserved. No captures, Wine, builds,
source edits, commits, or resource locks. No live processes remain.

## Findings that change the next action

`tests/oracle_sim_test.cpp:585` currently seeds frozen bikes only inside
`if (demo && solo)`. Human solo captures therefore do not hydrate frozen
laterals despite having nodeaddr rows. Separate frozen-world hydration
(`if (solo)`) from demo player brain/countdown logic. This is a diagnostic
replay initial-state repair; it does not establish faithful native grid
construction. Current `src/race.cpp:163-168` already initializes previous
copies; the older grid-setup handoff's missing-copy claim is superseded.

The existing replay reconstructs previous copies from current position and
lateral. That cannot reproduce original frozen `+0x3c` if build completion
changed `+0x28` after initial copies. Capture and hydrate actual copies.

## Minimal additive v1 fields

Emit header-frame data in the existing nodeaddr loop, independent of the
think-function filter. Keep existing `nodeaddr.N`, `.type`, `.position`,
`.half`, `.lateral`; extend **type 4 bike nodes** with:

- `bike_30/34/38/3c/40`: signed i32 previous placed words. Current bike
  lateral already exists as `.lateral` (+0x28).
- `bike_108/10c/110`: signed i32 frozen longitudinal/lateral/vertical
  velocities. These are bike fields, distinct from existing `.half108`.
- `bike_c0`: uint8 flags; `bike_2f4`: signed i32 crash state. These close
  the previous handoff's 13/15 unobserved crash/flag caveat.
- Through `half = *(bike+0x2fc)`, `brain = *(half+0x1d8)`, add existing
  namespace `.p.18c` as signed byte and `.p.190/.p.194/.p.198` as signed
  i32. These identify lane, grid bias, scratch lateral target, target pos.

For exact grid centre/lane decomposition, use **bike+0x2c**, then
`next = *(slice+0x10)`; this is the chain read by `0x004091f0`.
The existing slice36 probe uses bike+0x140, which is a different pointer.
Dump numeric `.grid.next` pointer plus `.grid.next_2c` width i32,
`.grid.next_cc` centre i32, and eight `.grid.bend.K` i32 values from +0xb4,
following +0x10 each time. Null/short chain needs explicit availability/count
rather than substituting -1 for a valid signed value. Guard each pointer
and field before reading. Do not call original functions from the logger.

Type 4 bike owns nonzero +0x2fc rider-half link; type 8 is its rider half.
Match the half using the pointer relation, not node iteration order or think.
Frozen think may be patched to zero. Native rider mapping uses original
bike position, but reject duplicate type4 positions instead of allowing map
insertion to silently choose one. Player is `probe.player`; extra type8
player-think row is not a second bike.

## Formula and rounding correction

REA 6.4.0 spot-check of `0x004091f0` succeeded using documented Node24,
Ghidra12.1.4/JDK21. Local output (not committed):
`/tmp/frozen-fields-rea-4091f0-20261014.json`. Matches existing export.
Assembly at 409225-409238 and 40926d-40928f confirms **truncation toward
zero**, not bare arithmetic shift, for signed `/256`, `/2`, and `/8`.
The bike-lateral handoff's sar3 shorthand should not be implemented literally.
`0x00449b40(next,8)` already averages eight sign-extended 24-bit bends
(raw >0x7fffff subtracts 0x1000000); `0x004091f0` divides that result by 8
again, truncating toward zero. Then:

```
lanes = width / 256;
laneIndex = lane==1 ? (lanes-1)/2 : lane==2 ? abs(lanes-1) : 0;
target = (average8 / 8) + (nextCentre + laneIndex*250)*256 + bias;
```

Model original 32-bit wrap explicitly where needed. The post-tick1 brain
+0x194 need not equal bike+0x28: target recomputation can occur without
copying target to the bike. Frozen stationary opponents are the useful
build-value observations. Capture decomposition is evidence for the
native grid formula, not permission to force player target==lateral.

## Backward-compatible integration

`tests/golden.hpp` parses arbitrary numeric keys in schema v1; adding keys
needs no magic bump. Add an optional frame0 `probe.frozen_words=1` marker.
For old captures preserve the existing lateral-only fallback and report
uncovered frozen velocity/copy coverage. For marked captures require the
full declared field set for each matched frozen type4 bike; partial input
must fail. Require exact half-address linkage. Hydrate frozen values once
before countdown/green, in human and demo solo modes; never overwrite RNG
or moving player words. Never reconstruct measured previous copies from
current lateral. Keep the demo player's brain hydration separately gated.

Synthetic parser/helper fixtures should cover old v1, negative i32 and lane
byte, all-zero valid words, pointer-linked half resolution, duplicate bike
positions, missing declared fields, and distinct current/previous laterals.
A focused integration fixture should prove human solo receives the frozen
words while the player remains unmodified. No new commercial golden belongs
in git. Validate current exact replay through tick210 after coordinated
capture; keep expected divergence visible, not blessed in manifest.

## Tool checks

Used rg/fd, existing disassembly and decompiled exports, npx rea-agents.
No client C++ LSP exposed; fallback via tools/agent_lsp.py with existing
build-agent-codex-goal-20261014 compile database reported healthy database.
Apple clangd21 and Homebrew clangd22 parsed the translation unit without
C++ diagnostics, but both exit3 due 20 ExtractFunction tweak errors
(`Cannot extract break/continue without corresponding loop/switch`).
Logs: `/tmp/frozen-fields-lsp-20261014.json` and
`/tmp/frozen-fields-clangd-homebrew-20261014.log`. This is not a clean LSP
gate or build result. Source remained read-only.
