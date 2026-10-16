# palette-oracle-20261016

Task: palette SetEntries oracle hook + native comparison

Baseline: 7819378443c873a473814d973b5780b765e85f0b

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-palette-oracle-20261016`
Owner PID: 66463
Resource token: `377d972e-44de-46c7-bd0d-4fab3d14d0a7`

Owned files:

- `/Users/archit/Dev/rr/native-port/oracle/aweman_proxy.c`
- `/Users/archit/Dev/rr/native-port/tests/hud_test.cpp`

Stale claim `terrain-integrate-20261015` (owner PID 53167, confirmed gone
with `ps`) was reaped before claiming the proxy.

## Changes

- `oracle/aweman_proxy.c` (all env-gated; default captures unchanged):
  - `RR_ORACLE_PALETTE=1`: vtable patch of `IDirectDrawPalette::SetEntries`
    (+0x18 on `DAT_00475424`), installed at course entry / first frame.
    Companion `<scenario>-palette.raw`: "RRPL", version 1, then per upload
    9 uint32 {seq, frame, timeGetTime ms, return address, source pointer,
    start, count, countdown `0x4753c0`, fade counter `0x482350`} and the
    PALETTEENTRY bytes. Frames gain `palette.uploads`, `countdown`,
    `race.mode` (`0x4753b0`) and `pump.active` (`0x4753b8`).
  - `RR_ORACLE_EXIT_FADEOUT=1`: finish the trace and exit 0 inside the
    fade-out's last upload (site `0x415f4c`, counter 1).
  - `RR_ORACLE_KEYS="frame:vk[:mode],..."`: post WM_KEYDOWN/WM_KEYUP to the
    game window after that frame; optional diagnostic game-mode write.
- `tools/oracle_palette.py`: reader/dumper for the companion.
- `tests/hud_test.cpp`: capture-gated comparison of every upload with the
  native maps.
- Docs: `REVERSE_ENGINEERING.md` (upload call-site table, runtime facts),
  `VERIFICATION.md` (commands, numbers, gaps).

## Evidence

- Accepted capture `analysis/oracle/napa-palette.tsv` (+ receipt, immutable
  run `napa-palette.captures/8cce6145…`), 311 frames, validated; 130
  uploads. Two scratch probes (no mode override; mode override) preceded it
  and were not published into `analysis/oracle`.
- Native vs original: 0/33,280 entries differ across all 130 uploads (fade-in
  63, expiry full, pause half, resume full, quit half, fade-out 63). Fade
  spans 867 ms each. Order and counters match.
- Native palette (`Palette::from_raw(PALETTE.RAW)`) equals the uploaded full
  palette exactly (entry 0 black in both).

## Divergences / open

- No native divergence found in palette values.
- Original renders race frames 0–1 before the fade-in's first upload under
  the preceding (unlogged) palette; native keeps the previous screen. Not
  compared.
- Pause/quit prompt evidence needs a mode 1 override because the course
  bootstrap races in attract mode; a genuine human-mode capture would remove
  that caveat.
- `0x41642d` (no-fade start) and `0x416667` (`0x482760`) uploads are not
  reached by this bootstrap.

## Checks

- `sh oracle/build.sh`: exit 0 (one -Wformat-truncation warning, same
  pattern as the existing raw companions).
- Full `cmake --build build-agent-palette-oracle-20261016 --parallel`: exit 0;
  `ctest --test-dir build-agent-palette-oracle-20261016 --output-on-failure -j8`:
  99/99 passed. `hud-test` prints
  `palette uploads vs original: 130 uploads, 0/33280 entries differ; fade spans 867/867 ms`.

## Live processes / resources

None. No Wine process left running; prefix/install claims were held only by
the capture wrapper during runs. The installed oracle DLL is the new proxy
(sha256 48f9bdf5…), backwards compatible.
