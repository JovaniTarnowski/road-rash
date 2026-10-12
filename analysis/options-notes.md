# Options trace (plan 18 Task 1) — research notes

DOCS-ONLY slice. No source files modified. All EXE reads below are read-only
parses of the reference binary
`/Users/archit/Dev/rr/cdrom/ROADRASH/ROADRASH.EXE`
(SHA-256 `73d7f974…a60f18a2`, image base `0x400000`, RVA→file-offset mapped
through the PE section table in throwaway Python; no binaries built, no
`build/` touched). Disassembly line numbers refer to
`analysis/original-disassembly.txt`. Decompiles are
`analysis/decompiled/<addr>.c`. ENU captions come from
`TEXT/ENU.DLL` STRINGTABLE resources (true string id = block*16+k, i.e. the
loader in `src/pe_strings.cpp` `load_game_strings` with base 0).

## (a) REVERSE_ENGINEERING.md subsection text (proposed, verbatim)

```md
## Options screens and pause (October 12, 2026, plan 18 Task 1)

All tables below were dumped straight from the reference EXE (image base
`0x400000`): screen table `0x0046a390` (0x24 entries x 0x14 bytes), run-time
counts from `0x00474668`/`0x0047466c`/`0x00465fb0`/`0x00474670`
(`0x00423881`), item rows 0x38 bytes (`src/frontend.cpp` `read_item`
field order). Captions/descriptions are `TEXT/ENU.DLL` string ids.

### Restroom (0x10): 13 items at 0x00473860, title slot 6 ("Restroom")

| # | target | caption/desc | handler | flags | geometry (x,y,w,h) |
| --- | --- | --- | --- | --- | --- |
| 0 | 0 | 48/49 Der Panzer Klub | `0x00445150` | 0xc | (0,114,0,28) |
| 1 | 0 | 50/51 Olley's | `0x00445370` | 0x20c | (0,142,0,28) |
| 2 | 0 | 52/53 Schmooze | `0x00445310` | 0xc | (0,170,0,28) |
| 3 | 0 | 54/55 Bulletin Board | `0x00445460` | 0xc | (0,198,0,28) |
| 4 | 19 | 56/57 The Street | none (enters screen) | 0xc | (0,226,0,28) |
| 5 | 0 | 58/59 Rat Bikes | `0x00445490` | 0xc | (0,254,0,28) |
| 6 | 0 | 60/61 Sport Bikes | `0x00445560` | 0xc | (0,282,0,28) |
| 7 | 14 | 62/63 Super Bikes | `0x00445390` | 0xc | (0,310,0,28) |
| 8 | 17 | 64/65 Load Game | `0x00445590` | 0x40c | (0,338,0,28) |
| 9 | 0 | 82/83 Sound Mix | `0x00401df0` | 0xc | (0,366,0,28) |
| 10 | 18 (0x12) | 84/85 Configure Input | `0x0040fd90` | 0xc | (0,394,0,28) |
| 11 | 0 | 86/87 Done | `0x004453c0` | 0x800c (Escape) | (0,422,0,28) |
| 12 | 6 | 8/9 Go Home | `0x0041f4f0` | 0xc | (0,450,0,30) |

Width 0 = title-box width at draw time (`0x004459f0`); nav chain is a plain
0-1-2-...-12-0 up/down loop. The sound mix and Configure Input live here,
NOT under Graphics Options.

### Jukebox (0x11): 9 slots at 0x00474048, title slot 80, runtime count 9

Items 0-5: captions 225-230 (Hammerbox, Monster Magnet, Paw, Soundgarden,
Swervedriver, Therapy?), all handler `0x00445600`, flags 0xc, (0,y,0,42)
y = 134..344 step 42. Item 6: caption 431 ("Avail") desc 496, handler
`0x00445730`, (0,386,0,42). Item 7: caption 86 target 16 (Done), flags
0x800c, (0,428,0,52). Item 8 is an all-zero dead slot (cap 0, no handler,
disabled, (0,114,0,20)) that the count of 9 still covers.
`0x00445600` opens a `0x004224b0` popup from the per-artist tables
`0x00473b38`/`0x00473be0`/`0x00473c50`/`0x00473d30`/`0x00473e48`/`0x00473ef0`
(track captions 241-254, handler `0x004455b0`); `0x00445730` formats two
"Now Playing" lines (`0x0049e088`/`0x0049e018`) then a 3-row popup at
`0x00473fa0`. Jukebox never leaves screen 0x11 (both handlers return 0).

### Configure Input (0x12): 22 slots at 0x00465ae0, title slot 84

Items 0-8: action rows, captions 256-264 (Accelerate, Brake, Turn, Lean,
Nitro boost, Swing, Kick, Backhand, Dismount), handler `0x0040e580`
(rebind flow, NOT traced), flags 0, (0,120+30k,240,30). Items 9-10:
captions 382/383 ("Keyboard defaults"/"Joystick defaults"), handler
`0x0040fdf0`. Item 11: caption 86 target 16 (Done), flags 0x8000.
Item 12: zeroed spacer. Items 13-21: current-key labels, captions 160-168
(all "*" in ENU), flags 0x10000, disabled, (260,120+30k,360,30).
`0x0040fd90` (Restroom item 10's handler) is INIT, not the screen: it calls
`joyGetDevCapsA` and runs `0x0040e7b0` over the 0x2c-byte table at
`0x00465fb8`, publishing name pointers at `0x00475378+0x280`; it returns
the item's target field (`param_1+4`), so activation then enters 0x12.
`0x0040e5f0` is the keyboard-layout default copier (layout pick via
`GetKeyboardLayoutNameA` -> one of `0x00465fe8`/`0x004660f0`/`0x004661f8`/
`0x00466300`/`0x00466408`/`0x00466510`, 0x40 bytes into `0x004c5d90`).
Rebind conflict rule and `0x0040e580`/`0x0040fdf0` bodies: NOT traced.

### Graphics Options (0x13): 8 slots at 0x004744a8, title slot 394

Only three live options: item 0 Resolution (cap 72, `0x004457b0`,
(0,134,180,48)), item 1 Bikers (cap 396, `0x004458a0`, (0,182,180,48)),
item 3 Videos (cap 431, `0x00445960`, (0,278,180,48)); item 6 Done -> 0x10
(0,422,180,58, flags 0x800c). Items 2, 4, 5 and 7 are zeroed/disabled
placeholder slots the count of 8 still covers. No sound/controls items.
`0x004457b0` opens the resolution popup `0x00474240` (choices 88/385/89 via
setter `0x004457f0`: 1 = low fullscreen `HiRes=0,Inset=0`, 2 = low inset,
3 = high fullscreen); `0x00445900` stores biker choice-1 to `HiResBikers`
`0x00475384`; `0x00445960` opens the videos popup `0x004743c8`.

### Sound mix: Win32 modal dialog, NOT a menu screen

`0x00401df0` (`analysis/decompiled/00401df0.c`) brackets
`DialogBoxParamA(0x12c)` (resource id 300) with `0x0041f490`/`0x0041f4c0`
(menu -> dialog transition). The template (id 300, 47 items) carries seven
`msctls_trackbar32` sliders `0xbbb`-`0xbd9` plus `%` statics, value edits
`0xbb9`-`0xbd5`, a mute checkbox `0xbdc` and OK/Cancel (1/2).
Channel table `0x00463038` (7 rows x 6 dwords: label id, slider id,
checkbox id, value-text id, default, spare): defaults 100/80/80/80/80/80/80
for Master/Music/Effects/Engines/Others?/Sirens?/Interface (labels
210/212/213/214/215/216, values at `+0x10`; mute bit `0x00463030`,
checkbox `0xbdc`).
`0x00402040` sets every slider range to 0-100 (`TBM_SETRANGE 0x406,1`,
`0x640000`), page size 10 (`TBM_SETPAGESIZE`), position from the channel
value (`TBM_SETPOS 0x405`). `0x00401ee0` (WM_INITDIALOG) shows `100-value`.
`0x004022d0` (HSCROLL/VSCROLL) writes `100-pos` back to the value cell and
posts thread message `0x004c8018` (live mixer update). IDOK (`0x004020a0`,
`param_2==1`) commits all seven + mute and writes 0x40 bytes at
`0x00463034` to the `Mixer` registry value; Cancel posts `0x004c800c`.
No preview click/sound call appears on the scroll/commit paths (traced
`0x00401e20`/`0x00401ee0`/`0x004020a0`/`0x004022d0` only).

### Controls at runtime: `0x0044f800` -> `0x004b7ed0`

`0x0044f800` ORs keyboard bits from `0x0044fa80` (256 scancodes through the
`0x004c5d90` key -> action table against `0x00465fb9` bindings and the
`0x00476c00` bit table) with joystick bits from `0x0044f880` (buttons,
POV/axes with dead zones `0x3fff/0xbffd` vs `0x6fff/0x8fff`). Bit order at
`0x00476c00` (from prior trace): 0x1 accel, 0x2 brake, 0x4/0x200 steer,
0x8/0x400 lean, 0x10 nitro, 0x20/0x40/0x80 attacks, 0x100 dismount.
Default key rows at `0x00465fb8` (22 u16) start
`0094 0096 0093 0084 006e 0085 000d 0020 0064 ...`. Polled once per frame
in `0x00442e10` (`0x442ea8: call 0x0044f800`, result to `0x004b7ed0`).

### In-race pause: NO pause screen; modal dialogs over a pumped race

- The frame driver `0x00442e10` has no pause branch: it polls controls,
  runs `0x0044b2e0(steps)` (which calls the per-tick `0x0044b3b0`), and the
  only exits are the quit flag `0x004753b4` and the video flag `0x004753c0`
  (disassembly `0x442e10`-`0x442f50`). A frozen "pause menu" state does not
  exist.
- `0x00443120` is the modal pause box: formats text, and if menus are up
  draws it via `0x00423240`+`0x0042f920`, else `0x0041f490` +
  `MessageBoxA(...,0x30)` + `0x0041f4c0`. Default text is string 317
  ("Game paused.\nHit any key to continue.", `0x00475378+0x5cc`). Its live
  callers are audio/error paths (`0x004027f4` slot 0x700, `0x00410486`+
  slots 0x72c/0x728) - NOT a pause key.
- `0x004431d0` is the modal race pump (quit-confirm "Hit <Escape>..."):
  stops sound (`0x00436260`), sets `0x004753b8=1`, renders each spin
  (`0x004439f0`), and consumes `PeekMessageA`: joystick button or any
  key-up dismisses with code 2; with text pending, Escape (`0x1b`) returns
  1 and any other key returns 2. Callers: menu quit prompts
  (`0x004233e9` slot 0x4f0 = string 316, `0x0042351d` slot 0x4f4,
  `0x0042358f` slot 0x5f0).
- While that pump spins, two keys act instead of dismissing: F10 (`0x79`
  key-up) cycles `MiniDash` `0x00467a98` 0->1->2->0; PrintScreen (`0x2c`
  key-up) sets screenshot flag `0x004c5ba0=1`. Menu-side key dispatch
  (`0x00422340` jump tables) has parallel cases: one increments
  `0x00474f48`-style view state, one increments `MiniDash` with the same
  0-1-2 wrap (`0x423625`), one toggles `OpponentList` `0x004753a8`
  (`0x423777`: 0->1, else stays 1 - i.e. menu key turns the list ON only).
  The exact menu keys feeding those cases were NOT decoded (jump tables at
  `0x00423fd8`/`0x00423f9c`, key = `ebx-0x10`).
- `MiniDash` consumers: `0x00446899`/`0x004468e1` skip tach/speedo math when
  nonzero; `0x00446c76` skips the name-bar draws when `==2`. So F10 cycles
  full -> mini -> off.
- Escape ends a race: NOT traced to a quit code. `0x00421df0(n)` posts
  `WM_QUIT(n)` and the main loop `0x00448200` maps message codes
  `0x100`-`0x106` through the byte table at `0x0044847c`; which code the
  race-quit path posts, and which screen it lands on, is NOT traced.
```

