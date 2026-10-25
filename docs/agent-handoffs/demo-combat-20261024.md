# demo-combat-20261024

Task: Proximity-gated autopilot combat and corrected two-minute demo render

Baseline: e865dd64f1d317f072f1d6f12fd071ee0b053407

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-demo-combat-20261024`
Owner PID: 95735
Resource token: `a1a1c4f5-fa14-4e10-931b-48b8bb7b520f`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/race_helpers.cpp`
- `/Users/archit/Dev/rr/native-port/tests/race_screen_test.cpp`
- `/Users/archit/Dev/rr/native-port/docs/DEMO_VIDEO.md`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`

## Progress / evidence

Replaced unconditional 40-tick punches in `autopilot` with native reach checks
against active same-road riders. Swing start is suppressed while another is
active; native combat still resolves the hit. Regression cases added to the
existing race-screen test. See docs/DEMO_VIDEO.md for the corrected recipe.

Release full build + focused race-screen pass; Debug full build + 105/105
parallel CTests pass, no skips (209.96 seconds). Local gameplay audit records
HIWAY swings 1640/1673 and player hit target 11, CANYN swings 1847/2134 in the
selected window. Frame QA shows normal riding and attacks beside real targets
on either side. Output/evidence folder:
`analysis/demo-video/twitter-2min-proximity-combat/`.

## Checks / remaining work

Corrected two-minute render completed with `--warmup 960 --courses 2 3 0 1 4`.
Final assembly verified: 120.000 seconds, 7,200 frames, 1280x960/60 fps,
H.264/yuv420p + AAC. Documentation and checks complete. Commit/push includes
only owned source/test/docs paths; source claims released afterward. Initial periodic-punch cut is retained
locally but superseded. No simulation or manual-control rules were changed;
no new original-game parity claim. No Android/sanitizer/oracle capture run.

## Live processes / resources

All export and test processes completed; no game/encoder/test process remains. Release build reused from this
agent's prior session: `build-agent-demo-video-20261024`; Debug gate used owned
`build-agent-demo-video-debug`. No Wine/Ghidra resources held.
