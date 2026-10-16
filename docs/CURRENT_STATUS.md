# Current status and evidence

Generated from `agent-status.json`. Historical entries describe their own checkpoints.

| Area | Implementation | Verification / remaining acceptance | Evidence |
| --- | --- | --- | --- |
| Native build | Playable native game + research viewer | Strict Debug full build and99/99 CTests (October15 screen/canvas and NULL-slice integration); historical gate receipts below remain separate; Simulation acceptance separate | [VERIFICATION.md](VERIFICATION.md), [RUN_NATIVE.md](RUN_NATIVE.md), [codex-sim735-integrate-20261015.md](agent-handoffs/codex-sim735-integrate-20261015.md) |
| Simulation parity | Tick replay, frozen scheduler/half/slice state, countdown timers, measured player row and human-branch probe | Demo600 compared fields match; sched900 first divergence893 RNG; declared human terrain first hidden-state gap tick1 (front point end132/133), coarser RNG189; strict acceptance unmet; Human terrain setup ordering/parity, contact eligibility, traffic-field scenarios and full human parity | [SIM_SCENARIOS.md](SIM_SCENARIOS.md), [10-simulation-oracle.md](plans/10-simulation-oracle.md), [countdown-timers-20261015.md](agent-handoffs/countdown-timers-20261015.md), [codex-sim-research-20261015.md](agent-handoffs/codex-sim-research-20261015.md), [codex-sim735-integrate-20261015.md](agent-handoffs/codex-sim735-integrate-20261015.md) |
| Napa race | Native scene/actors/traffic with remaining systems; police helpers exist but no live cops | Component comparisons are partial evidence; human physics gates demo-tuned; Car contact, actor nodes, human-gate ports, live police, and full-race acceptance | [NAPA_IMPLEMENTATION.md](NAPA_IMPLEMENTATION.md), [11a-napa-playtest-fixes.md](plans/11a-napa-playtest-fixes.md), [16-gameplay-systems.md](plans/16-gameplay-systems.md) |
| Other courses | Course data supported; composed renderer work pending | No full-course parity claim; Complete CITY/HIWAY/MEDLY/CANYN | [17-all-courses-port.md](plans/17-all-courses-port.md) |
| Frontend/settings | Menu/career/save paths exist; options gaps | Original save round-trip and options acceptance pending; Settings/screens and music completion | [18-settings-and-options.md](plans/18-settings-and-options.md), [13-music-and-frontend-completion.md](plans/13-music-and-frontend-completion.md) |
| Android | Native asset-free APK build | Hardware validation remains unverified; Device import/render/input/lifecycle/performance | [14-android-port.md](plans/14-android-port.md), [VERIFICATION.md](VERIFICATION.md) |
| Packaging | Source/commit/config/compiler stamp and manifest check | Current package receipt below if run; Owner signing/release acceptance | [USER_GUIDE.md](USER_GUIDE.md), [15-release-and-acceptance.md](plans/15-release-and-acceptance.md) |
| Agent tooling | Canonical instructions, claims, gates, capture validation, evidence index | Tooling regression suite; receipts below; Clients must honor claims; direct unmanaged commands bypass them | [AGENT_TOOLING.md](AGENT_TOOLING.md), [REVIEW.md](agent-review/REVIEW.md) |

## Latest gate receipts

- **android-debug**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-android/receipt.json`.
- **android-release**: passed; source `50298bc9812a`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-android-release-final/receipt.json`.
- **native**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-final/receipt.json`.
- **oracle**: failed; source `c326c2b97d5a`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-strict-final/receipt.json`.
- **package**: passed; source `0f31a63359bf`; commit `b76ffc899e20`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/p0-p1-package-final/receipt.json`.
- **sanitizers**: passed; source `c231b349b122`; commit `25daed6d1f41`; receipt `/Users/archit/Dev/rr/native-port/analysis/agent-gates/playtest-audio-ai-loading-asan-final/receipt.json`.
