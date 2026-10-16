# ped-contact-20261016f — Plan 24 Task 4 follow-up (READ-ONLY)

Task: follow-up spot-check of the pedestrian-hit branch `0x00440760` + the short-object
branch of `0x004049c0` (knock-down above speed 1000 / attacker, stagger otherwise; rider
nudge below 2000, ride-over above; car-vs-ped). No source, test, plan, or shared-doc edits.

- Baseline: `0141e7f` (HEAD at session start; `git status --short` shows only other workers'
  untracked `analysis/*`/`docs/agent-handoffs/*` files — none touched).
- Worktree: `/Users/archit/Dev/rr/native-port`. No owned build dir; no builds, no captures.
- Mode: READ-ONLY. No tracked file edited. No commits.

## Evidence (pinned REA, 2 calls — budget max)

Env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`npx --yes rea-agents@6.4.0 function "../cdrom/ROADRASH/ROADRASH.EXE" <addr> --format md`.

EXE sha256 verified this session:
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (matches pin).

| # | Address | evidence_id | Result |
| --- | --- | --- | --- |
| 1 | `0x00440760` (ped hit, +0x1a4 handler) | `ev_210c20fa0397622ebec41649ad642bf3dfbd533d0bd5e6d1531d866b5f904424` | exit 0; export norm-chars 1777 contained in REA pseudocode (match) |
| 2 | `0x004049c0` (bike contact; short-object region lines 166–188) | `ev_63dfb661a5d0d4f1ef8d123732d93565766f0fb931069d941025f5eefec8f6ac` | exit 0; export norm-chars 6714 contained in REA pseudocode (match) |

`0x00441500` (lane-clear) deliberately NOT re-fetched (2-call budget spent on the two
follow-up targets). Prior double-confirm stands: `pedcontact-rea-20261012` (REA 669 chars
vs export 670, TRUE). Export re-read this session (`analysis/decompiled/00441500.c`);
native formula matches (see table).

Raw outputs + compact branch notes (scratch only, never commit disc-derived dumps):
`analysis/ped-contact-20261016f/rea-440760-pedhit.md(.log)`,
`analysis/ped-contact-20261016f/rea-4049c0-bikecontact.md(.log)`,
`analysis/ped-contact-20261016f/notes.md`.

Read-only LSP: `python3 tools/agent_lsp.py --check src/pedestrians.cpp` → available, 0 errors
via `clangd --check` (fallback only; "healthy: false" because `CMakeLists.txt` is newer than
`build-clangd/compile_commands.json` — recorded limit, no refresh attempted on the shared symlink).

REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay/pixel claim is made from it.

## Speed thresholds / animation-voice codes / RNG draws (exact)

`0x00440760` (export `analysis/decompiled/00440760.c`):
- Guard `+0x1e0==0` → return. `state==2` skips the knock/stagger decision.
- Knock-down iff `(other +0xc4==4 && 0x00409510(other)) || (|other +0xec| > 1000 && !(other +0xc4==8 || [other+0x1c4]+0x2f4 != 2))`.
- Knock-down: scream iff `0 <= +0x1c−DAT_004b7ef4 < 0x401 && tick−voice > 0xb4 && 0x0044acd0(sel,2,2)` →
  `0x00440730` word + `0x00436830(0x16)` speech (native `voices[0]`); `state=2`; anim set **3**, queue **−1**;
  `+0x154=0x19`; `0x004498c0(obj,0,20000,0)` (speed cap 20000).
- Stagger: `state=3`; `+0x210` → set anim **2**; `+0x218==0 || (+0x21c && (rand&7)==0)` → queue **5** (has) else **1**,
  otherwise queue **4** + voice iff `tick−voice > 0xb4 && 0x0044acd0(sel,2,3)` (native `voices[1]`).
- Tail (`state!=2`): `+0xec = sar4_round(hitter +0xec)`, clamp `<0→0`; `+0x128 = hitter>0 ? 0 : 0x7f`.
- RNG in `0x00440760`: exactly ONE draw site — `FUN_00441750()` for `(rand&7)==0`. Scream path draws none.

