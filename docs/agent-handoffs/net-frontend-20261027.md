# net-frontend-20261027

Task: Finish adopted Task7 lobby/frontend and startup/race handover

Baseline: 71e0bed65174d5fdc1b62de15b71161e1db58900

Worktree: `/Users/archit/Dev/rr/.plan34-impl-20261026`
Build directory: `/Users/archit/Dev/rr/.plan34-impl-20261026/build-agent-net-frontend-20261027`
Owner PID: 57957
Resource token: `275a9452-cec3-4b25-adfa-dfb67ce95acd`

Owned files:

- `/Users/archit/Dev/rr/.plan34-impl-20261026/cmake/net.cmake`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/cmake/net_tests.cmake`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/career.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/career.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/frontend.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/frontend.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/frontend_net.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/native_screens.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/native_screens.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/native_strings.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/native_strings.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/net/client.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/net/client.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/net/compat.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/net/compat.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/net/mano.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/net/mano.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/settings.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/settings.hpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/game_main.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/src/platform_web.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/tests/settings_test.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/tests/net_mano_test.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/tests/net_frontend_test.cpp`
- `/Users/archit/Dev/rr/.plan34-impl-20261026/docs/agent-handoffs/net-frontend-20261027.md`

## Progress / evidence

Completed Task 7 after adopting the unfinished lobby/frontend changes at baseline 71e0bed. Host/Join, public rooms/Refresh, code/link paste, spectator choice, roster and chat, saved F1–F8 phrases, course vote, real Client handover, official results and rematch are wired. Lobby polling stops immediately on Start so the race client retains queued Frames/LogChunks. Result paths preserve career saves and career RNG. Native audio uses compact rider indices for sparse seats.

Production Hello verifies all 341 reference files, hashes the canonical manifest with SHA-256 and combines a source digest with a synthetic multi-human simulation. Bots now call the same gate as the game; their old executable-only digest would have rejected interactive peers. Browser WebSocket transport and `?room=` compile. Android manifest/activity were claimed but not edited: app-link registration, transport wiring and device cross-play remain Task 10.

Checks:

- Debug full CTest: 117/117, 444.93 seconds.
- Release full CTest: 116/116, 46.78 seconds.
- Clang 22 ASan/UBSan `net_mano|net_frontend`: 2/2, including full reference verification (200.33 seconds).
- Final production-gate frontend test passes with the actual disc. The in-process end-to-end uses a scripted agreed cutoff at 420 ticks, including live simulation; this does not claim full-course completion.
- Final native `rr-game` builds with real transport; dummy SDL three-frame invite smoke exits 0, using localhost port 1 and no external server.
- Emscripten Release `RR_WEB_BROWSER=ON` game compiles with Asyncify and JS WebSocket transport. Python >=3.10 required: prefix PATH with `/opt/homebrew/bin`.
- 1x/2x room/roster/chat/vote/results/unofficial captures inspected; captures and original data stay outside Git under `/Users/archit/Dev/rr/.plan34-logs/net-menus-20261027`.
- clang-format 22.1.4 and `git diff --check` pass. No live C++ LSP exposed; compiler and test builds are the fallback.

Builds: `build-p34` (Debug/native transport), `build-p34-rel` (Release/no socket dependency), `build-p34-fuzz` (ASan/UBSan), `build-p34-web` (browser). Logs `/tmp/rr-mano-*`. Full gate results precede the final audio index fix, then native game compile/smoke checked that path. No main-checkout frontend work or pre-existing analysis symlinks touched.

Next: Task 8 deployment scope from plan 34, then platform acceptance. Task 5 bots already exist and now share the production compatibility gate.

## Live processes / resources

File claims are released at completion; no server is left running. Owner helper PID 57957 is stopped after push.


Final socket verification: the updated Task 5 `net-bots` use the production
`make_hello` / full reference verification shared by interactive clients. A
self-owned localhost `rr-netd` passed both 2- and 8-client WebSocket runs to
420 confirmed ticks with matching group hashes and agreed official results.
The 2-client hash was `7706892312449139743`; the 8-client hash was
`4354729446459175121`. These are scripted cutoff diagnostics, not finished
laps. Socket logs/captured race logs remain outside Git under
`/Users/archit/Dev/rr/.plan34-logs/net-menus-20261027`. Both test servers were
terminated; no listeners remain. The first attempt to reuse the same port
hit TCP TIME_WAIT; the 8-client run used separate claimed ports.
