# dzhrsort-20261016s — DZHR sort/spawn background (READ-ONLY)

Task: read-only background on the DZHR sort/spawn slice — ring build walks each
slice's DZHR actor records; type `>= 0xb` police consume draws; 15-object pool
template `0x0046dde0`. Spot-check only; no builds/captures; no commits.

Baseline: `34d0601` (`git rev-parse HEAD`). Tree carries sibling uncommitted
changes at start AND end — none touched (see NOT-changed list).

Worktree: `/Users/archit/Dev/rr/native-port`. No owned build dir; no build run
(docs/scratch only).

## Owned / changed files

Tracked: NONE changed. No `git add`, no commit.

Created (this session only):
- `docs/agent-handoffs/dzhrsort-20261016s.md` (this file)
- `analysis/dzhrsort-20261016s/notes.md` (scratch analysis)
- `analysis/dzhrsort-20261016s/rea-452060-dzhrinit.md` + `.log` (REA spot-check 1)
- `analysis/dzhrsort-20261016s/rea-440640-poolalloc.md` + `.log` (REA spot-check 2)

(`analysis/dzhrsort-20261016s/` is scratch, not in `.gitignore` — integrator
must not stage it; REA dossiers are disc-derived, LOCAL ONLY, never commit.)

## 1. REA spot-check (pinned 6.4.0, this session, 2/2 allowed calls)

