# jukebox-port-20261012

Task: Plan 13 Task 4 Step 1 — port jukebox handlers `0x00445600`/`0x00445730` (+ `0x004455b0`/`0x004456d0`) with scripted test. Tree clean + index empty at start (verified); unowned slice only (frontend/game_main/test + plan checkbox + this handoff).

Baseline: `e00880c` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build`

## Trace (this turn, disc + disassembly, no REA/capture needed)

- `0x00445600` (export `00445600.c` + disasm `0x445600–0x4456b6`): switch on menu caption 0xe1–0xe6 → track-list popup table/count (`0x473b38`x3 … `0x473ef0`x3) via `0x004224b0`, current row = saved Group/Track match.
- `0x004455b0` (no export; disasm `0x4455b0–0x4455f2`): target−0xe1 = group, caption−0xf1 = track → `0x00424080` (export `00424080.c`: persists Group/Track to registry, pokes UI). Item rows verified: `(target=group caption, caption=track slot)`.
- `0x00445730` (export `00445730.c`): two `sprintf("%s : %s")` lines (fmt `0x474678`) + 3-item video popup `0x473fa0` (dumped: title + JESSIE/DUEL rows, captions 160/161, handler `0x004456d0`, targets 0/1).
- `0x004456d0` (export `004456d0.c`): save VideoMode, default 0→2, `0x004023e0(table[target], flag 4)`, restore, redraw restroom art, close popup, menu refresh. Video table `0x473f98` = {Jessie, Duel} (dumped).
- Strings dumped via scratch tool (throwaway, removed): bands 225–230, tracks 241–254, 431 "Videos", 86 "Done", 160/161 "*".

## Port

- `src/frontend.hpp`: new `FrontendEvent::Kind::select_soundtrack` + `group`/`track` fields (positional brace-init sites unaffected).
- `src/frontend.cpp`: preload 6 track tables + `0x473fa0` in `read_frontend_data`; cases `0x00445600` (popup by caption, current = saved track), `0x004455b0` (settings.music_group/track + event + close), `0x00445730` (video popup; the two sprintf header lines have no native popup-header concept — documented skip), `0x004456d0` (VideoMode force-2/restore + queue_video + close). `queue_video` with variants=0 draws no RNG, matching the original (rand only when VideoMode≠0 and variants>0).
- `src/game_main.cpp`: `select_soundtrack` → `music.play(ev.track)` (native MusicPlayer = plan's play_soundtrack).
- `tests/frontend_test.cpp`: 6 new checks (video popup opens, JESSIE plays, VideoMode force/restore + DUEL, track popup opens, soundtrack event + settings). Failing-first: compile-fail on missing event kind, then 5 FAILs from a wrong spec assumption (popup appends → size 12, focus lands on row 1) — corrected, not the code.

Deviations: jukebox popup header lines not shown (no native concept); track switch plays RRA index = track slot (native MusicPlayer semantics; original Group/Track→RRA mapping is the soundtrack thread's, unchanged).

## Checks

- `cmake --build build --parallel`: passes (`rr-game` links).
- `ctest -R "frontend|music|mixer|sound|career"`: 7/7 pass; `frontend-test`: 0 failures.
- Full 71-test gate ran green last turn (`bg_30`); this turn's change re-verified via the related suites above. Full gate left to the integrator/CI.
- Staged-fileset hygiene: verify `git diff --cached --name-only` == 6 pathspecs (frontend.hpp/cpp, game_main.cpp, frontend_test.cpp, plan 13, this handoff) before committing.

## Remaining work

- Task 4 Steps 3 (full outcome→video scripted test — partly exists) + 4 (commit) remain.
- `DUEL` video needs mano-a-mano (plan 09, disabled) — trigger ported, unreachable in-game.
- Integrator: link Task 4 Steps 1–2 from next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. Scratch tools in /tmp only (strdump, jbdbg*) — outside repo. No build locks.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
