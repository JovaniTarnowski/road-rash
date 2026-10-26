# Plan 34: Mano-a-mano multiplayer

> **Status (October 26, 2026):** New. Supersedes [plan 09](09-multiplayer.md), whose premise (two-player lockstep) the trace disproved. The analysis that plan 09 Task 1 asked for is in [../MULTIPLAYER.md](../MULTIPLAYER.md). The owner asked for this plan. Two items remain decision-gated: whether it joins the definition of done ([plan 15](15-release-and-acceptance.md) Task 4), and Task 8 (a hosted relay). Tasks 1–7 have no external blocker.

> **For agentic workers:** Read [00-INDEX.md](00-INDEX.md) and [../MULTIPLAYER.md](../MULTIPLAYER.md) first. Use superpowers:subagent-driven-development or superpowers:executing-plans. Steps use `- [ ]` checkboxes. Shared files need claims and serialized edits (AGENTS.md):
> - `src/race.*` and `src/game_main.cpp`;
> - `src/frontend.*`;
> - `oracle/aweman_proxy.c`.

**Goal:** Mano-a-mano plays like the original in the native port:
- host and join;
- two to eight humans on a LAN or over the internet;
- level choice, character choice and course vote;
- optional AI bikers and cars;
- F1–F8 chat;
- the original's distributed race simulation, results and disconnect rules.

macOS and Android first, then Linux and Windows (plan 33). The browser comes last.

**Architecture:** The original does not run lockstep (see MULTIPLAYER.md):
- every peer simulates its own human rider at 60 Hz, plus the AI riders it currently owns;
- each peer sends 24-byte records (snapshots and events), batched into 0x4c-byte frames, every 2nd tick;
- clients send only to the host, and the host relays to everyone;
- remote riders (`+0x324 == −1`) are moved from snapshots;
- the host owns the shared tables: AI owner table, interest masks, handicap bytes and cop.

The port reproduces exactly that model:
- **Wire format:** byte-identical frames and records, so a native peer can play an original peer under Wine. That interop is the acceptance oracle.
- **Socket layer:** modern UDP sockets underneath, IPv4 and IPv6.
- **Main-thread I/O:** the I/O runs on the main thread at tick boundaries, with no receive thread.
- **Replay:** a per-tick log of received records lets any networked race replay offline with identical results.

**Tech stack:**
- C++20 and SDL3;
- BSD sockets on macOS, Android and Linux; Winsock 2 on Windows;
- no new dependency for Tasks 1–7;
- CTest, libFuzzer (plan 22) and the Wine oracle.

## Recorded decisions

| Decision | Choice | Why |
| --- | --- | --- |
| Netcode model | Port the original's snapshot/ownership model, not deterministic lockstep | Fidelity: lockstep changes how remote riders move, how AI is handed off, and stalls on loss. The original tolerates loss through extrapolation |
| Wire format | The original's frames, records, ports 6996/6995, and lobby messages as traced in Task 1 | Lets the original under Wine act as a live peer oracle. No invented envelope on the classic path |
| Transports | TCP/IP only, shown as "Internet/LAN". **Modem and IP/X are dropped** and the popup shows them disabled with a native note | IPX has no modern OS stack. TAPI modems are not practical. Recorded deviation |
| Threads | Non-blocking sockets polled on the main thread once per frame before the tick batch; received records are applied at the start of the next simulated tick | The original applies packets asynchronously under critical sections, so its application tick is already arbitrary. A fixed rule keeps native races replayable |
| Internet reachability | Direct address entry ("Unlisted games") plus port forwarding, as in the original. Discovery is LAN broadcast | No project-run server is needed for Tasks 1–7. NAT traversal is Task 8, owner-gated |
| Untrusted input | Every received byte is parsed through bounds-checked codecs. Malformed frames are dropped and counted. The codecs have fuzz targets | The original trusts the wire. The port must not, because it can be reached from the internet |

## Global constraints