## (b) Corrections to the screen map in 18-settings-and-options.md

Current map (lines 55-62) says:

```
Restroom (0x10) ──► Jukebox (0x11)
               ├──► Configure Input (0x12)
               └──► Graphics Options (0x13)
                        ├──► Display (0x24)       native, Task 5
                        └──► Sound (0x25)         original sound mix 0x00401df0 if it is a screen; else native, Task 6
Race pause ──────────► Pause (0x26)               native, Task 8: ...
```

Corrections (Task 1 Step 4):

1. **Sound is a Win32 dialog off the Restroom, not a screen under Graphics
   Options.** `0x00401df0` runs `DialogBoxParamA(0x12c)`; there is no sound
   screen id. The map's `Sound (0x25)` branch under Graphics Options is
   wrong about provenance (the native-screen fallback it already
   anticipates IS the right call - Task 6 builds `native_sound` with the
   dialog's 7 sliders in dialog order, ranges 0-100, defaults
   100/80/80/80/80/80/80; document the menu-look as the deviation). Fix:
   move `Sound (0x25)` to hang off `Restroom (0x10)` (same level as
   Jukebox/Configure Input/Graphics Options), annotated
   "native look-alike of Win32 dialog 300".
2. **No original in-race pause screen exists** (frame driver `0x00442e10`
   has no pause branch; pause = modal `MessageBoxA` `0x00443120`, quit =
   modal pump `0x004431d0`). The plan's Task 8 premise ("the original ...
   in-race pause screen") is refuted; `Pause (0x26)` stays purely native,
   and its QUIT RACE outcome code still needs the untraced Escape path
   (open question below). The map's "Race pause ──► Pause (0x26)" line is
   fine as a native addition but should drop any implication it mirrors an
   original screen.
