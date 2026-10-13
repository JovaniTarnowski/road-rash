# Plans after 11a: research handoff

Task: review the later plans and perform the research/reverse engineering needed
for an implementation agent. Date: October 12, 2026.
Baseline: `5dbf6adc415f864ef755ca292c237c001511d121`.
Repository: `/Users/archit/Dev/rr/native-port`.

## Start here

[Implementation briefing](../research/post-11a-2026-10-12/README.md) contains a
plan-by-plan review and the execution sequence. Its four companion notes cover
simulation/courses, music, options/HUD and rendering/platforms. This is research
evidence and implementation guidance, not completion of those plans.

The most urgent corrections before implementing old plan snippets:

- Disc MIDI is RIFF **MIDS**, not Standard MIDI; six banks are SF1.0. Napa's
  AWE song is SBKGRNG4.MID with GRUNGE4.SBK plus RRDRUMS.SBK. Klown has no SBK,
  uses GRUNGE5.SBK and needs factory/base instruments absent from the disc banks.
- Finish locations are level-tagged CESR events (`0x004525c0`/`0x00454240`),
  not the mileage menu table. All courses together have26 independent routes;
  Napa alone has8. Current all-left/all-right route APIs miss mixed routes.
- `0x004052c0` mutates damage/steering and has two damage inputs plus a reason;
  it does not use RNG. Weapon stealing has strict attack-frame/deadline gates
  before its random draw. Reuse 11a's car-contact logic when extracting helpers.
- Solo versus human/demo driver must be independent in replay. Post-step RNG
  anchoring needs an exact pre-step boundary. Do not certify missing fields,
  empty traces, demo-control goldens or a replay-only setup as human parity.
- Input binding conflicts clear the old binding. MiniDash is full/mini/off
  (integer0–2), not bool. Slash toggles OpponentList; F10 cycles MiniDash unless
  Ctrl selects ChatState. Needles use original GDI solid pens of width2/3.
- All59 videos are accounted for by trigger family. JESSIE and DUEL come from
  the jukebox popup, not a duel-race result. Existing result-video code is
  partial implementation evidence; live triggers/RNG still need comparison.
- MMPX remap commutation is false for arbitrary color changes. The naive
  reciprocal depth extension is not exact. Signed mip selection, clipping/math
  width, worker-local scratch and GPU capability checks need explicit contracts.

## Files owned/created by this task

- `tools/research_after_11a.py`: dependency-free, read-only disc/EXE census and
  synthetic parser self-checks. Only output is to ignored `analysis/oracle/`.
- `docs/research/post-11a-2026-10-12/README.md`
- `docs/research/post-11a-2026-10-12/simulation-and-courses.md`
- `docs/research/post-11a-2026-10-12/music.md`
- `docs/research/post-11a-2026-10-12/options-and-hud.md`
- `docs/research/post-11a-2026-10-12/rendering-and-platforms.md`
- This handoff.

No pre-existing file was edited. Shared docs/plans and all implementation files
were left to their current owners. No commit was made. Concurrent changes to
`game_main`, `course_scene`, `hud`, `frontend`, `video_player`, CMake, tooling
and a new `race_music.hpp` appeared while this task ran; they are not ours.
Recheck the latest source before implementation; briefing source-audit claims
are anchored to the baseline unless explicitly described as later observations.

## Checks and evidence

```sh
python3 tools/research_after_11a.py --self-check
```

Passed synthetic MIDS multi-block/tempo, compact-event and padded-long-event
cases; rejected232 synthetic truncations. Parsed all5 course graphs,5 CAR
containers,6 SBK banks and11 MIDS songs. Verified the EXE hash against the
inventory. Repeated census with JSON key normalization and compared equal;
confirmed26 routes/17 music files/59 video paths. Decoded the race-window
message/key tables against assembly. Checked briefing local links and code fences.

Evidence (local, ignored):

- `analysis/oracle/post11a-research/census.json`
  SHA-256 `8ed67b527d0fb22e7a87019cc09297a994012efabf427c23e22cea6b84ef100a`
- `analysis/oracle/post11a-research/verification.log`
- Existing `analysis/decompiled/` address exports and original disassembly.
- Primary online references are linked beside the relevant findings in the notes.

Also checked ADB: no device attached. Probed plan15's shell wrapper with an
injected `false`: host `/bin/sh` returned1, no success banner. It was not found
broken on this host; the notes distinguish stronger future logging contracts
from demonstrated defects.

Skipped: native/Android builds (no game/build changes), live Wine captures
(shared prefix/proxy under active work), Ghidra/REA imports (existing exports
sufficed), audio listening, screenshot parity, Android/browser hardware/runtime
verification and dependency installs. No live processes, build locks, Wine
processes or Ghidra resources are held by this task.

## Integrator actions

After the 11a owner lands, merge the recovered facts into shared
`REVERSE_ENGINEERING.md` and link this briefing from `00-INDEX.md`. Correct plan13
Tasks1–2's file/format/synth assumptions; plan16's knock-off signature, scenery
scope and animation schema; plan17's bounds/route/field setup; plan18's conflict
and MiniDash storage; and the acceptance conditions in19–23 listed in the review.
Do not mark behavior or pixels verified from this static research.

Next concrete implementation prerequisite: factory/replay metadata with
independent human/demo driver, actual bike/character/field identities, pre-step
RNG/green boundary and required-field coverage. In parallel, an implementation
agent can start the MIDS/SF1 parser spike or settings model/rebinding using this
evidence; coordinate all shared `game_main.cpp` edits with 11a and the web owner.
Live original captures for finish/crash/combat/police/scenes/saves still need an
owned capture session. The notes spell out their relevant addresses and inputs.
