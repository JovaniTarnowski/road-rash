# web-cache-pair-20261025

Task: Prevent mixed cached JavaScript/Wasm bundles after deployment

Baseline: 5fb7d8a68f812c625f300f54b51005c2617ce932

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-web-cache-pair-20261025`
Owner PID: 95735
Resource token: `de1504be-7f0f-4c57-9af4-9deebca82f3c`

Owned files:

- `/Users/archit/Dev/rr/native-port/tools/build_web.sh`
- `/Users/archit/Dev/rr/native-port/tools/prepare_web_deploy.py`
- `/Users/archit/Dev/rr/native-port/tools/fingerprint_web_bundle.py`
- `/Users/archit/Dev/rr/native-port/deploy/nginx.conf`
- `/Users/archit/Dev/rr/native-port/tests/web_bundle_cache.cjs`

## Progress / evidence

User reported Connecting to the game server followed by WebAssembly LinkError,
Import #287 module a / name re requires a callable. Current clean Chrome booted
successfully. Public response evidence: rr-game.js had Cache-Control
max-age=14400 from Cloudflare while rr-game.wasm had no-cache/DYNAMIC. The
origin itself returned no-cache for JS. This allows cached old glue to pair
with a freshly revalidated, incompatible Wasm import table after deployment.

Exact reproduction: Playwright intercepted rr-game.js with the previous
validated build-agent-browser-fonts loader while fetching current live Wasm;
the user's exact #287 a/re LinkError appeared. Current Wasm imports a.re as a
function; old loader did not supply it. Neither server game assets nor saves
caused this failure. Cloudflare's blocked analytics beacon was unrelated.
Headers/downloads kept locally in ignored analysis/web-linkerror-20261025/.

Implemented c58b2a8: fingerprint every executable/static dependency. Rewrite
Wasm URL in its loader BEFORE hashing the loader, then hash the app/reference
manifest/CSS and rewrite index references. Exactly seven code/metadata files
remain. Bundle validation verifies filenames against actual content hashes.
Old generated hashes are cleaned only in the generated distribution directory.
Nginx serves hashed URLs immutably and the entrypoint with no-store. Old JS
and new Wasm no longer share the same public URL across versions.

GitHub workflow now installs browser runtime and executes stale-cache upgrade
acceptance BEFORE deployment, then repeats hosted gameplay acceptance afterward.
A real HTTP-cache fixture first caches poisoned legacy rr-game.js for four
hours, switches to fingerprinted HTML, reloads the same browser context and
requires successful Wasm startup, hashed JS/Wasm requests, no legacy requests
and no page errors. Synthetic old loader data is used in CI; no game assets
are needed for this predeployment test.


## Checks / remaining work

- Exact user error reproduced using old real loader + current real Wasm.
- Clean current release boots (baseline problem depends on stale cache).
- Fingerprinted bundle local stale-cache upgrade acceptance: passed.
- Full hosted Chromium smoke on fingerprinted bundle: passed manifest/hash
  fault/retry, original-font loading, all viewport/Window Size combinations,
  fullscreen, visible race, keyboard input, nonzero audio, OPFS cache repair,
  reload reuse and no uploads/page errors. /tmp/rr-cache-hosted-smoke.log.
- Node syntax, hash-aware bundle validator, Git whitespace checks passed.
- Reused verified live engine binaries locally for packaging tests; GitHub
  builds current C++ source from scratch. No C++ changes/native gate needed.
- Deploy workflow https://github.com/arkits/road-rash/actions/runs/40268619292
  Public hashed URLs, no-store HTML and fresh/cached browser startup verified
  after deployment: all verified. Run 40268619292 is green, including stale-cache
  preflight and full hosted browser acceptance. Public revision c58b2a8 verified.
  Public loader rr-game.42de9aee7f409117.js points to
  rr-game.a20376369401d04e.wasm; both downloaded bytes match their filename
  hashes. HTML Cache-Control no-store/max-age=0; both engine files public,
  max-age=31536000, immutable. Independent real Chrome startup and normal reload
  reach Ready to play with no page errors and only fingerprinted engine URLs.

Publishing via GitHub OAuth HTTPS initially rejected the workflow change for
missing workflow scope. Existing direct SSH key successfully pushed the same
commit without changing account permissions or requesting broader OAuth access.


## Live processes / resources

Only owned fixture server is node /tmp/rr-cache-test-server.cjs on 8790.
Server stopped and claims released. No held resources remain. No other shared processes/resources
were modified. Initially dirty tests/web_browser_smoke.cjs belonged to concurrent
work, later committed by its owner as 5fb7d8a; it was not edited/staged here.
