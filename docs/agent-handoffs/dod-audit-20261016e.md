# Plan 32 slice G hygiene audit — plan-00 Definition of Done vs fresh evidence (20261016e)

Scope: READ-ONLY audit at HEAD `1b79351` ("hud: resolution pen width, needle order, solo target gap normalization", 2026-10-15). No tracked file edited; no builds, captures, REA, or LSP run. `git log`/`git status` + `rg`/`fd` reads only. This file (`docs/agent-handoffs/dod-audit-20261016e.md`) is the sole write. No commits.

## Sources consulted (all read-only)

- DoD text: `docs/plans/00-INDEX.md` §"Definition of done for the whole port" (7 items, lines 212–223).
- Status matrix: `docs/CURRENT_STATUS.md` (generated from `agent-status.json`, `reviewed_on` 2026-10-15) + `docs/agent-status.json` gates block.
- Gate receipts on disk: `analysis/agent-gates/*/receipt.json` (canonical six referenced by CURRENT_STATUS + newest by mtime: `oracle-b008dfc…`, `native-76357b0f…`, `android-debug-00a97721…`).
- VERIFICATION.md: read-only; newest dated gate sections are the Oct-14 integrator post-merge gate (95/95, `build-integrator`) and older session gates; the freshest full-suite evidence lives in handoffs, not VERIFICATION.md.
- Recent handoffs: `gate-check-20261016.md` (baseline `b4d3c0c`, 99/99), `gate2-20261016.md` (baseline `7992dfe`, 99/99), `tick735-target-20261016.md` (stale-binary finding), `human-terrain-20261016.md`, `sim-human-proof-20261016.md`, `hud-needle-20261016d.md` (HEAD commit work), `bench-20261016d.md` (slice E, baseline `8851ef6`), `police-spawn-20261016d.md`.

## Global staleness flags (apply to every item below)

1. **Canonical CURRENT_STATUS receipts are 252 commits stale.** `25daed6d1f41..HEAD` = 252 commits; package/android-release/oracle receipts sit at `b76ffc8`. All six canonical receipts record `dirty: true` trees. Do not cite them as HEAD evidence.
2. **Freshest full native gates are handoff-only, 1–4 commits behind HEAD.** `gate-check-20261016` 99/99 at `b4d3c0c`; `gate2-20261016` 99/99 at `7992dfe` (4 commits behind `1b79351`; delta since is docs + HUD-needle slice `hud-needle-20261016d`, whose owned-dir evidence is `hud` 1/1 + neighbors 4/4 + full owned build exit 0 — not a full re-gate). VERIFICATION.md has no Oct-15/16 gate lines; the Oct-14 post-merge 95/95 entry is superseded by these handoffs.
3. **`build/` binary is stale (predates `b4d3c0c`).** `build/oracle-sim-test` linked Oct-15 19:26; NULL-slice source fix `b4d3c0c` landed 19:45–19:50 (`tick735-target-20261016.md`, `human-terrain-20261016.md`). Any tick-735 divergence quoted from `build/` is a stale-binary artifact; authoritative figures come from owned post-fix binaries (`build-agent-gate-20261016`, `build-agent-gate2-20261016`).

## DoD item-by-item

### DoD 1 — Clean-checkout import → runnable macOS .app (no Homebrew dep) + Android APK (runtime import, never bundled)
- **Status:** PARTIAL. Assembly proven; standalone-launch/import freshness and device install missing.
- **Evidence:** `docs/agent-status.json` gates `android-debug`/`android-release` = `passed`, coverage `hardware: not run`, receipts `analysis/agent-gates/playtest-audio-ai-loading-android/receipt.json` (`25daed6`, dirty) and `p0-p1-android-release-final/receipt.json` (`b76ffc8`, dirty); `package` = `passed` at `b76ffc8` dirty (`p0-p1-package-final/receipt.json`); newest-by-mtime `android-debug-00a97721…/receipt.json` = `passed`, source `120eb76e`, dirty. VERIFICATION.md Session 10 records standalone `.app` + `tools/ci.sh` passing historically; no Oct-15/16 clean-checkout/package re-run found.
- **Staleness:** STALE — all receipts dirty and ≥250 commits behind HEAD; newest mtime receipt still dirty-tree at `120eb76e`.
- **Next step (owner: integrator):** run clean-checkout native + Android + package gates at HEAD in owned dirs, record receipts + standalone-launch/import outcome; cite receipt paths in VERIFICATION.md.