3. **Jukebox item count is 8 live + 1 dead slot** (count 9 covers an
   all-zero 9th row); Graphics Options is 4 live + 4 dead slots. Tests that
   assert item counts from the EXE must expect 9 and 8 respectively, not
   8/4. The plan's "Graphics Options buttons" geometry template = item 0
   `(0,134,180,48)` still holds.
4. **Controls handler `0x0040fd90` is init, not the screen** - Task 7's
   "Port screen 0x12 exactly" is unaffected, but its "Starting point"
   should cite the rebind handler `0x0040e580` and reset `0x0040fdf0`
   instead (both untraced here).
5. Global-constraint registry keys confirmed present in code:
   `HiRes`/`Inset` writers in `0x004457f0`, `HiResBikers` in `0x00445900`,
   `Mixer` block write in `0x004020a0`, `MiniDash` `0x00467a98`,
   `OpponentList` `0x004753a8` toggle in menu dispatch. `VideoMode`
   `0x0047538c` read in `0x00445960` confirmed.

## How each claim was confirmed

- Menu tables: direct dump of `0x0046a390` + count globals + item arrays
  from the reference EXE with RVA mapped through the PE section table
  (throwaway script, output pasted above in condensed form). Geometry,
  nav, flags, handlers are file bytes, not inference.
