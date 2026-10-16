# actor-dispatch-20261016i — Plan 11 Task 3 background: think/draw dispatch + draw order

Task: read-only background for Plan 11 Task 3 (actor think/draw dispatch +
draw order: start-grid order, near road before shadow/body, `<0x80` fraction
threshold inserts scenery between actors). No implementation, no captures.

Baseline: `a7c1ecb`. Worktree: `/Users/archit/Dev/rr/native-port`
(shared tree; all prior `??` scratch dirs and sibling in-flight files preserved).

Owned files (this task only, both new/untracked):
- `docs/agent-handoffs/actor-dispatch-20261016i.md` (this file)
- `analysis/actor-dispatch-20261016i/` (`rea-443d70-project.md` + `.log`,
  `rea-444630-slice.md` + `.log`, `compare-443d70-444630.md`)

## REA spot-checks (2 calls, the allowed max; prefer-uncovered rule followed)

Pinned `rea-agents@6.4.0`, Ghidra 12.1.4,
`PATH="$HOME/.nvm/versions/node/v24.13.0/bin:$PATH"`,
`GHIDRA_INSTALL_DIR=/opt/homebrew/Cellar/ghidra/12.1.4/libexec`,
`JAVA_HOME=.../openjdk@21/...`. Reference EXE digest
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
matches inventory in both outputs. `--format md`, exit 0 both.
`0x00443af0` needed no re-fetch (already confirmed 3x); the two calls cover
the previously unconfirmed projection bytes it depends on:

| Function | evidence_id | Range | Match vs `analysis/decompiled/` |
| --- | --- | --- | --- |
| `0x00443d70` build 33 projected road slices | `ev_a7c9199dd7ea976c8390f3695e09a285bc6902eefab8b630374d8df510d7dcee` | 0x443d70..0x444625, 2230 B | TRUE (7363/7363 norm chars) |
| `0x00444630` project one road slice | `ev_bcf549edd59df3235dedfdc76b95d958777690f58e4e108642218b9a3b9a` | 0x444630..0x444e17, 2024 B | TRUE (8120/8120 norm chars) |

Call sites (REA edges): caller `0x00443a00` (count `0x21` = 33);
`0x00443d70` calls `0x00444630` at `0x443e99/0x44406b/0x44407b/0x444266/0x444276/0x4444a0`
(single + dual-road paths); `0x00444630` callees `0x0040b8e0/0x0040ccb0/0x0040ccf0/0x0041d6d0/0x00443940/0x0044f5a0`.
Detail + first-attempt window note in the scratch compare file.

## Dispatch table (prior trace, re-cited read-only — nothing re-derived here)

- Think: `0x00437e60` scheduler calls think at `+0xe0`, move at `+0xe4`
  (`docs/REVERSE_ENGINEERING.md` object-lists section); `0x00438170(0)` walks
  the ring, `0x00438aa0` position sort. Every bike (player included) thinks
  with `0x00407a90`; rider-halves with `0x00434660` (player half mirrors the
  player bike) / AI `0x00407bd0` (REA-confirmed, 2221 chars). AI state dispatch
  table `0x00465980`: 8 dwords at file offset `0x63f80`
  (`408e30 409000 4092d0 409460 409ca0 40a4f0 40a560 40a5b0`), byte-verified.
- Draw: `0x00446250` bike+shadow emits shadow first (clip 0, ground without
  vertical) then body (clip 1, ground + vertical>>8), re-running view select
  `0x0043af60` after the `+0x90` swap each side; armed `0x004463e0` adds a third
  `0x0044ba40` weapon-overlay node (`0x0040b750` frame); `0x0044d300` has NO
  shadow darkening (same textured path, index-0 transparency). Shadow colour
  `0x00446350`: `0xdf` iff body is cop (`0xde`), else `0xe0`. `0x00443af60`
  view select + `0x0044ba40` billboard REA-confirmed (3487/3686 chars).

## Order law (start-grid research, re-cited)

