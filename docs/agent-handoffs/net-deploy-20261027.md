# net-deploy-20261027

Task: Plan 34 Task 8 rr-netd deployment and production smoke

Baseline: 9742d3411021b6f08e24f674cc57eabd2154d502

Worktree: `/Users/archit/Dev/rr/.plan34-impl-20261026`
Build directory: `/Users/archit/Dev/rr/.plan34-impl-20261026/build-agent-net-deploy-20261027`
Owner PID: 77961
Resource token: `df33d8a1-3adb-4794-ab5d-574c1eb8ac9d`

Owned files:

- `/Users/archit/Dev/rr/.plan34-impl-20261026/deploy/road-rash-net`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/deploy/road-rash-tls.conf`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/.github/workflows/net-deploy.yml`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/docs/WEB_HOSTING.md`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/docs/plans/34-multiplayer.md`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/server/Dockerfile`

## Progress / evidence

Task 8 preparation complete; production deployment incomplete. New workflow and
`deploy/road-rash-net/` contain the binary runtime, healthcheck, Nginx routes,
Linux smoke image, verified ephemeral data fetcher and provisioning/rollback
runbook. `deploy/road-rash-tls.conf` shows the intended include; it is not
installed on the host. No change to server runtime C++ or game simulation.

Checks and evidence:

- `docker build --platform linux/amd64 -f server/Dockerfile --build-arg RR_NETD_REVISION=9742d3411021b6f08e24f674cc57eabd2154d502 ...`: passed.
- Extracted stripped binary + runtime Dockerfile/app.json: built and ran as uid/gid 999; health returned the exact revision.
- Native ARM64 bots -> Linux amd64 server: matched hash `7141664904817385239` at cutoff 420, agreed results.
- Nginx snippet `nginx -t`: passed; ARM64 bots through real proxy: matched `15309045843098366043`, agreed results.
- Linux amd64 `smoke.Dockerfile` built and ran through the real Nginx proxy: matched `9265308237651102975` at 420, agreed results. Disc is a read-only runtime mount, not copied into an image.
- Actionlint 1.7.7: passed.
- Fetcher: all 341 real-disc fixtures verified; hostile path/manifest rejected before writes. Live hosted manifest matched every committed reference; smallest hosted object size/hash verified. No claim of downloading the complete live set. Descriptive client user-agent fixes observed default-Python HTTP 403.
- `git diff --check`: passed. Native CTest is not repeated for deployment-only changes.

Local logs `/tmp/rr-net-*`; retained evidence/context outside Git at
`/Users/archit/Dev/rr/.plan34-logs/net-deploy-20261027/`. Docker images
`rr-netd:p34-task8`, `rr-netd:p34-package`, `rr-net-smoke:p34` are retained for
resumption and contain no commercial assets. All own test containers stopped.

## Blocker / next action

SSH `arkits@agnee` with BatchMode/ConnectTimeout failed after the 1Password
agent reported RSA signing failure. Direct public IP with IdentityAgent=none
and a host key from the existing authenticated GitHub known-hosts variable
also rejected the local Ed25519 key. Do not weaken host-key checks. Unlock or
restore the operator identity, then follow the runbook: read installed Dokku,
apps/listeners/TLS/firewall/hook; provision only the approved new app and route;
install dedicated restricted CI key; deploy/check health+revision; enable gated
workflow and require real production bot smoke. No remote mutation happened.
`RR_NET_DEPLOY_ENABLED` was confirmed absent; no credentials were generated or
uploaded. Task 8 Step 3 needs Task 9 implementation before real UDP acceptance.

## Live processes / resources

Owner helper PID 77961 and claim token above are released at completion.
No VPS resource was held; all local port claims/test containers released.
Main checkout's unrelated frontend acceptance work and pre-existing analysis
symlinks were preserved.


CI receipt: [multiplayer build 40523572291](https://github.com/arkits/road-rash/actions/runs/40523572291)
passed for `a731defe26bf95480c5115033f2729e71ea10f38` (build 1m21s;
deploy skipped by the provisioning gate). Downloaded artifact contains exactly
`rr-netd`, `Dockerfile`, `app.json`; the executable is ELF64 x86-64. Rebuilt
that artifact's runtime locally and verified `/net/health` returns `ok:true`
and the exact CI revision. All test containers stopped. No multiplayer VPS
provisioning or host configuration change was performed.
