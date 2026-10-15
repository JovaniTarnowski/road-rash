# Wall collision verify: `0043a610` middle vs disassembly (READ-ONLY)

- Baseline: main at verify time; no source edits, no Wine captures, no build dirs touched.
- Prior: `offroad-trace-20261015.md` §1.4 (head/tail only). This file covers the MIDDLE
  (type-5 dispatch, type-0/1/3/5 faces, scrape windows, tail) against
  `analysis/original-disassembly.txt` (`objdump`, `43a610`–`43ada5`; extracted to /tmp only).
- Verdict: **pseudocode is clean** — no pseudocode-vs-disassembly discrepancies found.
  One genuine original-code quirk confirmed (stack-slot reuse, §3); port formulas in §4.

## 1. Jump table (new fact; EXE bytes read, not just disasm)

Table at file offset for VA `0x43ada8` (`.text` rawptr math: `rawptr + (VA-0x401000)`):

| index = `*(u8*)(slice+0x3c)` | target | pseudocode |
|---|---|---|
| 0 | `0x43a8f6` | case 0 (lines 113–152) |
| 1 | `0x43aa37` | case 1 (lines 153–185) |
| 2 | `0x43ad9e` (exit) | `default:` → return (186–187) |
| 3 | `0x43aafa` | case 3 (lines 189–212) |
| 4 | `0x43ad9e` (exit) | `default:` → return |
| 5 | `0x43ab86` | case 5 (lines 213–221) |

Guard `43a8e1–43a8ef`: `cmp $0x5; ja exit; jmp *table(%eax,4)` — lines 112, 186–187 exact.
So type 2/4 = free passage (no-op), confirming §1.4's claim with table proof.

## 2. Confirmed ranges (all vs disassembly, strict-compare direction checked)

- Lines 16–21 early gate — `43a617–43a647` (`sarl $8` both, `e0 < lo && hi < e4` exit). Re-confirmed.
- Lines 24–34 type-5 + owner + height gate — `43a64d–43a6a0`. `+0x38` byte cmp 5;
  `+0xc4==4`, or `==8` with `[+0x1c4]+0x2f4==1`; height `(b(+0x3b)&0xfc)<<8`, default
  `0x40`; exit if `+0x20 >= height` (`jge`, strict `<` to enter). Exact.
- Lines 35–59 bit-0x1 face — `43a6a6–43a779`. `+0xe8` lerp by `+0x1c&0xff` (`imul/cltd/
  and $0xff/add/sar $8` = sar-rounded `(p + (p>>31 & 0xff))>>8`), minus sbyte `+0x3a`;
  strictly-inside window (`jge`/`jle` exits); inner face from `+0x40` via `+0x30&0xff`;
  clamp `+0x3c < face*0x100 ? (face-4)*0x100-hw : (face+4)*0x100+hw` (`jl`). Exact.
- Lines 61–87 bit-0x2 face — `43a77f–43a86e`. Mirror on `+0xec`/`+0xc`/`+0x3a` add;
  polarity reversed (`jg`): `+0x3c > (face+d)*0x100 ? hw+(face+4)*0x100 : (face-4)*0x100-hw`.
  `uVar9==0 → LAB_0043a874 →` type dispatch. Exact.
- Lines 89–107 crash-through (`uVar9=3`) — `43a878–43abbd`. `+0x160!=0` skip; height
  recompute; enter iff `+0x17c + +0x20 > height` (strict, `jle` exits); `+0xc4==4` with
  `+0x2f4==0 → 3`; `+0xc4==8 → uVar9=0` (AI cancels). Exact.
- Lines 110–112 dispatch entry — `43a870–43a8e1` (`test %ebx; je dispatch`). Exact.
- Lines 113–152 case 0 — `43a8f6–43aa32`. `+0x37 & 0x38` exit; faces `+0xec`/`+0xe8`;
  `0xfa` lerp = `((frac*250)/3)>>8` via LEA chain (`*5,*5,*5,*2`), signed `idiv`;
  `uVar9=1`, `iVar7 = face*0x100 ∓ hw`, overlap `iVar5-face` / `face-iVar2`, side 1 (right).
  Scrape window (145–151) at `43a9d2–43aa27`: next-flags `&0x18`, else
  `slice[side+0x148]!=-1 && next[side+0x148]==-1`; `(hw+hw/2)>>8 < overlap < (2hw)>>8`
  (sar-rounding; `cltd` clobbers side reg first — pseudocode's plain `hw/2` is faithful,
  §3). `uVar9=2`. Exact.
