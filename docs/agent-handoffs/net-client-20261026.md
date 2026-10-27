# Task 6 client, end-to-end test and Task 5 bots — October 26, 2026

Baseline `99a1fec18a3a0d104c974f1c44afc2144acc8586`, existing
`/Users/archit/Dev/rr/.plan34-impl-20261026` checkout (`plan34-impl`).
Continues the next action in `plan34-impl-20261026.md`. Main checkout's active
frontend-acceptance files were untouched. No subagents or shared oracle state.

## Delivered

- `src/net/client.{hpp,cpp}`: main-thread channel polling, Hello/Welcome,
  lobby/control events, Start-driven race factory, real `Rollback`, local
  input sampling, repeat sends of unsealed inputs (16 words per packet),
  contiguous seal acknowledgements, periodic confirmed hash reports.
- Ping/pong on the active race channel: smoothed RTT, p95 spread over a bounded
  64-sample history, prediction lead limited to the rollback window,
  adaptive 0–3-tick input delay (caller chooses its cap). After initial clock
  acquisition, time advances at 59–61 Hz; no later clock jumps. Each client
  may use an independently offset monotonic clock.
- Reliable lobby/control and optional unreliable race channel. Closed/unavailable
  unreliable transport falls back to the reliable channel. Control-channel
  failure and Desync/Reject surface through `failed()`. RTC negotiation itself
  is still Task 9.
- Event queue capped at 128, polling budget 256 messages per channel/update,
  resend queue bounded by the prediction window, out-of-order frame acceptance
  limited to 1024 ticks ahead of the contiguous seal. Full race logs in Rollback
  are deliberately retained; the client accepts a maximum 216000 ticks (one hour)
  per race. WS callback queue already has its own 4096-message bound.
- Official results freeze advancement. A caller-provided stop tick supports
  deterministic test cutoffs without predicting beyond the inspected state.
- `tests/net_client_test.cpp`: **84** scenarios, each with a real `Server`,
  2/3/4/5/6/7/8 clients, RTT settings 0/50/150/300 ms, per-direction jitter
  ±30 ms (clamped at zero), loss 0/5/20%, plus 10% duplication/reordering on
  the unreliable loopback. Reliable control never loses packets. Separate
  client clock origins and drift up to 700 ppm. Each race runs the original
  360-tick countdown and 120 live ticks. All eight 60-tick hash checks must
  contain every seat and match; final confirmed state and official tables
  must match, corrections must stay ≤16 ticks, delay/rate/queues stay bounded.
  Flood, malformed packet and closed-control-channel checks are included.
- `tools/net_bots.cpp`, target **net-bots**: 2–8 real WebSocket clients with
  disc-backed Race/Rollback, complete-roster readiness, bounded timeout,
  confirmed hash comparison and official result verification. Built whenever
  native transport is enabled, independently of whether tests are enabled.
  Its diagnostic compatibility value hashes src source contents; its digest
  hashes the local EXE. This is not yet the production startup gate (source
  fingerprint plus self-test and verification of all reference data).

The first run found unreliable Ping overtaking reliable Hello. Ping now waits
for Welcome. Also fixed double-counting the elapsed update interval on the
first clock sample. Server, sealer, simulation, goldens and transport codecs
were not changed.

## Verification

- Release configure/full build: `cmake -S . -B build-p34-rel
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`,
  `cmake --build build-p34-rel --parallel 8` (transport OFF, existing cache).
- Release full CTest: pending final gate below.
- Debug configure/full build: corresponding `build-p34` commands, transport ON.
  Debug full CTest: pending final gate below.
- No C++ LSP service exposed in this client. `tools/agent_lsp.py --database
  build-p34-rel/compile_commands.json --check src/net/client.cpp`: healthy
  compilation database and clangd check, zero errors.
- Local WebSocket server on 18080, health on 18081. Two bots at cutoff 480:
  both confirmed hash `7077463161033616746`, official results agreed. Eight
  bots at cutoff 600: all hash `17601689475801012404`, official results agreed.
  These smokes include AI and traffic, unlike the humans-only E2E matrix.
  Re-run after the final clock fix is recorded below.
- Bots report `Outcome::left` at a shared sealed cutoff. These are real
  simulation/transport checks, **not** evidence of completing a course or
  of a renderer, sound or result screen. Network test hashes are not an
  original-game oracle claim.

Example (local server already running):

```sh
build-p34/net-bots --url ws://127.0.0.1:18080/net --clients 8 --ticks 600 --timeout 120
```

## Remaining scope

Task 6 Steps 3–5: RaceScreen polling/render integration, sound deduplication,
sealed-only irreversible side effects, chat/announcements, quit without pause,
renderer-only smoothing and optional network HUD. Task 6 is not marked complete.
Production compatibility startup gate, reconnect UI, RTC signalling, staging,
VPS deployment, device/network observations and full-course/production soaks
remain in their applicable plan steps. Nothing deployed. No Android/device,
Windows, browser or production verification performed for this slice.

## Ownership and resources

Owned code: client pair, client test, bots, `cmake/net.cmake`,
`cmake/net_tests.cmake`; docs: this handoff, plan 34 and implementation handoff,
PROGRESS/VERIFICATION continuations. `server/room.cpp` was inspected but has
no final diff. No other worker changes staged.
Claims: `f8e7160d-b339-4d49-a8e4-621338d7e1c7` and
`719b3b22-15a2-4ee5-bf1a-c4355ba292a7`, owner PID 26823.
Build dirs reused from the completed predecessor session: `build-p34` and
`build-p34-rel`, no competing build owners. Build/test receipts copied to
`/Users/archit/Dev/rr/.plan34-logs/net-client-20261026/` on completion.
