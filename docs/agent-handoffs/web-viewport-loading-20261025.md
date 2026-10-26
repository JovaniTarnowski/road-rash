# web-viewport-loading-20261025

Task: Fit SDL canvas to browser viewport and present original-font loading overlay

Baseline: f23328cbca447f6712c1192c4933c6e3365063f3

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-web-viewport-loading-20261025`
Owner PID: 95735
Resource token: `ecfa4043-18f3-495e-ae6e-a9dc142bc3a4`

Owned files:

- `/Users/archit/Dev/rr/native-port/web/app.js`
- `/Users/archit/Dev/rr/native-port/web/index.html`
- `/Users/archit/Dev/rr/native-port/web/style.css`
- `/Users/archit/Dev/rr/native-port/src/game_main.cpp`
- `/Users/archit/Dev/rr/native-port/src/platform_web.cpp`
- `/Users/archit/Dev/rr/native-port/src/platform_web.hpp`
- `/Users/archit/Dev/rr/native-port/tests/web_hosted_smoke.cjs`

## Progress / evidence

Implemented/pushed 326faf8. User screenshots showed default 640×480 rendering
centered in a large black viewport, 1280×960 overflow behind the footer, and
SDL debug-font loading text. SDL backend source confirms inline CSS dimensions
via emscripten_set_element_css_size; those styles overrode normal stylesheet
width/height. Canvas presentation now uses fitted container-query dimensions
with !important, independent of SDL render-buffer/window-size preferences.
The entire 4:3 frame fits. Footer hides during play, gaining 24 px. Fullscreen
uses the viewport container so the loading overlay participates too.

Original fonts load before launch. A DOM loading panel now uses Badloc headings,
Futura status copy, orange/yellow colors and an animated indeterminate track
(reduced-motion variant). Web-only native stage notifications replace the
SDL bitmap loading drawing. Startup/menu/video/race handoffs clear the stage;
network reads keep loading active while data is pending. Error/closed messages
use the same panel. Native loading behavior stays behind the opposite #ifdef.
No simulation/assets/render-resolution settings were changed.


## Checks / remaining work

- Private web build: build-agent-web-viewport-loading-20261025, passed.
- Private native Debug configure/build and full CTest gate:
  build-agent-web-viewport-loading-native, 106/106 passed, zero failures/skips,
  1291.78 seconds. Logs /tmp/rr-viewport-native-{configure,build,tests}.log.
- Node syntax, bundle allowlist and Git whitespace checks passed.
- Hosted Chromium smoke passed: original-font loading while an asset request
  is held, native stage dismissal, both 640×480/1280×960 inline window sizes
  at 1907×1280, 1280×960, 1440×900, 390×844 and 844×390; complete fitted
  4:3 frame and hidden footer; fullscreen container entry/exit; race rendering,
  keyboard input, nonzero audio, asset hash fault/retry/cache recovery and reload.
- Headless synthetic Escape did not reliably exit browser fullscreen; regression
  uses document.exitFullscreen() to test the actual exit path, not a fake key.
- Main Game probe fetched/rendered INTRO.AVI and reached the Game Mode menu,
  no page or asset errors. Screenshot inspected. Loading overlay clears.
- No client C++ LSP exposed. Compile database preflight healthy; default Apple
  clangd check reported ExtractFunction tweak errors, without source diagnostics.
  clangd --check=src/game_main.cpp --compile-commands-dir=... --tweaks= --log=error
  passed with exit 0. Browser compilation separately checks Emscripten code.
- Evidence: ignored analysis/web-viewport-loading-20261025/ and /tmp/rr-viewport-*.
- Deployment workflow: https://github.com/arkits/road-rash/actions/runs/40266047279
  All CI steps green. Public revision 326faf8f12848bb73747f4c9551f1c48d9452029
  verified. Independent production smoke repeats all sizing/loading/gameplay
  checks and passes; evidence analysis/web-viewport-loading-live-20261025/.

Native/game fidelity limits remain in CURRENT_STATUS; no broader parity claim.


## Live processes / resources

Own local fixture HTTP server: node /tmp/rr-viewport-test-server.cjs, port 8780;
stopped. No Wine/oracle/Ghidra resources touched. Native full gate
finished and holds no process. File claim token above released. No held resources remain.

Concurrent work: after implementation commit, external launcher redesign edits
appeared in web/app.js, web/index.html and web/style.css. Those uncommitted
changes are preserved and excluded from this session’s commits. Production
verification uses the committed 326faf8 build, not those in-flight edits.
