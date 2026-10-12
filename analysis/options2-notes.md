# Options/HUD slice notes (October 12, 2026) — doc deltas for the session-28 trace

Reference EXE SHA-256 `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(verified against the mounted disc before reading bytes below).

## 1. Key rebinding storage + conflict rule (traced, now implemented)

- Binding table: 11 rows x u16 at `0x00465fb8` (`analysis/decompiled/0040fd90.c`
  init loop, `0x0040fbf0.c` conflict scan bound `iVar2 < 0xb`).
- Registry persistence: `0x0040e710` writes 0x16 bytes under the `Controls` value
  (`RegSetValueExA(..., &DAT_00465fb8, 0x16)`); `0x0040e5c0` reads them back with
  `RegQueryValueExA(..., &DAT_00465fb8, &local_4=0x16)`.
- Conflict rule (`analysis/decompiled/0040fbf0.c`, first lines): for every row
  `i != edited` with `param_1[1] == pbVar1[1] && *param_1 == *pbVar1`, both bytes
  are zeroed and `0x0040e7b0` refreshes the label — the previous action is
  **unbound, not swapped and not refused**. (Rows 9/10 and 2/3 have extra
  label-refresh side effects only.)
- Defaults read from the EXE (low byte first):
  - keyboard `0x00466618`: `0094 0096 0093 0084 006e 0085 000d 0020 0064 0095 0081`
  - joystick `0x00466630`: `0025 0027 0028 9300 6e00 0001 0002 2000 9400 0026 9500`
- Reset `0x0040fdf0`: caption `0x17e` copies 11 words from `0x00466618`
  (keyboard), `0x17f` from `0x00466630` (joystick), then `0x0040e7b0` per row,
  `0x00421cf0(0)`, `0x0040e710()` to commit.
- Session-28 note said "conflict rule NOT traced"; the decompile above closes it.
- Native: `Settings::controls` + `custom_controls` (hex `controls=` line),
  `default_keyboard_controls()` / `default_joystick_controls()`,
  `apply_rebind()` (throws on action >= 11), `read_control_tables()` (bounds-checked),
  `Frontend::rebind_key()` / `reset_keys()`. Frontend test covers EXE read-back,
  range check, store, conflict-unbind, and both resets.

## 2. Analog-control path (traced, documented)

- `0x0044f880` joystick branch (`analysis/decompiled/0044f880.c`): binding byte
  `< 0x21` = button bit; `> 0x24` = axis/POV with per-row analog flag at
  `0x00476c04[row*8]` selecting dead zones `0x3fff/0xbffd` (analog) vs
  `0x6fff/0x8fff` (digital), deflection scaled by `0x0044fa00`.
- Native `steer_dead_zone = 12000` keeps the existing digital-threshold feel;
  `src/input.hpp` comment cites the original edges. Full analog scaling is open.

## 3. MiniDash 0-1-2 modes, F10 cycle (traced, now implemented)

- `MiniDash` at `0x00467a98` is a 3-state value, not a bool (corrects the old
  `Settings::mini_dash` bool): F10 key-up (`0x79`) in the `0x004431d0` pump
  cycles 0->1->2->0.
- Consumers: `0x00446880`/`0x004468e0` skip tach/speedo math when nonzero;
  `0x00446c30` skips the third-meter/name-bar draws unless mode 0 and draws
  nothing at all when mode 2.
- Layout stride: `0x00446c30` computes `(MiniDash + HiRes*2)*0xdc` at `0x00474f50`
  — strides 0/1 are the low (320-wide) full/mini rows, 2/3 the high rows.
  Measured stride heads: lo-full `(32770, 0, 222, 86, ...)`, lo-mini
  `(32770, 0, 202, 79, ...)`, hi-full `(32770, 0, 446, 168, ...)`, hi-mini
  `(32770, 0, 404, 158, ...)`.
- Native: `Settings::mini_dash` is now `int` 0-2 (parse `0..2`); `DashMode`
  enum; `Hud::compose(..., mode)` (default full, keeps the game_main call site
  compiling); `Hud::cycle_mini_dash()`; `Frontend::cycle_mini_dash()` /
  `enable_opponent_list()` (ON-only per `0x00423777`). hud_test covers cycle,
  mini (no needle pixels), off (empty frame), and low-mode composition.

## 4. Needle-pen documentation (documented, NOT ported — per assignment)

- `0x00446c30` needles: `CreatePen(PS_SOLID, HiRes?3:2, 0x81172)` cached per
  resolution at `0x0049e0b0`, `MoveToEx`/`LineTo` with `(sin*dw>>32)` endpoints
  via `0x0044f510`. Native keeps the 3x3-block Bresenham in colour `0x721108`;
  the header comment cites the pen. No pixel comparison against GDI output exists.

## 5. Low-mode dashboard assets (traced, now implemented)

- `0x004457f0` choices 1/2 select low fullscreen/inset; low dashboards are
  320-wide: RAT/SUPER `MLODASH/KLODASH.BOB` 320x60 (19200 B), SPORT 320x61
  (19520 B); high are 640-wide (RAT/SUPER 119 rows, SPORT 121).
- `LILMETER.BOB` is 3040 B = 95x32, the half-stride partner of `BIGMETER.BOB`
  (190x64); it is NOT a prefix of the big meter (measured: no match at any offset).
- Native: `Hud` loads both sets, validates sizes, `set_low_mode()` switches art
  + stride + layout scale (low path upscales 2x so compose stays 640x480).

## Open questions (unchanged from session 28 + one new)

1. Menu keys feeding the MiniDash/OpponentList dispatch (`0x00422340` jump tables).
2. Sound-dialog scroll preview; Escape race-quit posting path.
3. No in-race OpponentList toggle (menu ON-only confirmed).
4. NEW: mini-mode numeric-speed rect (layout stride 3 offset 21) is plausible but
   not pixel-verified against the original; low-mode 2x upscale is a native
   deviation (original renders 320-wide), also unverified.