- **Offline is untouched.** Single-player races stay bit-identical. Every multiplayer branch is gated on the session flags, as `DAT_004c5af9`/`DAT_004c5b00` are. The CTest suite and `make verify-sim` stay green after every task.
- **Simulation code stays pure.** The network layer feeds records in and takes records out. Simulation code has no sockets, wall clock, allocation in the tick path, or floats.
- **Port line by line.** Every networked branch is ported from its cited address in the same style as the rest of `race.cpp`. Speculative mappings stay labelled until a Task 1 capture confirms them.
- **Evidence stays local.** Original captures go in git-ignored `analysis/oracle/net-*`. Committed tests use synthetic frames.

---

### Task 1: Two-instance original capture and the remaining traces

**Files:**
- Modify: `oracle/aweman_proxy.c`, `oracle/run.sh`, `oracle/install.sh`
- Create: `tools/net_capture_validate.py`
- Update: `docs/MULTIPLAYER.md`, `docs/REVERSE_ENGINEERING.md`, `docs/TOOLING.md`

- [ ] **Step 1: WSOCK32 hooks.** Under `RR_ORACLE_NET=<instance>`, the proxy rewrites the WSOCK32 import slots (`0x004ca990..0x004ca9d8`; see the MULTIPLAYER.md ordinal table). The hooks:
  - remap ports 6995/6996 to an instance-specific pair, so two instances run on one Mac;
  - turn the broadcast address into a send to the other instance's port;
  - log every `sendto`/`send`/`recv`/`recvfrom` as `tick, direction, peer, length, bytes` to `analysis/oracle/net-<name>-<instance>.log`.

  The log has a header, contiguous sequence numbers and a completion marker (the plan 10 capture rules).
- [ ] **Step 2: Scripted sessions.** Drive instance A as host and instance B as join with menu hooks of the `RR_ORACLE_MENU` kind: call handlers `0x0042fc40`/`0x0042fef0`, fix the popup choices and pick the course. Inject controls per tick as plan 10 does. Each instance runs from its own install directory and log names; the prefix is shared, so serialize it (claims). Capture:
  - (a) lobby only;
  - (b) a two-human Napa race with AI bikers None and cars disabled;
  - (c) the same race with Full AI and cars on;
  - (d) a crash, a remount and a walk-back on one peer;
  - (e) the client disconnecting mid-race;
  - (f) the host leaving.
- [ ] **Step 3: Trace what the static pass left open.** Use captures plus disassembly; record addresses and field widths for each:
  - lobby message kinds other than 0x0a (advertise, join request/accept/reject, roster, character, course vote, chat, start, leave). Start from timer `0x004301e0`, `0x0043b2b0`, `0x0043b320`, `0x00431020`, `0x00431740`, `0x0043b390`;
  - any acknowledgement or resend;
  - timeouts and drop detection (`0x004972d8`);
  - the networked grid count and slot layout in `0x00405600`;
  - how `DAT_0049bce8` is agreed;
  - the `0x0043fe90` remote-mover algorithm;
  - the `0x0040a9e0` remote walk;
  - the AI ownership hand-off rules (`0x00431e00`, `0x00431db0`, `0x00432a30`);
  - type 5/7/8 record meanings;
  - chat send and on-screen display;
  - the results-official flow (441/442);
  - which options the host sets (AI bikers, cars) and how they reach clients.
- [ ] **Step 4:** `tools/net_capture_validate.py` checks header, contiguity and completion, and decodes every frame with the record table. Unknown types fail it.
- [ ] **Step 5:** Update MULTIPLAYER.md: move every confirmed item from "inferred" to "traced", correct anything wrong, and record which earlier claim changed. Add a REVERSE_ENGINEERING.md section. Commit: `oracle: two-instance mano-a-mano capture and protocol trace`.

**Acceptance:** Captures (a)–(f) validate, and every byte position in every observed frame has a named meaning.

---

### Task 2: Wire codec and transport

**Files:**
- Create: `src/net/wire.hpp`, `src/net/wire.cpp`, `src/net/transport.hpp`, `src/net/udp_transport.cpp`, `src/net/loopback_transport.cpp`
- Create: `tests/net_wire_test.cpp`, `tests/net_transport_test.cpp`, `fuzz/fuzz_net_wire.cpp` (picked up by the `fuzz/fuzz_*.cpp` glob)
- Modify: `CMakeLists.txt`

