# Playable browser release — October 25, 2026

Task: review the unfinished web port, run the current native game in a browser,
and deploy to Vercel. Initial baseline `77b0b7c`; OMP concurrently committed
`b619cb4` and `91bdc91` and continued solo-split work. Their race/oracle edits
and handoffs are preserved and excluded from this commit. Initial registry was
empty; later oracle-capture claims cover only OMP's capture/prefix paths.

## Implementation

Owned: browser CMake branch/toolchain options; `src/platform_web.*`, browser
hooks in `assets.cpp`, driver comment in `game_main.cpp`, one-thread browser
choice in `race_screen.cpp`, no-pthread fallback in `render_threads.cpp`;
`web/`, `tools/build_web.sh`, `tools/prepare_web_deploy.py`, `vercel.json`,
`tests/web_browser_smoke.cjs`, scene-test exception diagnostics, browser docs.
Shared docs updated with narrowly scoped browser entries, preserving OMP work.

The existing game main runs with Asyncify (SDL Present/Delay already yield),
not a separate game implementation. Shared ImportJob verifies the original 341
hashes; folder/ISO ranges stay local. OPFS generations commit after successful
verification; IDBFS persists settings/saves. Assets are read on demand. Launch
creates/resumes SDL audio in the gesture. Browser visibility pushes lifecycle
events. WebGL drawing-buffer retention prevents blank frames across async I/O.
The page launches all five quick-race courses or the full frontend. No game
assets are served or uploaded. Callback rewrite is superseded as a prerequisite.

## Verification

- `tools/build_web.sh`: Release game + validated seven-file bundle, ~3.2 MiB wasm.
- Native owned Debug `build-agent-browser-native`, full build + CTest:
  106/106 passed, zero skips (165.35 s final native run).
- Default wasm/Node `build-agent-browser-tests`: 101 pass, 2 device skips, 1 fail
  of 104. Remaining failure is optional `raster-rgb-alloc` native pre-change
  checksum. Repeat-frame and no-allocation checks pass; golden untouched.
  Full enhanced-pixel parity remains open. Core indexed/hires/sim/oracle gates
  pass. Node-only max memory raised to 4 GiB for the large terrain oracle.
- Browser smoke with Playwright 1.62.1 / installed Chromium, normal autoplay:
  malformed ISO; 341-file folder + actual ISO fixture; cancel + quota failure
  preserve prior import; disc/save storage reload; driving and nonzero audio;
  pause/visibility bridge/fullscreen; five rendered courses; frontend;
  missing-file recovery. Same flow passes generated deployment CSP. Pixel
  checks reject blank course/frontend canvases. Network shows same-origin
  GETs for code/metadata only, no game data requests or JavaScript errors.
- Local evidence `analysis/browser-20261025/`; logs `/tmp/rr-browser-*.log` and
  `/tmp/rr-wasm-*.log`. Disc/ISO fixtures are in ignored build trees only.
- clangd assets check zero errors; C++ browser compile and JS/shell/Python syntax
  checks pass. No Android/device/Firefox/Safari/gamepad/touch/full-career-save
  acceptance or new oracle capture in this slice.

## Deployment

Linked existing `arkits/road-rash` project, ID
`prj_4cjPiJ0e7264UZDdwXz2l5v2Q6gf`, team `team_RP8alJ5QDxxRRWh3zopH1RHd`.
Vercel connector requested reauthentication; authenticated CLI 63.1.2 used.
Build produces `.vercel/output` for `vercel deploy --prebuilt --yes` preview;
remote source builds need emsdk and are not the release path. Release URL/status
will be recorded below after upload. Browser storage is per origin; use a stable
origin for subsequent releases to retain imports.

## Remaining / resources

Optional full-colour checksum difference, Firefox/Safari/mobile/gamepad/touch,
long-session performance and complete career original-save round-trip remain
unverified. Original native fidelity gaps remain. Optional web threads and the
callback rewrite are separate follow-up work.

Owned static servers on localhost ports 8765/8766 are temporary verification
resources; stop them at completion. No Wine/Ghidra/shared-prefix resources held.
