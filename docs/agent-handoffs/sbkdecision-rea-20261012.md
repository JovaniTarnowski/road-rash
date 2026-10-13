# sbkdecision-rea-20261012

Task: Plan 13 Task 1 SBK research + synthesis decision (Steps 1+3 done, Step 2 left open — no AWE channel/controller trace evidence). Tree clean + index empty at start (verified); unowned slice only.

Baseline: `09ced63` (own prior commit).

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `build` (rerun at commit time for docs-only change)

Owned files:
- `/Users/archit/Dev/rr/native-port/docs/REVERSE_ENGINEERING.md` (one bullet, music section — first mis-placed under Save-file, then CUT + re-placed correctly)
- `/Users/archit/Dev/rr/native-port/docs/plans/13-music-and-frontend-completion.md` (Task 1 Steps 1+3 checked; Step 2 deliberately unchecked)
- `/Users/archit/Dev/rr/native-port/docs/agent-handoffs/sbkdecision-rea-20261012.md` (this file)

Disc analysis (python3, capture-free, this turn):
- All 6 `.SBK` are SoundFont 1 (`sfbk`, `ifil` v1.0, 16-bit `sdta/smpl`): sizes 110–373KB, 2–6 presets + EOP, 3–20 instruments.
- All 11 `.MID` (GRUNGE/SBKGRNG/KLOWN) are RIFF `MIDS` (`fmt `+`data`), not SMF — consistent with confirmed `0x00434230`.
- Programs bank-specific, not GM (status-byte census across channels 0–15 per song).
- Full generator inventory recorded in the doc bullet (loop offsets, LFOs, filter, envelopes, sends, tuning, keyRange, exclusiveClass, root key).
- DECISION: option B (native sampler, already implemented in `src/race_music.cpp`; no third-party file added). Option A rejected on surface-size grounds; option C never needed.

Honesty note: Step 2 ([ ] left open) — the `0x00433bb0`→AWEMAN32 channel/program/controller trace is NOT evidenced (proxy forwards ordinal 2 without logging MIDI voice messages). Channel census from MIDS `data` bytes is song-side only, not the AWE path. A future capture (proxy MIDI logging or AWE call dump) can close it; the synthesis decision does not depend on it.

## Progress / evidence

Disc bytes read directly; no REA, no captures, no replay claims. The decision rationale matches the existing implementation (`race_music.hpp` header already documents SF1/MIDS/bank-specific programs; `music_test.cpp` covers parser + truncated-bank behavior).

Shared-doc edits: RE bullet + plan checkboxes only. No source, test, or other doc file touched.

## Checks / remaining work

- `cmake --build build --parallel`: must pass before commit (docs-only change; rerun at commit time).
- Staged-fileset hygiene: verify `git diff --cached --name-only` == my three pathspecs before committing.
- NOT done: plan 13 Task 1 Step 2 (AWE voice-message trace, needs proxy capture); Tasks 2/4/5/6 implementation scope belongs to owners.
- Integrator: plan 13 Task 1 now Steps 1+3 done; link from next index refresh.

## Live processes / resources

No live REA/Ghidra/Wine processes. No resource claims held. No build locks. Background ctest job `bg_30` from a prior turn may still be running — untouched.
Sibling sessions own uncommitted changes (tree was clean at turn start) — none touched. `oracle/`, prefix/install untouched.
