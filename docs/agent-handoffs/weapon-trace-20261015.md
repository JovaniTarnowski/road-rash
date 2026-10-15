# Weapon lifecycle trace (plan 16 Task 3, Step 1 — static only)

- Task/plan: plan 16 `docs/plans/16-gameplay-systems.md` Task 3, Step 1 (trace ONLY).
- Baseline commit: `4c14085` (plus uncommitted tree changes by other workers; untouched by this session).
- Mode: READ-ONLY re: sources. No source edits. No oracle captures run (SIM/pixel
  pipeline owner-debugging; Wine prefix shared). No REA calls: every function below
  already has a project Ghidra export whose control flow is non-suspect, and all
  load-bearing offsets were verified against `analysis/original-disassembly.txt`
  (objdump-style, `coff-i386`). Disassembly outranked pseudocode throughout; no
  conflicts found.
- Reference bytes re-read from `../cdrom/ROADRASH/ROADRASH.EXE` via the port's own
  section map (`.text` VA `0x00401000`→raw `0x400`; `.data` VA `0x00463000`→raw
  `0x61600`). Naive `VA-0x400000` indexing gives WRONG bytes for data tables;
  the values below use the correct map.

## Field map (rider-half object, all dwords unless noted)

| Offset | Role | Established in |
| --- | --- | --- |
| `+0x1c8` | Carried weapon 0..2 (0 fists, 1 club, 2 chain). Mirrors `+0x1d4` at race start; steal source/dest | `0x00405c80`, `0x004096f0`, `0x00432880` |
| `+0x1d4` | Latched attack weapon for the current swing: copied from `+0x1c8`, or 3 = kick. Persists after the swing | `0x00434660:43`, `0x00409460:18/22`, `0x00407d40:36/39` |
| `+0x1cc` | Steal-immunity deadline (tick). Set `tick+300` on the STEALER; zeroed at race start | `0x004096f0`, `0x00405c80:17` |
| `+0x1d0` | Hit cooldown deadline (tick). Set `tick+(5-level)*6` on the VICTIM; damage skipped while `tick <= +0x1d0` | `0x004096f0:106`, `0x00405c80:18` |
| `+0x80`/`+0x81` | Current anim code (byte) / anim frame (byte). Hit resolves on flagged frames or frame 2 | `0x004096f0:20`, `0x004095a0`, `0x00409510` |
| `+0x1e0` | Stats-row pointer (`0x00463c78` row, or malloc'd copy in non-race mode) | `0x00405990:45` |
| brain `+0x168` | Attack target object (0 = none → backhand/shove path) | `0x00409400`, `0x004096f0:37` |

`+0x1c8` vs `+0x1d4`: two distinct words, not one field. `+0x1c8` is the carried
weapon (steal transfers it); `+0x1d4` is per-swing (kick latches 3 without touching
`+0x1c8`). The steal gate reads the victim's `+0x1d4`, so a victim whose last swing
was a kick (`+0x1d4==3`) cannot be stolen from until they swing a non-kick attack.
Native `Rider::weapon` + `Attack::weapon` (`src/race.hpp:57,79,174,178`) already models
this split.

## (a) Fully determined from static evidence

### 1. Starting weapon — table `0x00463c78`, stride `0x40`, dword word 6+level
- `0x00405c80:13` (disassembly-verified): `rider+0x1d4 = *(row + 0x18 + level*4)`,
  then `rider+0x1c8 = rider+0x1d4`; `+0x1cc = +0x1d0 = 0`; stamina refilled from the row.
  Called per pair from `0x00405990` (player pair included), so the player and every
  AI rider start each race from the table — no carry-in.
- `0x00405990:45`: race mode stores a DIRECT pointer to `0x00463c78 + rider*0x40`
  in `+0x1e0` (non-race/attract mode malloc-copies the row). Player pair uses
  rider index `DAT_004b8a10`; AI pairs use slot number 1..N (`0x00405600:108`).
- Table contents (dwords, words 0–15): word 3 = value/condition, word 5 = bike/colour
  pack, words 6–10 = weapon per level 0–4, word 12 = strength, words 13–15 = stamina
  base/max/current. Sample: rider3 words 6–10 = `1,1,1,1,1` (club every level);
  rider4/rider8 = `2,2,2,2,2` (chain); rider1/2/6/7 = all 0 (fists); rider0 all 0.
- Native `original_opponents` (`src/race.cpp:99-111`) reads `word(row + 0x18 + 4*level)`.
  CONFIRMED exact.

### 2. Grab/steal rule — `0x004096f0`, steal block (disassembly lines 11080–11100 + 11100–11160)
All gates conjunctive, evaluated in this order (verified instruction by instruction):
1. Attacker-initiated gate (line 20): proceed unless (attack-anim flag word `& 0x600000 == 0`
   AND attacker frame `+0x81 != 2`). NOTE nuance: a swing whose anim word carries the
   `0x600000` hit flag resolves regardless of frame; without the flag only frame 2 lands.
2. Victim = brain target (`+0x168`) if set, else `0x00409400` wide-search bike
   (mask `+0xc4 == 0x20` required in the backhand path; reach `0x00409c10`; then the
   victim's `+0x1a0` handler is invoked with attacker as arg — a shove through the
   contact path, NOT damage).
3. Steal gates: victim `+0x1cc < tick` (strict; `cmpl` at 4097cb) AND victim frame
   `+0x81 < 2` AND attacker `+0x1d4 == 0` (fists) AND victim `+0x1d4 != 0`
   (`je` skip at 4097fe) AND victim `+0x1d4 != 3` (`je` skip at 409807) AND
   `0x00409510(victim) != 0` (victim actively in an attack anim) AND
   `(rand() & 3) == 3` (`andb $0x3; cmpb $0x3`, one `0x00441750` draw).
4. Effect (atomic, no road object): `attacker+0x1c8 = attacker+0x1d4 = victim+0x1c8`;
   `victim+0x1c8 = victim+0x1d4 = 0`; `attacker+0x1cc = tick + 0x12c` (300);
   sound `0x0043c980`; attacker anim = `dword[0x00464538 + (weapon + variant*4)*4]`
   (`leal (%edx,%ecx,4)` then scaled load at 4098ab–4098b4), queue 2; victim anim 2,
   queue 2; RETURN with no damage. Conservation holds by construction (copy + zero).
- So the "1 time in 4" stand-in (`docs/PROGRESS.md:288`) is exactly the
  `(rand&3)==3` draw, but the native trigger must ALSO check: attacker fists,
  victim armed non-kick, victim mid-attack, victim frame < 2, victim immunity expired —
  and must skip damage + set attacker immunity on success. Native `resolve_hit`
  (`src/race.cpp:1990-1999`) already implements all of this; the PROGRESS.md line is
  stale relative to that code (integrator to confirm and delete in the port commit).

### 3. Reach — `0x00409c10` (disassembly lines 11404–11460, all signed compares)
- side = `(attacker_lat < victim_lat)` (`setg` on `vic-att`, i.e. strict); must equal
  the side byte. Equal laterals → side 0.
- `|dpos| < 0x40` (signed `jge` fail), `|dlat| < reach*0x1000` (`shll $0xc`, signed
  `jle` fail — strictly less), `|dheight| < 0x3000` (signed `setl`). All three are
  `cltd/xor/sub` absolute values. Byte offsets `+0x28/+0x1c/+0x20` confirmed.
- reach: 5 default (fists AND kick=3 fall through), 7 if weapon == 1 (club),
  8 if weapon == 2 (chain). Native `weapon_reach[4] = {5,7,8,5}` (`src/race.hpp:52`)
  CONFIRMED exact, including kick = 5.

### 4. Damage — `0x00409ad0` + `0x004052c0`/`0x004050c0`/`0x004051e0`
- Mult table `0x00465a00`, dword-indexed by weapon (`movl 0x465a00(,%ebp,4)` at
  409b0a): `[256, 320, 384, 64, 4, 8, 25, 64, ...]`. Native `weapon_damage`
  (`src/race.hpp:53`) = `{256,320,384,64}` CONFIRMED exact (entries 4+ are the
  stamina/bike divisor constants, not weapon mults).
- base = `((max<<8)/base)*strength>>8` (signed `imull`, exact `>>8` shifts),
  floored at `strength/2`; scaled = `(mult*base)>>8` with sign-bias `&0xff`
  (no-op for non-negative). Native `src/race.cpp:2006-2009` matches exactly.
- Impact sounds when player involved: weapon 0→`0xb`, 1→`9`, 2→`8`, 3→`(tick&1)+10`.
- Kick: bike damage = `damage/4 + 1` (signed, `>>2` + 1, never 0 since damage > 0),
  reason always 5 (`5 - (q==0)`); non-kick reason 4. `0x004052c0` scales by reason
  (2: ×7/4, 3: ÷8) then `0x004050c0`: stamina max −= 4·dmg (floor base/2), current −=
  64·dmg (floor 0); `0x004051e0`: bike `+0x2f0` −= dmg (floor 0); either hitting 0
  starts the crash via `0x00403d60`. Reason-4 crash anim `0x23`, reason 2/3 → `0x1d/0x21`
  (jump table at 40406b; reason − 2, default `0x21`). Native `knock_off` comment block
  (`src/race.cpp:700-724`) already records this; verified accurate.
- Post-hit: victim `+0x1d0 = tick + (5-level)*6`; `rand()%(6-level)` decides repeat-attack
  requeue; else non-player `rand()&3==2` queues attacker anim 6. RNG draw ORDER:
  steal-draw → modulo-draw → (conditional) &3-draw. Native (1992, 2014–2015) matches.
- Shove `0x00409bd0`: if attacker's current anim `+0x80` is one of the backhand-variant
  codes (`0x00464508`/`0x00464528` for their weapon): victim speed −= 300, attacker
  += 300. Steer kicks: victim `+0x288 ±(3000 or 11000 if attacker kick)`, attacker
  `∓1000`, each clamped to `±(+0x29c)` (`andl $0xffffe890`/`$0x7d0` + adds at
  4099cc–4099f3). Native (2017–2025) matches.

### 5. Attack anim code tables (values static-confirmed; semantic labels in §(b))
32 bytes each, read as 8 dwords (native `read_attack_codes`, `src/race_setup.cpp:18-27`
already spans the pairs correctly):
- `0x004644f8` (right): `22,16,10,26, 24,18,12,26`
- `0x00464518` (left): `23,17,11,27, 25,19,13,27`
- i.e. `[side][backhand][weapon]`: right-normal {22,16,10,26}, right-backhand
  {24,18,12,26}, left-normal {23,17,11,27}, left-backhand {25,19,13,27}.
- `0x00409510` (is-attacking) accepts all four rows; `0x004095a0` (reach side gate)
  accepts backhand rows; `0x00409560` (steal-anim variant) accepts normal rows.
- Steal brandish table `0x00464538`: `22,20,14,26, 23,21,15,27` (index weapon+variant*4).
- Swish sounds: player-think `0x00434660:59-64` plays 7 (club) / 6 (chain) while the
  attack anim (codes 10–13) runs AND strike/club bits still held — NOT at swing start.
  Native plays swish at swing start (`resolve_hit`): known over-approximation, needs
  the animation-gated timing from plan 11 Task 2.

### 6. Crash-drop: NO drop, NO transfer, NO pickup object — weapons are kept
- `0x00403d60` full-disassembly audit (lines 4003–4210): stores touch bike words
  `+0x1c..+0x64`, `+0xec..+0x110`, `+0x118/+0x11c/+0x114/+0x120/+0x124/+0x128`,
  `+0x164/+0x198/+0x19c/+0x1a0`, `+0x12c..+0x160`, `+0x174..+0x17c`, `+0x2f4/+0x2f8`,
  `+0xbc/+0x288/+0x244`, `+0x20`, rider anim `0x1d/0x21/0x23`. ZERO stores to
  `+0x1c8/+0x1cc/+0x1d0/+0x1d4` (grep for `c8 01`/`d4 01`/`cc 01`/`d0 01` in range:
  absent). Victim keeps the weapon through the crash; attacker keeps theirs
  (stolen or not). No world/pickup object is created — no pool, no type, no fields:
  there is nothing to identify because the transfer is always rider-to-rider.
- Remount `0x0040a600` + `0x00404560`: restores stamina current=max, clears `+0x2f4`,
  zeroes velocities. No weapon writes. A remounted rider still holds their weapon.
- Callers of `0x00403d60` (reason codes): `0x004052c0` (combat, reasons 2/3/4/5),
  `0x004049c0:121` (contact), `0x00434660:79` (player dismount key, reason 6),
  `0x00438c70:79` (traction, 6), `0x0043a190:64` (vertical, 6), `0x0043a610:241` (wall, 3)
  and `:285` (6). None pass or create a weapon object.
- Other `+0x1c8` writers: only `0x00405c80` (race-start init), `0x004096f0` (steal),
  `0x00432880` (MULTIPLAYER remote-weapon sync: `table[0x4c4d40 + char_id + 6]`,
  called from the net-packet handler `0x0043e9c0` case 8 — not career).
  (The `0x00440990` `+0x1c8` hit is a car-layout field, unrelated.)
- ANSWER to "pickup (if weapons can lie on the road)": they cannot. No drop-to-road
  object exists in any crash/contact/vertical/wall path. Conservation rule for the
  synthetic test: weapon count over all riders is conserved EXCEPT it is never
  duplicated or destroyed — steal moves 1 unit rider-to-rider; crashes preserve.

### 7. Career carry: NO — three independent proofs
1. Save format is exactly `0xa0` = 160 bytes with NO free field
   (`src/career.cpp:186-199`): checksum[0..4], magic[4..8], head[8..16]
   (player, character, level, qualified, bike, course, place, flags), cash[16..20],
   name[20..40], 10 relation triples[40..160]. 40+120 = 160. A weapon byte cannot hide.
2. Every race rebuilds weapons from the table: `0x00405990` → `0x00405c80` sets
   `+0x1c8 = +0x1d4 = row word 6+level` for the player pair too. Mid-race steals write
   only the live rider-half words, never the table row (no function references
   `0x00463c78` for write except stamina via the aliased `+0x1e0` pointer — and stamina
   is refilled at race start anyway).
3. Shop (`bike_price` at `0x00468fc0`, `level_bike` at `0x004641c8`) trades BIKES only;
   no weapon price/inventory table exists in `read_career_tables`.
- Player starting weapon = rider-table row `DAT_004b8a10`, word 6+level
  (e.g. character mapping to rider3/4/8 starts club/chain; rider1 starts fists).

## (b) Needs capture confirmation (do NOT port on static evidence alone)

1. `napa-weapon-steal`: player (fists) punches an armed, attacking opponent repeatedly.
   Expect: steal ONLY while victim's attack anim runs (victim frame < 2 at resolve time),
   success exactly on `(rand&3)==3` draws in the shared stream, attacker immunity 300
   ticks (no immediate re-steal), victim continues unarmed (`+0x1c8=0` → fists damage).
   Validates gate order + RNG position (steal draw is the FIRST combat draw).
2. `napa-weapon-crash`: armed rider crashes beside another rider (Review Focus 2).
   Expect: victim remounts WITH weapon; no road object appears in scene dumps; rider
   weapon fields in sim dump unchanged across the crash. Validates §(a).6 dynamically.
3. Left/right labeling of `0x004644f8` vs `0x00464518` and of the backhand variant:
   static structure is certain, but which physical side is "left" needs a capture where
   the attacker is known-left vs known-right of the victim (extend `napa-weapon-steal`
   controls or reuse plan 11 Task 2 hit-frame captures).
4. Swish/impact sound timing (anim-gated, control-hold-gated) — needs audio-event
   capture, owned by plan 11 Task 2; listed here so the port doesn't bless swing-start
   swishes as final.
5. `DAT_004b8a10` identity (character vs rider-slot) for the player's starting row:
   static use as `0x00405990` rider index is certain; the menu-side meaning needs a
   career-mode capture (start race as each character, read player `weapon` at tick 0).
6. Backhand-shove branch frequency (`+0x168 == 0` path → victim handler call instead of
   damage): branch structure is (a), but how often AI attacks lack a brain target is
   behavioral — the steal/crash captures will show whether it ever fires on Napa.

## Minimal native edit sketch (for the port owner; NOT applied)
- `docs/PROGRESS.md:288`: delete/replace the "Weapon grabs" stand-in line in the same
  commit that (a) asserts the steal-gate conjunction in `resolve_hit` (already present
  at `src/race.cpp:1990-1999` — verify each conjunct against §(a).2 and the `tick<=`
  vs `<` strictness of both deadlines), (b) adds the `0x600000`-flag OR frame-2
  attacker gate to `fight_hit_frame` (currently frame/tick only; the flag word or its
  native equivalent is the plan 11 Task 2 descriptor word), (c) moves swish sounds
  from swing start to the anim-gated hold pattern of `0x00434660:52-65`,
  (d) adds the synthetic conservation test: N seeded fist-vs-armed attempts →
  total weapons constant, victim-zero iff steal succeeded, attacker immunity observed.
- `src/race.hpp:52-53`: no change — reach and damage tables confirmed exact.
- No crash/drop/pickup/career code: the correct port is the ABSENCE of such code;
  pin with a test that `knock_off`/`begin_crash`/remount never touch `weapon` and
  that `write_save`/`read_save` round-trips without a weapon field.
- Open accuracy item (capture-gated): victim selection `0x00409400`/`0x00449c80`
  nearest-side search (params 8 vs `0x38` windows) is currently "nearest on attacked
  side" in native — keep until the steal capture shows otherwise.
