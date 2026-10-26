# web-shell-restyle-20261026

Task: Restyle the whole browser shell to match the Road Rash title-screen
launcher (6945873), and add a GitHub link to the footer.

Baseline: 2327b0715448c3fe980c60a40cb45bdb33a6c437

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-launcher-ui-20261026`

Owned/changed files:

- `web/index.html`
- `web/style.css`

`docs/PROGRESS.md` and `docs/VERIFICATION.md` are claimed by
`frontend-layout-20261026`, so the entries below are for the integrator to
merge.

## Progress (for docs/PROGRESS.md)

### October 26, 2026: whole browser shell in the title-screen style

Carried the launcher's look through the rest of the web UI. Shared CSS tokens
now define the palette, Badloc/Futura roles, the hazard-tape stripe and the
notched corner used by every button. The toolbar has a skewed two-tone Badloc
"Road Rash" wordmark, a skewed status tag and a hazard-tape edge. Exit to
launcher is the orange accent button. The Controls panel matches the launcher
panel: hazard top edge, kicker, skewed Badloc title and yellow key-cap labels.
The loading/stopped overlay uses the same kicker plus yellow Badloc title and a
rolling hazard-stripe bar; the import progress bar uses the same stripe, and
errors get a red-edged callout. The footer uses spaced uppercase Futura Demi and
now links to the GitHub repository (https://github.com/arkits/road-rash) next to
Licences. Notched controls draw their focus ring inside the shape because
`clip-path` would cut off an outer outline. Presentation only; no engine change.

## Evidence (for docs/VERIFICATION.md)

- `RR_WEB_BUILD_DIR=build-agent-launcher-ui-20261026 tools/build_web.sh`: exit 0.
- Served `build-agent-launcher-ui-20261026/dist` on localhost:8765. Browser pane
  checks at 800x600 (import mode, ready view, open Controls panel, in-game
  toolbar and loading overlay) and 375x812.
- `tests/web_browser_smoke.cjs` (Playwright via NODE_PATH): all 14 steps PASS,
  exit 0. Its `analysis/browser-20261025/launcher-ready.png` (local only) shows
  the disc Badloc/Futura fonts applied.
- Not run: `tests/web_hosted_smoke.cjs`, which needs the hosted asset server.
  The `#loading-title` text/font and hidden-footer checks it makes are unchanged
  by this edit.

## Remaining work

None for this task. Hosted deploy picks it up on the next Dokku deploy.

## Live processes / resources

None. The preview http.server on port 8765 was started by the Claude desktop
app's preview pane and is not part of this handoff.
