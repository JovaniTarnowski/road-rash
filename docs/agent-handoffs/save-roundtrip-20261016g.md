# save-roundtrip-20261016g

Task: Plan 13/15 background — original save round-trip. READ-ONLY: no tracked
file edited, no builds, no captures. Baseline HEAD `242f718`.

Prior work: `savefile-rea-20261012.md` (trio CLEAN SWEEP MATCH, independently
re-verified). Status unchanged: **no original save has round-tripped yet**
(plan 13 Task 6 Step 1 needs a Wine-driven `.RRS` capture + `RR_ORACLE_SAVE`
proxy work, explicitly out of scope here).

## Evidence (2 pinned REA calls, budget exhausted)

Env per `docs/TOOLING.md`: Node v24.13.0, `GHIDRA_INSTALL_DIR=.../ghidra/12.1.4/libexec`,
JDK 21; `rea-agents@6.4.0`, `--format md`, exit 0 both. EXE digest in both
reports `73d7f9744b5b1a9482b3909950cdf3fe34119fe13d023b031e9c6739a60f18a2`
(matches `shasum` of `../cdrom/ROADRASH/ROADRASH.EXE`).

- `0x0042efc0` checksum — `ev_7d7edb41f300330f2e394dd0b5a7e546aa17c290de5103dee4b9fb7a219b7e98`.
  Pseudocode MATCHes `src/career.cpp:175`: countdown `n`, `sum += b`,
  `weighted += n*b`, `alt += (n odd ? b : -b)`, return
  `(alt<<16 | sum&0xffff) ^ weighted`.
- `0x0042f110` reader — `ev_fa306a79fbd62e918851298d827c0bad4ce4aff1e86679f4b8f14502634356ab`.
  Confirms: 0xa0-byte ReadFile; `0xcafedead` check; `0x0042efc0` over 0x9c
  bytes; `0x004078f0` reset; 8-dword state copy file+8 -> `0x004b8a10`;
  rider `0x00415fc0(state+1)`; 10x3-dword loop file+0x28 ->
  `0x004c6b40 + rider*12` stride 0x204; force mode 2; unsaved = 0.
- Writer `0x0042f000` not re-fetched (prior MATCH stands).

Raw reports + detail: `analysis/save-20261016g/` (`rea-42efc0-savecrc.md`,
`rea-42f110-saveread.md`, stderr logs, `notes.md`).

## Exact layout (0xa0 bytes, LE; `src/career.cpp:186-222`)

+0x00 checksum of bytes[4..0x9f]; +0x04 magic `0xcafedead`;
+0x08 player/character/level/qualified/bike/course/place/flags;
+0x10 cash u32; +0x14 20-byte NUL-terminated name;
+0x28 10x3 u32 `relations[p][rider][k]` at `40+12*p+4*k`
(`rider = character_rider(character)`; reset-to-defaults first, then slice
overwrite). Native-only: out-of-range/size rejection, Big Game force.

## Round-trip test plan (synthetic files only)

For the plan 13 Task 6 owner (`tests/career_test.cpp` slice): (1) fixed-vector
layout asserts incl. recomputed checksum; (2) read/write identity matrix over
riders 0..8, cash/name/relation corners, all state-byte corners;
(3) truncation/magic/bit-flip/garbage rejection (+ native-only range checks
documented as such); (4) random-buffer checksum vs from-spec reimplementation;
(5) original `.RRS` byte-compare + `0x004b8a10` check — needs capture, NOT
this slice. Existing synthetic coverage already in `career_test.cpp:82-105`.

## NOT-changed list

`src/career.cpp`, `src/career.hpp`, `tests/career_test.cpp`,
`docs/PROGRESS.md`, `docs/REVERSE_ENGINEERING.md`, `docs/VERIFICATION.md`,
`docs/TOOLING.md`, `docs/plans/13-music-and-frontend-completion.md`,
`oracle/aweman_proxy.c`, `CMakeLists.txt`, `analysis/inventory.json`,
`../cdrom/ROADRASH/ROADRASH.EXE`. Written: only this handoff +
`analysis/save-20261016g/`. No live REA/Wine/Ghidra processes held; no build
locks; no commits.