Env per `docs/TOOLING.md`: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=openjdk@21`. EXE sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(matches `analysis/inventory.json` and the task pin; also stamped in both
dossiers' `subject.digest`). Both `--format md`, both exit 0, provider Ghidra
12.1.4, `import_mode: ephemeral-source-immutable`.

| Function | evidence_id | REA norm chars | Export chars | Match |
| --- | --- | --- | --- | --- |
| `0x00452060` DZHR iterator init | `ev_f461cf269c9c20351413937e62c88448a6bf20d91effde3fe91fdfadef2f91cf` | 230 | 230 (`analysis/decompiled/00452060.c`) | TRUE (exact) |
| `0x00440640` ped pool alloc | `ev_08d1cb02ec351b857914f74ae684a8aaede6d8adf045bfc1c088cf8b1add8dad` | 78 | 78 (`analysis/decompiled/00440640.c`) | TRUE (exact) |

Whitespace-normalized (all `\s+` stripped) full-body comparison; both exact.
REA is a spot-check per `docs/TOOLING.md`, not runtime proof; no replay/pixel
claim is made from it.

Uncovered-address selection: `0x00453e60`/`0x00451ee0`/`0x00451fc0`
(dzhrsort-rea-20261012), `0x004520d0`/`0x004409e0` (dzhrspawn-rea-20261012),
`0x00451e20`/`0x00451cb0`/`0x00452230` (objectiter-rea-20261012),
`0x00440dd0`/`0x00440990`/`0x00440680` (pedchain),
`0x00440760`/`0x00440b80`/`0x00441500` + `0x00440730`/`0x004416a0`
(pedcontact/ped-helpers-20261016n), `0x00406b80`/`0x00407860`/`0x00407f30`
(policechain), `0x00454120`/`0x00449910`/`0x00449990` (ringplumb),
`0x00454c80`/`0x00453030` (trackring-verify), `0x00454240`/`0x00451380`
(terrain-interleave-20261016b), `0x00438930` (spawn-chain-20261016k) were all
covered by prior handoffs. `0x00452060` had only a `REVERSE_ENGINEERING.md:59`
table row ("Not decoded") plus a `src/track_ring.hpp:17` code comment;
`0x00440640` only an export-read cite in ped-helpers-20261016n. Neither had
any `rea-*` dossier — both spent here.

## 2. New pins (details in scratch `notes.md`)

- `0x00452060` (52 bytes, `0x452060-0x452093`, contiguous, no `rand()`):
  DZHR iterator init — `p[0]` = section; if nonzero, `param_4 != 0` (fresh
  enter) zeroes position/index, else index = `*(section+8)-1`, position =
  `(param_3-1)*0x100`. REA callers table: `0x004520a0` + `0x00452cb0`
  (section enter). Native `TrackRing::enter` (`src/track_ring.cpp:121`)
  models the fresh-enter shape (`w.dzhr = {node.link, 0, 0}`); the
  `(param_3-1)*0x100` shape has no native counterpart — consistent with the
  port's single-road-forward limit (`step` marks `unsupported_` on
  backward/split walks, `:95-96,129-136`).
- `0x00440640`: `DAT_004c1274 = FUN_00438b70(0x468, 0xf, NULL)` — allocates
  the 15-entry × `0x468`-byte ped pool. No `rand()`; template application is
  downstream (`0x00438930`/`0x004387a0`). Native: `pool_size = 15`
  (`src/pedestrians.hpp:40`), `pool_(pool_size)` + `read_exe_data(exe,
  0x0046dde0, 100)` with `template_[1] == 0x20` guard
  (`src/pedestrians.cpp:7-13`), `reset()` empties pool + clears consume marks
  (`:14-18`). Structural match.

## 3. Sort/spawn table (original → native, read-only `rg` at `34d0601`)

| Step | Original (all REA-confirmed) | Native | Verdict |
| --- | --- | --- | --- |
| DFS `0x00453e60` | visit-once over course graph; per road section jitter + sort | `jitter_events` (`track_ring.cpp:27-58`) | match (prior) |
| Jitter `0x00451ee0` | TWO draws/record: offset `rand%(spread+1)` on running base + level-gate `rand%10` consuming failures (`\|= 0x3f00`); bonus `level` vs `level-4` for actor-type `>= 0xb` | `jitter_events` `:41-52`; `place_dzhr` (`scenery.cpp:40-48`) | match (prior) |
| Sort `0x00451fc0` | pairwise swaps on `& 0x7fff0000`, NOT stable | `track_ring.cpp:53-55`; `scenery.cpp:49-51` (comment rejects `stable_sort`) | match (prior) |
| Iterator init `0x00452060` **(new pin)** | fresh-enter zero vs `(param_3-1)*0x100` + count-1 shapes | `enter` `:121` covers fresh-enter shape only | match + labelled gap (backward shape, out of port scope) |
| Index `0x00452230` | skip-while `sample <= key`, may end one past last | `step` `:143-150` (comment kept) | match (prior) |
| Slice build `0x00454240` | 2× `0x00451cb0`, then `0x004520d0`; billboards draw nothing | `build_slice` `:153-173` (slots ×2, no-draw billboard skip `:167`, one draw/record `:168`, `\|= 0x3f00` `:171`) | match (prior) |
| Spawn `0x004520d0 → 0x004409e0` | one draw/record; ped trigger `player + rand%0x4fb1` + serial; cop branch (type `>= 0xb`) two `rand%0x81` + `0x00449910` + `0x00406b80` | `spawn_pedestrian` (`race_pedestrians.cpp:75-128`): lateral `:84`, cop draws gated on `!ring_setup_` `:91`, pool-full still consumed `:97`, trigger `:114`, serial `:115` | match (prior); cops unbuilt by design (plan 24 Task 2) |
| Pool `0x00440640` **(new pin)** | `0x00438b70(0x468, 0xf)` → `DAT_004c1278`-serialled pool | `pool_size = 15`, `read_exe_data(0x46dde0, 100)`, `reset()` | structural match |

Known native gap (unchanged, porter scope, not this session):
`build_pedestrian_slices` (`race_pedestrians.cpp:32-54`) walks only the camera
roadway; the split second-roadway `0x00454240` call is not walked (`:17-18`
comment). No code changed here.

## 4. NOT-changed list (explicit)

- Sibling-live 8 (present as `M` at session start AND end, never read for
  content beyond `git status`; all `rg` scans excluded these paths):
  `src/course_scene.cpp`, `src/race_caches.hpp`, `src/race_screen.cpp`,
  `src/road.cpp`, `src/road.hpp`, `tests/course_scene_test.cpp`,
  `tests/race_cache_test.cpp`, `tests/road_test.cpp`.
- Untouched by design: all other tracked sources/tests (`src/track_ring.*`,
  `src/scenery.*`, `src/race_pedestrians.*`, `src/pedestrians.*`,
  `tests/*`), `oracle/*`, `CMakeLists.txt`, shared docs/plans/status
  (`REVERSE_ENGINEERING.md`, `PROGRESS.md`, `VERIFICATION.md`,
  `docs/plans/24-road-handoffs-police-crash-control.md`), Ghidra project,
  prefix/Wine installs, captures — read-only (`read`/`grep`) or unread, none
  modified.
- NOT done: no source/test/oracle edit, no build, no capture, no golden
  change, no commit/push, no `0x004520a0` follow-up trace, no runtime proof
  claim.

## Checks

- `git rev-parse HEAD` = `34d0601` (matches task pin); `git status --short`:
  the 8 pre-existing sibling `M` entries + untracked `analysis/*` /
  `docs/agent-handoffs/*` scratch (this session's six paths only).
  No tracked file modified.
- REA: 2/2 exit 0, whitespace-normalized pseudocode exact-identical to
  exports (230/230, 78/78).
- Reads (all excluding the 8): exports `00452060/00440640/004520d0/004409e0`,
  `src/track_ring.cpp` (full), `src/track_ring.hpp`, `src/scenery.cpp/.hpp`,
  `src/pedestrians.hpp`, `src/pedestrians.cpp:1-59`,
  `src/race_pedestrians.cpp:1-130`, prior
  dzhrsort/dzhrspawn/objectiter/pedchain/pedcontact/ped-helpers/police-spawn/
  ringplumb/spawn-chain/schedwalk/terrain-interleave handoffs, `docs/TOOLING.md`,
  EXE sha256.
- No builds or captures run; no locks taken.

## Live resources

None held by this session. No background shells, builds, captures, or Ghidra
projects started. Both `npx` calls completed exit 0 and returned.

## Remaining work (porter / integrator)

1. Port decision on the `0x00452060` non-fresh shape (`(param_3-1)*0x100` +
   count-1 index) if backward/split walks are ever modelled; today correctly
   absent per the single-road-forward limit.
2. Plan 24 Task 4 Step-5 acceptance still open (`city-pedestrians` /
   `city-pedestrian-hit` goldens + pixel frame); live-cop branch
   (`0x00406b80`) stays with plan 24 Task 2.
3. Integrator: link from plan 10/24 next index refresh; keep
   `analysis/dzhrsort-20261016s/rea-*` local (disc-derived, do not commit;
   the scratch dir is not in `.gitignore` — do not stage it).
