# Handoff: Vercel deployment scaffold (plan 23 Task 9, early)

- **Task/plan:** plan 23 Task 9 Steps 1–2, early scaffold only. Playable web
  deploy is blocked on Tasks 2–8 (see `docs/plans/23-web-port.md`).
- **Baseline commit:** `e2dce9a` (plus other workers' uncommitted changes in
  `src/game_main.cpp`, `src/mixer.*`, `src/race.cpp`, `src/race_music.cpp`,
  `src/sound.cpp`, tests — untouched, preserved).
- **Owned files (new):** `vercel.json`, `tools/build_web.sh`.
- **Changed files:** `docs/WEB.md` (appended "Deployment (Vercel scaffold)"
  section only).
- **Generated, git-ignored:** `build-web/dist/` (`build*/` is ignored; Vercel
  rebuilds it via `buildCommand`).
- **Evidence/checks run:**
  - `tools/build_web.sh` → bundle OK (index.html 978 B + LICENSES.txt),
    bundle check passed (no game extensions, no reference-hash match).
  - `python3 -c json.load(open('vercel.json'))` → valid.
  - `sh -n tools/build_web.sh` → syntax OK.
  - Local `python3 -m http.server` serve of `build-web/dist` → 200, correct bytes.
  - `vercel whoami` → CLI unauthenticated (`The specified token is not
    valid`); no deploy attempted. Needs `vercel login`.
- **Remaining work / next action:**
  1. Owner: `vercel login`, then `vercel` (preview) to create the project;
     confirm the placeholder URL.
  2. Owner decision per plan Task 9 Steps 3–4 before any `--prod` deploy
     (host choice, project naming/trademark presentation).
  3. Tasks 3–6 must land before the bundle is a game; then extend
     `tools/build_web.sh` to build the Emscripten game target instead of the
     placeholder, and Task 7 adds COOP/COEP headers for the threaded build.
- **Held resources / processes:** none. Local test HTTP server was killed.
- **Follow-up (same day):** owner's `vercel` run failed with "files should
  NOT have more than 15000 items, received 55658" (`.tools/` ~51k files
  uploaded). Added `.vercelignore` (`.tools/`, `build*/`, `analysis/`,
  Android build outputs, `Testing/`, `.cache/`, `.superpowers/`) and a
  no-op `installCommand` in `vercel.json`. Owner should retry `vercel`
  (or `vercel --archive=tgz` if the count is still high).
