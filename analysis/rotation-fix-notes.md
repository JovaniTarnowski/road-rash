# Body-roll rotation fix — 11a Task 3 Step 3 (2026-10-12)

RotationFixture's trace verified line-by-line against
`analysis/decompiled/00450ad0.c` + `0044f510.c` before porting. Three
defects fixed in `src/actor_render.cpp`:

- **D1 — q3 sin/cos arm** (`actor_sincos`): `0044f510` case 3 gives
  `param_2 = iVar2 * -2` (`-2*tab[1023-k]`), `param_3 = iVar1 * 2`
  (`2*tab[k]`). The port had `+2*tab[k]` / `-2*tab[(1024-k)&0x3ff]`
  (wrong sign AND `1024-k` reads `tab[0]` at k=0 where the original
  reads `tab[1023]`).
- **D2 — final-block slot mapping.** Port names are swapped
  (`cosv` holds `local_3c`, `sinv` holds `local_34[0]`); the final
  block was written as if `cosv` were C. Per the decompile:
  `i5b` uses the flip-negated S, `i9` uses un-negated C,
  `o2 = (qw*C>>8)*W`, `o3 = (qh*S>>8)*W` (qh, not qw — the scout's
  minimal "swap o2/o3" was incomplete; re-traced).
- **D3 — `masked_div` shift**: `(val & 0xffff00ff) >> 8` is an
  arithmetic SAR (`CONCAT44` sign-extends; REA value_flow shows
  `INT_SRIGHT`), not logical. Diverges for negative spans (flipped nodes).

Test harness (`tests/oracle_scene_test.cpp`, 11a Task 3 Step 2 area):

- Descriptor matched by sel bytes (unique bank entry required), not by
  dump `+0x80/+0x81`: `+0x81` lags dispatch by one RRAN advance for at
  least one rider (sel15 desc == bank[11][3] exactly; dump says index 4).
- Body roll comes from the dispatch `+0xbc` sel key (new
  `body_roll_override` on `ActorNodes::rider`, production default
  unchanged), not the `+0x60`-derived view roll (disagreed for bb12:
  view roll nonzero vs dispatch 0).

Golden result (`napa-drive-v5`, `--native-actors`): frame-750 body at
pos 26439 now matches all 4 view points + flip (was the 1 open defect).
19/21 nodes; next open item below.

## OPEN: shadow source for empty SHADOW.DAT frames (frame 750 pos 25615)

`--native-actors` stops at bb12 (pos 25615, checked 4): the shadow's
cell frame is 22 and `HISHADOW.DAT[22]` is genuinely empty on disc
(0x0). ~130 of 326 shadow frames are empty (22–40, 71–80, 90–99,
118–139, 144–153, 163–175, 180–190, 199–212, 221–224, 233–240, 249–251,
323–325). The original still drew the shadow (dump node 67, axis rect
18800x18800, frame 22) — square, so not darkened body pixels (27:65).
Where the fallback pixels come from (Bob page? TEMPLATE.MIP blob?) needs
its own trace. Untouched by this fix.
