# Mano-a-mano trace and multiplayer plan (October 26, 2026)

- **Task:** review the port's status; investigate mano-a-mano; write a
  multiplayer plan.
- **Baseline:** `44679e4`. This is a documentation-only session: no build, no
  test and no oracle run. No processes or resources are held.

## Owned and changed files

- New: `docs/MULTIPLAYER.md`, `docs/plans/34-multiplayer.md` and this handoff.
- Edited:
  - `docs/plans/09-multiplayer.md`: superseded banner;
  - `docs/plans/00-INDEX.md`: plan 09 row, plan 34 row, definition-of-done note;
  - `docs/plans/15-release-and-acceptance.md`: Task 4 now points to plan 34;
  - `docs/REVERSE_ENGINEERING.md`: new section, front-end note;
  - `docs/PROGRESS.md`: timeline entry.
- Not touched: the in-flight changes from other workers (VERIFICATION,
  SIM_SCENARIOS, oracle proxy, `race.cpp`, `rri.cpp`, CMake).

## Evidence

- The disc EXE SHA-256 matches `analysis/inventory.json`.
- Imports came from the PE import table: TAPI32 ×23, WSOCK32 ×19 by ordinal,
  plus the KERNEL32 Comm API.
- Call sites were mapped with `analysis/original-disassembly.txt`.
- Menu items and strings were read from the EXE tables and ENU.DLL with an
  ad hoc script, using the same offsets as `read_frontend_data`.
- Function readings come from `analysis/decompiled/*.c`.

## Remaining

The whole of plan 34. Start with Task 1: WSOCK32 IAT hooks in
`oracle/aweman_proxy.c` and a two-instance capture. Its Step 3 lists the
traces this static pass left open: lobby messages, resend and timeouts, the
networked grid, `DAT_0049bce8`, the remote mover and the type 5/7/8 meanings.
