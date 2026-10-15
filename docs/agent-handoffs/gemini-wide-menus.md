# Handoff: Plan 21 Task 10 — Wide Menus (Blurred Side Fill)

- **Task / Plan**: Plan 21 Task 10 (`menus: blurred side fill behind Widescreen`)
- **Baseline commit**: `fbaeb3a`
- **Worktree**: `/Users/archit/Dev/rr/native-port`
- **Build directory**: `/Users/archit/Dev/rr/native-port/build-agent-gemini-fuzz-fixes`
- **Owned & changed files**:
  - `src/frontend.hpp`
  - `src/frontend.cpp`
  - `src/game_main.cpp`
  - `src/race_screen.cpp`
  - `tests/frontend_test.cpp`
  - `docs/USER_GUIDE.md`
  - `docs/plans/21-enhanced-visuals.md`
  - `docs/agent-handoffs/gemini-wide-menus.md`

## Implementation

1. **Menu presentation (`Frontend::menu_width`, `Frontend::side_offset`, `Frontend::ensure_wide_bg`, `Frontend::draw`)**:
   - When `settings_.widescreen == Widescreen::off` or Visual Style is Original (`!settings_.enhancements.any()`), `menu_width()` returns `640` and `Frontend::draw` outputs the unchanged `640s x 480s` frame (preserving the 4:3 fidelity baseline and black pillarbox bars).
   - When `settings_.widescreen != Widescreen::off && settings_.enhancements.any()`, `menu_width()` returns `logical_width_for(settings_.widescreen, dw, dh) * 2` (`768` for 16:10, `852` for 16:9).
   - `ensure_wide_bg(wide_w)` computes a stretched, `darken555`-darkened, 2-pass separable CPU box-blurred (`radius = 8`) background image once per background/screen change (`set_background`, `open_pause`).
   - `Frontend::draw` renders the `640s x 480s` menu unchanged in the centre (`x = bar * scale`, `bar = (wide_w - 640) / 2`) and fills the left and right side bars from `wide_bg_`.
2. **Input offset (`Frontend::update`)**:
   - When `side_offset() > 0`, pointer events subtract `bar` from `pointer_x`; pointer events inside the left/right side bars (`pointer_x < 0 || pointer_x >= 640`) are ignored.
3. **Menu & pause-menu loop surface (`src/game_main.cpp`, `src/race_screen.cpp`)**:
   - Updated the menu loop and in-race pause menu to pass window dimensions (`set_display_size`), set logical presentation width to `menu_width()`, and size/update the streaming texture with `frame.width`.

## Verification

- Red/green verified in `tests/frontend_test.cpp`:
  - `widescreen == off` and `Original` visual style (`!enhancements.any()`) both render `640x480`.
  - `16:9` + `Enhanced`/`Custom` renders `852x480` with the `640x480` centre pixel-identical to the 4:3 render and non-black blurred/darkened side bars (`left_lit > 1000 && right_lit > 1000`).
  - Pointer input in the side bar (`x = 10`) does not focus a menu row; pointer input at `106 + row3.x + 4` focuses `row3`.
- CTests `widescreen`, `frontend`, `native_screens`, `display`, `settings`: 5/5 PASS.
- `rr-game` builds cleanly with zero warnings.

## Live processes / held resources

None; claim `gemini-wide-menus` released after commit.
