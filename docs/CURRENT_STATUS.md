# Current status and evidence

Generated from `agent-status.json`. Historical entries describe their own checkpoints.

| Area | Implementation | Verification / remaining acceptance | Evidence |
| --- | --- | --- | --- |
| Native build | Playable native game + research viewer | Strict Debug full build and99/99 CTests (October15 screen/canvas and NULL-slice integration); historical gate receipts below remain separate; Simulation acceptance separate | [VERIFICATION.md](VERIFICATION.md), [RUN_NATIVE.md](RUN_NATIVE.md), [codex-sim735-integrate-20261015.md](agent-handoffs/codex-sim735-integrate-20261015.md) |
| Simulation parity | Tick replay, frozen scheduler/half/slice state, countdown timers, measured player row, human-branch probe and scheduler node list | Demo600 compared fields match; sched900 first divergence893 RNG; human terrain200 trace identical (ring setup window walks, srand+0 pre draws); sched-human-cov2 1431/1431 identical with 0 golden-only and 0 uncovered native comparisons (strict acceptance met); Live-AI parking/waking, 70 unmodeled-but-constant original words (rider-half object, human brain window), billboard marking branch, contact eligibility, split/merge ring walking, traffic-field scenarios and full human parity | [SIM_SCENARIOS.md](SIM_SCENARIOS.md), [10-simulation-oracle.md](plans/10-simulation-oracle.md), [countdown-timers-20261015.md](agent-handoffs/countdown-timers-20261015.md), [codex-sim-research-20261015.md](agent-handoffs/codex-sim-research-20261015.md), [ring-terrain-20261016.md](agent-handoffs/ring-terrain-20261016.md), [coverage-20261017.md](agent-handoffs/coverage-20261017.md) |
| Napa race | Native scene/actors/traffic with remaining systems; police helpers exist but no live cops | Component comparisons are partial evidence; human physics gates demo-tuned; Car contact, actor nodes, human-gate ports, live police, and full-race acceptance | [NAPA_IMPLEMENTATION.md](NAPA_IMPLEMENTATION.md), [11a-napa-playtest-fixes.md](plans/11a-napa-playtest-fixes.md), [16-gameplay-systems.md](plans/16-gameplay-systems.md) |
| Other courses | Course data supported; composed renderer work pending | No full-course parity claim; Complete CITY/HIWAY/MEDLY/CANYN | [17-all-courses-port.md](plans/17-all-courses-port.md) |
| Frontend/settings | Menu/career/save paths exist; options gaps | Original save round-trip and options acceptance pending; Settings/screens and music completion | [18-settings-and-options.md](plans/18-settings-and-options.md), [13-music-and-frontend-completion.md](plans/13-music-and-frontend-completion.md) |
| Android | Native asset-free APK build | Hardware validation remains unverified; Device import/render/input/lifecycle/performance | [14-android-port.md](plans/14-android-port.md), [VERIFICATION.md](VERIFICATION.md) |
| Packaging | Source/commit/config/compiler stamp and manifest check | Current package receipt below if run; Owner signing/release acceptance | [USER_GUIDE.md](USER_GUIDE.md), [15-release-and-acceptance.md](plans/15-release-and-acceptance.md) |
| Agent tooling | Canonical instructions, claims, gates, capture validation, evidence index | Tooling regression suite; receipts below; Clients must honor claims; direct unmanaged commands bypass them | [AGENT_TOOLING.md](AGENT_TOOLING.md), [REVIEW.md](agent-review/REVIEW.md) |
| Linux/Windows | Not started | No builds yet; Builds, packaging, CI and acceptance on both | [33-linux-windows-ports.md](plans/33-linux-windows-ports.md) |

## Latest gate receipts

- **android-debug**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-android/receipt.json`.
- **android-release**: passed; source `50298bc9812a`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-android-release-final/receipt.json`.
- **native**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-final/receipt.json`.
- **oracle**: failed; source `c326c2b97d5a`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-strict-final/receipt.json`.
- **package**: passed; source `0f31a63359bf`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-package-final/receipt.json`.
- **sanitizers**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-asan-final/receipt.json`.