- Captions: `TEXT/ENU.DLL` STRINGTABLE blocks decoded with the game's own
  block*16+k id rule; every cited id was read back except the dead-slot
  cap 0 (absent = empty, as `LoadStringA` returns 0).
- Sound dialog: `analysis/decompiled/00401df0.c` (DialogBoxParamA 0x12c);
  template parsed from the EXE `.rsrc` DIALOG directory (ids 100/200/300/
  400/401/402/500; id 300 = 47 items incl. seven `msctls_trackbar32`
  `0xbbb`-`0xbd9`); slider semantics from `00402040.c` (range/page/pos
  messages), `004022d0.c` (scroll), `004020a0.c` (OK commit + `Mixer`
  registry write), `00401ee0.c` (init). Channel table `0x00463038` read
  from the EXE (7x6 dwords).
- Controls: `0040fd90.c`, `0040e5f0.c` decompiles + disassembly of
  `0x0044fa80` (key matrix scan) and `0x0044f880` (joystick thresholds);
  default rows at `0x00465fb8` read from the EXE.
- Pause/quit: disassembly of `0x00442e10` (no pause branch), decompiles
  `00443120.c`/`004431d0.c`, key constants `0x79`/`0x2c`/`0x1b` in the pump
  loop, `MiniDash` sites `0x00446899`/`0x004468e1`/`0x00446c76`/menu
  `0x00423625`, `OpponentList` site `0x00423777`, quit strings 316/317 at
  `0x00475378+0x5cc` region via callers.

## Open questions (explicitly NOT found)

1. Which menu keys feed the `MiniDash`/`OpponentList`/view dispatch cases
   in `0x00422340` (jump tables `0x00423fd8`/`0x00423f9c`, key `ebx-0x10`).
2. Rebind flow `0x0040e580`, conflict rule, reset `0x0040fdf0`, registry
   storage of bindings.
3. Whether a scroll tick in the sound dialog plays any preview (no call
   found on the traced paths; not disproven for the mixer thread side).
4. How Escape ends a race: the posting call site, the `WM_QUIT` code, and
   the landing screen (byte table `0x0044847c` for codes `0x100`-`0x106`
   partially read).
5. No in-race `OpponentList` toggle found (only the menu-side ON-only case
   at `0x00423777`); the plan's "original in-race toggles" premise holds
   for the mini dashboard (F10) only.
6. `0x0041f490`/`0x0041f4c0` (menu<->dialog transition) bodies not read.

## Suggested commit message (DO NOT COMMIT - owner merges)

`docs: original options screens, sound mix and pause behaviour`