**Interfaces:**
```cpp
namespace rr::net {
struct Record { uint8_t type, slot; std::array<uint8_t, 24> bytes; };         // 48-byte crash = two slots
struct Frame { uint8_t kind; std::vector<Record> records; };                  // 0x4c bytes on the wire
std::optional<Frame> decode_frame(std::span<const uint8_t>);                   // nullopt on any violation
std::array<uint8_t, 0x4c> encode_frame(const Frame&);
// Lobby messages: one struct per kind recovered in Task 1, same decode/encode pair.
struct Endpoint { /* v4 or v6 address + port */ };
class Transport {
  public:
    virtual void send(const Endpoint&, std::span<const uint8_t>) = 0;
    virtual void broadcast(uint16_t port, std::span<const uint8_t>) = 0;     // LAN discovery
    virtual std::optional<std::pair<Endpoint, std::vector<uint8_t>>> poll() = 0; // non-blocking
    virtual ~Transport() = default;
};
std::unique_ptr<Transport> udp_transport(uint16_t game_port, uint16_t discovery_port);
// Tests: N endpoints sharing one seeded network with loss, duplication, reordering and delay.
struct LossyNet { double loss, duplicate; int min_delay_ticks, max_delay_ticks; uint32_t seed; };
std::vector<std::unique_ptr<Transport>> loopback_net(int peers, LossyNet);
}
```

- [ ] **Step 1: Tests first.**
  - Every frame from the Task 1 captures round-trips byte-identically. This test is golden-gated with exit 77.
  - Synthetic frames for each record type round-trip.
  - Truncated, oversized and wrong-kind frames are rejected, as are out-of-range slots and unknown types.
  - The loopback net delivers in order with zero loss, and is reproducible from its seed under loss.
- [ ] **Step 2:** Implement. Sockets are non-blocking; the UDP transport binds dual-stack where available and sets `SO_BROADCAST`. Errors become a typed status; there are no exceptions in the poll path.
- [ ] **Step 3:** Add the fuzz target to the plan 22 fuzz set and CI. Commit: `net: original frame codec and UDP transport`.

---

### Task 3: Networked race simulation

**Files:**
- Modify: `src/race.hpp`, `src/race.cpp`, `src/race_schedule.cpp`, `src/race_setup.hpp`, `src/race_setup.cpp`, `src/traffic.cpp`, `src/police.cpp`, `src/crash.cpp`, `src/hud.cpp`
- Create: `src/net/race_link.hpp`, `src/net/race_link.cpp`, `tests/net_race_test.cpp`

**Interfaces:**
```cpp
namespace rr {
struct NetRaceConfig {                     // all-zero = offline (bit-identical to today)
    bool host = false, client = false;     // DAT_004c5af9 / DAT_004c5b00
    int local_slot = 0;                    // game state +0, rider +0x328
    std::array<bool, 8> slot_in_use{};     // 0x00497304 table
    int snapshot_mask = 1;                 // DAT_004c5afc
    int ai_bikers = 0;                     // 0 none, 1 limited, 2 full
    bool cars = true;
    int32_t shared_draw = 0;               // DAT_0049bce8
};
// RaceConfig gains `NetRaceConfig net;`.
class RaceLink {                           // owns the 0x00499430 frame buffers and tables
  public:
    void receive(int from_slot, const net::Frame&); // 0x0043e9c0: queue, applied at next tick start
    void before_tick(Race&);                        // apply queued records in arrival order
    void after_tick(Race&);                         // 0x0043cdb0 / 0x0044f040 / 0x00442f60 cadence
    std::vector<std::pair<int, net::Frame>> take_outgoing();
};
}
```

- [ ] **Step 1: Record/apply harness.** A networked race writes a log of everything it received, per tick. `net_race_test` replays a log into a fresh `Race` and requires identical per-tick field dumps. This makes every later step testable offline.
- [ ] **Step 2: Grid and roles.** Port the networked branch of `0x00405600` (Task 1 grid count and slots). Port rider `+0x324`/`+0x328` assignment, AI biker count by option, the countdown 0x168 (`0x00442dd0`), and `0x00441750` returning the shared draw in mode 3. Offline replays must stay identical.
- [ ] **Step 3: Send path.**
  - `0x0043cdb0` with `0x0043c7e0` (rider/bike snapshot), `0x0043cc80` (crash) and `0x0043cd50` (remount);
  - `0x0043c4a0` pending flags, `0x0043d040`/`0x0043d7d0`/`0x0043d610` frame filling, `0x0043cff0` flush;
  - the heartbeat and handicap `0x00442f60`.

  Test: frames built from a scripted local race are byte-identical to the original's frames for the same tick inputs in capture (b). This is a component comparison of what the sender produces.
