# web-widescreen-20261026

Task: web port letterboxing; canvas fill and widescreen scaling (user report:
bars on top and sides with widescreen enabled on road-rash.archit.xyz).

Baseline: bfb23de. Changed files: `web/style.css`, `src/display.cpp`,
`src/game_types.hpp`, `src/game_types.cpp`, `src/race_screen.cpp`,
`tests/web_hosted_smoke.cjs`, `docs/PROGRESS.md`, `docs/VERIFICATION.md`, `docs/WEB.md`.
Did not touch the in-flight `CMakeLists.txt`, `oracle/aweman_proxy.c` or `src/rri.cpp` changes.

## Checks

- Web build `build-agent-web-widescreen-20261026`: passed.
- `tests/web_hosted_smoke.cjs` against the local bundle + `.tools/hosted-game-assets`: 9/9 PASS.
- Native Debug `build-agent-web-widescreen-native`: full CTest **106/106 passed**, 0 skips (254.9 s).
- Manual in-app browser check: see VERIFICATION.md entry.

## Remaining

- Not deployed. Production road-rash.archit.xyz needs the usual Dokku deploy.
- Safari/Firefox/mobile hardware not checked.
- No live processes or held resources (local preview server on :8791 is a session tool).
