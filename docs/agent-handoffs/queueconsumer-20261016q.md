# queueconsumer-20261016q — sound queue consumer + voice-stealing priorities (read-only background)

Task: read-only background — sound queue consumer + voice-stealing priorities
(mixer oldest-voice provisional). Spot-check only; no builds/captures; no commits.

Baseline: `940f4d7` (`git rev-parse HEAD`). Tree had sibling uncommitted changes at start —
none touched (see NOT-changed list).

Worktree: `/Users/archit/Dev/rr/native-port`. No owned build dir; no build run (docs/scratch only).

## Owned / changed files

Tracked: NONE changed. `git status --short` before/after shows only the pre-existing sibling
eight modified + untracked scratch/handoff files. No `git add`, no commit.

Created (this session only):
- `docs/agent-handoffs/queueconsumer-20261016q.md` (this file)
- `analysis/queue-20261016q/notes.md` (scratch analysis)
- `analysis/queue-20261016q/rea-435e60-slots.md` + `.log` (REA spot-check 1, disc-derived)
- `analysis/queue-20261016q/rea-434c40-nearvoice.md` + `.log` (REA spot-check 2, disc-derived)

## Evidence

Pinned REA env: `PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`,
`npx --yes rea-agents@6.4.0`. EXE `../cdrom/ROADRASH/ROADRASH.EXE` sha256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2` (verified pre-call).

Prior handoff `queueconsumer-rea-20261012.md` covered `0x004351f0` + `0x004367f0`;
`soundhelpers-rea-20261012.md` covered `0x004349c0/0x00434ce0/0x00436830`.
This turn used the max 2 REA calls on UNCOVERED addrs in the voice-priority path:

| Function | REA norm chars | Export chars | Match | evidence_id |
| --- | --- | --- | --- | --- |
| `0x00435e60` slot-table init | 3683 | 3683 | TRUE | `ev_eef057bfa231067d6b3d1f6a1e7738bb52b485f9ba849d2089c45ea7dcd0e277` |
| `0x00434c40` per-tick voice dispatch | 388 | 388 | TRUE | `ev_59c91c6ad059b88a4eca3809376c3c44d8c82df1691b531a858e83360665ac76` |

Full-body whitespace-normalized comparison with both lengths printed (truncated-window
guard from prior handoff observed). REA is a spot-check per `docs/TOOLING.md`, not runtime
proof; no replay/pixel claim. Raw files local in `analysis/queue-20261016q/`.

Native code read (eight excluded via `rg --glob`): `src/mixer.hpp:62-75`, `src/mixer.cpp:59-65`,
`src/mixer.cpp:120-143` (steal line `:126`), `src/sound.hpp:29-31,50-61`, `src/sound.cpp:162-287`,
`src/sound.cpp:302-353`. Details in scratch `notes.md`.

## Priority table (original fixed slots vs native mixer)

| Priority | Original (confirmed addrs) | Native port | Status |
| --- | --- | --- | --- |
| P0 player engine | `0x00434c40` always refreshes player (`0x00434ce0`, or `0x004349c0` when muted-flag); slot 0 per bike (`0x00435e60`/`engine_for`), nitro→1 / tunnel→2 with others stopped (`sound.cpp:263-265`) | `SoundSystem::update` same slot rotation + `play_loop` update-or-start | Aligned |
| P1 nearest-two others | `0x00434c40` → `0x004350e0` → `0x00434fd0(obj,3/5)`; absent → `0x00414160` stop (`00434c40.c:15-27`, REA TRUE) | slots 3/5, `stop` when beyond hearing (`sound.cpp:267-287`) | Aligned |
| P2 one-shots | `0x004142c0`: restart slot buffer, skip if still playing — never interrupts/steals (`sound.cpp:162-169`) | `play_once` returns early if `playing(slot)` | Aligned |
| P3 loops | `0x004141c0`: update params, start if idle, stop below −90 dB (`sound.cpp:171-180`) | `play_loop` same gate | Aligned |
| P4 event queue | `0x00436830`: ≤10 pending; `0x004351f0` consumes per tick (siren-19 fast path, per-code vol/freq) — prior REA TRUE | `post` cap 10, drain+clear at end of `update` (`sound.cpp:209-210,303-352`) | Aligned |
| Overflow | Fixed 0x45 DirectSound buffers (`0x00435e60`, REA TRUE): pressure resolves by skip/stop at the slot — **no cross-slot stealing** | `Mixer::consume` (`mixer.cpp:125-126`): free slot first, else `min_element(a.id<b.id)` = **oldest-voice steal (provisional)** | DIVERGENCE (cold when voices ≥ slots+music; needs plan-13 policy decision) |

Provisional marker stays on `mixer.cpp:126` until plan 13 decides: (a) size + refuse,
(b) per-slot pinning, or (c) keep oldest-steal with test. No code changed this turn.

## NOT-changed list (sibling / live-claimed paths — neither read nor edited)

- `src/course_scene.cpp`
- `src/race_caches.hpp`
- `src/race_screen.cpp`
- `src/road.cpp`
- `src/road.hpp`
- `tests/course_scene_test.cpp`
- `tests/race_cache_test.cpp`
- `tests/road_test.cpp`
- Shared docs/status/plans (`REVERSE_ENGINEERING.md`, `PROGRESS.md`, `VERIFICATION.md`,
  plan 13, `game_main.cpp`, oracle proxy): untouched.
- Shared resources (`oracle/`, prefix/Wine installs, Ghidra project, captures): untouched.

## Checks / remaining work

- Checks run: `git rev-parse HEAD` + `git status --short`; EXE sha256; `fd`/`rg` reads
  (eight excluded); 2 REA `--format md` calls (both exit 0); whitespace comparisons (both TRUE).
  No build, no tests, no captures per task scope.
- Remaining for integrator/owner: plan-13 voice-policy decision on the `min_element`
  provisional; link this handoff from plan 13 Task 1 next refresh; keep
  `analysis/queue-20261016q/rea-*` local (disc-derived, do not commit).

## Live processes / resources

No live REA/Ghidra/Wine processes spawned by this session (REA calls completed, exit 0).
No build locks, no servers, no captures held. No file claims beyond the two new
handoff/scratch paths.