`0x004049c0` short-object branch (export lines 20–34, 42–75, 77–93, 166–188, 219–271):
- Player-only sounds: impulse `|+0x184| < 0x1771` → grunt `(rand&1)+3` + crash `rand&1`
  (silent when `< 0x7d1 && |+0x188| < 0x3e9`); `>= 0x1771` → grunt `(rand&1)+4` + crash `(rand&1)+1`.
  Two `0x00441750` draws per sounding contact.
- Class: `other +0x17c < 0x2000 → iVar9=0`. Ped `+0x17c=0x1e00` → always 0 (no `knock_off` path).
- Elasticity: `+0x184/+0x188/+0x18c *= +0x180(=192)/256` (div256).
- Nudge vs ride-over: `speed(+0xec) < 300 && +0x19c==0x20` → `0x00439270(2·dt)` crawl + nudge (`iVar9=6`);
  elif `other +0xc4==0x20 && other +0x1e8!=2` → nudge iff `speed < 2000`, else ride-over;
  else (down) → nudge iff `speed < 500`.
- Nudge (`LAB_00404f4d`): lateral `+= ±sar4(+0x174+other+0x174)·dt` away from other.
- Ride-over: `+0x20 += other+0x17c/2`; `+0xf4 += ((((+0x17c·|+0x184|·2 +r)>>14)+(r>>31&0xf))>>4)·4`.
- Tail: steer-rate block skipped; `+0xf4` clamp `0x600`; impulses zeroed; `+0x19c=0`;
  `+0xec ∈ [−1000, 30000]`; `+0xf4 ∈ ±10000`; `+0x2f4==0 → 0x00407410`.

`0x00441500` (export `analysis/decompiled/00441500.c`, prior REA TRUE):
`lane=0x0044a020(obj,0)`; `node=0x0044a070(obj,lane+delta,neg?0:2,0x14,−1)`; null → clear;
else `sum=(|v|&~0xff0)>>4 + (|v|&~0x1fe)>>1 + (|v|&~0x7f)·2`, `q=sum/(cruise DAT_004b7e8c+1)`,
`f=(factor·0x191)>>8`, clear iff `((q·f)&~0xf0)>>4 <= |gap|&~0xf`.

## Native-vs-original table (ported in `src/race_pedestrians.cpp`)

| Behavior | Original | Native | Verdict |
| --- | --- | --- | --- |
| Unloaded ped hit | `+0x1e0==0` → return | `if (!p.loaded()) return` (`race_pedestrians.cpp:344`) | match |
| Knock-down trigger | attacker (`+0xc4==4` + `0x00409510`) or `\|v\|>1000` minus walking-half carve-out | `hitter_attacking \|\| (magnitude>1000 && !hitter_walking)` (`:349`) | APPROX — `hitter_walking` always `false` from `contact_rider_pedestrian`; `+0xc4==8`/`+0x2f4` carve-out not evaluated (dead: rider halves never hitter in native pair loop) |
| Knock-down body (scream/state 2/anim 3/`+0x154=0x19`/cap 20000) | lines 18–30 | `:350–358` incl. `0..0x400` ahead, `>0xb4` voice gate, `voices[0]` | match |
| Stagger (state 3/anim 2/queue 5-1-4/voice[1]/`(rand&7)==0`) | lines 32–57 | `:359–368` | match |
| Hit tail push (`sar4`, clamp 0, `+0x128 = v>0?0:0x7f`) | lines 59–67 | `:370–374` | match |
| Contact sounds (`0x1771/0x7d1/0x3e9`, grunt+crash draws) | lines 42–75 | `:384–395` | match |
| Short class (`+0x17c<0x2000 → 0`, ped `0x1e00`) | line 77 | `:378–380` comment + no knock-off | match |
| Elasticity div256(`0x180=192`) | lines 89–93 | `:396–399` | match |
| Nudge gates (`300 & 0x20` crawl / `2000` up / `500` down) | lines 166–178 | `:400–404` | match (original tests `other +0xc4==0x20`; native tests paired `p.state!=2` — equivalent on this pair path) |
| Nudge lateral / ride-over vertical+`+0xf4` | lines 180–188 | `:405–413` | match |
| Contact tail (0x600/`±10000`/`[−1000,30000]` clamps, zeroing) | lines 247–271 | `:414–420` (steer tail skipped, `(void)steer`) | match |
| Lane-clear formula incl. `&~0xf0` mask | `00441500.c` | `:192–200` | match |
| Car-vs-ped separation | `0x004368f0` + `0x00436a40` axis choice + car `0x0044efa0` horn | lateral-only split `depth=(cw+pw−\|Δ\|)+0x800`, `back=depth·1800/(1800+mass)+3` (`:448–456`); horn not invoked | APPROX (documented in code) — ped-side `0x00440760` still applies, so car hits (speed≫1000) knock down |
| Walk-anim hitter (`ai_attack_anim`) | `0x00409510(other)` attack test | `ai_attack_anim(r)!=0` (`:433/:435`) | match-by-construction (shared helper) |

