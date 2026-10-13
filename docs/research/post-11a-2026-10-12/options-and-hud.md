# Options, input and HUD context

Extend the existing "Options screens and pause" section of
`docs/REVERSE_ENGINEERING.md` after integration. The original Sound dialog
(resource300), Restroom/Graphics/Input screen tables and absence of a menu-style
pause screen were already traced there. Do not redo those findings.

## Rebinding is neither swap nor refusal

`0x0040e580` disables rebind rows and enters capture; `0x0040e690` chooses
the action's binding pair; `0x0040e760` polls a keyboard/joystick choice,
cancels on Escape0x1b, writes the pair and calls `0x0040fbf0` then `0x0040e7b0`.
`0x0040e6d0` restores rows; `0x0040e710` persists the22-byte Controls block
under the original registry key.

Bindings at `0x00465fb8` are eleven2-byte pairs `{joystick_code,key_symbol}`.
`0x0040fbf0` scans all eleven; every duplicate of the newly assigned pair is
cleared to `{0,0}`. Special rows2/3 have alias rows9/10: duplicates involving
an alias clear the paired steering entry as well. Recover the paired axis
flow in `0x0040fc70` before allowing independent conflicting steer remaps.
An unbound action is valid state; a settings loader must not turn its zero
back into a default key. Store invalid codes and unbound distinctly.

`0x0040e5f0` is a **function** initializing a256-byte layout translation map,
not an address holding default binding entries. Source maps are `0x00465fe8`
(default), `0x004660f0` (German), `0x004661f8` (French), `0x00466300` (Italian),
`0x00466408` (Spanish), `0x00466510` (Finnish); layout language id masked0x3ff.
`0x0044fa80` maps Windows virtual keys through that map and then compares the
normalized key byte in each Controls pair. Do not interpret high-bit binding
bytes as SDL keycodes. The census contains initial pairs and control bits;
import the original defaults from EXE at runtime, then translate to SDL actions.

The action table at `0x00476c00` has8-byte entries, first word a control bit,
second word an axis/button policy. Eleven first words are bits0 through10:
accelerate1, brake2, left4, right8, nitro0x10, attacks0x20/0x40/0x80,
dismount0x100 and analog steering modifiers0x200/0x400. Keep the original
attack-side interpretation from `0x00434660`; native labels/mappings need to
state which action they select rather than collapsing all into "punch".

## Analog path

`0x0044f800` combines keyboard and joystick bits. `0x0044f880` supports:
codes1–32 button masks;33–36 POV;37–42 analog directions. Analog activation
thresholds depend on action-table policy: low/high0x6fff/0x8fff for policy0,
otherwise0x3fff/0xbffd. Boundaries are inclusive on the active side. Policy0
calls `0x0044fa00(action, magnitude)`, so analog steering is not just two
digital bits. Trace that amplitude update and `0x0040ad30` alongside thresholds.
Modern SDL dead-zone preferences are native additions; keep test cases for
the original thresholds and converted amplitude separately.

## MiniDash cannot remain a bool

The original cycles full0 → mini1 → off2 → full0; current
`Settings::mini_dash` is `bool`. `0x00446c30` selects layout:

```text
layout VA = 0x00474f50 + (mini_dash + 2*hires) * 0xdc
```

Layout selection and draw branches both use the integer mode. Preserve legacy
settings values0/1 when migrating to an enum/u8 and allow2 explicitly. Unknown
values must fall back per key. Inset/OpponentList can still cause overlays when
the dashboard is off; do not equate off with skipping all HUD drawing.
The current high-layout constructor reads row2 only.

The race window procedure starts at `0x004232a0`; its small decompile is absent,
so dispatch was decoded directly from assembly and EXE jump tables. Message
table `0x00423f80` sends WM_KEYUP/WM_SYSKEYUP to `0x00423345` and
WM_CHAR/WM_SYSCHAR to `0x00423649`. The keyup tables `0x00423fd8`/`0x00423f9c`
send F10 (VK0x79) to `0x004235f8`: Ctrl held (`DAT_00496008`) cycles ChatState,
otherwise MiniDash increments at `0x00423625` and wraps after2. The character
branch compares ASCII0x2f and toggles OpponentList at `0x00423777`–`0x0042378b`:
the key is **slash `/`**. It also clears `DAT_0046afdc`.
`0x0044f800` itself only polls race control words.
Original pause/quit use modal Win32 pumps `0x00443120`/`0x004431d0`.
Pause key VK_PAUSE0x13 reaches `0x004234f7` on keyup. Escape (VK0x1b) uses
the keyup branch at `0x00423356`: during active non-demo play it asks through
`0x004431d0`; on Yes it sets outcome5 at `0x0042346d`. Demo exits directly.
The original mode/finished/network gates must remain distinct from a native
pause-menu Quit race confirmation.
Native plan18 pause is an intentional addition, with no simulation ticks while
open and a TickClock reset on resume; it must retain a return-context stack so
Sound/Display/Controls Back returns to Pause rather than Restroom.

