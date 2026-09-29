# Multiplayer Implementation Plan (decision-gated)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Read [00-INDEX.md](00-INDEX.md) first. **Do not start Task 2 or later until Task 1's decision is recorded by the project owner.** Requires plans 04–06 and 08.

**Goal:** Recover the original's two-player modes (modem via TAPI, serial via the Win32 Comm API, network via Winsock). Then either port them as internet/LAN play over UDP using the deterministic simulation, or formally drop them with the reason documented.

**Architecture:** The original runs lockstep-style multiplayer:
- Flags `0x004c5b00` / `0x004c5af9` select network or link modes.
- In those modes `0x0044b2e0` calls `0x0043cdb0` and `0x0044f040` whenever `(0x004c5afc & tick) == 0`, and `0x004163b0` every 4,096 ticks (`(tick & 0xfff) == 0xffa`).
- Traffic spawning is disabled (`0x0044df60` returns early).
- `0x00441750` returns a fixed value instead of `rand()` when `0x004753b0 == 3`.

Because the native simulation is already a deterministic function of (seed, per-tick controls) (plan 04), a port can exchange per-tick `ControlMask`s over UDP with input delay and keep both simulations identical. That is the original's own model, over a modern transport.

**Tech Stack:** C++17, SDL3 (`SDL_net` is not required; plain BSD sockets on macOS/Android), CTest.

**Spec:** [00-INDEX.md](00-INDEX.md); plan 04 (`World`, `ControlMask`, `Random`, determinism tests).

## Global Constraints

- Only the transport differs from the original; simulation results must be identical to single-machine replays of the same input streams.
- No central server run by this project. Hosts connect directly (LAN discovery plus manual address entry).
- Android needs the `INTERNET` and `ACCESS_NETWORK_STATE` permissions, added only if this plan proceeds.

## Review Focus

1. **Packet loss and reordering:** the lockstep must stall and recover, never desynchronise. Task 3 runs a lossy in-process transport at 20% loss with reordering.
2. **Desynchronisation:** both peers exchange a state hash every N ticks, and a mismatch stops the race with a message. Task 3 includes a forced-desync test.
3. **A peer vanishing mid-race:** a timeout ends the race for the remaining player as the original does (Task 1 records that rule), without hanging.
4. **Mismatched game data or versions between peers:** the handshake compares the reference-hash digest and the protocol version. Task 2 tests a mismatch.
5. **Different frame rates on the two machines** (60 Hz Android vs 144 Hz Mac): fixed-tick lockstep must keep them in sync. Task 3 tests peers at 30 and 144 simulated Hz.

---

### Task 1: Recover the original modes and decide

**Files:**
- Create: `docs/MULTIPLAYER.md`

- [ ] **Step 1: Trace**

Trace:
- the mode selection UI
- setup for TAPI (`lineMakeCall`, `lineAnswer`, …), serial (`BuildCommDCBA`, `SetCommState`, `ReadFile`/`WriteFile` with overlapped IO) and Winsock (ordinal imports 2, 3, 4, 6, 8, 9, 12, 15, 16, 17, 19, 20, 21, 23, 52, 57, 111, 115, 116; map ordinals to names from the standard `WSOCK32` export table)
- the message format exchanged each sync (`0x0043cdb0`, `0x0044f040`)
- what state is synchronised (controls only, or positions too)
- the role of `0x004753b0 == 3` in `0x00441750`
- the disconnect rules
- which game modes are available in two-player

- [ ] **Step 2: Write `docs/MULTIPLAYER.md`**

Record the findings, and an estimate for porting (Tasks 2–4) versus dropping.

- [ ] **Step 3: Get the decision**

Ask the project owner to decide: **port** (continue with Task 2) or **drop** (record the decision and reason in `PROGRESS.md` and the definition of done, then stop).

- [ ] **Step 4:** Commit: `docs: original multiplayer analysis and decision`.

---

### Task 2: Session protocol and handshake (only if porting)

**Files:**
- Create: `src/net.hpp`, `src/net.cpp`, `tests/net_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace rr {
  struct NetMessage { enum Kind : uint8_t { hello, hello_ack, start, inputs, hash, bye } kind; std::vector<uint8_t> payload; };
  class Transport { public: virtual bool send(const NetMessage&) = 0; virtual std::optional<NetMessage> receive() = 0; virtual ~Transport() = default; };
  std::unique_ptr<Transport> udp_transport(const std::string& host, uint16_t port, bool listen);
  std::unique_ptr<Transport> loopback_pair(std::unique_ptr<Transport>& other, double loss, uint32_t seed);   // tests
  struct Hello { uint32_t protocol; std::array<uint8_t,32> data_digest; };   // digest of reference_hashes.inc contents
  }
  ```

- [ ] **Step 1:** Tests: the handshake succeeds on the loopback pair; a different `data_digest` or `protocol` fails with a readable reason; messages survive serialisation round trips; malformed payloads are rejected.
- [ ] **Step 2:** Implement (UDP with sequence numbers and acknowledgements for control messages). Commit: `net: transport and handshake`.

---

### Task 3: Lockstep race (only if porting)

**Files:**
- Create: `src/lockstep.hpp`, `src/lockstep.cpp`, `tests/lockstep_test.cpp`

**Interfaces:**
- Produces: `class Lockstep { public: Lockstep(Transport&, int local_player, int input_delay_ticks); bool ready(int32_t tick) const; void submit(int32_t tick, ControlMask); std::array<ControlMask,2> inputs(int32_t tick) const; void exchange_hash(int32_t tick, uint64_t world_hash); bool desynced() const; };` plus `uint64_t hash_world(const World&)`.

- [ ] **Step 1: Tests**

Run two in-process `Race` instances over `loopback_pair` with 20% loss and reordering, driving recorded input streams. Assert:
- identical `hash_world` at every tick and equality with a single-machine replay of both streams
- a forced desync (mutating one world) is detected within N ticks
- peers at 30 and 144 simulated Hz stay equal

- [ ] **Step 2:** Implement, including the original's multiplayer rule changes (no traffic, fixed `0x00441750` behaviour) behind a new field `bool multiplayer = false;` added to plan 04's `RaceConfig`.
- [ ] **Step 3:** Commit: `net: lockstep two-player races`.

---

### Task 4: Front end and platform integration (only if porting)

**Files:**
- Modify: `src/frontend.cpp` (host/join screens, LAN discovery via UDP broadcast on a fixed port, manual address), `android/app/src/main/AndroidManifest.xml` (network permissions)

- [ ] **Step 1:** Play a two-player race between the Mac and the Android device on the same LAN. Record it in `VERIFICATION.md`.
- [ ] **Step 2:** Update the docs. Commit: `game: two-player network mode`.
