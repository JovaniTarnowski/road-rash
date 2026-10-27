# Multiplayer server deployment

Status (October 27, 2026): code/archive and local container verification are
prepared. Production provisioning is blocked by the operator SSH identity:
1Password signing fails and the local key is rejected. No VPS change has been
made. `RR_NET_DEPLOY_ENABLED` is absent, so the workflow builds the code artifact
and skips deployment. Task 8 is still open.

## Package and workflow

`server/Dockerfile` builds a stripped Linux amd64 Release daemon with its exact
Git revision. The workflow copies that binary into an archive containing only
`rr-netd`, `Dockerfile` and `app.json`. The runtime uses Debian bookworm's SSL
libraries, runs as uid/gid 999, and has no disc files. Startup health checks use
container port 8081 and `/net/health`; the public check also requires the exact
Git revision. The WebSocket listener is container port 8080.

The deploy credential is a separate `DOKKU_ROAD_RASH_NET_KEY`. Its root
`authorized_keys` entry must force only the archive command, matching the
existing web deployment's restrictions:

```
restrict,command="/usr/bin/dokku git:from-archive road-rash-net --" ssh-ed25519 PUBLIC_KEY road-rash-net-ci
```

Confirm the Dokku executable path on the host before installing this entry.
Keep the private key out of logs/Git; feed its file to `gh secret set
DOKKU_ROAD_RASH_NET_KEY --repo arkits/road-rash < KEY_FILE`. The workflow
uses the already authenticated `DOKKU_ROAD_RASH_KNOWN_HOSTS` variable. No host-key
scan or unchecked host-key acceptance is used.

## Provision after operator access is restored

Inspect `dokku version`, apps, port listeners, the existing TLS file, firewall
and renewal hook first. Record backups of files before changing them. Preserve
other applications and any unrelated configuration.

For the new app only (first confirm ports 18080/18081 are free):

```sh
dokku apps:create road-rash-net
dokku domains:set road-rash-net road-rash-net.internal
dokku nginx:set road-rash-net bind-address-ipv4 127.0.0.1
dokku nginx:set road-rash-net bind-address-ipv6 ::1
dokku ports:set road-rash-net http:18080:8080 http:18081:8081
sudo install -d -m 750 -o 999 -g 999 /var/lib/dokku/data/storage/road-rash-net
dokku storage:mount road-rash-net /var/lib/dokku/data/storage/road-rash-net:/data
```

Verify these commands/properties against the installed Dokku version. Dokku
owns the loopback HTTP listeners and updates their container upstreams on each
deploy; fixed Docker host-port bindings would prevent overlapping deployment
health checks. Bind both listeners to loopback, and confirm that with `ss`.
Install the dedicated forced-command public key and deploy the code archive.
Check internal health and revision before exposing the route.

Install `tls-locations.conf` as `/etc/nginx/snippets/road-rash-net.conf` and add
its include inside the existing `road-rash.archit.xyz` TLS server block, as
shown in `../road-rash-tls.conf`. Merge with the current host file; do not blindly
replace it. `nginx -t` must pass before reload. Recheck web `/healthz`, existing
revision/assets, `/net/health`, WebSocket upgrade and the Certbot renewal hook.
Do not change certificate/challenge handling or other host virtual servers.

When the app, routes and key have passed those checks:

```sh
gh variable set RR_NET_DEPLOY_ENABLED --repo arkits/road-rash --body true
gh workflow run net-deploy.yml --repo arkits/road-rash --ref main
```

The enabled workflow fetches 341 hash-verified files into `$RUNNER_TEMP`, mounts
them read-only into a separate Linux bot container and runs two real rollback
clients against `wss://road-rash.archit.xyz/net`. The scripted 420-tick cutoff
must produce matching hashes and agreed official results. This is a short
transport/determinism check, not a completed lap. Game data enters neither the
image nor uploaded artifact and is removed in the always-run cleanup step.

## UDP and Task 9

The daemon currently has WebSocket transport only. Do not advertise a working
DataChannel or open an unused firewall range. Task 8's outside-network UDP and
blocked-UDP acceptance depends on Task 9's actual server peer, candidate IP,
port-range configuration, negotiation and fallback. Once that implementation
is ready, publish only 40000–40099/udp for this app and open the same range in
the host firewall, advertising configurable public IP `5.161.142.20`. Measure
both the DataChannel and explicit UDP-disabled fallback before checking off
Task 8 Step 3. No UDP/firewall change has been made by this slice.

## Rollback

Disable `RR_NET_DEPLOY_ENABLED` to stop further deploys. Redeploy the retained
code-only archive for the previous known-good revision through the same
restricted key, and require that revision in health. Failed startup checks
retain Dokku's previous container; a failed post-deploy smoke needs an explicit
rollback. Keep persistent race logs (the daemon prunes after seven days).
For a route rollback, restore only the saved TLS/include change, run `nginx -t`
and reload. Do not remove or reconfigure the existing web app.

Configuration references: [Dokku port management](https://dokku.com/docs/networking/port-management/),
[Nginx proxy properties](https://dokku.com/docs/networking/proxies/nginx/),
[startup health checks](https://dokku.com/docs/deployment/zero-downtime-deploys/).