- [ ] **Step 4: Receive path and remote riders.**
  - `0x0043e9c0` dispatch;
  - the `0x0043fe90` remote mover;
  - `0x0040a9e0` remote walk with `0x00440410`;
  - `0x00404480` remote remount;
  - remote crash entry;
  - `0x0043cbb0`;
  - host relay `0x0043f170`/`0x0043f2e0`/`0x0043f3a0`.

  Test: feed capture (b)'s received frames into instance B's native race. The remote rider's fields must match B's per-tick dump. This needs plan 10 field dumps from both instances, added to the Task 1 captures.
- [ ] **Step 5: Shared systems.**
  - AI ownership (types 0xa/0xb and host tables 0xf/6–8);
  - traffic: ×4 density, the odd-kind suppression and nearest-remote gates `0x0044f0d0`, car records type 6 through `0x0044e220`/`0x0044e270`;
  - cop ownership (type 0xd, `0x00407310`);
  - the pair events of type 7 and placements of type 8;
  - nearest-opponent HUD `0x0044f1b0` and the networked rider lists `0x00437860`/`0x00446bf0`.

  Accept each against capture (c).
- [ ] **Step 6: Two to eight peers in process.** Run N `Race` + `RaceLink` instances over `loopback_net` with 0%, 5% and 20% loss, duplication, reordering and 0–30 ticks of delay, for 2, 4 and 8 peers. Assert:
  - no crash, and no unbounded queue;
  - every peer's own rider state equals a replay of its own log;
  - every remote rider stays within the original mover's tolerance as measured in Task 1, never a native invention;
  - finish places agree across peers.
- [ ] **Step 7:** Run `make verify-sim` and the full gate. Commit: `race: networked mano-a-mano simulation`.

---

### Task 4: Lobby, session and race flow

**Files:**
- Create: `src/net/session.hpp`, `src/net/session.cpp`, `tests/net_session_test.cpp`
- Modify: `src/frontend.hpp`, `src/frontend.cpp`, `src/career.cpp`, `src/race_screen.cpp`, `src/game_main.cpp`, `src/native_strings.cpp`, `src/settings.*`

- [ ] **Step 1: Session state machine.** Port the Task 1 lobby protocol: host advertise (500 ms timer `0x004301e0`), discovery on 6995, join request/accept/reject, slot allocation, roster announcements (strings 402–408, 420), character choice, the host's level, AI biker and car options, course vote (312), start, drop detection and timeouts, and disconnect (`0x00431600`). Tests drive host and clients over `loopback_net`, including a full game, a rejected join, a client drop in lobby, and the host quitting.
- [ ] **Step 2: Screens.**
  - Enable main-menu item 2. Screens 0x0b, 0x0c (with Disconnect as the sixth item) and 0x0d (F1–F8 chat editing into 60-byte slots) use the original tables and enable rules from `0x00430140`.
  - The original Win32 dialogs become native screens in the original menu style, like the existing save list and plan 18 screens: the join list (Host/Level/Avail/Status, public and unlisted games), address entry and connecting/waiting status. Each replacement is listed in REVERSE_ENGINEERING.md "Not ported or replaced natively".
- [ ] **Step 3: Race flow.**
  - Start a `RaceSetup` with `NetRaceConfig`;
  - pump the transport before each tick batch in `RaceScreen`;
  - show F1–F8 chat in race as the original does (Task 1 trace);
  - show the finished/wrecked/busted announcements (427–429), the master-leave warning (430), "race results are now official" (441), and termination (442);
  - mano results: no qualification bits, network name in the outcome, the `DUEL` video;
  - return to screen 0x0b.

  Pause must not stop the networked simulation; follow the original's behaviour from Task 1.
- [ ] **Step 4: Transport popup.** TCP/IP is shown as "Internet/LAN" with native string slots. Modem and IP/X stay visible but disabled, with a native description saying they are unavailable on modern systems.
- [ ] **Step 5:** Commit: `frontend: mano-a-mano host, join, chat and results`.