- Lines 153–185 case 1 — `43aa37–43abb8`. Player = `param_1==DAT_004642d8` (absolute)
  or `[DAT+0x2fc]==param_1` (`43aa37–43aa48`); player clamped to `+0x14c/+0x158`
  (`+0x15c` when `+0x160`, `+0x150` when `+0x154==0` — `test/jg`, i.e. `<1`);
  others to `+0xec/+0xe8`. Note line 179's `iVar5` is the overwritten `+0xe8` face —
  keep statement order when porting. Exact.
- Lines 188–212 case 3 — `43aafa–43ab84`. Faces `+0xec`/`+0xe8`, overlap in `ecx`;
  scrape (207–211) at `43ab4e–43ab7f`: needs next-`+0x37` bit `0x40` CLEAR,
  `(hw/2)>>8 < overlap < (round2(hw)+hw)>>8`. Exact (rounding note §3).
- Lines 213–221 case 5 — `43ab86–43abb8`. `+0x158` over / `+0x14c` under, `uVar9=1`. Exact.
- Lines 223–288 tail — `43abbd–43ad9b`. Scrape path `|+0xec|>=0x9c4` else `bVar1=false`
  (`cmpl $0x9c4`, signed abs via `cltd/xor/sub`); non-scrape `+0xf0 *= -100>>8`
  (`*100` via `shl2/lea*5/lea*5/neg`, sar-rounded); `uVar9==3 & +0xc4==4` →
  `0x00403d60(reason 3)` + conditional re-reverse; `+0x60=0` unconditional;
  `+0xc4==4`: same-sign `+0xf0`/`+0x288` clear + `+0x5c=0`; `+0xc4==8`:
  `+0x5c = (+0x108>=0)-1 & 0x800000` (`setge/decl/and`); crash iff
  `(bVar1 || (|+0xf0|>0x9c3 && uVar9!=3)) && +0xc4==4` → grunt `0x00436830(5,·+0x1c,·+0xec)`
  if `+0x324==0`, packet `{0,0,0x10,0,0,0x50}` + `0x00403d60(reason 6)`; final `+0x28=iVar7`.
  Exact, incl. reason codes 3/6, event 5, callees `0x439270/0x436830/0x403d60`.
- Call-site context (bonus): per-rider step at `43ade1–43adf1` calls
  `0x4390c0 → 0x439920 → 0x43a610`, then `0x43a190` unless `+0xc4==0x10`.

Byte-offset audit: export already uses byte displacements everywhere
(`+0x1c&0xff`/`+0x30&0xff` = dword-load-then-`and $0xff`; `+0x3a`/`+0x148`/`+0x149`
`movsbl`/signed `cmpb`; `+0x37`/`+0x3b` unsigned; `+0x3c` zero-extended switch).
No element-scaling trap present. No discrepancies.

## 3. Quirk (confirmed in disassembly, not a pseudocode error)

Stack slot `0x18(%esp)` (`local_18[0]`) doubles as the `/3` divisor AND the case-0
side index. Right-hit forces index 1 (`43a962`); left-hit reads the slot (`43a9a4`),
which is `3` iff either `+0x148`/`+0x149 == -1` ran, else **uninitialized garbage**
used at `43a9db/43a9e9` (`slice[idx+0x148]`, `next[idx+0x148]`). Portable code must
not replicate the garbage read blindly — initialize (e.g. `3` when the taken side's
sign byte is `-1`, matching the divisor coincidence) and flag the divergence risk
for the tick-149 scrape port. Rounding notes: `(hw-sign)>>1` ≡ C `hw/2` and
`(2hw)>>8` vs `(round2(hw)+hw)>>8` differ only for negative/odd `hw`; `hw=+0x174 ≥ 0`
in practice, so plain C arithmetic is faithful.