Scene loop `0x00443af0` (0x224 stride from `0x004c0a5c`, index 31):
`W(31)`, then `W(j-1)` + `O(j)` + cursor commit down to `O(0)` — the whole
nearer world strip (lanes via `0x0043c170`, both terrain sides via `0x0043bef0`,
both roads + `0x0043be80` join on splits) precedes the farther object pass.
Inside `O(j)` = `0x00443b40`/`0x00438610`, when depth `+0x18 > 0x92`:
roadside mask `0x00000ff8`, then reverse-visible-order actors, but before the
first actor with rendered-position low byte (unsigned) `< 0x80`, remaining
roadside `0xfffff007` + placed scenery via `0x0044b5f0` (`cmp al,0x80; jae`
at `0x004386a7`). Depths ≤ 146 suppress scenery, not actor callbacks.
Start-grid capture tails: near road nodes, then shadow, then body (body bottom
y 164.5 below road seam 152.5; shadow/body Bob `0x00466684` frame 47).
Full trace: `docs/research/2026-10-12-start-grid-order.md`.

## Native approximation vs original (read 20261016, `src/` only)

- `src/course_scene.cpp:256-264`: front-half-only delay — actors with fraction
  low byte `< 0x80` queued in `front`, emitted after the next nearer road
  panels/marked ground (`:289` flushes `pending` at `i==count-1`), but BEFORE
  nearer shoulders/hills/buildings (`:290-310`), and back-half actors + scenery
  emit with their own strip (`:311-318`). Original: whole nearer world pass
  before farther object pass for BOTH halves; fraction split only inside the
  object pass. Approximation fixes tyre clipping; not the W/O law.
- `src/actor_nodes.cpp:27-53` `ActorNodes::rider`: shadow + body, 2 nodes only —
  missing the `0x004463e0` weapon overlay (known: reported club-without-attack).
- `src/actor_render.cpp:20-46` `actor_node` (`0x0044ba40` path): centre
  `(w&~1)*0x8000`, right/left cull vs `DAT_0046667c`, occlusion-row cull when
  `clip_ground`; shadow passes `clip_ground=false`, body `true` — matches the
  dispatch contract. Native `Race`/traffic/ped code cites scheduler cadence
  `0x00437e60`/`0x00438170` but steps riders/cars/peds in separate passes, not
  the original position-ordered ring interleave (open per `race.hpp:243-249`).

## Node/frame parity plan (for the Task 3 owner; not started)

1. Separate world-strip emission from per-slice object emission following the
   `W/O` sequence; keep farther-slice projection/occlusion state in `O(j)`.
2. Preserve scenery mask groups (`0x00000ff8` / `0xfffff007`) and
   scheduler-derived tie order (reverse visible array), not native enumeration.
3. Add the weapon-overlay third node; keep shadow/body order and clip flags.
4. Parity vectors: fractions 0/127/128/255 boundary actors, equal-depth
   rider-vs-car ties, a hill/building strip, start-grid frame 0
   (road → shadow → body, tyre-below-seam). Needs full bike/rider scheduler
   state in captures (v4 `dump_rider_objects` exists; `--native-actors`
   currently FAILS 4/4 isolated to view/subframe base — `read_actor_sequence`
   stores descriptor index where `0x0040b6f0` group lookup expects a group id;
   fix against `0x0040b200` ground truth first).

## NOT-changed list

- No tracked file edited (no `src/`, `docs/`, `tests/`, `tools/`, `CMakeLists.txt`
  touch; `git status` short of tracked files verified clean before writing).
- No builds, no `ctest`, no oracle captures/proxy/prefix/registry changes
  (capture outage note in REVERSE_ENGINEERING stands; untouched).
- Sibling in-flight `??` scratch dirs and any live-claimed paths avoided —
  nothing read was modified; no file claims taken, no locks held.
- REA runs used ephemeral import only; `analysis/ghidra-project/` untouched.
  New scratch files are disc-derived REA output under `analysis/` (git-ignored
  pattern per prior sessions) plus one comparison note; handoff is the only
  `docs/` write. No commits.

## Live processes / resources

No live REA/Ghidra/Wine processes. No build dirs, locks, or file claims held.
Next: hand to Plan 11 Task 3 owner with this background + scratch dir.
