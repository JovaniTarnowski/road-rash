# web-telemetry-20261025

Task: Add Sentry errors and PostHog analytics to web launcher

Baseline: 4ce83e7b49ab22b904061a35256b6d9259feaf98

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-web-telemetry-20261025`
Owner PID: 95735
Resource token: `cde1aec3-7284-4e9e-b702-9e37ff9dad80`

Owned files:

- `/Users/archit/Dev/rr/native-port/web/app.js`
- `/Users/archit/Dev/rr/native-port/web/index.html`
- `/Users/archit/Dev/rr/native-port/web/telemetry.js`
- `/Users/archit/Dev/rr/native-port/tools/build_web.sh`
- `/Users/archit/Dev/rr/native-port/tools/fingerprint_web_bundle.py`
- `/Users/archit/Dev/rr/native-port/tools/prepare_web_deploy.py`
- `/Users/archit/Dev/rr/native-port/tests/web_telemetry.cjs`
- `/Users/archit/Dev/rr/native-port/tests/web_hosted_smoke.cjs`
- `/Users/archit/Dev/rr/native-port/.github/workflows/web-deploy.yml`
- `/Users/archit/Dev/rr/native-port/docs/WEB_TELEMETRY.md`

## Progress / evidence

Pending.

## Checks / remaining work

Pending.

## Live processes / resources

File claims above. Record capture/Ghidra processes here.

## Implementation and checks

Sentry project and isolated PostHog Road Rash project created. Pinned, locally
bundled SDKs, production-only initialization, anonymous explicit analytics,
handled/uncaught/abort reporting, release tags, URL scrubbing, immutable telemetry
bundle and exact ingestion CSP allowances. See docs/WEB_TELEMETRY.md.

- Real SDK Chromium payload/CSP/privacy/blocked endpoint tests: passed.
- Full local hosted gameplay smoke: passed (manifest mismatch, corrupt file retry,
  fonts/cache repair, responsive/fullscreen loading, race rendering, input, audio,
  cache reuse and no uploads/page errors). analysis/web-telemetry-20261025.
- Stale-cache upgrade smoke: passed. Node syntax and diff whitespace passed.
- Reused current validated viewport engine pair for local packaging; CI rebuilds
  current C++ source. No native C++ changes; native gate not rerun.
- First fixture attempt used older Dokku engine pair and failed waiting for SDL3
  audio context; latest viewport pair passed. SDK fixture initially hit bot
  filtering and later required parsing the SDK batch wrapper; both corrected.

Concurrent unclaimed OG image/metadata changes appeared in web/index.html,
tools/build_web.sh and tools/prepare_web_deploy.py during validation. Preserved
in the working tree; stage only telemetry hunks, leaving OG work to its owner.

The OG owner committed only its new image/handoff as 6881864 and explicitly left
four overlapping edits for this integrator (see og-meta-20261026.md). Reviewed
those prepared edits and verified the combined nine-file bundle and both cache
and SDK tests; integrating them separately from cd14c0a. No foreign work deleted.

Bundling also appends installed production SDK dependency licence texts to the
existing LICENSES.txt. npm bundle command and nine-file validator passed after
this packaging addition; application code unchanged.

## Production verification

- Implementation cd14c0a, OG integration e6b6650, SDK notices f7eef0d pushed.
- CI run https://github.com/arkits/road-rash/actions/runs/40270664432 passed:
  fresh engine/SDK build, nine-file asset-free validator, stale cache upgrade,
  real SDK payload tests, Dokku deployment, revision/health and full live hosted
  gameplay acceptance. Superseded run e6b6650 was cancelled before deployment.
- Public revision f7eef0d7097a395a3a9a35ab9cb6af3d74cf0a1e verified.
- Public HTML no-store and CSP permits only the configured two telemetry hosts.
- Independent real browser production probe loaded launcher, clicked Quick Race,
  and received HTTP 200 from PostHog and Sentry ingestion. One explicitly labelled
  intentional integration verification error sent to validate live Sentry routing.
  Local evidence: analysis/web-telemetry-20261025/live-responses.json and
  production-launch.png; normal CI synthetic faults remain intercepted.
- Owned fixture server PID 33883 stopped. No native/oracle resources touched.
- Sentry indexed the probe as ROAD-RASH-1 with one event; marked resolved after
  confirming receipt. This is the intentional test, not a gameplay defect.
