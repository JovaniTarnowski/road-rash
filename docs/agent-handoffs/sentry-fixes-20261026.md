# Sentry browser fixes — October 26, 2026

Task: address recent Sentry issues, storage first. Baseline `7b9ba14`;
shared checkout `/Users/archit/Dev/rr/native-port`. Claims
`ab5cd3a9-b77a-4c76-a3e2-aba674c93520` and
`482d5fcd-bf0f-4a33-8da5-10a28d780e2c`; owner PID 95735.
Build: `build-agent-sentry-fixes-20261026`; native gate:
`build-agent-sentry-native-20261026`.

Owned/changed: `web/app.js`, `web/telemetry.js`,
`tests/web_recovery.cjs`, `tests/web_hosted_smoke.cjs`,
`tests/web_telemetry.cjs`, `tools/fingerprint_web_bundle.py`,
`.github/workflows/web-deploy.yml`, `cmake/emscripten.cmake`,
`src/game_main.cpp`, `src/platform_web.cpp`, these two session handoffs.
OMP owns shared summaries (PROGRESS/VERIFICATION); integrator entries follow.
Preserved OMP's CMake/JPEG/RRI/frontend/oracle edits; did not stage them.

## Progress entry for PROGRESS.md

- ROAD-RASH-4 / -9: OPFS is an optional hosted-asset cache. Blocked, missing,
  or transiently failed storage no longer prevents hosted play. Retry OPFS and
  IDBFS once; if IDBFS restore fails, unmount it and use MEMFS saves for this
  session, with an explicit launcher warning. Existing durable data is retained.
  Local disc import still requires working OPFS. WebKit actually reproduced the
  `UnknownError` at OPFS initialization and launched a race via the fallback.
- ROAD-RASH-3: feature-detect normal/prefixed fullscreen; disable unsupported
  control. Catch both synchronous throws and rejected promises; a denied request
  leaves gameplay visible instead of putting a stopped-game overlay over it.
- ROAD-RASH-2 / -5 and network instances of -8: bounded retries (three attempts)
  for reference/manifest/asset downloads and engine creation. Only exhausted
  startup failures reach error telemetry. Verification remains mandatory; retry
  never accepts corrupt data. Final native asset errors now preserve the JS
  cause instead of always advising disc re-import.
- ROAD-RASH-7: use sessionStorage for the game marker so one independent tab
  neither reports another live game as crashed nor clears that game's marker.
  Genuine unclean exits remain detectable; this does not establish their cause.
- Exit/lifecycle: clear running on main exit and abort; prevent subsequent
  launcher visibility calls into stopped SDL. Handle argv allocation failure and
  synchronous audio-start errors. Save-sync errors are reported as save failures,
  without turning a clean game exit into a game crash.
- ROAD-RASH-8 / -A diagnostics: retain C++ throw-site stacks and Wasm function
  names (`EXCEPTION_STACK_TRACES`, `-g2`). Copy the pinned Emscripten 6.0.10
  `exceptionLast.stack` during main's C++ catch into a fresh JS Error; no native
  exception pointer is retained. Preserve actual asset/engine messages. This is
  verified diagnostics, not a claim that the rare crashes are fixed.

## Verification entry for VERIFICATION.md

- Absolute `RR_WEB_BUILD_DIR=.../build-agent-sentry-fixes-20261026
  tools/build_web.sh`: PASS, code-only bundle validation (9 files). Initial
  relative build-dir attempt misplaced the telemetry output; a second attempt
  rejected the temporary local assets directory. Corrected the invocation and
  moved assets outside the dist before the final successful build. No commercial
  assets enter the bundle or Git.
- `node tests/web_recovery.cjs`: PASS; blocked/missing OPFS, IDBFS failure,
  transient recovery, exhausted downloads, save-only failure, absent/denied/
  prefixed fullscreen, allocation failures, actual game error, stopped lifecycle.
