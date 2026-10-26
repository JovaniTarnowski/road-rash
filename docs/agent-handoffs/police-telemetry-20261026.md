# Police review and web error tracking (2026-10-26)

Task: owner report "haven't been able to hit police; once close, the web game
crashed with a memory error; nothing in Sentry or PostHog". Baseline bca9d45.
Owned/changed: `web/telemetry.js`, `web/app.js`, `tests/web_telemetry.cjs`,
`docs/WEB_TELEMETRY.md`, `docs/CURRENT_STATUS.md` (police line only), this file.
Not touched: the other session's uncommitted README, PROGRESS, REVERSE_ENGINEERING,
crash/frontend/race sources and frontend test.

## Police state (read-only review)

Live police is implemented (police-live-20261018): `Race::spawn_cop`
(0x00406b80), `find_cop` (0x00407860), `police_think` (0x00407f30),
`remove_cop` (0x004073a0), cop AI branches, siren (0x00435ae0), BUSTED outcome.
Enabled in every non-oracle race (`race_setup.cpp`, `!config.solo`).
CURRENT_STATUS still said "no live cops"; corrected.

Why a player rarely meets one:
- Napa level 1 has no DZHR cop records (level-gated census: 0 cops). Only the
  periodic spawn runs: race tick 4096 (~68 s), then every 4096 ticks with no cop.
- The periodic cop starts 39 slices (0x27) behind the player; DZHR cops (other
  courses, Napa level 3+) start 0x200–0x280 Q8 ahead.
- A cop that falls behind the ring is deleted (`park` -> `remove_cop`).
- Arrest needs the player stopped (speed < 300; |speed| < 1800 in crash state 2), the cop
  within 512 Q8 and speed within 5000.
So at speed on Napa L1 the cop appears behind, then drops out. City L1 has 6
DZHR cops. Parity with an original capture is still open (plan 24 Task 2).

Memory crash probe: ASan+UBSan Debug build (`build-agent-police-asan`, includes
the shared tree's uncommitted edits) ran `police`, `race-screen`, `pedestrian`:
3/3 pass, no reports. `riders_` reserves the cop slot at construction and is
never copied, so appending the cop does not invalidate rider references.
No reproduction yet; the crash cause is unknown.

## Why the crash was not reported

Sentry (7 days) has the Safari `requestFullscreen is not a function` (ROAD-RASH-3),
wasm fetch failures, OPFS SecurityError and Failed to fetch, but no memory
trap. The `(In '...')` message format is WebKit, so the owner likely played on
Safari/iOS. A memory kill there reloads the tab with no JS error, so no SDK can
report it. PostHog had no error tracking: slim SDK with `capture_exceptions:
false` and no ErrorTrackingExtensions.

## Change

- PostHog error tracking: `__extensionClasses: ErrorTrackingExtensions`, the
  bundled `exception-autocapture` script, `capture_exceptions` for unhandled
  errors/rejections (not console). `rrTelemetry.error` also calls
  `posthog.captureException(error, {phase})`. `$exception` frames are URL-scrubbed.
- `engine_log`: last 40 engine stderr lines on Sentry `extra` and PostHog `$exception`.
- Unclean-exit report: localStorage `rr-game-session` while a game runs;
  a load that still finds it reports phase `unclean_exit`.

## Evidence

- `RR_TELEMETRY_OUTPUT=<scratch>/telemetry.js npm run --prefix web build`, then
  `NODE_PATH=.tools/browser-tests/node_modules RR_WEB_DIST=<scratch> node tests/web_telemetry.cjs`:
  PASS. Covers a real Wasm `unreachable` trap from `requestAnimationFrame` in
  Sentry envelopes and PostHog `$exception`, handled error phase/release/engine_log,
  and the unclean-exit report. Full `tools/build_web.sh` and hosted smoke not run
  locally; the deploy workflow runs them.

## Remaining

- Get the device, browser and exact message from the owner. Reproduce: Napa,
  ride until ~68 s, slow down, let the cop catch up.
- A production `unclean_exit` from Safari would point to memory pressure:
  measure wasm heap growth over a race (`ALLOW_MEMORY_GROWTH`, no maximum set).

## Follow-up: PostHog proxy and Sentry tunnel (same day)

- PostHog `api_host` is the owner's managed proxy `https://nazar.archit.xyz`
  (`ui_host` `https://us.posthog.com`); 3feddcd.
- Sentry `tunnel: '/t/e'`; nginx forwards to the fixed project envelope URL
  (see WEB_TELEMETRY.md); the Sentry host is removed from CSP.
- Local evidence in `nginx:1.28-alpine` built from `deploy/Dockerfile` (OrbStack
  Docker 28.5.2): `nginx -t` ok; empty POST `/t/e` -> Sentry 401 through
  verified TLS; GET -> 403; 130 parallel POSTs -> 103 forwarded, 27 x 429; CSP
  `connect-src 'self' https://nazar.archit.xyz`. `tests/web_telemetry.cjs` PASS
  with Sentry captured only at `/t/e`.
- Owner's earlier crash was not a tab kill, so it was most likely a blocker
  dropping both reports; the proxy and tunnel address that.
