# demo-video-20261024

Task: Offline engine gameplay video capture and reusable two-minute demo renderer

Baseline: be2ded71ee4f4bb19958b85edd8cab64e8f93887

Worktree: `/Users/archit/Dev/rr/native-port`
Build directory: `/Users/archit/Dev/rr/native-port/build-agent-demo-video-20261024`
Owner PID: 95735
Resource token: `befdc209-75ae-4fa5-bc11-00d613fc1f13`

Owned files:

- `/Users/archit/Dev/rr/native-port/src/offline_capture.hpp`
- `/Users/archit/Dev/rr/native-port/src/game_main.cpp`
- `/Users/archit/Dev/rr/native-port/src/game_types.hpp`
- `/Users/archit/Dev/rr/native-port/src/race_screen.cpp`
- `/Users/archit/Dev/rr/native-port/tools/render_demo.py`
- `/Users/archit/Dev/rr/native-port/tests/demo_capture_test.py`
- `/Users/archit/Dev/rr/native-port/docs/DEMO_VIDEO.md`
- `/Users/archit/Dev/rr/native-port/docs/PROGRESS.md`
- `/Users/archit/Dev/rr/native-port/docs/VERIFICATION.md`
- `/Users/archit/Dev/rr/native-port/README.md`

## Progress / evidence

Implemented `src/offline_capture.hpp` and capture CLI flags in the quick-race
path. RaceScreen renders at one tick/frame with original presentation and
native mixer PCM. `tools/render_demo.py` streams raw RGB to FFmpeg through
an inherited descriptor and produces five course clips plus the two-minute
H.264/AAC assembly. `docs/DEMO_VIDEO.md` documents reuse and low-level format.

Completed local video: `analysis/demo-video/twitter-2min/road-rash-native-demo.mp4`:
120.000 seconds, 7,200 frames, 1280x960, 60 fps, AAC stereo, 299,795,586 bytes.
Five 24-second course clips, `receipt.json`, engine/encoder logs, visual QA grid
and full audio/video decode log are in the same ignored directory. No black
intervals >=0.1 s. All five midpoint scenes inspected, plus early/late Napa.
Original gameplay presentation, engines/effects, no race soundtrack.

## Checks / remaining work

- Release full configure/build and 105/105 CTests pass, zero skips.
- Standalone capture integration passes on Release and Debug: repeat hashes
  across launch times, exact samples/frames, motion, non-silence, invalid CLI.
- Five-course 60-frame and uneven 62-frame wrapper assemblies pass.
- Python syntax checks and whitespace check pass.
- Debug full configure/build and 105/105 parallel CTests pass, zero skips
  (193.58 seconds); log copied to `analysis/demo-video/twitter-2min/ctest-debug.log`.
  Initial serial Debug run was stopped for runtime; its orphaned owned
  hires_render-test PID 2932 was explicitly terminated. Final gate is the
  fresh full `ctest --parallel 8` run, not the interrupted serial attempt.
- C++ client LSP unavailable. Owned database preflight healthy (187 entries).
  clangd game_main zero errors; RaceScreen reports two ExtractFunction tweak
  failures rather than compiler diagnostics. Both build configurations pass.
- Implementation, artifacts and verification complete. Commit/push uses only
  the explicit owned source/docs/test paths. Resource claims released after push.
- Not run: Android/device, sanitizers, packaging, new oracle captures. No new
  simulation-fidelity acceptance claimed; no Wine or Ghidra mutation.

## Live processes / resources

All export and test processes completed; no game, encoder or test process
remains. Owned builds retained: `build-agent-demo-video-debug` and
`build-agent-demo-video-20261024`. No Wine/Ghidra resources held.