## Needle rendering is a GDI pen

The original needles are drawn in `0x00446c30`, not the resource loader
`0x00416310`. It caches `CreatePen(PS_SOLID, hires+2, COLORREF0x00081172)`:
low width2, high width3; COLORREF translates to RGB#721108. It uses `MoveToEx`
and `LineTo` with integer endpoints computed by signed high32 products of
trig and layout radii. `LineTo` endpoint/cap/join coverage is GDI behavior.
The native3×3-brushed Bresenham line matches neither a measured GDI footprint
nor necessarily its final endpoint.

Use an original component probe drawing synthetic needle vectors on a known
surface at horizontal/vertical/diagonal and shallow slopes, in both widths.
Save mask-only results and compare an exact portable pen implementation.
This isolates the pen from text/GDI-font differences. Font hinting exceptions
must not mask needle pixels. Check full dashboard angles including reverse.

## Low/inset artwork and placement

`0x00416310` selects low horizon directory `Images/Horizons/L`, `LilMeter`,
`mLoDash.BOB`/`kLoDash.BOB`; high uses `Images/Horizons`, `BigMeter`,
`mHiDash.BOB`/`kHiDash.BOB`. `0x004162c0` picks bike artwork family.
Use low artwork, not a downscaled high dashboard. Actor loader's HiResBikers
choice is independent of view resolution; the existing `ActorNodes` constructor
already accepts that flag. Keep low/inset capture registry configuration in an
isolated owned prefix or coordinate access to the shared oracle prefix.

The race's current640×510 window contains a640×480 scene/HUD plus a native
status strip. Original view mapping is320×240 low,640×480 high, or320×240
at(160,120) in a640×480 inset display. Plan18's native Display module must
distinguish that extra strip from original raster coordinates when testing
logical rectangles and when plan19 scales presentation.

## Front-end work not yet validated

Jukebox handlers remain stubs at `0x00445600`/`0x00445730`. The music player
interface should be ready before wiring them. Sound slider changes must update
already-active mixer voices, not only the cached Settings struct. The default
master is100, other channels80; the original dialog commits on OK and offers
no traced preview sound. A native live-apply dialog is a documented difference.

For videos, `0x0041d970`/`0x0041dbc0` are **artwork** selection functions,
not the video trigger table. The code-level59-file audit is:

| Disc files (count) | Trigger and original address | Native status at baseline |
| --- | --- | --- |
| INTRO (1) | Startup, `0x00449150` default path after logo/credits | `Frontend::start` queues Intro; surrounding original startup flow unverified |
| START1–6 (6) | Course choice `0x004213c0`, except network mode3 | Start variants queued; runtime order/RNG unverified |
| WRECK1–6 (6) | Race-end screen0x1c via `0x00416120`; selected by `0x00449150` | `results_video` branch exists; live outcome selection unverified |
| BUSTED1–6 (6) | Race-end screen0x1d, outcome4 | `results_video` exists; depends on live police/outcome integration |
| WIN1–6 (6) | Race-end screen0x1e, qualification mask not0x1f | `results_video` exists; unverified original comparison |
| LEVEL1–6 (6) | Screen0x1e, mask0x1f, level<4 | `results_video` exists; career transition/RNG unverified |
| FINALWIN (1) | Screen0x1e, mask0x1f, level4 | `results_video` exists; full career unverified |
| LOSE1–10 (10) | Race-end screen0x1f, outcome3 | `results_video` exists; outcome/RNG unverified |
| JESSIE, DUEL (2) | Jukebox video popup `0x00445730`, handler `0x004456d0`, pointer table `0x00473f98` | Video-selection popup stub; JESSIE/DUEL are music videos, not duel-race outcomes |
| VIDEO/RAT, SPORT, SUPER (15) | Showroom selection `0x004216e0` → `0x004023e0` with flags1 | Bike video paths queued; interaction/audio continuation unverified |

All result families use `0x00449150`; race outcome2 means qualified/won,
3 lost,4 busted, other non-network outcomes map to wrecked; attract mode0
returns main screen9 without a result video. Qualification bits are updated
before choosing Win/Level/FinalWin. `0x004023e0` consumes `rand()%variants+1`
only when VideoMode is nonzero and variants>0. Video-disabled tests must verify
that RNG call is absent. Flags1 preserve/signal soundtrack for showroom;
other normal videos stop it. Jukebox videos temporarily force VideoMode2 when
it is off, then restore the user's value and redraw Restroom. The script lists
all59 exact disc paths locally; do not commit the asset data.

Attract timer is Win32 timer9/60000ms, started entering main-menu screen9
and killed on leaving it by `0x00421df0`. Timer dispatch/selected course/roster
still needs an original capture; do not confuse demo hold with user-control.

Save byte round-trip remains open:
read/write equality must include original opaque/reserved bytes, not just
known career fields. Capture a real Windows-written save before declaring it.