---

### Task 5: Native-to-original interop

**Files:**
- Create: `tools/net_interop.sh`
- Update: `docs/VERIFICATION.md`, `docs/MULTIPLAYER.md`

- [ ] **Step 1:** Host natively and join with the original under Wine (Task 1 proxy port remap), then the reverse. Race Napa with scripted controls on both sides.
- [ ] **Step 2: Assert** that:
  - both lobbies show the same roster and course;
  - both races start on the same tick;
  - every frame each side sends decodes cleanly on the other;
  - remote riders track on both screens;
  - places agree at the finish.

  Record the logs and screenshots under `analysis/oracle/net-interop-*`.
- [ ] **Step 3:** Commit: `verify: native and original mano-a-mano interop`.

---

### Task 6: Platform integration

**Files:**
- Modify: `android/app/src/main/AndroidManifest.xml`, the Android activity (JNI multicast lock), `macos/Info.plist.in`, `docs/USER_GUIDE.md`

- [ ] **Step 1: macOS.**
  - Add `NSLocalNetworkUsageDescription`, because LAN broadcast and receive trigger the Local Network privacy prompt.
  - Document the firewall prompt for incoming UDP.
  - Confirm that a signed package gets both prompts.
- [ ] **Step 2: Android.**
  - Add the `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE` and `CHANGE_WIFI_MULTICAST_STATE` permissions.
  - Hold a `WifiManager.MulticastLock` while the lobby is open; without it, broadcast receive fails on many devices.
  - Lifecycle: going to the background in a race sends a leave, then a drop, as a client quitting does.
- [ ] **Step 3: Linux and Windows** (with plan 33). Use Winsock 2 on Windows. Check that the Windows Defender firewall prompt appears.
- [ ] **Step 4: Hardware validation.** Mac ↔ MANGMI AIR X on one Wi-Fi network: discover, join, race and finish. Three peers: two Mac processes on different ports plus the device. Record what was observed in VERIFICATION.md. APK assembly is not this check.
- [ ] **Step 5:** Commit: `platform: mano-a-mano on macOS and Android`.

---

### Task 7: Robustness and acceptance

- [ ] **Step 1: Soak.** In process, run 100 eight-peer races at 20% loss with jitter. Then run a one-hour two-machine LAN session. The pass criteria are no desynced finish order, no stall over the Task 1 timeout, and no memory growth.
- [ ] **Step 2: Hostile input.** Fuzz the codecs. Replay corrupt, truncated and spoofed-slot frames into a live session: they must be dropped and counted, never crash it or move another peer's rider.
- [ ] **Step 3: Docs.** Update USER_GUIDE.md (how to host, join, port forward and chat), PROGRESS.md, CURRENT_STATUS.md / agent-status.json ("multiplayer disabled" becomes the measured state), and REVERSE_ENGINEERING.md (replaced dialogs, dropped transports).
- [ ] **Step 4:** Commit: `docs: mano-a-mano acceptance`.

**Plan acceptance (Tasks 1–7):**
- the Task 1 captures decode fully;
- sender frames and remote-rider fields match the original captures;
- native↔original interop races finish with agreed places;
- the 2/4/8-peer lossy soak passes;
- Mac↔Android LAN races were observed on hardware;
- offline replays (`make verify-sim` and the oracle goldens) are unchanged.

---

### Task 8 (owner-gated): Play beyond direct addresses, and the browser

The original reaches peers by address only. Two native extensions need infrastructure the project would have to run, so the owner decides:

1. **NAT traversal or relay:** a small self-hostable rendezvous/relay (UDP hole punching, falling back to relay). The classic wire format is carried unchanged inside the relay envelope, so players need no port forwarding.
2. **Browser (plan 23):** browsers have no UDP. This needs WebRTC data channels (unreliable, unordered) with the same relay as signalling, plus a `Transport` built on them.

Until the owner decides, the user guide states that internet play needs a forwarded UDP port 6996 on the host, and that the browser build has no mano-a-mano.

## Not in this plan

Same-screen split-screen play is not in the original. It would be a native enhancement with its own renderer and input work. If wanted, it gets a separate plan; it would build on Task 3's multiple-local-controller support.