## Capture spec — `city-pedestrian-hit` crossing script (for the Task-4 owner; NOT run)

No captures run this session (read-only + shared-prefix coordination required). Suggested spec:
- Course CITY (dense DZHR peds, signal slices for `0x00441500` red/green coverage); solo human anchor at green.
- Script A (knock-down): player accelerates to speed > 2000 on a straight crossing slice while a
  type-5/6/7 ped is mid-crossing (state 1); expect state→2, anim 3, `+0x154=0x19`, scream iff camera-ahead `0..0x400`.
- Script B (stagger): same geometry at speed < 1000 with no attack anim; expect state→3, anim 2/queue 4/5/1.
- Script C (nudge vs ride-over): upright ped, player at 1500 (nudge: lateral shift only) vs 2500
  (ride-over: `+0x20 += 0x1e00/2`, `+0xf4` kick); downed ped threshold 500.
- Script D (car-vs-ped): free car through a crossing ped; ped knocks down, car continues (horn-only).
- Goldens: per-tick ped `state/anim/+0xec/+0x128/+0x154/voice_tick` + rider `+0x20/+0xf4/+0x28/+0xec`
  for the contact tick ±10; pixel frame at contact tick. RNG note: stagger branch consumes
  `(rand&7)`; sound branches consume 2 draws — anchor RNG once, then free-run per acceptance.

## NOT-changed list (explicit)

- Tracked sources: `src/race.cpp`, `src/traffic.*`, `src/terrain.*`, `src/race_setup.*`, proxy,
  `CMakeLists.txt`, `oracle_sim_test`, `traffic_test`, `race_setup_test` — untouched (not even read beyond `grep`).
- Shared docs: `docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`,
  `docs/plans/10/*`, `docs/plans/32/*` — untouched.
- No builds, no test runs, no captures, no Ghidra project writes, no prefix/DLL changes.
- New/untracked only: this handoff + `analysis/ped-contact-20261016f/` (2 REA mds + logs + `notes.md`).
- Prior handoffs read (not modified): `pedcontact/pedchain/pedhelpers-rea-20261012.md`.

## Remaining work (owner / integrator)

- Task-4 Step-5 acceptance still open: `city-pedestrians` / `city-pedestrian-hit` goldens + pixel
  frame (need working captures; proxy `g_race_started` fix is under shared ownership).
- Two recorded approximations for the porter (both labelled in code/table): walking-hitter
  carve-out (`hitter_walking` always false) and lateral-only car-vs-ped separation (no `0x00436a40`
  axis choice, no car horn on that path).
- Integrator: link this follow-up from the Plan 24 Task-4 index on next refresh; keep
  `analysis/ped-contact-20261016f/` local (disc-derived REA dumps, never commit).

## Live processes / resources

No live REA/Ghidra/Wine processes (both `npx` calls exited 0 and returned). No build locks taken,
no worktrees created, no LSP/database writes, no shared resources held.