### DoD 2 — All 5 courses × 5 levels playable in Big Game + Thrash; opponents/traffic/police/weapons/crashes/cash/shop/saves like the original
- **Status:** NOT MET. Playable yes; parity/live-systems no.
- **Evidence:** CURRENT_STATUS matrix rows: Simulation parity "strict acceptance unmet", Napa race "remaining: car contact, actor nodes, human-gate ports, live police, full-race acceptance", Other courses "no full-course parity claim". `police-spawn-20261016d.md`: helpers exist (`src/police.*`, `police_test` goldens `napa-police-spawn.tsv`/`napa-police-arrest.tsv`) but **no live cops**; live-wiring plan + capture spec (`napa-cops` L3+, `city-cops`) open, proxy/pipeline down. `sim-human-proof-20261016.md`: human golden `sched-human-20261016` (1431 ticks, `demo_hold=0`, control-path response) diverges tick 2 (`ai_target_x` 18176 vs 0); strict acceptance `unexpected-failure`. Fresh replay: `gate2-20261016` sched600 600/600 identical, sched900 first divergence tick 893 RNG (707629878 vs 2897718112). PROGRESS.md: "no police", "no original save round-tripped yet".
- **Staleness:** FRESH (Oct-16 handoffs at `b4d3c0c`/`7992dfe`) for the negative result; stale-binary caveat applies only to `build/`-quoted tick-735 lines.
- **Next steps:** (a, capture owner) restore oracle capture pipeline, record `napa-cops`/`city-cops`/contact/traffic-field scenarios; (b, integrator) rebuild owned binary at HEAD and re-baseline sched900 (expect 893 RNG, not 735) + human tick-2 gap.

### DoD 3 — Oracle tick goldens (plans 10/11/16/17 scenarios) + natural-frame pixel goldens (plans 11/16/17/18) pass
- **Status:** NOT MET — diagnostic progress only.
- **Evidence:** `agent-status.json` gate `oracle` = **failed** (`p0-p1-strict-final/receipt.json`, `b76ffc8`, empty coverage); newest oracle receipt `oracle-b008dfc…` (source `59c8bf9`, dirty) ran `verify_sim.py --mode acceptance` → exit 1. Positive partials: `gate2-20261016` demo sched600 600 ticks identical; component/natural-frame display-list replays cited in 00-INDEX (600 CITY / 901 Napa moving / Napa start; 7 CITY + Napa 0/150/300/750 frames, 0 pixel diffs) are replay-over-original-lists, not native construction — 00-INDEX itself flags native construction uncompared. `human-terrain-20261016`: human terrain gap tick 1 (`end` 132 vs 133); back-walker parity unproved.
- **Staleness:** FRESH for demo-600 pass + 893/ tick-2 divergences (Oct-16 owned binaries); STALE for canonical oracle receipt (`b76ffc8`).
- **Next step (owner: integrator, with capture owner for fixtures):** `make verify-sim` strict at HEAD with validated human proof, no skips; per plan-32 slice C acceptance. Moving first divergence later is progress, not acceptance.

### DoD 4 — All audio (effects, RRA, race MIDI, engines, sirens) + all cutscenes at original trigger points
- **Status:** PARTIAL. Effects/RRA/videos proven; race MIDI, sirens, car engines open.
- **Evidence:** VERIFICATION.md Session 9: 59/59 AVIs byte-match FFmpeg (24,900 RGB frames, 67,840,002 samples); `wav-test --corpus` 72 effects + 14 RRA decode. REVERSE_ENGINEERING.md (Oct-12): all-59-video triggers present (`results_video`, showroom, jukebox handlers ported `bd31058`); SBK decision recorded (option B native sampler, `src/race_music.cpp`), race continues silently on missing bank; siren `0x00435ae0` + spatial `0x00435840` REA-confirmed but `src/sound.cpp:349` siren still `default: break` (per `police-spawn-20261016d`); `police-spawn-20261016d` event-19 → siren wiring open. PROGRESS.md status table: "race MIDI (plan 13), sirens and car engines (plan 11) not done". No Oct-15/16 audio re-verification found.
- **Staleness:** STALE — no fresh (Oct-15/16) audio/cutscene evidence; codec equality ≠ trigger/timing proof (VERIFICATION.md caveat stands).
- **Next step (owner: capture owner for triggers, integrator for gates):** scenario captures exercising race-MIDI/siren/engine triggers + cutscene outcome transitions; re-run `verify_video_audio.py` + audio suites at HEAD.