- Real SDK `tests/web_telemetry.cjs`: PASS, including independent tabs and
  restoring a killed per-tab session; all transports intercepted locally.
- `tests/web_bundle_cache.cjs`: PASS; stale fixed-name loader cannot contaminate
  the new hashed JS/Wasm pair. The bundle rewriter now fingerprints the reference
  literal used by the retry helper.
- Local real-engine `tests/web_hosted_smoke.cjs`: PASS after both initial WASM
  fetch paths are deliberately failed, manifest mismatch, corrupt download/cache
  repair, fonts, responsive canvas, fullscreen, driving, audio, cache reuse,
  blocked OPFS+IndexedDB live race, repeated visibility events, deliberately
  triggered C++ exception with named Wasm throw stack, visibility after main
  exits, and asset-read error surviving the C++ boundary.
- Chromium and WebKit live probes: no page errors over 120 seconds of throttle
  with visibility events. Heap settled at 104,464,384 bytes in this scenario.
  These probes do not prove absence of memory bugs or physical Safari/iOS parity.
- Client C++ LSP was not exposed. Refreshed the owned native compilation
  database: preflight healthy (187 entries). `clangd --check` on game_main
  exited 3 with 38 refactoring-tweak failures (ExpandDeducedType/ExtractFunction),
  no compiler diagnostics. Full native compilation and pinned Emscripten
  compile/link plus the real-engine exception regression passed.
- Logs: `/tmp/rr-sentry-{build,hosted,telemetry,cache,probe,stress}.log`;
  browser evidence `analysis/sentry-fixes-hosted/`; private asset staging
  `build-agent-sentry-fixes-20261026/game-assets` (original disc hash-verified).
- Native integration gate: PASS, 106/106 tests, zero skips, viewer smoke and
  all-course sweep; stable-source receipt
  `analysis/agent-gates/sentry-native-final-20261026/receipt.json`. Includes the
  shared checkout's uncommitted OMP JPEG/frontend work. The oracle diagnostic
  retains its documented known divergence; no new simulation acceptance claim.
  Initial serial gate was intentionally interrupted after web-only sources
  changed during its run; final gate used a stable source and parallel CTest.

## Remaining evidence blockers

- ROAD-RASH-8's `vector` C++ failures and ROAD-RASH-A's visibility callback
  signature mismatch have NOT reproduced. The owner also cannot reproduce them.
  Do not resolve these signatures merely on the strength of this patch. Use a
  future event's named throw/callback stack and engine log to identify the code
  path, then reproduce it and add a focused regression. No simulation behavior
  was changed based on guesses.
- Seer analysis for -8 returned HTTP 409: no repository is connected to the
  Sentry project. No connection/configuration was changed.
- ROAD-RASH-1 and -6 are explicit integration/tunnel verification events, not
  product defects. No Sentry statuses were changed by this read-only workflow.
- No original-game parity claim, Android/device test or physical iOS acceptance.

## Live processes/resources

Local HTTP server PID 19019 / exec session 82702 stopped. Native gate,
Chromium/WebKit probes and deployment watcher completed. Both session claims
released after the documentation commit. No held live processes/resources.
No Wine, oracle capture, or Ghidra resources used.

## Deployment verification

Implementation commit `045cbcaa29d164fd7c83e8095e32b838a2657f9f` pushed to
main. [GitHub Actions run 40368414082](https://github.com/arkits/road-rash/actions/runs/40368414082)
completed successfully: clean Linux browser build, code-only package, cache/SDK/
recovery tests, Dokku deploy, exact revision/health check and all extended hosted
smoke cases. Direct public GET of `/revision.txt` returned the implementation
SHA and `/healthz` returned `ok`. Deployment is live at
https://road-rash.archit.xyz. No synthetic failures were sent to Sentry or
PostHog (test transports intercepted). Remaining two unreproduced crash
signatures above are still open; this is not a claim that every issue is fixed.
