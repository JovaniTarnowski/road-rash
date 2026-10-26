# Browser error tracking and analytics

Production https://road-rash.archit.xyz uses:

- Sentry project `archit-khode/road-rash`, JavaScript browser SDK 11.9.0.
  https://archit-khode.sentry.io/issues/?project=4512320659914752
- PostHog project `Road Rash` (688403), browser SDK 1.438.21, US ingestion.
  https://us.posthog.com/project/688403/activity/events

`web/telemetry.js` uses public ingestion keys. No management token belongs in
browser code or the repository. Dependencies and build tool versions are pinned
in `web/package-lock.json`; `tools/build_web.sh` runs `npm ci` and bundles them
into one fingerprinted same-origin script. Node 22+ and npm are required.
Sentry release is `road-rash@<build Git SHA>`; environment is production.
The app runs with no-op hooks if telemetry fails to load, and SDK calls cannot
prevent playing. Localhost and other hostnames never initialize either SDK.

## Events

| Event | Properties / meaning |
| --- | --- |
| `$pageview` | One per page load |
| `launcher_ready` | `source`: hosted or local disc |
| `game_start` | `mode`: main_game or quick_race; quick race `course`: 0–4 |
| `game_exit` | Normal main() return; mode and duration_ms |
| `launcher_exit` | Exit to launcher button |
| `connection_retry` | Retry connection button |
| `disc_import_start` | source: iso or folder |
| `disc_import_complete`, `disc_import_cancelled` | Import outcome |
| `fullscreen` | Successful fullscreen entry |
| `web_error` | phase only; details go to Sentry |

Every event includes release. A start event means the user requested launch,
not that the race rendered or finished. No per-frame or simulation hooks added.

Sentry captures uncaught exceptions/rejections plus handled initialization,
import, fullscreen, game execution and Emscripten abort failures with phase tags.
JS stack locations remain available; this does not add C++/Wasm source maps or
promise native symbolication. Recovered cache misses are intentionally not errors.

PostHog uses a pseudonymous localStorage ID without identified person profiles.
Autocapture, replay, surveys, external extensions, feature flags and exception
capture are disabled. Explicit event properties are allowlisted. Disc bytes,
saves, keystrokes and user file objects are never passed to tracking.
SDK browser/device/session metadata is still present. Query strings and URL
fragments are stripped from analytics URL fields, Sentry request URLs and stack
frame URLs. Sentry default PII and console/network/interaction breadcrumbs are
disabled; error messages and stack traces still describe the failure.

CSP permits only the two exact HTTPS ingestion hosts in connect-src. Scripts
remain same-origin. Ad blockers may suppress delivery; gameplay still works.

## Verification

`tests/web_telemetry.cjs` exercises the real bundled SDKs in Chromium with both
services intercepted: handled/uncaught errors, phase tags, PostHog events and
properties, URL scrubbing, CSP, disabled localhost and blocked endpoints.
PostHog filters webdriver traffic, so this payload test simulates a human UA.
`tests/web_hosted_smoke.cjs` intercepts telemetry at the context level so its
intentional manifest/download faults never pollute production data. Its upload
check allows POST only to these two ingestion hosts. Game assets remain GET-only.
Both run in the deployment workflow; stale-cache and full hosted gameplay gates
remain required. Normal PostHog bot filtering stays enabled in production.

References: https://posthog.com/docs/libraries/js and the pinned SDK type/source
options; https://docs.sentry.io/platforms/javascript/configuration/options/.