### DoD 5 — Controls: keyboard, gamepad (incl. MANGMI AIR X built-ins), touch; plan-14 hardware checklist run on device
- **Status:** NOT MET — assembly only, zero hardware runs.
- **Evidence:** Every Android receipt on disk has coverage `hardware: not run` (canonical + `android-debug-00a97721…`). VERIFICATION.md: `adb devices` no devices across all sessions; Session 7/21 controls are synthetic-event intent tests, "no physical gamepad interaction"; plan-32 slice F checklist (serial, install/import, launch, gamepad/touch, controller loss, background/foreground, renderer recovery, frame-time/memory/thermal, sustained race, storage/interrupt/persistence) entirely open. CURRENT_STATUS Android row: "hardware validation remains unverified".
- **Staleness:** FRESH negative — consistently evidenced through Oct-16 (no device attached in any handoff).
- **Next step (owner: integrator/capture owner whoever holds the handheld):** attach MANGMI AIR X, record serial, run slice-F checklist on a HEAD build; APK assembly must not be cited as device evidence.

### DoD 6 — Every original options screen + native Display/Sound/Pause (controller-only); render to 2560×1920 (plan 19); local upscales (plan 20)
- **Status:** PARTIAL. Screens exist with gaps; resolution/perf measured in Debug only; upscale attach open.
- **Evidence:** CURRENT_STATUS Frontend/settings row: "options gaps; save round-trip and options acceptance pending". `hud-needle-20261016d` (HEAD commit): needle pen width/order + solo-gap normalization fixed against `0x00446c30`/`0x00438550`/`0x00446b80`, owned-dir 4/4 pass; multi-row opponent-list overlay, low-mode layout/blit, GDI band/cap SHAPE explicitly open (need captures). `bench-20261016d` (baseline `8851ef6`, 1 commit behind HEAD): full 12-row whole-frame matrix (s=1/2/4 × 4:3/16:9 × indexed/RGB) in **Debug** — output hashes + alloc counts identical to post-fix Release baselines (horizon −1 alloc retained), p50 ~6.5–8.8× Release (unoptimized build, not a regression); Release rerun, per-case memory, controlled cold-cache, alloc elimination (378–1674/frame), cancellable first-enable upscale attach all open. Plan-32 §E boxes: side panels + horizon retention done; benchmark-at-all-configs + upscale-attach open.
- **Staleness:** FRESH for Debug matrix + HUD slice (Oct-16); STALE for acceptance (no Release rerun at HEAD; device budgets absent).
- **Next step (owner: integrator):** Release rerun of the exact `race_cache-test --benchmark` matrix in an owned dir + `hires_render-test` verdict; opponent-list/low-res capture spec to capture owner.

### DoD 7 — Docs describe every recovered format and every known deviation
- **Status:** PARTIAL. Active maintenance; known gaps explicitly listed, not closed.
- **Evidence:** `agent-status.json` `reviewed_on` 2026-10-15 + regenerated CURRENT_STATUS.md (via `sim-human-proof-20261016` commits `240e54f`/`1a18b39`); plan-32 slice G boxes (save round-trip, career/shop/level-up, jukebox/settings, outcome movie/music, upscale items, canonical-matrix refresh, clean-checkout gates, signing/multiplayer decisions) all unchecked. 00-INDEX (Oct-14 review) "ported but never compared" + "not started or stand-ins" lists still stand (walls/crash stand-ins, RRAN animation/shadows, AI overtake/fight states, attack timing, split stitching, CITY/HIWAY/CANYN/MEDLY scenes, MIDI/sirens/car engines, jukebox/mix/controls, attract demo, busted/wrecked selection, device perf). `tick735-target` corrects a misattribution (bike-not-half node, side `0x8000`) that supersedes `codex-sim-research-20261015` arithmetic — needs folding into shared docs by integrator.
- **Staleness:** MIXED — matrix regeneration fresh (Oct-15); deviation lists aging as Oct-16 handoffs accumulate (tick735 correction, 893 RNG suspect, human terrain gap, bench numbers not yet folded in).
- **Next step (owner: integrator):** refresh canonical matrix + PROGRESS/RE/VERIFICATION from Oct-16 handoffs (tick735 stale-binary note, 893 RNG + contact-eligibility suspect, human tick-2 + terrain tick-1 gaps, bench Debug matrix, police live-wiring spec), preserving receipt source identity and diagnostic-vs-strict separation.

## Bottom line

No DoD item has fresh, source-identified HEAD evidence of completion. Closest: DoD 3 demo-tick replay (sched600 600/600, `gate2-20261016`) and DoD 6 rendering output identity (hashes/allocs match post-fix baselines, `bench-20261016d`); both are necessary-but-insufficient (strict acceptance and Release/device acceptance remain). Canonical CURRENT_STATUS receipts are dirty-tree and 252 commits stale — refresh clean-checkout gates at HEAD before any completion claim. Blockers in order: capture pipeline (slices C/D/G scenarios) → strict single-anchor human replay (slice C) → live police/crash/course bounds (slice D) → Release + device budgets (slices E/F) → slice G close-out.
