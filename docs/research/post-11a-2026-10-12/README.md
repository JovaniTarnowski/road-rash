# Implementation briefing after plan 11a

Research date: October 12, 2026. Baseline: `5dbf6adc415f864ef755ca292c237c001511d121`.
Read this alongside the plans, not as a claim that those plans have passed acceptance.
Scope: plans 11 and 13–23; plan 12 is superseded by 17. Plan 10 is included where
it blocks acceptance. Plan 11a is owned by another agent and was not edited.

## Read in this order

1. [Simulation and courses](simulation-and-courses.md): replay prerequisites,
   exact collision/weapon/nitro rules, animation fields, route census and finishes.
2. [Music](music.md): the actual MIDS song format, SF1 bank structure and missing
   factory instruments. This corrects plan 13's parser and synthesis assumptions.
3. [Options and HUD](options-and-hud.md): rebinding, analog controls, MiniDash,
   GDI needles and low-mode assets, extending the session 28 options trace.
4. [Rendering and platforms](rendering-and-platforms.md): corrections and test
   contracts for plans 14–15 and 19–23.

## Evidence and reproduction

Run from `native-port/`:

```sh
python3 tools/research_after_11a.py --self-check
```

The script verifies the unmodified disc executable's SHA-256
`73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`, matching
`analysis/inventory.json`. It reads the five course graphs and event tables,
five vehicle containers, six SBK banks, eleven MIDS files, 59 video paths and selected EXE
tables. The output is `analysis/oracle/post11a-research/census.json`, ignored by
Git. It includes selected assembly excerpts and all seven direct crash calls.
It contains disc-derived tables and must stay local.

Evidence grades throughout these notes:

- **Static binary:** address-keyed decompile checked against relevant assembly
  or original on-disc bytes. Specifies behavior to implement; does not prove a
  running native replay matches.
- **Corpus:** measured over the original files; catches false format or graph
  assumptions. Does not prove scene or tick parity.
- **Source audit:** present implementation at the baseline. Recheck after 11a
  lands, particularly `game_main.cpp`, `race.cpp` and oracle fields.
- **Open:** needs a live original capture, hardware, or a stated design decision.

No Wine captures or Ghidra imports were run: the oracle prefix/proxy and source
files are shared with active work. No simulation/render implementation, goldens,
build configuration, installed dependencies, or shared status summaries changed.
The research helper was run on the corpus and checked with synthetic MIDS cases;
native/Android builds were not needed for these documentation changes.

## Plan review and execution corrections

| Plan | What the next implementer should change or reuse |
| --- | --- |
| 10 prerequisite | Single-anchor default has landed, but `solo` still forces demo AI, bike/header metadata are incomplete, and replay setup differs from the game. Add independent driver selection and pre-step anchor evidence before calling a scenario human/exact. |
| 11 | Actor playback, crash and police numeric helpers already exist. Adapt the plan's old `AnimationState`/`Police` snippets to those interfaces. Scene/actor native construction comparisons remain open. |
| 12 | Do not implement; use 17. |
| 13 | Replace Standard MIDI as the mandatory corpus format with RIFF MIDS. Use `SBKGRNG*.MID`, `GRUNGE*.SBK` plus `RRDRUMS.SBK` for the AWE path. Klown is a song, not a bank. Factory presets are still needed for some notes. The mixer has no streaming PCM queue yet. The 59-video trigger audit is in the options note; JESSIE/DUEL are jukebox videos. |
| 14 | No device was attached when checked. Use an explicit ADB serial, non-destructive low-space injection, and distinguish rendering compute time from frame interval. Low view belongs to 18. |
| 15 | Required evidence must be enumerated, including new plans and scenario coverage. Parsing CTest skips alone misses empty goldens and skipped fields. Keep command exit status separate from displayed logs; the sample wrapper's injected failure did exit nonzero on this host. |
| 16 | Knock-off takes two damage inputs and a reason, mutates the victim, and has no random draw itself. Roadside collision has a real in-road BORR actor path; do not assume every pictured tree is solid. Animation belongs in Race tick state before extending snapshots. |
| 17 | All five courses have level-tagged finish events. The mileage table is menu text, not race bounds. There are 26 independent branch combinations, not ten global all-left/all-right routes. Field setup can have 15 riders including the player, not only eight. |
| 18 | Sound is Restroom's Win32 dialog 300; Configure Input init is distinct from rebinding. Conflicts unbind the previous action, not swap/refuse. MiniDash is a three-state value, currently incorrectly stored as bool. |
| 19 | Widened vertex types alone do not remove Q16 arithmetic overflow. Preserve original narrow/wrap behavior in the baseline path. Signed mip selection, per-worker scratch and LOD-aware scale tests are required. |
| 20 | MMPX's alpha/luminance tie-break invalidates arbitrary remap commutation. Its luma needs more than 16 bits for transparent colors. GPU state requires a GPU renderer; cache/parser/ZIP validation joins the fuzz work. |
| 21 | The longer-depth formula is unproven; a simple reciprocal fails on 4,297 entries. Wider projection must center only once; new geometry must be built, not just translated. Equal-depth perspective should explicitly use the original affine path for exactness. |
| 22 | Instrument the library, not only fuzz executables. Treat expected baseline simulation failures separately from new toolchain changes. Bound aggregate work/allocations and guard all asset/EXE readers. TSan is supported on Darwin ARM64. |
| 23 | Another session has started Task 1. Do not take ownership of its toolchain/CMake work. Screen transition semantics, asynchronous catalog/load failures and transactional browser import need stronger contracts before refactoring. |

## Practical sequence

After the 11a owner lands its fixes, reconcile interfaces and replay metadata.
Extract one stable race setup used by game and replay, with explicit human/demo
control and field identities. Add finish-event semantics and branch identities
before full-race or soak acceptance. Then complete animation-driven attacks,
contact/walls, weapons, police and native scene parity for Napa.

Music research and options implementation can proceed independently, but both
touch race/menu transitions in `game_main.cpp`; serialize those edits with 11a
and plan 23's callback conversion. Settings and renderer infrastructure can
proceed separately. Keep enhanced drawing out of baseline acceptance.

An unavailable `superpowers:*` skill in the plan boilerplate is not a prerequisite
to understanding these findings. This task was research, not execution of those
implementation workflows; no subagents were started.

## Remaining original-runtime work

The next implementation session still needs captures proving: human branch
entry and pre/post RNG state; field identity and scheduler order; finish crossing
on both branches; crash/double-contact/arrest sequences; weapon hits at actual
descriptor frames; low/inset HUD pixels; original saves/menu/cutscene triggers;
per-course natural scene construction. These notes give the relevant entry
points and schemas; none of those live comparisons was silently marked done.