## 4. Port formulas (C, fixed-width; bike `p`, slice `s`, `hw = p->w174` Q8 half-width)

```c
int32_t lo = (int32_t)(p->x28 + hw) >> 8, hi = (int32_t)(p->x28 - hw) >> 8; // names per export
if (s->e0 < hi && lo < s->e4) return;                    // strictly inside: no-op
// type 5, owner player(4)/cop-AI(8, rider+0x2f4==1):
int32_t hgt = (s->b3b & 0xFC) == 0 ? 0x40 : (int32_t)(s->b3b & 0xFC) << 8;
if (p->y20 >= hgt) goto dispatch;
// bit0: face = s->e8 + rnd8((p->f1c&0xFF)*(next->e8 - s->e8)) - (int8_t)s->b3a;
//   bit1: face = s->ec + rnd8((p->f1c&0xFF)*(next->ec - s->ec)) + (int8_t)s->b3a;
//   rnd8(q) = (q + ((q>>31)&0xFF)) >> 8; hit iff hi < face && face < lo (u=1).
//   clamp bit0: p->z3c < face*0x100 ? (face-4)*0x100-hw : (face+4)*0x100+hw;
//   clamp bit1: (face+d)*0x100 < p->z3c ? hw+(face+4)*0x100 : (face-4)*0x100-hw.
// crash-through: if (p->w160==0 && hgt < p->w17c+p->y20) { if (p->c4==4 && p->w2f4==0) u=3; else if (p->c4==8) u=0; }
// case 0: faces s->ec/s->e8 ± rnd8(((p->f1c&0xFF)*250)/3) when sbyte s->b149/b148==-1;
//   right: u=1, pos=face*0x100-hw, ov=lo-face, side=1; left: u=1, pos=hw+face*0x100, ov=face-hi.
//   scrape iff (((next->b37&0x18)!=0) || (s->sb[side]!=-1 && next->sb[side]==-1))
//     && (((hw+hw/2)>>8) < ov) && (ov < ((hw*2)>>8)) → bScrape=1, u=2.  // sb[1]=+0x149
// case 1: player/cop ? bounds +0x14c/+0x158 (+0x15c if +0x160, +0x150 if +0x154==0)
//        : right face +0xec (pos=face*0x100-hw) / left face +0xe8 (pos=hw+face*0x100); u=1.
// case 3: u=1, pos/ov as case 0 (no lerp); scrape iff ((next->b37&0x40)==0)
//     && (((hw/2)>>8) < ov) && (ov < (((hw/2)+hw)>>8)) → u=2.
// case 5: lo>s->w158 ? pos=s->w158*0x100-hw,u=1 : hi<s->w14c ? pos=hw+s->w14c*0x100,u=1 : noop.
// tail: if (bScrape && abs(p->vEC) < 0x9C4) bScrape=0; else if (bScrape && u!=2) { p->vEC=0; move(p, p->f30-p->f1c, 0); }
//   else if (u==1||u==2) p->vF0 = rnd8(-100*p->vF0);
//   else if (u==3 && p->c4==4) { crash(3); recompute hgt; if (p->w17c+p->y20 < hgt) p->vF0=rnd8(-100*p->vF0); else pos=p->x28; }
//   p->w60=0; [c4==4: same-sign vF0/w288→w288=0; w5c=0] [c4==8: w5c=(p->w108>=0)-1 & 0x800000] [else w5c=0];
//   if ((bScrape || (abs(p->vF0)>0x9C3 && u!=3)) && p->c4==4) { if (bScrape && p->w324==0) grunt(5,&p->f1c,&p->vEC); crash(6); }
//   p->x28 = pos;
```

All `>>` on signed values are arithmetic (`sarl`); `abs()` = `xor/sub` sign branchless.

## 5. REA

Not invoked: pseudocode nowhere contradicts the disassembly (one extra opinion on the
same bytes adds nothing), and the §3 quirk is a runtime-value question REA cannot
answer. Pinned recipe (`rea-agents@6.4.0`, env `docs/TOOLING.md:38–74`) noted for
the porter if the quirk needs a second rendering. No commits made.
