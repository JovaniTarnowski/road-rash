# Review: widescreen, performance and settings-menu bugs (owner playtest, 2026-10-13)

Reviewer: Claude session (read-only analysis plus scratch benchmarks; no source edits).
Code read at `cc87a14`. Line numbers are the **committed** `src/game_main.cpp`.
OMP's uncommitted plan 28 Task 4 moves the same code into `src/race_caches.hpp`
and `src/game_types.cpp`.

## 1. Switching widescreen back to 4:3 leaves the game wide (confirmed by code)

There are two independent widescreen settings:
- `Settings::widescreen` is the Display screen row.
- `Settings::enhancements.widescreen` is the Enhancements screen row; the
  "Enhanced" preset sets it to `match`.

`Game::logical_width()` (game_main.cpp:196) uses
`enhancements.any() ? enhancements.widescreen : widescreen`.

Once Visual Style is Enhanced or Custom (any enhancement on), the Display row
is **ignored**. Turning it Off changes only `settings_.widescreen`
(frontend.cpp:1130), so the race stays 16:9 while the row reads "4:3". The
Enhanced preset (frontend.cpp:1133) also writes both fields, which is how the
two get out of step.

**Fix:** keep one source of truth. Remove `Enhancements::widescreen`, or make it
mirror `Settings::widescreen`, so widescreen stays independent of Visual Style
(it is a display choice, not an effect). Concretely:
- `logical_width()` reads `widescreen` only;
- the preset stops setting it, or sets only `Settings::widescreen`;
- the Enhancements screen's Widescreen row edits `Settings::widescreen`;
- `any()` ignores widescreen;
- `parse_settings` migrates `enh_widescreen` into `widescreen` once.

**Test** (`settings_test`/`frontend_test`): Enhanced preset, then Display →
Widescreen Off, then `logical_width_for(effective) == 320`.

Related: `logical_width_for(Widescreen::match)` is called without display
dimensions (game_main.cpp:198), so **match is always 16:9 (426)**. On this
16:10 MacBook that letterboxes top and bottom. Pass the drawable size.

## 2. Widescreen performance is very bad (measured)

Scratch benchmarks: Release `rr-resource` at `cc87a14`, Napa after 900 ticks,
12 threads, M-series Mac. Frame budget at 60 fps is 16.7 ms.

| Stage, ms per frame | 2× 4:3 | 2× 16:9 | 4× 4:3 | 4× 16:9 |
| --- | --- | --- | --- | --- |
| Original indexed raster (threaded) | 2.5 | 3.0 | 6.7 | 7.8 |
| **Dashboard nearest-palette search** (every mode) | **29.5** | **29.5** | **116** | **116** |
| Enhanced: horizon → RGB | 1.6 | 2.2 | 6.3 | 8.8 |
| **Enhanced: RGB raster (single-threaded)** | **25.7** | **34.0** | **101** | **135** |
| Enhanced: post chain (bloom/vignette/blur) | 5.6 | 7.4 | 22 | 30 |

Causes, in order of cost:
1. **Dashboard palette search** (game_main.cpp:576–590). Every frame, each
   opaque dashboard pixel (188k at 2×, 754k at 4×) is matched against all 255
   palette entries. This alone exceeds the frame budget at 2×, in 4:3 too. In
   the RGB path the result is thrown away (the dashboard is composited again at
   :673), and `Hud::compose` also runs twice per frame. The Hor+ side-panel
   search (:600–618) costs ~0 ms because the dashboard's edge columns are
   transparent. The side panels are therefore never drawn, which may itself be
   a bug.
2. **Full-colour raster ignores the thread pool** (`(void)pool;`, review
   finding 9). Plan 29 Task 3 covers it.
3. **The "Enhanced" visual style turns widescreen on**, so "widescreen is slow"
   is mostly "Enhanced is slow". Widescreen itself adds ~33% to every per-pixel
   RGB stage.

Totals: Original 2× 4:3 ≈ 33 ms (30 fps). Enhanced 2× 16:9 ≈ 75 ms (~13 fps).
Enhanced 4× 16:9 ≈ 295 ms (~3 fps).

**Fixes:**
- (a) Compose the dashboard once per frame and skip the indexed mapping in the
  RGB path. In the indexed path, replace the search with a cached 32K-entry
  RGB555 → index table built once per palette, or better, have the HUD write
  palette indices directly, since its art is already indexed. Expect 29.5 ms →
  < 1 ms.
- (b) Plan 29 Tasks 2–3: a threaded, allocation-free RGB raster.
- (c) Thread or downsample the post chain.
- (d) Cap Match render resolution so the 4× Enhanced surface (6.5 MP) isn't
  chosen by default.

**Acceptance:** the FPS overlay (`show_fps`) at Original 2× 16:9 and Enhanced
2× 16:9 on this Mac, before and after, recorded in VERIFICATION.

## 3. Settings screens don't fit the text (confirmed by code and screenshot)

1. **The title banner shows the long help text in 72-pt Badloc.**
   `value_item` (native_screens.cpp:17) sets
   `item_flag::title_desc | item_flag::description`, so the focused row's long
   description (`d_*`) becomes the title as well as the description. The title
   measurement (frontend.cpp:1411) has no width limit, so `tw` > 640 and the
   centred text is clipped on both sides ("…e picture; sharp keeps texels
   crisp."). Original items with `title_desc` carry short title strings.
   **Fix:** native rows should not put `desc` in the title. Use
   `title_caption` (the row name), or neither flag, so the screen title
   ("DISPLAY") stays. Also clamp the title box to 640 and shrink the font size
   when a title is still too wide.
2. **The lists overflow the screen.** The Display screen has 11 rows on desktop
   (mode, size, scaling, filtering, resolution, widescreen, visual style,
   texture detail, vsync, FPS, back). The Enhancements ("Custom") screen also
   has 11 (10 effects plus back). They are laid out at `tpl.y + i*(tpl.h+4)`
   from the original 0x13 template, so rows 8–11 fall below y = 480. They stay
   focusable but can't be seen: on Display, Texture Detail, VSync, FPS and
   Back; on Enhancements, Vignette, Motion Blur, Widescreen and Back.
   **Fix:** compute `step = min(tpl.h+4, (480 - tpl.y - bottom_margin) / rows)`,
   or split into two columns or pages (Display: screen, Graphics). A
   `native_screens_test` check should assert that every item has
   `y + h <= 480` and `x + w <= 640`, for desktop and mobile.

## 4. Menus are still 4:3: can they be wide? (design note, not a bug)

The menus are the original 640×480 RRI JPEG backgrounds with item rectangles
in 640×480 coordinates, presented with `SDL_LOGICAL_PRESENTATION_LETTERBOX`.
Hor+ is impossible (there is no art outside 4:3), so the options are:
- **Recommended:** draw the menu unchanged at 640×480 in the centre, and fill
  the side bars with a blurred, darkened, stretched copy of the current
  background, computed once per screen change. Cost is near zero, and the
  original layout, hit-testing and the pixel-comparison baseline stay as they
  are. Mouse and touch input must subtract the side offset.
- Stretch to 16:9: distorts the art. Not recommended.
- Leave the pillarbox but use the dashboard-style edge extension, which looks
  worse on photographic art.

This belongs behind the existing Widescreen setting, with Original style
keeping black bars, so that the 640×480 fidelity baseline is unchanged.
